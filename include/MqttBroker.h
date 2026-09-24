#pragma once

#include <PicoMQTT.h>
#include "SettingsManager.h"

class MqttBroker {
public:
    using LocalMessageFn = void (*)(const char *topic, const char *payload);

    explicit MqttBroker(SettingsManager *settingsManager);

    void setLocalMessageHandler(LocalMessageFn handler);
    void dispatch(bool wifiHasAddress);
    bool isListening() const;
    int connectionCount() const;
    bool publishFromHost(const char *topic, const char *payload, bool retained);

private:
    class AuthServer : public PicoMQTT::Server {
    public:
        explicit AuthServer(SettingsManager *settingsManager);
        int connectedClientCount() const;

    protected:
        PicoMQTT::ConnectReturnCode auth(
            const char *clientId,
            const char *username,
            const char *password
        ) override;

    private:
        SettingsManager *settingsManager;
    };

    SettingsManager *settingsManager;
    AuthServer *server = nullptr;
    LocalMessageFn localMessageHandler = nullptr;

    void bindHostSubscriptions();
};
