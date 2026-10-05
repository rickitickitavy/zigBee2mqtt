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

static size_t jsonEscapeCap(const char *value) {
    if (value == nullptr) {
        return 0;
    }
    return strlen(value) + 1;
}

DeviceTopicMap::DeviceTopicMap() : head(nullptr) {}

DeviceTopicMap::~DeviceTopicMap() {
    clearAll();
}

DeviceTopicEntry *DeviceTopicMap::first() {
    return head;
}

const DeviceTopicEntry *DeviceTopicMap::first() const {
    return head;
}

DeviceTopicEntry *DeviceTopicMap::nextEntry(const DeviceTopicEntry *entry) {
    return entry != nullptr ? entry->next : nullptr;
}

char *DeviceTopicMap::duplicateBoundedString(const char *source, size_t maxLen) {
    if (maxLen == 0) {
        return nullptr;
    }
    if (source == nullptr) {
        source = "";
    }
    size_t copyLen = strlen(source);
    if (copyLen >= maxLen) {
        copyLen = maxLen - 1;
    }
    char *copy = (char *)malloc(copyLen + 1);
    if (copy == nullptr) {
        return nullptr;
    }
    memcpy(copy, source, copyLen);
    copy[copyLen] = '\0';
    return copy;
}

void DeviceTopicMap::clearEntryStrings(DeviceTopicEntry *entry) {
    if (entry == nullptr) {
        return;
    }
    free(entry->friendlyName);
    free(entry->stateTopic);
    free(entry->commandTopic);
    free(entry->availabilityTopic);
    entry->friendlyName = nullptr;
    entry->stateTopic = nullptr;
    entry->commandTopic = nullptr;
    entry->availabilityTopic = nullptr;
}

void DeviceTopicMap::freeEntry(DeviceTopicEntry *entry) {
    if (entry == nullptr) {
        return;
    }
    clearEntryStrings(entry);
    free(entry);
}

bool DeviceTopicMap::cloneEntry(DeviceTopicEntry *destination, const DeviceTopicEntry *source) {
    if (destination == nullptr || source == nullptr) {
        return false;
    }
    clearEntryStrings(destination);
    memcpy(destination->ieee, source->ieee, 8);
    destination->friendlyName =
        duplicateBoundedString(source->friendlyName, SPI_DEVICE_SYNC_NAME_LEN);
    destination->stateTopic =
        duplicateBoundedString(source->stateTopic, SPI_DEVICE_SYNC_TOPIC_LEN);
    destination->commandTopic =
        duplicateBoundedString(source->commandTopic, SPI_DEVICE_SYNC_TOPIC_LEN);
    destination->availabilityTopic =
        duplicateBoundedString(source->availabilityTopic, SPI_DEVICE_SYNC_TOPIC_LEN);
    if ((source->friendlyName != nullptr && source->friendlyName[0] != '\0' && destination->friendlyName == nullptr)
        || (source->stateTopic != nullptr && source->stateTopic[0] != '\0' && destination->stateTopic == nullptr)
        || (source->commandTopic != nullptr && source->commandTopic[0] != '\0' && destination->commandTopic == nullptr)
        || (source->availabilityTopic != nullptr
            && source->availabilityTopic[0] != '\0'
            && destination->availabilityTopic == nullptr)) {
        clearEntryStrings(destination);
        return false;
    }
    destination->channelCount = source->channelCount;
    destination->fullControl = source->fullControl;
    destination->zigbeeType = source->zigbeeType;
    destination->transport = source->transport;
    destination->used = source->used;
    destination->next = nullptr;
    return true;
}

void DeviceTopicMap::assignEntryStrings(
    DeviceTopicEntry *entry,
    const char *friendlyName,
    const char *stateTopic,
    const char *commandTopic,
    const char *availabilityTopic
) {
    if (entry == nullptr) {
        return;
    }
    free(entry->friendlyName);
    free(entry->stateTopic);
    free(entry->commandTopic);
    free(entry->availabilityTopic);
    entry->friendlyName = duplicateBoundedString(friendlyName, SPI_DEVICE_SYNC_NAME_LEN);
    entry->stateTopic = duplicateBoundedString(stateTopic, SPI_DEVICE_SYNC_TOPIC_LEN);
    entry->commandTopic = duplicateBoundedString(commandTopic, SPI_DEVICE_SYNC_TOPIC_LEN);
    entry->availabilityTopic = duplicateBoundedString(availabilityTopic, SPI_DEVICE_SYNC_TOPIC_LEN);
}

DeviceTopicEntry *DeviceTopicMap::allocateEntry() {
    if (usedCount() >= DEVICE_MAP_SLOTS) {
        return nullptr;
    }
    DeviceTopicEntry *entry = (DeviceTopicEntry *)calloc(1, sizeof(DeviceTopicEntry));
    if (entry == nullptr) {
        return nullptr;
    }
    entry->next = head;
    head = entry;
    return entry;
}

void DeviceTopicMap::unlinkEntry(DeviceTopicEntry *entry) {
    if (entry == nullptr || head == nullptr) {
        return;
    }
    if (head == entry) {
        head = entry->next;
        entry->next = nullptr;
        return;
    }
    DeviceTopicEntry *previous = head;
    while (previous->next != nullptr && previous->next != entry) {
        previous = previous->next;
    }
    if (previous->next == entry) {
        previous->next = entry->next;
        entry->next = nullptr;
    }
}

bool DeviceTopicMap::ieeeEqual(const uint8_t left[8], const uint8_t right[8]) {
    return memcmp(left, right, 8) == 0;
}

DeviceTopicEntry *DeviceTopicMap::slotAt(int index) {
    if (index < 0) {
        return nullptr;
    }
    int ordinal = 0;
    for (DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
        if (!entry->used) {
            continue;
        }
        if (ordinal == index) {
            return entry;
        }
        ordinal++;
    }
    return nullptr;
}

int DeviceTopicMap::slotIndex(const DeviceTopicEntry *entry) const {
    if (entry == nullptr) {
        return -1;
    }
    int index = 0;
    for (const DeviceTopicEntry *cursor = head; cursor != nullptr; cursor = cursor->next, index++) {
        if (cursor == entry) {
            return index;
        }
    }
    return -1;
}

int DeviceTopicMap::usedCount() const {
    int count = 0;
    for (const DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
        if (entry->used) {
            count++;
        }
    }
    return count;
}

int DeviceTopicMap::uniqueMqttTopicCount() const {
    const char *uniqueTopics[DEVICE_MAP_SLOTS * 2];
    int uniqueCount = 0;
    for (const DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
        if (!entry->used) {
            continue;
        }
        const char *candidateTopics[2] = {entry->stateTopic, entry->commandTopic};
        for (int topicIndex = 0; topicIndex < 2; topicIndex++) {
            const char *topic = candidateTopics[topicIndex];
            if (deviceTopicEmpty(topic)) {
                continue;
            }
            bool alreadySeen = false;
            for (int seenIndex = 0; seenIndex < uniqueCount; seenIndex++) {
                if (strcmp(uniqueTopics[seenIndex], topic) == 0) {
                    alreadySeen = true;
                    break;
                }
            }
            if (!alreadySeen) {
                uniqueTopics[uniqueCount] = topic;
                uniqueCount++;
            }
        }
    }
    return uniqueCount;
}

DeviceTopicEntry *DeviceTopicMap::findByIeee(const uint8_t ieee[8]) {
    return const_cast<DeviceTopicEntry *>(
        static_cast<const DeviceTopicMap *>(this)->findByIeee(ieee)
    );
}

const DeviceTopicEntry *DeviceTopicMap::findByIeee(const uint8_t ieee[8]) const {
    for (const DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
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
    if (entry == nullptr || deviceTopicEmpty(entry->stateTopic)) {
        return "";
    }
    if (usesTopicSuffix(entry->channelCount) && isUsableEndpoint(endpoint)) {
        return String(entry->stateTopic) + "/" + String(endpoint);
    }
    return String(entry->stateTopic);
}

String DeviceTopicMap::commandPublishTopic(const DeviceTopicEntry *entry, uint8_t endpoint) {
    if (entry == nullptr || deviceTopicEmpty(entry->commandTopic)) {
        return "";
    }
    if (usesTopicSuffix(entry->channelCount) && isUsableEndpoint(endpoint)) {
        return String(entry->commandTopic) + "/" + String(endpoint);
    }
    return String(entry->commandTopic);
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

static bool mappedTopicEquals(const char *mappedTopic, const char *topic) {
    while (*mappedTopic == ' ' || *mappedTopic == '\t') {
        mappedTopic++;
    }
    while (*topic == ' ' || *topic == '\t') {
        topic++;
    }
    while (*mappedTopic != '\0' && *topic != '\0' && *mappedTopic == *topic) {
        mappedTopic++;
        topic++;
    }
    while (*mappedTopic == ' ' || *mappedTopic == '\t') {
        mappedTopic++;
    }
    while (*topic == ' ' || *topic == '\t' || *topic == '\r' || *topic == '\n') {
        topic++;
    }
    return *mappedTopic == '\0' && *topic == '\0';
}

static DeviceTopicEntry *findByMappedTopic(
    DeviceTopicEntry *listHead,
    const char *topic,
    uint8_t *topicEndpoint,
    bool useStateTopic
) {
    if (topic == nullptr || topic[0] == '\0') {
        return nullptr;
    }
    if (topicEndpoint != nullptr) {
        *topicEndpoint = 0;
    }
    for (DeviceTopicEntry *entry = listHead; entry != nullptr; entry = entry->next) {
        if (!entry->used) {
            continue;
        }
        const char *mappedTopic = useStateTopic ? entry->stateTopic : entry->commandTopic;
        if (deviceTopicEmpty(mappedTopic)) {
            continue;
        }
        if (mappedTopicEquals(mappedTopic, topic)) {
            return entry;
        }
        if (!DeviceTopicMap::usesTopicSuffix(entry->channelCount)) {
            continue;
        }
        const size_t prefixLength = strlen(mappedTopic);
        if (strncmp(topic, mappedTopic, prefixLength) != 0 || topic[prefixLength] != '/') {
            continue;
        }
        const int parsed = atoi(topic + prefixLength + 1);
        if (!DeviceTopicMap::isUsableEndpoint((uint8_t)parsed)) {
            continue;
        }
        if (topicEndpoint != nullptr) {
            *topicEndpoint = (uint8_t)parsed;
        }
        return entry;
    }
    return nullptr;
}

DeviceTopicEntry *DeviceTopicMap::findByCommandTopic(const char *topic) {
    return findByCommandTopic(topic, nullptr);
}

DeviceTopicEntry *DeviceTopicMap::findByCommandTopic(const char *topic, uint8_t *topicEndpoint) {
    return findByMappedTopic(head, topic, topicEndpoint, false);
}

DeviceTopicEntry *DeviceTopicMap::findByStateTopic(const char *topic) {
    return findByStateTopic(topic, nullptr);
}

DeviceTopicEntry *DeviceTopicMap::findByStateTopic(const char *topic, uint8_t *topicEndpoint) {
    return findByMappedTopic(head, topic, topicEndpoint, true);
}

DeviceTopicEntry *DeviceTopicMap::findByAvailabilityTopic(const char *topic) {
    if (topic == nullptr || topic[0] == '\0') {
        return nullptr;
    }
    for (DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
        if (!entry->used || deviceTopicEmpty(entry->availabilityTopic)) {
            continue;
        }
        if (mappedTopicEquals(entry->availabilityTopic, topic)) {
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
    const char *availabilityTopic,
    uint8_t channelCount
) {
    DeviceTopicEntry *entry = findByIeee(ieee);
    uint8_t preservedFullControl = 0;
    uint8_t preservedZigbeeType = ZigbeeDeviceTypeUnknown;
    uint8_t preservedTransport = DeviceTransportZigbee;
    if (entry != nullptr && entry->used) {
        preservedFullControl = entry->fullControl;
        preservedZigbeeType = entry->zigbeeType;
        preservedTransport = clampDeviceTransport(entry->transport);
    }
    if (entry == nullptr) {
        entry = allocateEntry();
    }
    if (entry == nullptr) {
        return nullptr;
    }

    memcpy(entry->ieee, ieee, 8);
    entry->used = 1;
    assignEntryStrings(entry, friendlyName, stateTopic, commandTopic, availabilityTopic);
    entry->channelCount = normalizeChannelCount(channelCount);
    entry->fullControl = preservedFullControl;
    entry->zigbeeType = preservedZigbeeType;
    entry->transport = preservedTransport;
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
    entry->transport = clampDeviceTransport(source->transport);
    return entry;
}

bool DeviceTopicMap::removeByIeee(const uint8_t ieee[8]) {
    DeviceTopicEntry *entry = findByIeee(ieee);
    if (entry == nullptr) {
        return false;
    }
    unlinkEntry(entry);
    freeEntry(entry);
    return true;
}

void DeviceTopicMap::clearAll() {
    while (head != nullptr) {
        DeviceTopicEntry *next = head->next;
        freeEntry(head);
        head = next;
    }
    head = nullptr;
}

void DeviceTopicMap::replaceFrom(const DeviceTopicMap *source) {
    clearAll();
    if (source == nullptr) {
        return;
    }
    for (const DeviceTopicEntry *sourceEntry = source->head; sourceEntry != nullptr; sourceEntry = sourceEntry->next) {
        if (!sourceEntry->used) {
            continue;
        }
        upsertFromEntry(sourceEntry, false);
    }
}

void DeviceTopicMap::copyFullControlFrom(const DeviceTopicMap *source) {
    if (source == nullptr) {
        return;
    }
    for (DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
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
    if (source == nullptr) {
        return;
    }
    for (DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
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

void DeviceTopicMap::keepTransportEntriesFrom(const DeviceTopicMap *source, uint8_t transport) {
    if (source == nullptr) {
        return;
    }
    const uint8_t wantedTransport = clampDeviceTransport(transport);
    for (const DeviceTopicEntry *entry = source->head; entry != nullptr; entry = entry->next) {
        if (!entry->used || clampDeviceTransport(entry->transport) != wantedTransport) {
            continue;
        }
        upsertFromEntry(entry, false);
    }
}

int DeviceTopicMap::nextUsedIndex(int startIndex) const {
    if (startIndex < 0) {
        startIndex = 0;
    }
    int ordinal = 0;
    for (const DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
        if (!entry->used) {
            continue;
        }
        if (ordinal >= startIndex) {
            return ordinal;
        }
        ordinal++;
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
            if (entry != nullptr) {
                String transportText;
                if (extractJsonString(object.c_str(), "transport", transportText)) {
                    entry->transport = deviceTransportFromJsonId(transportText.c_str());
                } else {
                    entry->transport = DeviceTransportZigbee;
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
        strncpy((char *)out + 9, deviceTopicCStr(entry->friendlyName), SPI_DEVICE_SYNC_NAME_LEN - 1);
        strncpy((char *)out + 33, deviceTopicCStr(entry->stateTopic), SPI_DEVICE_SYNC_TOPIC_LEN - 1);
        strncpy((char *)out + 97, deviceTopicCStr(entry->commandTopic), SPI_DEVICE_SYNC_TOPIC_LEN - 1);
        strncpy((char *)out + 161, deviceTopicCStr(entry->availabilityTopic), SPI_DEVICE_SYNC_TOPIC_LEN - 1);
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
        // Callers pass a fresh/zeroed entry; do not free() garbage stack pointers.
        memset(entry, 0, sizeof(DeviceTopicEntry));
    }
    if ((*flags & (SPI_DEVICE_SYNC_ENTRY | SPI_DEVICE_SYNC_DELETE)) == 0) {
        return true;
    }
    if (length < SPI_DEVICE_SYNC_ENTRY_LEN_NO_CHANNELS || entry == nullptr) {
        return false;
    }
    memcpy(entry->ieee, in + 1, 8);
    entry->friendlyName = duplicateBoundedString((const char *)in + 9, SPI_DEVICE_SYNC_NAME_LEN);
    entry->stateTopic = duplicateBoundedString((const char *)in + 33, SPI_DEVICE_SYNC_TOPIC_LEN);
    entry->commandTopic = duplicateBoundedString((const char *)in + 97, SPI_DEVICE_SYNC_TOPIC_LEN);
    entry->availabilityTopic = duplicateBoundedString((const char *)in + 161, SPI_DEVICE_SYNC_TOPIC_LEN);
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
    entry->transport = DeviceTransportZigbee;
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

static void appendDeviceListEntryJson(
    DeviceTopicMap *deviceMap,
    String &json,
    const DeviceTopicEntry *deviceEntry,
    DeviceTopicMap::OnlineFn isOnline,
    DeviceTopicMap::LastRssiFn lastRssi,
    DeviceTopicMap::ListTelemetryFn telemetry
) {
    if (deviceEntry == nullptr) {
        json += "null";
        return;
    }
    json += "{\"ieee\":\"";
    json += deviceMap->formatIeee(deviceEntry->ieee);
    json += "\",\"name\":\"";
    appendJsonEscaped(json, deviceEntry->friendlyName, jsonEscapeCap(deviceEntry->friendlyName));
    json += "\",\"state\":\"";
    appendJsonEscaped(json, deviceEntry->stateTopic, jsonEscapeCap(deviceEntry->stateTopic));
    json += "\",\"command\":\"";
    appendJsonEscaped(json, deviceEntry->commandTopic, jsonEscapeCap(deviceEntry->commandTopic));
    json += "\",\"availability\":\"";
    appendJsonEscaped(json, deviceEntry->availabilityTopic, jsonEscapeCap(deviceEntry->availabilityTopic));
    json += "\",\"channels\":";
    json += String(deviceEntry->channelCount);
    json += ",\"fullControl\":";
    json += deviceEntry->fullControl ? "true" : "false";
    json += ",\"type\":\"";
    json += zigbeeDeviceTypeJsonId(deviceEntry->zigbeeType);
    json += "\",\"transport\":\"";
    json += deviceTransportJsonId(deviceEntry->transport);
    json += "\"";
    if (isOnline != nullptr) {
        json += ",\"online\":";
        json += isOnline(deviceEntry->ieee) ? "true" : "false";
    }
    if (lastRssi != nullptr) {
        int8_t rssiDbm = 0;
        if (lastRssi(deviceEntry->ieee, &rssiDbm)) {
            json += ",\"rssi\":";
            json += String((int)rssiDbm);
        }
    }
    if (telemetry != nullptr) {
        telemetry(deviceEntry->ieee, json);
    }
    json += "}";
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
    for (DeviceTopicEntry *deviceEntry = head; deviceEntry != nullptr; deviceEntry = deviceEntry->next) {
        if (!deviceEntry->used) {
            continue;
        }
        if (!first) {
            json += ",";
        }
        first = false;
        appendDeviceListEntryJson(this, json, deviceEntry, isOnline, lastRssi, telemetry);
    }
    json += "]";
    return json;
}

String DeviceTopicMap::entryJson(
    const uint8_t ieee[8],
    OnlineFn isOnline,
    LastRssiFn lastRssi,
    ListTelemetryFn telemetry
) {
    const DeviceTopicEntry *deviceEntry = findByIeee(ieee);
    if (deviceEntry == nullptr || !deviceEntry->used) {
        return String();
    }
    String json;
    appendDeviceListEntryJson(this, json, deviceEntry, isOnline, lastRssi, telemetry);
    return json;
}

String DeviceTopicMap::listStoreJson() {
    return listJson(nullptr);
}
