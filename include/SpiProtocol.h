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
    SpiCmdSetDevice = 0x0A,
    SpiCmdGetDevices = 0x0B,
    SpiCmdGetDevicesFile = 0x0C,
    SpiCmdReadEvent = 0x10
};

constexpr uint8_t SPI_DEVICE_SYNC_RESET = 0x01;
constexpr uint8_t SPI_DEVICE_SYNC_LAST = 0x02;
constexpr uint8_t SPI_DEVICE_SYNC_ENTRY = 0x04;
constexpr uint8_t SPI_DEVICE_SYNC_ALLOW_EMPTY = 0x08;
constexpr uint8_t SPI_DEVICE_SYNC_DELETE = 0x10;
constexpr size_t SPI_DEVICE_SYNC_NAME_LEN = 24;
constexpr size_t SPI_DEVICE_SYNC_TOPIC_LEN = 64;
constexpr size_t SPI_DEVICE_SYNC_ENTRY_LEN_NO_CHANNELS =
    1 + 8 + SPI_DEVICE_SYNC_NAME_LEN + (SPI_DEVICE_SYNC_TOPIC_LEN * 3);
constexpr size_t SPI_DEVICE_SYNC_ENTRY_LEN = SPI_DEVICE_SYNC_ENTRY_LEN_NO_CHANNELS + 1;
constexpr size_t SPI_DEVICE_MESSAGE_MAX = 64;
constexpr uint8_t SPI_FILE_FIRST = 0x01;
constexpr uint8_t SPI_FILE_LAST = 0x02;
constexpr size_t SPI_FILE_CHUNK_MAX = SPI_MAX_PAYLOAD - 1;
static_assert(SPI_DEVICE_SYNC_ENTRY_LEN <= SPI_MAX_PAYLOAD, "device sync frame must fit SPI payload");

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
    SpiEvtDeviceMap = 0x8A,
    SpiEvtDevicesFile = 0x8B,
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
