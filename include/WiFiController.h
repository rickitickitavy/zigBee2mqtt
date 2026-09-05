#pragma once

#include <Arduino.h>
#include "SettingsManager.h"

class WiFiController {
public:
    WiFiController(SettingsManager *settingsManager, bool forceAp);

    bool isApMode() const;
    bool isStaConnected() const;
    void update();

private:
    static constexpr unsigned long kApTimeoutMs = 5UL * 60UL * 1000UL;
    static constexpr unsigned long kStaReconnectMs = 10UL * 1000UL;

    SettingsManager *settingsManager;
    bool forceAp;
    bool apActive = false;
    bool staEnabledAtBoot = false;
    unsigned long apStartedMs = 0;
    unsigned long lastStaReconnectMs = 0;
    bool staWasConnected = false;

    void startAp(const String &deviceName);
    void stopAp();
    void startSta(const String &deviceName);
    void reconnectSta();
    void onStaConnected();
};
