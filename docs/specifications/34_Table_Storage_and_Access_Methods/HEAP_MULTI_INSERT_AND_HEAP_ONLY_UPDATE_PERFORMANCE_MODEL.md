# Heap Multi Insert and Heap Only Update Performance Model

Status: reconstructed_required_with_current_substrate

## Purpose

Define the required Beta 1 row-store performance model for ScratchBird heap
storage so front-door batched insert workloads and non-indexed updates do not
degrade into repeated singleton heap or index maintenance work.

This file translates the accepted benchmark-regression audit into authoritative
row-store behavior.

## Scope

This file owns:

- heap multi-insert and page-coalesced insert behavior
- statement-local row-layout and metadata hoisting for admitted insert batches
- heap-only or stable-head-preserving update behavior for non-indexed updates
- locality-preserving append and page reservation rules for bulk row-store
  writes
- observability and refusal rules for the row-store fast path

This file does not replace:

- section `08` MGA record truth
- section `11` TOAST ownership and oversized value authority
- section `18` index-family maintenance policy
- section `39` front-door ingest-lane selection

## Hard invariants

1. Every inserted or updated row still receives MGA ownership, stable `TID`,
   and `row_uuid` truth.
2. No row-store fast path may bypass TOAST, checksum, forced-write, or
   schema-epoch rules.
3. Heap fast paths may amortize work, but may not weaken commit publication or
   visibility truth.
4. Heap-only update behavior may avoid index mutation only when section `18`
   proves that the affected index families do not require key or payload change.
5. Page-local write optimization is subordinate to stable tuple identity rather
   than replacing it.

## Canonical row-store write shapes

Every row-changing front-door shape that reaches heap storage shall classify
into one of these row-store write shapes before the first heap mutation:

| Write shape | Use case | Required row-store behavior |
| --- | --- | --- |
| `SINGLETON_RETAIL` | one row at a time DML | ordinary heap mutation path |
| `MICRO_BATCH_VALUES` | multi-row `VALUES`, `executemany`, small staged insert batches | shared metadata, page reservation, and multi-insert |
| `SET_SOURCED_INSERT` | `INSERT ... SELECT`, CTAS-like additive row production | staged row batches, heap multi-insert, and section-39 lane handoff |
| `NON_INDEXED_UPDATE_BATCH` | updates where indexed keys and indexed payload do not change | heap-only or stable-head-preserving update path |

The row-store write shape is chosen once per statement or admitted batch and
shall not be recomputed per row unless the statement crosses an explicit refusal
boundary.

## Statement-local metadata hoisting

For `MICRO_BATCH_VALUES`, `SET_SOURCED_INSERT`, and
`NON_INDEXED_UPDATE_BATCH`, the runtime shall resolve and cache statement-local
metadata once before entering the per-row loop:

- target column layout
- row serializer or deserializer state
- index family list and maintenance-class summary
- exact-family normalized-key layout metadata
- TOAST admission and externalization policy
- online-maintenance and generation-publication state needed for the whole batch

The runtime shall not re-run full table metadata discovery per row when the
table, schema epoch, and index set are unchanged for the statement batch.

## Heap multi-insert

### Required behavior

For `MICRO_BATCH_VALUES` and `SET_SOURCED_INSERT`, ScratchBird shall use a heap
multi-insert path with these properties:

1. reserve writable heap page state once for a run of rows that fit the same
   locality target
2. pin and mutate the current heap page for multiple rows before releasing it
3. reuse prepared row layout and tuple-header templates across the batch
4. emit section-18 maintenance deltas from the batch envelope instead of
   rebuilding invariant metadata per row
5. preserve locality across the batch so append-heavy tables do not thrash page
   selection

No admitted batched row source may devolve into repeated singleton
`insertTuple()` behavior once the row-store shape is `MICRO_BATCH_VALUES` or
`SET_SOURCED_INSERT`, unless an explicit refusal boundary fires.

### Required local reservation state

```cpp
struct HeapBatchInsertState {
  Uuid table_id;
  uint16_t target_tablespace;
  uint64_t statement_xid;
  uint32_t reserved_page_id;
  uint16_t reserved_free_slot_count;
  bool locality_locked;
  bool requires_toast;
  bool exact_family_work_present;
  uint32_t staged_row_count;
};
```

Physical code may encode this differently, but the logical behavior is fixed by
this file.

### Row-store refusal boundaries

The heap multi-insert path may refuse back to `SINGLETON_RETAIL` only when one
of the following is true:

- the statement activates row-level triggers or side effects that require
  singleton visibility points
- the row source crosses a schema epoch boundary during execution
- the target row shape or TOAST policy changes in a way that invalidates the
  current batch template
- section `39` routes the statement into `SHADOW_LOAD_CUTOVER` instead

The runtime shall emit an explicit refusal reason for any fallback.

## Heap-only and stable-head-preserving update

### Eligibility

An update qualifies for `NON_INDEXED_UPDATE_BATCH` only when all of the
following hold:

1. no exact-family normalized key changes
2. no index expression result changes
3. no partial-index predicate membership changes
4. no indexed include payload stored in an exact-family leaf changes
5. no active online-maintenance rule requires per-row family delta capture for
   the unchanged indexes

If these conditions hold, ScratchBird shall avoid per-row exact-family index
enumeration in the hot update loop.

### Required behavior

For `NON_INDEXED_UPDATE_BATCH`, ScratchBird shall:

1. prove unchanged indexed state once from statement-local metadata
2. choose a heap-only or stable-head-preserving row-store rewrite
3. avoid delete-plus-insert exact-family work
4. aggregate observability and contention publication at statement or batch
   scope instead of rescanning index metadata for every row

### Explicit forbidden behavior

For an admitted `NON_INDEXED_UPDATE_BATCH`, the runtime shall not:

- call full `listIndexesForTable()` per updated row
- rebind every exact family pointer per updated row
- bump exact-family contention counters through a per-row index walk when no
  exact-family mutation is required

Those behaviors are allowed only after an explicit refusal from the batch path.

## Locality and page reservation

The row-store fast path shall preserve locality:

1. append-heavy batches prefer the current writable locality before global free
   page search
2. `SET_SOURCED_INSERT` batches may cluster rows by dominant locality input
   chosen under section `39`
3. the runtime may reserve multiple heap pages ahead when the staged batch size
   proves they are needed

## Observability

The runtime shall publish:

- row-store write shape selected
- batch row count
- page reuse count
- average rows per pinned page
- batch fallback count and refusal reasons
- heap-only update hit rate
- non-indexed update batch count

## Required tests

1. multi-row `VALUES` insert uses page-coalesced heap multi-insert instead of
   repeated singleton insert calls
2. `INSERT ... SELECT` uses the same row-store batch path once admitted by
   section `39`
3. non-indexed `UPDATE` avoids per-row exact-family metadata walks
4. heap-only or stable-head-preserving update keeps visible row truth and
   uniqueness semantics unchanged
5. refusal boundaries fall back cleanly and emit explicit diagnostics

## Cross-section references

- `HEAP_AND_PRIMARY_STORAGE_BOUNDARY.md`
- `ACCESS_METHOD_DDL_DML_AND_MAINTENANCE_INTERACTION.md`
- `../18_Index_Framework/DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md`
- `../39_Backup_Restore_and_Bulk_Data_Paths/BULK_INGEST_LANES_AND_SHADOW_LOAD_CUTOVER_MODEL.md`
