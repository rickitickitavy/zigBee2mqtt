## Context

See `proposal.md`. Host-only web UI in `data/index.html` + `data/css/all.css`, served by `WebConsole`. Observed today:

- Update POSTs only to `/update` with `U_FLASH`. No LittleFS form.
- `#section-wifi` sets `overflow: hidden`, so Save/Reload sit below the clip. `.btn { flex: 1 }` makes action buttons full-width.
- Log textarea has `min-height: 16rem` plus panel `overflow: auto`, so the page scroll hides Refresh.
- MQTT `loadMqtt()` runs on `window.onload` and swallows fetch/JSON errors. Fields exist; values stay empty if `/api/mqtt` fails or parse fails. Reloading the card does not refetch.

Assumption: **Reset** means reload the form from `GET /api/wifi` (and MQTT if we add the same), not factory-default settings.

## Goals / Non-Goals

**Goals:**

- Filesystem upload on Update.
- Log: one scroller (the textarea); compact left Refresh.
- MQTT fields filled from `GET /api/mqtt` when the card is shown.
- WiFi scrolls; Save and Reset reachable.

**Non-Goals:**

- New Status / ZigBee / Devices content.
- Auth on upload.
- Changing EEPROM settings schema.

## Decisions

### 1. Two Update cards, two endpoints

**Choice:** Keep `POST /update` for firmware (`U_FLASH`). Add `POST /update/data` (or `/update/fs`) with `Update.begin(..., U_FS)` / LittleFS partition. Second file input + progress on the Update tab, same XHR pattern as firmware.

**Why:** Matches `pio run -t upload` vs `uploadfs`. One POST cannot safely guess image type.

**Alternative:** Single file input that sniffs magic — rejected; operators have two distinct binaries.

### 2. Log layout

**Choice:** System Log panel is a column: hint, textarea `flex: 1; min-height: 0; overflow: auto`, then a left `form-actions` row. Refresh uses `.btn` without `flex: 1` (e.g. `.btn-inline { flex: 0 0 auto; width: auto; }`). Panel `overflow: hidden` so only the log box scrolls.

### 3. MQTT populate

**Choice:** Call `loadMqtt()` on `window.onload` **and** when `showMainSection('mqtt')`. Surface fetch/parse failure in `mqttSaveError` instead of an empty `catch`. Escape JSON strings on the server if passwords/topics can break `response.json()` (quote/backslash).

**Alternative:** Only fix CSS — rejected; empty fields are a data-bind issue.

### 4. WiFi scroll and Reset

**Choice:** Remove `#section-wifi { overflow: hidden }`. Use the same column + optional sticky footer as MQTT: fields `overflow: auto`, footer `flex-shrink: 0`. Rename Reload (`window.location='/'`) to **Reset** calling `loadWifi()`.

**Why:** Full page reload is what the user is missing; they asked for Reset.

## Risks / Trade-offs

- [Wrong image on FS slot] → Separate forms and endpoints; fail closed like firmware OTA.
- [Stale LittleFS after HTML change] → Apply notes say `uploadfs` (or the new form) after building `data/`.
- [64 KiB log in textarea] → Textarea is the only scroller so Refresh stays on screen.

## Migration Plan

1. Ship firmware + `data/` together.
2. First filesystem upload can be `pio run -t uploadfs` if the Update form is not on the device yet.

## Open Questions

- None that change the four bugs. Endpoint path `/update/data` vs `/update/fs` can follow existing ESP32 skill (`/update/data`) at apply time.
