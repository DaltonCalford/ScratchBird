# Failure Model and Fault Classification

Status: current_authority

## Purpose

ScratchBird failure handling is MGA-native. The engine does not recover by replaying an authoritative WAL. Instead, recovery reconciles durable page images, transaction inventory state, version-lineage truth, prepared-transaction evidence, checksum state, and committed metadata epochs. This section defines the exact failure model, truth-source precedence, and derivative-lane boundaries.

## MGA versus WAL

ScratchBird and WAL systems solve the same durability and ACID problem through different authority models.

### WAL-shaped systems

A WAL system treats the log as recovery authority. Crash restart replays the log to reconstruct truth. The log is the correctness backbone, and page images may lag behind the log so long as replay can restore them.

### ScratchBird MGA

ScratchBird uses Firebird-style MGA authority:

- committed truth is materialized in the durable database state itself
- transaction inventory defines which versions are visible, retired, prepared, rolled back, or still active
- updates and deletes materialize new transactional states rather than destructively replacing the old truth
- version chains preserve temporal lineage until reclaim proof authorizes cleanup
- restart reconciles durable state; it does not replay a WAL to invent missing truth

Derivative logs, write-after streams, temporal exports, and shadow copies may exist, but they remain subordinate to durable MGA state.

## Truth-source precedence

Truth-source precedence is fixed:

1. durable page images in the primary filespace
2. durable transaction inventory and prepared-transaction evidence
3. verified version-lineage state and committed schema epochs
4. shadow files and verified shadow mirrors
5. derivative write-after streams and temporal archive outputs
6. operator reports, dashboards, or external replay tooling

If two layers disagree, the higher-precedence layer wins. No lower-precedence layer may redefine committed transaction truth.

## Audit lookup anchors

- `src/core/database.cpp` search `storeWritebackIncidentControlState` for
  durable incident persistence and fail-closed reopen posture.
- `include/scratchbird/core/database.h` search
  `AUDIT CONTRACT: when write_admission_fenced() is true` for write-admission
  refusal semantics consumed by failure handling.

## Always-in-transaction rule

Failure handling inherits the core transaction invariants:

- the engine is always in a transaction context
- `COMMIT` ends the current transaction and immediately starts the next transaction
- `ROLLBACK` ends the current transaction and immediately starts the next transaction
- `START TRANSACTION` changes transaction defaults/settings; it does not create transactional mode from idle
- `DDL` and `DML` follow the same transaction, savepoint, rollback, visibility, locking, and recovery rules

Because the engine is always in a transaction, there is never a correctness state defined as "outside a transaction". Restart and repair must restore the ability to resume this model directly.

## Failure classes

Every fault shall be classified into one of the following canonical classes.

### Class F1: transient session or worker failure

Examples:

- parser worker exit
- listener worker exit
- engine worker crash with no durable-state ambiguity
- client disconnect

Required handling:

- retire the failed worker or session
- preserve durable transaction truth
- leave committed state unchanged
- reattach future work through a fresh transaction context

### Class F2: transaction-scope failure with intact durable state

Examples:

- statement execution error
- savepoint rollback
- update conflict
- deadlock victim selection
- lock timeout

Required handling:

- no commit occurs on the failing statement
- the current transaction remains active unless the explicit rollback path is taken
- autocommit mode must not commit the failed statement
- row visibility remains determined by MGA inventory and lineage

### Class F3: prepared or limbo transaction ambiguity

Examples:

- failure between prepare and external resolution
- coordinator loss while a prepared transaction remains durable

Required handling:

- mark the transaction as prepared/uncertain
- retain all versions and derivative evidence required by that state
- refuse reclaim of affected lineage until resolution
- expose the state to operators and tooling as a first-class recovery target

### Class F4: page-local integrity failure

Examples:

- header checksum mismatch
- payload/data checksum mismatch
- page-family marker corruption
- malformed slot table or family-local structure

Required handling:

- classify the page as `repair_required` or `containment_required`
- block destructive cleanup on the page
- allow diagnostic walk where safe
- prefer reconciliation from higher-precedence durable state; never invent truth from derivative logs

### Class F5: transaction-inventory inconsistency

Examples:

- invalid TIP slot state
- missing or malformed transaction inventory page
- inconsistent horizon summaries relative to inventory pages

Required handling:

- stop any reclaim or visibility decision that depends on the damaged inventory region
- treat affected versions as retained/uncertain
- run validation and repair procedures on the inventory pages first
- never advance `OIT`, sweep watermarks, or reclaim markers through uncertain inventory

### Class F6: lineage or version-chain corruption

Examples:

- broken backversion pointer
- impossible transaction-state progression on a lineage
- delete/update lineage missing a required predecessor

Required handling:

- block destructive reclaim on the chain
- classify the target as `repair_required`, `rebuild_required`, or `containment_required`
- preserve all reachable versions until the chain is repaired or explicitly quarantined

### Class F7: index structural failure

Examples:

- broken sibling link
- malformed branch/leaf routing state
- impossible key order for the family
- stale candidate entries after a failed cleanup step

Required handling:

- keep MGA row visibility authoritative
- continue to treat index entries only as candidates
- block destructive family cleanup on the damaged segment
- route to family-local repair or rebuild where defined

### Class F8: derivative-lane failure

Examples:

- write-after stream unavailable
- shadow file write failure
- temporal archive export failure
- derivative queue backlog beyond policy window
- derivative sink quarantine
- multi-tablespace shadow group degraded or incomplete

Required handling:

- committed local MGA truth remains authoritative
- record derivative-lane degradation explicitly
- if the lane is configured `required_before_prune`, block destructive prune for affected items
- if the lane is `best_effort`, allow primary truth to continue while surfacing degraded downstream posture
- preserve derivative source identity and ordering for retry or quarantine
- distinguish single-lane degradation from full shadow-group failover unreadiness

### Derivative-lane subclassification

Class `F8` is further subclassified as:

1. `F8A_SHADOW_MIRROR_FAILURE`
2. `F8B_WAL_AFTER_DELIVERY_FAILURE`
3. `F8C_TEMPORAL_ARCHIVE_DELIVERY_FAILURE`
4. `F8D_DERIVATIVE_BACKPRESSURE`
5. `F8E_DERIVATIVE_QUARANTINE`
6. `F8F_SHADOW_GROUP_DEGRADED`

Required rules:

1. `F8A` and `F8F` are evaluated against the active durability policy domain for the affected filespace or filespace group
2. `F8B` and `F8C` are derivative shipping failures and do not retroactively change local commit truth
3. `F8D` changes derivative operating posture only; it does not, by itself, redefine recovery truth
4. `F8E` requires preserved identity, preserved sequence ordering, and operator-visible release procedure

### Class F9: forced-write or ordered-write breach

Examples:

- transaction inventory state not durable before dependent reclaim
- free-space publication outrunning durable page write
- shadow target not honoring forced-write rules when shadow mode is required

Required handling:

- classify as correctness-critical
- stop advancement of dependent metadata or reclaim markers
- require containment and operator-visible repair action
- treat any apparently completed downstream steps as untrusted until revalidated

## Ordered-write correctness model

ScratchBird correctness depends on ordered writes. The minimum required order is:

1. materialize page-local state for the transaction outcome or maintenance action
2. durably publish transaction inventory state needed for visibility truth
3. durably publish dependent page-image updates
4. only then publish derivative metadata such as free-space summaries, cleanup watermarks, or sweep advancement markers
5. only then permit derivative lanes to mark themselves caught up

Any implementation detail borrowed from another engine is acceptable only if it preserves this authority order under the MGA model.

## Forced-write posture

Forced writes are not an optimization hint. They are a correctness contract.

Required rules:

- primary durable state must honor forced-write posture for transaction truth and maintenance actions that change correctness state
- shadow files must honor the same correctness posture when shadowing is enabled as a required lane
- write-after logs and temporal archive outputs may lag only within the policy explicitly allowed for derivative lanes
- no derivative lane may be treated as proof that a non-durable primary write is safe

## Restart model

Restart shall proceed as state reconciliation in this order:

1. admit only if primary durable filespace state is structurally readable enough to classify
2. validate header pages, transaction inventory pages, and other bootstrap/control pages
3. reconstruct current horizons and transaction-state inventory from durable pages
4. classify prepared, active-interrupted, committed, and rolled-back states
5. validate heap lineage and index structural state as needed for safe service admission
6. reopen the engine into service only for modes supported by the classification outcome

Restart is not log replay. If derivative logs exist, they may support observability, replication catch-up, or forensic comparison, but they do not become recovery authority.

## Restore, promotion, and failback relationship

Restore, promoted-shadow handoff, and failback are part of the failure model because they change which durable page-image set becomes the live route.

Required rules:

1. restore, promotion, and failback are page-image and reconciliation procedures, not derivative-log replay procedures
2. a promoted shadow becomes authoritative only by route selection and successful reconciliation, not because derivative shadow traffic outranks primary truth
3. failback is restore-style and must refuse blind reattachment to a stale route
4. if a promoted or failback route fails reconciliation, the system enters quarantine, read-only, or refusal posture according to the classified failure
5. derivative continuity markers before and after restore or failover are inspection aids only

## Multi-tablespace shadow-group failure semantics

When multiple tablespaces form one promoted or protected unit, shadow readiness is evaluated as a group.

Required group states:

1. `ACTIVE`
2. `DEGRADED`
3. `PROMOTION_READY`
4. `PROMOTED`
5. `RETIRED`

Required rules:

1. a missing or unverified required member places the group in `DEGRADED`
2. a `DEGRADED` group may continue local MGA truth where policy allows, but it may not claim full failover readiness
3. full-group promotion is refused while any required member remains missing or degraded
4. after promotion or failback, a new shadow group must be established from the newly live route set

## Derivative backpressure and fence policy

Derivative backlog is a failure-model concern only insofar as it affects policy-controlled downstream guarantees.

Required backpressure classes:

1. `NONE`
2. `OBSERVE_ONLY`
3. `THROTTLE_DERIVATIVE_ONLY`
4. `FENCE_OPTIONAL_POLICY`

Required rules:

1. `NONE`, `OBSERVE_ONLY`, and `THROTTLE_DERIVATIVE_ONLY` do not redefine commit truth
2. `FENCE_OPTIONAL_POLICY` is valid only when an explicit policy requires derivative durability in addition to local durability
3. no backpressure class permits derivative lanes to outrank MGA truth
4. quarantine and retry remain identity-preserving across backpressure transitions

## Sweep and garbage collection relationship

Sweep is part of the failure model because MGA retains historical truth until reclaim proof exists.

Required rules:

- sweep must validate before prune
- sweep must preserve prepared and uncertain states
- sweep may emit write-after, shadow, and temporal derivative records before prune when configured
- sweep may reclaim only after verified transaction-state and visibility proof
- sweep must not use derivative outputs as permission to override uncertain local truth

## Locking relationship

The failure model assumes Firebird-style MGA locking semantics:

- ordinary row-version conflict is write-write only
- read visibility is resolved through MGA inventory and lineage, not through reader-held tuple locks
- metadata locks and structural latches may exist, but they are subordinate to MGA truth
- any borrowed PostgreSQL-style lock machinery is acceptable only if it does not compromise these rules

## Index relationship

The failure model assumes MGA-first index semantics:

- indexes are candidate finders, not truth stores
- split, merge, sibling chase, and page latches are structural rules only
- dead-entry cleanup is downstream of heap-version reclaim proof
- optimizer-visible metrics must track visibility rejects, dead-entry debt, and structural health

## Derivative-lane relationship

### Write-after log

- used for replication and downstream consumers
- emitted after authoritative local truth exists
- never treated as recovery authority

### Shadow files

- page-image mirrors under forced-write discipline
- may support faster recovery paths or redundancy
- never redefine transaction truth independently of primary durable MGA state

### Temporal archive

- preserves committed history and, where policy allows, rolled-back history for forensic or historical use
- remains a derivative history lane, not the primary database of truth

## Explicit non-requirements

ScratchBird does not require or authorize:

- WAL replay as the primary crash-recovery algorithm
- redo-first truth reconstruction from a log
- undo-log-authoritative rollback reconstruction
- visibility rules defined by tuple reader locks instead of MGA inventory and lineage
- destructive replacement of history before reclaim proof succeeds
