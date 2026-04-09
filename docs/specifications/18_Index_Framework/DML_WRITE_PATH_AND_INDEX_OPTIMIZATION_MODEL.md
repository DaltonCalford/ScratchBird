# DML Write Path and Index Optimization Model

Status: reconstructed_required_with_current_substrate

## Purpose

Define the canonical Beta 1 optimization model for ScratchBird `INSERT`,
`UPDATE`, `DELETE`, and index-maintenance work. This file turns the approved
optimization audit into implementation-driving canon.

This file is not a donor summary. It is the required ScratchBird behavior for
the Beta 1 implementation wave.

## Scope

This file owns:

- exact-family same-key update suppression
- statement-local write-shape classification and metadata hoisting for exact
  maintenance
- unchanged-key non-indexed update elision for exact families
- commit-group batch apply for index deltas
- reclaim-driven exact cleanup debt and compaction
- narrow cold-page delta buffering for admitted exact secondaries
- pending lanes for boolean inverted, ranked text, and other many-key families
- standardized shadow-build, side-log, validate, and publish flow
- immutable generation publication for heavy families
- hot-leaf and monotonic-key contention mitigation
- required tunables, observability, refusal rules, and test obligations

This file does not replace:

- section `08` MGA record truth
- section `10` reclaim legality
- section `35` checkpoint and restart authority
- section `37` schema-epoch and online-DDL authority
- section `38` global maintenance scheduling authority
- section `39` bulk-ingest external surfaces

## Current implementation-backed substrate

Current code-backed substrate that this file builds upon:

- `CatalogManager::createShadowIndex(` and `CatalogManager::promoteShadowIndex(`
- current index metadata carrying `state`, `valid_from_xid`, and `retired_xid`
- family registry and runtime taxonomy through `IndexFactory`
- `GIN_PENDING_LIST_THRESHOLD` and current pending-list machinery
- `auto BTree::bulkLoad(` for sorted exact build
- family-local dead-entry cleanup hooks
- MGA-aware visibility checks in exact, columnar, and ANN paths
- transactional schema-epoch publication and `classifySchemaChangeClassForSql(`

## Beta 1 adoption matrix

| Optimization | Required Beta 1 state |
| --- | --- |
| `OPT-01` same-key exact update suppression | required |
| `OPT-02` reclaim-driven exact cleanup debt | required |
| `OPT-03` cold-page secondary delta buffer | required with narrow admission |
| `OPT-04` many-key pending lanes | required |
| `OPT-05` shadow-build and resumable publish | required |
| `OPT-06` immutable heavy-family publication | required |
| `OPT-08` commit-group batch apply | required |
| `OPT-09` hot-leaf mitigation | required |
| `OPT-10` statement-local metadata hoist and unchanged-key elision | required |

`OPT-07` checkpoint-bound delta reconciliation is owned jointly with section
`35`.

## Hard invariants

1. Heap and version-chain truth remain authoritative under section `08`.
2. Indexes are candidate finders only. Final acceptance always rechecks MGA
   visibility.
3. No optimization may make uncommitted data visible.
4. No optimization may remove exact-family uniqueness proof from the exact
   family itself.
5. Reclaim and destructive cleanup remain subordinate to reclaim legality.
6. Parser, wire protocol, and donor-catalog emulation remain outside this file.
7. Background work may reduce foreground cost, but it may not weaken commit
   publication ordering.
8. Heavy-family publication units may not replace exact-family correctness.

## Family maintenance classes

Every index instance shall belong to one maintenance class:

| Maintenance class | Families | Foreground write contract | Read contract | Publication unit |
| --- | --- | --- | --- | --- |
| `EXACT` | `BTREE`, `HASH`, exact `LSM`, exact `ART`, exact `TRIE` | commit-bound correctness | direct candidate probe | mutable page or bucket state |
| `SUMMARY` | `BRIN`, `BLOOM`, `BITMAP`, summary `COLUMNSTORE` | summary delta allowed | merge mutable plus sealed state | immutable generation or summary block |
| `BOOLEAN_INVERTED` | `GIN`, boolean `INVERTED` | pending-lane plus main structure | evaluate pending plus sealed view | pending list plus main tree |
| `RANKED_TEXT` | ranked `INVERTED`, `SPARSE_INVERTED`, `SPARSE_WAND`, text `COLUMNSTORE` | append mini-segment and tombstone segment | query across mutable and sealed segments | immutable segment |
| `ANN` | `HNSW`, `IVF`, `DISKANN`, `SCANN`, `ANNOY`, `NSG`, `GPU_CAGRA` | append mutable delta or shadow build | query resident delta plus sealed generations | immutable generation |
| `OTHER_HEAVY` | any future non-exact large-fanout family | class-specific | class-specific | immutable generation unless a later canonical file states otherwise |

Any new family admitted to section `18` must bind to one of these maintenance
classes before implementation begins.

## Canonical DML flow

The executor shall process row-changing statements in this order:

1. classify the front-door write shape once per statement or admitted batch
2. resolve statement-local metadata, affected exact index set, and
   maintenance-class binding once per statement or admitted batch
3. resolve old visible row head and construct the new candidate row image
4. evaluate index expressions, predicates, and normalized key bytes once per
   affected index under the statement-local binding
5. produce per-index deltas in the transaction-local delta buffer
6. run same-key suppression rules for admitted exact-family updates
7. run unchanged-key non-indexed update elision before any exact-family
   commit-group enqueue
8. enqueue exact-family deltas for commit-group batch apply
9. enqueue heavy-family mutable-lane deltas for their maintenance class
10. publish the transaction only after all commit-bound exact-family work is
    complete
11. emit maintenance debt for any deferred merge, cleanup, compaction, or
    generation work

## Transaction-local delta buffer

Every transaction that mutates rows shall own a transaction-local index delta
buffer with this logical shape:

```cpp
enum class IndexMaintenanceClass : uint8_t {
  kExact,
  kSummary,
  kBooleanInverted,
  kRankedText,
  kAnn,
  kOtherHeavy,
};

enum class IndexDeltaOp : uint8_t {
  kInsert,
  kDelete,
  kUpdateSameKey,
  kUpdateKeyChange,
  kTombstone,
};

struct IndexRowDelta {
  Uuid logical_index_id;
  IndexMaintenanceClass maintenance_class;
  IndexDeltaOp op;
  RowUuid logical_row_uuid;
  TupleId old_tid;
  TupleId new_tid;
  Bytes normalized_key;
  Bytes normalized_old_key;
  bool predicate_old_matches;
  bool predicate_new_matches;
  bool exact_key_changed;
  bool commit_bound;
  bool same_page_candidate;
};
```

The executor shall not recompute normalized keys during commit-group apply.

## Statement-local metadata hoist

Exact-family maintenance admission shall resolve the following statement-local
metadata once per admitted statement or batch shape, then reuse it for every
affected row:

- affected exact index set
- normalized-key layout and comparator binding
- expression-index and partial-predicate binding
- online-maintenance capture requirements
- include-payload layout needed for exact-family leaf records

When schema epoch, index set, and maintenance state remain unchanged across the
statement or batch, the executor shall not perform full exact-family catalog
discovery, index pointer rebinding, or expression/predicate rebinding per row.

If a schema epoch shift or online-maintenance state transition invalidates the
statement-local binding mid-statement, the executor shall refuse continued
batch admission and restart or fail closed under the governing DDL authority.

## Same-key exact update suppression

### Eligibility

An `UPDATE` qualifies for exact-family same-key suppression for one index only
when all of the following hold:

1. the index maintenance class is `EXACT`
2. `predicate_old_matches == true`
3. `predicate_new_matches == true`
4. `normalized_key == normalized_old_key`
5. the update does not change any include-column payload materialized inside
   that family's exact leaf record
6. the family confirms it can represent the new row head through a redirect or
   anchor update without deleting the old key entry

If any condition fails, the update is treated as an exact key change.

### Canonical exact-head modes

Every exact runtime shall implement one of these logical head modes:

```cpp
enum class ExactHeadMode : uint8_t {
  kDirectVersionRef,
  kPageLocalRedirect,
  kStableHeadAnchor,
};

struct ExactHeadAnchor {
  Bytes normalized_key;
  RowUuid logical_row_uuid;
  ExactHeadMode mode;
  TupleId anchor_tid;
  TupleId head_tid;
  uint64_t head_commit_seq_hint;
};
```

Physical exact backends may encode these logical fields in backend-specific
 page structures, but the logical behavior is fixed by this file.

### Required hybrid rule

ScratchBird Beta 1 shall use this hybrid rule:

1. prefer `kPageLocalRedirect` when the new visible head can remain on the same
   leaf page or bucket group and the redirect-chain hop count stays within
   bounds
2. otherwise use `kStableHeadAnchor`
3. never fall back to delete-plus-insert when the family can still satisfy
   either rule above
4. fall back to delete-plus-insert only when the family proves that neither
   redirect nor stable-head rewrite is safe

### Redirect and anchor limits

Default Beta 1 limits:

| Tunable | Default | Range | Reloadability |
| --- | --- | --- | --- |
| `sb.index.exact_redirect_chain_max_hops` | `8` | `1..32` | reloadable |
| `sb.index.exact_anchor_rewrite_batch` | `64` | `1..1024` | reloadable |
| `sb.index.same_key_probe_budget` | `256` candidates | `64..4096` | reloadable |

### Exact-family update algorithm

```cpp
ExactUpdateDecision planExactUpdate(const IndexRowDelta& delta,
                                    ExactRuntime& runtime) {
  if (!delta.exact_key_changed &&
      delta.predicate_old_matches &&
      delta.predicate_new_matches) {
    if (runtime.canUsePageLocalRedirect(delta) &&
        runtime.currentRedirectDepth(delta.normalized_key) <
            config.exact_redirect_chain_max_hops) {
      return ExactUpdateDecision::PageLocalRedirect(delta);
    }
    if (runtime.canRewriteStableHead(delta)) {
      return ExactUpdateDecision::StableHeadAnchor(delta);
    }
  }
  return ExactUpdateDecision::DeleteInsert(delta);
}
```

### Unique exact-family rule

For unique exact families:

1. uniqueness probe keys are the normalized exact key bytes
2. conflict resolution must inspect the current visible head resolved through
   redirect or stable-head mode
3. superseded versions behind the visible head do not constitute conflicts
4. prepared or limbo versions retain their blocking behavior from section `08`
5. unique enforcement is still commit-bound and cannot move to a deferred lane

## Unchanged-key non-indexed update elision

### Eligibility

An `UPDATE` qualifies for unchanged-key non-indexed elision only when all of
the following hold for the exact families on the target relation:

1. every exact-family normalized key remains unchanged
2. no expression-index value changes
3. no partial-index predicate-membership change
4. no include-column payload materialized in exact-family leaf records changes
5. no active online-maintenance rule requires per-row exact-family capture

### Required behavior

When all eligibility conditions hold, the executor shall:

1. prove the exact-family no-op once from the statement-local metadata binding
2. skip per-row exact-family catalog and pointer walks
3. skip delete-plus-insert exact maintenance for the admitted rows
4. preserve only the row-store update work required by section `34`
5. publish aggregated observability counters if per-row exact-family deltas are
   intentionally elided

### Forbidden behavior

When unchanged-key non-indexed update elision is admitted, the executor shall
not:

1. enumerate the full exact index set per row
2. rebind exact index pointers per row
3. emit per-row exact-family delete-plus-insert maintenance
4. walk per-row exact-family publication surfaces when no exact mutation is
   required

## Commit-group batch apply

### Required behavior

Exact-family deltas shall not be applied one transaction at a time when the
commit queue can safely coalesce them.

The engine shall:

1. gather ready-to-commit transactions in commit-sequence order
2. merge their exact-family deltas by `logical_index_id`
3. subgroup by physical locality such as target leaf, sibling range, or bucket
4. apply the subgroup in one combined job under the commit fence
5. preserve transaction publication order inside the combined apply result

### Defaults

| Tunable | Default | Range | Reloadability |
| --- | --- | --- | --- |
| `sb.index.commit_group_max_transactions` | `64` | `1..512` | reloadable |
| `sb.index.commit_group_max_deltas` | `8192` | `256..65536` | reloadable |
| `sb.index.commit_group_max_wait_us` | `200` | `0..5000` | reloadable |

### Commit-group algorithm

```cpp
void applyCommitGroup(CommitGroup& group) {
  auto merged = mergeByIndexAndLocality(group.transactions);
  for (auto& exact_job : merged.exact_jobs) {
    exact_job.runtime->applyCombined(exact_job.deltas);
  }
  publishTransactionsInOrder(group.transactions);
  for (auto& async_job : merged.async_jobs) {
    maintenanceScheduler().enqueue(async_job.toDebtItem());
  }
}
```

### Failure rule

If any exact-family combined apply fails:

1. the entire commit group fails before publication
2. no transaction in the group becomes committed
3. heavy-family deferred work derived from those transactions is discarded
4. diagnostics must name the failing `logical_index_id`

## Exact cleanup debt and compaction

### Required debt classes

Section `18` contributes these debt classes to section `38`:

- `EXACT_RECLAIM`
- `EXACT_COMPACT`
- `EXACT_POSTING_TRIM`
- `GIN_PENDING_MERGE`
- `RANKED_SEGMENT_MERGE`
- `GENERATION_MERGE`
- `SIDELOG_DRAIN`
- `COLD_PAGE_DELTA_MERGE`
- `HOT_LEAF_RESHAPE`
- `BLOOM_REBUILD`

### Reclaim-driven exact cleanup algorithm

1. sweep or reclaim proof marks a row version reclaim-eligible
2. the owning exact family emits a debt item keyed by `logical_index_id` plus
   physical locality
3. background cleanup removes only entries whose referenced heap version is
   reclaim-eligible
4. page or bucket compaction occurs immediately after safe removals when the
   free-space gain exceeds the configured threshold
5. if structural verification fails, the affected locality is marked
   `repair_required` and destructive cleanup stops

### Defaults

| Tunable | Default | Range | Reloadability |
| --- | --- | --- | --- |
| `sb.index.cleanup_compact_min_free_percent` | `15` | `5..60` | reloadable |
| `sb.index.cleanup_page_scan_batch` | `128` pages | `8..4096` | reloadable |
| `sb.index.cleanup_posting_trim_batch` | `4096` tids | `256..65536` | reloadable |

### Cleanup sample code

```cpp
void runExactCleanup(const DebtItem& item) {
  auto reclaimable = heapReclaimProof(item.heap_versions);
  auto page = exactRuntime(item.logical_index_id).lockLocality(item.locality);
  page.removeReclaimable(reclaimable);
  if (page.freePercent() >= config.cleanup_compact_min_free_percent) {
    page.compact();
  }
  page.verifyOrFenceRepair();
}
```

## Narrow cold-page secondary delta buffer

### Admitted scope

The cold-page delta buffer is admitted only for:

1. `EXACT` maintenance class
2. non-unique secondary indexes
3. `BTREE` and `HASH` exact runtimes
4. operations whose normalized key bytes are fully known at statement time
5. targets whose leaf or bucket is not resident or is explicitly classified
   cold by the buffer policy

The cold-page delta buffer is forbidden for:

1. primary-key or cluster-identity indexes
2. unique or exclusion-enforcing indexes
3. indexes used as immediate uniqueness or foreign-key parent proof
4. any family that cannot merge a delta before answering a read

### Durable delta record

Deferred exact-secondary writes shall persist a durable engine-owned delta row
in `sb_catalog.index_page_delta` with this shape:

| Column | Type | Meaning |
| --- | --- | --- |
| `index_page_delta_uuid` | `cat_uuid` | row identity |
| `logical_index_id` | `cat_uuid` | owning index |
| `target_locality_key` | `cat_binary` | leaf-fence or bucket identity |
| `delta_op` | `cat_identifier` | `INSERT`, `DELETE`, `UPDATE_SAME_KEY`, `UPDATE_KEY_CHANGE` |
| `logical_row_uuid` | `cat_uuid` | owning row |
| `old_tid` | `cat_uint64` nullable | prior row locator |
| `new_tid` | `cat_uint64` nullable | new row locator |
| `normalized_key` | `cat_blob` | post-image key |
| `normalized_old_key` | `cat_blob` nullable | pre-image key |
| `created_xid` | `cat_uint64` | creating transaction |
| `merge_state` | `cat_identifier` | `PENDING`, `MERGING`, `MERGED`, `FAILED_FENCE` |
| `created_at` | `cat_timestamp` | creation time |

### Merge triggers

A cold-page delta must merge when any of the following occur first:

1. the target locality is read
2. the locality becomes resident
3. checkpoint or sweep schedules a `COLD_PAGE_DELTA_MERGE`
4. `sb.index.cold_delta_max_age_ms` is exceeded
5. per-index or global delta backlog exceeds the configured byte limit

### Defaults

| Tunable | Default | Range | Reloadability |
| --- | --- | --- | --- |
| `sb.index.cold_delta_max_bytes_per_index` | `64 MiB` | `8 MiB..512 MiB` | reloadable |
| `sb.index.cold_delta_max_age_ms` | `5000` | `100..60000` | reloadable |
| `sb.index.cold_delta_merge_batch` | `2048` deltas | `128..32768` | reloadable |

## Pending lanes for many-key families

### Boolean inverted

`GIN` and boolean inverted families shall use:

1. a mutable pending list for foreground writes
2. a sealed main structure for read-mostly probing
3. background merge into the main structure when threshold or age rules fire

### Ranked text

Ranked text families shall use:

1. append-only mini-segments for inserts
2. tombstone mini-segments for delete or update compensation
3. background merge into larger immutable ranked segments
4. query-time fan-in across mutable and sealed segments

### Defaults

| Tunable | Default | Range | Reloadability |
| --- | --- | --- | --- |
| `sb.index.boolean_pending_target_bytes` | `32 MiB` | `4 MiB..256 MiB` | reloadable |
| `sb.index.ranked_segment_flush_target_bytes` | `16 MiB` | `1 MiB..128 MiB` | reloadable |
| `sb.index.ranked_segment_query_fan_in_max` | `8` | `2..64` | reloadable |

### Pending-lane sample code

```cpp
void applyTextDelta(const IndexRowDelta& delta, TextRuntime& runtime) {
  if (runtime.maintenanceClass() == IndexMaintenanceClass::kBooleanInverted) {
    runtime.pendingList().append(delta);
    return;
  }
  runtime.mutableSegment().append(delta.toSegmentRecord());
  if (runtime.mutableSegment().bytes() >=
      config.ranked_segment_flush_target_bytes) {
    maintenanceScheduler().enqueue(runtime.sealMutableSegmentDebt());
  }
}
```

## Shadow-build, side-log, validate, and publish

### Durable build rows

Online build and rebuild work shall use these engine-owned catalog rows:

#### `sb_catalog.index_build_plan`

| Column | Type | Meaning |
| --- | --- | --- |
| `index_build_plan_uuid` | `cat_uuid` | plan identity |
| `logical_index_id` | `cat_uuid` | target index |
| `build_reason` | `cat_identifier` | `CREATE`, `REBUILD`, `RELOCATE`, `REPAIR` |
| `build_state` | `cat_identifier` | phase state |
| `shadow_index_uuid` | `cat_uuid` | current shadow instance |
| `baseline_schema_epoch` | `cat_version_u64` | schema binding |
| `build_snapshot_xid` | `cat_uint64` | build snapshot |
| `resume_anchor_row_uuid` | `cat_uuid` nullable | scan resume point |
| `resume_payload_json` | `cat_json` nullable | family-local resume data |
| `is_valid` | `cat_bool` | row validity |

#### `sb_catalog.index_build_event`

| Column | Type | Meaning |
| --- | --- | --- |
| `index_build_event_uuid` | `cat_uuid` | event identity |
| `index_build_plan_uuid` | `cat_uuid` | owning plan |
| `event_seq` | `cat_uint64` | monotonic sequence |
| `phase_from` | `cat_identifier` nullable | old phase |
| `phase_to` | `cat_identifier` | new phase |
| `event_code` | `cat_identifier` nullable | reason |
| `event_time` | `cat_timestamp` | event time |
| `is_valid` | `cat_bool` | row validity |

#### `sb_catalog.index_build_progress`

| Column | Type | Meaning |
| --- | --- | --- |
| `index_build_progress_uuid` | `cat_uuid` | progress identity |
| `index_build_plan_uuid` | `cat_uuid` | owning plan |
| `rows_scanned` | `cat_uint64` | source rows scanned |
| `rows_applied` | `cat_uint64` | rows applied to shadow |
| `side_log_records_applied` | `cat_uint64` | side-log drain count |
| `last_resume_row_uuid` | `cat_uuid` nullable | durable resume anchor |
| `partial_chunk_rewind_required` | `cat_bool` | restart rewind flag |
| `restart_disposition` | `cat_identifier` | `RESUME`, `RESTART_SCAN`, `FAIL_CLOSED` |
| `is_valid` | `cat_bool` | row validity |

#### `sb_catalog.index_build_cutover_guard`

| Column | Type | Meaning |
| --- | --- | --- |
| `index_build_cutover_guard_uuid` | `cat_uuid` | guard identity |
| `index_build_plan_uuid` | `cat_uuid` | owning plan |
| `expected_schema_epoch` | `cat_version_u64` | schema binding |
| `side_log_drained` | `cat_bool` | final drain complete |
| `validation_manifest_hash` | `cat_uint64` | validation summary |
| `guard_state` | `cat_identifier` | `READY`, `BLOCKED`, `FAILED` |
| `checked_at` | `cat_timestamp` | check time |
| `is_valid` | `cat_bool` | row validity |

### Build phases

Required phases:

1. `DRAFTED`
2. `BUILDING`
3. `SIDELOG_ACTIVE`
4. `DRAINING_PERMISSIVE`
5. `DRAINING_FINAL`
6. `VALIDATING`
7. `CUTOVER_PENDING`
8. `PUBLISHED`
9. `PAUSED`
10. `ABORTED_FAIL_CLOSED`

### Online build algorithm

1. create a shadow index in `BUILDING`
2. capture the build snapshot and scan the source rows
3. intercept concurrent writes into a side log keyed by `logical_index_id`
4. bulk build the shadow index from the snapshot
5. enter `DRAINING_PERMISSIVE` and apply side-log work while normal writes
   continue
6. enter `DRAINING_FINAL`, briefly fence the write interception boundary, and
   apply the final side-log tail
7. validate structure, row counts, and uniqueness proof
8. write a durable cutover guard row
9. publish by setting the new index `valid_from_xid` and retiring the old
   version if present
10. retain the retired version until all old snapshots release it

### Online build sample code

```cpp
void runIndexBuild(BuildPlan& plan) {
  auto shadow = catalog.createShadowIndex(plan.logical_index_id);
  bulkBuildShadow(plan.snapshot, shadow);
  applySideLog(plan, DrainMode::kPermissive);
  fenceFinalDrain(plan.logical_index_id);
  applySideLog(plan, DrainMode::kFinalExclusive);
  validateShadow(shadow);
  catalog.writeCutoverGuard(plan);
  catalog.promoteShadowIndex(plan.logical_index_id, shadow);
}
```

## Immutable generation publication for heavy families

### Families

The following families shall publish by immutable generation in Beta 1:

- `BRIN`
- `COLUMNSTORE`
- ranked `INVERTED`
- `SPARSE_INVERTED`
- `SPARSE_WAND`
- `HNSW`
- `IVF`
- `DISKANN`
- `SCANN`
- `ANNOY`
- `NSG`

### Durable generation manifest

Heavy families shall track generations in `sb_catalog.index_generation_manifest`
with this shape:

| Column | Type | Meaning |
| --- | --- | --- |
| `index_generation_manifest_uuid` | `cat_uuid` | row identity |
| `logical_index_id` | `cat_uuid` | owning index |
| `generation_id` | `cat_uint64` | monotonic generation |
| `generation_kind` | `cat_identifier` | `MUTABLE`, `SEALED`, `MERGED`, `TOMBSTONE` |
| `published_at_xid` | `cat_uint64` nullable | publication xid |
| `retired_at_xid` | `cat_uint64` nullable | retirement xid |
| `row_count` | `cat_uint64` | contained rows |
| `byte_count` | `cat_uint64` | size |
| `validation_hash` | `cat_uint64` | integrity summary |
| `manifest_state` | `cat_identifier` | `BUILDING`, `PUBLISHED`, `RETIRED`, `FAILED` |
| `is_valid` | `cat_bool` | row validity |

### Merge rules

1. mutable state seals when byte, row, age, or memory thresholds are crossed
2. a sealed generation is validated before publication
3. a tiny manifest publication row makes the generation visible
4. readers evaluate every published generation plus the current mutable lane
5. merge is debt-driven and merges enough generations to reduce read fan-in to
   the configured bound

### Defaults

| Tunable | Default | Range | Reloadability |
| --- | --- | --- | --- |
| `sb.index.heavy_mutable_flush_target_bytes` | `64 MiB` | `8 MiB..1 GiB` | reloadable |
| `sb.index.heavy_generation_merge_fan_in_target` | `4` | `2..16` | reloadable |
| `sb.index.heavy_generation_query_fan_in_max` | `8` | `2..32` | reloadable |

## Hot-leaf and monotonic-key mitigation

### Required detection metrics

Every exact runtime shall emit:

- rightmost-leaf latch retry count
- split-per-second rate
- median and p99 exact-apply locality depth
- monotonic-key streak length
- batch-apply locality score

### Required mitigation ladder

For a locality classified `HOT_RIGHT_EDGE`, apply this ladder in order:

1. reserve free space on the rightmost leaf according to the configured free
   percent
2. pre-split or extend the right edge if the forecasted insert batch exceeds
   reserved space
3. route the locality through commit-group batch apply
4. if the key is explicitly marked `unordered_synthetic`, apply deterministic
   bucket spreading using the row UUID low bits
5. do not use key spreading for keys with user-visible ordered semantics

### Defaults

| Tunable | Default | Range | Reloadability |
| --- | --- | --- | --- |
| `sb.index.hot_leaf_reserved_free_percent` | `15` | `5..40` | reloadable |
| `sb.index.hot_leaf_retry_threshold_per_sec` | `1024` | `64..65535` | reloadable |
| `sb.index.hot_leaf_presplit_batch_threshold` | `4096` rows | `256..65536` | reloadable |

## Observability

The engine shall expose at minimum:

- same-key suppression hit rate
- redirect versus stable-head selection rate
- average redirect depth
- commit-group size, delta count, and wait time
- exact cleanup backlog bytes and pages
- cold-page delta backlog bytes, age, and failed merges
- pending-list bytes and ranked-text segment fan-in
- build-plan phase, progress, and side-log drain counters
- generation count, merge backlog, and query fan-in
- hot-leaf detection and mitigation counters

## Explicit refusals and non-guarantees

The engine must refuse:

1. deferred uniqueness proof
2. cold-page delta buffering for unique or primary exact indexes
3. exact-family async publication that would let commit succeed first
4. ranked-text or ANN generation publish without a manifest row
5. shadow-build cutover without a durable cutover guard

This file does not require:

1. latch-free exact-tree replacement in Beta 1
2. fractal-tree or message-buffered-tree replacement in Beta 1
3. exact-family visibility from index structures without heap recheck

## Required tests

Minimum Beta 1 proof:

1. same-key exact update keeps one live exact key and resolves the visible head
   through redirect or anchor mode
2. unique exact update suppression still rejects conflicting visible heads
3. commit-group batch apply preserves commit order and fails the entire group on
   exact-apply error
4. reclaim-driven cleanup never deletes an unreclaimable candidate
5. cold-page delta buffering merges before the first read that needs the
   locality
6. GIN and ranked-text pending lanes expose one coherent read view
7. online build survives restart with exactly one legal visible generation after
   cutover
8. immutable heavy-family publication preserves old generations for older
   snapshots
9. hot-leaf mitigation lowers right-edge retry rate on monotonic insert loads

## Cross-section references

- `INDEX_ARCHITECTURE.md`
- `INDEX_CONCURRENCY_AND_VISIBILITY.md`
- `INDEX_MGA_PUBLICATION_AND_RECLAIM.md`
- `INDEX_VERSION_SEMANTICS_AND_DEAD_ENTRY_LIFECYCLE.md`
- `../08_Transaction_Core/MGA_RECORD_STATE_AND_PUBLICATION_MODEL.md`
- `../35_Durability_Crash_Recovery_and_Checkpoint_Model/CHECKPOINT_BOUND_DELTA_RECONCILIATION_AND_MAINTENANCE_MARKERS.md`
- `../37_Statistics_Metadata_and_Schema_DDL/ONLINE_SCHEMA_CHANGE_AND_BACKFILL_MODEL.md`
- `../38_Workload_Governance_and_Parallelism/MAINTENANCE_DEBT_LEDGER_AND_SCHEDULING_MODEL.md`
- `../39_Backup_Restore_and_Bulk_Data_Paths/BULK_INGEST_LANES_AND_SHADOW_LOAD_CUTOVER_MODEL.md`
