Status: current_authority

# B-Tree Proof Corpus Benchmark and Maintenance Evidence Model

## Purpose

This file defines the benchmark and proof-corpus contract for ordered B-tree
runtime behavior under insert, search, restart-anchor, and maintenance-heavy
workloads.

## Current benchmark authority

The current benchmark creates fresh test trees and runs scenarios covering:

1. UUIDv7-compressed key workloads
2. prefixed string-compressed key workloads
3. random uncompressed key workloads
4. restart-anchor and maintenance-heavy workloads with removals

## Scenario metrics

Each scenario currently records:

- label
- entry count
- search count
- average insert latency in microseconds
- average search latency in microseconds
- total results returned

The benchmark emits a structured stdout line beginning with:

- `BTREE_PROOF_SCENARIO`

## Gate expectations

The current benchmark enforces:

1. search result count must be positive for exercised scenarios
2. average insert latency must remain bounded below 10000 microseconds
3. average search latency must remain bounded below 5000 microseconds

## Maintenance evidence

The restart-anchor and maintenance corpus exists to prove more than point-query
speed. It also exercises:

1. repeated insert behavior under common-prefix data
2. search behavior after large insert populations
3. alternating delete or remove patterns
4. maintenance-sensitive ordered-family behavior

## Interpretation rule

This lane is both:

1. a benchmark lane
2. a correctness-adjacent evidence lane for ordered-index maintenance behavior

It is not only a throughput microbenchmark.

## Reconstructed required expansion

The rebuild requires future artifact outputs for:

1. page split counts
2. compression effectiveness
3. restart-anchor reuse effectiveness
4. dead-entry burden
5. ordered-family native metrics packet correlation
