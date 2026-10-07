#pragma once

#include "GlobalSettings.h"
#include "DeviceTopicMap.h"
#include <Arduino.h>

class SettingsManager {
public:
    SettingsManager();

    GlobalSettings *getSettings();
    DeviceTopicMap *deviceMap();
    void readSettings();
    void saveSetting(bool restart);
    void saveMain(bool restart);
    bool saveDeviceSlot(int slotIndex);
    bool saveDevicesJson();
    void loadDeviceFile();
    String devicesJsonFile();
    void applyDefaults();
    void resetWiFi();
    void logSettings();
    void requestRestart();
    bool handlePendingRestart(unsigned long delayMs);
    bool mainEqualsCommitted() const;
    uint32_t spiSpeedHz() const;
    void setSpiSpeedHz(uint32_t speedHz);
    uint8_t blueLedBrightness() const;
    uint8_t greenLedBrightness() const;
    void setLedBrightness(uint8_t bluePercent, uint8_t greenPercent);
    uint8_t uiTheme() const;
    void setUiTheme(uint8_t themeId);
    static const char *uiThemeJsonId(uint8_t themeId);
    static uint8_t uiThemeFromJsonId(const char *themeId);
    static uint32_t clampSpiSpeedHz(uint32_t speedHz);
    static uint8_t clampLedBrightnessPercent(uint8_t percent);
    static void clampMqttClientTimeout(int &timeoutMs);
    static void clampZigbeeChannel(uint8_t &channel);

private:
    GlobalSettings settings;
    GlobalSettings committedMain;
    DeviceTopicMap topicMap;
    bool pendingRestart = false;
    unsigned long restartRequestedMs = 0;

    void readSettings(GlobalSettings *destination);
    void writeEepromDirty(const uint8_t *nextImage, const uint8_t *previousImage, size_t length);
    void createEmptyDeviceFile();
    void parseDevicesJson(const String &json);
    void upgradeLegacyMainFromEeprom();
    void upgradeMainFromVersion5();
};
