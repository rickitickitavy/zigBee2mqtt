# Spec Delta

## MODIFIED Requirements

### Requirement: Firmware update from the Update tab

The Update tab MUST show the running firmware version and accept a firmware binary upload over HTTP. After a valid firmware file is received, the host MUST program the slave’s inactive application slot over SPI first. Only after the slave reports a successful firmware commit MUST the host program its own inactive application slot and restart. A failed HTTP receive, a failed slave transfer, or a failed slave commit MUST leave the host’s running application unchanged, MUST NOT restart the host, and MUST show a failure on the page. Filesystem upload on the same tab is unchanged by this requirement.

#### Scenario: Successful firmware upload

- **WHEN** the operator uploads a valid firmware image from the Update tab and the slave commit succeeds
- **THEN** the slave is running or restarting into that image before the host restarts into the same image

#### Scenario: Failed firmware upload

- **WHEN** the upload is aborted or the image is rejected
- **THEN** the currently running firmware remains unchanged and the page reports the failure

#### Scenario: Slave OTA fails

- **WHEN** the HTTP firmware file is accepted but the slave transfer or slave commit fails
- **THEN** the host does not restart, the host keeps its previously running application, and the Update tab reports the failure

## ADDED Requirements

### Requirement: Firmware Update shows slave-then-host progress

After the browser finishes sending the firmware file, the Update tab SHALL show that the gateway is updating the slave, then the host, until success or failure. The HTTP file-upload progress bar alone SHALL NOT be treated as a completed firmware update.

#### Scenario: Status after file lands

- **WHEN** the firmware file has finished uploading over HTTP and the slave is still being programmed
- **THEN** the Update tab shows that the slave update is in progress and does not claim the device has already restarted

#### Scenario: Phase percents

- **WHEN** firmware update is in progress
- **THEN** the Update tab shows upload-to-master percent, master-to-slave percent, and host-apply percent together with the current phase

### Requirement: Firmware Update warns before host reboot

When the host is about to restart after a successful firmware write, the web UI SHALL show a modal warning that the router will reboot, SHALL count down a 30 second wait, and SHALL ping the router in the background until it responds or the wait expires.

#### Scenario: Router returns within wait

- **WHEN** the host restarts after firmware apply and `/api/version` succeeds within 30 seconds
- **THEN** the dialog reports that the update finished successfully

#### Scenario: Router wait times out

- **WHEN** the host does not answer `/api/version` within 30 seconds after the reboot warning
- **THEN** the dialog reports a wait timeout error
