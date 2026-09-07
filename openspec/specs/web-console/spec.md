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

### Requirement: Main navigation uses sidebar sections

The console MUST present five main sections labeled Status, WiFi, ZigBee, Devices, and System as a left sidebar strip (horizontal folder strip on a narrow viewport). Selecting a main section MUST show that section’s panel and hide the others without a full page reload. Status, ZigBee, and Devices MUST render as empty placeholder panels in this change.

#### Scenario: Switch main section

- **WHEN** the operator selects WiFi after Status was visible
- **THEN** the WiFi panel is shown and the Status panel is hidden

#### Scenario: Placeholder tabs have no forms

- **WHEN** the operator opens Status, ZigBee, or Devices
- **THEN** the panel has no configuration controls and does not change device settings

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

When System is selected, the console MUST present two nested folder-style tabs labeled Update and Log. Selecting one MUST show that panel and hide the other without a full page reload.

#### Scenario: Nested System tabs

- **WHEN** the operator opens System and then selects Log
- **THEN** the Log panel is shown and the Update panel is hidden

### Requirement: Firmware update from the Update tab

The Update tab MUST show the running firmware version and accept a firmware binary upload over HTTP. A successful write MUST program the inactive OTA application slot and then restart the device. A failed write MUST leave the running application slot unchanged, MUST NOT restart, and MUST show a failure on the page.

#### Scenario: Successful firmware upload

- **WHEN** the operator uploads a valid firmware image from the Update tab
- **THEN** the image is written to the inactive OTA slot and the device restarts into the new image

#### Scenario: Failed firmware upload

- **WHEN** the upload is aborted or the image is rejected
- **THEN** the currently running firmware remains unchanged and the page reports the failure

### Requirement: Log tab shows recent firmware logs

The Log tab MUST display recent firmware log lines that are also emitted on USB when console logging is enabled. The page MUST be able to refresh that list without leaving the Log tab. Older lines MAY drop when the in-memory buffer is full.

#### Scenario: New log line appears

- **WHEN** the firmware emits a log line and the operator refreshes the Log tab (automatically or manually)
- **THEN** that line appears in the log view (unless it has already aged out of the buffer)

#### Scenario: USB logging still works

- **WHEN** a log line is emitted
- **THEN** it still appears on the USB serial console when USB debug logging is enabled
