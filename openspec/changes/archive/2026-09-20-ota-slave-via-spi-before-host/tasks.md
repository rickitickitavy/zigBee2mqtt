# Tasks

## 1. SPI firmware OTA protocol

- [x] 1.1 Add a host-to-slave firmware OTA command (begin / data / last flags, payload ≤ `SPI_MAX_PAYLOAD`) and verify a packed begin+chunk+end round-trips without colliding with existing command ids
- [x] 1.2 On the slave, queue those frames off the SPI ISR, write the inactive app slot, commit only on successful end, and verify a truncated or CRC-failed chunk leaves the running app unchanged
- [x] 1.3 On the host, pump one in-flight chunk from the staging file, wait for result or timeout, abort the sequence on failure, and verify a missing slave reply does not enqueue the rest of the file

## 2. Host staging and order

- [x] 2.1 Change `POST /update` to write LittleFS staging (reject if not enough space or an update is already running) and return success only when the file is stored, and verify the host OTA slot is not committed at that point
- [x] 2.2 After slave commit success, program the host from the staging file, delete the file, restart the host, and verify a slave failure skips host `Update` and does not restart
- [x] 2.3 Leave `POST /update/data` as host-only filesystem OTA and verify a LittleFS upload still does not start slave firmware OTA

## 3. Console progress

- [x] 3.1 Add an update-status GET that reports idle / receiving / slave / host / failed / done / rebooting with upload, slave, and host percents (plus an error string on failed) and verify it matches the state machine during a transfer
- [x] 3.2 After firmware HTTP upload, poll that status on the Update tab and show all phase percents (not only the XHR upload bar), and verify a slave failure message appears without claiming a restart
- [x] 3.3 When host apply finishes, show a reboot wait dialog with a 30 second timer, ping `/api/version` until ready, and verify success vs timeout messaging

## 4. Version and flash

- [x] 4.1 Set `FIRMWARE_VERSION` from `0.1.0` to `0.2.0` and verify Status and the Update tab (and `/api/version`) show `0.2.0`
- [ ] 4.2 USB-flash host and slave with the new image, upload a firmware `.bin` from System → Update, and verify the slave restarts into it before the host does and a second upload with SPI disconnected reports failure without host restart
