## ADDED Requirements

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

## MODIFIED Requirements

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
