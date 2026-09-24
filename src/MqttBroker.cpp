#include "MqttBroker.h"
#include "Logger.h"
#include "StatusRgb.h"

#include <string.h>

MqttBroker::AuthServer::AuthServer(SettingsManager *settingsManager)
    : PicoMQTT::Server((uint16_t)settingsManager->getSettings()->mqtt.port),
      settingsManager(settingsManager) {}

PicoMQTT::ConnectReturnCode MqttBroker::AuthServer::auth(
    const char *clientId,
    const char *username,
    const char *password
) {
    (void)clientId;
    if (clients.size() > MQTT_BROKER_CLIENT_MAX) {
        return PicoMQTT::CRC_SERVER_UNAVAILABLE;
    }
    GlobalSettings *settings = settingsManager->getSettings();
    if (settings->mqtt.username[0] == '\0' || settings->mqtt.password[0] == '\0') {
        return PicoMQTT::CRC_ACCEPTED;
    }
    if (username == nullptr || password == nullptr) {
        return PicoMQTT::CRC_NOT_AUTHORIZED;
    }
    if (strcmp(username, settings->mqtt.username) == 0
        && strcmp(password, settings->mqtt.password) == 0) {
        return PicoMQTT::CRC_ACCEPTED;
    }
    return PicoMQTT::CRC_BAD_USERNAME_OR_PASSWORD;
}

MqttBroker::MqttBroker(SettingsManager *settingsManager)
    : settingsManager(settingsManager) {}

void MqttBroker::setLocalMessageHandler(LocalMessageFn handler) {
    localMessageHandler = handler;
}

bool MqttBroker::isListening() const {
    return server != nullptr;
}

void MqttBroker::bindHostSubscriptions() {
    if (server == nullptr) {
        return;
    }
    server->subscribe("#", [this](char *topic, char *payload) {
        if (localMessageHandler != nullptr) {
            localMessageHandler(topic, payload != nullptr ? payload : "");
        }
    });
}

bool MqttBroker::publishFromHost(const char *topic, const char *payload, bool retained) {
    if (server == nullptr || topic == nullptr || topic[0] == '\0') {
        return false;
    }
    const char *data = payload != nullptr ? payload : "";
    return server->publish(topic, data, (uint8_t)0, retained);
}

void MqttBroker::dispatch(bool wifiHasAddress) {
    GlobalSettings *settings = settingsManager->getSettings();
    const bool shouldListen = settings->mqtt.serverType == MqttServerTypeLocal && wifiHasAddress;
    if (!shouldListen) {
        if (server != nullptr) {
            delete server;
            server = nullptr;
            STATUS_RGB.setMqttBrokerListening(false);
            LOGGER.info("MQTT broker stopped");
        }
        return;
    }
    if (server == nullptr) {
        server = new AuthServer(settingsManager);
        server->begin();
        bindHostSubscriptions();
        STATUS_RGB.setMqttBrokerListening(true);
        LOGGER.info("MQTT broker listening on 0.0.0.0:" + String(settings->mqtt.port));
    }
    server->loop();
}
