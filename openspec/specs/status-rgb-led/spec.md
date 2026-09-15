# status-rgb-led Specification

## Purpose

Gives each board an onboard RGB (GPIO8) and four external red LEDs (GPIO18–21) that work together. RGB is red for boot/critical (green off). Host RGB is green while MQTT is connected. Slave RGB is green when the chip is ready and has no critical fault. Slave pairing blinks blue.

## Requirements

### Requirement: RGB and LED1–LED4 work together

Each chip SHALL drive LED1 on GPIO18, LED2 on GPIO19, LED3 on GPIO20, LED4 on GPIO21, and the onboard WS2812 on GPIO8. Host LED3 and LED4 SHALL stay off. RGB green SHALL mean host MQTT connected, or slave ready with no fault. A HIGH GPIO level SHALL light each external LED.

#### Scenario: Pins

- **WHEN** status indication is shown
- **THEN** host uses LED1, LED2, and RGB only; slave uses LED1–LED4 and RGB

### Requirement: RGB red while that chip is booting or in a critical error

Each chip SHALL turn the onboard RGB **red** as soon as firmware starts, before Serial, Wi-Fi, SPI, or Zigbee init, and SHALL keep it red until that chip is ready, and while a critical error is present. LED1–LED4 SHALL NOT be held on for boot or critical error. MQTT connection SHALL NOT be required to clear host boot red. Critical-error red SHALL outrank pairing blink and activity pulses.

#### Scenario: Host still preparing

- **WHEN** the host has started but Wi-Fi has no address yet or the slave is not yet in normal work
- **THEN** the host onboard RGB stays red and host LED1–LED4 stay off except later activity pulses after boot

#### Scenario: Host preparation finished

- **WHEN** the host has a usable Wi-Fi address and the slave link is in normal work
- **THEN** host RGB is no longer held red for boot

#### Scenario: Slave still preparing

- **WHEN** the slave has started but has not yet applied host settings or started the coordinator
- **THEN** the slave onboard RGB stays red and LED1–LED4 stay off except later activity pulses after boot

#### Scenario: Slave preparation finished

- **WHEN** the slave has applied host settings and the Zigbee coordinator is running, and no critical error is present
- **THEN** slave RGB is no longer held red for boot and stays **green** while no critical error is present

#### Scenario: Critical error appears

- **WHEN** a chip enters a critical error
- **THEN** that chip’s onboard RGB stays red and green stays off until that error is cleared

### Requirement: Host RGB green while MQTT is connected

After host boot red is cleared, the host onboard RGB SHALL stay **green** while the MQTT client is connected to the broker. It SHALL not be green while MQTT is disabled, STA is down, or the broker is disconnected. Host LED3 SHALL stay off.

#### Scenario: Broker connected

- **WHEN** the host is ready and MQTT is connected
- **THEN** the host onboard RGB is green and host LED3 stays off

#### Scenario: Broker disconnected

- **WHEN** the host is ready and MQTT is not connected
- **THEN** the host onboard RGB is not held green

### Requirement: Slave RGB green when ready

After slave boot red is cleared, and while no critical error is present, the slave onboard RGB SHALL stay **green** to show the chip is ready. While boot or a critical error is present, RGB SHALL be red and green SHALL be off.

#### Scenario: Slave ready

- **WHEN** the slave coordinator is running and no critical error is present
- **THEN** the slave onboard RGB is green

#### Scenario: Slave trouble

- **WHEN** the slave is still booting or a critical error is present
- **THEN** the slave onboard RGB is red and green is off

### Requirement: Host pulses LED1 and LED2 for MQTT device traffic

After host boot red is cleared, the host SHALL pulse LED1 for 0.1 seconds when it receives a device command from MQTT, and SHALL pulse LED2 for 0.1 seconds when it successfully publishes a device state message. Host LED3 and LED4 SHALL NOT light. RGB SHALL NOT flash green or red for those MQTT events.

#### Scenario: MQTT device command received

- **WHEN** the host is ready and receives an MQTT message on a registered device command topic
- **THEN** host LED1 is on for 0.1 seconds

#### Scenario: MQTT device state published

- **WHEN** the host is ready and successfully publishes a device state message
- **THEN** host LED2 is on for 0.1 seconds

### Requirement: Slave pulses LED1–LED4 for Zigbee packets

After slave boot red is cleared, and while no critical error is present:

- LED1 for 0.1 seconds when a packet is received from an **unregistered** device
- LED2 for 0.1 seconds when a packet is received from a **registered** device
- LED3 for 0.1 seconds when a ZCL default-response ACK is processed
- LED4 for 0.1 seconds **only** when a Zigbee command is sent to a device

LED1 SHALL NOT pulse for a registered-device packet. RGB green SHALL NOT flash for a command send.

#### Scenario: Packet from an unregistered device

- **WHEN** the slave is ready and receives a Zigbee event from an unregistered IEEE
- **THEN** LED1 is on for 0.1 seconds and LED2 does not pulse

#### Scenario: Packet from a registered device

- **WHEN** the slave is ready and receives a Zigbee event from a registered IEEE
- **THEN** LED2 is on for 0.1 seconds and LED1 does not pulse

#### Scenario: Command sent to a device

- **WHEN** the slave is ready and sends a Zigbee command to an end device
- **THEN** LED4 is on for 0.1 seconds and the RGB does not turn green

#### Scenario: ACK for a command

- **WHEN** the slave is ready and a ZCL default response for a command is processed
- **THEN** LED3 is on for 0.1 seconds

### Requirement: Slave pairing blinks the onboard blue LED

While pairing is open and the slave is ready with no critical error, the slave SHALL blink the onboard WS2812 **blue**. Pairing blink MUST NOT run while boot or critical-error red is held. LED1–LED4 SHALL keep their packet-pulse roles during pairing.

#### Scenario: Pairing after ready

- **WHEN** the slave is ready, no critical error is present, and pairing is open
- **THEN** the onboard LED blinks blue and LED1–LED4 still pulse for Zigbee packets

#### Scenario: Pairing during bring-up

- **WHEN** pairing would start but slave boot red is still held
- **THEN** the RGB stays red and does not blink blue
