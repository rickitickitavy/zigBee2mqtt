#pragma once

#include "GlobalSettings.h"
#include <Arduino.h>
#include <stddef.h>

class DeviceTopicMap {
public:
    explicit DeviceTopicMap(DeviceTopicEntry *slots);

    DeviceTopicEntry *findByIeee(const uint8_t ieee[8]);
    const DeviceTopicEntry *findByIeee(const uint8_t ieee[8]) const;
    DeviceTopicEntry *findByCommandTopic(const char *topic);
    DeviceTopicEntry *upsert(
        const uint8_t ieee[8],
        const char *friendlyName,
        const char *stateTopic,
        const char *commandTopic,
        const char *availabilityTopic
    );
    bool removeByIeee(const uint8_t ieee[8]);
    void clearAll();
    int slotIndex(const DeviceTopicEntry *entry) const;
    DeviceTopicEntry *slotAt(int index);
    int usedCount() const;
    int nextUsedIndex(int startIndex) const;

    String formatIeee(const uint8_t ieee[8]);
    bool parseIeee(const char *text, uint8_t ieee[8]);
    String listJson();
    void replaceFromJson(const String &json);
    bool loadFromFile(const char *path);
    bool saveToFile(const char *path);

    static bool ieeeEqual(const uint8_t left[8], const uint8_t right[8]);
    static size_t packSyncPayload(uint8_t *out, size_t outMax, uint8_t flags, const DeviceTopicEntry *entry);
    static bool unpackSyncPayload(
        const uint8_t *in,
        uint16_t length,
        uint8_t *flags,
        DeviceTopicEntry *entry
    );

private:
    DeviceTopicEntry *slots;
};
