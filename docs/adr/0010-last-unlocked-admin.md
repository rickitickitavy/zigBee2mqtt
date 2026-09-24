# 0010. Last unlocked admin

## Context

Delete and demote already refused removing the last `isAdmin`. Blocking that user, or restoring a users list with only blocked admins, still left the console with nobody who could sign in as admin.

## Decision

After every successful users-table write (create, update, delete, settings restore), at least one user MUST remain who has `isAdmin` and is not blocked. The host rejects the write when the result would have zero unlocked admins. A blocked `isAdmin` does not satisfy this rule.

See `0001` for role meaning. This ADR is the lockout floor, not a new role.

## Consequences

Do not treat `adminCount` (including blocked) as enough. Restore must fail closed and leave the current table unchanged. The Security dialog must show the host error.
