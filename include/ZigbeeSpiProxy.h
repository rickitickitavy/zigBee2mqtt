#pragma once

#include <Arduino.h>
#include "SpiProtocol.h"
#include "DeviceTopicMap.h"

class ZigbeeSpiProxy {
public:
    using LightStateFn = void (*)(const char *message, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr);

    void begin();
    void onSpiEvent(const SpiFrame &frame);
    bool permitJoin(uint8_t seconds);
    bool closeJoin();
    bool controlOnOff(const uint8_t ieee[8], const char *command, uint8_t endpoint);
    void startRegistrySync(DeviceTopicMap *topicMap, bool allowEmptyReplace = false);
    void requestRegistryPull(DeviceTopicMap *topicMap);
    void pumpRegistrySync();
    String devicesJson(DeviceTopicMap *topicMap);
    void setLightStateHandler(LightStateFn handler);
    void setRegistryPullDoneHandler(void (*handler)());
    bool commandsAllowed() const;

private:
    static constexpr int kMaxDevices = 16;

    struct CachedDevice {
        uint8_t ieee[8];
        uint16_t shortAddr;
        uint8_t endpoint;
        char manufacturer[32];
        char model[32];
        bool occupied;
    };

    CachedDevice devices[kMaxDevices]{};
    LightStateFn lightStateHandler = nullptr;
    DeviceTopicMap *registryMap = nullptr;
    int registryScanIndex = 0;
    bool registrySyncActive = false;
    bool registryResetSent = false;
    bool registryAllowEmptyReplace = false;
    bool registryPullRequested = false;
    bool registryPullActive = false;
    bool registryPullCleared = false;
    int registryPullCount = 0;
    void (*registryPullDone)() = nullptr;

    CachedDevice *findByIeee(const uint8_t ieee[8]);
    CachedDevice *allocSlot(const uint8_t ieee[8]);
    int nextUsedSlot(int startIndex) const;
    bool enqueueRegistryFrame(uint8_t flags, const DeviceTopicEntry *entry);
    void applyPulledRegistry(const SpiFrame &frame);
    void finishRegistryPull();
};

extern ZigbeeSpiProxy ZIGBEE_SPI_PROXY;
