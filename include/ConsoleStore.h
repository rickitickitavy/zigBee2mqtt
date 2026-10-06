#pragma once

#include "Defines.h"
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

struct ConsoleBinding {
    char *ieeeText;
    uint8_t channel;
    char *title;
    char *unit;
    ConsoleBinding *next;
};

struct ConsoleWidget {
    char *title;
    bool showGroupOnOff;
    bool isSpacer;
    ConsoleBinding *bindings;
    ConsoleWidget *next;
};

struct ConsoleRecord {
    char id[CONSOLE_ID_BUF];
    char *name;
    bool active;
    uint8_t columns;
    ConsoleWidget *widgets;
    ConsoleRecord *next;
};

enum ConsoleWriteResult {
    ConsoleWriteOk = 0,
    ConsoleWriteNotFound,
    ConsoleWriteFull,
    ConsoleWriteBadId,
    ConsoleWriteBadName,
    ConsoleWriteOutOfMemory,
    ConsoleWriteParseError
};

class ConsoleStore {
public:
    ConsoleStore();
    ~ConsoleStore();

    bool begin(bool persistToLittleFs);
    void requestPersist();
    void persistIfDue();
    int consoleCount() const;
    ConsoleRecord *first();
    const ConsoleRecord *first() const;
    ConsoleRecord *findById(const char *consoleId);
    const ConsoleRecord *findById(const char *consoleId) const;
    ConsoleWriteResult createConsole(const char *name, bool active, ConsoleRecord **createdOut);
    ConsoleWriteResult replaceConsoleFromJson(const char *consoleId, const char *json, bool keepId);
    ConsoleWriteResult deleteConsole(const char *consoleId);
    bool replaceAllFromJson(const String &json);
    String listSummaryJson(class UserStore *userStore) const;
    String listFullJson() const;
    String consoleFullJson(const ConsoleRecord *console) const;
    String mineJson(const class UserRecord *user) const;
    static bool heapAllowsAllocation(size_t bytes);
    static char *duplicateBoundedString(const char *source, size_t maxLength);
    static void freeBindingList(ConsoleBinding *&head);
    static void freeWidgetList(ConsoleWidget *&head);
    static void freeConsole(ConsoleRecord *console);
    static void freeConsoleList(ConsoleRecord *&head);

private:
    ConsoleRecord *head;
    int storedCount;
    bool persistEnabled;
    bool persistPending;

    bool loadFromFile();
    bool persistNow();
    bool saveToFile() const;
    void clearAll();
    ConsoleWriteResult parseOneConsole(const char *objectJson, ConsoleRecord **outConsole, bool requireId);
    static bool parseWidgets(const char *objectJson, ConsoleWidget **outWidgets);
    static bool parseBindings(const char *widgetJson, ConsoleBinding **outBindings);
    static void appendConsoleFull(String &json, const ConsoleRecord *console);
    static void makeConsoleId(char *outId);
};
