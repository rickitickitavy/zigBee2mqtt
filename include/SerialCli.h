#pragma once

#include "SettingsManager.h"
#include "DeviceTopicMap.h"
#include "ZigbeeCoordinator.h"
#include "MqttClient.h"

class SerialCli {
public:
    SerialCli(
        SettingsManager *settingsManager,
        DeviceTopicMap *topicMap,
        ZigbeeCoordinator *coordinator,
        MqttClient *mqttClient
    );

    void dispatch();

private:
    SettingsManager *settingsManager;
    DeviceTopicMap *topicMap;
    ZigbeeCoordinator *coordinator;
    MqttClient *mqttClient;
    String lineBuffer;

    void handleLine(const String &line);
    void printHelp();
};
