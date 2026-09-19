#pragma once

#include <Arduino.h>
#include "Defines.h"
#include "SpiProtocol.h"
#include "DeviceTopicMap.h"

class InterChipHost {
public:
    using EventFn = void (*)(const SpiFrame &frame);

    void begin();
    void resetSlaveSynchronous();
    void pump(); // host SPI task only
    bool tryEnqueue(uint8_t cmd, const uint8_t *payload, uint16_t length);
    bool isNormal() const;
    bool isLinkHealthy() const;
    HostBringupState bringupState() const;
    void setSettingsSource(uint8_t channel, uint8_t permitJoinSec);
    void setClockHz(uint32_t speedHz);
    uint32_t clockHz() const;
    void setEventHandler(EventFn handler);
    void requestTimeSync();

private:
    static constexpr int kOutQueue = 8;
    static constexpr unsigned long kReadyTimeoutMs = 10000UL;
    static constexpr unsigned long kReplyTimeoutMs = 3000UL;
    static constexpr unsigned long kPollMs = 50UL;
    static constexpr unsigned long kPingPeriodMs = 10000UL;
    static constexpr unsigned long kTimeSyncMs = 30000UL;
    static constexpr unsigned long kMinTransferGapUs = 2000UL;
    static constexpr unsigned long kRstPulseMs = 15UL;

    struct QueuedFrame {
        bool used = false;
        SpiFrame frame{};
        bool expectReply = false;
        unsigned long deadlineMs = 0;
    };

    QueuedFrame outbound[kOutQueue]{};
    EventFn eventHandler = nullptr;
    HostBringupState state = HostBringupReset;
    uint8_t nextSeq = 1;
    uint8_t zigbeeChannel = 15;
    uint8_t permitJoinSec = 0;
    volatile uint32_t spiClockHz = DEFAULT_SPI_SPEED_HZ;
    unsigned long resetStartedMs = 0;
    unsigned long waitStartedMs = 0;
    unsigned long lastPollMs = 0;
    unsigned long lastPingMs = 0;
    unsigned long lastPongMs = 0;
    unsigned long lastTimeSyncMs = 0;
    unsigned long lastSettingsOkMs = 0;
    bool resetAsserting = false;
    bool settingsQueued = false;
    uint8_t settingsRetries = 0;
    uint8_t pendingSeq = 0;
    uint8_t pendingCmd = 0;
    unsigned long pendingDeadlineMs = 0;
    bool hasPending = false;
    uint8_t pingTimeouts = 0;
    bool bootResetCompleted = false;

    void pulseResetStart();
    void pulseResetFinishIfDue();
    bool enqueueInternal(uint8_t cmd, const uint8_t *payload, uint16_t length, bool expectReply);
    bool tryCoalesceDeviceControl(uint8_t cmd, const uint8_t *payload, uint16_t length, bool expectReply);
    void transferOnce(const SpiFrame *hostFrame);
    void handleInbound(const SpiFrame &frame);
    void emitLocalTimeout();
    void enterReset();
    void maybePushSettings();
};

extern InterChipHost INTER_CHIP_HOST;
