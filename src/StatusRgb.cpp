#include "StatusRgb.h"
#include "pins.h"

StatusRgb STATUS_RGB;

static const int kLedPins[6] = {PIN_LED1, PIN_LED2, PIN_LED3, PIN_LED4, PIN_LED5, PIN_LED6};

#if defined(BOARD_ROLE_HOST)
static const LedColor kLedColors[6] = {
    LedColorBlue,
    LedColorGreen,
    LedColorGreen,
    LedColorGreen,
    LedColorGreen,
    LedColorRed
};
#elif defined(BOARD_ROLE_SLAVE)
static const LedColor kLedColors[6] = {
    LedColorRed,
    LedColorBlue,
    LedColorGreen,
    LedColorBlue,
    LedColorGreen,
    LedColorBlue
};
#else
#error "Define BOARD_ROLE_HOST or BOARD_ROLE_SLAVE"
#endif

bool StatusRgb::allowsPairingBlink() const {
    return !criticalHeld && !bootHeld;
}

bool StatusRgb::allowsActivityPulse() const {
    return !criticalHeld && !bootHeld && !updateHeld;
}

bool StatusRgb::slaveFaultBlinkHeld() const {
    return !hostRole && (criticalHeld || bootHeld);
}

bool StatusRgb::hostBootBlinkHeld() const {
    return hostRole && bootHeld;
}

LedColor StatusRgb::colorForLed(int ledIndex) const {
    if (ledIndex < 0 || ledIndex >= kLedCount) {
        return LedColorRed;
    }
    return kLedColors[ledIndex];
}

uint8_t StatusRgb::clampBrightnessPercent(uint8_t percent) const {
    if (percent < 1) {
        return 1;
    }
    if (percent > 100) {
        return 100;
    }
    return percent;
}

uint32_t StatusRgb::dutyForColor(LedColor color) const {
    uint8_t percent = 100;
    if (color == LedColorBlue) {
        percent = blueBrightnessPercent;
    } else if (color == LedColorGreen) {
        percent = greenBrightnessPercent;
    } else {
        return kPwmMaxDuty;
    }
    return (kPwmMaxDuty * (uint32_t)percent) / 100UL;
}

void StatusRgb::configureLedPin(int ledIndex) {
    const int gpioNumber = kLedPins[ledIndex];
    const LedColor color = colorForLed(ledIndex);
    if (color == LedColorRed) {
        pinMode(gpioNumber, OUTPUT);
        digitalWrite(gpioNumber, kLedOffLevel);
        pwmAttached[ledIndex] = false;
        return;
    }
    if (ledcAttach(gpioNumber, kPwmFrequencyHz, kPwmResolutionBits)) {
        ledcWrite(gpioNumber, 0);
        pwmAttached[ledIndex] = true;
        return;
    }
    pinMode(gpioNumber, OUTPUT);
    digitalWrite(gpioNumber, kLedOffLevel);
    pwmAttached[ledIndex] = false;
}

void StatusRgb::begin() {
    bootHeld = true;
    blueBrightnessPercent = 100;
    greenBrightnessPercent = 100;
    for (int ledIndex = 0; ledIndex < kLedCount; ledIndex++) {
        configureLedPin(ledIndex);
        lastOutput[ledIndex] = 0xFFFF;
    }
#if defined(BOARD_ROLE_HOST)
    hostRole = true;
#elif defined(BOARD_ROLE_SLAVE)
    hostRole = false;
#else
#error "Define BOARD_ROLE_HOST or BOARD_ROLE_SLAVE"
#endif
    pinsReady = true;
    faultBlinkOn = true;
    faultBlinkToggleMs = millis();
    updateBlinkOn = true;
    updateBlinkToggleMs = millis();
#if PIN_STATUS_RGB >= 0
    rgbLedWrite(PIN_STATUS_RGB, 0, 0, 0);
#endif
    apply();
}

void StatusRgb::setColorBrightness(uint8_t bluePercent, uint8_t greenPercent) {
    blueBrightnessPercent = clampBrightnessPercent(bluePercent);
    greenBrightnessPercent = clampBrightnessPercent(greenPercent);
    for (int ledIndex = 0; ledIndex < kLedCount; ledIndex++) {
        const LedColor color = colorForLed(ledIndex);
        if (color == LedColorBlue || color == LedColorGreen) {
            lastOutput[ledIndex] = 0xFFFF;
        }
    }
    apply();
}

void StatusRgb::setCritical(bool enabled) {
    criticalHeld = enabled;
    apply();
    if (enabled && !hostRole) {
        delay(200);
        ESP.restart();
    }
}

void StatusRgb::setBootHeld(bool enabled) {
    bootHeld = enabled;
    apply();
}

void StatusRgb::setUpdateHeld(bool enabled) {
    updateHeld = enabled;
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
    if (hostBootBlinkHeld() || slaveFaultBlinkHeld()) {
        if ((long)(nowMs - faultBlinkToggleMs) >= (long)kFaultBlinkHalfMs) {
            faultBlinkToggleMs = nowMs;
            faultBlinkOn = !faultBlinkOn;
        }
    } else {
        faultBlinkOn = true;
        faultBlinkToggleMs = nowMs;
    }
    if (hostRole && updateHeld && !criticalHeld) {
        if ((long)(nowMs - updateBlinkToggleMs) >= (long)kUpdateBlinkHalfMs) {
            updateBlinkToggleMs = nowMs;
            updateBlinkOn = !updateBlinkOn;
        }
    } else {
        updateBlinkOn = true;
        updateBlinkToggleMs = nowMs;
    }
    apply();
}

void StatusRgb::apply() {
    bool levelOn[kLedCount] = {false, false, false, false, false, false};

    if (slaveFaultBlinkHeld()) {
        levelOn[kLed5Index] = faultBlinkOn;
        writeLevels(levelOn);
        return;
    }

    if (hostRole) {
        if (hostBootBlinkHeld()) {
            levelOn[kLed5Index] = faultBlinkOn;
        } else {
            for (int ledIndex = 0; ledIndex < kPulseLedCount; ledIndex++) {
                levelOn[ledIndex] = pulseActive[ledIndex];
            }
            if (mqttBrokerListening) {
                levelOn[3] = true;
            }
            levelOn[kLed5Index] = mqttConnected;
        }
        if (criticalHeld) {
            levelOn[kLed6Index] = true;
        } else if (updateHeld) {
            levelOn[kLed6Index] = updateBlinkOn;
        }
        writeLevels(levelOn);
        return;
    }

    for (int ledIndex = 0; ledIndex < kPulseLedCount; ledIndex++) {
        levelOn[ledIndex] = pulseActive[ledIndex];
    }
    levelOn[kLed5Index] = readyGreen;
    if (pairingHeld) {
        levelOn[kLed6Index] = pairingPhaseOn;
    }
    writeLevels(levelOn);
}

void StatusRgb::writeLevels(const bool levelOn[kLedCount]) {
    if (!pinsReady) {
        return;
    }
    for (int ledIndex = 0; ledIndex < kLedCount; ledIndex++) {
        const LedColor color = colorForLed(ledIndex);
        const int gpioNumber = kLedPins[ledIndex];
        uint16_t outputValue = 0;
        if (levelOn[ledIndex]) {
            if (color == LedColorRed || !pwmAttached[ledIndex]) {
                outputValue = kLedOnLevel;
            } else {
                outputValue = (uint16_t)dutyForColor(color);
            }
        } else {
            outputValue = 0;
        }
        if (outputValue == lastOutput[ledIndex]) {
            continue;
        }
        lastOutput[ledIndex] = outputValue;
        if (color == LedColorRed || !pwmAttached[ledIndex]) {
            digitalWrite(gpioNumber, levelOn[ledIndex] ? kLedOnLevel : kLedOffLevel);
        } else {
            ledcWrite(gpioNumber, outputValue);
        }
    }
}
