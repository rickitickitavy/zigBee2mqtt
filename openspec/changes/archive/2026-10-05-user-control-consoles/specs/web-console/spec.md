# Spec Delta

## MODIFIED Requirements

### Requirement: Login form before the console
The first time a browser opens the console URL without a valid session, the operator MUST see a login form and MUST NOT see the sidebar console. The form MUST include user name, password, and a Remember me checkbox. The form MUST NOT be HTTP Basic. The login view MUST use the existing dark (slate-night) theme. Field layout beyond those three controls MAY be refined later. After a successful login, a simple user MUST see the separate control-console page (top tabs, no gateway sidebar). A user who has any operator role MUST see the gateway console. After logout or when the session ends, the login form MUST return.

#### Scenario: First visit
- **WHEN** a browser opens `http://<host-ip>/` with no session
- **THEN** the page shows the dark login form with user name, password, and Remember me, and does not show Status or other sidebar sections

#### Scenario: Successful login
- **WHEN** the operator submits a valid user name and password
- **THEN** the login form is hidden and the console sidebar is shown

#### Scenario: Simple user login opens control page
- **WHEN** a simple user submits a valid user name and password
- **THEN** the gateway sidebar is not shown and the control-console page is shown

### Requirement: Sidebar and System tabs follow roles
The console MUST hide main sections and System folder tabs the signed-in user is not allowed to use. A simple user MUST NOT use the gateway sidebar. A user with operator roles and without `isAdmin` MUST see Status, Devices, System → Log, and System → Maintenance (Appearance only) plus any extra sections their roles allow. A user without `isAdmin` MUST NOT see WiFi, MQTT, ZigBee, System → Update, System → Hardware, or Maintenance Settings (export/restore) unless `isAdmin`. `editUsers` or `isAdmin` MUST show Security. `editConsoles` or `isAdmin` MUST show Consoles immediately after Security. `isAdmin` MUST see every main section and every System tab.

#### Scenario: Simple user navigation
- **WHEN** a simple user is signed in
- **THEN** the gateway sidebar is not shown

#### Scenario: editUsers sees Security
- **WHEN** a user with `editUsers` and no `isAdmin` is signed in
- **THEN** the sidebar includes Security

#### Scenario: Console editor sees Consoles after Security
- **WHEN** a user with `editConsoles` and `editUsers` and no `isAdmin` is signed in
- **THEN** the sidebar shows Consoles immediately after Security

#### Scenario: Admin sees Consoles without editConsoles
- **WHEN** an `isAdmin` user with `editConsoles` unset is signed in
- **THEN** the sidebar shows Consoles immediately after Security

### Requirement: Security tab manages users
Security MUST show a table of users (user name, added-at, roles, blocked, theme) and MUST allow add and edit dialogs for operators who have `editUsers` or `isAdmin`. Password fields MUST never display a stored hash. The user dialog MUST include a Console editor checkbox (`editConsoles`) and a group of checkboxes for existing control consoles (`consoles`). An `editUsers` operator who is not `isAdmin` MUST NOT create `isAdmin`, MUST NOT edit or delete a user who has `isAdmin`, and MUST NOT see an enabled admin-role control for those actions. `isAdmin` MUST be able to create and edit `isAdmin` users. Deleting a user MUST ask the operator to confirm. A save, block, role change, or delete that would leave zero unlocked `isAdmin` users MUST fail and MUST show an error; the users table MUST stay unchanged.

#### Scenario: Non-admin cannot create admin
- **WHEN** an `editUsers` operator opens Add user
- **THEN** the admin role cannot be assigned

#### Scenario: Admin row is locked for editUsers
- **WHEN** an `editUsers` operator views a user who has `isAdmin`
- **THEN** that operator cannot save changes to that row or delete it

#### Scenario: Last unlocked admin cannot be blocked
- **WHEN** the only unlocked `isAdmin` user is edited to blocked
- **THEN** the console does not persist the change and shows an error

#### Scenario: Assign consoles in user dialog
- **WHEN** consoles `Kitchen` and `Hall` exist and the operator edits user `bob`
- **THEN** the dialog shows checkboxes for `Kitchen` and `Hall` and saving checked `Kitchen` stores that assignment

### Requirement: Maintenance exports settings without Wi-Fi
System → Maintenance SHALL show **Export settings** above **Restore settings**. Both SHALL use compact inline buttons, not full-width bars. Export SHALL download a JSON file that includes MQTT settings, Zigbee settings, hardware SPI speed, the signed-in user’s theme, the persisted device list (same device fields as Devices Export, no live telemetry), the users table, and control consoles. Each exported user SHALL include user name, added-at, roles including `editConsoles`, `consoles`, `isBlocked`, theme, and password **hash and salt**. The file SHALL NOT contain a plaintext user password. The file SHALL NOT contain a Wi-Fi group (no BSSID, password, MODE, AP IP, hostname, or OTG). Only `isAdmin` SHALL see or use Export settings and Restore settings.

#### Scenario: Export omits Wi-Fi
- **WHEN** the operator clicks Export settings and the host has Wi-Fi, MQTT, Zigbee, hardware, a saved theme, and at least one registered device
- **THEN** the downloaded JSON has mqtt, zigbee, hardware, ui theme, and devices, and has no wifi object or Wi-Fi password

#### Scenario: Buttons stay compact
- **WHEN** the operator opens System → Maintenance
- **THEN** Export settings and Restore settings are stacked and no wider than a normal inline button

#### Scenario: Export includes users without plaintext passwords
- **WHEN** an `isAdmin` operator clicks Export settings and the host has at least one stored user
- **THEN** the downloaded JSON includes those users with hashes (and salts) and does not include any plaintext user password

#### Scenario: Export includes consoles
- **WHEN** the host has a saved control console and an `isAdmin` operator exports settings
- **THEN** the JSON includes that console’s name, active flag, and layout

### Requirement: Maintenance restores settings after confirm
Restore settings SHALL ask the operator to confirm before applying a file. After confirm, the host SHALL replace MQTT, Zigbee, hardware SPI speed, and the registered device list from the file. When `ui.theme` is present, the host SHALL apply it to the **signed-in user** only. When a users list is present, the host SHALL replace the users table from that list (hashes and salts, never plaintext passwords). When a consoles list is present, the host SHALL replace control consoles from that list. The host SHALL leave Wi-Fi unchanged even if the file contains a wifi object. A file that is not valid JSON, or that lacks a usable settings body, SHALL be rejected and SHALL NOT write settings. Only `isAdmin` SHALL restore settings. Devices Export/Restore on the Devices page SHALL remain.

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

#### Scenario: Consoles in file replace layouts
- **WHEN** an `isAdmin` operator confirms restore of a valid file that includes consoles
- **THEN** the host control consoles match the file

## ADDED Requirements

### Requirement: Consoles section lists and edits layouts
Consoles MUST show a table with columns Console name, Active, and Users (user names assigned that console). Add and row edit MUST open a large dialog for name, active, widgets, export, import (empty only), and Save. Closing the overlay MUST follow the same dirty-check and mousedown-dismiss rules as other dialogs. Delete MUST ask for confirmation.

#### Scenario: Table columns
- **WHEN** console `Kitchen` is active and assigned to `alice` and `bob`
- **THEN** the row shows `Kitchen`, active, and both user names

#### Scenario: Large edit dialog
- **WHEN** the operator adds or edits a console
- **THEN** a large dialog opens for that console’s content
