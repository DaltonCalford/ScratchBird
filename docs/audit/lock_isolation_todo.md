# TODO: Locking & Isolation (MGA + Optional Locks)

Goals:
- Preserve MGA defaults: readers don’t block writers; writers don’t block readers; snapshot visibility by default.
- Provide explicit lock hints (e.g., “WITH LOCK”) to request row/table locks when stronger precedence is needed (override last-write-wins).
- Make lock manager behavior, deadlock detection, and isolation semantics explicit.

Requirements:
- Default snapshot (MGA/TIP): no blocking between readers/writers; visibility per transaction rules.
- Optional locks: allow DML to request row/table locks; when requested, conflicting transactions are blocked or forced to wait/abort per policy.
- Lock modes and granularity: table vs row; define conflict matrix and escalation policy (if any).
- Deadlock detection: periodic waits-for detection and victim selection when locks are used.
- Error/timeout behavior: configurable lock timeouts; clear error reporting.
- Catalog visibility: inspect held locks for diagnostics (privileged).
- Emulated engines: only expose locking modes consistent with their native semantics; ScratchBird-only locks stay ScratchBird-scoped.

Work Items:
- Define lock modes, conflict matrix, and “WITH LOCK” syntax/semantics; add to parser/AST/executor.
- Implement row/table lock acquisition in executor when explicitly requested; keep MGA default otherwise.
- Add deadlock detection/timeout handling for locked operations.
- Expose lock inspection view for admins.
- Tests: conflicting locks, timeouts, deadlock detection, default MGA non-blocking behavior, emulation constraints.
