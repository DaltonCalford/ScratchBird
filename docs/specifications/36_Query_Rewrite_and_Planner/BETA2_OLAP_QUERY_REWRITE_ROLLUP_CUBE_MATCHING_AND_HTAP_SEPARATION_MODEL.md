# Beta 2 OLAP Query Rewrite Rollup Cube Matching And HTAP Separation Model

Status: reconstructed_required_beta2

## Purpose

Define the Beta 2 planner rules for rewriting analytical queries to rollups,
projections, or cube materializations while protecting OLTP service classes.

## Governing rules

1. Rewrite is legal only when coverage, freshness, and security all match.
2. Rollup and cube candidates are ordinary optimizer competitors, not forced
   hints.
3. HTAP separation is explicit. Analytical work may not silently consume OLTP
   protected reserves.

## Rewrite matching algorithm

The planner shall compare:

- requested dimension set versus candidate dimension coverage
- requested measures versus candidate measure coverage
- filter semantics versus candidate grain and partition coverage
- requested freshness versus materialization freshness class
- security and tenant visibility digest

The planner shall reject the candidate if any one of those checks fails.

## Admitted analytical candidates

- base columnstore path
- projection path
- materialized summary path
- cube materialization path

## HTAP separation rules

- analytical rewrite candidates shall declare service class and node-role
  requirements
- if only OLTP-protected nodes are available, analytical work must queue,
  degrade, or refuse according to declared policy
- planner traces must show when a faster analytical candidate lost because the
  service envelope was unavailable

## Explain requirements

`EXPLAIN` shall disclose:

- rewrite candidate list
- selected acceleration class
- freshness check result
- HTAP envelope decision
- refusal reason for rejected cube or rollup candidates

## Refusal rules

- `OLAP_REWRITE_FRESHNESS_REFUSED`
- `OLAP_REWRITE_DIMENSION_COVERAGE_REFUSED`
- `OLAP_REWRITE_MEASURE_COVERAGE_REFUSED`
- `HTAP_SERVICE_ENVELOPE_REFUSED`

## Metrics

- rewrite hit ratio
- cube versus projection win rate
- freshness-refusal count
- HTAP envelope refusal count

## Cross-section requirements

- section `36` owns rewrite matching and HTAP planner behavior
- section `24` owns the freshness and rewrite-contract rows
- section `25` owns service-class and node-role envelopes
