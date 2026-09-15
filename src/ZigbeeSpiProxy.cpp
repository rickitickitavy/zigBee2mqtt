#include "ZigbeeSpiProxy.h"
#include "InterChipHost.h"
#include "Logger.h"

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
    for (int i = 0; i < kMaxDevices; i++) {
        if (devices[i].occupied && memcmp(devices[i].ieee, ieee, 8) == 0) {
            return &devices[i];
        }
    }
    return nullptr;
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
    return INTER_CHIP_HOST.tryEnqueue(SpiCmdPermitJoin, &seconds, 1);
}

bool ZigbeeSpiProxy::closeJoin() {
    uint8_t seconds = 0;
    return INTER_CHIP_HOST.tryEnqueue(SpiCmdPermitJoin, &seconds, 1);
}

bool ZigbeeSpiProxy::controlOnOff(const uint8_t ieee[8], const char *command, uint8_t endpoint) {
    const char *body = command != nullptr ? command : "";
    uint8_t payload[9 + SPI_DEVICE_MESSAGE_MAX];
    memcpy(payload, ieee, 8);
    payload[8] = endpoint;
    strncpy((char *)payload + 9, body, SPI_DEVICE_MESSAGE_MAX - 1);
    payload[8 + SPI_DEVICE_MESSAGE_MAX] = 0;
    const uint16_t length = (uint16_t)(9 + strlen((char *)payload + 9) + 1);
    return INTER_CHIP_HOST.tryEnqueue(SpiCmdZclOnOff, payload, length);
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
    return INTER_CHIP_HOST.tryEnqueue(SpiCmdZclWriteAttr, payload, SPI_ZCL_WRITE_ATTR_LEN);
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
    if (filePullRequested && commandsAllowed() && !registryPullRequested && !pullCollecting) {
        if (INTER_CHIP_HOST.tryEnqueue(SpiCmdGetDevicesFile, nullptr, 0)) {
            filePullRequested = false;
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
    pumpPendingChanges();
}

void ZigbeeSpiProxy::onSpiEvent(const SpiFrame &frame) {
    if (frame.cmd == SpiEvtDevicesFile) {
        applyDevicesFile(frame);
        return;
    }
    if (frame.cmd == SpiEvtDeviceMap) {
        applyPulledRegistry(frame);
        return;
    }
    if (frame.cmd == SpiEvtAttrReport && frame.length >= 12) {
        uint8_t ieee[8];
        memcpy(ieee, frame.payload, 8);
        const uint8_t endpoint = frame.payload[8];
        const uint16_t shortAddr = (uint16_t)frame.payload[9] | ((uint16_t)frame.payload[10] << 8);
        char message[SPI_DEVICE_MESSAGE_MAX];
        memset(message, 0, sizeof(message));
        if (frame.length == 12 && frame.payload[11] < 2) {
            strncpy(message, frame.payload[11] != 0 ? "ON" : "OFF", sizeof(message) - 1);
        } else {
            const size_t copyLength = frame.length - 11;
            const size_t bounded = copyLength >= sizeof(message) ? sizeof(message) - 1 : copyLength;
            memcpy(message, frame.payload + 11, bounded);
            message[sizeof(message) - 1] = '\0';
        }
        CachedDevice *slot = allocSlot(ieee);
        if (slot != nullptr) {
            slot->endpoint = endpoint;
            slot->shortAddr = shortAddr;
            slot->lastSeenMs = millis();
        }
        if (lightStateHandler != nullptr) {
            lightStateHandler(message, ieee, endpoint, shortAddr);
        }
        return;
    }
    if (frame.cmd == SpiEvtDeviceJoin && frame.length >= 75) {
        uint8_t ieee[8];
        memcpy(ieee, frame.payload, 8);
        CachedDevice *slot = allocSlot(ieee);
        if (slot == nullptr) {
            return;
        }
        slot->shortAddr = (uint16_t)frame.payload[8] | ((uint16_t)frame.payload[9] << 8);
        slot->endpoint = frame.payload[10];
        slot->lastSeenMs = millis();
        strncpy(slot->manufacturer, (const char *)frame.payload + 11, sizeof(slot->manufacturer) - 1);
        strncpy(slot->model, (const char *)frame.payload + 43, sizeof(slot->model) - 1);
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
