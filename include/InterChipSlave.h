#pragma once

#include <Arduino.h>
#include "SpiProtocol.h"

class InterChipSlave {
public:
    using SettingsFn = void (*)(uint8_t channel, uint8_t permitJoinSec, uint32_t unixSec);
    using PermitJoinFn = void (*)(uint8_t seconds);
    using OnOffFn = void (*)(const uint8_t ieee[8], uint8_t action);

    void begin();
    void pump();
    void enqueueLogLine(const char *line);
    void enqueueAttrReport(bool on, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr);
    void enqueueDeviceJoin(
        const uint8_t ieee[8],
        uint16_t shortAddr,
        uint8_t endpoint,
        const char *manufacturer,
        const char *model
    );
    void setSettingsHandler(SettingsFn handler);
    void setPermitJoinHandler(PermitJoinFn handler);
    void setOnOffHandler(OnOffFn handler);
    void applyHostTime(uint32_t unixSec);
    void applyDeferredSettings();

private:
    static constexpr int kQueue = 8;

    struct QueuedFrame {
        bool used = false;
        SpiFrame frame{};
    };

    QueuedFrame outbound[kQueue]{};
    SettingsFn settingsHandler = nullptr;
    PermitJoinFn permitJoinHandler = nullptr;
    OnOffFn onOffHandler = nullptr;
    uint8_t nextSeq = 1;
    bool readySent = false;
    bool spiReady = false;
    bool settingsPending = false;
    uint8_t pendingChannel = 15;
    uint8_t pendingPermitJoinSec = 0;
    uint32_t pendingUnixSec = 0;

    bool enqueueEvent(uint8_t cmd, const uint8_t *payload, uint16_t length);
    bool enqueueReply(uint8_t cmd, uint8_t seq, const uint8_t *payload, uint16_t length);
    void updateIrq();
    void handleHostFrame(const SpiFrame &frame);
    bool takeOutbound(SpiFrame &frame);
    void serviceSpi();
};

extern InterChipSlave INTER_CHIP_SLAVE;
void interChipSlaveLogHook(const char *line);
