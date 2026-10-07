# Spec Delta

## ADDED Requirements

### Requirement: Hardware tab sets blue and green LED brightness

System → Hardware SHALL show SPI speed plus **BLUE LED brightness** and **GREEN LED brightness** as integer fields from 1 to 100 (percent). GET `/api/hardware` SHALL return `spiSpeedHz`, `blueLedBrightness`, and `greenLedBrightness`. POST `/api/hardware` SHALL require all three, reject values outside 1–100 for either brightness, persist them with hardware settings, apply them without requiring a reboot, and push brightness to the slave when the link is up. Factory default for both brightness fields SHALL be 100. Only `isAdmin` SHALL use Hardware.

#### Scenario: Load shows brightness

- **WHEN** an admin opens System → Hardware
- **THEN** blue and green brightness show the stored values (default 100 if never saved)

#### Scenario: Save applies brightness

- **WHEN** the admin sets blue to 40 and green to 70 and clicks Save
- **THEN** GET `/api/hardware` returns those values and host blue/green LEDs use them while on

#### Scenario: Out of range rejected

- **WHEN** POST `/api/hardware` sends `blueLedBrightness` 0 or 101
- **THEN** the host responds 400 and does not change stored brightness

### Requirement: Login Enter focuses empty password

On the login form, when the operator presses Enter and the password field is empty, the console MUST move focus to the password field and MUST NOT submit the login request. When the password is non-empty, Enter MAY submit as today.

#### Scenario: Empty password Enter

- **WHEN** the login form is shown, the password is empty, and the operator presses Enter
- **THEN** focus moves to the password field and no login request is sent

#### Scenario: Password filled Enter

- **WHEN** the login form has a non-empty password and the operator presses Enter
- **THEN** the form submits as today

### Requirement: Error and success plates hide after ten seconds

Whenever the web console shows an error plate (`save-error`) or success plate (`save-success`), including login errors, that plate MUST become hidden automatically after 10 seconds unless a newer message replaces it sooner. Hiding MUST NOT clear unrelated form fields.

#### Scenario: Success auto-hides

- **WHEN** a settings Save shows a success plate
- **THEN** that plate is hidden about 10 seconds later without a page reload

#### Scenario: Error auto-hides

- **WHEN** a Save shows an error plate
- **THEN** that plate is hidden about 10 seconds later

#### Scenario: Newer message resets the timer

- **WHEN** an error plate is visible and a later Save shows a success plate before 10 seconds
- **THEN** the success plate is shown and the previous error is not left visible after the new plate’s own 10-second window

## MODIFIED Requirements

### Requirement: Maintenance exports settings without Wi-Fi

System → Maintenance SHALL show **Export settings** above **Restore settings**. Both SHALL use compact inline buttons, not full-width bars. Export SHALL download a JSON file that includes MQTT settings, Zigbee settings, hardware SPI speed and blue/green LED brightness, the signed-in user’s theme, the persisted device list (same device fields as Devices Export, no live telemetry), the users table, and control consoles. Each exported user SHALL include user name, added-at, roles including `editConsoles`, `consoles`, `isBlocked`, theme, and password **hash and salt**. The file SHALL NOT contain a plaintext user password. The file SHALL NOT contain a Wi-Fi group (no BSSID, password, MODE, AP IP, hostname, or OTG). Only `isAdmin` SHALL see or use Export settings and Restore settings.

#### Scenario: Export omits Wi-Fi

- **WHEN** the operator clicks Export settings and the host has Wi-Fi, MQTT, Zigbee, hardware, a saved theme, and at least one registered device
- **THEN** the downloaded JSON has mqtt, zigbee, hardware, ui theme, and devices, and has no wifi object or Wi-Fi password

#### Scenario: Export includes LED brightness

- **WHEN** the operator exports settings after saving blue and green brightness
- **THEN** `hardware` in the JSON includes `blueLedBrightness` and `greenLedBrightness`

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

Restore settings SHALL ask the operator to confirm before applying a file. After confirm, the host SHALL replace MQTT, Zigbee, hardware SPI speed, blue/green LED brightness, and the registered device list from the file. When `ui.theme` is present, the host SHALL apply it to the **signed-in user** only. When a users list is present, the host SHALL replace the users table from that list (hashes and salts, never plaintext passwords). When a consoles list is present, the host SHALL replace control consoles from that list. The host SHALL leave Wi-Fi unchanged even if the file contains a wifi object. A file that is not valid JSON, or that lacks a usable settings body, SHALL be rejected and SHALL NOT write settings. Only `isAdmin` SHALL restore settings. Devices Export/Restore on the Devices page SHALL remain. A restore file whose `hardware` object lacks brightness fields SHALL keep the host’s current brightness values.

#### Scenario: Confirm then apply

- **WHEN** the operator chooses Restore settings, selects a valid export file, and confirms
- **THEN** MQTT, Zigbee, hardware, theme for the signed-in user, devices, and users (when present in the file) match the file and Wi-Fi settings are unchanged

#### Scenario: Restore applies LED brightness

- **WHEN** the file’s hardware has `blueLedBrightness` 25 and `greenLedBrightness` 80 and the operator confirms
- **THEN** those brightness values are stored and applied on the host (and pushed to the slave when linked)

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

### Requirement: Device controls follow roles

On Devices, a user without `addDevices` and without `isAdmin` MUST NOT see or complete Add device. A user without `editDevices` and without `isAdmin` MUST NOT open the parameter edit dialog for a registered device. A user without `removeDevices` and without `isAdmin` MUST NOT delete a registered device. A user without `controlDevices` and without `isAdmin` MUST NOT see row type-action controls (the **…** menu) or Manual command, and MUST NOT send those device commands from the Devices page. `isAdmin` MUST retain full Devices controls. Delete MUST still ask the operator to confirm. Control-console widget commands are out of scope for this requirement.

#### Scenario: Simple user actions only

- **WHEN** a simple user opens Devices
- **THEN** Add device is not available, edit parameters is not available, delete is not available, and the **…** menu and Manual command are not shown

#### Scenario: No controlDevices hides actions

- **WHEN** a signed-in user has Devices access but neither `controlDevices` nor `isAdmin`
- **THEN** the **…** menu and Manual command are not shown

#### Scenario: controlDevices can command

- **WHEN** a user has `controlDevices` and not `isAdmin`
- **THEN** that user can use the **…** menu and Manual command when the device type supports them

#### Scenario: addDevices without edit

- **WHEN** a user has `addDevices` and not `editDevices` or `isAdmin`
- **THEN** that user can add a device and cannot open edit on an already registered row

### Requirement: Security tab manages users

Security MUST show a table of users (user name, added-at, roles, blocked, theme) and MUST allow add and edit dialogs for operators who have `editUsers` or `isAdmin`. Password fields MUST never display a stored hash. The user dialog MUST include a Console editor checkbox (`editConsoles`), an **Other device control** checkbox (`controlDevices`), and a group of checkboxes for existing control consoles (`consoles`). An `editUsers` operator who is not `isAdmin` MUST NOT create `isAdmin`, MUST NOT edit or delete a user who has `isAdmin`, and MUST NOT see an enabled admin-role control for those actions. `isAdmin` MUST be able to create and edit `isAdmin` users. Deleting a user MUST ask the operator to confirm. A save, block, role change, or delete that would leave zero unlocked `isAdmin` users MUST fail and MUST show an error; the users table MUST stay unchanged.

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

#### Scenario: Other device control checkbox

- **WHEN** an `editUsers` or `isAdmin` operator opens Add or Edit user
- **THEN** the dialog shows an Other device control checkbox bound to `controlDevices`
