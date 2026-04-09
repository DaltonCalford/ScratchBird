# Cassandra Audit

## Architectural Summary

Cassandra is not a relational optimizer donor. It is an excellent donor for storage-attached index publication, query-time merging of memtable and SSTable index state, and exact-vs-post-filter discipline under repair and compaction.

## Query and Index Flow

1. A `ReadCommand` defines the partition or token-range read.
2. The secondary-index framework chooses an `Index.QueryPlan`.
3. For SAI, `StorageAttachedIndexQueryPlan` splits the original filter into:
   - index-usable filter
   - post-index filter
4. `QueryController` builds a query view across relevant memtable and SSTable indexes.
5. Per-expression search results are created for disk and in-memory indexes.
6. Results are intersected or unioned depending on strict filtering and repair state.
7. Matching keys are then fetched from storage for exact row materialization.

## How Cassandra Uses Indexes

### Storage-Attached Index

- SAI is attached to SSTables and memtables, not layered as a global mutable B-tree.
- Query-time index reads are naturally segment/SSTable aware.
- Index usage is shaped by repair state and strict filtering guarantees.

### Important correctness point

When strict filtering is not allowed, Cassandra does not over-trust intersections from partially updated unrepaired data. It separates repaired and unrepaired result sets and combines them conservatively. That is a strong donor for “family is not fully exact under all maintenance states” reasoning.

### Compaction and publication

- SSTable-attached indexes are rebuilt/published with storage lifecycle.
- Query engine merges current memtable state with persisted SSTable index state.

## Transaction and Visibility Model

Cassandra does not use MGA. Its truth model is:

- timestamps
- tombstones
- repair status
- compaction publication

The comparison value for ScratchBird is not transaction parity. It is:

- publication discipline
- partial-exactness honesty
- search across mutable plus immutable index sources

## What ScratchBird Should Borrow

- Per-family trust classification when maintenance state can weaken exactness
- Memtable plus persisted-index merge semantics for write-optimized families
- Conservative post-filter retention when exactness cannot be guaranteed

## ScratchBird Comparison Hooks

- Compare ScratchBird storage-attached summary/text/vector families to Cassandra SAI publication and query-controller flow.
- Compare any family with compaction or immutable-generation behavior against Cassandra’s repaired/unrepaired trust split.
