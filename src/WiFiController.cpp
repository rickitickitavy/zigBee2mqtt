#include "WiFiController.h"
#include "Logger.h"

#include <WiFi.h>

WiFiController::WiFiController(SettingsManager *settingsManager, bool forceAp)
    : settingsManager(settingsManager), forceAp(forceAp) {
    GlobalSettings *settings = settingsManager->getSettings();
    const String deviceName = String(settings->network.hostName);

    WiFi.persistent(false);
    WiFi.setHostname(deviceName.c_str());

    const bool ssidEmpty = settings->network.ssid[0] == '\0';
    const bool useAp = forceAp || !settings->network.wifiEnabled || ssidEmpty;
    if (useAp) {
        startAp(deviceName);
        return;
    }

    staEnabledAtBoot = true;
    startSta(deviceName);
}

bool WiFiController::isApMode() const {
    return apActive;
}

bool WiFiController::isStaConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

void WiFiController::startAp(const String &deviceName) {
    LOGGER.info("Starting WiFi AP mode...");
    WiFi.persistent(false);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(300);

    WiFi.mode(WIFI_AP);
    delay(200);

    String ssid = deviceName.length() > 0 ? (deviceName + "-AP") : String("z2m-gateway-AP");
    if (ssid.length() > 32) {
        ssid = ssid.substring(0, 32);
    }

    const IPAddress apIp(192, 168, 0, 1);
    const IPAddress gateway(192, 168, 0, 1);
    const IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIp, gateway, subnet);
    const bool ok = WiFi.softAP(ssid.c_str(), "00000000", 6, 0, 4);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);

    apActive = ok;
    apStartedMs = millis();
    LOGGER.info(ok ? ("AP " + ssid + " IP " + WiFi.softAPIP().toString()) : "softAP FAILED");
}

void WiFiController::stopAp() {
    if (!apActive) {
        return;
    }
    LOGGER.warning("Stopping WiFi AP...");
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);
    apActive = false;
}

void WiFiController::onStaConnected() {
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_15dBm);
    LOGGER.info(
        "STA ready. IP " + WiFi.localIP().toString() + ", RSSI " + String(WiFi.RSSI()) + " dBm"
    );
}

void WiFiController::reconnectSta() {
    GlobalSettings *settings = settingsManager->getSettings();
    LOGGER.warning("STA reconnecting to '" + String(settings->network.ssid) + "'...");
    WiFi.setSleep(false);
    WiFi.disconnect(false, false);
    delay(100);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.begin(settings->network.ssid, settings->network.password);
    WiFi.setTxPower(WIFI_POWER_15dBm);
}

void WiFiController::startSta(const String &deviceName) {
    GlobalSettings *settings = settingsManager->getSettings();
    LOGGER.info("Starting STA to '" + String(settings->network.ssid) + "' as " + deviceName);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setHostname(deviceName.c_str());
    WiFi.begin(settings->network.ssid, settings->network.password);
    WiFi.setTxPower(WIFI_POWER_15dBm);
}

void WiFiController::update() {
    if (apActive) {
        if ((millis() - apStartedMs) >= kApTimeoutMs) {
            stopAp();
            GlobalSettings *settings = settingsManager->getSettings();
            if (settings->network.wifiEnabled && settings->network.ssid[0] != '\0') {
                staEnabledAtBoot = true;
                startSta(String(settings->network.hostName));
            }
        }
        return;
    }

    if (!staEnabledAtBoot) {
        return;
    }

    const bool connected = isStaConnected();
    if (connected && !staWasConnected) {
        onStaConnected();
    }
    staWasConnected = connected;

    if (!connected && (millis() - lastStaReconnectMs) >= kStaReconnectMs) {
        lastStaReconnectMs = millis();
        reconnectSta();
    }
}
