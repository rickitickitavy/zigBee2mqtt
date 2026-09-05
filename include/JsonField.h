#pragma once

#include <Arduino.h>
#include <string.h>

inline bool extractJsonString(const char *json, const char *key, String &out) {
    String pattern = String("\"") + key + "\"";
    const char *found = strstr(json, pattern.c_str());
    if (found == nullptr) {
        return false;
    }
    const char *colon = strchr(found + pattern.length(), ':');
    if (colon == nullptr) {
        return false;
    }
    const char *quote = strchr(colon, '"');
    if (quote == nullptr) {
        return false;
    }
    quote++;
    const char *end = strchr(quote, '"');
    if (end == nullptr) {
        return false;
    }
    out = String(quote).substring(0, (unsigned int)(end - quote));
    return true;
}
