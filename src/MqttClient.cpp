#include "MqttClient.h"
#include "Logger.h"

MqttClient::MqttClient(SettingsManager *settingsManager, DeviceTopicMap *topicMap)
    : settingsManager(settingsManager), topicMap(topicMap), client(nullptr) {
    client = new PubSubClient(wifiClient);
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

void MqttClient::subscribeDeviceCommands() {
    if (!isConnected()) {
        return;
    }
    GlobalSettings *settings = settingsManager->getSettings();
    for (int i = 0; i < DEVICE_MAP_SLOTS; i++) {
        DeviceTopicEntry *entry = &settings->devices[i];
        if (entry->used && entry->commandTopic[0] != '\0') {
            client->subscribe(entry->commandTopic);
            LOGGER.info("MQTT subscribe " + String(entry->commandTopic));
        }
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

void MqttClient::publishStatus(const char *payload) {
    if (!isConnected()) {
        return;
    }
    client->publish(topicStatus.c_str(), payload, true);
}

void MqttClient::publishDevices(const String &json) {
    if (!isConnected()) {
        return;
    }
    client->publish(topicDevices.c_str(), json.c_str(), false);
}

void MqttClient::publishDeviceState(const DeviceTopicEntry *entry, bool on) {
    if (!isConnected() || entry == nullptr || entry->stateTopic[0] == '\0') {
        return;
    }
    client->publish(entry->stateTopic, on ? "ON" : "OFF", true);
}
