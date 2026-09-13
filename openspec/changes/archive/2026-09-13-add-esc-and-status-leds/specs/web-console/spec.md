## ADDED Requirements

### Requirement: Escape dismisses overlay dialogs

The console SHALL treat Escape as dismiss for every overlay dialog (search, device parameters, restore-skipped list, and delete confirm). Escape SHALL run the same dismiss path as that dialog’s Cancel or Close control: it MUST NOT save, MUST NOT confirm delete, and dismissing the search dialog MUST stop pairing search. When more than one overlay is visible, Escape SHALL dismiss only the front-most overlay. Escape SHALL do nothing when no overlay is visible.

#### Scenario: Escape closes search

- **WHEN** the search dialog is visible and the operator presses Escape
- **THEN** the search dialog closes, pairing search stops, and no new registered device is written

#### Scenario: Escape cancels delete

- **WHEN** the delete-confirm dialog is visible and the operator presses Escape
- **THEN** the confirm dialog closes and the selected device remains registered

#### Scenario: Escape ignored with no overlay

- **WHEN** no overlay dialog is visible and the operator presses Escape
- **THEN** the page does not navigate away and no dialog opens or closes
