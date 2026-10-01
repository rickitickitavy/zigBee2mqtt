## Context

See proposal.md — Why. Today `WebConsole` serves `/` from LittleFS `/index.html` and `/css` via `serveStatic` on LittleFS; if files are missing it returns a PROGMEM page telling the operator to `uploadfs`. System → Update still posts to `/update/data` (`U_FS` / `U_SPIFFS`) for a full LittleFS image. Firmware OTA stages on host LittleFS (`FirmwareOta`, ADR 0008), flashes slave over SPI, then host `U_FLASH`. Partition map (`partitions/zigbee_zczr_16MB.csv`): dual OTA **4 MB** each, LittleFS (~`spiffs`) **~8 MB**, Zigbee fat slots, **coredump 64 KB**. Live `firmware.bin` ≈ **1.79 MB**; `data/index.html` + `data/css/all.css` ≈ **185 KB** raw / ≈ **26 KB** gzip-9. `FIRMWARE_VERSION` is `0.2.24`.

## Goals / Non-Goals

**Goals:**

- One firmware `.bin` carries a matching console UI.
- Keep editing real files under `data/`; generate embed blobs at build time.
- Preserve slave-then-host firmware OTA and LittleFS staging headroom.
- Remove UI filesystem OTA from the Update tab and server routes.

**Non-Goals:**

- Splitting host/slave PlatformIO envs so slave omits the console.
- Moving EEPROM/settings or slave device/user stores off LittleFS.
- Growing OTA slots to 5 MB without a measured size need.
- Hand-maintained C++ string literals as the long-term source of HTML/CSS.
- Combined custom “app+FS” multi-partition OTA format.

## Decisions

1. **Build-time gzip embed, not class `static char` arrays**  
   Keep `data/index.html` and `data/css/all.css` as the editable sources. A PlatformIO `pre:` script (or equivalent) gzip-compresses each file and writes generated headers (e.g. `include/generated/EmbeddedWebAssets.h`) with `const uint8_t[] PROGMEM`, length, MIME type, and a gzip flag. `WebConsole` serves with `beginResponse_P` / byte response plus `Content-Encoding: gzip` when compressed.  
   **Rejected:** Pasting HTML into classes — unmaintainable, no gzip, drifts from `data/`.  
   **Rejected:** Uncompressed PROGMEM only — wastes flash on dual OTA (stored in both slots).

2. **Keep 4 MB app0/app1 unless post-embed size forces growth**  
   1.79 MB + ~26 KB gzip leaves large margin under 4 MB. Growing both slots to 5 MB costs **2 MB** of LittleFS and hurts staging of future larger images. Revisit slot size only if `firmware.bin` approaches ~3.5 MB+.  
   **Rejected:** Default “increase to 5 MB” from the earlier sketch — not justified by current sizes.

3. **LittleFS stays for OTA staging; not for console assets**  
   Continue mounting LittleFS on host for `/ota` staging and any residual host files. Stop reading console paths from LittleFS. Ignore leftover `/index.html` / `/css` on disk after migration (embedded wins). Optionally delete those paths on boot later; not required for correctness.

4. **Remove `/update/data` and Update-tab filesystem UI**  
   Drop the route, `U_FS` upload handler branch used for web refresh, and the Filesystem card in `data/index.html`. Document that `uploadfs` is no longer required for the console. Factory USB flash remains firmware-only for UI bring-up (still flash both chips per ADR 0008).

5. **`coredump` reclaim is optional and tiny**  
   Removing `coredump` frees 64 KB into LittleFS. Do it only if nothing enables ESP core-dump to that partition; it is not required for embedding the UI.

6. **Cache headers**  
   Keep `Cache-Control: no-store` (or short max-age) on HTML so a just-updated firmware is not stuck behind an old cached shell; CSS may use the same for simplicity.

## Risks / Trade-offs

- **[Risk] Generated headers forgotten in clean builds** → Mitigation: wire the generator as a PlatformIO `pre:` extra script so every build regenerates; do not require committing generated files if the script is reliable (or commit them only if CI/IDE needs them — prefer generate-always).
- **[Risk] AsyncWebServer gzip quirks** → Mitigation: send explicit `Content-Encoding: gzip` and correct MIME; verify Chrome/Firefox on AP IP; fall back to uncompressed embed only if a client path fails.
- **[Risk] Operators still upload old LittleFS images** → Mitigation: remove UI control and return 404/410 from `/update/data` with a clear body if kept temporarily; prefer hard removal in the same change.
- **[Risk] Larger future UI** → Mitigation: stay on gzip; measure `firmware.bin` after UI growth before touching partition sizes.
- **[Trade-off] UI change requires full firmware OTA** → Accepted: that is the goal (version lock). Slave SPI OTA time stays dominated by the ~1.8 MB image, not the ~26 KB UI.

## Migration Plan

1. Build host/slave with embedded assets; flash USB once (both chips) or OTA firmware as today.
2. Confirm `/` and `/css/all.css` work without `uploadfs`.
3. Confirm System → Update no longer shows filesystem upload; firmware OTA still slave-then-host.
4. Rollback: flash previous firmware (console returns to that build’s assets if it was LittleFS-based, operators may need the matching `uploadfs` for that old build).

## Open Questions

- None that block specs or tasks: prefer generate-on-build without committing blobs unless an IDE limitation appears during apply.
