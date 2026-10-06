#include "ConsoleStore.h"
#include "UserStore.h"
#include "JsonField.h"
#include "Logger.h"

#include <LittleFS.h>
#include <esp_random.h>
#include <string.h>
#include <stdio.h>

ConsoleStore::ConsoleStore()
    : head(nullptr), storedCount(0), persistEnabled(false), persistPending(false) {
}

ConsoleStore::~ConsoleStore() {
    clearAll();
}

bool ConsoleStore::heapAllowsAllocation(size_t bytes) {
    if (bytes == 0) {
        return true;
    }
    const size_t freeHeap = ESP.getFreeHeap();
    const size_t maxBlock = ESP.getMaxAllocHeap();
    if (maxBlock < bytes) {
        return false;
    }
    if (freeHeap < bytes) {
        return false;
    }
    if (freeHeap - bytes < CONSOLE_HEAP_RESERVE_BYTES) {
        return false;
    }
    return true;
}

char *ConsoleStore::duplicateBoundedString(const char *source, size_t maxLength) {
    const char *text = source != nullptr ? source : "";
    size_t length = strlen(text);
    if (length > maxLength) {
        length = maxLength;
    }
    if (!heapAllowsAllocation(length + 1)) {
        return nullptr;
    }
    char *copy = (char *)malloc(length + 1);
    if (copy == nullptr) {
        return nullptr;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

void ConsoleStore::freeBindingList(ConsoleBinding *&listHead) {
    while (listHead != nullptr) {
        ConsoleBinding *next = listHead->next;
        free(listHead->ieeeText);
        free(listHead->title);
        free(listHead->unit);
        free(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

void ConsoleStore::freeWidgetList(ConsoleWidget *&listHead) {
    while (listHead != nullptr) {
        ConsoleWidget *next = listHead->next;
        free(listHead->title);
        freeBindingList(listHead->bindings);
        free(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

void ConsoleStore::freeConsole(ConsoleRecord *console) {
    if (console == nullptr) {
        return;
    }
    free(console->name);
    freeWidgetList(console->widgets);
    free(console);
}

void ConsoleStore::freeConsoleList(ConsoleRecord *&listHead) {
    while (listHead != nullptr) {
        ConsoleRecord *next = listHead->next;
        freeConsole(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

void ConsoleStore::clearAll() {
    freeConsoleList(head);
    storedCount = 0;
}

void ConsoleStore::makeConsoleId(char *outId) {
    if (outId == nullptr) {
        return;
    }
    uint32_t randomValue = esp_random();
    snprintf(outId, CONSOLE_ID_BUF, "%08x", (unsigned int)randomValue);
}

int ConsoleStore::consoleCount() const {
    return storedCount;
}

ConsoleRecord *ConsoleStore::first() {
    return head;
}

const ConsoleRecord *ConsoleStore::first() const {
    return head;
}

ConsoleRecord *ConsoleStore::findById(const char *consoleId) {
    return const_cast<ConsoleRecord *>(static_cast<const ConsoleStore *>(this)->findById(consoleId));
}

const ConsoleRecord *ConsoleStore::findById(const char *consoleId) const {
    if (consoleId == nullptr || consoleId[0] == '\0') {
        return nullptr;
    }
    for (const ConsoleRecord *console = head; console != nullptr; console = console->next) {
        if (strcmp(console->id, consoleId) == 0) {
            return console;
        }
    }
    return nullptr;
}

bool ConsoleStore::begin(bool persistToLittleFs) {
    persistEnabled = persistToLittleFs;
    if (!persistEnabled) {
        return true;
    }
    if (loadFromFile()) {
        LOGGER.info("Loaded " + String(storedCount) + " control console(s)");
        return true;
    }
    clearAll();
    return true;
}

void ConsoleStore::requestPersist() {
    persistPending = true;
}

void ConsoleStore::persistIfDue() {
    if (!persistPending) {
        return;
    }
    if (!persistEnabled) {
        persistPending = false;
        return;
    }
    persistPending = false;
    if (!saveToFile()) {
        LOGGER.error("Console store write failed");
        persistPending = true;
        return;
    }
    LOGGER.info("Console store saved " + String(storedCount) + " console(s)");
}

bool ConsoleStore::saveToFile() const {
    File file = LittleFS.open(CONSOLES_STORE_PATH, "w");
    if (!file) {
        LOGGER.error("Consoles file write failed");
        return false;
    }
    const String json = listFullJson();
    const size_t written = file.print(json);
    file.close();
    return written == json.length();
}

bool ConsoleStore::persistNow() {
    if (!persistEnabled) {
        return true;
    }
    if (!saveToFile()) {
        LOGGER.error("Console store write failed");
        return false;
    }
    return true;
}

bool ConsoleStore::parseBindings(const char *widgetJson, ConsoleBinding **outBindings) {
    if (outBindings == nullptr) {
        return false;
    }
    *outBindings = nullptr;
    String bindingsJson;
    if (!extractJsonKeyedSlice(widgetJson, "devices", '[', bindingsJson)
        && !extractJsonKeyedSlice(widgetJson, "bindings", '[', bindingsJson)) {
        return true;
    }
    ConsoleBinding *tail = nullptr;
    const char *cursor = bindingsJson.c_str();
    while (cursor != nullptr && *cursor != '\0') {
        const char *objectStart = strchr(cursor, '{');
        if (objectStart == nullptr) {
            break;
        }
        const char *objectEnd = strchr(objectStart, '}');
        if (objectEnd == nullptr) {
            freeBindingList(*outBindings);
            return false;
        }
        String object = String(objectStart).substring(0, (unsigned int)(objectEnd - objectStart + 1));
        if (!heapAllowsAllocation(sizeof(ConsoleBinding))) {
            freeBindingList(*outBindings);
            return false;
        }
        ConsoleBinding *binding = (ConsoleBinding *)malloc(sizeof(ConsoleBinding));
        if (binding == nullptr) {
            freeBindingList(*outBindings);
            return false;
        }
        memset(binding, 0, sizeof(*binding));
        String ieeeText;
        String title;
        String unit;
        int channel = 1;
        int decimals = -1;
        extractJsonString(object.c_str(), "ieee", ieeeText);
        if (ieeeText.length() == 0) {
            extractJsonString(object.c_str(), "deviceIeee", ieeeText);
        }
        extractJsonString(object.c_str(), "title", title);
        extractJsonString(object.c_str(), "unit", unit);
        extractJsonInt(object.c_str(), "channel", channel);
        extractJsonInt(object.c_str(), "decimals", decimals);
        if (channel < 1) {
            channel = 1;
        }
        if (channel > DEVICE_CHANNEL_COUNT_MAX) {
            channel = DEVICE_CHANNEL_COUNT_MAX;
        }
        if (decimals < -1) {
            decimals = -1;
        }
        if (decimals > 3) {
            decimals = 3;
        }
        binding->ieeeText = duplicateBoundedString(ieeeText.c_str(), IEEE_TEXT_MAX - 1);
        binding->title = duplicateBoundedString(title.c_str(), CONSOLE_BINDING_TITLE_MAX - 1);
        binding->unit = duplicateBoundedString(unit.c_str(), CONSOLE_UNIT_MAX - 1);
        if (binding->ieeeText == nullptr || binding->title == nullptr || binding->unit == nullptr) {
            free(binding->ieeeText);
            free(binding->title);
            free(binding->unit);
            free(binding);
            freeBindingList(*outBindings);
            return false;
        }
        binding->channel = (uint8_t)channel;
        binding->decimals = (int8_t)decimals;
        binding->next = nullptr;
        if (tail == nullptr) {
            *outBindings = binding;
        } else {
            tail->next = binding;
        }
        tail = binding;
        cursor = objectEnd + 1;
    }
    return true;
}

bool ConsoleStore::parseWidgets(const char *objectJson, ConsoleWidget **outWidgets) {
    if (outWidgets == nullptr) {
        return false;
    }
    *outWidgets = nullptr;
    String widgetsJson;
    if (!extractJsonKeyedSlice(objectJson, "widgets", '[', widgetsJson)) {
        return true;
    }
    ConsoleWidget *tail = nullptr;
    const char *cursor = widgetsJson.c_str();
    while (cursor != nullptr && *cursor != '\0') {
        const char *objectStart = strchr(cursor, '{');
        if (objectStart == nullptr) {
            break;
        }
        int depth = 0;
        bool inString = false;
        bool escape = false;
        const char *objectEnd = nullptr;
        for (const char *scan = objectStart; *scan != '\0'; scan++) {
            const char character = *scan;
            if (inString) {
                if (escape) {
                    escape = false;
                    continue;
                }
                if (character == '\\') {
                    escape = true;
                    continue;
                }
                if (character == '"') {
                    inString = false;
                }
                continue;
            }
            if (character == '"') {
                inString = true;
                continue;
            }
            if (character == '{') {
                depth++;
                continue;
            }
            if (character == '}') {
                depth--;
                if (depth == 0) {
                    objectEnd = scan;
                    break;
                }
            }
        }
        if (objectEnd == nullptr) {
            freeWidgetList(*outWidgets);
            return false;
        }
        String object = String(objectStart).substring(0, (unsigned int)(objectEnd - objectStart + 1));
        if (!heapAllowsAllocation(sizeof(ConsoleWidget))) {
            freeWidgetList(*outWidgets);
            return false;
        }
        ConsoleWidget *widget = (ConsoleWidget *)malloc(sizeof(ConsoleWidget));
        if (widget == nullptr) {
            freeWidgetList(*outWidgets);
            return false;
        }
        memset(widget, 0, sizeof(*widget));
        String title;
        extractJsonBool(object.c_str(), "spacer", widget->isSpacer);
        extractJsonString(object.c_str(), "title", title);
        extractJsonBool(object.c_str(), "showGroupOnOff", widget->showGroupOnOff);
        if (widget->isSpacer) {
            widget->showGroupOnOff = false;
            title = "";
        }
        widget->title = duplicateBoundedString(title.c_str(), CONSOLE_WIDGET_TITLE_MAX - 1);
        if (widget->title == nullptr) {
            free(widget);
            freeWidgetList(*outWidgets);
            return false;
        }
        if (widget->isSpacer) {
            widget->bindings = nullptr;
        } else if (!parseBindings(object.c_str(), &widget->bindings)) {
            free(widget->title);
            free(widget);
            freeWidgetList(*outWidgets);
            return false;
        }
        widget->next = nullptr;
        if (tail == nullptr) {
            *outWidgets = widget;
        } else {
            tail->next = widget;
        }
        tail = widget;
        cursor = objectEnd + 1;
    }
    return true;
}

ConsoleWriteResult ConsoleStore::parseOneConsole(const char *objectJson, ConsoleRecord **outConsole, bool requireId) {
    if (objectJson == nullptr || outConsole == nullptr) {
        return ConsoleWriteParseError;
    }
    *outConsole = nullptr;
    String idText;
    String nameText;
    bool active = true;
    int columns = CONSOLE_GRID_COLUMNS_DEFAULT;
    extractJsonString(objectJson, "id", idText);
    extractJsonString(objectJson, "name", nameText);
    extractJsonBool(objectJson, "active", active);
    extractJsonInt(objectJson, "columns", columns);
    if (columns < 1) {
        columns = CONSOLE_GRID_COLUMNS_DEFAULT;
    }
    if (columns > CONSOLE_GRID_COLUMNS_MAX) {
        columns = CONSOLE_GRID_COLUMNS_MAX;
    }
    if (requireId && (idText.length() == 0 || idText.length() >= CONSOLE_ID_BUF)) {
        return ConsoleWriteBadId;
    }
    if (nameText.length() == 0 || nameText.length() >= CONSOLE_NAME_MAX) {
        return ConsoleWriteBadName;
    }
    if (!heapAllowsAllocation(sizeof(ConsoleRecord))) {
        return ConsoleWriteOutOfMemory;
    }
    ConsoleRecord *console = (ConsoleRecord *)malloc(sizeof(ConsoleRecord));
    if (console == nullptr) {
        return ConsoleWriteOutOfMemory;
    }
    memset(console, 0, sizeof(*console));
    if (idText.length() > 0) {
        strncpy(console->id, idText.c_str(), sizeof(console->id) - 1);
    } else {
        makeConsoleId(console->id);
    }
    console->name = duplicateBoundedString(nameText.c_str(), CONSOLE_NAME_MAX - 1);
    if (console->name == nullptr) {
        free(console);
        return ConsoleWriteOutOfMemory;
    }
    console->active = active;
    console->columns = (uint8_t)columns;
    if (!parseWidgets(objectJson, &console->widgets)) {
        freeConsole(console);
        return ConsoleWriteOutOfMemory;
    }
    *outConsole = console;
    return ConsoleWriteOk;
}

bool ConsoleStore::loadFromFile() {
    File file = LittleFS.open(CONSOLES_STORE_PATH, "r");
    if (!file) {
        return false;
    }
    String json = file.readString();
    file.close();
    json.trim();
    if (json.length() == 0 || json.charAt(0) != '[') {
        return false;
    }
    return replaceAllFromJson(json);
}

bool ConsoleStore::replaceAllFromJson(const String &json) {
    ConsoleRecord *newHead = nullptr;
    ConsoleRecord *tail = nullptr;
    int count = 0;
    const char *cursor = json.c_str();
    while (cursor != nullptr && *cursor != '\0' && count < CONSOLE_STORE_MAX) {
        const char *objectStart = strchr(cursor, '{');
        if (objectStart == nullptr) {
            break;
        }
        int depth = 0;
        bool inString = false;
        bool escape = false;
        const char *objectEnd = nullptr;
        for (const char *scan = objectStart; *scan != '\0'; scan++) {
            const char character = *scan;
            if (inString) {
                if (escape) {
                    escape = false;
                    continue;
                }
                if (character == '\\') {
                    escape = true;
                    continue;
                }
                if (character == '"') {
                    inString = false;
                }
                continue;
            }
            if (character == '"') {
                inString = true;
                continue;
            }
            if (character == '{') {
                depth++;
                continue;
            }
            if (character == '}') {
                depth--;
                if (depth == 0) {
                    objectEnd = scan;
                    break;
                }
            }
        }
        if (objectEnd == nullptr) {
            freeConsoleList(newHead);
            return false;
        }
        String object = String(objectStart).substring(0, (unsigned int)(objectEnd - objectStart + 1));
        ConsoleRecord *console = nullptr;
        if (parseOneConsole(object.c_str(), &console, true) != ConsoleWriteOk || console == nullptr) {
            freeConsoleList(newHead);
            return false;
        }
        if (tail == nullptr) {
            newHead = console;
        } else {
            tail->next = console;
        }
        tail = console;
        count++;
        cursor = objectEnd + 1;
    }
    clearAll();
    head = newHead;
    storedCount = count;
    if (persistEnabled) {
        requestPersist();
    }
    return true;
}

ConsoleWriteResult ConsoleStore::createConsole(const char *name, bool active, ConsoleRecord **createdOut) {
    if (storedCount >= CONSOLE_STORE_MAX) {
        return ConsoleWriteFull;
    }
    if (name == nullptr || name[0] == '\0' || strlen(name) >= CONSOLE_NAME_MAX) {
        return ConsoleWriteBadName;
    }
    if (!heapAllowsAllocation(sizeof(ConsoleRecord))) {
        return ConsoleWriteOutOfMemory;
    }
    ConsoleRecord *console = (ConsoleRecord *)malloc(sizeof(ConsoleRecord));
    if (console == nullptr) {
        return ConsoleWriteOutOfMemory;
    }
    memset(console, 0, sizeof(*console));
    makeConsoleId(console->id);
    while (findById(console->id) != nullptr) {
        makeConsoleId(console->id);
    }
    console->name = duplicateBoundedString(name, CONSOLE_NAME_MAX - 1);
    if (console->name == nullptr) {
        free(console);
        return ConsoleWriteOutOfMemory;
    }
    console->active = active;
    console->columns = CONSOLE_GRID_COLUMNS_DEFAULT;
    console->widgets = nullptr;
    console->next = nullptr;
    if (head == nullptr) {
        head = console;
    } else {
        ConsoleRecord *tail = head;
        while (tail->next != nullptr) {
            tail = tail->next;
        }
        tail->next = console;
    }
    storedCount++;
    if (persistEnabled) {
        requestPersist();
    }
    if (createdOut != nullptr) {
        *createdOut = console;
    }
    return ConsoleWriteOk;
}

ConsoleWriteResult ConsoleStore::replaceConsoleFromJson(const char *consoleId, const char *json, bool keepId) {
    ConsoleRecord *parsed = nullptr;
    const ConsoleWriteResult parseResult = parseOneConsole(json, &parsed, false);
    if (parseResult != ConsoleWriteOk || parsed == nullptr) {
        return parseResult;
    }
    ConsoleRecord *existing = nullptr;
    if (consoleId != nullptr && consoleId[0] != '\0') {
        existing = findById(consoleId);
    }
    if (existing == nullptr && keepId) {
        freeConsole(parsed);
        return ConsoleWriteNotFound;
    }
    if (existing != nullptr) {
        if (keepId) {
            strncpy(parsed->id, existing->id, sizeof(parsed->id) - 1);
        }
        free(existing->name);
        freeWidgetList(existing->widgets);
        existing->name = parsed->name;
        existing->active = parsed->active;
        existing->columns = parsed->columns;
        existing->widgets = parsed->widgets;
        parsed->name = nullptr;
        parsed->widgets = nullptr;
        freeConsole(parsed);
        if (persistEnabled) {
            requestPersist();
        }
        return ConsoleWriteOk;
    }
    if (storedCount >= CONSOLE_STORE_MAX) {
        freeConsole(parsed);
        return ConsoleWriteFull;
    }
    if (keepId && consoleId != nullptr && consoleId[0] != '\0') {
        strncpy(parsed->id, consoleId, sizeof(parsed->id) - 1);
        parsed->id[sizeof(parsed->id) - 1] = '\0';
    }
    parsed->next = nullptr;
    if (head == nullptr) {
        head = parsed;
    } else {
        ConsoleRecord *tail = head;
        while (tail->next != nullptr) {
            tail = tail->next;
        }
        tail->next = parsed;
    }
    storedCount++;
    if (persistEnabled) {
        requestPersist();
    }
    return ConsoleWriteOk;
}

ConsoleWriteResult ConsoleStore::deleteConsole(const char *consoleId) {
    if (consoleId == nullptr || consoleId[0] == '\0') {
        return ConsoleWriteBadId;
    }
    ConsoleRecord *previous = nullptr;
    ConsoleRecord *current = head;
    while (current != nullptr) {
        if (strcmp(current->id, consoleId) == 0) {
            if (previous == nullptr) {
                head = current->next;
            } else {
                previous->next = current->next;
            }
            freeConsole(current);
            storedCount--;
            if (persistEnabled) {
                requestPersist();
            }
            return ConsoleWriteOk;
        }
        previous = current;
        current = current->next;
    }
    return ConsoleWriteNotFound;
}

void ConsoleStore::appendConsoleFull(String &json, const ConsoleRecord *console) {
    json += "{\"id\":\"";
    appendJsonEscaped(json, console->id, sizeof(console->id));
    json += "\",\"name\":\"";
    appendJsonEscaped(json, console->name != nullptr ? console->name : "", CONSOLE_NAME_MAX);
    json += "\",\"active\":";
    json += console->active ? "true" : "false";
    json += ",\"columns\":";
    json += String((unsigned int)(console->columns > 0 ? console->columns : CONSOLE_GRID_COLUMNS_DEFAULT));
    json += ",\"widgets\":[";
    bool firstWidget = true;
    for (const ConsoleWidget *widget = console->widgets; widget != nullptr; widget = widget->next) {
        if (!firstWidget) {
            json += ",";
        }
        firstWidget = false;
        json += "{\"title\":\"";
        appendJsonEscaped(json, widget->title != nullptr ? widget->title : "", CONSOLE_WIDGET_TITLE_MAX);
        json += "\",\"showGroupOnOff\":";
        json += widget->showGroupOnOff ? "true" : "false";
        json += ",\"spacer\":";
        json += widget->isSpacer ? "true" : "false";
        json += ",\"devices\":[";
        bool firstBinding = true;
        if (!widget->isSpacer) {
            for (const ConsoleBinding *binding = widget->bindings; binding != nullptr; binding = binding->next) {
                if (!firstBinding) {
                    json += ",";
                }
                firstBinding = false;
                json += "{\"ieee\":\"";
                appendJsonEscaped(json, binding->ieeeText != nullptr ? binding->ieeeText : "", IEEE_TEXT_MAX);
                json += "\",\"channel\":";
                json += String(binding->channel);
                json += ",\"title\":\"";
                appendJsonEscaped(json, binding->title != nullptr ? binding->title : "", CONSOLE_BINDING_TITLE_MAX);
                json += "\",\"unit\":\"";
                appendJsonEscaped(json, binding->unit != nullptr ? binding->unit : "", CONSOLE_UNIT_MAX);
                json += "\",\"decimals\":";
                json += String((int)binding->decimals);
                json += "}";
            }
        }
        json += "]}";
    }
    json += "]}";
}

String ConsoleStore::consoleFullJson(const ConsoleRecord *console) const {
    String json;
    if (console == nullptr) {
        return String("null");
    }
    appendConsoleFull(json, console);
    return json;
}

String ConsoleStore::listFullJson() const {
    String json = "[";
    bool first = true;
    for (const ConsoleRecord *console = head; console != nullptr; console = console->next) {
        if (!first) {
            json += ",";
        }
        first = false;
        appendConsoleFull(json, console);
    }
    json += "]";
    return json;
}

String ConsoleStore::listSummaryJson(UserStore *userStore) const {
    String json = "[";
    bool first = true;
    for (const ConsoleRecord *console = head; console != nullptr; console = console->next) {
        if (!first) {
            json += ",";
        }
        first = false;
        json += "{\"id\":\"";
        appendJsonEscaped(json, console->id, sizeof(console->id));
        json += "\",\"name\":\"";
        appendJsonEscaped(json, console->name != nullptr ? console->name : "", CONSOLE_NAME_MAX);
        json += "\",\"active\":";
        json += console->active ? "true" : "false";
        json += ",\"users\":[";
        bool firstUser = true;
        if (userStore != nullptr) {
            for (int index = 0; index < userStore->userCount(); index++) {
                const UserRecord *user = userStore->userAt(index);
                if (user == nullptr || !UserStore::userHasConsole(user, console->id)) {
                    continue;
                }
                if (!firstUser) {
                    json += ",";
                }
                firstUser = false;
                json += "\"";
                appendJsonEscaped(json, user->userName, sizeof(user->userName));
                json += "\"";
            }
        }
        json += "]}";
    }
    json += "]";
    return json;
}

String ConsoleStore::mineJson(const UserRecord *user) const {
    String json = "[";
    bool first = true;
    if (user != nullptr) {
        for (const UserConsoleIdNode *node = user->consoleIds; node != nullptr; node = node->next) {
            const ConsoleRecord *console = findById(node->id);
            if (console == nullptr || !console->active) {
                continue;
            }
            if (!first) {
                json += ",";
            }
            first = false;
            appendConsoleFull(json, console);
        }
    }
    json += "]";
    return json;
}
