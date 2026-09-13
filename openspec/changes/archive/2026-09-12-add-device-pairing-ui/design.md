## Context

See proposal.md for motivation. Today `GlobalSettings` is one EEPROM blob (`EEPROM.begin(4096)`): marker, version, wifi, mqtt, zigbee, then `devices[DEVICE_MAP_SLOTS]`. `saveSetting` writes every byte then `commit()`. Joins already arrive as `SpiEvtDeviceJoin`; MQTT `config/device` already upserts the host map. The Devices card is empty. Slave pairing (permit join + LED) already exists.

## Goals / Non-Goals

**Goals:**
- Stable main-block layout with 256-byte reserved tail (128-aligned) and the device list after it
- Versioned migrate from v4 so existing wifi/mqtt/zigbee and device slots survive
- Save APIs that write only dirty EEPROM addresses (per slot or per main block)
- Host RAM found-list + HTTP for the two dialogs; Search maps to existing permit-join SPI
- 128 registered-device slots on the host

**Non-Goals:**
- Storing the operator map on the slave
- New SPI commands (reuse permit join and join events)
- Device delete/edit UI (can follow later)
- WebSockets; search dialog may poll found devices
- Changing NVS instead of EEPROM

## Decisions

### 1. Settings layout (v5)

Keep one `GlobalSettings` in EEPROM.

1. Main fields: marker, version, wifi, mqtt, zigbee (same meanings as v4)
2. Padding so `offsetof(reserved) % 128 == 0`
3. `uint8_t reserved[256]` (zeros until a later change uses them)
4. `DeviceTopicEntry devices[128]` immediately after reserved, stored on LittleFS (not inside the 4 KiB EEPROM blob)

**Why:** 128 × ~225 bytes is about 29 KiB. EEPROM is 4 KiB and NVS is 20 KiB, so the list cannot live in `EEPROM.begin(4096)`. Main + 256-byte reserve stay in EEPROM; the device file is the persistent “after reserve” list. `DEVICE_MAP_SLOTS` becomes 128.

**Alternative:** Keep all 128 slots in one EEPROM image. Rejected: it does not fit the nvs/EEPROM partition.

### 2. Partial EEPROM writes

Keep a committed snapshot in RAM. `saveMain()` / `saveDeviceSlot(index)` compute the byte range, `EEPROM.write` only addresses that differ from the snapshot, then `commit()`, then refresh the snapshot.

**Why:** Stops blindly looping `sizeof(GlobalSettings)` on every save. Slot save does not mark the main block dirty.

**Trade-off:** On ESP32 Arduino, `EEPROM.commit()` often persists the whole mapped region. Dirty addressing still avoids rewriting unchanged cache bytes and is the right API if the backend later supports range commit. Do not switch to Preferences/NVS.

### 3. Found devices are host RAM only

On `SpiEvtDeviceJoin`, if IEEE is not registered, upsert into a small RAM table (IEEE, short addr, endpoint, manufacturer, model). Cleared when search stops or Add succeeds for that IEEE.

**Why:** Spec says join does not register. No EEPROM until parameter Save.

**Alternative:** Auto-register on join. Rejected.

### 4. HTTP surface (host)

- `GET /api/devices` — registered rows
- `POST /api/devices` — save one registered record (ieee + friendlyName + topics); no restart
- `GET /api/devices/found` — RAM found list
- `POST /api/devices/search` — permit join (default 180 s or stored permit-join-on-boot)
- `POST /api/devices/search/stop` — close join, clear or freeze found list

Search dialog polls `GET /api/devices/found` about once per second while open.

**Alternative:** SSE. Rejected to stay on the existing fetch-style console.

### 5. Parameter dialog fields

IEEE read-only. Editable: friendly name, state topic, command topic, availability topic (existing `DeviceTopicEntry`). Prefill topics from MQTT `baseTopic` + a slug of the name when the name first changes; operator may edit. Friendly name required on Save.

### 6. Search duration and LED

Search uses the same slave permit-join path as MQTT. Closing the search dialog sends stop so the slave LED pairing blink ends.

## Risks / Trade-offs

- [v4 migrate offset mistakes] → Unit-style fixture: pack a v4 blob, load, assert wifi SSID and one device IEEE survive
- [ESP32 commit still wears a full EEPROM blob] → Slot-level dirty writes anyway; do not save main+devices together unless both changed
- [Join flood fills found table] → Cap the RAM table; drop oldest unregistered
- [Search while SPI is unhappy] → Show an error if permit-join enqueue fails; table can stay empty

## Migration Plan

1. Bump `GLOBAL_CURRENT_SETTINGS_VERSION` to 5
2. On load of v4: copy marker/wifi/mqtt/zigbee into the new main block, zero reserved, copy each used `DeviceTopicEntry` into the new array, write v5 (full write once)
3. Fresh boards: defaults + empty devices + zero reserved
4. Rollback: flash old firmware only if operator accepts losing v5 layout (old code would misread devices)

## Open Questions

None that block the task list. Slot count is **128**.
