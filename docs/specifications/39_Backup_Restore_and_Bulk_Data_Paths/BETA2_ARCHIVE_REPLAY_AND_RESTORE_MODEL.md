# Beta 2 Archive Replay And Restore Model

## Purpose

Define how archived historical material is replayed or restored without
mutating current primary MGA truth.

## Governing rules

1. Archive replay materializes an isolated historical image.
2. Restore from archive must verify manifest and hash continuity before data is
   admitted.
3. Archive replay is never implicit during ordinary startup recovery.

## Replay targets

- `FORENSIC_IMAGE`
- `REHEARSAL_IMAGE`
- `EXPORT_IMAGE`
- `PROMOTION_CANDIDATE_IMAGE` when enclosing HA policy explicitly allows it

## Restore workflow

1. Select archive batch range and schema epoch coverage.
2. Verify manifest hash chain.
3. Materialize a scratch restore image.
4. Import archived payloads into the image.
5. Reconstruct historical visibility and lineage windows.
6. Publish the image in one of the admitted target classes.

## Constraints

- restored archive images are read-only by default
- promotion to writable or failover-eligible state requires a distinct
  operator-controlled cutover and compatibility validation
- archive replay may not modify the current live database root

## Refusal rules

- `ARCHIVE_REPLAY_CHAIN_BROKEN`
- `ARCHIVE_REPLAY_SCHEMA_EPOCH_MISSING`
- `ARCHIVE_REPLAY_TARGET_CLASS_REFUSED`

## Metrics

- archive replay duration
- verified batch count
- replayed lineage range
- replay refusal count

## Cross-section requirements

- section 10 owns archive lifecycle and legal-hold policy
- section 39 owns restore execution and target-image publication
- section 31 owns rehearsal certification
