# Tasks

## 1. Type ids and SPI join

- [x] 1.1 Add shared type ids (`unknown`/`onOff`/`iasZone`/`windowCovering` and SPI bytes 0–3) plus a cluster-priority classifier (`0x0500` then `0x0102` then `0x0006`) and verify a unit-style table of cluster lists maps to the expected id
- [x] 1.2 Extend `SpiEvtDeviceJoin` to 76 bytes with type at offset 75; parse `length >= 76` on the host and default `unknown` when shorter; verify a packed join round-trips type without moving IEEE or manufacturer offsets

## 2. Slave classification

- [x] 2.1 After a usable IEEE and endpoint are known, request simple descriptor(s), classify, then `enqueueDeviceJoin` with that type (timeout still sends `unknown`); verify a second join is sent if a later descriptor upgrades `unknown`
- [x] 2.2 Pair an On/Off device and an IAS Zone device (or inject cluster lists) and verify the join type is `onOff` vs `iasZone` even if both expose On/Off

## 3. Host found list and persistence

- [x] 3.1 Store type on found devices and include `"type"` in `/api/devices/found`; verify Search JSON shows `onOff` for a classified join
- [x] 3.2 Add `zigbeeType` to `DeviceTopicEntry`; read/write `"type"` in `listJson` / LittleFS load; verify a file without `type` loads as `unknown`
- [x] 3.5 Carry `zigbeeType` on `SpiCmdSetDevice` / dump; verify unpack of a full entry keeps type and a shorter legacy entry does not wipe a known host type
- [x] 3.3 On first save of a found IEEE, copy found type; POST body `type` is stored when the record is still `unknown`; on later POST, ignore client type when stored type is not `unknown`; verify a tampered POST cannot change `onOff`
- [x] 3.4 When a join arrives for a registered `unknown` IEEE, update and save the type without adding it to the found list; verify `windowCovering` fills in

## 4. Console

- [x] 4.1 Add a Type column to the pairing found table with the spec labels and verify an `onOff` row shows On/Off
- [x] 4.2 Add a readonly TYPE field on the parameter dialog (Add from found, edit from registered); Save still posts the stored type JSON id so persistence is not skipped; IEEE-style readonly is unchanged

## 5. Flash together

- [ ] 5.1 Flash host and slave together, pair one new device, and verify Search type, saved `devices.json` type, and readonly TYPE on edit match
