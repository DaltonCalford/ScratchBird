# Beta 2 Columnstore And OLAP Acceleration Implementation Closure Model

## Purpose

Close the implementation gap between current columnstore and OLAP canon and a
fully operator-visible analytical acceleration product.

## Governing rules

1. Columnstore remains derivative of heap and MGA truth.
2. Segment lifecycle is explicit and measurable.
3. OLTP interference budgets are enforced.
4. Refresh, compaction, and pruning quality are benchmarked.

## Required closure items

- segment build and seal thresholds
- compaction scheduler policy
- deleted-row cleanup flow
- dictionary and encoding selection telemetry
- hybrid OLTP/OLAP interference controls
- benchmark and SLO targets

## Refusal rules

- `COLUMNSTORE_SEGMENT_POLICY_INVALID`
- `COLUMNSTORE_COMPACTION_DEFERRED`
- `COLUMNSTORE_INTERFERENCE_LIMIT_EXCEEDED`

## Metrics

- segment build rate
- prune ratio
- compaction debt
- interference events
- aggregate pushdown win rate

## Cross-section requirements

- section `18` owns segment closure and pruning metrics
- section `31` owns analytical benchmark certification
- section `25` owns service-class isolation for hybrid workloads
