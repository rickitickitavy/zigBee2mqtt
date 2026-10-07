# Spec Delta

## ADDED Requirements

### Requirement: Host pushes LED brightness to the slave

The host SHALL deliver configured blue and green LED brightness (1–100) to the slave after the slave is ready and whenever those values change on the host. The slave SHALL apply received brightness to its blue and green LEDs and SHALL NOT persist them. Delivering brightness alone SHALL NOT restart Zigbee or re-run the radio settings bring-up path.

#### Scenario: Brightness after ready

- **WHEN** the slave has accepted radio settings and is in normal work
- **THEN** the host also delivers current blue and green brightness so slave PWM LEDs match the host Hardware settings

#### Scenario: Brightness change without radio restart

- **WHEN** the operator saves new LED brightness while the slave Zigbee radio is already running
- **THEN** the slave updates LED PWM brightness and does not restart Zigbee solely because of that brightness push

#### Scenario: Slave reboot waits for host

- **WHEN** the slave reboots and loses RAM brightness
- **THEN** it uses default 100 until the host delivers brightness again
