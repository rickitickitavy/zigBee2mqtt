## 1. StatusRgb host LED split

- [x] 1.1 Add host update-held API and a 10 Hz (50 ms half-period) blink timer in `StatusRgb`, and verify `setUpdateHeld` / service toggles without affecting slave pairing timing
- [x] 1.2 Change host `apply()` so boot blinks only LED5 (0.25 s), critical holds LED6 solid, update blinks LED6 at 10 Hz when not critical, and LED5 never shows critical; verify host role paths in code review against the delta scenarios
- [x] 1.3 Keep slave `apply()` boot/critical on LED5 and pairing on LED6 unchanged, and verify slave path still uses the combined fault blink on LED5

## 2. Wire host OTA and docs

- [x] 2.1 From the host main loop (or OTA pump path), call `STATUS_RGB.setUpdateHeld(FIRMWARE_OTA.busy())` every cycle, and verify LED6 10 Hz runs across Receiving / Slave / Host / Rebooting then clears when idle
- [x] 2.2 Confirm existing `setCritical` / `setBootHeld` call sites need no semantic change, and verify lost-slave still toggles critical while boot clear leaves LED5 for MQTT only
- [x] 2.3 Update `README.md` status LED table for host LED5/LED6, and verify it matches `openspec/changes/host-led6-critical-ota/specs/status-rgb-led/spec.md`

## 3. Device check

- [ ] 3.1 Flash host, confirm boot LED5 blink then MQTT solid on LED5, force lost slave → LED6 solid with LED5 still MQTT if connected, run System → Update → LED6 10 Hz while busy
