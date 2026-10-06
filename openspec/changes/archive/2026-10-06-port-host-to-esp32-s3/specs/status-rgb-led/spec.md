## MODIFIED Requirements

### Requirement: Host status and boot GPIOs on ESP32-S3
On the ESP32-S3 host, the firmware SHALL drive LED1–LED6 and sample the boot button on the host pin map from design (LED1–4: GPIO **4–7**, LED5: **15**, LED6: **16**, boot button: **41**). The firmware SHALL NOT require the former C6 role strap for LED or button behavior.

#### Scenario: Host LED outputs
- **WHEN** the host needs to show status on LED1–LED6
- **THEN** it toggles the ESP32-S3 GPIOs assigned to those LEDs

#### Scenario: Boot button
- **WHEN** the operator holds the boot button according to existing AP/recovery rules
- **THEN** the host samples GPIO **41** for that button
