#include "StatusRgb.h"
#include "pins.h"

StatusRgb STATUS_RGB;

static const int kLedPins[4] = {PIN_LED1, PIN_LED2, PIN_LED3, PIN_LED4};

bool StatusRgb::allowsPairingBlink() const {
    return !criticalHeld && !bootHeld;
}

bool StatusRgb::allowsActivityPulse() const {
    return !criticalHeld && !bootHeld;
}

void StatusRgb::configureLedPin(int gpioNumber) {
    pinMode(gpioNumber, OUTPUT);
    digitalWrite(gpioNumber, kLedOffLevel);
}

void StatusRgb::begin() {
    bootHeld = true;
    for (int ledIndex = 0; ledIndex < kLedCount; ledIndex++) {
        configureLedPin(kLedPins[ledIndex]);
        lastLevel[ledIndex] = 255;
    }
    pinsReady = true;
    lastRgbRed = 255;
    apply();
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

void StatusRgb::setMqttConnected(bool enabled) {
    mqttConnected = enabled;
    apply();
}

void StatusRgb::setReadyGreen(bool enabled) {
    readyGreen = enabled;
    apply();
}

void StatusRgb::startLedPulse(int ledIndex) {
    if (ledIndex < 0 || ledIndex >= kLedCount || !allowsActivityPulse()) {
        return;
    }
    pulseActive[ledIndex] = true;
    pulseUntilMs[ledIndex] = millis() + kPulseMs;
    apply();
}

void StatusRgb::pulseMqttCommandReceived() {
    startLedPulse(0);
}

void StatusRgb::pulseMqttPublished() {
    startLedPulse(1);
}

void StatusRgb::pulsePacketReceived() {
    startLedPulse(0);
}

void StatusRgb::pulseKnownDevicePacket() {
    startLedPulse(1);
}

void StatusRgb::pulseAckSent() {
    startLedPulse(2);
}

void StatusRgb::pulsePacketToDevice() {
    startLedPulse(3);
}

void StatusRgb::writePairingPhase(bool ledOn) {
    pairingPhaseOn = ledOn;
    apply();
}

void StatusRgb::service() {
    const unsigned long nowMs = millis();
    for (int ledIndex = 0; ledIndex < kLedCount; ledIndex++) {
        if (pulseActive[ledIndex] && (long)(nowMs - pulseUntilMs[ledIndex]) >= 0) {
            pulseActive[ledIndex] = false;
        }
    }
    apply();
}

void StatusRgb::apply() {
    bool levelOn[kLedCount] = {false, false, false, false};
    if (criticalHeld || bootHeld) {
        writeLevels(levelOn);
        writeRgb(kRgbBrightness, 0, 0);
        return;
    }

    for (int ledIndex = 0; ledIndex < kLedCount; ledIndex++) {
        levelOn[ledIndex] = pulseActive[ledIndex];
    }
    writeLevels(levelOn);

    if (pairingHeld) {
        writeRgb(0, 0, pairingPhaseOn ? kRgbBrightness : 0);
        return;
    }
    if (mqttConnected || readyGreen) {
        writeRgb(0, kRgbBrightness, 0);
        return;
    }
    writeRgb(0, 0, 0);
}

void StatusRgb::writeLevels(const bool levelOn[kLedCount]) {
    if (!pinsReady) {
        return;
    }
    for (int ledIndex = 0; ledIndex < kLedCount; ledIndex++) {
        const uint8_t level = levelOn[ledIndex] ? kLedOnLevel : kLedOffLevel;
        if (level == lastLevel[ledIndex]) {
            continue;
        }
        lastLevel[ledIndex] = level;
        digitalWrite(kLedPins[ledIndex], level);
    }
}

void StatusRgb::writeRgb(uint8_t red, uint8_t green, uint8_t blue) {
    if (red == lastRgbRed && green == lastRgbGreen && blue == lastRgbBlue) {
        return;
    }
    lastRgbRed = red;
    lastRgbGreen = green;
    lastRgbBlue = blue;
    rgbLedWrite(PIN_STATUS_RGB, red, green, blue);
}
