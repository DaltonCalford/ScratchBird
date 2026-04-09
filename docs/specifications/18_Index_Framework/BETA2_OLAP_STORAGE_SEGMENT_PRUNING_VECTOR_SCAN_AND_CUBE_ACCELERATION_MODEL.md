# Beta 2 OLAP Storage Segment Pruning Vector Scan And Cube Acceleration Model

Status: reconstructed_required_beta2

## Purpose

Define the Beta 2 analytical storage contract over the existing columnstore and
summary substrate.

## Governing rules

1. Analytical storage remains derivative of committed MGA base-table truth.
2. Segment metadata must be sufficient for deterministic pruning.
3. Vectorized scan quantum is fixed and visible, not an accidental runtime
   constant.
4. Cube and projection acceleration are planner-visible competitors.

## Canonical analytical segment contract

Each analytical segment shall publish:

- segment uuid
- row count
- compressed bytes
- min and max values per prunable column
- null count per prunable column
- dictionary or encoding class
- sort key digest
- source lineage or refresh generation

## Vector scan contract

- default analytical vector quantum is `2048` rows
- operators may use smaller tail batches but may not publish larger logical
  vectors without explicit capability change
- late materialization is preferred for wide analytical projections

## Admitted acceleration classes

- base columnstore segment
- projection
- materialized summary table
- cube materialization
- pruning-only auxiliary summary

## Pruning algorithm

1. Load segment metadata.
2. Eliminate segments that cannot satisfy partition, min/max, null, or sort-key
   constraints.
3. Prefer projections or cube materializations when coverage and freshness are
   satisfied.
4. Fall back to base columnstore or base exact scan when acceleration is
   refused.

## Refusal rules

- `OLAP_SEGMENT_METADATA_MISSING`
- `OLAP_VECTOR_QUANTUM_UNSUPPORTED`
- `OLAP_ACCELERATOR_FRESHNESS_REFUSED`
- `OLAP_ACCELERATOR_COVERAGE_REFUSED`

## Metrics

- segment pruning ratio
- vector batches processed
- late-materialization hit ratio
- projection or cube acceleration hit ratio
- refresh-generation mismatch count

## Cross-section requirements

- section `18` owns segment structure, vector quantum, and acceleration classes
- section `24` owns cube and refresh metadata
- section `36` owns rewrite and candidate competition
