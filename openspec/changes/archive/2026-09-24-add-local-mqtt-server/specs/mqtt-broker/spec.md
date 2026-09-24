## Purpose

Gives the host an optional lightweight MQTT 3.1.1 broker so the gateway can replace an external broker (including Home Assistant) on the LAN when SERVER TYPE is local.

## ADDED Requirements

### Requirement: Local broker starts only for SERVER TYPE local
The host MUST start the onboard MQTT broker only when stored SERVER TYPE is `local` and Wi-Fi has a usable IPv4 address (AP or STA). The host MUST NOT start the broker when SERVER TYPE is `disable` or `remote`. Changing away from `local` (after the usual MQTT save/restart) MUST leave the broker not listening.

#### Scenario: Local after Wi-Fi has an address
- **WHEN** SERVER TYPE is `local` and the host has an AP or STA IPv4 address
- **THEN** the onboard MQTT broker is listening on the stored PORT

#### Scenario: Disabled
- **WHEN** SERVER TYPE is `disable`
- **THEN** the onboard MQTT broker is not listening

#### Scenario: Remote client-only
- **WHEN** SERVER TYPE is `remote`
- **THEN** the onboard MQTT broker is not listening

### Requirement: Remote clients can connect
While the onboard broker is listening, it MUST accept MQTT connections from clients on the LAN using the host’s AP or STA address and the stored PORT. The listen socket MUST NOT be restricted to loopback.

#### Scenario: LAN client
- **WHEN** the local broker is listening and a remote MQTT client connects to the host IPv4 address on the stored PORT
- **THEN** the broker accepts the TCP connection (CONNECT still subject to credential rules)

### Requirement: CONNECT authorization follows stored user and password
When both stored MQTT USERNAME and PASSWORD are non-empty, the onboard broker MUST require a matching username and password on CONNECT and MUST reject anonymous or wrong credentials. When USERNAME or PASSWORD is empty, the broker MUST accept CONNECT without MQTT authentication.

#### Scenario: Both credentials set
- **WHEN** USERNAME and PASSWORD are both non-empty and a client CONNECTs with a different password
- **THEN** the broker rejects that CONNECT

#### Scenario: Either credential empty
- **WHEN** USERNAME is empty or PASSWORD is empty and a client CONNECTs with no MQTT user or password
- **THEN** the broker accepts that CONNECT

### Requirement: Host client uses the local broker
When SERVER TYPE is `local` and the broker is listening, the host MQTT client MUST connect to that local broker (loopback) on the stored PORT using CLIENT_ID and the same USERNAME/PASSWORD rule as remote mode. Device topic publish and subscribe behavior MUST stay the same as today’s client.

#### Scenario: Host joins its own broker
- **WHEN** SERVER TYPE is `local` and the broker is listening
- **THEN** the host MQTT client is connected to the local broker and can publish device state
