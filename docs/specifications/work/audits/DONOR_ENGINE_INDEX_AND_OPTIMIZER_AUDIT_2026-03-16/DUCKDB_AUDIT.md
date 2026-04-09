# DuckDB Audit

## Architectural Summary

DuckDB is the strongest donor for optimizer-executor-storage coherence. It does not have the broadest index-family catalogue, but it is excellent at making planner decisions line up with vectorized execution, row-group statistics, and late materialization.

## Planning Flow

1. Logical plan rewrites feed a query graph into `JoinOrderOptimizer`.
2. The join-order optimizer builds a graph, initializes leaf plans, solves join order, and reconstructs a new logical plan.
3. `StatisticsPropagator` pushes statistics through logical operators and expressions so later passes know real cardinality and value-range constraints.
4. Specialized passes such as filter pushdown, join-filter pushdown, row-group pruning, Top-N, and late materialization refine the plan around execution realities.

## How DuckDB Uses Indexes and Storage Metadata

DuckDB’s most important access accelerators are not “many AMs,” but:

- row-group statistics
- zonemap-like min/max pruning
- storage indexes where available
- row-id based late materialization
- selective column fetch after ordering/limit/filter work is done

`row_group_pruner.cpp` shows that DuckDB is willing to reorder or prune row groups based on order/limit information. That is a major performance idea: the optimizer is storage-shape aware.

## Execution Coupling

DuckDB’s late materialization path is not bolted on afterwards. The optimizer:

- finds when only a small column subset is needed early
- carries row ids through filters/orders
- fetches wide columns later only if the reduced row set survives

This is why DuckDB feels coherent. The planner knows what the vectorized executor wants.

## What ScratchBird Should Borrow

- Statistics propagation as a default pass, not an optional afterthought
- Row-group and chunk pruning as first-class plan optimization
- Late materialization driven by actual referenced-column sets
- Storage-order-aware optimization when `ORDER BY` + `LIMIT` can be exploited

## What ScratchBird Should Not Misread

DuckDB is not the donor for a wide exact index-family taxonomy. Its real lesson is:

- reduce bytes read
- reduce columns fetched
- reduce row groups touched
- align planner and executor tightly

## ScratchBird Comparison Hooks

- Compare ScratchBird row-group/chunk pruning and late materialization plans to DuckDB, not only to warehouse engines.
- Compare ScratchBird statistics propagation depth to DuckDB’s operator and expression propagation model.
- Use DuckDB as the donor for execution coherence, not for AM breadth.
