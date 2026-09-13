#include "StatusRgb.h"
#include "SpiProtocol.h"
#include "pins.h"

StatusRgb STATUS_RGB;

bool StatusRgb::isApplicationFrame(uint8_t cmd) {
    return cmd != SpiCmdPing && cmd != SpiCmdGetStatus && cmd != SpiCmdTimeSync && cmd != SpiCmdReadEvent
        && cmd != SpiEvtPong && cmd != SpiEvtStatus && cmd != SpiEvtSlaveReady && cmd != SpiEvtLogRecord;
}

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

void StatusRgb::pulseReceive() {
    if (!allowsActivityPulse()) {
        return;
    }
    activityReceive = true;
    activityActive = true;
    activityUntilMs = millis() + kPulseMs;
    apply();
}

void StatusRgb::pulseSend() {
    if (!allowsActivityPulse()) {
        return;
    }
    activityReceive = false;
    activityActive = true;
    activityUntilMs = millis() + kPulseMs;
    apply();
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
        if (activityReceive) {
            write(0, kBrightness, 0);
        } else {
            write(kBrightness, 0, 0);
        }
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
