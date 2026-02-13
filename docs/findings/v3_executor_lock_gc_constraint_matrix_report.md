# Executor Lock/GC/Constraint Enforcement Matrix - V3 Findings

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`

Status: **Partially verified**. The spec is authoritative but the code does not show explicit, centralized enforcement of the required lock ordering, constraint ordering, or SBX-* error codes. This requires deeper executor-level validation.

## Key Gaps / Risks
- **SBX-* error codes not found** in codebase; constraint failures appear to map to generic `Status::CONSTRAINT_VIOLATION` and SQLSTATE mapping, not the specific SBX codes required by the spec.
  - Evidence: no matches for `SBX-` in `src/`.
- **Deterministic global lock ordering** and lock escalation rules are not visibly enforced in executor flow (no clear centralized ordering logic in `src/sblr/executor.cpp`).
- **Constraint enforcement order** (domain -> not null -> column check -> table check -> FK -> unique/PK) is not visibly orchestrated in a single place; likely distributed across catalog/constraint code, but not verified.

## Checklist (Not Fully Verified)

### Lock Ordering / Deadlocks
[ ] Global lock ordering (database -> tablespace -> schema -> table -> index -> row -> LOB) not verified.
[ ] Deadlock handling with `SBX-LOCK-DEADLOCK` and SQLSTATE `40P01` not verified.
[ ] NOWAIT handling with `SBX-LOCK-NOTAVAILABLE` not verified.

### Constraint Ordering
[ ] Domain validation order not verified.
[ ] NOT NULL / CHECK / FK / UNIQUE ordering not verified.
[ ] DEFERRABLE queue and commit-time validation not verified (executor has `SET CONSTRAINTS` handling, but full queue semantics not audited).
  - Evidence: `src/sblr/executor.cpp:57563-57628`.

### GC / Visibility
[ ] MGA visibility/GC rules not verified.

### Per-Opcode Matrix
[ ] SELECT lock modes / FOR UPDATE/SHARE behavior not verified.
[ ] INSERT/UPDATE/DELETE lock and constraint rules not verified.
[ ] MERGE behavior not verified.
[ ] COPY lock/constraint behavior not verified.
[ ] DDL lock rules not verified.

## Notes
- There are generic constraint violations and SQLSTATE mappings in core code, but no explicit SBX-* codes were located. This suggests a mismatch with the spec’s required error map.

