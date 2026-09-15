## 1. Persist FULL CONTROL

- [x] 1.1 Add `fullControl` to `DeviceTopicEntry` (default false) and thread it through upsert, `listJson`, and `replaceFromJson` so a missing JSON key stays off, and verify a round-trip of store JSON includes `"fullControl":true` only when set
- [x] 1.2 Accept `fullControl` on web `POST /api/devices` and MQTT `config/device` without changing SPI device-sync length, and verify a save without the field does not turn an existing off record on

## 2. Parse and SPI write

- [x] 2.1 Parse comma-separated `key=value` bodies (`cl`, `attr`, `val`, `ep`, `type`; decimal or `0x`) and fill absent keys with defaults (`cl` 0x0006, `attr` 0x0000, `val` 0, `ep` from mapping, `type` from val width or U8), and verify `cl=0x0102,val=0x1` yields cluster 0x0102, attr 0, value 1, type U8
- [x] 2.2 Add `SpiCmdZclWriteAttr` (0x0D) with 18-byte little-endian payload and host enqueue helper, and verify `pio run` succeeds
- [x] 2.3 After channels mapping, if FULL CONTROL is on and at least one attribute parsed enqueue the write with defaults; if none parsed enqueue `SpiCmdZclOnOff` with the entire remaining body; if the flag is off keep on/off only, and verify `ON` still sends on/off while `ch-2##cl=0x0006,attr=0x0000,val=1` writes endpoint 2

## 3. Slave transmit

- [x] 3.1 Decode `SpiCmdZclWriteAttr` on the slave and send ZCL write-attribute to the framed IEEE and endpoint, and verify a write for ep=4 logs that endpoint and does not use another cached bind endpoint

## 4. Web console

- [x] 4.1 Add a FULL CONTROL checkbox as the last field in the Devices parameter dialog (after AVAILABILITY, `field-check`, default unchecked), load it from GET JSON, and POST it on Save, and verify a new device shows it off at the end and checking it then saving returns `fullControl` true
