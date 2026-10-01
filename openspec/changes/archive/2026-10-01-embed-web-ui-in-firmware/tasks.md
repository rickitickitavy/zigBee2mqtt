## 1. Version and build embed

- [x] 1.1 Confirm `FIRMWARE_VERSION` is `0.2.25` in `Defines.h` (propose `0.2.24`, apply bump)
- [x] 1.2 Add a PlatformIO `pre:` script that gzip-compresses `data/index.html` and `data/css/all.css` into a generated header (PROGMEM byte arrays + lengths + MIME), wire it in `platformio.ini`, and verify a clean `pio run` regenerates the header and links successfully
- [x] 1.3 After that build, confirm `firmware.bin` size still fits comfortably under the 4 MB OTA slot (leave ≥~0.5 MB margin) and do **not** enlarge app0/app1 unless it does not

## 2. Serve embedded console

- [x] 2.1 Change `WebConsole` to serve `/` and `/index.html` from the embedded HTML (with `Content-Encoding: gzip` when compressed) and remove the LittleFS-missing fallback page, then verify a host without web files on LittleFS still returns the console HTML
- [x] 2.2 Replace `serveStatic("/css", LittleFS, ...)` with an embedded `/css/all.css` (and any other needed CSS paths) response, and verify the signed-in console loads styles without LittleFS CSS
- [x] 2.3 Keep `Cache-Control: no-store` (or equivalent) on the HTML response and verify a hard refresh after OTA shows the new page shell

## 3. Remove filesystem UI update path

- [x] 3.1 Remove the System → Update Filesystem (LittleFS) card and `uploadFilesystem` / `POST /update/data` client code from `data/index.html`, regenerate embeds, and verify the Update tab shows firmware upload only
- [x] 3.2 Remove the host `POST /update/data` route and filesystem `Update` branch used for web refresh, and verify that path is gone (404) while `POST /update` firmware staging still works
- [x] 3.3 Update README (and any flash notes) so console bring-up no longer requires `uploadfs`, and verify docs match the single-firmware workflow

## 4. Integration check

- [ ] 4.1 Flash host (and slave if exercising SPI OTA) with the new image only, open `/` on AP or STA, and verify login + styled console without a filesystem flash
- [ ] 4.2 Run a firmware OTA through System → Update and verify slave-then-host progress still works and the console UI matches the new build after reboot
- [x] 4.3 Confirm LittleFS still mounts and firmware OTA staging still has enough free space for the current `firmware.bin`
