#include "UserStore.h"
#include "JsonField.h"
#include "Logger.h"

#include <LittleFS.h>
#include <mbedtls/sha256.h>
#include <esp_random.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

UserStore::UserStore() : storedCount(0) {
    clearUsers();
    memset(sessions, 0, sizeof(sessions));
}

void UserStore::clearUsers() {
    memset(users, 0, sizeof(users));
    storedCount = 0;
}

int UserStore::userCount() const {
    return storedCount;
}

bool UserStore::nameEquals(const char *left, const char *right) {
    if (left == nullptr || right == nullptr) {
        return false;
    }
    return strcmp(left, right) == 0;
}

const char *UserStore::themeJsonId(uint8_t themeId) {
    return themeId == UI_THEME_DARK ? "dark" : "light";
}

uint8_t UserStore::themeFromJsonId(const char *themeId) {
    if (themeId != nullptr && strcmp(themeId, "dark") == 0) {
        return UI_THEME_DARK;
    }
    return UI_THEME_LIGHT;
}

void UserStore::bytesToHex(const uint8_t *bytes, size_t length, char *hex) {
    static const char digits[] = "0123456789abcdef";
    for (size_t index = 0; index < length; index++) {
        hex[index * 2] = digits[bytes[index] >> 4];
        hex[index * 2 + 1] = digits[bytes[index] & 0x0F];
    }
    hex[length * 2] = '\0';
}

bool UserStore::hexToBytes(const char *hex, uint8_t *bytes, size_t length) {
    if (hex == nullptr || bytes == nullptr) {
        return false;
    }
    for (size_t index = 0; index < length; index++) {
        unsigned int value = 0;
        if (sscanf(hex + index * 2, "%2x", &value) != 1) {
            return false;
        }
        bytes[index] = (uint8_t)value;
    }
    return true;
}

void UserStore::hashPassword(const uint8_t *salt, const char *password, uint8_t *hashOut) {
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    mbedtls_sha256_starts(&context, 0);
    mbedtls_sha256_update(&context, salt, USER_PASSWORD_SALT_LEN);
    if (password != nullptr) {
        mbedtls_sha256_update(&context, (const unsigned char *)password, strlen(password));
    }
    mbedtls_sha256_finish(&context, hashOut);
    mbedtls_sha256_free(&context);
}

bool UserStore::hashesEqual(const uint8_t *left, const uint8_t *right, size_t length) {
    uint8_t mix = 0;
    for (size_t index = 0; index < length; index++) {
        mix = (uint8_t)(mix | (left[index] ^ right[index]));
    }
    return mix == 0;
}

bool UserStore::actorMayEditUsers(const UserRecord *actor) {
    return actor != nullptr && (actor->isAdmin || actor->editUsers);
}

void UserStore::copyRolesAndFlags(UserRecord *destination, const UserRecord *source) {
    destination->isAdmin = source->isAdmin;
    destination->editDevices = source->editDevices;
    destination->addDevices = source->addDevices;
    destination->removeDevices = source->removeDevices;
    destination->editUsers = source->editUsers;
    destination->isBlocked = source->isBlocked;
    destination->theme = source->theme == UI_THEME_DARK ? UI_THEME_DARK : UI_THEME_LIGHT;
}

UserRecord *UserStore::findByName(const char *userName) {
    return const_cast<UserRecord *>(static_cast<const UserStore *>(this)->findByName(userName));
}

const UserRecord *UserStore::findByName(const char *userName) const {
    if (userName == nullptr || userName[0] == '\0') {
        return nullptr;
    }
    for (int index = 0; index < storedCount; index++) {
        if (nameEquals(users[index].userName, userName)) {
            return &users[index];
        }
    }
    return nullptr;
}

int UserStore::adminCount() const {
    int count = 0;
    for (int index = 0; index < storedCount; index++) {
        if (users[index].isAdmin) {
            count++;
        }
    }
    return count;
}

bool UserStore::setPassword(UserRecord *user, const char *password) {
    if (user == nullptr || password == nullptr || password[0] == '\0') {
        return false;
    }
    uint8_t salt[USER_PASSWORD_SALT_LEN];
    uint8_t hash[USER_PASSWORD_HASH_LEN];
    esp_fill_random(salt, sizeof(salt));
    hashPassword(salt, password, hash);
    bytesToHex(salt, sizeof(salt), user->passwordSaltHex);
    bytesToHex(hash, sizeof(hash), user->passwordHashHex);
    return true;
}

bool UserStore::passwordMatches(const UserRecord *user, const char *password) const {
    if (user == nullptr || password == nullptr) {
        return false;
    }
    uint8_t salt[USER_PASSWORD_SALT_LEN];
    uint8_t storedHash[USER_PASSWORD_HASH_LEN];
    uint8_t computedHash[USER_PASSWORD_HASH_LEN];
    if (!hexToBytes(user->passwordSaltHex, salt, sizeof(salt))) {
        return false;
    }
    if (!hexToBytes(user->passwordHashHex, storedHash, sizeof(storedHash))) {
        return false;
    }
    hashPassword(salt, password, computedHash);
    return hashesEqual(storedHash, computedHash, sizeof(storedHash));
}

void UserStore::seedAdmin() {
    clearUsers();
    UserRecord *admin = &users[0];
    strncpy(admin->userName, "admin", sizeof(admin->userName) - 1);
    admin->isAdmin = true;
    admin->theme = UI_THEME_DARK;
    admin->isBlocked = false;
    time_t wallClock = time(nullptr);
    admin->addedAt = wallClock > 1600000000 ? (uint32_t)wallClock : 0;
    setPassword(admin, "admin");
    storedCount = 1;
    saveToFile();
    LOGGER.info("Seeded console user admin");
}

bool UserStore::saveToFile() const {
    File file = LittleFS.open(USERS_STORE_PATH, "w");
    if (!file) {
        LOGGER.error("Users file write failed");
        return false;
    }
    const String json = listExportJson();
    const size_t written = file.print(json);
    file.close();
    return written == json.length();
}

bool UserStore::parseUsersJson(const String &json, bool requireHashes) {
    clearUsers();
    const char *cursor = json.c_str();
    while (cursor != nullptr && *cursor != '\0' && storedCount < USER_STORE_MAX) {
        const char *objectStart = strchr(cursor, '{');
        if (objectStart == nullptr) {
            break;
        }
        const char *objectEnd = strchr(objectStart, '}');
        if (objectEnd == nullptr) {
            break;
        }
        String object = String(objectStart).substring(0, (unsigned int)(objectEnd - objectStart + 1));
        String userName;
        String saltHex;
        String hashHex;
        String themeText;
        extractJsonString(object.c_str(), "userName", userName);
        extractJsonString(object.c_str(), "passwordSalt", saltHex);
        extractJsonString(object.c_str(), "passwordHash", hashHex);
        extractJsonString(object.c_str(), "theme", themeText);
        if (userName.length() == 0 || userName.length() >= USER_NAME_MAX) {
            cursor = objectEnd + 1;
            continue;
        }
        if (findByName(userName.c_str()) != nullptr) {
            cursor = objectEnd + 1;
            continue;
        }
        if (requireHashes && (saltHex.length() != USER_PASSWORD_SALT_LEN * 2 || hashHex.length() != USER_PASSWORD_HASH_LEN * 2)) {
            cursor = objectEnd + 1;
            continue;
        }
        UserRecord *user = &users[storedCount];
        memset(user, 0, sizeof(*user));
        strncpy(user->userName, userName.c_str(), sizeof(user->userName) - 1);
        strncpy(user->passwordSaltHex, saltHex.c_str(), sizeof(user->passwordSaltHex) - 1);
        strncpy(user->passwordHashHex, hashHex.c_str(), sizeof(user->passwordHashHex) - 1);
        int addedAt = 0;
        extractJsonInt(object.c_str(), "addedAt", addedAt);
        user->addedAt = addedAt > 0 ? (uint32_t)addedAt : 0;
        extractJsonBool(object.c_str(), "isAdmin", user->isAdmin);
        extractJsonBool(object.c_str(), "editDevices", user->editDevices);
        extractJsonBool(object.c_str(), "addDevices", user->addDevices);
        extractJsonBool(object.c_str(), "removeDevices", user->removeDevices);
        extractJsonBool(object.c_str(), "editUsers", user->editUsers);
        extractJsonBool(object.c_str(), "isBlocked", user->isBlocked);
        user->theme = themeFromJsonId(themeText.c_str());
        storedCount++;
        cursor = objectEnd + 1;
    }
    return storedCount > 0;
}

bool UserStore::loadFromFile() {
    File file = LittleFS.open(USERS_STORE_PATH, "r");
    if (!file) {
        return false;
    }
    String json = file.readString();
    file.close();
    json.trim();
    if (json.length() == 0 || json.charAt(0) != '[') {
        return false;
    }
    return parseUsersJson(json, true);
}

bool UserStore::loadOrSeed() {
    if (loadFromFile()) {
        LOGGER.info("Loaded " + String(storedCount) + " console user(s)");
        return true;
    }
    seedAdmin();
    return storedCount > 0;
}

bool UserStore::replaceFromExportJson(const String &json) {
    UserStore scratch;
    if (!scratch.parseUsersJson(json, true) || scratch.adminCount() < 1) {
        return false;
    }
    memcpy(users, scratch.users, sizeof(users));
    storedCount = scratch.storedCount;
    return saveToFile();
}

UserWriteResult UserStore::createUser(const UserRecord *actor, const UserRecord *source, const char *password) {
    if (!actorMayEditUsers(actor) || source == nullptr) {
        return UserWriteForbidden;
    }
    if (source->userName[0] == '\0') {
        return UserWriteBadName;
    }
    if (source->isAdmin && (actor == nullptr || !actor->isAdmin)) {
        return UserWriteForbidden;
    }
    if (findByName(source->userName) != nullptr) {
        return UserWriteDuplicate;
    }
    if (storedCount >= USER_STORE_MAX) {
        return UserWriteFull;
    }
    if (password == nullptr || password[0] == '\0') {
        return UserWriteNeedPassword;
    }
    UserRecord *user = &users[storedCount];
    memset(user, 0, sizeof(*user));
    strncpy(user->userName, source->userName, sizeof(user->userName) - 1);
    copyRolesAndFlags(user, source);
    if (!actor->isAdmin) {
        user->isAdmin = false;
    }
    time_t wallClock = time(nullptr);
    user->addedAt = wallClock > 1600000000 ? (uint32_t)wallClock : 0;
    if (!setPassword(user, password)) {
        return UserWriteNeedPassword;
    }
    storedCount++;
    saveToFile();
    return UserWriteOk;
}

UserWriteResult UserStore::updateUser(
    const UserRecord *actor,
    const char *userName,
    const UserRecord *source,
    const char *password
) {
    if (!actorMayEditUsers(actor) || source == nullptr) {
        return UserWriteForbidden;
    }
    UserRecord *user = findByName(userName);
    if (user == nullptr) {
        return UserWriteNotFound;
    }
    if (user->isAdmin && !actor->isAdmin) {
        return UserWriteForbidden;
    }
    if (source->isAdmin && !actor->isAdmin) {
        return UserWriteForbidden;
    }
    if (user->isAdmin && !source->isAdmin && adminCount() <= 1) {
        return UserWriteForbidden;
    }
    copyRolesAndFlags(user, source);
    if (!actor->isAdmin) {
        user->isAdmin = false;
    }
    if (password != nullptr && password[0] != '\0') {
        if (!setPassword(user, password)) {
            return UserWriteNeedPassword;
        }
    }
    saveToFile();
    return UserWriteOk;
}

UserWriteResult UserStore::deleteUser(const UserRecord *actor, const char *userName) {
    if (!actorMayEditUsers(actor)) {
        return UserWriteForbidden;
    }
    int foundIndex = -1;
    for (int index = 0; index < storedCount; index++) {
        if (nameEquals(users[index].userName, userName)) {
            foundIndex = index;
            break;
        }
    }
    if (foundIndex < 0) {
        return UserWriteNotFound;
    }
    if (users[foundIndex].isAdmin && !actor->isAdmin) {
        return UserWriteForbidden;
    }
    if (users[foundIndex].isAdmin && adminCount() <= 1) {
        return UserWriteForbidden;
    }
    if (foundIndex < storedCount - 1) {
        memmove(&users[foundIndex], &users[foundIndex + 1], sizeof(UserRecord) * (storedCount - foundIndex - 1));
    }
    storedCount--;
    memset(&users[storedCount], 0, sizeof(UserRecord));
    saveToFile();
    return UserWriteOk;
}

void UserStore::appendUserPublicJson(String &json, const UserRecord *user) const {
    json += "{\"userName\":\"";
    appendJsonEscaped(json, user->userName, sizeof(user->userName));
    json += "\",\"addedAt\":";
    json += String((unsigned long)user->addedAt);
    json += ",\"isAdmin\":";
    json += user->isAdmin ? "true" : "false";
    json += ",\"editDevices\":";
    json += user->editDevices ? "true" : "false";
    json += ",\"addDevices\":";
    json += user->addDevices ? "true" : "false";
    json += ",\"removeDevices\":";
    json += user->removeDevices ? "true" : "false";
    json += ",\"editUsers\":";
    json += user->editUsers ? "true" : "false";
    json += ",\"isBlocked\":";
    json += user->isBlocked ? "true" : "false";
    json += ",\"theme\":\"";
    json += themeJsonId(user->theme);
    json += "\"}";
}

void UserStore::appendUserExportJson(String &json, const UserRecord *user) const {
    json += "{\"userName\":\"";
    appendJsonEscaped(json, user->userName, sizeof(user->userName));
    json += "\",\"addedAt\":";
    json += String((unsigned long)user->addedAt);
    json += ",\"isAdmin\":";
    json += user->isAdmin ? "true" : "false";
    json += ",\"editDevices\":";
    json += user->editDevices ? "true" : "false";
    json += ",\"addDevices\":";
    json += user->addDevices ? "true" : "false";
    json += ",\"removeDevices\":";
    json += user->removeDevices ? "true" : "false";
    json += ",\"editUsers\":";
    json += user->editUsers ? "true" : "false";
    json += ",\"isBlocked\":";
    json += user->isBlocked ? "true" : "false";
    json += ",\"theme\":\"";
    json += themeJsonId(user->theme);
    json += "\",\"passwordSalt\":\"";
    json += user->passwordSaltHex;
    json += "\",\"passwordHash\":\"";
    json += user->passwordHashHex;
    json += "\"}";
}

String UserStore::listPublicJson() const {
    String json = "[";
    for (int index = 0; index < storedCount; index++) {
        if (index > 0) {
            json += ",";
        }
        appendUserPublicJson(json, &users[index]);
    }
    json += "]";
    return json;
}

String UserStore::listExportJson() const {
    String json = "[";
    for (int index = 0; index < storedCount; index++) {
        if (index > 0) {
            json += ",";
        }
        appendUserExportJson(json, &users[index]);
    }
    json += "]";
    return json;
}

UserStore::SessionSlot *UserStore::findSession(const uint8_t token[AUTH_SESSION_TOKEN_LEN]) {
    for (int index = 0; index < AUTH_SESSION_MAX; index++) {
        if (sessions[index].used && memcmp(sessions[index].token, token, AUTH_SESSION_TOKEN_LEN) == 0) {
            return &sessions[index];
        }
    }
    return nullptr;
}

bool UserStore::startSession(
    const UserRecord *user,
    bool rememberMe,
    uint32_t nowMs,
    char *tokenHex,
    size_t tokenHexSize,
    uint32_t *maxAgeSec
) {
    if (user == nullptr || user->isBlocked || tokenHex == nullptr || tokenHexSize < AUTH_SESSION_TOKEN_LEN * 2 + 1) {
        return false;
    }
    int slotIndex = -1;
    for (int index = 0; index < AUTH_SESSION_MAX; index++) {
        if (!sessions[index].used) {
            slotIndex = index;
            break;
        }
    }
    if (slotIndex < 0) {
        slotIndex = 0;
    }
    SessionSlot *slot = &sessions[slotIndex];
    memset(slot, 0, sizeof(*slot));
    esp_fill_random(slot->token, sizeof(slot->token));
    strncpy(slot->userName, user->userName, sizeof(slot->userName) - 1);
    slot->lastActivityMs = nowMs;
    if (user->isAdmin) {
        slot->maxIdleMs = AUTH_ADMIN_IDLE_MS;
        if (maxAgeSec != nullptr) {
            *maxAgeSec = AUTH_ADMIN_IDLE_MS / 1000UL;
        }
    } else if (rememberMe) {
        slot->maxIdleMs = AUTH_REMEMBER_MS;
        if (maxAgeSec != nullptr) {
            *maxAgeSec = AUTH_REMEMBER_MS / 1000UL;
        }
    } else {
        slot->maxIdleMs = AUTH_SESSION_MS;
        if (maxAgeSec != nullptr) {
            *maxAgeSec = 0;
        }
    }
    slot->used = true;
    bytesToHex(slot->token, sizeof(slot->token), tokenHex);
    return true;
}

void UserStore::endSession(const char *tokenHex) {
    uint8_t token[AUTH_SESSION_TOKEN_LEN];
    if (tokenHex == nullptr || !hexToBytes(tokenHex, token, sizeof(token))) {
        return;
    }
    SessionSlot *slot = findSession(token);
    if (slot != nullptr) {
        memset(slot, 0, sizeof(*slot));
    }
}

void UserStore::touchSession(const char *tokenHex, uint32_t nowMs) {
    uint8_t token[AUTH_SESSION_TOKEN_LEN];
    if (tokenHex == nullptr || !hexToBytes(tokenHex, token, sizeof(token))) {
        return;
    }
    SessionSlot *slot = findSession(token);
    if (slot != nullptr) {
        slot->lastActivityMs = nowMs;
    }
}

const UserRecord *UserStore::sessionUser(
    const char *cookieHeader,
    uint32_t nowMs,
    char *tokenHexOut,
    size_t tokenHexOutSize
) {
    if (cookieHeader == nullptr) {
        return nullptr;
    }
    const char *found = strstr(cookieHeader, AUTH_COOKIE_NAME "=");
    if (found == nullptr) {
        return nullptr;
    }
    found += strlen(AUTH_COOKIE_NAME "=");
    char tokenHex[AUTH_SESSION_TOKEN_LEN * 2 + 1];
    size_t hexIndex = 0;
    while (hexIndex < AUTH_SESSION_TOKEN_LEN * 2 && found[hexIndex] != '\0' && found[hexIndex] != ';') {
        tokenHex[hexIndex] = found[hexIndex];
        hexIndex++;
    }
    tokenHex[hexIndex] = '\0';
    uint8_t token[AUTH_SESSION_TOKEN_LEN];
    if (hexIndex != AUTH_SESSION_TOKEN_LEN * 2 || !hexToBytes(tokenHex, token, sizeof(token))) {
        return nullptr;
    }
    SessionSlot *slot = findSession(token);
    if (slot == nullptr) {
        return nullptr;
    }
    if ((uint32_t)(nowMs - slot->lastActivityMs) > slot->maxIdleMs) {
        memset(slot, 0, sizeof(*slot));
        return nullptr;
    }
    const UserRecord *user = findByName(slot->userName);
    if (user == nullptr || user->isBlocked) {
        memset(slot, 0, sizeof(*slot));
        return nullptr;
    }
    slot->lastActivityMs = nowMs;
    if (tokenHexOut != nullptr && tokenHexOutSize > hexIndex) {
        strncpy(tokenHexOut, tokenHex, tokenHexOutSize - 1);
        tokenHexOut[tokenHexOutSize - 1] = '\0';
    }
    return user;
}
