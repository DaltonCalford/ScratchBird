# ClickHouse DML Write-Path Audit

## Architectural Summary

ClickHouse is a donor for ingest shape, immutable part publication, and sparse granule-level pruning. It is not a donor for transactional visibility truth, but it is extremely useful for learning how to keep heavy insert workloads fast by sorting once, publishing immutable parts, and letting background merge debt absorb consolidation cost.

## Insert Optimizations

- Every insert creates a data part that is lexicographically sorted by the primary key.
- Small frequent inserts can use `Compact` parts instead of `Wide` parts, reducing filesystem overhead and improving small-batch ingest behavior.
- The primary key indexes granules rather than rows, so the memory footprint of the pruning structure stays small even at very large scale.

## Update/Delete Optimizations

- ClickHouse treats consolidation as background work. Older data and TTL-driven actions are retired through merges and part actions rather than foreground per-row cleanup.
- This is the correct donor lesson for summary or append-heavy ScratchBird families, not for exact transactional table truth.

## Index Maintenance Optimizations

- Granules and marks are the center of the design. Index structures are sparse by intent.
- `MergeTreeIndexGranularity` exists to control row count between marks and thereby tune read amplification against metadata size.
- Merge and mutation code publishes replacement parts after building them, instead of mutating all existing published parts in place.

## Reliability And Publication Pattern

- Data parts are the publication unit.
- Background merge code builds a replacement part, then renames and replaces old parts under controlled transaction-like conditions.
- The important general lesson is "build new, validate, then replace."

## Best Borrow Candidates For ScratchBird

- Compact-versus-wide staging formats for small versus bulk insert batches.
- Immutable part publication for summary-style and append-heavy families.
- Sparse granule summaries and marks for families where page or range pruning matters more than tuple exactness.

## Local Source Anchors

- `docs/en/engines/table-engines/mergetree-family/mergetree.md`
- `src/Storages/MergeTree/MergeTreeDataWriter.cpp`
- `src/Storages/MergeTree/MergeTreeDataMergerMutator.cpp`
- `src/Storages/MergeTree/MergeTreeIndexGranularity.h`
