#include "WiFiController.h"
#include "Logger.h"
#include "Defines.h"

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_coexist.h>
#include <esp_system.h>

static bool parseApIp(const char *text, IPAddress &address) {
    if (text == nullptr || text[0] == '\0') {
        return address.fromString(WIFI_DEFAULT_AP_IP);
    }
    if (!address.fromString(text)) {
        return address.fromString(WIFI_DEFAULT_AP_IP);
    }
    return true;
}

WiFiController::WiFiController(SettingsManager *settingsManager, bool forceAp)
    : settingsManager(settingsManager) {
    GlobalSettings *settings = settingsManager->getSettings();
    WiFi.persistent(false);
    WiFi.mode(WIFI_OFF);
    const bool coldStart = esp_reset_reason() == ESP_RST_POWERON
        || esp_reset_reason() == ESP_RST_BROWNOUT;
    delay(coldStart ? 150 : 800);
    WiFi.setHostname(settings->wifi.deviceName);

    if (forceAp || settings->wifi.mode == WifiSettingsModeAp || settings->wifi.bssid[0] == '\0') {
        startAp(false);
        return;
    }

    startSta();
    const unsigned long joinStartedMs = millis();
    while ((millis() - joinStartedMs) < kStaJoinTimeoutMs) {
        if (WiFi.status() == WL_CONNECTED) {
            staEnabledAtBoot = true;
            staWasConnected = true;
            onStaConnected();
            return;
        }
        delay(50);
    }

    LOGGER.warning("STA join failed; starting recovery AP");
    startAp(true);
}

bool WiFiController::isApMode() const {
    return apActive;
}

bool WiFiController::isStaConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool WiFiController::hasUsableInterface() const {
    if (isStaConnected()) {
        return true;
    }
    return apActive && isApRadioUp();
}

void WiFiController::setInterfaceReadyHandler(InterfaceReadyFn handler) {
    interfaceReadyHandler = handler;
}

String WiFiController::apNetworkName() const {
    GlobalSettings *settings = settingsManager->getSettings();
    String name = String(settings->wifi.bssid);
    if (name.length() == 0) {
        name = WIFI_DEFAULT_BSSID;
    }
    if (name.length() > 32) {
        name = name.substring(0, 32);
    }
    return name;
}

String WiFiController::apPassword() const {
    GlobalSettings *settings = settingsManager->getSettings();
    if (settings->wifi.password[0] == '\0') {
        return String(WIFI_DEFAULT_PASSWORD);
    }
    return String(settings->wifi.password);
}

bool WiFiController::isApRadioUp() const {
    const wifi_mode_t mode = WiFi.getMode();
    return mode == WIFI_AP || mode == WIFI_AP_STA;
}

void WiFiController::stopStationBeforeAp() {
    const wifi_mode_t currentMode = WiFi.getMode();
    if (currentMode != WIFI_STA && currentMode != WIFI_AP_STA) {
        return;
    }
    LOGGER.info("Stopping STA before AP");
    WiFi.disconnect(false, false, 2000);
    delay(200);
    WiFi.mode(WIFI_OFF);
    delay(400);
}

void WiFiController::startAp(bool useRecoveryIdentity) {
    recoveryApIdentity = useRecoveryIdentity;
    LOGGER.info(useRecoveryIdentity ? "Starting recovery WiFi AP..." : "Starting WiFi AP mode...");
    WiFi.persistent(false);
    WiFi.setSleep(false);
    stopStationBeforeAp();

    WiFi.mode(WIFI_AP);
    delay(200);
    esp_wifi_set_ps(WIFI_PS_NONE);
    const uint8_t apProtocol = WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N;
    if (esp_wifi_set_protocol(WIFI_IF_AP, apProtocol) != ESP_OK) {
        LOGGER.error("AP protocol 11b/g/n failed");
    }

    const String ssid = useRecoveryIdentity ? String(WIFI_DEFAULT_BSSID) : apNetworkName();
    const String password = useRecoveryIdentity ? String(WIFI_DEFAULT_PASSWORD) : apPassword();
    LOGGER.info(
        String("AP passphrase length ") + String(password.length())
            + (password == WIFI_DEFAULT_PASSWORD ? ", default" : ", custom")
    );

    IPAddress apIp;
    parseApIp(settingsManager->getSettings()->wifi.apIp, apIp);
    const IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIp, apIp, subnet);

    const bool ok = WiFi.softAP(ssid.c_str(), password.c_str(), 6, 0, 4);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);

    apActive = ok;
    staEnabledAtBoot = false;
    lastApRestartMs = millis();
    if (ok) {
        LOGGER.info(
            "AP " + ssid + " IP " + WiFi.softAPIP().toString() + " MAC " + WiFi.softAPmacAddress()
        );
        LOGGER.info("STA MAC " + WiFi.macAddress());
    } else {
        LOGGER.error("softAP FAILED");
    }
}

void WiFiController::applyStaRadio() {
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.setTxPower(WIFI_POWER_21dBm);
    esp_wifi_set_max_tx_power(84);
    esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
}

void WiFiController::enableIeee154Coex() {
#if CONFIG_ESP_COEX_SW_COEXIST_ENABLE && CONFIG_SOC_IEEE802154_SUPPORTED
    if (esp_coex_wifi_i154_enable() != ESP_OK) {
        LOGGER.error("Wi-Fi/802.15.4 coexist enable failed");
        return;
    }
    LOGGER.info("Wi-Fi/802.15.4 coexist enabled");
#else
    LOGGER.warning("Wi-Fi/802.15.4 coexist API not in this build");
#endif
}

int WiFiController::staWifiChannel() const {
    return WiFi.channel();
}

uint8_t WiFiController::zigbeeChannelOverlappingSta() const {
    const int wifiChannel = WiFi.channel();
    if (wifiChannel < 1 || wifiChannel > 13) {
        return 0;
    }
    int zigbeeChannel = wifiChannel + 10;
    if (zigbeeChannel < 11) {
        zigbeeChannel = 11;
    }
    if (zigbeeChannel > 26) {
        zigbeeChannel = 26;
    }
    return (uint8_t)zigbeeChannel;
}

void WiFiController::onStaConnected() {
    applyStaRadio();
    LOGGER.info(
        "STA ready. IP " + WiFi.localIP().toString() + ", RSSI " + String(WiFi.RSSI())
            + " dBm MAC " + WiFi.macAddress()
    );
}

void WiFiController::beginStaJoin() {
    GlobalSettings *settings = settingsManager->getSettings();
    String targetSsid = String(settings->wifi.bssid);
    targetSsid.trim();

    wifi_country_t country;
    memset(&country, 0, sizeof(country));
    country.cc[0] = '0';
    country.cc[1] = '1';
    country.schan = 1;
    country.nchan = 13;
    country.max_tx_power = 20;
    country.policy = WIFI_COUNTRY_POLICY_MANUAL;
    esp_wifi_set_country(&country);
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

    LOGGER.info("STA scanning 2.4 GHz for '" + targetSsid + "'");
    const int16_t foundCount = WiFi.scanNetworks(false, true, false, 300);
    int matchIndex = -1;
    int32_t bestRssi = -127;
    if (foundCount <= 0) {
        LOGGER.warning("STA scan found no networks");
    }
    for (int16_t i = 0; i < foundCount; i++) {
        const String seenSsid = WiFi.SSID(i);
        const int32_t seenRssi = WiFi.RSSI(i);
        LOGGER.info(
            "STA saw " + seenSsid + " ch " + String(WiFi.channel(i)) + " RSSI " + String(seenRssi)
        );
        if (seenSsid == targetSsid && seenRssi > bestRssi) {
            bestRssi = seenRssi;
            matchIndex = i;
        }
    }

    if (matchIndex >= 0) {
        LOGGER.info(
            "STA joining '" + targetSsid + "' on ch " + String(WiFi.channel(matchIndex))
                + " RSSI " + String(bestRssi)
        );
        WiFi.begin(
            targetSsid.c_str(),
            settings->wifi.password,
            WiFi.channel(matchIndex),
            WiFi.BSSID(matchIndex)
        );
    } else {
        LOGGER.warning("STA target not in 2.4 GHz scan (5 GHz-only or hidden); trying begin");
        WiFi.begin(targetSsid.c_str(), settings->wifi.password);
    }
    WiFi.scanDelete();
    applyStaRadio();
}

void WiFiController::reconnectSta() {
    GlobalSettings *settings = settingsManager->getSettings();
    LOGGER.warning("STA reconnecting to '" + String(settings->wifi.bssid) + "'...");
    WiFi.setSleep(false);
    WiFi.disconnect(false, false);
    delay(100);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    beginStaJoin();
}

void WiFiController::startSta() {
    GlobalSettings *settings = settingsManager->getSettings();
    LOGGER.info(
        "Starting STA to '" + String(settings->wifi.bssid) + "' as " + String(settings->wifi.deviceName)
    );
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setHostname(settings->wifi.deviceName);
    delay(200);
    beginStaJoin();
    LOGGER.info("STA MAC " + WiFi.macAddress());
}

void WiFiController::update() {
    if (apActive) {
        if (!isApRadioUp() && (millis() - lastApRestartMs) >= kApRestartMs) {
            LOGGER.error("AP dropped; restarting");
            startAp(recoveryApIdentity);
        }
        return;
    }

    if (!staEnabledAtBoot) {
        return;
    }

    const bool connected = isStaConnected();
    if (!connected && staWasConnected) {
        LOGGER.warning("STA disconnected; reconnecting");
        staNeedsWebRebind = true;
        lastStaReconnectMs = millis();
        reconnectSta();
    }
    if (connected && !staWasConnected) {
        onStaConnected();
        if (staNeedsWebRebind && interfaceReadyHandler != nullptr) {
            interfaceReadyHandler();
        }
        staNeedsWebRebind = false;
    }
    if (connected && (millis() - lastStaRadioRefreshMs) >= kStaRadioRefreshMs) {
        lastStaRadioRefreshMs = millis();
        applyStaRadio();
    }
    staWasConnected = connected;

    if (!connected && (millis() - lastStaReconnectMs) >= kStaReconnectMs) {
        lastStaReconnectMs = millis();
        reconnectSta();
    }
}
