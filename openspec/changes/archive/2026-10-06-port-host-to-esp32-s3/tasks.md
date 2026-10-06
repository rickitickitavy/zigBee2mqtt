# Tasks

## 1. ADR and build split

- [x] 1.1 Supersede `docs/adr/0002-two-chips-one-image.md` with a new ADR: two chips, two images, one joined Update ZIP
- [x] 1.2 Add PlatformIO `esp32-s3-host` env (N16R8-class board, no Zigbee ZCZR libs) and keep/adapt `esp32-c6-slave` env (ZCZR)
- [x] 1.3 Introduce compile-time `BOARD_ROLE_HOST` / `BOARD_ROLE_SLAVE` (or equivalent) and remove runtime GPIO15 role sampling from product boot

## 2. Pins and board I/O

- [x] 2.1 Add host pin map for ESP32-S3 per design (SPI 11/12/13/10, IRQ 14, EN 9, LEDs 4–7/15/16, boot 41); exclude flash/PSRAM GPIOs 26–37
- [x] 2.2 Keep slave C6 SPI/IRQ/LED pins as today unless PCB requires change; leave role pin unused
- [x] 2.3 Update `status-rgb` / button code paths to use the host pin header only on S3 host builds

## 3. Joined firmware package

- [x] 3.1 Define ZIP members `slave.bin` + `host.bin` (DEFLATE or STORE)
- [x] 3.2 Add `scripts/firmware_join.py` and `scripts/firmware_split.py` (or equivalent) and document usage
- [x] 3.3 Change host OTA staging to accept the joined ZIP, validate members, extract slave then host
- [x] 3.4 Preserve slave-then-host apply order and failure rules (no host apply if slave fails)
- [x] 3.5 Ensure LittleFS free-space checks cover ZIP staging (and extraction scratch if needed)

## 4. Web console

- [x] 4.1 Update System → Update UX copy to say joined package / ZIP if needed
- [x] 4.2 Keep single-file upload; verify progress phases still show slave then host

## 5. Verification

- [x] 5.1 Build host (S3) and slave (C6) images separately
- [ ] 5.2 Join → upload via web → slave updates → host updates → both report new versions
- [x] 5.3 Split a joined ZIP and confirm member binaries match inputs
- [ ] 5.4 Confirm SPI bring-up (reset, `SLAVE_READY`, settings, ping/Zigbee) on the new pin wiring
