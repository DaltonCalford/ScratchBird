# Online Tablespace Migration and Proxy Cutover Model

## Scope

This file defines the current code-backed online migration algorithm used for local tablespace migration, and the required reconstructed cutover discipline for donor-engine proxy or passthrough migration.

This file is authoritative for:

- online tablespace migration state
- copy, catch-up, swap, cancel, and history rules
- index TID update requirements
- migration history persistence
- the canonical cutover shape for donor-engine migration where natural replication is absent

## Current code-backed online migration substrate

### Start conditions

The current code-backed `startOnlineMigration` path requires:

- target table exists in table cache
- no migration already in progress
- target tablespace differs from current tablespace

It then derives:

- `migration_xid` from the current transaction manager when present
- fallback `migration_xid = 1` if no active transaction is available
- full page inventory for the table

### Persisted or cached state

The current migration state tracks:

- `migration_id`
- `table_id`
- `source_tablespace`
- `target_tablespace`
- `phase`
- `migration_xid`
- `total_pages`
- `pages_copied`
- `start_time`
- `end_time`
- dirty-page bitmap
- catch-up iteration count
- copied-byte counters

The corresponding table row cache carries:

- `migration_in_progress`
- `migration_id`
- `migration_xid`
- `migration_target_ts`
- `migration_phase`

Canonical rule:

- online migration is transaction-bound and stateful
- it is not an ad hoc background copy with no migration identity

## Phase model

The current code proves the following explicit migration phases:

- `MIGRATION_INIT`
- `MIGRATION_COPYING`
- `MIGRATION_CATCH_UP`
- `MIGRATION_READY_FOR_SWAP`
- `MIGRATION_SWAP`
- `MIGRATION_CLEANUP`
- `MIGRATION_COMPLETE`
- `MIGRATION_FAILED`
- `MIGRATION_ABORTED`

Canonical rule:

- every migration must move through explicit phase state
- cancellation and cleanup rules are phase-dependent

## Dirty-page tracking

### Current code-backed behavior

The current migration runtime already maintains a dirty-page bitmap:

- pages can be marked dirty by page number
- bitmap can be queried for dirty-page ids
- bitmap can be cleared
- dirty-page count can be computed

Canonical rule:

- catch-up is based on explicit dirty tracking, not blind recopy of the full table after the initial copy

## Copying phase

### Current algorithm

The current copying phase algorithm is:

1. verify migration is in `MIGRATION_COPYING`
2. enumerate all source heap pages
3. allocate target page in target tablespace per source page
4. copy page with TID remapping
5. maintain page mapping
6. record source-to-target TID mappings in `TIDResolver`
7. track page and byte progress

Canonical rule:

- copy phase is page-granular and TID-aware
- migration does not merely move a root pointer without rewriting dependent tuple references

## Catch-up phase

### Current algorithm

The current catch-up algorithm is:

1. verify migration is in `MIGRATION_CATCH_UP`
2. gather dirty pages
3. stop if dirty count is below threshold
4. otherwise, re-copy dirty pages to newly allocated target pages
5. update TID mappings in `TIDResolver`
6. clear dirty bitmap
7. repeat up to max iterations

Canonical rule:

- catch-up is iterative and threshold-driven
- threshold completion is explicit
- incomplete catch-up can still proceed with a logged high-dirty boundary, but the state is not hidden

## Swap phase

### Current algorithm

The current swap algorithm is:

1. verify migration is in `MIGRATION_SWAP`
2. retrieve all TID mappings from `TIDResolver`
3. update all indexes using the new TIDs
4. update table catalog/cache state atomically to target tablespace
5. clear migration flags from the table
6. free old source tablespace pages
7. clear `TIDResolver` migration state
8. mark migration complete and log statistics

### Index update requirement

The current tests prove index TID update support exists across families such as:

- bitmap
- SP-GiST
- LSM and others in the tablespace-migration update suite

Canonical rule:

- swap is not complete until dependent index references have been updated
- any index family lacking correct TID migration support is non-conforming

## Cancel and rollback

### Current cancellation rules

The current code allows cancellation in earlier phases, but rejects cancellation when it is already too late:

- cannot cancel during `MIGRATION_SWAP`
- cannot cancel during `MIGRATION_CLEANUP`
- cannot cancel after complete

Canonical rule:

- cancellation window is explicit
- after swap begins, migration is effectively committed to completion or failure handling

## Migration history

### Current code-backed history

The current runtime persists migration history with:

- `history_id`
- `migration_id`
- `table_id`
- source and target tablespace
- final phase
- `migration_xid`
- total pages
- pages copied
- start and end time
- catch-up iterations
- total bytes copied

History is:

- written before active migration cache removal
- listable globally
- listable per table
- sorted by most recent first

Canonical rule:

- migration completion must leave durable audit history

## Donor-engine proxy and cutover discipline

### Current code-backed substrate

The current engine already has:

- connector attestation
- passthrough policy
- trusted-proxy identity rules
- migration state machine for local online cutover

### Required reconstructed behavior

For donor engines that do not expose natural replication or reliable transaction forcing, proxy migration must follow a cutover discipline derived from the local migration algorithm:

1. assess connector capability and trust posture
2. create explicit migration identity
3. perform initial copy or snapshot extraction
4. track changed or dirty work after the base copy
5. iterate catch-up until below threshold or until explicit cutover decision
6. freeze or fence writes according to donor capability
7. swap routing or execution authority only at an explicit cutover point
8. preserve durable history and rollback evidence

### Canonical rule for weak donor engines

If a donor engine cannot provide:

- natural replication
- stable change cursor
- forced write fencing
- trustworthy transaction boundary visibility

then the migration path must:

- run in bounded proxy or passthrough mode
- remain explicit about degraded guarantees
- refuse to overclaim exact live sync semantics

That is a fail-closed requirement, not optional documentation style.

## MGA boundary

The local online migration algorithm runs inside ScratchBird's MGA engine.

Canonical split:

- MGA remains the truth source for local durability and visibility
- migration state, TID remapping, and cutover sequencing are operational control layers on top of MGA
- donor-engine migration logs or cursors are derivative inputs, not authoritative truth for ScratchBird recovery
