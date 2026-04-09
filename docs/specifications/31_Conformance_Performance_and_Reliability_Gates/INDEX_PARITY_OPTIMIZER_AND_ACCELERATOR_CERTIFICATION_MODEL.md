Status: reconstructed_required_with_current_substrate

# Index Parity Optimizer and Accelerator Certification Model

## Purpose

This document defines the certification gates required to prove that all implemented index families participate as primary optimizer candidates and that optional accelerator paths degrade safely.

## Required Certification Families

Certification shall cover every implemented runtime family, including at minimum:

- ordered
- hash
- bitmap
- BRIN or summary
- inverted or text
- spatial
- GiST or generalized
- ANN and vector
- columnar or segment-summary families where queryable

## Mandatory Proof Classes

Each family shall have evidence for:

- candidate generation
- metrics publication
- cost normalization
- winning-plan eligibility
- MGA visibility correctness after index hit
- degraded or stale-metrics behavior

## Required Winning-Plan Gate

For each implemented family, the certification corpus shall include at least one workload where:

- the family is legal
- metrics are current
- the family wins the optimizer comparison
- execution preserves MGA visibility and result correctness

## Required Degraded-State Gate

For each implemented family, the certification corpus shall include at least one workload where:

- metrics are stale, degraded, or partial
- the family remains visible to the optimizer
- the planner applies the documented penalties
- the chosen path remains semantically correct

## Accelerator Gate

Where optional accelerator support exists, certification shall prove:

- admitted accelerator path
- accelerator refusal path
- non-accelerated fallback path
- identical result correctness across all three

## No Secondary Class Gate

Certification fails if any implemented index family is only reachable through:

- manual hints
- advisory mode
- special developer-only toggles
- hidden fallback that excludes the family from ordinary planning

## Evidence Requirements

The certification record shall preserve:

- winning candidate set
- normalized metrics packet
- cost comparison evidence
- final plan selection
- post-execution correctness evidence

## Failure Criteria

Certification fails when:

- a family cannot emit the required metrics packet
- a family never appears in the candidate set despite legal workload coverage
- a family cannot win under conditions where it should
- accelerator admission changes correctness instead of only changing cost and runtime
