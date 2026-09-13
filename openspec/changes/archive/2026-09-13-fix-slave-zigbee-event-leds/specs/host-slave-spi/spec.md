## RENAMED Requirements

- FROM: `### Requirement: Host resets and brings up the slave asynchronously`
- TO: `### Requirement: Host resets the slave first then brings it up`

## MODIFIED Requirements

### Requirement: Host resets the slave first then brings it up

The host SHALL drive a reset line (GPIO11 to the slave EN pin, active-low pulse). The **first** step of host role init SHALL be that pulse in **sync** mode: the host SHALL assert reset, wait the pulse width, and release reset before it starts Wi-Fi, the web console, MQTT, LittleFS, or the host SPI task. The host SHALL NOT finish that first pulse from the SPI pump task. On **every** host boot the host SHALL perform that first-step pulse. The host SHALL NOT skip it because the slave already asserts the ready line, because IRQ is already high, or because the slave still answers from a previous run. After releasing that first pulse, the host SHALL wait for `SLAVE_READY` without blocking Wi-Fi, MQTT, web, or `loop`. If `SLAVE_READY` does not arrive within the ready timeout, the host SHALL pulse reset again and wait again. Those later retries SHALL NOT use blocking delays.

#### Scenario: Slave ready then configure

- **WHEN** the host starts and has completed the first-step slave reset
- **THEN** it continues other host work and, when `SLAVE_READY` arrives, pushes settings and then accepts normal Zigbee operations

#### Scenario: Slave silent

- **WHEN** the ready timeout elapses with no `SLAVE_READY`
- **THEN** the host pulses reset again and repeats the async wait

#### Scenario: Host boot always resets a live slave

- **WHEN** the host boots (including a warm restart) and the slave is still running from before
- **THEN** the host still performs the first-step reset pulse and does not skip it because the slave already looks ready

#### Scenario: Reset is the first host-init step

- **WHEN** the chip starts in the host role
- **THEN** the slave reset pulse is completed before Wi-Fi, the web console, MQTT, or the host SPI task start
