#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ZigbeeCluster.h"

enum ZigbeeDeviceType : uint8_t {
    ZigbeeDeviceTypeUnknown = 0,
    ZigbeeDeviceTypeOnOff = 1,
    ZigbeeDeviceTypeIasZone = 2,
    ZigbeeDeviceTypeWindowCovering = 3
};

constexpr uint8_t kZigbeeDeviceTypeNeverEmitted = 0xFF;

inline uint8_t zigbeeDeviceTypeRank(uint8_t type) {
    switch (type) {
        case ZigbeeDeviceTypeIasZone:
            return 3;
        case ZigbeeDeviceTypeWindowCovering:
            return 2;
        case ZigbeeDeviceTypeOnOff:
            return 1;
        default:
            return 0;
    }
}

inline uint8_t mergeZigbeeDeviceType(uint8_t current, uint8_t incoming) {
    return zigbeeDeviceTypeRank(incoming) > zigbeeDeviceTypeRank(current) ? incoming : current;
}

inline uint8_t zigbeeDeviceTypeFromCluster(uint16_t clusterId) {
    if (zigbeeClusterIsAuxiliary(clusterId)) {
        return ZigbeeDeviceTypeUnknown;
    }
    if (clusterId == kZigbeeClusterIasZone) {
        return ZigbeeDeviceTypeIasZone;
    }
    if (clusterId == kZigbeeClusterWindowCovering) {
        return ZigbeeDeviceTypeWindowCovering;
    }
    if (clusterId == kZigbeeClusterOnOff) {
        return ZigbeeDeviceTypeOnOff;
    }
    return ZigbeeDeviceTypeUnknown;
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

inline const char *zigbeeDeviceTypeJsonId(uint8_t type) {
    switch (type) {
        case ZigbeeDeviceTypeOnOff:
            return "onOff";
        case ZigbeeDeviceTypeIasZone:
            return "iasZone";
        case ZigbeeDeviceTypeWindowCovering:
            return "windowCovering";
        default:
            return "unknown";
    }
}

inline uint8_t zigbeeDeviceTypeFromJsonId(const char *text) {
    if (text == nullptr || text[0] == '\0') {
        return ZigbeeDeviceTypeUnknown;
    }
    if (strcmp(text, "onOff") == 0) {
        return ZigbeeDeviceTypeOnOff;
    }
    if (strcmp(text, "iasZone") == 0) {
        return ZigbeeDeviceTypeIasZone;
    }
    if (strcmp(text, "windowCovering") == 0) {
        return ZigbeeDeviceTypeWindowCovering;
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
