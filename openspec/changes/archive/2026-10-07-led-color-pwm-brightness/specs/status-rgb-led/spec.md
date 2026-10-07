# Spec Delta

## ADDED Requirements

### Requirement: Fixed LED colors per board role

Each board SHALL treat LED1–LED6 as fixed physical colors by role. Host (master): LED1 blue, LED2 green, LED3 green, LED4 green, LED5 green, LED6 red. Slave: LED1 red, LED2 blue, LED3 green, LED4 blue, LED5 green, LED6 blue. Status meanings for each LED index SHALL stay as today; only the drive method and brightness by color change.

#### Scenario: Host color map

- **WHEN** the host drives LED1–LED6 for status
- **THEN** LED1 is blue, LED2–LED5 are green, and LED6 is red

#### Scenario: Slave color map

- **WHEN** the slave drives LED1–LED6 for status
- **THEN** LED1 is red, LED2 blue, LED3 green, LED4 blue, LED5 green, and LED6 blue

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

## MODIFIED Requirements

### Requirement: RGB and LED1–LED4 work together

Each chip SHALL drive LED1–LED6 on that board’s pin map (host ESP32-S3 and slave ESP32-C6 as in the Host status requirement / pins). Host LED3 SHALL stay off. Host LED4 SHALL stay on while the local MQTT broker is listening (after boot). Host LED5 SHALL mean MQTT connected (solid) or boot (blink) and SHALL NOT mean any error. Host LED6 SHALL mean critical error (solid) or firmware update cycle (10 Hz blink). Slave LED5 SHALL mean ready with no fault (solid) or boot/critical (blink). Slave LED6 SHALL mean pairing blink. Red LEDs SHALL light with digital HIGH when on. Blue and green LEDs SHALL light with PWM when on. The onboard WS2812 SHALL NOT be used.

#### Scenario: Pins

- **WHEN** status indication is shown
- **THEN** host uses LED1, LED2, LED4, LED5, and LED6; slave uses LED1–LED6

#### Scenario: Drive by color

- **WHEN** a status LED is logically on
- **THEN** a red LED is digital HIGH and a blue or green LED is PWM at that color’s brightness
