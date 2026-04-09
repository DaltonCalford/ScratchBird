# Bulk Ingest Lanes and Shadow Load Cutover Model

Status: reconstructed_required_with_current_substrate

## Purpose

Define the Beta 1 bulk-ingest model for ScratchBird so large loads use
deliberate lanes instead of paying retail DML cost for every row.

## Scope

This file owns:

- the three canonical bulk-ingest lanes
- lane-classification rules
- front-door ingest-lane classification across `COPY`, multi-row `VALUES`,
  client-batched insert, and `INSERT ... SELECT`
- sort and build requirements for exact families
- heavy-family generation build during ingest
- shadow-load and cutover rules for transformative loads

## Hard invariants

1. Every ingest lane remains transaction-scoped.
2. Every inserted row still receives lineage, UUID identity, and MGA ownership.
3. Bulk ingest does not bypass checksum, forced-write, or schema-epoch rules.
4. Bulk ingest does not invent donor `NOLOGGING` or relaxed durability modes.

## Canonical ingest lanes

| Lane | Use case | Required behavior |
| --- | --- | --- |
| `RETAIL_MICRO_BATCH` | ordinary OLTP insert streams | group small inserts, but use the ordinary DML path |
| `SORTED_EXACT_BULK` | large additive loads into existing shape | sort by exact-key locality and bulk-build exact structures |
| `SHADOW_LOAD_CUTOVER` | table-scale replacement, repartitioning, or transformative load | build into a shadow target and publish at cutover |

## Lane selection rules

1. classify the front-door row source before the first row is written
2. use `RETAIL_MICRO_BATCH` when the admitted row source is below
   `sb.bulk.sorted_exact_min_rows`
3. use `SORTED_EXACT_BULK` when the load is additive, schema-compatible, and
   the admitted row source meets the sorted threshold
4. use `SHADOW_LOAD_CUTOVER` when the load changes partitioning, storage
   layout, or requires a replacement target

Defaults:

| Tunable | Default | Range | Reloadability |
| --- | --- | --- | --- |
| `sb.bulk.sorted_exact_min_rows` | `100000` | `1000..10000000` | reloadable |
| `sb.bulk.micro_batch_target_rows` | `2048` | `64..65536` | reloadable |
| `sb.bulk.shadow_cutover_validate_sample_rows` | `10000` | `100..1000000` | reloadable |

### Front-door classification surfaces

The ingest classifier shall evaluate at least these row-source shapes:

- `COPY FROM`
- client-visible multi-row `INSERT ... VALUES`
- driver-lowered batched insert equivalents
- `INSERT ... SELECT`
- CTAS-like additive row-production paths admitted by later canonical files

No row source that is already batch-shaped may be forced through singleton
retail insertion when `RETAIL_MICRO_BATCH` or `SORTED_EXACT_BULK` can admit it
without violating correctness, transform, or cutover rules.

## Retail micro-batch

Required behavior:

1. batch inserts in memory up to the configured row target
2. assign row UUIDs and lineage fields before any index work
3. preserve the batch shape that arrived from the front-door classifier
4. request ahead-of-demand allocator or filespace growth reservation when the
   current tablespace free window is insufficient for the admitted batch
5. route row-store writes through the section `34` heap multi-insert path
6. route exact families through commit-group batch apply
7. route heavy families through their mutable lanes

## Sorted exact bulk

Required behavior:

1. parse and normalize rows into staging runs
2. sort staging runs by the dominant exact-key locality
3. acquire a coarse allocator or filespace growth reservation for the current
   run before heap writeback begins
4. write heap rows in locality-preserving batches through the section `34`
   multi-insert path
5. build exact families from the sorted runs using bulk-build paths
6. build heavy families as immutable generations
7. publish the batch transaction only after exact structures and catalog state
   are durable

`SORTED_EXACT_BULK` is not limited to `COPY FROM`. It shall be admitted for any
front-door row source that satisfies the additive and schema-compatibility
rules, including `INSERT ... SELECT` and large driver-batched insert streams.

### Sorted exact bulk sample code

```cpp
void runSortedExactBulk(BulkPlan& plan) {
  auto runs = stageAndSort(plan.input, plan.primary_exact_key);
  for (auto& run : runs) {
    writeHeapRun(run);
    buildExactRun(run);
    appendHeavyFamilyDelta(run);
  }
  sealAndPublishHeavyGenerations(plan);
  commitBulkPlan(plan);
}
```

## Shadow-load and cutover

Required behavior:

1. create a shadow target with its own object UUID and cutover plan
2. load rows into the shadow target using sorted exact bulk rules
3. build indexes on the shadow target using section `18` shadow-build rules
4. validate row counts, checksum summaries, and key constraints
5. publish by swapping the live object binding at commit-bound cutover
6. retain the prior target for rollback-safe retirement until old snapshots and
   operator hold rules release it

### Durable plan rows

Transformative bulk loads shall use:

- `sb_catalog.bulk_load_plan`
- `sb_catalog.bulk_load_event`
- `sb_catalog.bulk_load_progress`
- `sb_catalog.bulk_load_cutover_guard`

These rows mirror the phase and progress model used by schema change and index
build plans.

## Heavy-family build during bulk ingest

During `SORTED_EXACT_BULK` and `SHADOW_LOAD_CUTOVER`:

1. exact families build from sorted runs
2. summary, ranked text, and ANN families build sealed generations
3. no heavy family is forced through retail one-row-at-a-time maintenance

## Required tests

1. sorted exact bulk produces the same visible rows as retail insert
2. shadow-load cutover publishes exactly one live target
3. bulk ingest still stamps lineage and UUID identity on every row
4. bulk ingest failure before commit leaves no committed visibility

## Cross-section references

- `BULK_IMPORT_EXPORT_AND_COPY_SURFACES.md`
- `../18_Index_Framework/DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md`
- `../37_Statistics_Metadata_and_Schema_DDL/ONLINE_SCHEMA_CHANGE_AND_BACKFILL_MODEL.md`
