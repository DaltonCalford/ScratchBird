# Vectorized, Pipelined, and Intra-Query Parallel Execution Model

Status: reconstructed_required_with_current_substrate

## Purpose

Define the required Beta 1 executor model for:

- vectorized operator execution
- pipelined operator boundaries
- single-node intra-query parallel execution
- exchange, gather, and worker-budget behavior

This file exists so implementation agents can land donor-competitive executor
behavior without guessing about batch size, worker roles, or fallback
conditions.

## Scope

This file owns:

- vector batch contract
- pipeline and exchange contract
- single-node worker admission and leader/worker roles
- vectorized operator coverage for benchmark-governed paths
- morsel scheduling and bounded work stealing
- runtime-plan evidence and test obligations

This file does not replace:

- section `33` memory ownership and grant authority
- section `03` NUMA or locality authority
- section `36` path enumeration legality
- any distributed or cross-machine scheduler model

## Hard invariants

1. This file authorizes single-node intra-query parallel execution only. It
   does not authorize distributed execution.
2. Vectorized execution may change physical batching, not result semantics.
3. Parallel execution must consume explicit worker and leader memory grants.
4. Work stealing is allowed only inside the same query stage and memory domain.
5. Any unsupported type or operator combination shall fail closed to a named
   row-mode fallback path, not to hidden ad hoc execution.

## Canonical vector batch

Every vectorized operator shall consume and produce this logical batch shape:

```cpp
struct VectorBatch {
  uint16_t column_count;
  uint16_t row_count;
  uint16_t capacity;
  SelectionVector* selection;
  vector<ColumnVectorRef> columns;
};
```

Default batch-size tunable:

| Tunable | Default | Range | Reloadability |
| --- | --- | --- | --- |
| `sb.executor.vector_batch_rows` | `2048` | `256..8192` | reloadable |

All vectorized operators shall preserve null semantics, collation semantics,
and stable row identity exactly as row-mode execution would.

## Canonical pipeline units

The executor shall reason in these units:

| Unit | Meaning |
| --- | --- |
| `SOURCE_PIPELINE` | scan or material source producing batches |
| `INTERMEDIATE_PIPELINE` | filter, projection, join probe, local aggregate, and similar transforms |
| `SINK_PIPELINE` | final aggregate, final sort merge, final window emission, or result sink |
| `EXCHANGE_BOUNDARY` | gather, gather-merge, repartition, or local handoff between worker stages |
| `MORSEL` | bounded source work unit assigned to one worker |

## Vectorized operator coverage

Before parity closure, the executor shall provide vectorized paths for the
benchmark-governed operators below whenever their input types are admitted:

- scan
- filter
- projection
- hash join build and probe
- merge join compare and emit
- hash aggregate
- distinct
- sort run generation
- window partition ranking and frame-local operations

### Admitted type classes

First-wave vectorized coverage shall include:

- fixed-width numeric
- date/time scalar
- boolean
- dictionary-backed text
- nullable variants of the above

### Required fallback naming

If a vectorized path is not legal, the runtime plan shall explicitly name the
fallback family:

- `ROW_MODE_SCAN`
- `ROW_MODE_JOIN`
- `ROW_MODE_AGG`
- `ROW_MODE_SORT`
- `ROW_MODE_WINDOW`

## Morsel scheduling

### Canonical morsel

```cpp
struct Morsel {
  Uuid stage_uuid;
  Uuid relation_uuid;
  GPID start_page;
  uint32_t page_count;
  uint64_t estimated_rows;
  uint16_t preferred_partition;
};
```

### Required behavior

1. source pipelines shall split scanable input into bounded morsels
2. each worker shall request a morsel from the stage queue
3. a worker shall prefer morsels whose `preferred_partition` matches the
   worker's locality assignment
4. work stealing is legal only after local morsels are exhausted
5. a stolen morsel must retain its original query-stage ownership and memory
   charging

## Single-node intra-query parallel execution

### Canonical worker roles

| Role | Responsibility |
| --- | --- |
| `LEADER` | plan setup, stage orchestration, optional participation in worker work |
| `WORKER` | execute assigned morsels or exchange partitions |
| `GATHER` | merge ordered or unordered worker output for the parent stage |

### Worker-count contract

The runtime plan shall carry:

- `planned_worker_count`
- `max_worker_count`
- `leader_participates`
- `exchange_mode`

Actual worker count shall be:

```text
actual_worker_count =
  min(
    planned_worker_count,
    max_worker_count,
    cpu_quota_cap,
    memory_grant_cap
  )
```

No parallel path may execute with hidden extra workers beyond the admitted
count.

## Exchange modes

The executor shall support these single-node exchange modes:

- `GATHER`
- `GATHER_MERGE`
- `REPARTITION_HASH`
- `LOCAL_BROADCAST`

### Required semantics

1. `GATHER` preserves row multiplicity but not worker-local order
2. `GATHER_MERGE` preserves a global order proven by the child worker outputs
3. `REPARTITION_HASH` hashes rows by the declared partition key before worker
   handoff
4. `LOCAL_BROADCAST` duplicates a bounded build-side input to every consumer
   worker only when memory admission allows it

## Parallel operator contracts

### Parallel scan

1. parallel scan assigns morsels to workers
2. each worker emits `VectorBatch` output
3. when runtime filters are present, workers may apply them before batch
   emission

### Parallel hash join

1. build input may be repartitioned by hash key
2. each worker builds local hash partitions or one shared partition group,
   depending on the emitted plan contract
3. probe workers may only consult hash partitions that are fully published for
   the stage

### Parallel aggregate and distinct

1. workers shall perform partial aggregation or distinct compaction locally
2. final aggregation or distinct merge shall occur at a sink pipeline after an
   exchange boundary

### Parallel sort

1. workers shall produce independently sorted runs
2. final ordered output shall use `GATHER_MERGE`
3. spill, if needed, shall remain bounded under section `12`

### Parallel window

1. parallel window execution is legal only when partitions can be assigned to
   workers without splitting one logical partition across workers
2. if partition independence is not proven, the planner shall reject the
   parallel window candidate

## Memory and locality binding

### Required worker charging

For any parallel stage, the runtime shall charge at least:

- leader reservation
- worker-local batch buffers
- exchange buffers
- worker-local hash, sort, aggregate, or window state

No worker-local memory may be hidden inside generic process heap.

### Required locality rules

1. workers shall prefer local morsels first
2. exchange buffers shall record their owning worker or partition
3. a gather or merge stage shall publish cross-partition traffic if it occurs
4. a parallel path on a locality-sensitive host shall emit locality metrics even
   when the path falls back to serial execution

## Runtime-plan fields

- `vectorized_enabled`
- `vector_batch_rows`
- `fallback_row_mode_reason`
- `planned_worker_count`
- `actual_worker_count`
- `leader_participates`
- `exchange_mode`
- `morsel_count`
- `locality_preferred`
- `work_steal_count`
- `cross_partition_transfer_bytes`

## Structured refusal reasons

- `P23_VECTOR_TYPE_UNSUPPORTED`
- `P23_VECTOR_OPERATOR_UNSUPPORTED`
- `P23_PARALLEL_UNBUDGETED`
- `P23_PARALLEL_LOCALITY_UNBOUND`
- `P23_WINDOW_PARTITION_NOT_SPLIT_SAFE`
- `P23_ORDERED_GATHER_UNSUPPORTED`

## Required tests

1. admitted benchmark-governed scan, join, aggregate, sort, and window paths
   run through vectorized batches instead of row-at-a-time fallback
2. parallel paths consume only admitted worker count and explicit worker memory
3. work stealing occurs only after local morsels are exhausted
4. `GATHER_MERGE` preserves global order for sorted worker outputs
5. unsupported type or operator combinations preserve an explicit row-mode
   fallback reason

## Cross-section references

- `ACCESS_PATH_ORDERING_AND_UPPER_STAGE_PLANNING.md`
- `JOIN_SEARCH_AND_METHOD_ENUMERATION.md`
- `../33_Memory_Management/MEMORY_GRANT_FEEDBACK_AND_OPERATOR_RESERVATION_MODEL.md`
- `../03_Disk_Allocator_and_Free_Space/NUMA_LOCALITY_AND_FRAME_OWNERSHIP.md`
