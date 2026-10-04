# Spec Delta

## ADDED Requirements

### Requirement: Search dialog offers WIFI DEVICE
The Add-device search dialog SHALL include a **WIFI DEVICE** button. Activating it SHALL close or leave the search dialog and open the device parameter dialog for a new `wifi` device with transport locked to Wi‑Fi. Pairing search SHALL NOT be required. IEEE SHALL be editable on that create form.

#### Scenario: Open WiFi create
- **WHEN** the operator opens Add device and clicks WIFI DEVICE
- **THEN** the parameter dialog opens with transport Wi‑Fi (not editable) and an editable IEEE field

### Requirement: Parameter dialog shows transport before NAME
The device parameter dialog SHALL show a transport field (Zigbee or Wi‑Fi) immediately before the NAME field. Transport SHALL be readonly after it is set for the dialog session. Zigbee add/edit SHALL show Zigbee. WIFI DEVICE create and `wifi` edit SHALL show Wi‑Fi.

#### Scenario: Zigbee edit order
- **WHEN** the operator opens parameters for a registered `zigbee` device
- **THEN** the settings fields include transport Zigbee immediately before NAME

#### Scenario: WiFi create shows WiFi
- **WHEN** the operator opens parameters via WIFI DEVICE
- **THEN** transport shows Wi‑Fi and cannot be changed to Zigbee in that dialog

### Requirement: MQTT card edits server-born topic and interval
The MQTT section SHALL show and allow saving SERVER BORN TOPIC and BORN INTERVAL (minutes). GET `/api/mqtt` and POST `/api/mqtt` SHALL include `serverBornTopic` and `bornIntervalMin`. The form SHALL load stored values when the MQTT section opens. Interval input SHALL use the stored clamp rules (5–120, default 20). Settings export and restore SHALL include both fields.

#### Scenario: Load born fields
- **WHEN** the host has `serverBornTopic` `homeassistant/status` and `bornIntervalMin` 30 and the operator opens MQTT
- **THEN** those fields show that topic and 30

#### Scenario: Save born fields
- **WHEN** the operator sets SERVER BORN TOPIC to `z2m/birth`, BORN INTERVAL to 15, and saves MQTT
- **THEN** GET `/api/mqtt` returns those values

## MODIFIED Requirements

### Requirement: Add device uses a search dialog then a parameter dialog
Add device SHALL open a search dialog that contains a table of found-but-unregistered devices, a Search control, and a WIFI DEVICE control. Opening the dialog SHALL NOT start pairing. Search SHALL start pairing on the slave only when clicked and SHALL update the found table as the host learns new joins. Add SHALL be enabled only when one found row is selected. Add SHALL close the search dialog and open a parameter dialog for that Zigbee device with transport `zigbee`. WIFI DEVICE SHALL open a parameter dialog for a new `wifi` device without requiring a found row. The Zigbee parameter dialog SHALL show IEEE (not editable) and SHALL accept friendly name and MQTT topic fields. The Wi‑Fi create dialog SHALL accept editable IEEE, friendly name, MQTT topic fields, and cluster type. Save SHALL persist the device and close the parameter dialog. Dismissing the search dialog SHALL stop pairing search.

#### Scenario: Search fills the found table
- **WHEN** the search dialog is open, the operator clicks Search, and an unregistered device joins
- **THEN** that device appears in the found table without closing the dialog

#### Scenario: Add then Save
- **WHEN** the operator selects a found device, clicks Add, fills the parameter form, and clicks Save
- **THEN** the dialogs are closed and the Devices table shows the new registered device with transport `zigbee`

#### Scenario: Cancel search
- **WHEN** the operator closes the search dialog without Add
- **THEN** no new registered device is written and pairing search stops

#### Scenario: WIFI DEVICE then Save
- **WHEN** the operator clicks WIFI DEVICE, fills IEEE, name, and topics, and saves
- **THEN** the Devices table shows the new registered device with transport `wifi`

### Requirement: Device parameters show readonly type
For a `zigbee` device, the device parameter dialog SHALL show the cluster device type as a readonly field. Add from Search SHALL show the found type. Edit of a registered `zigbee` device SHALL show the stored type. Save of a `zigbee` device SHALL persist other editable fields and SHALL NOT take a new type from an editable control. For a `wifi` device, the cluster device type SHALL be an editable control among Unknown, On/Off, IAS Zone, and Window covering, and Save SHALL persist the chosen type.

#### Scenario: Add shows found type
- **WHEN** the operator Adds a found `iasZone` device
- **THEN** the parameter dialog shows IAS Zone and the operator cannot edit that field

#### Scenario: Edit keeps stored type
- **WHEN** the operator opens parameters for a registered `zigbee` `onOff` device and saves a new name
- **THEN** the type remains `onOff` and the dialog still shows On/Off

#### Scenario: WiFi type editable
- **WHEN** the operator opens parameters for a registered `wifi` device
- **THEN** the cluster type control is editable and Save can change the stored type

### Requirement: MQTT card shows stored settings

When the operator opens the MQTT section, each MQTT field SHALL display the value currently stored on the host, including SERVER BORN TOPIC and BORN INTERVAL. Empty stored fields MAY stay empty; the form SHALL NOT stay blank when the device has MQTT settings.

#### Scenario: Open MQTT with saved server

- **WHEN** the host has a stored MQTT server and the operator opens the MQTT card
- **THEN** the server field (and the other MQTT fields, including born topic and interval) show the stored values
