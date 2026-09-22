## ADDED Requirements

### Requirement: Devices row type actions
When the pointer is over a registered-device row that has type actions, the console SHALL show a compact **…** control on the right of that row. Activating **…** SHALL open a menu of actions for that row’s stored type. When `channels` is `2` through `16`, or `channels` is `0` with more than one known endpoint, the top-level menu SHALL list actions only (ON/OFF/TOGGLE or OPEN/CLOSE/STOP). Each action SHALL show a submenu marker and open a submenu whose items are channel numbers only (`1`, `2`, `3`). The submenu SHALL stay inside the visible Devices card so it does not add a card scrollbar. When `channels` is `1`, or `channels` is `0` with no extra endpoints, the menu SHALL list actions once with no channel submenu. `onOff` actions SHALL be ON, OFF, and TOGGLE. `windowCovering` actions SHALL be OPEN, CLOSE, and STOP. `iasZone` and `unknown` SHALL have no type actions and SHALL NOT show **…**. Choosing an action SHALL send that body on the same path as Manual command / MQTT `set` for that IEEE and channel. Activating **…** or a menu item SHALL NOT open the device edit dialog. The table column order SHALL stay online, name, type, status, battery, RSSI; **…** is an overlay, not a new column.

#### Scenario: Hover shows ellipsis
- **WHEN** the pointer is over a registered `onOff` row
- **THEN** a **…** control appears on the right of that row

#### Scenario: Single-channel on/off
- **WHEN** the operator opens **…** on an `onOff` device with `channels` `1` and chooses ON
- **THEN** the host sends `ON` on the same path as Manual command for that IEEE without opening edit

#### Scenario: Multi-channel on/off
- **WHEN** the operator opens **…** on an `onOff` device with `channels` `3`
- **THEN** the top-level menu is ON, OFF, and TOGGLE, and each action’s submenu is `1`, `2`, and `3` only

#### Scenario: Covering stop on channel 2
- **WHEN** the operator chooses STOP for channel 2 on a `windowCovering` device with `channels` `2`
- **THEN** the host sends `STOP` to endpoint 2 the same way MQTT `{set}/2` would

#### Scenario: No actions for IAS
- **WHEN** the pointer is over a registered `iasZone` row
- **THEN** **…** is not shown
