#include "DeviceStore.h"
#include "Defines.h"
#include "Logger.h"

#include <LittleFS.h>
#include <string.h>

DeviceStore::DeviceStore() {}

bool DeviceStore::begin(bool persistToLittleFs) {
    persistEnabled = persistToLittleFs;
    if (!LittleFS.begin(false)) {
        LOGGER.error("LittleFS mount failed; not formatting");
        return false;
    }
    if (!persistEnabled) {
        LittleFS.remove(DEVICES_STORE_PATH);
        LittleFS.remove(USERS_STORE_PATH);
        LOGGER.info("No device or user files on this chip");
        return true;
    }
    if (!topicMap.loadFromFile(DEVICES_STORE_PATH)) {
        LOGGER.info("Slave device store file missing; starting with an empty list");
        return true;
    }
    LOGGER.info("Slave device store loaded " + String(topicMap.usedCount()) + " device(s)");
    return true;
}

bool DeviceStore::reloadFromFile() {
    if (!topicMap.loadFromFile(DEVICES_STORE_PATH)) {
        return false;
    }
    LOGGER.info("Slave device store reloaded " + String(topicMap.usedCount()) + " device(s)");
    return true;
}

DeviceTopicMap *DeviceStore::deviceMap() {
    return &topicMap;
}

String DeviceStore::readFileText() {
    File file = LittleFS.open(DEVICES_STORE_PATH, "r");
    if (!file) {
        return String("[]");
    }
    String json = file.readString();
    file.close();
    json.trim();
    if (json.length() == 0) {
        return String("[]");
    }
    return json;
}

void DeviceStore::requestPersist(bool allowEmpty) {
    persistPending = true;
    persistAllowEmpty = allowEmpty;
}

void DeviceStore::persistIfDue() {
    if (!persistPending) {
        return;
    }
    if (!persistEnabled) {
        persistPending = false;
        return;
    }
    persistPending = false;
    if (topicMap.usedCount() == 0 && !persistAllowEmpty) {
        LOGGER.warning("Refusing to erase slave device store with an empty list");
        reloadFromFile();
        return;
    }
    if (!topicMap.saveToFile(DEVICES_STORE_PATH)) {
        LOGGER.error("Slave device store write failed");
        persistPending = true;
        return;
    }
    LOGGER.info("Slave device store saved " + String(topicMap.usedCount()) + " device(s)");
}
