#include "SerialCli.h"
#include "Logger.h"
#include "Defines.h"

SerialCli::SerialCli(
    SettingsManager *settingsManager,
    DeviceTopicMap *topicMap,
    ZigbeeCoordinator *coordinator,
    MqttClient *mqttClient
)
    : settingsManager(settingsManager),
      topicMap(topicMap),
      coordinator(coordinator),
      mqttClient(mqttClient) {}

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
    LOGGER.info("Commands:");
    LOGGER.info("  help");
    LOGGER.info("  devices");
    LOGGER.info("  permit [seconds]");
    LOGGER.info("  join close");
    LOGGER.info("  map <ieee> <stateTopic> <commandTopic> [availabilityTopic] [name]");
    LOGGER.info("  wifi <ssid> <password>");
    LOGGER.info("  mqtt <server> [port]");
    LOGGER.info("  mqttuser <username> <password>");
    LOGGER.info("  channel <11-26>");
    LOGGER.info("  save");
}

void SerialCli::handleLine(const String &line) {
    String trimmed = line;
    trimmed.trim();
    if (trimmed.length() == 0) {
        return;
    }

    LOGGER.info("> " + trimmed);

    if (trimmed == "help") {
        printHelp();
        return;
    }
    if (trimmed == "devices") {
        LOGGER.info(coordinator->devicesJson(topicMap));
        return;
    }
    if (trimmed == "join close") {
        coordinator->closeJoin();
        return;
    }
    if (trimmed.startsWith("permit")) {
        int seconds = DEFAULT_PERMIT_JOIN_SEC;
        if (trimmed.length() > 7) {
            seconds = trimmed.substring(7).toInt();
        }
        if (seconds <= 0) {
            coordinator->closeJoin();
        } else {
            coordinator->permitJoin((uint8_t)constrain(seconds, 1, 254));
        }
        return;
    }
    if (trimmed.startsWith("map ")) {
        String rest = trimmed.substring(4);
        rest.trim();
        int firstSpace = rest.indexOf(' ');
        int secondSpace = rest.indexOf(' ', firstSpace + 1);
        if (firstSpace < 0 || secondSpace < 0) {
            LOGGER.error("map <ieee> <state> <command> [avail] [name]");
            return;
        }
        String ieeeText = rest.substring(0, firstSpace);
        String afterIeee = rest.substring(firstSpace + 1);
        afterIeee.trim();
        int stateEnd = afterIeee.indexOf(' ');
        String stateTopic = afterIeee.substring(0, stateEnd);
        String afterState = afterIeee.substring(stateEnd + 1);
        afterState.trim();

        String commandTopic;
        String availability;
        String friendlyName;
        int commandEnd = afterState.indexOf(' ');
        if (commandEnd < 0) {
            commandTopic = afterState;
        } else {
            commandTopic = afterState.substring(0, commandEnd);
            String leftover = afterState.substring(commandEnd + 1);
            leftover.trim();
            int availEnd = leftover.indexOf(' ');
            if (availEnd < 0) {
                availability = leftover;
            } else {
                availability = leftover.substring(0, availEnd);
                friendlyName = leftover.substring(availEnd + 1);
                friendlyName.trim();
            }
        }

        uint8_t ieee[8];
        if (!topicMap->parseIeee(ieeeText.c_str(), ieee)) {
            LOGGER.error("Bad IEEE");
            return;
        }
        if (topicMap->upsert(ieee, friendlyName.c_str(), stateTopic.c_str(), commandTopic.c_str(), availability.c_str())
            == nullptr) {
            LOGGER.error("Device map full");
            return;
        }
        settingsManager->saveSetting(false);
        mqttClient->subscribeDeviceCommands();
        mqttClient->publishDevices(coordinator->devicesJson(topicMap));
        LOGGER.info("Mapped " + ieeeText);
        return;
    }
    if (trimmed.startsWith("wifi ")) {
        String rest = trimmed.substring(5);
        rest.trim();
        int split = rest.indexOf(' ');
        if (split < 0) {
            LOGGER.error("wifi <ssid> <password>");
            return;
        }
        GlobalSettings *settings = settingsManager->getSettings();
        String ssid = rest.substring(0, split);
        String password = rest.substring(split + 1);
        strncpy(settings->network.ssid, ssid.c_str(), sizeof(settings->network.ssid) - 1);
        strncpy(settings->network.password, password.c_str(), sizeof(settings->network.password) - 1);
        settings->network.wifiEnabled = true;
        settingsManager->saveSetting(true);
        LOGGER.info("WiFi saved; restarting");
        return;
    }
    if (trimmed.startsWith("mqttuser ")) {
        String rest = trimmed.substring(9);
        rest.trim();
        int split = rest.indexOf(' ');
        GlobalSettings *settings = settingsManager->getSettings();
        if (split < 0) {
            strncpy(settings->mqttUsername, rest.c_str(), sizeof(settings->mqttUsername) - 1);
            settings->mqttPassword[0] = '\0';
        } else {
            strncpy(settings->mqttUsername, rest.substring(0, split).c_str(), sizeof(settings->mqttUsername) - 1);
            strncpy(settings->mqttPassword, rest.substring(split + 1).c_str(), sizeof(settings->mqttPassword) - 1);
        }
        settingsManager->saveSetting(false);
        LOGGER.info("MQTT user saved");
        return;
    }
    if (trimmed.startsWith("mqtt ")) {
        String rest = trimmed.substring(5);
        rest.trim();
        int split = rest.indexOf(' ');
        GlobalSettings *settings = settingsManager->getSettings();
        if (split < 0) {
            strncpy(settings->mqttServer, rest.c_str(), sizeof(settings->mqttServer) - 1);
        } else {
            strncpy(settings->mqttServer, rest.substring(0, split).c_str(), sizeof(settings->mqttServer) - 1);
            settings->mqttPort = rest.substring(split + 1).toInt();
        }
        settingsManager->saveSetting(false);
        LOGGER.info("MQTT broker saved");
        return;
    }
    if (trimmed.startsWith("channel ")) {
        uint8_t channel = (uint8_t)trimmed.substring(8).toInt();
        SettingsManager::clampZigbeeChannel(channel);
        settingsManager->getSettings()->zigbeeChannel = channel;
        settingsManager->saveSetting(true);
        LOGGER.info("Channel saved; restarting");
        return;
    }
    if (trimmed == "save") {
        settingsManager->saveSetting(false);
        return;
    }

    LOGGER.error("Unknown command. Type help");
}
