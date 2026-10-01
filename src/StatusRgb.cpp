#include "StatusRgb.h"
#include "pins.h"

StatusRgb STATUS_RGB;

static const int kLedPins[6] = {PIN_LED1, PIN_LED2, PIN_LED3, PIN_LED4, PIN_LED5, PIN_LED6};

bool StatusRgb::allowsPairingBlink() const {
    return !criticalHeld && !bootHeld;
}

bool StatusRgb::allowsActivityPulse() const {
    return !criticalHeld && !bootHeld;
}

bool StatusRgb::faultHeld() const {
    return criticalHeld || bootHeld;
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
    pinMode(PIN_BOARD_ROLE, INPUT);
    delay(2);
    hostRole = digitalRead(PIN_BOARD_ROLE) == LOW;
    pinsReady = true;
    faultBlinkOn = true;
    faultBlinkToggleMs = millis();
    rgbLedWrite(PIN_STATUS_RGB, 0, 0, 0);
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

void StatusRgb::setMqttBrokerListening(bool enabled) {
    mqttBrokerListening = enabled;
    apply();
}

void StatusRgb::setReadyGreen(bool enabled) {
    readyGreen = enabled;
    apply();
}

void StatusRgb::startLedPulse(int ledIndex) {
    if (ledIndex < 0 || ledIndex >= kPulseLedCount || !allowsActivityPulse()) {
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
    for (int ledIndex = 0; ledIndex < kPulseLedCount; ledIndex++) {
        if (pulseActive[ledIndex] && (long)(nowMs - pulseUntilMs[ledIndex]) >= 0) {
            pulseActive[ledIndex] = false;
        }
    }
    if (faultHeld()) {
        if ((long)(nowMs - faultBlinkToggleMs) >= (long)kFaultBlinkHalfMs) {
            faultBlinkToggleMs = nowMs;
            faultBlinkOn = !faultBlinkOn;
        }
    } else {
        faultBlinkOn = true;
        faultBlinkToggleMs = nowMs;
    }
    apply();
}

void StatusRgb::apply() {
    bool levelOn[kLedCount] = {false, false, false, false, false, false};
    if (faultHeld()) {
        levelOn[kLed5Index] = faultBlinkOn;
        writeLevels(levelOn);
        return;
    }

    for (int ledIndex = 0; ledIndex < kPulseLedCount; ledIndex++) {
        levelOn[ledIndex] = pulseActive[ledIndex];
    }
    if (mqttBrokerListening) {
        levelOn[3] = true;
    }
    if (hostRole) {
        levelOn[kLed5Index] = mqttConnected;
    } else {
        levelOn[kLed5Index] = readyGreen;
        if (pairingHeld) {
            levelOn[kLed6Index] = pairingPhaseOn;
        }
    }
    writeLevels(levelOn);
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
