#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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
    SpiCmdZclWriteAttr = 0x0D,
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
constexpr size_t SPI_DEVICE_SYNC_ENTRY_LEN_WITH_CHANNELS = SPI_DEVICE_SYNC_ENTRY_LEN_NO_CHANNELS + 1;
constexpr size_t SPI_DEVICE_SYNC_ENTRY_LEN = SPI_DEVICE_SYNC_ENTRY_LEN_WITH_CHANNELS + 1;
constexpr size_t SPI_DEVICE_MESSAGE_MAX = 64;
constexpr size_t SPI_ZCL_WRITE_ATTR_LEN = 18;
constexpr uint8_t ZCL_ATTR_TYPE_U8 = 0x20;
constexpr uint8_t ZCL_ATTR_TYPE_U16 = 0x21;
constexpr uint8_t ZCL_ATTR_TYPE_U32 = 0x23;
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

inline bool spiPackZclWriteAttr(
    uint8_t *out,
    size_t outMax,
    const uint8_t ieee[8],
    uint8_t endpoint,
    uint16_t clusterId,
    uint16_t attributeId,
    uint8_t dataType,
    uint32_t attributeValue
) {
    if (out == nullptr || ieee == nullptr || outMax < SPI_ZCL_WRITE_ATTR_LEN) {
        return false;
    }
    memcpy(out, ieee, 8);
    out[8] = endpoint;
    out[9] = (uint8_t)(clusterId & 0xFF);
    out[10] = (uint8_t)((clusterId >> 8) & 0xFF);
    out[11] = (uint8_t)(attributeId & 0xFF);
    out[12] = (uint8_t)((attributeId >> 8) & 0xFF);
    out[13] = dataType;
    out[14] = (uint8_t)(attributeValue & 0xFF);
    out[15] = (uint8_t)((attributeValue >> 8) & 0xFF);
    out[16] = (uint8_t)((attributeValue >> 16) & 0xFF);
    out[17] = (uint8_t)((attributeValue >> 24) & 0xFF);
    return true;
}

inline bool spiUnpackZclWriteAttr(
    const uint8_t *in,
    uint16_t length,
    uint8_t ieee[8],
    uint8_t *endpoint,
    uint16_t *clusterId,
    uint16_t *attributeId,
    uint8_t *dataType,
    uint32_t *attributeValue
) {
    if (in == nullptr || length < SPI_ZCL_WRITE_ATTR_LEN || ieee == nullptr || endpoint == nullptr
        || clusterId == nullptr || attributeId == nullptr || dataType == nullptr || attributeValue == nullptr) {
        return false;
    }
    memcpy(ieee, in, 8);
    *endpoint = in[8];
    *clusterId = (uint16_t)in[9] | ((uint16_t)in[10] << 8);
    *attributeId = (uint16_t)in[11] | ((uint16_t)in[12] << 8);
    *dataType = in[13];
    *attributeValue = (uint32_t)in[14] | ((uint32_t)in[15] << 8) | ((uint32_t)in[16] << 16)
        | ((uint32_t)in[17] << 24);
    return true;
}

uint16_t spiCrc16(const uint8_t *data, size_t length);
size_t spiEncodeFrame(const SpiFrame &frame, uint8_t *out, size_t outMax);
bool spiDecodeFrame(const uint8_t *in, size_t inLength, SpiFrame &frame);
