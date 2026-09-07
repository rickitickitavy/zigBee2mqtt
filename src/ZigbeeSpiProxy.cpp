#include "ZigbeeSpiProxy.h"
#include "InterChipHost.h"
#include "Logger.h"

#include <string.h>

ZigbeeSpiProxy ZIGBEE_SPI_PROXY;

void ZigbeeSpiProxy::begin() {}

void ZigbeeSpiProxy::setLightStateHandler(LightStateFn handler) {
    lightStateHandler = handler;
}

bool ZigbeeSpiProxy::commandsAllowed() const {
    return INTER_CHIP_HOST.isNormal();
}

ZigbeeSpiProxy::CachedDevice *ZigbeeSpiProxy::findByIeee(const uint8_t ieee[8]) {
    for (int i = 0; i < kMaxDevices; i++) {
        if (devices[i].occupied && memcmp(devices[i].ieee, ieee, 8) == 0) {
            return &devices[i];
        }
    }
    return nullptr;
}

ZigbeeSpiProxy::CachedDevice *ZigbeeSpiProxy::allocSlot(const uint8_t ieee[8]) {
    CachedDevice *existing = findByIeee(ieee);
    if (existing != nullptr) {
        return existing;
    }
    for (int i = 0; i < kMaxDevices; i++) {
        if (!devices[i].occupied) {
            memset(&devices[i], 0, sizeof(devices[i]));
            memcpy(devices[i].ieee, ieee, 8);
            devices[i].occupied = true;
            return &devices[i];
        }
    }
    return nullptr;
}

void ZigbeeSpiProxy::permitJoin(uint8_t seconds) {
    INTER_CHIP_HOST.tryEnqueue(SpiCmdPermitJoin, &seconds, 1);
}

void ZigbeeSpiProxy::closeJoin() {
    uint8_t seconds = 0;
    INTER_CHIP_HOST.tryEnqueue(SpiCmdPermitJoin, &seconds, 1);
}

bool ZigbeeSpiProxy::controlOnOff(const uint8_t ieee[8], const char *command) {
    String action = String(command);
    action.trim();
    action.toLowerCase();
    uint8_t code = 0xFF;
    if (action == "off" || action == "0" || action == "false") {
        code = 0;
    } else if (action == "on" || action == "1" || action == "true") {
        code = 1;
    } else if (action == "toggle") {
        code = 2;
    } else {
        LOGGER.warning("Unknown on/off command: " + action);
        return false;
    }
    uint8_t payload[9];
    memcpy(payload, ieee, 8);
    payload[8] = code;
    return INTER_CHIP_HOST.tryEnqueue(SpiCmdZclOnOff, payload, 9);
}

void ZigbeeSpiProxy::onSpiEvent(const SpiFrame &frame) {
    if (frame.cmd == SpiEvtAttrReport && frame.length >= 12) {
        uint8_t ieee[8];
        memcpy(ieee, frame.payload, 8);
        const uint8_t endpoint = frame.payload[8];
        const uint16_t shortAddr = (uint16_t)frame.payload[9] | ((uint16_t)frame.payload[10] << 8);
        const bool on = frame.payload[11] != 0;
        CachedDevice *slot = allocSlot(ieee);
        if (slot != nullptr) {
            slot->endpoint = endpoint;
            slot->shortAddr = shortAddr;
        }
        if (lightStateHandler != nullptr) {
            lightStateHandler(on, ieee, endpoint, shortAddr);
        }
        return;
    }
    if (frame.cmd == SpiEvtDeviceJoin && frame.length >= 75) {
        uint8_t ieee[8];
        memcpy(ieee, frame.payload, 8);
        CachedDevice *slot = allocSlot(ieee);
        if (slot == nullptr) {
            return;
        }
        slot->shortAddr = (uint16_t)frame.payload[8] | ((uint16_t)frame.payload[9] << 8);
        slot->endpoint = frame.payload[10];
        strncpy(slot->manufacturer, (const char *)frame.payload + 11, sizeof(slot->manufacturer) - 1);
        strncpy(slot->model, (const char *)frame.payload + 43, sizeof(slot->model) - 1);
    }
}

String ZigbeeSpiProxy::devicesJson(DeviceTopicMap *topicMap) {
    String json = "[";
    bool first = true;
    for (int i = 0; i < kMaxDevices; i++) {
        CachedDevice *device = &devices[i];
        if (!device->occupied) {
            continue;
        }
        if (!first) {
            json += ",";
        }
        first = false;
        json += "{\"ieee\":\"";
        json += topicMap->formatIeee(device->ieee);
        json += "\",\"nwk\":\"0x";
        json += String(device->shortAddr, HEX);
        json += "\",\"endpoint\":";
        json += String(device->endpoint);
        json += ",\"manufacturer\":\"";
        json += device->manufacturer;
        json += "\",\"model\":\"";
        json += device->model;
        json += "\"";
        DeviceTopicEntry *mapped = topicMap->findByIeee(device->ieee);
        if (mapped != nullptr) {
            json += ",\"name\":\"";
            json += mapped->friendlyName;
            json += "\",\"state\":\"";
            json += mapped->stateTopic;
            json += "\",\"command\":\"";
            json += mapped->commandTopic;
            json += "\"";
        }
        json += "}";
    }
    json += "]";
    return json;
}
