#pragma once

#include <Arduino.h>

class StatusRgb {
public:
    void setCritical(bool enabled);
    void setBootHeld(bool enabled);
    void setPairingHeld(bool enabled);
    void pulseGreen();
    void pulseRed();
    void pulseBlue();
    bool allowsPairingBlink() const;
    void writePairingPhase(bool ledOn);
    void service();

private:
    static constexpr uint8_t kBrightness = 48;
    static constexpr unsigned long kPulseMs = 100UL;

    volatile bool criticalHeld = false;
    volatile bool bootHeld = false;
    volatile bool pairingHeld = false;
    volatile bool pairingPhaseOn = false;
    volatile bool activityActive = false;
    volatile uint8_t activityRed = 0;
    volatile uint8_t activityGreen = 0;
    volatile uint8_t activityBlue = 0;
    volatile unsigned long activityUntilMs = 0;
    uint8_t lastRed = 255;
    uint8_t lastGreen = 255;
    uint8_t lastBlue = 255;

    bool allowsActivityPulse() const;
    void startPulse(uint8_t red, uint8_t green, uint8_t blue);
    void apply();
    void write(uint8_t red, uint8_t green, uint8_t blue);
};

extern StatusRgb STATUS_RGB;
