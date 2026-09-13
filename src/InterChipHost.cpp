#include "InterChipHost.h"
#include "pins.h"
#include "Logger.h"

#include <SPI.h>
#include <string.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static void hostSpiTask(void *arg) {
    (void)arg;
    for (;;) {
        INTER_CHIP_HOST.pump();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

InterChipHost INTER_CHIP_HOST;

static bool commandExpectsReply(uint8_t cmd) {
    return cmd == SpiCmdPing || cmd == SpiCmdGetStatus || cmd == SpiCmdSetSettings || cmd == SpiCmdTimeSync;
}

void InterChipHost::resetSlaveSynchronous() {
    pinMode(PIN_SLAVE_RST, OUTPUT);
    digitalWrite(PIN_SLAVE_RST, LOW);
    delay(kRstPulseMs);
    digitalWrite(PIN_SLAVE_RST, HIGH);
    resetAsserting = false;
    state = HostBringupWaitReady;
    waitStartedMs = millis();
    settingsQueued = false;
    settingsRetries = 0;
    hasPending = false;
    pingTimeouts = 0;
    bootResetCompleted = true;
    LOGGER.info("Slave reset released (sync), waiting SLAVE_READY");
}

void InterChipHost::begin() {
    pinMode(PIN_SPI_IRQ, INPUT_PULLDOWN);
    pinMode(PIN_SLAVE_RST, OUTPUT);
    digitalWrite(PIN_SLAVE_RST, HIGH);
    pinMode(PIN_SPI_CS, OUTPUT);
    digitalWrite(PIN_SPI_CS, HIGH);
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);
    if (!bootResetCompleted) {
        resetSlaveSynchronous();
    }
    xTaskCreate(hostSpiTask, "hostSpi", 4096, nullptr, 1, nullptr);
}

void InterChipHost::setSettingsSource(uint8_t channel, uint8_t permitJoinSecValue) {
    zigbeeChannel = channel;
    permitJoinSec = permitJoinSecValue;
}

void InterChipHost::setClockHz(uint32_t speedHz) {
    if (speedHz < SPI_SPEED_HZ_MIN || speedHz > SPI_SPEED_HZ_MAX) {
        speedHz = DEFAULT_SPI_SPEED_HZ;
    }
    spiClockHz = speedHz;
    LOGGER.info("SPI clock " + String((unsigned long)spiClockHz) + " Hz");
}

uint32_t InterChipHost::clockHz() const {
    return spiClockHz;
}

void InterChipHost::setEventHandler(EventFn handler) {
    eventHandler = handler;
}

bool InterChipHost::isNormal() const {
    return state == HostBringupNormal;
}

HostBringupState InterChipHost::bringupState() const {
    return state;
}

bool InterChipHost::tryEnqueue(uint8_t cmd, const uint8_t *payload, uint16_t length) {
    if (state != HostBringupNormal && cmd != SpiCmdPing && cmd != SpiCmdSetSettings && cmd != SpiCmdTimeSync
        && cmd != SpiCmdGetStatus && cmd != SpiCmdReadEvent) {
        return false;
    }
    return enqueueInternal(cmd, payload, length, commandExpectsReply(cmd));
}

void InterChipHost::requestTimeSync() {
    uint32_t now = 0;
    time_t wall = time(nullptr);
    if (wall > 1700000000) {
        now = (uint32_t)wall;
    }
    uint8_t payload[4];
    payload[0] = (uint8_t)(now & 0xFF);
    payload[1] = (uint8_t)((now >> 8) & 0xFF);
    payload[2] = (uint8_t)((now >> 16) & 0xFF);
    payload[3] = (uint8_t)((now >> 24) & 0xFF);
    enqueueInternal(SpiCmdTimeSync, payload, 4, true);
}

bool InterChipHost::enqueueInternal(uint8_t cmd, const uint8_t *payload, uint16_t length, bool expectReply) {
    if (length > SPI_MAX_PAYLOAD) {
        return false;
    }
    for (int i = 0; i < kOutQueue; i++) {
        if (outbound[i].used) {
            continue;
        }
        outbound[i].used = true;
        outbound[i].expectReply = expectReply;
        outbound[i].frame.cmd = cmd;
        outbound[i].frame.seq = nextSeq++;
        if (nextSeq == 0) {
            nextSeq = 1;
        }
        outbound[i].frame.length = length;
        if (length > 0 && payload != nullptr) {
            memcpy(outbound[i].frame.payload, payload, length);
        }
        return true;
    }
    LOGGER.warning("SPI host outbound queue full");
    return false;
}

void InterChipHost::enterReset() {
    state = HostBringupReset;
    settingsQueued = false;
    settingsRetries = 0;
    hasPending = false;
    pingTimeouts = 0;
    pulseResetStart();
}

void InterChipHost::pulseResetStart() {
    digitalWrite(PIN_SLAVE_RST, LOW);
    resetStartedMs = millis();
    resetAsserting = true;
}

void InterChipHost::pulseResetFinishIfDue() {
    if (!resetAsserting) {
        return;
    }
    if ((millis() - resetStartedMs) < kRstPulseMs) {
        return;
    }
    digitalWrite(PIN_SLAVE_RST, HIGH);
    resetAsserting = false;
    state = HostBringupWaitReady;
    waitStartedMs = millis();
    LOGGER.info("Slave reset released, waiting SLAVE_READY");
}

void InterChipHost::maybePushSettings() {
    if (settingsQueued) {
        return;
    }
    uint8_t payload[6];
    payload[0] = zigbeeChannel;
    payload[1] = permitJoinSec;
    uint32_t now = 0;
    time_t wall = time(nullptr);
    if (wall > 1700000000) {
        now = (uint32_t)wall;
    }
    payload[2] = (uint8_t)(now & 0xFF);
    payload[3] = (uint8_t)((now >> 8) & 0xFF);
    payload[4] = (uint8_t)((now >> 16) & 0xFF);
    payload[5] = (uint8_t)((now >> 24) & 0xFF);
    enqueueInternal(SpiCmdSetSettings, payload, 6, true);
    settingsQueued = true;
    state = HostBringupPushSettings;
    LOGGER.info("Pushing settings to slave");
}

void InterChipHost::handleInbound(const SpiFrame &frame) {
    lastPongMs = millis();
    pingTimeouts = 0;
    if (hasPending && frame.seq == pendingSeq) {
        hasPending = false;
    }
    if (frame.cmd == SpiEvtSlaveReady) {
        LOGGER.info("SLAVE_READY");
        if (state == HostBringupWaitReady) {
            maybePushSettings();
        } else if (state == HostBringupNormal && lastSettingsOkMs != 0
            && (millis() - lastSettingsOkMs) >= 8000UL) {
            LOGGER.warning("Slave ready after drop; re-pushing settings");
            settingsQueued = false;
            hasPending = false;
            maybePushSettings();
        }
    }
    if (frame.cmd == SpiEvtSettingsOk) {
        state = HostBringupNormal;
        lastPongMs = millis();
        lastPingMs = millis();
        lastSettingsOkMs = millis();
        LOGGER.info("Slave settings applied, normal work");
    }
    if (frame.cmd == SpiEvtLogRecord && frame.length > 0) {
        const uint16_t textLen = frame.length;
        char line[SPI_MAX_PAYLOAD + 1];
        memcpy(line, frame.payload, textLen);
        line[textLen] = '\0';
        LOGGER.appendSlaveLine(line);
        return;
    }
    if (eventHandler != nullptr) {
        eventHandler(frame);
    }
}

void InterChipHost::emitLocalTimeout() {
    hasPending = false;
    SpiFrame timeoutFrame;
    timeoutFrame.cmd = SpiEvtTimeout;
    timeoutFrame.seq = pendingSeq;
    timeoutFrame.length = 1;
    timeoutFrame.payload[0] = pendingCmd;
    LOGGER.warning("SPI slave reply timeout cmd=" + String(pendingCmd));
    if (eventHandler != nullptr) {
        eventHandler(timeoutFrame);
    }
    if (state == HostBringupPushSettings) {
        if (settingsRetries < 3) {
            settingsRetries++;
            settingsQueued = false;
            LOGGER.warning("Retry SET_SETTINGS");
            maybePushSettings();
            return;
        }
        enterReset();
        return;
    }
    if (state == HostBringupWaitReady) {
        enterReset();
        return;
    }
    if (state == HostBringupNormal && pendingCmd == SpiCmdPing) {
        pingTimeouts++;
        if (pingTimeouts >= 3) {
            LOGGER.warning("SPI ping lost, resetting slave");
            pingTimeouts = 0;
            enterReset();
        }
    }
}

void InterChipHost::transferOnce(const SpiFrame *hostFrame) {
    uint8_t tx[SPI_MAX_FRAME];
    uint8_t rx[SPI_MAX_FRAME];
    memset(tx, 0, sizeof(tx));
    memset(rx, 0, sizeof(rx));
    SpiFrame toSend;
    if (hostFrame != nullptr) {
        toSend = *hostFrame;
    } else {
        toSend.cmd = SpiCmdReadEvent;
        toSend.seq = nextSeq++;
        if (nextSeq == 0) {
            nextSeq = 1;
        }
        toSend.length = 0;
    }
    if (spiEncodeFrame(toSend, tx, sizeof(tx)) == 0) {
        return;
    }

    SPI.beginTransaction(SPISettings((int)spiClockHz, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_SPI_CS, LOW);
    delayMicroseconds(50);
    SPI.transferBytes(tx, rx, SPI_MAX_FRAME);
    digitalWrite(PIN_SPI_CS, HIGH);
    SPI.endTransaction();
    delayMicroseconds(kMinTransferGapUs);

    SpiFrame inbound;
    if (spiDecodeFrame(rx, SPI_MAX_FRAME, inbound)) {
        handleInbound(inbound);
    }
}

void InterChipHost::pump() {
    pulseResetFinishIfDue();

    if (state == HostBringupWaitReady && !resetAsserting) {
        if ((millis() - waitStartedMs) >= kReadyTimeoutMs) {
            LOGGER.warning("Slave ready timeout, resetting again");
            enterReset();
        }
    }

    if (hasPending && (long)(millis() - pendingDeadlineMs) >= 0) {
        emitLocalTimeout();
    }

    if (state == HostBringupNormal) {
        if ((millis() - lastPingMs) >= kPingPeriodMs) {
            lastPingMs = millis();
            enqueueInternal(SpiCmdPing, nullptr, 0, true);
        }
        if ((millis() - lastTimeSyncMs) >= kTimeSyncMs) {
            lastTimeSyncMs = millis();
            requestTimeSync();
        }
    }

    if (resetAsserting) {
        return;
    }

    QueuedFrame *nextOut = nullptr;
    if (!hasPending) {
        for (int i = 0; i < kOutQueue; i++) {
            if (outbound[i].used) {
                nextOut = &outbound[i];
                break;
            }
        }
    }

    const bool irqHigh = digitalRead(PIN_SPI_IRQ) == HIGH;
    const bool pollDue = (millis() - lastPollMs) >= kPollMs;
    if (nextOut == nullptr && !irqHigh && !pollDue) {
        return;
    }
    lastPollMs = millis();

    if (nextOut != nullptr) {
        if (nextOut->expectReply) {
            hasPending = true;
            pendingSeq = nextOut->frame.seq;
            pendingCmd = nextOut->frame.cmd;
            pendingDeadlineMs = millis() + kReplyTimeoutMs;
        }
        transferOnce(&nextOut->frame);
        nextOut->used = false;
        return;
    }

    transferOnce(nullptr);
}
