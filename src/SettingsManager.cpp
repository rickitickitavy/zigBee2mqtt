#include "SettingsManager.h"
#include "Logger.h"
#include "Defines.h"
#include "JsonField.h"

#include <EEPROM.h>
#include <stdlib.h>
#include <string.h>
#include <WiFi.h>

SettingsManager::SettingsManager() : topicMap(deviceSlots) {
    memset(deviceSlots, 0, sizeof(deviceSlots));
    EEPROM.begin(4096);
    LOGGER.info("Load settings...");

    char marker[4];
    for (int i = 0; i < 4; i++) {
        marker[i] = (char)EEPROM.read(i);
    }
    const bool haveMarker = marker[0] == (char)GLOBAL_SETTINGS_MARKER_0
        && marker[1] == (char)GLOBAL_SETTINGS_MARKER_1
        && marker[2] == (char)GLOBAL_SETTINGS_MARKER_2
        && marker[3] == (char)GLOBAL_SETTINGS_MARKER_3;
    if (!haveMarker) {
        LOGGER.warning("Settings uninitialized; writing defaults");
        applyDefaults();
        saveMain(true);
        memcpy(&committedMain, &settings, sizeof(settings));
        logSettings();
        return;
    }

    const unsigned char storedVersion = (unsigned char)EEPROM.read(offsetof(GlobalSettings, version));
    LOGGER.info("Settings loaded. Version " + String(storedVersion));
    if (storedVersion == 4) {
        upgradeLegacyMainFromEeprom();
        saveMain(false);
    } else if (storedVersion == GLOBAL_CURRENT_SETTINGS_VERSION) {
        readSettings();
    } else {
        applyDefaults();
        saveMain(false);
    }

    clampMqttClientTimeout(settings.mqtt.clientTimeoutMs);
    clampZigbeeChannel(settings.zigbee.channel);
    if (settings.wifi.mode != WifiSettingsModeAp && settings.wifi.mode != WifiSettingsModeSta) {
        settings.wifi.mode = WifiSettingsModeAp;
    }
    {
        uint8_t rawOtg = 0;
        memcpy(&rawOtg, &settings.wifi.otgEnabled, sizeof(rawOtg));
        if (rawOtg > 1) {
            settings.wifi.otgEnabled = false;
        }
    }
    {
        uint8_t rawMqtt = 0;
        memcpy(&rawMqtt, &settings.mqtt.enabled, sizeof(rawMqtt));
        if (rawMqtt > 1) {
            settings.mqtt.enabled = true;
        }
    }
    memcpy(&committedMain, &settings, sizeof(settings));
    logSettings();
}

void SettingsManager::applyDefaults() {
    memset(&settings, 0, sizeof(settings));
    memset(deviceSlots, 0, sizeof(deviceSlots));
    settings.initMarker[0] = GLOBAL_SETTINGS_MARKER_0;
    settings.initMarker[1] = GLOBAL_SETTINGS_MARKER_1;
    settings.initMarker[2] = GLOBAL_SETTINGS_MARKER_2;
    settings.initMarker[3] = GLOBAL_SETTINGS_MARKER_3;
    settings.version = GLOBAL_CURRENT_SETTINGS_VERSION;

    resetWiFi();

    strncpy(settings.mqtt.server, DEFAULT_MQTT_SERVER, sizeof(settings.mqtt.server) - 1);
    settings.mqtt.port = DEFAULT_MQTT_PORT;
    settings.mqtt.reconnectIntervalMs = DEFAULT_MQTT_RECONNECT_MS;
    settings.mqtt.clientTimeoutMs = DEFAULT_MQTT_CLIENT_TIMEOUT_MS;
    settings.mqtt.enabled = true;
    strncpy(settings.mqtt.clientId, DEFAULT_MQTT_CLIENT_ID, sizeof(settings.mqtt.clientId) - 1);
    strncpy(settings.mqtt.baseTopic, DEFAULT_MQTT_BASE_TOPIC, sizeof(settings.mqtt.baseTopic) - 1);

    settings.zigbee.channel = DEFAULT_ZIGBEE_CHANNEL;
    settings.zigbee.permitJoinOnBootSec = DEFAULT_PERMIT_JOIN_SEC;
    setSpiSpeedHz(DEFAULT_SPI_SPEED_HZ);
}

void SettingsManager::upgradeLegacyMainFromEeprom() {
    memset(&settings, 0, sizeof(settings));
    uint8_t *mainBytes = (uint8_t *)&settings;
    for (size_t i = 0; i < kSettingsMainUsed; i++) {
        mainBytes[i] = EEPROM.read(i);
    }
    settings.version = GLOBAL_CURRENT_SETTINGS_VERSION;
    memset(settings.alignPad, 0, sizeof(settings.alignPad));
    memset(settings.reserved, 0, sizeof(settings.reserved));
    memset(deviceSlots, 0, sizeof(deviceSlots));
    LOGGER.info("Legacy EEPROM main upgraded; devices stay on the slave");
}

bool SettingsManager::mainEqualsCommitted() const {
    return memcmp(&settings, &committedMain, sizeof(settings)) == 0;
}

void SettingsManager::resetWiFi() {
    strncpy(settings.wifi.bssid, WIFI_DEFAULT_BSSID, sizeof(settings.wifi.bssid) - 1);
    strncpy(settings.wifi.password, WIFI_DEFAULT_PASSWORD, sizeof(settings.wifi.password) - 1);
    strncpy(settings.wifi.deviceName, WIFI_DEFAULT_DEVICE_NAME, sizeof(settings.wifi.deviceName) - 1);
    strncpy(settings.wifi.apIp, WIFI_DEFAULT_AP_IP, sizeof(settings.wifi.apIp) - 1);
    settings.wifi.mode = WifiSettingsModeAp;
    settings.wifi.otgEnabled = false;
}

void SettingsManager::clampMqttClientTimeout(int &timeoutMs) {
    if (timeoutMs < MQTT_CLIENT_TIMEOUT_MS_MIN || timeoutMs > MQTT_CLIENT_TIMEOUT_MS_MAX) {
        timeoutMs = DEFAULT_MQTT_CLIENT_TIMEOUT_MS;
    }
}

uint32_t SettingsManager::clampSpiSpeedHz(uint32_t speedHz) {
    if (speedHz < SPI_SPEED_HZ_MIN || speedHz > SPI_SPEED_HZ_MAX) {
        return DEFAULT_SPI_SPEED_HZ;
    }
    return speedHz;
}

uint32_t SettingsManager::spiSpeedHz() const {
    uint32_t speedHz = 0;
    memcpy(&speedHz, settings.reserved, sizeof(speedHz));
    return clampSpiSpeedHz(speedHz);
}

void SettingsManager::setSpiSpeedHz(uint32_t speedHz) {
    const uint32_t clamped = clampSpiSpeedHz(speedHz);
    memcpy(settings.reserved, &clamped, sizeof(clamped));
}

void SettingsManager::clampZigbeeChannel(uint8_t &channel) {
    if (channel < 11 || channel > 26) {
        channel = DEFAULT_ZIGBEE_CHANNEL;
    }
}

void SettingsManager::readSettings(GlobalSettings *destination) {
    uint8_t *buffer = (uint8_t *)destination;
    LOGGER.info("Loading " + String((int)sizeof(GlobalSettings)) + " main bytes");
    for (size_t i = 0; i < sizeof(GlobalSettings); i++) {
        buffer[i] = EEPROM.read(i);
    }
}

void SettingsManager::readSettings() {
    readSettings(&settings);
}

void SettingsManager::createEmptyDeviceFile() {
}

void SettingsManager::parseDevicesJson(const String &json) {
    topicMap.replaceFromJson(json);
}

void SettingsManager::loadDeviceFile() {
    memset(deviceSlots, 0, sizeof(deviceSlots));
    LOGGER.info("Host device list waits for slave pull");
}

String SettingsManager::devicesJsonFile() {
    return topicMap.listJson();
}

bool SettingsManager::saveDevicesJson() {
    return true;
}

void SettingsManager::writeEepromDirty(
    const uint8_t *nextImage,
    const uint8_t *previousImage,
    size_t length
) {
    for (size_t addr = 0; addr < length; addr++) {
        if (nextImage[addr] != previousImage[addr]) {
            EEPROM.write(addr, nextImage[addr]);
        }
    }
}

void SettingsManager::requestRestart() {
    pendingRestart = true;
    restartRequestedMs = millis();
    LOGGER.warning("Restart scheduled");
}

void SettingsManager::saveMain(bool restart) {
    LOGGER.info("Saving main settings...");
    writeEepromDirty((const uint8_t *)&settings, (const uint8_t *)&committedMain, sizeof(settings));
    EEPROM.commit();
    memcpy(&committedMain, &settings, sizeof(settings));
    if (restart) {
        requestRestart();
    }
}

void SettingsManager::saveSetting(bool restart) {
    saveMain(restart);
}

bool SettingsManager::saveDeviceSlot(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= DEVICE_MAP_SLOTS) {
        return false;
    }
    return saveDevicesJson();
}

bool SettingsManager::handlePendingRestart(unsigned long delayMs) {
    if (!pendingRestart) {
        return false;
    }
    if ((millis() - restartRequestedMs) < delayMs) {
        return false;
    }
    LOGGER.warning("RESTARTING...");
    WiFi.persistent(false);
    WiFi.disconnect(false, false, 2000);
    delay(150);
    WiFi.mode(WIFI_OFF);
    delay(400);
    ESP.restart();
    return true;
}

GlobalSettings *SettingsManager::getSettings() {
    return &settings;
}

DeviceTopicMap *SettingsManager::deviceMap() {
    return &topicMap;
}

DeviceTopicEntry *SettingsManager::devices() {
    return deviceSlots;
}

void SettingsManager::logSettings() {
    LOGGER.info("----- SETTINGS ----");
    LOGGER.info("  wifi bssid: " + String(settings.wifi.bssid));
    LOGGER.info("  deviceName: " + String(settings.wifi.deviceName));
    LOGGER.info(
        String("  MODE ") + (settings.wifi.mode == WifiSettingsModeSta ? "STA" : "AP")
    );
    LOGGER.info("  OTG_ENABLED: " + String(settings.wifi.otgEnabled ? "true" : "false"));
    LOGGER.info("  AP IP: " + String(settings.wifi.apIp));
    LOGGER.info("  mqtt enabled: " + String(settings.mqtt.enabled ? "true" : "false"));
    LOGGER.info("  mqtt server: " + String(settings.mqtt.server) + ":" + String(settings.mqtt.port));
    LOGGER.info("  mqtt base: " + String(settings.mqtt.baseTopic));
    LOGGER.info("  zigbee channel: " + String(settings.zigbee.channel));
    LOGGER.info("  zigbee permitJoinOnBootSec: " + String(settings.zigbee.permitJoinOnBootSec));
    LOGGER.info("  devices used: " + String(topicMap.usedCount()) + "/" + String(DEVICE_MAP_SLOTS));
    LOGGER.info("  spi speed Hz: " + String((unsigned long)spiSpeedHz()));
}
