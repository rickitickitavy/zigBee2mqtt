# web-console Specification

## Purpose

Gives this Zigbee–MQTT gateway a browser console on its own IP so an operator can open a tabbed UI, set Wi-Fi AP/STA, flash firmware, and read recent logs without the USB CLI.

## Requirements

### Requirement: Console is reachable over HTTP

The firmware MUST serve an HTTP web console on port 80 whenever Wi-Fi AP or STA has an address. Opening the device root URL MUST return the console page. The USB CLI, MQTT, and Zigbee control paths MUST keep working while the console is served. This change MUST NOT require HTTP authentication.

#### Scenario: Open console on AP

- **WHEN** the gateway is in AP mode and a client opens `http://192.168.0.1/`
- **THEN** the browser receives the web console HTML

#### Scenario: Open console on STA

- **WHEN** the gateway has a STA address and a client opens `http://<sta-ip>/`
- **THEN** the browser receives the same web console HTML

#### Scenario: Wi-Fi is down

- **WHEN** neither AP nor STA is up
- **THEN** the console is not reachable on the LAN and existing USB CLI behavior is unchanged

### Requirement: Console stays reachable after STA address refresh
When the host is in STA mode and the station obtains an IPv4 address after a disconnect, roam, or DHCP renew, the HTTP web console on port 80 MUST answer on that current STA address. ICMP reachability and MQTT client traffic MUST NOT be treated as proof that the console is already bound. The first HTTP bind after a successful boot join MUST remain the bind used until a later STA address event; that first bind MUST NOT be torn down immediately. USB CLI, MQTT, and Zigbee/SPI control MUST keep working while the console listen socket is rebound.

#### Scenario: Console answers after STA reconnect
- **WHEN** STA drops and later has an IPv4 address again
- **THEN** opening `http://<sta-ip>/` returns the web console HTML without requiring a device reboot

#### Scenario: Console answers after STA GOT_IP without a polled disconnect
- **WHEN** STA reports a new or renewed IPv4 address while ICMP and MQTT still work
- **THEN** opening `http://<sta-ip>/` returns the web console HTML

#### Scenario: First boot bind is not rebound
- **WHEN** the host has just bound the console after the boot STA join succeeded
- **THEN** the console remains that first listen and is not immediately closed and opened again

### Requirement: Main navigation uses sidebar sections

The console MUST present six main sections labeled Status, WiFi, MQTT, ZigBee, Devices, and System as a left sidebar strip (horizontal folder strip on a narrow viewport). Selecting a main section MUST show that section’s panel and hide the others without a full page reload. Status MUST show the live summary (counts, packet counters, version) without settings forms. Devices MUST show the registered-device table and add-device flow.

#### Scenario: Switch main section

- **WHEN** the operator selects WiFi after Status was visible
- **THEN** the WiFi panel is shown and the Status panel is hidden

#### Scenario: Placeholder tabs have no forms

- **WHEN** the operator opens Status
- **THEN** the panel has no configuration controls and does not change device settings

### Requirement: Devices card lists registered devices and can add one
The Devices section SHALL show a table of registered devices (at least IEEE, friendly name, last RSSI when known) and Add device, Export, and Restore. Opening Devices SHALL load the registered list from the host. After a successful parameter Save, the table SHALL include the new row without requiring a full page reload.

#### Scenario: Open Devices
- **WHEN** the operator opens the Devices section and the host has at least one registered device
- **THEN** that device appears in the table

#### Scenario: Empty map
- **WHEN** the operator opens Devices and no devices are registered
- **THEN** the table is empty and Add device is still available

### Requirement: Add device uses a search dialog then a parameter dialog
Add device SHALL open a search dialog that contains a table of found-but-unregistered devices and a Search control. Opening the dialog SHALL NOT start pairing. Search SHALL start pairing on the slave only when clicked and SHALL update the found table as the host learns new joins. Add SHALL be enabled only when one found row is selected. Add SHALL close the search dialog and open a parameter dialog for that device. The parameter dialog SHALL show IEEE (not editable) and SHALL accept friendly name and MQTT topic fields. Save SHALL persist the device and close the parameter dialog. Dismissing the search dialog SHALL stop pairing search.

#### Scenario: Search fills the found table
- **WHEN** the search dialog is open, the operator clicks Search, and an unregistered device joins
- **THEN** that device appears in the found table without closing the dialog

#### Scenario: Add then Save
- **WHEN** the operator selects a found device, clicks Add, fills the parameter form, and clicks Save
- **THEN** the dialogs are closed and the Devices table shows the new registered device

#### Scenario: Cancel search
- **WHEN** the operator closes the search dialog without Add
- **THEN** no new registered device is written and pairing search stops

### Requirement: ZigBee tab edits the Zigbee group

The ZigBee tab MUST show and allow saving CHANNEL and PERMIT_JOIN_ON_BOOT_SEC from the Zigbee group record. CHANNEL MUST be 11–26. PERMIT_JOIN_ON_BOOT_SEC MUST be 0–254. The page MUST load those values from `GET /api/zigbee` when the tab opens and on first page load, and MUST persist them with `POST /api/zigbee`. Saving MUST write EEPROM and restart so the host pushes settings to the radio slave. Reset MUST reload stored values and MUST NOT write settings.

#### Scenario: Load Zigbee settings

- **WHEN** the operator opens the ZigBee section
- **THEN** CHANNEL and PERMIT_JOIN_ON_BOOT_SEC show the values stored on the host

#### Scenario: Save Zigbee settings

- **WHEN** the operator sets a valid channel and permit-join duration and saves
- **THEN** the Zigbee group is persisted and the device restarts to apply the radio settings

### Requirement: Settings are stored in groups

Persistent settings MUST live in `GlobalSettings` as separate group records. Each functional group MUST be one record. The Wi-Fi group record MUST contain BSSID, PASSWORD, DEVICE_NAME, AP IP, MODE, and OTG_ENABLED. Default MODE MUST be AP. Default BSSID MUST be `z2m-gateway` and MUST be the network name for both STA join and AP advertise. Default PASSWORD MUST be `00000000` for both AP and STA. Default AP IP MUST be `192.168.0.1` and MUST apply only when the radio is in AP mode. Default OTG_ENABLED MUST be false. DEVICE_NAME MUST be used only as the Wi-Fi hostname and MUST NOT be used as the AP or STA network name. Other existing setting fields MUST be placed in their own group records without changing their meaning in this change.

#### Scenario: Defaults on first boot

- **WHEN** settings are uninitialized and defaults are written
- **THEN** the Wi-Fi group has MODE AP, OTG_ENABLED false, PASSWORD `00000000`, AP IP `192.168.0.1`, BSSID `z2m-gateway`, and a default DEVICE_NAME used only as hostname

#### Scenario: Wi-Fi group is one record

- **WHEN** the firmware reads or writes Wi-Fi configuration
- **THEN** BSSID, PASSWORD, DEVICE_NAME, AP IP, MODE, and OTG_ENABLED are stored together in the Wi-Fi group record

### Requirement: WiFi tab edits the Wi-Fi group

The WiFi tab MUST show and allow saving BSSID, PASSWORD, DEVICE_NAME, AP IP, MODE (AP or STA), and OTG_ENABLED. AP IP MAY be hidden while MODE is STA. Saving MUST persist the Wi-Fi group and apply the boot rules on the next Wi-Fi start (restart is allowed). BSSID is one network-name string for STA and AP (not a MAC address). DEVICE_NAME MUST NOT be written to MQTT or Zigbee settings.

#### Scenario: Save STA settings

- **WHEN** the operator sets MODE to STA, enters router BSSID and PASSWORD, and saves
- **THEN** the Wi-Fi group is persisted with those values and MODE STA

#### Scenario: Save AP settings

- **WHEN** the operator sets MODE to AP, enters BSSID and PASSWORD, and saves
- **THEN** the Wi-Fi group is persisted with those values and MODE AP

### Requirement: Wi-Fi boot mode and AP naming

If BSSID is empty at boot, the device MUST start in AP mode using the default BSSID `z2m-gateway`. If MODE is STA and BSSID is set, the device MUST try to join that network for 5 seconds. If the join does not succeed in that window, the device MUST switch to AP mode using the same BSSID and MUST remain in AP until the next boot or a settings apply. The AP IPv4 address MUST be the stored AP IP (default `192.168.0.1`). PASSWORD MUST be the PSK for both STA join and AP (default `00000000`). DEVICE_NAME MUST be applied only as the Wi-Fi hostname. OTG_ENABLED MUST be persisted and shown; it MUST NOT change radio mode in this change.

#### Scenario: Unconfigured starts AP

- **WHEN** the device boots with default Wi-Fi settings (MODE AP, BSSID `z2m-gateway`)
- **THEN** it starts AP and does not attempt STA

#### Scenario: STA connects within 5 seconds

- **WHEN** the device boots with MODE STA and a configured BSSID and the router accepts the join within 5 seconds
- **THEN** the device stays in STA with a LAN address

#### Scenario: STA fails then AP uses the same BSSID

- **WHEN** the device boots with MODE STA and a configured BSSID and the join does not succeed within 5 seconds
- **THEN** the device switches to AP and the AP network name is that same BSSID

#### Scenario: Configured AP uses BSSID

- **WHEN** the device boots with MODE AP and BSSID `z2m-gateway`
- **THEN** it starts AP whose network name is `z2m-gateway`

### Requirement: System has Update and Log tabs

When System is selected, the console MUST present nested folder-style tabs in this order: Log, devices.json, Update, Hardware. The default System panel MUST be Log. Selecting a tab MUST show that panel and hide the others without a full page reload.

#### Scenario: Nested System tabs

- **WHEN** the operator opens System and then selects Log
- **THEN** the Log panel is shown and the Update panel is hidden

#### Scenario: Default System tab is Log
- **WHEN** the operator opens System
- **THEN** the Log tab is selected first

### Requirement: Firmware update from the Update tab

The Update tab MUST show the running firmware version and accept a firmware binary upload over HTTP. A successful write MUST program the inactive OTA application slot and then restart the device. A failed write MUST leave the running application slot unchanged, MUST NOT restart, and MUST show a failure on the page.

#### Scenario: Successful firmware upload

- **WHEN** the operator uploads a valid firmware image from the Update tab
- **THEN** the image is written to the inactive OTA slot and the device restarts into the new image

#### Scenario: Failed firmware upload

- **WHEN** the upload is aborted or the image is rejected
- **THEN** the currently running firmware remains unchanged and the page reports the failure

### Requirement: Update tab can flash the web filesystem

The System → Update tab SHALL offer a filesystem upload (LittleFS image) in addition to the firmware binary upload. A successful filesystem write SHALL replace the on-device web files and SHALL NOT replace the application firmware slot.

#### Scenario: Filesystem form present

- **WHEN** the operator opens System → Update
- **THEN** the page shows a filesystem upload control as well as the firmware upload control

#### Scenario: Successful filesystem upload

- **WHEN** the operator uploads a valid LittleFS image from that form
- **THEN** the device stores that image as the web filesystem and the console HTML/CSS update is what the next page load uses

### Requirement: MQTT card shows stored settings

When the operator opens the MQTT section, each MQTT field SHALL display the value currently stored on the host. Empty stored fields MAY stay empty; the form SHALL NOT stay blank when the device has MQTT settings.

#### Scenario: Open MQTT with saved server

- **WHEN** the host has a stored MQTT server and the operator opens the MQTT card
- **THEN** the server field (and the other MQTT fields) show the stored values

### Requirement: WiFi card scrolls and keeps Save and Reset reachable

The WiFi section SHALL scroll when its fields do not fit the main panel. Save and Reset SHALL remain usable (visible after scroll or pinned at the bottom of the card). Reset SHALL reload the form from the device without writing new settings.

#### Scenario: Tall WiFi card

- **WHEN** the WiFi fields do not fit the viewport
- **THEN** the operator can scroll the WiFi card and reach Save and Reset

#### Scenario: Reset reloads WiFi

- **WHEN** the operator changes a WiFi field and then clicks Reset
- **THEN** the fields return to the last stored values from the device

### Requirement: Log tab shows recent firmware logs

The Log tab MUST display recent firmware log lines that are also emitted on USB when console logging is enabled. The page MUST be able to refresh that list without leaving the Log tab. Older lines MAY drop when the in-memory buffer is full. The Refresh control MUST stay visible without a second page scroller hiding it, MUST be left-aligned, and MUST use a compact content width (not a full-width bar).

#### Scenario: New log line appears

- **WHEN** the firmware emits a log line and the operator refreshes the Log tab (automatically or manually)
- **THEN** that line appears in the log view (unless it has already aged out of the buffer)

#### Scenario: USB logging still works

- **WHEN** a log line is emitted
- **THEN** it still appears on the USB serial console when USB debug logging is enabled

#### Scenario: Refresh stays visible

- **WHEN** the operator opens System → Log
- **THEN** Refresh is visible, left-aligned, and not stretched across the card

### Requirement: Escape dismisses overlay dialogs

The console SHALL treat Escape as dismiss for every overlay dialog (search, device parameters, restore-skipped list, and delete confirm). Escape SHALL run the same dismiss path as that dialog’s Cancel or Close control: it MUST NOT save, MUST NOT confirm delete, and dismissing the search dialog MUST stop pairing search. When more than one overlay is visible, Escape SHALL dismiss only the front-most overlay. Escape SHALL do nothing when no overlay is visible.

#### Scenario: Escape closes search

- **WHEN** the search dialog is visible and the operator presses Escape
- **THEN** the search dialog closes, pairing search stops, and no new registered device is written

#### Scenario: Escape cancels delete

- **WHEN** the delete-confirm dialog is visible and the operator presses Escape
- **THEN** the confirm dialog closes and the selected device remains registered

#### Scenario: Escape ignored with no overlay

- **WHEN** no overlay dialog is visible and the operator presses Escape
- **THEN** the page does not navigate away and no dialog opens or closes

### Requirement: Device parameter dialog includes FULL CONTROL
The Devices parameter dialog SHALL include a FULL CONTROL checkbox as the last setting field (after IEEE, name, channels, and the MQTT topic fields). The control SHALL load from the stored record and SHALL be unchecked when the flag is omitted or off. Save SHALL persist the checkbox with the device. Binary device settings SHALL use a checkbox, not a text field or select of on/off strings.

#### Scenario: New device shows off at the end
- **WHEN** the operator opens the parameter dialog for a newly added device
- **THEN** FULL CONTROL is unchecked and appears after AVAILABILITY

#### Scenario: Load stored on
- **WHEN** the operator opens parameters for a device stored with FULL CONTROL on
- **THEN** the FULL CONTROL checkbox is checked and still last in the form

#### Scenario: Save turns it on
- **WHEN** the operator checks FULL CONTROL and saves
- **THEN** the stored record has FULL CONTROL on

### Requirement: NAME edits rewrite default MQTT topics
While the device parameter dialog is open, each change to NAME SHALL rewrite the state, command, and availability fields to the default topics for that name and the stored MQTT base topic (`{base}/{slug}/state`, `{base}/{slug}/set`, `{base}/{slug}/availability`). This SHALL apply when adding and when editing an existing device.

#### Scenario: Rename rewrites topics
- **WHEN** the operator edits NAME from `switch_1` to `kitchen_lamp`
- **THEN** the three topic fields update to the default pattern that uses `kitchen_lamp`

### Requirement: Double-click opens device edit
A double-click on a registered-device table row SHALL open the parameter dialog for that IEEE, same as Edit.

#### Scenario: Double-click row
- **WHEN** the operator double-clicks a registered device row
- **THEN** the edit parameter dialog opens for that device

### Requirement: Devices Export downloads the slave store
Beneath the registered-device table the console SHALL provide Export. Export SHALL download the slave devices store JSON (the same document as System devices.json), for later Restore.

#### Scenario: Export file
- **WHEN** the operator clicks Export
- **THEN** the browser downloads the current slave `devices.json` content

### Requirement: Devices Restore matches System restore
Beneath the registered-device table the console SHALL provide Restore that uses the same file-pick and restore path as System devices.json Restore (one device record at a time to the slave).

#### Scenario: Restore from Devices
- **WHEN** the operator chooses Restore on Devices and selects a valid devices JSON array
- **THEN** missing devices are restored the same way as System devices.json Restore

### Requirement: Search follows the slave join window
Opening the pairing dialog SHALL NOT start pairing on the slave. Search SHALL start the slave join window only when the operator clicks Search. While that window is open, Search SHALL stay disabled. When the slave join window ends, Search SHALL become enabled again even if the dialog stays open. Closing the dialog SHALL still stop pairing.

#### Scenario: Open does not search
- **WHEN** the operator opens Add device
- **THEN** the pairing dialog is visible and the slave join window is not started

#### Scenario: Search until slave ends
- **WHEN** the operator clicks Search and the slave join window later closes
- **THEN** Search is enabled again without requiring the dialog to close

### Requirement: Devices table shows last packet RSSI
The registered-device table SHALL include last received signal strength for each IEEE when the host has stored RSSI from the last inbound Zigbee packet for that device. A device with no received packet yet SHALL show an empty signal cell.

#### Scenario: RSSI after report
- **WHEN** the host has received a packet for a registered IEEE with RSSI −62 dBm
- **THEN** that row shows −62 dBm (or an equivalent dBm display of that value)

### Requirement: Status shows counts, traffic, and version
The Status section SHALL show registered device count, online device count, packets received, packets sent, and firmware version. Status SHALL NOT contain settings forms that write configuration.

#### Scenario: Open Status with devices
- **WHEN** the operator opens Status and three devices are registered with one online
- **THEN** Status shows device count 3, online count 1, firmware version from the running image, and the packet counters

### Requirement: Pairing found table shows readable device type
The pairing search found table SHALL show a readable device type for each found row. Labels SHALL be: `unknown` → Unknown; `onOff` → On/Off; `iasZone` → IAS Zone; `windowCovering` → Window covering. Manufacturer and model MAY still appear. The type SHALL come from the found-device API, not from the operator.

#### Scenario: Switch appears in Search
- **WHEN** Search is open and an unregistered `onOff` device joins
- **THEN** that row shows On/Off

### Requirement: Device parameters show readonly type
The device parameter dialog SHALL show the device type as a readonly field, like IEEE. Add from Search SHALL show the found type. Edit of a registered device SHALL show the stored type. Save SHALL persist other editable fields and SHALL NOT take a new type from an editable control.

#### Scenario: Add shows found type
- **WHEN** the operator Adds a found `iasZone` device
- **THEN** the parameter dialog shows IAS Zone and the operator cannot edit that field

#### Scenario: Edit keeps stored type
- **WHEN** the operator opens parameters for a registered `onOff` device and saves a new name
- **THEN** the type remains `onOff` and the dialog still shows On/Off
