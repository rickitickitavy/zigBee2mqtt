# Tasks

## 1. StatusRgb color map and PWM

- [x] 1.1 Add per-role LED color tables (host/slave maps from design) and LEDC setup for blue/green pins in `StatusRgb`; keep red on `digitalWrite` — verify host and slave builds compile with `BOARD_ROLE_HOST` / `BOARD_ROLE_SLAVE`
- [x] 1.2 Implement `setColorBrightness(blue, green)` (clamp 1–100, default 100) and use duty in `writeLevels` for blue/green when on — verify a green LED on at 50 looks dimmer than at 100, and a red LED stays full digital on
- [x] 1.3 Confirm logical-off still yields zero duty / LOW for all colors — verify boot/off states leave unused LEDs dark

## 2. Host settings and Hardware API

- [x] 2.1 Add reserved offsets and `SettingsManager` get/set/clamp for `blueLedBrightness` / `greenLedBrightness` (default 100; 0 in EEPROM → 100) — verify factory reset and cold boot report 100/100
- [x] 2.2 Extend `/api/hardware` GET/POST and `applyHardwareJson` for both fields; apply to `STATUS_RGB` on save — verify POST rejects 0/101 and GET returns saved values
- [x] 2.3 Include brightness in settings export JSON and restore (missing keys keep current) — verify export/restore round-trip of brightness and SPI speed

## 3. Hardware tab UI

- [x] 3.1 Add BLUE and GREEN LED brightness controls (1–100) on System → Hardware; wire load/save with existing SPI speed — verify admin UI loads defaults, saves, and Reset reloads stored values

## 4. Host→slave brightness SPI

- [x] 4.1 Add `SpiCmdSetLedBrightness` (two-byte payload) on host and slave; apply on slave without starting Zigbee — verify radio stays up when brightness is pushed after normal work
- [x] 4.2 Push brightness after settings OK / when linked, and again on Hardware save/restore — verify slave green/blue LEDs match host percentages after save and after slave reboot + re-link

## 5. Other device control role

- [x] 5.1 Add `controlDevices` to `ConsoleUser`, UserStore JSON, SPI flags, and Security dialog checkbox labeled Other device control — verify save/list/export/reboot round-trip
- [x] 5.2 Hide Devices row **…** and Manual command unless `controlDevices` or `isAdmin`; reject matching command APIs without the role — verify a non-admin without the role cannot send ON from Devices, and with the role can

## 6. Login Enter and plate auto-hide

- [x] 6.1 On login form Enter/submit with empty password, focus password and do not POST — verify empty password Enter focuses password; filled password still logs in
- [x] 6.2 Auto-hide every shown `.save-error` / `.save-success` after 10 s (shared helper) — verify a success plate disappears ~10 s after Save and a newer message replaces the previous plate cleanly

## 7. Integration

- [x] 7.1 Build and flash joined host+slave firmware; smoke-check LED maps, brightness, Devices role gating, login Enter, and plate hide — verify status LED meanings unchanged (boot, MQTT, broker, pairing, pulses)
