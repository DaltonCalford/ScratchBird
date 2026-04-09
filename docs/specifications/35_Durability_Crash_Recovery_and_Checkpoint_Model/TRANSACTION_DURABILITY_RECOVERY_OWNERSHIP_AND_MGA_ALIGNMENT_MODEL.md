# Transaction Durability Recovery Ownership and MGA Alignment Model

Status: current_authority_with_reconstructed_expansion

## Purpose

Reconcile the older 2026-03-18 transaction, durability, and recovery expansion
plan against the current ScratchBird canon.

This file exists so future hardening work uses the good parts of that plan
without re-importing donor assumptions or reopening already closed MGA truth.

## Governing rule

ScratchBird transaction, durability, checkpoint, sweep, and recovery semantics
must be specified in one MGA-native authority chain.

That authority chain is:

1. transaction and record-state truth
2. ordered durable publication and forced-write fences
3. restart-time state reconciliation
4. reclaim legality and sweep execution
5. observability and certification

No WAL, redo-log, undo-log, or LSN-based explanation may replace that chain.

## What the 2026-03-18 plan got right

The older plan correctly required:

- one owner per topic
- exact state machines rather than loose prose
- ordered commit and rollback durability rules
- checkpoint and startup recovery classification
- reclaim and sweep legality tied to transaction truth
- explicit diagnostics and gate obligations
- anti-WAL protection for Alpha semantics

Those requirements remain correct and binding.

## What is already integrated into current canon

The main architectural asks from the older plan are already satisfied in the
current canonical tree:

- unified MGA record-state truth:
  - `08_Transaction_Core/MGA_RECORD_STATE_AND_PUBLICATION_MODEL.md`
- ordered transaction lifecycle and publication:
  - `08_Transaction_Core/TRANSACTION_LIFECYCLE.md`
  - `08_Transaction_Core/MGA_TRANSACTION_PUBLICATION_AND_RESTART_SEMANTICS.md`
- checkpoint and restart state machine:
  - `08_Transaction_Core/CHECKPOINT_AND_RECOVERY_STATE_MACHINE.md`
  - `35_Durability_Crash_Recovery_and_Checkpoint_Model/STARTUP_RECOVERY_FLOW.md`
- MGA durability and anti-WAL authority:
  - `35_Durability_Crash_Recovery_and_Checkpoint_Model/DURABILITY_MODEL_AND_CORRECTNESS_BOUNDARY.md`
- reclaim legality and sweep sequencing:
  - `10_GC_and_Sweep/RECLAIM_ELIGIBILITY_AND_PUBLICATION_ORDERING.md`
  - `10_GC_and_Sweep/SWEEP_CURSOR_PERSISTENCE_AND_RESTART_RESUMPTION.md`
- failure classification:
  - `42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md`

The 2026-03-18 plan is therefore no longer a roadmap for missing topics.
It is now a reconciliation input for tightening proof depth and owner clarity.

## Cross-section owner matrix

| Topic | Owning section | Notes |
| --- | --- | --- |
| transaction-state machine, savepoints, visibility, publication | `08` | section `08` owns transaction truth and record-state truth |
| durable page truth, forced-write ordering, checkpoint correctness | `35` | section `35` owns durable publication and restart ordering |
| buffer dirty-state staging and writeback coordination | `03` | section `03` owns buffer and writeback mechanics, not transaction truth |
| reclaim legality and sweep execution | `10` | section `10` owns sweep progression and reclaim execution |
| page integrity, checksum, repairability markers | `05` | section `05` owns binary page structure and integrity markers |
| recovery metrics, repair observability, support artifacts | `20` | section `20` owns operator-visible observability |
| certification gates and evidence obligations | `31` | section `31` owns release and fault-injection certification |
| failure precedence and degraded-mode classification | `42` | section `42` owns fault classes and truth-source precedence |

## Explicit non-owner rules

1. section `03` must not redefine commit truth, visibility truth, or restart
   truth
2. section `10` must not define reclaim legality independently of section `08`
3. section `31` must not invent alternative recovery semantics for gate prose
4. section `42` must not redefine durable publication order already owned by
   section `35`

## Anti-WAL normalization rules

The older plan was correct to reject WAL as Alpha truth, but future work can
still drift through donor terminology. The following are therefore
non-canonical unless explicitly labeled optional derivative scope:

- redo replay as restart authority
- WAL distance as durability or flush legality
- LSN advancement as visibility or checkpoint truth
- "replay to rebuild truth" as the primary recovery explanation

ScratchBird restart remains:

- durable page-image reconciliation
- transaction-inventory normalization
- prepared-state evidence validation
- checkpoint and queue rebuild classification

## Remaining useful hardening direction from the older plan

The older plan still provides useful direction in three areas:

### 1. Proof depth

The canon should continue to demand:

- crash-window proof for commit, rollback, checkpoint, and sweep
- partial-write and corruption-containment proof
- prepared or limbo contradiction proof
- savepoint and nested rollback restart proof

### 2. Owner clarity

Future edits must keep the owner matrix above intact rather than scattering
truth across neighboring sections.

### 3. Evidence discipline

Durability and recovery claims must remain bound to executable evidence and
replayable fault-injection artifacts rather than narrative assertions.

## Audit lookup anchors

- `src/core/database.cpp` search `storeCheckpointControlState` for persisted
  checkpoint control truth.
- `include/scratchbird/core/database.h` search
  `AUDIT CONTRACT: when write_admission_fenced() is true` for fail-closed
  publication refusal when durability posture is unsafe.

## Audit rule

Any future finding or donor reference about transaction, durability, or
recovery must be translated into ScratchBird MGA terms before it may alter
canon.

If a proposal depends on:

- WAL truth
- redo replay
- LSN ownership
- PostgreSQL MVCC semantics as default authority

it must be rewritten or rejected.
