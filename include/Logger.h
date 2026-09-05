#pragma once

#include <Arduino.h>
#include "Defines.h"

class Logger {
public:
    char logLevel = LOG_LEVEL;

    Logger();

    void error(String msg);
    void warning(String msg);
    void info(String msg);
    void debug(String msg);

private:
    void println(const String &msg);
};

extern Logger LOGGER;
