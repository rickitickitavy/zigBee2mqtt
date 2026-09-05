#pragma once

#include <PubSubClient.h>
#include <WiFi.h>
#include "SettingsManager.h"
#include "DeviceTopicMap.h"

class MqttClient {
public:
    using MessageFn = void (*)(const char *topic, const char *payload);

    explicit MqttClient(SettingsManager *settingsManager, DeviceTopicMap *topicMap);
    ~MqttClient();

    void setMessageHandler(MessageFn handler);
    void begin(void (*rawCallback)(char *topic, byte *payload, unsigned int length));
    void onMessage(char *topic, byte *payload, unsigned int length);
    void dispatch(bool staConnected);
    bool isConnected() const;
    void publishStatus(const char *payload);
    void publishDevices(const String &json);
    void publishDeviceState(const DeviceTopicEntry *entry, bool on);
    void subscribeDeviceCommands();
    const String &permitJoinTopic() const { return topicPermitJoin; }
    const String &configDeviceTopic() const { return topicConfigDevice; }

private:
    SettingsManager *settingsManager;
    DeviceTopicMap *topicMap;
    WiFiClient wifiClient;
    PubSubClient *client;
    MessageFn messageHandler = nullptr;
    unsigned long lastReconnectMs = 0;
    unsigned long reconnectBackoffMs = 0;

    String topicStatus;
    String topicDevices;
    String topicPermitJoin;
    String topicConfigDevice;

    void rebuildTopics();
    void reconnect();
    void subscribeBridge();
};
