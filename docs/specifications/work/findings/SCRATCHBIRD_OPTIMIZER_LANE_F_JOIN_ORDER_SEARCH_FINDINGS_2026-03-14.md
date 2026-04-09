# ScratchBird Optimizer Lane F Findings

Lane: F

Topic: Join-order search

Status: First-pass findings

Date: 2026-03-14

Primary planning inputs:
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_PROGRAM_2026-03-14.md`
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_AGENT_OPERATIONS_2026-03-14.md`

## 1. Scope and Lane Objective

Lane F defines how ScratchBird should search join order once access-path and logical-rewrite inputs are available.

The core objective is to produce a search strategy that is:

- exact for small and medium reorderable join graphs
- legality-aware for outer, semi, anti, lateral, and parameterized joins
- bushy-capable where that matters
- able to degrade gracefully when search-space growth becomes unbounded

This lane covers:

- exhaustive dynamic programming
- bounded dynamic programming
- bushy planning
- hypergraph and DPhyp-style search
- legality barriers
- search-budget fallback
- Cartesian-product suppression and last-ditch handling

## 2. ScratchBird Current-State Baseline

ScratchBird already has a real join-order subsystem, but it is still structurally weaker than the best donor engines.

Implemented now:

- `AUTO` chooses exhaustive subset-DP for smaller joins, bounded DP for medium joins, then switches to heuristic modes
- dynamic-programming enumeration can produce bushy trees
- heuristic modes include a hypergraph-greedy merger and a generic greedy fallback
- join-legality metadata already exists for outer, semi, anti, natural, using, and lateral barriers

Current gaps:

- current `HYPERGRAPH_GREEDY` is not true DPhyp or connected-subgraph enumeration
- disconnected subsets become explicit cross joins too early
- any reorder barrier can collapse the whole query to input-order planning rather than fencing only the affected region
- join-method legality is richer than the search actually consumes
- merge join does not materially shape the join-order search phase

Inference:

- ScratchBird has strong groundwork for join search, but it needs a better exact-to-approximate search architecture and finer-grained legality fencing

## 3. Donor-Engine Research Synthesis

### PostgreSQL

PostgreSQL is the best donor for exact DP, legality fencing, and last-ditch Cartesian behavior.

Key donor patterns:

- bottom-up dynamic programming over join relation levels
- explicit left-deep, right-deep, and bushy combination generation
- `SpecialJoinInfo` and `join_is_legal()` confine legality barriers to the relevant region
- Cartesian products are delayed until all legal connected joins fail
- large-query fallback switches to GEQO rather than pretending exhaustive search is still viable

What ScratchBird should adopt:

- partial legality rather than whole-query bailout
- delay Cartesian products until last ditch
- explicit legality metadata feeding the search state itself

### MySQL

MySQL is the strongest donor for DPhyp-style hypergraph search and graph simplification.

Key donor patterns:

- join hypergraph and conflict-rule representation
- connected-subgraph / connected-complement enumeration
- legality encoded directly into hypergraph structure
- graph simplification when subgraph-pair explosion gets too large
- suppression of useless parameterized or lateral-driven alternatives

What ScratchBird should adopt:

- real DPhyp-like enumeration for reorderable inner-join components
- graph-simplification fallback instead of a purely greedy agglomerative heuristic

### DuckDB

DuckDB is the best lightweight donor for exact-to-approximate transition control.

Key donor patterns:

- build the join graph only for reorderable joins
- exact connected-subgraph enumeration up to a bounded complexity
- switch to approximate search after relation-count or emitted-pair thresholds
- fence non-reorderable operators as local optimization boundaries

What ScratchBird should adopt:

- exact search until budget is exhausted
- budget-driven, not ad hoc, transition to fallback search

## 4. Primary Literature and Official-Document Synthesis

The strongest primary sources for Lane F are the PostgreSQL optimizer README, the DPhyp literature, and the Join Order Benchmark work.

Key synthesis points:

- DPhyp-style connected-subgraph enumeration is the right exact-search donor for reorderable inner-join regions
- legality barriers must be represented structurally, not as one global boolean
- the Join Order Benchmark literature confirms that wrong join ordering remains one of the largest drivers of catastrophic optimizer failures
- pure heuristic search without a clean exact-search core is not adequate for a best-in-class optimizer target

Practical implication for ScratchBird:

- keep exact DP for small graphs
- use DPhyp-style enumeration for larger reorderable components
- use fallback heuristics only after an explicit budget or simplification stage

## 5. Normalized Algorithm Packet

### 5.1 Search partitioning

Recommended search flow:

1. Partition the join graph into reorderable and fenced regions.
2. Build legality metadata for each region.
3. For small regions, use exhaustive DP.
4. For medium and larger reorderable inner-join regions, use connected-subgraph/DPhyp enumeration.
5. If the search budget is exceeded, simplify the graph or fall back to bounded greedy search.
6. Delay Cartesian products until all connected legal alternatives are exhausted.

### 5.2 Search modes

Recommended mode ownership:

- `2-8` relations: exhaustive DP
- `9-15` relations: DPhyp-style exact search with budgets
- above budget or after graph explosion: graph simplification, then bounded greedy fallback

These thresholds are placeholders and must be calibrated from ScratchBird planning telemetry.

### 5.3 Legality handling

Search state must carry:

- join type
- null-introduction region
- semi and anti placement constraints
- lateral and parameterized dependencies
- reorder-fence boundaries

No single barrier should collapse the entire query to input order unless the whole graph is truly non-reorderable.

### 5.4 Join-method interaction

Join-order search must surface at least:

- nested loop
- hash join
- merge join when legal and property-relevant

If merge join is not costed during search, the search can systematically miss orderings that exist only because sort avoidance makes merge join globally superior.

## 6. Formula and Heuristic Packet

Required search formulas and heuristics:

- subset DP complexity tracking
- subgraph-pair budget for DPhyp enumeration
- Cartesian-product penalty and last-ditch gating
- greedy fallback trigger after budget exhaustion
- legality pruning before cost comparison

Recommended first-pass budget model:

- `search_cost = explored_states + emitted_pairs + surviving_frontier_entries`
- once `search_cost` exceeds the configured budget, simplify or approximate

Recommended heuristics:

- never emit disconnected joins while connected legal joins remain
- prefer connected edges with stronger predicates before weak-selectivity cross edges in greedy fallback
- treat parameterized and lateral constraints as hard pruning inputs, not soft post-filters

## 7. ScratchBird Contract Draft

Required contract additions:

- `JoinSearchRegion` representing a reorderable component
- `JoinLegalityContext` carried per subset or subgraph
- explicit search budgets and fallback reasons in plan payloads
- join-order candidates retaining method-eligibility metadata, not just a scalar cost
- telemetry fields for explored states, emitted pairs, pruned states, and fallback trigger

Planner guarantees:

- exact search is attempted when the configured budget allows it
- legality pruning occurs before cost comparison
- Cartesian products are documented when chosen

## 8. Validation and Benchmark Packet

Required validation families:

- star joins with clear connected join graphs
- snowflake joins where bushy plans matter
- outer/semi/anti join legality fences
- lateral and parameterized cases
- dense join graphs triggering hypergraph search
- large join graphs triggering simplification or fallback
- JOB-style benchmark queries comparing plan quality against donor engines

Key metrics:

- planning latency
- explored-state count
- exact-search versus fallback usage rate
- join-order optimality gap against reference plans
- accidental-Cartesian rate

## 9. Adopt/Adapt/Reject/Defer Matrix

- `Adopt`: PostgreSQL-style legality fencing and delayed Cartesian products
- `Adopt`: MySQL DPhyp-style connected-subgraph search
- `Adapt`: DuckDB exact-to-approximate budget transition
- `Adapt`: graph simplification thresholds using ScratchBird telemetry
- `Reject`: whole-query bailout on any reorder barrier as a long-term design
- `Defer`: GEQO-style genetic search until exact plus DPhyp plus bounded greedy have been exhausted

## 10. Open Questions and Integration Dependencies

- Lane E must define how many base-path alternatives each join state may consume
- Lane G must provide lower-bound and detailed costing layers for search pruning
- Lane H may require parallel-aware join-search properties later
- Lane I may eventually add re-optimization hooks for bad join-order outcomes

## 11. Recommended Next-Step Specification Tasks

- specify `JoinSearchRegion`, `JoinLegalityContext`, and search-budget structures
- formalize DPhyp-style enumeration over ScratchBird join graphs
- define graph-simplification fallback
- add merge-join legality and order-property integration to join-search states
- define join-search telemetry and regression tests
