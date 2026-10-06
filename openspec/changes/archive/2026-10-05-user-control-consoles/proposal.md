# Proposal

## Why

Simple users (no operator roles) still land in the gateway console with Status and Devices. Household users need a separate control UI: named consoles, widgets, and on/off / cover / sensor controls only. Editors need a place to build those layouts and assign them to users.

## What Changes

- Add persisted **control consoles** (name, active flag, widget grid, stacked devices per widget) on the host. Cap is **32** consoles. Console, widget, binding, and per-user assignment storage MUST be **heap-allocated** (no fixed static arrays for those). Allocations MUST leave a reserved free-heap floor for AsyncWebServer and Wi‑Fi.
- Add user role **`editConsoles`** (UI: Console editor). Treat “Content editor” as the same role. **`isAdmin` already includes this and every other privilege**; the host MUST NOT require `editConsoles` to be stored or checked separately for an admin.
- Add user attribute **`consoles`**: multiple assigned console ids. User edit shows a checkbox group of existing consoles.
- After login, a **simple user** MUST open a **separate** control console page (not the gateway sidebar). Top tabs are the active consoles assigned to that user.
- Operators with `editUsers` or `isAdmin` keep Security. After Security, add sidebar **Consoles** for `editConsoles` or `isAdmin`: table Console name, Active, Users; large edit dialog.
- Widget grid is drag-reorderable. An add-widget tile sits after the last widget and moves down when a widget is added. Each widget has a title, optional group On/Off (ON if any contained On/Off device is ON), and edit-only Add device. Device rows: On/Off switch; window covering up / down / stop plus position; sensors show value and chosen unit. Drag-reorder and click-to-edit devices in edit mode. Edit mode MUST NOT send device commands. Save persists. Export JSON always; Import JSON only when the console being edited is empty.
- **BREAKING** for simple users: they no longer use Status / Devices / System after login; they use the control console page only.
- ESP32-S3 host remains a future change; not in this work.
- Firmware `FIRMWARE_VERSION` is `0.4.15` for this proposal.

## Capabilities

### New Capabilities

- `control-consoles`: Persist, edit, import/export, and run user-facing control consoles (widgets, device bindings, live controls, assignment).

### Modified Capabilities

- `console-users`: Add `editConsoles` and multi-console assignment; persist `consoles` on the host (SPI carries the role flag; assignment merges so slave sync cannot wipe it).
- `web-console`: Consoles sidebar after Security; user dialog checkboxes; simple-user login goes to the control page; settings export/restore includes consoles.

## Impact

Host `UserStore` / `WebConsole` / LittleFS, heap `ConsoleStore`, SPI `editConsoles` flag (console id lists stay host LittleFS and merge on user sync), `data/index.html` (Security, user dialog, Consoles section), new user control page, existing device list/action and devices WebSocket APIs for live values. Slave Zigbee stack unchanged except the extra user role flag. Settings export JSON grows a `consoles` group.
