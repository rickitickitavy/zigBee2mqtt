#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include "JoinedFirmwareZip.h"
#include "SpiProtocol.h"

class FirmwareOta {
public:
    enum class Phase : uint8_t {
        Idle = 0,
        Receiving = 1,
        Preparing = 2,
        Slave = 3,
        VerifyingSlave = 4,
        Host = 5,
        Failed = 6,
        Done = 7,
        Rebooting = 8
    };

    static constexpr const char *kPackagePath = "/ota/package.zip";
    static constexpr uint8_t kMaxSlaveFrameFails = 5;
    static constexpr uint8_t kSlaveVerifyMaxAttempts = 3;
    static constexpr unsigned long kSlaveVerifyWaitMs = 15000UL;

    bool busy() const;
    bool isUpdatingSlave() const;
    bool isApplyingImage() const;
    bool slaveBeginAcked() const;
    bool isLastSlaveFrameEnd() const;
    Phase phase() const;
    String statusJson() const;

    bool beginStaging(size_t contentLength);
    bool writeStaging(const uint8_t *data, size_t length);
    bool finishStaging();
    void abortStaging();

    void pump();
    void onSpiFrame(const SpiFrame &frame);

    bool consumeFirmwareRestart();
    bool consumeHostRestart();

    bool handleSlaveFrame(const SpiFrame &frame);
    bool queueSlaveBegin(const SpiFrame &frame);
    void onSlaveSpiTransferDone();

    using SpiClockHzFn = uint32_t (*)();
    void setSpiClockRestoreHandler(SpiClockHzFn handler);

private:
    Phase currentPhase = Phase::Idle;
    String errorText;
    File stagingFile;
    File packageFile;
    JoinedFirmwareZip::MemberInfo slaveMember{};
    JoinedFirmwareZip::MemberInfo hostMember{};
    JoinedFirmwareZip::MemberReader memberReader{};
    uint8_t *slaveImageRam = nullptr;
    uint32_t slaveImageRamSize = 0;
    bool hasSlaveImage = false;
    bool hasHostImage = false;
    uint32_t packageSize = 0;
    uint32_t slaveImageSize = 0;
    uint32_t hostImageSize = 0;
    uint32_t imageSize = 0;
    uint32_t imageOffset = 0;
    uint32_t streamPos = 0;
    bool waitingForSlaveResult = false;
    bool beginQueued = false;
    bool beginAcked = false;
    bool hostUpdateStarted = false;
    bool slaveRestartPending = false;
    bool hostRestartPending = false;
    bool slaveOtaActive = false;
    bool packagePreparePending = false;
    bool slaveApplyPending = false;
    bool slaveApplyArmed = false;
    uint8_t slaveApplyWaitTransfers = 0;
    uint32_t slaveBytesRemaining = 0;
    uint32_t receivedBytes = 0;
    unsigned long rebootReadyMs = 0;
    bool slaveBeginPending = false;
    uint8_t slaveBeginSeq = 0;
    uint16_t slaveBeginExtraLength = 0;
    uint8_t slaveBeginExtra[SPI_MAX_PAYLOAD]{};
    uint8_t lastSlavePayload[SPI_MAX_PAYLOAD]{};
    uint16_t lastSlavePayloadLength = 0;
    uint8_t lastSlaveSeq = 0;
    uint32_t lastSlaveDataBytes = 0;
    uint8_t slaveFrameFails = 0;
    volatile bool slavePackClaimed = false;
    SpiClockHzFn spiClockRestoreFn = nullptr;
    bool spiClockOverrideActive = false;
    char expectedSlaveVersion[SPI_STATUS_VERSION_MAX + 1]{};
    char preOtaSlaveVersion[SPI_STATUS_VERSION_MAX + 1]{};
    uint8_t slaveVerifyAttempt = 0;
    unsigned long slaveVerifyDeadlineMs = 0;
    bool slaveVerifyResetPending = false;
    bool slaveAwaitingLinkForReupload = false;

    void fail(const char *message);
    void clearStagingFiles();
    void closePackageStream();
    void freeSlaveImageRam();
    bool loadSlaveImageToRam();
    bool packSlaveChunkFromStream(uint8_t *payloadOut, uint16_t *packedLengthOut, uint32_t *dataBytesOut);
    bool enqueueNextSlaveChunk();
    bool sendLastSlaveFrame(bool isResend);
    void onSlaveFrameLost();
    bool prepareJoinedPackage();
    void startSlaveUploadCycle();
    void beginSlaveVersionVerify();
    void pumpSlaveVersionVerify();
    bool slaveVersionMatchesExpected() const;
    bool startHostApply();
    void finishSlaveOnlySuccess();
    void pumpHostApply();
    void pumpSlaveBegin();
    void pumpSlaveApply();
    void armSlaveApplyAfterAckDrain();
    void pumpPackagePrepare();
    void beginSlaveSpiClockOverride();
    void endSlaveSpiClockOverride();
    const char *phaseId() const;
    uint8_t percentOf(uint32_t done, uint32_t total) const;
};

extern FirmwareOta FIRMWARE_OTA;
