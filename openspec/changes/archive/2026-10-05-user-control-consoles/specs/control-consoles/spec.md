# Spec Delta

## Purpose

Host-persisted control consoles that household users open after login: tabbed layouts of widgets and bound devices with live on/off, covering, and sensor controls.

## ADDED Requirements

### Requirement: Consoles persist with widgets and devices
The host MUST persist control consoles. Each console MUST have a unique id, a name, an active flag, and an ordered list of widgets. Each widget MUST have a title, a show-group-on-off flag, and an ordered list of device bindings (registered device, optional channel, title, optional unit). Empty consoles MUST be allowed. After reboot the same consoles MUST load. The host MUST allow at most 32 consoles.

#### Scenario: Save and reload
- **WHEN** an editor saves console `Kitchen` with one widget that binds an On/Off device
- **THEN** after reboot `Kitchen` still has that widget and binding

#### Scenario: Thirty-third console rejected
- **WHEN** the host already has 32 consoles and an editor saves a new one
- **THEN** the host does not persist the thirty-third console and returns an error

### Requirement: Inactive consoles are hidden from users
A console that is not active MUST NOT appear as a tab for simple users even if those users are assigned to it. Editors MUST still see inactive consoles in the Consoles table.

#### Scenario: Assigned but inactive
- **WHEN** user `alice` is assigned console `Garden` and `Garden` is not active
- **THEN** `alice` does not see a `Garden` tab

### Requirement: User page shows assigned active consoles as tabs
A simple user MUST see one top tab per active console assigned to that user. Each tab MUST show only that console’s widgets and devices. If the user has no visible consoles, the page MUST show that none are assigned.

#### Scenario: Two assigned consoles
- **WHEN** simple user `bob` is assigned active consoles `Kitchen` and `Hall`
- **THEN** the control page shows tabs `Kitchen` and `Hall` and selecting `Hall` shows only `Hall` content

#### Scenario: None assigned
- **WHEN** a simple user has no assigned active consoles
- **THEN** the page does not show control tabs with widgets

### Requirement: Widgets stack in a reorderable grid
Widgets MUST sit in a grid. In edit mode the editor MUST drag a widget onto a new cell to change order (including left/right). A widget-styled add control MUST sit after the last widget and MUST move down when a widget is added.

#### Scenario: Add widget
- **WHEN** the editor activates the add-widget control
- **THEN** a new widget appears and the add control sits after it

#### Scenario: Drag widget
- **WHEN** the editor drops widget B onto widget A’s place
- **THEN** the saved order places B where A was

### Requirement: Widget title and group On/Off
Each widget MUST have a title. The editor MUST set the title and whether the group On/Off control is shown. In view mode, if group On/Off is shown, it MUST be On when at least one bound On/Off device in that widget is On, and Off when all such devices are Off. Toggling group On/Off in view mode MUST switch every bound On/Off device in that widget. Coverings and sensors MUST NOT be included in that group control.

#### Scenario: Group on when any on
- **WHEN** a widget has two On/Off devices and one is On
- **THEN** the group switch shows On

#### Scenario: Group toggle all
- **WHEN** the user turns the group switch Off
- **THEN** every On/Off device in that widget is commanded Off

### Requirement: Device bindings match device class
An editor MUST add a device from registered devices, pick a channel when the device has more than one channel, and set a row title. For a measurement sensor the editor MUST pick a unit from a list for that class (temperature, pressure, humidity, illuminance, flow, speed, direction, and similar). On/Off rows MUST show a switch. Window covering rows MUST show up, down, stop, and current position. Sensor rows MUST show the live value in the chosen unit.

#### Scenario: Multi-channel On/Off
- **WHEN** the editor adds a two-channel On/Off device and selects channel 2
- **THEN** the row controls only channel 2

#### Scenario: Covering controls
- **WHEN** a window covering is bound
- **THEN** the row shows up, down, stop, and position

#### Scenario: Temperature unit
- **WHEN** the editor adds a temperature sensor
- **THEN** the unit list includes Celsius, Fahrenheit, and Kelvin

### Requirement: Edit mode does not command devices
While a console is open in the editor dialog, the host MUST NOT send On/Off, covering, or other device commands from widget or row controls. View mode on the user page MUST send those commands using the same device actions as today.

#### Scenario: Editor flips a switch
- **WHEN** the editor toggles an On/Off control inside the unsaved edit dialog
- **THEN** the device is not commanded

#### Scenario: User flips a switch
- **WHEN** a simple user toggles an On/Off control on an assigned console
- **THEN** the host sends the On/Off command for that device and channel

### Requirement: Edit devices by click and drag
In edit mode the editor MUST reorder device rows by drag and drop and MUST open the device-binding dialog again by activating an existing row. The add-device control MUST be visible per widget in edit mode only.

#### Scenario: Reorder rows
- **WHEN** the editor drops device row 2 above row 1 and saves
- **THEN** the persisted widget lists that device first

#### Scenario: Edit binding
- **WHEN** the editor activates an existing row and changes its title
- **THEN** after save the row shows the new title

### Requirement: Save, export, and empty-only import
The editor MUST persist the console only when Save is used. Export MUST download that console as JSON. Import from JSON MUST be enabled only when the console under edit has no widgets and no device bindings. Import MUST replace the empty layout from a valid export file.

#### Scenario: Export then import empty
- **WHEN** the editor exports console A and later imports that JSON into a new empty console
- **THEN** the new console matches A’s widgets and bindings

#### Scenario: Import disabled when not empty
- **WHEN** the console under edit already has a widget
- **THEN** Import from JSON is not available

### Requirement: Simple users read layouts they are assigned
A simple-user session MUST receive layouts only for that user’s assigned active consoles. That session MUST NOT create, update, delete, import, or export console definitions. `editConsoles` or `isAdmin` MUST be required to mutate consoles.

#### Scenario: Simple user cannot save layout
- **WHEN** a simple-user session posts a console create or update
- **THEN** the host does not persist it

#### Scenario: Unassigned console hidden
- **WHEN** a simple-user session requests a console they are not assigned
- **THEN** the host does not return that console’s layout

### Requirement: Console data is heap-grown with Wi-Fi headroom
Console objects, widget lists, device bindings, and each user’s assigned-console list MUST be allocated at runtime (no compile-time arrays of consoles, widgets, or bindings). The host MUST refuse a create or grow that would leave too little free heap for AsyncWebServer and Wi‑Fi, and MUST free heap when a console, widget, binding, or assignment is removed.

#### Scenario: Grow then shrink
- **WHEN** an editor adds a widget and later deletes that widget and saves
- **THEN** that widget is gone after reload and its heap is released

#### Scenario: Low heap refuses grow
- **WHEN** free heap is at or below the reserved Wi‑Fi/HTTP floor and the editor adds a widget
- **THEN** the host does not add the widget and shows an error
