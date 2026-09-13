#include "DeviceTopicMap.h"
#include "JsonField.h"
#include "SpiProtocol.h"
#include "Defines.h"

#include <LittleFS.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

DeviceTopicMap::DeviceTopicMap(DeviceTopicEntry *deviceSlots) : slots(deviceSlots) {}

bool DeviceTopicMap::ieeeEqual(const uint8_t left[8], const uint8_t right[8]) {
    return memcmp(left, right, 8) == 0;
}

DeviceTopicEntry *DeviceTopicMap::slotAt(int index) {
    if (slots == nullptr || index < 0 || index >= DEVICE_MAP_SLOTS) {
        return nullptr;
    }
    return &slots[index];
}

int DeviceTopicMap::slotIndex(const DeviceTopicEntry *entry) const {
    if (slots == nullptr || entry == nullptr) {
        return -1;
    }
    const ptrdiff_t offset = entry - slots;
    if (offset < 0 || offset >= DEVICE_MAP_SLOTS) {
        return -1;
    }
    return (int)offset;
}

int DeviceTopicMap::usedCount() const {
    int count = 0;
    if (slots == nullptr) {
        return 0;
    }
    for (int i = 0; i < DEVICE_MAP_SLOTS; i++) {
        if (slots[i].used) {
            count++;
        }
    }
    return count;
}

DeviceTopicEntry *DeviceTopicMap::findByIeee(const uint8_t ieee[8]) {
    return const_cast<DeviceTopicEntry *>(
        static_cast<const DeviceTopicMap *>(this)->findByIeee(ieee)
    );
}

const DeviceTopicEntry *DeviceTopicMap::findByIeee(const uint8_t ieee[8]) const {
    if (slots == nullptr) {
        return nullptr;
    }
    for (int i = 0; i < DEVICE_MAP_SLOTS; i++) {
        const DeviceTopicEntry *entry = &slots[i];
        if (entry->used && ieeeEqual(entry->ieee, ieee)) {
            return entry;
        }
    }
    return nullptr;
}

uint8_t DeviceTopicMap::normalizeChannelCount(int raw) {
    if (raw == DEVICE_CHANNEL_PARSE) {
        return DEVICE_CHANNEL_PARSE;
    }
    if (raw >= DEVICE_CHANNEL_COUNT_DEFAULT && raw <= DEVICE_CHANNEL_COUNT_MAX) {
        return (uint8_t)raw;
    }
    return DEVICE_CHANNEL_COUNT_DEFAULT;
}

bool DeviceTopicMap::usesPayloadParse(uint8_t channelCount) {
    return channelCount == DEVICE_CHANNEL_PARSE;
}

bool DeviceTopicMap::usesTopicSuffix(uint8_t channelCount) {
    return channelCount >= 2 && channelCount <= DEVICE_CHANNEL_COUNT_MAX;
}

bool DeviceTopicMap::isUsableEndpoint(uint8_t endpoint) {
    return endpoint >= 1 && endpoint <= 240;
}

String DeviceTopicMap::statePublishTopic(const DeviceTopicEntry *entry, uint8_t endpoint) {
    if (entry == nullptr || entry->stateTopic[0] == '\0') {
        return "";
    }
    if (usesTopicSuffix(entry->channelCount) && isUsableEndpoint(endpoint) && endpoint != 1) {
        return String(entry->stateTopic) + "/" + String(endpoint);
    }
    return String(entry->stateTopic);
}

String DeviceTopicMap::statePublishPayload(const DeviceTopicEntry *entry, uint8_t endpoint, const char *message) {
    const char *body = message != nullptr ? message : "";
    if (entry != nullptr && usesPayloadParse(entry->channelCount) && isUsableEndpoint(endpoint)) {
        return String("ch-") + String(endpoint) + "##" + body;
    }
    return String(body);
}

bool DeviceTopicMap::parseChannelPayload(const char *payload, uint8_t *endpoint, String *action) {
    if (payload == nullptr || endpoint == nullptr || action == nullptr) {
        return false;
    }
    if (strncmp(payload, "ch-", 3) != 0) {
        return false;
    }
    const char *digits = payload + 3;
    if (*digits < '0' || *digits > '9') {
        return false;
    }
    const int parsed = atoi(digits);
    const char *separator = strstr(digits, "##");
    if (separator == nullptr || parsed < 1 || parsed > 240) {
        return false;
    }
    *endpoint = (uint8_t)parsed;
    *action = String(separator + 2);
    return true;
}

DeviceTopicEntry *DeviceTopicMap::findByCommandTopic(const char *topic) {
    return findByCommandTopic(topic, nullptr);
}

DeviceTopicEntry *DeviceTopicMap::findByCommandTopic(const char *topic, uint8_t *topicEndpoint) {
    if (slots == nullptr || topic == nullptr || topic[0] == '\0') {
        return nullptr;
    }
    if (topicEndpoint != nullptr) {
        *topicEndpoint = 0;
    }
    for (int i = 0; i < DEVICE_MAP_SLOTS; i++) {
        DeviceTopicEntry *entry = &slots[i];
        if (!entry->used || entry->commandTopic[0] == '\0') {
            continue;
        }
        if (strcmp(entry->commandTopic, topic) == 0) {
            return entry;
        }
        if (!usesTopicSuffix(entry->channelCount)) {
            continue;
        }
        const size_t prefixLength = strlen(entry->commandTopic);
        if (strncmp(topic, entry->commandTopic, prefixLength) != 0 || topic[prefixLength] != '/') {
            continue;
        }
        const int parsed = atoi(topic + prefixLength + 1);
        if (!isUsableEndpoint((uint8_t)parsed) || parsed == 1) {
            continue;
        }
        if (topicEndpoint != nullptr) {
            *topicEndpoint = (uint8_t)parsed;
        }
        return entry;
    }
    return nullptr;
}

DeviceTopicEntry *DeviceTopicMap::upsert(
    const uint8_t ieee[8],
    const char *friendlyName,
    const char *stateTopic,
    const char *commandTopic,
    const char *availabilityTopic,
    uint8_t channelCount
) {
    if (slots == nullptr) {
        return nullptr;
    }
    DeviceTopicEntry *entry = findByIeee(ieee);
    if (entry == nullptr) {
        for (int i = 0; i < DEVICE_MAP_SLOTS; i++) {
            if (!slots[i].used) {
                entry = &slots[i];
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
    entry->channelCount = normalizeChannelCount(channelCount);
    return entry;
}

bool DeviceTopicMap::removeByIeee(const uint8_t ieee[8]) {
    DeviceTopicEntry *entry = findByIeee(ieee);
    if (entry == nullptr) {
        return false;
    }
    memset(entry, 0, sizeof(DeviceTopicEntry));
    return true;
}

void DeviceTopicMap::clearAll() {
    if (slots == nullptr) {
        return;
    }
    memset(slots, 0, sizeof(DeviceTopicEntry) * DEVICE_MAP_SLOTS);
}

int DeviceTopicMap::nextUsedIndex(int startIndex) const {
    if (slots == nullptr) {
        return -1;
    }
    for (int i = startIndex; i < DEVICE_MAP_SLOTS; i++) {
        if (slots[i].used) {
            return i;
        }
    }
    return -1;
}

void DeviceTopicMap::replaceFromJson(const String &json) {
    clearAll();
    const char *cursor = json.c_str();
    while (cursor != nullptr && *cursor != '\0') {
        const char *objectStart = strchr(cursor, '{');
        if (objectStart == nullptr) {
            break;
        }
        const char *objectEnd = strchr(objectStart, '}');
        if (objectEnd == nullptr) {
            break;
        }
        String object = String(objectStart).substring(0, (unsigned int)(objectEnd - objectStart + 1));
        String ieeeText;
        String friendlyName;
        String stateTopic;
        String commandTopic;
        String availability;
        extractJsonString(object.c_str(), "ieee", ieeeText);
        extractJsonString(object.c_str(), "name", friendlyName);
        if (friendlyName.length() == 0) {
            extractJsonString(object.c_str(), "friendlyName", friendlyName);
        }
        extractJsonString(object.c_str(), "state", stateTopic);
        extractJsonString(object.c_str(), "command", commandTopic);
        extractJsonString(object.c_str(), "availability", availability);
        int parsedChannels = DEVICE_CHANNEL_COUNT_DEFAULT;
        if (!extractJsonInt(object.c_str(), "channels", parsedChannels)) {
            parsedChannels = DEVICE_CHANNEL_COUNT_DEFAULT;
        }
        uint8_t ieee[8];
        if (ieeeText.length() > 0 && parseIeee(ieeeText.c_str(), ieee)) {
            upsert(
                ieee,
                friendlyName.c_str(),
                stateTopic.c_str(),
                commandTopic.c_str(),
                availability.c_str(),
                normalizeChannelCount(parsedChannels)
            );
        }
        cursor = objectEnd + 1;
    }
}

bool DeviceTopicMap::loadFromFile(const char *path) {
    if (path == nullptr) {
        return false;
    }
    File file = LittleFS.open(path, "r");
    if (!file) {
        return false;
    }
    String json = file.readString();
    file.close();
    json.trim();
    if (json.length() == 0 || json.charAt(0) != '[') {
        return false;
    }
    replaceFromJson(json);
    return true;
}

bool DeviceTopicMap::saveToFile(const char *path) {
    if (path == nullptr) {
        return false;
    }
    File file = LittleFS.open(path, "w");
    if (!file) {
        return false;
    }
    const String json = listJson();
    const size_t written = file.print(json);
    file.close();
    return written == json.length();
}

size_t DeviceTopicMap::packSyncPayload(
    uint8_t *out,
    size_t outMax,
    uint8_t flags,
    const DeviceTopicEntry *entry
) {
    if (out == nullptr || outMax < 1) {
        return 0;
    }
    const bool hasEntry = (flags & SPI_DEVICE_SYNC_ENTRY) != 0 && entry != nullptr;
    const size_t length = hasEntry ? SPI_DEVICE_SYNC_ENTRY_LEN : 1;
    if (outMax < length) {
        return 0;
    }
    memset(out, 0, length);
    out[0] = flags;
    if (hasEntry) {
        memcpy(out + 1, entry->ieee, 8);
        strncpy((char *)out + 9, entry->friendlyName, SPI_DEVICE_SYNC_NAME_LEN - 1);
        strncpy((char *)out + 33, entry->stateTopic, SPI_DEVICE_SYNC_TOPIC_LEN - 1);
        strncpy((char *)out + 97, entry->commandTopic, SPI_DEVICE_SYNC_TOPIC_LEN - 1);
        strncpy((char *)out + 161, entry->availabilityTopic, SPI_DEVICE_SYNC_TOPIC_LEN - 1);
        out[SPI_DEVICE_SYNC_ENTRY_LEN_NO_CHANNELS] = entry->channelCount;
    }
    return length;
}

bool DeviceTopicMap::unpackSyncPayload(
    const uint8_t *in,
    uint16_t length,
    uint8_t *flags,
    DeviceTopicEntry *entry
) {
    if (in == nullptr || flags == nullptr || length < 1) {
        return false;
    }
    *flags = in[0];
    if (entry != nullptr) {
        memset(entry, 0, sizeof(DeviceTopicEntry));
    }
    if ((*flags & SPI_DEVICE_SYNC_ENTRY) == 0) {
        return true;
    }
    if (length < SPI_DEVICE_SYNC_ENTRY_LEN_NO_CHANNELS || entry == nullptr) {
        return false;
    }
    memcpy(entry->ieee, in + 1, 8);
    strncpy(entry->friendlyName, (const char *)in + 9, sizeof(entry->friendlyName) - 1);
    strncpy(entry->stateTopic, (const char *)in + 33, sizeof(entry->stateTopic) - 1);
    strncpy(entry->commandTopic, (const char *)in + 97, sizeof(entry->commandTopic) - 1);
    strncpy(entry->availabilityTopic, (const char *)in + 161, sizeof(entry->availabilityTopic) - 1);
    if (length >= SPI_DEVICE_SYNC_ENTRY_LEN) {
        entry->channelCount = normalizeChannelCount(in[SPI_DEVICE_SYNC_ENTRY_LEN_NO_CHANNELS]);
    } else {
        entry->channelCount = DEVICE_CHANNEL_COUNT_DEFAULT;
    }
    entry->used = 1;
    return true;
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
    if (slots == nullptr) {
        json += "]";
        return json;
    }
    for (int i = 0; i < DEVICE_MAP_SLOTS; i++) {
        DeviceTopicEntry *entry = &slots[i];
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
        appendJsonEscaped(json, entry->friendlyName, sizeof(entry->friendlyName));
        json += "\",\"state\":\"";
        appendJsonEscaped(json, entry->stateTopic, sizeof(entry->stateTopic));
        json += "\",\"command\":\"";
        appendJsonEscaped(json, entry->commandTopic, sizeof(entry->commandTopic));
        json += "\",\"availability\":\"";
        appendJsonEscaped(json, entry->availabilityTopic, sizeof(entry->availabilityTopic));
        json += "\",\"channels\":";
        json += String(entry->channelCount);
        json += "}";
    }
    json += "]";
    return json;
}
