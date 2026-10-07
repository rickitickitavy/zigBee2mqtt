#pragma once

#include <Arduino.h>

enum LedColor : uint8_t {
    LedColorRed = 0,
    LedColorGreen = 1,
    LedColorBlue = 2
};

class StatusRgb {
public:
    void begin();
    void setCritical(bool enabled);
    void setBootHeld(bool enabled);
    void setUpdateHeld(bool enabled);
    void setPairingHeld(bool enabled);
    void setMqttConnected(bool enabled);
    void setMqttBrokerListening(bool enabled);
    void setReadyGreen(bool enabled);
    void setColorBrightness(uint8_t bluePercent, uint8_t greenPercent);
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
    static constexpr int kLedCount = 6;
    static constexpr int kPulseLedCount = 4;
    static constexpr unsigned long kPulseMs = 100UL;
    static constexpr unsigned long kFaultBlinkHalfMs = 125UL;
    static constexpr unsigned long kUpdateBlinkHalfMs = 50UL;
    static constexpr uint8_t kLedOnLevel = HIGH;
    static constexpr uint8_t kLedOffLevel = LOW;
    static constexpr int kLed5Index = 4;
    static constexpr int kLed6Index = 5;
    static constexpr uint32_t kPwmFrequencyHz = 5000UL;
    static constexpr uint8_t kPwmResolutionBits = 8;
    static constexpr uint32_t kPwmMaxDuty = (1UL << kPwmResolutionBits) - 1UL;

    volatile bool criticalHeld = false;
    volatile bool bootHeld = false;
    volatile bool updateHeld = false;
    volatile bool pairingHeld = false;
    volatile bool pairingPhaseOn = false;
    volatile bool mqttConnected = false;
    volatile bool mqttBrokerListening = false;
    volatile bool readyGreen = false;
    volatile bool pulseActive[kLedCount] = {false, false, false, false, false, false};
    volatile unsigned long pulseUntilMs[kLedCount] = {0, 0, 0, 0, 0, 0};
    uint16_t lastOutput[kLedCount] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    bool pwmAttached[kLedCount] = {false, false, false, false, false, false};
    uint8_t blueBrightnessPercent = 100;
    uint8_t greenBrightnessPercent = 100;
    bool pinsReady = false;
    bool hostRole = true;
    bool faultBlinkOn = true;
    unsigned long faultBlinkToggleMs = 0;
    bool updateBlinkOn = true;
    unsigned long updateBlinkToggleMs = 0;

    bool allowsActivityPulse() const;
    bool slaveFaultBlinkHeld() const;
    bool hostBootBlinkHeld() const;
    void startLedPulse(int ledIndex);
    void apply();
    void writeLevels(const bool levelOn[kLedCount]);
    void configureLedPin(int ledIndex);
    uint8_t clampBrightnessPercent(uint8_t percent) const;
    uint32_t dutyForColor(LedColor color) const;
    LedColor colorForLed(int ledIndex) const;
};

extern StatusRgb STATUS_RGB;
