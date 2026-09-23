## Why

The Devices table already shows type and per-channel status, but the only way to act is open the edit dialog and type a command. Operators need type-aware actions (On/Off per gang, covering move) without leaving the list.

## What Changes

- On hover, a registered-device row SHALL show a compact **…** control on the right of that row.
- Clicking **…** SHALL open a menu of actions for that device’s stored type, taking `channels` into account (one set of actions per channel when there is more than one).
- `onOff` SHALL offer **ON**, **OFF**, and **TOGGLE** per channel. `windowCovering` SHALL offer **OPEN**, **CLOSE**, and **STOP** per channel. Types with no radio actions SHALL not show **…**.
- Each action SHALL use the same host path as MQTT `set` / Manual command (`POST /api/devices/command`). Row click SHALL still open edit; **…** SHALL not open edit.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: hover row-actions menu by device type and channel count.

## Impact

- `data/index.html` + `data/css/all.css` (hover **…**, menu, stop row-click).
- Reuse existing command POST; no new SPI/MQTT topics.
- Firmware **0.2.10**.
