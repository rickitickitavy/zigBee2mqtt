#include "Logger.h"

Logger LOGGER;

Logger::Logger() {}

void Logger::println(const String &msg) {
#ifdef CON_DEBUG
    Serial.println(msg);
#endif
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
