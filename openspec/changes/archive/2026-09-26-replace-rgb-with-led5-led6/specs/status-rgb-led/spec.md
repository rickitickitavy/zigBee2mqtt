## ADDED Requirements

### Requirement: Onboard RGB is unused

Each chip SHALL NOT drive the onboard WS2812 for status. Status SHALL use only LED1–LED6.

#### Scenario: RGB stays off

- **WHEN** the chip is booting, ready, pairing, connected to MQTT, or in a critical error
- **THEN** the onboard RGB LED is not used for that indication

## MODIFIED Requirements

### Requirement: RGB and LED1–LED4 work together

Each chip SHALL drive LED1 on GPIO18, LED2 on GPIO19, LED3 on GPIO20, LED4 on GPIO21, LED5 on GPIO2, and LED6 on GPIO3. Host LED3 and host LED6 SHALL stay off. Host LED4 SHALL stay on while the local MQTT broker is listening. Host LED5 SHALL mean MQTT connected (solid) or boot/critical (blink). Slave LED5 SHALL mean ready with no fault (solid) or boot/critical (blink). A HIGH GPIO level SHALL light each external LED. The onboard WS2812 SHALL NOT be used.

#### Scenario: Pins

- **WHEN** status indication is shown
- **THEN** host uses LED1, LED2, LED4, and LED5; slave uses LED1–LED6

### Requirement: RGB red while that chip is booting or in a critical error

Each chip SHALL show its former RGB-red meaning as soon as firmware starts, before Serial, Wi-Fi, SPI, or Zigbee init, and SHALL keep that indication until that chip is ready, and while a critical error is present. The **host** and the **slave** SHALL blink **LED5** with period 0.25 seconds (125 ms on, 125 ms off) for boot and critical error. LED1–LED4 SHALL NOT be held on for boot or critical error. Host LED6 SHALL stay off. MQTT connection SHALL NOT be required to clear host boot indication. Critical-error indication SHALL outrank pairing blink and activity pulses.

#### Scenario: Host still preparing

- **WHEN** the host has started but Wi-Fi has no address yet or the slave is not yet in normal work
- **THEN** host LED5 blinks with period 0.25 seconds and host LED1–LED4 and LED6 stay off except later activity pulses after boot

#### Scenario: Host preparation finished

- **WHEN** the host has a usable Wi-Fi address and the slave link is in normal work
- **THEN** host LED5 is no longer blinking for boot

#### Scenario: Host lost the slave after boot

- **WHEN** the host had finished boot and the slave SPI link is no longer healthy (not in normal work, or ping replies have timed out)
- **THEN** host LED5 blinks with period 0.25 seconds until that link is healthy again

#### Scenario: Host slave link restored

- **WHEN** the host was holding critical LED5 blink for a lost slave and the slave link is healthy again
- **THEN** host LED5 is no longer blinking for that error

#### Scenario: Slave still preparing

- **WHEN** the slave has started but has not yet applied host settings or started the coordinator
- **THEN** slave LED5 blinks with period 0.25 seconds and LED1–LED4 and LED6 stay off except later activity pulses after boot

#### Scenario: Slave preparation finished

- **WHEN** the slave has applied host settings and the Zigbee coordinator is running, and no critical error is present
- **THEN** slave LED5 is no longer blinking for boot and stays on while no critical error is present

#### Scenario: Critical error appears

- **WHEN** a chip enters a critical error
- **THEN** that chip blinks LED5 with period 0.25 seconds and does not hold LED5 solid, until that error is cleared

### Requirement: Host RGB green while MQTT is connected

After host boot indication is cleared, the host SHALL hold **LED5** on while MQTT is connected, whether SERVER TYPE is `remote` or `local`. LED5 SHALL not stay on while SERVER TYPE is `disable`, or while MQTT is not connected. Host LED3 SHALL stay off.

#### Scenario: Remote broker connected

- **WHEN** the host is ready, SERVER TYPE is `remote`, and MQTT is connected
- **THEN** host LED5 is on and host LED3 stays off

#### Scenario: Local broker connected

- **WHEN** the host is ready, SERVER TYPE is `local`, the local MQTT broker is listening, and MQTT is connected
- **THEN** host LED5 is on and host LED4 is on

#### Scenario: Broker disconnected

- **WHEN** the host is ready and MQTT is not connected and the local broker is not listening
- **THEN** host LED5 is not held on

### Requirement: Host LED4 on while the local MQTT broker is listening

After host boot indication is cleared, and while no critical error is present, host LED4 SHALL stay **on** while the onboard MQTT broker is listening. Host LED3 SHALL stay off. The onboard RGB SHALL NOT be used for the local broker.

#### Scenario: Local broker listening

- **WHEN** the host is ready and the local MQTT broker is listening
- **THEN** host LED4 is on and the onboard RGB is not used

#### Scenario: Local broker down

- **WHEN** the host is ready and the local MQTT broker is not listening
- **THEN** host LED4 is not held on for the broker

### Requirement: Slave RGB green when ready

After slave boot indication is cleared, and while no critical error is present, the slave SHALL hold **LED5** on to show the chip is ready. While boot or a critical error is present, LED5 SHALL blink with period 0.25 seconds and SHALL NOT stay solid.

#### Scenario: Slave ready

- **WHEN** the slave coordinator is running and no critical error is present
- **THEN** slave LED5 is on and not blinking

#### Scenario: Slave trouble

- **WHEN** the slave is still booting or a critical error is present
- **THEN** slave LED5 blinks with period 0.25 seconds and is not held solid

### Requirement: Host pulses LED1 and LED2 for MQTT device traffic

After host boot indication is cleared, the host SHALL pulse LED1 for 0.1 seconds when it receives a device command from MQTT, and SHALL pulse LED2 for 0.1 seconds when it successfully publishes a device state message. Host LED3 SHALL NOT light. Host LED4 SHALL stay on while the local MQTT broker is listening and SHALL NOT pulse for MQTT traffic. LED5 SHALL NOT flash for those MQTT events.

#### Scenario: MQTT device command received

- **WHEN** the host is ready and receives an MQTT message on a registered device command topic
- **THEN** host LED1 is on for 0.1 seconds

#### Scenario: MQTT device state published

- **WHEN** the host is ready and successfully publishes a device state message
- **THEN** host LED2 is on for 0.1 seconds

### Requirement: Slave pulses LED1–LED4 for Zigbee packets

After slave boot indication is cleared, and while no critical error is present:

- LED1 for 0.1 seconds when a packet is received from an **unregistered** device
- LED2 for 0.1 seconds when a packet is received from a **registered** device
- LED3 for 0.1 seconds when the radio confirms delivery of an outbound command (MAC ACK of our TX)
- LED4 for 0.1 seconds **only** when a Zigbee command is actually handed to the radio

LED1 SHALL NOT pulse for a registered-device packet. LED5 SHALL NOT flash for a command send.

#### Scenario: Packet from an unregistered device

- **WHEN** the slave is ready and receives a Zigbee event from an unregistered IEEE
- **THEN** LED1 is on for 0.1 seconds and LED2 does not pulse

#### Scenario: Packet from a registered device

- **WHEN** the slave is ready and receives a Zigbee event from a registered IEEE
- **THEN** LED2 is on for 0.1 seconds and LED1 does not pulse

#### Scenario: Command sent to a device

- **WHEN** the slave is ready and sends a Zigbee command to an end device
- **THEN** LED4 is on for 0.1 seconds and LED5 stays on for ready (does not flash)

#### Scenario: ACK for a command

- **WHEN** the slave is ready and the radio reports success for an outbound command
- **THEN** LED3 is on for 0.1 seconds

### Requirement: Slave pairing blinks the onboard blue LED

While pairing is open and the slave is ready with no critical error, the slave SHALL blink **LED6**. Pairing blink MUST NOT run while boot or critical-error indication is held. LED1–LED4 SHALL keep their packet-pulse roles during pairing. LED5 SHALL stay on for ready during pairing.

#### Scenario: Pairing after ready

- **WHEN** the slave is ready, no critical error is present, and pairing is open
- **THEN** LED6 blinks and LED1–LED4 still pulse for Zigbee packets and LED5 stays on

#### Scenario: Pairing during bring-up

- **WHEN** pairing would start but slave boot indication is still held
- **THEN** LED5 keeps blinking for boot and LED6 does not blink for pairing
