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

inline bool extractJsonBool(const char *json, const char *key, bool &out) {
    String pattern = String("\"") + key + "\"";
    const char *found = strstr(json, pattern.c_str());
    if (found == nullptr) {
        return false;
    }
    const char *colon = strchr(found + pattern.length(), ':');
    if (colon == nullptr) {
        return false;
    }
    colon++;
    while (*colon == ' ' || *colon == '\t') {
        colon++;
    }
    if (strncmp(colon, "true", 4) == 0) {
        out = true;
        return true;
    }
    if (strncmp(colon, "false", 5) == 0) {
        out = false;
        return true;
    }
    return false;
}

inline bool extractJsonInt(const char *json, const char *key, int &out) {
    String pattern = String("\"") + key + "\"";
    const char *found = strstr(json, pattern.c_str());
    if (found == nullptr) {
        return false;
    }
    const char *colon = strchr(found + pattern.length(), ':');
    if (colon == nullptr) {
        return false;
    }
    colon++;
    while (*colon == ' ' || *colon == '\t') {
        colon++;
    }
    if (*colon == '"') {
        colon++;
    }
    if (*colon != '-' && (*colon < '0' || *colon > '9')) {
        return false;
    }
    out = atoi(colon);
    return true;
}
