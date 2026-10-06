#include "FoundDeviceList.h"
#include "JsonField.h"
#include "ZigbeeDeviceType.h"

#include <stdlib.h>
#include <string.h>

FoundDeviceList::FoundDeviceList() : foundHead(nullptr), foundCount(0) {}

FoundDeviceList::~FoundDeviceList() {
    clear();
}

void FoundDeviceList::freeFoundDevice(FoundDevice *device) {
    if (device == nullptr) {
        return;
    }
    free(device->manufacturer);
    free(device->model);
    free(device);
}

FoundDeviceList::FoundDevice *FoundDeviceList::allocateFoundDevice() {
    if (foundCount >= kMaxFound) {
        return nullptr;
    }
    FoundDevice *device = (FoundDevice *)calloc(1, sizeof(FoundDevice));
    if (device == nullptr) {
        return nullptr;
    }
    device->next = foundHead;
    foundHead = device;
    foundCount++;
    return device;
}

FoundDeviceList::FoundDevice *FoundDeviceList::findByIeee(const uint8_t ieee[8]) {
    return const_cast<FoundDevice *>(static_cast<const FoundDeviceList *>(this)->findByIeee(ieee));
}

const FoundDeviceList::FoundDevice *FoundDeviceList::findByIeee(const uint8_t ieee[8]) const {
    if (ieee == nullptr) {
        return nullptr;
    }
    for (const FoundDevice *device = foundHead; device != nullptr; device = device->next) {
        if (device->used && DeviceTopicMap::ieeeEqual(device->ieee, ieee)) {
            return device;
        }
    }
    return nullptr;
}

void FoundDeviceList::clear() {
    while (foundHead != nullptr) {
        FoundDevice *next = foundHead->next;
        freeFoundDevice(foundHead);
        foundHead = next;
    }
    foundCount = 0;
}

void FoundDeviceList::removeIeee(const uint8_t ieee[8]) {
    FoundDevice **link = &foundHead;
    while (*link != nullptr) {
        FoundDevice *device = *link;
        if (device->used && DeviceTopicMap::ieeeEqual(device->ieee, ieee)) {
            *link = device->next;
            freeFoundDevice(device);
            foundCount--;
            return;
        }
        link = &device->next;
    }
}

uint8_t FoundDeviceList::zigbeeTypeForIeee(const uint8_t ieee[8]) const {
    const FoundDevice *device = findByIeee(ieee);
    if (device == nullptr) {
        return ZigbeeDeviceTypeUnknown;
    }
    return device->zigbeeType;
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
    FoundDevice *slot = findByIeee(ieee);
    uint8_t preservedType = ZigbeeDeviceTypeUnknown;
    if (slot != nullptr) {
        preservedType = slot->zigbeeType;
    }
    if (slot == nullptr) {
        slot = allocateFoundDevice();
        if (slot == nullptr && foundHead != nullptr) {
            FoundDevice *oldest = foundHead;
            while (oldest->next != nullptr) {
                oldest = oldest->next;
            }
            FoundDevice **link = &foundHead;
            while (*link != oldest) {
                link = &(*link)->next;
            }
            *link = nullptr;
            freeFoundDevice(oldest);
            foundCount--;
            slot = allocateFoundDevice();
        }
    }
    if (slot == nullptr) {
        return;
    }
    free(slot->manufacturer);
    free(slot->model);
    slot->manufacturer = DeviceTopicMap::duplicateBoundedString(manufacturer, 32);
    slot->model = DeviceTopicMap::duplicateBoundedString(model, 32);
    memcpy(slot->ieee, ieee, 8);
    slot->shortAddr = shortAddr;
    slot->endpoint = endpoint;
    slot->zigbeeType = mergeZigbeeDeviceType(preservedType, zigbeeType);
    slot->used = true;
}

String FoundDeviceList::listJson(DeviceTopicMap *formatter) {
    String json = "[";
    bool first = true;
    for (const FoundDevice *device = foundHead; device != nullptr; device = device->next) {
        if (!device->used) {
            continue;
        }
        if (formatter != nullptr && formatter->findByIeee(device->ieee) != nullptr) {
            continue;
        }
        if (!first) {
            json += ",";
        }
        first = false;
        json += "{\"ieee\":\"";
        if (formatter != nullptr) {
            json += formatter->formatIeee(device->ieee);
        }
        json += "\",\"nwk\":";
        json += String(device->shortAddr);
        json += ",\"endpoint\":";
        json += String(device->endpoint);
        json += ",\"manufacturer\":\"";
        appendJsonEscaped(json, device->manufacturer, device->manufacturer ? strlen(device->manufacturer) + 1 : 0);
        json += "\",\"model\":\"";
        appendJsonEscaped(json, device->model, device->model ? strlen(device->model) + 1 : 0);
        json += "\",\"type\":\"";
        json += zigbeeDeviceTypeJsonId(device->zigbeeType);
        json += "\"}";
    }
    json += "]";
    return json;
}
