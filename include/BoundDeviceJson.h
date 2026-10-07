#pragma once

#include <Arduino.h>
#include <stdio.h>
#include "DeviceTopicMap.h"

inline void appendBoundRadioDeviceJson(
    String &json,
    bool &first,
    DeviceTopicMap *topicMap,
    const uint8_t ieee[8],
    uint16_t shortAddr,
    uint8_t endpoint,
    const char *manufacturer,
    const char *model
) {
    if (topicMap == nullptr || ieee == nullptr) {
        return;
    }
    if (!first) {
        json += ',';
    }
    first = false;
    char shortHex[8];
    snprintf(shortHex, sizeof(shortHex), "%x", (unsigned)shortAddr);
    char endpointText[8];
    snprintf(endpointText, sizeof(endpointText), "%u", (unsigned)endpoint);
    json += "{\"ieee\":\"";
    json += topicMap->formatIeee(ieee);
    json += "\",\"nwk\":\"0x";
    json += shortHex;
    json += "\",\"endpoint\":";
    json += endpointText;
    json += ",\"manufacturer\":\"";
    json += manufacturer != nullptr ? manufacturer : "";
    json += "\",\"model\":\"";
    json += model != nullptr ? model : "";
    json += '"';
    DeviceTopicEntry *mapped = topicMap->findByIeee(ieee);
    if (mapped != nullptr) {
        json += ",\"name\":\"";
        json += deviceTopicCStr(mapped->friendlyName);
        json += "\",\"state\":\"";
        json += deviceTopicCStr(mapped->stateTopic);
        json += "\",\"command\":\"";
        json += deviceTopicCStr(mapped->commandTopic);
        json += '"';
    }
    json += '}';
}
