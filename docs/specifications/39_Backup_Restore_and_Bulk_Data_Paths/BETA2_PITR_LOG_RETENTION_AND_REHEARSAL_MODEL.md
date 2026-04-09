# Beta 2 PITR Log Retention And Rehearsal Model

## Purpose

Define point-in-time restore for ScratchBird using derivative recovery capsules
derived from committed MGA state, plus mandatory rehearsal support.

## Governing rules

1. PITR is not WAL-authoritative recovery.
2. PITR consumes a base restore image plus ordered derivative recovery capsules.
3. A target timestamp is a selection point over certified capsules, not a
   request to invent missing history.
4. Every admitted PITR policy must support rehearsal.

## PITR artifact classes

- `BASE_IMAGE`
- `RECOVERY_CAPSULE`
- `RESTORE_CHAIN_MANIFEST`
- `REHEARSAL_RUN_RECORD`

## Recovery capsule contents

Each `RECOVERY_CAPSULE` shall contain:

- sequence range
- commit timestamp floor and ceiling
- transaction lineage digest
- page or object delta payload
- schema epoch references
- checksum and manifest hash

## Retention policy

Each protected database group shall declare:

- `pitr_window`
- base image interval
- capsule interval
- rehearsal interval
- immutable hold class if required

Capsules outside the retention window may be pruned only after the next valid
base image and chain manifest are verified.

## Restore workflow

1. Select base image at or before the target timestamp.
2. Validate chain manifest continuity.
3. Restore the base image into a scratch target.
4. Apply ordered recovery capsules whose commit ceiling is at or before the
   target timestamp.
5. Validate schema epoch references and transaction lineage continuity.
6. Publish the restored target as:
   - rehearsal-only
   - operator inspection image
   - failover-eligible image where the enclosing HA policy permits it

## Rehearsal workflow

1. Create or reuse an isolated rehearsal target.
2. Restore the declared base image.
3. Apply PITR capsules to one or more target timestamps.
4. Record:
   - restore duration
   - missing or corrupt capsule count
   - validation failures
   - operator-visible divergence notes
5. Mark the chain `REHEARSAL_VALID` or `REHEARSAL_FAILED`.

## Refusal rules

- chain gap detected: `PITR_CHAIN_GAP`
- target timestamp predates retained window: `PITR_TARGET_BEFORE_WINDOW`
- schema epoch unresolved: `PITR_SCHEMA_EPOCH_MISSING`
- rehearsal overdue for protected policy: `PITR_REHEARSAL_REQUIRED`

## Metrics

- oldest and newest restorable timestamp
- rehearsal success rate
- capsule lag and base image lag
- restore duration by database group

## Sample selection logic

```cpp
BaseImage base = pitr.pick_base_image(target_ts);
CapsuleRange range = pitr.pick_capsules(base.image_id, target_ts);
if (!range.contiguous()) return fail(PITR_CHAIN_GAP);
restore(base, range);
```

## Cross-section requirements

- section 39 owns artifact packaging and restore execution
- section 42 owns whether the restored image may become a promoted service node
- section 31 owns the rehearsal and certification gates
