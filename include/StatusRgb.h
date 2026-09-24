#pragma once

#include <Arduino.h>

class StatusRgb {
public:
    void begin();
    void setCritical(bool enabled);
    void setBootHeld(bool enabled);
    void setPairingHeld(bool enabled);
    void setMqttConnected(bool enabled);
    void setMqttBrokerListening(bool enabled);
    void setReadyGreen(bool enabled);
    void pulseMqttCommandReceived();
    void pulseMqttPublished();
    void pulsePacketReceived();
    void pulseKnownDevicePacket();
    void pulseAckSent();
    void pulsePacketToDevice();
    bool allowsPairingBlink() const;
    void writePairingPhase(bool ledOn);
    void service();

private:
    static constexpr int kLedCount = 4;
    static constexpr unsigned long kPulseMs = 100UL;
    static constexpr uint8_t kLedOnLevel = HIGH;
    static constexpr uint8_t kLedOffLevel = LOW;
    static constexpr uint8_t kRgbBrightness = 48;

    volatile bool criticalHeld = false;
    volatile bool bootHeld = false;
    volatile bool pairingHeld = false;
    volatile bool pairingPhaseOn = false;
    volatile bool mqttConnected = false;
    volatile bool mqttBrokerListening = false;
    volatile bool readyGreen = false;
    volatile bool pulseActive[kLedCount] = {false, false, false, false};
    volatile unsigned long pulseUntilMs[kLedCount] = {0, 0, 0, 0};
    uint8_t lastLevel[kLedCount] = {255, 255, 255, 255};
    uint8_t lastRgbRed = 255;
    uint8_t lastRgbGreen = 255;
    uint8_t lastRgbBlue = 255;
    bool pinsReady = false;

    bool allowsActivityPulse() const;
    void startLedPulse(int ledIndex);
    void apply();
    void writeLevels(const bool levelOn[kLedCount]);
    void configureLedPin(int gpioNumber);
    void writeRgb(uint8_t red, uint8_t green, uint8_t blue);
};

extern StatusRgb STATUS_RGB;
