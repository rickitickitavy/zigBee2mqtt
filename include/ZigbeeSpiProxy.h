#pragma once

#include <Arduino.h>
#include "SpiProtocol.h"
#include "DeviceTopicMap.h"

class ZigbeeSpiProxy {
public:
    using LightStateFn = void (*)(const char *message, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr);

    ZigbeeSpiProxy();
    void begin();
    void onSpiEvent(const SpiFrame &frame);
    bool permitJoin(uint8_t seconds);
    bool closeJoin();
    bool controlOnOff(const uint8_t ieee[8], const char *command, uint8_t endpoint);
    bool writeAttribute(
        const uint8_t ieee[8],
        uint8_t endpoint,
        uint16_t clusterId,
        uint16_t attributeId,
        uint8_t dataType,
        uint32_t attributeValue
    );
    void requestRegistryPull(DeviceTopicMap *topicMap);
    void requestDevicesFile();
    String devicesFileJson() const;
    void pumpRegistrySync();
    bool enqueueDeviceUpsert(const DeviceTopicEntry *entry);
    bool enqueueDeviceDelete(const uint8_t ieee[8]);
    bool registryHydrated() const;
    bool isOnline(const uint8_t ieee[8]) const;
    void noteSeen(const uint8_t ieee[8]);
    String devicesJson(DeviceTopicMap *topicMap);
    void setLightStateHandler(LightStateFn handler);
    void setRegistryPullDoneHandler(void (*handler)());
    bool commandsAllowed() const;

private:
    static constexpr int kMaxDevices = 16;
    static constexpr int kPendingChangeSlots = 1;
    static constexpr unsigned long kOnlineWindowMs = 15UL * 60UL * 1000UL;

    struct CachedDevice {
        uint8_t ieee[8];
        uint16_t shortAddr;
        uint8_t endpoint;
        char manufacturer[32];
        char model[32];
        bool occupied;
        unsigned long lastSeenMs;
    };

    struct PendingDeviceChange {
        bool used;
        uint8_t flags;
        DeviceTopicEntry entry;
    };

    CachedDevice devices[kMaxDevices]{};
    LightStateFn lightStateHandler = nullptr;
    DeviceTopicMap *registryMap = nullptr;
    DeviceTopicEntry pullSlots[DEVICE_MAP_SLOTS]{};
    DeviceTopicMap pullMap;
    PendingDeviceChange pendingChanges[kPendingChangeSlots]{};
    bool registryPullRequested = false;
    bool registryPullActive = false;
    bool registryReady = false;
    bool pullCollecting = false;
    int registryPullCount = 0;
    int pullExpectedCount = -1;
    uint8_t pullRetries = 0;
    unsigned long registryPullStartedMs = 0;
    void (*registryPullDone)() = nullptr;
    bool filePullRequested = false;
    bool filePullCollecting = false;
    String filePullBuffer;
    String fileCache;

    CachedDevice *findByIeee(const uint8_t ieee[8]);
    CachedDevice *allocSlot(const uint8_t ieee[8]);
    bool enqueueRegistryFrame(uint8_t flags, const DeviceTopicEntry *entry);
    bool queueDeviceChange(uint8_t flags, const DeviceTopicEntry *entry);
    void applyPendingChangeToMap(const PendingDeviceChange *change);
    void replayPendingChanges();
    void pumpPendingChanges();
    void beginPullSnapshot();
    void applyPulledRegistry(const SpiFrame &frame);
    void finishRegistryPull();
    void applyDevicesFile(const SpiFrame &frame);
};

extern ZigbeeSpiProxy ZIGBEE_SPI_PROXY;
