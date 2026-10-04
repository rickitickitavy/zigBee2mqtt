#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ZigbeeCluster.h"

// Classic ids kept for Zigbee radio paths and stored JSON compatibility.
enum ZigbeeDeviceType : uint8_t {
    ZigbeeDeviceTypeUnknown = 0,
    ZigbeeDeviceTypeOnOff = 1,
    ZigbeeDeviceTypeIasZone = 2,
    ZigbeeDeviceTypeWindowCovering = 3
};

constexpr uint8_t kZigbeeDeviceTypeNeverEmitted = 0xFF;
constexpr uint16_t kZigbeeDeviceTypeNoCluster = 0xFFFF;

// Catalog from Zigbee Cluster Library (Bolukan / ZCL Rev 6 draft list).
// https://www.bolukan.nl/?p=354
struct ZigbeeDeviceTypeInfo {
    uint8_t typeId;
    uint16_t clusterId;
    const char *jsonId;
    const char *label;
};

inline const ZigbeeDeviceTypeInfo *zigbeeDeviceTypeCatalog(size_t *count) {
    static const ZigbeeDeviceTypeInfo kCatalog[] = {
        {0, kZigbeeDeviceTypeNoCluster, "unknown", "Unknown"},
        {1, 0x0006, "onOff", "On/Off"},
        {2, 0x0500, "iasZone", "IAS Zone"},
        {3, 0x0102, "windowCovering", "Window Covering"},
        {4, 0x0000, "basic", "Basic"},
        {5, 0x0001, "powerConfiguration", "Power Configuration"},
        {6, 0x0002, "deviceTemperatureConfiguration", "Device Temperature Configuration"},
        {7, 0x0003, "identify", "Identify"},
        {8, 0x0004, "groups", "Groups"},
        {9, 0x0005, "scenes", "Scenes"},
        {10, 0x0007, "onOffSwitchConfiguration", "On/Off Switch Configuration"},
        {11, 0x0008, "levelControl", "Level Control"},
        {12, 0x0009, "alarms", "Alarms"},
        {13, 0x000A, "time", "Time"},
        {14, 0x000B, "rssi", "RSSI"},
        {15, 0x000C, "analogInput", "Analog Input"},
        {16, 0x000D, "analogOutput", "Analog Output"},
        {17, 0x000E, "analogValue", "Analog Value"},
        {18, 0x000F, "binaryInput", "Binary Input"},
        {19, 0x0010, "binaryOutput", "Binary Output"},
        {20, 0x0011, "binaryValue", "Binary Value"},
        {21, 0x0012, "multistateInput", "Multistate Input"},
        {22, 0x0013, "multistateOutput", "Multistate Output"},
        {23, 0x0014, "multistateValue", "Multistate Value"},
        {24, 0x0015, "commissioning", "Commissioning"},
        {25, 0x0016, "partition", "Partition"},
        {26, 0x0019, "otaUpgrade", "OTA Upgrade"},
        {27, 0x001A, "powerProfile", "Power Profile"},
        {28, 0x001B, "applianceControl", "EN50523 Appliance Control"},
        {29, 0x0020, "pollControl", "Poll Control"},
        {30, 0x0022, "mobileDeviceConfiguration", "Mobile Device Configuration"},
        {31, 0x0023, "neighborCleaning", "Neighbor Cleaning"},
        {32, 0x0024, "nearestGateway", "Nearest Gateway"},
        {33, 0x0100, "shadeConfiguration", "Shade Configuration"},
        {34, 0x0101, "doorLock", "Door Lock"},
        {35, 0x0200, "pumpConfigurationAndControl", "Pump Configuration and Control"},
        {36, 0x0201, "thermostat", "Thermostat"},
        {37, 0x0202, "fanControl", "Fan Control"},
        {38, 0x0203, "dehumidificationControl", "Dehumidification Control"},
        {39, 0x0204, "thermostatUserInterfaceConfiguration", "Thermostat User Interface Configuration"},
        {40, 0x0300, "colorControl", "Color Control"},
        {41, 0x0301, "ballastConfiguration", "Ballast Configuration"},
        {42, 0x0400, "illuminanceMeasurement", "Illuminance Measurement"},
        {43, 0x0401, "illuminanceLevelSensing", "Illuminance Level Sensing"},
        {44, 0x0402, "temperatureMeasurement", "Temperature Measurement"},
        {45, 0x0403, "pressureMeasurement", "Pressure Measurement"},
        {46, 0x0404, "flowMeasurement", "Flow Measurement"},
        {47, 0x0405, "relativeHumidity", "Relative Humidity"},
        {48, 0x0406, "occupancySensing", "Occupancy Sensing"},
        {49, 0x0501, "iasAce", "IAS ACE"},
        {50, 0x0502, "iasWd", "IAS WD"},
        {51, 0x0600, "genericTunnel", "Generic Tunnel"},
        {52, 0x0601, "bacnetProtocolTunnel", "BACnet Protocol Tunnel"},
        {53, 0x0602, "bacnetAnalogInputRegular", "Analog Input (BACnet Regular)"},
        {54, 0x0603, "bacnetAnalogInputExtended", "Analog Input (BACnet Extended)"},
        {55, 0x0604, "bacnetAnalogOutputRegular", "Analog Output (BACnet Regular)"},
        {56, 0x0605, "bacnetAnalogOutputExtended", "Analog Output (BACnet Extended)"},
        {57, 0x0606, "bacnetAnalogValueRegular", "Analog Value (BACnet Regular)"},
        {58, 0x0607, "bacnetAnalogValueExtended", "Analog Value (BACnet Extended)"},
        {59, 0x0608, "bacnetBinaryInputRegular", "Binary Input (BACnet Regular)"},
        {60, 0x0609, "bacnetBinaryInputExtended", "Binary Input (BACnet Extended)"},
        {61, 0x060A, "bacnetBinaryOutputRegular", "Binary Output (BACnet Regular)"},
        {62, 0x060B, "bacnetBinaryOutputExtended", "Binary Output (BACnet Extended)"},
        {63, 0x060C, "bacnetBinaryValueRegular", "Binary Value (BACnet Regular)"},
        {64, 0x060D, "bacnetBinaryValueExtended", "Binary Value (BACnet Extended)"},
        {65, 0x060E, "bacnetMultistateInputRegular", "Multistate Input (BACnet Regular)"},
        {66, 0x060F, "bacnetMultistateInputExtended", "Multistate Input (BACnet Extended)"},
        {67, 0x0610, "bacnetMultistateOutputRegular", "Multistate Output (BACnet Regular)"},
        {68, 0x0611, "bacnetMultistateOutputExtended", "Multistate Output (BACnet Extended)"},
        {69, 0x0612, "bacnetMultistateValueRegular", "Multistate Value (BACnet Regular)"},
        {70, 0x0613, "bacnetMultistateValueExtended", "Multistate Value (BACnet Extended)"},
        {71, 0x0614, "protocolTunnel11073", "11073 Protocol Tunnel"},
        {72, 0x0615, "iso7818ProtocolTunnel", "ISO 7818 Protocol Tunnel"},
        {73, 0x0617, "retailTunnel", "Retail Tunnel"},
        {74, 0x0700, "price", "Price"},
        {75, 0x0701, "demandResponseAndLoadControl", "Demand Response and Load Control"},
        {76, 0x0702, "metering", "Metering (Smart Energy)"},
        {77, 0x0703, "messaging", "Messaging (Smart Energy)"},
        {78, 0x0704, "tunneling", "Tunneling (Smart Energy)"},
        {79, 0x0800, "keyEstablishment", "Key Establishment (Smart Energy)"},
        {80, 0x0900, "information", "Information (Telecom)"},
        {81, 0x0904, "voiceOverZigbee", "Voice Over ZigBee"},
        {82, 0x0905, "chatting", "Chatting"},
        {83, 0x0B00, "applianceIdentification", "EN50523 Appliance Identification"},
        {84, 0x0B01, "meterIdentification", "Meter Identification"},
        {85, 0x0B04, "electricalMeasurement", "Electrical Measurement"},
        {86, 0x0B05, "diagnostics", "Diagnostics"},
        {87, 0x1000, "touchlinkCommissioning", "Touchlink Commissioning"},
    };
    if (count != nullptr) {
        *count = sizeof(kCatalog) / sizeof(kCatalog[0]);
    }
    return kCatalog;
}

inline const ZigbeeDeviceTypeInfo *zigbeeDeviceTypeInfo(uint8_t typeId) {
    size_t count = 0;
    const ZigbeeDeviceTypeInfo *catalog = zigbeeDeviceTypeCatalog(&count);
    for (size_t i = 0; i < count; i++) {
        if (catalog[i].typeId == typeId) {
            return &catalog[i];
        }
    }
    return &catalog[0];
}

inline uint8_t zigbeeDeviceTypeRank(uint8_t type) {
    const ZigbeeDeviceTypeInfo *info = zigbeeDeviceTypeInfo(type);
    if (info == nullptr || info->clusterId == kZigbeeDeviceTypeNoCluster) {
        return 0;
    }
    switch (info->clusterId) {
        case 0x0500:
            return 6;
        case 0x0102:
            return 5;
        case 0x0300:
            return 4;
        case 0x0008:
            return 3;
        case 0x0006:
            return 2;
        default:
            return 1;
    }
}

inline uint8_t mergeZigbeeDeviceType(uint8_t current, uint8_t incoming) {
    return zigbeeDeviceTypeRank(incoming) > zigbeeDeviceTypeRank(current) ? incoming : current;
}

inline uint8_t zigbeeDeviceTypeFromCluster(uint16_t clusterId) {
    if (zigbeeClusterIsAuxiliary(clusterId)) {
        return ZigbeeDeviceTypeUnknown;
    }
    size_t count = 0;
    const ZigbeeDeviceTypeInfo *catalog = zigbeeDeviceTypeCatalog(&count);
    for (size_t i = 0; i < count; i++) {
        if (catalog[i].clusterId == clusterId) {
            return catalog[i].typeId;
        }
    }
    return ZigbeeDeviceTypeUnknown;
}

inline uint8_t zigbeeDeviceTypeFromHaDeviceId(uint16_t deviceId) {
    uint16_t clusterId = kZigbeeDeviceTypeNoCluster;
    switch (deviceId) {
        case 0x0000:
        case 0x0002:
        case 0x0009:
        case 0x0051:
        case 0x0100:
        case 0x0103:
        case 0x010A:
            clusterId = 0x0006;
            break;
        case 0x0001:
        case 0x0003:
        case 0x0101:
        case 0x0104:
        case 0x010B:
            clusterId = 0x0008;
            break;
        case 0x0102:
        case 0x0105:
            clusterId = 0x0300;
            break;
        case 0x0200:
        case 0x0201:
        case 0x0202:
        case 0x0203:
            clusterId = 0x0102;
            break;
        case 0x0301:
            clusterId = 0x0201;
            break;
        case 0x0302:
            clusterId = 0x0402;
            break;
        case 0x0402:
            clusterId = 0x0500;
            break;
        case 0x000A:
            clusterId = 0x0101;
            break;
        default:
            break;
    }
    if (clusterId == kZigbeeDeviceTypeNoCluster) {
        return ZigbeeDeviceTypeUnknown;
    }
    return zigbeeDeviceTypeFromCluster(clusterId);
}

inline uint8_t classifyZigbeeDeviceTypeFromInClusters(const uint16_t *inClusters, uint8_t inClusterCount) {
    uint8_t classified = ZigbeeDeviceTypeUnknown;
    if (inClusters == nullptr) {
        return classified;
    }
    for (uint8_t i = 0; i < inClusterCount; i++) {
        classified = mergeZigbeeDeviceType(classified, zigbeeDeviceTypeFromCluster(inClusters[i]));
    }
    return classified;
}

inline uint8_t classifyZigbeeDeviceTypeFromClusterList(
    const uint16_t *clusters,
    uint8_t inClusterCount,
    uint8_t outClusterCount
) {
    uint8_t classified = classifyZigbeeDeviceTypeFromInClusters(clusters, inClusterCount);
    if (clusters == nullptr) {
        return classified;
    }
    for (uint8_t i = 0; i < outClusterCount; i++) {
        classified = mergeZigbeeDeviceType(
            classified,
            zigbeeDeviceTypeFromCluster(clusters[inClusterCount + i])
        );
    }
    return classified;
}

inline bool zigbeeJoinShouldEmit(uint8_t lastEmittedType, uint8_t currentType) {
    if (lastEmittedType == kZigbeeDeviceTypeNeverEmitted) {
        return true;
    }
    return zigbeeDeviceTypeRank(currentType) > zigbeeDeviceTypeRank(lastEmittedType);
}

inline bool zigbeeDeviceTypeIsMeasurement(uint8_t type) {
    const uint16_t clusterId = zigbeeDeviceTypeInfo(type)->clusterId;
    switch (clusterId) {
        case 0x0400:
        case 0x0401:
        case 0x0402:
        case 0x0403:
        case 0x0404:
        case 0x0405:
        case 0x0702:
        case 0x0B04:
            return true;
        default:
            return false;
    }
}

inline const char *zigbeeDeviceTypeJsonId(uint8_t type) {
    return zigbeeDeviceTypeInfo(type)->jsonId;
}

inline const char *zigbeeDeviceTypeLabel(uint8_t type) {
    return zigbeeDeviceTypeInfo(type)->label;
}

inline uint8_t zigbeeDeviceTypeFromJsonId(const char *text) {
    if (text == nullptr || text[0] == '\0') {
        return ZigbeeDeviceTypeUnknown;
    }
    size_t count = 0;
    const ZigbeeDeviceTypeInfo *catalog = zigbeeDeviceTypeCatalog(&count);
    for (size_t i = 0; i < count; i++) {
        if (strcmp(catalog[i].jsonId, text) == 0) {
            return catalog[i].typeId;
        }
    }
    return ZigbeeDeviceTypeUnknown;
}

inline uint8_t zigbeeDeviceTypeFromAttrMessage(const char *message) {
    if (message == nullptr) {
        return ZigbeeDeviceTypeUnknown;
    }
    const char *clusterTag = strstr(message, "cl=0x");
    if (clusterTag == nullptr) {
        clusterTag = strstr(message, "cl=0X");
    }
    if (clusterTag == nullptr) {
        return ZigbeeDeviceTypeUnknown;
    }
    clusterTag += 5;
    char *endPointer = nullptr;
    const unsigned long clusterId = strtoul(clusterTag, &endPointer, 16);
    if (endPointer == clusterTag || clusterId > 0xFFFFul) {
        return ZigbeeDeviceTypeUnknown;
    }
    return zigbeeDeviceTypeFromCluster((uint16_t)clusterId);
}
