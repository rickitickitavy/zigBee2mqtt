#pragma once

#include <Arduino.h>
#include "Defines.h"

class Logger {
public:
    using LineHookFn = void (*)(const char *line);

    char logLevel = LOG_LEVEL;

    Logger();

    void setRoleLabel(const char *label);
    void setStoreRing(bool enabled);
    void setLineHook(LineHookFn hook);
    void appendSlaveLine(const char *line);

    void error(String msg);
    void warning(String msg);
    void info(String msg);
    void debug(String msg);
    void copyRing(String &destination) const;

private:
    static const size_t kRingSize = 65536;

    char ring[kRingSize]{};
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
