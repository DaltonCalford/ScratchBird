# Secondary Access Locality, Pushdown, and Covering Execution Model

Status: reconstructed_required_with_current_substrate

## Purpose

Define the required Beta 1 locality and covering-read behavior for ordered
exact secondary access so ScratchBird can match donor engines on secondary
lookup, range scan, and indexed join probe workloads without changing MGA
truth.

This file turns donor-proven `MRR`, `ICP`, `BKA`, and index-only techniques
into ScratchBird-native canonical behavior.

## Scope

This file owns:

- ordered-exact residual predicate pushdown
- rowid-ordered heap fetch for secondary access
- batched indexed join probes
- covering and index-only execution legality
- visibility-summary requirements for index-only execution
- required runtime-plan evidence, observability, refusal rules, and tests

This file does not replace:

- section `08` MGA visibility truth
- section `18` family metrics and maintenance policy
- section `23` upper-stage execution contracts
- section `33` memory-grant and spill authority

## Hard invariants

1. Secondary access remains subordinate to MGA visibility truth.
2. No pushdown or locality optimization may make an invisible row visible.
3. No covering path may skip heap or primary-storage validation unless the
   canonical visibility-summary proof is present and current.
4. `MRR` and `BKA` may change physical fetch order. They may not change query
   result semantics.
5. `ICP` may reject candidate tuples early. It may not evaluate volatile,
   side-effecting, or heap-only predicates inside secondary access.

## Canonical secondary-access path families

Every admitted ordered exact family that supports the corresponding capability
shall expose these path families:

| Path family | Purpose |
| --- | --- |
| `ORDERED_EXACT_SCAN` | ordinary secondary lookup or range scan with heap fetch |
| `ORDERED_EXACT_ICP_SCAN` | secondary lookup or range scan with residual predicate pushdown before heap fetch |
| `ORDERED_EXACT_MRR_SCAN` | secondary scan with rowid-ordered heap fetch |
| `ORDERED_EXACT_BKA_PROBE` | batched indexed join probe using ordered-exact access |
| `ORDERED_EXACT_INDEX_ONLY_SCAN` | covering read that avoids heap fetch under visibility-summary proof |

The planner shall preserve these as distinct path identities when their cost,
heap-touch count, or order-delivery behavior differs materially.

## Capability object

Every ordered exact family shall publish these additional capability fields:

- `supports_residual_predicate_pushdown`
- `supports_rowid_ordered_fetch`
- `supports_batched_key_access`
- `supports_covering_payload`
- `supports_visibility_summary`
- `rowid_order_kind`
- `max_pushdown_qual_count`

`rowid_order_kind` values:

- `GPID_TID`
- `PRIMARY_KEY`
- `FAMILY_NATIVE_CLUSTER_ORDER`

If a family does not publish a capability as supported, the corresponding path
family is illegal and shall emit a structured refusal.

## Residual predicate pushdown (`ICP`)

### Eligibility

`ORDERED_EXACT_ICP_SCAN` is legal only when all of the following hold:

1. the family publishes `supports_residual_predicate_pushdown = true`
2. each pushed predicate depends only on:
   - key columns
   - included payload columns
   - persisted expression-index payload that is already stored in the family
3. each pushed predicate is deterministic and non-volatile
4. each pushed predicate can execute without:
   - heap-only columns
   - external row reconstruction
   - side effects
   - user-defined runtime callbacks that are not certified pushdown-safe
5. the pushed predicate count does not exceed `max_pushdown_qual_count`

### Required behavior

For `ORDERED_EXACT_ICP_SCAN`, the runtime shall:

1. descend or iterate the index exactly as an ordinary ordered exact scan would
2. evaluate pushed predicates against index-resident material before emitting a
   heap fetch request
3. suppress heap fetch for candidates rejected by the pushed predicates
4. retain non-pushable residual predicates as ordinary upper-stage rechecks
5. retain MGA visibility recheck unless the path is also admitted as
   `ORDERED_EXACT_INDEX_ONLY_SCAN`

### Forbidden behavior

The runtime shall not:

- push a predicate that depends on heap-only data
- push volatile or exception-sensitive predicates
- claim full predicate evaluation in the runtime plan if any residual qual is
  still pending above the access node

## Rowid-ordered fetch (`MRR`)

### Eligibility

`ORDERED_EXACT_MRR_SCAN` is legal only when all of the following hold:

1. the family publishes `supports_rowid_ordered_fetch = true`
2. the access path still requires heap or primary-row fetch
3. the parent stage does not require preservation of index tuple arrival order
   unless an explicit reordering buffer is present above the access node
4. estimated candidate rows exceed the configured `mrr_min_candidate_rows`
5. the rowid buffer fits the current operator memory admission or a bounded
   spill/workfile path exists

### Required behavior

For `ORDERED_EXACT_MRR_SCAN`, the runtime shall:

1. accumulate candidate row references in a bounded buffer
2. sort or group the buffered references by `rowid_order_kind`
3. fetch heap or primary rows using that reordered reference list
4. emit rows in semantic query order:
   - reordered physical fetch order is allowed when the parent does not require
     stable index order
   - if ordered output is required, a semantic reorder layer must restore the
     requested output order
5. publish the physical reorder burden in the runtime plan

### Canonical row-reference shape

```cpp
struct RowFetchRef {
  Bytes index_key;
  GPID heap_page;
  TupleId tid;
  uint64_t original_arrival_ordinal;
};
```

Physical families may encode row references differently, but the logical
behavior is fixed by this file.

## Batched key access joins (`BKA`)

### Eligibility

`ORDERED_EXACT_BKA_PROBE` is legal only when all of the following hold:

1. the inner or probe side of the join is an ordered exact family
2. the family publishes `supports_batched_key_access = true`
3. the join predicate is exact equality on the searchable key prefix
4. join semantics are batch-safe:
   - `INNER`
   - `SEMI`
   - `ANTI` only when the planner proves early-exit semantics are preserved
5. the outer key batch fits the current operator memory admission or a bounded
   spill path exists

### Required behavior

For `ORDERED_EXACT_BKA_PROBE`, the runtime shall:

1. collect outer-side join keys into a bounded batch
2. normalize the keys once per batch
3. deduplicate batch keys only when the join multiplicity semantics are
   preserved by separate match accounting
4. probe the inner family in batch mode
5. when the inner family also supports `MRR`, feed the resulting row
   references through rowid-ordered fetch
6. restore outer-row multiplicity and join output order semantics after batched
   probe completion

### Forbidden behavior

The runtime shall not:

- collapse duplicate outer keys without separate multiplicity replay
- silently fall back to row-at-a-time inner probing after a `BKA` plan was
  chosen, unless an explicit runtime refusal reason is emitted

## Covering and index-only execution

### Covering eligibility

A secondary path is covering only when all projected, filtered, join, grouping,
ordering, and distinct columns required above the access node are available from
the index-resident key, included payload, or persisted expression payload.

Covering alone is not enough for index-only execution.

### Visibility-summary requirement

`ORDERED_EXACT_INDEX_ONLY_SCAN` is legal only when all of the following hold:

1. the family publishes:
   - `supports_covering_payload = true`
   - `supports_visibility_summary = true`
2. the path is covering
3. the visibility-summary surface proves the addressed heap or primary-storage
   page is visible to the current snapshot without consulting the heap
4. no contradictory reclaim, stale-entry, or maintenance-debt signal marks the
   page as requiring heap confirmation

### Canonical visibility-summary shape

```cpp
struct VisibilitySummaryEntry {
  GPID heap_page;
  uint64_t visibility_epoch;
  uint64_t all_visible_floor_commit_seq;
  bool all_visible_for_newer_snapshots;
  bool heap_recheck_required;
};
```

Logical rules:

1. any insert, update, delete, or page-local visibility uncertainty shall set
   `heap_recheck_required = true`
2. only sweep or another visibility-certifying maintenance action may restore
   `all_visible_for_newer_snapshots = true`
3. an index-only path shall fail closed to ordinary heap fetch if the summary
   entry is missing, stale, or marks heap recheck as required

### Required behavior

For `ORDERED_EXACT_INDEX_ONLY_SCAN`, the runtime shall:

1. read key and payload directly from the family structure
2. consult the visibility summary for each addressed heap or primary page
3. skip heap fetch only when the summary says heap recheck is not required
4. fall back to ordinary heap fetch on any uncertainty

`HASH` is not first-wave index-only.

## Costing additions

The ordered exact cost model shall expose at least these additional terms:

```text
cost_icp = cost_secondary_lookup
         - C_heap_avoided * icp_rejected_fraction
         + C_icp_eval * pushed_qual_count

cost_mrr = cost_secondary_lookup
         - C_random_heap_saved * mrr_locality_gain
         + C_ref_buffer * buffered_ref_rows
         + C_ref_sort * ref_sort_rows

cost_bka = cost_nested_loop_probe
         - C_batch_probe_gain * batch_key_count
         - C_mrr_gain * mrr_locality_gain
         + C_batch_build * batch_key_count

cost_index_only = cost_secondary_lookup
                - C_heap_avoided * visibility_summary_hit_fraction
                + C_vis_summary * visibility_summary_checks
```

The planner shall not hide `MRR`, `ICP`, `BKA`, or index-only benefits inside a
single generic secondary-lookup constant.

## Runtime-plan fields

Every chosen path above shall emit these fields:

- `pushdown_predicate_count`
- `pushdown_predicate_classes`
- `mrr_enabled`
- `mrr_buffer_rows`
- `mrr_rowid_order_kind`
- `bka_enabled`
- `bka_batch_rows`
- `covering_enabled`
- `index_only_enabled`
- `visibility_summary_required`
- `heap_fetch_required_fraction_est`

## Structured refusal reasons

The planner and runtime shall preserve these refusal identities:

- `P18_ICP_HEAP_ONLY_PREDICATE`
- `P18_ICP_VOLATILE_PREDICATE`
- `P18_MRR_ORDER_REQUIRED`
- `P18_MRR_BUFFER_UNBUDGETED`
- `P18_BKA_MULTIPLICITY_UNSAFE`
- `P18_INDEX_ONLY_NOT_COVERING`
- `P18_INDEX_ONLY_VISIBILITY_UNPROVEN`

## Required tests

1. a range scan with residual predicates rejects heap fetch for tuples filtered
   by `ICP`
2. `MRR` fetches heap or primary rows in rowid order while preserving query
   semantics
3. `BKA` batches indexed join probes and preserves duplicate outer-row
   multiplicity
4. index-only execution falls back cleanly when visibility-summary proof is
   missing or invalidated
5. planner traces preserve the exact refusal reason when `ICP`, `MRR`, `BKA`,
   or index-only are not legal

## Cross-section references

- `ORDERED_EXACT_AND_RANGE_PLANNER_SPEC.md`
- `INDEX_PLANNER_PATH_TAXONOMY_AND_EXACTNESS.md`
- `../34_Table_Storage_and_Access_Methods/BTREE_AND_SECONDARY_INDEX_ACCESS_METHODS.md`
- `../33_Memory_Management/MEMORY_GRANT_FEEDBACK_AND_OPERATOR_RESERVATION_MODEL.md`
