# web-console Specification

## Purpose

Gives this Zigbee–MQTT gateway a browser console on its own IP so an operator can open a tabbed UI, set Wi-Fi AP/STA, flash firmware, and read recent logs without the USB CLI.

## Requirements

### Requirement: Console is reachable over HTTP

The firmware MUST serve an HTTP web console on port 80 whenever Wi-Fi AP or STA has an address. Opening the device root URL MUST return the console page (login form when there is no session, signed-in chrome when there is a valid session). The USB CLI, MQTT, and Zigbee control paths MUST keep working while the console is served. Protected console APIs MUST require a valid session.

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

The Update tab MUST show the running firmware version and accept a firmware binary upload over HTTP. After a valid firmware file is received, the host MUST program the slave’s inactive application slot over SPI first. Only after the slave reports a successful firmware commit MUST the host program its own inactive application slot and restart. A failed HTTP receive, a failed slave transfer, or a failed slave commit MUST leave the host’s running application unchanged, MUST NOT restart the host, and MUST show a failure on the page. Filesystem upload on the same tab is unchanged by this requirement.

#### Scenario: Successful firmware upload

- **WHEN** the operator uploads a valid firmware image from the Update tab and the slave commit succeeds
- **THEN** the slave is running or restarting into that image before the host restarts into the same image

#### Scenario: Failed firmware upload

- **WHEN** the upload is aborted or the image is rejected
- **THEN** the currently running firmware remains unchanged and the page reports the failure

#### Scenario: Slave OTA fails

- **WHEN** the HTTP firmware file is accepted but the slave transfer or slave commit fails
- **THEN** the host does not restart, the host keeps its previously running application, and the Update tab reports the failure

### Requirement: Firmware Update shows slave-then-host progress

After the browser finishes sending the firmware file, the Update tab SHALL show that the gateway is updating the slave, then the host, until success or failure. The HTTP file-upload progress bar alone SHALL NOT be treated as a completed firmware update.

#### Scenario: Status after file lands

- **WHEN** the firmware file has finished uploading over HTTP and the slave is still being programmed
- **THEN** the Update tab shows that the slave update is in progress and does not claim the device has already restarted

#### Scenario: Phase percents

- **WHEN** firmware update is in progress
- **THEN** the Update tab shows upload-to-master percent, master-to-slave percent, and host-apply percent together with the current phase

### Requirement: Firmware Update warns before host reboot

When the host is about to restart after a successful firmware write, the web UI SHALL show a modal warning that the router will reboot, SHALL count down a 30 second wait, and SHALL ping the router in the background until it responds or the wait expires.

#### Scenario: Router returns within wait

- **WHEN** the host restarts after firmware apply and `/api/version` succeeds within 30 seconds
- **THEN** the dialog reports that the update finished successfully

#### Scenario: Router wait times out

- **WHEN** the host does not answer `/api/version` within 30 seconds after the reboot warning
- **THEN** the dialog reports a wait timeout error

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

### Requirement: MQTT SERVER TYPE replaces ENABLED
The MQTT section MUST show SERVER TYPE as a three-option control (`disable`, `remote`, `local`) and MUST NOT show an ENABLED checkbox. GET `/api/mqtt` MUST return `serverType` as one of those strings. POST `/api/mqtt` MUST persist `serverType` and MUST NOT require a boolean `enabled`. Factory default SERVER TYPE MUST be `disable`. Settings export and restore MUST include `mqtt.serverType`. Restore of a file that still has `mqtt.enabled` and no `serverType` MUST map `false` to `disable` and `true` to `remote`.

#### Scenario: Open MQTT card
- **WHEN** the operator opens the MQTT section
- **THEN** SERVER TYPE shows the stored value and ENABLED is not shown

#### Scenario: Save local
- **WHEN** the operator sets SERVER TYPE to `local` and saves
- **THEN** GET `/api/mqtt` returns `"serverType":"local"`

#### Scenario: Restore legacy enabled
- **WHEN** an admin restores a file whose MQTT object has `"enabled":true` and no `serverType`
- **THEN** stored SERVER TYPE is `remote`

### Requirement: SERVER field hidden when type is local
When SERVER TYPE is `local`, the MQTT SERVER field MUST be hidden and POST `/api/mqtt` MUST NOT require `server`. When SERVER TYPE is `disable` or `remote`, the SERVER field MUST be visible. Empty SERVER on save in those modes MAY keep the stored default host string.

#### Scenario: Local hides SERVER
- **WHEN** the operator selects SERVER TYPE `local`
- **THEN** the SERVER field is not shown

#### Scenario: Remote shows SERVER
- **WHEN** the operator selects SERVER TYPE `remote`
- **THEN** the SERVER field is shown

### Requirement: WiFi card scrolls and keeps Save and Reset reachable

The WiFi section SHALL scroll when its fields do not fit the main panel. Save and Reset SHALL remain usable (visible after scroll or pinned at the bottom of the card). Reset SHALL reload the form from the device without writing new settings.

#### Scenario: Tall WiFi card

- **WHEN** the WiFi fields do not fit the viewport
- **THEN** the operator can scroll the WiFi card and reach Save and Reset

#### Scenario: Reset reloads WiFi

- **WHEN** the operator changes a WiFi field and then clicks Reset
- **THEN** the fields return to the last stored values from the device

### Requirement: Log tab shows recent firmware logs

The Log tab MUST display recent firmware log lines that are also emitted on USB when console logging is enabled. The page MUST be able to refresh that list without leaving the Log tab. Older lines MAY drop when the in-memory buffer is full. The Refresh control MUST stay visible without a second page scroller hiding it, MUST be left-aligned, and MUST use a compact content width (not a full-width bar). The Log card MUST NOT repeat a second “Log” title under the Log tab.

#### Scenario: New log line appears

- **WHEN** the firmware emits a log line and the operator refreshes the Log tab (automatically or manually)
- **THEN** that line appears in the log view (unless it has already aged out of the buffer)

#### Scenario: USB logging still works

- **WHEN** a log line is emitted
- **THEN** it still appears on the USB serial console when USB debug logging is enabled

#### Scenario: Refresh stays visible

- **WHEN** the operator opens System → Log
- **THEN** Refresh is visible, left-aligned, and not stretched across the card

#### Scenario: No duplicate Log title
- **WHEN** the operator opens System → Log
- **THEN** the tab is labeled Log and the card does not show another Log heading

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
A single click on a registered-device table row SHALL select that IEEE and open the parameter dialog for that IEEE, same as Edit. A double-click SHALL NOT be required.

#### Scenario: Double-click row
- **WHEN** the operator single-clicks a registered device row
- **THEN** that row is selected and the edit parameter dialog opens for that device

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
The Status section SHALL show host online time, master and slave firmware versions, registered device count, online device count, packets sent, packets received, MQTT local status, MQTT active topic count, MQTT connection count, Wi-Fi mode, BSSID, and Wi-Fi signal strength. Status SHALL obtain these values from the gateway status API. Status SHALL NOT contain settings forms that write configuration.

#### Scenario: Open Status with devices
- **WHEN** the operator opens Status and three devices are registered with one online
- **THEN** Status shows device count 3, online count 1, master firmware version from the running image, slave firmware version from the last slave report, and the packet counters

### Requirement: Status groups use compact name-value rows
Status SHALL render live attributes as groups in this order: online time (one row, not inside another group), then **Version**, then **Devices**, then **MQTT**, then **WiFi**. Each group SHALL have a heading. Each attribute SHALL keep its name and value on a single row. Values in a group SHALL align to the right. Groups SHALL use a mindful width (content plus padding) and SHALL NOT stretch to the full main-panel width. Status SHALL NOT contain settings forms that write configuration.

#### Scenario: Grouped Status layout
- **WHEN** the operator opens Status
- **THEN** the panel shows online time, then Version, Devices, MQTT, and WiFi groups, each narrower than the main panel, with names on the left and values on the right of the same row

### Requirement: Status online time includes days
Status SHALL show host online time as one value on one row. The value SHALL include days, hours, minutes, and seconds derived from host uptime seconds.

#### Scenario: Multi-day uptime
- **WHEN** the host has been up for 90061 seconds
- **THEN** Status online time shows 1 day, 1 hour, 1 minute, and 1 second (any compact `d h m s` wording)

### Requirement: Status Version group
The Version group SHALL show master firmware version and slave firmware version.

#### Scenario: Both versions
- **WHEN** Status is open and the host image is `0.2.21` and the slave last reported `0.2.21`
- **THEN** Version shows master `0.2.21` and slave `0.2.21`

### Requirement: Status Devices group
The Devices group SHALL show registered device count, online device count, packets sent, and packets received.

#### Scenario: Device counters
- **WHEN** three devices are registered, one is online, and the host has sent 4 packets and received 9
- **THEN** Devices shows count 3, online 1, packets sent 4, and packets received 9

### Requirement: Status MQTT group
The MQTT group SHALL show local broker status, active MQTT topic count, and MQTT connection count. Connection count SHALL include remote LAN clients connected to the onboard broker when it is listening, plus the host client when it is connected.

#### Scenario: Local broker with a remote client
- **WHEN** SERVER TYPE is local, the onboard broker is listening, the host client is connected, and one other MQTT client is connected
- **THEN** MQTT shows local status as listening, a topic count, and connections 2

#### Scenario: Broker unused
- **WHEN** SERVER TYPE is disable
- **THEN** MQTT local status is not listening (or unused) and connections is 0

### Requirement: Status WiFi group
The WiFi group SHALL show Wi-Fi mode, BSSID (the same network name field as the WiFi card), and Wi-Fi signal strength. When the host is STA-associated, signal strength SHALL be the current STA RSSI. When there is no STA link, signal strength SHALL show an empty or placeholder value, not a stale STA RSSI.

#### Scenario: STA signal
- **WHEN** the host is in STA mode and associated with RSSI −58 dBm and BSSID `home-net`
- **THEN** WiFi shows mode STA, BSSID `home-net`, and signal −58 dBm

#### Scenario: AP without STA
- **WHEN** the host is in AP mode and is not STA-associated
- **THEN** WiFi shows mode AP and a placeholder for signal strength

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

### Requirement: Devices table column order
The registered-device table SHALL show columns in this order: online, name, type, status, battery, RSSI. The table SHALL NOT show an IEEE column. IEEE SHALL remain visible on the device parameter dialog and SHALL remain the row identity for selection, edit, delete, and Manual command.

#### Scenario: Column order
- **WHEN** the operator opens Devices and at least one device is registered
- **THEN** the table headers are online, name, type, status, battery, RSSI and there is no IEEE header

### Requirement: Devices table shows battery percent or N/A
The battery cell SHALL show an integer percentage when the registered list JSON includes a battery percent for that IEEE. When that value is absent, the cell SHALL show `N/A`. Battery is live telemetry: the console SHALL NOT present it as an editable device setting.

#### Scenario: Percent after report
- **WHEN** the list JSON for a device includes battery `67`
- **THEN** that row’s battery cell shows `67`

#### Scenario: No battery yet
- **WHEN** the list JSON for a device omits battery
- **THEN** that row’s battery cell shows `N/A`

### Requirement: Devices table shows type-specific status
The status cell SHALL show the last known state for the device type. For `onOff`, each channel SHALL be a small circle: dark for OFF and light for ON. For other types, each channel SHALL show the last report text for that endpoint. When `channels` is `1`, the cell SHALL show one indicator. When `channels` is `0` or `2` through `16`, the cell SHALL show one indicator per channel, bound to inbound reports by the packet `ep`. A channel with no report yet SHALL show an empty or unknown indicator, not a guessed ON.

#### Scenario: Single-channel on/off on
- **WHEN** a registered `onOff` device has `channels` `1` and last state ON
- **THEN** that row’s status cell shows one light circle

#### Scenario: Parse-mode two endpoints
- **WHEN** a registered `onOff` device has `channels` `0`, endpoint 1 last reported OFF, and endpoint 3 last reported ON
- **THEN** that row’s status cell shows a dark circle for endpoint 1 and a light circle for endpoint 3

#### Scenario: IAS zone text
- **WHEN** a registered `iasZone` device last reported `LEAK` on its status endpoint
- **THEN** that row’s status cell shows `LEAK`

### Requirement: Manual command terminal
Beneath the registered-device table the console SHALL provide Manual command. The button SHALL be enabled only when a registered row is selected. Activating it SHALL open a terminal dialog for that IEEE. The dialog SHALL include a read-only answers pane and, below it, a command input. Pressing Enter in the input SHALL send the typed body through the same host command path as that device’s MQTT `set` topic and SHALL NOT require a broker. When the selected device has more than one channel (`channels` `0` or `2`–`16`), the dialog SHALL include a channel dropdown; the chosen channel SHALL select the destination endpoint the same way MQTT suffix or `ch-<ep>##` mapping would. Escape SHALL dismiss the dialog without sending. New inbound messages for that IEEE SHALL append to the answers pane while the dialog is open.

#### Scenario: Send ON like MQTT set
- **WHEN** the operator selects a single-channel device, opens Manual command, types `ON`, and presses Enter
- **THEN** the host sends that body on the same path as a publish to that device’s stored command topic

#### Scenario: Channel dropdown
- **WHEN** the selected device has `channels` `4` and the operator chooses channel 3, types `OFF`, and presses Enter
- **THEN** the host sends `OFF` to endpoint 3 the same way MQTT `set/3` would

#### Scenario: Disabled without selection
- **WHEN** no registered row is selected
- **THEN** Manual command is disabled

### Requirement: Devices row type actions
When the pointer is over a registered-device row that has type actions, the console SHALL show a compact **…** control on the right of that row. Activating **…** SHALL open a menu of actions for that row’s stored type. When `channels` is `2` through `16`, or `channels` is `0` with more than one known endpoint, the top-level menu SHALL list actions only (ON/OFF/TOGGLE or OPEN/CLOSE/STOP). Each action SHALL show a submenu marker and open a submenu whose items are channel numbers only (`1`, `2`, `3`). The submenu SHALL stay inside the visible Devices card so it does not add a card scrollbar. When `channels` is `1`, or `channels` is `0` with no extra endpoints, the menu SHALL list actions once with no channel submenu. `onOff` actions SHALL be ON, OFF, and TOGGLE. `windowCovering` actions SHALL be OPEN, CLOSE, and STOP. `iasZone` and `unknown` SHALL have no type actions and SHALL NOT show **…**. Choosing an action SHALL send that body on the same path as Manual command / MQTT `set` for that IEEE and channel. Activating **…** or a menu item SHALL NOT open the device edit dialog. The table column order SHALL stay online, name, type, status, battery, RSSI; **…** is an overlay, not a new column.

#### Scenario: Hover shows ellipsis
- **WHEN** the pointer is over a registered `onOff` row
- **THEN** a **…** control appears on the right of that row

#### Scenario: Single-channel on/off
- **WHEN** the operator opens **…** on an `onOff` device with `channels` `1` and chooses ON
- **THEN** the host sends `ON` on the same path as Manual command for that IEEE without opening edit

#### Scenario: Multi-channel on/off
- **WHEN** the operator opens **…** on an `onOff` device with `channels` `3`
- **THEN** the top-level menu is ON, OFF, and TOGGLE, and each action’s submenu is `1`, `2`, and `3` only

#### Scenario: Covering stop on channel 2
- **WHEN** the operator chooses STOP for channel 2 on a `windowCovering` device with `channels` `2`
- **THEN** the host sends `STOP` to endpoint 2 the same way MQTT `{set}/2` would

#### Scenario: No actions for IAS
- **WHEN** the pointer is over a registered `iasZone` row
- **THEN** **…** is not shown

### Requirement: Maintenance exports settings without Wi-Fi
System → Maintenance SHALL show **Export settings** above **Restore settings**. Both SHALL use compact inline buttons, not full-width bars. Export SHALL download a JSON file that includes MQTT settings, Zigbee settings, hardware SPI speed, the signed-in user’s theme, the persisted device list (same device fields as Devices Export, no live telemetry), and the users table. Each exported user SHALL include user name, added-at, roles, `isBlocked`, theme, and password **hash and salt**. The file SHALL NOT contain a plaintext user password. The file SHALL NOT contain a Wi-Fi group (no BSSID, password, MODE, AP IP, hostname, or OTG). Only `isAdmin` SHALL see or use Export settings and Restore settings.

#### Scenario: Export omits Wi-Fi
- **WHEN** the operator clicks Export settings and the host has Wi-Fi, MQTT, Zigbee, hardware, a saved theme, and at least one registered device
- **THEN** the downloaded JSON has mqtt, zigbee, hardware, ui theme, and devices, and has no wifi object or Wi-Fi password

#### Scenario: Buttons stay compact
- **WHEN** the operator opens System → Maintenance
- **THEN** Export settings and Restore settings are stacked and no wider than a normal inline button

#### Scenario: Export includes users without plaintext passwords
- **WHEN** an `isAdmin` operator clicks Export settings and the host has at least one stored user
- **THEN** the downloaded JSON includes those users with hashes (and salts) and does not include any plaintext user password

### Requirement: Maintenance restores settings after confirm
Restore settings SHALL ask the operator to confirm before applying a file. After confirm, the host SHALL replace MQTT, Zigbee, hardware SPI speed, and the registered device list from the file. When `ui.theme` is present, the host SHALL apply it to the **signed-in user** only. When a users list is present, the host SHALL replace the users table from that list (hashes and salts, never plaintext passwords). The host SHALL leave Wi-Fi unchanged even if the file contains a wifi object. A file that is not valid JSON, or that lacks a usable settings body, SHALL be rejected and SHALL NOT write settings. Only `isAdmin` SHALL restore settings. Devices Export/Restore on the Devices page SHALL remain.

#### Scenario: Confirm then apply
- **WHEN** the operator chooses Restore settings, selects a valid export file, and confirms
- **THEN** MQTT, Zigbee, hardware, theme for the signed-in user, devices, and users (when present in the file) match the file and Wi-Fi settings are unchanged

#### Scenario: Cancel confirm
- **WHEN** the restore confirm dialog is visible and the operator cancels
- **THEN** no settings or devices are written

#### Scenario: Wi-Fi in file is ignored
- **WHEN** the file includes a wifi password different from the running host and the operator confirms restore
- **THEN** the host Wi-Fi password is unchanged

#### Scenario: Users in file replace the table
- **WHEN** an `isAdmin` operator confirms restore of a valid file whose users list differs from the host
- **THEN** the host users table matches the file (hashes, not plaintext passwords)

### Requirement: Maintenance theme picker
System → Maintenance SHALL show a Theme control with options **Light** and **Dark**. Light SHALL be the current gray/white chrome. Dark SHALL use a cool slate-night dashboard: near-black blue-gray page and sidebar, slightly lighter cards, cool off-white body text, muted blue-gray secondary text, and steel-blue primary buttons with dark labels. No magenta, pink, or red-violet accents. Changing the picker SHALL apply that theme to the whole console immediately without a reload. The last **saved** theme for the **signed-in user** SHALL load after login; the login form SHALL use Dark. Changing the picker without Save SHALL NOT persist; a later reload with the same session SHALL restore that user’s last saved theme.

#### Scenario: Dark applies immediately
- **WHEN** the operator opens Maintenance and selects Dark
- **THEN** the page, sidebar, cards, fields, dialogs, tables, log viewer, and buttons use the dark tokens without a reload

#### Scenario: Unsaved change is lost on reload
- **WHEN** the signed-in user’s saved theme is Light, the operator selects Dark, and then reloads without Save
- **THEN** the console opens in Light

### Requirement: Theme save icon
Immediately to the right of the Theme picker SHALL be a compact icon-only Save control (not a full-width bar). Activating it SHALL persist the picker value on the signed-in user without restart. After persist, a later login of that same user SHALL open in that theme.

#### Scenario: Save dark
- **WHEN** the signed-in operator selects Dark and clicks the Theme Save icon
- **THEN** a later login of that same user opens in Dark

#### Scenario: Icon sits beside picker
- **WHEN** the operator opens System → Maintenance
- **THEN** the Save icon is on the same row, immediately to the right of the Theme picker

### Requirement: Login form before the console
The first time a browser opens the console URL without a valid session, the operator MUST see a login form and MUST NOT see the sidebar console. The form MUST include user name, password, and a Remember me checkbox. The form MUST NOT be HTTP Basic. The login view MUST use the existing dark (slate-night) theme. Field layout beyond those three controls MAY be refined later. After a successful login the console MUST appear. After logout or when the session ends, the login form MUST return.

#### Scenario: First visit
- **WHEN** a browser opens `http://<host-ip>/` with no session
- **THEN** the page shows the dark login form with user name, password, and Remember me, and does not show Status or other sidebar sections

#### Scenario: Successful login
- **WHEN** the operator submits a valid user name and password
- **THEN** the login form is hidden and the console sidebar is shown

### Requirement: Sidebar and System tabs follow roles
The console MUST hide main sections and System folder tabs the signed-in user is not allowed to use. A simple user MUST see Status, Devices, System → Log, and System → Maintenance (Appearance only). A simple user MUST NOT see WiFi, MQTT, ZigBee, System → Update, System → Hardware, System → Security, or Maintenance Settings (export/restore). `editUsers` or `isAdmin` MUST show System → Security. `isAdmin` MUST see every main section and every System tab.

#### Scenario: Simple user navigation
- **WHEN** a simple user is signed in
- **THEN** the sidebar shows Status, Devices, and System, and System shows Log and Maintenance, and WiFi, MQTT, ZigBee, Update, Hardware, and Security are not shown

#### Scenario: editUsers sees Security
- **WHEN** a user with `editUsers` and no `isAdmin` is signed in
- **THEN** System includes a Security tab

### Requirement: Device controls follow roles
On Devices, a user without `addDevices` and without `isAdmin` MUST NOT see or complete Add device. A user without `editDevices` and without `isAdmin` MUST NOT open the parameter edit dialog for a registered device. A user without `removeDevices` and without `isAdmin` MUST NOT delete a registered device. A simple user MUST still use row type actions and Manual command. Delete MUST still ask the operator to confirm.

#### Scenario: Simple user actions only
- **WHEN** a simple user opens Devices
- **THEN** Add device is not available, edit parameters is not available, delete is not available, and row **…** actions and Manual command still work

#### Scenario: addDevices without edit
- **WHEN** a user has `addDevices` and not `editDevices` or `isAdmin`
- **THEN** that user can add a device and cannot open edit on an already registered row

### Requirement: Security tab manages users
System → Security MUST show a table of users (user name, added-at, roles, blocked, theme) and MUST allow add and edit dialogs for operators who have `editUsers` or `isAdmin`. Password fields MUST never display a stored hash. An `editUsers` operator who is not `isAdmin` MUST NOT create `isAdmin`, MUST NOT edit or delete a user who has `isAdmin`, and MUST NOT see an enabled admin-role control for those actions. `isAdmin` MUST be able to create and edit `isAdmin` users. Deleting a user MUST ask the operator to confirm. A save, block, role change, or delete that would leave zero unlocked `isAdmin` users MUST fail and MUST show an error; the users table MUST stay unchanged.

#### Scenario: Non-admin cannot create admin
- **WHEN** an `editUsers` operator opens Add user
- **THEN** the admin role cannot be assigned

#### Scenario: Admin row is locked for editUsers
- **WHEN** an `editUsers` operator views a user who has `isAdmin`
- **THEN** that operator cannot save changes to that row or delete it

#### Scenario: Last unlocked admin cannot be blocked
- **WHEN** the only unlocked `isAdmin` user is edited to blocked
- **THEN** the console does not persist the change and shows an error

### Requirement: Appearance theme is the signed-in user
The Theme picker and Save icon MUST stay on System → Maintenance → Appearance (same place as today). After login the console MUST apply that user’s saved theme. Saving Theme there MUST persist that theme on the signed-in user only. The login form MUST stay dark regardless of any user theme. A later login of the same user MUST reopen in that user’s last saved theme. The control MUST NOT move to Security or to a new page.

#### Scenario: User theme after login
- **WHEN** user `alice` has theme `light` and signs in from the dark login form
- **THEN** the console chrome switches to Light

#### Scenario: Save theme is per user
- **WHEN** `alice` saves Dark and `bob` has Light saved
- **THEN** `alice` later logs in to Dark and `bob` later logs in to Light

