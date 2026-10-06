#include "JoinedFirmwareZip.h"

#include <string.h>

namespace JoinedFirmwareZip {
namespace {

constexpr uint32_t kLocalFileHeaderSig = 0x04034b50UL;
constexpr uint32_t kCentralDirSig = 0x02014b50UL;
constexpr uint32_t kEndOfCentralDirSig = 0x06054b50UL;
constexpr uint16_t kMethodStore = 0;
constexpr uint16_t kMethodDeflate = 8;

uint16_t readU16(const uint8_t *bytes) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

uint32_t readU32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16)
        | ((uint32_t)bytes[3] << 24);
}

bool readExact(File &file, uint8_t *destination, size_t length) {
    size_t total = 0;
    while (total < length) {
        const size_t got = file.read(destination + total, length - total);
        if (got == 0) {
            return false;
        }
        total += got;
    }
    return true;
}

bool nameEquals(const char *name, size_t nameLength, const char *expected) {
    const size_t expectedLength = strlen(expected);
    if (nameLength != expectedLength) {
        return false;
    }
    return memcmp(name, expected, expectedLength) == 0;
}

bool locateEndOfCentralDirectory(File &zipFile, uint32_t *centralOffsetOut, uint16_t *entryCountOut) {
    const size_t fileSize = zipFile.size();
    if (fileSize < 22) {
        return false;
    }
    const size_t scanMax = fileSize < 65557UL ? fileSize : 65557UL;
    const size_t start = fileSize - scanMax;
    if (!zipFile.seek(start)) {
        return false;
    }
    uint8_t *window = static_cast<uint8_t *>(malloc(scanMax));
    if (window == nullptr) {
        return false;
    }
    if (!readExact(zipFile, window, scanMax)) {
        free(window);
        return false;
    }
    bool found = false;
    for (size_t i = scanMax; i >= 22; i--) {
        const size_t offset = i - 22;
        if (readU32(window + offset) != kEndOfCentralDirSig) {
            continue;
        }
        *entryCountOut = readU16(window + offset + 10);
        *centralOffsetOut = readU32(window + offset + 16);
        found = true;
        break;
    }
    free(window);
    return found;
}

bool fillMemberFromLocalHeader(File &zipFile, uint32_t localHeaderOffset, MemberInfo *memberOut) {
    uint8_t header[30];
    if (!zipFile.seek(localHeaderOffset) || !readExact(zipFile, header, sizeof(header))) {
        return false;
    }
    if (readU32(header) != kLocalFileHeaderSig) {
        return false;
    }
    const uint16_t method = readU16(header + 8);
    const uint32_t compressedSize = readU32(header + 18);
    const uint32_t uncompressedSize = readU32(header + 22);
    const uint16_t nameLength = readU16(header + 26);
    const uint16_t extraLength = readU16(header + 28);
    memberOut->localHeaderOffset = localHeaderOffset;
    memberOut->dataOffset = localHeaderOffset + 30UL + nameLength + extraLength;
    memberOut->compressedSize = compressedSize;
    memberOut->uncompressedSize = uncompressedSize;
    memberOut->method = method;
    memberOut->found = uncompressedSize > 0
        && (method == kMethodStore || method == kMethodDeflate);
    return memberOut->found;
}

}  // namespace

bool findMembers(File &zipFile, MemberInfo *slaveOut, MemberInfo *hostOut) {
    if (slaveOut == nullptr || hostOut == nullptr) {
        return false;
    }
    *slaveOut = MemberInfo{};
    *hostOut = MemberInfo{};
    uint32_t centralOffset = 0;
    uint16_t entryCount = 0;
    if (!locateEndOfCentralDirectory(zipFile, &centralOffset, &entryCount) || entryCount == 0) {
        return false;
    }
    uint32_t cursor = centralOffset;
    for (uint16_t index = 0; index < entryCount; index++) {
        uint8_t central[46];
        if (!zipFile.seek(cursor) || !readExact(zipFile, central, sizeof(central))) {
            return false;
        }
        if (readU32(central) != kCentralDirSig) {
            return false;
        }
        const uint16_t nameLength = readU16(central + 28);
        const uint16_t extraLength = readU16(central + 30);
        const uint16_t commentLength = readU16(central + 32);
        const uint32_t localHeaderOffset = readU32(central + 42);
        char name[96];
        if (nameLength >= sizeof(name)) {
            cursor += 46UL + nameLength + extraLength + commentLength;
            continue;
        }
        if (!readExact(zipFile, reinterpret_cast<uint8_t *>(name), nameLength)) {
            return false;
        }
        name[nameLength] = '\0';
        MemberInfo *target = nullptr;
        if (nameEquals(name, nameLength, "slave.bin")) {
            target = slaveOut;
        } else if (nameEquals(name, nameLength, "host.bin")) {
            target = hostOut;
        }
        if (target != nullptr) {
            if (!fillMemberFromLocalHeader(zipFile, localHeaderOffset, target)) {
                return false;
            }
        }
        cursor += 46UL + nameLength + extraLength + commentLength;
    }
    return slaveOut->found && hostOut->found;
}

bool MemberReader::open(File &zipFile, const MemberInfo &member) {
    close();
    if (!member.found || (member.method != kMethodStore && member.method != kMethodDeflate)) {
        return false;
    }
    zip = &zipFile;
    info = member;
    if (!zip->seek(info.dataOffset)) {
        zip = nullptr;
        return false;
    }
    if (info.method == kMethodDeflate) {
        dict = static_cast<uint8_t *>(malloc(TINFL_LZ_DICT_SIZE));
        if (dict == nullptr) {
            zip = nullptr;
            return false;
        }
        tinfl_init(&inflator);
    }
    openFlag = true;
    failFlag = false;
    uncompressedDone = 0;
    compressedDone = 0;
    dictOffset = 0;
    inAvail = 0;
    inConsumed = 0;
    outHoldLen = 0;
    outHoldPos = 0;
    inflateFinished = false;
    return true;
}

void MemberReader::close() {
    free(dict);
    dict = nullptr;
    zip = nullptr;
    openFlag = false;
    info = MemberInfo{};
    uncompressedDone = 0;
    compressedDone = 0;
    inAvail = 0;
    inConsumed = 0;
    outHoldLen = 0;
    outHoldPos = 0;
    inflateFinished = false;
    failFlag = false;
}

uint32_t MemberReader::remaining() const {
    if (!openFlag || uncompressedDone >= info.uncompressedSize) {
        return 0;
    }
    return info.uncompressedSize - uncompressedDone;
}

bool MemberReader::fillInput() {
    if (zip == nullptr || compressedDone >= info.compressedSize) {
        return false;
    }
    const uint32_t left = info.compressedSize - compressedDone;
    const size_t toRead = left > kInBufSize ? kInBufSize : (size_t)left;
    if (!zip->seek(info.dataOffset + compressedDone)) {
        failFlag = true;
        return false;
    }
    size_t got = 0;
    while (got < toRead) {
        const size_t n = zip->read(inBuf + got, toRead - got);
        if (n == 0) {
            failFlag = true;
            return false;
        }
        got += n;
    }
    compressedDone += (uint32_t)got;
    inAvail = got;
    inConsumed = 0;
    return true;
}

size_t MemberReader::readStore(uint8_t *destination, size_t maxLength) {
    if (zip == nullptr || destination == nullptr || maxLength == 0) {
        return 0;
    }
    const uint32_t left = remaining();
    if (left == 0) {
        return 0;
    }
    const size_t toRead = left > maxLength ? maxLength : (size_t)left;
    if (!zip->seek(info.dataOffset + uncompressedDone)) {
        failFlag = true;
        return 0;
    }
    size_t got = 0;
    while (got < toRead) {
        const size_t n = zip->read(destination + got, toRead - got);
        if (n == 0) {
            failFlag = true;
            return got;
        }
        got += n;
    }
    uncompressedDone += (uint32_t)got;
    compressedDone = uncompressedDone;
    return got;
}

size_t MemberReader::readDeflate(uint8_t *destination, size_t maxLength) {
    if (destination == nullptr || maxLength == 0 || dict == nullptr) {
        return 0;
    }
    size_t produced = 0;
    while (produced < maxLength && uncompressedDone < info.uncompressedSize) {
        if (outHoldPos < outHoldLen) {
            const size_t take = (outHoldLen - outHoldPos) < (maxLength - produced)
                ? (outHoldLen - outHoldPos)
                : (maxLength - produced);
            memcpy(destination + produced, outHold + outHoldPos, take);
            outHoldPos += take;
            produced += take;
            uncompressedDone += (uint32_t)take;
            if (outHoldPos >= outHoldLen) {
                outHoldPos = 0;
                outHoldLen = 0;
            }
            continue;
        }
        if (inflateFinished) {
            break;
        }
        if (inConsumed >= inAvail) {
            if (compressedDone >= info.compressedSize) {
                inAvail = 0;
                inConsumed = 0;
            } else if (!fillInput()) {
                return produced;
            }
        }
        const mz_uint8 *inPtr = inBuf + inConsumed;
        size_t inSize = inAvail - inConsumed;
        uint8_t *outPtr = dict + dictOffset;
        size_t outSize = TINFL_LZ_DICT_SIZE - dictOffset;
        uint32_t flags = 0;
        if (compressedDone < info.compressedSize || inSize > 0) {
            if (compressedDone < info.compressedSize) {
                flags |= TINFL_FLAG_HAS_MORE_INPUT;
            }
        }
        const tinfl_status status = tinfl_decompress(
            &inflator,
            inPtr,
            &inSize,
            dict,
            outPtr,
            &outSize,
            flags
        );
        inConsumed += inSize;
        if (outSize > 0) {
            const size_t room = maxLength - produced;
            if (outSize <= room) {
                memcpy(destination + produced, outPtr, outSize);
                produced += outSize;
                uncompressedDone += (uint32_t)outSize;
            } else {
                memcpy(destination + produced, outPtr, room);
                produced += room;
                uncompressedDone += (uint32_t)room;
                const size_t leftover = outSize - room;
                if (leftover > kOutHoldSize) {
                    failFlag = true;
                    return produced;
                }
                memcpy(outHold, outPtr + room, leftover);
                outHoldLen = leftover;
                outHoldPos = 0;
            }
            dictOffset = (dictOffset + outSize) & (TINFL_LZ_DICT_SIZE - 1);
        }
        if (status == TINFL_STATUS_DONE) {
            inflateFinished = true;
            break;
        }
        if (status == TINFL_STATUS_NEEDS_MORE_INPUT) {
            if (compressedDone >= info.compressedSize && inConsumed >= inAvail) {
                // Final inflate without HAS_MORE_INPUT.
                inSize = 0;
                outPtr = dict + dictOffset;
                outSize = TINFL_LZ_DICT_SIZE - dictOffset;
                const tinfl_status finalStatus = tinfl_decompress(
                    &inflator,
                    inPtr,
                    &inSize,
                    dict,
                    outPtr,
                    &outSize,
                    0
                );
                if (outSize > 0) {
                    const size_t room = maxLength - produced;
                    const size_t take = outSize < room ? outSize : room;
                    memcpy(destination + produced, outPtr, take);
                    produced += take;
                    uncompressedDone += (uint32_t)take;
                    if (outSize > take) {
                        const size_t leftover = outSize - take;
                        if (leftover > kOutHoldSize) {
                            failFlag = true;
                            return produced;
                        }
                        memcpy(outHold, outPtr + take, leftover);
                        outHoldLen = leftover;
                        outHoldPos = 0;
                    }
                    dictOffset = (dictOffset + outSize) & (TINFL_LZ_DICT_SIZE - 1);
                }
                if (finalStatus == TINFL_STATUS_DONE) {
                    inflateFinished = true;
                } else if (
                    finalStatus != TINFL_STATUS_HAS_MORE_OUTPUT
                    && finalStatus != TINFL_STATUS_NEEDS_MORE_INPUT
                ) {
                    failFlag = true;
                    return produced;
                }
            }
            continue;
        }
        if (status == TINFL_STATUS_HAS_MORE_OUTPUT) {
            continue;
        }
        failFlag = true;
        return produced;
    }
    return produced;
}

size_t MemberReader::read(uint8_t *destination, size_t maxLength) {
    if (!openFlag || failFlag || destination == nullptr || maxLength == 0) {
        return 0;
    }
    if (remaining() == 0) {
        return 0;
    }
    if (info.method == kMethodStore) {
        return readStore(destination, maxLength);
    }
    return readDeflate(destination, maxLength);
}

}  // namespace JoinedFirmwareZip
