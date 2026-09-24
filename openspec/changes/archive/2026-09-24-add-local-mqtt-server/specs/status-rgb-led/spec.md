## ADDED Requirements

### Requirement: Host LED4 on while the local MQTT broker is listening
After host boot red is cleared, and while no critical error is present, host LED4 SHALL stay **on** while the onboard MQTT broker is listening. Host RGB SHALL NOT use blue for the local broker. Host LED3 SHALL stay off.

#### Scenario: Local broker listening
- **WHEN** the host is ready and the local MQTT broker is listening
- **THEN** host LED4 is on and host RGB is not held blue

#### Scenario: Local broker down
- **WHEN** the host is ready and the local MQTT broker is not listening
- **THEN** host LED4 is not held on for the broker

## MODIFIED Requirements

### Requirement: Host RGB green while MQTT is connected

After host boot red is cleared, the host onboard RGB SHALL stay **green** while MQTT is connected, whether SERVER TYPE is `remote` or `local`. It SHALL not be green while SERVER TYPE is `disable`, or while MQTT is not connected. Host LED3 SHALL stay off.

#### Scenario: Remote broker connected

- **WHEN** the host is ready, SERVER TYPE is `remote`, and MQTT is connected
- **THEN** the host onboard RGB is green and host LED3 stays off

#### Scenario: Local broker connected

- **WHEN** the host is ready, SERVER TYPE is `local`, the local MQTT broker is listening, and MQTT is connected
- **THEN** the host onboard RGB is green and host LED4 is on

#### Scenario: Broker disconnected

- **WHEN** the host is ready and MQTT is not connected and the local broker is not listening
- **THEN** the host onboard RGB is not held green
