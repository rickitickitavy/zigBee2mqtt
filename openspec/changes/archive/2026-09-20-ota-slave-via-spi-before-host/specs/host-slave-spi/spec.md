# Spec Delta

## ADDED Requirements

### Requirement: Host can send firmware to the slave over SPI

The SPI protocol SHALL carry a host-to-slave firmware-update sequence: begin, data chunks that fit in a single framed payload, and end. Each command SHALL use the existing framed length and integrity check. The host SHALL enqueue those commands asynchronously and SHALL NOT block Wi-Fi, HTTP, MQTT, or `loop` waiting for the whole image to transfer. A missing or late slave reply SHALL abort the firmware-update sequence. The slave SHALL write received firmware to its inactive application slot. On a successful end, the slave SHALL commit that slot and restart into the new application. On failure, the slave SHALL leave its running application unchanged and SHALL NOT restart.

#### Scenario: Chunked transfer

- **WHEN** the host sends a firmware image larger than one SPI payload
- **THEN** the slave receives it as multiple chunks and, after a successful end, restarts into that image

#### Scenario: Failed chunk aborts

- **WHEN** a firmware chunk is corrupted or the slave times out during an in-progress firmware update
- **THEN** the slave does not commit a new application and the host treats the firmware update as failed
