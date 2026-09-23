#pragma once

#include <ESPAsyncWebServer.h>
#include "SettingsManager.h"
#include "FoundDeviceList.h"
#include "UserStore.h"

class WebConsole {
public:
    using SearchStartFn = bool (*)();
    using SearchStopFn = void (*)();
    using DeviceUpsertedFn = bool (*)(const DeviceTopicEntry *entry);
    using DeviceRemovedFn = bool (*)(const uint8_t ieee[8]);
    using HardwareApplyFn = void (*)(uint32_t spiSpeedHz);
    using DeviceOnlineFn = bool (*)(const uint8_t ieee[8]);
    using DeviceRssiFn = bool (*)(const uint8_t ieee[8], int8_t *rssiDbm);
    using DevicesFileFn = String (*)();
    using GatewayStatusFn = String (*)();
    using DeviceTelemetryFn = void (*)(const uint8_t ieee[8], String &json);
    using DeviceCommandFn = int (*)(const char *ieeeText, const char *payload, int channel);
    using DevicesRestoredFn = bool (*)(const uint8_t (*removedIeees)[8], int removedCount);

    explicit WebConsole(SettingsManager *settingsManager, UserStore *userStore);

    void begin();
    void rebind();
    void setDeviceServices(
        FoundDeviceList *foundDevices,
        SearchStartFn startSearch,
        SearchStopFn stopSearch,
        DeviceUpsertedFn onDeviceUpserted,
        DeviceRemovedFn onDeviceRemoved
    );
    void setHardwareApplyHandler(HardwareApplyFn handler);
    void setDeviceOnlineHandler(DeviceOnlineFn handler);
    void setDeviceRssiHandler(DeviceRssiFn handler);
    void setDevicesFileHandler(DevicesFileFn handler);
    void setGatewayStatusHandler(GatewayStatusFn handler);
    void setDeviceTelemetryHandler(DeviceTelemetryFn handler);
    void setDeviceCommandHandler(DeviceCommandFn handler);
    void setDevicesRestoredHandler(DevicesRestoredFn handler);

private:
    SettingsManager *settingsManager;
    UserStore *userStore;
    FoundDeviceList *foundDevices = nullptr;
    SearchStartFn startSearch = nullptr;
    SearchStopFn stopSearch = nullptr;
    DeviceUpsertedFn onDeviceUpserted = nullptr;
    DeviceRemovedFn onDeviceRemoved = nullptr;
    HardwareApplyFn applyHardware = nullptr;
    DeviceOnlineFn isDeviceOnline = nullptr;
    DeviceRssiFn lastDeviceRssi = nullptr;
    DevicesFileFn devicesFileJson = nullptr;
    GatewayStatusFn gatewayStatusJson = nullptr;
    DeviceTelemetryFn appendDeviceTelemetry = nullptr;
    DeviceCommandFn applyDeviceCommand = nullptr;
    DevicesRestoredFn applyDevicesRestored = nullptr;
    AsyncWebServer server;
    String requestBody;
    bool otaStarted = false;
    bool otaFailed = false;
    int otaCommand = 0;

    void handleRoot(AsyncWebServerRequest *request);
    void handleWifiGet(AsyncWebServerRequest *request);
    void handleWifiPost(AsyncWebServerRequest *request);
    void handleMqttGet(AsyncWebServerRequest *request);
    void handleMqttPost(AsyncWebServerRequest *request);
    void handleZigbeeGet(AsyncWebServerRequest *request);
    void handleZigbeePost(AsyncWebServerRequest *request);
    void handleHardwareGet(AsyncWebServerRequest *request);
    void handleHardwarePost(AsyncWebServerRequest *request);
    void handleSettingsExportGet(AsyncWebServerRequest *request);
    void handleSettingsRestorePost(AsyncWebServerRequest *request);
    void handleThemeGet(AsyncWebServerRequest *request);
    void handleThemePost(AsyncWebServerRequest *request);
    bool applyThemeJson(const char *json, UserRecord *user, String *errorText);
    const UserRecord *authenticatedUser(AsyncWebServerRequest *request);
    bool requireUser(AsyncWebServerRequest *request, const UserRecord **userOut);
    bool requireAdmin(AsyncWebServerRequest *request, const UserRecord **userOut);
    bool requireEditUsers(AsyncWebServerRequest *request, const UserRecord **userOut);
    bool userCanAddDevices(const UserRecord *user) const;
    bool userCanEditDevices(const UserRecord *user) const;
    bool userCanRemoveDevices(const UserRecord *user) const;
    void sendAuthCookie(AsyncWebServerResponse *response, const char *tokenHex, uint32_t maxAgeSec);
    void handleAuthLoginPost(AsyncWebServerRequest *request);
    void handleAuthLogoutPost(AsyncWebServerRequest *request);
    void handleAuthMeGet(AsyncWebServerRequest *request);
    void handleUsersGet(AsyncWebServerRequest *request);
    void handleUsersPost(AsyncWebServerRequest *request);
    void handleUsersUpdatePost(AsyncWebServerRequest *request);
    void handleUsersDelete(AsyncWebServerRequest *request);
    void fillUserFromJson(const char *json, UserRecord *user);
    void sendUserWriteResult(AsyncWebServerRequest *request, UserWriteResult result);
    bool applyMqttJson(const char *json, String *errorText);
    bool applyZigbeeJson(const char *json, String *errorText);
    bool applyHardwareJson(const char *json, String *errorText);
    void handleDevicesGet(AsyncWebServerRequest *request);
    void handleDevicesPost(AsyncWebServerRequest *request);
    void handleDevicesDelete(AsyncWebServerRequest *request);
    void handleDevicesFoundGet(AsyncWebServerRequest *request);
    void handleDevicesSearchPost(AsyncWebServerRequest *request);
    void handleDevicesSearchStopPost(AsyncWebServerRequest *request);
    void handleDevicesStoreGet(AsyncWebServerRequest *request);
    void handleDevicesCommandPost(AsyncWebServerRequest *request);
    void handleGatewayStatusGet(AsyncWebServerRequest *request);
    void appendRequestBody(uint8_t *data, size_t len, size_t index);
    void handleLogGet(AsyncWebServerRequest *request);
    void handleVersionGet(AsyncWebServerRequest *request);
    void handleFirmwareUpdateStatusGet(AsyncWebServerRequest *request);
    void handleOtaUpload(
        AsyncWebServerRequest *request,
        const String &filename,
        size_t index,
        uint8_t *data,
        size_t len,
        bool final,
        int command
    );
    void handleOtaDone(AsyncWebServerRequest *request);
};
