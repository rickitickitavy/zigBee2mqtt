#include "WebConsole.h"
#include "Logger.h"
#include "Defines.h"
#include "JsonField.h"
#include "ZigbeeDeviceType.h"
#include "FirmwareOta.h"
#include "UserStore.h"

#include <LittleFS.h>
#include <Update.h>
#include <IPAddress.h>
#include <string.h>

static const char kMissingFsPage[] PROGMEM =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>z2m-gateway</title></head>"
    "<body><p>LittleFS web files are missing. Flash the filesystem: <code>pio run -t uploadfs</code></p></body></html>";

WebConsole::WebConsole(SettingsManager *settingsManager, UserStore *userStore)
    : settingsManager(settingsManager), userStore(userStore), server(80) {}

void WebConsole::begin() {
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) { handleRoot(request); });
    server.on("/index.html", HTTP_GET, [this](AsyncWebServerRequest *request) { handleRoot(request); });
    server.serveStatic("/css", LittleFS, "/css");

    server.on(
        "/api/auth/login",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleAuthLoginPost(request); },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)total;
            appendRequestBody(data, len, index);
        }
    );
    server.on(
        "/api/auth/logout",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleAuthLogoutPost(request); }
    );
    server.on("/api/auth/me", HTTP_GET, [this](AsyncWebServerRequest *request) { handleAuthMeGet(request); });
    server.on("/api/users", HTTP_GET, [this](AsyncWebServerRequest *request) { handleUsersGet(request); });
    server.on(
        "/api/users",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleUsersPost(request); },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)total;
            appendRequestBody(data, len, index);
        }
    );
    server.on(
        "/api/users/update",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleUsersUpdatePost(request); },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)total;
            appendRequestBody(data, len, index);
        }
    );
    server.on(
        "/api/users",
        HTTP_DELETE,
        [this](AsyncWebServerRequest *request) { handleUsersDelete(request); },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)total;
            appendRequestBody(data, len, index);
        }
    );

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
    server.on(
        "/api/settings/export",
        HTTP_GET,
        [this](AsyncWebServerRequest *request) { handleSettingsExportGet(request); }
    );
    server.on(
        "/api/settings/restore",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleSettingsRestorePost(request); },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)total;
            appendRequestBody(data, len, index);
        }
    );
    server.on("/api/theme", HTTP_GET, [this](AsyncWebServerRequest *request) { handleThemeGet(request); });
    server.on(
        "/api/theme",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleThemePost(request); },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)total;
            appendRequestBody(data, len, index);
        }
    );

    server.on("/api/devices/store", HTTP_GET, [this](AsyncWebServerRequest *request) { handleDevicesStoreGet(request); });
    server.on(
        "/api/devices/command",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleDevicesCommandPost(request); },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)total;
            appendRequestBody(data, len, index);
        }
    );
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
    server.on(
        "/api/update/status",
        HTTP_GET,
        [this](AsyncWebServerRequest *request) { handleFirmwareUpdateStatusGet(request); }
    );
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
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html", "text/html");
        response->addHeader("Cache-Control", "no-store");
        request->send(response);
        return;
    }
    request->send(200, "text/html", kMissingFsPage);
}

const UserRecord *WebConsole::authenticatedUser(AsyncWebServerRequest *request) {
    if (userStore == nullptr || request == nullptr || !request->hasHeader("Cookie")) {
        return nullptr;
    }
    return userStore->sessionUser(request->header("Cookie").c_str(), millis(), nullptr, 0);
}

bool WebConsole::requireUser(AsyncWebServerRequest *request, const UserRecord **userOut) {
    const UserRecord *user = authenticatedUser(request);
    if (user == nullptr) {
        request->send(401, "text/plain", "Unauthorized");
        return false;
    }
    if (userOut != nullptr) {
        *userOut = user;
    }
    return true;
}

bool WebConsole::requireAdmin(AsyncWebServerRequest *request, const UserRecord **userOut) {
    const UserRecord *user = nullptr;
    if (!requireUser(request, &user)) {
        return false;
    }
    if (!user->isAdmin) {
        request->send(403, "text/plain", "Forbidden");
        return false;
    }
    if (userOut != nullptr) {
        *userOut = user;
    }
    return true;
}

bool WebConsole::requireEditUsers(AsyncWebServerRequest *request, const UserRecord **userOut) {
    const UserRecord *user = nullptr;
    if (!requireUser(request, &user)) {
        return false;
    }
    if (!user->isAdmin && !user->editUsers) {
        request->send(403, "text/plain", "Forbidden");
        return false;
    }
    if (userOut != nullptr) {
        *userOut = user;
    }
    return true;
}

bool WebConsole::userCanAddDevices(const UserRecord *user) const {
    return user != nullptr && (user->isAdmin || user->addDevices);
}

bool WebConsole::userCanEditDevices(const UserRecord *user) const {
    return user != nullptr && (user->isAdmin || user->editDevices);
}

bool WebConsole::userCanRemoveDevices(const UserRecord *user) const {
    return user != nullptr && (user->isAdmin || user->removeDevices);
}

void WebConsole::sendAuthCookie(AsyncWebServerResponse *response, const char *tokenHex, uint32_t maxAgeSec) {
    String cookie = String(AUTH_COOKIE_NAME) + "=" + tokenHex + "; Path=/; HttpOnly; SameSite=Lax";
    if (maxAgeSec > 0) {
        cookie += "; Max-Age=";
        cookie += String((unsigned long)maxAgeSec);
    }
    response->addHeader("Set-Cookie", cookie);
}

void WebConsole::fillUserFromJson(const char *json, UserRecord *user) {
    memset(user, 0, sizeof(*user));
    String userName;
    String themeText;
    extractJsonString(json, "userName", userName);
    strncpy(user->userName, userName.c_str(), sizeof(user->userName) - 1);
    extractJsonBool(json, "isAdmin", user->isAdmin);
    extractJsonBool(json, "editDevices", user->editDevices);
    extractJsonBool(json, "addDevices", user->addDevices);
    extractJsonBool(json, "removeDevices", user->removeDevices);
    extractJsonBool(json, "editUsers", user->editUsers);
    extractJsonBool(json, "isBlocked", user->isBlocked);
    extractJsonString(json, "theme", themeText);
    user->theme = UserStore::themeFromJsonId(themeText.c_str());
}

void WebConsole::sendUserWriteResult(AsyncWebServerRequest *request, UserWriteResult result) {
    if (result == UserWriteOk) {
        request->send(200, "text/plain", "Saved");
        return;
    }
    if (result == UserWriteForbidden) {
        request->send(403, "text/plain", "Forbidden");
        return;
    }
    if (result == UserWriteLastAdmin) {
        request->send(403, "text/plain", "At least one unlocked admin must remain");
        return;
    }
    if (result == UserWriteNotFound) {
        request->send(404, "text/plain", "User not found");
        return;
    }
    if (result == UserWriteDuplicate) {
        request->send(400, "text/plain", "User already exists");
        return;
    }
    if (result == UserWriteFull) {
        request->send(400, "text/plain", "User table full");
        return;
    }
    if (result == UserWriteNeedPassword) {
        request->send(400, "text/plain", "Need password");
        return;
    }
    request->send(400, "text/plain", "Bad user name");
}

void WebConsole::handleAuthLoginPost(AsyncWebServerRequest *request) {
    if (userStore == nullptr) {
        request->send(500, "text/plain", "Users unavailable");
        return;
    }
    String userName;
    String password;
    bool rememberMe = false;
    extractJsonString(requestBody.c_str(), "userName", userName);
    extractJsonString(requestBody.c_str(), "password", password);
    extractJsonBool(requestBody.c_str(), "rememberMe", rememberMe);
    const UserRecord *user = userStore->findByName(userName.c_str());
    if (user == nullptr || user->isBlocked || !userStore->passwordMatches(user, password.c_str())) {
        request->send(401, "text/plain", "Invalid user name or password");
        return;
    }
    char tokenHex[AUTH_SESSION_TOKEN_LEN * 2 + 1];
    uint32_t maxAgeSec = 0;
    if (!userStore->startSession(user, rememberMe, millis(), tokenHex, sizeof(tokenHex), &maxAgeSec)) {
        request->send(401, "text/plain", "Invalid user name or password");
        return;
    }
    String body = "{\"ok\":true}";
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", body);
    sendAuthCookie(response, tokenHex, maxAgeSec);
    request->send(response);
}

void WebConsole::handleAuthLogoutPost(AsyncWebServerRequest *request) {
    if (userStore != nullptr && request->hasHeader("Cookie")) {
        const char *cookie = request->header("Cookie").c_str();
        const char *found = strstr(cookie, AUTH_COOKIE_NAME "=");
        if (found != nullptr) {
            found += strlen(AUTH_COOKIE_NAME "=");
            char tokenHex[AUTH_SESSION_TOKEN_LEN * 2 + 1];
            size_t hexIndex = 0;
            while (hexIndex < AUTH_SESSION_TOKEN_LEN * 2 && found[hexIndex] != '\0' && found[hexIndex] != ';') {
                tokenHex[hexIndex] = found[hexIndex];
                hexIndex++;
            }
            tokenHex[hexIndex] = '\0';
            userStore->endSession(tokenHex);
        }
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Signed out");
    response->addHeader("Set-Cookie", String(AUTH_COOKIE_NAME) + "=; Path=/; HttpOnly; Max-Age=0");
    request->send(response);
}

void WebConsole::handleAuthMeGet(AsyncWebServerRequest *request) {
    const UserRecord *user = nullptr;
    if (!requireUser(request, &user)) {
        return;
    }
    String json;
    userStore->appendUserPublicJson(json, user);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleUsersGet(AsyncWebServerRequest *request) {
    if (!requireEditUsers(request, nullptr)) {
        return;
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", userStore->listPublicJson());
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleUsersPost(AsyncWebServerRequest *request) {
    const UserRecord *actor = nullptr;
    if (!requireEditUsers(request, &actor)) {
        return;
    }
    if (request->url().indexOf("update") >= 0) {
        handleUsersUpdatePost(request);
        return;
    }
    UserRecord source;
    fillUserFromJson(requestBody.c_str(), &source);
    String password;
    extractJsonString(requestBody.c_str(), "password", password);
    bool isEdit = false;
    const bool hasIsEdit = extractJsonBool(requestBody.c_str(), "isEdit", isEdit);
    if (isEdit || (!hasIsEdit && userStore->findByName(source.userName) != nullptr)) {
        sendUserWriteResult(request, userStore->updateUser(actor, source.userName, &source, password.c_str()));
        return;
    }
    sendUserWriteResult(request, userStore->createUser(actor, &source, password.c_str()));
}

void WebConsole::handleUsersUpdatePost(AsyncWebServerRequest *request) {
    const UserRecord *actor = nullptr;
    if (!requireEditUsers(request, &actor)) {
        return;
    }
    UserRecord source;
    fillUserFromJson(requestBody.c_str(), &source);
    String password;
    extractJsonString(requestBody.c_str(), "password", password);
    sendUserWriteResult(request, userStore->updateUser(actor, source.userName, &source, password.c_str()));
}

void WebConsole::handleUsersDelete(AsyncWebServerRequest *request) {
    const UserRecord *actor = nullptr;
    if (!requireEditUsers(request, &actor)) {
        return;
    }
    String userName;
    extractJsonString(requestBody.c_str(), "userName", userName);
    const UserWriteResult result = userStore->deleteUser(actor, userName.c_str());
    if (result == UserWriteOk) {
        request->send(200, "text/plain", "Deleted");
        return;
    }
    sendUserWriteResult(request, result);
}

void WebConsole::handleWifiGet(AsyncWebServerRequest *request) {
    if (!requireAdmin(request, nullptr)) {
        return;
    }
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
    if (!requireAdmin(request, nullptr)) {
        return;
    }
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
    if (!requireAdmin(request, nullptr)) {
        return;
    }
    String json = "{";
    appendMqttSettingsJson(json);
    json += "}";
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleMqttPost(AsyncWebServerRequest *request) {
    if (!requireAdmin(request, nullptr)) {
        return;
    }
    String errorText;
    if (!applyMqttJson(requestBody.c_str(), &errorText)) {
        request->send(400, "text/plain", errorText.length() > 0 ? errorText : "Need mqtt settings");
        return;
    }
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
    if (!requireAdmin(request, nullptr)) {
        return;
    }
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
    if (!requireAdmin(request, nullptr)) {
        return;
    }
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

void WebConsole::setDeviceTelemetryHandler(WebConsole::DeviceTelemetryFn handler) {
    appendDeviceTelemetry = handler;
}

void WebConsole::setDeviceCommandHandler(WebConsole::DeviceCommandFn handler) {
    applyDeviceCommand = handler;
}

void WebConsole::setDevicesRestoredHandler(WebConsole::DevicesRestoredFn handler) {
    applyDevicesRestored = handler;
}

void WebConsole::setUsersRestoredHandler(WebConsole::UsersRestoredFn handler) {
    applyUsersRestored = handler;
}

void WebConsole::setDevicesFileHandler(WebConsole::DevicesFileFn handler) {
    devicesFileJson = handler;
}

void WebConsole::setHardwareApplyHandler(WebConsole::HardwareApplyFn handler) {
    applyHardware = handler;
}

void WebConsole::handleHardwareGet(AsyncWebServerRequest *request) {
    if (!requireAdmin(request, nullptr)) {
        return;
    }
    const uint32_t speedHz = settingsManager->spiSpeedHz();
    LOGGER.info("Hardware GET spiSpeedHz=" + String((unsigned long)speedHz));
    String json = "{\"spiSpeedHz\":";
    json += String((unsigned long)speedHz);
    json += "}";
    request->send(200, "application/json", json);
}

void WebConsole::handleHardwarePost(AsyncWebServerRequest *request) {
    if (!requireAdmin(request, nullptr)) {
        return;
    }
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

void WebConsole::appendMqttSettingsJson(String &json) {
    GlobalSettings *settings = settingsManager->getSettings();
    json += "\"serverType\":\"";
    json += mqttServerTypeName(settings->mqtt.serverType);
    json += "\",\"server\":\"";
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
}

bool WebConsole::applyMqttJson(const char *json, String *errorText) {
    String server;
    String username;
    String password;
    String clientId;
    String baseTopic;
    String serverTypeText;
    int port = DEFAULT_MQTT_PORT;
    int reconnectIntervalMs = DEFAULT_MQTT_RECONNECT_MS;
    int clientTimeoutMs = DEFAULT_MQTT_CLIENT_TIMEOUT_MS;
    bool enabledLegacy = false;
    MqttServerType serverType = MqttServerTypeDisable;
    const bool haveServerType = extractJsonString(json, "serverType", serverTypeText);
    const bool haveEnabled = extractJsonBool(json, "enabled", enabledLegacy);
    const bool haveServer = extractJsonString(json, "server", server);
    const bool haveClientId = extractJsonString(json, "clientId", clientId);
    const bool haveBaseTopic = extractJsonString(json, "baseTopic", baseTopic);
    extractJsonString(json, "username", username);
    extractJsonString(json, "password", password);
    extractJsonInt(json, "port", port);
    extractJsonInt(json, "reconnectIntervalMs", reconnectIntervalMs);
    extractJsonInt(json, "clientTimeoutMs", clientTimeoutMs);
    if (haveServerType) {
        if (!parseMqttServerType(serverTypeText.c_str(), serverType)) {
            if (errorText != nullptr) {
                *errorText = "serverType must be disable, remote, or local";
            }
            return false;
        }
    } else if (haveEnabled) {
        serverType = enabledLegacy ? MqttServerTypeRemote : MqttServerTypeDisable;
    } else {
        if (errorText != nullptr) {
            *errorText = "Need mqtt serverType";
        }
        return false;
    }
    if (!haveClientId || !haveBaseTopic) {
        if (errorText != nullptr) {
            *errorText = "Need mqtt clientId, baseTopic";
        }
        return false;
    }
    if (serverType != MqttServerTypeLocal && !haveServer) {
        if (errorText != nullptr) {
            *errorText = "Need mqtt server, clientId, baseTopic";
        }
        return false;
    }
    if (!haveServer) {
        server = String(settingsManager->getSettings()->mqtt.server);
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
        if (errorText != nullptr) {
            *errorText = "port must be 1-65535";
        }
        return false;
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
    settings->mqtt.serverType = serverType;
    strncpy(settings->mqtt.username, username.c_str(), sizeof(settings->mqtt.username) - 1);
    settings->mqtt.username[sizeof(settings->mqtt.username) - 1] = '\0';
    strncpy(settings->mqtt.password, password.c_str(), sizeof(settings->mqtt.password) - 1);
    settings->mqtt.password[sizeof(settings->mqtt.password) - 1] = '\0';
    strncpy(settings->mqtt.clientId, clientId.c_str(), sizeof(settings->mqtt.clientId) - 1);
    settings->mqtt.clientId[sizeof(settings->mqtt.clientId) - 1] = '\0';
    strncpy(settings->mqtt.baseTopic, baseTopic.c_str(), sizeof(settings->mqtt.baseTopic) - 1);
    settings->mqtt.baseTopic[sizeof(settings->mqtt.baseTopic) - 1] = '\0';
    return true;
}

bool WebConsole::applyZigbeeJson(const char *json, String *errorText) {
    int channel = DEFAULT_ZIGBEE_CHANNEL;
    int permitJoinOnBootSec = DEFAULT_PERMIT_JOIN_SEC;
    if (!extractJsonInt(json, "channel", channel)
        || !extractJsonInt(json, "permitJoinOnBootSec", permitJoinOnBootSec)) {
        if (errorText != nullptr) {
            *errorText = "Need zigbee channel, permitJoinOnBootSec";
        }
        return false;
    }
    if (channel < 11 || channel > 26) {
        if (errorText != nullptr) {
            *errorText = "channel must be 11-26";
        }
        return false;
    }
    if (permitJoinOnBootSec < 0 || permitJoinOnBootSec > 254) {
        if (errorText != nullptr) {
            *errorText = "permitJoinOnBootSec must be 0-254";
        }
        return false;
    }
    GlobalSettings *settings = settingsManager->getSettings();
    settings->zigbee.channel = (uint8_t)channel;
    settings->zigbee.permitJoinOnBootSec = (uint8_t)permitJoinOnBootSec;
    return true;
}

bool WebConsole::applyHardwareJson(const char *json, String *errorText) {
    int spiSpeedHz = DEFAULT_SPI_SPEED_HZ;
    if (!extractJsonInt(json, "spiSpeedHz", spiSpeedHz)) {
        if (errorText != nullptr) {
            *errorText = "Need hardware spiSpeedHz";
        }
        return false;
    }
    const uint32_t clamped = SettingsManager::clampSpiSpeedHz((uint32_t)spiSpeedHz);
    if (clamped != (uint32_t)spiSpeedHz) {
        if (errorText != nullptr) {
            *errorText = "spiSpeedHz must be 100000-40000000";
        }
        return false;
    }
    settingsManager->setSpiSpeedHz(clamped);
    if (applyHardware != nullptr) {
        applyHardware(clamped);
    }
    return true;
}

void WebConsole::handleSettingsExportGet(AsyncWebServerRequest *request) {
    const UserRecord *user = nullptr;
    if (!requireAdmin(request, &user)) {
        return;
    }
    GlobalSettings *settings = settingsManager->getSettings();
    String json = "{";
    json += "\"version\":\"";
    json += FIRMWARE_VERSION;
    json += "\",\"mqtt\":{";
    appendMqttSettingsJson(json);
    json += "},\"zigbee\":{";
    json += "\"channel\":";
    json += String(settings->zigbee.channel);
    json += ",\"permitJoinOnBootSec\":";
    json += String(settings->zigbee.permitJoinOnBootSec);
    json += "},\"hardware\":{\"spiSpeedHz\":";
    json += String((unsigned long)settingsManager->spiSpeedHz());
    json += "},\"ui\":{\"theme\":\"";
    json += UserStore::themeJsonId(user->theme);
    json += "\"},\"devices\":";
    json += settingsManager->deviceMap()->listStoreJson();
    json += ",\"users\":";
    json += userStore->listExportJson();
    json += "}";
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleSettingsRestorePost(AsyncWebServerRequest *request) {
    const UserRecord *user = nullptr;
    if (!requireAdmin(request, &user)) {
        return;
    }
    String errorText;
    String mqttJson;
    String zigbeeJson;
    String hardwareJson;
    String devicesJson;
    String uiJson;
    const bool haveMqtt = extractJsonKeyedSlice(requestBody.c_str(), "mqtt", '{', mqttJson);
    const bool haveZigbee = extractJsonKeyedSlice(requestBody.c_str(), "zigbee", '{', zigbeeJson);
    const bool haveHardware = extractJsonKeyedSlice(requestBody.c_str(), "hardware", '{', hardwareJson);
    const bool haveDevices = extractJsonKeyedSlice(requestBody.c_str(), "devices", '[', devicesJson);
    String usersJson;
    const bool haveUi = extractJsonKeyedSlice(requestBody.c_str(), "ui", '{', uiJson);
    const bool haveUsers = extractJsonKeyedSlice(requestBody.c_str(), "users", '[', usersJson);
    if (haveMqtt && !applyMqttJson(mqttJson.c_str(), &errorText)) {
        request->send(400, "text/plain", errorText);
        return;
    }
    if (haveZigbee && !applyZigbeeJson(zigbeeJson.c_str(), &errorText)) {
        request->send(400, "text/plain", errorText);
        return;
    }
    if (haveHardware && !applyHardwareJson(hardwareJson.c_str(), &errorText)) {
        request->send(400, "text/plain", errorText);
        return;
    }
    char actorName[USER_NAME_MAX];
    strncpy(actorName, user->userName, sizeof(actorName) - 1);
    actorName[sizeof(actorName) - 1] = '\0';
    char removedUserNames[USER_STORE_MAX][USER_NAME_MAX];
    int removedUserCount = 0;
    if (haveUsers
        && !userStore->replaceFromExportJson(
            usersJson,
            removedUserNames,
            &removedUserCount,
            USER_STORE_MAX
        )) {
        request->send(400, "text/plain", "Need a valid users list with at least one unlocked admin");
        return;
    }
    if (haveUsers && applyUsersRestored != nullptr
        && !applyUsersRestored(removedUserNames, removedUserCount)) {
        request->send(503, "text/plain", "Slave is not ready to store the user list");
        return;
    }
    if (haveUi && !applyThemeJson(uiJson.c_str(), userStore->findByName(actorName), &errorText)) {
        request->send(400, "text/plain", errorText);
        return;
    }
    if (haveDevices) {
        DeviceTopicMap *deviceMap = settingsManager->deviceMap();
        static uint8_t previousIeees[DEVICE_MAP_SLOTS][8];
        static uint8_t removedIeees[DEVICE_MAP_SLOTS][8];
        int previousCount = 0;
        for (int i = 0; i < DEVICE_MAP_SLOTS; i++) {
            DeviceTopicEntry *entry = deviceMap->slotAt(i);
            if (entry == nullptr || !entry->used) {
                continue;
            }
            memcpy(previousIeees[previousCount], entry->ieee, 8);
            previousCount++;
        }
        deviceMap->replaceFromJson(devicesJson);
        int removedCount = 0;
        for (int i = 0; i < previousCount; i++) {
            if (deviceMap->findByIeee(previousIeees[i]) == nullptr) {
                memcpy(removedIeees[removedCount], previousIeees[i], 8);
                removedCount++;
            }
        }
        if (applyDevicesRestored != nullptr && !applyDevicesRestored(removedIeees, removedCount)) {
            request->send(503, "text/plain", "Slave is not ready to store the device list");
            return;
        }
        if (applyDevicesRestored == nullptr) {
            settingsManager->saveDevicesJson();
        }
    }
    if (haveMqtt || haveZigbee) {
        settingsManager->saveMain(true);
        request->send(200, "text/plain", "Restored. Device will restart to apply settings.");
        return;
    }
    if (haveHardware || haveUi) {
        settingsManager->saveMain(false);
    }
    request->send(200, "text/plain", "Restored");
}

bool WebConsole::applyThemeJson(const char *json, UserRecord *user, String *errorText) {
    String themeText;
    if (user == nullptr) {
        if (errorText != nullptr) {
            *errorText = "Need signed-in user";
        }
        return false;
    }
    if (!extractJsonString(json, "theme", themeText)) {
        if (errorText != nullptr) {
            *errorText = "Need ui theme";
        }
        return false;
    }
    themeText.toLowerCase();
    if (themeText != "light" && themeText != "dark") {
        if (errorText != nullptr) {
            *errorText = "theme must be light or dark";
        }
        return false;
    }
    user->theme = UserStore::themeFromJsonId(themeText.c_str());
    userStore->noteRecordChanged(user);
    return true;
}

void WebConsole::handleThemeGet(AsyncWebServerRequest *request) {
    const UserRecord *user = nullptr;
    if (!requireUser(request, &user)) {
        return;
    }
    String json = "{\"theme\":\"";
    json += UserStore::themeJsonId(user->theme);
    json += "\"}";
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleThemePost(AsyncWebServerRequest *request) {
    const UserRecord *user = nullptr;
    if (!requireUser(request, &user)) {
        return;
    }
    String errorText;
    if (!applyThemeJson(requestBody.c_str(), userStore->findByName(user->userName), &errorText)) {
        request->send(400, "text/plain", errorText);
        return;
    }
    request->send(200, "text/plain", "Saved");
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
    if (!requireUser(request, nullptr)) {
        return;
    }
    DeviceTopicMap *deviceMap = settingsManager->deviceMap();
    String json = deviceMap->listJson(isDeviceOnline, lastDeviceRssi, appendDeviceTelemetry);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleDevicesPost(AsyncWebServerRequest *request) {
    const UserRecord *user = nullptr;
    if (!requireUser(request, &user)) {
        return;
    }
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
    DeviceTopicEntry *existing = deviceMap->findByIeee(ieee);
    if (existing != nullptr && existing->used) {
        if (!userCanEditDevices(user)) {
            request->send(403, "text/plain", "Forbidden");
            return;
        }
    } else if (!userCanAddDevices(user)) {
        request->send(403, "text/plain", "Forbidden");
        return;
    }
    const uint8_t storedType =
        existing != nullptr && existing->used ? existing->zigbeeType : ZigbeeDeviceTypeUnknown;
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
    if (storedType != ZigbeeDeviceTypeUnknown) {
        entry->zigbeeType = storedType;
    } else if (foundDevices != nullptr) {
        entry->zigbeeType = foundDevices->zigbeeTypeForIeee(ieee);
    }
    if (entry->zigbeeType == ZigbeeDeviceTypeUnknown) {
        String typeText;
        if (extractJsonString(requestBody.c_str(), "type", typeText)) {
            entry->zigbeeType = zigbeeDeviceTypeFromJsonId(typeText.c_str());
        }
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
    const UserRecord *user = nullptr;
    if (!requireUser(request, &user)) {
        return;
    }
    if (!userCanRemoveDevices(user)) {
        request->send(403, "text/plain", "Forbidden");
        return;
    }
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

void WebConsole::handleDevicesCommandPost(AsyncWebServerRequest *request) {
    if (!requireUser(request, nullptr)) {
        return;
    }
    if (applyDeviceCommand == nullptr) {
        request->send(503, "text/plain", "Command path is not ready");
        return;
    }
    String ieeeText;
    String payload;
    if (!extractJsonString(requestBody.c_str(), "ieee", ieeeText)) {
        request->send(400, "text/plain", "Need ieee");
        return;
    }
    extractJsonString(requestBody.c_str(), "payload", payload);
    int channel = 0;
    if (!extractJsonInt(requestBody.c_str(), "channel", channel)) {
        channel = 0;
    }
    const int status = applyDeviceCommand(ieeeText.c_str(), payload.c_str(), channel);
    if (status == 404) {
        request->send(404, "text/plain", "Device not found");
        return;
    }
    if (status == 400) {
        request->send(400, "text/plain", "Need payload");
        return;
    }
    if (status != 200) {
        request->send(status, "text/plain", "Command failed");
        return;
    }
    request->send(200, "text/plain", "Sent");
}

void WebConsole::handleDevicesFoundGet(AsyncWebServerRequest *request) {
    const UserRecord *user = nullptr;
    if (!requireUser(request, &user)) {
        return;
    }
    if (!userCanAddDevices(user)) {
        request->send(403, "text/plain", "Forbidden");
        return;
    }
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
    const UserRecord *user = nullptr;
    if (!requireUser(request, &user)) {
        return;
    }
    if (!userCanAddDevices(user)) {
        request->send(403, "text/plain", "Forbidden");
        return;
    }
    if (startSearch == nullptr || !startSearch()) {
        LOGGER.warning("Device search did not start");
        request->send(500, "text/plain", "Slave is not ready for pairing");
        return;
    }
    LOGGER.info("Device search started");
    request->send(200, "text/plain", "Search started");
}

void WebConsole::handleDevicesStoreGet(AsyncWebServerRequest *request) {
    if (!requireAdmin(request, nullptr)) {
        return;
    }
    String json = devicesFileJson != nullptr ? devicesFileJson() : String("[]");
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleGatewayStatusGet(AsyncWebServerRequest *request) {
    if (!requireUser(request, nullptr)) {
        return;
    }
    String json = gatewayStatusJson != nullptr ? gatewayStatusJson() : String("{}");
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleDevicesSearchStopPost(AsyncWebServerRequest *request) {
    const UserRecord *user = nullptr;
    if (!requireUser(request, &user)) {
        return;
    }
    if (!userCanAddDevices(user)) {
        request->send(403, "text/plain", "Forbidden");
        return;
    }
    if (stopSearch != nullptr) {
        stopSearch();
    }
    request->send(200, "text/plain", "Search stopped");
}

void WebConsole::handleLogGet(AsyncWebServerRequest *request) {
    if (!requireUser(request, nullptr)) {
        return;
    }
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
    if (!requireUser(request, nullptr)) {
        return;
    }
    String json = "{\"version\":\"";
    json += FIRMWARE_VERSION;
    json += "\"}";
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void WebConsole::handleFirmwareUpdateStatusGet(AsyncWebServerRequest *request) {
    if (!requireAdmin(request, nullptr)) {
        return;
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", FIRMWARE_OTA.statusJson());
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
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
    (void)filename;
    if (index == 0) {
        const UserRecord *user = authenticatedUser(request);
        if (user == nullptr || !user->isAdmin) {
            otaFailed = true;
            return;
        }
        otaStarted = true;
        otaFailed = false;
        otaCommand = command;
        LOGGER.info(command == U_FLASH ? "HTTP OTA firmware start" : "HTTP OTA filesystem start");
        if (command == U_FLASH) {
            const size_t contentLength = request != nullptr ? request->contentLength() : 0;
            if (!FIRMWARE_OTA.beginStaging(contentLength)) {
                otaFailed = true;
                return;
            }
        } else if (!Update.begin(UPDATE_SIZE_UNKNOWN, command)) {
            LOGGER.error("HTTP OTA begin failed");
            otaFailed = true;
            return;
        }
    }
    if (otaFailed) {
        return;
    }
    if (otaCommand == U_FLASH) {
        if (!FIRMWARE_OTA.writeStaging(data, len)) {
            otaFailed = true;
        }
        if (final && !otaFailed && !FIRMWARE_OTA.finishStaging()) {
            otaFailed = true;
        }
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
    if (!requireAdmin(request, nullptr)) {
        return;
    }
    if (otaCommand == U_FLASH) {
        if (!otaStarted || otaFailed || FIRMWARE_OTA.phase() != FirmwareOta::Phase::Slave) {
            String errorMessage = "Upload failed";
            if (FIRMWARE_OTA.phase() == FirmwareOta::Phase::Failed) {
                errorMessage = FIRMWARE_OTA.statusJson();
            }
            LOGGER.error("HTTP firmware staging failed");
            FIRMWARE_OTA.abortStaging();
            otaStarted = false;
            otaFailed = false;
            request->send(500, "text/plain", errorMessage);
            return;
        }
        otaStarted = false;
        request->send(200, "text/plain", "Firmware stored. Updating slave, then host.");
        return;
    }
    if (!otaStarted || otaFailed || Update.hasError()) {
        String errorMessage = Update.hasError() ? String(Update.errorString()) : String("Upload failed");
        LOGGER.error("HTTP OTA failed: " + errorMessage);
        Update.abort();
        otaStarted = false;
        otaFailed = false;
        request->send(500, "text/plain", errorMessage);
        return;
    }
    LOGGER.info("HTTP OTA filesystem finished");
    otaStarted = false;
    request->send(200, "text/plain", "Filesystem written. Device will restart.");
    settingsManager->requestRestart();
}
