## 1. Slave RSSI and join-closed

- [x] 1.1 Put signed RSSI (dBm) in a fixed ATTR_REPORT header slot and parse it on the host into last-RSSI-by-IEEE, and verify a report with RSSI −62 is stored for that IEEE while the MQTT body is still the message text
- [x] 1.2 When the slave join window expires or closeJoin runs, enqueue a join-closed SPI event and set host pairingActive false, and verify pairingActive is false after the window without closing the web dialog
- [x] 1.3 Increment host packetsRx on inbound Zigbee SPI events and packetsTx on successful ZCL on/off and write-attribute enqueue, and verify both counters increase from 0 after one report and one command

## 2. Status API

- [x] 2.1 Add GET `/api/status` with devices, online, packetsRx, packetsTx, version, pairingActive, and verify JSON matches the host map, online window, FIRMWARE_VERSION, and pairing flag

## 3. Devices console

- [x] 3.1 Rewrite state, command, and availability on every NAME input (including edit) using default `{base}/{slug}` topics, and verify renaming to `kitchen_lamp` fills those three fields
- [x] 3.2 Open edit on table double-click, and verify a double-click opens the same parameter dialog as Edit
- [x] 3.3 Put Export and Restore under the Devices table (Export downloads GET `/api/devices/store`; Restore reuses System restore), and verify Export saves slave JSON and Restore still adds only missing devices one at a time
- [x] 3.4 Show last RSSI in the Devices table from GET `/api/devices` (empty when unknown), and verify a device with stored −62 dBm shows it

## 4. Pairing Search button

- [x] 4.1 Keep Search idle on dialog open; disable Search only while pairingActive; poll `/api/status` (or join-closed) to enable Search when the slave window ends; still POST search/stop on close, and verify open does not permit-join and Search enables after the slave window ends

## 5. Status and System tabs

- [x] 5.1 Fill the Status section from GET `/api/status` (counts, packet counters, version, no settings forms), and verify Status shows the same version as `/api/version` and matching device/online counts
- [x] 5.2 Order System tabs Log, devices.json, Update, Hardware with Log default/active, and verify opening System shows Log first
