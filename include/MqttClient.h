#pragma once

#include <PubSubClient.h>
#include <WiFi.h>
#include "SettingsManager.h"
#include "DeviceTopicMap.h"

class MqttBroker;

class MqttClient {
public:
    using MessageFn = void (*)(const char *topic, const char *payload);

    explicit MqttClient(SettingsManager *settingsManager, DeviceTopicMap *topicMap);
    ~MqttClient();

    void setMessageHandler(MessageFn handler);
    void setLocalBroker(MqttBroker *broker);
    void begin(void (*rawCallback)(char *topic, byte *payload, unsigned int length));
    void onMessage(char *topic, byte *payload, unsigned int length);
    void dispatch(bool staConnected);
    bool isConnected() const;
    void publishStatus(const char *payload);
    void publishDevices(const String &json);
    void publishDeviceState(const DeviceTopicEntry *entry, const char *message, uint8_t endpoint);
    bool publishDeviceCommand(const DeviceTopicEntry *entry, const char *message, uint8_t endpoint);
    void subscribeDeviceCommands();
    void serviceBornAnnounce();
    bool publishServerBornAnnounce();
    const String &permitJoinTopic() const { return topicPermitJoin; }
    const String &configDeviceTopic() const { return topicConfigDevice; }
    const String &serverBornTopic() const { return topicServerBorn; }

private:
    struct TopicSubscription {
        char *topic;
        bool keepMarked;
        TopicSubscription *next;
    };

    SettingsManager *settingsManager;
    DeviceTopicMap *topicMap;
    MqttBroker *localBroker = nullptr;
    bool localAttached = false;
    WiFiClient wifiClient;
    PubSubClient *client;
    MessageFn messageHandler = nullptr;
    unsigned long lastReconnectMs = 0;
    unsigned long reconnectBackoffMs = 0;
    unsigned long bootMs = 0;
    unsigned long lastBornAnnounceMs = 0;
    bool bornBootAnnounceDone = false;

    String topicStatus;
    String topicDevices;
    String topicPermitJoin;
    String topicConfigDevice;
    String topicServerBorn;
    String lastDevicesJson;
    TopicSubscription *subscriptionHead = nullptr;

    void rebuildTopics();
    bool usesLocalBroker() const;
    void attachLocalBroker();
    void applyRemoteBrokerTarget();
    void reconnect();
    void subscribeBridge();
    void clearTopicSubscriptions();
    TopicSubscription *findTopicSubscription(const char *topic) const;
    void keepOrSubscribe(const String &subscribeTopic);
    bool publishMessage(const char *topic, const char *payload, bool retained);
};
