# ScratchBird Optimizer Lane H Findings

Lane: H

Topic: Parallel optimization

Status: First-pass findings

Date: 2026-03-14

Primary planning inputs:
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_PROGRAM_2026-03-14.md`
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_AGENT_OPERATIONS_2026-03-14.md`

## 1. Scope and Lane Objective

Lane H defines how ScratchBird should reason about parallel planning rather than simply wrapping a serial plan in a gather node.

The core objective is to produce:

- real partial-path planning
- explicit worker-count selection
- operator-by-operator parallel safety and benefit rules
- explicit exchange, gather, and gather-merge cost terms
- a planner-to-executor contract for local/global state and skew handling

This lane covers:

- parallel scan
- parallel hash join
- partial aggregation
- gather
- gather-merge
- exchange costs
- skew and repartition handling

## 2. ScratchBird Current-State Baseline

ScratchBird already exposes parallel controls and some planner-side wrappers, but the implementation is still largely scaffolding.

Current state:

- planner and session controls exist for enabling parallel operators, worker caps, leader participation, and exchange costs
- gather and gather-merge wrappers exist in planner structures
- parallel eligibility checks and costing hooks exist
- worker pool and manager scaffolding exist in execution code

Current gaps:

- planning is wrapper-based rather than partial-path-based
- parallel scan, parallel aggregate, and parallel hash join still fail closed or fall back to sequential execution
- gather and gather-merge appear primarily in planning and EXPLAIN rather than in a complete executor contract
- worker-count selection and skew handling are much thinner than mature donor engines

Inference:

- ScratchBird has enough scaffolding to specify parallel planning now
- it does not yet have a production-grade parallel execution architecture

## 3. Donor-Engine Research Synthesis

### PostgreSQL

PostgreSQL is the strongest donor for planner semantics.

Key donor patterns:

- true `partial_pathlist` support rather than root wrapping
- partial seq scans and partial join paths are generated early
- gather and gather-merge are layered over partial paths at the right point in planning
- worker selection depends on relation size and planner thresholds
- gather and gather-merge have explicit setup, transfer, and merge costs
- partial hash join and partial aggregation are explicit plan contracts

What ScratchBird should adopt:

- partial-path planning as the foundational model
- worker-count formulas and exchange costing
- partial hash join and partial aggregate planner contracts

### DuckDB

DuckDB is the strongest donor for execution architecture.

Key donor patterns:

- pipeline-native parallelism
- local and global source, sink, and operator state
- thread-count clamping based on source, operator, sink, and scheduler limits
- staged parallel hash join and aggregation pipelines
- skew-aware finalize and repartition logic

What ScratchBird should adopt:

- local/global state model for scan, hash join, and aggregate
- explicit exchange and pipeline boundaries
- skew and repartition behavior as executor-visible realities

Inference:

- PostgreSQL should define the planning contract
- DuckDB should shape the executor-side mechanics

## 4. Primary Literature and Official-Document Synthesis

The most useful primary sources for Lane H are official parallel-query documentation and engine design notes.

Key synthesis points:

- PostgreSQL documentation confirms that parallel planning is not “parallel everywhere”; it is operator-by-operator and safety-gated
- PostgreSQL docs also confirm that gather, gather-merge, parallel scan, parallel index scan, and two-stage aggregation require distinct planner semantics
- DuckDB design and code show that parallelism works best when the executor has explicit local/global state and pipeline boundaries rather than ad hoc thread pools alone

Practical implication for ScratchBird:

- it should not try to bolt parallelism onto the root plan only
- it needs partial-path planning and an executor contract that matches it

## 5. Normalized Algorithm Packet

### 5.1 Parallel planning flow

Recommended planning flow:

1. determine relation and operator parallel safety
2. generate partial scan paths where beneficial
3. generate partial join and partial aggregate candidates where legal
4. compute worker-count options
5. cost gather and gather-merge over the partial candidates
6. compare serial and parallel alternatives using explicit exchange costs

### 5.2 Partial-path contract

Each partial path must carry:

- operator family
- worker-safe flag
- required partitioning or distribution
- expected worker count
- local and global state expectations
- ordered versus unordered output semantics

### 5.3 Worker-count selection

Worker count should depend on:

- relation size or scanned pages
- estimated rows
- operator parallelizability
- expected skew
- memory availability
- scheduler ceiling

### 5.4 Parallel operator families

Required first-wave families:

- parallel sequential scan
- parallel index or index-only scan only if storage access semantics support it
- parallel hash join
- partial aggregate plus finalize aggregate
- gather
- gather-merge

## 6. Formula and Heuristic Packet

Recommended first-pass formulas:

- `parallel_divisor = workers + leader_fraction`
- `parallel_scan_cost = serial_scan_cost / parallel_divisor + setup + transfer`
- `parallel_hash_cost = build + probe / parallel_divisor + shared_state + spill + skew_penalty`
- `partial_agg_cost = local_agg + exchange + finalize_agg`
- `gather_cost = setup_cost + tuple_transfer_cost`
- `gather_merge_cost = gather_cost + merge_heap_cost + extra synchronization`

Recommended heuristics:

- do not parallelize very small relations
- do not parallelize when setup plus transfer dominates saved scan or join work
- penalize skewed hash partitions more aggressively than balanced ones
- veto parallel hash or aggregate when memory limits make spill almost certain
- ordered gather should require a meaningful downstream sort avoidance or order-preservation benefit

## 7. ScratchBird Contract Draft

Required contract additions:

- `PartialPath` and `ParallelPathProperties`
- worker-count selection API
- local and global execution-state contracts for scan, hash join, and aggregate
- explicit gather and gather-merge operator requirements
- skew metadata and repartition flags

Planner guarantees:

- every parallel plan candidate has a serial comparator
- gather and gather-merge are only emitted over valid partial paths
- worker counts and parallel-safety decisions are visible in plan payloads

## 8. Validation and Benchmark Packet

Required validation families:

- large sequential scan benefiting from parallelization
- ordered result requiring gather-merge
- parallel hash join with balanced and skewed key distributions
- partial aggregation and finalize aggregation
- memory-pressure and spill cases
- worker-count scaling curves
- disabled-by-default safety tests confirming no accidental parallelization

Key metrics:

- speedup versus serial
- worker utilization
- skew penalty accuracy
- gather and gather-merge overhead
- spill rate under parallel operators

## 9. Adopt/Adapt/Reject/Defer Matrix

- `Adopt`: PostgreSQL partial-path planning and worker-selection model
- `Adopt`: PostgreSQL explicit gather and gather-merge costing
- `Adapt`: DuckDB local/global state and pipeline mechanics
- `Adapt`: DuckDB skew-aware repartition behavior into ScratchBird executor rules
- `Reject`: wrapper-only root parallelization as the long-term design
- `Defer`: broad parallelization of every operator family before scan, hash join, aggregate, and gather are stable

## 10. Open Questions and Integration Dependencies

- Lane G must finalize exchange, setup, and skew cost coefficients
- Lane E must decide whether parallel-aware scans are separate path families or path properties
- Lane F must decide how parallel-aware properties influence join search
- Lane K will need explicit EXPLAIN visibility for worker counts, partial paths, and skew decisions

## 11. Recommended Next-Step Specification Tasks

- specify `PartialPath` and `ParallelPathProperties`
- define worker-count formulas and safety gates
- formalize gather and gather-merge planner contracts
- specify local/global executor state for scan, hash join, and aggregate
- add parallel-planning telemetry and validation suites
