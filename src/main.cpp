#include <Arduino.h>
#include <LittleFS.h>

#include "pins.h"
#include "Defines.h"
#include "Logger.h"
#include "SettingsManager.h"
#include "DeviceTopicMap.h"
#include "WiFiController.h"
#include "MqttClient.h"
#include "ZigbeeCoordinator.h"
#include "SerialCli.h"
#include "JsonField.h"

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator mode is not selected (ZIGBEE_MODE_ZCZR)"
#endif

static SettingsManager *settingsManager = nullptr;
static DeviceTopicMap *topicMap = nullptr;
static WiFiController *wifiController = nullptr;
static MqttClient *mqttClient = nullptr;
static ZigbeeCoordinator *zigbeeCoordinator = nullptr;
static SerialCli *serialCli = nullptr;

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
    mqttClient->publishDevices(zigbeeCoordinator->devicesJson(topicMap));
    LOGGER.info("Configured topics for " + ieeeText);
}

static void onMqttLogicalMessage(const char *topic, const char *payload) {
    if (strcmp(topic, mqttClient->permitJoinTopic().c_str()) == 0) {
        String body = String(payload);
        body.trim();
        body.toLowerCase();
        if (body == "off" || body == "0" || body == "false" || body == "close") {
            zigbeeCoordinator->closeJoin();
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
            zigbeeCoordinator->closeJoin();
        } else {
            zigbeeCoordinator->permitJoin((uint8_t)constrain(seconds, 1, 254));
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
        zigbeeCoordinator->controlOnOff(entry->ieee, payload);
    }
}

static void onLightState(bool on, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr) {
    (void)endpoint;
    (void)shortAddr;
    DeviceTopicEntry *entry = topicMap->findByIeee(ieee);
    if (entry == nullptr) {
        LOGGER.info("Unmapped device state; assign topics via map or MQTT bridge/config/device");
        mqttClient->publishDevices(zigbeeCoordinator->devicesJson(topicMap));
        return;
    }
    mqttClient->publishDeviceState(entry, on);
}

static void onZigbeeLightSource(bool on, uint8_t endpoint, esp_zb_zcl_addr_t source) {
    if (zigbeeCoordinator != nullptr) {
        zigbeeCoordinator->handleLightStateWithSource(on, endpoint, source);
    }
}

void setup() {
    Serial.begin(115200);
    delay(400);
    LOGGER.info("z2m-gateway " FIRMWARE_VERSION);
    LOGGER.info("USB CDC on native ESP32-C6 port (not USB-OTG host)");

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
    zigbeeCoordinator = new ZigbeeCoordinator();
    serialCli = new SerialCli(settingsManager, topicMap, zigbeeCoordinator, mqttClient);

    mqttClient->setMessageHandler(onMqttLogicalMessage);
    mqttClient->begin(onMqttRawMessage);

    zigbeeCoordinator->setLightStateHandler(onLightState);
    zigbeeCoordinator->attachLibraryCallbacks(onZigbeeLightSource);

    GlobalSettings *settings = settingsManager->getSettings();
    if (!zigbeeCoordinator->begin(settings->zigbeeChannel, settings->permitJoinOnBootSec)) {
        LOGGER.error("Zigbee start failed — check ZCZR partition / erase flash");
    }

    LOGGER.info("USB CLI ready. Type help");
}

void loop() {
    settingsManager->handlePendingRestart(400);
    wifiController->update();
    mqttClient->dispatch(wifiController->isStaConnected());
    zigbeeCoordinator->dispatch();
    serialCli->dispatch();
    delay(10);
}
