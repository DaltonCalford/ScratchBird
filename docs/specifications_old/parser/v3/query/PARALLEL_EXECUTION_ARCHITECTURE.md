# Parallel Execution Architecture (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define intra-node parallel execution for V3. Distributed/cluster execution is
out of scope and rejected in V3.

## Scope

- Parallel scan, join, aggregate, and sort within a single node
- Deterministic execution and MGA visibility

Out of scope:
- Cross-node execution
- Remote data movement

## Core Model

### Roles
- Coordinator: plans and schedules work, assembles final results
- Workers: execute plan fragments over assigned granules

### Exchange Operators
Parallelism is expressed via explicit exchange operators:
- `EXCHANGE_DISTRIBUTE`: round-robin or broadcast
- `EXCHANGE_REPARTITION`: hash repartition on keys
- `EXCHANGE_GATHER`: merge/concatenate streams

### Degree of Parallelism (DOP)
- Chosen by the optimizer based on cost and configuration limits.
- DOP MAY be reduced at runtime if resources are constrained.

### Parallel Safety
Operators/functions are tagged:
- `ParallelSafe`
- `ParallelRestricted` (coordinator only)
- `ParallelUnsafe` (forces serial plan)

If any operator is `ParallelUnsafe`, the optimizer MUST insert `EXCHANGE_GATHER`
and force downstream serial execution.

## Granule Scheduling

Granules are the smallest unit of parallel work:
- Table scan: page ranges
- Index scan: key ranges
- Aggregate: hash buckets
- Sort: key-range partitions

Workers request granules dynamically from the coordinator to balance load.

## Determinism Rules

- Parallel plans MUST preserve required ordering semantics.
- Results MUST be identical to serial execution under the same snapshot.
- Any nondeterministic function forces serial execution.

## Related Specs

- `docs/specifications/parser/v3/query/QUERY_OPTIMIZER_SPEC.md`
- `docs/specifications/parser/v3/transaction/TRANSACTION_MGA_CORE.md`
- `docs/specifications/parser/v3/transaction/TRANSACTION_LOCK_MANAGER.md`
