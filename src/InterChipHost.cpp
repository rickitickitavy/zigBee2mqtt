#include "InterChipHost.h"
#include "FirmwareOta.h"
#include "pins.h"
#include "Logger.h"

#include <SPI.h>
#include <string.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static TaskHandle_t hostSpiTaskHandle = nullptr;
static uint8_t hostSpiTx[SPI_MAX_FRAME] __attribute__((aligned(4)));
static uint8_t hostSpiRx[SPI_MAX_FRAME] __attribute__((aligned(4)));

static void hostSpiTask(void *arg) {
    (void)arg;
    UBaseType_t lastPriority = 1;
    for (;;) {
        const bool slaveOta = FIRMWARE_OTA.isUpdatingSlave();
        const UBaseType_t wantedPriority = slaveOta ? 10 : 1;
        if (hostSpiTaskHandle != nullptr && wantedPriority != lastPriority) {
            vTaskPrioritySet(hostSpiTaskHandle, wantedPriority);
            lastPriority = wantedPriority;
        }
        INTER_CHIP_HOST.pump();
        if (slaveOta) {
            taskYIELD();
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

InterChipHost INTER_CHIP_HOST;

static bool commandExpectsReply(uint8_t cmd) {
    return cmd == SpiCmdPing || cmd == SpiCmdGetStatus || cmd == SpiCmdSetSettings || cmd == SpiCmdTimeSync
        || cmd == SpiCmdFirmwareOta;
}

static bool isDeviceControlCommand(uint8_t cmd) {
    return cmd == SpiCmdZclOnOff || cmd == SpiCmdZclWriteAttr || cmd == SpiCmdZclCommand;
}

static bool sameDeviceControlDest(const SpiFrame &frame, const uint8_t *payload, uint16_t length) {
    if (payload == nullptr || length < 9 || frame.length < 9) {
        return false;
    }
    return memcmp(frame.payload, payload, 8) == 0 && frame.payload[8] == payload[8];
}

bool InterChipHost::isSlaveResetPaused() const {
    if (slaveResetPauseUntilMs == 0) {
        return false;
    }
    return (long)(millis() - slaveResetPauseUntilMs) < 0;
}

uint32_t InterChipHost::slaveResetPauseRemainingMs() const {
    if (!isSlaveResetPaused()) {
        return 0;
    }
    return (uint32_t)(slaveResetPauseUntilMs - millis());
}

void InterChipHost::releaseSlaveResetLine() {
    pinMode(PIN_SLAVE_RST, OUTPUT);
    digitalWrite(PIN_SLAVE_RST, HIGH);
    if (resetAsserting) {
        resetAsserting = false;
        state = HostBringupWaitReady;
        waitStartedMs = millis();
    }
}

void InterChipHost::startSlaveResetPause() {
    slaveResetPauseUntilMs = millis() + kSlaveResetPauseMs;
    releaseSlaveResetLine();
    LOGGER.info("Slave reset pause 120s (USB update window)");
}

void InterChipHost::cancelSlaveResetPause() {
    slaveResetPauseUntilMs = 0;
    LOGGER.info("Slave reset pause cancelled");
}

void InterChipHost::resetSlaveSynchronous() {
    if (isSlaveResetPaused()) {
        LOGGER.warning("Skipping sync slave reset; pause active");
        releaseSlaveResetLine();
        return;
    }
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
    slaveVersionText[0] = '\0';
    slaveZigbeeStarted = false;
    zigbeeStartDeadlineMs = 0;
    bootResetCompleted = true;
    LOGGER.info("Slave reset released (sync), waiting SLAVE_READY");
}

void InterChipHost::begin() {
    pinMode(PIN_SPI_IRQ, INPUT_PULLDOWN);
    pinMode(PIN_SLAVE_RST, OUTPUT);
    digitalWrite(PIN_SLAVE_RST, HIGH);
    pinMode(PIN_SPI_CS, OUTPUT);
    digitalWrite(PIN_SPI_CS, HIGH);
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_CS);
    if (!bootResetCompleted) {
        resetSlaveSynchronous();
    }
    xTaskCreate(hostSpiTask, "hostSpi", 8192, nullptr, 1, &hostSpiTaskHandle);
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

void InterChipHost::pushLedBrightness(uint8_t bluePercent, uint8_t greenPercent) {
    if (state != HostBringupNormal) {
        return;
    }
    uint8_t payload[2];
    payload[0] = bluePercent;
    payload[1] = greenPercent;
    enqueueInternal(SpiCmdSetLedBrightness, payload, 2, false, 0);
}

const char *InterChipHost::slaveFirmwareVersion() const {
    return slaveVersionText;
}

void InterChipHost::setEventHandler(EventFn handler) {
    eventHandler = handler;
}

bool InterChipHost::isNormal() const {
    return state == HostBringupNormal;
}

bool InterChipHost::isLinkHealthy() const {
    return state == HostBringupNormal && pingTimeouts == 0;
}

HostBringupState InterChipHost::bringupState() const {
    return state;
}

bool InterChipHost::isKeepaliveCommand(uint8_t cmd) {
    return cmd == SpiCmdPing || cmd == SpiCmdGetStatus || cmd == SpiCmdTimeSync;
}

bool InterChipHost::isSpiEventCommand(uint8_t cmd) {
    return cmd >= 0x80;
}

void InterChipHost::noteKeepaliveSuccess() {
    pingTimeouts = 0;
    lastKeepaliveOkMs = millis();
    lastPongMs = lastKeepaliveOkMs;
}

bool InterChipHost::tryEnqueue(uint8_t cmd, const uint8_t *payload, uint16_t length) {
    if (state != HostBringupNormal && cmd != SpiCmdPing && cmd != SpiCmdSetSettings && cmd != SpiCmdTimeSync
        && cmd != SpiCmdGetStatus && cmd != SpiCmdReadEvent) {
        return false;
    }
    return enqueueInternal(cmd, payload, length, commandExpectsReply(cmd), 0);
}

bool InterChipHost::tryEnqueueWithSeq(uint8_t cmd, uint8_t seq, const uint8_t *payload, uint16_t length) {
    if (state != HostBringupNormal && cmd != SpiCmdFirmwareOta) {
        return false;
    }
    return enqueueInternal(cmd, payload, length, commandExpectsReply(cmd), seq);
}

uint8_t InterChipHost::lastEnqueuedSeq() const {
    return lastQueuedSeq;
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
    enqueueInternal(SpiCmdTimeSync, payload, 4, true, 0);
}

void InterChipHost::noteKeepaliveQuiet() {
    lastPingMs = millis();
    lastTimeSyncMs = millis();
    noteKeepaliveSuccess();
}

void InterChipHost::holdForFirmwareOta() {
    noteKeepaliveQuiet();
    for (int i = 0; i < kOutQueue; i++) {
        if (!outbound[i].used) {
            continue;
        }
        if (outbound[i].frame.cmd != SpiCmdFirmwareOta) {
            outbound[i].used = false;
        }
    }
    if (hasPending && pendingCmd != SpiCmdFirmwareOta) {
        hasPending = false;
    }
}

bool InterChipHost::tryCoalesceDeviceControl(
    uint8_t cmd,
    const uint8_t *payload,
    uint16_t length,
    bool expectReply
) {
    if (!isDeviceControlCommand(cmd) || payload == nullptr || length < 9) {
        return false;
    }
    for (int i = 0; i < kOutQueue; i++) {
        if (!outbound[i].used || !isDeviceControlCommand(outbound[i].frame.cmd)) {
            continue;
        }
        if (!sameDeviceControlDest(outbound[i].frame, payload, length)) {
            continue;
        }
        outbound[i].expectReply = expectReply;
        outbound[i].frame.cmd = cmd;
        outbound[i].frame.length = length;
        memcpy(outbound[i].frame.payload, payload, length);
        return true;
    }
    return false;
}

bool InterChipHost::enqueueInternal(
    uint8_t cmd,
    const uint8_t *payload,
    uint16_t length,
    bool expectReply,
    uint8_t seq
) {
    if (length > SPI_MAX_PAYLOAD) {
        return false;
    }
    if (FIRMWARE_OTA.isUpdatingSlave() && cmd != SpiCmdFirmwareOta) {
        return false;
    }
    if (FIRMWARE_OTA.isUpdatingSlave() && cmd == SpiCmdFirmwareOta) {
        // At most one OTA frame in the outbound queue — avoid flood on retry/pump races.
        for (int i = 0; i < kOutQueue; i++) {
            if (!outbound[i].used || outbound[i].frame.cmd != SpiCmdFirmwareOta) {
                continue;
            }
            outbound[i].expectReply = expectReply;
            if (seq != 0) {
                outbound[i].frame.seq = seq;
            }
            lastQueuedSeq = outbound[i].frame.seq;
            outbound[i].frame.length = length;
            if (length > 0 && payload != nullptr) {
                memcpy(outbound[i].frame.payload, payload, length);
            } else {
                outbound[i].frame.length = 0;
            }
            return true;
        }
    }
    if (tryCoalesceDeviceControl(cmd, payload, length, expectReply)) {
        return true;
    }
    for (int i = 0; i < kOutQueue; i++) {
        if (outbound[i].used) {
            continue;
        }
        outbound[i].used = true;
        outbound[i].expectReply = expectReply;
        outbound[i].frame.cmd = cmd;
        if (seq != 0) {
            outbound[i].frame.seq = seq;
        } else {
            outbound[i].frame.seq = nextSeq++;
            if (nextSeq == 0) {
                nextSeq = 1;
            }
        }
        lastQueuedSeq = outbound[i].frame.seq;
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
    if (isSlaveResetPaused()) {
        LOGGER.warning("Skipping slave reset; pause active");
        releaseSlaveResetLine();
        return;
    }
    if (FIRMWARE_OTA.isUpdatingSlave()) {
        LOGGER.warning("Skipping slave reset during firmware OTA");
        return;
    }
    if (state == HostBringupReset && resetAsserting) {
        return;
    }
    state = HostBringupReset;
    settingsQueued = false;
    settingsRetries = 0;
    hasPending = false;
    pingTimeouts = 0;
    slaveVersionText[0] = '\0';
    slaveZigbeeStarted = false;
    zigbeeStartDeadlineMs = 0;
    pulseResetStart();
}

void InterChipHost::noteBringupFailureAndReset() {
    bringupFailStreak++;
    if (bringupFailStreak >= kBringupFailBeforeFallbackClock && spiClockHz != SPI_SPEED_HZ_4M) {
        LOGGER.warning("Slave silent at boot 3 times; SPI clock -> 4 MHz before reset");
        setClockHz(SPI_SPEED_HZ_4M);
    }
    enterReset();
}

void InterChipHost::noteLinkLostAndReset() {
    LOGGER.warning("SPI link lost, resetting slave");
    pingTimeouts = 0;
    lastKeepaliveOkMs = 0;
    enterReset();
}

void InterChipHost::pulseResetStart() {
    if (isSlaveResetPaused()) {
        LOGGER.warning("Skipping slave reset pulse; pause active");
        releaseSlaveResetLine();
        return;
    }
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
    enqueueInternal(SpiCmdSetSettings, payload, 6, true, 0);
    settingsQueued = true;
    state = HostBringupPushSettings;
    LOGGER.info("Pushing settings to slave");
}

void InterChipHost::handleInbound(const SpiFrame &frame) {
    lastPongMs = millis();
    // Only SPI events complete a pending command. Echo of our own TX (same cmd+seq
    // on MISO) must not clear pending or the keepalive fail streak.
    if (hasPending && frame.seq == pendingSeq && isSpiEventCommand(frame.cmd)) {
        hasPending = false;
        if (isKeepaliveCommand(pendingCmd) || pendingCmd == SpiCmdSetSettings) {
            noteKeepaliveSuccess();
        }
    }
    if (frame.cmd == SpiEvtSlaveReady) {
        LOGGER.info("SLAVE_READY");
        if (state == HostBringupWaitReady) {
            maybePushSettings();
        } else if (state == HostBringupNormal && lastSettingsOkMs != 0
            && (millis() - lastSettingsOkMs) >= 8000UL
            && !FIRMWARE_OTA.isUpdatingSlave()) {
            LOGGER.warning("Slave ready after drop; re-pushing settings");
            settingsQueued = false;
            hasPending = false;
            maybePushSettings();
        }
    }
    if (frame.cmd == SpiEvtSettingsOk) {
        state = HostBringupNormal;
        bringupFailStreak = 0;
        noteKeepaliveSuccess();
        lastPingMs = millis();
        lastSettingsOkMs = millis();
        lastStatusMs = 0;
        slaveZigbeeStarted = false;
        zigbeeStartDeadlineMs = millis() + kZigbeeStartTimeoutMs;
        LOGGER.info("Slave settings applied, waiting for Zigbee start");
    }
    if (frame.cmd == SpiEvtPong && frame.length >= 1) {
        const bool zigbeeUp = (frame.payload[0] & SPI_STATUS_FLAG_ZIGBEE_STARTED) != 0;
        if (zigbeeUp) {
            if (!slaveZigbeeStarted) {
                LOGGER.info("Slave Zigbee started");
            }
            slaveZigbeeStarted = true;
            zigbeeStartDeadlineMs = 0;
        } else if (slaveZigbeeStarted) {
            LOGGER.warning("Slave Zigbee lost (ping); resetting slave");
            slaveZigbeeStarted = false;
            zigbeeStartDeadlineMs = 0;
            enterReset();
        }
    }
    if (frame.cmd == SpiEvtStatus && frame.length >= 2) {
        size_t versionLength = (size_t)(frame.length - 1);
        if (versionLength > SPI_STATUS_VERSION_MAX) {
            versionLength = SPI_STATUS_VERSION_MAX;
        }
        memcpy(slaveVersionText, frame.payload + 1, versionLength);
        slaveVersionText[versionLength] = '\0';
        if ((frame.payload[0] & SPI_STATUS_FLAG_ZIGBEE_STARTED) != 0) {
            if (!slaveZigbeeStarted) {
                LOGGER.info("Slave Zigbee started");
            }
            slaveZigbeeStarted = true;
            zigbeeStartDeadlineMs = 0;
        }
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
    const uint8_t timedOutCmd = pendingCmd;
    hasPending = false;
    SpiFrame timeoutFrame;
    timeoutFrame.cmd = SpiEvtTimeout;
    timeoutFrame.seq = pendingSeq;
    timeoutFrame.length = 1;
    timeoutFrame.payload[0] = timedOutCmd;
    if (!(FIRMWARE_OTA.isUpdatingSlave() && timedOutCmd == SpiCmdFirmwareOta)) {
        LOGGER.warning("SPI slave reply timeout cmd=" + String(timedOutCmd));
    } else {
        // Silent during exclusive OTA — first-try timeouts are expected under flash load.
    }
    if (FIRMWARE_OTA.busy() && timedOutCmd != SpiCmdFirmwareOta) {
        return;
    }
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
        noteBringupFailureAndReset();
        return;
    }
    if (state == HostBringupWaitReady) {
        noteBringupFailureAndReset();
        return;
    }
    if (state == HostBringupNormal && isKeepaliveCommand(timedOutCmd)) {
        if (FIRMWARE_OTA.busy()) {
            return;
        }
        pingTimeouts++;
        LOGGER.warning("SPI keepalive fail streak=" + String(pingTimeouts));
        if (pingTimeouts >= kLinkLostTimeouts) {
            noteLinkLostAndReset();
        }
    }
}

void InterChipHost::transferOnce(const SpiFrame *hostFrame) {
    memset(hostSpiTx, 0, sizeof(hostSpiTx));
    memset(hostSpiRx, 0, sizeof(hostSpiRx));
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
    if (spiEncodeFrame(toSend, hostSpiTx, sizeof(hostSpiTx)) == 0) {
        return;
    }

    SPI.beginTransaction(SPISettings((int)spiClockHz, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_SPI_CS, LOW);
    delayMicroseconds(50);
    SPI.transferBytes(hostSpiTx, hostSpiRx, SPI_MAX_FRAME);
    digitalWrite(PIN_SPI_CS, HIGH);
    SPI.endTransaction();
    const unsigned long transferGapUs = FIRMWARE_OTA.isUpdatingSlave() ? 50UL : kMinTransferGapUs;
    delayMicroseconds(transferGapUs);

    SpiFrame inbound;
    if (spiDecodeFrame(hostSpiRx, SPI_MAX_FRAME, inbound)) {
        handleInbound(inbound);
    }
}

void InterChipHost::pump() {
    pulseResetFinishIfDue();

    if (state == HostBringupWaitReady && !resetAsserting) {
        if ((millis() - waitStartedMs) >= kReadyTimeoutMs) {
            LOGGER.warning("Slave ready timeout, resetting again");
            noteBringupFailureAndReset();
        }
    }

    if (hasPending && (long)(millis() - pendingDeadlineMs) >= 0) {
        emitLocalTimeout();
    }

    if (state == HostBringupNormal && !FIRMWARE_OTA.busy()) {
        if (zigbeeStartDeadlineMs != 0
            && !slaveZigbeeStarted
            && (int32_t)(millis() - zigbeeStartDeadlineMs) >= 0) {
            LOGGER.warning("Slave Zigbee did not start within 30s; resetting slave");
            zigbeeStartDeadlineMs = 0;
            enterReset();
        } else if (lastKeepaliveOkMs != 0 && (millis() - lastKeepaliveOkMs) >= kLinkDeadMs) {
            LOGGER.warning("SPI keepalive silent too long, resetting slave");
            noteLinkLostAndReset();
        } else {
            if ((millis() - lastPingMs) >= kPingPeriodMs) {
                lastPingMs = millis();
                enqueueInternal(SpiCmdPing, nullptr, 0, true, 0);
            }
            if ((millis() - lastTimeSyncMs) >= kTimeSyncMs) {
                lastTimeSyncMs = millis();
                requestTimeSync();
            }
            const unsigned long statusPeriodMs =
                (zigbeeStartDeadlineMs != 0 && !slaveZigbeeStarted)
                    ? kZigbeeStartStatusPeriodMs
                    : kStatusPeriodMs;
            if (lastStatusMs == 0 || (millis() - lastStatusMs) >= statusPeriodMs) {
                lastStatusMs = millis();
                enqueueInternal(SpiCmdGetStatus, nullptr, 0, true, 0);
            }
        }
    }

    if (resetAsserting) {
        return;
    }

    QueuedFrame *nextOut = nullptr;
    if (!hasPending) {
        for (int i = 0; i < kOutQueue; i++) {
            if (!outbound[i].used) {
                continue;
            }
            if (FIRMWARE_OTA.isUpdatingSlave() && outbound[i].frame.cmd != SpiCmdFirmwareOta) {
                outbound[i].used = false;
                continue;
            }
            nextOut = &outbound[i];
            break;
        }
    }

    const bool irqHigh = digitalRead(PIN_SPI_IRQ) == HIGH;
    const unsigned long pollMs =
        (FIRMWARE_OTA.isUpdatingSlave() && hasPending) ? 20UL : kPollMs;
    const bool pollDue = (millis() - lastPollMs) >= pollMs;
    if (FIRMWARE_OTA.isUpdatingSlave() && !hasPending && nextOut == nullptr && !irqHigh) {
        return;
    }
    if (nextOut == nullptr && !irqHigh && !pollDue) {
        return;
    }
    lastPollMs = millis();

    if (nextOut != nullptr) {
        if (nextOut->expectReply) {
            hasPending = true;
            pendingSeq = nextOut->frame.seq;
            pendingCmd = nextOut->frame.cmd;
            unsigned long replyTimeoutMs = kReplyTimeoutMs;
            if (nextOut->frame.cmd == SpiCmdFirmwareOta) {
                if (!FIRMWARE_OTA.slaveBeginAcked()) {
                    replyTimeoutMs = kOtaBeginReplyTimeoutMs;
                } else if (FIRMWARE_OTA.isLastSlaveFrameEnd()) {
                    replyTimeoutMs = kOtaEndReplyTimeoutMs;
                } else {
                    replyTimeoutMs = kOtaChunkReplyTimeoutMs;
                }
            }
            pendingDeadlineMs = millis() + replyTimeoutMs;
        }
        transferOnce(&nextOut->frame);
        nextOut->used = false;
        return;
    }

    transferOnce(nullptr);
}
