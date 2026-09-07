#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

constexpr uint8_t SPI_FRAME_SYNC = 0xA5;
constexpr uint8_t SPI_FRAME_VERSION = 1;
constexpr size_t SPI_MAX_PAYLOAD = 256;
constexpr size_t SPI_FRAME_HEADER = 6;
constexpr size_t SPI_FRAME_CRC = 2;
constexpr size_t SPI_MAX_FRAME = SPI_FRAME_HEADER + SPI_MAX_PAYLOAD + SPI_FRAME_CRC;

enum SpiCommand : uint8_t {
    SpiCmdPing = 0x01,
    SpiCmdGetStatus = 0x02,
    SpiCmdSetSettings = 0x03,
    SpiCmdTimeSync = 0x04,
    SpiCmdClearLog = 0x05,
    SpiCmdPermitJoin = 0x06,
    SpiCmdLeave = 0x07,
    SpiCmdZclOnOff = 0x08,
    SpiCmdResetRadio = 0x09,
    SpiCmdReadEvent = 0x10
};

enum SpiEvent : uint8_t {
    SpiEvtPong = 0x81,
    SpiEvtStatus = 0x82,
    SpiEvtSettingsOk = 0x83,
    SpiEvtLogRecord = 0x84,
    SpiEvtSlaveReady = 0x85,
    SpiEvtDeviceJoin = 0x86,
    SpiEvtDeviceLeave = 0x87,
    SpiEvtAttrReport = 0x88,
    SpiEvtCmdResult = 0x89,
    SpiEvtErr = 0x8E,
    SpiEvtTimeout = 0x8F
};

enum HostBringupState : uint8_t {
    HostBringupReset = 0,
    HostBringupWaitReady = 1,
    HostBringupPushSettings = 2,
    HostBringupNormal = 3
};

struct SpiFrame {
    uint8_t cmd = 0;
    uint8_t seq = 0;
    uint16_t length = 0;
    uint8_t payload[SPI_MAX_PAYLOAD]{};
};

uint16_t spiCrc16(const uint8_t *data, size_t length);
size_t spiEncodeFrame(const SpiFrame &frame, uint8_t *out, size_t outMax);
bool spiDecodeFrame(const uint8_t *in, size_t inLength, SpiFrame &frame);
