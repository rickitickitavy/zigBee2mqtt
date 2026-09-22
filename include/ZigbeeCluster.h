#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

constexpr uint16_t kZigbeeClusterBasic = 0x0000;
constexpr uint16_t kZigbeeClusterPowerConfig = 0x0001;
constexpr uint16_t kZigbeeClusterIdentify = 0x0003;
constexpr uint16_t kZigbeeClusterGroups = 0x0004;
constexpr uint16_t kZigbeeClusterScenes = 0x0005;
constexpr uint16_t kZigbeeClusterOnOff = 0x0006;
constexpr uint16_t kZigbeeClusterLevelControl = 0x0008;
constexpr uint16_t kZigbeeClusterTime = 0x000A;
constexpr uint16_t kZigbeeClusterOta = 0x0019;
constexpr uint16_t kZigbeeClusterShadeConfig = 0x0100;
constexpr uint16_t kZigbeeClusterDoorLock = 0x0101;
constexpr uint16_t kZigbeeClusterWindowCovering = 0x0102;
constexpr uint16_t kZigbeeClusterThermostat = 0x0201;
constexpr uint16_t kZigbeeClusterFanControl = 0x0202;
constexpr uint16_t kZigbeeClusterColorControl = 0x0300;
constexpr uint16_t kZigbeeClusterIlluminance = 0x0400;
constexpr uint16_t kZigbeeClusterTemperature = 0x0402;
constexpr uint16_t kZigbeeClusterPressure = 0x0403;
constexpr uint16_t kZigbeeClusterHumidity = 0x0405;
constexpr uint16_t kZigbeeClusterOccupancy = 0x0406;
constexpr uint16_t kZigbeeClusterIasZone = 0x0500;
constexpr uint16_t kZigbeeClusterIasAce = 0x0501;
constexpr uint16_t kZigbeeClusterIasWd = 0x0502;
constexpr uint16_t kZigbeeClusterMetering = 0x0702;
constexpr uint16_t kZigbeeClusterElectricalMeasurement = 0x0B04;
constexpr uint16_t kZigbeeAttrBatteryPercentageRemaining = 0x0021;
constexpr uint16_t kZigbeeAttrIasZoneStatus = 0x0002;
constexpr uint16_t kZigbeeAttrCurrentPositionLiftPercentage = 0x0008;
constexpr uint16_t kZigbeeAttrWindowCoveringMoveStatus = 0xF000;
constexpr uint8_t kZigbeeWindowCoveringPositionMax = 0x64;
constexpr uint8_t kZigbeeWindowCoveringMoveDown = 0x00;
constexpr uint8_t kZigbeeWindowCoveringMoveStopped = 0x01;
constexpr uint8_t kZigbeeWindowCoveringMoveUp = 0x02;
constexpr uint8_t kZigbeeWindowCoveringCmdOpen = 0x00;
constexpr uint8_t kZigbeeWindowCoveringCmdClose = 0x01;
constexpr uint8_t kZigbeeWindowCoveringCmdStop = 0x02;
constexpr uint8_t kZigbeeBatteryPercentageRemainingMax = 200;

inline const char *zigbeeClusterName(uint16_t clusterId) {
    switch (clusterId) {
        case kZigbeeClusterBasic:
            return "Basic";
        case kZigbeeClusterPowerConfig:
            return "Power configuration";
        case 0x0002:
            return "Device temperature configuration";
        case kZigbeeClusterIdentify:
            return "Identify";
        case kZigbeeClusterGroups:
            return "Groups";
        case kZigbeeClusterScenes:
            return "Scenes";
        case kZigbeeClusterOnOff:
            return "On/Off";
        case 0x0007:
            return "On/Off switch configuration";
        case kZigbeeClusterLevelControl:
            return "Level control";
        case 0x0009:
            return "Alarms";
        case kZigbeeClusterTime:
            return "Time";
        case kZigbeeClusterOta:
            return "OTA";
        case kZigbeeClusterShadeConfig:
            return "Shade configuration";
        case kZigbeeClusterDoorLock:
            return "Door lock";
        case kZigbeeClusterWindowCovering:
            return "Window covering";
        case kZigbeeClusterThermostat:
            return "Thermostat";
        case kZigbeeClusterFanControl:
            return "Fan control";
        case 0x0203:
            return "Dehumidification control";
        case 0x0204:
            return "Thermostat UI configuration";
        case kZigbeeClusterColorControl:
            return "Color control";
        case 0x0301:
            return "Ballast configuration";
        case kZigbeeClusterIlluminance:
            return "Illuminance measurement";
        case 0x0401:
            return "Illuminance level sensing";
        case kZigbeeClusterTemperature:
            return "Temperature measurement";
        case kZigbeeClusterPressure:
            return "Pressure measurement";
        case 0x0404:
            return "Flow measurement";
        case kZigbeeClusterHumidity:
            return "Relative humidity";
        case kZigbeeClusterOccupancy:
            return "Occupancy sensing";
        case kZigbeeClusterIasZone:
            return "IAS Zone";
        case kZigbeeClusterIasAce:
            return "IAS ACE";
        case kZigbeeClusterIasWd:
            return "IAS WD";
        case 0x0700:
            return "Price";
        case kZigbeeClusterMetering:
            return "Metering";
        case 0x0B01:
            return "Meter identification";
        case kZigbeeClusterElectricalMeasurement:
            return "Electrical measurement";
        case 0x1000:
            return "Touchlink commissioning";
        default:
            return "";
    }
}

inline bool zigbeeClusterIsAuxiliary(uint16_t clusterId) {
    return clusterId == kZigbeeClusterBasic
        || clusterId == kZigbeeClusterPowerConfig
        || clusterId == kZigbeeClusterIdentify
        || clusterId == kZigbeeClusterGroups
        || clusterId == kZigbeeClusterScenes
        || clusterId == kZigbeeClusterTime
        || clusterId == kZigbeeClusterOta;
}

inline bool zigbeeBatteryPercentageRemainingValid(uint32_t rawValue) {
    return rawValue <= kZigbeeBatteryPercentageRemainingMax;
}

inline unsigned zigbeeBatteryPercentageFromRemaining(uint32_t rawValue) {
    return (unsigned)(rawValue / 2u);
}

inline unsigned zigbeeWindowCoveringPositionPercent(uint32_t rawValue) {
    if (rawValue > kZigbeeWindowCoveringPositionMax) {
        return kZigbeeWindowCoveringPositionMax;
    }
    return (unsigned)rawValue;
}

inline const char *zigbeeWindowCoveringMoveName(uint32_t rawValue) {
    switch (rawValue) {
        case kZigbeeWindowCoveringMoveDown:
            return "DOWN";
        case kZigbeeWindowCoveringMoveUp:
            return "UP";
        default:
            return nullptr;
    }
}

inline void zigbeeWindowCoveringFormatStatus(
    char *out,
    size_t outSize,
    bool hasMove,
    uint32_t moveRaw,
    bool hasPosition,
    unsigned positionPercent
) {
    if (out == nullptr || outSize == 0) {
        return;
    }
    out[0] = '\0';
    if (hasMove && moveRaw == kZigbeeWindowCoveringMoveStopped) {
        hasMove = false;
    }
    const char *moveName = hasMove ? zigbeeWindowCoveringMoveName(moveRaw) : nullptr;
    if (hasMove && hasPosition && moveName != nullptr) {
        snprintf(out, outSize, "%s %u%%", moveName, positionPercent);
        return;
    }
    if (hasMove && hasPosition) {
        snprintf(out, outSize, "%u %u%%", (unsigned)moveRaw, positionPercent);
        return;
    }
    if (hasMove && moveName != nullptr) {
        snprintf(out, outSize, "%s", moveName);
        return;
    }
    if (hasMove) {
        snprintf(out, outSize, "%u", (unsigned)moveRaw);
        return;
    }
    if (hasPosition) {
        snprintf(out, outSize, "%u%%", positionPercent);
    }
}
