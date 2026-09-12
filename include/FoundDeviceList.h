#pragma once

#include "GlobalSettings.h"
#include "DeviceTopicMap.h"
#include "SpiProtocol.h"
#include <Arduino.h>

class FoundDeviceList {
public:
    static constexpr int kMaxFound = 16;

    struct FoundDevice {
        uint8_t ieee[8];
        uint16_t shortAddr;
        uint8_t endpoint;
        char manufacturer[32];
        char model[32];
        bool used;
    };

    void clear();
    void noteJoin(const SpiFrame &frame, DeviceTopicMap *registered);
    void removeIeee(const uint8_t ieee[8]);
    String listJson(DeviceTopicMap *formatter);

private:
    FoundDevice found[kMaxFound]{};
};
