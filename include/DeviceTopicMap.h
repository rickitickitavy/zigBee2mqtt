#pragma once

#include "GlobalSettings.h"
#include "SpiProtocol.h"
#include <Arduino.h>
#include <stddef.h>

class DeviceTopicMap {
public:
    explicit DeviceTopicMap(DeviceTopicEntry *slots);

    DeviceTopicEntry *findByIeee(const uint8_t ieee[8]);
    const DeviceTopicEntry *findByIeee(const uint8_t ieee[8]) const;
    DeviceTopicEntry *findByCommandTopic(const char *topic);
    DeviceTopicEntry *findByCommandTopic(const char *topic, uint8_t *topicEndpoint);
    DeviceTopicEntry *upsert(
        const uint8_t ieee[8],
        const char *friendlyName,
        const char *stateTopic,
        const char *commandTopic,
        const char *availabilityTopic,
        uint8_t channelCount = DEVICE_CHANNEL_COUNT_DEFAULT
    );
    DeviceTopicEntry *upsertFromEntry(const DeviceTopicEntry *source, bool keepExistingFullControl);
    static uint8_t normalizeChannelCount(int raw);
    static bool usesPayloadParse(uint8_t channelCount);
    static bool usesTopicSuffix(uint8_t channelCount);
    static bool isUsableEndpoint(uint8_t endpoint);
    static String statePublishTopic(const DeviceTopicEntry *entry, uint8_t endpoint);
    static String statePublishPayload(const DeviceTopicEntry *entry, uint8_t endpoint, const char *message);
    static bool parseChannelPayload(const char *payload, uint8_t *endpoint, String *action);

    struct ZclWriteFields {
        uint16_t clusterId;
        uint16_t attributeId;
        uint32_t attributeValue;
        uint8_t endpoint;
        uint8_t dataType;
        bool parsedAny;
        bool hasWrite;
    };
    struct ZclCommandFields {
        uint16_t clusterId;
        uint8_t commandId;
        uint8_t endpoint;
        uint8_t payload[SPI_DEVICE_MESSAGE_MAX];
        uint8_t payloadLength;
        bool parsed;
    };
    static bool parseFullControlBody(const char *body, uint8_t mappedEndpoint, ZclWriteFields *fields);
    static bool parseZclCommandBody(const char *body, uint8_t mappedEndpoint, ZclCommandFields *fields);
    bool removeByIeee(const uint8_t ieee[8]);
    void clearAll();
    void replaceFrom(const DeviceTopicMap *source);
    void copyFullControlFrom(const DeviceTopicMap *source);
    void copyZigbeeTypeFrom(const DeviceTopicMap *source);
    int slotIndex(const DeviceTopicEntry *entry) const;
    DeviceTopicEntry *slotAt(int index);
    int usedCount() const;
    int uniqueMqttTopicCount() const;
    int nextUsedIndex(int startIndex) const;

    String formatIeee(const uint8_t ieee[8]);
    bool parseIeee(const char *text, uint8_t ieee[8]);
    using OnlineFn = bool (*)(const uint8_t ieee[8]);
    using LastRssiFn = bool (*)(const uint8_t ieee[8], int8_t *rssiDbm);
    using ListTelemetryFn = void (*)(const uint8_t ieee[8], String &json);

    String listJson();
    String listJson(OnlineFn isOnline);
    String listJson(OnlineFn isOnline, LastRssiFn lastRssi);
    String listJson(OnlineFn isOnline, LastRssiFn lastRssi, ListTelemetryFn telemetry);
    String listStoreJson();
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
