# Spec Delta

## ADDED Requirements

### Requirement: HTTP POST bodies are bounded and request-isolated
The host web console MUST collect each authenticated JSON POST body with a fixed maximum size. Bodies larger than that limit MUST be rejected with an HTTP error and MUST NOT be applied. Concurrent or overlapping body uploads MUST NOT corrupt another request’s body. Chunk append MUST NOT grow storage one character at a time.

#### Scenario: Oversized restore rejected
- **WHEN** an admin posts a body larger than the configured maximum to a JSON settings or restore endpoint
- **THEN** the host responds with an error and does not change persisted settings or the device list from that body

#### Scenario: Concurrent posts do not share one body buffer
- **WHEN** two HTTP POST handlers receive body chunks overlapping in time
- **THEN** each request uses only its own collected body when the handler runs

### Requirement: Large console JSON does not grow unbounded Strings
Device-list, settings-export, and gateway-status JSON responses served by the host MUST be produced without unbounded Arduino `String` concatenation that reallocates for every field. The operator-visible JSON field set for those APIs MUST remain compatible with the existing console.

#### Scenario: Devices list still loads
- **WHEN** a signed-in operator opens Devices after this change
- **THEN** `GET` of the registered-device list returns JSON the console can render as before

#### Scenario: Settings export still downloads
- **WHEN** an admin exports settings from the console
- **THEN** the download contains the same logical groups and device list the export used before

### Requirement: Console stays usable during STA scan and MQTT reconnect
While the host performs a blocking Wi‑Fi network scan or an MQTT TCP connect/reconnect, the HTTP console on port 80 and host SPI keepalive work MUST remain able to progress within a few seconds (no multi-tens-of-seconds freeze of the main service loop caused by those operations running synchronously on that loop).

#### Scenario: Status answers during MQTT reconnect
- **WHEN** the remote MQTT broker is unreachable and the host is attempting reconnect
- **THEN** opening Status or calling `GET /api/status` still returns within a few seconds

#### Scenario: Console answers during STA rescan
- **WHEN** STA reconnect triggers a network scan
- **THEN** the web console on the current AP or STA address still answers HTTP within a few seconds
