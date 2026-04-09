# Memoize, Incremental Sort, Runtime Filter, and Adaptive Join Model

Status: reconstructed_required_with_current_substrate

## Purpose

Define the required Beta 1 upper-stage planning model for donor-proven planning
and adaptive techniques that materially affect benchmark speed:

- memoized parameterized rescans
- incremental sort
- runtime filters and dynamic pruning
- bounded adaptive build-side selection for legal hash joins

This file is intentionally prescriptive so implementation agents do not need to
guess when each technique is legal or how it must surface in runtime plans.

## Scope

This file owns:

- memoize legality, costing, identity, and runtime-plan fields
- incremental sort legality, costing, and execution contract
- runtime-filter production and consumption contracts
- bounded adaptive join-side selection rules
- refusal identities, observability, and test obligations

This file does not replace:

- section `23` executor pipeline and worker contract
- section `33` memory-grant and spill authority
- section `18` secondary access legality
- section `08` MGA visibility truth

## Hard invariants

1. No adaptive behavior may change query semantics.
2. No runtime filter may reject a row unless its filter class is proven safe for
   that row source.
3. Memoized results are cacheable operator state, not visibility truth.
4. Incremental sort may reuse delivered prefix order only when the prefix proof
   is explicit in the child path.
5. Adaptive join behavior in this file is bounded. It is not license for silent
   global runtime replanning.

## Canonical candidate wrappers

The planner shall preserve these as distinct candidate identities:

| Candidate | Purpose |
| --- | --- |
| `MEMOIZE_WRAP` | cache repeated parameterized inner results |
| `INCREMENTAL_SORT` | sort only the unsatisfied suffix of an already ordered input |
| `RUNTIME_FILTER_BUILD` | build-side filter production |
| `RUNTIME_FILTER_CONSUMER` | scan or probe-side filter consumption |
| `ADAPTIVE_BUILD_SIDE_HASH_JOIN` | choose build side at runtime from a bounded reversible pair |

## Memoize

### Eligibility

`MEMOIZE_WRAP` is legal only when all of the following hold:

1. the wrapped child path is parameterized by one or more outer values
2. the wrapped child is deterministic for a fixed parameter vector under the
   current snapshot
3. the wrapped child does not depend on volatile expressions, random order, or
   side effects
4. the planner estimates repeated probe keys or repeated parameter vectors
5. the estimated memoized result fits within the admitted operator memory
   budget or a bounded spill/workfile path exists

### Canonical identity

Memoize identity shall include:

- normalized child plan signature
- schema epoch
- parameter type vector
- parameter value vector
- collation and comparison profile
- privilege and security context
- workload class

No memoized entry may be reused across a change in any identity field.

### Required behavior

For `MEMOIZE_WRAP`, the runtime shall:

1. hash the parameter vector into the memoize table
2. on hit, replay the cached result batches or tuple references
3. on miss, execute the child path once, cache the result, then replay it
4. evict using bounded LRU or clock policy within the admitted memory grant
5. preserve hit, miss, eviction, and bypass counters in runtime-plan evidence

### Canonical entry shape

```cpp
struct MemoizeEntry {
  Bytes parameter_key;
  uint64_t row_count;
  uint64_t cached_bytes;
  bool spill_backed;
  vector<ResultBatchRef> result_batches;
};
```

### Refusal reasons

- `P36_MEMOIZE_VOLATILE_CHILD`
- `P36_MEMOIZE_UNBUDGETED`
- `P36_MEMOIZE_LOW_REUSE_EXPECTED`

## Incremental sort

### Eligibility

`INCREMENTAL_SORT` is legal only when all of the following hold:

1. the child path publishes an ordered prefix proof
2. the requested order list extends that prefix
3. the child order is stable enough to delimit consecutive prefix groups
4. the remaining suffix sort fits within the admitted grant or a bounded spill
   path exists

### Canonical order proof

The child path shall publish:

- `delivered_order_keys`
- `delivered_prefix_length`
- `null_order_profile`
- `collation_profile`

If any one of these is missing or mismatched, the planner shall cost a full
sort instead of `INCREMENTAL_SORT`.

### Required behavior

For `INCREMENTAL_SORT`, the runtime shall:

1. read rows from the child in delivered-prefix order
2. accumulate the current prefix group
3. sort only the suffix keys within that group
4. emit the fully ordered group
5. release group memory before the next prefix group begins

### Canonical execution sketch

```cpp
while (readChild(row)) {
  if (!samePrefix(current_group.prefix, row, prefix_key_count)) {
    sortBySuffix(current_group.rows, suffix_keys);
    emit(current_group.rows);
    current_group.clear();
  }
  current_group.add(row);
}
sortBySuffix(current_group.rows, suffix_keys);
emit(current_group.rows);
```

### Refusal reasons

- `P36_INCREMENTAL_SORT_NO_PREFIX_PROOF`
- `P36_INCREMENTAL_SORT_COLLATION_MISMATCH`
- `P36_INCREMENTAL_SORT_UNBUDGETED`

## Runtime filters and dynamic pruning

### Filter classes

The planner may emit only these runtime-filter classes in Beta 1:

- `EXACT_KEY_SET`
- `MINMAX_RANGE`
- `LOSSY_BLOOM`

### Producer legality

A runtime-filter producer is legal only when:

1. the join build side proves a key domain on an equality or range-compatible
   predicate
2. the filter keys are deterministic and type-compatible with the consumer
3. the build side can produce the filter before the consumer has completed all
   relevant reads

### Consumer legality

A runtime-filter consumer is legal only when:

1. the filter keys map directly to:
   - scan predicates
   - partition pruning keys
   - ordered exact probe keys
2. the filter does not target a null-preserving outer side in a way that would
   change semantics
3. lossy filters preserve a residual recheck above the consumer

### Required behavior

The planner and runtime shall:

1. emit producer and consumer identities explicitly in the runtime plan
2. delay consumer activation until the producer has published a usable filter
3. allow dynamic partition pruning when the filter targets partition keys
4. retain residual recheck for `LOSSY_BLOOM`
5. count rows, partitions, or probe attempts pruned by the filter

### Canonical filter packet

```cpp
struct RuntimeFilterPacket {
  Uuid filter_uuid;
  RuntimeFilterClass filter_class;
  vector<NormalizedKeyBytes> exact_keys;
  optional<KeyRange> minmax_range;
  optional<BloomPayload> bloom;
  uint64_t producer_row_count;
  bool lossy;
};
```

### Refusal reasons

- `P36_RUNTIME_FILTER_OUTER_SEMANTICS_UNSAFE`
- `P36_RUNTIME_FILTER_TYPE_MISMATCH`
- `P36_RUNTIME_FILTER_LATE_FOR_CONSUMER`
- `P36_RUNTIME_FILTER_UNBUDGETED`

## Bounded adaptive join-side selection

### Scope

This file permits only bounded adaptive selection of the build side for legal
hash joins. It does not authorize arbitrary runtime join reordering.

### Eligibility

`ADAPTIVE_BUILD_SIDE_HASH_JOIN` is legal only when all of the following hold:

1. the join is an `INNER` equi-join
2. both left-build/right-probe and right-build/left-probe orientations are
   legal and semantically equivalent
3. neither orientation depends on preserved input order
4. both orientations have complete runtime support
5. the planner emits both orientations as a bounded reversible pair

### Required behavior

The planner shall emit:

- `planned_build_side`
- `adaptive_alternative_build_side`
- `adaptive_probe_sample_rows`
- `adaptive_flip_ratio_threshold`

The runtime shall:

1. read startup batches from both sides up to `adaptive_probe_sample_rows`
2. compare the observed row and byte footprint
3. flip build side only when the observed smaller side beats the planned build
   side by at least `adaptive_flip_ratio_threshold`
4. freeze the choice before hash-table materialization begins
5. publish whether the adaptive flip was taken

### Forbidden behavior

The runtime shall not:

- flip build side after hash table build has begun
- apply this adaptive rule to outer, anti, semi, or lateral joins
- change join order beyond the emitted reversible pair

### Refusal reasons

- `P36_ADAPTIVE_JOIN_NOT_REVERSIBLE`
- `P36_ADAPTIVE_JOIN_ORDER_SENSITIVE`
- `P36_ADAPTIVE_JOIN_RUNTIME_UNSUPPORTED`

## Costing additions

```text
cost_memoize =
  cost_child
  - C_reuse_gain * expected_repeat_probes
  + C_cache_build * expected_miss_count
  + C_cache_memory * memoized_bytes

cost_incremental_sort =
  cost_child
  + C_group_detect * rows
  + C_suffix_sort * rows_per_prefix_group
  - C_full_sort_saved * rows

cost_runtime_filter =
  cost_child
  - C_rows_pruned * estimated_pruned_rows
  - C_partitions_pruned * estimated_pruned_partitions
  + C_filter_build * producer_rows

cost_adaptive_hash_join =
  min(planned_orientation_cost, alternative_orientation_cost)
  + C_adaptive_sampling
```

The planner shall not hide these behaviors inside generic join, sort, or
materialize costs.

## Required runtime-plan fields

- `memoize_enabled`
- `memoize_expected_reuse`
- `memoize_entry_cap`
- `incremental_sort_enabled`
- `delivered_prefix_length`
- `runtime_filter_producer_count`
- `runtime_filter_consumer_count`
- `runtime_filter_classes`
- `adaptive_join_enabled`
- `adaptive_join_flip_taken`
- `adaptive_join_sample_rows`

## Required tests

1. repeated parameterized rescans choose `MEMOIZE_WRAP` and preserve cache hits
   only within identity boundaries
2. a child path with a delivered order prefix chooses `INCREMENTAL_SORT` rather
   than full sort
3. runtime filters prune probe or scan work while preserving correctness and
   residual recheck for lossy filters
4. bounded adaptive hash join flips build side only for legal reversible inner
   joins and only before build begins
5. planner trace preserves the exact refusal reason when each candidate is not
   legal

## Cross-section references

- `PLANNER_STRATEGY_AND_PLAN_STABILITY.md`
- `OPTIMIZER_CANDIDATE_BUNDLE_AND_ACCESS_PATH_ANNOTATION_PIPELINE.md`
- `../23_SBLR_VM_Compiler_and_Executor/INDEX_FAMILY_ACCESS_PATH_AND_RUNTIMEPLAN_INTEGRATION.md`
- `../33_Memory_Management/MEMORY_GRANT_FEEDBACK_AND_OPERATOR_RESERVATION_MODEL.md`
