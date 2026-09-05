#include "ZigbeeCoordinator.h"
#include "Logger.h"
#include "Defines.h"

#include <string.h>

static constexpr uint8_t kSwitchEndpoint = 5;

ZigbeeCoordinator::ZigbeeCoordinator() : zigbeeSwitch(kSwitchEndpoint) {
    memset(boundDevices, 0, sizeof(boundDevices));
}

void ZigbeeCoordinator::setLightStateHandler(LightStateFn handler) {
    lightStateHandler = handler;
}

void ZigbeeCoordinator::attachLibraryCallbacks(void (*withSource)(bool, uint8_t, esp_zb_zcl_addr_t)) {
    zigbeeSwitch.onLightStateChangeWithSource(withSource);
}

bool ZigbeeCoordinator::begin(uint8_t channel, uint8_t permitJoinSec) {
    zigbeeSwitch.setManufacturerAndModel("z2m-gateway", "ESP32-C6");
    zigbeeSwitch.allowMultipleBinding(true);
    Zigbee.addEndpoint(&zigbeeSwitch);

    if (channel >= 11 && channel <= 26) {
        Zigbee.setPrimaryChannelMask(1UL << channel);
    }

    if (permitJoinSec > 0) {
        Zigbee.setRebootOpenNetwork(permitJoinSec);
    }

    LOGGER.info("Starting Zigbee coordinator on channel " + String(channel));
    if (!Zigbee.begin(ZIGBEE_COORDINATOR)) {
        LOGGER.error("Zigbee.begin failed");
        return false;
    }

    LOGGER.info("Zigbee coordinator started");
    return true;
}

void ZigbeeCoordinator::permitJoin(uint8_t seconds) {
    LOGGER.info("Permit join " + String(seconds) + "s");
    Zigbee.openNetwork(seconds);
}

void ZigbeeCoordinator::closeJoin() {
    LOGGER.info("Closing join window");
    Zigbee.closeNetwork();
}

void ZigbeeCoordinator::storeBoundDevice(zb_device_params_t *device) {
    if (device == nullptr) {
        return;
    }

    BoundZigbeeDevice *slot = findByIeee(device->ieee_addr);
    if (slot == nullptr) {
        for (int i = 0; i < kMaxBoundDevices; i++) {
            if (!boundDevices[i].occupied) {
                slot = &boundDevices[i];
                break;
            }
        }
    }
    if (slot == nullptr) {
        LOGGER.warning("Bound device table full");
        return;
    }

    memset(slot, 0, sizeof(BoundZigbeeDevice));
    memcpy(slot->ieee, device->ieee_addr, 8);
    slot->shortAddr = device->short_addr;
    slot->endpoint = device->endpoint;
    slot->occupied = true;

    char *manufacturer = zigbeeSwitch.readManufacturer(device->endpoint, device->short_addr, device->ieee_addr);
    char *model = zigbeeSwitch.readModel(device->endpoint, device->short_addr, device->ieee_addr);
    if (manufacturer != nullptr) {
        strncpy(slot->manufacturer, manufacturer, sizeof(slot->manufacturer) - 1);
    }
    if (model != nullptr) {
        strncpy(slot->model, model, sizeof(slot->model) - 1);
    }
}

void ZigbeeCoordinator::refreshBoundDevices() {
    std::list<zb_device_params_t *> boundLights = zigbeeSwitch.getBoundDevices();
    for (zb_device_params_t *device : boundLights) {
        storeBoundDevice(device);
    }
}

void ZigbeeCoordinator::dispatch() {
    if ((millis() - lastRefreshMs) < 10000) {
        return;
    }
    lastRefreshMs = millis();
    if (zigbeeSwitch.bound()) {
        refreshBoundDevices();
        zigbeeSwitch.getLightState();
    }
}

BoundZigbeeDevice *ZigbeeCoordinator::findByIeee(const uint8_t ieee[8]) {
    for (int i = 0; i < kMaxBoundDevices; i++) {
        if (boundDevices[i].occupied && memcmp(boundDevices[i].ieee, ieee, 8) == 0) {
            return &boundDevices[i];
        }
    }
    return nullptr;
}

BoundZigbeeDevice *ZigbeeCoordinator::findByShortAddr(uint16_t shortAddr) {
    for (int i = 0; i < kMaxBoundDevices; i++) {
        if (boundDevices[i].occupied && boundDevices[i].shortAddr == shortAddr) {
            return &boundDevices[i];
        }
    }
    return nullptr;
}

void ZigbeeCoordinator::resolveIeeeFromSource(esp_zb_zcl_addr_t source, uint8_t ieee[8], uint16_t *shortAddr) {
    memset(ieee, 0, 8);
    *shortAddr = 0;
    if (source.addr_type == ESP_ZB_ZCL_ADDR_TYPE_IEEE) {
        memcpy(ieee, source.u.ieee_addr, 8);
        BoundZigbeeDevice *known = findByIeee(ieee);
        if (known != nullptr) {
            *shortAddr = known->shortAddr;
        }
        return;
    }
    *shortAddr = source.u.short_addr;
    BoundZigbeeDevice *known = findByShortAddr(*shortAddr);
    if (known != nullptr) {
        memcpy(ieee, known->ieee, 8);
    }
}

void ZigbeeCoordinator::handleLightStateWithSource(bool on, uint8_t endpoint, esp_zb_zcl_addr_t source) {
    uint8_t ieee[8];
    uint16_t shortAddr = 0;
    resolveIeeeFromSource(source, ieee, &shortAddr);
    LOGGER.info(
        String("On/Off ") + (on ? "ON" : "OFF") + " ep=" + String(endpoint) + " nwk=" + String(shortAddr, HEX)
    );
    if (lightStateHandler != nullptr) {
        lightStateHandler(on, ieee, endpoint, shortAddr);
    }
}

bool ZigbeeCoordinator::controlOnOff(const uint8_t ieee[8], const char *command) {
    BoundZigbeeDevice *device = findByIeee(ieee);
    if (device == nullptr) {
        LOGGER.warning("No bound Zigbee device for command");
        return false;
    }

    String action = String(command);
    action.trim();
    action.toLowerCase();

    esp_zb_ieee_addr_t ieeeAddr;
    memcpy(ieeeAddr, device->ieee, 8);

    if (action == "on" || action == "1" || action == "true") {
        zigbeeSwitch.lightOn(device->endpoint, ieeeAddr);
        return true;
    }
    if (action == "off" || action == "0" || action == "false") {
        zigbeeSwitch.lightOff(device->endpoint, ieeeAddr);
        return true;
    }
    if (action == "toggle") {
        zigbeeSwitch.lightToggle(device->endpoint, ieeeAddr);
        return true;
    }
    LOGGER.warning("Unknown on/off command: " + action);
    return false;
}

String ZigbeeCoordinator::devicesJson(DeviceTopicMap *topicMap) {
    String json = "[";
    bool first = true;
    for (int i = 0; i < kMaxBoundDevices; i++) {
        BoundZigbeeDevice *device = &boundDevices[i];
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
