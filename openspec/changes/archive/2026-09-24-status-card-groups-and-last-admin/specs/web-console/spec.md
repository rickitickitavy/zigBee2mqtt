## ADDED Requirements

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

## MODIFIED Requirements

### Requirement: Status shows counts, traffic, and version
The Status section SHALL show host online time, master and slave firmware versions, registered device count, online device count, packets sent, packets received, MQTT local status, MQTT active topic count, MQTT connection count, Wi-Fi mode, BSSID, and Wi-Fi signal strength. Status SHALL obtain these values from the gateway status API. Status SHALL NOT contain settings forms that write configuration.

#### Scenario: Open Status with devices
- **WHEN** the operator opens Status and three devices are registered with one online
- **THEN** Status shows device count 3, online count 1, master firmware version from the running image, slave firmware version from the last slave report, and the packet counters

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
