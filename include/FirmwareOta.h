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
        Host = 4,
        Failed = 5,
        Done = 6,
        Rebooting = 7
    };

    static constexpr const char *kPackagePath = "/ota/package.zip";
    static constexpr uint8_t kMaxSlaveFrameFails = 5;

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

private:
    Phase currentPhase = Phase::Idle;
    String errorText;
    File stagingFile;
    File packageFile;
    JoinedFirmwareZip::MemberInfo slaveMember{};
    JoinedFirmwareZip::MemberInfo hostMember{};
    JoinedFirmwareZip::MemberReader memberReader{};
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

    void fail(const char *message);
    void clearStagingFiles();
    void closePackageStream();
    bool packSlaveChunkFromStream(uint8_t *payloadOut, uint16_t *packedLengthOut, uint32_t *dataBytesOut);
    bool enqueueNextSlaveChunk();
    bool sendLastSlaveFrame(bool isResend);
    void onSlaveFrameLost();
    bool prepareJoinedPackage();
    bool startHostApply();
    void pumpHostApply();
    void pumpSlaveBegin();
    void pumpSlaveApply();
    void armSlaveApplyAfterAckDrain();
    void pumpPackagePrepare();
    const char *phaseId() const;
    uint8_t percentOf(uint32_t done, uint32_t total) const;
};

extern FirmwareOta FIRMWARE_OTA;
