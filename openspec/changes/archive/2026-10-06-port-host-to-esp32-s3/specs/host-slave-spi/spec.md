## MODIFIED Requirements

### Requirement: Host resets the slave first then brings it up
The host SHALL drive a reset line to the slave EN pin (active-low pulse) using the host GPIO assigned as `PIN_SLAVE_RST` on the ESP32-S3 pin map (design: GPIO **9**). The **first** step of host init SHALL be that pulse in **sync** mode: assert reset, wait the pulse width, and release reset before Wi-Fi, the web console, MQTT, LittleFS, or the host SPI task start. On **every** host boot the host SHALL perform that first-step pulse. After release, the host SHALL wait for `SLAVE_READY` without blocking Wi-Fi, MQTT, web, or `loop`. If `SLAVE_READY` does not arrive within the ready timeout, the host SHALL pulse reset again and wait again. Those later retries SHALL NOT use blocking delays.

#### Scenario: Slave ready then configure
- **WHEN** the host starts and has completed the first-step slave reset
- **THEN** it continues other host work and, when `SLAVE_READY` arrives, pushes settings and then accepts normal Zigbee operations

#### Scenario: Ready timeout
- **WHEN** the ready timeout elapses with no `SLAVE_READY`
- **THEN** the host pulses slave reset again and resumes waiting without blocking the Arduino `loop`

### Requirement: Inter-chip SPI pins on the host
The ESP32-S3 host SHALL use the following GPIOs for the inter-chip link unless a later ADR supersedes the map: SPI SCK **12**, MOSI **11**, MISO **13**, CS **10**, IRQ **14**, slave EN/reset **9**. The host SHALL NOT use GPIOs reserved for octal flash/PSRAM (typically 26–37) for these signals.

#### Scenario: Host SPI master wiring
- **WHEN** the host boots as SPI master
- **THEN** it drives CS/SCK/MOSI and samples MISO/IRQ on the GPIOs listed above
