# Reclaim Eligibility and Publication Ordering

Status: current_authority

## Purpose

This document defines the exact proof required before ScratchBird may reclaim historical versions, publish reclaimed free space, or clean dead index entries under the MGA model.

## Reclaim proof

A heap version is reclaimable only when all of the following are true:

1. the creating transaction is terminal
2. any superseding or deleting transaction is terminal if present
3. no active snapshot, prepared transaction, or preserved forensic hold can legally observe the version
4. the version chain is structurally valid
5. derivative evidence requirements have completed or are explicitly configured as best-effort
6. no page-verification failure blocks destructive mutation on the page

## Terminal transaction classes

For reclaim purposes transaction states are grouped as:

- `committed_visible`
- `rolled_back_invisible`
- `prepared_uncertain`
- `active_uncertain`
- `damaged_unknown`

Only `committed_visible` and `rolled_back_invisible` states are terminal enough to participate in reclaim proof. `prepared_uncertain`, `active_uncertain`, and `damaged_unknown` force retention.

## Ordered publication for update chains

For an update that creates a new version and obsoletes an older version, the authoritative ordering is:

1. materialize the new version with creating transaction stamp
2. maintain lineage pointer from the new version to the prior visible version where the family requires it
3. insert or update candidate index entries as non-authoritative search aids
4. durably publish transaction terminal state in transaction inventory
5. allow newer transactions to see the new version through MGA visibility rules
6. retain the older version until reclaim proof succeeds
7. only then allow sweep to prune the older version and clean associated dead index entries

## Ordered publication for delete chains

A delete is a new transactional state on the version chain. ScratchBird shall:

1. stamp the delete action with the deleting transaction
2. preserve earlier visible history until terminal-state proof exists
3. treat the deleted head as visible or invisible according to the reader's snapshot
4. reclaim historical material only after the delete transaction is terminal and no older snapshot can still require the prior visible state

## Ordered publication for rollback

A rolled-back writer does not publish a visible new truth. The system shall:

1. mark the transaction as rolled back in transaction inventory
2. keep any damaged or uncertain lineage intact until verified
3. make rolled-back versions eligible for reclaim once the chain is verified and no active repair hold exists

## Derivative evidence ordering

If configured, derivative evidence shall run before destructive reclaim in this order:

1. local audit event
2. write-after record for replication
3. shadow-page copy
4. temporal archive export

If a derivative lane is in `required_before_prune` mode and fails, prune must not proceed for the affected versions. If the lane is in `best_effort` mode, prune may proceed but the failure must be recorded as degraded observability.

## Index cleanup ordering

Index cleanup must obey all of the following:

- heap reclaim proof first
- dead-entry identification second
- family-local structural re-check third
- physical deletion or compaction fourth
- metrics refresh last

No index family may delete an entry solely because a newer candidate entry exists. The entry is removable only when the referenced heap-version truth is already reclaimable.

## Publication of reclaimed free space

Free-space publication shall obey this order:

1. write compacted heap page image
2. durably persist page checksums and generation markers
3. publish updated free-space inventory
4. update sweep metrics and debt counters

## Containment rules

If page verification, checksum validation, or lineage validation fails:

- freeze reclaim on that page or chain
- classify the target as `repair_required` or `containment_required`
- allow read-only diagnostic walk if supported
- refuse destructive cleanup until a later verified pass or an explicit repair path authorizes it
