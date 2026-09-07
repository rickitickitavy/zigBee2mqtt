#pragma once

#include <Arduino.h>
#include "SettingsManager.h"

class SerialCli {
public:
    explicit SerialCli(SettingsManager *settingsManager);

    void dispatch();

private:
    SettingsManager *settingsManager;
    String lineBuffer;

    void handleLine(const String &line);
    void printHelp();
};
