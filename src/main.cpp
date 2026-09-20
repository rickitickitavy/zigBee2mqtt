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
#include "ZigbeeDeviceType.h"
#include "ZigbeeCluster.h"

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
static bool persistHostDeviceList();
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
    bool parsedFullControl = false;
    if (extractJsonBool(payload, "fullControl", parsedFullControl)) {
        entry->fullControl = parsedFullControl ? 1 : 0;
    }
    mqttClient->subscribeDeviceCommands();
    persistHostDeviceList();
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
    if (entry->fullControl) {
        DeviceTopicMap::ZclWriteFields fields;
        if (DeviceTopicMap::parseFullControlBody(action.c_str(), commandEndpoint, &fields) && fields.parsedAny) {
            ZIGBEE_SPI_PROXY.writeAttribute(
                entry->ieee,
                fields.endpoint,
                fields.clusterId,
                fields.attributeId,
                fields.dataType,
                fields.attributeValue
            );
            return;
        }
    }
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
        if (foundDevices->noteJoin(frame, topicMap)) {
            persistHostDeviceList();
        }
    }
    if (frame.cmd == SpiEvtAttrReport && frame.length >= SPI_ATTR_REPORT_MESSAGE_OFFSET + 1
        && foundDevices != nullptr && topicMap != nullptr) {
        uint8_t ieee[8];
        memcpy(ieee, frame.payload, 8);
        const uint8_t endpoint = frame.payload[8];
        const uint16_t shortAddr = (uint16_t)frame.payload[9] | ((uint16_t)frame.payload[10] << 8);
        const char *reportMessage = (const char *)frame.payload + SPI_ATTR_REPORT_MESSAGE_OFFSET;
        foundDevices->noteIdentity(
            ieee,
            shortAddr,
            endpoint,
            "",
            "",
            topicMap,
            zigbeeDeviceTypeFromAttrMessage(reportMessage)
        );
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

static bool persistHostDeviceList() {
    if (settingsManager == nullptr) {
        return false;
    }
    return settingsManager->saveDevicesJson();
}

static bool hostLastDeviceRssi(const uint8_t ieee[8], int8_t *rssiDbm) {
    return ZIGBEE_SPI_PROXY.lastRssiDbm(ieee, rssiDbm);
}

static String hostGatewayStatusJson() {
    int registeredCount = topicMap != nullptr ? topicMap->usedCount() : 0;
    int onlineCount = 0;
    if (topicMap != nullptr) {
        int slotIndex = topicMap->nextUsedIndex(0);
        while (slotIndex >= 0) {
            DeviceTopicEntry *entry = topicMap->slotAt(slotIndex);
            if (entry != nullptr && ZIGBEE_SPI_PROXY.isOnline(entry->ieee)) {
                onlineCount++;
            }
            slotIndex = topicMap->nextUsedIndex(slotIndex + 1);
        }
    }
    String json = "{\"devices\":";
    json += String(registeredCount);
    json += ",\"online\":";
    json += String(onlineCount);
    json += ",\"packetsRx\":";
    json += String((unsigned long)ZIGBEE_SPI_PROXY.packetsReceived());
    json += ",\"packetsTx\":";
    json += String((unsigned long)ZIGBEE_SPI_PROXY.packetsSent());
    json += ",\"version\":\"";
    json += FIRMWARE_VERSION;
    json += "\",\"pairingActive\":";
    json += ZIGBEE_SPI_PROXY.pairingActive() ? "true" : "false";
    json += "}";
    return json;
}

static bool onDeviceUpserted(const DeviceTopicEntry *entry) {
    if (mqttClient != nullptr) {
        mqttClient->subscribeDeviceCommands();
    }
    persistHostDeviceList();
    return ZIGBEE_SPI_PROXY.enqueueDeviceUpsert(entry);
}

static bool onDeviceRemoved(const uint8_t ieee[8]) {
    if (mqttClient != nullptr) {
        mqttClient->subscribeDeviceCommands();
    }
    persistHostDeviceList();
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

    const uint16_t onOffOnlyClusters[] = { kZigbeeClusterOnOff };
    const uint16_t iasWithOnOffClusters[] = { kZigbeeClusterOnOff, kZigbeeClusterIasZone };
    const uint16_t tempOnlyClusters[] = { 0x0402 };
    const uint16_t coveringClusters[] = { kZigbeeClusterWindowCovering };
    const uint16_t leakDetectorClusters[] = {
        kZigbeeClusterPowerConfig,
        kZigbeeClusterIasZone
    };
    const uint16_t batteryOnlyClusters[] = { kZigbeeClusterPowerConfig };
    const uint16_t coveringOutClusters[] = { kZigbeeClusterWindowCovering };
    if (classifyZigbeeDeviceTypeFromInClusters(onOffOnlyClusters, 1) != ZigbeeDeviceTypeOnOff
        || classifyZigbeeDeviceTypeFromInClusters(iasWithOnOffClusters, 2) != ZigbeeDeviceTypeIasZone
        || classifyZigbeeDeviceTypeFromInClusters(leakDetectorClusters, 2) != ZigbeeDeviceTypeIasZone
        || classifyZigbeeDeviceTypeFromInClusters(batteryOnlyClusters, 1) != ZigbeeDeviceTypeUnknown
        || classifyZigbeeDeviceTypeFromInClusters(tempOnlyClusters, 1) != ZigbeeDeviceTypeUnknown
        || classifyZigbeeDeviceTypeFromInClusters(coveringClusters, 1) != ZigbeeDeviceTypeWindowCovering
        || classifyZigbeeDeviceTypeFromClusterList(coveringOutClusters, 0, 1) != ZigbeeDeviceTypeWindowCovering
        || zigbeeDeviceTypeFromCluster(kZigbeeClusterWindowCovering) != ZigbeeDeviceTypeWindowCovering
        || zigbeeDeviceTypeFromAttrMessage("Window covering cl=0x0102,attr=0x0008,val=0x0")
            != ZigbeeDeviceTypeWindowCovering
        || strcmp(zigbeeClusterName(kZigbeeClusterPowerConfig), "Power configuration") != 0
        || strcmp(zigbeeClusterName(kZigbeeClusterIasZone), "IAS Zone") != 0
        || zigbeeBatteryPercentageFromRemaining(0x86) != 67
        || !zigbeeJoinShouldEmit(kZigbeeDeviceTypeNeverEmitted, ZigbeeDeviceTypeUnknown)
        || !zigbeeJoinShouldEmit(ZigbeeDeviceTypeUnknown, ZigbeeDeviceTypeOnOff)
        || zigbeeJoinShouldEmit(ZigbeeDeviceTypeOnOff, ZigbeeDeviceTypeOnOff)) {
        LOGGER.error("Device type classifier fixture failed");
    } else {
        LOGGER.info("Device type classifier fixture ok");
    }

    uint8_t packedJoin[SPI_DEVICE_JOIN_LEN];
    uint8_t packIeee[8];
    memset(packIeee, 0, sizeof(packIeee));
    packIeee[0] = 0xCC;
    if (!spiPackDeviceJoin(packedJoin, sizeof(packedJoin), packIeee, 0x1234, 1, "Acme", "Plug", ZigbeeDeviceTypeOnOff)
        || packedJoin[0] != 0xCC
        || packedJoin[SPI_DEVICE_JOIN_MANUFACTURER_OFFSET] != 'A'
        || packedJoin[SPI_DEVICE_JOIN_MODEL_OFFSET] != 'P'
        || packedJoin[SPI_DEVICE_JOIN_TYPE_OFFSET] != ZigbeeDeviceTypeOnOff
        || spiDeviceJoinType(packedJoin, SPI_DEVICE_JOIN_MIN_LEN) != ZigbeeDeviceTypeUnknown) {
        LOGGER.error("Device join pack fixture failed");
    } else {
        LOGGER.info("Device join pack fixture ok");
    }

    foundDevices->clear();
    SpiFrame joinFrame;
    memset(&joinFrame, 0, sizeof(joinFrame));
    joinFrame.cmd = SpiEvtDeviceJoin;
    joinFrame.length = SPI_DEVICE_JOIN_MIN_LEN;
    joinFrame.payload[0] = 0xAA;
    joinFrame.payload[SPI_DEVICE_JOIN_NWK_OFFSET] = 0x34;
    joinFrame.payload[SPI_DEVICE_JOIN_NWK_OFFSET + 1] = 0x12;
    joinFrame.payload[SPI_DEVICE_JOIN_ENDPOINT_OFFSET] = 1;
    strncpy((char *)joinFrame.payload + SPI_DEVICE_JOIN_MANUFACTURER_OFFSET, "Acme", 31);
    strncpy((char *)joinFrame.payload + SPI_DEVICE_JOIN_MODEL_OFFSET, "Plug", 31);
    foundDevices->noteJoin(joinFrame, topicMap);
    String foundJson = foundDevices->listJson(topicMap);
    if (foundJson.indexOf("AA") < 0 || foundJson.indexOf("\"type\":\"unknown\"") < 0) {
        LOGGER.error("Found-join fixture failed");
    } else {
        LOGGER.info("Found-join fixture ok");
    }

    foundDevices->clear();
    memcpy(joinFrame.payload, packedJoin, SPI_DEVICE_JOIN_LEN);
    joinFrame.length = SPI_DEVICE_JOIN_LEN;
    foundDevices->noteJoin(joinFrame, topicMap);
    foundJson = foundDevices->listJson(topicMap);
    if (foundJson.indexOf("\"type\":\"onOff\"") < 0) {
        LOGGER.error("Found type JSON fixture failed");
    } else {
        LOGGER.info("Found type JSON fixture ok");
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
    joinFrame.length = SPI_DEVICE_JOIN_MIN_LEN;
    foundDevices->noteJoin(joinFrame, topicMap);
    foundJson = foundDevices->listJson(topicMap);
    if (foundJson.indexOf("BB") >= 0) {
        LOGGER.error("Registered join leaked into found list");
    } else {
        LOGGER.info("Registered join skipped found list");
    }

    if (probe != nullptr) {
        probe->zigbeeType = ZigbeeDeviceTypeUnknown;
    }
    uint8_t coveringIeee[8];
    memset(coveringIeee, 0, sizeof(coveringIeee));
    coveringIeee[0] = 0xDD;
    DeviceTopicEntry *coveringEntry = topicMap->upsert(coveringIeee, "cover-fixture", "", "", "");
    if (coveringEntry != nullptr) {
        coveringEntry->zigbeeType = ZigbeeDeviceTypeUnknown;
        spiPackDeviceJoin(
            joinFrame.payload,
            sizeof(joinFrame.payload),
            coveringIeee,
            0x2222,
            1,
            "Acme",
            "Shade",
            ZigbeeDeviceTypeWindowCovering
        );
        joinFrame.length = SPI_DEVICE_JOIN_LEN;
        const bool filledType = foundDevices->noteJoin(joinFrame, topicMap);
        foundJson = foundDevices->listJson(topicMap);
        if (!filledType || coveringEntry->zigbeeType != ZigbeeDeviceTypeWindowCovering
            || foundJson.indexOf("DD") >= 0) {
            LOGGER.error("Registered unknown type fill fixture failed");
        } else {
            LOGGER.info("Registered unknown type fill fixture ok");
        }
        memset(coveringEntry, 0, sizeof(DeviceTopicEntry));
    }

    uint8_t persistIeee[8];
    memset(persistIeee, 0, sizeof(persistIeee));
    persistIeee[0] = 0xEE;
    DeviceTopicEntry *persistEntry = topicMap->upsert(persistIeee, "type-fixture", "", "", "");
    if (persistEntry != nullptr) {
        persistEntry->zigbeeType = ZigbeeDeviceTypeOnOff;
        String storedJson = topicMap->listJson();
        const uint8_t storedType = persistEntry->zigbeeType;
        persistEntry->zigbeeType = storedType != ZigbeeDeviceTypeUnknown
            ? storedType
            : ZigbeeDeviceTypeIasZone;
        if (persistEntry->zigbeeType != ZigbeeDeviceTypeOnOff || storedJson.indexOf("\"type\":\"onOff\"") < 0) {
            LOGGER.error("Device type persist fixture failed");
        } else {
            LOGGER.info("Device type persist fixture ok");
        }
        topicMap->replaceFromJson(
            "[{\"ieee\":\"00:00:00:00:00:00:00:EE\",\"name\":\"cover\",\"state\":\"z2m/cover/state\",\"type\":\"windowCovering\"}]"
        );
        DeviceTopicEntry *reloadedEntry = topicMap->findByIeee(persistIeee);
        if (reloadedEntry == nullptr || reloadedEntry->zigbeeType != ZigbeeDeviceTypeWindowCovering) {
            LOGGER.error("Window covering type JSON fixture failed");
        } else {
            LOGGER.info("Window covering type JSON fixture ok");
        }
        topicMap->replaceFromJson("[{\"ieee\":\"00:00:00:00:00:00:00:EE\",\"name\":\"legacy\"}]");
        DeviceTopicEntry *legacyEntry = topicMap->findByIeee(persistIeee);
        if (legacyEntry == nullptr || legacyEntry->zigbeeType != ZigbeeDeviceTypeUnknown) {
            LOGGER.error("Missing type JSON fixture failed");
        } else {
            LOGGER.info("Missing type JSON fixture ok");
        }
        memset(legacyEntry, 0, sizeof(DeviceTopicEntry));
    }

    DeviceTopicEntry syncSource;
    memset(&syncSource, 0, sizeof(syncSource));
    syncSource.used = 1;
    syncSource.ieee[0] = 0xAB;
    strncpy(syncSource.friendlyName, "sync-type", sizeof(syncSource.friendlyName) - 1);
    syncSource.channelCount = DEVICE_CHANNEL_COUNT_DEFAULT;
    syncSource.zigbeeType = ZigbeeDeviceTypeIasZone;
    uint8_t syncPacked[SPI_DEVICE_SYNC_ENTRY_LEN];
    const size_t syncPackedLen = DeviceTopicMap::packSyncPayload(
        syncPacked,
        sizeof(syncPacked),
        SPI_DEVICE_SYNC_ENTRY,
        &syncSource
    );
    DeviceTopicEntry syncUnpacked;
    uint8_t syncFlags = 0;
    if (syncPackedLen != SPI_DEVICE_SYNC_ENTRY_LEN
        || !DeviceTopicMap::unpackSyncPayload(syncPacked, (uint16_t)syncPackedLen, &syncFlags, &syncUnpacked)
        || syncUnpacked.zigbeeType != ZigbeeDeviceTypeIasZone) {
        LOGGER.error("Device type SPI sync fixture failed");
    } else {
        LOGGER.info("Device type SPI sync fixture ok");
    }

    if (createdProbe && probe != nullptr) {
        memset(probe, 0, sizeof(DeviceTopicEntry));
    }
    foundDevices->clear();

    LOGGER.info("Device map cap " + String(DEVICE_MAP_SLOTS) + " (overflow rejected)");
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
    zigbeeCoordinator->setJoinClosedHandler(
        []() { INTER_CHIP_SLAVE.enqueueJoinClosed(); }
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
                device->model,
                device->zigbeeType
            );
        }
    );
    zigbeeCoordinator->setLightStateHandler(
        [](const char *message, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr, int8_t rssiDbm) {
            INTER_CHIP_SLAVE.enqueueAttrReport(message, ieee, endpoint, shortAddr, rssiDbm);
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
            storeMap->upsertFromEntry(entry, false);
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

static void onSlaveWriteAttr(
    const uint8_t ieee[8],
    uint8_t endpoint,
    uint16_t clusterId,
    uint16_t attributeId,
    uint8_t dataType,
    uint32_t attributeValue
) {
    if (zigbeeCoordinator == nullptr) {
        return;
    }
    zigbeeCoordinator->writeAttribute(ieee, endpoint, clusterId, attributeId, dataType, attributeValue);
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
        DeviceTopicMap::ZclWriteFields fields;
        const bool parsedWrite = DeviceTopicMap::parseFullControlBody("cl=0x0102,val=0x1", 1, &fields);
        if (!parsedWrite || !fields.parsedAny || fields.clusterId != 0x0102 || fields.attributeId != 0
            || fields.attributeValue != 1 || fields.dataType != ZCL_ATTR_TYPE_U8 || fields.endpoint != 1) {
            LOGGER.error("Full-control parse fixture failed");
        } else {
            LOGGER.info("Full-control parse fixture ok");
        }
        uint8_t packedWrite[SPI_ZCL_WRITE_ATTR_LEN];
        const uint8_t fixtureIeee[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        if (!spiPackZclWriteAttr(
                packedWrite,
                sizeof(packedWrite),
                fixtureIeee,
                4,
                0x0006,
                0,
                ZCL_ATTR_TYPE_U8,
                1
            )
            || packedWrite[8] != 4) {
            LOGGER.error("Write-attr pack fixture failed");
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
    settingsManager->loadDeviceFile();
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
    webConsole->setDeviceRssiHandler(hostLastDeviceRssi);
    webConsole->setGatewayStatusHandler(hostGatewayStatusJson);
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
        persistHostDeviceList();
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
    INTER_CHIP_SLAVE.setWriteAttrHandler(onSlaveWriteAttr);
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
        if (hostPreparationLatched) {
            STATUS_RGB.setCritical(!INTER_CHIP_HOST.isLinkHealthy());
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
