# Spec Delta

## ADDED Requirements

### Requirement: Update tab can pause host slave resets for 120 seconds

System → Update SHALL show an admin-only **Wait for update slave** control for a USB slave-programming window. It SHALL start a 120 second host slave-reset pause, show live remaining time on the control, cancel on a second press, and load real remaining time when the Update tab becomes active.

#### Scenario: Start pause

- **WHEN** an admin opens System → Update and activates Wait for update slave while no pause is active
- **THEN** the host starts a 120 second slave-reset pause (so the operator can flash the slave over USB without host EN pulses) and the button shows a countdown that decreases toward zero

#### Scenario: Show remaining on tab enter

- **WHEN** a pause is already active and the admin opens or re-selects the Update tab
- **THEN** the Wait for update slave control shows the remaining seconds until the pause ends (matching host state)

#### Scenario: Cancel pause

- **WHEN** a pause is active and the admin activates Wait for update slave again
- **THEN** the host ends the pause immediately and the control returns to the inactive label without a countdown

#### Scenario: Pause expires

- **WHEN** a pause reaches zero remaining time without cancel
- **THEN** the host ends the pause and the control returns to the inactive label
