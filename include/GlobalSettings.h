#pragma once

#include "Defines.h"
#include <stdint.h>

#define GLOBAL_CURRENT_SETTINGS_VERSION 1
#define GLOBAL_SETTINGS_MARKER_0 0x5A
#define GLOBAL_SETTINGS_MARKER_1 0x32
#define GLOBAL_SETTINGS_MARKER_2 0x4D
#define GLOBAL_SETTINGS_MARKER_3 0x47

struct NetworkSettings {
    char ssid[64];
    char password[64];
    char hostName[64];
    bool wifiEnabled;
};

struct DeviceTopicEntry {
    uint8_t ieee[8];
    char friendlyName[24];
    char stateTopic[64];
    char commandTopic[64];
    char availabilityTopic[64];
    uint8_t used;
};

struct GlobalSettings {
    char initMarker[4];
    unsigned char version;
    NetworkSettings network;

    char mqttServer[64];
    int mqttPort;
    long mqttReconnectIntervalMs;
    int mqttClientTimeoutMs;
    bool mqttEnabled;
    char mqttUsername[32];
    char mqttPassword[64];
    char mqttClientId[32];
    char mqttBaseTopic[32];

    uint8_t zigbeeChannel;
    uint8_t permitJoinOnBootSec;

    DeviceTopicEntry devices[DEVICE_MAP_SLOTS];
};
