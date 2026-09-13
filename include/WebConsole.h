#pragma once

#include <ESPAsyncWebServer.h>
#include "SettingsManager.h"
#include "FoundDeviceList.h"

class WebConsole {
public:
    using SearchStartFn = bool (*)();
    using SearchStopFn = void (*)();
    using DeviceUpsertedFn = bool (*)(const DeviceTopicEntry *entry);
    using DeviceRemovedFn = bool (*)(const uint8_t ieee[8]);
    using HardwareApplyFn = void (*)(uint32_t spiSpeedHz);
    using DeviceOnlineFn = bool (*)(const uint8_t ieee[8]);
    using DevicesFileFn = String (*)();

    explicit WebConsole(SettingsManager *settingsManager);

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
    void setDevicesFileHandler(DevicesFileFn handler);

private:
    SettingsManager *settingsManager;
    FoundDeviceList *foundDevices = nullptr;
    SearchStartFn startSearch = nullptr;
    SearchStopFn stopSearch = nullptr;
    DeviceUpsertedFn onDeviceUpserted = nullptr;
    DeviceRemovedFn onDeviceRemoved = nullptr;
    HardwareApplyFn applyHardware = nullptr;
    DeviceOnlineFn isDeviceOnline = nullptr;
    DevicesFileFn devicesFileJson = nullptr;
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
    void handleDevicesGet(AsyncWebServerRequest *request);
    void handleDevicesPost(AsyncWebServerRequest *request);
    void handleDevicesDelete(AsyncWebServerRequest *request);
    void handleDevicesFoundGet(AsyncWebServerRequest *request);
    void handleDevicesSearchPost(AsyncWebServerRequest *request);
    void handleDevicesSearchStopPost(AsyncWebServerRequest *request);
    void handleDevicesStoreGet(AsyncWebServerRequest *request);
    void appendRequestBody(uint8_t *data, size_t len, size_t index);
    void handleLogGet(AsyncWebServerRequest *request);
    void handleVersionGet(AsyncWebServerRequest *request);
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
