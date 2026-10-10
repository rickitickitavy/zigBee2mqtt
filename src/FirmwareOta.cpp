#include "FirmwareOta.h"
#include "Defines.h"
#include "JoinedFirmwareZip.h"
#include "InterChipHost.h"
#include "InterChipSlave.h"
#include "Logger.h"

#include <Update.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/semphr.h>

FirmwareOta FIRMWARE_OTA;

static portMUX_TYPE gFirmwareOtaMux = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t gPackageFsMutex = nullptr;

static void ensurePackageFsMutex() {
    if (gPackageFsMutex != nullptr) {
        return;
    }
    // Created on loop before Slave SPI transfer; later SPI fail paths only take it.
    gPackageFsMutex = xSemaphoreCreateRecursiveMutex();
}

static void takePackageFs() {
    ensurePackageFsMutex();
    if (gPackageFsMutex != nullptr) {
        xSemaphoreTakeRecursive(gPackageFsMutex, portMAX_DELAY);
    }
}

static void givePackageFs() {
    if (gPackageFsMutex != nullptr) {
        xSemaphoreGiveRecursive(gPackageFsMutex);
    }
}

bool FirmwareOta::busy() const {
    return currentPhase == Phase::Receiving || currentPhase == Phase::Preparing
        || currentPhase == Phase::Slave || currentPhase == Phase::VerifyingSlave
        || currentPhase == Phase::Host || currentPhase == Phase::Rebooting;
}

bool FirmwareOta::isUpdatingSlave() const {
    return currentPhase == Phase::Slave;
}

bool FirmwareOta::isApplyingImage() const {
    return slaveOtaActive || slaveBeginPending || slaveApplyPending || slaveApplyArmed;
}

bool FirmwareOta::slaveBeginAcked() const {
    return beginAcked;
}

bool FirmwareOta::isLastSlaveFrameEnd() const {
    return lastSlavePayloadLength > 0 && (lastSlavePayload[0] & SPI_OTA_END) != 0;
}

FirmwareOta::Phase FirmwareOta::phase() const {
    return currentPhase;
}

const char *FirmwareOta::phaseId() const {
    switch (currentPhase) {
        case Phase::Receiving:
            return "receiving";
        case Phase::Preparing:
            return "preparing";
        case Phase::Slave:
            return "slave";
        case Phase::VerifyingSlave:
            return "verifying_slave";
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
        uploadPercent = percentOf(receivedBytes, packageSize);
    } else if (currentPhase != Phase::Idle && currentPhase != Phase::Failed) {
        uploadPercent = 100;
    }
    if (!hasSlaveImage
        && (currentPhase == Phase::Host || currentPhase == Phase::Rebooting
            || currentPhase == Phase::Done)) {
        slavePercent = 100;
    } else if (currentPhase == Phase::Slave) {
        slavePercent = percentOf(imageOffset, slaveImageSize);
    } else if (
        currentPhase == Phase::VerifyingSlave || currentPhase == Phase::Host
        || currentPhase == Phase::Rebooting || currentPhase == Phase::Done
    ) {
        slavePercent = 100;
    }
    if (!hasHostImage
        && (currentPhase == Phase::VerifyingSlave || currentPhase == Phase::Done
            || (currentPhase == Phase::Slave && hasSlaveImage))) {
        hostPercent = 100;
    } else if (currentPhase == Phase::Host) {
        hostPercent = percentOf(imageOffset, hostImageSize);
    } else if (currentPhase == Phase::Rebooting || currentPhase == Phase::Done) {
        hostPercent = 100;
    }
    if (currentPhase == Phase::Failed && packageSize > 0) {
        uploadPercent = percentOf(receivedBytes > 0 ? receivedBytes : imageOffset, packageSize);
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
    json += ",\"hasSlave\":";
    json += hasSlaveImage ? "true" : "false";
    json += ",\"hasHost\":";
    json += hasHostImage ? "true" : "false";
    const bool needsReboot = currentPhase == Phase::Rebooting || hostRestartPending
        || (currentPhase == Phase::Done && hasHostImage && hostUpdateStarted);
    json += ",\"needsReboot\":";
    json += needsReboot ? "true" : "false";
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

void FirmwareOta::freeSlaveImageRam() {
    if (slaveImageRam != nullptr) {
        free(slaveImageRam);
        slaveImageRam = nullptr;
    }
    slaveImageRamSize = 0;
}

void FirmwareOta::closePackageStream() {
    takePackageFs();
    memberReader.close();
    if (packageFile) {
        packageFile.close();
    }
    givePackageFs();
    freeSlaveImageRam();
}

bool FirmwareOta::loadSlaveImageToRam() {
    freeSlaveImageRam();
    if (slaveImageSize == 0 || !memberReader.isOpen()) {
        return false;
    }
    uint8_t *buffer = static_cast<uint8_t *>(
        heap_caps_malloc(slaveImageSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (buffer == nullptr) {
        buffer = static_cast<uint8_t *>(malloc(slaveImageSize));
    }
    if (buffer == nullptr) {
        return false;
    }
    size_t got = 0;
    while (got < slaveImageSize) {
        const size_t n = memberReader.read(buffer + got, slaveImageSize - got);
        if (n == 0 || memberReader.failed()) {
            free(buffer);
            return false;
        }
        got += n;
        yield();
    }
    memberReader.close();
    slaveImageRam = buffer;
    slaveImageRamSize = slaveImageSize;
    return true;
}

void FirmwareOta::setSpiClockRestoreHandler(SpiClockHzFn handler) {
    spiClockRestoreFn = handler;
}

void FirmwareOta::beginSlaveSpiClockOverride() {
    if (spiClockOverrideActive) {
        return;
    }
    spiClockOverrideActive = true;
    INTER_CHIP_HOST.setClockHz(SPI_SPEED_HZ_4M);
    LOGGER.info("SPI clock temporarily 4 MHz for slave firmware OTA");
}

void FirmwareOta::endSlaveSpiClockOverride() {
    if (!spiClockOverrideActive) {
        return;
    }
    spiClockOverrideActive = false;
    uint32_t restoreHz = DEFAULT_SPI_SPEED_HZ;
    if (spiClockRestoreFn != nullptr) {
        restoreHz = spiClockRestoreFn();
    }
    INTER_CHIP_HOST.setClockHz(restoreHz);
    LOGGER.info("SPI clock restored after slave firmware OTA");
}

void FirmwareOta::fail(const char *message) {
    errorText = message != nullptr ? message : "Firmware update failed";
    const bool abortUpdate = hostUpdateStarted || slaveOtaActive;
    currentPhase = Phase::Failed;
    waitingForSlaveResult = false;
    beginQueued = false;
    beginAcked = false;
    hostUpdateStarted = false;
    packagePreparePending = false;
    rebootReadyMs = 0;
    if (stagingFile) {
        stagingFile.close();
    }
    closePackageStream();
    if (abortUpdate) {
        Update.abort();
    }
    slaveOtaActive = false;
    slaveBeginPending = false;
    slaveApplyPending = false;
    slaveApplyArmed = false;
    slaveApplyWaitTransfers = 0;
    slaveBeginExtraLength = 0;
    slaveBytesRemaining = 0;
    lastSlavePayloadLength = 0;
    lastSlaveDataBytes = 0;
    streamPos = 0;
    slaveFrameFails = 0;
    slavePackClaimed = false;
    slaveVerifyAttempt = 0;
    slaveVerifyDeadlineMs = 0;
    slaveVerifyResetPending = false;
    slaveAwaitingLinkForReupload = false;
    endSlaveSpiClockOverride();
    clearStagingFiles();
    LOGGER.error(errorText);
}

void FirmwareOta::clearStagingFiles() {
    closePackageStream();
    if (stagingFile) {
        stagingFile.close();
    }
    if (LittleFS.exists(kPackagePath)) {
        LittleFS.remove(kPackagePath);
    }
}

bool FirmwareOta::beginStaging(size_t contentLength) {
    if (busy()) {
        return false;
    }
    errorText = "";
    packageSize = (uint32_t)contentLength;
    slaveImageSize = 0;
    hostImageSize = 0;
    imageSize = 0;
    imageOffset = 0;
    streamPos = 0;
    receivedBytes = 0;
    waitingForSlaveResult = false;
    beginQueued = false;
    beginAcked = false;
    hostUpdateStarted = false;
    hostRestartPending = false;
    packagePreparePending = false;
    rebootReadyMs = 0;
    hasSlaveImage = false;
    hasHostImage = false;
    slaveMember = JoinedFirmwareZip::MemberInfo{};
    hostMember = JoinedFirmwareZip::MemberInfo{};
    LittleFS.mkdir("/ota");
    if (contentLength > 0) {
        const size_t freeBytes = LittleFS.totalBytes() - LittleFS.usedBytes();
        if (freeBytes < contentLength + 4096) {
            fail("Not enough filesystem space for firmware");
            return false;
        }
    }
    clearStagingFiles();
    stagingFile = LittleFS.open(kPackagePath, "w");
    if (!stagingFile) {
        fail("Could not create firmware staging file");
        return false;
    }
    currentPhase = Phase::Receiving;
    LOGGER.info("Firmware staging start (joined ZIP)");
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

bool FirmwareOta::prepareJoinedPackage() {
    ensurePackageFsMutex();
    closePackageStream();
    takePackageFs();
    packageFile = LittleFS.open(kPackagePath, "r");
    if (!packageFile) {
        givePackageFs();
        fail("Joined package missing");
        return false;
    }
    if (!JoinedFirmwareZip::findMembers(packageFile, &slaveMember, &hostMember)) {
        givePackageFs();
        closePackageStream();
        fail("Joined package needs slave.bin and/or host.bin");
        return false;
    }
    hasSlaveImage = slaveMember.found;
    hasHostImage = hostMember.found;
    expectedSlaveVersion[0] = '\0';
    slaveImageSize = hasSlaveImage ? slaveMember.uncompressedSize : 0;
    hostImageSize = hasHostImage ? hostMember.uncompressedSize : 0;
    imageOffset = 0;
    streamPos = 0;
    if (hasSlaveImage) {
        if (!JoinedFirmwareZip::readVersionText(
                packageFile,
                expectedSlaveVersion,
                sizeof(expectedSlaveVersion)
            )) {
            LOGGER.warning("Joined package has no version.txt; will require slave version change after OTA");
        }
        if (!memberReader.open(packageFile, slaveMember)) {
            givePackageFs();
            closePackageStream();
            fail("Failed to open slave.bin stream");
            return false;
        }
        imageSize = slaveImageSize;
        // Prefetch slave image into PSRAM so hostSpi can pack without LittleFS.
        if (!loadSlaveImageToRam()) {
            givePackageFs();
            closePackageStream();
            fail("Failed to load slave.bin into RAM");
            return false;
        }
    } else {
        imageSize = hostImageSize;
    }
    givePackageFs();
    LOGGER.info(
        "Joined package OK slave="
        + (hasSlaveImage ? String((unsigned long)slaveImageSize) : String("skip")) + " host="
        + (hasHostImage ? String((unsigned long)hostImageSize) : String("skip")) + " expect="
        + (expectedSlaveVersion[0] != '\0' ? expectedSlaveVersion : "(any-new)")
    );
    return true;
}

bool FirmwareOta::finishStaging() {
    if (currentPhase != Phase::Receiving || !stagingFile) {
        return false;
    }
    stagingFile.close();
    stagingFile = File();
    packageSize = 0;
    {
        File sized = LittleFS.open(kPackagePath, "r");
        if (!sized) {
            fail("Firmware staging file missing");
            return false;
        }
        packageSize = (uint32_t)sized.size();
        sized.close();
    }
    if (packageSize == 0) {
        fail("Firmware image is empty");
        return false;
    }
    lastSlavePayloadLength = 0;
    lastSlaveDataBytes = 0;
    slaveFrameFails = 0;
    packagePreparePending = true;
    currentPhase = Phase::Preparing;
    LOGGER.info("Firmware staging done, preparing joined package stream");
    return true;
}

void FirmwareOta::abortStaging() {
    if (stagingFile) {
        stagingFile.close();
    }
    if (currentPhase == Phase::Receiving || currentPhase == Phase::Preparing) {
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
        "SPI firmware frame lost seq=" + String((int)lastSlaveSeq) + " fail="
        + String((int)slaveFrameFails) + "/" + String((int)kMaxSlaveFrameFails)
    );
    if (slaveFrameFails >= kMaxSlaveFrameFails) {
        // Last frame often times out while slave finalizes flash; image may already be complete.
        const bool imageComplete = imageOffset + lastSlaveDataBytes >= slaveImageSize;
        if (beginAcked && imageComplete) {
            imageOffset = slaveImageSize;
            lastSlavePayloadLength = 0;
            lastSlaveDataBytes = 0;
            INTER_CHIP_HOST.noteKeepaliveQuiet();
            LOGGER.info("Slave END ACK lost after complete image; verifying slave version");
            beginSlaveVersionVerify();
            return;
        }
        fail("Slave firmware update failed after repeated SPI frame loss");
        return;
    }
    sendLastSlaveFrame(true);
}

bool FirmwareOta::packSlaveChunkFromStream(
    uint8_t *payloadOut,
    uint16_t *packedLengthOut,
    uint32_t *dataBytesOut
) {
    if (payloadOut == nullptr || packedLengthOut == nullptr || dataBytesOut == nullptr) {
        return false;
    }
    if (slaveImageRam == nullptr || slaveImageRamSize == 0 || slaveImageRamSize != slaveImageSize) {
        fail("Slave image RAM missing");
        return false;
    }
    if (streamPos >= slaveImageSize) {
        *packedLengthOut = 0;
        *dataBytesOut = 0;
        return true;
    }
    uint8_t chunk[SPI_FILE_CHUNK_MAX];
    const uint32_t remaining = slaveImageSize - streamPos;
    const size_t toRead = remaining > SPI_FILE_CHUNK_MAX ? SPI_FILE_CHUNK_MAX : (size_t)remaining;
    memcpy(chunk, slaveImageRam + streamPos, toRead);
    uint8_t flags = 0;
    if (streamPos + (uint32_t)toRead >= slaveImageSize) {
        flags = SPI_OTA_END;
    }
    const uint16_t packedLength =
        spiPackFirmwareOta(payloadOut, SPI_MAX_PAYLOAD, flags, chunk, (uint16_t)toRead);
    if (packedLength == 0) {
        fail("Firmware OTA pack failed");
        return false;
    }
    streamPos += (uint32_t)toRead;
    *packedLengthOut = packedLength;
    *dataBytesOut = (uint32_t)toRead;
    return true;
}

bool FirmwareOta::enqueueNextSlaveChunk() {
    if (waitingForSlaveResult) {
        return true;
    }
    if (!INTER_CHIP_HOST.isNormal()) {
        fail("Slave link is not ready");
        return false;
    }

    portENTER_CRITICAL(&gFirmwareOtaMux);
    if (waitingForSlaveResult || slavePackClaimed) {
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        return true;
    }
    if (lastSlavePayloadLength > 0) {
        const bool isResend = slaveFrameFails > 0;
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        return sendLastSlaveFrame(isResend);
    }
    slavePackClaimed = true;
    const bool doBegin = !beginQueued;
    if (doBegin) {
        beginQueued = true;
    }
    portEXIT_CRITICAL(&gFirmwareOtaMux);

    uint8_t payload[SPI_MAX_PAYLOAD];
    uint16_t packedLength = 0;
    uint32_t dataBytes = 0;
    if (doBegin) {
        uint8_t sizeBytes[4];
        sizeBytes[0] = (uint8_t)(slaveImageSize & 0xFF);
        sizeBytes[1] = (uint8_t)((slaveImageSize >> 8) & 0xFF);
        sizeBytes[2] = (uint8_t)((slaveImageSize >> 16) & 0xFF);
        sizeBytes[3] = (uint8_t)((slaveImageSize >> 24) & 0xFF);
        packedLength = spiPackFirmwareOta(payload, sizeof(payload), SPI_OTA_BEGIN, sizeBytes, 4);
        LOGGER.info("Slave firmware begin queued; waiting for erase");
        if (packedLength == 0) {
            portENTER_CRITICAL(&gFirmwareOtaMux);
            slavePackClaimed = false;
            beginQueued = false;
            portEXIT_CRITICAL(&gFirmwareOtaMux);
            fail("Firmware OTA pack failed");
            return false;
        }
    } else if (!packSlaveChunkFromStream(payload, &packedLength, &dataBytes)) {
        portENTER_CRITICAL(&gFirmwareOtaMux);
        slavePackClaimed = false;
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        return false;
    }

    if (packedLength == 0) {
        portENTER_CRITICAL(&gFirmwareOtaMux);
        slavePackClaimed = false;
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        beginSlaveVersionVerify();
        return true;
    }

    portENTER_CRITICAL(&gFirmwareOtaMux);
    memcpy(lastSlavePayload, payload, packedLength);
    lastSlavePayloadLength = packedLength;
    lastSlaveDataBytes = dataBytes;
    slavePackClaimed = false;
    portEXIT_CRITICAL(&gFirmwareOtaMux);
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
    portENTER_CRITICAL(&gFirmwareOtaMux);
    imageOffset += lastSlaveDataBytes;
    lastSlavePayloadLength = 0;
    lastSlaveDataBytes = 0;
    const bool done = imageOffset >= slaveImageSize;
    portEXIT_CRITICAL(&gFirmwareOtaMux);
    if (done) {
        beginSlaveVersionVerify();
        return;
    }
    enqueueNextSlaveChunk();
}

bool FirmwareOta::slaveVersionMatchesExpected() const {
    if (!INTER_CHIP_HOST.isNormal()) {
        return false;
    }
    const char *seen = INTER_CHIP_HOST.slaveFirmwareVersion();
    if (seen == nullptr || seen[0] == '\0') {
        return false;
    }
    if (expectedSlaveVersion[0] != '\0') {
        return strcmp(seen, expectedSlaveVersion) == 0;
    }
    if (preOtaSlaveVersion[0] == '\0') {
        return false;
    }
    return strcmp(seen, preOtaSlaveVersion) != 0;
}

void FirmwareOta::startSlaveUploadCycle() {
    waitingForSlaveResult = false;
    beginQueued = false;
    beginAcked = false;
    imageSize = slaveImageSize;
    imageOffset = 0;
    streamPos = 0;
    lastSlavePayloadLength = 0;
    lastSlaveDataBytes = 0;
    lastSlaveSeq = 0;
    slaveFrameFails = 0;
    slavePackClaimed = false;
    slaveVerifyResetPending = false;
    slaveAwaitingLinkForReupload = false;
    currentPhase = Phase::Slave;
    beginSlaveSpiClockOverride();
    INTER_CHIP_HOST.holdForFirmwareOta();
    INTER_CHIP_HOST.noteKeepaliveQuiet();
}

void FirmwareOta::beginSlaveVersionVerify() {
    endSlaveSpiClockOverride();
    waitingForSlaveResult = false;
    lastSlavePayloadLength = 0;
    lastSlaveDataBytes = 0;
    slaveVerifyResetPending = false;
    slaveAwaitingLinkForReupload = false;
    if (slaveVerifyAttempt == 0) {
        slaveVerifyAttempt = 1;
    }
    slaveVerifyDeadlineMs = millis() + kSlaveVerifyWaitMs;
    currentPhase = Phase::VerifyingSlave;
    INTER_CHIP_HOST.noteKeepaliveQuiet();
    LOGGER.info(
        "Slave image transferred; waiting up to 15s for version "
        + String(expectedSlaveVersion[0] != '\0' ? expectedSlaveVersion : "(changed)")
        + " (attempt " + String((int)slaveVerifyAttempt) + "/"
        + String((int)kSlaveVerifyMaxAttempts) + ")"
    );
}

void FirmwareOta::pumpSlaveVersionVerify() {
    if (currentPhase != Phase::VerifyingSlave) {
        return;
    }
    if (slaveVerifyResetPending) {
        slaveVerifyResetPending = false;
        INTER_CHIP_HOST.resetSlaveSynchronous();
        slaveAwaitingLinkForReupload = true;
        slaveVerifyDeadlineMs = millis() + kSlaveVerifyWaitMs;
        LOGGER.info(
            "Slave reset before re-upload attempt "
            + String((int)slaveVerifyAttempt) + "/" + String((int)kSlaveVerifyMaxAttempts)
        );
        return;
    }
    if (slaveAwaitingLinkForReupload) {
        if (slaveVersionMatchesExpected()) {
            slaveAwaitingLinkForReupload = false;
            LOGGER.info(
                "Slave version verified: " + String(INTER_CHIP_HOST.slaveFirmwareVersion())
            );
            if (hasHostImage) {
                startHostApply();
            } else {
                finishSlaveOnlySuccess();
            }
            return;
        }
        if (INTER_CHIP_HOST.isNormal()) {
            LOGGER.info(
                "Re-uploading slave firmware attempt "
                + String((int)slaveVerifyAttempt) + "/" + String((int)kSlaveVerifyMaxAttempts)
            );
            startSlaveUploadCycle();
            return;
        }
        if ((long)(millis() - slaveVerifyDeadlineMs) < 0) {
            return;
        }
        fail("Slave link not ready for firmware re-upload");
        return;
    }
    if (slaveVersionMatchesExpected()) {
        LOGGER.info(
            "Slave version verified: " + String(INTER_CHIP_HOST.slaveFirmwareVersion())
        );
        if (hasHostImage) {
            startHostApply();
        } else {
            finishSlaveOnlySuccess();
        }
        return;
    }
    if ((long)(millis() - slaveVerifyDeadlineMs) < 0) {
        return;
    }
    if (slaveVerifyAttempt >= kSlaveVerifyMaxAttempts) {
        fail("Slave did not report expected firmware version after transfer");
        return;
    }
    slaveVerifyAttempt++;
    slaveVerifyResetPending = true;
    LOGGER.warning(
        "Slave version not ready (seen=\""
        + String(INTER_CHIP_HOST.slaveFirmwareVersion())
        + "\"); will reset and re-upload"
    );
}

void FirmwareOta::finishSlaveOnlySuccess() {
    endSlaveSpiClockOverride();
    freeSlaveImageRam();
    closePackageStream();
    clearStagingFiles();
    currentPhase = Phase::Done;
    LOGGER.info("Slave-only firmware update complete (host unchanged)");
}

bool FirmwareOta::startHostApply() {
    endSlaveSpiClockOverride();
    freeSlaveImageRam();
    if (!hasHostImage) {
        fail("Joined package has no host.bin");
        return false;
    }
    if (!packageFile) {
        packageFile = LittleFS.open(kPackagePath, "r");
        if (!packageFile) {
            fail("Joined package missing for host stream");
            return false;
        }
        if (!JoinedFirmwareZip::findMembers(packageFile, &slaveMember, &hostMember)
            || !hostMember.found) {
            closePackageStream();
            fail("Joined package lost host.bin");
            return false;
        }
        hasHostImage = hostMember.found;
        hasSlaveImage = slaveMember.found;
    }
    memberReader.close();
    if (!memberReader.open(packageFile, hostMember)) {
        closePackageStream();
        fail("Failed to open host.bin stream");
        return false;
    }
    hostImageSize = hostMember.uncompressedSize;
    if (hostImageSize == 0) {
        closePackageStream();
        fail("Host image is empty");
        return false;
    }
    Update.abort();
    if (!Update.begin(hostImageSize, U_FLASH)) {
        closePackageStream();
        fail("Host OTA begin failed");
        return false;
    }
    hostUpdateStarted = true;
    currentPhase = Phase::Host;
    imageSize = hostImageSize;
    imageOffset = 0;
    INTER_CHIP_HOST.noteKeepaliveQuiet();
    LOGGER.info(
        hasSlaveImage ? "Slave firmware committed, streaming host update"
                      : "Host-only package, streaming host update"
    );
    return true;
}

void FirmwareOta::pumpHostApply() {
    if (!hostUpdateStarted || !memberReader.isOpen()) {
        return;
    }
    uint8_t chunk[1024];
    const size_t remaining = hostImageSize - imageOffset;
    const size_t toRead = remaining > sizeof(chunk) ? sizeof(chunk) : remaining;
    const size_t readLength = memberReader.read(chunk, toRead);
    if (readLength != toRead || memberReader.failed()) {
        fail("Host OTA stream read failed");
        return;
    }
    if (Update.write(chunk, readLength) != readLength) {
        fail("Host OTA write failed");
        return;
    }
    imageOffset += (uint32_t)readLength;
    if (imageOffset < hostImageSize) {
        return;
    }
    if (!Update.end(true)) {
        fail("Host OTA end failed");
        return;
    }
    clearStagingFiles();
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

void FirmwareOta::pumpPackagePrepare() {
    if (!packagePreparePending || currentPhase != Phase::Preparing) {
        return;
    }
    packagePreparePending = false;
    if (!prepareJoinedPackage()) {
        return;
    }
    if (!hasSlaveImage) {
        LOGGER.info("Joined package has no slave.bin; updating host only");
        startHostApply();
        return;
    }
    strncpy(preOtaSlaveVersion, INTER_CHIP_HOST.slaveFirmwareVersion(), sizeof(preOtaSlaveVersion) - 1);
    preOtaSlaveVersion[sizeof(preOtaSlaveVersion) - 1] = '\0';
    slaveVerifyAttempt = 1;
    startSlaveUploadCycle();
    LOGGER.info(
        hasHostImage ? "Joined package stream ready, updating slave then host"
                     : "Joined package stream ready, updating slave only"
    );
}

void FirmwareOta::pump() {
    pumpSlaveBegin();
    pumpSlaveApply();
    pumpPackagePrepare();
    if (currentPhase == Phase::Slave) {
        enqueueNextSlaveChunk();
        return;
    }
    if (currentPhase == Phase::VerifyingSlave) {
        pumpSlaveVersionVerify();
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
        if (slaveOtaActive || slaveApplyPending || slaveApplyArmed) {
            Update.abort();
        }
        slaveOtaActive = false;
        slaveBeginPending = false;
        slaveApplyPending = false;
        slaveApplyArmed = false;
        slaveApplyWaitTransfers = 0;
        slaveBeginExtraLength = 0;
        slaveBytesRemaining = 0;
        return true;
    }

    bool writeOk = true;
    if ((flags & SPI_OTA_BEGIN) != 0) {
        return queueSlaveBegin(frame);
    }
    if (!slaveOtaActive || slaveBeginPending) {
        return false;
    }
    const uint16_t dataLength = frame.length - 1;
    if (dataLength > 0) {
        uint8_t dataBytes[SPI_MAX_PAYLOAD];
        memcpy(dataBytes, frame.payload + 1, dataLength);
        if (dataLength > slaveBytesRemaining || Update.write(dataBytes, dataLength) != dataLength) {
            writeOk = false;
        } else {
            slaveBytesRemaining -= dataLength;
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
    // Write is done; ACK must leave SPI before Update.end (flash lock kills replies).
    slaveOtaActive = false;
    // Arm after caller queues the ACK; wait one more completed transfer to clock it out.
    slaveApplyArmed = true;
    slaveApplyPending = false;
    slaveApplyWaitTransfers = 1;
    return true;
}

void FirmwareOta::armSlaveApplyAfterAckDrain() {
    // Kept for call sites that re-arm after reset; END path sets waitTransfers itself.
    if (!INTER_CHIP_SLAVE.hasOutboundPending()) {
        slaveApplyArmed = false;
        slaveApplyPending = true;
        slaveApplyWaitTransfers = 0;
        return;
    }
    slaveApplyArmed = true;
    slaveApplyPending = false;
    slaveApplyWaitTransfers = 1;
}

void FirmwareOta::onSlaveSpiTransferDone() {
    if (!slaveApplyArmed) {
        return;
    }
    if (slaveApplyWaitTransfers > 0) {
        slaveApplyWaitTransfers--;
        return;
    }
    slaveApplyArmed = false;
    slaveApplyPending = true;
}

void FirmwareOta::pumpSlaveApply() {
    if (!slaveApplyPending) {
        return;
    }
    slaveApplyPending = false;
    if (!Update.end(true)) {
        LOGGER.error("Slave firmware Update.end failed");
        Update.abort();
        return;
    }
    slaveRestartPending = true;
    LOGGER.info("Slave firmware image committed");
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
