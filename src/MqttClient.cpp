#include "MqttClient.h"
#include "Logger.h"

#include <string.h>

MqttClient::MqttClient(SettingsManager *settingsManager, DeviceTopicMap *topicMap)
    : settingsManager(settingsManager), topicMap(topicMap), client(nullptr) {
    client = new PubSubClient(wifiClient);
    memset(subscribedCommandTopics, 0, sizeof(subscribedCommandTopics));
}

MqttClient::~MqttClient() {
    delete client;
}

void MqttClient::setMessageHandler(MessageFn handler) {
    messageHandler = handler;
}

void MqttClient::rebuildTopics() {
    GlobalSettings *settings = settingsManager->getSettings();
    const String base = String(settings->mqtt.baseTopic);
    topicStatus = base + "/bridge/status";
    topicDevices = base + "/bridge/devices";
    topicPermitJoin = base + "/bridge/permit_join";
    topicConfigDevice = base + "/bridge/config/device";
}

void MqttClient::begin(void (*rawCallback)(char *topic, byte *payload, unsigned int length)) {
    GlobalSettings *settings = settingsManager->getSettings();
    rebuildTopics();
    client->setServer(settings->mqtt.server, settings->mqtt.port);
    client->setCallback(rawCallback);
    client->setBufferSize(1024);
    client->setSocketTimeout(settings->mqtt.clientTimeoutMs / 1000 > 0 ? settings->mqtt.clientTimeoutMs / 1000 : 1);
}

void MqttClient::onMessage(char *topic, byte *payload, unsigned int length) {
    String body;
    body.reserve(length + 1);
    for (unsigned int i = 0; i < length; i++) {
        body += (char)payload[i];
    }
    LOGGER.debug("MQTT " + String(topic) + " = " + body);
    if (messageHandler != nullptr) {
        messageHandler(topic, body.c_str());
    }
}

bool MqttClient::isConnected() const {
    return client != nullptr && client->connected();
}

void MqttClient::subscribeBridge() {
    client->subscribe(topicPermitJoin.c_str());
    client->subscribe(topicConfigDevice.c_str());
    subscribeDeviceCommands();
}

void MqttClient::clearCommandSubscriptions() {
    memset(subscribedCommandTopics, 0, sizeof(subscribedCommandTopics));
}

int MqttClient::findCommandSubscription(const char *topic) const {
    if (topic == nullptr || topic[0] == '\0') {
        return -1;
    }
    for (int i = 0; i < kMaxCommandSubscriptions; i++) {
        if (subscribedCommandTopics[i][0] != '\0' && strcmp(subscribedCommandTopics[i], topic) == 0) {
            return i;
        }
    }
    return -1;
}

int MqttClient::nextFreeCommandSubscription() const {
    for (int i = 0; i < kMaxCommandSubscriptions; i++) {
        if (subscribedCommandTopics[i][0] == '\0') {
            return i;
        }
    }
    return -1;
}

void MqttClient::subscribeDeviceCommands() {
    if (!isConnected() || topicMap == nullptr) {
        return;
    }

    bool keepSubscription[kMaxCommandSubscriptions];
    memset(keepSubscription, 0, sizeof(keepSubscription));

    for (int slotIndex = 0; slotIndex < DEVICE_MAP_SLOTS; slotIndex++) {
        DeviceTopicEntry *entry = topicMap->slotAt(slotIndex);
        if (entry == nullptr || !entry->used || entry->commandTopic[0] == '\0') {
            continue;
        }

        String subscribeTopics[2];
        subscribeTopics[0] = String(entry->commandTopic);
        int topicCount = 1;
        if (DeviceTopicMap::usesTopicSuffix(entry->channelCount)) {
            subscribeTopics[1] = String(entry->commandTopic) + "/+";
            topicCount = 2;
        }

        for (int topicIndex = 0; topicIndex < topicCount; topicIndex++) {
            const String &subscribeTopic = subscribeTopics[topicIndex];
            const int existing = findCommandSubscription(subscribeTopic.c_str());
            if (existing >= 0) {
                keepSubscription[existing] = true;
                continue;
            }
            if (!client->subscribe(subscribeTopic.c_str())) {
                LOGGER.warning("MQTT subscribe failed topic=" + subscribeTopic);
                continue;
            }
            const int freeIndex = nextFreeCommandSubscription();
            if (freeIndex < 0) {
                LOGGER.warning("MQTT subscribe table full topic=" + subscribeTopic);
                continue;
            }
            strncpy(
                subscribedCommandTopics[freeIndex],
                subscribeTopic.c_str(),
                sizeof(subscribedCommandTopics[freeIndex]) - 1
            );
            subscribedCommandTopics[freeIndex][sizeof(subscribedCommandTopics[freeIndex]) - 1] = '\0';
            keepSubscription[freeIndex] = true;
            LOGGER.info("MQTT subscribe topic=" + subscribeTopic);
        }
    }

    for (int i = 0; i < kMaxCommandSubscriptions; i++) {
        if (subscribedCommandTopics[i][0] == '\0' || keepSubscription[i]) {
            continue;
        }
        client->unsubscribe(subscribedCommandTopics[i]);
        LOGGER.info("MQTT unsubscribe topic=" + String(subscribedCommandTopics[i]));
        subscribedCommandTopics[i][0] = '\0';
    }
}

void MqttClient::reconnect() {
    GlobalSettings *settings = settingsManager->getSettings();
    LOGGER.info("MQTT connecting to " + String(settings->mqtt.server));

    const char *user = settings->mqtt.username[0] != '\0' ? settings->mqtt.username : nullptr;
    const char *password = user != nullptr ? settings->mqtt.password : nullptr;
    const bool ok = client->connect(settings->mqtt.clientId, user, password);
    if (!ok) {
        LOGGER.warning("MQTT connect failed, state " + String(client->state()));
        reconnectBackoffMs = reconnectBackoffMs == 0 ? 2000 : min(reconnectBackoffMs * 2, (unsigned long)30000);
        return;
    }

    reconnectBackoffMs = 0;
    lastDevicesJson = "";
    clearCommandSubscriptions();
    LOGGER.info("MQTT connected");
    subscribeBridge();
    publishStatus("online");
}

void MqttClient::dispatch(bool staConnected) {
    GlobalSettings *settings = settingsManager->getSettings();
    if (!settings->mqtt.enabled || !staConnected) {
        return;
    }

    if (client->connected()) {
        client->loop();
        return;
    }

    const unsigned long now = millis();
    const unsigned long waitMs = reconnectBackoffMs > 0 ? reconnectBackoffMs : (unsigned long)settings->mqtt.reconnectIntervalMs;
    if ((now - lastReconnectMs) < waitMs) {
        return;
    }
    lastReconnectMs = now;
    reconnect();
}

bool MqttClient::publishMessage(const char *topic, const char *payload, bool retained) {
    if (!isConnected() || topic == nullptr || topic[0] == '\0') {
        return false;
    }
    const char *data = payload != nullptr ? payload : "";
    const bool sent = client->publish(topic, data, retained);
    if (sent) {
        LOGGER.info(String("MQTT send topic=") + topic + " data=" + data);
    } else {
        LOGGER.warning(String("MQTT send failed topic=") + topic + " data=" + data);
    }
    return sent;
}

void MqttClient::publishStatus(const char *payload) {
    publishMessage(topicStatus.c_str(), payload, true);
}

void MqttClient::publishDevices(const String &json) {
    if (json == lastDevicesJson) {
        return;
    }
    if (publishMessage(topicDevices.c_str(), json.c_str(), false)) {
        lastDevicesJson = json;
    }
}

void MqttClient::publishDeviceState(const DeviceTopicEntry *entry, const char *message, uint8_t endpoint) {
    if (entry == nullptr || entry->stateTopic[0] == '\0') {
        return;
    }
    if (message == nullptr) {
        return;
    }
    if (DeviceTopicMap::usesPayloadParse(entry->channelCount) && !DeviceTopicMap::isUsableEndpoint(endpoint)) {
        LOGGER.warning("Skip state publish; parse mode needs a real endpoint");
        return;
    }
    const String topic = DeviceTopicMap::statePublishTopic(entry, endpoint);
    const String payload = DeviceTopicMap::statePublishPayload(entry, endpoint, message);
    publishMessage(topic.c_str(), payload.c_str(), true);
}
