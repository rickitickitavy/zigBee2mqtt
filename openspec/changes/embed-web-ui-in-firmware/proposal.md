## Why

Operators must flash two artifacts today (`firmware.bin` plus a LittleFS image) and can end up with firmware and console HTML/CSS from different builds. One firmware OTA file should deliver a matching web console so host/slave SPI OTA stays a single upload path.

## What Changes

- Embed the host web console assets (`data/index.html`, `data/css/all.css`) into the application image at **build time** (prefer gzipped PROGMEM blobs generated from the real files — not hand-maintained `static char` arrays in classes).
- Serve `/` and `/css/*` from those embedded assets with the correct MIME type and `Content-Encoding: gzip` when compressed; stop depending on LittleFS for console HTML/CSS.
- **BREAKING (operator workflow):** remove the System → Update **filesystem** upload (`POST /update/data` / LittleFS image) as the way to refresh the console UI. Console updates ship only with firmware OTA.
- Keep LittleFS for OTA staging (`/ota/...`), not as the source of truth for the web UI. Do **not** grow dual OTA app slots to 5 MB unless measured image size requires it: current slots are 4 MB and the live `firmware.bin` is ~1.8 MB; gzipped UI adds ~26 KB. Growing both OTA slots to 5 MB would remove ~2 MB from LittleFS and shrink staging headroom.
- Optionally reclaim the unused `coredump` partition (~64 KB) into LittleFS only if crash-dump capture is not needed; that is a small gain, not the main lever.
- Bump `FIRMWARE_VERSION` build to `0.2.24` at propose time.

## Capabilities

### New Capabilities

- _(none)_

### Modified Capabilities

- `web-console`: Console HTML/CSS MUST come from the firmware image (version-locked). Update tab MUST no longer offer a separate LittleFS/web-filesystem flash for UI files. A single firmware upload MUST refresh both application and console UI after the existing slave-then-host OTA flow.

## Impact

- `WebConsole` static routes (`/`, `/index.html`, `/css`), fallback “missing LittleFS” page, and `POST /update/data`
- Update tab in `data/index.html` (remove filesystem upload UI)
- Build pipeline (`platformio.ini` / pre-build script) to generate embedded asset headers from `data/`
- Partition CSV only if sizing or `coredump` reclaim is chosen after measuring post-embed binary size
- README / flash instructions (`uploadfs` no longer required for the console)
- `FIRMWARE_VERSION` in `Defines.h` → `0.2.24`
- Slave image still builds the same firmware binary for SPI OTA; unused console code on slave remains acceptable unless a later change splits envs
