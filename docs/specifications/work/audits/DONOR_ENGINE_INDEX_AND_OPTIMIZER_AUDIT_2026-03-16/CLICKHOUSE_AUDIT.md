# ClickHouse Audit

## Architectural Summary

ClickHouse is not a donor for row-MVCC SQL planning. It is a donor for aggressive read-set reduction before execution. Its planner is only half the story; the other half is MergeTree storage deciding which parts, marks, granules, skip indexes, and projections need to be touched at all.

## Planning Flow

1. The analyzer produces a query tree.
2. `Planner.cpp` and related planner modules build a query-plan pipeline of expressions, joins, sorting, aggregation, windows, and read steps.
3. Before physical reading, table-expression data and filters are collected for storage analysis.
4. MergeTree storage uses partition pruning, primary-key conditions, and secondary skip-index conditions to reduce marks and parts.
5. Read algorithms then choose in-order, reverse-order, or projection-backed reading strategies.

## How ClickHouse Uses Indexes

ClickHouse uses several different “index” concepts:

### Sparse primary key

- MergeTree primary key is a sparse mark index over sorted data, not a tuple-by-tuple exact locator.
- `KeyCondition` converts predicates into a reverse-polish representation and evaluates whether a hyperrectangle or key range can still satisfy the query.
- This is the main reason ClickHouse can skip large data regions cheaply.

### Partition pruning

- `PartitionPruner` runs the same kind of predicate reduction against partition keys.
- This removes whole parts before mark-level work begins.

### Skip indexes

ClickHouse supports several pruning indexes:

- minmax
- set
- bloom
- text bloom/text posting helpers
- vector similarity skip/index granules

These are evaluated per granule and are explicitly allowed to be lossy. They are storage pruning tools first, not universal exact operators.

### Projections

- Projections are alternate physically stored layouts or pre-aggregated shapes.
- Planner and read pools can route into projections when they reduce cost enough.

## Execution Consequences

The ClickHouse lesson is that index usage happens in layers:

1. prune partitions
2. prune parts/marks via primary key
3. prune granules via skip indexes
4. optionally switch to projection
5. only then read column files

That layered pruning is a major donor idea for ScratchBird summary, bitmap, columnstore, and vector families.

## What ScratchBird Should Borrow

- A formal predicate interpreter for sparse primary ordering keys
- Granule-level skip index contracts distinct from exact tuple locators
- Projection-aware planning for alternate physical layouts
- Explicit bulk filtering APIs for advanced families

## What ScratchBird Should Exceed

- Clearer exact/recheck/approximate labeling in the runtime plan
- Stronger transaction/visibility integration for summary families than ClickHouse needs

## ScratchBird Comparison Hooks

- Compare ScratchBird summary/columnstore/bitmap/vector families to ClickHouse skip-index and projection layering.
- Compare any future “storage pruning” claims against ClickHouse’s explicit distinction between pruning and exact answer production.
