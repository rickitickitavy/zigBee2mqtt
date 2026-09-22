## 0. Version

- [x] 0.1 Confirm `FIRMWARE_VERSION` is `0.2.8` in `Defines.h` (bumped at propose)

## 1. SPI and radio cluster command

- [x] 1.1 Add `SpiCmdZclCommand` `0x11` plus pack/unpack (IEEE, endpoint, cluster LE, command id, payload length, payload) and verify a no-payload covering-stop frame round-trips endpoint 1 / cluster `0x0102` / command `0x02`
- [x] 1.2 Slave handles the command with `sendZclToDevice` cluster-specific, latest-wins with on/off and write-attr, and verify a missing short address does not stop the coordinator
- [x] 1.3 Host `ZigbeeSpiProxy` enqueues the new command and verify the slave is given endpoint 3 when that endpoint was provided

## 2. MQTT and Manual command parse

- [x] 2.1 Parse covering shortcuts and `cl`/`cmd` (optional payload) after FULL CONTROL write-attr and before on/off, and verify `OPEN` enqueues `0x0102`/`0x00` while `ON` still enqueues on/off
- [x] 2.2 Apply the same parse from MQTT `set` (including `{command}/1` in suffix mode) and from `POST /api/devices/command`, and verify `STOP` on `/1` targets endpoint 1 and Manual command `cl=0x0102,cmd=0x02` matches MQTT

## 3. Settings export and restore

- [x] 3.1 Add `GET /api/settings/export` that returns version, mqtt, zigbee, hardware.spiSpeedHz, and `listStoreJson` devices, and verify the body has no wifi keys and includes a registered device
- [x] 3.2 Add `POST /api/settings/restore` that ignores wifi, writes mqtt/zigbee/hardware, upserts listed devices and deletes IEEEs not in the file, and verify a file with a different wifi password leaves Wi-Fi unchanged and a missing IEEE is removed
- [x] 3.3 Put compact stacked Export settings and Restore settings on System → Maintenance with a confirm overlay, and verify the buttons are `btn-inline`, confirm cancel writes nothing, and confirm apply downloads/posts as above

## 4. Build

- [x] 4.1 `pio run` succeeds for host and slave with `0.2.8`
