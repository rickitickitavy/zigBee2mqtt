#pragma once

#include <Arduino.h>
#include "SpiProtocol.h"
#include "DeviceTopicMap.h"

class ZigbeeSpiProxy {
public:
    using LightStateFn = void (*)(bool on, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr);

    void begin();
    void onSpiEvent(const SpiFrame &frame);
    void permitJoin(uint8_t seconds);
    void closeJoin();
    bool controlOnOff(const uint8_t ieee[8], const char *command);
    String devicesJson(DeviceTopicMap *topicMap);
    void setLightStateHandler(LightStateFn handler);
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

    CachedDevice *findByIeee(const uint8_t ieee[8]);
    CachedDevice *allocSlot(const uint8_t ieee[8]);
};

extern ZigbeeSpiProxy ZIGBEE_SPI_PROXY;
