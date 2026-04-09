# Online Schema Change and Backfill Model

Status: current_authority_with_reconstructed_expansion

## Purpose

Define the bounded online schema-change model for eligible operations,
including phase states, lock behavior, invalidation, backfill, cutover,
rollback, and observability.

This file defines the Beta 1 online-schema-change classes admitted by canon.
Any operation not classified here is `REWRITE_REQUIRED` or refused.

## Current code-backed authority

Current code now proves:
- durable `schema_change_plan`, `schema_change_event`,
  `schema_change_backfill_progress`, and `schema_change_cutover_guard` row
  families
- commit-bound schema-epoch publication together with schema-change plan and
  event emission
- `ALTER TABLE ... ALTER COLUMN ... SET NOT NULL` validated promotion as
  `EXPAND_BACKFILL_CUTOVER`
- metadata-only `ALTER COLUMN TYPE` support for bounded compatible widening or
  precision expansion that requires no stored-row rewrite
- fail-closed `REWRITE_REQUIRED` refusal for `DROP COLUMN` and incompatible
  `ALTER COLUMN TYPE` changes that would require a physical rewrite or unsafe
  conversion
- metadata-only classification for bounded catalog-only schema changes

This file additionally defines the required Beta 1 expansion to:

- make invisible and candidate index states canonical
- use resumable backfill and cutover guard semantics for admitted index builds
- share one phase vocabulary across schema backfill and online index publication
- widen the admitted `METADATA_ONLY` and `EXPAND_BACKFILL_CUTOVER` surfaces
  where ScratchBird already has schema-epoch and plan-row substrate

## Eligibility classes

| Class | Model |
| --- | --- |
| `METADATA_ONLY` | transactional metadata change with no background backfill |
| `EXPAND_BACKFILL_CUTOVER` | bounded phased change with background population and commit-bound cutover |
| `REWRITE_REQUIRED` | offline or refused if a safe phased model is unavailable |

## Beta 1 operation classification matrix

| Operation family | Class | Canonical rule |
| --- | --- | --- |
| `CREATE` or `DROP` schema object with no data rewrite | `METADATA_ONLY` | transactional catalog publication only |
| `RENAME` table, column, index, constraint, view, or sequence | `METADATA_ONLY` | commit-bound rename publication, no data rewrite |
| `SET DEFAULT` or `DROP DEFAULT` | `METADATA_ONLY` | affects future writes only |
| `ADD NULLABLE COLUMN` with no default/backfill | `METADATA_ONLY` | old rows read `NULL` without data rewrite |
| compatible widening `ALTER COLUMN TYPE` or precision expansion that preserves stored-row representation | `METADATA_ONLY` | commit-bound catalog publication only; refuse if a conversion or rewrite would be required |
| `ALTER INDEX ... SET VISIBLE` or `ALTER INDEX ... SET INVISIBLE` | `METADATA_ONLY` | metadata-only state change; no data rewrite |
| `CREATE INDEX` on an empty relation | `METADATA_ONLY` | publish metadata and active storage in one transaction |
| row-UUID alias metadata binding that only rebinds existing system row UUID semantics | `METADATA_ONLY` | catalog-only metadata publication |
| `ADD COLUMN` with deterministic constant default or stored-generated value requiring existing-row population | `EXPAND_BACKFILL_CUTOVER` | requires resumable backfill and cutover guard |
| validated promotion from nullable to not-null after explicit backfill proof | `EXPAND_BACKFILL_CUTOVER` | requires validation and commit-bound cutover |
| `CREATE INDEX` on a populated relation | `EXPAND_BACKFILL_CUTOVER` | requires online build plan, side log, validation, and cutover guard |
| `CREATE INDEX INVISIBLE` on a populated relation | `EXPAND_BACKFILL_CUTOVER` | same as `CREATE INDEX`, but remains invisible after cutover until promoted |
| `ADD STORED GENERATED COLUMN` with deterministic expression and explicit backfill plan | `EXPAND_BACKFILL_CUTOVER` | requires resumable backfill and validation |
| incompatible type rewrite, storage-layout rewrite, `DROP COLUMN`, repartitioning, filespace rewrite, or non-deterministic data transform | `REWRITE_REQUIRED` | offline or refused unless a future spec admits a safe phased model |

If an operation family is not listed above, the engine must classify it as
`REWRITE_REQUIRED` or refuse it.

## Phase states

1. `DRAFTED`
2. `EXPANDED_METADATA`
3. `BACKFILL_ACTIVE`
4. `CUTOVER_PENDING`
5. `CUTOVER_COMMITTED`
6. `CONTRACT_PENDING`
7. `ROLLED_BACK`
8. `ABORTED_FAIL_CLOSED`

For index-build plans integrated with schema DDL, the same phase vocabulary
applies and the durable plan rows defined by section `18` are authoritative for
the index-build payload.

## Canonical index visibility states

Admitted index visibility states:

| State | Meaning |
| --- | --- |
| `BUILDING` | not available for query planning or enforcement |
| `INVISIBLE` | physically valid but planner-hidden except by explicit operator override |
| `VISIBLE_CANDIDATE` | visible only to advisor or explicit controlled trials |
| `VISIBLE_ACTIVE` | fully planner-visible and enforcement-active |
| `RETIRED` | retained only for old snapshots or operator rollback hold |

Required rules:

1. `BUILDING` and `INVISIBLE` indexes are excluded from ordinary planner use.
2. `VISIBLE_CANDIDATE` may be used only by explicit advisor or operator trial
   sessions.
3. unique or constraint-backed indexes must reach `VISIBLE_ACTIVE` before they
   become enforcement authority.
4. `ALTER INDEX ... SET VISIBLE` and `ALTER INDEX ... SET INVISIBLE` are
   metadata-only state changes within the rules above.

## Durable catalog records

### Table: `schema_change_plan`

Columns:
- `schema_change_plan_uuid` `[sb_dom]cat_uuid` PK
- `object_uuid` `[sb_dom]cat_uuid`
- `object_type` `[sb_dom]cat_identifier`
- `requested_operation` `[sb_dom]cat_identifier`
- `change_class` `[sb_dom]cat_identifier`
- `requested_by_uuid` `[sb_dom]cat_uuid`
- `requested_at` `[sb_dom]cat_timestamp`
- `phase_state` `[sb_dom]cat_identifier`
- `baseline_schema_epoch` `[sb_dom]cat_version_u64`
- `expanded_schema_epoch` `[sb_dom]cat_version_u64` nullable
- `cutover_schema_epoch` `[sb_dom]cat_version_u64` nullable
- `rollback_class` `[sb_dom]cat_identifier`
- `refusal_reason_code` `[sb_dom]cat_identifier` nullable
- `refusal_detail_uuid` `[sb_dom]cat_uuid` nullable
- `is_valid` `[sb_dom]cat_bool`

### Table: `schema_change_event`

Columns:
- `schema_change_event_uuid` `[sb_dom]cat_uuid` PK
- `schema_change_plan_uuid` `[sb_dom]cat_uuid`
- `event_seq` `[sb_dom]cat_uint64`
- `phase_from` `[sb_dom]cat_identifier` nullable
- `phase_to` `[sb_dom]cat_identifier`
- `event_state` `[sb_dom]cat_identifier`
- `event_code` `[sb_dom]cat_identifier` nullable
- `event_detail_uuid` `[sb_dom]cat_uuid` nullable
- `event_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`schema_change_plan_uuid`, `event_seq`)

### Table: `schema_change_backfill_progress`

Columns:
- `schema_change_backfill_progress_uuid` `[sb_dom]cat_uuid` PK
- `schema_change_plan_uuid` `[sb_dom]cat_uuid`
- `worker_generation` `[sb_dom]cat_version_u64`
- `scanned_row_count` `[sb_dom]cat_uint64`
- `written_row_count` `[sb_dom]cat_uint64`
- `validated_row_count` `[sb_dom]cat_uint64`
- `last_resume_row_uuid` `[sb_dom]cat_uuid` nullable
- `last_resume_key_json` `[sb_dom]cat_json` nullable
- `partial_chunk_rewind_required` `[sb_dom]cat_bool`
- `restart_disposition` `[sb_dom]cat_identifier`
- `last_heartbeat_at` `[sb_dom]cat_timestamp` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`schema_change_plan_uuid`)

### Table: `schema_change_cutover_guard`

Columns:
- `schema_change_cutover_guard_uuid` `[sb_dom]cat_uuid` PK
- `schema_change_plan_uuid` `[sb_dom]cat_uuid`
- `expected_pre_cutover_schema_epoch` `[sb_dom]cat_version_u64`
- `validation_manifest_hash` `[sb_dom]cat_uint64`
- `dependency_refresh_complete` `[sb_dom]cat_bool`
- `expected_security_epoch` `[sb_dom]cat_version_u64` nullable
- `guard_state` `[sb_dom]cat_identifier`
- `checked_at` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`schema_change_plan_uuid`)

## Lock and visibility rules

1. Metadata publication remains transaction-scoped and epoch-bound.
2. Expand metadata may become visible before cutover only when old and new
   layouts are both valid.
3. Cutover is a commit-bound publication point.
4. Contract/drop of legacy layout may occur only after backfill and validation
   proof.
5. If any phase cannot preserve MGA visibility and deterministic binding, the
   engine must refuse the operation or classify it as `REWRITE_REQUIRED`.
6. Planner-visible index state changes must bind to the same committed schema
   epoch and dependency-invalidation boundary as other metadata changes.

## Phase transition rules

1. `DRAFTED -> EXPANDED_METADATA` requires that the catalog plan row exists,
   baseline schema epoch is pinned, and the expanded metadata shape is valid.
2. `EXPANDED_METADATA -> BACKFILL_ACTIVE` is allowed only for
   `EXPAND_BACKFILL_CUTOVER` plans.
3. `BACKFILL_ACTIVE -> CUTOVER_PENDING` requires durable progress rows showing
   backfill completion and a durable cutover-guard row showing validation
   readiness.
4. `CUTOVER_PENDING -> CUTOVER_COMMITTED` occurs only at the commit boundary
   that publishes the new schema epoch.
5. `CUTOVER_COMMITTED -> CONTRACT_PENDING` is allowed only after the engine can
   prove both old and new metadata are no longer required for correctness.
6. Any pre-cutover phase may move to `ROLLED_BACK` on rollback.
7. Any phase must move to `ABORTED_FAIL_CLOSED` if restart, validation, or
   visibility proof becomes ambiguous.

For index builds:

8. `CUTOVER_PENDING -> CUTOVER_COMMITTED` is allowed only when the index build
   cutover guard proves side-log drain complete and the expected schema epoch
   still matches.
9. `CUTOVER_COMMITTED -> CONTRACT_PENDING` is allowed only after older
   snapshots no longer require the retired index version.

## Backfill rules

- backfill workers operate under bounded budget and observability controls
- backfill progress must be resumable or restart-safe
- backfill writes are ordinary transactional writes and obey the same MGA rules
  as other data changes
- partial backfill state must be observable and must not be mistaken for
  cutover completion
- progress state must persist enough anchor data to resume from the last
  committed row-UUID anchor or deterministic key anchor
- restart may rewind the final partial chunk, but it must not guess that
  in-memory-only progress was committed

For index builds, resumable progress and side-log drain state are owned by the
section `18` durable build rows and must follow the same restart-safety rule.

## Cutover guard rules

Cutover requires a durable guard record proving at minimum:
- expected pre-cutover schema epoch
- validation manifest hash for the expanded layout
- dependency-refresh completion
- expected security epoch when security-sensitive metadata changes are part of
  the same plan

## Rollback rules

1. Before cutover, rollback retires new metadata and backfill progress for the
   change.
2. After cutover, rollback requires an explicit reverse change plan; it is not
   assumed to be an interior savepoint rewind.
3. Failure to guarantee safe reversal requires fail-closed refusal.

Invisible index promotion or demotion is metadata-only and may roll back inside
the current transaction before commit. Physical index builds already published
by an earlier transaction require a new forward change plan rather than an
interior savepoint rewind.

## Observability rules

The engine shall expose:
- phase state
- progress counters
- validation failures
- cutover boundary identity
- rollback/refusal reason
- cache invalidation epoch changes
- durable plan, event, backfill-progress, and cutover-guard identities
- index visibility state and candidate-state transitions
- integrated index-build plan identity when the change includes online index
  work

## Required Beta 1 defaults

| Tunable | Default | Range | Reloadability |
| --- | --- | --- | --- |
| `sb.ddl.backfill_chunk_rows` | `10000` | `100..1000000` | reloadable |
| `sb.ddl.backfill_resume_heartbeat_ms` | `1000` | `100..10000` | reloadable |
| `sb.ddl.invisible_index_trial_max_sessions` | `4` | `1..128` | reloadable |

## Sample control flow

```cpp
SchemaChangeResult executeSchemaPlan(SchemaChangePlan& plan) {
  classifyOrRefuse(plan);
  persistPlan(plan);
  expandMetadataIfRequired(plan);
  if (plan.changeClass() == ChangeClass::kExpandBackfillCutover) {
    runResumableBackfill(plan);
    writeCutoverGuard(plan);
  }
  commitCutover(plan);
  invalidateBoundMetadata(plan);
  return SchemaChangeResult::Committed(plan.cutoverEpoch());
}
```
