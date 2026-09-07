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
#include "ZigbeeSpiProxy.h"

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
static bool ntpStarted = false;
static bool slaveZigbeeStarted = false;

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

    uint8_t ieee[8];
    if (!topicMap->parseIeee(ieeeText.c_str(), ieee)) {
        LOGGER.error("Bad IEEE in config/device");
        return;
    }
    if (topicMap->upsert(
            ieee,
            friendlyName.c_str(),
            stateTopic.c_str(),
            commandTopic.c_str(),
            availability.c_str()
        )
        == nullptr) {
        LOGGER.error("Device map full");
        return;
    }
    settingsManager->saveSetting(false);
    mqttClient->subscribeDeviceCommands();
    mqttClient->publishDevices(ZIGBEE_SPI_PROXY.devicesJson(topicMap));
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

    DeviceTopicEntry *entry = topicMap->findByCommandTopic(topic);
    if (entry != nullptr) {
        ZIGBEE_SPI_PROXY.controlOnOff(entry->ieee, payload);
    }
}

static void onLightState(bool on, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr) {
    (void)endpoint;
    (void)shortAddr;
    DeviceTopicEntry *entry = topicMap->findByIeee(ieee);
    if (entry == nullptr) {
        LOGGER.info("Unmapped device state; assign topics via map or MQTT bridge/config/device");
        mqttClient->publishDevices(ZIGBEE_SPI_PROXY.devicesJson(topicMap));
        return;
    }
    mqttClient->publishDeviceState(entry, on);
}

static void onHostSpiEvent(const SpiFrame &frame) {
    ZIGBEE_SPI_PROXY.onSpiEvent(frame);
    if (frame.cmd == SpiEvtDeviceJoin && mqttClient != nullptr && topicMap != nullptr) {
        mqttClient->publishDevices(ZIGBEE_SPI_PROXY.devicesJson(topicMap));
    }
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

static void onSlaveSettings(uint8_t channel, uint8_t permitJoinSec, uint32_t unixSec) {
    (void)unixSec;
    if (slaveZigbeeStarted) {
        LOGGER.info("Settings again; coordinator already started");
        return;
    }
    if (zigbeeCoordinator == nullptr) {
        zigbeeCoordinator = new ZigbeeCoordinator();
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
            [](bool on, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr) {
                INTER_CHIP_SLAVE.enqueueAttrReport(on, ieee, endpoint, shortAddr);
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
    if (!zigbeeCoordinator->begin(channel, permitJoinSec)) {
        LOGGER.error("Zigbee start failed — check ZCZR partition / erase flash");
        return;
    }
    slaveZigbeeStarted = true;
}

static void onSlavePermitJoin(uint8_t seconds) {
    if (zigbeeCoordinator == nullptr) {
        return;
    }
    if (seconds == 0) {
        zigbeeCoordinator->closeJoin();
        return;
    }
    zigbeeCoordinator->permitJoin(seconds);
}

static void onSlaveOnOff(const uint8_t ieee[8], uint8_t action) {
    if (zigbeeCoordinator == nullptr) {
        return;
    }
    const char *command = "off";
    if (action == 1) {
        command = "on";
    } else if (action == 2) {
        command = "toggle";
    }
    zigbeeCoordinator->controlOnOff(ieee, command);
}

static void setupHost() {
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
    topicMap = new DeviceTopicMap(settingsManager->getSettings());
    wifiController = new WiFiController(settingsManager, forceAp);
    mqttClient = new MqttClient(settingsManager, topicMap);
    serialCli = new SerialCli(settingsManager);
    webConsole = new WebConsole(settingsManager);
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

    GlobalSettings *settings = settingsManager->getSettings();
    INTER_CHIP_HOST.setSettingsSource(settings->zigbee.channel, settings->zigbee.permitJoinOnBootSec);
    INTER_CHIP_HOST.setEventHandler(onHostSpiEvent);
    INTER_CHIP_HOST.begin();
    LOGGER.info("Host SPI ready; Zigbee radio stays on the slave");
    LOGGER.info("USB CLI ready. Type help");
}

static void setupSlave() {
    LOGGER.setRoleLabel("slave");
    LOGGER.setStoreRing(false);
    LOGGER.setLineHook(interChipSlaveLogHook);
    LOGGER.info("role=slave");
    LOGGER.info("z2m-gateway " FIRMWARE_VERSION " slave");
    LOGGER.info("Reset " + String((int)esp_reset_reason()));
    LOGGER.info("No Wi-Fi / settings store on slave");

    INTER_CHIP_SLAVE.setSettingsHandler(onSlaveSettings);
    INTER_CHIP_SLAVE.setPermitJoinHandler(onSlavePermitJoin);
    INTER_CHIP_SLAVE.setOnOffHandler(onSlaveOnOff);
    INTER_CHIP_SLAVE.begin();
}

void setup() {
    Serial.begin(115200);
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
        delay(5);
        return;
    }

    INTER_CHIP_SLAVE.applyDeferredSettings();
    if (zigbeeCoordinator != nullptr) {
        zigbeeCoordinator->dispatch();
    }
    delay(5);
}
