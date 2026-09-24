#include "UserStore.h"
#include "JsonField.h"
#include "Logger.h"

#include <LittleFS.h>
#include <mbedtls/sha256.h>
#include <esp_random.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static_assert(USER_NAME_MAX == SPI_USER_SYNC_NAME_LEN, "user name SPI width must match store");
static_assert(USER_PASSWORD_SALT_LEN == SPI_USER_SYNC_SALT_LEN, "user salt SPI width must match store");
static_assert(USER_PASSWORD_HASH_LEN == SPI_USER_SYNC_HASH_LEN, "user hash SPI width must match store");

UserStore::UserStore() : storedCount(0), persistEnabled(false), persistPending(false), changedHandler(nullptr) {
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

int UserStore::nextUsedIndex(int startIndex) const {
    if (startIndex < 0) {
        startIndex = 0;
    }
    for (int index = startIndex; index < storedCount; index++) {
        if (users[index].userName[0] != '\0') {
            return index;
        }
    }
    return -1;
}

UserRecord *UserStore::userAt(int index) {
    return const_cast<UserRecord *>(static_cast<const UserStore *>(this)->userAt(index));
}

const UserRecord *UserStore::userAt(int index) const {
    if (index < 0 || index >= storedCount) {
        return nullptr;
    }
    return &users[index];
}

void UserStore::setChangedHandler(ChangedFn handler) {
    changedHandler = handler;
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

int UserStore::unlockedAdminCount() const {
    int count = 0;
    for (int index = 0; index < storedCount; index++) {
        if (users[index].isAdmin && !users[index].isBlocked) {
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

bool UserStore::begin(bool persistToLittleFs) {
    persistEnabled = persistToLittleFs;
    if (!persistEnabled) {
        return true;
    }
    if (loadFromFile()) {
        LOGGER.info("Loaded " + String(storedCount) + " console user(s) from slave store");
        return true;
    }
    seedAdmin();
    if (!persistNow()) {
        LOGGER.error("Slave user seed persist failed");
        return false;
    }
    return storedCount > 0;
}

bool UserStore::persistNow() {
    if (!persistEnabled) {
        return true;
    }
    if (storedCount == 0) {
        LOGGER.warning("Refusing to erase slave user store with an empty list");
        loadFromFile();
        return false;
    }
    if (!saveToFile()) {
        LOGGER.error("Slave user store write failed");
        return false;
    }
    LOGGER.info("Slave user store saved " + String(storedCount) + " user(s)");
    return true;
}

void UserStore::requestPersist() {
    persistPending = true;
}

void UserStore::persistIfDue() {
    if (!persistPending) {
        return;
    }
    persistPending = false;
    if (storedCount == 0) {
        LOGGER.warning("Refusing to erase slave user store with an empty list");
        loadFromFile();
        return;
    }
    if (!saveToFile()) {
        LOGGER.error("Slave user store write failed");
        persistPending = true;
        return;
    }
    LOGGER.info("Slave user store saved " + String(storedCount) + " user(s)");
}

void UserStore::afterMutation(UserChangeKind kind, const UserRecord *user) {
    if (persistEnabled) {
        requestPersist();
        return;
    }
    if (changedHandler != nullptr && user != nullptr) {
        changedHandler(kind, user);
    }
}

void UserStore::noteRecordChanged(const UserRecord *user) {
    afterMutation(UserChangeUpsert, user);
}

void UserStore::replaceFrom(const UserStore *source) {
    if (source == nullptr) {
        return;
    }
    memcpy(users, source->users, sizeof(users));
    storedCount = source->storedCount;
}

void UserStore::resetToEmpty() {
    clearUsers();
    memset(sessions, 0, sizeof(sessions));
    persistPending = false;
}

bool UserStore::upsertFromRecord(const UserRecord *source, bool notify) {
    if (source == nullptr || source->userName[0] == '\0') {
        return false;
    }
    UserRecord *existing = findByName(source->userName);
    if (existing != nullptr) {
        *existing = *source;
        if (notify) {
            afterMutation(UserChangeUpsert, existing);
        }
        return true;
    }
    if (storedCount >= USER_STORE_MAX) {
        return false;
    }
    users[storedCount] = *source;
    storedCount++;
    if (notify) {
        afterMutation(UserChangeUpsert, &users[storedCount - 1]);
    }
    return true;
}

bool UserStore::removeByName(const char *userName, bool notify) {
    int foundIndex = -1;
    for (int index = 0; index < storedCount; index++) {
        if (nameEquals(users[index].userName, userName)) {
            foundIndex = index;
            break;
        }
    }
    if (foundIndex < 0) {
        return false;
    }
    UserRecord removed = users[foundIndex];
    if (foundIndex < storedCount - 1) {
        memmove(&users[foundIndex], &users[foundIndex + 1], sizeof(UserRecord) * (storedCount - foundIndex - 1));
    }
    storedCount--;
    memset(&users[storedCount], 0, sizeof(UserRecord));
    if (notify) {
        afterMutation(UserChangeDelete, &removed);
    }
    return true;
}

bool UserStore::replaceFromExportJson(
    const String &json,
    char (*removedNames)[USER_NAME_MAX],
    int *removedCount,
    int removedMax
) {
    UserStore scratch;
    if (!scratch.parseUsersJson(json, true) || scratch.unlockedAdminCount() < 1) {
        return false;
    }
    int collected = 0;
    for (int index = 0; index < storedCount; index++) {
        if (scratch.findByName(users[index].userName) != nullptr) {
            continue;
        }
        if (removedNames != nullptr && collected < removedMax) {
            strncpy(removedNames[collected], users[index].userName, USER_NAME_MAX - 1);
            removedNames[collected][USER_NAME_MAX - 1] = '\0';
            collected++;
        }
    }
    if (removedCount != nullptr) {
        *removedCount = collected;
    }
    memcpy(users, scratch.users, sizeof(users));
    storedCount = scratch.storedCount;
    if (persistEnabled) {
        requestPersist();
    }
    return true;
}

size_t UserStore::packSyncPayload(uint8_t *out, size_t outMax, uint8_t flags, const UserRecord *user) {
    if (out == nullptr) {
        return 0;
    }
    if ((flags & SPI_USER_SYNC_RESET) != 0 || (flags & SPI_USER_SYNC_LAST) != 0) {
        if (outMax < 2 && (flags & SPI_USER_SYNC_RESET) != 0) {
            return 0;
        }
        if (outMax < 1) {
            return 0;
        }
        out[0] = flags;
        if ((flags & SPI_USER_SYNC_RESET) != 0) {
            return 1;
        }
        return 1;
    }
    if (user == nullptr || outMax < SPI_USER_SYNC_ENTRY_LEN) {
        return 0;
    }
    memset(out, 0, SPI_USER_SYNC_ENTRY_LEN);
    out[0] = flags;
    strncpy((char *)out + 1, user->userName, SPI_USER_SYNC_NAME_LEN - 1);
    if ((flags & SPI_USER_SYNC_DELETE) != 0) {
        return SPI_USER_SYNC_ENTRY_LEN;
    }
    uint8_t salt[SPI_USER_SYNC_SALT_LEN];
    uint8_t hash[SPI_USER_SYNC_HASH_LEN];
    if (!hexToBytes(user->passwordSaltHex, salt, sizeof(salt))
        || !hexToBytes(user->passwordHashHex, hash, sizeof(hash))) {
        return 0;
    }
    memcpy(out + 1 + SPI_USER_SYNC_NAME_LEN, salt, sizeof(salt));
    memcpy(out + 1 + SPI_USER_SYNC_NAME_LEN + SPI_USER_SYNC_SALT_LEN, hash, sizeof(hash));
    const size_t addedAtOffset = 1 + SPI_USER_SYNC_NAME_LEN + SPI_USER_SYNC_SALT_LEN + SPI_USER_SYNC_HASH_LEN;
    out[addedAtOffset] = (uint8_t)(user->addedAt & 0xFF);
    out[addedAtOffset + 1] = (uint8_t)((user->addedAt >> 8) & 0xFF);
    out[addedAtOffset + 2] = (uint8_t)((user->addedAt >> 16) & 0xFF);
    out[addedAtOffset + 3] = (uint8_t)((user->addedAt >> 24) & 0xFF);
    uint8_t roleFlags = 0;
    if (user->isAdmin) {
        roleFlags |= SPI_USER_FLAG_ADMIN;
    }
    if (user->editDevices) {
        roleFlags |= SPI_USER_FLAG_EDIT_DEVICES;
    }
    if (user->addDevices) {
        roleFlags |= SPI_USER_FLAG_ADD_DEVICES;
    }
    if (user->removeDevices) {
        roleFlags |= SPI_USER_FLAG_REMOVE_DEVICES;
    }
    if (user->editUsers) {
        roleFlags |= SPI_USER_FLAG_EDIT_USERS;
    }
    if (user->isBlocked) {
        roleFlags |= SPI_USER_FLAG_BLOCKED;
    }
    out[addedAtOffset + 4] = roleFlags;
    out[addedAtOffset + 5] = user->theme == UI_THEME_DARK ? UI_THEME_DARK : UI_THEME_LIGHT;
    return SPI_USER_SYNC_ENTRY_LEN;
}

bool UserStore::unpackSyncPayload(const uint8_t *in, uint16_t length, uint8_t *flags, UserRecord *user) {
    if (in == nullptr || flags == nullptr || length < 1) {
        return false;
    }
    *flags = in[0];
    if ((*flags & SPI_USER_SYNC_RESET) != 0 || (*flags & SPI_USER_SYNC_LAST) != 0) {
        return true;
    }
    if (user == nullptr || length < 1 + SPI_USER_SYNC_NAME_LEN) {
        return false;
    }
    memset(user, 0, sizeof(*user));
    memcpy(user->userName, in + 1, SPI_USER_SYNC_NAME_LEN - 1);
    user->userName[USER_NAME_MAX - 1] = '\0';
    if ((*flags & SPI_USER_SYNC_DELETE) != 0) {
        return true;
    }
    if (length < SPI_USER_SYNC_ENTRY_LEN) {
        return false;
    }
    const uint8_t *salt = in + 1 + SPI_USER_SYNC_NAME_LEN;
    const uint8_t *hash = salt + SPI_USER_SYNC_SALT_LEN;
    bytesToHex(salt, SPI_USER_SYNC_SALT_LEN, user->passwordSaltHex);
    bytesToHex(hash, SPI_USER_SYNC_HASH_LEN, user->passwordHashHex);
    const size_t addedAtOffset = 1 + SPI_USER_SYNC_NAME_LEN + SPI_USER_SYNC_SALT_LEN + SPI_USER_SYNC_HASH_LEN;
    user->addedAt = (uint32_t)in[addedAtOffset] | ((uint32_t)in[addedAtOffset + 1] << 8)
        | ((uint32_t)in[addedAtOffset + 2] << 16) | ((uint32_t)in[addedAtOffset + 3] << 24);
    const uint8_t roleFlags = in[addedAtOffset + 4];
    user->isAdmin = (roleFlags & SPI_USER_FLAG_ADMIN) != 0;
    user->editDevices = (roleFlags & SPI_USER_FLAG_EDIT_DEVICES) != 0;
    user->addDevices = (roleFlags & SPI_USER_FLAG_ADD_DEVICES) != 0;
    user->removeDevices = (roleFlags & SPI_USER_FLAG_REMOVE_DEVICES) != 0;
    user->editUsers = (roleFlags & SPI_USER_FLAG_EDIT_USERS) != 0;
    user->isBlocked = (roleFlags & SPI_USER_FLAG_BLOCKED) != 0;
    user->theme = in[addedAtOffset + 5] == UI_THEME_DARK ? UI_THEME_DARK : UI_THEME_LIGHT;
    return true;
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
    if (unlockedAdminCount() < 1) {
        storedCount--;
        memset(user, 0, sizeof(*user));
        return UserWriteLastAdmin;
    }
    afterMutation(UserChangeUpsert, user);
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
    UserRecord previous = *user;
    copyRolesAndFlags(user, source);
    if (!actor->isAdmin) {
        user->isAdmin = false;
    }
    if (unlockedAdminCount() < 1) {
        *user = previous;
        return UserWriteLastAdmin;
    }
    if (password != nullptr && password[0] != '\0') {
        if (!setPassword(user, password)) {
            *user = previous;
            return UserWriteNeedPassword;
        }
    }
    afterMutation(UserChangeUpsert, user);
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
    int remainingUnlockedAdmins = unlockedAdminCount();
    if (users[foundIndex].isAdmin && !users[foundIndex].isBlocked) {
        remainingUnlockedAdmins--;
    }
    if (remainingUnlockedAdmins < 1) {
        return UserWriteLastAdmin;
    }
    UserRecord removed = users[foundIndex];
    if (foundIndex < storedCount - 1) {
        memmove(&users[foundIndex], &users[foundIndex + 1], sizeof(UserRecord) * (storedCount - foundIndex - 1));
    }
    storedCount--;
    memset(&users[storedCount], 0, sizeof(UserRecord));
    afterMutation(UserChangeDelete, &removed);
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
