# Online Table Migration and Catch Up Swap Runtime Model

## Purpose

Define the current online table migration runtime used for tablespace relocation and similar in-engine movement.

## Migration Phases

Current code-backed online migration phases are:

- `MIGRATION_NONE`
- `MIGRATION_INIT`
- `MIGRATION_COPYING`
- `MIGRATION_CATCH_UP`
- `MIGRATION_READY_FOR_SWAP`
- `MIGRATION_SWAP`
- `MIGRATION_CLEANUP`
- `MIGRATION_COMPLETE`
- `MIGRATION_FAILED`
- `MIGRATION_ABORTED`

## In-Memory State

The in-memory migration state carries:

- migration identity
- table identity
- source and target tablespace
- migration phase
- migration transaction identifier
- page counts
- time bounds
- dirty-page bitmap
- catch-up iteration count
- final dirty-page count
- total bytes copied

## Runtime Model

The runtime model is:

1. initialize migration
2. copy source pages in the background
3. track dirty-page churn while copy proceeds
4. execute catch-up iterations until convergence
5. reach ready-for-swap only after the table converges sufficiently
6. perform atomic swap
7. clean up source state
8. persist history and terminal phase

## Persistence

Completed or terminal migration history is persisted for audit and diagnostics. Runtime state and persisted history are distinct structures.

## Index and TID Follow-Up

Index families that store heap references shall participate in migration follow-up through TID or GPID remapping logic after page movement. Online migration is not complete if secondary structures continue to reference old heap locations.

## Non-Replicating Source Boundary

This online migration model is local-engine movement. It is separate from foreign-engine ingestion, remote passthrough, or replication-channel synchronization. For remote systems without natural replication, connector snapshot and staged copy paths are the canonical migration fallback.
