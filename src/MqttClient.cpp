#include "MqttClient.h"
#include "MqttBroker.h"
#include "Logger.h"
#include "StatusRgb.h"
#include "Defines.h"
#include "ZigbeeDeviceType.h"

#include <string.h>

MqttClient::MqttClient(SettingsManager *settingsManager, DeviceTopicMap *topicMap)
    : settingsManager(settingsManager), topicMap(topicMap), client(nullptr) {
    client = new PubSubClient(wifiClient);
    bootMs = millis();
}

MqttClient::~MqttClient() {
    clearTopicSubscriptions();
    delete client;
}

void MqttClient::setMessageHandler(MessageFn handler) {
    messageHandler = handler;
}

void MqttClient::setLocalBroker(MqttBroker *broker) {
    localBroker = broker;
}

bool MqttClient::usesLocalBroker() const {
    GlobalSettings *settings = settingsManager->getSettings();
    return settings->mqtt.serverType == MqttServerTypeLocal;
}

void MqttClient::rebuildTopics() {
    GlobalSettings *settings = settingsManager->getSettings();
    const String base = String(settings->mqtt.baseTopic);
    topicStatus = base + "/bridge/status";
    topicDevices = base + "/bridge/devices";
    topicPermitJoin = base + "/bridge/permit_join";
    topicConfigDevice = base + "/bridge/config/device";
    topicServerBorn = String(settings->mqtt.serverBornTopic);
}

void MqttClient::applyRemoteBrokerTarget() {
    GlobalSettings *settings = settingsManager->getSettings();
    client->setServer(settings->mqtt.server, settings->mqtt.port);
}

void MqttClient::begin(void (*rawCallback)(char *topic, byte *payload, unsigned int length)) {
    GlobalSettings *settings = settingsManager->getSettings();
    rebuildTopics();
    applyRemoteBrokerTarget();
    client->setCallback(rawCallback);
    client->setBufferSize(1024);
    client->setSocketTimeout(settings->mqtt.clientTimeoutMs / 1000 > 0 ? settings->mqtt.clientTimeoutMs / 1000 : 1);
    bootMs = millis();
    lastBornAnnounceMs = 0;
    bornBootAnnounceDone = false;
}

void MqttClient::onMessage(char *topic, byte *payload, unsigned int length) {
    // PubSubClient can underflow the payload length on a truncated PUBLISH after a
    // TCP reset; walking that length into ROM (e.g. 0x40000000) panics the host.
    static constexpr unsigned int kMaxPayloadBytes = 1024;
    if (topic == nullptr || topic[0] == '\0') {
        return;
    }
    if (length > kMaxPayloadBytes) {
        LOGGER.warning(
            "MQTT ignore corrupt/oversized message topic=" + String(topic)
            + " len=" + String((unsigned long)length)
        );
        return;
    }
    if (length > 0 && payload == nullptr) {
        LOGGER.warning("MQTT ignore message with null payload topic=" + String(topic));
        return;
    }

    String body;
    if (length > 0) {
        body.concat(reinterpret_cast<const char *>(payload), length);
    }
    LOGGER.debug("MQTT " + String(topic) + " = " + body);
    STATUS_RGB.pulseMqttCommandReceived();
    if (messageHandler != nullptr) {
        messageHandler(topic, body.c_str());
    }
}

bool MqttClient::isConnected() const {
    if (usesLocalBroker()) {
        return localAttached && localBroker != nullptr && localBroker->isListening();
    }
    return client != nullptr && client->connected();
}

void MqttClient::clearTopicSubscriptions() {
    while (subscriptionHead != nullptr) {
        TopicSubscription *next = subscriptionHead->next;
        free(subscriptionHead->topic);
        free(subscriptionHead);
        subscriptionHead = next;
    }
}

MqttClient::TopicSubscription *MqttClient::findTopicSubscription(const char *topic) const {
    if (topic == nullptr || topic[0] == '\0') {
        return nullptr;
    }
    for (TopicSubscription *node = subscriptionHead; node != nullptr; node = node->next) {
        if (node->topic != nullptr && strcmp(node->topic, topic) == 0) {
            return node;
        }
    }
    return nullptr;
}

void MqttClient::keepOrSubscribe(const String &subscribeTopic) {
    if (subscribeTopic.length() == 0) {
        return;
    }
    TopicSubscription *existing = findTopicSubscription(subscribeTopic.c_str());
    if (existing != nullptr) {
        existing->keepMarked = true;
        return;
    }
    if (!client->subscribe(subscribeTopic.c_str())) {
        LOGGER.warning("MQTT subscribe failed topic=" + subscribeTopic);
        return;
    }
    TopicSubscription *node = (TopicSubscription *)calloc(1, sizeof(TopicSubscription));
    if (node == nullptr) {
        return;
    }
    const size_t topicLen = subscribeTopic.length();
    node->topic = (char *)malloc(topicLen + 1);
    if (node->topic == nullptr) {
        free(node);
        return;
    }
    memcpy(node->topic, subscribeTopic.c_str(), topicLen + 1);
    node->keepMarked = true;
    node->next = subscriptionHead;
    subscriptionHead = node;
    LOGGER.info("MQTT subscribe topic=" + subscribeTopic);
}

void MqttClient::subscribeBridge() {
    client->subscribe(topicPermitJoin.c_str());
    client->subscribe(topicConfigDevice.c_str());
    subscribeDeviceCommands();
}

void MqttClient::subscribeDeviceCommands() {
    rebuildTopics();
    if (usesLocalBroker()) {
        return;
    }
    if (!isConnected() || topicMap == nullptr) {
        return;
    }

    for (TopicSubscription *node = subscriptionHead; node != nullptr; node = node->next) {
        node->keepMarked = false;
    }

    if (topicServerBorn.length() > 0) {
        keepOrSubscribe(topicServerBorn);
    }

    for (DeviceTopicEntry *entry = topicMap->first(); entry != nullptr; entry = DeviceTopicMap::nextEntry(entry)) {
        if (!entry->used) {
            continue;
        }

        if (entry->transport != DeviceTransportMqtt && !deviceTopicEmpty(entry->commandTopic)) {
            keepOrSubscribe(String(entry->commandTopic));
            if (DeviceTopicMap::usesTopicSuffix(entry->channelCount)) {
                keepOrSubscribe(String(entry->commandTopic) + "/+");
            }
        }

        if (!deviceTopicEmpty(entry->stateTopic)
            && (entry->transport == DeviceTransportMqtt
                || zigbeeDeviceTypeIsMeasurement(entry->zigbeeType))) {
            keepOrSubscribe(String(entry->stateTopic));
            if (entry->transport != DeviceTransportMqtt
                && DeviceTopicMap::usesTopicSuffix(entry->channelCount)) {
                keepOrSubscribe(String(entry->stateTopic) + "/+");
            }
        }
        if (entry->transport == DeviceTransportMqtt && !deviceTopicEmpty(entry->availabilityTopic)) {
            keepOrSubscribe(String(entry->availabilityTopic));
        }
    }

    TopicSubscription **link = &subscriptionHead;
    while (*link != nullptr) {
        TopicSubscription *node = *link;
        if (node->keepMarked) {
            link = &node->next;
            continue;
        }
        if (node->topic != nullptr) {
            client->unsubscribe(node->topic);
            LOGGER.info("MQTT unsubscribe topic=" + String(node->topic));
        }
        *link = node->next;
        free(node->topic);
        free(node);
    }
}

void MqttClient::attachLocalBroker() {
    rebuildTopics();
    lastDevicesJson = "";
    localAttached = true;
    LOGGER.info("MQTT connected to local broker");
    publishStatus("online");
}

void MqttClient::reconnect() {
    GlobalSettings *settings = settingsManager->getSettings();
    applyRemoteBrokerTarget();
    LOGGER.info("MQTT connecting to " + String(settings->mqtt.server) + ":" + String(settings->mqtt.port));

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
    clearTopicSubscriptions();
    LOGGER.info("MQTT connected");
    subscribeBridge();
    publishStatus("online");
}

void MqttClient::dispatch(bool staConnected) {
    GlobalSettings *settings = settingsManager->getSettings();
    if (settings->mqtt.serverType == MqttServerTypeLocal) {
        const bool brokerUp = localBroker != nullptr && localBroker->isListening();
        if (!brokerUp) {
            localAttached = false;
            STATUS_RGB.setMqttConnected(false);
            return;
        }
        if (!localAttached) {
            attachLocalBroker();
        }
        STATUS_RGB.setMqttConnected(true);
        serviceBornAnnounce();
        return;
    }

    localAttached = false;
    if (settings->mqtt.serverType != MqttServerTypeRemote || !staConnected) {
        STATUS_RGB.setMqttConnected(false);
        return;
    }

    if (client->connected()) {
        STATUS_RGB.setMqttConnected(true);
        client->loop();
        serviceBornAnnounce();
        return;
    }

    STATUS_RGB.setMqttConnected(false);

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
    const bool sent = usesLocalBroker()
        ? localBroker->publishFromHost(topic, data, retained)
        : client->publish(topic, data, retained);
    if (sent) {
        LOGGER.info(String("MQTT send topic=") + topic + " data=" + data);
    } else {
        LOGGER.warning(String("MQTT send failed topic=") + topic + " data=" + data);
        // Drop a half-closed remote socket so the next loop() does not parse junk.
        if (!usesLocalBroker() && client != nullptr) {
            client->disconnect();
        }
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
    if (entry == nullptr || deviceTopicEmpty(entry->stateTopic)) {
        return;
    }
    if (entry->transport == DeviceTransportMqtt) {
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
    if (publishMessage(topic.c_str(), payload.c_str(), true)) {
        STATUS_RGB.pulseMqttPublished();
    }
}

bool MqttClient::publishDeviceCommand(const DeviceTopicEntry *entry, const char *message, uint8_t endpoint) {
    if (entry == nullptr || deviceTopicEmpty(entry->commandTopic) || message == nullptr) {
        return false;
    }
    if (entry->transport == DeviceTransportMqtt) {
        const bool sent = publishMessage(entry->commandTopic, message, false);
        if (sent) {
            STATUS_RGB.pulseMqttPublished();
        }
        return sent;
    }
    uint8_t publishEndpoint = endpoint;
    String publishBody = String(message);
    if (DeviceTopicMap::usesPayloadParse(entry->channelCount)) {
        if (!DeviceTopicMap::isUsableEndpoint(endpoint)) {
            publishEndpoint = 1;
        }
        publishBody = DeviceTopicMap::statePublishPayload(entry, publishEndpoint, message);
        publishEndpoint = 0;
    } else if (DeviceTopicMap::usesTopicSuffix(entry->channelCount)
        && !DeviceTopicMap::isUsableEndpoint(publishEndpoint)) {
        publishEndpoint = 1;
    }
    const String topic = DeviceTopicMap::commandPublishTopic(
        entry,
        DeviceTopicMap::usesTopicSuffix(entry->channelCount) ? publishEndpoint : 1
    );
    const bool sent = publishMessage(topic.c_str(), publishBody.c_str(), false);
    if (sent) {
        STATUS_RGB.pulseMqttPublished();
    }
    return sent;
}

bool MqttClient::publishServerBornAnnounce() {
    rebuildTopics();
    if (topicServerBorn.length() == 0) {
        return false;
    }
    return publishMessage(topicServerBorn.c_str(), "online", false);
}

void MqttClient::serviceBornAnnounce() {
    rebuildTopics();
    if (topicServerBorn.length() == 0 || !isConnected()) {
        return;
    }
    GlobalSettings *settings = settingsManager->getSettings();
    const unsigned long now = millis();
    const unsigned long intervalMs =
        (unsigned long)clampBornIntervalMin(settings->mqtt.bornIntervalMin) * 60UL * 1000UL;
    if (!bornBootAnnounceDone) {
        const unsigned long bootDelayMs = usesLocalBroker()
            ? MQTT_BORN_BOOT_DELAY_MS
            : MQTT_BORN_BOOT_DELAY_REMOTE_MS;
        if ((now - bootMs) < bootDelayMs) {
            return;
        }
        if (publishServerBornAnnounce()) {
            bornBootAnnounceDone = true;
            lastBornAnnounceMs = now;
        }
        return;
    }
    if ((now - lastBornAnnounceMs) < intervalMs) {
        return;
    }
    if (publishServerBornAnnounce()) {
        lastBornAnnounceMs = now;
    }
}
