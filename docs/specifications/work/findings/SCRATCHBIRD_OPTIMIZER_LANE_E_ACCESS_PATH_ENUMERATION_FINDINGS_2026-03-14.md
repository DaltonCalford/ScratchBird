# ScratchBird Optimizer Lane E Findings

Lane: E

Topic: Access-path enumeration

Status: First-pass findings

Date: 2026-03-14

Primary planning inputs:
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_PROGRAM_2026-03-14.md`
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_AGENT_OPERATIONS_2026-03-14.md`

## 1. Scope and Lane Objective

Lane E defines how ScratchBird should enumerate base-relation access paths before join search and upper-operator planning.

The core objective is to replace the current single-winner access-path selection with a bounded frontier that preserves materially different candidates when they differ in:

- total cost and startup cost
- ordering properties
- coverage and index-only eligibility
- parameterization requirements
- spill, memory, and runtime-filter interaction

This lane covers:

- sequential scan
- index scan
- index-only scan
- bitmap scan
- skip scan
- index intersection and index union
- parameterized scans
- interesting orders
- path pruning and dominance rules

This lane does not finalize join-order search or full operator costing. It defines the access-path side of the contract that those later stages consume.

## 2. ScratchBird Current-State Baseline

ScratchBird already enumerates multiple base access choices, but it collapses them to a single chosen path too early.

Current base-relation enumeration behavior:

- sequential scan, per-predicate index scan, index-only scan, heuristic skip-scan, and bitmap index scan are costed in the main planner path
- partition pruning occurs before base scan costing
- index-order matching exists for simple `ORDER BY` prefix reuse
- path metadata already carries ordering, ordered-prefix length, parameterization markers, and an interesting-order score

Current structural gaps:

- all enumerated base paths collapse to one winner before downstream join search
- interesting orders are annotations, not retained alternatives
- bitmap OR remains mostly nominal because predicate decomposition and `predicate_combination` handling are shallow
- predicate-to-index matching is limited to simple `AND`-split predicates and prefix `LIKE`
- parameterization is mostly tagging for lateral references rather than a first-class frontier dimension
- skip-scan is still heuristic rather than access-method-aware
- index intersection and index union are not first-class path families

Inference:

- ScratchBird today has access-path enumeration logic, but not an access-path frontier
- the path metadata is already rich enough to support a stronger frontier without redesigning the entire planner

## 3. Donor-Engine Research Synthesis

### PostgreSQL

PostgreSQL is the strongest donor for frontier management and path pruning.

Key donor patterns:

- `set_plain_rel_pathlist()` and `create_index_paths()` build a true candidate list rather than a single provisional winner
- `build_index_paths()` keeps materially different alternatives alive when they differ in pathkeys, parameterization, or coverage
- `add_path()` and `set_cheapest()` perform dominance pruning across multiple dimensions, not just scalar total cost
- bitmap path generation supports OR-arm expansion, AND combination, and redundancy pruning
- parameterized index paths are first-class and are not retrofitted after join selection

What ScratchBird should adopt:

- a per-relation path frontier
- explicit dominance rules over cost, ordering, and required outer references
- real bitmap AND/OR construction and redundancy pruning

What ScratchBird should adapt rather than copy:

- PostgreSQL skip-scan support depends on access-method hooks and selectivity routines; ScratchBird should not pretend to support true skip-scan until access-method semantics are real

### MySQL

MySQL is the strongest donor for explicit skip-scan and multi-index row-ID composition.

Key donor patterns:

- access-path taxonomy explicitly separates skip-scan, row-ID intersection, row-ID union, and index-merge families
- covering scans and ordered scans are treated as genuinely different candidates
- interesting orders are modeled explicitly and can keep otherwise non-cheapest paths alive
- predicate bookkeeping separates applied, subsumed, and delayed predicates
- parameterized and lateral constraints are carried through access-path generation and pruned when they would create useless bushy states

What ScratchBird should adopt:

- explicit path families for index intersection and index union
- a real skip-scan path type with legality and cost gating
- better predicate bookkeeping around which filters are satisfied by the path itself and which remain as residual filters

What ScratchBird should adapt:

- MySQL’s access-path variety is broader than ScratchBird needs in the first wave; ScratchBird should implement the frontier model first, then add more path families

### DuckDB

DuckDB is not the main donor for multi-index composition, but it is useful for pruning and storage-local skipping.

Key donor patterns:

- row-group pruning and reordering for `ORDER BY ... LIMIT`
- Top-N pushdown
- lightweight index eligibility checks
- strong decorrelation support that reduces the need for parameterized scans in some query shapes

What ScratchBird should adopt:

- row-group or segment skipping where storage summaries exist
- Top-N-aware pruning for ordered scans

Inference:

- DuckDB is a secondary donor for Lane E; PostgreSQL and MySQL provide the primary frontier and access-path patterns

## 4. Primary Literature and Official-Document Synthesis

Official engine documentation and source design notes align with the donor-code findings.

Most relevant primary-source conclusions:

- PostgreSQL planner documentation and source commentary support keeping multiple index and bitmap alternatives alive because index choice is entangled with ordering, parameterization, and parallel-scan eligibility
- PostgreSQL index-only and bitmap-scan documentation reinforces that coverage and heap-visibility behavior are distinct cost dimensions, not just syntactic plan labels
- MySQL optimizer documentation treats Index Merge and Skip Scan as explicit optimizer features with their own applicability limits and switchable controls
- DuckDB indexing and performance documentation reinforces the value of storage-level skipping and ordered data layouts even when secondary indexes are sparse

Practical implication for ScratchBird:

- access-path enumeration is not only about finding the lowest local scan cost
- it must preserve candidates that can win later because they reduce sort cost, enable merge join, satisfy parameterization, or avoid heap fetches

## 5. Normalized Algorithm Packet

### 5.1 Base frontier generation

For each base relation, ScratchBird should generate a bounded frontier of `AccessPathCandidate` records.

Each candidate must capture:

- path family
- startup and total cost
- rows and pages
- output ordering and ordered prefix
- required outer relations
- covered columns
- residual predicates
- runtime-filter compatibility
- memory and spill expectations where relevant

### 5.2 Candidate families

Required first-wave families:

- `SEQ_SCAN`
- `INDEX_SCAN`
- `INDEX_ONLY_SCAN`
- `BITMAP_AND_SCAN`
- `BITMAP_OR_SCAN`
- `SKIP_SCAN`
- `INDEX_INTERSECTION`
- `INDEX_UNION`
- `PARAMETERIZED_INDEX_SCAN`

### 5.3 Enumeration order

Recommended order:

1. Apply partition or segment pruning.
2. Always build a sequential-scan baseline.
3. Enumerate direct index paths for matched predicates.
4. Enumerate covering variants and ordered variants.
5. Enumerate bitmap combinations when there are multiple usable predicate groups.
6. Enumerate skip-scan only when leading-prefix conditions fail but later key parts are selective enough.
7. Enumerate parameterized variants when lateral or join-parameter requirements are present.
8. Apply dominance pruning and keep a bounded frontier.

### 5.4 Dominance pruning

A path `P1` dominates `P2` only if:

- `P1` is no worse on required ordering
- `P1` is no worse on parameterization
- `P1` is no worse on coverage
- `P1` has lower or equal startup and total cost
- `P1` does not leave more residual predicate work than `P2`

If any of these differ materially, both paths remain in the frontier.

### 5.5 Interesting orders

An interesting order should keep a non-cheapest path alive only when:

- it satisfies full `ORDER BY`, grouping order, or merge-join input order
- it avoids a likely downstream sort
- it is within a bounded cost ratio of the cheapest unordered alternative

### 5.6 Parameterized paths

Parameterized paths must be first-class, not relabeled after join choice.

They should carry:

- required outer relation set
- correlated predicate bundle
- expected rescan frequency model
- inner-side legality flags for later join-method filtering

## 6. Formula and Heuristic Packet

Recommended first-pass formulas:

- sequential scan:
  - `cost_seq = pages * seq_page_cost + rows * cpu_tuple_cost + residual_cpu`
- index scan:
  - `cost_idx = index_probe_cost + heap_fetch_cost + rows * cpu_tuple_cost + residual_cpu`
- index-only scan:
  - `cost_ios = index_probe_cost + visibility_penalty + rows * cpu_tuple_cost`
- bitmap AND/OR:
  - `cost_bitmap = sum(index_probe_cost_i) + bitmap_build_cpu + heap_visit_cost`
- skip-scan:
  - `cost_skip = distinct_prefixes * per_probe_cost + qualifying_rows * tuple_cpu`
- parameterized rescan:
  - `cost_param = startup + rescan_count * rescan_cost`

Heuristic rules:

- keep the sequential-scan baseline always
- do not emit skip-scan if estimated leading-prefix distinct count is too high relative to table size
- do not keep a parameterized path if its required outer set makes it unusable under all legal join families
- for bitmap OR, emit only when no single direct index path already dominates the combined result

Inference:

- exact coefficients belong to Lane G, but Lane E needs these formula families to define what data the access-path subsystem must surface

## 7. ScratchBird Contract Draft

Recommended contract additions:

- `RelPathSet` per base relation holding a bounded list of surviving candidates
- `PathProperties` object for order, coverage, required outer, residual predicates, and pruning provenance
- `PredicateMatchResult` separating applied, residual, and delayed predicates
- explicit path-family IDs for intersection, union, and skip-scan
- dominance-pruning utilities shared by base-path enumeration and later join-search frontier pruning

Planner guarantees:

- at least one sequential path always exists
- all emitted paths carry explicit property and residual-predicate metadata
- path pruning must record why a candidate lost

## 8. Validation and Benchmark Packet

Required validation families:

- selective equality on single-column indexed predicates
- covering-index wins versus heap-fetch-heavy index scans
- bitmap AND and bitmap OR combinations
- skip-scan on composite indexes with missing leading-column predicates
- `ORDER BY ... LIMIT` queries where an ordered index should beat a sort
- parameterized inner scans under lateral or correlated query shapes
- partition-pruning and segment-skipping regressions

Key metrics:

- path frontier size
- chosen-path optimality against donor engines
- missed ordered-path opportunities
- false-positive skip-scan selection rate
- percentage of cases where a non-cheapest local path becomes globally winning

## 9. Adopt/Adapt/Reject/Defer Matrix

- `Adopt`: PostgreSQL-style per-relation frontier and dominance pruning
- `Adopt`: MySQL-style explicit intersection and union path families
- `Adapt`: MySQL interesting-orders machinery into a smaller ScratchBird property model
- `Adapt`: DuckDB row-group and Top-N pruning where storage summaries exist
- `Reject`: current heuristic-only skip-scan as a permanent design
- `Defer`: highly engine-specific path families that require access-method features ScratchBird does not yet expose

## 10. Open Questions and Integration Dependencies

- Lane F must define how many base-path alternatives join search is allowed to consume before the search budget collapses
- Lane G must calibrate ordered-scan versus sort crossover and skip-scan break-even coefficients
- Lane H must decide whether parallel-aware scans become separate path families or wrappers over base paths
- Lane I may later add runtime-filter-aware rescoring of base paths

## 11. Recommended Next-Step Specification Tasks

- define `RelPathSet`, `PathProperties`, and `PredicateMatchResult` structures
- specify dominance-pruning rules formally
- define bitmap AND/OR path construction and residual-predicate semantics
- specify real skip-scan legality and access-method requirements
- write a planner frontier test suite with path-retention and path-pruning assertions
