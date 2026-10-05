#pragma once

#include "DeviceTopicMap.h"

class DeviceStore {
public:
    DeviceStore();

    bool begin(bool persistToLittleFs);
    bool reloadFromFile();
    DeviceTopicMap *deviceMap();
    String readFileText();
    void requestPersist(bool allowEmpty);
    void persistIfDue();

private:
    DeviceTopicMap topicMap;
    bool persistEnabled = false;
    bool persistPending = false;
    bool persistAllowEmpty = false;
};
