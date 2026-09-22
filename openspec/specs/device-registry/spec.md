# device-registry Specification

## Purpose

Keeps the operator’s Zigbee device map on the Wi-Fi host, with a reserved main-settings tail, a device list after that reserve, pairing search results in RAM, and EEPROM writes limited to the bytes that changed.

## Requirements

### Requirement: Registered devices live only on the host
The host SHALL persist the registered-device list in its settings store. The Zigbee slave SHALL NOT persist that list. A join report from the slave SHALL NOT by itself create a registered record.

#### Scenario: Join without Save
- **WHEN** the slave reports a joining device and the operator has not saved that IEEE on the host
- **THEN** the device is eligible for the found list and MUST NOT appear as a registered device

#### Scenario: Slave reboot
- **WHEN** the slave reboots after the host has registered a device
- **THEN** the host still has that registered record after its own settings load

### Requirement: Main settings end with a reserved aligned tail
The host settings image SHALL place wifi, mqtt, zigbee, and other current main fields in a main block. After those fields the main block SHALL include a reserved region of 256 bytes. The start of that reserved region SHALL be aligned to 128 bytes from the start of the settings image. The registered device list SHALL begin immediately after the reserved region and SHALL provide 128 slots. Uninitialized reserved bytes SHALL be zero. The 128-slot list SHALL NOT be stored inside the 4 KiB EEPROM main image (it does not fit); it SHALL live on the host immediately after that image in persistent storage.

#### Scenario: Layout after migrate
- **WHEN** settings are saved at the new version
- **THEN** the device list does not overlap the reserved 256-byte tail and the reserved tail starts on a 128-byte boundary

#### Scenario: Reserved bytes unused
- **WHEN** the operator saves only a device record
- **THEN** the reserved tail is left as stored zeros or previous reserved data and is not used as device fields

#### Scenario: One hundred twenty-eight slots
- **WHEN** the host settings are at the new version
- **THEN** the operator can register up to 128 devices and a 129th save is rejected

### Requirement: Settings writes touch only dirty EEPROM bytes
A settings save SHALL program only the EEPROM bytes that differ from the last committed image (or a contiguous dirty span that contains those bytes). A save that changes one registered device SHALL NOT rewrite unchanged main-block bytes. A save that changes only wifi or mqtt SHALL NOT rewrite unchanged device-list bytes.

#### Scenario: Save one new device
- **WHEN** the operator saves a newly registered device and main settings are unchanged
- **THEN** EEPROM writes are limited to the device-list bytes for that save

#### Scenario: Save Wi-Fi only
- **WHEN** the operator saves Wi-Fi settings and the device list is unchanged
- **THEN** EEPROM writes do not include unchanged device-list bytes

### Requirement: Search opens pairing and lists found unregistered devices
Starting a device search from the host SHALL open the slave join window. While search is active, each slave join report for an IEEE that is not already registered SHALL appear in the host found-device list without waiting for a page reload. A found entry SHALL include at least IEEE and a device type (`unknown` if the slave did not send one), and, when the slave sent them, manufacturer and model.

#### Scenario: Search then join
- **WHEN** the operator starts Search and a new unregistered device joins
- **THEN** that device appears in the found list while Search is still open

#### Scenario: Already registered ignored
- **WHEN** a join report arrives for an IEEE that is already registered
- **THEN** that IEEE is not added to the found list

#### Scenario: Found type from join
- **WHEN** an unregistered device joins with classified type `onOff`
- **THEN** the found list entry for that IEEE includes type `onOff`

### Requirement: Registering a found device requires operator Save
Selecting a found device and confirming Add SHALL identify that IEEE for the parameter form. The host SHALL create or update a registered record only when the operator saves the parameter form with a friendly name. Empty MQTT topics MAY be stored empty. Save SHALL fail when the device list has no free slot.

#### Scenario: Save after Add
- **WHEN** the operator Adds a found device, fills a friendly name, and saves
- **THEN** that IEEE is a registered device on the host

#### Scenario: Map full
- **WHEN** every device slot is used and the operator tries to save a new IEEE
- **THEN** the host rejects the save and the list is unchanged

### Requirement: Registered devices store FULL CONTROL
Each registered device record SHALL store a FULL CONTROL flag. The default SHALL be off, which SHALL keep existing command behavior (channels mapping, then on/off/toggle). A save that omits the flag SHALL leave a new record off and SHALL NOT turn an existing record on. The host SHALL persist the flag with the device list. The slave SHALL NOT be required to persist or interpret this flag.

#### Scenario: New device default
- **WHEN** the operator saves a device without a FULL CONTROL value
- **THEN** that record has FULL CONTROL off and MQTT commands still use on/off/toggle

#### Scenario: Flag survives host reload
- **WHEN** a device is saved with FULL CONTROL on and the host reloads settings
- **THEN** that device still has FULL CONTROL on

### Requirement: Registered devices store cluster device type
Each registered device record SHALL store a device type of `unknown`, `onOff`, `iasZone`, or `windowCovering`. Type is a persisted device setting: it SHALL appear in `devices.json`, in `SpiCmdSetDevice` / dump payloads, and in list JSON after reboot. Readonly UI SHALL prevent the operator from editing type; it SHALL NOT skip persistence. A new save from a found device SHALL copy the type from that found record. A save that omits type on a new record SHALL store `unknown`. After a record has a type other than `unknown`, later operator saves SHALL leave that type unchanged even if the request body includes a different type. If the stored type is `unknown` and the host later receives a join classification other than `unknown` for the same IEEE, the host SHALL update the stored type to that classification. The slave SHALL persist the same type on its device dump so a host pull cannot replace a known type with `unknown`.

#### Scenario: Save from found list
- **WHEN** the operator saves a found device whose join type is `iasZone`
- **THEN** the registered record has type `iasZone`

#### Scenario: Operator cannot overwrite a known type
- **WHEN** a registered device already has type `onOff` and the operator saves parameters with a different type in the request
- **THEN** the record still has type `onOff`

#### Scenario: Unknown fills in later
- **WHEN** a registered IEEE is `unknown` and a later join classifies it as `windowCovering`
- **THEN** the registered record becomes `windowCovering`

### Requirement: Device list JSON includes type
Host device list APIs and the persisted device store SHALL include the stored type for each registered device so the console and future actions can read it.

#### Scenario: List after save
- **WHEN** the operator requests the registered device list and a device was saved as `onOff`
- **THEN** that device’s JSON includes type `onOff`

### Requirement: Device list JSON includes runtime status and battery
Host registered-device list APIs SHALL include last known per-endpoint status and battery percent when the host has received those values. Status entries SHALL include the source endpoint (`ep`) and the last type-specific payload for that endpoint. Battery SHALL be an integer percent derived from Power Configuration Battery Percentage Remaining when a valid report exists. These fields are runtime telemetry: they SHALL appear after reboot only if a new report or a start-time read has arrived; they SHALL NOT be written into the persisted device list, `devices.json`, or EEPROM device slots. A list replace, export, or restore SHALL NOT require or store these fields.

#### Scenario: List after on/off report
- **WHEN** the operator requests the registered device list and endpoint 2 of that IEEE last reported `ON`
- **THEN** that device’s JSON includes a status entry for `ep` `2` with payload `ON`

#### Scenario: List after battery report
- **WHEN** the host has a valid battery percentage remaining that decodes to 67 percent
- **THEN** that device’s list JSON includes battery `67`

#### Scenario: Persist does not keep telemetry
- **WHEN** the host saves or exports the device list after it has cached battery and status
- **THEN** the persisted document has no battery or live status fields, and those values are absent after a reboot until a new report or start-time read arrives
