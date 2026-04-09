# ScratchBird Optimizer Lane G Findings

Lane: G

Topic: Cost model and calibration

Status: First-pass findings

Date: 2026-03-14

Primary planning inputs:
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_PROGRAM_2026-03-14.md`
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_AGENT_OPERATIONS_2026-03-14.md`

## 1. Scope and Lane Objective

Lane G defines the cost model contract that converts row estimates, access-path properties, and operator behaviors into comparable plan costs.

The core objective is not to produce one magic scalar formula. The objective is to produce a coherent family of formulas and calibration procedures for:

- scans
- joins
- sorting and Top-N
- aggregation and DISTINCT
- materialization and rescans
- spill and memory pressure
- exchange, gather, and gather-merge

This lane also defines how calibrated constants, confidence penalties, and runtime-observed corrections should enter planning.

## 2. ScratchBird Current-State Baseline

ScratchBird already has a broad cost subsystem.

Implemented now:

- cost knobs exist for sequential and random page access, CPU tuple work, work memory, spill, hash, merge, aggregate, and parallel overhead
- the cost record already carries memory budget, spill prediction, formula profile, and calibration IDs
- scans, joins, sort, aggregate, DISTINCT, window, and exchange are all explicitly costed
- planner integration is real rather than ornamental; scan choices, join methods, and upper operators already consume cost results

Current gaps:

- join-order search still reasons with a narrow subset of the full cost model
- merge join can become globally winning only after join-tree shape is mostly fixed
- calibration is still profile-driven rather than execution-fed
- some assumptions remain heuristic or hard-coded instead of empirically calibrated
- cost confidence and statistics confidence are related, but not yet a formal contract

Inference:

- ScratchBird does not need a new cost subsystem from scratch
- it needs a more layered, calibratable, and search-aware version of the current one

## 3. Donor-Engine Research Synthesis

### PostgreSQL

PostgreSQL is the best donor for cost-model structure.

Key donor patterns:

- explicit formulas for scans, joins, sort, materialize, aggregate, and gather
- two-stage join costing with cheap lower bounds first and detailed final costing later
- correlation and cache-aware scan costing
- exchange and gather-merge costing separated cleanly from scan and join logic
- planner cost parameters exposed as tunable calibrated constants

What ScratchBird should adopt:

- two-stage join/path costing
- explicit lower-bound costing for join search
- clearer separation between lower-bound cost and final detailed cost

### MySQL

MySQL is the strongest donor for calibration discipline and temp crossover modeling.

Key donor patterns:

- regression-fit cost constants
- normalized cost terms rather than ad hoc magic numbers
- temp-table and materialization cost treatment that is more graded than a binary spill flag
- separate modeling for sort, aggregate, and materialize behavior

What ScratchBird should adopt:

- regression-based constant fitting
- explicit temp crossover and spill-threshold modeling

### DuckDB

DuckDB is the strongest donor for executor-visible memory governance.

Key donor patterns:

- runtime memory reservation influences sort, hash join, and aggregation behavior
- staged externalization and repartitioning are explicit execution realities
- exchange and pipeline dependencies matter even when the planner formulas remain relatively light

What ScratchBird should adopt:

- planner formulas aligned with executor-visible reservation and spill states
- spill and exchange cost terms that reflect staged externalization rather than a simple boolean penalty

## 4. Primary Literature and Official-Document Synthesis

The most relevant primary-source family for Lane G is planner cost documentation and code-level design notes rather than classical optimizer papers alone.

Key synthesis points:

- PostgreSQL planner documentation and source show that cost models remain hand-built and highly effective when they are explicit, decomposed, and calibrated
- MySQL source and documentation reinforce that calibration and crossover modeling matter as much as formula shape
- modern cost-model studies continue to show that cost accuracy depends heavily on row estimates, but poor cost decomposition can still destroy plan quality even with good cardinalities

Practical implication for ScratchBird:

- keep the cost model explicit and interpretable
- calibrate constants empirically
- connect confidence and spill behavior to actual planner decisions

## 5. Normalized Algorithm Packet

### 5.1 Cost layers

ScratchBird should expose two layers of costing:

- `LowerBoundCost` for search pruning
- `DetailedCost` for final winner comparison

`LowerBoundCost` must be cheap to compute and conservative enough not to prune likely winners incorrectly.

`DetailedCost` must include:

- startup and total cost
- CPU, I/O, memory, and spill components
- exchange and synchronization overhead
- confidence penalty or uncertainty band

### 5.2 Operator families

Required detailed families:

- sequential scan
- index and index-only scan
- bitmap scan
- nested loop join
- hash join
- merge join
- sort and Top-N
- hash and sort aggregate
- materialize and memoize-like rescans where applicable
- gather and gather-merge

### 5.3 Confidence-aware costing

The cost model should not replace cardinality confidence with arbitrary pessimism, but it should expose:

- statistics confidence
- freshness class
- expected spill uncertainty
- calibration profile

These should produce a bounded penalty or uncertainty band, not an unbounded multiplier.

### 5.4 Calibration program

Recommended calibration workflow:

1. measure sequential scan throughput
2. measure random probe behavior
3. measure sort and spill crossover
4. measure hash build/probe and spill behavior
5. measure merge-join sort-preparation cost
6. measure gather and gather-merge overhead
7. fit stable coefficients and store calibration profile IDs

## 6. Formula and Heuristic Packet

Recommended first-pass formula families:

- sequential scan:
  - `pages * seq_page_cost + rows * cpu_tuple_cost`
- random or indexed access:
  - `index_probe + fetched_pages * random_page_cost * cache_factor + rows * cpu_tuple_cost`
- nested loop:
  - `outer_cost + outer_rows * inner_rescan_cost + join_cpu`
- hash join:
  - `build_cost + probe_cost + hash_cpu + spill_penalty`
- merge join:
  - `left_cost + right_cost + optional_sort_cost + merge_cpu`
- sort:
  - `rows * log2(rows) * cpu_operator_cost + spill_penalty`
- aggregate:
  - `input_cost + agg_cpu + group_state_cost + spill_penalty`
- gather:
  - `input_cost + setup_cost + tuple_transfer_cost`
- gather-merge:
  - `input_cost + setup_cost + tuple_transfer_cost + merge_heap_cost`

Recommended heuristics:

- search pruning should use a cheaper join lower bound than full detailed cost
- spill cost should scale with expected passes and bytes, not just a flat flag
- memory over-commit should create a steeper penalty for hash and sort than for sequential scan
- uncertainty penalty should be capped to avoid dominating every comparison

## 7. ScratchBird Contract Draft

Required contract additions:

- `LowerBoundCostEstimate`
- `DetailedCostBreakdown`
- calibrated coefficient profile storage
- explicit spill model structure containing predicted bytes, passes, and operator type
- uncertainty or confidence band attached to the cost record

Planner guarantees:

- every chosen path has a detailed cost breakdown
- every search-pruned path records which lower-bound rule discarded it
- calibration profile identity is preserved in plan payloads

## 8. Validation and Benchmark Packet

Required validation families:

- sequential versus index crossover tests
- merge join versus hash join crossover with ordered inputs
- sort versus ordered-scan crossover
- hash aggregate versus sort aggregate crossover
- spill and no-spill variants for sort and hash operators
- gather and gather-merge overhead validation
- calibration stability across different hardware profiles

Key metrics:

- chosen-plan latency versus estimated total cost ordering
- startup-cost ordering for row-goal queries
- spill prediction accuracy
- calibration drift over time

## 9. Adopt/Adapt/Reject/Defer Matrix

- `Adopt`: PostgreSQL-style explicit operator formulas and two-stage join costing
- `Adopt`: MySQL-style regression-based calibration discipline
- `Adapt`: DuckDB executor-governed spill and reservation signals into planner-visible cost terms
- `Adapt`: confidence-aware costing with bounded penalties
- `Reject`: opaque or unexplainable learned cost model in the first serious implementation wave
- `Defer`: fully learned cost replacement until deterministic formulas and calibration have matured

## 10. Open Questions and Integration Dependencies

- Lane D must finalize the estimator confidence contract consumed by the cost model
- Lane F must define how lower-bound cost interacts with join-search pruning
- Lane H must define worker-count and exchange-cost inputs for parallel operator costing
- Lane I may later add runtime-observed correction factors to calibration profiles

## 11. Recommended Next-Step Specification Tasks

- specify lower-bound versus detailed cost record structures
- formalize spill and materialization formulas
- define calibration-profile storage and benchmark procedure
- integrate merge join and exchange costing into join-search interfaces
- add cost-breakdown observability fields to plan payloads
