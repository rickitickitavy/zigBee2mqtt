## 1. Settings layout and partial writes

- [x] 1.1 Reshape settings to v5 (EEPROM main fields, 128-aligned 256-byte reserve) plus a LittleFS device list of **128** slots, and verify main `sizeof` fits 4096, `offsetof(reserved) % 128 == 0`, and `DEVICE_MAP_SLOTS == 128`
- [x] 1.2 Migrate v4 EEPROM to v5 (copy wifi/mqtt/zigbee and used device slots, zero reserve) and verify a packed v4 blob keeps SSID and one IEEE after load
- [x] 1.3 Add `saveMain` / `saveDeviceSlot` dirty-byte EEPROM writes plus a committed snapshot, point Wi-Fi/MQTT/Zigbee saves at `saveMain` and map upsert at `saveDeviceSlot`, and verify a device save does not mark main-block bytes dirty

## 2. Host found list and HTTP

- [x] 2.1 Record unregistered `SpiEvtDeviceJoin` rows in a capped host RAM table and verify a join for a new IEEE appears there and a registered IEEE does not
- [x] 2.2 Add `GET /api/devices` and `POST /api/devices` (ieee, friendlyName, topics; no restart) and verify list/save/full-map 400 against the host map
- [x] 2.3 Add `GET /api/devices/found`, `POST /api/devices/search`, and `POST /api/devices/search/stop` (permit join / close join) and verify Search enqueues permit join and stop closes the window

## 3. Devices web UI

- [x] 3.1 Replace the Devices placeholder with a registered-device table and Add device, load `GET /api/devices` when the section opens, and verify stored rows appear
- [x] 3.2 Add the search dialog (found table, Search, Add) that polls found devices while open, and verify Search fills the table and closing the dialog calls stop
- [x] 3.3 Add the parameter dialog (read-only IEEE, name + MQTT topics, Save) and verify Save persists one slot, closes dialogs, and the Devices table shows the new row

## 4. On-device check

- [ ] 4.1 On the host browser, add one real or simulated join through Search → Add → Save and verify the Devices table and EEPROM map match, and that Wi-Fi save still does not require rewriting device slots in the snapshot
