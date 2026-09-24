#include "ZigbeeSpiProxy.h"
#include "InterChipHost.h"
#include "JsonField.h"
#include "Logger.h"

#include <stdio.h>
#include <string.h>

ZigbeeSpiProxy ZIGBEE_SPI_PROXY;

ZigbeeSpiProxy::ZigbeeSpiProxy() : pullMap(pullSlots) {
    memset(pullSlots, 0, sizeof(pullSlots));
}

void ZigbeeSpiProxy::begin() {}

void ZigbeeSpiProxy::setLightStateHandler(LightStateFn handler) {
    lightStateHandler = handler;
}

bool ZigbeeSpiProxy::commandsAllowed() const {
    return INTER_CHIP_HOST.isNormal();
}

ZigbeeSpiProxy::CachedDevice *ZigbeeSpiProxy::findByIeee(const uint8_t ieee[8]) {
    return const_cast<CachedDevice *>(static_cast<const ZigbeeSpiProxy *>(this)->findByIeee(ieee));
}

const ZigbeeSpiProxy::CachedDevice *ZigbeeSpiProxy::findByIeee(const uint8_t ieee[8]) const {
    if (ieee == nullptr) {
        return nullptr;
    }
    for (int i = 0; i < kMaxDevices; i++) {
        if (devices[i].occupied && memcmp(devices[i].ieee, ieee, 8) == 0) {
            return &devices[i];
        }
    }
    return nullptr;
}

void ZigbeeSpiProxy::noteReportTelemetry(CachedDevice *slot, uint8_t endpoint, const char *message) {
    if (slot == nullptr || message == nullptr || message[0] == '\0') {
        return;
    }
    unsigned parsedPercent = 0;
    if (sscanf(message, "BATTERY %u", &parsedPercent) == 1) {
        slot->hasBattery = true;
        slot->batteryPercent = parsedPercent;
        return;
    }
    if (strstr(message, "cl=0x") != nullptr && strstr(message, "attr=") != nullptr) {
        return;
    }
    if (!DeviceTopicMap::isUsableEndpoint(endpoint)) {
        return;
    }
    CachedDevice::EndpointStatus *statusSlot = nullptr;
    for (int i = 0; i < DEVICE_CHANNEL_COUNT_MAX; i++) {
        if (slot->endpointStatus[i].used && slot->endpointStatus[i].endpoint == endpoint) {
            statusSlot = &slot->endpointStatus[i];
            break;
        }
    }
    if (statusSlot == nullptr) {
        for (int i = 0; i < DEVICE_CHANNEL_COUNT_MAX; i++) {
            if (!slot->endpointStatus[i].used) {
                statusSlot = &slot->endpointStatus[i];
                statusSlot->used = true;
                statusSlot->endpoint = endpoint;
                break;
            }
        }
    }
    if (statusSlot == nullptr) {
        return;
    }
    strncpy(statusSlot->state, message, sizeof(statusSlot->state) - 1);
    statusSlot->state[sizeof(statusSlot->state) - 1] = '\0';
}

ZigbeeSpiProxy::CachedDevice *ZigbeeSpiProxy::allocSlot(const uint8_t ieee[8]) {
    CachedDevice *existing = findByIeee(ieee);
    if (existing != nullptr) {
        existing->lastSeenMs = millis();
        return existing;
    }
    for (int i = 0; i < kMaxDevices; i++) {
        if (!devices[i].occupied) {
            memset(&devices[i], 0, sizeof(devices[i]));
            memcpy(devices[i].ieee, ieee, 8);
            devices[i].occupied = true;
            devices[i].lastSeenMs = millis();
            return &devices[i];
        }
    }
    return nullptr;
}

bool ZigbeeSpiProxy::permitJoin(uint8_t seconds) {
    const bool queued = INTER_CHIP_HOST.tryEnqueue(SpiCmdPermitJoin, &seconds, 1);
    if (queued) {
        pairingOpen = seconds != 0;
    }
    return queued;
}

bool ZigbeeSpiProxy::closeJoin() {
    uint8_t seconds = 0;
    const bool queued = INTER_CHIP_HOST.tryEnqueue(SpiCmdPermitJoin, &seconds, 1);
    if (queued) {
        pairingOpen = false;
    }
    return queued;
}

bool ZigbeeSpiProxy::controlOnOff(const uint8_t ieee[8], const char *command, uint8_t endpoint) {
    const char *body = command != nullptr ? command : "";
    uint8_t payload[9 + SPI_DEVICE_MESSAGE_MAX];
    memcpy(payload, ieee, 8);
    payload[8] = endpoint;
    strncpy((char *)payload + 9, body, SPI_DEVICE_MESSAGE_MAX - 1);
    payload[8 + SPI_DEVICE_MESSAGE_MAX] = 0;
    const uint16_t length = (uint16_t)(9 + strlen((char *)payload + 9) + 1);
    const bool queued = INTER_CHIP_HOST.tryEnqueue(SpiCmdZclOnOff, payload, length);
    if (queued) {
        packetsTx++;
    }
    return queued;
}

bool ZigbeeSpiProxy::writeAttribute(
    const uint8_t ieee[8],
    uint8_t endpoint,
    uint16_t clusterId,
    uint16_t attributeId,
    uint8_t dataType,
    uint32_t attributeValue
) {
    uint8_t payload[SPI_ZCL_WRITE_ATTR_LEN];
    if (!spiPackZclWriteAttr(
            payload,
            sizeof(payload),
            ieee,
            endpoint,
            clusterId,
            attributeId,
            dataType,
            attributeValue
        )) {
        return false;
    }
    const bool queued = INTER_CHIP_HOST.tryEnqueue(SpiCmdZclWriteAttr, payload, SPI_ZCL_WRITE_ATTR_LEN);
    if (queued) {
        packetsTx++;
    }
    return queued;
}

bool ZigbeeSpiProxy::readAttribute(
    const uint8_t ieee[8],
    uint8_t endpoint,
    uint16_t clusterId,
    uint16_t attributeId
) {
    uint8_t payload[SPI_ZCL_READ_ATTR_LEN];
    if (!spiPackZclReadAttr(payload, sizeof(payload), ieee, endpoint, clusterId, attributeId)) {
        return false;
    }
    const bool queued = INTER_CHIP_HOST.tryEnqueue(SpiCmdZclReadAttr, payload, SPI_ZCL_READ_ATTR_LEN);
    if (queued) {
        packetsTx++;
    }
    return queued;
}

bool ZigbeeSpiProxy::sendClusterCommand(
    const uint8_t ieee[8],
    uint8_t endpoint,
    uint16_t clusterId,
    uint8_t commandId,
    const uint8_t *payload,
    uint8_t payloadLength
) {
    uint8_t frame[SPI_MAX_PAYLOAD];
    if (!spiPackZclCommand(
            frame,
            sizeof(frame),
            ieee,
            endpoint,
            clusterId,
            commandId,
            payload,
            payloadLength
        )) {
        return false;
    }
    const bool queued = INTER_CHIP_HOST.tryEnqueue(
        SpiCmdZclCommand,
        frame,
        spiZclCommandFrameLength(payloadLength)
    );
    if (queued) {
        packetsTx++;
    }
    return queued;
}

void ZigbeeSpiProxy::queueDeletedIeeesAndPushAll(const uint8_t (*deletedIeees)[8], int deletedCount) {
    pendingDeleteCount = 0;
    pendingDeleteIndex = 0;
    pendingUpsertWalk = 0;
    if (deletedIeees != nullptr) {
        const int bounded = deletedCount > DEVICE_MAP_SLOTS ? DEVICE_MAP_SLOTS : deletedCount;
        for (int i = 0; i < bounded; i++) {
            memcpy(pendingDeleteIeees[i], deletedIeees[i], 8);
        }
        pendingDeleteCount = bounded;
    }
    fullPushActive = true;
}

void ZigbeeSpiProxy::setRegistryPullDoneHandler(void (*handler)()) {
    registryPullDone = handler;
}

bool ZigbeeSpiProxy::registryHydrated() const {
    return registryReady;
}

void ZigbeeSpiProxy::requestDevicesFile() {
    filePullRequested = true;
}

String ZigbeeSpiProxy::devicesFileJson() const {
    return fileCache.length() > 0 ? fileCache : String("[]");
}

void ZigbeeSpiProxy::applyDevicesFile(const SpiFrame &frame) {
    if (frame.length < 1) {
        return;
    }
    const uint8_t flags = frame.payload[0];
    const char *chunk = (const char *)(frame.payload + 1);
    const int chunkLength = (int)frame.length - 1;
    if ((flags & SPI_FILE_FIRST) != 0) {
        filePullBuffer = "";
        filePullCollecting = true;
    }
    if (!filePullCollecting && (flags & SPI_FILE_FIRST) == 0) {
        filePullBuffer = "";
        filePullCollecting = true;
    }
    if (chunkLength > 0) {
        filePullBuffer.concat(chunk, (unsigned int)chunkLength);
    }
    if ((flags & SPI_FILE_LAST) != 0) {
        fileCache = filePullBuffer;
        filePullBuffer = "";
        filePullCollecting = false;
        filePullInFlight = false;
        filePullStartedMs = 0;
        LOGGER.info("Received slave devices.json (" + String(fileCache.length()) + " bytes)");
    }
}

void ZigbeeSpiProxy::requestRegistryPull(DeviceTopicMap *topicMap) {
    if (topicMap == nullptr) {
        return;
    }
    registryMap = topicMap;
    if (registryReady || pullCollecting) {
        return;
    }
    if (registryPullStartedMs == 0) {
        registryPullStartedMs = millis();
        LOGGER.info("Waiting for slave device list after radio start");
    }
}

void ZigbeeSpiProxy::requestUsersPull(UserStore *store) {
    if (store == nullptr) {
        return;
    }
    userMap = store;
    if (userReady || userCollecting) {
        return;
    }
    userPullRequested = true;
    if (userPullStartedMs == 0) {
        userPullStartedMs = millis();
        LOGGER.info("Waiting for slave user list after radio start");
    }
}

bool ZigbeeSpiProxy::queueDeviceChange(uint8_t flags, const DeviceTopicEntry *entry) {
    if (entry == nullptr) {
        return false;
    }
    for (int i = 0; i < kPendingChangeSlots; i++) {
        if (pendingChanges[i].used) {
            continue;
        }
        pendingChanges[i].used = true;
        pendingChanges[i].flags = flags;
        pendingChanges[i].entry = *entry;
        return true;
    }
    LOGGER.warning("Only one device record change at a time");
    return false;
}

bool ZigbeeSpiProxy::enqueueDeviceUpsert(const DeviceTopicEntry *entry) {
    if (entry == nullptr || !entry->used) {
        return false;
    }
    return queueDeviceChange(SPI_DEVICE_SYNC_ENTRY, entry);
}

bool ZigbeeSpiProxy::enqueueDeviceDelete(const uint8_t ieee[8]) {
    if (ieee == nullptr) {
        return false;
    }
    DeviceTopicEntry entry;
    memset(&entry, 0, sizeof(entry));
    memcpy(entry.ieee, ieee, 8);
    entry.used = 1;
    return queueDeviceChange(SPI_DEVICE_SYNC_DELETE, &entry);
}

void ZigbeeSpiProxy::applyPendingChangeToMap(const PendingDeviceChange *change) {
    if (registryMap == nullptr || change == nullptr) {
        return;
    }
    if ((change->flags & SPI_DEVICE_SYNC_DELETE) != 0) {
        registryMap->removeByIeee(change->entry.ieee);
        return;
    }
    if ((change->flags & SPI_DEVICE_SYNC_ENTRY) != 0) {
        registryMap->upsertFromEntry(&change->entry, false);
    }
}

void ZigbeeSpiProxy::replayPendingChanges() {
    for (int i = 0; i < kPendingChangeSlots; i++) {
        if (pendingChanges[i].used) {
            applyPendingChangeToMap(&pendingChanges[i]);
        }
    }
}

void ZigbeeSpiProxy::pumpPendingChanges() {
    if (!commandsAllowed() || registryPullRequested || registryPullActive || pullCollecting) {
        return;
    }
    for (int i = 0; i < kPendingChangeSlots; i++) {
        if (!pendingChanges[i].used) {
            continue;
        }
        if (enqueueRegistryFrame(pendingChanges[i].flags, &pendingChanges[i].entry)) {
            pendingChanges[i].used = false;
        }
        return;
    }
    if (!fullPushActive || registryMap == nullptr) {
        return;
    }
    if (pendingDeleteIndex < pendingDeleteCount) {
        DeviceTopicEntry deletedEntry;
        memset(&deletedEntry, 0, sizeof(deletedEntry));
        memcpy(deletedEntry.ieee, pendingDeleteIeees[pendingDeleteIndex], 8);
        deletedEntry.used = 1;
        if (enqueueRegistryFrame(SPI_DEVICE_SYNC_DELETE, &deletedEntry)) {
            pendingDeleteIndex++;
        }
        return;
    }
    const int nextIndex = registryMap->nextUsedIndex(pendingUpsertWalk);
    if (nextIndex < 0) {
        fullPushActive = false;
        pendingDeleteCount = 0;
        pendingDeleteIndex = 0;
        pendingUpsertWalk = 0;
        return;
    }
    DeviceTopicEntry *entry = registryMap->slotAt(nextIndex);
    if (entry != nullptr && enqueueRegistryFrame(SPI_DEVICE_SYNC_ENTRY, entry)) {
        pendingUpsertWalk = nextIndex + 1;
    }
}

bool ZigbeeSpiProxy::enqueueRegistryFrame(uint8_t flags, const DeviceTopicEntry *entry) {
    if ((flags & SPI_DEVICE_SYNC_RESET) != 0) {
        LOGGER.warning("Host cannot replace the full device store");
        return false;
    }
    const bool isUpsert = (flags & SPI_DEVICE_SYNC_ENTRY) != 0;
    const bool isDelete = (flags & SPI_DEVICE_SYNC_DELETE) != 0;
    if (isUpsert == isDelete) {
        LOGGER.warning("Host device change must be one create/update or one delete");
        return false;
    }
    uint8_t payload[SPI_DEVICE_SYNC_ENTRY_LEN];
    const size_t length = DeviceTopicMap::packSyncPayload(payload, sizeof(payload), flags, entry);
    if (length == 0) {
        return false;
    }
    return INTER_CHIP_HOST.tryEnqueue(SpiCmdSetDevice, payload, (uint16_t)length);
}

bool ZigbeeSpiProxy::queueUserChange(uint8_t flags, const UserRecord *user) {
    if (user == nullptr) {
        return false;
    }
    for (int i = 0; i < kPendingChangeSlots; i++) {
        if (pendingUserChanges[i].used) {
            continue;
        }
        pendingUserChanges[i].used = true;
        pendingUserChanges[i].flags = flags;
        pendingUserChanges[i].user = *user;
        return true;
    }
    LOGGER.warning("Only one user record change at a time");
    return false;
}

bool ZigbeeSpiProxy::enqueueUserUpsert(const UserRecord *user) {
    if (user == nullptr || user->userName[0] == '\0') {
        return false;
    }
    return queueUserChange(SPI_USER_SYNC_ENTRY, user);
}

bool ZigbeeSpiProxy::enqueueUserDelete(const char *userName) {
    if (userName == nullptr || userName[0] == '\0') {
        return false;
    }
    UserRecord user;
    memset(&user, 0, sizeof(user));
    strncpy(user.userName, userName, sizeof(user.userName) - 1);
    return queueUserChange(SPI_USER_SYNC_DELETE, &user);
}

void ZigbeeSpiProxy::queueDeletedUserNamesAndPushAll(const char (*removedNames)[USER_NAME_MAX], int removedCount) {
    pendingUserDeleteCount = 0;
    pendingUserDeleteIndex = 0;
    pendingUserUpsertWalk = 0;
    if (removedNames != nullptr) {
        const int bounded = removedCount > USER_STORE_MAX ? USER_STORE_MAX : removedCount;
        for (int i = 0; i < bounded; i++) {
            strncpy(pendingDeleteUserNames[i], removedNames[i], USER_NAME_MAX - 1);
            pendingDeleteUserNames[i][USER_NAME_MAX - 1] = '\0';
        }
        pendingUserDeleteCount = bounded;
    }
    userFullPushActive = true;
}

void ZigbeeSpiProxy::applyPendingUserChangeToStore(const PendingUserChange *change) {
    if (userMap == nullptr || change == nullptr) {
        return;
    }
    if ((change->flags & SPI_USER_SYNC_DELETE) != 0) {
        userMap->removeByName(change->user.userName, false);
        return;
    }
    if ((change->flags & SPI_USER_SYNC_ENTRY) != 0) {
        userMap->upsertFromRecord(&change->user, false);
    }
}

void ZigbeeSpiProxy::replayPendingUserChanges() {
    for (int i = 0; i < kPendingChangeSlots; i++) {
        if (pendingUserChanges[i].used) {
            applyPendingUserChangeToStore(&pendingUserChanges[i]);
        }
    }
}

void ZigbeeSpiProxy::pumpPendingUserChanges() {
    if (!commandsAllowed() || userPullRequested || userPullActive || userCollecting) {
        return;
    }
    for (int i = 0; i < kPendingChangeSlots; i++) {
        if (!pendingUserChanges[i].used) {
            continue;
        }
        if (enqueueUserFrame(pendingUserChanges[i].flags, &pendingUserChanges[i].user)) {
            pendingUserChanges[i].used = false;
        }
        return;
    }
    if (!userFullPushActive || userMap == nullptr) {
        return;
    }
    if (pendingUserDeleteIndex < pendingUserDeleteCount) {
        UserRecord deletedUser;
        memset(&deletedUser, 0, sizeof(deletedUser));
        strncpy(deletedUser.userName, pendingDeleteUserNames[pendingUserDeleteIndex], sizeof(deletedUser.userName) - 1);
        if (enqueueUserFrame(SPI_USER_SYNC_DELETE, &deletedUser)) {
            pendingUserDeleteIndex++;
        }
        return;
    }
    const int nextIndex = userMap->nextUsedIndex(pendingUserUpsertWalk);
    if (nextIndex < 0) {
        userFullPushActive = false;
        pendingUserDeleteCount = 0;
        pendingUserDeleteIndex = 0;
        pendingUserUpsertWalk = 0;
        return;
    }
    const UserRecord *user = userMap->userAt(nextIndex);
    if (user != nullptr && enqueueUserFrame(SPI_USER_SYNC_ENTRY, user)) {
        pendingUserUpsertWalk = nextIndex + 1;
    }
}

bool ZigbeeSpiProxy::enqueueUserFrame(uint8_t flags, const UserRecord *user) {
    if ((flags & SPI_USER_SYNC_RESET) != 0) {
        LOGGER.warning("Host cannot replace the full user store");
        return false;
    }
    const bool isUpsert = (flags & SPI_USER_SYNC_ENTRY) != 0;
    const bool isDelete = (flags & SPI_USER_SYNC_DELETE) != 0;
    if (isUpsert == isDelete) {
        LOGGER.warning("Host user change must be one create/update or one delete");
        return false;
    }
    uint8_t payload[SPI_USER_SYNC_ENTRY_LEN];
    const size_t length = UserStore::packSyncPayload(payload, sizeof(payload), flags, user);
    if (length == 0) {
        return false;
    }
    return INTER_CHIP_HOST.tryEnqueue(SpiCmdSetUser, payload, (uint16_t)length);
}

void ZigbeeSpiProxy::beginUserPullSnapshot() {
    pullUsers.resetToEmpty();
    userCollecting = true;
    userPullActive = true;
    userPullExpectedCount = -1;
}

void ZigbeeSpiProxy::applyPulledUsers(const SpiFrame &frame) {
    if (userMap == nullptr) {
        return;
    }
    uint8_t flags = 0;
    UserRecord user;
    if (!UserStore::unpackSyncPayload(frame.payload, frame.length, &flags, &user)) {
        return;
    }
    if ((flags & SPI_USER_SYNC_RESET) != 0) {
        beginUserPullSnapshot();
        if (frame.length >= 2) {
            userPullExpectedCount = frame.payload[1];
        }
    }
    if ((flags & SPI_USER_SYNC_ENTRY) != 0) {
        if (!userCollecting) {
            beginUserPullSnapshot();
        }
        pullUsers.upsertFromRecord(&user, false);
    }
    if ((flags & SPI_USER_SYNC_LAST) != 0) {
        finishUsersPull();
    }
}

void ZigbeeSpiProxy::finishUsersPull() {
    const int receivedCount = userCollecting ? pullUsers.userCount() : 0;
    if (userCollecting && userPullExpectedCount >= 0 && receivedCount < userPullExpectedCount) {
        LOGGER.warning(
            "User dump incomplete " + String(receivedCount) + "/" + String(userPullExpectedCount)
        );
        userCollecting = false;
        userPullActive = false;
        userReady = false;
        if (userPullRetries < 5) {
            userPullRetries++;
            userPullRequested = true;
            userPullStartedMs = millis();
        }
        return;
    }
    if (userCollecting) {
        if (receivedCount == 0 && userMap != nullptr && userMap->userCount() > 0) {
            LOGGER.warning("Ignoring empty user dump; keeping current list");
        } else if (userMap != nullptr) {
            userMap->replaceFrom(&pullUsers);
        }
    }
    userCollecting = false;
    userPullActive = false;
    userPullRequested = false;
    userPullStartedMs = 0;
    userPullRetries = 0;
    userPullExpectedCount = -1;
    replayPendingUserChanges();
    userReady = true;
    LOGGER.info("Pulled " + String(userMap != nullptr ? userMap->userCount() : 0) + " user(s) from slave");
}

void ZigbeeSpiProxy::beginPullSnapshot() {
    pullMap.clearAll();
    pullCollecting = true;
    registryPullActive = true;
    registryPullCount = 0;
    pullExpectedCount = -1;
}

void ZigbeeSpiProxy::applyPulledRegistry(const SpiFrame &frame) {
    if (registryMap == nullptr) {
        return;
    }
    uint8_t flags = 0;
    DeviceTopicEntry entry;
    if (!DeviceTopicMap::unpackSyncPayload(frame.payload, frame.length, &flags, &entry)) {
        return;
    }
    if ((flags & SPI_DEVICE_SYNC_RESET) != 0) {
        beginPullSnapshot();
        if (frame.length >= 2) {
            pullExpectedCount = frame.payload[1];
        }
    }
    if ((flags & SPI_DEVICE_SYNC_ENTRY) != 0) {
        if (!pullCollecting) {
            beginPullSnapshot();
        }
        pullMap.upsertFromEntry(&entry, false);
        registryPullCount++;
    }
    if ((flags & SPI_DEVICE_SYNC_LAST) != 0) {
        finishRegistryPull();
    }
}

void ZigbeeSpiProxy::finishRegistryPull() {
    const int receivedCount = pullCollecting ? pullMap.usedCount() : 0;
    if (pullCollecting && pullExpectedCount >= 0 && receivedCount < pullExpectedCount) {
        LOGGER.warning(
            "Device dump incomplete " + String(receivedCount) + "/" + String(pullExpectedCount)
        );
        pullCollecting = false;
        registryPullActive = false;
        registryReady = false;
        if (pullRetries < 5) {
            pullRetries++;
            registryPullRequested = true;
            registryPullStartedMs = millis();
        }
        return;
    }
    if (pullCollecting) {
        if (receivedCount == 0 && registryMap != nullptr && registryMap->usedCount() > 0) {
            LOGGER.warning("Ignoring empty device dump; keeping current list");
        } else if (registryMap != nullptr) {
            pullMap.copyZigbeeTypeFrom(registryMap);
            registryMap->replaceFrom(&pullMap);
        }
    }
    pullCollecting = false;
    registryPullActive = false;
    registryPullRequested = false;
    registryPullStartedMs = 0;
    pullRetries = 0;
    pullExpectedCount = -1;
    replayPendingChanges();
    registryReady = true;
    LOGGER.info("Pulled " + String(registryMap != nullptr ? registryMap->usedCount() : 0) + " device(s) from slave");
    filePullRequested = true;
    if (registryPullDone != nullptr) {
        registryPullDone();
    }
}

void ZigbeeSpiProxy::pumpRegistrySync() {
    if (filePullInFlight && filePullStartedMs != 0
        && (long)(millis() - filePullStartedMs) >= 15000L) {
        LOGGER.warning("Slave devices.json not received; continuing");
        filePullInFlight = false;
        filePullCollecting = false;
        filePullStartedMs = 0;
    }
    if (filePullRequested && commandsAllowed() && !registryPullRequested && !pullCollecting) {
        if (INTER_CHIP_HOST.tryEnqueue(SpiCmdGetDevicesFile, nullptr, 0)) {
            filePullRequested = false;
            filePullInFlight = true;
            filePullStartedMs = millis();
            LOGGER.info("Requesting slave devices.json");
        }
        return;
    }
    if (registryPullRequested && commandsAllowed()) {
        if (INTER_CHIP_HOST.tryEnqueue(SpiCmdGetDevices, nullptr, 0)) {
            registryPullRequested = false;
            registryPullActive = true;
            registryPullStartedMs = millis();
            LOGGER.info("Requesting device registry from slave");
        }
        return;
    }
    if (!registryReady && !pullCollecting && !registryPullActive && !registryPullRequested
        && registryMap != nullptr && commandsAllowed() && registryPullStartedMs != 0
        && (long)(millis() - registryPullStartedMs) >= 15000L) {
        registryPullRequested = true;
        LOGGER.warning("Slave device list not received; requesting it");
    }
    if (userPullRequested && commandsAllowed() && !registryPullRequested && !registryPullActive
        && !pullCollecting && !filePullRequested && !filePullCollecting && !filePullInFlight) {
        if (INTER_CHIP_HOST.tryEnqueue(SpiCmdGetUsers, nullptr, 0)) {
            userPullRequested = false;
            userPullActive = true;
            userPullStartedMs = millis();
            LOGGER.info("Requesting user store from slave");
        }
        return;
    }
    if (!userReady && !userCollecting && !userPullActive && !userPullRequested
        && userMap != nullptr && commandsAllowed() && userPullStartedMs != 0
        && (long)(millis() - userPullStartedMs) >= 15000L) {
        userPullRequested = true;
        LOGGER.warning("Slave user list not received; requesting it");
    }
    pumpPendingChanges();
    pumpPendingUserChanges();
}

void ZigbeeSpiProxy::onSpiEvent(const SpiFrame &frame) {
    if (frame.cmd == SpiEvtJoinClosed) {
        pairingOpen = false;
        return;
    }
    if (frame.cmd == SpiEvtDevicesFile) {
        applyDevicesFile(frame);
        return;
    }
    if (frame.cmd == SpiEvtDeviceMap) {
        applyPulledRegistry(frame);
        return;
    }
    if (frame.cmd == SpiEvtUserMap) {
        applyPulledUsers(frame);
        return;
    }
    if (frame.cmd == SpiEvtAttrReport && frame.length >= SPI_ATTR_REPORT_MESSAGE_OFFSET + 1) {
        uint8_t ieee[8];
        memcpy(ieee, frame.payload, 8);
        const uint8_t endpoint = frame.payload[8];
        const uint16_t shortAddr = (uint16_t)frame.payload[9] | ((uint16_t)frame.payload[10] << 8);
        const int8_t rssiDbm = (int8_t)frame.payload[SPI_ATTR_REPORT_RSSI_OFFSET];
        char message[SPI_DEVICE_MESSAGE_MAX];
        memset(message, 0, sizeof(message));
        const size_t copyLength = frame.length - SPI_ATTR_REPORT_MESSAGE_OFFSET;
        const size_t bounded = copyLength >= sizeof(message) ? sizeof(message) - 1 : copyLength;
        memcpy(message, frame.payload + SPI_ATTR_REPORT_MESSAGE_OFFSET, bounded);
        message[sizeof(message) - 1] = '\0';
        CachedDevice *slot = allocSlot(ieee);
        if (slot != nullptr) {
            slot->endpoint = endpoint;
            slot->shortAddr = shortAddr;
            slot->lastSeenMs = millis();
            slot->lastRssiDbm = rssiDbm;
            slot->hasRssi = true;
            noteReportTelemetry(slot, endpoint, message);
        }
        packetsRx++;
        if (lightStateHandler != nullptr) {
            lightStateHandler(message, ieee, endpoint, shortAddr);
        }
        return;
    }
    if (frame.cmd == SpiEvtDeviceJoin && frame.length >= 75) {
        uint8_t ieee[8];
        memcpy(ieee, frame.payload, 8);
        CachedDevice *slot = allocSlot(ieee);
        packetsRx++;
        if (slot == nullptr) {
            return;
        }
        slot->shortAddr = (uint16_t)frame.payload[8] | ((uint16_t)frame.payload[9] << 8);
        slot->endpoint = frame.payload[10];
        slot->lastSeenMs = millis();
        strncpy(slot->manufacturer, (const char *)frame.payload + 11, sizeof(slot->manufacturer) - 1);
        strncpy(slot->model, (const char *)frame.payload + 43, sizeof(slot->model) - 1);
    }
    if (frame.cmd == SpiEvtDeviceLeave) {
        packetsRx++;
    }
}

void ZigbeeSpiProxy::noteSeen(const uint8_t ieee[8]) {
    CachedDevice *slot = allocSlot(ieee);
    if (slot != nullptr) {
        slot->lastSeenMs = millis();
    }
}

bool ZigbeeSpiProxy::isOnline(const uint8_t ieee[8]) const {
    if (ieee == nullptr) {
        return false;
    }
    for (int i = 0; i < kMaxDevices; i++) {
        if (!devices[i].occupied || memcmp(devices[i].ieee, ieee, 8) != 0) {
            continue;
        }
        return (long)(millis() - devices[i].lastSeenMs) < (long)kOnlineWindowMs;
    }
    return false;
}

bool ZigbeeSpiProxy::lastRssiDbm(const uint8_t ieee[8], int8_t *rssiDbm) const {
    if (ieee == nullptr || rssiDbm == nullptr) {
        return false;
    }
    for (int i = 0; i < kMaxDevices; i++) {
        if (!devices[i].occupied || memcmp(devices[i].ieee, ieee, 8) != 0) {
            continue;
        }
        if (!devices[i].hasRssi) {
            return false;
        }
        *rssiDbm = devices[i].lastRssiDbm;
        return true;
    }
    return false;
}

void ZigbeeSpiProxy::appendListTelemetry(const uint8_t ieee[8], String &json) const {
    const CachedDevice *slot = findByIeee(ieee);
    if (slot == nullptr) {
        return;
    }
    if (slot->hasBattery) {
        json += ",\"battery\":";
        json += String((unsigned)slot->batteryPercent);
    }
    bool anyStatus = false;
    for (int i = 0; i < DEVICE_CHANNEL_COUNT_MAX; i++) {
        if (slot->endpointStatus[i].used) {
            anyStatus = true;
            break;
        }
    }
    if (!anyStatus) {
        return;
    }
    json += ",\"status\":[";
    bool emitted[DEVICE_CHANNEL_COUNT_MAX];
    memset(emitted, 0, sizeof(emitted));
    bool first = true;
    for (int pass = 0; pass < DEVICE_CHANNEL_COUNT_MAX; pass++) {
        int bestIndex = -1;
        for (int i = 0; i < DEVICE_CHANNEL_COUNT_MAX; i++) {
            if (!slot->endpointStatus[i].used || emitted[i]) {
                continue;
            }
            if (bestIndex < 0
                || slot->endpointStatus[i].endpoint < slot->endpointStatus[bestIndex].endpoint) {
                bestIndex = i;
            }
        }
        if (bestIndex < 0) {
            break;
        }
        emitted[bestIndex] = true;
        if (!first) {
            json += ",";
        }
        first = false;
        json += "{\"ep\":";
        json += String((unsigned)slot->endpointStatus[bestIndex].endpoint);
        json += ",\"state\":\"";
        appendJsonEscaped(
            json,
            slot->endpointStatus[bestIndex].state,
            sizeof(slot->endpointStatus[bestIndex].state)
        );
        json += "\"}";
    }
    json += "]";
}

uint32_t ZigbeeSpiProxy::packetsReceived() const {
    return packetsRx;
}

uint32_t ZigbeeSpiProxy::packetsSent() const {
    return packetsTx;
}

bool ZigbeeSpiProxy::pairingActive() const {
    return pairingOpen;
}

int ZigbeeSpiProxy::onlineCount() const {
    int count = 0;
    for (int i = 0; i < kMaxDevices; i++) {
        if (!devices[i].occupied) {
            continue;
        }
        if ((long)(millis() - devices[i].lastSeenMs) < (long)kOnlineWindowMs) {
            count++;
        }
    }
    return count;
}

String ZigbeeSpiProxy::devicesJson(DeviceTopicMap *topicMap) {
    String json = "[";
    bool first = true;
    for (int i = 0; i < kMaxDevices; i++) {
        CachedDevice *device = &devices[i];
        if (!device->occupied) {
            continue;
        }
        if (!first) {
            json += ",";
        }
        first = false;
        json += "{\"ieee\":\"";
        json += topicMap->formatIeee(device->ieee);
        json += "\",\"nwk\":\"0x";
        json += String(device->shortAddr, HEX);
        json += "\",\"endpoint\":";
        json += String(device->endpoint);
        json += ",\"manufacturer\":\"";
        json += device->manufacturer;
        json += "\",\"model\":\"";
        json += device->model;
        json += "\"";
        DeviceTopicEntry *mapped = topicMap->findByIeee(device->ieee);
        if (mapped != nullptr) {
            json += ",\"name\":\"";
            json += mapped->friendlyName;
            json += "\",\"state\":\"";
            json += mapped->stateTopic;
            json += "\",\"command\":\"";
            json += mapped->commandTopic;
            json += "\"";
        }
        json += "}";
    }
    json += "]";
    return json;
}
