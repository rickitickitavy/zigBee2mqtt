## 0. Version

- [x] 0.1 Confirm `FIRMWARE_VERSION` is `0.2.9` in `Defines.h` (bumped at propose)

## 1. Persist and HTTP

- [x] 1.1 Store theme as `reserved[4]` (`0` light, `1` dark) without moving SPI speed bytes, and verify an unset reserved byte reads as light
- [x] 1.2 Add `GET`/`POST /api/theme` that reads/writes `{ "theme": "light"|"dark" }` with `saveMain(false)`, and verify POST dark then GET returns dark without restart
- [x] 1.3 Include `ui.theme` in settings export and apply it on restore when present, and verify a file with dark theme and a different wifi password leaves Wi-Fi unchanged and GET theme is dark

## 2. Chrome

- [x] 2.1 Drive idle tabs, primary button fill/label, and other leftover light hex from CSS tokens, and verify Light still matches today’s gray/white chrome
- [x] 2.2 Add `html[data-theme="dark"]` slate-night tokens (page `#0E141B`, surface `#18222D`, accent `#6A9FD8`, text `#E4EAF0`, muted `#8A97A6`, dark primary labels), and verify sidebar, cards, fields, dialogs, tables, log, and buttons all switch

## 3. Maintenance UI

- [x] 3.1 Put Theme Light/Dark on System → Maintenance with an icon Save immediately to the right, and verify the picker applies dark immediately and Save is not a full-width bar
- [x] 3.2 Load GET theme on page open and persist only on the icon, and verify reload without Save returns to the last saved theme

## 4. Build

- [x] 4.1 `pio run` succeeds for host and slave with `0.2.9`
