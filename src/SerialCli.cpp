#include "SerialCli.h"
#include "Logger.h"
#include "Defines.h"

#include <string.h>

SerialCli::SerialCli(SettingsManager *settingsManager)
    : settingsManager(settingsManager) {}

void SerialCli::dispatch() {
    while (Serial.available() > 0) {
        char incoming = (char)Serial.read();
        if (incoming == '\r') {
            continue;
        }
        if (incoming == '\n') {
            handleLine(lineBuffer);
            lineBuffer = "";
            continue;
        }
        if (lineBuffer.length() < 240) {
            lineBuffer += incoming;
        }
    }
}

void SerialCli::printHelp() {
    LOGGER.info("USB CLI (web console may be down on STA)");
    LOGGER.info("  help");
    LOGGER.info("  settings");
    LOGGER.info("  wifi <ssid> <password>");
    LOGGER.info("  mode AP|STA");
    LOGGER.info("  mqtt <server> [port]");
    LOGGER.info("  mqttuser <username> <password>");
    LOGGER.info("  mqttid <clientId>");
    LOGGER.info("  mqttbase <baseTopic>");
    LOGGER.info("  mqtttype disable|remote|local");
    LOGGER.info("  mqtten on|off");
    LOGGER.info("  save");
    LOGGER.info("  log");
}

void SerialCli::handleLine(const String &line) {
    String trimmed = line;
    trimmed.trim();
    if (trimmed.length() == 0) {
        return;
    }
    if (trimmed == "help") {
        printHelp();
        return;
    }
    if (trimmed == "settings") {
        settingsManager->logSettings();
        return;
    }
    if (trimmed == "save") {
        settingsManager->saveMain(true);
        return;
    }
    if (trimmed == "log") {
        LOGGER.writeRing(Serial);
        return;
    }

    GlobalSettings *settings = settingsManager->getSettings();

    if (trimmed.startsWith("mode ")) {
        String modeText = trimmed.substring(5);
        modeText.trim();
        modeText.toUpperCase();
        if (modeText == "STA") {
            settings->wifi.mode = WifiSettingsModeSta;
        } else if (modeText == "AP") {
            settings->wifi.mode = WifiSettingsModeAp;
        } else {
            LOGGER.error("mode AP|STA");
            return;
        }
        LOGGER.info("MODE " + modeText + " (save to apply)");
        return;
    }

    if (trimmed.startsWith("wifi ")) {
        String rest = trimmed.substring(5);
        rest.trim();
        const int split = rest.indexOf(' ');
        if (split < 1) {
            LOGGER.error("wifi <ssid> <password>");
            return;
        }
        String ssid = rest.substring(0, split);
        String password = rest.substring(split + 1);
        ssid.trim();
        password.trim();
        strncpy(settings->wifi.bssid, ssid.c_str(), sizeof(settings->wifi.bssid) - 1);
        settings->wifi.bssid[sizeof(settings->wifi.bssid) - 1] = '\0';
        strncpy(settings->wifi.password, password.c_str(), sizeof(settings->wifi.password) - 1);
        settings->wifi.password[sizeof(settings->wifi.password) - 1] = '\0';
        settings->wifi.mode = WifiSettingsModeSta;
        LOGGER.info("Wi-Fi STA '" + ssid + "' (save to apply)");
        return;
    }

    if (trimmed.startsWith("mqtt ")) {
        String rest = trimmed.substring(5);
        rest.trim();
        const int split = rest.indexOf(' ');
        String server = split < 0 ? rest : rest.substring(0, split);
        server.trim();
        if (server.length() == 0) {
            LOGGER.error("mqtt <server> [port]");
            return;
        }
        strncpy(settings->mqtt.server, server.c_str(), sizeof(settings->mqtt.server) - 1);
        settings->mqtt.server[sizeof(settings->mqtt.server) - 1] = '\0';
        if (split >= 0) {
            int port = rest.substring(split + 1).toInt();
            if (port < 1 || port > 65535) {
                LOGGER.error("port must be 1-65535");
                return;
            }
            settings->mqtt.port = port;
        }
        LOGGER.info(
            "MQTT " + String(settings->mqtt.server) + ":" + String(settings->mqtt.port) + " (save to apply)"
        );
        return;
    }

    if (trimmed.startsWith("mqttuser ")) {
        String rest = trimmed.substring(9);
        rest.trim();
        const int split = rest.indexOf(' ');
        if (split < 0) {
            strncpy(settings->mqtt.username, rest.c_str(), sizeof(settings->mqtt.username) - 1);
            settings->mqtt.username[sizeof(settings->mqtt.username) - 1] = '\0';
            settings->mqtt.password[0] = '\0';
        } else {
            String username = rest.substring(0, split);
            strncpy(settings->mqtt.username, username.c_str(), sizeof(settings->mqtt.username) - 1);
            settings->mqtt.username[sizeof(settings->mqtt.username) - 1] = '\0';
            strncpy(
                settings->mqtt.password,
                rest.substring(split + 1).c_str(),
                sizeof(settings->mqtt.password) - 1
            );
            settings->mqtt.password[sizeof(settings->mqtt.password) - 1] = '\0';
        }
        LOGGER.info("MQTT user set (save to apply)");
        return;
    }

    if (trimmed.startsWith("mqttid ")) {
        String clientId = trimmed.substring(7);
        clientId.trim();
        strncpy(settings->mqtt.clientId, clientId.c_str(), sizeof(settings->mqtt.clientId) - 1);
        settings->mqtt.clientId[sizeof(settings->mqtt.clientId) - 1] = '\0';
        LOGGER.info("MQTT client id (save to apply)");
        return;
    }

    if (trimmed.startsWith("mqttbase ")) {
        String baseTopic = trimmed.substring(9);
        baseTopic.trim();
        strncpy(settings->mqtt.baseTopic, baseTopic.c_str(), sizeof(settings->mqtt.baseTopic) - 1);
        settings->mqtt.baseTopic[sizeof(settings->mqtt.baseTopic) - 1] = '\0';
        LOGGER.info("MQTT base topic (save to apply)");
        return;
    }

    if (trimmed.startsWith("mqtttype ")) {
        String typeText = trimmed.substring(9);
        typeText.trim();
        typeText.toLowerCase();
        MqttServerType serverType = MqttServerTypeDisable;
        if (!parseMqttServerType(typeText.c_str(), serverType)) {
            LOGGER.error("mqtttype disable|remote|local");
            return;
        }
        settings->mqtt.serverType = serverType;
        LOGGER.info(String("MQTT serverType ") + mqttServerTypeName(serverType) + " (save to apply)");
        return;
    }

    if (trimmed.startsWith("mqtten ")) {
        String flag = trimmed.substring(7);
        flag.trim();
        flag.toLowerCase();
        const bool turnOn = (flag == "on" || flag == "1" || flag == "true");
        settings->mqtt.serverType = turnOn ? MqttServerTypeRemote : MqttServerTypeDisable;
        LOGGER.info(String("MQTT serverType ") + mqttServerTypeName(settings->mqtt.serverType) + " (save to apply)");
        return;
    }

    LOGGER.error("Unknown command. Type help");
}
