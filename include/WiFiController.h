#pragma once

#include <Arduino.h>
#include "SettingsManager.h"

class WiFiController {
public:
    WiFiController(SettingsManager *settingsManager, bool forceAp);

    using InterfaceReadyFn = void (*)();

    bool isApMode() const;
    bool isStaConnected() const;
    bool hasUsableInterface() const;
    void setInterfaceReadyHandler(InterfaceReadyFn handler);
    void notifyStaDisconnected();
    void notifyStaGotIp();
    void applyStaRadio();
    void enableIeee154Coex();
    uint8_t zigbeeChannelOverlappingSta() const;
    int staWifiChannel() const;
    void update();

private:
    static constexpr unsigned long kStaJoinTimeoutMs = 20000UL;
    static constexpr unsigned long kStaReconnectMs = 10UL * 1000UL;
    static constexpr unsigned long kApRestartMs = 10000UL;
    static constexpr unsigned long kStaRadioRefreshMs = 10000UL;

    SettingsManager *settingsManager;
    bool apActive = false;
    bool staEnabledAtBoot = false;
    unsigned long lastStaReconnectMs = 0;
    unsigned long lastApRestartMs = 0;
    unsigned long lastStaRadioRefreshMs = 0;
    bool staWasConnected = false;
    bool staNeedsWebRebind = false;
    bool staWebRebindArmed = false;
    InterfaceReadyFn interfaceReadyHandler = nullptr;
    bool recoveryApIdentity = false;

    void stopStationBeforeAp();
    void startAp(bool useRecoveryIdentity);
    bool isApRadioUp() const;
    void startSta();
    void beginStaJoin();
    void reconnectSta();
    void onStaConnected();
    void requestStaWebRebind();
    void runPendingStaWebRebind(bool staHasIpv4);
    String apNetworkName() const;
    String apPassword() const;
};
