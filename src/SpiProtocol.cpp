#include "SpiProtocol.h"

#include <string.h>

uint16_t spiCrc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            if ((crc & 0x8000) != 0) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

size_t spiEncodeFrame(const SpiFrame &frame, uint8_t *out, size_t outMax) {
    if (frame.length > SPI_MAX_PAYLOAD) {
        return 0;
    }
    const size_t total = SPI_FRAME_HEADER + frame.length + SPI_FRAME_CRC;
    if (out == nullptr || outMax < total) {
        return 0;
    }
    out[0] = SPI_FRAME_SYNC;
    out[1] = SPI_FRAME_VERSION;
    out[2] = frame.cmd;
    out[3] = frame.seq;
    out[4] = (uint8_t)(frame.length & 0xFF);
    out[5] = (uint8_t)((frame.length >> 8) & 0xFF);
    if (frame.length > 0) {
        memcpy(out + SPI_FRAME_HEADER, frame.payload, frame.length);
    }
    const uint16_t crc = spiCrc16(out, SPI_FRAME_HEADER + frame.length);
    out[SPI_FRAME_HEADER + frame.length] = (uint8_t)(crc & 0xFF);
    out[SPI_FRAME_HEADER + frame.length + 1] = (uint8_t)((crc >> 8) & 0xFF);
    return total;
}

bool spiDecodeFrame(const uint8_t *in, size_t inLength, SpiFrame &frame) {
    if (in == nullptr || inLength < SPI_FRAME_HEADER + SPI_FRAME_CRC) {
        return false;
    }
    if (in[0] != SPI_FRAME_SYNC || in[1] != SPI_FRAME_VERSION) {
        return false;
    }
    const uint16_t length = (uint16_t)in[4] | ((uint16_t)in[5] << 8);
    if (length > SPI_MAX_PAYLOAD) {
        return false;
    }
    const size_t total = SPI_FRAME_HEADER + length + SPI_FRAME_CRC;
    if (inLength < total) {
        return false;
    }
    const uint16_t crcRead = (uint16_t)in[SPI_FRAME_HEADER + length]
        | ((uint16_t)in[SPI_FRAME_HEADER + length + 1] << 8);
    const uint16_t crcCalc = spiCrc16(in, SPI_FRAME_HEADER + length);
    if (crcRead != crcCalc) {
        return false;
    }
    frame.cmd = in[2];
    frame.seq = in[3];
    frame.length = length;
    if (length > 0) {
        memcpy(frame.payload, in + SPI_FRAME_HEADER, length);
    }
    return true;
}
