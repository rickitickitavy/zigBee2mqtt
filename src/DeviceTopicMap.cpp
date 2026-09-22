#include "DeviceTopicMap.h"
#include "JsonField.h"
#include "SpiProtocol.h"
#include "Defines.h"
#include "ZigbeeCluster.h"
#include "ZigbeeDeviceType.h"

#include <LittleFS.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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
    if (usesTopicSuffix(entry->channelCount) && isUsableEndpoint(endpoint)) {
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

static bool parseNumericField(const String &text, uint32_t *outValue) {
    if (outValue == nullptr) {
        return false;
    }
    String trimmed = text;
    trimmed.trim();
    if (trimmed.length() == 0) {
        return false;
    }
    char *endPointer = nullptr;
    unsigned long parsed = strtoul(trimmed.c_str(), &endPointer, 0);
    if (endPointer == trimmed.c_str()) {
        return false;
    }
    *outValue = (uint32_t)parsed;
    return true;
}

static uint8_t dataTypeFromValue(uint32_t attributeValue) {
    if (attributeValue <= 0xFFu) {
        return ZCL_ATTR_TYPE_U8;
    }
    if (attributeValue <= 0xFFFFu) {
        return ZCL_ATTR_TYPE_U16;
    }
    return ZCL_ATTR_TYPE_U32;
}

bool DeviceTopicMap::parseFullControlBody(const char *body, uint8_t mappedEndpoint, ZclWriteFields *fields) {
    if (fields == nullptr) {
        return false;
    }
    fields->clusterId = 0x0006;
    fields->attributeId = 0x0000;
    fields->attributeValue = 0;
    fields->endpoint = mappedEndpoint;
    fields->dataType = ZCL_ATTR_TYPE_U8;
    fields->parsedAny = false;
    fields->hasWrite = false;
    if (body == nullptr) {
        return true;
    }

    String remaining = String(body);
    remaining.trim();
    bool sawType = false;
    while (remaining.length() > 0) {
        const int commaIndex = remaining.indexOf(',');
        String token = commaIndex >= 0 ? remaining.substring(0, commaIndex) : remaining;
        remaining = commaIndex >= 0 ? remaining.substring(commaIndex + 1) : "";
        token.trim();
        const int equalsIndex = token.indexOf('=');
        if (equalsIndex <= 0) {
            continue;
        }
        String key = token.substring(0, equalsIndex);
        String fieldValue = token.substring(equalsIndex + 1);
        key.trim();
        key.toLowerCase();
        fieldValue.trim();
        uint32_t parsedNumber = 0;
        if (key == "cl") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            fields->clusterId = (uint16_t)parsedNumber;
            fields->parsedAny = true;
        } else if (key == "attr") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            fields->attributeId = (uint16_t)parsedNumber;
            fields->parsedAny = true;
            fields->hasWrite = true;
        } else if (key == "val") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            fields->attributeValue = parsedNumber;
            fields->parsedAny = true;
            fields->hasWrite = true;
        } else if (key == "ep") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            const uint8_t parsedEndpoint = (uint8_t)parsedNumber;
            if (isUsableEndpoint(parsedEndpoint)) {
                fields->endpoint = parsedEndpoint;
            }
            fields->parsedAny = true;
        } else if (key == "type") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            fields->dataType = (uint8_t)parsedNumber;
            sawType = true;
            fields->parsedAny = true;
        }
    }
    if (!sawType) {
        fields->dataType = dataTypeFromValue(fields->attributeValue);
    }
    return true;
}

static bool parseHexPayload(const String &text, uint8_t *out, uint8_t *outLength, size_t outMax) {
    if (out == nullptr || outLength == nullptr) {
        return false;
    }
    int highNibble = -1;
    uint8_t filled = 0;
    for (unsigned index = 0; index < text.length(); index++) {
        const char character = text.charAt(index);
        if (character == ' ' || character == ':' || character == '-') {
            continue;
        }
        int nibble = -1;
        if (character >= '0' && character <= '9') {
            nibble = character - '0';
        } else if (character >= 'a' && character <= 'f') {
            nibble = 10 + (character - 'a');
        } else if (character >= 'A' && character <= 'F') {
            nibble = 10 + (character - 'A');
        } else {
            return false;
        }
        if (highNibble < 0) {
            highNibble = nibble;
            continue;
        }
        if (filled >= outMax) {
            return false;
        }
        out[filled++] = (uint8_t)((highNibble << 4) | nibble);
        highNibble = -1;
    }
    if (highNibble >= 0) {
        return false;
    }
    *outLength = filled;
    return true;
}

bool DeviceTopicMap::parseZclCommandBody(const char *body, uint8_t mappedEndpoint, ZclCommandFields *fields) {
    if (fields == nullptr) {
        return false;
    }
    memset(fields, 0, sizeof(*fields));
    fields->clusterId = kZigbeeClusterWindowCovering;
    fields->endpoint = mappedEndpoint;
    if (body == nullptr) {
        return true;
    }

    String trimmed = String(body);
    trimmed.trim();
    String shortcut = trimmed;
    shortcut.toLowerCase();
    if (shortcut == "open" || shortcut == "up") {
        fields->commandId = kZigbeeWindowCoveringCmdOpen;
        fields->parsed = true;
        return true;
    }
    if (shortcut == "close" || shortcut == "down") {
        fields->commandId = kZigbeeWindowCoveringCmdClose;
        fields->parsed = true;
        return true;
    }
    if (shortcut == "stop") {
        fields->commandId = kZigbeeWindowCoveringCmdStop;
        fields->parsed = true;
        return true;
    }

    String remaining = trimmed;
    bool sawCluster = false;
    bool sawCommand = false;
    while (remaining.length() > 0) {
        const int commaIndex = remaining.indexOf(',');
        String token = commaIndex >= 0 ? remaining.substring(0, commaIndex) : remaining;
        remaining = commaIndex >= 0 ? remaining.substring(commaIndex + 1) : "";
        token.trim();
        const int equalsIndex = token.indexOf('=');
        if (equalsIndex <= 0) {
            continue;
        }
        String key = token.substring(0, equalsIndex);
        String fieldValue = token.substring(equalsIndex + 1);
        key.trim();
        key.toLowerCase();
        fieldValue.trim();
        uint32_t parsedNumber = 0;
        if (key == "cl") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            fields->clusterId = (uint16_t)parsedNumber;
            sawCluster = true;
        } else if (key == "cmd") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            fields->commandId = (uint8_t)parsedNumber;
            sawCommand = true;
        } else if (key == "ep") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            const uint8_t parsedEndpoint = (uint8_t)parsedNumber;
            if (isUsableEndpoint(parsedEndpoint)) {
                fields->endpoint = parsedEndpoint;
            }
        } else if (key == "pl" || key == "payload") {
            if (!parseHexPayload(
                    fieldValue,
                    fields->payload,
                    &fields->payloadLength,
                    sizeof(fields->payload)
                )) {
                continue;
            }
        }
    }
    fields->parsed = sawCluster && sawCommand;
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
        if (!isUsableEndpoint((uint8_t)parsed)) {
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
    uint8_t preservedFullControl = 0;
    uint8_t preservedZigbeeType = ZigbeeDeviceTypeUnknown;
    if (entry != nullptr && entry->used) {
        preservedFullControl = entry->fullControl;
        preservedZigbeeType = entry->zigbeeType;
    }
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
    entry->fullControl = preservedFullControl;
    entry->zigbeeType = preservedZigbeeType;
    return entry;
}

DeviceTopicEntry *DeviceTopicMap::upsertFromEntry(const DeviceTopicEntry *source, bool keepExistingFullControl) {
    if (source == nullptr || !source->used) {
        return nullptr;
    }
    DeviceTopicEntry *entry = upsert(
        source->ieee,
        source->friendlyName,
        source->stateTopic,
        source->commandTopic,
        source->availabilityTopic,
        source->channelCount
    );
    if (entry == nullptr) {
        return nullptr;
    }
    if (!keepExistingFullControl || source->fullControl) {
        entry->fullControl = source->fullControl ? 1 : 0;
    }
    if (source->zigbeeType != ZigbeeDeviceTypeUnknown) {
        entry->zigbeeType = source->zigbeeType;
    }
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

void DeviceTopicMap::replaceFrom(const DeviceTopicMap *source) {
    if (slots == nullptr) {
        return;
    }
    if (source == nullptr || source->slots == nullptr) {
        clearAll();
        return;
    }
    memcpy(slots, source->slots, sizeof(DeviceTopicEntry) * DEVICE_MAP_SLOTS);
}

void DeviceTopicMap::copyFullControlFrom(const DeviceTopicMap *source) {
    if (slots == nullptr || source == nullptr) {
        return;
    }
    for (int i = 0; i < DEVICE_MAP_SLOTS; i++) {
        DeviceTopicEntry *entry = &slots[i];
        if (!entry->used) {
            continue;
        }
        const DeviceTopicEntry *previous = source->findByIeee(entry->ieee);
        if (previous != nullptr) {
            entry->fullControl = previous->fullControl;
        }
    }
}

void DeviceTopicMap::copyZigbeeTypeFrom(const DeviceTopicMap *source) {
    if (slots == nullptr || source == nullptr) {
        return;
    }
    for (int i = 0; i < DEVICE_MAP_SLOTS; i++) {
        DeviceTopicEntry *entry = &slots[i];
        if (!entry->used) {
            continue;
        }
        const DeviceTopicEntry *previous = source->findByIeee(entry->ieee);
        if (previous == nullptr) {
            continue;
        }
        entry->zigbeeType = mergeZigbeeDeviceType(entry->zigbeeType, previous->zigbeeType);
    }
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
            DeviceTopicEntry *entry = upsert(
                ieee,
                friendlyName.c_str(),
                stateTopic.c_str(),
                commandTopic.c_str(),
                availability.c_str(),
                normalizeChannelCount(parsedChannels)
            );
            bool parsedFullControl = false;
            if (entry != nullptr && extractJsonBool(object.c_str(), "fullControl", parsedFullControl)) {
                entry->fullControl = parsedFullControl ? 1 : 0;
            }
            const char *typeKey = strstr(object.c_str(), "\"type\":");
            if (entry != nullptr && typeKey != nullptr) {
                String typeText;
                if (extractJsonString(typeKey, "type", typeText)) {
                    entry->zigbeeType = zigbeeDeviceTypeFromJsonId(typeText.c_str());
                }
            }
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
    const String json = listStoreJson();
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
    const bool hasEntry = entry != nullptr
        && ((flags & SPI_DEVICE_SYNC_ENTRY) != 0 || (flags & SPI_DEVICE_SYNC_DELETE) != 0);
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
        out[SPI_DEVICE_SYNC_ENTRY_LEN_WITH_CHANNELS] = entry->fullControl ? 1 : 0;
        out[SPI_DEVICE_SYNC_ENTRY_LEN_WITH_FULL_CONTROL] = entry->zigbeeType;
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
    if ((*flags & (SPI_DEVICE_SYNC_ENTRY | SPI_DEVICE_SYNC_DELETE)) == 0) {
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
    if (length >= SPI_DEVICE_SYNC_ENTRY_LEN_WITH_CHANNELS) {
        entry->channelCount = normalizeChannelCount(in[SPI_DEVICE_SYNC_ENTRY_LEN_NO_CHANNELS]);
    } else {
        entry->channelCount = DEVICE_CHANNEL_COUNT_DEFAULT;
    }
    if (length >= SPI_DEVICE_SYNC_ENTRY_LEN_WITH_FULL_CONTROL) {
        entry->fullControl = in[SPI_DEVICE_SYNC_ENTRY_LEN_WITH_CHANNELS] != 0 ? 1 : 0;
    }
    if (length >= SPI_DEVICE_SYNC_ENTRY_LEN) {
        entry->zigbeeType = in[SPI_DEVICE_SYNC_ENTRY_LEN_WITH_FULL_CONTROL];
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
    return listJson(nullptr, nullptr);
}

String DeviceTopicMap::listJson(OnlineFn isOnline) {
    return listJson(isOnline, nullptr);
}

String DeviceTopicMap::listJson(OnlineFn isOnline, LastRssiFn lastRssi) {
    return listJson(isOnline, lastRssi, nullptr);
}

String DeviceTopicMap::listJson(OnlineFn isOnline, LastRssiFn lastRssi, ListTelemetryFn telemetry) {
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
        json += ",\"fullControl\":";
        json += entry->fullControl ? "true" : "false";
        json += ",\"type\":\"";
        json += zigbeeDeviceTypeJsonId(entry->zigbeeType);
        json += "\"";
        if (isOnline != nullptr) {
            json += ",\"online\":";
            json += isOnline(entry->ieee) ? "true" : "false";
        }
        if (lastRssi != nullptr) {
            int8_t rssiDbm = 0;
            if (lastRssi(entry->ieee, &rssiDbm)) {
                json += ",\"rssi\":";
                json += String((int)rssiDbm);
            }
        }
        if (telemetry != nullptr) {
            telemetry(entry->ieee, json);
        }
        json += "}";
    }
    json += "]";
    return json;
}

String DeviceTopicMap::listStoreJson() {
    return listJson(nullptr);
}
