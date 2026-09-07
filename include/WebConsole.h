#pragma once

#include <ESPAsyncWebServer.h>
#include "SettingsManager.h"

class WebConsole {
public:
    explicit WebConsole(SettingsManager *settingsManager);

    void begin();
    void rebind();

private:
    SettingsManager *settingsManager;
    AsyncWebServer server;
    String requestBody;
    bool otaStarted = false;
    bool otaFailed = false;

    void handleRoot(AsyncWebServerRequest *request);
    void handleWifiGet(AsyncWebServerRequest *request);
    void handleWifiPost(AsyncWebServerRequest *request);
    void handleMqttGet(AsyncWebServerRequest *request);
    void handleMqttPost(AsyncWebServerRequest *request);
    void handleLogGet(AsyncWebServerRequest *request);
    void handleVersionGet(AsyncWebServerRequest *request);
    void handleOtaUpload(
        AsyncWebServerRequest *request,
        const String &filename,
        size_t index,
        uint8_t *data,
        size_t len,
        bool final
    );
    void handleOtaDone(AsyncWebServerRequest *request);
};
