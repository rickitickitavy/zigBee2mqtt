## ADDED Requirements

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
System → Security MUST show a table of users (user name, added-at, roles, blocked, theme) and MUST allow add and edit dialogs for operators who have `editUsers` or `isAdmin`. Password fields MUST never display a stored hash. An `editUsers` operator who is not `isAdmin` MUST NOT create `isAdmin`, MUST NOT edit or delete a user who has `isAdmin`, and MUST NOT see an enabled admin-role control for those actions. `isAdmin` MUST be able to create and edit `isAdmin` users. Deleting a user MUST ask the operator to confirm.

#### Scenario: Non-admin cannot create admin
- **WHEN** an `editUsers` operator opens Add user
- **THEN** the admin role cannot be assigned

#### Scenario: Admin row is locked for editUsers
- **WHEN** an `editUsers` operator views a user who has `isAdmin`
- **THEN** that operator cannot save changes to that row or delete it

### Requirement: Appearance theme is the signed-in user
The Theme picker and Save icon MUST stay on System → Maintenance → Appearance (same place as today). After login the console MUST apply that user’s saved theme. Saving Theme there MUST persist that theme on the signed-in user only. The login form MUST stay dark regardless of any user theme. A later login of the same user MUST reopen in that user’s last saved theme. The control MUST NOT move to Security or to a new page.

#### Scenario: User theme after login
- **WHEN** user `alice` has theme `light` and signs in from the dark login form
- **THEN** the console chrome switches to Light

#### Scenario: Save theme is per user
- **WHEN** `alice` saves Dark and `bob` has Light saved
- **THEN** `alice` later logs in to Dark and `bob` later logs in to Light

## MODIFIED Requirements

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
