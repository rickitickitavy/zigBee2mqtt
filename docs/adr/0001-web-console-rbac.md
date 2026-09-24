# 0001. Web console RBAC

## Context

The console is a security layer. New tabs and APIs must not ship open to every signed-in user.

## Decision

Every new console surface or protected HTTP API MUST name an existing role or add a role before implementation.

Existing roles: simple user (no extra flags), `editDevices`, `addDevices`, `removeDevices`, `editUsers`, `isAdmin`.

When proposing a feature, ask which role may use it, or whether a new role is needed. Do not assume admin-only or everyone.

Enforce the same rule in the UI (hide) and on the host (401/403). `isAdmin` has every right. Only `isAdmin` may grant `isAdmin`.

## Consequences

Propose and apply update this ADR when a role is added. Specs list the chosen role. At least one unlocked `isAdmin` must remain after every users-table write; see `0010`.
