Status: current_authority

# Parser V3 Pipeline Benchmark Model

## Purpose

This file defines the in-repo benchmark model for the V3 parser and compiler
pipeline.

## Current benchmark authority

The current benchmark provisions a fresh temporary database and a live
`QueryCompilerV3` instance bound to the `PUBLIC` schema.

The benchmark currently measures compile latency across statement families:

1. simple constant `SELECT`
2. multi-constant `SELECT`
3. arithmetic expressions
4. logical expressions
5. string concatenation
6. `CASE` expressions
7. `CAST` expressions
8. simple `CREATE TABLE`
9. transaction statements:
   - `START TRANSACTION`
   - `COMMIT`
   - `ROLLBACK`

## Iteration model

The current benchmark uses:

- `1000` iterations per statement family

Each family records:

- total runtime in microseconds
- average microseconds per iteration

## Artifact model

The benchmark currently emits stdout sections containing:

1. test name
2. SQL sample or truncated SQL sample
3. iteration count
4. total V3 time
5. average V3 time per iteration

The benchmark also includes a summary block describing V3 pipeline benefits.

## Gate rule

The current gate rule is bounded but minimal:

1. each benchmarked family must complete
2. measured runtime must be positive

This is a pipeline liveness and measurement lane, not yet a hard regression
threshold lane.

## Interpretation rule

This benchmark proves:

1. the V3 path is benchmarked as a real pipeline
2. transaction statements are part of the parser benchmark surface
3. V3 benchmark scope includes semantic-analysis-bearing statements, not only arithmetic toy cases

## Reconstructed required expansion

The rebuild requires future additions for:

1. p50 and p95 parser latency outputs
2. UUID-resolution and catalog-helper contribution metrics
3. parser cache warm versus cold split
4. statement-class-specific regression thresholds
5. direct linkage to SBLR output size and lowering cost
