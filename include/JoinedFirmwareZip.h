#pragma once

#include <Arduino.h>
#include <FS.h>

#if __has_include(<miniz.h>)
#include <miniz.h>
#elif __has_include(<esp32/rom/miniz.h>)
#include <esp32/rom/miniz.h>
#elif __has_include("esp32/rom/miniz.h")
#include "esp32/rom/miniz.h"
#else
#include "miniz.h"
#endif

// Locate ZIP members and stream-decompress them (STORE or DEFLATE).
namespace JoinedFirmwareZip {

struct MemberInfo {
    uint32_t localHeaderOffset = 0;
    uint32_t dataOffset = 0;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint16_t method = 0;
    bool found = false;
};

bool findMembers(File &zipFile, MemberInfo *slaveOut, MemberInfo *hostOut);
// Reads optional version.txt (STORE or DEFLATE). Returns false if missing/invalid.
bool readVersionText(File &zipFile, char *destination, size_t destinationSize);

// Sequential uncompressed reader over one ZIP member.
class MemberReader {
public:
    bool open(File &zipFile, const MemberInfo &member);
    void close();
    bool isOpen() const { return openFlag; }
    uint32_t size() const { return info.uncompressedSize; }
    uint32_t remaining() const;
    // Returns bytes written to destination, or 0 on EOF/error. Check failed() after 0.
    size_t read(uint8_t *destination, size_t maxLength);
    bool failed() const { return failFlag; }

private:
    static constexpr uint16_t kMethodStore = 0;
    static constexpr uint16_t kMethodDeflate = 8;
    static constexpr size_t kInBufSize = 1024;
    static constexpr size_t kOutHoldSize = 512;

    File *zip = nullptr;
    MemberInfo info{};
    bool openFlag = false;
    bool failFlag = false;
    uint32_t uncompressedDone = 0;
    uint32_t compressedDone = 0;
    tinfl_decompressor inflator{};
    uint8_t *dict = nullptr;
    size_t dictOffset = 0;
    uint8_t inBuf[kInBufSize]{};
    size_t inAvail = 0;
    size_t inConsumed = 0;
    uint8_t outHold[kOutHoldSize]{};
    size_t outHoldLen = 0;
    size_t outHoldPos = 0;
    bool inflateFinished = false;

    bool fillInput();
    size_t readStore(uint8_t *destination, size_t maxLength);
    size_t readDeflate(uint8_t *destination, size_t maxLength);
};

}  // namespace JoinedFirmwareZip
