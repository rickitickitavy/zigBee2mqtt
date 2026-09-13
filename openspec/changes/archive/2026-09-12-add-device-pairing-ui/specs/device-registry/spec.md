## Purpose

Keeps the operator’s Zigbee device map on the Wi-Fi host, with a reserved main-settings tail, a device list after that reserve, pairing search results in RAM, and EEPROM writes limited to the bytes that changed.

## ADDED Requirements

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
Starting a device search from the host SHALL open the slave join window. While search is active, each slave join report for an IEEE that is not already registered SHALL appear in the host found-device list without waiting for a page reload. A found entry SHALL include at least IEEE and, when the slave sent them, manufacturer and model.

#### Scenario: Search then join
- **WHEN** the operator starts Search and a new unregistered device joins
- **THEN** that device appears in the found list while Search is still open

#### Scenario: Already registered ignored
- **WHEN** a join report arrives for an IEEE that is already registered
- **THEN** that IEEE is not added to the found list

### Requirement: Registering a found device requires operator Save
Selecting a found device and confirming Add SHALL identify that IEEE for the parameter form. The host SHALL create or update a registered record only when the operator saves the parameter form with a friendly name. Empty MQTT topics MAY be stored empty. Save SHALL fail when the device list has no free slot.

#### Scenario: Save after Add
- **WHEN** the operator Adds a found device, fills a friendly name, and saves
- **THEN** that IEEE is a registered device on the host

#### Scenario: Map full
- **WHEN** every device slot is used and the operator tries to save a new IEEE
- **THEN** the host rejects the save and the list is unchanged
