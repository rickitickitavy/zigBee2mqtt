#include "DeviceTopicMap.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

DeviceTopicMap::DeviceTopicMap(GlobalSettings *settings) : settings(settings) {}

bool DeviceTopicMap::ieeeEqual(const uint8_t left[8], const uint8_t right[8]) {
    return memcmp(left, right, 8) == 0;
}

DeviceTopicEntry *DeviceTopicMap::findByIeee(const uint8_t ieee[8]) {
    for (int i = 0; i < DEVICE_MAP_SLOTS; i++) {
        DeviceTopicEntry *entry = &settings->devices[i];
        if (entry->used && ieeeEqual(entry->ieee, ieee)) {
            return entry;
        }
    }
    return nullptr;
}

DeviceTopicEntry *DeviceTopicMap::findByCommandTopic(const char *topic) {
    if (topic == nullptr || topic[0] == '\0') {
        return nullptr;
    }
    for (int i = 0; i < DEVICE_MAP_SLOTS; i++) {
        DeviceTopicEntry *entry = &settings->devices[i];
        if (entry->used && entry->commandTopic[0] != '\0' && strcmp(entry->commandTopic, topic) == 0) {
            return entry;
        }
    }
    return nullptr;
}

DeviceTopicEntry *DeviceTopicMap::upsert(
    const uint8_t ieee[8],
    const char *friendlyName,
    const char *stateTopic,
    const char *commandTopic,
    const char *availabilityTopic
) {
    DeviceTopicEntry *entry = findByIeee(ieee);
    if (entry == nullptr) {
        for (int i = 0; i < DEVICE_MAP_SLOTS; i++) {
            if (!settings->devices[i].used) {
                entry = &settings->devices[i];
                break;
            }
        }
    }
    if (entry == nullptr) {
        return nullptr;
    }

    memset(entry, 0, sizeof(DeviceTopicEntry));
    memcpy(entry->ieee, ieee, 8);
    entry->used = 1;
    if (friendlyName != nullptr) {
        strncpy(entry->friendlyName, friendlyName, sizeof(entry->friendlyName) - 1);
    }
    if (stateTopic != nullptr) {
        strncpy(entry->stateTopic, stateTopic, sizeof(entry->stateTopic) - 1);
    }
    if (commandTopic != nullptr) {
        strncpy(entry->commandTopic, commandTopic, sizeof(entry->commandTopic) - 1);
    }
    if (availabilityTopic != nullptr) {
        strncpy(entry->availabilityTopic, availabilityTopic, sizeof(entry->availabilityTopic) - 1);
    }
    return entry;
}

String DeviceTopicMap::formatIeee(const uint8_t ieee[8]) {
    char buffer[24];
    snprintf(
        buffer,
        sizeof(buffer),
        "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
        ieee[7],
        ieee[6],
        ieee[5],
        ieee[4],
        ieee[3],
        ieee[2],
        ieee[1],
        ieee[0]
    );
    return String(buffer);
}

static int hexNibble(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

bool DeviceTopicMap::parseIeee(const char *text, uint8_t ieee[8]) {
    if (text == nullptr) {
        return false;
    }
    uint8_t msbFirst[8];
    int filled = 0;
    int highNibble = -1;
    for (const char *cursor = text; *cursor != '\0'; cursor++) {
        if (*cursor == ':' || *cursor == '-' || *cursor == ' ') {
            continue;
        }
        int nibble = hexNibble(*cursor);
        if (nibble < 0) {
            return false;
        }
        if (highNibble < 0) {
            highNibble = nibble;
        } else {
            if (filled >= 8) {
                return false;
            }
            msbFirst[filled++] = (uint8_t)((highNibble << 4) | nibble);
            highNibble = -1;
        }
    }
    if (filled != 8 || highNibble >= 0) {
        return false;
    }
    for (int i = 0; i < 8; i++) {
        ieee[i] = msbFirst[7 - i];
    }
    return true;
}

String DeviceTopicMap::listJson() {
    String json = "[";
    bool first = true;
    for (int i = 0; i < DEVICE_MAP_SLOTS; i++) {
        DeviceTopicEntry *entry = &settings->devices[i];
        if (!entry->used) {
            continue;
        }
        if (!first) {
            json += ",";
        }
        first = false;
        json += "{\"ieee\":\"";
        json += formatIeee(entry->ieee);
        json += "\",\"name\":\"";
        json += entry->friendlyName;
        json += "\",\"state\":\"";
        json += entry->stateTopic;
        json += "\",\"command\":\"";
        json += entry->commandTopic;
        json += "\",\"availability\":\"";
        json += entry->availabilityTopic;
        json += "\"}";
    }
    json += "]";
    return json;
}
