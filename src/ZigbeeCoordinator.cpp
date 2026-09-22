#include "ZigbeeCoordinator.h"
#include "Logger.h"
#include "Defines.h"
#include "StatusRgb.h"
#include "ZigbeeCluster.h"

#include <nwk/esp_zigbee_nwk.h>
#include <zdo/esp_zigbee_zdo_command.h>
#include <stdio.h>
#include <string.h>

extern "C" {
#include "zboss_api.h"
#include "zcl/zb_zcl_commands.h"
}
#include "zcl/esp_zigbee_zcl_ias_zone.h"
#include "zcl/esp_zigbee_zcl_power_config.h"

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

static void formatAttrEventName(
    char *buffer,
    size_t bufferSize,
    uint16_t clusterId,
    uint16_t attributeId,
    uint32_t value
) {
    const char *clusterName = zigbeeClusterName(clusterId);
    if (clusterName != nullptr && clusterName[0] != '\0') {
        snprintf(
            buffer,
            bufferSize,
            "%s cl=0x%04X,attr=0x%04X,val=0x%lX",
            clusterName,
            (unsigned int)clusterId,
            (unsigned int)attributeId,
            (unsigned long)value
        );
        return;
    }
    snprintf(
        buffer,
        bufferSize,
        "cl=0x%04X,attr=0x%04X,val=0x%lX",
        (unsigned int)clusterId,
        (unsigned int)attributeId,
        (unsigned long)value
    );
}

static bool isZeroIeee(const uint8_t ieee[8]) {
    static const uint8_t kZeroIeee[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    return ieee != nullptr && memcmp(ieee, kZeroIeee, 8) == 0;
}

static void onZclSendStatus(zb_uint8_t param) {
    if (param == 0) {
        return;
    }
    const zb_zcl_command_send_status_t *sendStatus = ZB_BUF_GET_PARAM(param, zb_zcl_command_send_status_t);
    if (sendStatus->status == RET_OK) {
        STATUS_RGB.pulseAckSent();
    }
    zb_buf_free(param);
}

static ZigbeeCoordinator *coordinatorForDefaultResponse = nullptr;

static void onCoordinatorDefaultResponse(
    zb_cmd_type_t respToCmd,
    esp_zb_zcl_status_t status,
    uint8_t endpoint,
    uint16_t cluster
) {
    (void)respToCmd;
    (void)status;
    if (coordinatorForDefaultResponse != nullptr) {
        coordinatorForDefaultResponse->noteDefaultResponse(endpoint, cluster);
    }
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

void ZigbeeCoordinator::CoordinatorSwitch::zbAttributeRead(
    uint16_t clusterId,
    const esp_zb_zcl_attribute_t *attribute,
    uint8_t srcEndpoint,
    esp_zb_zcl_addr_t srcAddress
) {
    if (owner != nullptr) {
        owner->handleAttributeReport(clusterId, attribute, srcEndpoint, srcAddress);
    }
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

void ZigbeeCoordinator::setJoinClosedHandler(JoinClosedFn handler) {
    joinClosedHandler = handler;
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
    Zigbee.onGlobalDefaultResponse(onCoordinatorDefaultResponse);

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
    coordinatorForDefaultResponse = this;
    lastRefreshMs = millis();
    maybeStartStatusRefresh();
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
    const bool wasOpen = pairingUntilMs != 0;
    pairingUntilMs = 0;
    pairingLedOn = false;
    STATUS_RGB.setPairingHeld(false);
    if (wasOpen && joinClosedHandler != nullptr) {
        joinClosedHandler();
    }
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
        slot->lastEmittedType = kZigbeeDeviceTypeNeverEmitted;
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
        slot->lastEmittedType = kZigbeeDeviceTypeNeverEmitted;
    }
    const bool wasIncomplete = !isNewDevice
        && (slot->shortAddr == 0 || slot->shortAddr == 0xFFFF
            || !DeviceTopicMap::isUsableEndpoint(slot->endpoint));
    slot->shortAddr = shortAddr;
    slot->endpoint = endpoint;
    slot->occupied = true;
    addKnownEndpoint(slot, endpoint);
    if (isNewDevice || wasIncomplete) {
        logDeviceEvent("join", slot->ieee, slot->shortAddr, slot->endpoint, registeredName(slot->ieee));
    }
    if (slot->lastEmittedType == kZigbeeDeviceTypeNeverEmitted
        || slot->zigbeeType == ZigbeeDeviceTypeUnknown) {
        startDescriptorProbe(slot);
    }
}

void ZigbeeCoordinator::emitDeviceJoin(BoundZigbeeDevice *slot) {
    if (slot == nullptr || deviceBoundHandler == nullptr) {
        return;
    }
    if (!zigbeeJoinShouldEmit(slot->lastEmittedType, slot->zigbeeType)) {
        return;
    }
    deviceBoundHandler(slot);
    slot->lastEmittedType = slot->zigbeeType;
    slot->pairingOffered = true;
    if (slot->zigbeeType != ZigbeeDeviceTypeUnknown) {
        slot->typeProbeDeadlineMs = 0;
    }
    char pairingEvent[40];
    snprintf(
        pairingEvent,
        sizeof(pairingEvent),
        "pairing type=%s",
        zigbeeDeviceTypeJsonId(slot->zigbeeType)
    );
    logDeviceEvent(pairingEvent, slot->ieee, slot->shortAddr, slot->endpoint, registeredName(slot->ieee));
}

void ZigbeeCoordinator::mergeBoundDeviceType(BoundZigbeeDevice *slot, uint8_t incomingType) {
    if (slot == nullptr) {
        return;
    }
    const uint8_t merged = mergeZigbeeDeviceType(slot->zigbeeType, incomingType);
    if (merged == slot->zigbeeType) {
        return;
    }
    slot->zigbeeType = merged;
    if (slot->zigbeeType != ZigbeeDeviceTypeUnknown) {
        emitDeviceJoin(slot);
    }
}

ZigbeeCoordinator::DescriptorProbe *ZigbeeCoordinator::allocDescriptorProbe(uint16_t shortAddr) {
    for (int i = 0; i < kMaxDescriptorProbes; i++) {
        if (!descriptorProbes[i].occupied) {
            descriptorProbes[i].occupied = true;
            descriptorProbes[i].owner = this;
            descriptorProbes[i].shortAddr = shortAddr;
            return &descriptorProbes[i];
        }
    }
    return nullptr;
}

void ZigbeeCoordinator::requestSimpleDescriptor(uint16_t shortAddr, uint8_t endpoint) {
    if (!DeviceTopicMap::isUsableEndpoint(endpoint)) {
        return;
    }
    DescriptorProbe *probe = allocDescriptorProbe(shortAddr);
    if (probe == nullptr) {
        return;
    }
    esp_zb_zdo_simple_desc_req_param_t request{};
    request.addr_of_interest = shortAddr;
    request.endpoint = endpoint;
    const bool tookLock = esp_zb_lock_acquire(pdMS_TO_TICKS(20));
    esp_zb_zdo_simple_desc_req(&request, onSimpleDescriptor, probe);
    if (tookLock) {
        esp_zb_lock_release();
    }
}

void ZigbeeCoordinator::requestActiveEndpoints(uint16_t shortAddr) {
    DescriptorProbe *probe = allocDescriptorProbe(shortAddr);
    if (probe == nullptr) {
        return;
    }
    esp_zb_zdo_active_ep_req_param_t request{};
    request.addr_of_interest = shortAddr;
    const bool tookLock = esp_zb_lock_acquire(pdMS_TO_TICKS(20));
    esp_zb_zdo_active_ep_req(&request, onActiveEndpoints, probe);
    if (tookLock) {
        esp_zb_lock_release();
    }
}

void ZigbeeCoordinator::startDescriptorProbe(BoundZigbeeDevice *slot) {
    if (slot == nullptr || slot->shortAddr == 0 || slot->shortAddr == 0xFFFF) {
        return;
    }
    if (slot->typeProbeDeadlineMs != 0) {
        return;
    }
    slot->typeProbeDeadlineMs = millis() + kTypeProbeTimeoutMs;
    requestActiveEndpoints(slot->shortAddr);
}

void ZigbeeCoordinator::serviceTypeProbes() {
    const unsigned long nowMs = millis();
    for (int i = 0; i < kMaxBoundDevices; i++) {
        BoundZigbeeDevice *slot = &boundDevices[i];
        if (!slot->occupied || slot->typeProbeDeadlineMs == 0) {
            continue;
        }
        if ((long)(nowMs - slot->typeProbeDeadlineMs) < 0) {
            continue;
        }
        slot->typeProbeDeadlineMs = 0;
        emitDeviceJoin(slot);
    }
}

void ZigbeeCoordinator::onActiveEndpoints(
    esp_zb_zdp_status_t zdoStatus,
    uint8_t epCount,
    uint8_t *epIdList,
    void *userCtx
) {
    DescriptorProbe *probe = static_cast<DescriptorProbe *>(userCtx);
    if (probe == nullptr || probe->owner == nullptr) {
        return;
    }
    ZigbeeCoordinator *coordinator = probe->owner;
    const uint16_t shortAddr = probe->shortAddr;
    probe->occupied = false;
    BoundZigbeeDevice *slot = coordinator->findByShortAddr(shortAddr);
    if (zdoStatus != ESP_ZB_ZDP_STATUS_SUCCESS || epIdList == nullptr || epCount == 0) {
        if (slot != nullptr) {
            coordinator->requestSimpleDescriptor(shortAddr, slot->endpoint);
        }
        return;
    }
    for (uint8_t i = 0; i < epCount; i++) {
        coordinator->addKnownEndpoint(slot, epIdList[i]);
        coordinator->requestSimpleDescriptor(shortAddr, epIdList[i]);
    }
}

void ZigbeeCoordinator::onSimpleDescriptor(
    esp_zb_zdp_status_t zdoStatus,
    esp_zb_af_simple_desc_1_1_t *simpleDesc,
    void *userCtx
) {
    DescriptorProbe *probe = static_cast<DescriptorProbe *>(userCtx);
    if (probe == nullptr || probe->owner == nullptr) {
        return;
    }
    ZigbeeCoordinator *coordinator = probe->owner;
    const uint16_t shortAddr = probe->shortAddr;
    probe->occupied = false;
    if (zdoStatus != ESP_ZB_ZDP_STATUS_SUCCESS || simpleDesc == nullptr) {
        return;
    }
    BoundZigbeeDevice *slot = coordinator->findByShortAddr(shortAddr);
    if (slot == nullptr) {
        return;
    }
    const uint8_t classified = classifyZigbeeDeviceTypeFromClusterList(
        simpleDesc->app_cluster_list,
        simpleDesc->app_input_cluster_count,
        simpleDesc->app_output_cluster_count
    );
    coordinator->mergeBoundDeviceType(slot, classified);
    coordinator->addKnownEndpoint(slot, simpleDesc->endpoint);
    const uint8_t clusterTotal =
        (uint8_t)(simpleDesc->app_input_cluster_count + simpleDesc->app_output_cluster_count);
    if (simpleDesc->app_cluster_list != nullptr) {
        for (uint8_t clusterIndex = 0; clusterIndex < clusterTotal; clusterIndex++) {
            if (simpleDesc->app_cluster_list[clusterIndex] == kZigbeeClusterPowerConfig) {
                slot->hasPowerConfig = true;
                break;
            }
        }
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
    esp_zb_nwk_info_iterator_t iterator = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t neighbor{};
    while (esp_zb_nwk_get_next_neighbor(&iterator, &neighbor) == ESP_OK) {
        if (isZeroIeee(neighbor.ieee_addr) || neighbor.short_addr == 0 || neighbor.short_addr == 0xFFFF) {
            continue;
        }
        if (registeredMap->findByIeee(neighbor.ieee_addr) != nullptr) {
            rememberShortIeee(neighbor.short_addr, neighbor.ieee_addr, 0);
        }
    }
}

void ZigbeeCoordinator::dispatch() {
    updatePairingLed();
    serviceCommandFlights();
    serviceTypeProbes();
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
                slot->lastEmittedType = kZigbeeDeviceTypeNeverEmitted;
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
        addKnownEndpoint(slot, endpoint);
    }
    slot->occupied = true;
}

bool ZigbeeCoordinator::fillIeeeFromNeighbor(uint16_t shortAddr, uint8_t ieee[8]) const {
    if (ieee == nullptr || shortAddr == 0 || shortAddr == 0xFFFF) {
        return false;
    }
    esp_zb_nwk_info_iterator_t iterator = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t neighbor{};
    while (esp_zb_nwk_get_next_neighbor(&iterator, &neighbor) == ESP_OK) {
        if (neighbor.short_addr != shortAddr || isZeroIeee(neighbor.ieee_addr)) {
            continue;
        }
        memcpy(ieee, neighbor.ieee_addr, 8);
        return true;
    }
    return false;
}

bool ZigbeeCoordinator::fillIeeeFromUniqueUnresolved(uint8_t ieee[8]) {
    if (ieee == nullptr || registeredMap == nullptr) {
        return false;
    }
    const DeviceTopicEntry *orphan = nullptr;
    int orphanCount = 0;
    int slotIndex = registeredMap->nextUsedIndex(0);
    while (slotIndex >= 0) {
        DeviceTopicEntry *entry = registeredMap->slotAt(slotIndex);
        if (entry != nullptr && entry->used && !isZeroIeee(entry->ieee)) {
            const uint16_t mappedShort = esp_zb_address_short_by_ieee(entry->ieee);
            BoundZigbeeDevice *known = findByIeee(entry->ieee);
            const bool hasBoundShort = known != nullptr && known->shortAddr != 0 && known->shortAddr != 0xFFFF;
            if ((mappedShort == 0 || mappedShort == 0xFFFF) && !hasBoundShort) {
                orphan = entry;
                orphanCount++;
            }
        }
        slotIndex = registeredMap->nextUsedIndex(slotIndex + 1);
    }
    if (orphanCount != 1 || orphan == nullptr) {
        return false;
    }
    memcpy(ieee, orphan->ieee, 8);
    return true;
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
        if (fillIeeeFromNeighbor(*shortAddr, ieee)) {
            rememberShortIeee(*shortAddr, ieee, 0);
            return;
        }
        if (fillIeeeFromUniqueUnresolved(ieee)) {
            rememberShortIeee(*shortAddr, ieee, 0);
        }
    }
}

void ZigbeeCoordinator::offerPairingIfNeeded(const uint8_t ieee[8]) {
    BoundZigbeeDevice *slot = findByIeee(ieee);
    if (slot == nullptr) {
        return;
    }
    const bool usableIdentity = slot->shortAddr != 0 && slot->shortAddr != 0xFFFF
        && DeviceTopicMap::isUsableEndpoint(slot->endpoint) && !isZeroIeee(slot->ieee);
    if (!usableIdentity) {
        return;
    }
    if (slot->lastEmittedType == kZigbeeDeviceTypeNeverEmitted
        || slot->zigbeeType == ZigbeeDeviceTypeUnknown) {
        startDescriptorProbe(slot);
    }
}

void ZigbeeCoordinator::adoptReportIdentity(const uint8_t ieee[8], uint16_t shortAddr, uint8_t endpoint) {
    if (ieee == nullptr || isZeroIeee(ieee) || shortAddr == 0 || shortAddr == 0xFFFF) {
        return;
    }
    rememberShortIeee(shortAddr, ieee, endpoint);
    BoundZigbeeDevice *reportSlot = findByIeee(ieee);
    addKnownEndpoint(reportSlot, endpoint);
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
    if (registeredMap->upsertFromEntry(entry, false) == nullptr) {
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
    maybeStartStatusRefresh();
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
    char eventName[96];
    formatAttrEventName(
        eventName,
        sizeof(eventName),
        ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE,
        ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONESTATUS_ID,
        (uint32_t)message->zone_status
    );
    logDeviceEvent(eventName, ieee, shortAddr, message->info.src_endpoint, registeredName(ieee));
    pulseInboundDevice(ieee);
    if (lightStateHandler != nullptr) {
        lightStateHandler(alarm ? "LEAK" : "DRY", ieee, message->info.src_endpoint, shortAddr, message->info.header.rssi);
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
    uint8_t enrollPayload[2];
    enrollPayload[0] = (uint8_t)ESP_ZB_ZCL_IAS_ZONE_ENROLL_RESPONSE_CODE_SUCCESS;
    enrollPayload[1] = zoneId;
    bool sentEnroll = false;
    if (!isZeroIeee(ieee)) {
        sentEnroll = sendZclWithoutApsAck(
            ZB_APS_ADDR_MODE_64_ENDP_PRESENT,
            ieee,
            0,
            message->info.src_endpoint,
            endpoint->getEndpoint(),
            ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE,
            true,
            ESP_ZB_ZCL_CMD_IAS_ZONE_ZONE_ENROLL_RESPONSE_ID,
            enrollPayload,
            2
        );
    } else {
        sentEnroll = sendZclWithoutApsAck(
            ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            nullptr,
            shortAddr,
            message->info.src_endpoint,
            endpoint->getEndpoint(),
            ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE,
            true,
            ESP_ZB_ZCL_CMD_IAS_ZONE_ZONE_ENROLL_RESPONSE_ID,
            enrollPayload,
            2
        );
    }
    if (sentEnroll) {
        STATUS_RGB.pulsePacketToDevice();
    }
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
    if (!isZeroIeee(ieee) && DeviceTopicMap::isUsableEndpoint(message->info.src_endpoint)) {
        zb_device_params_t enrollDevice{};
        memcpy(enrollDevice.ieee_addr, ieee, 8);
        enrollDevice.short_addr = shortAddr;
        enrollDevice.endpoint = message->info.src_endpoint;
        storeBoundDevice(&enrollDevice);
        BoundZigbeeDevice *slot = findByIeee(ieee);
        mergeBoundDeviceType(slot, ZigbeeDeviceTypeIasZone);
    }
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
    BoundZigbeeDevice *slot = findByIeee(ieee);
    mergeBoundDeviceType(slot, zigbeeDeviceTypeFromCluster(clusterId));
    uint32_t value = 0;
    if (attribute->data.value != nullptr && attribute->data.size > 0) {
        memcpy(&value, attribute->data.value, attribute->data.size > 4 ? 4 : attribute->data.size);
    }
    char eventName[96];
    formatAttrEventName(eventName, sizeof(eventName), clusterId, attribute->id, value);
    logDeviceEvent(eventName, ieee, shortAddr, srcEndpoint, registeredName(ieee));
    pulseInboundDevice(ieee);
    if (lightStateHandler == nullptr) {
        return;
    }
    if (clusterId == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF && attribute->id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
        lightStateHandler(value != 0 ? "ON" : "OFF", ieee, srcEndpoint, shortAddr, rssiForShortAddr(shortAddr));
        return;
    }
    if (clusterId == ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG
        && attribute->id == ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID
        && zigbeeBatteryPercentageRemainingValid(value)) {
        char batteryMessage[16];
        snprintf(
            batteryMessage,
            sizeof(batteryMessage),
            "BATTERY %u",
            zigbeeBatteryPercentageFromRemaining(value)
        );
        lightStateHandler(batteryMessage, ieee, srcEndpoint, shortAddr, rssiForShortAddr(shortAddr));
        return;
    }
    if (clusterId == kZigbeeClusterIasZone && attribute->id == kZigbeeAttrIasZoneStatus) {
        const bool alarm = (value & ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1) != 0;
        lightStateHandler(alarm ? "LEAK" : "DRY", ieee, srcEndpoint, shortAddr, rssiForShortAddr(shortAddr));
        return;
    }
    if (clusterId == kZigbeeClusterWindowCovering
        && (attribute->id == kZigbeeAttrCurrentPositionLiftPercentage
            || attribute->id == kZigbeeAttrWindowCoveringMoveStatus)) {
        BoundZigbeeDevice *coveringSlot = findByIeee(ieee);
        BoundZigbeeDevice::CoveringStatus *coveringStatus = coveringStatusFor(coveringSlot, srcEndpoint);
        if (coveringStatus != nullptr) {
            if (attribute->id == kZigbeeAttrCurrentPositionLiftPercentage) {
                coveringStatus->hasPosition = true;
                coveringStatus->positionPercent = (uint8_t)zigbeeWindowCoveringPositionPercent(value);
            } else {
                coveringStatus->hasMove = true;
                coveringStatus->moveStatus = (uint8_t)value;
            }
            emitWindowCoveringStatus(coveringSlot, srcEndpoint, ieee, shortAddr);
            return;
        }
        char coveringMessage[SPI_DEVICE_MESSAGE_MAX];
        const bool isPosition = attribute->id == kZigbeeAttrCurrentPositionLiftPercentage;
        zigbeeWindowCoveringFormatStatus(
            coveringMessage,
            sizeof(coveringMessage),
            !isPosition,
            value,
            isPosition,
            zigbeeWindowCoveringPositionPercent(value)
        );
        if (coveringMessage[0] != '\0') {
            lightStateHandler(coveringMessage, ieee, srcEndpoint, shortAddr, rssiForShortAddr(shortAddr));
        }
        return;
    }
    lightStateHandler(eventName, ieee, srcEndpoint, shortAddr, rssiForShortAddr(shortAddr));
}

void ZigbeeCoordinator::handleLightStateWithSource(bool on, uint8_t endpoint, esp_zb_zcl_addr_t source) {
    uint8_t ieee[8];
    uint16_t shortAddr = 0;
    resolveIeeeFromSource(source, ieee, &shortAddr);
    adoptReportIdentity(ieee, shortAddr, endpoint);
    BoundZigbeeDevice *onOffSlot = findByIeee(ieee);
    mergeBoundDeviceType(onOffSlot, ZigbeeDeviceTypeOnOff);
    char eventName[96];
    formatAttrEventName(
        eventName,
        sizeof(eventName),
        ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
        on ? 1u : 0u
    );
    logDeviceEvent(eventName, ieee, shortAddr, endpoint, registeredName(ieee));
    pulseInboundDevice(ieee);
    if (lightStateHandler != nullptr) {
        lightStateHandler(on ? "ON" : "OFF", ieee, endpoint, shortAddr, rssiForShortAddr(shortAddr));
    }
}

void ZigbeeCoordinator::pulseInboundDevice(const uint8_t ieee[8]) {
    if (ieee != nullptr && isRegistered(ieee)) {
        STATUS_RGB.pulseKnownDevicePacket();
        return;
    }
    STATUS_RGB.pulsePacketReceived();
}

int8_t ZigbeeCoordinator::rssiForShortAddr(uint16_t shortAddr) const {
    if (shortAddr == 0 || shortAddr == 0xFFFF) {
        return 0;
    }
    esp_zb_nwk_info_iterator_t iterator = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t neighbor{};
    while (esp_zb_nwk_get_next_neighbor(&iterator, &neighbor) == ESP_OK) {
        if (neighbor.short_addr == shortAddr) {
            return neighbor.rssi;
        }
    }
    return 0;
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
    if (actionLower != "on" && actionLower != "1" && actionLower != "true" && actionLower != "off"
        && actionLower != "0" && actionLower != "false" && actionLower != "toggle") {
        return true;
    }

    DestCommandSlot *slot = destSlotFor(device->ieee, targetEndpoint, true);
    if (slot == nullptr) {
        LOGGER.warning("No dest slot for command");
        return false;
    }
    if (slot->inFlight) {
        stashNextOnOff(slot, action.c_str());
        return true;
    }
    return transmitOnOff(slot, action.c_str());
}

bool ZigbeeCoordinator::writeAttribute(
    const uint8_t ieee[8],
    uint8_t endpoint,
    uint16_t clusterId,
    uint16_t attributeId,
    uint8_t dataType,
    uint32_t attributeValue
) {
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

    DestCommandSlot *slot = destSlotFor(device->ieee, targetEndpoint, true);
    if (slot == nullptr) {
        LOGGER.warning("No dest slot for command");
        return false;
    }
    if (slot->inFlight) {
        stashNextWriteAttr(slot, clusterId, attributeId, dataType, attributeValue);
        return true;
    }
    return transmitWriteAttr(slot, clusterId, attributeId, dataType, attributeValue);
}

bool ZigbeeCoordinator::readAttribute(
    const uint8_t ieee[8],
    uint8_t endpoint,
    uint16_t clusterId,
    uint16_t attributeId
) {
    if (!started) {
        LOGGER.warning("Zigbee is not started");
        return false;
    }
    BoundZigbeeDevice *device = findByIeee(ieee);
    if (device == nullptr && ieee != nullptr) {
        uint8_t lookupIeee[8];
        memcpy(lookupIeee, ieee, 8);
        const uint16_t resolvedShort = esp_zb_address_short_by_ieee(lookupIeee);
        if (resolvedShort != 0 && resolvedShort != 0xFFFF) {
            rememberShortIeee(resolvedShort, ieee, endpoint);
            device = findByIeee(ieee);
        }
    }
    if (device == nullptr) {
        return true;
    }
    if (device->shortAddr == 0 || device->shortAddr == 0xFFFF) {
        return true;
    }

    uint8_t targetEndpoint = endpoint;
    if (!DeviceTopicMap::isUsableEndpoint(targetEndpoint)) {
        targetEndpoint = device->endpoint;
    }
    if (!DeviceTopicMap::isUsableEndpoint(targetEndpoint)) {
        return true;
    }

    DestCommandSlot *slot = destSlotFor(device->ieee, targetEndpoint, true);
    if (slot == nullptr) {
        LOGGER.warning("No dest slot for command");
        return false;
    }
    if (slot->inFlight) {
        stashNextReadAttr(slot, clusterId, attributeId);
        return true;
    }
    return transmitReadAttr(slot, clusterId, attributeId);
}

void ZigbeeCoordinator::addKnownEndpoint(BoundZigbeeDevice *slot, uint8_t endpoint) {
    if (slot == nullptr || !DeviceTopicMap::isUsableEndpoint(endpoint)) {
        return;
    }
    for (uint8_t i = 0; i < slot->knownEndpointCount; i++) {
        if (slot->knownEndpoints[i] == endpoint) {
            return;
        }
    }
    if (slot->knownEndpointCount >= DEVICE_CHANNEL_COUNT_MAX) {
        return;
    }
    slot->knownEndpoints[slot->knownEndpointCount] = endpoint;
    slot->knownEndpointCount++;
}

void ZigbeeCoordinator::maybeStartStatusRefresh() {
    if (!started || !registryReady || statusRefreshStarted) {
        return;
    }
    statusRefreshStarted = true;
    enqueueRegisteredStatusReads();
}

void ZigbeeCoordinator::enqueueTypeStatusRead(const DeviceTopicEntry *entry, uint8_t endpoint) {
    if (entry == nullptr) {
        return;
    }
    if (entry->zigbeeType == ZigbeeDeviceTypeOnOff) {
        readAttribute(entry->ieee, endpoint, kZigbeeClusterOnOff, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID);
        return;
    }
    if (entry->zigbeeType == ZigbeeDeviceTypeIasZone) {
        readAttribute(entry->ieee, endpoint, kZigbeeClusterIasZone, kZigbeeAttrIasZoneStatus);
        return;
    }
    if (entry->zigbeeType == ZigbeeDeviceTypeWindowCovering) {
        readAttribute(
            entry->ieee,
            endpoint,
            kZigbeeClusterWindowCovering,
            kZigbeeAttrCurrentPositionLiftPercentage
        );
        readAttribute(
            entry->ieee,
            endpoint,
            kZigbeeClusterWindowCovering,
            kZigbeeAttrWindowCoveringMoveStatus
        );
    }
}

BoundZigbeeDevice::CoveringStatus *ZigbeeCoordinator::coveringStatusFor(
    BoundZigbeeDevice *slot,
    uint8_t endpoint
) {
    if (slot == nullptr || !DeviceTopicMap::isUsableEndpoint(endpoint)) {
        return nullptr;
    }
    for (int i = 0; i < DEVICE_CHANNEL_COUNT_MAX; i++) {
        if (slot->coveringStatus[i].used && slot->coveringStatus[i].endpoint == endpoint) {
            return &slot->coveringStatus[i];
        }
    }
    for (int i = 0; i < DEVICE_CHANNEL_COUNT_MAX; i++) {
        if (!slot->coveringStatus[i].used) {
            slot->coveringStatus[i].used = true;
            slot->coveringStatus[i].endpoint = endpoint;
            return &slot->coveringStatus[i];
        }
    }
    return nullptr;
}

void ZigbeeCoordinator::emitWindowCoveringStatus(
    BoundZigbeeDevice *slot,
    uint8_t endpoint,
    const uint8_t ieee[8],
    uint16_t shortAddr
) {
    if (lightStateHandler == nullptr) {
        return;
    }
    bool hasMove = false;
    bool hasPosition = false;
    uint32_t moveRaw = 0;
    unsigned positionPercent = 0;
    BoundZigbeeDevice::CoveringStatus *coveringStatus = coveringStatusFor(slot, endpoint);
    if (coveringStatus != nullptr) {
        hasMove = coveringStatus->hasMove;
        hasPosition = coveringStatus->hasPosition;
        moveRaw = coveringStatus->moveStatus;
        positionPercent = coveringStatus->positionPercent;
    }
    char statusMessage[SPI_DEVICE_MESSAGE_MAX];
    zigbeeWindowCoveringFormatStatus(
        statusMessage,
        sizeof(statusMessage),
        hasMove,
        moveRaw,
        hasPosition,
        positionPercent
    );
    if (statusMessage[0] == '\0') {
        return;
    }
    lightStateHandler(statusMessage, ieee, endpoint, shortAddr, rssiForShortAddr(shortAddr));
}

void ZigbeeCoordinator::enqueueRegisteredStatusReads() {
    if (registeredMap == nullptr) {
        return;
    }
    int slotIndex = registeredMap->nextUsedIndex(0);
    while (slotIndex >= 0) {
        DeviceTopicEntry *entry = registeredMap->slotAt(slotIndex);
        slotIndex = registeredMap->nextUsedIndex(slotIndex + 1);
        if (entry == nullptr || !entry->used) {
            continue;
        }
        BoundZigbeeDevice *bound = findByIeee(entry->ieee);
        if (bound == nullptr) {
            const uint16_t resolvedShort = esp_zb_address_short_by_ieee(entry->ieee);
            if (resolvedShort != 0 && resolvedShort != 0xFFFF) {
                rememberShortIeee(resolvedShort, entry->ieee, 1);
                bound = findByIeee(entry->ieee);
            }
        }
        if (bound == nullptr || bound->shortAddr == 0 || bound->shortAddr == 0xFFFF) {
            continue;
        }
        uint8_t endpoints[DEVICE_CHANNEL_COUNT_MAX];
        uint8_t endpointCount = 0;
        if (entry->channelCount == DEVICE_CHANNEL_PARSE) {
            if (bound->knownEndpointCount > 0) {
                endpointCount = bound->knownEndpointCount;
                memcpy(endpoints, bound->knownEndpoints, endpointCount);
            } else if (DeviceTopicMap::isUsableEndpoint(bound->endpoint)) {
                endpoints[0] = bound->endpoint;
                endpointCount = 1;
            } else {
                endpoints[0] = 1;
                endpointCount = 1;
            }
        } else if (entry->channelCount >= 2) {
            endpointCount = entry->channelCount;
            for (uint8_t channel = 1; channel <= entry->channelCount; channel++) {
                endpoints[channel - 1] = channel;
            }
        } else {
            endpoints[0] = 1;
            endpointCount = 1;
        }
        for (uint8_t i = 0; i < endpointCount; i++) {
            enqueueTypeStatusRead(entry, endpoints[i]);
        }
        const uint8_t batteryEndpoint = endpointCount > 0 ? endpoints[0] : 1;
        readAttribute(
            entry->ieee,
            batteryEndpoint,
            kZigbeeClusterPowerConfig,
            kZigbeeAttrBatteryPercentageRemaining
        );
    }
}

ZigbeeCoordinator::DestCommandSlot *ZigbeeCoordinator::destSlotFor(
    const uint8_t ieee[8],
    uint8_t endpoint,
    bool allocate
) {
    for (int i = 0; i < kMaxDestFlights; i++) {
        DestCommandSlot *slot = &destFlights[i];
        if (slot->occupied && memcmp(slot->ieee, ieee, 8) == 0 && slot->endpoint == endpoint) {
            return slot;
        }
    }
    if (!allocate) {
        return nullptr;
    }
    for (int i = 0; i < kMaxDestFlights; i++) {
        DestCommandSlot *slot = &destFlights[i];
        if (slot->occupied) {
            continue;
        }
        *slot = DestCommandSlot{};
        slot->occupied = true;
        memcpy(slot->ieee, ieee, 8);
        slot->endpoint = endpoint;
        return slot;
    }
    LOGGER.warning("Dest command table full");
    return nullptr;
}

void ZigbeeCoordinator::markInFlight(DestCommandSlot *slot, uint16_t clusterId) {
    slot->inFlight = true;
    slot->inFlightCluster = clusterId;
    slot->inFlightDeadlineMs = millis() + kCommandInFlightTimeoutMs;
}

void ZigbeeCoordinator::stashNextOnOff(DestCommandSlot *slot, const char *command) {
    slot->hasNext = true;
    slot->nextKind = RadioCommandKind::OnOff;
    memset(slot->nextOnOff, 0, sizeof(slot->nextOnOff));
    if (command != nullptr) {
        strncpy(slot->nextOnOff, command, sizeof(slot->nextOnOff) - 1);
    }
}

void ZigbeeCoordinator::stashNextWriteAttr(
    DestCommandSlot *slot,
    uint16_t clusterId,
    uint16_t attributeId,
    uint8_t dataType,
    uint32_t attributeValue
) {
    slot->hasNext = true;
    slot->nextKind = RadioCommandKind::WriteAttr;
    slot->nextCluster = clusterId;
    slot->nextAttribute = attributeId;
    slot->nextType = dataType;
    slot->nextValue = attributeValue;
}

void ZigbeeCoordinator::stashNextReadAttr(
    DestCommandSlot *slot,
    uint16_t clusterId,
    uint16_t attributeId
) {
    slot->hasNext = true;
    slot->nextKind = RadioCommandKind::ReadAttr;
    slot->nextCluster = clusterId;
    slot->nextAttribute = attributeId;
    slot->nextType = 0;
    slot->nextValue = 0;
}

bool ZigbeeCoordinator::transmitReadAttr(
    DestCommandSlot *slot,
    uint16_t clusterId,
    uint16_t attributeId
) {
    BoundZigbeeDevice *device = findByIeee(slot->ieee);
    if (device == nullptr) {
        slot->occupied = false;
        slot->inFlight = false;
        slot->hasNext = false;
        return false;
    }
    uint8_t readPayload[2];
    readPayload[0] = (uint8_t)(attributeId & 0xFF);
    readPayload[1] = (uint8_t)((attributeId >> 8) & 0xFF);
    if (!sendZclToDevice(
            device,
            slot->endpoint,
            clusterId,
            false,
            ZB_ZCL_CMD_READ_ATTRIB,
            readPayload,
            2
        )) {
        return false;
    }
    STATUS_RGB.pulsePacketToDevice();
    markInFlight(slot, clusterId);
    return true;
}

bool ZigbeeCoordinator::transmitOnOff(DestCommandSlot *slot, const char *command) {
    BoundZigbeeDevice *device = findByIeee(slot->ieee);
    if (device == nullptr) {
        slot->occupied = false;
        slot->inFlight = false;
        slot->hasNext = false;
        return false;
    }

    const char *body = command != nullptr ? command : "";
    String action = String(body);
    action.trim();
    String actionLower = action;
    actionLower.toLowerCase();

    char commandLabel[80];
    snprintf(commandLabel, sizeof(commandLabel), "command %s", action.c_str());
    logDeviceEvent(
        commandLabel,
        device->ieee,
        device->shortAddr,
        slot->endpoint,
        registeredName(device->ieee)
    );
    if (actionLower == "on" || actionLower == "1" || actionLower == "true") {
        if (!sendZclToDevice(
                device,
                slot->endpoint,
                ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
                true,
                ESP_ZB_ZCL_CMD_ON_OFF_ON_ID,
                nullptr,
                0
            )) {
            return false;
        }
        STATUS_RGB.pulsePacketToDevice();
        markInFlight(slot, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF);
        return true;
    }
    if (actionLower == "off" || actionLower == "0" || actionLower == "false") {
        if (!sendZclToDevice(
                device,
                slot->endpoint,
                ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
                true,
                ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID,
                nullptr,
                0
            )) {
            return false;
        }
        STATUS_RGB.pulsePacketToDevice();
        markInFlight(slot, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF);
        return true;
    }
    if (actionLower == "toggle") {
        if (!sendZclToDevice(
                device,
                slot->endpoint,
                ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
                true,
                ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID,
                nullptr,
                0
            )) {
            return false;
        }
        STATUS_RGB.pulsePacketToDevice();
        markInFlight(slot, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF);
        return true;
    }
    return true;
}

bool ZigbeeCoordinator::sendZclToDevice(
    BoundZigbeeDevice *device,
    uint8_t dstEndpoint,
    uint16_t clusterId,
    bool clusterSpecific,
    uint8_t commandId,
    const uint8_t *payload,
    uint16_t payloadLength
) {
    if (device == nullptr) {
        return false;
    }
    const bool haveShort = device->shortAddr != 0 && device->shortAddr != 0xFFFF;
    return sendZclWithoutApsAck(
        haveShort ? ZB_APS_ADDR_MODE_16_ENDP_PRESENT : ZB_APS_ADDR_MODE_64_ENDP_PRESENT,
        device->ieee,
        haveShort ? device->shortAddr : 0,
        dstEndpoint,
        kSwitchEndpoint,
        clusterId,
        clusterSpecific,
        commandId,
        payload,
        payloadLength
    );
}

bool ZigbeeCoordinator::sendZclWithoutApsAck(
    uint8_t addressMode,
    const uint8_t ieee[8],
    uint16_t shortAddr,
    uint8_t dstEndpoint,
    uint8_t srcEndpoint,
    uint16_t clusterId,
    bool clusterSpecific,
    uint8_t commandId,
    const uint8_t *payload,
    uint16_t payloadLength
) {
    if (!esp_zb_lock_acquire(portMAX_DELAY)) {
        LOGGER.warning("Zigbee lock failed for ZCL send");
        return false;
    }
    const zb_bufid_t buffer = zb_buf_get_out();
    if (buffer == 0) {
        esp_zb_lock_release();
        LOGGER.warning("No Zigbee buffer for ZCL send");
        return false;
    }
    const zb_uint8_t frameType =
        clusterSpecific ? ZB_ZCL_FRAME_TYPE_CLUSTER_SPECIFIC : ZB_ZCL_FRAME_TYPE_COMMON;
    const zb_uint8_t frameControl = ZB_ZCL_CONSTRUCT_FRAME_CONTROL(
        frameType,
        ZB_ZCL_NOT_MANUFACTURER_SPECIFIC,
        ZB_ZCL_FRAME_DIRECTION_TO_SRV,
        ZB_ZCL_DISABLE_DEFAULT_RESPONSE
    );
    zb_uint8_t *payloadPtr = (zb_uint8_t *)zb_zcl_start_command_header(
        buffer,
        frameControl,
        0,
        commandId,
        nullptr
    );
    if (payload != nullptr && payloadLength > 0) {
        memcpy(payloadPtr, payload, payloadLength);
        payloadPtr += payloadLength;
    }
    zb_addr_u destAddr{};
    if (addressMode == ZB_APS_ADDR_MODE_16_ENDP_PRESENT) {
        destAddr.addr_short = shortAddr;
    } else if (ieee != nullptr) {
        memcpy(destAddr.addr_long, ieee, 8);
    }
    const zb_ret_t sendResult = zb_zcl_finish_and_send_packet_new(
        buffer,
        payloadPtr,
        &destAddr,
        addressMode,
        dstEndpoint,
        srcEndpoint,
        ZB_AF_HA_PROFILE_ID,
        clusterId,
        onZclSendStatus,
        ZB_FALSE,
        ZB_TRUE,
        0
    );
    esp_zb_lock_release();
    if (sendResult != RET_OK) {
        LOGGER.warning("ZCL send failed status=" + String((int)sendResult));
        return false;
    }
    return true;
}

bool ZigbeeCoordinator::transmitWriteAttr(
    DestCommandSlot *slot,
    uint16_t clusterId,
    uint16_t attributeId,
    uint8_t dataType,
    uint32_t attributeValue
) {
    BoundZigbeeDevice *device = findByIeee(slot->ieee);
    if (device == nullptr) {
        slot->occupied = false;
        slot->inFlight = false;
        slot->hasNext = false;
        return false;
    }

    uint16_t valueSize = 1;
    if (dataType == ESP_ZB_ZCL_ATTR_TYPE_U16) {
        valueSize = 2;
    } else if (dataType == ESP_ZB_ZCL_ATTR_TYPE_U32) {
        valueSize = 4;
    } else if (dataType != ESP_ZB_ZCL_ATTR_TYPE_U8) {
        if (attributeValue > 0xFFFFu) {
            valueSize = 4;
        } else if (attributeValue > 0xFFu) {
            valueSize = 2;
        }
    }

    uint8_t writePayload[2 + 1 + 4];
    writePayload[0] = (uint8_t)(attributeId & 0xFF);
    writePayload[1] = (uint8_t)((attributeId >> 8) & 0xFF);
    writePayload[2] = dataType;
    writePayload[3] = (uint8_t)(attributeValue & 0xFF);
    writePayload[4] = (uint8_t)((attributeValue >> 8) & 0xFF);
    writePayload[5] = (uint8_t)((attributeValue >> 16) & 0xFF);
    writePayload[6] = (uint8_t)((attributeValue >> 24) & 0xFF);
    const uint16_t writeLength = (uint16_t)(3 + valueSize);

    char eventName[96];
    formatAttrEventName(eventName, sizeof(eventName), clusterId, attributeId, attributeValue);
    logDeviceEvent(eventName, device->ieee, device->shortAddr, slot->endpoint, registeredName(device->ieee));
    if (!sendZclToDevice(
            device,
            slot->endpoint,
            clusterId,
            false,
            ZB_ZCL_CMD_WRITE_ATTRIB,
            writePayload,
            writeLength
        )) {
        return false;
    }
    STATUS_RGB.pulsePacketToDevice();
    markInFlight(slot, clusterId);
    return true;
}

void ZigbeeCoordinator::sendNextIfReady(DestCommandSlot *slot) {
    if (slot == nullptr || slot->inFlight || !slot->hasNext) {
        return;
    }
    const RadioCommandKind kind = slot->nextKind;
    char onOffCopy[SPI_DEVICE_MESSAGE_MAX];
    memcpy(onOffCopy, slot->nextOnOff, sizeof(onOffCopy));
    const uint16_t clusterId = slot->nextCluster;
    const uint16_t attributeId = slot->nextAttribute;
    const uint8_t dataType = slot->nextType;
    const uint32_t attributeValue = slot->nextValue;
    slot->hasNext = false;
    slot->nextKind = RadioCommandKind::None;
    if (kind == RadioCommandKind::OnOff) {
        transmitOnOff(slot, onOffCopy);
        return;
    }
    if (kind == RadioCommandKind::WriteAttr) {
        transmitWriteAttr(slot, clusterId, attributeId, dataType, attributeValue);
        return;
    }
    if (kind == RadioCommandKind::ReadAttr) {
        transmitReadAttr(slot, clusterId, attributeId);
    }
}

void ZigbeeCoordinator::serviceCommandFlights() {
    const unsigned long nowMs = millis();
    for (int i = 0; i < kMaxDestFlights; i++) {
        DestCommandSlot *slot = &destFlights[i];
        if (!slot->occupied) {
            continue;
        }
        if (slot->inFlight && (long)(nowMs - slot->inFlightDeadlineMs) >= 0) {
            slot->inFlight = false;
        }
        sendNextIfReady(slot);
        if (!slot->inFlight && !slot->hasNext) {
            slot->occupied = false;
        }
    }
}

void ZigbeeCoordinator::noteDefaultResponse(uint8_t endpoint, uint16_t cluster) {
    DestCommandSlot *bestSlot = nullptr;
    for (int i = 0; i < kMaxDestFlights; i++) {
        DestCommandSlot *slot = &destFlights[i];
        if (!slot->occupied || !slot->inFlight) {
            continue;
        }
        if (slot->endpoint != endpoint || slot->inFlightCluster != cluster) {
            continue;
        }
        if (bestSlot == nullptr
            || (long)(slot->inFlightDeadlineMs - bestSlot->inFlightDeadlineMs) < 0) {
            bestSlot = slot;
        }
    }
    if (bestSlot != nullptr) {
        bestSlot->inFlight = false;
    }
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
