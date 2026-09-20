#pragma once

#include "Defines.h"
#include <stddef.h>
#include <stdint.h>

#define GLOBAL_CURRENT_SETTINGS_VERSION 5
#define GLOBAL_SETTINGS_MARKER_0 0x5A
#define GLOBAL_SETTINGS_MARKER_1 0x32
#define GLOBAL_SETTINGS_MARKER_2 0x4D
#define GLOBAL_SETTINGS_MARKER_3 0x47

enum WifiSettingsMode : uint8_t {
    WifiSettingsModeAp = 0,
    WifiSettingsModeSta = 1
};

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
    bool enabled;
    char username[32];
    char password[64];
    char clientId[32];
    char baseTopic[32];
};

struct ZigbeeSettings {
    uint8_t channel;
    uint8_t permitJoinOnBootSec;
};

struct DeviceTopicEntry {
    uint8_t ieee[8];
    char friendlyName[24];
    char stateTopic[64];
    char commandTopic[64];
    char availabilityTopic[64];
    uint8_t channelCount;
    uint8_t fullControl;
    uint8_t zigbeeType;
    uint8_t used;
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
