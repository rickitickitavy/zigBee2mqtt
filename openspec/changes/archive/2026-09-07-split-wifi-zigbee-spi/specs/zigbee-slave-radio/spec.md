## Purpose

Keeps the Zigbee coordinator on the slave independent of host Wi-Fi mode, after the host has pushed settings for that boot.

## ADDED Requirements

### Requirement: Zigbee independent of Wi-Fi mode
After the host has accepted a settings-applied result for the current slave boot, the slave SHALL keep the Zigbee coordinator running while powered and not in a fatal radio error, including while the host is in SoftAP, STA, or recovery AP. Until that settings apply, the slave SHALL run SPI and the RAM log only. The host SHALL NOT start a Zigbee radio on its own chip.

#### Scenario: Host in SoftAP
- **WHEN** the host is serving SoftAP (including recovery AP) and slave settings have been applied
- **THEN** the slave Zigbee coordinator remains up and continues to receive and send

#### Scenario: Host STA connected
- **WHEN** the host is joined to a router and slave settings have been applied
- **THEN** the slave Zigbee coordinator remains up and continues to receive and send

#### Scenario: Before settings
- **WHEN** the slave has booted but has not yet applied host `SET_SETTINGS`
- **THEN** it does not start the coordinator and only runs SPI plus the RAM log

### Requirement: Host Wi-Fi stays usable
With the two-chip split, the host SHALL run Wi-Fi (SoftAP or STA) without enabling IEEE 802.15.4 coexist on the host radio. Operator-visible services on the host (ping, HTTP web console, MQTT client) SHALL remain usable while the slave Zigbee radio is active.

#### Scenario: Web while mesh is up
- **WHEN** the slave coordinator is started and the host has a usable Wi-Fi interface
- **THEN** the host web console and ICMP to the host address remain reachable without requiring Zigbee to be stopped
