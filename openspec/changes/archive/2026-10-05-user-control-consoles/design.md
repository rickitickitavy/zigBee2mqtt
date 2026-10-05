# Design

## Context

See proposal.md — Why. Today `UserStore` holds `isAdmin`, `editDevices`, `addDevices`, `removeDevices`, `editUsers`, theme, and block; SPI packs those into `SPI_USER_SYNC_ENTRY_LEN` (87 bytes, payload max 256). Simple users still get the gateway `index.html` sidebar (Status, Devices, Log, Appearance). Devices already expose type, `channelCount`, On/Off, window covering, and measurement classes; `/api/devices` plus `/ws/devices` and `/api/devices/command` already drive live state. There is no console layout store. Security is a main sidebar section (not a System folder tab). Firmware at propose is `0.4.15`.

## Goals / Non-Goals

**Goals:**

- Host-only heap layout store (max 32 consoles) plus per-user assignment that survives reboot, user list, and settings export/restore, without a compile-time console/widget/binding table.
- A reserved free-heap floor so AsyncWebServer and Wi‑Fi keep buffers after layouts grow.
- Separate control page for simple users; operator chrome gains Consoles after Security.
- Shared live device APIs for view mode; editor dialog never issues commands.
- Same web chrome tokens; dialog overlay dismiss and dirty-close rules from web-ui.

**Non-Goals:**

- ESP32-S3 host / dual-image OTA (future).
- Scene/automation engine, custom JS widgets, or a visual floorplan.
- Per-device access ACLs beyond console assignment (a user who can see a console can control every binding on it).
- Pre-allocating 32 console slots, widget grids, or binding arrays in BSS/data.
- Putting 32 console-id lists on SPI (payload too small); assignments stay host LittleFS.
- Changing Zigbee classification or adding new ZCL types in this change (bind what the registry already knows; unknown types show title + raw state, no switch).
- Converting existing `UserStore` / `DeviceTopicMap` tables to heap.

## Decisions

1. **Separate page `data/user.html` (and gzip embed like `index.html`), not a hidden shell in `index.html`.**  
   Simple users must never load operator sections. Login stays on both pages; `/api/auth/me` after login redirects: no operator roles → `/user.html`; any of `isAdmin` / `edit*` → `/` (`index.html`). Visiting `/` as a simple user redirects to `/user.html`.  
   Alternative: one HTML file with two roots — rejected because a missed `hidden` leaks Wi-Fi/OTA chrome.

2. **`editConsoles` plus a heap list of console ids on each user (not a bitmask).**  
   JSON `consoles` is an array of stable string ids (max 32 entries, one per existing console). Allocate/free the id list with the user record. UI label: **Console editor**. Authorization is `isAdmin || editConsoles` (same pattern as other extra roles). Seed `admin` needs only `isAdmin`.  
   Alternative: `uint32_t` mask of slots 0–31 — rejected; it is a fixed-width table and fights heap-only storage. Alternative: store assignment only on the console (`users: []`) — then user dialog checkboxes and user export are harder to keep consistent. **Source of truth is the user record**; the Consoles table **Users** column is computed by scanning users.

3. **Console identity is a stable `id` string (8 hex chars) plus display `name`.**  
   Assignments survive a rename. New consoles get a random id at create. Import into an empty console keeps the **current** id and name unless the editor has not set a name yet. At most **32** consoles.

4. **`ConsoleStore` on LittleFS (`/consoles.json`), heap nodes, not SPI.**  
   Grow a linked list (or heap vector) of consoles; each console owns a heap list of widgets; each widget owns a heap list of bindings. No `ConsoleRecord consoles[N]`. Layouts are host-web only. Slave does not render consoles. User SPI: add `SPI_USER_FLAG_EDIT_CONSOLES = 0x40` only. Do **not** pack console ids on SPI. Unpack: merge `consoles` from the existing host user of the same name so a slave dump cannot wipe assignments.

5. **Heap floor for HTTP/Wi‑Fi (`CONSOLE_HEAP_RESERVE_BYTES`, 48 KiB unless measurement on the host shows a safer value).**  
   Before malloc of a console, widget, binding, or assignment node, if `ESP.getFreeHeap()` minus the request would be below that floor (or `max_alloc` cannot fit the node), refuse and return an error. Free nodes on delete. Do not keep a second full copy of all layouts in RAM besides the store plus the one editor buffer. Widget/row counts are limited by this floor, not by 12/16 static caps.

6. **Existing `UserStore` `users[USER_STORE_MAX]` and `DeviceTopicMap` slots stay as they are.** This change does not convert those tables to heap.

7. **Widget / device edit UX.**  
   Large dialog: console name, Active checkbox, toolbar (Export, Import, Save, Cancel), then the grid.  
   - Widget header: drag handle, **title field**, **Group On/Off** checkbox (show control), delete (confirm).  
   - View preview in the dialog is visual only; pointer on switches/covers does **not** call `/api/devices/command` while the dialog is open (`editMode` flag).  
   - Add device: per-widget button (edit only) opens a nested dialog: device `<select>` from registered store, channel `<select>` if `channelCount > 1` (1..N), title, unit `<select>` if `zigbeeDeviceTypeIsMeasurement`. Clicking an existing row reopens that dialog. HTML5 drag-and-drop for widgets (grid cells) and for vertical device lists.  
   - Add-widget control is a dashed card in the same grid cell style, always last.

8. **Group On/Off semantics.**  
   Consider only bindings whose registered type is On/Off (not covering, not IAS, not sensors). Displayed On iff any of those channels reports On. Command: group Off → Off to each; group On → On to each. Partial (mixed) still displays On per spec.

9. **Units.**  
   Flash `const` unit catalogs per measurement class (`C`/`F`/`K`, `Pa`/`hPa`/`kPa`/`bar`/`psi`, `%`, `lx`, `m/s`/`km/h`, degrees, etc.) are allowed. Store the chosen unit id on the heap binding. Display converts from the gateway’s native value when a conversion is known; otherwise show native + unit label.

10. **Live user page.**  
    Snapshot devices on open (existing GET), then `/ws/devices` for changes. Render only bindings on the selected tab. Commands: existing `/api/devices/command` (simple users already allowed). Sign out on this page. Theme from the user record. Folder-style **top tabs** (`.tabs`) for console names — not a second sidebar.

11. **APIs.**  
    - `GET/POST /api/consoles` list/create (`editConsoles` or `isAdmin`).  
    - `GET/PUT/DELETE /api/consoles/:id` (`GET` also for simple users if assigned and active).  
    - `GET /api/consoles/mine` for the user page (assigned ∩ active).  
    Export/import are client JSON file read/write of one console object; Import POSTs as a normal save only when the in-memory editor layout has zero widgets. Empty means zero widgets (name/active may already be set).

12. **Settings export.**  
    Add top-level `consoles` array (full layouts). Users already in export gain `editConsoles` and `consoles`. Restore replaces the console store when the key is present; omit key → leave consoles unchanged.

## Risks / Trade-offs

- **Simple users lose Status/Devices.** → Spec BREAKING; operators keep the gateway.  
- **Heap fragmentation / Wi‑Fi stalls.** → Refuse grow under `CONSOLE_HEAP_RESERVE_BYTES`; one editor buffer; free on delete. Flash host and slave together so `editConsoles` unpacks.  
- **Stale bindings** (device deleted). → Row shows the title and “unavailable”; commands no-op; editor can remove.  
- **JSON size on C6.** → 32-console cap plus heap floor; gzip UI already used.  
- **Group On/Off vs HA.** → HA sometimes uses “on if any”; we lock that (specified). Mixed state looks On.

## Migration Plan

Existing users: `editConsoles` false, empty heap `consoles` list. Empty `/consoles.json` until an editor saves. Deploy one firmware to host and slave so the new role flag is understood. Old firmware ignores unknown role bits.

## Open Questions

None that block apply. Extra widget types can be added later without changing assignment or the user page shell.
