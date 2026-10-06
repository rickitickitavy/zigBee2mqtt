#pragma once

#include <Arduino.h>
#include "Defines.h"

class Logger {
public:
    using LineHookFn = void (*)(const char *line);

    static const size_t kRingCapacityTarget = 512 * 1024;
    static const size_t kWebViewMaxBytes = 64 * 1024;
    static const size_t kInternalFallbackBytes = 64 * 1024;

    char logLevel = LOG_LEVEL;

    Logger();

    void begin();
    void setRoleLabel(const char *label);
    void setStoreRing(bool enabled);
    void setLineHook(LineHookFn hook);
    void appendSlaveLine(const char *line);

    void error(String msg);
    void warning(String msg);
    void info(String msg);
    void debug(String msg);
    void snapshotRing(size_t *start, size_t *length) const;
    void snapshotRingTail(size_t maxBytes, size_t *start, size_t *length) const;
    size_t copyRingSlice(size_t start, size_t length, size_t offset, char *destination, size_t maxLength) const;
    void writeRing(Print &out) const;
    size_t ringCapacityBytes() const { return ringCapacity; }

private:
    char *ring = nullptr;
    size_t ringCapacity = 0;
    size_t writePos = 0;
    size_t used = 0;
    const char *roleLabel = "host";
    bool storeRing = true;
    LineHookFn lineHook = nullptr;

    void println(const String &msg);
    void appendRing(const String &msg);
    void formatTimestamp(char *buffer, size_t bufferSize) const;
};

extern Logger LOGGER;
