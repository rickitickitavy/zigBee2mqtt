#include "SettingsManager.h"
#include "Logger.h"
#include "Defines.h"

#include <EEPROM.h>
#include <string.h>
#include <WiFi.h>

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

    strncpy(settings.mqtt.server, DEFAULT_MQTT_SERVER, sizeof(settings.mqtt.server) - 1);
    settings.mqtt.port = DEFAULT_MQTT_PORT;
    settings.mqtt.reconnectIntervalMs = DEFAULT_MQTT_RECONNECT_MS;
    settings.mqtt.clientTimeoutMs = DEFAULT_MQTT_CLIENT_TIMEOUT_MS;
    settings.mqtt.enabled = true;
    strncpy(settings.mqtt.clientId, DEFAULT_MQTT_CLIENT_ID, sizeof(settings.mqtt.clientId) - 1);
    strncpy(settings.mqtt.baseTopic, DEFAULT_MQTT_BASE_TOPIC, sizeof(settings.mqtt.baseTopic) - 1);

    settings.zigbee.channel = DEFAULT_ZIGBEE_CHANNEL;
    settings.zigbee.permitJoinOnBootSec = DEFAULT_PERMIT_JOIN_SEC;
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

void SettingsManager::requestRestart() {
    pendingRestart = true;
    restartRequestedMs = millis();
    LOGGER.warning("Restart scheduled");
}

void SettingsManager::saveSetting(bool restart) {
    LOGGER.info("Saving settings...");
    char *dataPtr = (char *)&settings;
    for (size_t addr = 0; addr < sizeof(GlobalSettings); addr++) {
        EEPROM.write(addr, dataPtr[addr]);
    }
    EEPROM.commit();
    if (restart) {
        requestRestart();
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
}
