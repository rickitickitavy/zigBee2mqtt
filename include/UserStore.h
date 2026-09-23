#pragma once

#include "Defines.h"
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

struct UserRecord {
    char userName[USER_NAME_MAX];
    char passwordSaltHex[USER_PASSWORD_SALT_LEN * 2 + 1];
    char passwordHashHex[USER_PASSWORD_HASH_LEN * 2 + 1];
    uint32_t addedAt;
    bool isAdmin;
    bool editDevices;
    bool addDevices;
    bool removeDevices;
    bool editUsers;
    bool isBlocked;
    uint8_t theme;
};

enum UserWriteResult {
    UserWriteOk = 0,
    UserWriteForbidden,
    UserWriteNotFound,
    UserWriteDuplicate,
    UserWriteFull,
    UserWriteBadName,
    UserWriteNeedPassword
};

class UserStore {
public:
    UserStore();

    bool loadOrSeed();
    bool saveToFile() const;
    int userCount() const;
    UserRecord *findByName(const char *userName);
    const UserRecord *findByName(const char *userName) const;
    bool passwordMatches(const UserRecord *user, const char *password) const;
    bool setPassword(UserRecord *user, const char *password);
    UserWriteResult createUser(const UserRecord *actor, const UserRecord *source, const char *password);
    UserWriteResult updateUser(const UserRecord *actor, const char *userName, const UserRecord *source, const char *password);
    UserWriteResult deleteUser(const UserRecord *actor, const char *userName);
    bool replaceFromExportJson(const String &json);
    String listPublicJson() const;
    String listExportJson() const;
    void appendUserPublicJson(String &json, const UserRecord *user) const;
    void appendUserExportJson(String &json, const UserRecord *user) const;
    static const char *themeJsonId(uint8_t themeId);
    static uint8_t themeFromJsonId(const char *themeId);
    static bool nameEquals(const char *left, const char *right);

    const UserRecord *sessionUser(
        const char *cookieHeader,
        uint32_t nowMs,
        char *tokenHexOut,
        size_t tokenHexOutSize
    );
    bool startSession(
        const UserRecord *user,
        bool rememberMe,
        uint32_t nowMs,
        char *tokenHex,
        size_t tokenHexSize,
        uint32_t *maxAgeSec
    );
    void endSession(const char *tokenHex);
    void touchSession(const char *tokenHex, uint32_t nowMs);

private:
    UserRecord users[USER_STORE_MAX];
    int storedCount;
    struct SessionSlot {
        uint8_t token[AUTH_SESSION_TOKEN_LEN];
        char userName[USER_NAME_MAX];
        uint32_t lastActivityMs;
        uint32_t maxIdleMs;
        bool used;
    };
    SessionSlot sessions[AUTH_SESSION_MAX];

    void clearUsers();
    void seedAdmin();
    bool loadFromFile();
    bool parseUsersJson(const String &json, bool requireHashes);
    int adminCount() const;
    static void bytesToHex(const uint8_t *bytes, size_t length, char *hex);
    static bool hexToBytes(const char *hex, uint8_t *bytes, size_t length);
    static void hashPassword(const uint8_t *salt, const char *password, uint8_t *hashOut);
    static bool hashesEqual(const uint8_t *left, const uint8_t *right, size_t length);
    static bool actorMayEditUsers(const UserRecord *actor);
    static void copyRolesAndFlags(UserRecord *destination, const UserRecord *source);
    SessionSlot *findSession(const uint8_t token[AUTH_SESSION_TOKEN_LEN]);
};
