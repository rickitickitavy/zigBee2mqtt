#include "FirmwareOta.h"
#include "InterChipHost.h"
#include "InterChipSlave.h"
#include "Logger.h"

#include <Update.h>
#include <string.h>

FirmwareOta FIRMWARE_OTA;

bool FirmwareOta::busy() const {
    return currentPhase == Phase::Receiving || currentPhase == Phase::Slave
        || currentPhase == Phase::Host || currentPhase == Phase::Rebooting;
}

bool FirmwareOta::isUpdatingSlave() const {
    return currentPhase == Phase::Slave;
}

bool FirmwareOta::isApplyingImage() const {
    return slaveOtaActive || slaveBeginPending;
}

bool FirmwareOta::slaveBeginAcked() const {
    return beginAcked;
}

FirmwareOta::Phase FirmwareOta::phase() const {
    return currentPhase;
}

const char *FirmwareOta::phaseId() const {
    switch (currentPhase) {
        case Phase::Receiving:
            return "receiving";
        case Phase::Slave:
            return "slave";
        case Phase::Host:
            return "host";
        case Phase::Failed:
            return "failed";
        case Phase::Rebooting:
            return "rebooting";
        case Phase::Done:
            return "done";
        case Phase::Idle:
        default:
            return "idle";
    }
}

uint8_t FirmwareOta::percentOf(uint32_t done, uint32_t total) const {
    if (total == 0) {
        return 0;
    }
    if (done >= total) {
        return 100;
    }
    return (uint8_t)((done * 100UL) / total);
}

String FirmwareOta::statusJson() const {
    uint8_t uploadPercent = 0;
    uint8_t slavePercent = 0;
    uint8_t hostPercent = 0;
    if (currentPhase == Phase::Receiving) {
        uploadPercent = percentOf(receivedBytes, imageSize);
    } else if (currentPhase != Phase::Idle && currentPhase != Phase::Failed) {
        uploadPercent = 100;
    }
    if (currentPhase == Phase::Slave) {
        slavePercent = percentOf(imageOffset, imageSize);
    } else if (currentPhase == Phase::Host || currentPhase == Phase::Rebooting
        || currentPhase == Phase::Done) {
        slavePercent = 100;
    }
    if (currentPhase == Phase::Host) {
        hostPercent = percentOf(imageOffset, imageSize);
    } else if (currentPhase == Phase::Rebooting || currentPhase == Phase::Done) {
        hostPercent = 100;
    }
    if (currentPhase == Phase::Failed && imageSize > 0) {
        uploadPercent = percentOf(receivedBytes > 0 ? receivedBytes : imageOffset, imageSize);
    }
    String json = "{\"phase\":\"";
    json += phaseId();
    json += "\",\"uploadPercent\":";
    json += String((int)uploadPercent);
    json += ",\"slavePercent\":";
    json += String((int)slavePercent);
    json += ",\"hostPercent\":";
    json += String((int)hostPercent);
    json += ",\"bytesDone\":";
    json += String((unsigned long)imageOffset);
    json += ",\"bytesTotal\":";
    json += String((unsigned long)imageSize);
    if (currentPhase == Phase::Failed && errorText.length() > 0) {
        json += ",\"error\":\"";
        for (size_t i = 0; i < errorText.length(); i++) {
            const char character = errorText[i];
            if (character == '"' || character == '\\') {
                json += '\\';
            }
            json += character;
        }
        json += "\"";
    }
    json += "}";
    return json;
}

void FirmwareOta::fail(const char *message) {
    errorText = message != nullptr ? message : "Firmware update failed";
    const bool abortUpdate = hostUpdateStarted || slaveOtaActive;
    currentPhase = Phase::Failed;
    waitingForSlaveResult = false;
    beginQueued = false;
    beginAcked = false;
    hostUpdateStarted = false;
    rebootReadyMs = 0;
    if (stagingFile) {
        stagingFile.close();
    }
    if (abortUpdate) {
        Update.abort();
    }
    slaveOtaActive = false;
    slaveBeginPending = false;
    slaveBeginExtraLength = 0;
    slaveBytesRemaining = 0;
    lastSlavePayloadLength = 0;
    lastSlaveDataBytes = 0;
    slaveFrameFails = 0;
    clearStagingFile();
    LOGGER.error(errorText);
}

void FirmwareOta::clearStagingFile() {
    if (LittleFS.exists(kStagingPath)) {
        LittleFS.remove(kStagingPath);
    }
}

bool FirmwareOta::beginStaging(size_t contentLength) {
    if (busy()) {
        return false;
    }
    errorText = "";
    imageSize = (uint32_t)contentLength;
    imageOffset = 0;
    receivedBytes = 0;
    waitingForSlaveResult = false;
    beginQueued = false;
    beginAcked = false;
    hostUpdateStarted = false;
    hostRestartPending = false;
    rebootReadyMs = 0;
    LittleFS.mkdir("/ota");
    if (contentLength > 0) {
        const size_t freeBytes = LittleFS.totalBytes() - LittleFS.usedBytes();
        if (freeBytes < contentLength + 4096) {
            fail("Not enough filesystem space for firmware");
            return false;
        }
    }
    clearStagingFile();
    stagingFile = LittleFS.open(kStagingPath, "w");
    if (!stagingFile) {
        fail("Could not create firmware staging file");
        return false;
    }
    currentPhase = Phase::Receiving;
    LOGGER.info("Firmware staging start");
    return true;
}

bool FirmwareOta::writeStaging(const uint8_t *data, size_t length) {
    if (currentPhase != Phase::Receiving || !stagingFile) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    if (data == nullptr) {
        fail("Firmware staging write failed");
        return false;
    }
    if (stagingFile.write(data, length) != length) {
        fail("Firmware staging write failed");
        return false;
    }
    receivedBytes += (uint32_t)length;
    return true;
}

bool FirmwareOta::finishStaging() {
    if (currentPhase != Phase::Receiving || !stagingFile) {
        return false;
    }
    stagingFile.close();
    stagingFile = LittleFS.open(kStagingPath, "r");
    if (!stagingFile) {
        fail("Firmware staging file missing");
        return false;
    }
    imageSize = (uint32_t)stagingFile.size();
    if (imageSize == 0) {
        fail("Firmware image is empty");
        return false;
    }
    imageOffset = 0;
    lastSlavePayloadLength = 0;
    lastSlaveDataBytes = 0;
    slaveFrameFails = 0;
    currentPhase = Phase::Slave;
    INTER_CHIP_HOST.holdForFirmwareOta();
    LOGGER.info("Firmware staging done, updating slave");
    return true;
}

void FirmwareOta::abortStaging() {
    if (stagingFile) {
        stagingFile.close();
    }
    if (currentPhase == Phase::Receiving) {
        fail("Firmware upload aborted");
    }
}

bool FirmwareOta::sendLastSlaveFrame(bool isResend) {
    if (lastSlavePayloadLength == 0) {
        fail("Firmware OTA frame missing");
        return false;
    }
    bool queued = false;
    if (isResend && lastSlaveSeq != 0) {
        queued = INTER_CHIP_HOST.tryEnqueueWithSeq(
            SpiCmdFirmwareOta,
            lastSlaveSeq,
            lastSlavePayload,
            lastSlavePayloadLength
        );
    } else {
        queued = INTER_CHIP_HOST.tryEnqueue(SpiCmdFirmwareOta, lastSlavePayload, lastSlavePayloadLength);
        lastSlaveSeq = INTER_CHIP_HOST.lastEnqueuedSeq();
    }
    if (!queued) {
        waitingForSlaveResult = false;
        LOGGER.warning("SPI firmware frame queue full, will retry");
        return false;
    }
    waitingForSlaveResult = true;
    return true;
}

void FirmwareOta::onSlaveFrameLost() {
    waitingForSlaveResult = false;
    slaveFrameFails++;
    LOGGER.warning(
        "SPI firmware frame lost seq=" + String((int)lastSlaveSeq)
        + " fail=" + String((int)slaveFrameFails) + "/" + String((int)kMaxSlaveFrameFails)
    );
    if (slaveFrameFails >= kMaxSlaveFrameFails) {
        fail("Slave firmware update failed after repeated SPI frame loss");
        return;
    }
    sendLastSlaveFrame(true);
}

bool FirmwareOta::enqueueNextSlaveChunk() {
    if (waitingForSlaveResult) {
        return true;
    }
    if (!INTER_CHIP_HOST.isNormal()) {
        fail("Slave link is not ready");
        return false;
    }
    if (lastSlavePayloadLength > 0) {
        return sendLastSlaveFrame(slaveFrameFails > 0);
    }
    uint8_t payload[SPI_MAX_PAYLOAD];
    uint16_t packedLength = 0;
    lastSlaveDataBytes = 0;
    if (!beginQueued) {
        uint8_t sizeBytes[4];
        sizeBytes[0] = (uint8_t)(imageSize & 0xFF);
        sizeBytes[1] = (uint8_t)((imageSize >> 8) & 0xFF);
        sizeBytes[2] = (uint8_t)((imageSize >> 16) & 0xFF);
        sizeBytes[3] = (uint8_t)((imageSize >> 24) & 0xFF);
        packedLength = spiPackFirmwareOta(payload, sizeof(payload), SPI_OTA_BEGIN, sizeBytes, 4);
        beginQueued = true;
        LOGGER.info("Slave firmware begin queued; waiting for erase");
    } else {
        if (!stagingFile) {
            fail("Firmware staging file missing");
            return false;
        }
        if (!stagingFile.seek(imageOffset)) {
            fail("Firmware staging seek failed");
            return false;
        }
        uint8_t chunk[SPI_FILE_CHUNK_MAX];
        const size_t remaining = imageSize - imageOffset;
        const size_t toRead = remaining > SPI_FILE_CHUNK_MAX ? SPI_FILE_CHUNK_MAX : remaining;
        const size_t readLength = stagingFile.read(chunk, toRead);
        if (readLength != toRead) {
            fail("Firmware staging read failed");
            return false;
        }
        uint8_t flags = 0;
        if (imageOffset + (uint32_t)readLength >= imageSize) {
            flags = SPI_OTA_END;
        }
        packedLength = spiPackFirmwareOta(payload, sizeof(payload), flags, chunk, (uint16_t)readLength);
        lastSlaveDataBytes = (uint32_t)readLength;
    }
    if (packedLength == 0) {
        fail("Firmware OTA pack failed");
        return false;
    }
    memcpy(lastSlavePayload, payload, packedLength);
    lastSlavePayloadLength = packedLength;
    return sendLastSlaveFrame(false);
}

void FirmwareOta::onSpiFrame(const SpiFrame &frame) {
    if (currentPhase != Phase::Slave || !waitingForSlaveResult) {
        return;
    }
    if (frame.cmd == SpiEvtTimeout) {
        if (frame.length < 1 || frame.payload[0] != SpiCmdFirmwareOta) {
            return;
        }
        onSlaveFrameLost();
        return;
    }
    if (frame.cmd != SpiEvtCmdResult || frame.length < 1) {
        return;
    }
    waitingForSlaveResult = false;
    if (frame.payload[0] == 0) {
        onSlaveFrameLost();
        return;
    }
    slaveFrameFails = 0;
    if (!beginAcked) {
        beginAcked = true;
        lastSlavePayloadLength = 0;
        lastSlaveDataBytes = 0;
        LOGGER.info("Slave firmware begin acked, sending image");
        enqueueNextSlaveChunk();
        return;
    }
    imageOffset += lastSlaveDataBytes;
    lastSlavePayloadLength = 0;
    lastSlaveDataBytes = 0;
    if (imageOffset >= imageSize) {
        startHostApply();
        return;
    }
    enqueueNextSlaveChunk();
}

bool FirmwareOta::startHostApply() {
    if (stagingFile) {
        stagingFile.close();
    }
    currentPhase = Phase::Host;
    LOGGER.info("Slave firmware committed, updating host");
    File sourceFile = LittleFS.open(kStagingPath, "r");
    if (!sourceFile) {
        fail("Firmware staging file missing");
        return false;
    }
    const size_t size = sourceFile.size();
    sourceFile.close();
    if (!Update.begin(size, U_FLASH)) {
        fail("Host OTA begin failed");
        return false;
    }
    hostUpdateStarted = true;
    imageOffset = 0;
    return true;
}

void FirmwareOta::pumpHostApply() {
    if (!hostUpdateStarted) {
        return;
    }
    File sourceFile = LittleFS.open(kStagingPath, "r");
    if (!sourceFile) {
        fail("Firmware staging file missing");
        return;
    }
    if (!sourceFile.seek(imageOffset)) {
        sourceFile.close();
        fail("Host OTA seek failed");
        return;
    }
    uint8_t chunk[1024];
    const size_t remaining = imageSize - imageOffset;
    const size_t toRead = remaining > sizeof(chunk) ? sizeof(chunk) : remaining;
    const size_t readLength = sourceFile.read(chunk, toRead);
    sourceFile.close();
    if (readLength != toRead) {
        fail("Host OTA read failed");
        return;
    }
    if (Update.write(chunk, readLength) != readLength) {
        fail("Host OTA write failed");
        return;
    }
    imageOffset += (uint32_t)readLength;
    if (imageOffset < imageSize) {
        return;
    }
    if (!Update.end(true)) {
        fail("Host OTA end failed");
        return;
    }
    clearStagingFile();
    currentPhase = Phase::Rebooting;
    rebootReadyMs = millis() + 2500;
    LOGGER.info("Host firmware written; reboot wait");
}

void FirmwareOta::pumpSlaveBegin() {
    if (!slaveBeginPending) {
        return;
    }
    slaveBeginPending = false;
    LOGGER.info("Slave firmware erase start size=" + String((unsigned long)slaveBytesRemaining));
    if (!Update.begin(slaveBytesRemaining, U_FLASH)) {
        slaveOtaActive = false;
        slaveBytesRemaining = 0;
        slaveBeginExtraLength = 0;
        LOGGER.error("Slave firmware erase failed");
        INTER_CHIP_SLAVE.completeCommandResult(slaveBeginSeq, false);
        return;
    }
    bool writeOk = true;
    if (slaveBeginExtraLength > 0) {
        if (slaveBeginExtraLength > slaveBytesRemaining
            || Update.write(slaveBeginExtra, slaveBeginExtraLength) != slaveBeginExtraLength) {
            writeOk = false;
        } else {
            slaveBytesRemaining -= slaveBeginExtraLength;
        }
        slaveBeginExtraLength = 0;
    }
    if (!writeOk) {
        Update.abort();
        slaveOtaActive = false;
        slaveBytesRemaining = 0;
        LOGGER.error("Slave firmware first chunk write failed");
        INTER_CHIP_SLAVE.completeCommandResult(slaveBeginSeq, false);
        return;
    }
    LOGGER.info("Slave firmware erase done");
    INTER_CHIP_SLAVE.completeCommandResult(slaveBeginSeq, true);
}

void FirmwareOta::pump() {
    pumpSlaveBegin();
    if (currentPhase == Phase::Slave) {
        enqueueNextSlaveChunk();
        return;
    }
    if (currentPhase == Phase::Host) {
        pumpHostApply();
        return;
    }
    if (currentPhase == Phase::Rebooting && rebootReadyMs != 0 && millis() >= rebootReadyMs) {
        rebootReadyMs = 0;
        hostRestartPending = true;
        currentPhase = Phase::Done;
        LOGGER.info("Host restarting after firmware update");
    }
}

bool FirmwareOta::consumeHostRestart() {
    if (!hostRestartPending) {
        return false;
    }
    hostRestartPending = false;
    return true;
}

bool FirmwareOta::consumeFirmwareRestart() {
    if (!slaveRestartPending) {
        return false;
    }
    slaveRestartPending = false;
    return true;
}

bool FirmwareOta::handleSlaveFrame(const SpiFrame &frame) {
    if (frame.cmd != SpiCmdFirmwareOta || frame.length < 1) {
        return false;
    }
    const uint8_t flags = frame.payload[0];
    if ((flags & SPI_OTA_ABORT) != 0) {
        if (slaveOtaActive) {
            Update.abort();
        }
        slaveOtaActive = false;
        slaveBeginPending = false;
        slaveBeginExtraLength = 0;
        slaveBytesRemaining = 0;
        return true;
    }

    bool writeOk = true;
    if ((flags & SPI_OTA_BEGIN) != 0) {
        return queueSlaveBegin(frame);
    } else {
        if (!slaveOtaActive || slaveBeginPending) {
            return false;
        }
        const uint16_t dataLength = frame.length - 1;
        if (dataLength > 0) {
            uint8_t dataBytes[SPI_MAX_PAYLOAD];
            memcpy(dataBytes, frame.payload + 1, dataLength);
            if (dataLength > slaveBytesRemaining
                || Update.write(dataBytes, dataLength) != dataLength) {
                writeOk = false;
            } else {
                slaveBytesRemaining -= dataLength;
            }
        }
    }

    if (!writeOk) {
        Update.abort();
        slaveOtaActive = false;
        slaveBytesRemaining = 0;
        return false;
    }

    if ((flags & SPI_OTA_END) == 0) {
        return true;
    }
    if (!slaveOtaActive || slaveBytesRemaining != 0) {
        Update.abort();
        slaveOtaActive = false;
        slaveBytesRemaining = 0;
        return false;
    }
    if (!Update.end(true)) {
        slaveOtaActive = false;
        return false;
    }
    slaveOtaActive = false;
    slaveRestartPending = true;
    return true;
}

bool FirmwareOta::queueSlaveBegin(const SpiFrame &frame) {
    uint32_t size = 0;
    if (!spiUnpackFirmwareOtaSize(frame.payload, frame.length, &size) || size == 0) {
        return false;
    }
    if (slaveBeginPending) {
        return slaveBeginSeq == frame.seq;
    }
    if (slaveOtaActive) {
        Update.abort();
    }
    slaveOtaActive = true;
    slaveBeginPending = true;
    slaveBeginSeq = frame.seq;
    slaveBytesRemaining = size;
    slaveBeginExtraLength =
        frame.length > SPI_OTA_BEGIN_LEN ? (uint16_t)(frame.length - SPI_OTA_BEGIN_LEN) : 0;
    if (slaveBeginExtraLength > 0) {
        memcpy(slaveBeginExtra, frame.payload + SPI_OTA_BEGIN_LEN, slaveBeginExtraLength);
    }
    LOGGER.info("Slave firmware begin accepted, erase scheduled size=" + String((unsigned long)size));
    return true;
}
