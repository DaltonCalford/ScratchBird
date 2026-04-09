# Failure Model and Recovery Classification

## Purpose

This file defines the canonical failure classes and startup-recovery classifications for the transaction core. The model is MGA-based. Recovery is state reconciliation and bounded repair. Recovery is not WAL replay.

## Inputs to Classification

The startup classifier shall consider the checkpoint/control state loaded by `Database`.

The startup classifier shall consider transaction-map header validity, transaction-map entry validity, and transaction-map normalization debt owned by `TransactionManager`.

The startup classifier shall consider queue rebuild debt, page-scan findings, and prepared-state inventory findings.

The startup classifier shall consider catalog-backed evidence required for committed schema epochs and prepared transactions.

## Canonical Recovery Classes

### `CLEAN_START`

The persisted checkpoint state, transaction map, and required catalog-backed evidence are coherent. No repair is required.

### `CHECKPOINT_REBUILD_REQUIRED`

The durable checkpoint/control state is incomplete or stale enough that checkpoint-derived runtime state must be rebuilt before the database may open for normal service, but the persisted transaction truth remains within supported repair bounds.

### `TIP_NORMALIZATION_REQUIRED`

The transaction-map header or entries contain a supported contradiction that can be normalized without inventing new transaction truth. Normalization is required before normal service.

### `REPAIRED_RECOVERY`

One or more supported repair steps were applied at startup. The database may open after the repair completes and the resulting state passes the remaining validation rules.

### `WRITE_FENCED_RECOVERY`

The database may reopen in a bounded state, but writes shall remain fenced until operator action or further system work clears the unsafe condition.

### `CORRUPTION_STOP`

The database shall not open for write service because the startup evidence requires unsupported recovery semantics or shows fail-closed corruption.

## Canonical Corruption and Repair Rules

A missing durable commit sequence for a transaction that otherwise appears committed on reopen is `TIP_NORMALIZATION_REQUIRED` only if the current repair rules can derive a unique supported normalization.

A duplicate durable commit sequence on reopen is `TIP_NORMALIZATION_REQUIRED` only if the current repair rules can normalize the reopen drift without inventing new committed visibility.

A prepared transaction without the required durable prepared-state evidence is `CORRUPTION_STOP`.

A committed schema epoch referenced by replay binding but missing from the required catalog-backed evidence is `CORRUPTION_STOP`.

A contradiction between transaction-map header inventory and transaction-map entry inventory that can be reconciled by supported normalization is `TIP_NORMALIZATION_REQUIRED`.

A contradiction that cannot be reconciled by supported normalization is `CORRUPTION_STOP`.

## Normative Startup Classification Algorithm

1. Load control and checkpoint state.
2. Validate fixed bootstrap page placement and required transaction-map roots.
3. Validate transaction-map header structure.
4. Validate transaction-map entry structure and state encodings.
5. Validate prepared-state cross references.
6. Validate queue and runtime rebuild debt required to reopen.
7. Validate schema-epoch and forensic-linkage evidence required by transaction replay bindings.
8. Apply only supported normalizations.
9. Emit exactly one canonical recovery class.
10. Open in the service state allowed by that class.

## Service State Mapping

`CLEAN_START` opens normal read-write service.

`CHECKPOINT_REBUILD_REQUIRED` opens only after the required rebuild completes successfully.

`TIP_NORMALIZATION_REQUIRED` opens only after supported normalization completes successfully.

`REPAIRED_RECOVERY` opens normal read-write service after repair completion and post-repair validation.

`WRITE_FENCED_RECOVERY` may open a bounded service state, but write admission remains fenced.

`CORRUPTION_STOP` does not open normal service.

## Incident Vocabulary Requirements

Every startup refusal shall map to one canonical recovery class.

Every fenced startup shall expose a stable refusal reason and the owning subsystem.

Every repaired startup shall record that repair occurred and which repair class was applied.

## Explicit Non-Goals

This file does not define WAL replay phases.

This file does not define archive-log recovery.

This file does not define PITR, donor-log recovery, or log-ship catch-up.
