# ScratchBird Optimizer Lane A Findings

Lane: A

Topic: Optimizer architecture and memo framework

Status: First-pass findings

Date: 2026-03-14

Primary planning inputs:
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_PROGRAM_2026-03-14.md`
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_AGENT_OPERATIONS_2026-03-14.md`

## 1. Scope and Lane Objective

Lane A defines the optimizer control architecture that later lanes will depend on. The immediate objective is not to design every rewrite or every cost formula. The objective is to decide what planning framework ScratchBird should build so that:

- logical rewrites can be added without hard-coding them into one monolithic planner function
- access-path, join-order, and post-join operator choices can be searched in one consistent framework
- required physical properties are explicit planner inputs instead of ad hoc side channels
- later statistics, cardinality, cost, parallel, and observability work can attach to stable contracts

This lane therefore answers five questions:

1. What optimizer architecture family best fits ScratchBird's current code and future requirements?
2. What memo, group, expression, property, winner, and rule APIs are minimally sufficient?
3. How should rule firing, search, pruning, and winner selection work?
4. How should ScratchBird migrate from the current staged planner without a destabilizing rewrite?
5. What validation hooks and diagnostics must exist from the first memo-capable implementation?

This lane does not finalize:

- the full logical rewrite catalog
- cardinality formulas for all predicates and joins
- calibrated cost coefficients
- complete parallel planning semantics

Those are downstream lane responsibilities, but this document defines the control plane they plug into.

## 2. ScratchBird Current-State Baseline

### 2.1 Observed planner shape

Current ScratchBird planning is a staged cost-based planner with strong local instrumentation but no memoized equivalence framework.

Observed flow:

1. `V3SemanticAnalyzer::resolveSelect` resolves a `SelectStmt` into `ResolvedSelectQuery`, `ResolvedRelation`, and `ResolvedJoin` structures.
2. `QueryPlanner::buildSelectPlan` enumerates scan choices per base relation, records considered and rejected choices, and commits one `BaseAccessChoice` per relation.
3. `JoinOrderingOptimizer` receives only the chosen base `Path` for each relation, not the full access-path frontier.
4. The join-order backend picks one best path per subset using exhaustive DP, bounded DP, hypergraph-greedy, heuristic-greedy, or input-order fallback.
5. `buildSubtreeJoinDecision` rematerializes the frozen join tree and re-evaluates join methods on that fixed tree shape.
6. Aggregation, DISTINCT, windowing, sorting, limit, gather, and runtime-plan diagnostics are attached after join search.

### 2.2 Existing strengths

ScratchBird already has several building blocks that a memo framework can reuse instead of replacing.

Strengths directly visible in current code:

- A resolved-query layer already separates semantic resolution from plan construction.
- `AccessPathDescriptor` already carries partial physical-property information: ordering state, ordered prefix length, parameterization, required outer relation indexes, and an interesting-order score.
- `CostEstimate` already carries more than a single scalar: startup cost, total cost, rows, memory bytes, memory budget, spill flags, spill passes, spill bytes, and cost-formula provenance.
- Join legality is already explicit through `JoinLegalityClass`, join-method legality checks, and barrier flags for outer, semi, anti, USING, NATURAL, and lateral cases.
- Runtime diagnostics are unusually rich for this maturity level: `RuntimePlanSearchSummary`, `considered_paths`, `rejected_paths`, statistics provenance, optimizer controls, and bounded adaptive feedback already exist.
- The join backend already supports multiple search modes and budgets, so search throttling is not new to the codebase.

### 2.3 Current structural limits

The current design falls short of top-tier optimizer architecture in four important ways.

1. No memo or equivalence classes for logical alternatives
Current planning operates over one resolved query tree. Equivalent expressions are not inserted into shared groups, so rewrites cannot feed a common search structure.

2. No retained frontier per relation or join subset
Base-relation enumeration considers many candidates, but only one `best_path` survives for downstream join ordering. Join-order DP also keeps only one `DPEntry` per subset. This means path shape, ordering, parameterization, and row-goal tradeoffs are discarded early.

3. Join order and join method are not jointly optimized in one search state
The join-order backend chooses one composite tree using nested loop versus hash in subset DP. Merge join and detailed method selection are reconsidered only after the join tree shape is frozen. This is materially weaker than a property-aware memo or frontier search.

4. Rewrite support is fragmented and shallow
Current code performs:

- simple derived-table pass-through flattening
- simple local predicate extraction
- partition pruning and runtime-filter attachment

Current code does not provide:

- an explicit rewrite rule registry
- on-demand exploration of equivalent logical forms
- a formal distinction between transformation rules, implementation rules, and enforcer rules
- winner caching keyed by required physical properties

### 2.4 What exists but is not yet integrated

ScratchBird already contains optimizer-adjacent modules that are architecturally important but not yet wired into the main planning flow:

- `MVRewriter` exists, but is marked V3-pending and is not part of `buildSelectPlan`
- `CSEOptimizer` exists as a separate expression optimizer, not as a memo rule
- statistics, selectivity, memory, spill, and plan payload structures are richer than the control architecture currently uses

### 2.5 Baseline conclusion

ScratchBird is not starting from zero. It already has:

- a resolved-query front end
- path and plan node abstractions
- cost and diagnostics contracts
- legality modeling

But it does not yet have the one thing Lane A is responsible for: a shared search memory that retains equivalence, property-specific winners, and reusable optimization work.

Inference: the current planner is best described as a capable staged cost-based planner with partial physical-property metadata and strong observability, but without a search architecture that can scale to deep rewrite and property-aware exploration.

## 3. Donor-Engine Research Synthesis

### 3.1 PostgreSQL

PostgreSQL's planner is not a Cascades memo optimizer, but it is a mature example of disciplined path-frontier planning.

Observed architectural traits:

- `RelOptInfo` owns a `pathlist`, `partial_pathlist`, `ppilist`, and explicit cheapest-path selections.
- Parameterized paths are first-class, not an afterthought.
- Required ordering is modeled through pathkeys and related helper APIs.
- Join search is built around levels of join relations (`join_rel_level`) and dynamic programming over legal joins.
- Uniqueness, equivalence-class participation, lateral dependencies, and grouping/upper-rel planning have dedicated planner data structures.
- PostgreSQL also has a `Memoize` plan node, but that is executor-time caching for parameterized scans, not a search memo for optimization.

What PostgreSQL contributes to ScratchBird:

- a concrete example of separating relation state from individual path state
- a proven property vocabulary for ordering, parameterization, uniqueness, and partition-aware planning
- a good template for keeping multiple candidate paths alive even without full memoization

What PostgreSQL does not solve for ScratchBird:

- it does not provide a full memo/group framework for rewrite search
- it does not natively unify rewrite exploration and physical implementation search the way Cascades does
- its path-frontier style alone would still leave ScratchBird with a weaker rewrite architecture than desired

### 3.2 MySQL

MySQL's hypergraph join optimizer is the closest donor for bounded search over a richer alternative frontier without committing to a full classical Cascades engine.

Observed architectural traits:

- planning converts a query block into a join hypergraph
- `EnumerateAllConnectedPartitions()` implements DPhyp-style connected-subgraph enumeration
- `CostingReceiver` receives subplans, costs them, and retains the best surviving `AccessPath` alternatives
- `ProposeAccessPath()` compares new candidates against existing candidates and prunes dominated paths
- access-path comparison is property-aware, not just scalar-cost-aware
- interesting orders and functional dependencies are explicit planner state
- root candidates survive past core join enumeration and can still compete when ORDER BY, GROUP BY, LIMIT, and filter expansion are applied

Important MySQL lesson:

The code explicitly notes that a single "best" path per subplan is insufficient once more dimensions matter. That is directly relevant to ScratchBird, because ScratchBird already has startup cost, spill behavior, parameterization, and ordering metadata but currently discards most of it.

What MySQL contributes to ScratchBird:

- DPhyp-style exact enumeration for reorderable join components
- Pareto-frontier retention rather than scalar winner-only search
- explicit handling of interesting orders and functional dependencies
- a strong pattern for keeping post-join properties in the same search universe as base access paths

What MySQL does not contribute directly:

- it is still not a full logical memo framework for rewrite search
- its control flow is more specialized around query-block join planning than a general logical/physical memo

### 3.3 DuckDB

DuckDB is not a memo optimizer either, but it is the clearest donor for optimizer staging.

Observed architectural traits:

- a large ordered rewrite pipeline runs before and after join ordering
- expression simplification, filter pullup/pushdown, IN rewriting, join elimination, TopN rewriting, late materialization, and statistics propagation are explicit optimizer passes
- join ordering is a dedicated service, not the whole optimizer
- join ordering runs exactly where it should: after important rewrites and before later physical cleanups
- exact search falls back to approximate search when the pair count grows too large

What DuckDB contributes to ScratchBird:

- a staged rewrite-first architecture
- the lesson that not all optimizer logic belongs inside the core join enumerator
- a practical separation between rule pipeline, join-order service, and later physical cleanup

What DuckDB does not contribute directly:

- no memoized equivalence search
- no generalized winner cache keyed by required physical properties

### 3.4 Cross-donor synthesis

The donor engines converge on one conclusion:

- PostgreSQL shows how to keep planner state and path frontiers disciplined.
- MySQL shows how to retain incomparable alternatives and integrate hypergraph join search.
- DuckDB shows that aggressive rewrite staging should exist outside pure join enumeration.

No one donor matches ScratchBird's target by itself. The recommended ScratchBird design should therefore be:

- Cascades-style memo and task model for equivalence and property-driven search
- MySQL-style hypergraph join enumeration inside reorderable join components
- PostgreSQL-style explicit property and parameterization vocabulary
- DuckDB-style pre-memo and post-memo rewrite staging

## 4. Primary Literature and Official-Document Synthesis

### 4.1 Volcano

The Volcano literature contributes three durable ideas:

- separate logical algebra from physical algorithms
- make physical properties explicit optimizer goals
- use top-down, goal-directed dynamic programming instead of expanding every possibility eagerly

Volcano also treats enforcers such as sort as special physical operators that exist to satisfy required properties. This matters for ScratchBird because current sort, gather, and materialization decisions are not represented as a unified enforcer concept.

Most important implication from Volcano:

- optimization should be driven by requested properties and cost bounds, not just by enumerating locally cheap plans

### 4.2 Cascades

The Cascades paper is the clearest literature source for the memo architecture ScratchBird should target.

Key ideas directly applicable to ScratchBird:

- a `memo` structure stores groups of equivalent expressions
- optimization and exploration are different tasks
- optimization goals are keyed by group or expression plus cost limit plus required and excluded physical properties
- duplicate elimination happens when new expressions are inserted into the memo
- logical exploration happens on demand, not by fully expanding all equivalences first
- rules are objects with promise/guidance, so the engine can schedule promising work earlier
- enforcers are inserted through normal rules, not through a hard-coded side channel

Most important implication from Cascades:

- ScratchBird needs a memo keyed by equivalence groups and property requests, not just a better join-order function

### 4.3 Join-order literature

The DPhyp literature contributes the strongest first-pass answer for join-order architecture inside a larger optimizer.

Direct takeaways:

- represent complex predicates and some non-inner join constraints as hyperedges
- enumerate connected subgraph / connected complement pairs instead of testing all partitions
- keep dynamic programming exact for reorderable join components as long as the search budget allows
- use graph structure to avoid generating impossible or obviously non-connecting joins

Most important implication from DPhyp:

- the best first memo-enabled ScratchBird should not use pure Cascades-style generic join exploration for every inner-join component; it should call a hypergraph join enumerator as a specialized implementation service inside the memo

### 4.4 Order and grouping literature

The order-optimization literature, as reflected both in MySQL's implementation comments and the referenced papers, reinforces one critical point: ordering and grouping should be first-class planner state, not a single scalar "interesting order" score.

Inference: ScratchBird can keep a lightweight scalar for tie-breaking, but the actual memo contract should treat ordering, grouping compatibility, and functional-dependency-derived order states as canonical properties or canonical property-derived states.

### 4.5 Lane A synthesis

The literature does not point to "full Cascades everywhere" as the only serious answer. It points to a hybrid:

- use Cascades for equivalence, rules, memoization, enforcers, and property-driven optimization tasks
- use DPhyp-style exact join enumeration inside reorderable join groups
- use property-aware frontier retention rather than single scalar winners

Inference: for ScratchBird, the target architecture should be a Cascades-inspired memo shell with specialized join-component enumeration, not a textbook-pure reimplementation of either Selinger, Volcano, or MySQL.

## 5. Normalized Algorithm Packet

### 5.1 Architecture options

| Option | Shape | Advantages | Weaknesses | First-pass verdict |
|---|---|---|---|---|
| A. Harden the current staged planner | Keep current planner, add more rules and heuristics inline | Lowest short-term code churn | Alternatives are still discarded too early; no shared equivalence memory; later lanes would keep adding special cases | Reject as target architecture |
| B. Path frontier only | Keep staged planning but retain multiple `Path` candidates per relation and subset | Easier bridge from current code; good interim step | Still weak on logical rewrites and enforcers; still not a general equivalence framework | Adapt as migration stage, not end state |
| C. Staged logical memo plus physical frontier memo | Normalize query to canonical algebra, insert into memo groups, optimize by required properties, retain incomparable winners per request | Best fit for later lanes; handles rewrites, enforcers, orders, parameterization, and diagnostics coherently | Higher implementation cost than A/B | Recommended target architecture |
| D. Full day-one Cascades replacement | Replace current planner core at once with full memo search everywhere | Theoretically cleanest | High delivery risk; destabilizes current planner and diagnostics; too much simultaneous surface area | Defer |
| E. Equality saturation / e-graph first | Use e-graph style equivalence saturation as the primary optimizer | Strong rewrite exploration | Harder cost/property integration for a first production wave; likely too much search explosion without mature extraction strategy | Defer |

### 5.2 Recommended target architecture

Recommended architecture: Option C, a staged logical memo plus physical frontier memo.

Target shape:

```text
Resolved query / canonical relational algebra
  -> normalization pass
  -> pre-memo rewrite batch
  -> memo(groups, expressions, logical properties)
  -> optimize(group, required_properties, cost_limit, row_goal)
       -> on-demand logical exploration
       -> implementation rule expansion
       -> enforcer insertion
       -> hypergraph join enumeration for reorderable join components
       -> frontier pruning and winner caching
  -> lower winning memo expressions to current Path/PlanNode layer
  -> emit RuntimePlan diagnostics
```

Core design decisions:

- Keep `Path` and `PlanNode` initially as lowering targets, not as the long-lived search memory.
- Introduce a canonical memo over logical and physical expressions.
- Key winners by required properties, parameter scope, and row-goal class.
- Keep a small frontier of incomparable winners instead of a single scalar winner.
- Treat sort, gather, gather-merge, materialize, and distinct/aggregate reshaping as enforcers.
- Call a join hypergraph enumerator when optimizing a reorderable join group, rather than forcing join ordering to emerge only from generic rule recursion.

### 5.3 Memo vocabulary

Required normalized vocabulary for ScratchBird:

| Term | Meaning |
|---|---|
| Group | An equivalence class of logical results with identical output semantics |
| Expression | A logical or physical operator instance whose children are group references |
| Logical properties | Output columns, keys, FDs, equivalence classes, nullability, duplicate behavior, cardinality estimate, estimate confidence, lateral dependencies, barrier flags |
| Physical properties | Ordering, grouping compatibility, partitioning/distribution, rewindability, parameterization scope, parallel shape, materialization state |
| Request | A group plus required physical properties, row goal, and cost limit |
| Winner | Best known physical implementation of a group for a request |
| Frontier | The set of non-dominated winners for the same group under comparable requests |
| Enforcer | A physical operator inserted only to satisfy a required property |
| Rule | A transformation, implementation, or enforcer mapping with promise and legality checks |

### 5.4 Group and expression state model

Minimum state model:

```text
Group
  group_id
  logical_properties
  logical_expr_ids
  physical_expr_ids
  winner_cache[request_key] -> frontier
  explored_patterns
  diagnostics_counters

Expression
  expr_id
  expr_kind = LOGICAL | PHYSICAL | ENFORCER
  operator_tag
  child_group_ids
  scalar_payload_fingerprint
  derived_logical_properties
  delivered_physical_properties
  fired_rule_ids
  lower_bound_cost
  insertion_origin
```

Key rule:

- expressions are deduplicated structurally on insert
- groups are deduplicated semantically by expression integration
- a group owns winners; an expression never owns "global best plan" status outside a request context

### 5.5 Property model

Recommended logical properties:

- output column set
- candidate key set
- functional dependency set
- equivalence classes
- nullability vector
- duplicate-preservation flag
- max-one-row flag
- barrier flags for outer, semi, anti, lateral, and non-reorderable semantics
- cardinality estimate and confidence class

Recommended physical properties:

- ordering state
- grouping-compatibility state
- partitioning or distribution state
- rewindability or rescannability
- parameterization scope
- parallel eligibility and preferred parallel shape
- materialization requirement

Inference: the existing `AccessPathDescriptor` should be treated as the seed of the physical-property API, but the scalar `interesting_order_score` should be demoted to a tie-breaker and never remain the canonical representation of ordering.

### 5.6 Rule firing model

Recommended rule classes:

| Rule class | Purpose |
|---|---|
| Normalize | Canonicalize commutative and associative forms, flatten conjunctions, standardize scalar fingerprints |
| Transform | Derive equivalent logical forms |
| Implement | Map a logical expression to one or more physical expressions |
| Enforce | Insert sort, gather, materialize, or similar operators when required properties are missing |

Recommended firing sequence:

1. Normalize the initial logical tree before memo insertion.
2. On `OptimizeGroup(group, request)`, first check winner cache.
3. If the request is new, run `ExploreGroup(group, pattern/request)` lazily.
4. Fire transformation rules only when they can contribute to the current request class.
5. Fire implementation rules for matching logical expressions.
6. If no implementation delivers required properties, consider enforcer rules.
7. Optimize child groups with derived input requests.
8. Insert resulting winners into the group's frontier, prune dominated alternatives, and publish the best winner.

Cycle and termination controls:

- canonicalize commutative forms before insertion
- do exact-expression dedupe on memo insert
- track `(expr_id, rule_id, bind_fingerprint)` to avoid repeat fires
- track explored patterns per group
- bound search by cost limit, request budget, and join enumeration budget

### 5.7 Specialized join-component search

The memo should not treat every join group as an unstructured binary tree search problem.

Recommended behavior:

- build a join hypergraph for reorderable inner join components
- keep outer, semi, anti, natural, USING, and lateral barriers outside that free-reorder component
- represent complex predicates as hyperedges when a predicate references more than two relations
- run exact DPhyp-style enumeration while the budget allows
- fall back to bounded or heuristic search when the exact budget is exhausted

This yields:

- better search quality than current greedy fallback
- lower complexity than naive exhaustive partition testing
- compatibility with current join-legality modeling

### 5.8 Winner selection and pruning

ScratchBird should not keep only one winner per group. It should keep:

- one best winner for each exact request key
- a small frontier of non-dominated winners for nearby but not identical property states

Dominance should compare at least:

- delivered physical properties
- total cost
- startup cost
- memory footprint
- spill expectation
- parameter scope
- rewindability

Recommended pruning concepts:

- exact-request winner cache
- dominance pruning within a frontier
- branch-and-bound pruning from incumbent winner cost
- lower-bound pruning on partial implementations
- budget-based join enumeration cutover
- property-relaxation pruning when an enforcer is known to be cheaper than exploring additional native-property alternatives

Inference: ScratchBird should begin with very small frontiers, because current diagnostics and cost model are already detailed enough to benefit from multiple winners, but the codebase does not yet have the cardinality or cost maturity to justify very wide frontier retention.

### 5.9 Recommended migration strategy from current ScratchBird planning

Migration should be staged, not revolutionary.

Phase 0: Stabilize current interfaces

- freeze current `ResolvedSelectQuery`, `Path`, `PlanNode`, and `RuntimePlan` lowering contracts
- add explicit conversion hooks so a future memo winner can lower through the current plan path

Phase 1: Formalize properties around current paths

- lift ordering, parameterization, memory, spill, and parallel metadata into reusable property structs
- key current path winners by a formal request object even before memo groups exist

Phase 2: Retain base and subset frontiers

- replace single `best_path` retention with a bounded frontier per base relation and join subset
- keep current join-order backend, but allow multiple input alternatives

Phase 3: Introduce logical memo

- normalize resolved queries into canonical algebra nodes
- insert them into groups
- support a small rule set: projection/predicate normalization, commutativity/associativity where legal, simple decorrelation-safe rewrites from Lane B

Phase 4: Introduce property-driven optimization tasks

- add `OptimizeGroup` and `ExploreGroup`
- move implementation selection and enforcer insertion into the memo

Phase 5: Integrate join hypergraph service

- optimize reorderable join groups by DPhyp-style enumeration inside the memo
- keep barrier joins explicit outside free-reorder components

Phase 6: Expand post-join operators

- represent aggregate, distinct, sort, limit, gather, and materialization alternatives as memo implementations and enforcers instead of post hoc selections

This migration lets ScratchBird reuse its current strengths while replacing only the parts that are structurally limiting.

## 6. Formula and Heuristic Packet

### 6.1 Canonical request and expression keys

Recommended canonical keys:

```text
ExpressionFingerprint =
  hash(operator_tag,
       normalized_scalar_payload,
       child_group_ids,
       expr_kind)

RequestKey =
  hash(group_id,
       canonical_required_properties,
       parameter_scope,
       row_goal_class,
       parallel_class)
```

Where:

- `canonical_required_properties` must remove synonyms and normalize ordering/grouping states
- `parameter_scope` must distinguish unparameterized, lateral, and outer-parameter-dependent requests
- `row_goal_class` must at least distinguish `ALL_ROWS`, `FIRST_ROWS`, `EXISTS`, and `TOP_K`

### 6.2 Dominance heuristic

Recommended initial dominance predicate:

```text
A dominates B iff
  A and B produce the same group semantics
  and A.parameter_scope == B.parameter_scope
  and A.delivered_properties cover B.delivered_properties
  and A.total_cost <= B.total_cost * (1 + eps_total)
  and A.startup_cost <= B.startup_cost * (1 + eps_startup)
  and A.memory_bytes <= B.memory_bytes
  and A.spill_expected <= B.spill_expected
  and A.rewindability is not weaker than B.rewindability
```

Recommended first-pass tolerances:

- `eps_total = 0.01`
- `eps_startup = 0.01`

Inference: small epsilon dominance is safer than exact floating-point equality because ScratchBird already has rich cost expansion and future calibration will introduce more near-ties.

### 6.3 Lower-bound heuristic

Recommended lower bound for partial implementations:

```text
lower_bound(expr, request) =
  local_min_cost(expr)
  + sum(child_best_lower_bounds(child_request_i))
  + min_required_enforcer_cost(expr, request)
```

Prune when:

```text
lower_bound(expr, request) >= incumbent_best.total_cost * (1 + prune_eps)
```

Recommended first-pass value:

- `prune_eps = 0.00` for exact search
- `prune_eps = 0.02` for bounded search

### 6.4 Winner ranking heuristic

The frontier should use dominance first and scalar ranking second.

Recommended ranking score for choosing a preferred winner among non-dominated candidates:

```text
winner_score =
  total_cost
  + row_goal_weight * startup_cost
  + spill_penalty
  + confidence_penalty
  + enforcement_penalty
```

Suggested components:

- `row_goal_weight = 0` for `ALL_ROWS`
- `row_goal_weight > 0` for `FIRST_ROWS`, `EXISTS`, and `TOP_K`
- `spill_penalty` should be very large when spill is forbidden, moderate otherwise
- `confidence_penalty` should grow when estimates are stale or low-confidence
- `enforcement_penalty` should bias toward native property delivery when costs are otherwise near-equal

### 6.5 Rule scheduling heuristic

Recommended promise heuristic:

```text
promise(rule, expr, request) =
  structural_gain
  + property_gain
  + estimated_selectivity_gain
  - duplicate_risk
  - legality_risk
  - barrier_penalty
```

Interpretation:

- `structural_gain`: does the rule simplify shape or expose a known implementation?
- `property_gain`: does it expose order, grouping, or parameterization advantages?
- `estimated_selectivity_gain`: does it push filters or tighten a join component?
- `duplicate_risk`: is the result likely already in the memo?
- `legality_risk`: does the rule frequently fail on NULL or outer-join semantics?

### 6.6 Join search budget heuristic

Recommended first-pass thresholds:

- exact hypergraph DP for reorderable components up to 10 relations
- bounded exact search up to 16 relations if connected-subgraph pair count remains within budget
- hypergraph greedy fallback beyond that

Inference: these thresholds are not final formulas; they are a practical bridge from current ScratchBird defaults and donor-engine practice until Lane F and Lane G supply calibrated search-budget and cost-quality guidance.

### 6.7 Frontier-size heuristic

Recommended first-pass retention:

- keep 1 exact-request best winner
- keep at most 4 additional non-dominated winners per `(group, order_state, parameter_scope)` bucket

Inference: this cap is intentionally conservative. The immediate goal is to stop losing critical alternatives, not to maximize frontier width on day one.

## 7. ScratchBird Contract Draft

### 7.1 Contract intent

This contract is a first-pass implementation sketch for the memo subsystem boundary. It is not a final C++ header set, but it is specific enough to guide downstream specification and coding.

### 7.2 Core types

```cpp
enum class RulePhase : uint8_t {
    NORMALIZE,
    EXPLORE,
    IMPLEMENT,
    ENFORCE
};

struct LogicalProperties {
    ColumnSet output_columns;
    KeySet candidate_keys;
    FunctionalDependencySet functional_dependencies;
    EquivalenceClassSet equivalence_classes;
    NullabilitySet nullability;
    bool duplicate_preserving = true;
    bool max_one_row = false;
    bool has_lateral_dependency = false;
    JoinBarrierFlags barrier_flags;
    CardinalityEstimate row_estimate;
    EstimateConfidence estimate_confidence;
};

struct PhysicalProperties {
    OrderingState ordering;
    GroupingState grouping;
    DistributionState distribution;
    RewindabilityState rewindability;
    ParameterScope parameter_scope;
    ParallelShape parallel_shape;
    bool materialized = false;
};

struct OptimizationRequest {
    GroupId group_id;
    PhysicalProperties required;
    RowGoal row_goal;
    CostLimit cost_limit;
};

struct Winner {
    ExprId expr_id;
    OptimizationRequest request;
    PhysicalProperties delivered;
    CostEstimate cost;
    uint64_t memory_bytes = 0;
    bool spill_expected = false;
    TraceToken trace_token;
};
```

### 7.3 Memo API

```cpp
class Memo {
public:
    GroupId insertLogical(LogicalExpr expr);
    ExprId insertPhysical(PhysicalExpr expr);

    const GroupState& group(GroupId id) const;
    GroupState& group(GroupId id);

    WinnerSet& frontier(const OptimizationRequest& request);
    std::optional<Winner> best(const OptimizationRequest& request) const;

    void recordRuleFire(ExprId expr_id, RuleId rule_id, RuleBindingKey binding);
    bool alreadyFired(ExprId expr_id, RuleId rule_id, RuleBindingKey binding) const;
};
```

### 7.4 Rule API

```cpp
class Rule {
public:
    RuleId id() const;
    RulePhase phase() const;
    Pattern antecedent() const;
    PromiseScore promise(const Memo&, ExprId, const OptimizationRequest&) const;
    bool legal(const Memo&, ExprId, const OptimizationRequest&) const;
    std::vector<DerivedExpr> apply(const Memo&, ExprId, const OptimizationRequest&) const;
};
```

Required rule families:

- normalization rules
- logical transformation rules
- implementation rules
- enforcer rules

### 7.5 Optimizer task API

```cpp
class OptimizerSearch {
public:
    Winner optimizeGroup(const OptimizationRequest& request);
    void exploreGroup(GroupId group_id, const ExplorationPattern& pattern);
    void optimizeExpression(ExprId expr_id, const OptimizationRequest& request);
    void applyRule(ExprId expr_id, RuleId rule_id, const OptimizationRequest& request);
};
```

Required behavior:

- `optimizeGroup()` must be idempotent for the same request key
- `exploreGroup()` must be lazy and pattern-scoped
- `optimizeExpression()` must derive child requests instead of assuming child property independence
- `applyRule()` must integrate derived expressions back into the memo before further scheduling

### 7.6 Join component service API

```cpp
struct JoinComponentRequest {
    GroupId group_id;
    JoinHypergraph hypergraph;
    OptimizationRequest request;
    JoinSearchBudget budget;
};

class JoinComponentEnumerator {
public:
    std::vector<PhysicalExpr> enumerate(const JoinComponentRequest& request);
};
```

Required behavior:

- only free-reorder join components use this service
- barrier joins remain explicit logical operators or constrained hyperedges
- returned join skeletons must still enter the memo and compete against other implementations

### 7.7 Lowering contract with current ScratchBird code

The first implementation wave should preserve current downstream surfaces.

Required compatibility rules:

- a chosen memo winner must lower to the current `Path` abstraction or a thin successor abstraction
- current `PlanNode` lowering remains valid during migration
- current `RuntimePlan` emission remains the canonical external diagnostics surface
- existing `CostEstimate` stays the first-wave physical costing payload

This keeps current EXPLAIN and adaptive-feedback plumbing alive while the planner core changes.

### 7.8 Diagnostics contract

The memo architecture should extend, not replace, current diagnostics.

Additions recommended for the runtime plan and trace contract:

- memo group count
- memo expression count
- winner-request count
- dominance-pruned candidate count
- lower-bound-pruned candidate count
- rule fire count by phase
- enforcer insertion count
- join-component enumeration mode and budget usage

Each considered or rejected path trace should gain:

- `group_id`
- `expr_id`
- `request_key`
- `rule_id` when rule-derived

Inference: ScratchBird's existing runtime plan payload is already strong enough that a memo architecture should be designed to enrich that contract, not to invent a second parallel diagnostics path.

## 8. Validation and Benchmark Packet

### 8.1 Functional validation

Required functional tests:

- memo deduplicates equivalent commuted and reassociated inner joins
- enforcer rules insert sort or materialize only when required properties are not already delivered
- parameterized and unparameterized requests do not share winners incorrectly
- outer, semi, anti, NATURAL, USING, and lateral barriers are preserved
- rule firing terminates without duplicate-expression loops

### 8.2 Property validation

Required property tests:

- ordering propagation through scan, merge join, sort, gather-merge, and aggregate
- parameter-scope propagation for lateral and nested-loop dependent inner paths
- rewindability propagation through materialize versus streaming operators
- spill metadata propagation into winner selection

### 8.3 Join search validation

Required join tests:

- chain joins
- star joins
- clique joins
- disconnected join graphs requiring explicit cross joins
- complex predicate hyperedges
- non-inner join barrier cases

For each test, validate:

- chosen join skeleton
- method legality
- whether exact or fallback search was used
- number of candidates pruned by dominance and by budget

### 8.4 Migration regression validation

During migration, compare:

- current planner chosen plan versus memo-lowered chosen plan
- current runtime diagnostics versus new diagnostics
- optimization latency for simple queries
- plan quality for multi-join queries

Required first migration gate:

- no semantic regression on existing plan correctness tests
- no diagnostic regression in EXPLAIN contract shape without an intentional version bump

### 8.5 Benchmark corpus

Recommended benchmark families:

- single-relation access-path cases
- small OLTP-style join graphs
- larger analytic join graphs
- ORDER BY and GROUP BY property-sensitive queries
- LIMIT and EXISTS row-goal-sensitive queries
- spill-prone hash and sort workloads
- lateral or parameterized nested-loop cases

Recommended measurement set:

- optimization time
- memo groups and expressions created
- winners retained
- candidates pruned
- final estimated cost
- actual execution quality when executable workloads are available

### 8.6 Acceptance criteria for Lane A architecture adoption

The architecture is ready for downstream implementation work when all of the following are true:

- a memo request can produce a winner keyed by required properties
- at least one enforcer rule works end-to-end
- at least one logical rewrite and one implementation rule compete inside the memo
- reorderable joins can be optimized through a dedicated join-component service
- memo diagnostics are visible through the runtime plan contract

## 9. Adopt/Adapt/Reject/Defer Matrix

| Concept | Primary donor | Disposition | Reason |
|---|---|---|---|
| Cascades memo groups and on-demand exploration | Cascades | Adopt | This is the clearest answer to ScratchBird's missing equivalence and request-caching architecture |
| Cascades task model (`OptimizeGroup`, `ExploreGroup`, `ApplyRule`) | Cascades | Adopt | ScratchBird needs explicit search tasks, not more nested planner control flow |
| Enforcer rules as normal optimizer rules | Volcano, Cascades | Adopt | Sort, gather, materialize, and similar operators should be part of the search space |
| Required physical properties as explicit optimization goals | Volcano, Cascades | Adopt | ScratchBird already has property fragments; they need a first-class request contract |
| PostgreSQL-style path frontier discipline | PostgreSQL | Adapt | Strong donor for path-state organization, but not enough alone for rewrite search |
| PostgreSQL pathkeys and parameterized-path thinking | PostgreSQL | Adapt | Good property vocabulary, but should be generalized for ScratchBird rather than copied literally |
| PostgreSQL `Memoize` executor node as optimizer architecture | PostgreSQL | Reject | Execution-time result caching is not a substitute for optimizer memo groups |
| Hypergraph join enumeration over reorderable components | MySQL, DPhyp | Adopt | Best exact-search donor for complex predicates and join graphs |
| Interesting orders plus FD-aware order state | MySQL, order literature | Adapt | Strong concept, but ScratchBird should model canonical property state rather than only MySQL's ordering-state machinery |
| Single scalar winner per subplan | Current ScratchBird, current MySQL limitation note | Reject | ScratchBird already has enough cost and property dimensions that one scalar winner is too lossy |
| Rewrite pipeline before core join search | DuckDB | Adopt | Useful for cheap normalizations and clearly beneficial rewrites before memo search |
| DuckDB-style non-memo optimizer as final target | DuckDB | Reject | Good staging model, insufficient final architecture for Lane A requirements |
| Full day-one optimizer replacement | None | Defer | Delivery risk is too high relative to incremental migration value |
| Equality saturation / e-graph mainline optimizer | Recent literature | Defer | Interesting long-term direction, but not the right first-wave architecture for ScratchBird |

## 10. Open Questions and Integration Dependencies

### 10.1 Open questions

- How large should the first-wave frontier caps be once real cost and cardinality quality are measured?
- Should grouping compatibility be represented as a separate property state or as a sub-state of ordering?
- How much of current `Path` survives after memo lowering versus becoming a compatibility shim?
- Should row-goal classes be represented as required properties or as request modifiers outside the property set?
- How should memo winners be invalidated when adaptive feedback requests replan or statistics refresh?

### 10.2 Lane dependencies

Lane A depends on later lanes for precision, but not for first-pass architecture.

Required downstream integration:

- Lane B: legal rewrite catalog, phase boundaries, and proof obligations
- Lane C: logical property and statistics storage needed for keys, FDs, and confidence
- Lane D: request-time cardinality confidence and correction-factor semantics
- Lane F: final exact versus bounded join-search thresholds and fallback policy
- Lane G: multi-dimensional cost comparison and calibration
- Lane H: parallel property vocabulary and enforcers
- Lane K: runtime-plan trace extensions and observability schema versioning

### 10.3 Risk register

- Biggest architecture risk: building a memo but continuing to throw away alternatives early due to overly narrow winner retention
- Biggest delivery risk: attempting a full planner rewrite before stabilizing lowering and diagnostics contracts
- Biggest correctness risk: under-modeling barrier semantics for outer, semi, anti, and lateral joins

## 11. Recommended Next-Step Specification Tasks

1. Write a dedicated memo contract spec that freezes group, expression, request, winner, and rule interfaces.
2. Write a physical-property model spec that formalizes ordering, grouping, parameterization, rewindability, and parallel shape.
3. Write a join-component enumeration spec that defines how reorderable join groups are converted into hypergraphs and when exact search falls back.
4. Write a rule task model spec that defines normalization, transformation, implementation, and enforcer phases.
5. Write a migration spec mapping each current `QueryPlanner` responsibility to memo-era responsibilities.
6. Write a diagnostics versioning spec that extends the runtime plan payload with memo counters and request identifiers.
7. Hand this Lane A output to Lane B, Lane F, Lane G, and Lane K as the control-plane baseline for their drafts.
