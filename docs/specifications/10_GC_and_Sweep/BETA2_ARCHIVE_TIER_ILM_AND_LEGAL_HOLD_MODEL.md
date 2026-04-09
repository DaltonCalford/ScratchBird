# Beta 2 Archive Tier ILM And Legal Hold Model

## Purpose

Define the real archive tier, lifecycle policy, and legal-hold behavior needed
for Beta 2 retention and replay from archive.

## Governing rules

1. Archive is an explicit lifecycle tier, not an implied side effect of sweep.
2. Archive-before-prune requires verified manifest and payload durability.
3. Legal hold outranks retention expiry and prune.
4. Archived evidence never becomes primary MGA truth automatically.

## Lifecycle tiers

- `HOT`
- `WARM`
- `COLD`
- `IMMUTABLE_ARCHIVE`

## Policy row

`sb_archive_policy` shall persist:

- `policy_uuid`
- `scope_uuid`
- `tier_target`
- `eligibility_horizon`
- `minimum_replay_window`
- `compression_policy`
- `encryption_policy`
- `legal_hold_class`
- `object_lock_required`

## Archive unit

The archive unit is `ARCHIVE_BATCH` and must contain:

- batch uuid
- scope identity
- version range or lineage range
- schema epoch references
- payload manifest
- payload hash
- manifest hash
- tier target
- legal-hold flags
- archive commit marker

## Archive workflow

1. Sweep identifies archive-eligible versions.
2. Archive planner groups them into an `ARCHIVE_BATCH`.
3. Payloads and manifests are materialized and checksummed.
4. Archive storage write completes.
5. Archive commit marker is durably published.
6. Only then may prune eligibility be re-evaluated.

## Legal hold

Legal hold may be attached at:

- transaction lineage scope
- object scope
- policy scope
- archive batch scope

Held material may be copied to immutable storage but may not be pruned.

## Refusal rules

- `ARCHIVE_COMMIT_MARKER_MISSING`
- `ARCHIVE_PAYLOAD_HASH_MISMATCH`
- `ARCHIVE_OBJECT_LOCK_REQUIRED`
- `ARCHIVE_LEGAL_HOLD_BLOCK`

## Metrics

- eligible versus archived version counts
- archive lag by scope
- prune blocked by legal hold count
- archive verification failures

## Cross-section requirements

- section 10 owns lifecycle eligibility and archive-before-prune
- section 39 owns archive restore and replay materialization
- section 20 owns hold and archive audit surfaces
