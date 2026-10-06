## MODIFIED Requirements

### Requirement: Firmware update uploads one package for both chips
The Update tab MUST show the running firmware version and accept a **joined firmware package** upload over HTTP (ZIP containing `slave.bin` and `host.bin`). After a valid file is received, the host MUST program the slave from `slave.bin` over SPI first. Only after the slave reports a successful firmware commit MUST the host program itself from `host.bin` and restart. A failed HTTP receive, invalid package, failed slave transfer, or failed slave commit MUST leave the host’s running application unchanged, MUST NOT restart the host, and MUST show a failure on the page.

#### Scenario: Successful firmware upload
- **WHEN** the operator uploads a valid joined package from the Update tab and the slave commit succeeds
- **THEN** the host applies its image and restarts; both chips run the new firmware

#### Scenario: Failed firmware upload
- **WHEN** the upload is rejected or staging fails
- **THEN** the currently running firmware remains unchanged and the page reports the failure

#### Scenario: Slave OTA fails
- **WHEN** the joined package is accepted but the slave transfer or slave commit fails
- **THEN** the host does not apply its image, does not restart for update, and the page reports the failure
