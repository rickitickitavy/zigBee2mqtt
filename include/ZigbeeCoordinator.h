#pragma once

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator mode is not selected (ZIGBEE_MODE_ZCZR)"
#endif

#include <Arduino.h>
#include <Zigbee.h>
#include "DeviceTopicMap.h"
#include "SpiProtocol.h"

struct BoundZigbeeDevice {
    uint8_t ieee[8];
    uint16_t shortAddr;
    uint8_t endpoint;
    char manufacturer[32];
    char model[32];
    bool occupied;
    bool pairingOffered;
};

class ZigbeeCoordinator {
public:
    using LightStateFn = void (*)(
        const char *message,
        const uint8_t ieee[8],
        uint8_t endpoint,
        uint16_t shortAddr,
        int8_t rssiDbm
    );
    using DeviceBoundFn = void (*)(const BoundZigbeeDevice *device);
    using RegistryChangedFn = void (*)();
    using JoinClosedFn = void (*)();

    ZigbeeCoordinator();

    void attachLibraryCallbacks(void (*withSource)(bool, uint8_t, esp_zb_zcl_addr_t));
    bool begin(uint8_t channel, uint8_t permitJoinSec);
    bool isStarted() const;
    void permitJoin(uint8_t seconds);
    void closeJoin();
    void refreshBoundDevices();
    void dispatch();
    String devicesJson(DeviceTopicMap *topicMap);
    bool controlOnOff(const uint8_t ieee[8], const char *command, uint8_t endpoint);
    bool writeAttribute(
        const uint8_t ieee[8],
        uint8_t endpoint,
        uint16_t clusterId,
        uint16_t attributeId,
        uint8_t dataType,
        uint32_t attributeValue
    );
    BoundZigbeeDevice *findByIeee(const uint8_t ieee[8]);
    BoundZigbeeDevice *findByShortAddr(uint16_t shortAddr);
    void setLightStateHandler(LightStateFn handler);
    void setDeviceBoundHandler(DeviceBoundFn handler);
    void setRegistryChangedHandler(RegistryChangedFn handler);
    void setJoinClosedHandler(JoinClosedFn handler);
    void handleLightStateWithSource(bool on, uint8_t endpoint, esp_zb_zcl_addr_t source);
    void handleIasZoneStatus(const esp_zb_zcl_ias_zone_status_change_notification_message_t *message);
    void handleIasZoneEnroll(ZigbeeEP *endpoint, const esp_zb_zcl_ias_zone_enroll_request_message_t *message);
    void handleAttributeReport(
        uint16_t clusterId,
        const esp_zb_zcl_attribute_t *attribute,
        uint8_t srcEndpoint,
        esp_zb_zcl_addr_t srcAddress
    );
    void noteDefaultResponse(uint8_t endpoint, uint16_t cluster);
    void setRegisteredMap(DeviceTopicMap *deviceMap);
    void clearRegisteredDevices();
    void upsertRegisteredDevice(const DeviceTopicEntry *entry);
    void removeRegisteredDevice(const uint8_t ieee[8]);
    void markRegistryReady();
    bool isRegistered(const uint8_t ieee[8]) const;
    const char *registeredName(const uint8_t ieee[8]) const;

    static constexpr int kMaxBoundDevices = 16;
    static constexpr int kMaxDestFlights = 16;
    static constexpr unsigned long kCommandInFlightTimeoutMs = 1500UL;

private:
    class CoordinatorSwitch : public ZigbeeSwitch {
    public:
        CoordinatorSwitch(uint8_t endpoint, ZigbeeCoordinator *owner);

    private:
        ZigbeeCoordinator *owner;
        void zbIASZoneStatusChangeNotification(
            const esp_zb_zcl_ias_zone_status_change_notification_message_t *message
        ) override;
        void zbIASZoneEnrollRequest(const esp_zb_zcl_ias_zone_enroll_request_message_t *message) override;
    };

    class IasCieEndpoint : public ZigbeeEP {
    public:
        IasCieEndpoint(uint8_t endpoint, ZigbeeCoordinator *owner);

    private:
        ZigbeeCoordinator *owner;
        void zbAttributeRead(
            uint16_t clusterId,
            const esp_zb_zcl_attribute_t *attribute,
            uint8_t srcEndpoint,
            esp_zb_zcl_addr_t srcAddress
        ) override;
        void zbIASZoneStatusChangeNotification(
            const esp_zb_zcl_ias_zone_status_change_notification_message_t *message
        ) override;
        void zbIASZoneEnrollRequest(const esp_zb_zcl_ias_zone_enroll_request_message_t *message) override;
    };

    enum class RadioCommandKind : uint8_t {
        None = 0,
        OnOff = 1,
        WriteAttr = 2
    };

    struct DestCommandSlot {
        bool occupied = false;
        bool inFlight = false;
        bool hasNext = false;
        uint8_t ieee[8]{};
        uint8_t endpoint = 255;
        uint16_t inFlightCluster = 0;
        unsigned long inFlightDeadlineMs = 0;
        RadioCommandKind nextKind = RadioCommandKind::None;
        char nextOnOff[SPI_DEVICE_MESSAGE_MAX]{};
        uint16_t nextCluster = 0;
        uint16_t nextAttribute = 0;
        uint8_t nextType = 0;
        uint32_t nextValue = 0;
    };

    CoordinatorSwitch zigbeeSwitch;
    IasCieEndpoint iasCie;
    BoundZigbeeDevice boundDevices[kMaxBoundDevices];
    DeviceTopicMap *registeredMap = nullptr;
    uint8_t nextIasZoneId = 1;
    LightStateFn lightStateHandler = nullptr;
    DeviceBoundFn deviceBoundHandler = nullptr;
    RegistryChangedFn registryChangedHandler = nullptr;
    JoinClosedFn joinClosedHandler = nullptr;
    bool registryReady = false;
    unsigned long lastRefreshMs = 0;
    unsigned long pairingUntilMs = 0;
    unsigned long pairingLedToggleMs = 0;
    bool pairingLedOn = false;
    bool started = false;
    DestCommandSlot destFlights[kMaxDestFlights]{};

    void startPairingWindow(uint8_t seconds);
    void stopPairingWindow();
    void updatePairingLed();
    void storeBoundDevice(zb_device_params_t *device);
    void refreshRegisteredShorts();
    void resolveIeeeFromSource(esp_zb_zcl_addr_t source, uint8_t ieee[8], uint16_t *shortAddr);
    void rememberShortIeee(uint16_t shortAddr, const uint8_t ieee[8], uint8_t endpoint);
    void adoptReportIdentity(const uint8_t ieee[8], uint16_t shortAddr, uint8_t endpoint);
    void offerPairingIfNeeded(const uint8_t ieee[8]);
    bool migrateRegisteredIeee(const uint8_t previousIeee[8], const uint8_t nextIeee[8]);
    void pulseInboundDevice(const uint8_t ieee[8]);
    int8_t rssiForShortAddr(uint16_t shortAddr) const;
    DestCommandSlot *destSlotFor(const uint8_t ieee[8], uint8_t endpoint, bool allocate);
    bool transmitOnOff(DestCommandSlot *slot, const char *command);
    bool sendZclWithoutApsAck(
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
    );
    bool transmitWriteAttr(
        DestCommandSlot *slot,
        uint16_t clusterId,
        uint16_t attributeId,
        uint8_t dataType,
        uint32_t attributeValue
    );
    void stashNextOnOff(DestCommandSlot *slot, const char *command);
    void stashNextWriteAttr(
        DestCommandSlot *slot,
        uint16_t clusterId,
        uint16_t attributeId,
        uint8_t dataType,
        uint32_t attributeValue
    );
    void sendNextIfReady(DestCommandSlot *slot);
    void serviceCommandFlights();
    void markInFlight(DestCommandSlot *slot, uint16_t clusterId);
};
