# MGA Conflict And Locking Policy

Status: current_authority

## 1. Scope

This file defines the normative lock and conflict model for ScratchBird Alpha under Firebird-style MGA.

The authoritative rule is simple:

- ordinary visibility is MGA-based and does not require read locks.
- ordinary blocking is write-write only at the row or tuple lineage level.

Any implementation machinery borrowed from other engines is non-authoritative unless it preserves these rules exactly.

## 2. Firebird-Style MGA Authority

Firebird-style MGA semantics are authoritative for all lock behavior in ScratchBird Alpha.

That means:

1. readers determine visibility from transaction state, version lineage, and snapshot rules.
2. readers do not lock rows merely to read them.
3. read-read does not block.
4. read-write does not block for visibility.
5. write-write on the same tuple lineage conflicts.

If any lower-level locking machinery contradicts these rules, the machinery is wrong and must be treated as an implementation defect rather than a competing source of truth.

## 3. Tuple Conflict Rule

ScratchBird currently enforces ordinary row-write conflicts using tuple locks.

Normative behavior:

1. update and delete acquire a row-exclusive tuple lock through StorageEngine acquireTupleLock.
2. tuple lock identity is table UUID plus page id plus item id.
3. tuple-target conflict evaluation is unconditional for overlapping tuple locks.
4. because current tuple acquisition is write-side only, ordinary tuple lock waiting is effectively write-write only.

This is the current implementation expression of the user-facing MGA rule.

## 4. Reader Rule

Ordinary readers must not acquire tuple locks to enforce visibility.

Readers instead use:

1. TIP and CLOG transaction state.
2. tuple version stamps.
3. version-chain traversal.
4. snapshot or statement restart rules where applicable.

If future code introduces reader tuple locking, that is a behavioral change and must be rejected unless the canonical MGA specifications are deliberately changed.

## 5. Read-Consistency Restart Rule

Under read committed read consistency, a write conflict may be converted into statement restart instead of indefinite waiting.

That conversion is still MGA behavior. It does not turn ordinary reads into lock-based visibility.

## 5.1 Worked MGA lock examples

### Read-read

1. `T1` reads row version `V1`
2. `T2` reads the same logical row
3. both transactions decide visibility from transaction state and version lineage
4. no ordinary row lock wait is required

### Read-write

1. `T1` reads committed row version `V1`
2. `T2` updates the row by creating `V2`
3. while `T2` is active, `T1` still reads according to its own MGA view
4. `T1` does not need to acquire a tuple lock merely to continue reading

### Write-write

1. `T1` attempts to update tuple lineage `R`
2. `T2` attempts to update the same tuple lineage `R`
3. tuple conflict logic detects the overlap
4. one writer waits, fails no-wait, or restarts according to policy

Only the third example is ordinary blocking MGA row conflict.

## 6. Structural Coordination Boundary

ScratchBird may use physical coordination locks for internal structure protection, but only under these conditions:

1. they must not redefine ordinary MGA visibility.
2. they must not make readers participate in row-visibility locking.
3. they must not turn the engine into a PostgreSQL-style lock-first concurrency model.
4. they must remain bounded implementation details for physical structure management.

## 7. PostgreSQL-Derived Machinery Boundary

Names, enums, or helper matrices that resemble PostgreSQL locking may appear in implementation code, but they are not authoritative behavior by themselves.

They are allowed only if they do not compromise Firebird-style MGA semantics.

This means:

1. PostgreSQL-derived mode names do not grant PostgreSQL semantics.
2. PostgreSQL-style table or page lock vocabulary cannot override MGA row-version rules.
3. any borrowed optimization or lock helper must be evaluated against Firebird-style MGA first.
4. if a borrowed rule causes reader blocking or visibility behavior inconsistent with MGA, it is non-conforming.

## 8. B-Tree Structural Lock Rule

Current code uses page-level lock coupling inside B-tree traversal.

This is only acceptable as a structural coordination device.

Normative limits:

1. it must not be treated as the ordinary row-conflict rule.
2. it must not justify reader participation in row visibility locking.
3. it must be minimized so that Firebird-style MGA semantics remain dominant.
4. any structural page-lock behavior that materially compromises MGA reader behavior must be treated as a defect to be corrected.

## 9. Explicit Table Reservation Rule

Explicit table reservations are administrative controls, not the default MGA conflict model.

They may exist as opt-in operational features, but they must not be described as the normal concurrency mechanism for DDL or DML visibility.

## 10. Prepared Transaction Lock Persistence

When a transaction enters prepared state, granted locks are snapshotted into catalog evidence before prepared transaction publication becomes durable.

This exists so restart can reconstruct prepared ownership without inventing lock state.

## 10.1 Audit lookup anchors

- `src/core/lock_manager.cpp` search `conflict_matrix_` for the current
  lock-conflict lookup anchor.
- `src/core/storage_engine.cpp` search `StorageEngine::acquireTupleLock` for
  tuple-lineage write-conflict entry.

## 11. Anti-Drift Rule

The following statements are incorrect for ScratchBird Alpha:

1. readers must take ordinary row locks to observe consistent committed data.
2. table or page locks are the main visibility mechanism.
3. PostgreSQL lock semantics are authoritative because the implementation uses similar names.
4. Firebird-style MGA can be compromised in exchange for implementation convenience.

## 12. Certification Requirements

Implementation is conforming only if tests prove:

1. ordinary reads do not require tuple locks.
2. write-write attempts on the same tuple conflict.
3. read-write visibility does not block merely because a newer version exists.
4. any structural page locks remain bounded physical coordination only.
5. borrowed PostgreSQL-style machinery does not compromise Firebird-style MGA semantics.
