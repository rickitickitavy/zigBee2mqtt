# Tasks

## 1. User fields and SPI

- [ ] 1.1 Add `editConsoles` and a heap-allocated list of console id strings on each user (no bitmask, no static id array); persist JSON `consoles`; free the list on user delete; verify one-user save/reload and users list include both after reboot.
- [ ] 1.2 Add `SPI_USER_FLAG_EDIT_CONSOLES` on the existing role byte (do not pack console ids on SPI); merge host `consoles` by user name on unpack; verify host↔slave user sync does not clear assignments.
- [ ] 1.3 Include `editConsoles` and `consoles` in user public JSON, export JSON, and settings restore parse; verify export/restore round-trips them without plaintext passwords.

## 2. Console store and APIs

- [ ] 2.1 Add heap `ConsoleStore` (`/consoles.json`): linked/grown consoles (max 32), each with heap widget and binding lists; `CONSOLE_HEAP_RESERVE_BYTES` (48 KiB unless host measurement needs more) checked before every grow; verify save, list, reboot, 33rd console error, low-heap error, and that free heap stays above the floor after a max-size save.
- [ ] 2.2 Wire `GET/POST /api/consoles`, `GET/PUT/DELETE /api/consoles/:id`, `GET /api/consoles/mine` with (`isAdmin` or `editConsoles`) for mutate and assigned+active for simple-user GET; verify an `isAdmin` session with `editConsoles` false can save, and a simple-user POST is forbidden.
- [ ] 2.3 Add `consoles` to settings export/restore (replace store when key present); verify admin export contains layouts and restore replaces them.

## 3. Operator Consoles UI

- [ ] 3.1 Add sidebar **Consoles** immediately after Security when `isAdmin` or `editConsoles`; table columns Console name, Active, Users (names computed from user `consoles`); verify an admin with `editConsoles` unset still sees the section and simple users never see the button.
- [ ] 3.2 Add Console editor checkbox and a checkbox group of existing consoles on the user dialog; verify save stores multiple assignments and the Consoles table Users column updates.
- [ ] 3.3 Open a large edit dialog (new or existing): name, active, widget grid, add-widget tile last, widget title + Group On/Off checkbox, per-widget add device, nested device dialog (device, channel if `channelCount > 1`, title, unit for measurement classes), drag-reorder widgets and rows, click row to re-edit, confirm delete widget/console; verify overlay mousedown-dismiss and dirty close prompt; verify no `/api/devices/command` while the dialog is open.
- [ ] 3.4 Add Save, Export Console (JSON download), Import from JSON enabled only when the editor has zero widgets; verify import fills an empty console and stays disabled when a widget exists.

## 4. User control page

- [ ] 4.1 Add `data/user.html` (chrome tokens, top `.tabs`, sign out), gzip-embed it like `index.html`, serve `/user.html`; after login redirect simple users there and operators to `/`; verify a simple user never sees the gateway sidebar.
- [ ] 4.2 Render assigned active consoles as tabs; widget cards with optional group On/Off (On if any bound On/Off is On; toggle commands all On/Off rows); On/Off switch, covering up/down/stop + position, sensor value+unit; device snapshot then `/ws/devices`; verify commands go to existing device APIs and inactive/unassigned consoles are omitted.
- [ ] 4.3 Empty state when the user has no assigned active consoles; deleted-device bindings show unavailable and do not command; verify Light/Dark from the user theme.

## 5. Apply version

- [ ] 5.1 While applying this change, increment `FIRMWARE_VERSION` build by 1 from `0.4.15` and verify `pio run -e esp32-c6-devkitc-1` succeeds. Flash host and slave together so the `editConsoles` SPI flag matches.
