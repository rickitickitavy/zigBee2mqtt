#pragma once

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator mode is not selected (ZIGBEE_MODE_ZCZR)"
#endif

#include <Arduino.h>
#include <Zigbee.h>
#include "DeviceTopicMap.h"

struct BoundZigbeeDevice {
    uint8_t ieee[8];
    uint16_t shortAddr;
    uint8_t endpoint;
    char manufacturer[32];
    char model[32];
    bool occupied;
};

class ZigbeeCoordinator {
public:
    using LightStateFn = void (*)(bool on, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr);
    using DeviceBoundFn = void (*)(const BoundZigbeeDevice *device);

    ZigbeeCoordinator();

    void attachLibraryCallbacks(void (*withSource)(bool, uint8_t, esp_zb_zcl_addr_t));
    bool begin(uint8_t channel, uint8_t permitJoinSec);
    bool isStarted() const;
    void permitJoin(uint8_t seconds);
    void closeJoin();
    void refreshBoundDevices();
    void dispatch();
    String devicesJson(DeviceTopicMap *topicMap);
    bool controlOnOff(const uint8_t ieee[8], const char *command);
    BoundZigbeeDevice *findByIeee(const uint8_t ieee[8]);
    BoundZigbeeDevice *findByShortAddr(uint16_t shortAddr);
    void setLightStateHandler(LightStateFn handler);
    void setDeviceBoundHandler(DeviceBoundFn handler);
    void handleLightStateWithSource(bool on, uint8_t endpoint, esp_zb_zcl_addr_t source);

    static constexpr int kMaxBoundDevices = 16;

private:
    ZigbeeSwitch zigbeeSwitch;
    BoundZigbeeDevice boundDevices[kMaxBoundDevices];
    LightStateFn lightStateHandler = nullptr;
    DeviceBoundFn deviceBoundHandler = nullptr;
    unsigned long lastRefreshMs = 0;
    bool started = false;

    void storeBoundDevice(zb_device_params_t *device);
    void resolveIeeeFromSource(esp_zb_zcl_addr_t source, uint8_t ieee[8], uint16_t *shortAddr);
};
