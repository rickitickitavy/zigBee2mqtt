# Spec Delta

## ADDED Requirements

### Requirement: Pointer-down outside an overlay dismisses it

The console SHALL dismiss an overlay dialog only when the pointer-down target is that overlay (the dimmed area outside the dialog panel). Pointer-up on the overlay MUST NOT dismiss when the pointer-down started on the dialog panel (including a text selection that ends outside the panel). Pointer-down on the panel MUST NOT dismiss. Dismiss SHALL use the same path as that overlay’s Cancel or Close (no save, no delete confirm; search dismiss still stops pairing). When more than one overlay is visible, only the front-most SHALL dismiss.

#### Scenario: Drag-select then pointer-up on overlay

- **WHEN** a dialog is open, the operator pointer-downs on text inside the panel, drags, and pointer-ups on the dimmed overlay
- **THEN** the dialog stays open and the selection is not treated as a dismiss

#### Scenario: Pointer-down on overlay dismisses

- **WHEN** a dialog is open and the operator pointer-downs on the dimmed overlay outside the panel
- **THEN** the dialog dismisses the same way as Cancel or Close for that overlay

### Requirement: Readonly fields look distinct from editable fields

Readonly and disabled form controls in console field rows SHALL use a slightly grayed background and muted text relative to editable controls of the same type so the operator can tell they cannot edit those values.

#### Scenario: IEEE readonly on Zigbee edit

- **WHEN** the operator opens the parameter dialog for a registered Zigbee device
- **THEN** the IEEE field is not editable and its background is visibly grayer than the NAME field

### Requirement: Session idle follows operator activity

While the operator is signed in, the console SHALL treat pointer, keyboard, and form input on the signed-in UI as operator activity and SHALL reset the session idle timer on that activity. Automatic requests (including session-alive polls and status or log polls) MUST NOT reset the idle timer. After the idle window with no operator activity, the console MUST return to the login form.

#### Scenario: Working in the console keeps the session

- **WHEN** an `isAdmin` operator stays on the console and uses it (clicks or types) throughout a period longer than 10 minutes since login
- **THEN** the session stays valid and the login form does not appear

#### Scenario: Idle without using the console logs out

- **WHEN** an `isAdmin` operator leaves the signed-in console unused for 10 minutes
- **THEN** the next check shows the login form and protected APIs reject the old session
