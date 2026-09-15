#pragma once

#include "GlobalSettings.h"
#include "DeviceTopicMap.h"
#include <Arduino.h>

class SettingsManager {
public:
    SettingsManager();

    GlobalSettings *getSettings();
    DeviceTopicMap *deviceMap();
    DeviceTopicEntry *devices();
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
    static uint32_t clampSpiSpeedHz(uint32_t speedHz);
    static void clampMqttClientTimeout(int &timeoutMs);
    static void clampZigbeeChannel(uint8_t &channel);

private:
    GlobalSettings settings;
    GlobalSettings committedMain;
    DeviceTopicEntry deviceSlots[DEVICE_MAP_SLOTS];
    DeviceTopicMap topicMap;
    bool pendingRestart = false;
    unsigned long restartRequestedMs = 0;

    void readSettings(GlobalSettings *destination);
    void writeEepromDirty(const uint8_t *nextImage, const uint8_t *previousImage, size_t length);
    void createEmptyDeviceFile();
    void parseDevicesJson(const String &json);
    void upgradeLegacyMainFromEeprom();
};
