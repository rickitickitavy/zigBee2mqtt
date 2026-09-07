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

void Logger::copyRing(String &destination) const {
    destination = "";
    destination.reserve(used + 1);
    const size_t start = used < kRingSize ? 0 : writePos;
    for (size_t i = 0; i < used; i++) {
        destination += ring[(start + i) % kRingSize];
    }
}

void Logger::appendSlaveLine(const char *line) {
    if (line == nullptr || line[0] == '\0') {
        return;
    }
    appendRing(String(line));
#ifdef CON_DEBUG
    Serial.println(line);
#endif
}

void Logger::println(const String &msg) {
    char timestamp[24];
    formatTimestamp(timestamp, sizeof(timestamp));
    String line = String(timestamp) + " [" + roleLabel + "] " + msg;
    appendRing(line);
#ifdef CON_DEBUG
    Serial.println(line);
#endif
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
