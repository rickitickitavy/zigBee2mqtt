#pragma once

#include "GlobalSettings.h"
#include <Arduino.h>

class DeviceTopicMap {
public:
    explicit DeviceTopicMap(GlobalSettings *settings);

    DeviceTopicEntry *findByIeee(const uint8_t ieee[8]);
    DeviceTopicEntry *findByCommandTopic(const char *topic);
    DeviceTopicEntry *upsert(
        const uint8_t ieee[8],
        const char *friendlyName,
        const char *stateTopic,
        const char *commandTopic,
        const char *availabilityTopic
    );

    String formatIeee(const uint8_t ieee[8]);
    bool parseIeee(const char *text, uint8_t ieee[8]);
    String listJson();

    static bool ieeeEqual(const uint8_t left[8], const uint8_t right[8]);

private:
    GlobalSettings *settings;
};
