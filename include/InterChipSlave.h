#pragma once

#include <Arduino.h>
#include "SpiProtocol.h"
#include "DeviceTopicMap.h"

class InterChipSlave {
public:
    using SettingsFn = void (*)(uint8_t channel, uint8_t permitJoinSec, uint32_t unixSec);
    using PermitJoinFn = bool (*)(uint8_t seconds);
    using OnOffFn = void (*)(const uint8_t ieee[8], const char *command, uint8_t endpoint);
    using DeviceSyncFn = void (*)(uint8_t flags, const DeviceTopicEntry *entry);
    using DevicesFileFn = String (*)();

    void begin();
    void pump();
    void enqueueLogLine(const char *line);
    void enqueueAttrReport(const char *message, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr);
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
    void setDeviceSyncHandler(DeviceSyncFn handler);
    void setDeviceMapSource(DeviceTopicMap *deviceMap);
    void applyHostTime(uint32_t unixSec);
    void applyDeferredSettings();
    void applyDeferredRadioCommands();
    void pumpDeviceDump();
    void pumpDevicesFileDump();
    void requestDeviceDump();
    void requestDevicesFileDump();
    void setDevicesFileSource(DevicesFileFn handler);
    void setPumpPaused(bool paused);
    void resumeAfterRadioPause();

private:
    static constexpr int kQueue = 16;
    static constexpr int kHwSlots = 2;

    struct QueuedFrame {
        SpiFrame frame{};
    };

    QueuedFrame outbound[kQueue]{};
    int outboundCount = 0;
    SettingsFn settingsHandler = nullptr;
    PermitJoinFn permitJoinHandler = nullptr;
    OnOffFn onOffHandler = nullptr;
    DeviceSyncFn deviceSyncHandler = nullptr;
    DeviceTopicMap *deviceMapSource = nullptr;
    DevicesFileFn devicesFileSource = nullptr;
    bool deviceDumpPending = false;
    bool deviceDumpHeaderSent = false;
    int deviceDumpIndex = 0;
    bool fileDumpPending = false;
    bool fileDumpStarted = false;
    int fileDumpOffset = 0;
    String fileDumpText;
    uint8_t nextSeq = 1;
    bool readySent = false;
    bool spiReady = false;
    bool settingsPending = false;
    bool permitJoinPending = false;
    bool onOffPending = false;
    bool pumpPaused = false;
    uint8_t pendingChannel = 15;
    uint8_t pendingPermitJoinSec = 0;
    uint8_t deferredPermitSeconds = 0;
    uint8_t deferredOnOffIeee[8]{};
    char deferredOnOffCommand[SPI_DEVICE_MESSAGE_MAX]{};
    uint8_t deferredOnOffEndpoint = 255;
    uint32_t pendingUnixSec = 0;

    bool enqueueEvent(uint8_t cmd, const uint8_t *payload, uint16_t length);
    bool enqueueReply(uint8_t cmd, uint8_t seq, const uint8_t *payload, uint16_t length);
    bool tryEnqueue(uint8_t cmd, uint8_t seq, const uint8_t *payload, uint16_t length);
    bool dropOldestLogRecord();
    bool enqueueDeviceMap(const uint8_t *payload, uint16_t length);
    void removeOutboundAt(int index);
    void updateIrq();
    void handleHostFrame(const SpiFrame &frame);
    bool takeOutbound(SpiFrame &frame);
    void fillHardwareQueue();
    void serviceSpi();
    bool initializeBus();
};

extern InterChipSlave INTER_CHIP_SLAVE;
void interChipSlaveLogHook(const char *line);
