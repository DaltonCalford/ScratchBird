# Section 09 Specification Outline

Status: current_authority

## Section scope

Section 09 covers the implemented lock-manager core that is actually present in ScratchBird:

1. lock-manager ownership, lifecycle, lock modes, and lock targets
2. deadlock detection, wait history, statistics, and metadata lock observability
3. interaction between lock conflicts and MGA-aware transaction behavior
4. isolation-level and phantom-protection claims that can be proven from code today
5. test and gate expectations for locking, deadlock, timeout, and restart behavior

## Canonical files

1. LOCK_MANAGER_NORMATIVE_IMPLEMENTATION.md
2. ISOLATION_LEVEL_AND_PHANTOM_PROTECTION_MATRIX.md
3. MGA_CONFLICT_AND_LOCKING_POLICY.md
4. DEPENDENCIES.md
5. TEST_CONTRACT.md
6. DECISION_RECORD.md
7. README.md

## Current implementation-backed themes

- PostgreSQL-like lock modes are the current engine truth.
- DATABASE, TABLE, PAGE, and TUPLE are the proven lock targets in this audit wave.
- Tuple update locking integrates with READ_COMMITTED_READ_CONSISTENCY statement-restart handling.
- Deadlock detection, wait history, and metadata lock observability are real and test-backed.
- Predicate or range locking and certified phantom prevention remain unsupported.
