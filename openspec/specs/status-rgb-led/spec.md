# status-rgb-led Specification

## Purpose

Gives each board six external status LEDs with a fixed color map per role (host and slave). Blue and green LEDs use PWM brightness from Hardware settings; red LEDs stay digital on/off. Host LED5 is solid while MQTT is connected and blinks (0.25 s) for boot only (not errors). Host LED6 is solid for critical error and blinks at 10 Hz during firmware update. Host LED4 is on while the local MQTT broker is listening. Slave LED5 is solid when ready and blinks (0.25 s) for boot/critical. Slave pairing blinks LED6. The onboard WS2812 is unused.

## Requirements

### Requirement: RGB and LED1–LED4 work together

Each chip SHALL drive LED1–LED6 on that board’s pin map (host ESP32-S3 and slave ESP32-C6 as in the Host status requirement / pins). Host LED3 SHALL stay off. Host LED4 SHALL stay on while the local MQTT broker is listening (after boot). Host LED5 SHALL mean MQTT connected (solid) or boot (blink) and SHALL NOT mean any error. Host LED6 SHALL mean critical error (solid) or firmware update cycle (10 Hz blink). Slave LED5 SHALL mean ready with no fault (solid) or boot/critical (blink). Slave LED6 SHALL mean pairing blink. Red LEDs SHALL light with digital HIGH when on. Blue and green LEDs SHALL light with PWM when on. The onboard WS2812 SHALL NOT be used.

#### Scenario: Pins

- **WHEN** status indication is shown
- **THEN** host uses LED1, LED2, LED4, LED5, and LED6; slave uses LED1–LED6

#### Scenario: Drive by color

- **WHEN** a status LED is logically on
- **THEN** a red LED is digital HIGH and a blue or green LED is PWM at that color’s brightness

### Requirement: Fixed LED colors per board role

Each board SHALL treat LED1–LED6 as fixed physical colors by role. Host (master): LED1 blue, LED2 green, LED3 green, LED4 green, LED5 green, LED6 red. Slave: LED1 red, LED2 green, LED3 green, LED4 blue, LED5 green, LED6 blue. Status meanings for each LED index SHALL stay as today; only the drive method and brightness by color change.

#### Scenario: Host color map

- **WHEN** the host drives LED1–LED6 for status
- **THEN** LED1 is blue, LED2–LED5 are green, and LED6 is red

#### Scenario: Slave color map

- **WHEN** the slave drives LED1–LED6 for status
- **THEN** LED1 is red, LED2 green, LED3 green, LED4 blue, LED5 green, and LED6 blue

### Requirement: Blue and green LEDs use PWM brightness

When a blue or green LED is logically on, the firmware SHALL drive it with PWM at the configured brightness for that color (1–100 percent). When that LED is logically off, PWM duty SHALL be zero. Red LEDs SHALL keep digital on/off (full on or full off) and SHALL NOT use the blue/green brightness settings.

#### Scenario: Green LED on at half brightness

- **WHEN** a green status LED is logically on and green brightness is 50
- **THEN** that LED is driven with about half PWM duty, not digital full HIGH

#### Scenario: Red LED ignores brightness

- **WHEN** a red status LED is logically on and blue or green brightness is below 100
- **THEN** that red LED is fully on digitally and does not dim with those settings

#### Scenario: LED off

- **WHEN** a blue or green status LED is logically off
- **THEN** that LED stays dark (zero PWM duty)

### Requirement: Brightness applies on host and slave

Configured blue and green brightness SHALL apply to every blue or green LED on the host and on the slave. Until the slave has received brightness from the host, the slave SHALL use default brightness 100 for both colors.

#### Scenario: Host save updates host LEDs

- **WHEN** the operator saves new blue or green brightness on the host
- **THEN** host blue/green LEDs that are on immediately use the new duty

#### Scenario: Slave uses host brightness after push

- **WHEN** the host has pushed blue and green brightness to a ready slave
- **THEN** slave blue/green LEDs that are on use those same percentages

### Requirement: Host status and boot GPIOs on ESP32-S3
On the ESP32-S3 host, the firmware SHALL drive LED1–LED6 and sample the boot button on the host pin map from design (LED1–4: GPIO **4–7**, LED5: **15**, LED6: **16**, boot button: **41**). The firmware SHALL NOT require the former C6 role strap for LED or button behavior.

#### Scenario: Host LED outputs
- **WHEN** the host needs to show status on LED1–LED6
- **THEN** it toggles the ESP32-S3 GPIOs assigned to those LEDs

#### Scenario: Boot button
- **WHEN** the operator holds the boot button according to existing AP/recovery rules
- **THEN** the host samples GPIO **41** for that button

### Requirement: Onboard RGB is unused

Each chip SHALL NOT drive the onboard WS2812 for status. Status SHALL use only LED1–LED6.

#### Scenario: RGB stays off

- **WHEN** the chip is booting, ready, pairing, connected to MQTT, or in a critical error
- **THEN** the onboard RGB LED is not used for that indication

### Requirement: RGB red while that chip is booting or in a critical error

Each chip SHALL show boot indication as soon as firmware starts, before Serial, Wi-Fi, SPI, or Zigbee init, and SHALL keep that indication until that chip is ready. The **host** and the **slave** SHALL blink **LED5** with period 0.25 seconds (125 ms on, 125 ms off) for **boot**. The **host** SHALL NOT use LED5 for any error: while a critical error is present the host SHALL hold **LED6** solid on (outranking host firmware-update blink). After boot is cleared, host LED5 MAY stay on for MQTT connected and host LED4 MAY stay on for the local broker while critical is held. The **slave** SHALL blink **LED5** with period 0.25 seconds for critical error. LED1–LED4 SHALL NOT be held on for boot. MQTT connection SHALL NOT be required to clear host boot indication. Critical-error indication SHALL outrank pairing blink and activity pulses.

#### Scenario: Host still preparing

- **WHEN** the host has started but Wi-Fi has no address yet or the slave is not yet in normal work
- **THEN** host LED5 blinks with period 0.25 seconds and host LED1–LED4 stay off except later activity pulses after boot

#### Scenario: Host preparation finished

- **WHEN** the host has a usable Wi-Fi address and the slave link is in normal work
- **THEN** host LED5 is no longer blinking for boot

#### Scenario: Host lost the slave after boot

- **WHEN** the host had finished boot and the slave SPI link is no longer healthy (not in normal work, or ping replies have timed out)
- **THEN** host LED6 is solid on and host LED5 does not blink for that error

#### Scenario: Host slave link restored

- **WHEN** the host was holding critical LED6 for a lost slave and the slave link is healthy again
- **THEN** host LED6 is no longer held on for that error

#### Scenario: Slave still preparing

- **WHEN** the slave has started but has not yet applied host settings or started the coordinator
- **THEN** slave LED5 blinks with period 0.25 seconds and LED1–LED4 and LED6 stay off except later activity pulses after boot

#### Scenario: Slave preparation finished

- **WHEN** the slave has applied host settings and the Zigbee coordinator is running, and no critical error is present
- **THEN** slave LED5 is no longer blinking for boot and stays on while no critical error is present

#### Scenario: Critical error appears

- **WHEN** a chip enters a critical error
- **THEN** the host holds LED6 solid (LED5 does not blink for that error, and may stay on for MQTT after boot) and the slave blinks LED5 with period 0.25 seconds until that error is cleared

#### Scenario: Critical while MQTT connected

- **WHEN** the host has finished boot, MQTT is connected, and a critical error is present
- **THEN** host LED6 is solid on and host LED5 stays on for MQTT connected

### Requirement: Host LED6 blinks during firmware update cycle

While the host firmware update cycle is active (HTTP receive, slave SPI OTA, host apply, or reboot-armed after a successful update) and no critical error is present, the host SHALL blink **LED6** at 10 Hz (50 ms on, 50 ms off). When a critical error is present, LED6 SHALL stay solid for critical and SHALL NOT show the update blink. Host LED5 SHALL NOT indicate update progress.

#### Scenario: Update cycle running

- **WHEN** the host is applying a firmware update cycle and no critical error is present
- **THEN** host LED6 blinks at 10 Hz

#### Scenario: Update cycle with critical

- **WHEN** a critical error is present during a firmware update cycle
- **THEN** host LED6 stays solid for critical and does not blink for update

#### Scenario: Update cycle finished

- **WHEN** the firmware update cycle is no longer active and no critical error is present
- **THEN** host LED6 is off

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

After host boot indication is cleared, host LED4 SHALL stay **on** while the onboard MQTT broker is listening, including while a critical error is indicated on LED6. Host LED3 SHALL stay off. The onboard RGB SHALL NOT be used for the local broker.

#### Scenario: Local broker listening

- **WHEN** the host is ready and the local MQTT broker is listening
- **THEN** host LED4 is on and the onboard RGB is not used

#### Scenario: Local broker listening during critical

- **WHEN** the host is ready, the local MQTT broker is listening, and a critical error is present
- **THEN** host LED4 stays on and host LED6 is solid on

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
