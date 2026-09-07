#pragma once

#include "GlobalSettings.h"

class SettingsManager {
public:
    SettingsManager();

    GlobalSettings *getSettings();
    void readSettings();
    void saveSetting(bool restart);
    void applyDefaults();
    void resetWiFi();
    void logSettings();
    void requestRestart();
    bool handlePendingRestart(unsigned long delayMs);
    static void clampMqttClientTimeout(int &timeoutMs);
    static void clampZigbeeChannel(uint8_t &channel);

private:
    GlobalSettings settings;
    bool pendingRestart = false;
    unsigned long restartRequestedMs = 0;

    void readSettings(GlobalSettings *destination);
};
