#include "Logger.h"

#include <time.h>
#include <string.h>

Logger LOGGER;

Logger::Logger() {}

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
    if (!storeRing) {
        return;
    }
    const size_t length = msg.length();
    for (size_t i = 0; i < length; i++) {
        ring[writePos] = msg[i];
        writePos = (writePos + 1) % kRingSize;
        if (used < kRingSize) {
            used++;
        }
    }
    ring[writePos] = '\n';
    writePos = (writePos + 1) % kRingSize;
    if (used < kRingSize) {
        used++;
    }
}

void Logger::snapshotRing(size_t *start, size_t *length) const {
    if (length != nullptr) {
        *length = used;
    }
    if (start != nullptr) {
        *start = used < kRingSize ? 0 : writePos;
    }
}

size_t Logger::copyRingSlice(
    size_t start,
    size_t length,
    size_t offset,
    char *destination,
    size_t maxLength
) const {
    if (destination == nullptr || offset >= length || maxLength == 0) {
        return 0;
    }
    const size_t remaining = length - offset;
    const size_t toCopy = remaining < maxLength ? remaining : maxLength;
    const size_t physical = (start + offset) % kRingSize;
    const size_t firstRun = kRingSize - physical;
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
