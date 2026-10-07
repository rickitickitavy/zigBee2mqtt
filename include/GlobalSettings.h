#pragma once

#include "Defines.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define GLOBAL_CURRENT_SETTINGS_VERSION 6
#define GLOBAL_SETTINGS_MARKER_0 0x5A
#define GLOBAL_SETTINGS_MARKER_1 0x32
#define GLOBAL_SETTINGS_MARKER_2 0x4D
#define GLOBAL_SETTINGS_MARKER_3 0x47

enum WifiSettingsMode : uint8_t {
    WifiSettingsModeAp = 0,
    WifiSettingsModeSta = 1
};

enum DeviceTransport : uint8_t {
    DeviceTransportZigbee = 0,
    DeviceTransportMqtt = 1
};

inline DeviceTransport clampDeviceTransport(uint8_t rawTransport) {
    if (rawTransport == DeviceTransportMqtt) {
        return DeviceTransportMqtt;
    }
    return DeviceTransportZigbee;
}

inline const char *deviceTransportJsonId(uint8_t transport) {
    if (transport == DeviceTransportMqtt) {
        return "mqtt";
    }
    return "zigbee";
}

inline DeviceTransport deviceTransportFromJsonId(const char *transportId) {
    if (transportId != nullptr && strcmp(transportId, "mqtt") == 0) {
        return DeviceTransportMqtt;
    }
    return DeviceTransportZigbee;
}

inline uint8_t clampBornIntervalMin(int rawMinutes) {
    if (rawMinutes < MQTT_BORN_INTERVAL_MIN) {
        return MQTT_BORN_INTERVAL_MIN;
    }
    if (rawMinutes > MQTT_BORN_INTERVAL_MAX) {
        return MQTT_BORN_INTERVAL_MAX;
    }
    return (uint8_t)rawMinutes;
}

enum MqttServerType : uint8_t {
    MqttServerTypeDisable = 0,
    MqttServerTypeRemote = 1,
    MqttServerTypeLocal = 2
};

inline MqttServerType clampMqttServerType(uint8_t rawType) {
    if (rawType == MqttServerTypeDisable
        || rawType == MqttServerTypeRemote
        || rawType == MqttServerTypeLocal) {
        return (MqttServerType)rawType;
    }
    return MqttServerTypeDisable;
}

inline const char *mqttServerTypeName(MqttServerType serverType) {
    if (serverType == MqttServerTypeRemote) {
        return "remote";
    }
    if (serverType == MqttServerTypeLocal) {
        return "local";
    }
    return "disable";
}

inline bool parseMqttServerType(const char *text, MqttServerType &serverType) {
    if (text == nullptr) {
        return false;
    }
    if (strcmp(text, "disable") == 0) {
        serverType = MqttServerTypeDisable;
        return true;
    }
    if (strcmp(text, "remote") == 0) {
        serverType = MqttServerTypeRemote;
        return true;
    }
    if (strcmp(text, "local") == 0) {
        serverType = MqttServerTypeLocal;
        return true;
    }
    return false;
}

struct WifiSettings {
    char bssid[64];
    char password[64];
    char deviceName[64];
    char apIp[16];
    WifiSettingsMode mode;
    bool otgEnabled;
};

struct MqttSettings {
    char server[64];
    int port;
    long reconnectIntervalMs;
    int clientTimeoutMs;
    MqttServerType serverType;
    char username[32];
    char password[64];
    char clientId[32];
    char baseTopic[32];
    char serverBornTopic[64];
    uint8_t bornIntervalMin;
};

struct ZigbeeSettings {
    uint8_t channel;
    uint8_t permitJoinOnBootSec;
};

struct SettingsMainCore {
    char initMarker[4];
    unsigned char version;
    WifiSettings wifi;
    MqttSettings mqtt;
    ZigbeeSettings zigbee;
};

constexpr size_t kSettingsMainUsed =
    offsetof(SettingsMainCore, zigbee) + sizeof(ZigbeeSettings);
constexpr size_t kSettingsAlignPad = (128u - (kSettingsMainUsed % 128u)) % 128u;

struct GlobalSettings {
    char initMarker[4];
    unsigned char version;
    WifiSettings wifi;
    MqttSettings mqtt;
    ZigbeeSettings zigbee;
    uint8_t alignPad[kSettingsAlignPad];
    uint8_t reserved[256];
};

static_assert(sizeof(GlobalSettings) <= 4096, "GlobalSettings main block must fit in EEPROM");
static_assert(offsetof(GlobalSettings, reserved) % 128u == 0, "reserved must start on a 128-byte boundary");
static_assert(DEVICE_MAP_SLOTS == 128, "device list must provide 128 slots");
