#include "FoundDeviceList.h"
#include "JsonField.h"

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

void FoundDeviceList::noteJoin(const SpiFrame &frame, DeviceTopicMap *registered) {
    if (frame.cmd != SpiEvtDeviceJoin || frame.length < 75) {
        return;
    }
    uint8_t ieee[8];
    memcpy(ieee, frame.payload, 8);
    const uint16_t shortAddr = (uint16_t)frame.payload[8] | ((uint16_t)frame.payload[9] << 8);
    const uint8_t endpoint = frame.payload[10];
    noteIdentity(
        ieee,
        shortAddr,
        endpoint,
        (const char *)frame.payload + 11,
        (const char *)frame.payload + 43,
        registered
    );
}

void FoundDeviceList::noteIdentity(
    const uint8_t ieee[8],
    uint16_t shortAddr,
    uint8_t endpoint,
    const char *manufacturer,
    const char *model,
    DeviceTopicMap *registered
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
        json += "\"}";
    }
    json += "]";
    return json;
}
