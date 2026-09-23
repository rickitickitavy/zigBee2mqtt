#pragma once

#include <Arduino.h>
#include "SpiProtocol.h"
#include "DeviceTopicMap.h"
#include "UserStore.h"

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
    bool readAttribute(
        const uint8_t ieee[8],
        uint8_t endpoint,
        uint16_t clusterId,
        uint16_t attributeId
    );
    bool sendClusterCommand(
        const uint8_t ieee[8],
        uint8_t endpoint,
        uint16_t clusterId,
        uint8_t commandId,
        const uint8_t *payload,
        uint8_t payloadLength
    );
    void queueDeletedIeeesAndPushAll(const uint8_t (*deletedIeees)[8], int deletedCount);
    void requestRegistryPull(DeviceTopicMap *topicMap);
    void requestUsersPull(UserStore *store);
    void requestDevicesFile();
    String devicesFileJson() const;
    void pumpRegistrySync();
    bool enqueueDeviceUpsert(const DeviceTopicEntry *entry);
    bool enqueueDeviceDelete(const uint8_t ieee[8]);
    bool enqueueUserUpsert(const UserRecord *user);
    bool enqueueUserDelete(const char *userName);
    void queueDeletedUserNamesAndPushAll(const char (*removedNames)[USER_NAME_MAX], int removedCount);
    bool registryHydrated() const;
    bool isOnline(const uint8_t ieee[8]) const;
    bool lastRssiDbm(const uint8_t ieee[8], int8_t *rssiDbm) const;
    void appendListTelemetry(const uint8_t ieee[8], String &json) const;
    void noteSeen(const uint8_t ieee[8]);
    uint32_t packetsReceived() const;
    uint32_t packetsSent() const;
    bool pairingActive() const;
    int onlineCount() const;
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
        int8_t lastRssiDbm;
        bool hasRssi;
        bool hasBattery;
        unsigned batteryPercent;
        struct EndpointStatus {
            bool used;
            uint8_t endpoint;
            char state[SPI_DEVICE_MESSAGE_MAX];
        };
        EndpointStatus endpointStatus[DEVICE_CHANNEL_COUNT_MAX];
    };

    struct PendingDeviceChange {
        bool used;
        uint8_t flags;
        DeviceTopicEntry entry;
    };

    struct PendingUserChange {
        bool used;
        uint8_t flags;
        UserRecord user;
    };

    CachedDevice devices[kMaxDevices]{};
    LightStateFn lightStateHandler = nullptr;
    DeviceTopicMap *registryMap = nullptr;
    DeviceTopicEntry pullSlots[DEVICE_MAP_SLOTS]{};
    DeviceTopicMap pullMap;
    PendingDeviceChange pendingChanges[kPendingChangeSlots]{};
    UserStore *userMap = nullptr;
    UserStore pullUsers;
    PendingUserChange pendingUserChanges[kPendingChangeSlots]{};
    bool userPullRequested = false;
    bool userPullActive = false;
    bool userReady = false;
    bool userCollecting = false;
    int userPullExpectedCount = -1;
    uint8_t userPullRetries = 0;
    unsigned long userPullStartedMs = 0;
    char pendingDeleteUserNames[USER_STORE_MAX][USER_NAME_MAX]{};
    int pendingUserDeleteCount = 0;
    int pendingUserDeleteIndex = 0;
    int pendingUserUpsertWalk = 0;
    bool userFullPushActive = false;
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
    bool filePullInFlight = false;
    unsigned long filePullStartedMs = 0;
    String filePullBuffer;
    String fileCache;
    uint32_t packetsRx = 0;
    uint32_t packetsTx = 0;
    bool pairingOpen = false;
    uint8_t pendingDeleteIeees[DEVICE_MAP_SLOTS][8]{};
    int pendingDeleteCount = 0;
    int pendingDeleteIndex = 0;
    int pendingUpsertWalk = 0;
    bool fullPushActive = false;

    CachedDevice *findByIeee(const uint8_t ieee[8]);
    const CachedDevice *findByIeee(const uint8_t ieee[8]) const;
    CachedDevice *allocSlot(const uint8_t ieee[8]);
    void noteReportTelemetry(CachedDevice *slot, uint8_t endpoint, const char *message);
    bool enqueueRegistryFrame(uint8_t flags, const DeviceTopicEntry *entry);
    bool queueDeviceChange(uint8_t flags, const DeviceTopicEntry *entry);
    void applyPendingChangeToMap(const PendingDeviceChange *change);
    void replayPendingChanges();
    void pumpPendingChanges();
    void beginPullSnapshot();
    void applyPulledRegistry(const SpiFrame &frame);
    void finishRegistryPull();
    void applyDevicesFile(const SpiFrame &frame);
    bool enqueueUserFrame(uint8_t flags, const UserRecord *user);
    bool queueUserChange(uint8_t flags, const UserRecord *user);
    void applyPendingUserChangeToStore(const PendingUserChange *change);
    void replayPendingUserChanges();
    void pumpPendingUserChanges();
    void beginUserPullSnapshot();
    void applyPulledUsers(const SpiFrame &frame);
    void finishUsersPull();
};

extern ZigbeeSpiProxy ZIGBEE_SPI_PROXY;
