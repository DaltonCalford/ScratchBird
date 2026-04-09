# ScratchBird Optimizer Lane K Findings

Lane: K

Topic: Observability and introspection

Status: First-pass findings

Date: 2026-03-14

Primary planning inputs:
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_PROGRAM_2026-03-14.md`
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_AGENT_OPERATIONS_2026-03-14.md`

## 1. Scope and Lane Objective

Lane K defines how ScratchBird should explain, trace, and audit optimizer behavior.

The objective is not only nice EXPLAIN output. The objective is a full optimizer observability contract that makes it possible to answer:

- what alternatives were considered
- why the chosen plan won
- which statistics and feedback were trusted
- where the estimates were wrong
- when the optimizer switched strategy or fell back

## 2. ScratchBird Current-State Baseline

ScratchBird already has unusually strong optimizer observability for its current maturity.

Implemented now:

- plan payloads carry chosen and rejected alternatives
- statistics provenance is recorded
- optimizer controls are surfaced
- adaptive feedback hooks exist
- EXPLAIN and EXPLAIN ANALYZE rendering already exist
- query profiling records estimated and actual row information

Current gaps:

- query-profiler outputs are not yet fully integrated into one optimizer observability contract
- rewrite tracing is weaker than physical-plan tracing
- cost breakdowns and fallback reasons need further normalization
- adaptive and parallel traces need clearer surface contracts

Inference:

- ScratchBird should treat observability as a product feature, not a debugging afterthought

## 3. Donor-Engine Research Synthesis

### PostgreSQL

PostgreSQL is the strongest donor for explainability discipline.

Key donor patterns:

- `EXPLAIN` and `EXPLAIN ANALYZE` expose plan shape, costs, rows, loops, timing, buffer data, and planner settings
- plan-node output is stable enough to support automated performance and regression analysis
- auxiliary facilities such as `auto_explain` and statistics views reinforce that planner behavior must be introspectable outside the debugger

What ScratchBird should adopt:

- stable machine-readable and human-readable plan explanations
- explicit estimate versus actual comparisons at every major operator boundary

### DuckDB

DuckDB is the strongest donor for optimizer-pass profiling.

Key donor patterns:

- query profiling and `EXPLAIN ANALYZE`
- optimizer-pass timing and profiling output
- explicit optimizer configuration visibility

What ScratchBird should adopt:

- rewrite-pass and optimizer-stage timing
- optimizer-pass visibility alongside final plan output

## 4. Primary Literature and Official-Document Synthesis

The official documentation and public engine behavior point to the same requirement: a high-end optimizer must explain itself at three levels.

- plan level: chosen operator tree, costs, rows, ordering, and execution metrics
- search level: alternatives, pruning, fallback reasons, and search budgets
- statistics and feedback level: provenance, confidence, corrections, and drift

Practical implication for ScratchBird:

- observability must be built as a structured contract across rewrite, search, cost, execution, and feedback

## 5. Normalized Algorithm Packet

Recommended observability layers:

1. rewrite trace
2. search trace
3. chosen-plan explanation
4. execution actuals
5. adaptive feedback trace

Required surfaces:

- human-readable EXPLAIN
- machine-readable plan and trace payloads
- benchmark and regression reports

### 5.1 Rewrite trace

Expose:

- rules considered
- rules fired
- rules rejected with legality reasons
- resulting normalized query shape

### 5.2 Search trace

Expose:

- explored states
- surviving frontiers
- pruned alternatives
- fallback triggers
- Cartesian-product explanations when applicable

### 5.3 Estimate and actual trace

Expose:

- estimated rows
- actual rows
- q-error
- cost breakdown
- stats provenance and freshness

## 6. Formula and Heuristic Packet

Required observability metrics:

- `q_error = max(actual / estimated, estimated / actual)`
- plan instability rate across repeated executions
- fallback frequency by search mode
- ordered-path retention rate
- spill prediction accuracy

Recommended heuristic:

- every optimizer fallback or adaptive action should produce a structured reason code, not just prose

## 7. ScratchBird Contract Draft

Required contract additions:

- `RewriteTraceRecord`
- `SearchTraceRecord`
- `CostBreakdownRecord`
- `EstimateActualRecord`
- `AdaptiveTraceRecord`

Planner guarantees:

- all chosen plans have explainable statistics and cost provenance
- all pruned and fallback states have structured reason codes
- machine-readable traces remain versioned and stable enough for regression tooling

## 8. Validation and Benchmark Packet

Required validation families:

- EXPLAIN and EXPLAIN ANALYZE for rewrite-heavy queries
- join-search fallback visibility
- estimate-versus-actual reporting under severe misestimates
- parallel and adaptive traces once those features mature
- regression harnesses that diff machine-readable plans and traces

Key metrics:

- trace completeness
- field stability across versions
- mismatch rate between recorded and observed operator behaviors

## 9. Adopt/Adapt/Reject/Defer Matrix

- `Adopt`: PostgreSQL-style stable EXPLAIN discipline
- `Adopt`: DuckDB-style optimizer-pass profiling
- `Adapt`: ScratchBird’s existing chosen/rejected path payloads into a broader trace schema
- `Reject`: opaque optimizer decisions with no structured rationale
- `Defer`: ultra-heavy always-on tracing that would distort normal execution until performance costs are understood

## 10. Open Questions and Integration Dependencies

- Lane B must define rewrite-rule naming and proof records
- Lane F must define search-state and fallback telemetry
- Lane G must define stable cost-breakdown fields
- Lane I must define adaptive trace events

## 11. Recommended Next-Step Specification Tasks

- define the optimizer trace schema set
- add structured reason codes for pruning and fallback
- specify EXPLAIN text and machine-readable outputs
- define benchmark diffing and regression-report formats
- integrate query-profiler outputs into the unified trace contract
