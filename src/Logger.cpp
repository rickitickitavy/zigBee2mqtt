#include "Logger.h"

#include <esp_heap_caps.h>
#include <time.h>
#include <string.h>

#if defined(BOARD_ROLE_HOST)
#include <esp32-hal-psram.h>
#endif

Logger LOGGER;

Logger::Logger() {}

void Logger::begin() {
    if (ring != nullptr) {
        return;
    }
#if defined(BOARD_ROLE_HOST)
    if (psramFound()) {
        ring = static_cast<char *>(ps_malloc(kRingCapacityTarget));
        if (ring != nullptr) {
            ringCapacity = kRingCapacityTarget;
            memset(ring, 0, ringCapacity);
            return;
        }
    }
#endif
    ring = static_cast<char *>(
        heap_caps_malloc(kRingCapacityTarget, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (ring == nullptr) {
        ring = static_cast<char *>(heap_caps_malloc(kRingCapacityTarget, MALLOC_CAP_SPIRAM));
    }
    if (ring != nullptr) {
        ringCapacity = kRingCapacityTarget;
        memset(ring, 0, ringCapacity);
        return;
    }
    ring = static_cast<char *>(
        heap_caps_malloc(kInternalFallbackBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
    );
    if (ring != nullptr) {
        ringCapacity = kInternalFallbackBytes;
        memset(ring, 0, ringCapacity);
    }
}

void Logger::setRoleLabel(const char *label) {
    if (label != nullptr && label[0] != '\0') {
        roleLabel = label;
    }
}

void Logger::setStoreRing(bool enabled) {
    storeRing = enabled;
}

void Logger::setLineHook(LineHookFn hook) {
    lineHook = hook;
}

void Logger::formatTimestamp(char *buffer, size_t bufferSize) const {
    time_t now = time(nullptr);
    struct tm timeInfo;
    bool haveWallClock = false;
    if (now > 1700000000 && localtime_r(&now, &timeInfo) != nullptr) {
        haveWallClock = true;
    }
    if (haveWallClock) {
        snprintf(
            buffer,
            bufferSize,
            "%04d-%02d-%02d %02d:%02d:%02d",
            timeInfo.tm_year + 1900,
            timeInfo.tm_mon + 1,
            timeInfo.tm_mday,
            timeInfo.tm_hour,
            timeInfo.tm_min,
            timeInfo.tm_sec
        );
        return;
    }
    const unsigned long totalSec = millis() / 1000UL;
    const unsigned long hours = (totalSec / 3600UL) % 24UL;
    const unsigned long minutes = (totalSec / 60UL) % 60UL;
    const unsigned long seconds = totalSec % 60UL;
    snprintf(
        buffer,
        bufferSize,
        "1970-01-01 %02lu:%02lu:%02lu",
        hours,
        minutes,
        seconds
    );
}

void Logger::appendRing(const String &msg) {
    if (!storeRing || ring == nullptr || ringCapacity == 0) {
        return;
    }
    const size_t length = msg.length();
    for (size_t i = 0; i < length; i++) {
        ring[writePos] = msg[i];
        writePos = (writePos + 1) % ringCapacity;
        if (used < ringCapacity) {
            used++;
        }
    }
    ring[writePos] = '\n';
    writePos = (writePos + 1) % ringCapacity;
    if (used < ringCapacity) {
        used++;
    }
}

void Logger::snapshotRing(size_t *start, size_t *length) const {
    if (length != nullptr) {
        *length = used;
    }
    if (start != nullptr) {
        *start = (ringCapacity == 0 || used < ringCapacity) ? 0 : writePos;
    }
}

void Logger::snapshotRingTail(size_t maxBytes, size_t *start, size_t *length) const {
    size_t fullStart = 0;
    size_t fullLength = 0;
    snapshotRing(&fullStart, &fullLength);
    if (maxBytes > 0 && fullLength > maxBytes && ringCapacity > 0) {
        fullStart = (fullStart + (fullLength - maxBytes)) % ringCapacity;
        fullLength = maxBytes;
    }
    if (start != nullptr) {
        *start = fullStart;
    }
    if (length != nullptr) {
        *length = fullLength;
    }
}

size_t Logger::copyRingSlice(
    size_t start,
    size_t length,
    size_t offset,
    char *destination,
    size_t maxLength
) const {
    if (ring == nullptr || ringCapacity == 0 || destination == nullptr || offset >= length || maxLength == 0) {
        return 0;
    }
    const size_t remaining = length - offset;
    const size_t toCopy = remaining < maxLength ? remaining : maxLength;
    const size_t physical = (start + offset) % ringCapacity;
    const size_t firstRun = ringCapacity - physical;
    if (toCopy <= firstRun) {
        memcpy(destination, ring + physical, toCopy);
        return toCopy;
    }
    memcpy(destination, ring + physical, firstRun);
    memcpy(destination + firstRun, ring, toCopy - firstRun);
    return toCopy;
}

void Logger::writeRing(Print &out) const {
    size_t start = 0;
    size_t length = 0;
    snapshotRing(&start, &length);
    char chunk[256];
    size_t offset = 0;
    while (offset < length) {
        const size_t copied = copyRingSlice(start, length, offset, chunk, sizeof(chunk));
        if (copied == 0) {
            break;
        }
        out.write(reinterpret_cast<const uint8_t *>(chunk), copied);
        offset += copied;
    }
}

void Logger::appendSlaveLine(const char *line) {
    if (line == nullptr || line[0] == '\0') {
        return;
    }
    appendRing(String(line));
    Serial.println(line);
}

void Logger::println(const String &msg) {
    char timestamp[24];
    formatTimestamp(timestamp, sizeof(timestamp));
    String line = String(timestamp) + " [" + roleLabel + "] " + msg;
    appendRing(line);
    Serial.println(line);
    if (lineHook != nullptr) {
        lineHook(line.c_str());
    }
}

void Logger::error(String msg) {
    if (logLevel <= LOG_LEVEL_ERROR) {
        println("ERROR: " + msg);
    }
}

void Logger::warning(String msg) {
    if (logLevel <= LOG_LEVEL_WARNING) {
        println("WARNING: " + msg);
    }
}

void Logger::debug(String msg) {
    if (logLevel <= LOG_LEVEL_DEBUG) {
        println("DEBUG: " + msg);
    }
}

void Logger::info(String msg) {
    if (logLevel <= LOG_LEVEL_INFO) {
        println("INFO: " + msg);
    }
}
