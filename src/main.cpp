#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_system.h>
#include <time.h>

#include "pins.h"
#include "Defines.h"
#include "Logger.h"
#include "SettingsManager.h"
#include "DeviceTopicMap.h"
#include "WiFiController.h"
#include "MqttClient.h"
#include "ZigbeeCoordinator.h"
#include "SerialCli.h"
#include "WebConsole.h"
#include "JsonField.h"
#include "InterChipHost.h"
#include "InterChipSlave.h"
#include "StatusRgb.h"
#include "ZigbeeSpiProxy.h"
#include "FoundDeviceList.h"
#include "DeviceStore.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator mode is not selected (ZIGBEE_MODE_ZCZR)"
#endif

enum BoardRole : uint8_t {
    BoardRoleHost = 0,
    BoardRoleSlave = 1
};

static BoardRole boardRole = BoardRoleHost;
static SettingsManager *settingsManager = nullptr;
static DeviceTopicMap *topicMap = nullptr;
static WiFiController *wifiController = nullptr;
static MqttClient *mqttClient = nullptr;
static ZigbeeCoordinator *zigbeeCoordinator = nullptr;
static SerialCli *serialCli = nullptr;
static WebConsole *webConsole = nullptr;
static FoundDeviceList *foundDevices = nullptr;
static DeviceStore *deviceStore = nullptr;
static bool ntpStarted = false;
static bool slaveZigbeeStarted = false;
static bool slaveZigbeeStarting = false;
static bool hostPreparationLatched = false;

static BoardRole readBoardRole() {
    pinMode(PIN_BOARD_ROLE, INPUT);
    delay(2);
    if (digitalRead(PIN_BOARD_ROLE) == HIGH) {
        return BoardRoleSlave;
    }
    return BoardRoleHost;
}

static void onMqttRawMessage(char *topic, byte *payload, unsigned int length) {
    if (mqttClient != nullptr) {
        mqttClient->onMessage(topic, payload, length);
    }
}

static void applyDeviceConfigPayload(const char *payload) {
    String ieeeText;
    String friendlyName;
    String stateTopic;
    String commandTopic;
    String availability;
    if (!extractJsonString(payload, "ieee", ieeeText)) {
        LOGGER.error("config/device needs \"ieee\"");
        return;
    }
    extractJsonString(payload, "name", friendlyName);
    extractJsonString(payload, "state", stateTopic);
    extractJsonString(payload, "command", commandTopic);
    extractJsonString(payload, "availability", availability);
    int parsedChannels = DEVICE_CHANNEL_COUNT_DEFAULT;
    if (!extractJsonInt(payload, "channels", parsedChannels)) {
        parsedChannels = DEVICE_CHANNEL_COUNT_DEFAULT;
    }

    uint8_t ieee[8];
    if (!topicMap->parseIeee(ieeeText.c_str(), ieee)) {
        LOGGER.error("Bad IEEE in config/device");
        return;
    }
    DeviceTopicEntry *entry = topicMap->upsert(
        ieee,
        friendlyName.c_str(),
        stateTopic.c_str(),
        commandTopic.c_str(),
        availability.c_str(),
        DeviceTopicMap::normalizeChannelCount(parsedChannels)
    );
    if (entry == nullptr) {
        LOGGER.error("Device map full");
        return;
    }
    mqttClient->subscribeDeviceCommands();
    if (!ZIGBEE_SPI_PROXY.enqueueDeviceUpsert(entry)) {
        LOGGER.error("Slave device change queue full");
        return;
    }
    LOGGER.info("Configured topics for " + ieeeText);
}

static void onMqttLogicalMessage(const char *topic, const char *payload) {
    if (strcmp(topic, mqttClient->permitJoinTopic().c_str()) == 0) {
        String body = String(payload);
        body.trim();
        body.toLowerCase();
        if (body == "off" || body == "0" || body == "false" || body == "close") {
            ZIGBEE_SPI_PROXY.closeJoin();
            mqttClient->publishStatus("join_closed");
            return;
        }
        int seconds = DEFAULT_PERMIT_JOIN_SEC;
        if (body == "on" || body == "true") {
            seconds = DEFAULT_PERMIT_JOIN_SEC;
        } else if (body.length() > 0) {
            seconds = body.toInt();
        }
        if (seconds <= 0) {
            ZIGBEE_SPI_PROXY.closeJoin();
        } else {
            ZIGBEE_SPI_PROXY.permitJoin((uint8_t)constrain(seconds, 1, 254));
        }
        mqttClient->publishStatus("join_open");
        return;
    }

    if (strcmp(topic, mqttClient->configDeviceTopic().c_str()) == 0) {
        applyDeviceConfigPayload(payload);
        return;
    }

    uint8_t topicEndpoint = 0;
    DeviceTopicEntry *entry = topicMap->findByCommandTopic(topic, &topicEndpoint);
    if (entry == nullptr) {
        return;
    }
    String action = String(payload);
    uint8_t commandEndpoint = topicEndpoint;
    if (DeviceTopicMap::usesPayloadParse(entry->channelCount)) {
        uint8_t parsedEndpoint = 0;
        String parsedAction;
        if (!DeviceTopicMap::parseChannelPayload(payload, &parsedEndpoint, &parsedAction)) {
            LOGGER.warning("MQTT command ignored; expected ch-<ep>##ON");
            return;
        }
        commandEndpoint = parsedEndpoint;
        action = parsedAction;
    } else if (DeviceTopicMap::usesTopicSuffix(entry->channelCount) && commandEndpoint == 0) {
        commandEndpoint = 1;
    }
    LOGGER.info(String("MQTT recv topic=") + topic + " data=" + payload);
    ZIGBEE_SPI_PROXY.controlOnOff(entry->ieee, action.c_str(), commandEndpoint);
}

static void onLightState(const char *message, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr) {
    (void)shortAddr;
    DeviceTopicEntry *entry = topicMap->findByIeee(ieee);
    if (entry == nullptr) {
        LOGGER.info("Unmapped device state; assign topics via map or MQTT bridge/config/device");
        return;
    }
    if (entry->stateTopic[0] == '\0') {
        LOGGER.info("Device has no state topic configured");
        return;
    }
    mqttClient->publishDeviceState(entry, message, endpoint);
}

static void onHostSpiEvent(const SpiFrame &frame) {
    ZIGBEE_SPI_PROXY.onSpiEvent(frame);
    if (frame.cmd == SpiEvtDeviceJoin && foundDevices != nullptr && topicMap != nullptr) {
        foundDevices->noteJoin(frame, topicMap);
    }
    if (frame.cmd == SpiEvtAttrReport && frame.length >= 12 && foundDevices != nullptr && topicMap != nullptr) {
        uint8_t ieee[8];
        memcpy(ieee, frame.payload, 8);
        const uint8_t endpoint = frame.payload[8];
        const uint16_t shortAddr = (uint16_t)frame.payload[9] | ((uint16_t)frame.payload[10] << 8);
        foundDevices->noteIdentity(ieee, shortAddr, endpoint, "", "", topicMap);
    }
    if (frame.cmd == SpiEvtSettingsOk && topicMap != nullptr) {
        ZIGBEE_SPI_PROXY.requestRegistryPull(topicMap);
    }
}

static bool startDeviceSearch() {
    if (settingsManager == nullptr) {
        return false;
    }
    if (!ZIGBEE_SPI_PROXY.commandsAllowed()) {
        LOGGER.warning("Search skipped: slave SPI not ready");
        return false;
    }
    uint8_t seconds = settingsManager->getSettings()->zigbee.permitJoinOnBootSec;
    if (seconds == 0) {
        seconds = DEFAULT_PERMIT_JOIN_SEC;
    }
    LOGGER.info("Search permit join " + String(seconds) + "s");
    return ZIGBEE_SPI_PROXY.permitJoin(seconds);
}

static void stopDeviceSearch() {
    ZIGBEE_SPI_PROXY.closeJoin();
}

static bool onDeviceUpserted(const DeviceTopicEntry *entry) {
    if (mqttClient != nullptr) {
        mqttClient->subscribeDeviceCommands();
    }
    return ZIGBEE_SPI_PROXY.enqueueDeviceUpsert(entry);
}

static bool onDeviceRemoved(const uint8_t ieee[8]) {
    if (mqttClient != nullptr) {
        mqttClient->subscribeDeviceCommands();
    }
    return ZIGBEE_SPI_PROXY.enqueueDeviceDelete(ieee);
}

static void runDeviceRegistryFixtures() {
    LOGGER.info(
        "Settings v5 main " + String((int)sizeof(GlobalSettings))
        + " reserved@ " + String((int)offsetof(GlobalSettings, reserved))
        + " slots " + String(DEVICE_MAP_SLOTS)
    );
    if (sizeof(GlobalSettings) > 4096 || (offsetof(GlobalSettings, reserved) % 128u) != 0
        || DEVICE_MAP_SLOTS != 128) {
        LOGGER.error("Settings layout fixture failed");
    } else {
        LOGGER.info("Settings layout fixture ok");
    }

    if (!settingsManager->mainEqualsCommitted()) {
        LOGGER.error("Main snapshot fixture failed before device save");
    } else if (!settingsManager->saveDeviceSlot(0)) {
        LOGGER.error("Device slot save fixture failed");
    } else if (!settingsManager->mainEqualsCommitted()) {
        LOGGER.error("Device save dirtied main-block snapshot");
    } else {
        LOGGER.info("Device slot save leaves main snapshot clean");
    }

    if (foundDevices == nullptr || topicMap == nullptr) {
        return;
    }
    foundDevices->clear();
    SpiFrame joinFrame;
    memset(&joinFrame, 0, sizeof(joinFrame));
    joinFrame.cmd = SpiEvtDeviceJoin;
    joinFrame.length = 75;
    joinFrame.payload[0] = 0xAA;
    joinFrame.payload[8] = 0x34;
    joinFrame.payload[9] = 0x12;
    joinFrame.payload[10] = 1;
    strncpy((char *)joinFrame.payload + 11, "Acme", 31);
    strncpy((char *)joinFrame.payload + 43, "Plug", 31);
    foundDevices->noteJoin(joinFrame, topicMap);
    String foundJson = foundDevices->listJson(topicMap);
    if (foundJson.indexOf("AA") < 0) {
        LOGGER.error("Found-join fixture failed");
    } else {
        LOGGER.info("Found-join fixture ok");
    }

    uint8_t registeredIeee[8];
    memset(registeredIeee, 0, sizeof(registeredIeee));
    registeredIeee[0] = 0xBB;
    DeviceTopicEntry *probe = topicMap->findByIeee(registeredIeee);
    const bool createdProbe = probe == nullptr;
    if (createdProbe) {
        probe = topicMap->upsert(registeredIeee, "fixture", "", "", "");
    }
    foundDevices->clear();
    joinFrame.payload[0] = 0xBB;
    foundDevices->noteJoin(joinFrame, topicMap);
    foundJson = foundDevices->listJson(topicMap);
    if (foundJson.indexOf("BB") >= 0) {
        LOGGER.error("Registered join leaked into found list");
    } else {
        LOGGER.info("Registered join skipped found list");
    }
    if (createdProbe && probe != nullptr) {
        memset(probe, 0, sizeof(DeviceTopicEntry));
    }
    foundDevices->clear();

    DeviceTopicEntry *backup = (DeviceTopicEntry *)malloc(sizeof(DeviceTopicEntry) * DEVICE_MAP_SLOTS);
    if (backup == nullptr) {
        LOGGER.error("Device map full fixture alloc failed");
        return;
    }
    memcpy(backup, settingsManager->devices(), sizeof(DeviceTopicEntry) * DEVICE_MAP_SLOTS);
    for (int i = 0; i < DEVICE_MAP_SLOTS; i++) {
        DeviceTopicEntry *slot = topicMap->slotAt(i);
        if (slot != nullptr && !slot->used) {
            slot->used = 1;
            slot->ieee[0] = (uint8_t)(i + 1);
        }
    }
    uint8_t extraIeee[8];
    memset(extraIeee, 0xCC, sizeof(extraIeee));
    if (topicMap->upsert(extraIeee, "overflow", "", "", "") != nullptr) {
        LOGGER.error("Full map fixture did not reject 129th device");
    } else {
        LOGGER.info("Full map fixture rejected 129th device");
    }
    memcpy(settingsManager->devices(), backup, sizeof(DeviceTopicEntry) * DEVICE_MAP_SLOTS);
    free(backup);
}

static void onWiFiArduinoEvent(arduino_event_id_t event, arduino_event_info_t info) {
    (void)info;
    if (event == ARDUINO_EVENT_WIFI_AP_START) {
        LOGGER.info("WiFi AP start");
        return;
    }
    if (event == ARDUINO_EVENT_WIFI_AP_STOP) {
        LOGGER.warning("WiFi AP stop");
        return;
    }
    if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
        LOGGER.info("WiFi AP client joined");
        return;
    }
    if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
        LOGGER.info("WiFi AP client left");
        return;
    }
    if (wifiController == nullptr) {
        return;
    }
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED || event == ARDUINO_EVENT_WIFI_STA_LOST_IP) {
        wifiController->notifyStaDisconnected();
        return;
    }
    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        wifiController->notifyStaGotIp();
    }
}

static void maybeStartNtp() {
    if (ntpStarted || wifiController == nullptr || !wifiController->isStaConnected()) {
        return;
    }
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    ntpStarted = true;
    LOGGER.info("NTP started");
}

static void startZigbeeOnOwnTask(uint8_t channel, uint8_t permitJoinSec) {
    struct StartArgs {
        uint8_t channel;
        uint8_t permitJoinSec;
    };
    StartArgs *args = new StartArgs{channel, permitJoinSec};
    const BaseType_t created = xTaskCreate(
        [](void *context) {
            StartArgs *startArgs = static_cast<StartArgs *>(context);
            INTER_CHIP_SLAVE.setPumpPaused(true);
            vTaskDelay(pdMS_TO_TICKS(40));
            bool started = false;
            if (zigbeeCoordinator != nullptr) {
                started = zigbeeCoordinator->begin(startArgs->channel, startArgs->permitJoinSec);
            }
            if (!started) {
                LOGGER.error("Zigbee start failed — check ZCZR partition / erase flash");
                slaveZigbeeStarting = false;
            } else {
                slaveZigbeeStarted = true;
                slaveZigbeeStarting = false;
            }
            INTER_CHIP_SLAVE.resumeAfterRadioPause();
            delete startArgs;
            vTaskDelete(nullptr);
        },
        "zbStart",
        16384,
        args,
        1,
        nullptr
    );
    if (created != pdPASS) {
        delete args;
        slaveZigbeeStarting = false;
        LOGGER.error("Zigbee start task failed");
        INTER_CHIP_SLAVE.setPumpPaused(false);
    }
}

static void persistSlaveDeviceStore(bool allowEmpty) {
    if (deviceStore == nullptr) {
        return;
    }
    deviceStore->requestPersist(allowEmpty);
    deviceStore->persistIfDue();
}

static void createSlaveCoordinator() {
    if (zigbeeCoordinator != nullptr) {
        return;
    }
    zigbeeCoordinator = new ZigbeeCoordinator();
    zigbeeCoordinator->setRegistryChangedHandler(
        []() {
            persistSlaveDeviceStore(false);
            INTER_CHIP_SLAVE.requestDeviceDump();
            INTER_CHIP_SLAVE.requestDevicesFileDump();
        }
    );
    zigbeeCoordinator->setDeviceBoundHandler(
        [](const BoundZigbeeDevice *device) {
            if (device == nullptr) {
                return;
            }
            INTER_CHIP_SLAVE.enqueueDeviceJoin(
                device->ieee,
                device->shortAddr,
                device->endpoint,
                device->manufacturer,
                device->model
            );
        }
    );
    zigbeeCoordinator->setLightStateHandler(
        [](const char *message, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr) {
            INTER_CHIP_SLAVE.enqueueAttrReport(message, ieee, endpoint, shortAddr);
        }
    );
    zigbeeCoordinator->attachLibraryCallbacks(
        [](bool on, uint8_t endpoint, esp_zb_zcl_addr_t source) {
            if (zigbeeCoordinator != nullptr) {
                zigbeeCoordinator->handleLightStateWithSource(on, endpoint, source);
            }
        }
    );
}

static void onSlaveSettings(uint8_t channel, uint8_t permitJoinSec, uint32_t unixSec) {
    (void)unixSec;
    createSlaveCoordinator();
    if (slaveZigbeeStarted || slaveZigbeeStarting) {
        LOGGER.info("Settings again; coordinator already started");
        return;
    }
    slaveZigbeeStarting = true;
    startZigbeeOnOwnTask(channel, permitJoinSec);
}

static bool onSlavePermitJoin(uint8_t seconds) {
    if (zigbeeCoordinator == nullptr || !zigbeeCoordinator->isStarted()) {
        return false;
    }
    if (seconds == 0) {
        zigbeeCoordinator->closeJoin();
        return true;
    }
    zigbeeCoordinator->permitJoin(seconds);
    return true;
}

static void onSlaveDeviceSync(uint8_t flags, const DeviceTopicEntry *entry) {
    if ((flags & SPI_DEVICE_SYNC_RESET) != 0) {
        LOGGER.warning("Host cannot replace the full device store");
        return;
    }
    DeviceTopicMap *storeMap = deviceStore != nullptr ? deviceStore->deviceMap() : nullptr;
    if ((flags & SPI_DEVICE_SYNC_DELETE) != 0 && entry != nullptr) {
        if (zigbeeCoordinator != nullptr) {
            zigbeeCoordinator->removeRegisteredDevice(entry->ieee);
        } else if (storeMap != nullptr) {
            storeMap->removeByIeee(entry->ieee);
        }
        persistSlaveDeviceStore(true);
    } else if ((flags & SPI_DEVICE_SYNC_ENTRY) != 0 && entry != nullptr) {
        if (zigbeeCoordinator != nullptr) {
            zigbeeCoordinator->upsertRegisteredDevice(entry);
        } else if (storeMap != nullptr) {
            storeMap->upsert(
                entry->ieee,
                entry->friendlyName,
                entry->stateTopic,
                entry->commandTopic,
                entry->availabilityTopic,
                entry->channelCount
            );
        }
        persistSlaveDeviceStore(false);
    }
    if (zigbeeCoordinator != nullptr) {
        zigbeeCoordinator->markRegistryReady();
    }
}

static void onSlaveOnOff(const uint8_t ieee[8], const char *command, uint8_t endpoint) {
    if (zigbeeCoordinator == nullptr) {
        return;
    }
    zigbeeCoordinator->controlOnOff(ieee, command, endpoint);
}

static void setupHost() {
    INTER_CHIP_HOST.resetSlaveSynchronous();
    STATUS_RGB.setBootHeld(true);
    LOGGER.setRoleLabel("host");
    LOGGER.setStoreRing(true);
    {
        SpiFrame fixture;
        fixture.cmd = SpiCmdPing;
        fixture.seq = 7;
        fixture.length = 3;
        fixture.payload[0] = 0x11;
        fixture.payload[1] = 0x22;
        fixture.payload[2] = 0x33;
        uint8_t encoded[SPI_MAX_FRAME];
        const size_t encodedLen = spiEncodeFrame(fixture, encoded, sizeof(encoded));
        SpiFrame decoded;
        if (encodedLen == 0 || !spiDecodeFrame(encoded, encodedLen, decoded) || decoded.cmd != SpiCmdPing
            || decoded.seq != 7 || decoded.length != 3 || decoded.payload[1] != 0x22) {
            LOGGER.error("SPI frame fixture failed");
        } else {
            LOGGER.info("SPI frame fixture ok");
        }
    }
    LOGGER.info("role=host");
    LOGGER.info("z2m-gateway " FIRMWARE_VERSION);
    LOGGER.info("USB CDC on native ESP32-C6 port (not USB-OTG host)");
    LOGGER.info("Reset " + String((int)esp_reset_reason()));
    WiFi.onEvent(onWiFiArduinoEvent);

    pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
    delay(20);
    const bool forceAp = digitalRead(PIN_BOOT_BUTTON) == LOW;
    if (forceAp) {
        LOGGER.warning("BOOT held LOW — forcing SoftAP");
    }

    if (!LittleFS.begin(true)) {
        LOGGER.error("LittleFS mount failed");
    }

    settingsManager = new SettingsManager();
    topicMap = settingsManager->deviceMap();
    foundDevices = new FoundDeviceList();
    wifiController = new WiFiController(settingsManager, forceAp);
    mqttClient = new MqttClient(settingsManager, topicMap);
    serialCli = new SerialCli(settingsManager);
    webConsole = new WebConsole(settingsManager);
    webConsole->setDeviceServices(
        foundDevices,
        startDeviceSearch,
        stopDeviceSearch,
        onDeviceUpserted,
        onDeviceRemoved
    );
    webConsole->setHardwareApplyHandler([](uint32_t speedHz) { INTER_CHIP_HOST.setClockHz(speedHz); });
    webConsole->setDeviceOnlineHandler([](const uint8_t ieee[8]) {
        return ZIGBEE_SPI_PROXY.isOnline(ieee);
    });
    webConsole->setDevicesFileHandler([]() {
        ZIGBEE_SPI_PROXY.requestDevicesFile();
        return ZIGBEE_SPI_PROXY.devicesFileJson();
    });
    webConsole->begin();
    wifiController->setInterfaceReadyHandler([]() {
        if (webConsole != nullptr) {
            webConsole->rebind();
        }
    });

    mqttClient->setMessageHandler(onMqttLogicalMessage);
    mqttClient->begin(onMqttRawMessage);
    ZIGBEE_SPI_PROXY.begin();
    ZIGBEE_SPI_PROXY.setLightStateHandler(onLightState);
    ZIGBEE_SPI_PROXY.setRegistryPullDoneHandler([]() {
        if (mqttClient != nullptr && topicMap != nullptr) {
            mqttClient->subscribeDeviceCommands();
        }
    });

    GlobalSettings *settings = settingsManager->getSettings();
    INTER_CHIP_HOST.setSettingsSource(settings->zigbee.channel, settings->zigbee.permitJoinOnBootSec);
    INTER_CHIP_HOST.setClockHz(settingsManager->spiSpeedHz());
    INTER_CHIP_HOST.setEventHandler(onHostSpiEvent);
    INTER_CHIP_HOST.begin();
    runDeviceRegistryFixtures();
    LOGGER.info("Host SPI ready; Zigbee radio stays on the slave");
    LOGGER.info("USB CLI ready. Type help");
}

static void setupSlave() {
    STATUS_RGB.setReadyGreen(true);
    STATUS_RGB.setBootHeld(true);
    LOGGER.setRoleLabel("slave");
    LOGGER.setStoreRing(false);
    INTER_CHIP_SLAVE.setSettingsHandler(onSlaveSettings);
    INTER_CHIP_SLAVE.setPermitJoinHandler(onSlavePermitJoin);
    INTER_CHIP_SLAVE.setOnOffHandler(onSlaveOnOff);
    INTER_CHIP_SLAVE.setDeviceSyncHandler(onSlaveDeviceSync);
    deviceStore = new DeviceStore();
    deviceStore->begin();
    createSlaveCoordinator();
    if (zigbeeCoordinator != nullptr) {
        zigbeeCoordinator->setRegisteredMap(deviceStore->deviceMap());
        if (deviceStore->deviceMap()->usedCount() > 0) {
            zigbeeCoordinator->markRegistryReady();
        }
    }
    INTER_CHIP_SLAVE.setDeviceMapSource(deviceStore->deviceMap());
    INTER_CHIP_SLAVE.setDevicesFileSource([]() {
        return deviceStore != nullptr ? deviceStore->readFileText() : String("[]");
    });
    INTER_CHIP_SLAVE.begin();
    LOGGER.setLineHook(interChipSlaveLogHook);
    LOGGER.info("role=slave");
    LOGGER.info("z2m-gateway " FIRMWARE_VERSION " slave");
    LOGGER.info("Reset " + String((int)esp_reset_reason()));
    LOGGER.info("Device store on slave LittleFS; no Wi-Fi on slave");
}

void setup() {
    STATUS_RGB.begin();
    Serial.begin(115200);
    Serial.setTxTimeoutMs(20);
    delay(400);
    boardRole = readBoardRole();
    if (boardRole == BoardRoleHost) {
        setupHost();
    } else {
        setupSlave();
    }
}

void loop() {
    if (boardRole == BoardRoleHost) {
        settingsManager->handlePendingRestart(400);
        wifiController->update();
        maybeStartNtp();
        mqttClient->dispatch(wifiController->isStaConnected());
        serialCli->dispatch();
        ZIGBEE_SPI_PROXY.pumpRegistrySync();
        if (!hostPreparationLatched && wifiController->hasUsableInterface() && INTER_CHIP_HOST.isNormal()) {
            hostPreparationLatched = true;
            STATUS_RGB.setBootHeld(false);
        }
        STATUS_RGB.service();
        delay(5);
        return;
    }

    if (zigbeeCoordinator != nullptr && zigbeeCoordinator->isStarted()) {
        STATUS_RGB.setBootHeld(false);
    }
    STATUS_RGB.service();
    INTER_CHIP_SLAVE.applyDeferredSettings();
    INTER_CHIP_SLAVE.applyDeferredRadioCommands();
    INTER_CHIP_SLAVE.pumpDeviceDump();
    if (deviceStore != nullptr) {
        deviceStore->persistIfDue();
    }
    INTER_CHIP_SLAVE.pumpDevicesFileDump();
    if (zigbeeCoordinator != nullptr) {
        zigbeeCoordinator->dispatch();
    }
    delay(5);
}
