## ADDED Requirements

### Requirement: Console HTML and CSS ship inside the firmware image

The host MUST serve the web console HTML and CSS from assets that are part of the application firmware image for the running build. The console page and styles MUST NOT depend on a separately flashed LittleFS web image. Opening `/` or `/index.html` MUST return the console HTML for that firmware version. Requests under `/css/` MUST return the matching stylesheet from the same image.

#### Scenario: Root after firmware-only flash

- **WHEN** the operator has applied a firmware image that includes the console and has not flashed a separate web filesystem image
- **THEN** opening `http://<device-ip>/` returns the console HTML for that firmware version

#### Scenario: Stylesheet from the same image

- **WHEN** the signed-in console page requests its stylesheet under `/css/`
- **THEN** the browser receives the CSS that belongs to that same firmware build

### Requirement: Firmware upload alone updates the console UI

A successful firmware OTA (existing slave-then-host flow) MUST be sufficient to update both the application and the web console UI. The System → Update tab MUST NOT offer a separate LittleFS or web-filesystem upload as the way to refresh console HTML/CSS.

#### Scenario: Update tab has firmware only for UI

- **WHEN** the operator opens System → Update
- **THEN** the page shows the firmware upload path and does not show a filesystem upload control for refreshing the console UI

#### Scenario: New UI after firmware OTA

- **WHEN** the operator completes a successful firmware update that embeds newer console assets
- **THEN** the next full page load of the console shows that newer HTML/CSS without a separate filesystem flash

## REMOVED Requirements

### Requirement: Update tab can flash the web filesystem

**Reason**: Console HTML/CSS now ship inside the firmware image, so a separate LittleFS image is no longer the source of truth for the UI and a second upload path risks version skew.

**Migration**: Flash only the firmware binary via System → Update (or USB). Do not use `pio run -t uploadfs` or `POST /update/data` to refresh the console. LittleFS remains for OTA staging and other non-UI data.
