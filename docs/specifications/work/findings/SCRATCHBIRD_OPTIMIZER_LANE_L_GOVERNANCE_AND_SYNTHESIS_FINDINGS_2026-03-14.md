# ScratchBird Optimizer Lane L Findings

Lane: L

Topic: Governance, benchmark corpus, synthesis, and roadmap

Status: First-pass findings

Date: 2026-03-14

Primary planning inputs:
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_PROGRAM_2026-03-14.md`
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_AGENT_OPERATIONS_2026-03-14.md`

## 1. Scope and Lane Objective

Lane L merges the research lanes into one executable optimizer program.

The objective is to define:

- synthesis rules across lanes
- benchmark corpus and scorecards
- implementation phase gates
- deferral policy
- research-to-spec handoff process

## 2. ScratchBird Current-State Baseline

ScratchBird already has:

- a meaningful optimizer codebase
- a canonical research program
- first-pass findings across the lane structure
- existing unit and benchmark infrastructure that can be extended

Current gap:

- there is not yet one integrated optimizer specification set or benchmark contract tying the lanes together

## 3. Donor-Engine Research Synthesis

The best donor for Lane L is the benchmark and optimizer-evaluation literature rather than any one engine codebase.

Useful donor families:

- PostgreSQL and MySQL as plan-quality references
- DuckDB as an analytic-performance and pruning reference
- JOB, TPC-H, TPC-DS, and Star Schema Benchmark as workload families

What ScratchBird should adopt:

- lane synthesis through explicit pass-fail gates
- benchmark families covering cardinality, join order, and planning latency separately

## 4. Primary Literature and Official-Document Synthesis

The literature is clear on two points:

- join-order and cardinality quality require targeted benchmarks, not generic smoke tests
- optimizer research must be evaluated on tail behavior, not just means

Practical implication for ScratchBird:

- plan-quality gates must include q-error percentiles, planning latency, fallback rates, and workload runtime outcomes

## 5. Normalized Algorithm Packet

Recommended synthesis workflow:

1. freeze first-pass lane findings
2. resolve cross-lane contradictions
3. convert each lane into a formal engineering specification
4. define benchmark suites and pass-fail thresholds
5. build the implementation backlog in dependency order

Recommended implementation order:

1. architecture and memo framework
2. logical rewrites
3. statistics engine
4. cardinality estimation
5. access-path frontier
6. join search
7. cost model and calibration
8. parallel planning
9. adaptive feedback
10. observability completion
11. future ML advisory layer

## 6. Formula and Heuristic Packet

Required governance metrics:

- q-error p50, p90, p95, p99
- plan optimality versus donor references
- planning latency by relation count bucket
- fallback frequency by search mode
- spill prediction accuracy
- parallel speedup and regression rates

Recommended first-pass targets:

- no severe pathological regressions on benchmark suites
- planning latency bounded for common query sizes
- explicit deferral rationale for every non-shipped advanced feature

## 7. ScratchBird Contract Draft

Required governance artifacts:

- optimizer scorecard schema
- benchmark suite registry
- lane-to-spec conversion template
- contradiction-resolution log
- deferred-feature register

Program guarantees:

- no optimizer feature is considered complete without benchmark coverage
- no deferred feature disappears silently; each has a rationale
- implementation starts from formal lane-derived specs, not from raw findings notes

## 8. Validation and Benchmark Packet

Required benchmark families:

- TPC-H
- TPC-DS where feasible
- Star Schema Benchmark
- Join Order Benchmark
- correlated-predicate synthetic suites
- skew and heavy-hitter suites
- plan-stability suites
- parallel-benefit suites

Required reporting:

- benchmark configuration and seed control
- reference-engine comparison snapshots
- q-error scorecards
- plan-quality regression reports

## 9. Adopt/Adapt/Reject/Defer Matrix

- `Adopt`: benchmark-led optimizer signoff
- `Adopt`: lane-to-spec conversion before implementation
- `Adapt`: donor-reference comparisons to ScratchBird workload realities
- `Reject`: feature completion claims without scorecard evidence
- `Defer`: low-value optimizer sophistication that lacks benchmark justification

## 10. Open Questions and Integration Dependencies

- all prior lanes must be stabilized enough for contradiction review
- benchmark environments and datasets must be specified
- donor-reference collection must remain reproducible over time

## 11. Recommended Next-Step Specification Tasks

- create the integrated optimizer specification pack from lanes A-L
- define benchmark and scorecard documents
- create an optimizer contradiction-resolution note
- produce the implementation backlog and phase gates
