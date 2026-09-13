#include "StatusRgb.h"
#include "pins.h"

StatusRgb STATUS_RGB;

bool StatusRgb::allowsPairingBlink() const {
    return !criticalHeld && !bootHeld;
}

bool StatusRgb::allowsActivityPulse() const {
    return !criticalHeld && !bootHeld && !pairingHeld;
}

void StatusRgb::setCritical(bool enabled) {
    criticalHeld = enabled;
    apply();
}

void StatusRgb::setBootHeld(bool enabled) {
    bootHeld = enabled;
    apply();
}

void StatusRgb::setPairingHeld(bool enabled) {
    pairingHeld = enabled;
    if (!enabled) {
        pairingPhaseOn = false;
    }
    apply();
}

void StatusRgb::startPulse(uint8_t red, uint8_t green, uint8_t blue) {
    if (!allowsActivityPulse()) {
        return;
    }
    activityRed = red;
    activityGreen = green;
    activityBlue = blue;
    activityActive = true;
    activityUntilMs = millis() + kPulseMs;
    apply();
}

void StatusRgb::pulseGreen() {
    startPulse(0, kBrightness, 0);
}

void StatusRgb::pulseRed() {
    startPulse(kBrightness, 0, 0);
}

void StatusRgb::pulseBlue() {
    startPulse(0, 0, kBrightness);
}

void StatusRgb::writePairingPhase(bool ledOn) {
    pairingPhaseOn = ledOn;
    apply();
}

void StatusRgb::service() {
    if (activityActive && (long)(millis() - activityUntilMs) >= 0) {
        activityActive = false;
    }
    apply();
}

void StatusRgb::apply() {
    if (criticalHeld || bootHeld) {
        write(kBrightness, 0, 0);
        return;
    }
    if (pairingHeld) {
        if (pairingPhaseOn) {
            write(0, 0, kBrightness);
        } else {
            write(0, 0, 0);
        }
        return;
    }
    if (activityActive) {
        write(activityRed, activityGreen, activityBlue);
        return;
    }
    write(0, 0, 0);
}

void StatusRgb::write(uint8_t red, uint8_t green, uint8_t blue) {
    if (red == lastRed && green == lastGreen && blue == lastBlue) {
        return;
    }
    lastRed = red;
    lastGreen = green;
    lastBlue = blue;
    rgbLedWrite(PIN_STATUS_RGB, red, green, blue);
}
