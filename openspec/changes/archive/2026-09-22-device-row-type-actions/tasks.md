## 0. Version

- [x] 0.1 Confirm `FIRMWARE_VERSION` is `0.2.10` in `Defines.h` (bumped at propose)

## 1. Row menu

- [x] 1.1 Show **…** on the right of a type-action row on hover (always on narrow viewports), and verify `iasZone` and `unknown` never show **…**
- [x] 1.2 Build the menu from type and `channels` (`onOff`: ON/OFF/TOGGLE; `windowCovering`: OPEN/CLOSE/STOP; multi-channel top level is action, submenu is channel numbers only), and verify a `channels` `3` on/off menu is ON/OFF/TOGGLE each with `1` `2` `3`
- [x] 1.3 Send the chosen action through `POST /api/devices/command` with the mapped channel, and verify STOP on covering channel 2 does not open edit
- [x] 1.4 Keep an open menu across Devices poll redraw, and verify clicking **…** or a menu item does not open the edit dialog

## 2. Build

- [x] 2.1 `pio run` succeeds for host and slave with `0.2.10`
