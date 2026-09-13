## ADDED Requirements

### Requirement: Slave flashes for Zigbee device events

After slave boot-red is cleared, and while no critical error is present and pairing is not blinking, the slave SHALL flash the status RGB for Zigbee device events only:

- **green** for 0.1 seconds when the coordinator sends a command event to an end device
- **red** for 0.1 seconds when an event is received from a registered device
- **blue** for 0.1 seconds when an event is received from an unregistered device

SPI frames, logs, keep-alives, settings, and other host–slave link traffic MUST NOT trigger a flash. If two flashes overlap, the later event SHALL replace the color for a new 0.1 second window.

#### Scenario: Command sent to a device

- **WHEN** the slave is ready, idle, and sends a Zigbee command to an end device
- **THEN** the status RGB is green for 0.1 seconds and then returns to dark

#### Scenario: Event from a registered device

- **WHEN** the slave is ready, idle, and receives a Zigbee event from a registered IEEE
- **THEN** the status RGB is red for 0.1 seconds and then returns to dark

#### Scenario: Event from an unregistered device

- **WHEN** the slave is ready, idle, pairing is not blinking, and a Zigbee event arrives from an unregistered IEEE
- **THEN** the status RGB is blue for 0.1 seconds and then returns to dark

#### Scenario: SPI traffic does not flash

- **WHEN** the host and slave exchange SPI frames with no Zigbee device event
- **THEN** the slave status RGB does not flash for that transfer

### Requirement: Host flashes for MQTT device traffic

After host boot-red is cleared, the host SHALL flash the status RGB for MQTT **device** traffic only:

- **green** for 0.1 seconds when it receives a device event from MQTT (a subscribed device command topic)
- **red** for 0.1 seconds when it successfully publishes a device message to MQTT (device state or availability)

Gateway topics (online/status, device list), failed publishes, and broker keep-alives MUST NOT trigger a flash. Boot-red SHALL outrank these pulses. If two flashes overlap, the later event SHALL replace the color for a new 0.1 second window.

#### Scenario: MQTT device command received

- **WHEN** the host is ready and receives an MQTT message on a registered device command topic
- **THEN** the host status RGB is green for 0.1 seconds and then returns to dark

#### Scenario: MQTT device state published

- **WHEN** the host is ready and successfully publishes a device state or availability message
- **THEN** the host status RGB is red for 0.1 seconds and then returns to dark

#### Scenario: Gateway MQTT does not flash

- **WHEN** the host publishes gateway status or the devices list, or only keeps the broker connection alive
- **THEN** the host status RGB does not flash for that traffic

## MODIFIED Requirements

### Requirement: Slave holds red while a critical error is present

The slave SHALL turn the status RGB red and keep it red for the entire time a critical error is present. A critical error is a fatal or unrecoverable fault that stops normal radio or SPI work (for example SPI slave hardware failed to start, or the Zigbee coordinator failed fatally). Transient log errors that the slave can continue after MUST NOT hold the LED. Critical-error red SHALL outrank boot-red, activity flashes, and pairing blink. When the critical error is no longer present, the LED SHALL follow the other requirements in this capability.

#### Scenario: Critical error appears

- **WHEN** the slave enters a critical error
- **THEN** the status RGB stays red until that error is cleared

#### Scenario: Activity during critical error

- **WHEN** a critical error is present and a Zigbee device event is sent or received
- **THEN** the status RGB stays red and does not flash green, red, or blue for that event

### Requirement: Pairing blink yields to boot-red and critical-error red

While pairing is open and the slave is ready with no critical error, the existing pairing blink on the same status RGB SHALL continue as it does today. Pairing blink MUST NOT run while boot-red or critical-error red is held. Activity flashes (green send, red registered receive, blue unregistered receive) MUST NOT run while pairing is blinking.

#### Scenario: Pairing after ready

- **WHEN** the slave is ready, no critical error is present, and pairing is open
- **THEN** the status RGB uses the pairing blink and does not show send or receive flashes

#### Scenario: Pairing during bring-up

- **WHEN** pairing would start but slave boot-red is still held
- **THEN** the status RGB stays red for boot and does not blink for pairing

## REMOVED Requirements

### Requirement: Slave flashes green on receive and red on send

**Reason**: Flashes meant SPI application frames, which are not Zigbee device events.
**Migration**: Use “Slave flashes for Zigbee device events” (green send, red registered receive, blue unregistered receive).
