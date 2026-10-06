## ADDED Requirements

### Requirement: Joined firmware package format
The product SHALL define a single **joined firmware package** file that contains both the slave application image and the host application image. The package SHALL be a **ZIP** archive whose members are named exactly `slave.bin` and `host.bin`. Member order in the archive SHALL NOT matter. The package MAY use ZIP DEFLATE or ZIP STORE; tools and the host MUST accept both.

#### Scenario: Valid joined package
- **WHEN** a ZIP contains non-empty `slave.bin` and `host.bin` members
- **THEN** the package is accepted as a joined firmware package for Update

#### Scenario: Missing member rejected
- **WHEN** a ZIP is missing `slave.bin` or `host.bin`
- **THEN** the Update path rejects the file and does not program either chip

### Requirement: Join and split tools
The repository SHALL provide scripts (or documented commands) to **join** a host `.bin` and a slave `.bin` into the joined ZIP and to **split** a joined ZIP back into `host.bin` and `slave.bin`. Those tools SHALL be usable offline without the device.

#### Scenario: Join
- **WHEN** a developer runs the join tool with host and slave application binaries
- **THEN** the tool writes a joined ZIP that the web Update tab can accept

#### Scenario: Split
- **WHEN** a developer runs the split tool on a joined ZIP
- **THEN** the tool writes separate `host.bin` and `slave.bin` files matching the archive members

### Requirement: Web Update uses one joined file
The System → Update tab SHALL continue to accept a **single** file upload. That file SHALL be the joined firmware package. After a valid receive, the host MUST extract `slave.bin`, program the slave over SPI first, and only after slave success extract and program `host.bin`, then restart. Failure before host apply MUST leave the running host application unchanged.

#### Scenario: Successful joined update
- **WHEN** the operator uploads a valid joined ZIP and slave then host commits succeed
- **THEN** both chips run the new images after the host restart completes

#### Scenario: Slave fails inside joined update
- **WHEN** the joined ZIP is accepted but slave programming fails
- **THEN** the host does not apply `host.bin` and reports failure
