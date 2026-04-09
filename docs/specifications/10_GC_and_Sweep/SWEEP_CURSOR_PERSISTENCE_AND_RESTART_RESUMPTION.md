# Sweep Cursor Persistence and Restart Resumption

Status: reconstructed_required_with_current_substrate

## Purpose

Define the persisted sweep-cursor format, publish timing, restart validation,
and rewind rules for resumable sweep runs.

## Governing rule

Cursor persistence is authoritative progress state for sweep resume. It is not
reclaim truth.

The current runtime already persists richer progress state than the older short
prose captured. The richer shape is the canonical contract for Beta 1.

## Persisted cursor fields

Each persisted cursor stores:

- `sweep_generation`
- `active`
- `stage`
- `relation_uuid`
- `filespace_uuid`
- `page_id`
- `slot_id`
- `captured_oit`
- `captured_oat`
- `captured_ost`
- `checkpoint_generation_seen`
- `resume_lane_mask`
- `resume_strict_audit`
- `reclaimed_versions`
- `reclaimed_pages`
- `index_backlog_count`
- `checksum_valid`
- `cursor_crc32c`
- `persist_time`

## Persist checkpoints

The cursor is persisted:

- at sweep start
- every configured interval or page budget
- during in-flight reclaim page progress when the reclaim contract requires it
- before orderly shutdown when sweep is active
- at terminal failure
- at sweep completion

## Restart resume rules

On restart:

1. validate cursor checksum-valid state and `cursor_crc32c`
2. validate generation compatibility and checkpoint-generation compatibility
3. validate relation/filespace/page-family compatibility
4. validate `resume_lane_mask` and `resume_strict_audit` against current policy
5. validate captured `OIT/OAT/OST` compatibility with current restart posture
6. resume from the persisted cursor only if all checks pass
7. otherwise rewind deterministically to the start of the current relation or a
   safe relation boundary

## Hard invariants

1. Cursor persistence is advisory progress, not reclaim truth.
2. A cursor captured under one horizon set may not be resumed under an
   incompatible one.
3. Cursor persistence may not claim work complete before heap/index ordering
   obligations are published.
4. Failed cursor persistence must be observable and must not silently mark sweep
   complete.

## Required errors

The runtime shall expose or map to these canonical resume/persist error classes:

- `SWEEP_CURSOR_CHECKSUM_FAIL`
- `SWEEP_CURSOR_GENERATION_MISMATCH`
- `SWEEP_CURSOR_REPAIR_REWIND_REQUIRED`
- `SWEEP_CURSOR_PERSIST_FAILED`

## Required operator-visible outcomes

- `RESUME_FROM_CURSOR`
- `REWIND_TO_SAFE_RELATION_BOUNDARY`
- `START_FRESH_SWEEP_GENERATION`

## Required tests

- valid compatible cursor resumes from the persisted position
- checksum failure rewinds deterministically
- generation/checkpoint mismatch rewinds deterministically
- incompatible lane-mask/strict-audit state refuses blind resume
- in-flight reclaim persistence points survive restart without claiming more
  progress than was durably published
- terminal failure persistence does not masquerade as completion

## Cross-section references

- `GC_SWEEP_ALGORITHM.md`
- `RECLAIM_ELIGIBILITY_AND_PUBLICATION_ORDERING.md`
- `../08_Transaction_Core/CHECKPOINT_AND_RECOVERY_STATE_MACHINE.md`
- `../24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_RUNTIME_CONTEXT.md`
