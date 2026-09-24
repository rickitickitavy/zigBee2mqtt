## ADDED Requirements

### Requirement: At least one unlocked admin remains
After every successful users-table write the table MUST still contain at least one user who has `isAdmin` and is not blocked. The host MUST reject create, update, delete, and settings-restore replacement when the resulting table would have zero unlocked `isAdmin` users. Unlocking or adding another `isAdmin` MUST still be allowed. Counting MUST treat a blocked `isAdmin` as not satisfying this rule.

#### Scenario: Block last unlocked admin
- **WHEN** the table has one `isAdmin` who is not blocked and an operator sets that user to blocked
- **THEN** the host rejects the write and the user stays unblocked

#### Scenario: Delete last unlocked admin
- **WHEN** the table has one unlocked `isAdmin` and that user is deleted
- **THEN** the host rejects the delete

#### Scenario: Demote last unlocked admin
- **WHEN** the table has one unlocked `isAdmin` and that user’s `isAdmin` is cleared
- **THEN** the host rejects the write

#### Scenario: Restore without unlocked admin
- **WHEN** settings restore supplies a users list with only blocked admins or no admins
- **THEN** the host does not replace the users table

#### Scenario: Second admin can be blocked
- **WHEN** the table has two unlocked `isAdmin` users and one of them is blocked
- **THEN** the host accepts the write and the other `isAdmin` stays unlocked
