#include "SettingsManager.h"
#include "Logger.h"
#include "Defines.h"

#include <EEPROM.h>
#include <string.h>

static_assert(sizeof(GlobalSettings) <= 4096, "GlobalSettings must fit in EEPROM");

SettingsManager::SettingsManager() {
    EEPROM.begin(4096);
    LOGGER.info("Load settings...");
    readSettings();

    if ((settings.initMarker[0] != GLOBAL_SETTINGS_MARKER_0)
        || (settings.initMarker[1] != GLOBAL_SETTINGS_MARKER_1)
        || (settings.initMarker[2] != GLOBAL_SETTINGS_MARKER_2)
        || (settings.initMarker[3] != GLOBAL_SETTINGS_MARKER_3)) {
        LOGGER.warning("Settings uninitialized; writing defaults");
        applyDefaults();
        saveSetting(true);
    } else {
        LOGGER.info("Settings loaded. Version " + String(settings.version));
        if (settings.version != GLOBAL_CURRENT_SETTINGS_VERSION) {
            applyDefaults();
            settings.version = GLOBAL_CURRENT_SETTINGS_VERSION;
            saveSetting(false);
        }
        clampMqttClientTimeout(settings.mqttClientTimeoutMs);
        clampZigbeeChannel(settings.zigbeeChannel);
        {
            uint8_t rawWifi = 0;
            memcpy(&rawWifi, &settings.network.wifiEnabled, sizeof(rawWifi));
            if (rawWifi > 1) {
                settings.network.wifiEnabled = false;
            }
        }
        {
            uint8_t rawMqtt = 0;
            memcpy(&rawMqtt, &settings.mqttEnabled, sizeof(rawMqtt));
            if (rawMqtt > 1) {
                settings.mqttEnabled = true;
            }
        }
    }

    logSettings();
}

void SettingsManager::applyDefaults() {
    memset(&settings, 0, sizeof(settings));
    settings.initMarker[0] = GLOBAL_SETTINGS_MARKER_0;
    settings.initMarker[1] = GLOBAL_SETTINGS_MARKER_1;
    settings.initMarker[2] = GLOBAL_SETTINGS_MARKER_2;
    settings.initMarker[3] = GLOBAL_SETTINGS_MARKER_3;
    settings.version = GLOBAL_CURRENT_SETTINGS_VERSION;

    resetWiFi();
    settings.network.wifiEnabled = true;

    strncpy(settings.mqttServer, DEFAULT_MQTT_SERVER, sizeof(settings.mqttServer) - 1);
    settings.mqttPort = DEFAULT_MQTT_PORT;
    settings.mqttReconnectIntervalMs = DEFAULT_MQTT_RECONNECT_MS;
    settings.mqttClientTimeoutMs = DEFAULT_MQTT_CLIENT_TIMEOUT_MS;
    settings.mqttEnabled = true;
    strncpy(settings.mqttClientId, WIFI_DEFAULT_HOST_NAME, sizeof(settings.mqttClientId) - 1);
    strncpy(settings.mqttBaseTopic, DEFAULT_MQTT_BASE_TOPIC, sizeof(settings.mqttBaseTopic) - 1);

    settings.zigbeeChannel = DEFAULT_ZIGBEE_CHANNEL;
    settings.permitJoinOnBootSec = DEFAULT_PERMIT_JOIN_SEC;
}

void SettingsManager::resetWiFi() {
    strncpy(settings.network.ssid, WIFI_DEFAULT_SSID, sizeof(settings.network.ssid) - 1);
    strncpy(settings.network.password, WIFI_DEFAULT_PASSWORD, sizeof(settings.network.password) - 1);
    strncpy(settings.network.hostName, WIFI_DEFAULT_HOST_NAME, sizeof(settings.network.hostName) - 1);
}

void SettingsManager::clampMqttClientTimeout(int &timeoutMs) {
    if (timeoutMs < MQTT_CLIENT_TIMEOUT_MS_MIN || timeoutMs > MQTT_CLIENT_TIMEOUT_MS_MAX) {
        timeoutMs = DEFAULT_MQTT_CLIENT_TIMEOUT_MS;
    }
}

void SettingsManager::clampZigbeeChannel(uint8_t &channel) {
    if (channel < 11 || channel > 26) {
        channel = DEFAULT_ZIGBEE_CHANNEL;
    }
}

void SettingsManager::readSettings(GlobalSettings *destination) {
    char *buffer = (char *)destination;
    LOGGER.info("Loading " + String((int)sizeof(GlobalSettings)) + " bytes");
    for (size_t i = 0; i < sizeof(GlobalSettings); i++) {
        buffer[i] = EEPROM.read(i);
    }
}

void SettingsManager::readSettings() {
    readSettings(&settings);
}

void SettingsManager::saveSetting(bool restart) {
    LOGGER.info("Saving settings...");
    char *dataPtr = (char *)&settings;
    for (size_t addr = 0; addr < sizeof(GlobalSettings); addr++) {
        EEPROM.write(addr, dataPtr[addr]);
    }
    EEPROM.commit();
    if (restart) {
        pendingRestart = true;
        restartRequestedMs = millis();
        LOGGER.warning("Restart scheduled");
    }
}

bool SettingsManager::handlePendingRestart(unsigned long delayMs) {
    if (!pendingRestart) {
        return false;
    }
    if ((millis() - restartRequestedMs) < delayMs) {
        return false;
    }
    LOGGER.warning("RESTARTING...");
    ESP.restart();
    return true;
}

GlobalSettings *SettingsManager::getSettings() {
    return &settings;
}

void SettingsManager::logSettings() {
    LOGGER.info("----- SETTINGS ----");
    LOGGER.info("  wifi ssid: " + String(settings.network.ssid));
    LOGGER.info("  host: " + String(settings.network.hostName));
    LOGGER.info("  wifiEnabled: " + String(settings.network.wifiEnabled ? "true" : "false"));
    LOGGER.info("  mqtt enabled: " + String(settings.mqttEnabled ? "true" : "false"));
    LOGGER.info("  mqtt server: " + String(settings.mqttServer) + ":" + String(settings.mqttPort));
    LOGGER.info("  mqtt base: " + String(settings.mqttBaseTopic));
    LOGGER.info("  zigbee channel: " + String(settings.zigbeeChannel));
}
