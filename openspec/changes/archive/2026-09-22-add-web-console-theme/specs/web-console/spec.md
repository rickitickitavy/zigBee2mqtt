## ADDED Requirements

### Requirement: Maintenance theme picker
System → Maintenance SHALL show a Theme control with options **Light** and **Dark**. Light SHALL be the current gray/white chrome. Dark SHALL use a cool slate-night dashboard: near-black blue-gray page and sidebar, slightly lighter cards, cool off-white body text, muted blue-gray secondary text, and steel-blue primary buttons with dark labels. No magenta, pink, or red-violet accents. Changing the picker SHALL apply that theme to the whole console immediately without a reload. The last **saved** theme SHALL load when the page opens; if none is stored the console SHALL open in Light. Changing the picker without Save SHALL NOT persist; a later reload SHALL restore the last saved theme.

#### Scenario: Dark applies immediately
- **WHEN** the operator opens Maintenance and selects Dark
- **THEN** the page, sidebar, cards, fields, dialogs, tables, log viewer, and buttons use the dark tokens without a reload

#### Scenario: Unsaved change is lost on reload
- **WHEN** the saved theme is Light, the operator selects Dark, and then reloads without Save
- **THEN** the console opens in Light

### Requirement: Theme save icon
Immediately to the right of the Theme picker SHALL be a compact icon-only Save control (not a full-width bar). Activating it SHALL persist the picker value on the host without restart. After persist, a later load of the console on that host SHALL open in that theme.

#### Scenario: Save dark
- **WHEN** the operator selects Dark and clicks the Theme Save icon
- **THEN** a later page load on that host opens in Dark

#### Scenario: Icon sits beside picker
- **WHEN** the operator opens System → Maintenance
- **THEN** the Save icon is on the same row, immediately to the right of the Theme picker

## MODIFIED Requirements

### Requirement: Maintenance exports settings without Wi-Fi
System → Maintenance SHALL show **Export settings** above **Restore settings**. Both SHALL use compact inline buttons, not full-width bars. Export SHALL download a JSON file that includes MQTT settings, Zigbee settings, hardware SPI speed, the saved UI theme, and the persisted device list (same device fields as Devices Export, no live telemetry). The file SHALL NOT contain a Wi-Fi group (no BSSID, password, MODE, AP IP, hostname, or OTG).

#### Scenario: Export omits Wi-Fi
- **WHEN** the operator clicks Export settings and the host has Wi-Fi, MQTT, Zigbee, hardware, a saved theme, and at least one registered device
- **THEN** the downloaded JSON has mqtt, zigbee, hardware, ui theme, and devices, and has no wifi object or Wi-Fi password

#### Scenario: Buttons stay compact
- **WHEN** the operator opens System → Maintenance
- **THEN** Export settings and Restore settings are stacked and no wider than a normal inline button

### Requirement: Maintenance restores settings after confirm
Restore settings SHALL ask the operator to confirm before applying a file. After confirm, the host SHALL replace MQTT, Zigbee, hardware SPI speed, the saved UI theme, and the registered device list from the file. The host SHALL leave Wi-Fi unchanged even if the file contains a wifi object. A file that is not valid JSON, or that lacks a usable settings body, SHALL be rejected and SHALL NOT write settings. Devices Export/Restore on the Devices page SHALL remain.

#### Scenario: Confirm then apply
- **WHEN** the operator chooses Restore settings, selects a valid export file, and confirms
- **THEN** MQTT, Zigbee, hardware, theme, and devices match the file and Wi-Fi settings are unchanged

#### Scenario: Cancel confirm
- **WHEN** the restore confirm dialog is visible and the operator cancels
- **THEN** no settings or devices are written

#### Scenario: Wi-Fi in file is ignored
- **WHEN** the file includes a wifi password different from the running host and the operator confirms restore
- **THEN** the host Wi-Fi password is unchanged
