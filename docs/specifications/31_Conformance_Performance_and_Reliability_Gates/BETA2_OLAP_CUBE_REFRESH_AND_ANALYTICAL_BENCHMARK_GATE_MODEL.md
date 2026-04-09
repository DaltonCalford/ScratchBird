# Beta 2 OLAP Cube Refresh And Analytical Benchmark Gate Model

Status: reconstructed_required_beta2

## Purpose

Define the Beta 2 certification gates for OLAP scans, rewrite behavior, and
cube refresh.

## Required benchmark families

- vectorized scan microbench
- pruning-heavy star-schema query pack
- projection-versus-base analytical crossover pack
- cube rewrite correctness pack
- incremental and full-refresh cube pack
- mixed HTAP interference pack

## Required metrics

- scan rows per second
- vector batches per second
- pruning ratio
- rewrite hit ratio
- cube refresh latency
- OLTP latency interference during analytical load

## Artifact requirements

Each gate run shall publish:

- dataset profile
- dimensionality and measure profile
- freshness configuration
- rewrite decision trace
- benchmark output artifacts
- regression classification

## Certification rules

- every accepted analytical benchmark must disclose whether it used base
  columnstore, projection, or cube materialization
- every accepted refresh benchmark must disclose refresh mode and source
  watermark behavior
- mixed HTAP runs must prove OLTP service classes remain within declared SLO
  bounds or are refused deterministically

## Refusal rules

- `OLAP_GATE_REWRITE_TRACE_MISSING`
- `OLAP_GATE_REFRESH_ARTIFACT_MISSING`
- `OLAP_GATE_HTAP_INTERFERENCE_DISCLOSURE_MISSING`

## Cross-section requirements

- section `31` owns benchmark corpus and regression thresholds
- section `18` owns analytical storage and acceleration metrics
- section `24` owns refresh and freshness state disclosure
- section `36` owns rewrite-decision disclosure
