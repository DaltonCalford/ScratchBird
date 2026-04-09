# ScratchBird Optimizer Lane I Findings

Lane: I

Topic: Adaptive optimization and runtime feedback

Status: First-pass findings

Date: 2026-03-14

Primary planning inputs:
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_PROGRAM_2026-03-14.md`
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_AGENT_OPERATIONS_2026-03-14.md`

## 1. Scope and Lane Objective

Lane I defines what bounded adaptive behavior ScratchBird should implement after the classical optimizer produces an initial plan.

The first-wave objective is not full mid-query self-rewriting. The objective is to add high-value adaptive behavior that is:

- observable
- bounded
- stable
- rollback-safe

This lane covers:

- runtime filters
- cardinality-feedback capture
- bounded adaptive estimation repair
- limited runtime re-optimization
- adaptive join switching only if a stable executor contract exists
- dynamic partition or segment pruning

## 2. ScratchBird Current-State Baseline

ScratchBird already has more adaptive scaffolding than a typical early-stage engine.

Implemented now:

- prior feedback can influence recompilation paths
- execution captures actual row counts
- query profiling records estimated versus actual rows, correction factors, observation counts, and replan thresholds
- runtime-filter and pruning metadata already exist in path and plan payload structures

Current gaps:

- adaptive behavior is targeted rather than pervasive
- there is no full runtime join-method switch contract
- feedback is captured, but not yet normalized into a durable optimizer feedback store
- runtime filters and re-estimation are not yet formalized as a lane-wide contract

Inference:

- ScratchBird already has the raw instrumentation needed for a serious adaptive wave
- the missing piece is bounded policy, not just more counters

## 3. Donor-Engine Research Synthesis

### DuckDB

DuckDB is the strongest donor for runtime specialization.

Key donor patterns:

- row-group reorder and pruning for `ORDER BY ... LIMIT`
- runtime dynamic filters from joins and Top-N
- adaptive filter ordering
- perfect-hash specialization in narrow eligible cases

What ScratchBird should adopt:

- runtime filter production and consumption as a first-class plan contract
- dynamic pruning and lightweight operator specialization where the guardrails are clear

### PostgreSQL

PostgreSQL is the strongest donor for bounded executor-time adaptation rather than full re-optimization.

Key donor patterns:

- executor-time partition pruning
- Memoize for parameterized rescans
- generic versus custom plan selection
- no broad core runtime re-optimizer

What ScratchBird should adopt:

- bounded executor-time pruning and caching
- conservative approach to runtime plan shape changes

### LEO and related adaptive literature

The LEO line of work is the strongest donor for feedback-driven repair.

Key donor patterns:

- record repeated misestimates
- feed them back into future optimization
- prefer learning from recurring workload shapes rather than rewriting every single execution

What ScratchBird should adopt:

- statement-shape and clause-bundle feedback
- bounded q-error-driven re-estimation or stats-refresh triggers

## 4. Primary Literature and Official-Document Synthesis

The strongest primary-source conclusions are consistent.

- LEO demonstrates that repeated misestimates can be corrected productively when the loop is narrow and evidence-backed
- PostgreSQL documentation and behavior confirm that bounded executor-time pruning is valuable without requiring a full adaptive optimizer
- DuckDB shows that runtime filters and specialization can improve execution quality without requiring a whole new search pass

Practical implication for ScratchBird:

- the first serious adaptive wave should focus on runtime filters, q-error feedback, and bounded re-optimization triggers
- full adaptive join switching should remain deferred until the executor and operator-state contracts are stronger

## 5. Normalized Algorithm Packet

### 5.1 Adaptive loop

Recommended first-wave loop:

1. collect estimated versus actual row counts per scan, join, and aggregate boundary
2. compute q-error and severity bands
3. record feedback keyed by normalized statement shape, clause bundle, and join edge
4. allow later replanning to consume the feedback as correction hints or stats-refresh triggers
5. emit runtime filters when build-side operators prove selective key domains

### 5.2 Allowed first-wave adaptive families

- runtime bloom, min/max, and IN-style filters
- dynamic partition or segment pruning
- feedback-driven cardinality repair on repeated statements
- bounded re-optimization before expensive remaining work

### 5.3 Deferred families

- arbitrary mid-query join-order replacement
- aggressive adaptive join-method switching without preserved operator state
- opaque feedback loops that cannot be surfaced in EXPLAIN or diagnostics

## 6. Formula and Heuristic Packet

Recommended first-wave heuristics:

- `q_error = max(actual / estimated, estimated / actual)`
- `q_error <= 4`: informational only
- `4 < q_error <= 8`: warm correction candidate
- `8 < q_error <= 32`: strong feedback candidate
- `q_error > 32`: trigger stats-refresh or re-optimization eligibility review

Re-optimization should require all of:

- severe misestimate on a high-cost remaining subtree
- expected remaining work materially larger than replan overhead
- no unstable or non-transferable operator state that would make replacement unsafe

Feedback aging:

- repeated confirmations strengthen correction confidence
- old feedback decays as data changes or stats freshness drops

## 7. ScratchBird Contract Draft

Required contract additions:

- `FeedbackKey` over normalized statement shape, clause bundle, and join edge
- `ObservedCardinalityRecord`
- runtime filter production and consumption interfaces
- re-optimization eligibility flags on plan nodes
- feedback aging and invalidation metadata

Planner guarantees:

- adaptive behavior is opt-in and traceable
- every correction source is surfaced in plan payloads
- no adaptive action occurs without a recorded trigger and rationale

## 8. Validation and Benchmark Packet

Required validation families:

- repeated statements with chronic misestimates
- selective hash joins producing runtime filters
- partition-pruning-at-execution scenarios
- regressions where adaptive logic must not thrash or oscillate
- large-query cases where re-optimization overhead would exceed benefit

Key metrics:

- q-error improvement on repeated statements
- bad-plan recurrence rate
- runtime filter selectivity benefit
- replan success versus replan regret rate

## 9. Adopt/Adapt/Reject/Defer Matrix

- `Adopt`: LEO-style bounded feedback on repeated misestimates
- `Adopt`: runtime filters and dynamic pruning
- `Adapt`: PostgreSQL-style bounded executor-time pruning and caching
- `Adapt`: DuckDB-style runtime specialization where contracts are clear
- `Reject`: unconstrained mid-query re-optimization in the first serious wave
- `Defer`: adaptive join switching until operator-state transfer is formally specified

## 10. Open Questions and Integration Dependencies

- Lane D must finalize estimator-confidence and q-error contracts
- Lane G must quantify replan-overhead thresholds
- Lane H must define what parallel operator state can survive adaptive replacement
- Lane K must expose adaptive traces in EXPLAIN and diagnostics

## 11. Recommended Next-Step Specification Tasks

- define feedback storage and aging contracts
- specify runtime filter types and operator interfaces
- formalize re-optimization safety gates
- define adaptive traces in plan payloads and EXPLAIN output
- add repeated-workload adaptive regression suites
