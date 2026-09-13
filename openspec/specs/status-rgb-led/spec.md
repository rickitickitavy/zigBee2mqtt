# status-rgb-led Specification

## Purpose

Gives each DevKitC board a single onboard RGB meaning so an operator can see boot still in progress, slave SPI application traffic, and a slave critical fault without opening the console.

## Requirements

### Requirement: Boot red until that chip is ready

Each chip SHALL turn the onboard status RGB red as soon as that chip’s role setup starts. The host SHALL keep boot-red until it has a usable Wi-Fi interface (STA associated or SoftAP serving an address) and the slave link is in normal work after settings have been applied. The slave SHALL keep boot-red until it has applied host settings for this boot and the Zigbee coordinator is running. MQTT connection SHALL NOT be required to clear host boot-red. After that chip’s boot-red clears, the LED SHALL go dark unless another requirement in this capability holds it or flashes it.

#### Scenario: Host still preparing

- **WHEN** the host has started but Wi-Fi has no address yet or the slave is not yet in normal work
- **THEN** the host status RGB stays red

#### Scenario: Host preparation finished

- **WHEN** the host has a usable Wi-Fi address and the slave link is in normal work
- **THEN** the host status RGB is no longer held red for boot

#### Scenario: Slave still preparing

- **WHEN** the slave has started but has not yet applied host settings or started the coordinator
- **THEN** the slave status RGB stays red

#### Scenario: Slave preparation finished

- **WHEN** the slave has applied host settings and the Zigbee coordinator is running, and no critical error is present
- **THEN** the slave status RGB is no longer held red for boot

### Requirement: Slave flashes green on receive and red on send

After slave boot-red is cleared, and while no critical error is present and pairing is not blinking, the slave SHALL turn the status RGB green for 0.1 seconds when it receives an application SPI frame from the host, and red for 0.1 seconds when it sends an application SPI frame to the host. Periodic link keep-alives (ping, time sync, and empty event polls) MUST NOT trigger a flash. If a send flash and a receive flash overlap, the later event SHALL replace the color for a new 0.1 second window.

#### Scenario: Application frame from host

- **WHEN** the slave is ready, idle, and decodes an application command from the host
- **THEN** the status RGB is green for 0.1 seconds and then returns to dark

#### Scenario: Application frame to host

- **WHEN** the slave is ready, idle, and clocks out a queued application event to the host
- **THEN** the status RGB is red for 0.1 seconds and then returns to dark

#### Scenario: Keep-alive does not flash

- **WHEN** the host only pings, time-syncs, or polls for events with no application payload
- **THEN** the slave status RGB does not flash for that transfer

### Requirement: Slave holds red while a critical error is present

The slave SHALL turn the status RGB red and keep it red for the entire time a critical error is present. A critical error is a fatal or unrecoverable fault that stops normal radio or SPI work (for example SPI slave hardware failed to start, or the Zigbee coordinator failed fatally). Transient log errors that the slave can continue after MUST NOT hold the LED. Critical-error red SHALL outrank boot-red, activity flashes, and pairing blink. When the critical error is no longer present, the LED SHALL follow the other requirements in this capability.

#### Scenario: Critical error appears

- **WHEN** the slave enters a critical error
- **THEN** the status RGB stays red until that error is cleared

#### Scenario: Activity during critical error

- **WHEN** a critical error is present and an application SPI frame is received or sent
- **THEN** the status RGB stays red and does not flash green

### Requirement: Pairing blink yields to boot-red and critical-error red

While pairing is open and the slave is ready with no critical error, the existing pairing blink on the same status RGB MAY continue. Pairing blink MUST NOT run while boot-red or critical-error red is held. Activity flashes MUST NOT run while pairing is blinking.

#### Scenario: Pairing after ready

- **WHEN** the slave is ready, no critical error is present, and pairing is open
- **THEN** the status RGB uses the pairing blink and does not show receive/send flashes

#### Scenario: Pairing during bring-up

- **WHEN** pairing would start but slave boot-red is still held
- **THEN** the status RGB stays red for boot and does not blink for pairing
