#include "ZigbeeCoordinator.h"
#include "Logger.h"
#include "Defines.h"
#include "StatusRgb.h"

#include <stdio.h>
#include <string.h>

static constexpr uint8_t kSwitchEndpoint = 5;

static String formatIeeeText(const uint8_t ieee[8]) {
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

static bool isZeroIeee(const uint8_t ieee[8]) {
    static const uint8_t kZeroIeee[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    return ieee != nullptr && memcmp(ieee, kZeroIeee, 8) == 0;
}

static void logDeviceEvent(
    const char *eventName,
    const uint8_t ieee[8],
    uint16_t shortAddr,
    uint8_t endpoint,
    const char *deviceName
) {
    String line = String("Device event ") + eventName;
    if (deviceName != nullptr && deviceName[0] != '\0') {
        line += " name=";
        line += deviceName;
    }
    line += " ieee=" + formatIeeeText(ieee);
    line += " nwk=0x" + String(shortAddr, HEX);
    line += " ep=" + String(endpoint);
    LOGGER.info(line);
}

static void addIasZoneClient(esp_zb_cluster_list_t *clusterList) {
    if (clusterList == nullptr) {
        return;
    }
    esp_zb_cluster_list_add_ias_zone_cluster(
        clusterList,
        esp_zb_ias_zone_cluster_create(nullptr),
        ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE
    );
}

ZigbeeCoordinator::CoordinatorSwitch::CoordinatorSwitch(uint8_t endpoint, ZigbeeCoordinator *coordinator)
    : ZigbeeSwitch(endpoint), owner(coordinator) {
    addIasZoneClient(_cluster_list);
}

void ZigbeeCoordinator::CoordinatorSwitch::zbIASZoneStatusChangeNotification(
    const esp_zb_zcl_ias_zone_status_change_notification_message_t *message
) {
    if (owner != nullptr) {
        owner->handleIasZoneStatus(message);
    }
}

void ZigbeeCoordinator::CoordinatorSwitch::zbIASZoneEnrollRequest(
    const esp_zb_zcl_ias_zone_enroll_request_message_t *message
) {
    if (owner != nullptr) {
        owner->handleIasZoneEnroll(this, message);
    }
}

ZigbeeCoordinator::IasCieEndpoint::IasCieEndpoint(uint8_t endpoint, ZigbeeCoordinator *coordinator)
    : ZigbeeEP(endpoint), owner(coordinator) {
    _device_id = ESP_ZB_HA_IAS_CONTROL_INDICATING_EQUIPMENT_ID;
    _cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(_cluster_list, esp_zb_basic_cluster_create(nullptr), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(
        _cluster_list,
        esp_zb_identify_cluster_create(nullptr),
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE
    );
    addIasZoneClient(_cluster_list);
    _ep_config = {
        .endpoint = _endpoint,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_IAS_CONTROL_INDICATING_EQUIPMENT_ID,
        .app_device_version = 0
    };
}

void ZigbeeCoordinator::IasCieEndpoint::zbAttributeRead(
    uint16_t clusterId,
    const esp_zb_zcl_attribute_t *attribute,
    uint8_t srcEndpoint,
    esp_zb_zcl_addr_t srcAddress
) {
    if (owner != nullptr) {
        owner->handleAttributeReport(clusterId, attribute, srcEndpoint, srcAddress);
    }
}

void ZigbeeCoordinator::IasCieEndpoint::zbIASZoneStatusChangeNotification(
    const esp_zb_zcl_ias_zone_status_change_notification_message_t *message
) {
    if (owner != nullptr) {
        owner->handleIasZoneStatus(message);
    }
}

void ZigbeeCoordinator::IasCieEndpoint::zbIASZoneEnrollRequest(
    const esp_zb_zcl_ias_zone_enroll_request_message_t *message
) {
    if (owner != nullptr) {
        owner->handleIasZoneEnroll(this, message);
    }
}

ZigbeeCoordinator::ZigbeeCoordinator() : zigbeeSwitch(kSwitchEndpoint, this), iasCie(1, this) {
    memset(boundDevices, 0, sizeof(boundDevices));
}

void ZigbeeCoordinator::setLightStateHandler(LightStateFn handler) {
    lightStateHandler = handler;
}

void ZigbeeCoordinator::setDeviceBoundHandler(DeviceBoundFn handler) {
    deviceBoundHandler = handler;
}

void ZigbeeCoordinator::setRegistryChangedHandler(RegistryChangedFn handler) {
    registryChangedHandler = handler;
}

void ZigbeeCoordinator::attachLibraryCallbacks(void (*withSource)(bool, uint8_t, esp_zb_zcl_addr_t)) {
    zigbeeSwitch.onLightStateChangeWithSource(withSource);
}

bool ZigbeeCoordinator::begin(uint8_t channel, uint8_t permitJoinSec) {
    zigbeeSwitch.setManufacturerAndModel("z2m-gateway", "ESP32-C6");
    zigbeeSwitch.allowMultipleBinding(true);
    iasCie.setManufacturerAndModel("z2m-gateway", "ESP32-C6");
    Zigbee.addEndpoint(&zigbeeSwitch);
    Zigbee.addEndpoint(&iasCie);

    if (channel >= 11 && channel <= 26) {
        Zigbee.setPrimaryChannelMask(1UL << channel);
    }

    if (permitJoinSec > 0) {
        Zigbee.setRebootOpenNetwork(permitJoinSec);
    }

    LOGGER.info("Starting Zigbee coordinator on channel " + String(channel));
    if (!Zigbee.begin(ZIGBEE_COORDINATOR)) {
        LOGGER.error("Zigbee.begin failed");
        STATUS_RGB.setCritical(true);
        return false;
    }

    started = true;
    lastRefreshMs = millis();
    esp_zb_ieee_addr_t localIeee;
    memset(localIeee, 0, sizeof(localIeee));
    esp_zb_get_long_address(localIeee);
    LOGGER.info("Zigbee coordinator started ieee=" + formatIeeeText(localIeee));
    if (permitJoinSec > 0) {
        startPairingWindow(permitJoinSec);
    }
    return true;
}

bool ZigbeeCoordinator::isStarted() const {
    return started;
}

void ZigbeeCoordinator::permitJoin(uint8_t seconds) {
    if (!started) {
        LOGGER.warning("Zigbee is not started");
        return;
    }
    LOGGER.info("Permit join " + String(seconds) + "s");
    Zigbee.openNetwork(seconds);
    startPairingWindow(seconds);
}

void ZigbeeCoordinator::closeJoin() {
    if (!started) {
        return;
    }
    LOGGER.info("Closing join window");
    Zigbee.closeNetwork();
    stopPairingWindow();
}

void ZigbeeCoordinator::startPairingWindow(uint8_t seconds) {
    if (seconds == 0) {
        stopPairingWindow();
        return;
    }
    pairingUntilMs = millis() + (unsigned long)seconds * 1000UL;
    pairingLedToggleMs = 0;
    pairingLedOn = false;
    STATUS_RGB.setPairingHeld(true);
    for (int i = 0; i < kMaxBoundDevices; i++) {
        boundDevices[i].pairingOffered = false;
    }
}

void ZigbeeCoordinator::stopPairingWindow() {
    pairingUntilMs = 0;
    pairingLedOn = false;
    STATUS_RGB.setPairingHeld(false);
}

void ZigbeeCoordinator::updatePairingLed() {
    if (pairingUntilMs == 0) {
        return;
    }
    if ((long)(millis() - pairingUntilMs) >= 0) {
        stopPairingWindow();
        return;
    }
    if (!STATUS_RGB.allowsPairingBlink()) {
        return;
    }
    if ((millis() - pairingLedToggleMs) < 250) {
        return;
    }
    pairingLedToggleMs = millis();
    pairingLedOn = !pairingLedOn;
    STATUS_RGB.writePairingPhase(pairingLedOn);
}

void ZigbeeCoordinator::storeBoundDevice(zb_device_params_t *device) {
    if (device == nullptr) {
        return;
    }

    BoundZigbeeDevice *slot = findByIeee(device->ieee_addr);
    const bool isNewDevice = slot == nullptr;
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

    uint16_t shortAddr = device->short_addr;
    if (shortAddr == 0xFFFF || shortAddr == 0) {
        const uint16_t resolved = esp_zb_address_short_by_ieee(device->ieee_addr);
        if (resolved != 0 && resolved != 0xFFFF) {
            shortAddr = resolved;
        }
    }
    const uint8_t endpoint = device->endpoint;
    const bool incomingUsable = shortAddr != 0 && shortAddr != 0xFFFF
        && DeviceTopicMap::isUsableEndpoint(endpoint) && !isZeroIeee(device->ieee_addr);

    if (!incomingUsable) {
        if (!isNewDevice) {
            return;
        }
        memset(slot, 0, sizeof(BoundZigbeeDevice));
        memcpy(slot->ieee, device->ieee_addr, 8);
        slot->shortAddr = shortAddr;
        slot->endpoint = endpoint;
        slot->occupied = true;
        LOGGER.info(
            "Ignoring incomplete join ieee=" + formatIeeeText(slot->ieee)
            + " nwk=0x" + String(shortAddr, HEX) + " ep=" + String(endpoint)
        );
        return;
    }

    if (isNewDevice) {
        memset(slot, 0, sizeof(BoundZigbeeDevice));
        memcpy(slot->ieee, device->ieee_addr, 8);
    }
    const bool wasIncomplete = !isNewDevice
        && (slot->shortAddr == 0 || slot->shortAddr == 0xFFFF
            || !DeviceTopicMap::isUsableEndpoint(slot->endpoint));
    const bool addressChanged = !isNewDevice
        && (slot->shortAddr != shortAddr || slot->endpoint != endpoint);
    slot->shortAddr = shortAddr;
    slot->endpoint = endpoint;
    slot->occupied = true;
    if (isNewDevice || wasIncomplete) {
        logDeviceEvent("join", slot->ieee, slot->shortAddr, slot->endpoint, registeredName(slot->ieee));
    }
    if (deviceBoundHandler != nullptr && (isNewDevice || addressChanged || wasIncomplete || !slot->pairingOffered)) {
        slot->pairingOffered = true;
        deviceBoundHandler(slot);
    }
}

void ZigbeeCoordinator::refreshBoundDevices() {
    std::list<zb_device_params_t *> boundLights = zigbeeSwitch.getBoundDevices();
    for (zb_device_params_t *device : boundLights) {
        storeBoundDevice(device);
    }
}

void ZigbeeCoordinator::refreshRegisteredShorts() {
    if (registeredMap == nullptr) {
        return;
    }
    int slotIndex = registeredMap->nextUsedIndex(0);
    while (slotIndex >= 0) {
        DeviceTopicEntry *entry = registeredMap->slotAt(slotIndex);
        if (entry != nullptr && entry->used) {
            const uint16_t resolvedShort = esp_zb_address_short_by_ieee(entry->ieee);
            if (resolvedShort != 0 && resolvedShort != 0xFFFF) {
                rememberShortIeee(resolvedShort, entry->ieee, 0);
            }
        }
        slotIndex = registeredMap->nextUsedIndex(slotIndex + 1);
    }
}

void ZigbeeCoordinator::dispatch() {
    updatePairingLed();
    if (!started) {
        return;
    }
    if ((millis() - lastRefreshMs) < 10000) {
        return;
    }
    lastRefreshMs = millis();
    if (zigbeeSwitch.bound()) {
        refreshBoundDevices();
    }
    refreshRegisteredShorts();
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

void ZigbeeCoordinator::rememberShortIeee(uint16_t shortAddr, const uint8_t ieee[8], uint8_t endpoint) {
    if (ieee == nullptr || isZeroIeee(ieee) || shortAddr == 0 || shortAddr == 0xFFFF) {
        return;
    }
    BoundZigbeeDevice *slot = findByIeee(ieee);
    if (slot == nullptr) {
        slot = findByShortAddr(shortAddr);
    }
    if (slot == nullptr) {
        for (int i = 0; i < kMaxBoundDevices; i++) {
            if (!boundDevices[i].occupied) {
                slot = &boundDevices[i];
                memset(slot, 0, sizeof(BoundZigbeeDevice));
                break;
            }
        }
    }
    if (slot == nullptr) {
        return;
    }
    memcpy(slot->ieee, ieee, 8);
    slot->shortAddr = shortAddr;
    if (DeviceTopicMap::isUsableEndpoint(endpoint)) {
        slot->endpoint = endpoint;
    }
    slot->occupied = true;
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
        if (*shortAddr == 0 || *shortAddr == 0xFFFF) {
            const uint16_t resolvedShort = esp_zb_address_short_by_ieee(ieee);
            if (resolvedShort != 0 && resolvedShort != 0xFFFF) {
                *shortAddr = resolvedShort;
            }
        }
        if (!isZeroIeee(ieee)) {
            rememberShortIeee(*shortAddr, ieee, 0);
            return;
        }
    }
    *shortAddr = source.u.short_addr;
    BoundZigbeeDevice *known = findByShortAddr(*shortAddr);
    if (known != nullptr && !isZeroIeee(known->ieee)) {
        memcpy(ieee, known->ieee, 8);
        return;
    }
    if (*shortAddr != 0 && *shortAddr != 0xFFFF) {
        uint8_t resolvedIeee[8];
        memset(resolvedIeee, 0, sizeof(resolvedIeee));
        if (esp_zb_ieee_address_by_short(*shortAddr, resolvedIeee) == ESP_OK && !isZeroIeee(resolvedIeee)) {
            memcpy(ieee, resolvedIeee, 8);
            rememberShortIeee(*shortAddr, ieee, 0);
            return;
        }
        if (registeredMap != nullptr) {
            int slotIndex = registeredMap->nextUsedIndex(0);
            while (slotIndex >= 0) {
                DeviceTopicEntry *entry = registeredMap->slotAt(slotIndex);
                if (entry != nullptr && entry->used) {
                    const uint16_t mappedShort = esp_zb_address_short_by_ieee(entry->ieee);
                    if (mappedShort == *shortAddr) {
                        memcpy(ieee, entry->ieee, 8);
                        rememberShortIeee(*shortAddr, ieee, 0);
                        return;
                    }
                }
                slotIndex = registeredMap->nextUsedIndex(slotIndex + 1);
            }
        }
        for (int i = 0; i < kMaxBoundDevices; i++) {
            if (!boundDevices[i].occupied || isZeroIeee(boundDevices[i].ieee)) {
                continue;
            }
            if (boundDevices[i].shortAddr == 0 || boundDevices[i].shortAddr == 0xFFFF) {
                const uint16_t mappedShort = esp_zb_address_short_by_ieee(boundDevices[i].ieee);
                if (mappedShort == *shortAddr) {
                    memcpy(ieee, boundDevices[i].ieee, 8);
                    rememberShortIeee(*shortAddr, ieee, 0);
                    return;
                }
            }
        }
    }
}

bool ZigbeeCoordinator::migrateRegisteredIeee(const uint8_t previousIeee[8], const uint8_t nextIeee[8]) {
    if (registeredMap == nullptr || previousIeee == nullptr || nextIeee == nullptr) {
        return false;
    }
    if (isZeroIeee(previousIeee) || isZeroIeee(nextIeee) || memcmp(previousIeee, nextIeee, 8) == 0) {
        return false;
    }
    DeviceTopicEntry *previous = registeredMap->findByIeee(previousIeee);
    if (previous == nullptr || registeredMap->findByIeee(nextIeee) != nullptr) {
        return false;
    }
    if (registeredMap->upsert(
            nextIeee,
            previous->friendlyName,
            previous->stateTopic,
            previous->commandTopic,
            previous->availabilityTopic,
            previous->channelCount
        )
        == nullptr) {
        return false;
    }
    registeredMap->removeByIeee(previousIeee);
    LOGGER.info(
        "Moved registered device " + String(registeredName(nextIeee) != nullptr ? registeredName(nextIeee) : "")
        + " ieee=" + formatIeeeText(previousIeee) + " -> ieee=" + formatIeeeText(nextIeee)
    );
    if (registryChangedHandler != nullptr) {
        registryChangedHandler();
    }
    return true;
}

void ZigbeeCoordinator::offerPairingIfNeeded(const uint8_t ieee[8]) {
    BoundZigbeeDevice *slot = findByIeee(ieee);
    if (slot == nullptr || deviceBoundHandler == nullptr || slot->pairingOffered) {
        return;
    }
    const bool usableIdentity = slot->shortAddr != 0 && slot->shortAddr != 0xFFFF
        && DeviceTopicMap::isUsableEndpoint(slot->endpoint) && !isZeroIeee(slot->ieee);
    if (!usableIdentity) {
        return;
    }
    slot->pairingOffered = true;
    if (!isRegistered(slot->ieee)) {
        logDeviceEvent("join", slot->ieee, slot->shortAddr, slot->endpoint, registeredName(slot->ieee));
    }
    pulseInboundDevice(slot->ieee);
    deviceBoundHandler(slot);
}

void ZigbeeCoordinator::adoptReportIdentity(const uint8_t ieee[8], uint16_t shortAddr, uint8_t endpoint) {
    if (ieee == nullptr || isZeroIeee(ieee) || shortAddr == 0 || shortAddr == 0xFFFF) {
        return;
    }
    rememberShortIeee(shortAddr, ieee, endpoint);
    if (!isRegistered(ieee)) {
        DeviceTopicEntry *orphan = nullptr;
        int orphanCount = 0;
        if (registeredMap != nullptr) {
            int slotIndex = registeredMap->nextUsedIndex(0);
            while (slotIndex >= 0) {
                DeviceTopicEntry *entry = registeredMap->slotAt(slotIndex);
                if (entry != nullptr && entry->used) {
                    const uint16_t mappedShort = esp_zb_address_short_by_ieee(entry->ieee);
                    if (mappedShort == 0 || mappedShort == 0xFFFF) {
                        orphan = entry;
                        orphanCount++;
                    }
                }
                slotIndex = registeredMap->nextUsedIndex(slotIndex + 1);
            }
        }
        if (orphanCount == 1 && orphan != nullptr) {
            uint8_t previousIeee[8];
            memcpy(previousIeee, orphan->ieee, 8);
            migrateRegisteredIeee(previousIeee, ieee);
        }
    }
    offerPairingIfNeeded(ieee);
}

void ZigbeeCoordinator::setRegisteredMap(DeviceTopicMap *deviceMap) {
    registeredMap = deviceMap;
}

void ZigbeeCoordinator::clearRegisteredDevices() {
    if (registeredMap != nullptr) {
        registeredMap->clearAll();
    }
    registryReady = false;
}

void ZigbeeCoordinator::upsertRegisteredDevice(const DeviceTopicEntry *entry) {
    if (registeredMap == nullptr || entry == nullptr || !entry->used) {
        return;
    }
    if (registeredMap->upsert(
            entry->ieee,
            entry->friendlyName,
            entry->stateTopic,
            entry->commandTopic,
            entry->availabilityTopic,
            entry->channelCount
        )
        == nullptr) {
        LOGGER.warning("Registered device table full");
    }
}

void ZigbeeCoordinator::removeRegisteredDevice(const uint8_t ieee[8]) {
    if (registeredMap == nullptr || ieee == nullptr) {
        return;
    }
    registeredMap->removeByIeee(ieee);
}

void ZigbeeCoordinator::markRegistryReady() {
    registryReady = true;
    const int count = registeredMap != nullptr ? registeredMap->usedCount() : 0;
    LOGGER.info("Serving " + String(count) + " registered device(s)");
}

bool ZigbeeCoordinator::isRegistered(const uint8_t ieee[8]) const {
    return registeredName(ieee) != nullptr;
}

const char *ZigbeeCoordinator::registeredName(const uint8_t ieee[8]) const {
    if (ieee == nullptr || registeredMap == nullptr) {
        return nullptr;
    }
    const DeviceTopicEntry *entry = registeredMap->findByIeee(ieee);
    if (entry == nullptr) {
        return nullptr;
    }
    return entry->friendlyName;
}

void ZigbeeCoordinator::handleIasZoneStatus(
    const esp_zb_zcl_ias_zone_status_change_notification_message_t *message
) {
    if (message == nullptr) {
        return;
    }
    uint8_t ieee[8];
    uint16_t shortAddr = 0;
    resolveIeeeFromSource(message->info.src_address, ieee, &shortAddr);
    adoptReportIdentity(ieee, shortAddr, message->info.src_endpoint);
    const bool alarm = (message->zone_status & ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1) != 0;
    char eventName[48];
    snprintf(
        eventName,
        sizeof(eventName),
        "message %s ias=0x%04X",
        alarm ? "LEAK" : "DRY",
        (unsigned int)message->zone_status
    );
    logDeviceEvent(eventName, ieee, shortAddr, message->info.src_endpoint, registeredName(ieee));
    pulseInboundDevice(ieee);
    if (lightStateHandler != nullptr) {
        lightStateHandler(alarm ? "LEAK" : "DRY", ieee, message->info.src_endpoint, shortAddr);
    }
}

void ZigbeeCoordinator::handleIasZoneEnroll(
    ZigbeeEP *endpoint,
    const esp_zb_zcl_ias_zone_enroll_request_message_t *message
) {
    if (endpoint == nullptr || message == nullptr) {
        return;
    }
    uint8_t ieee[8];
    uint16_t shortAddr = 0;
    resolveIeeeFromSource(message->info.src_address, ieee, &shortAddr);
    if (shortAddr == 0 || shortAddr == 0xFFFF) {
        shortAddr = message->info.src_address.u.short_addr;
    }
    adoptReportIdentity(ieee, shortAddr, message->info.src_endpoint);
    const uint8_t zoneId = nextIasZoneId;
    if (nextIasZoneId < 254) {
        nextIasZoneId++;
    }
    endpoint->sendIASZoneEnrollResponse(shortAddr, message->info.src_endpoint, zoneId);
    char eventName[48];
    snprintf(
        eventName,
        sizeof(eventName),
        "enroll zoneType=0x%04X zone=%u",
        (unsigned int)message->zone_type,
        (unsigned int)zoneId
    );
    logDeviceEvent(eventName, ieee, shortAddr, message->info.src_endpoint, registeredName(ieee));
    pulseInboundDevice(ieee);
}

void ZigbeeCoordinator::handleAttributeReport(
    uint16_t clusterId,
    const esp_zb_zcl_attribute_t *attribute,
    uint8_t srcEndpoint,
    esp_zb_zcl_addr_t srcAddress
) {
    if (attribute == nullptr) {
        return;
    }
    uint8_t ieee[8];
    uint16_t shortAddr = 0;
    resolveIeeeFromSource(srcAddress, ieee, &shortAddr);
    adoptReportIdentity(ieee, shortAddr, srcEndpoint);
    uint32_t value = 0;
    if (attribute->data.value != nullptr && attribute->data.size > 0) {
        memcpy(&value, attribute->data.value, attribute->data.size > 4 ? 4 : attribute->data.size);
    }
    if (clusterId == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF && attribute->id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
        const bool on = value != 0;
        logDeviceEvent(
            on ? "message ON" : "message OFF",
            ieee,
            shortAddr,
            srcEndpoint,
            registeredName(ieee)
        );
        pulseInboundDevice(ieee);
        if (lightStateHandler != nullptr) {
            lightStateHandler(on ? "ON" : "OFF", ieee, srcEndpoint, shortAddr);
        }
        return;
    }
    char eventName[64];
    snprintf(
        eventName,
        sizeof(eventName),
        "cl=0x%04X,attr=0x%04X,val=0x%lX",
        (unsigned int)clusterId,
        (unsigned int)attribute->id,
        (unsigned long)value
    );
    logDeviceEvent(eventName, ieee, shortAddr, srcEndpoint, registeredName(ieee));
    pulseInboundDevice(ieee);
    if (lightStateHandler != nullptr) {
        lightStateHandler(eventName, ieee, srcEndpoint, shortAddr);
    }
}

void ZigbeeCoordinator::handleLightStateWithSource(bool on, uint8_t endpoint, esp_zb_zcl_addr_t source) {
    uint8_t ieee[8];
    uint16_t shortAddr = 0;
    resolveIeeeFromSource(source, ieee, &shortAddr);
    adoptReportIdentity(ieee, shortAddr, endpoint);
    logDeviceEvent(
        on ? "message ON" : "message OFF",
        ieee,
        shortAddr,
        endpoint,
        registeredName(ieee)
    );
    pulseInboundDevice(ieee);
    if (lightStateHandler != nullptr) {
        lightStateHandler(on ? "ON" : "OFF", ieee, endpoint, shortAddr);
    }
}

void ZigbeeCoordinator::pulseInboundDevice(const uint8_t ieee[8]) {
    if (ieee == nullptr) {
        STATUS_RGB.pulseBlue();
        return;
    }
    if (isRegistered(ieee)) {
        STATUS_RGB.pulseRed();
        return;
    }
    STATUS_RGB.pulseBlue();
}

bool ZigbeeCoordinator::controlOnOff(const uint8_t ieee[8], const char *command, uint8_t endpoint) {
    if (!started) {
        LOGGER.warning("Zigbee is not started");
        return false;
    }
    BoundZigbeeDevice *device = findByIeee(ieee);
    if (device == nullptr) {
        LOGGER.warning("No bound Zigbee device for command");
        return false;
    }
    if (registryReady && !isRegistered(ieee)) {
        LOGGER.warning("Command ignored; device is not registered");
        return false;
    }

    uint8_t targetEndpoint = endpoint;
    if (!DeviceTopicMap::isUsableEndpoint(targetEndpoint)) {
        targetEndpoint = device->endpoint;
    }
    if (!DeviceTopicMap::isUsableEndpoint(targetEndpoint)) {
        LOGGER.warning("No usable Zigbee endpoint for command");
        return false;
    }

    const char *body = command != nullptr ? command : "";
    String action = String(body);
    action.trim();
    String actionLower = action;
    actionLower.toLowerCase();

    esp_zb_ieee_addr_t ieeeAddr;
    memcpy(ieeeAddr, device->ieee, 8);

    char commandLabel[80];
    snprintf(commandLabel, sizeof(commandLabel), "command %s", action.c_str());
    logDeviceEvent(
        commandLabel,
        device->ieee,
        device->shortAddr,
        targetEndpoint,
        registeredName(device->ieee)
    );
    if (actionLower == "on" || actionLower == "1" || actionLower == "true") {
        zigbeeSwitch.lightOn(targetEndpoint, ieeeAddr);
        STATUS_RGB.pulseGreen();
        return true;
    }
    if (actionLower == "off" || actionLower == "0" || actionLower == "false") {
        zigbeeSwitch.lightOff(targetEndpoint, ieeeAddr);
        STATUS_RGB.pulseGreen();
        return true;
    }
    if (actionLower == "toggle") {
        zigbeeSwitch.lightToggle(targetEndpoint, ieeeAddr);
        STATUS_RGB.pulseGreen();
        return true;
    }
    return true;
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
