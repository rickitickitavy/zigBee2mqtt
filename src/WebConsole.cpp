#include "WebConsole.h"
#include "Logger.h"
#include "Defines.h"
#include "JsonField.h"

#include <LittleFS.h>
#include <Update.h>
#include <IPAddress.h>

static const char kMissingFsPage[] PROGMEM =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>z2m-gateway</title></head>"
    "<body><p>LittleFS web files are missing. Flash the filesystem: <code>pio run -t uploadfs</code></p></body></html>";

WebConsole::WebConsole(SettingsManager *settingsManager)
    : settingsManager(settingsManager), server(80) {}

void WebConsole::begin() {
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) { handleRoot(request); });
    server.on("/index.html", HTTP_GET, [this](AsyncWebServerRequest *request) { handleRoot(request); });
    server.serveStatic("/css", LittleFS, "/css");

    server.on("/api/wifi", HTTP_GET, [this](AsyncWebServerRequest *request) { handleWifiGet(request); });
    server.on(
        "/api/wifi",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleWifiPost(request); },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)total;
            appendRequestBody(data, len, index);
        }
    );

    server.on("/api/mqtt", HTTP_GET, [this](AsyncWebServerRequest *request) { handleMqttGet(request); });
    server.on(
        "/api/mqtt",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleMqttPost(request); },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)total;
            appendRequestBody(data, len, index);
        }
    );

    server.on("/api/zigbee", HTTP_GET, [this](AsyncWebServerRequest *request) { handleZigbeeGet(request); });
    server.on(
        "/api/zigbee",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleZigbeePost(request); },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)total;
            appendRequestBody(data, len, index);
        }
    );

    server.on(
        "/api/hardware",
        HTTP_GET,
        [this](AsyncWebServerRequest *request) { handleHardwareGet(request); }
    );
    server.on(
        "/api/hardware",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleHardwarePost(request); },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)total;
            appendRequestBody(data, len, index);
        }
    );
    server.on(
        "/api/hw",
        HTTP_GET,
        [this](AsyncWebServerRequest *request) { handleHardwareGet(request); }
    );

    server.on("/api/devices/store", HTTP_GET, [this](AsyncWebServerRequest *request) { handleDevicesStoreGet(request); });
    server.on("/api/devices/found", HTTP_GET, [this](AsyncWebServerRequest *request) { handleDevicesFoundGet(request); });
    server.on(
        "/api/devices/search/stop",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleDevicesSearchStopPost(request); },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)total;
            appendRequestBody(data, len, index);
        }
    );
    server.on(
        "/api/devices/search",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleDevicesSearchPost(request); },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)total;
            appendRequestBody(data, len, index);
        }
    );
    server.on("/api/devices", HTTP_GET, [this](AsyncWebServerRequest *request) { handleDevicesGet(request); });
    server.on(
        "/api/devices",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleDevicesPost(request); },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)total;
            appendRequestBody(data, len, index);
        }
    );
    server.on(
        "/api/devices",
        HTTP_DELETE,
        [this](AsyncWebServerRequest *request) { handleDevicesDelete(request); },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)total;
            appendRequestBody(data, len, index);
        }
    );

    server.on("/api/log", HTTP_GET, [this](AsyncWebServerRequest *request) { handleLogGet(request); });
    server.on("/api/version", HTTP_GET, [this](AsyncWebServerRequest *request) { handleVersionGet(request); });
    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGatewayStatusGet(request); });

    server.on(
        "/update",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleOtaDone(request); },
        [this](
            AsyncWebServerRequest *request,
            const String &filename,
            size_t index,
            uint8_t *data,
            size_t len,
            bool final
        ) { handleOtaUpload(request, filename, index, data, len, final, U_FLASH); }
    );

    server.on(
        "/update/data",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleOtaDone(request); },
        [this](
            AsyncWebServerRequest *request,
            const String &filename,
            size_t index,
            uint8_t *data,
            size_t len,
            bool final
        ) {
#ifdef U_FS
            const int filesystemCommand = U_FS;
#else
            const int filesystemCommand = U_SPIFFS;
#endif
            handleOtaUpload(request, filename, index, data, len, final, filesystemCommand);
        }
    );

    server.begin();
    LOGGER.info("Web console on port 80");
}

void WebConsole::rebind() {
    LOGGER.info("Rebinding web console after Wi-Fi change");
    server.end();
    delay(50);
    server.begin();
    LOGGER.info("Web console rebound on port 80");
}

void WebConsole::handleRoot(AsyncWebServerRequest *request) {
    if (LittleFS.exists("/index.html")) {
        request->send(LittleFS, "/index.html", "text/html");
        return;
    }
    request->send(200, "text/html", kMissingFsPage);
}

void WebConsole::handleWifiGet(AsyncWebServerRequest *request) {
    GlobalSettings *settings = settingsManager->getSettings();
    String json = "{";
    json += "\"bssid\":\"";
    appendJsonEscaped(json, settings->wifi.bssid, sizeof(settings->wifi.bssid));
    json += "\",\"password\":\"";
    appendJsonEscaped(json, settings->wifi.password, sizeof(settings->wifi.password));
    json += "\",\"deviceName\":\"";
    appendJsonEscaped(json, settings->wifi.deviceName, sizeof(settings->wifi.deviceName));
    json += "\",\"apIp\":\"";
    appendJsonEscaped(json, settings->wifi.apIp, sizeof(settings->wifi.apIp));
    json += "\",\"mode\":\"";
    json += settings->wifi.mode == WifiSettingsModeSta ? "STA" : "AP";
    json += "\",\"otgEnabled\":";
    json += settings->wifi.otgEnabled ? "true" : "false";
    json += "}";
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleWifiPost(AsyncWebServerRequest *request) {
    String bssid;
    String password;
    String deviceName;
    String apIpText;
    String modeText;
    bool otgEnabled = false;
    bool haveBssid = extractJsonString(requestBody.c_str(), "bssid", bssid);
    if (!haveBssid) {
        haveBssid = extractJsonString(requestBody.c_str(), "bsid", bssid);
    }
    const bool havePassword = extractJsonString(requestBody.c_str(), "password", password);
    const bool haveDeviceName = extractJsonString(requestBody.c_str(), "deviceName", deviceName);
    const bool haveApIp = extractJsonString(requestBody.c_str(), "apIp", apIpText);
    const bool haveMode = extractJsonString(requestBody.c_str(), "mode", modeText);
    extractJsonBool(requestBody.c_str(), "otgEnabled", otgEnabled);
    if (!haveBssid || !havePassword || !haveDeviceName || !haveMode) {
        request->send(400, "text/plain", "Need bssid, password, deviceName, mode");
        return;
    }
    if (!haveApIp || apIpText.length() == 0) {
        apIpText = WIFI_DEFAULT_AP_IP;
    }
    IPAddress apIp;
    if (!apIp.fromString(apIpText)) {
        request->send(400, "text/plain", "apIp must be a dotted IPv4 address");
        return;
    }
    modeText.toUpperCase();
    if (modeText != "AP" && modeText != "STA") {
        request->send(400, "text/plain", "mode must be AP or STA");
        return;
    }
    if (password.length() == 0) {
        password = WIFI_DEFAULT_PASSWORD;
    }

    GlobalSettings *settings = settingsManager->getSettings();
    if (bssid.length() == 0) {
        bssid = WIFI_DEFAULT_BSSID;
    }
    strncpy(settings->wifi.bssid, bssid.c_str(), sizeof(settings->wifi.bssid) - 1);
    settings->wifi.bssid[sizeof(settings->wifi.bssid) - 1] = '\0';
    strncpy(settings->wifi.password, password.c_str(), sizeof(settings->wifi.password) - 1);
    settings->wifi.password[sizeof(settings->wifi.password) - 1] = '\0';
    strncpy(settings->wifi.deviceName, deviceName.c_str(), sizeof(settings->wifi.deviceName) - 1);
    settings->wifi.deviceName[sizeof(settings->wifi.deviceName) - 1] = '\0';
    strncpy(settings->wifi.apIp, apIp.toString().c_str(), sizeof(settings->wifi.apIp) - 1);
    settings->wifi.apIp[sizeof(settings->wifi.apIp) - 1] = '\0';
    settings->wifi.mode = modeText == "STA" ? WifiSettingsModeSta : WifiSettingsModeAp;
    settings->wifi.otgEnabled = otgEnabled;
    settingsManager->saveMain(true);
    request->send(200, "text/plain", "Saved. Device will restart to apply Wi-Fi.");
}

void WebConsole::handleMqttGet(AsyncWebServerRequest *request) {
    GlobalSettings *settings = settingsManager->getSettings();
    String json = "{";
    json += "\"enabled\":";
    json += settings->mqtt.enabled ? "true" : "false";
    json += ",\"server\":\"";
    appendJsonEscaped(json, settings->mqtt.server, sizeof(settings->mqtt.server));
    json += "\",\"port\":";
    json += String(settings->mqtt.port);
    json += ",\"username\":\"";
    appendJsonEscaped(json, settings->mqtt.username, sizeof(settings->mqtt.username));
    json += "\",\"password\":\"";
    appendJsonEscaped(json, settings->mqtt.password, sizeof(settings->mqtt.password));
    json += "\",\"clientId\":\"";
    appendJsonEscaped(json, settings->mqtt.clientId, sizeof(settings->mqtt.clientId));
    json += "\",\"baseTopic\":\"";
    appendJsonEscaped(json, settings->mqtt.baseTopic, sizeof(settings->mqtt.baseTopic));
    json += "\",\"reconnectIntervalMs\":";
    json += String((long)settings->mqtt.reconnectIntervalMs);
    json += ",\"clientTimeoutMs\":";
    json += String(settings->mqtt.clientTimeoutMs);
    json += "}";
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleMqttPost(AsyncWebServerRequest *request) {
    String server;
    String username;
    String password;
    String clientId;
    String baseTopic;
    int port = DEFAULT_MQTT_PORT;
    int reconnectIntervalMs = DEFAULT_MQTT_RECONNECT_MS;
    int clientTimeoutMs = DEFAULT_MQTT_CLIENT_TIMEOUT_MS;
    bool enabled = true;

    const bool haveServer = extractJsonString(requestBody.c_str(), "server", server);
    const bool haveClientId = extractJsonString(requestBody.c_str(), "clientId", clientId);
    const bool haveBaseTopic = extractJsonString(requestBody.c_str(), "baseTopic", baseTopic);
    extractJsonString(requestBody.c_str(), "username", username);
    extractJsonString(requestBody.c_str(), "password", password);
    extractJsonBool(requestBody.c_str(), "enabled", enabled);
    extractJsonInt(requestBody.c_str(), "port", port);
    extractJsonInt(requestBody.c_str(), "reconnectIntervalMs", reconnectIntervalMs);
    extractJsonInt(requestBody.c_str(), "clientTimeoutMs", clientTimeoutMs);

    if (!haveServer || !haveClientId || !haveBaseTopic) {
        request->send(400, "text/plain", "Need server, clientId, baseTopic");
        return;
    }
    if (server.length() == 0) {
        server = DEFAULT_MQTT_SERVER;
    }
    if (clientId.length() == 0) {
        clientId = DEFAULT_MQTT_CLIENT_ID;
    }
    if (baseTopic.length() == 0) {
        baseTopic = DEFAULT_MQTT_BASE_TOPIC;
    }
    if (port < 1 || port > 65535) {
        request->send(400, "text/plain", "port must be 1-65535");
        return;
    }
    if (reconnectIntervalMs < 500) {
        reconnectIntervalMs = DEFAULT_MQTT_RECONNECT_MS;
    }
    SettingsManager::clampMqttClientTimeout(clientTimeoutMs);

    GlobalSettings *settings = settingsManager->getSettings();
    strncpy(settings->mqtt.server, server.c_str(), sizeof(settings->mqtt.server) - 1);
    settings->mqtt.server[sizeof(settings->mqtt.server) - 1] = '\0';
    settings->mqtt.port = port;
    settings->mqtt.reconnectIntervalMs = reconnectIntervalMs;
    settings->mqtt.clientTimeoutMs = clientTimeoutMs;
    settings->mqtt.enabled = enabled;
    strncpy(settings->mqtt.username, username.c_str(), sizeof(settings->mqtt.username) - 1);
    settings->mqtt.username[sizeof(settings->mqtt.username) - 1] = '\0';
    strncpy(settings->mqtt.password, password.c_str(), sizeof(settings->mqtt.password) - 1);
    settings->mqtt.password[sizeof(settings->mqtt.password) - 1] = '\0';
    strncpy(settings->mqtt.clientId, clientId.c_str(), sizeof(settings->mqtt.clientId) - 1);
    settings->mqtt.clientId[sizeof(settings->mqtt.clientId) - 1] = '\0';
    strncpy(settings->mqtt.baseTopic, baseTopic.c_str(), sizeof(settings->mqtt.baseTopic) - 1);
    settings->mqtt.baseTopic[sizeof(settings->mqtt.baseTopic) - 1] = '\0';
    settingsManager->saveMain(true);
    request->send(200, "text/plain", "Saved. Device will restart to apply MQTT.");
}

void WebConsole::appendRequestBody(uint8_t *data, size_t len, size_t index) {
    if (index == 0) {
        requestBody = "";
    }
    for (size_t byteIndex = 0; byteIndex < len; byteIndex++) {
        requestBody += (char)data[byteIndex];
    }
}

void WebConsole::handleZigbeeGet(AsyncWebServerRequest *request) {
    GlobalSettings *settings = settingsManager->getSettings();
    String json = "{";
    json += "\"channel\":";
    json += String(settings->zigbee.channel);
    json += ",\"permitJoinOnBootSec\":";
    json += String(settings->zigbee.permitJoinOnBootSec);
    json += "}";
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleZigbeePost(AsyncWebServerRequest *request) {
    int channel = DEFAULT_ZIGBEE_CHANNEL;
    int permitJoinOnBootSec = DEFAULT_PERMIT_JOIN_SEC;
    if (!extractJsonInt(requestBody.c_str(), "channel", channel)
        || !extractJsonInt(requestBody.c_str(), "permitJoinOnBootSec", permitJoinOnBootSec)) {
        request->send(400, "text/plain", "Need channel, permitJoinOnBootSec");
        return;
    }
    if (channel < 11 || channel > 26) {
        request->send(400, "text/plain", "channel must be 11-26");
        return;
    }
    if (permitJoinOnBootSec < 0 || permitJoinOnBootSec > 254) {
        request->send(400, "text/plain", "permitJoinOnBootSec must be 0-254");
        return;
    }
    GlobalSettings *settings = settingsManager->getSettings();
    settings->zigbee.channel = (uint8_t)channel;
    settings->zigbee.permitJoinOnBootSec = (uint8_t)permitJoinOnBootSec;
    settingsManager->saveMain(true);
    request->send(200, "text/plain", "Saved. Device will restart to apply ZigBee.");
}

void WebConsole::setDeviceOnlineHandler(WebConsole::DeviceOnlineFn handler) {
    isDeviceOnline = handler;
}

void WebConsole::setDeviceRssiHandler(WebConsole::DeviceRssiFn handler) {
    lastDeviceRssi = handler;
}

void WebConsole::setGatewayStatusHandler(WebConsole::GatewayStatusFn handler) {
    gatewayStatusJson = handler;
}

void WebConsole::setDevicesFileHandler(WebConsole::DevicesFileFn handler) {
    devicesFileJson = handler;
}

void WebConsole::setHardwareApplyHandler(WebConsole::HardwareApplyFn handler) {
    applyHardware = handler;
}

void WebConsole::handleHardwareGet(AsyncWebServerRequest *request) {
    const uint32_t speedHz = settingsManager->spiSpeedHz();
    LOGGER.info("Hardware GET spiSpeedHz=" + String((unsigned long)speedHz));
    String json = "{\"spiSpeedHz\":";
    json += String((unsigned long)speedHz);
    json += "}";
    request->send(200, "application/json", json);
}

void WebConsole::handleHardwarePost(AsyncWebServerRequest *request) {
    int spiSpeedHz = DEFAULT_SPI_SPEED_HZ;
    if (!extractJsonInt(requestBody.c_str(), "spiSpeedHz", spiSpeedHz)) {
        request->send(400, "text/plain", "Need spiSpeedHz");
        return;
    }
    const uint32_t clamped = SettingsManager::clampSpiSpeedHz((uint32_t)spiSpeedHz);
    if (clamped != (uint32_t)spiSpeedHz) {
        request->send(400, "text/plain", "spiSpeedHz must be 100000-40000000");
        return;
    }
    settingsManager->setSpiSpeedHz(clamped);
    settingsManager->saveMain(false);
    if (applyHardware != nullptr) {
        applyHardware(clamped);
    }
    request->send(200, "text/plain", "Saved. SPI speed is active now.");
}

void WebConsole::setDeviceServices(
    FoundDeviceList *foundList,
    WebConsole::SearchStartFn startSearchFn,
    WebConsole::SearchStopFn stopSearchFn,
    WebConsole::DeviceUpsertedFn upserted,
    WebConsole::DeviceRemovedFn removed
) {
    foundDevices = foundList;
    startSearch = startSearchFn;
    stopSearch = stopSearchFn;
    onDeviceUpserted = upserted;
    onDeviceRemoved = removed;
}

void WebConsole::handleDevicesGet(AsyncWebServerRequest *request) {
    DeviceTopicMap *deviceMap = settingsManager->deviceMap();
    String json = deviceMap->listJson(isDeviceOnline, lastDeviceRssi);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleDevicesPost(AsyncWebServerRequest *request) {
    String ieeeText;
    String friendlyName;
    String stateTopic;
    String commandTopic;
    String availability;
    if (!extractJsonString(requestBody.c_str(), "ieee", ieeeText)) {
        request->send(400, "text/plain", "Need ieee");
        return;
    }
    extractJsonString(requestBody.c_str(), "name", friendlyName);
    extractJsonString(requestBody.c_str(), "friendlyName", friendlyName);
    extractJsonString(requestBody.c_str(), "state", stateTopic);
    extractJsonString(requestBody.c_str(), "command", commandTopic);
    extractJsonString(requestBody.c_str(), "availability", availability);
    int parsedChannels = DEVICE_CHANNEL_COUNT_DEFAULT;
    if (!extractJsonInt(requestBody.c_str(), "channels", parsedChannels)) {
        parsedChannels = DEVICE_CHANNEL_COUNT_DEFAULT;
    }
    if (friendlyName.length() == 0) {
        request->send(400, "text/plain", "Need friendly name");
        return;
    }
    uint8_t ieee[8];
    DeviceTopicMap *deviceMap = settingsManager->deviceMap();
    if (!deviceMap->parseIeee(ieeeText.c_str(), ieee)) {
        request->send(400, "text/plain", "Bad IEEE");
        return;
    }
    DeviceTopicEntry *entry = deviceMap->upsert(
        ieee,
        friendlyName.c_str(),
        stateTopic.c_str(),
        commandTopic.c_str(),
        availability.c_str(),
        DeviceTopicMap::normalizeChannelCount(parsedChannels)
    );
    if (entry == nullptr) {
        request->send(400, "text/plain", "Device map full");
        return;
    }
    bool parsedFullControl = false;
    if (extractJsonBool(requestBody.c_str(), "fullControl", parsedFullControl)) {
        entry->fullControl = parsedFullControl ? 1 : 0;
    }
    if (foundDevices != nullptr) {
        foundDevices->removeIeee(ieee);
    }
    if (onDeviceUpserted != nullptr && !onDeviceUpserted(entry)) {
        request->send(503, "text/plain", "Slave is not ready to store the device");
        return;
    }
    request->send(200, "text/plain", "Saved");
}

void WebConsole::handleDevicesDelete(AsyncWebServerRequest *request) {
    String ieeeText;
    if (!extractJsonString(requestBody.c_str(), "ieee", ieeeText)) {
        request->send(400, "text/plain", "Need ieee");
        return;
    }
    uint8_t ieee[8];
    DeviceTopicMap *deviceMap = settingsManager->deviceMap();
    if (!deviceMap->parseIeee(ieeeText.c_str(), ieee)) {
        request->send(400, "text/plain", "Bad IEEE");
        return;
    }
    if (!deviceMap->removeByIeee(ieee)) {
        request->send(404, "text/plain", "Device not found");
        return;
    }
    if (onDeviceRemoved != nullptr && !onDeviceRemoved(ieee)) {
        request->send(503, "text/plain", "Slave is not ready to store the device");
        return;
    }
    request->send(200, "text/plain", "Deleted");
}

void WebConsole::handleDevicesFoundGet(AsyncWebServerRequest *request) {
    if (foundDevices == nullptr) {
        request->send(200, "application/json", "[]");
        return;
    }
    String json = foundDevices->listJson(settingsManager->deviceMap());
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleDevicesSearchPost(AsyncWebServerRequest *request) {
    if (startSearch == nullptr || !startSearch()) {
        LOGGER.warning("Device search did not start");
        request->send(500, "text/plain", "Slave is not ready for pairing");
        return;
    }
    LOGGER.info("Device search started");
    request->send(200, "text/plain", "Search started");
}

void WebConsole::handleDevicesStoreGet(AsyncWebServerRequest *request) {
    String json = devicesFileJson != nullptr ? devicesFileJson() : String("[]");
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleGatewayStatusGet(AsyncWebServerRequest *request) {
    String json = gatewayStatusJson != nullptr ? gatewayStatusJson() : String("{}");
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleDevicesSearchStopPost(AsyncWebServerRequest *request) {
    if (stopSearch != nullptr) {
        stopSearch();
    }
    request->send(200, "text/plain", "Search stopped");
}

void WebConsole::handleLogGet(AsyncWebServerRequest *request) {
    size_t start = 0;
    size_t length = 0;
    LOGGER.snapshotRing(&start, &length);
    AsyncWebServerResponse *response = request->beginResponse(
        "text/plain",
        length,
        [start, length](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            return LOGGER.copyRingSlice(start, length, index, reinterpret_cast<char *>(buffer), maxLen);
        }
    );
    request->send(response);
}

void WebConsole::handleVersionGet(AsyncWebServerRequest *request) {
    String json = "{\"version\":\"";
    json += FIRMWARE_VERSION;
    json += "\"}";
    request->send(200, "application/json", json);
}

void WebConsole::handleOtaUpload(
    AsyncWebServerRequest *request,
    const String &filename,
    size_t index,
    uint8_t *data,
    size_t len,
    bool final,
    int command
) {
    (void)request;
    (void)filename;
    if (index == 0) {
        otaStarted = true;
        otaFailed = false;
        otaCommand = command;
        LOGGER.info(command == U_FLASH ? "HTTP OTA firmware start" : "HTTP OTA filesystem start");
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, command)) {
            LOGGER.error("HTTP OTA begin failed");
            otaFailed = true;
            return;
        }
    }
    if (otaFailed) {
        return;
    }
    if (len > 0 && Update.write(data, len) != len) {
        LOGGER.error("HTTP OTA write failed");
        otaFailed = true;
        return;
    }
    if (final) {
        if (!Update.end(true)) {
            LOGGER.error("HTTP OTA end failed");
            otaFailed = true;
        }
    }
}

void WebConsole::handleOtaDone(AsyncWebServerRequest *request) {
    if (!otaStarted || otaFailed || Update.hasError()) {
        String errorMessage = Update.hasError() ? String(Update.errorString()) : String("Upload failed");
        LOGGER.error("HTTP OTA failed: " + errorMessage);
        Update.abort();
        otaStarted = false;
        otaFailed = false;
        request->send(500, "text/plain", errorMessage);
        return;
    }
    LOGGER.info(otaCommand == U_FLASH ? "HTTP OTA firmware finished" : "HTTP OTA filesystem finished");
    otaStarted = false;
    request->send(
        200,
        "text/plain",
        otaCommand == U_FLASH ? "Firmware written. Device will restart."
                              : "Filesystem written. Device will restart."
    );
    settingsManager->requestRestart();
}
