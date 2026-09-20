#include "FoundDeviceList.h"
#include "JsonField.h"
#include "ZigbeeDeviceType.h"

#include <string.h>

void FoundDeviceList::clear() {
    memset(found, 0, sizeof(found));
}

void FoundDeviceList::removeIeee(const uint8_t ieee[8]) {
    for (int i = 0; i < kMaxFound; i++) {
        if (found[i].used && DeviceTopicMap::ieeeEqual(found[i].ieee, ieee)) {
            found[i].used = false;
        }
    }
}

uint8_t FoundDeviceList::zigbeeTypeForIeee(const uint8_t ieee[8]) const {
    if (ieee == nullptr) {
        return ZigbeeDeviceTypeUnknown;
    }
    for (int i = 0; i < kMaxFound; i++) {
        if (found[i].used && DeviceTopicMap::ieeeEqual(found[i].ieee, ieee)) {
            return found[i].zigbeeType;
        }
    }
    return ZigbeeDeviceTypeUnknown;
}

bool FoundDeviceList::noteJoin(const SpiFrame &frame, DeviceTopicMap *registered) {
    if (frame.cmd != SpiEvtDeviceJoin || frame.length < SPI_DEVICE_JOIN_MIN_LEN) {
        return false;
    }
    uint8_t ieee[8];
    memcpy(ieee, frame.payload, 8);
    const uint16_t shortAddr =
        (uint16_t)frame.payload[SPI_DEVICE_JOIN_NWK_OFFSET]
        | ((uint16_t)frame.payload[SPI_DEVICE_JOIN_NWK_OFFSET + 1] << 8);
    const uint8_t endpoint = frame.payload[SPI_DEVICE_JOIN_ENDPOINT_OFFSET];
    const uint8_t joinType = spiDeviceJoinType(frame.payload, frame.length);
    if (registered != nullptr) {
        DeviceTopicEntry *entry = registered->findByIeee(ieee);
        if (entry != nullptr && entry->used) {
            if (entry->zigbeeType == ZigbeeDeviceTypeUnknown && joinType != ZigbeeDeviceTypeUnknown) {
                entry->zigbeeType = joinType;
                return true;
            }
            return false;
        }
    }
    noteIdentity(
        ieee,
        shortAddr,
        endpoint,
        (const char *)frame.payload + SPI_DEVICE_JOIN_MANUFACTURER_OFFSET,
        (const char *)frame.payload + SPI_DEVICE_JOIN_MODEL_OFFSET,
        registered,
        joinType
    );
    return false;
}

void FoundDeviceList::noteIdentity(
    const uint8_t ieee[8],
    uint16_t shortAddr,
    uint8_t endpoint,
    const char *manufacturer,
    const char *model,
    DeviceTopicMap *registered,
    uint8_t zigbeeType
) {
    if (ieee == nullptr || registered == nullptr) {
        return;
    }
    if (shortAddr == 0 || shortAddr == 0xFFFF || !DeviceTopicMap::isUsableEndpoint(endpoint)) {
        return;
    }
    bool ieeePresent = false;
    for (int i = 0; i < 8; i++) {
        if (ieee[i] != 0) {
            ieeePresent = true;
            break;
        }
    }
    if (!ieeePresent) {
        return;
    }
    if (registered->findByIeee(ieee) != nullptr) {
        return;
    }
    FoundDevice *slot = nullptr;
    for (int i = 0; i < kMaxFound; i++) {
        if (found[i].used && DeviceTopicMap::ieeeEqual(found[i].ieee, ieee)) {
            slot = &found[i];
            break;
        }
    }
    uint8_t preservedType = ZigbeeDeviceTypeUnknown;
    if (slot != nullptr) {
        preservedType = slot->zigbeeType;
    }
    if (slot == nullptr) {
        for (int i = 0; i < kMaxFound; i++) {
            if (!found[i].used) {
                slot = &found[i];
                break;
            }
        }
    }
    if (slot == nullptr) {
        slot = &found[0];
        preservedType = ZigbeeDeviceTypeUnknown;
    }
    memset(slot, 0, sizeof(*slot));
    memcpy(slot->ieee, ieee, 8);
    slot->shortAddr = shortAddr;
    slot->endpoint = endpoint;
    if (manufacturer != nullptr) {
        strncpy(slot->manufacturer, manufacturer, sizeof(slot->manufacturer) - 1);
    }
    if (model != nullptr) {
        strncpy(slot->model, model, sizeof(slot->model) - 1);
    }
    slot->zigbeeType = mergeZigbeeDeviceType(preservedType, zigbeeType);
    slot->used = true;
}

String FoundDeviceList::listJson(DeviceTopicMap *formatter) {
    String json = "[";
    bool first = true;
    for (int i = 0; i < kMaxFound; i++) {
        if (!found[i].used) {
            continue;
        }
        if (formatter != nullptr && formatter->findByIeee(found[i].ieee) != nullptr) {
            continue;
        }
        if (!first) {
            json += ",";
        }
        first = false;
        json += "{\"ieee\":\"";
        if (formatter != nullptr) {
            json += formatter->formatIeee(found[i].ieee);
        }
        json += "\",\"nwk\":";
        json += String(found[i].shortAddr);
        json += ",\"endpoint\":";
        json += String(found[i].endpoint);
        json += ",\"manufacturer\":\"";
        appendJsonEscaped(json, found[i].manufacturer, sizeof(found[i].manufacturer));
        json += "\",\"model\":\"";
        appendJsonEscaped(json, found[i].model, sizeof(found[i].model));
        json += "\",\"type\":\"";
        json += zigbeeDeviceTypeJsonId(found[i].zigbeeType);
        json += "\"}";
    }
    json += "]";
    return json;
}
