#pragma once

#include <Arduino.h>
#include <string.h>

inline void appendJsonEscaped(String &json, const char *value, size_t maxLength = 128) {
    if (value == nullptr) {
        return;
    }
    for (size_t index = 0; index < maxLength; index++) {
        const unsigned char character = (unsigned char)value[index];
        if (character == '\0' || character == 0xFF) {
            break;
        }
        if (character == '"' || character == '\\') {
            json += '\\';
            json += (char)character;
            continue;
        }
        if (character < 32 || character >= 127) {
            continue;
        }
        json += (char)character;
    }
}

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

inline bool extractJsonKeyedSlice(const char *json, const char *key, char openBrace, String &out) {
    if (json == nullptr || key == nullptr) {
        return false;
    }
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
    while (*colon == ' ' || *colon == '\t' || *colon == '\n' || *colon == '\r') {
        colon++;
    }
    if (*colon != openBrace) {
        return false;
    }
    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (const char *cursor = colon; *cursor != '\0'; cursor++) {
        const char character = *cursor;
        if (inString) {
            if (escape) {
                escape = false;
                continue;
            }
            if (character == '\\') {
                escape = true;
                continue;
            }
            if (character == '"') {
                inString = false;
            }
            continue;
        }
        if (character == '"') {
            inString = true;
            continue;
        }
        if (character == '{' || character == '[') {
            depth++;
            continue;
        }
        if (character == '}' || character == ']') {
            depth--;
            if (depth == 0) {
                out = String(colon).substring(0, (unsigned int)(cursor - colon + 1));
                return true;
            }
        }
    }
    return false;
}
