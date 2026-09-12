#include "ZigbeeSpiProxy.h"
#include "InterChipHost.h"
#include "Logger.h"

#include <string.h>

ZigbeeSpiProxy ZIGBEE_SPI_PROXY;

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
        return existing;
    }
    for (int i = 0; i < kMaxDevices; i++) {
        if (!devices[i].occupied) {
            memset(&devices[i], 0, sizeof(devices[i]));
            memcpy(devices[i].ieee, ieee, 8);
            devices[i].occupied = true;
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

bool ZigbeeSpiProxy::controlOnOff(const uint8_t ieee[8], const char *command) {
    String action = String(command);
    action.trim();
    action.toLowerCase();
    uint8_t code = 0xFF;
    if (action == "off" || action == "0" || action == "false") {
        code = 0;
    } else if (action == "on" || action == "1" || action == "true") {
        code = 1;
    } else if (action == "toggle") {
        code = 2;
    } else {
        LOGGER.warning("Unknown on/off command: " + action);
        return false;
    }
    uint8_t payload[9];
    memcpy(payload, ieee, 8);
    payload[8] = code;
    return INTER_CHIP_HOST.tryEnqueue(SpiCmdZclOnOff, payload, 9);
}

void ZigbeeSpiProxy::setRegistryPullDoneHandler(void (*handler)()) {
    registryPullDone = handler;
}

void ZigbeeSpiProxy::startRegistrySync(DeviceTopicMap *topicMap, bool allowEmptyReplace) {
    registryMap = topicMap;
    registryScanIndex = 0;
    registryResetSent = false;
    registryAllowEmptyReplace = allowEmptyReplace;
    registrySyncActive = topicMap != nullptr;
    registryPullRequested = false;
    registryPullActive = false;
}

void ZigbeeSpiProxy::requestRegistryPull(DeviceTopicMap *topicMap) {
    registryMap = topicMap;
    registryPullRequested = topicMap != nullptr;
    registryPullActive = false;
    registryPullCleared = false;
    registryPullCount = 0;
    registrySyncActive = false;
}

int ZigbeeSpiProxy::nextUsedSlot(int startIndex) const {
    if (registryMap == nullptr) {
        return -1;
    }
    return registryMap->nextUsedIndex(startIndex);
}

bool ZigbeeSpiProxy::enqueueRegistryFrame(uint8_t flags, const DeviceTopicEntry *entry) {
    uint8_t payload[SPI_DEVICE_SYNC_ENTRY_LEN];
    const size_t length = DeviceTopicMap::packSyncPayload(payload, sizeof(payload), flags, entry);
    if (length == 0) {
        return false;
    }
    return INTER_CHIP_HOST.tryEnqueue(SpiCmdSetDevice, payload, (uint16_t)length);
}

void ZigbeeSpiProxy::applyPulledRegistry(const SpiFrame &frame) {
    if (!registryPullActive || registryMap == nullptr) {
        return;
    }
    uint8_t flags = 0;
    DeviceTopicEntry entry;
    if (!DeviceTopicMap::unpackSyncPayload(frame.payload, frame.length, &flags, &entry)) {
        return;
    }
    if ((flags & SPI_DEVICE_SYNC_ENTRY) != 0) {
        if (!registryPullCleared) {
            registryMap->clearAll();
            registryPullCleared = true;
        }
        registryMap->upsert(
            entry.ieee,
            entry.friendlyName,
            entry.stateTopic,
            entry.commandTopic,
            entry.availabilityTopic
        );
        registryPullCount++;
    }
    if ((flags & SPI_DEVICE_SYNC_LAST) != 0) {
        finishRegistryPull();
    }
}

void ZigbeeSpiProxy::finishRegistryPull() {
    registryPullActive = false;
    registryPullRequested = false;
    if (registryPullCount == 0 && registryMap != nullptr && registryMap->usedCount() > 0) {
        LOGGER.info("Slave device store empty; migrating host cache to slave");
        startRegistrySync(registryMap);
        return;
    }
    LOGGER.info("Pulled " + String(registryPullCount) + " device(s) from slave");
    if (registryPullDone != nullptr) {
        registryPullDone();
    }
}

void ZigbeeSpiProxy::pumpRegistrySync() {
    if (registryPullRequested && commandsAllowed()) {
        if (INTER_CHIP_HOST.tryEnqueue(SpiCmdGetDevices, nullptr, 0)) {
            registryPullRequested = false;
            registryPullActive = true;
            registryPullCleared = false;
            registryPullCount = 0;
            LOGGER.info("Requesting device registry from slave");
        }
        return;
    }

    if (!registrySyncActive || registryMap == nullptr || !commandsAllowed()) {
        return;
    }

    if (!registryResetSent) {
        const int firstIndex = nextUsedSlot(0);
        uint8_t flags = SPI_DEVICE_SYNC_RESET;
        DeviceTopicEntry *entry = nullptr;
        if (firstIndex < 0) {
            if (!registryAllowEmptyReplace) {
                registrySyncActive = false;
                LOGGER.warning("Refusing to push empty device registry to slave");
                return;
            }
            flags |= SPI_DEVICE_SYNC_LAST | SPI_DEVICE_SYNC_ALLOW_EMPTY;
            if (enqueueRegistryFrame(flags, nullptr)) {
                registrySyncActive = false;
                LOGGER.info("Pushed empty device registry to slave");
            }
            return;
        }
        flags |= SPI_DEVICE_SYNC_ENTRY;
        entry = registryMap->slotAt(firstIndex);
        if (nextUsedSlot(firstIndex + 1) < 0) {
            flags |= SPI_DEVICE_SYNC_LAST;
        }
        if (!enqueueRegistryFrame(flags, entry)) {
            return;
        }
        registryResetSent = true;
        registryScanIndex = firstIndex + 1;
        if ((flags & SPI_DEVICE_SYNC_LAST) != 0) {
            registrySyncActive = false;
            LOGGER.info("Pushed device registry to slave");
        }
        return;
    }

    const int slotIndex = nextUsedSlot(registryScanIndex);
    if (slotIndex < 0) {
        if (enqueueRegistryFrame(SPI_DEVICE_SYNC_LAST, nullptr)) {
            registrySyncActive = false;
            LOGGER.info("Pushed device registry to slave");
        }
        return;
    }
    uint8_t flags = SPI_DEVICE_SYNC_ENTRY;
    if (nextUsedSlot(slotIndex + 1) < 0) {
        flags |= SPI_DEVICE_SYNC_LAST;
    }
    DeviceTopicEntry *entry = registryMap->slotAt(slotIndex);
    if (!enqueueRegistryFrame(flags, entry)) {
        return;
    }
    registryScanIndex = slotIndex + 1;
    if ((flags & SPI_DEVICE_SYNC_LAST) != 0) {
        registrySyncActive = false;
        LOGGER.info("Pushed device registry to slave");
    }
}

void ZigbeeSpiProxy::onSpiEvent(const SpiFrame &frame) {
    if (frame.cmd == SpiEvtDeviceMap) {
        applyPulledRegistry(frame);
        return;
    }
    if (frame.cmd == SpiEvtAttrReport && frame.length >= 12) {
        uint8_t ieee[8];
        memcpy(ieee, frame.payload, 8);
        const uint8_t endpoint = frame.payload[8];
        const uint16_t shortAddr = (uint16_t)frame.payload[9] | ((uint16_t)frame.payload[10] << 8);
        const bool on = frame.payload[11] != 0;
        CachedDevice *slot = allocSlot(ieee);
        if (slot != nullptr) {
            slot->endpoint = endpoint;
            slot->shortAddr = shortAddr;
        }
        if (lightStateHandler != nullptr) {
            lightStateHandler(on, ieee, endpoint, shortAddr);
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
        strncpy(slot->manufacturer, (const char *)frame.payload + 11, sizeof(slot->manufacturer) - 1);
        strncpy(slot->model, (const char *)frame.payload + 43, sizeof(slot->model) - 1);
    }
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
