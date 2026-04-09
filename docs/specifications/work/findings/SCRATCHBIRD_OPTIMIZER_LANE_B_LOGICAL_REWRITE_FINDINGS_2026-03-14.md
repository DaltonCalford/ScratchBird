# ScratchBird Optimizer Lane B Findings

Lane: B

Topic: Logical rewrite system

Status: First-pass findings

Date: 2026-03-14

Primary planning inputs:
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_PROGRAM_2026-03-14.md`
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_AGENT_OPERATIONS_2026-03-14.md`

## 1. Scope and Lane Objective

Lane B should define a pre-physical logical rewrite subsystem for ScratchBird that runs after semantic resolution and before physical access-path and join-path enumeration. The first-pass objective is not a full memo/Cascades implementation. The objective is a bounded, staged rewrite pipeline that:

- normalizes and simplifies expressions and relational operators
- derives proof-bearing logical properties needed by later planning
- rewrites subqueries and joins into planner-friendly canonical forms
- exposes legality barriers explicitly instead of relying on scattered planner heuristics

The minimum first-wave rule families are:

- predicate pushdown
- projection pushdown
- transitive predicate inference
- subquery unnesting and decorrelation
- semijoin and antijoin conversion
- join elimination
- aggregation pushdown
- DISTINCT simplification
- constant folding
- predicate and expression normalization
- rule ordering, legality checking, and proof recording

The ScratchBird-specific success condition for Lane B is a rule system that improves plan quality without changing MGA-visible row semantics, duplicate semantics, NULL semantics, or volatile-expression behavior.

## 2. ScratchBird Current-State Baseline

Direct code evidence indicates that ScratchBird already performs a small set of logical rewrites, but they are fragmented across semantic analysis and physical planning rather than exposed as a standalone rewrite subsystem.

Current baseline capabilities:

- Planning is centered in `query_planner.cpp` and remains primarily physical and cost-oriented. It resolves relations, chooses access paths, builds join plans, and then chooses upper operators for aggregation, windows, and DISTINCT.
- `v3_semantic_analyzer.cpp` can flatten only trivial pass-through derived tables, subqueries, and views. The flattening gate is narrow: no `WITH`, no `DISTINCT`, no joins, no `WHERE`, no grouping, no windows, no ordering, no limits, and the select list must be `*` or `table.*`.
- Simple scan-local predicate extraction exists. `collectSimplePredicates()` recognizes simple binary comparisons and LIKE-prefix cases, splits on top-level `AND`, and only extracts local predicates when outer-join semantics are not involved.
- Column pruning exists in partial form. `collectRequiredColumnIds()` walks the select, group, having, order, and local-predicate expressions to compute required column IDs per relation.
- Partition pruning exists and is driven from extracted local predicates.
- Join legality metadata already exists. The semantic layer classifies join properties such as row preservation, null introduction, and some reorderability barriers.
- Aggregation and DISTINCT are currently handled as upper-stage operator choices rather than semantic simplifications. The planner chooses between alternatives such as ordered versus hash distinct and group aggregate versus hash aggregate.
- Predicate implication machinery exists in `predicate_matcher.cpp`, but it is narrow and aimed at filtered-index and partial-index matching. It supports exact conjunct containment and some simple same-column range implication.
- `mv_rewriter.cpp` is effectively a stub. `tryRewrite()` returns `nullptr`.
- `cse.cpp` implements expression common-subexpression elimination with volatility and side-effect checks, but no direct evidence was found that it is integrated into the main select-planning path.

Current baseline gaps relative to Lane B scope:

- Predicate pushdown is scan-local only. There is no general operator-by-operator legality matrix.
- Projection pushdown is limited to required-column collection. There is no explicit projection insertion, projection movement, or column-lifetime trimming pass.
- Transitive inference is not generalized into equivalence classes or proof-bearing predicate generation.
- Subquery unnesting is limited to trivial pass-through flattening.
- There is no general EXISTS/IN to semijoin rewrite, NOT EXISTS to antijoin rewrite, or correlated decorrelation pass.
- Join elimination is absent.
- Aggregation pushdown is absent as a logical rewrite.
- DISTINCT simplification is absent. DISTINCT is implemented, but not simplified away when uniqueness is provable.
- Constant folding and normalization are not exposed as an integrated planner rewrite phase.
- Rule ordering, legality preconditions, and proof obligations are implicit and scattered.

Inference:

- ScratchBird today should be described as having logical rewrite fragments, not a logical rewrite system.
- The existing planner shape suggests that a staged pre-physical rewrite layer can be introduced without first requiring a full memo architecture.

## 3. Donor-Engine Research Synthesis

### PostgreSQL

PostgreSQL provides the clearest model for proof-aware rewrite legality.

- `planner.c` shows an explicit preprocessing order before the main grouping and path-generation stages: relation preprocessing, sublink pull-up, subquery pull-up, simple `UNION ALL` flattening, outer-join reduction, and useless-RTE removal.
- `util/clauses.c` exposes `eval_const_expressions()`, which folds constant subexpressions, simplifies boolean structure, and refuses to pre-evaluate non-immutable functions.
- `prep/prepqual.c` exposes `canonicalize_qual()`, which assumes prior constant folding and then removes redundant boolean structure rather than attempting an overly aggressive normal form.
- `optimizer/README` documents equivalence classes, join domains, and the key outer-join caveat: transitive equalities derived inside an outer-join domain cannot be applied everywhere.
- `initsplan.c` processes mergejoinable equalities into equivalence classes and treats outer joins specially.
- `predtest.c` provides predicate implication and refutation routines, with explicit assumptions about immutability and prior normalization.
- `subselect.c` rewrites qualifying `ANY` and `EXISTS` sublinks into semijoins and antijoins with explicit gates for volatility, parent references, and subquery shape.
- `analyzejoins.c` removes useless joins only when uniqueness and non-reference preconditions are proven.
- PostgreSQL handles projection pruning mostly through path-target construction and trimming rather than a free-standing “projection pushdown” pass.

Takeaways for ScratchBird:

- Equivalence classes and proof objects should be first-class.
- Outer-join domains must be represented explicitly.
- Constant folding must be gated by function volatility and semantic safety.
- Join elimination should be proof-driven and conservative.

### DuckDB

DuckDB provides the clearest model for a modular, ordered rewrite pipeline.

- `optimizer.cpp` enumerates an explicit pass order rather than leaving rule order implicit.
- DuckDB’s expression rewriter includes constant folding, arithmetic simplification, conjunction simplification, constant movement, comparison simplification, and DISTINCT-sensitive aggregate cleanup.
- `filter_pushdown.cpp` and its operator-specific helpers implement pushdown with explicit legality gates per operator kind.
- `pushdown_projection.cpp` only pushes filters through projections when expressions are non-volatile and cannot throw.
- `pushdown_aggregate.cpp` pushes group-key filters below aggregates only when they do not depend on aggregate outputs and are safe across grouping sets.
- `filter_pullup.cpp` pulls filters up to create new pushdown opportunities and to duplicate filters over equality relationships.
- `remove_unused_columns.cpp` and `column_lifetime_analyzer.cpp` are strong donors for explicit projection pruning and column-lifetime trimming.
- `flatten_dependent_join.cpp` is a strong donor for correlated subquery decorrelation.
- `join_elimination.cpp` shows a narrower but practical join-removal implementation, currently focused on structurally provable cases.
- `remove_duplicate_groups.cpp`, `distinct_aggregate_optimizer.cpp`, and `common_aggregate_optimizer.cpp` are strong donors for DISTINCT and aggregate simplification.

Takeaways for ScratchBird:

- A first-wave staged pass manager is realistic and valuable even without a full memo.
- Projection pruning should be an explicit subsystem, not just a side effect of scan setup.
- Rule legality should be checked at each operator boundary, not globally.
- Cleanup passes that remove dead columns, duplicate grouping keys, and redundant DISTINCT modifiers materially reduce downstream search space.

### MySQL

MySQL provides the clearest model for explicit candidate gating in subquery rewrites and join simplification.

- `item_subselect.cc` contains a detailed semijoin-candidate checklist covering query simplicity, grouping, HAVING, windows, determinism, truth-value compatibility, and LIMIT/OFFSET behavior.
- `sql_resolver.cc` performs bottom-up subquery flattening, can rewrite some subqueries to joined derived tables, and then converts selected candidates into semijoins or antijoins.
- `sql_resolver.cc` also simplifies joins by converting outer joins to inner joins when later predicates reject NULL-complemented rows.
- `make_join_hypergraph.cc` contains explicit pushdown restrictions for inner joins, outer joins, semijoins, and antijoins, and discusses normalization around multiple equalities.
- `join_optimizer.cc` includes semijoin deduplication logic that can convert semijoin-like work into inner-join-like work plus duplicate removal when appropriate.

Takeaways for ScratchBird:

- Rewrite candidates should be gated by a checklist, not by ad hoc pattern matches alone.
- Outer-to-inner demotion must be explicit because it directly expands later join-order search space.
- NOT IN and other NULL-sensitive forms need separate legality treatment from EXISTS and IN.

Inference:

- Current local MySQL evidence emphasizes subquery rewriting, outer-to-inner simplification, and pushdown legality more than a standalone FK-driven join-elimination framework.

### Cross-Engine Synthesis

The donor engines converge on the same architectural pattern even though their implementations differ:

- normalize first
- derive logical properties next
- apply structurally safe rewrites before physical search
- treat outer joins, semijoins, antijoins, and volatile expressions as hard barriers unless a proof discharges the barrier
- rerun cleanup after major rewrites so the physical planner sees a smaller search space

ScratchBird should adopt that pattern directly.

## 4. Primary Literature and Official-Document Synthesis

The most useful primary-source synthesis for Lane B comes from official engine documentation and classic rewrite literature rather than benchmark folklore.

Official engine documentation and source-design notes:

- PostgreSQL optimizer design notes in `optimizer/README` formalize equivalence classes, join domains, and the rule that equalities derived inside outer joins have restricted scope. This is the strongest donor source for transitive inference and outer-join legality.
- PostgreSQL comments around `eval_const_expressions()`, `canonicalize_qual()`, and `predicate_implied_by()` formalize an important discipline: normalization and proof routines assume immutable expressions and prior simplification.
- The MySQL 8.4 manual section on semijoin and antijoin transformations states that these are preparation-time rewrites and lists concrete execution strategies such as table pullout, duplicate weedout, first match, loose scan, and materialization. That confirms that semijoin conversion is not merely a plan-node choice; it is a rewrite with downstream search consequences.
- The MySQL manual and resolver comments also reinforce that outer-to-inner conversion is valuable because it frees later join ordering.
- DuckDB official internals documentation describes a logical planner followed by optimizer passes including expression rewriting, filter pushdown, and join-order optimization. DuckDB’s public optimizer controls and optimizer inventory are evidence that a modular pass architecture is intended rather than accidental.

Primary literature synthesis:

- Cesar Galindo-Legaria’s “Parameterized Queries and Nesting Equivalencies” models subqueries with an `Apply` operator and shows that arbitrary SQL subqueries can be unnested when duplicate semantics and parameterized execution are represented explicitly. ScratchBird does not need to adopt `Apply` as a user-visible concept, but it should adopt the core lesson: decorrelation requires an explicit algebraic representation of outer references and duplicate semantics.
- Surajit Chaudhuri and Kyuseok Shim’s “Including Group-By in Query Optimization” treats aggregation as a search-space-transforming operator rather than a late decoration. The practical implication for ScratchBird is that aggregation pushdown cannot be an isolated local peephole; it changes join search shape and must therefore record key and fanout proofs.
- The broader optimizer literature summarized in “An Overview of Query Optimization in Relational Systems” supports the same conclusion: rewrite rules are only safe when their algebraic preconditions are explicit, and grouping, subqueries, and duplicates cannot be treated as minor variants of SPJ planning.

Inference:

- The literature family around “Orthogonal Optimization of Subqueries and Aggregation” further supports handling subquery decorrelation and aggregate movement in one algebraic framework, but this first-pass draft did not complete a full paper-level extraction from that source.

Lane-B implication:

- ScratchBird should not encode rewrite legality as parser-specific SQL special cases.
- ScratchBird should encode legality as algebra-level contracts over nullability, duplicates, volatility, and join domain.
- First-wave rewrite infrastructure can be staged and non-memoized, but it still needs formal proof artifacts to avoid donor-style edge-case regressions.

## 5. Normalized Algorithm Packet

### 5.1 Proposed rewrite pipeline

First-wave recommendation: introduce a bounded `LogicalRewriteManager` between semantic analysis and physical planning.

Phase order:

1. Canonicalize expressions and predicates.
2. Derive logical properties and proof artifacts.
3. Apply local pushdowns and local simplifications.
4. Apply relational rewrites that change join and subquery structure.
5. Apply cleanup rewrites and dead-operator removal.
6. Re-run canonicalization and stop at a bounded fixpoint.

This is intentionally staged. The goal is predictable rule ordering and traceability, not full search-space saturation.

### 5.2 Rule families

#### A. Constant folding and normalization

Input patterns:

- scalar expressions
- boolean predicate trees
- join conditions
- derived-table filters

Actions:

- flatten nested `AND` and `OR`
- normalize commutative comparison order
- push simple constants to a canonical side where safe
- fold constant subexpressions
- remove tautologies and contradictions
- collapse duplicate conjuncts
- normalize `NOT` around booleans into a stable internal form

Legality:

- fold only pure, deterministic, immutable expressions
- do not pre-evaluate volatile, nondeterministic, or side-effecting functions
- do not reorder or pre-evaluate expressions if doing so can surface exceptions earlier than the unfused expression would

Proof obligations:

- `PurityProof`
- `DeterminismProof`
- `NoEarlyExceptionProof` or conservative refusal

#### B. Predicate pushdown

Input pattern:

- `Filter(p, child)`

Actions:

- push scan-local predicates into scans and partition-pruning state
- push predicates through projections after column remapping
- split predicates around joins into left-only, right-only, and join predicates
- pull predicates above inner joins when needed to duplicate through equivalence classes, then push again
- push group-key-only predicates below aggregates when safe

Legality:

- predicate references must be available in the target child after remapping
- projection pushdown is blocked by volatile or exception-throwing expressions
- inner joins allow left-only and right-only pushdown freely
- left/right/full joins require null-introduction checks and join-domain checks
- semijoins and antijoins push differently: left-only filters move freely, right-side movement is restricted
- DISTINCT and grouping operators only allow pushdown for expressions that preserve duplicate semantics

Proof obligations:

- `ColumnAvailabilityProof`
- `JoinDomainProof`
- `NullRejectingProof` for outer-join demotion or aggressive movement
- `DuplicateSafetyProof`

#### C. Projection pushdown

Input pattern:

- any operator with a child producing more columns than are needed above

Actions:

- compute required columns from parent demand, local expressions, join keys, grouping keys, ordering, distinct keys, window expressions, and runtime filter needs
- insert or move projections so that scans, joins, and derived-table outputs carry only required columns
- trim dead columns after each major rewrite

Legality:

- projections cannot drop columns needed by later predicates, keys, orderings, grouping, or proof derivations
- for joins, some dropped columns may still be needed transiently to prove uniqueness or null rejection; proof-time dependencies must be modeled explicitly

Proof obligations:

- `RequiredColumnProof`
- `TransientProofDependencySet`

#### D. Transitive inference

Input pattern:

- equality predicates and proven equalities

Actions:

- build equivalence classes
- derive scan-local constants from equalities such as `a=b` and `a=10`
- derive safe mirrored filters across inner-join equalities
- detect contradictions inside an equivalence class or range lattice

Legality:

- outer-join domains restrict where a derived predicate may be installed
- semijoin and antijoin domains must be treated conservatively
- only immutable predicates may participate in proof derivation

Proof obligations:

- `EquivalenceClassProof`
- `JoinDomainProof`
- `PredicateImplicationProof`

#### E. Subquery unnesting and decorrelation

Input pattern:

- trivial pass-through derived tables
- `EXISTS`, `NOT EXISTS`, `IN`, `NOT IN`, and selected `ANY` forms
- correlated subqueries with equality-style correlation

Actions:

- preserve existing trivial flattening
- convert qualifying `EXISTS` to semijoin
- convert qualifying `NOT EXISTS` to antijoin
- convert qualifying `IN` to semijoin when NULL semantics are preserved
- rewrite selected subqueries to joined derived tables when direct semijoin conversion is blocked but decorrelation is still possible
- represent still-correlated forms explicitly rather than silently forcing them into unsafe join shapes

Legality:

- subquery shape must be simple enough for the chosen rewrite
- volatile predicates block conversion
- `NOT IN` requires a strict nullability proof or it must remain in a more conservative form
- correlation predicates must be externalizable without changing duplicates or NULL behavior
- lateral dependencies must be represented explicitly

Proof obligations:

- `CorrelationBindingProof`
- `NullSemanticsProof`
- `DuplicateSemanticsProof`
- `SubqueryShapeProof`

#### F. Semijoin and antijoin conversion

Input pattern:

- normalized subquery predicates and mark-like existential forms

Actions:

- canonicalize to `SEMI` or `ANTI` logical operators when legal
- deduplicate the right side only when needed by later implementation strategy, not as a mandatory semantic step
- keep a conservative representation for NULL-sensitive cases that do not satisfy antijoin legality

Legality:

- follow a candidate checklist similar to MySQL’s gating, including grouping, HAVING, windows, determinism, truth-value mode, and LIMIT/OFFSET stability

Proof obligations:

- `SemiAntiLegalityProof`
- `NullTruthTableProof`

#### G. Join elimination and outer-to-inner demotion

Input pattern:

- joins whose one side is unreferenced above or only needed for filtering that has already been discharged

Actions:

- demote outer joins to inner joins when post-join predicates reject NULL-complemented rows on the nullable side
- eliminate removable joins when uniqueness and non-reference are proven
- start first wave with left-join elimination and selected inner-join elimination cases only

Legality:

- no columns from the eliminated side may be required above
- the eliminated side must be uniqueness-preserving on the join keys under its local filters
- the retained side must not gain or lose duplicates
- join filters with volatile or side-effecting behavior block elimination

Proof obligations:

- `NullRejectingProof`
- `UniquenessProof`
- `NoReferencedOutputProof`
- `DuplicatePreservationProof`

#### H. Aggregation pushdown and DISTINCT simplification

Input pattern:

- aggregate over join
- DISTINCT over input already known unique
- aggregates or windows with redundant `DISTINCT`
- duplicate grouping keys

Actions:

- push grouping below joins only when pushed groups remain semantically equivalent
- remove redundant DISTINCT when uniqueness is already proven
- remove duplicate grouping keys
- remove `DISTINCT` from duplicate-insensitive aggregates only when function semantics permit it

Legality:

- aggregation pushdown is blocked if join fanout can multiply groups unexpectedly
- outer joins need special care and should be out of scope for first-wave aggressive pushdown
- duplicate-insensitive aggregate knowledge must come from a function catalog, not heuristics

Proof obligations:

- `GroupingKeyProof`
- `PreJoinUniquenessProof`
- `FanoutSafetyProof`
- `AggregateDistinctSensitivityProof`

### 5.3 Recommended first-wave ordering

Recommended order inside one query block:

1. normalize and fold
2. derive properties
3. push predicates and projections locally
4. build equivalence classes and generate bounded transitive predicates
5. decorrelate subqueries and canonicalize semijoin or antijoin forms
6. demote outer joins where null rejection proves safety
7. attempt join elimination
8. attempt aggregate and DISTINCT simplification
9. remove dead columns and dead operators
10. normalize again and stop at fixpoint bound

Reasoning:

- Pushdown is more effective after normalization.
- Join elimination and aggregate pushdown require already-derived proofs.
- Cleanup should happen after shape-changing rewrites.

## 6. Formula and Heuristic Packet

Use the following first-pass symbols:

- `Req(n)`: columns required above node `n`
- `Out(n)`: columns output by node `n`
- `Loc(n)`: columns referenced by node-local expressions at `n`
- `Eq(n)`: equivalence classes valid at node `n`
- `JD(n)`: join domain of node `n`
- `U(n, K)`: proof that node `n` is unique on key set `K`
- `NR(p, S)`: proof that predicate `p` is null-rejecting for symbol set `S`
- `Vol(e)`: expression `e` is volatile, side-effecting, or may throw early

### 6.1 Projection requirement propagation

Required-column recurrence:

`Req(child) = (Req(parent) intersect Out(child)) union Loc(parent->child) union ProofDeps(parent->child)`

Where:

- `Loc(parent->child)` are child columns needed to evaluate predicates, join keys, grouping, ordering, distinct keys, window partitions, and runtime filters at the parent
- `ProofDeps(parent->child)` are columns needed only to prove legality such as uniqueness or null rejection

Fallback:

- If proof dependencies are not tracked separately in first wave, keep them in `Req(child)` and accept less aggressive pruning.

### 6.2 Predicate pushdown test

A predicate `p` can be pushed from node `n` to child `c` only if:

- `refs(p) subseteq Out(c)` after symbol remapping
- `Vol(p) = false`
- `JD(c)` covers the domain in which `p` is valid
- duplicate semantics are unchanged by evaluating `p` at `c`

For outer-join demotion, define:

`NR(p, nullable_side) = fold(substitute_nullable_side_with_NULL(p)) in {FALSE, NULL}`

If `NR` holds for a post-join predicate over the nullable side of a left join, then the left join may be demoted to inner join.

### 6.3 Transitive inference bound

For one equivalence class with `r` relation-local representatives, generate at most:

`max_generated_preds = min(2 * r, 16)`

and prefer:

- one constant predicate per scan when a class contains a constant
- one mirrored filter per inner-join edge

Fallback:

- If the bound is hit, keep only scan-local constant predicates and drop join-derived duplicates.

### 6.4 Contradiction detection

Maintain a per-symbol range lattice where possible.

If for any symbol:

`lower_bound > upper_bound`

then replace the affected subtree with an empty-result logical node.

If no range lattice exists for the data type:

- fall back to equality contradiction checks
- fall back to boolean-folding contradictions

### 6.5 Join elimination test

A join `Join(L, R, on)` may eliminate `R` only if all conditions hold:

- `Req_above(join) intersect Out(R) = empty`
- `U(R_filtered, join_keys_R)` is proven
- join type preserves the rows of `L` after elimination
- no remaining predicate above depends on match multiplicity from `R`

For first wave, recommend restricting elimination to:

- removable left joins
- selected inner joins where the right side is key-preserving and contributes no filtering beyond guaranteed existence

### 6.6 Aggregate pushdown test

An aggregate `Agg(G, A, Join(L, R))` may push below a join only if:

- every pushed grouping key is expressible in the target child
- the join does not multiply rows for the grouped side, or that fanout is neutralized by a uniqueness proof
- aggregate functions are distributive or otherwise legally decomposable for the chosen rewrite

Heuristic:

- first-wave implementation should allow only inner joins plus strong uniqueness proof
- outer joins and many-to-many joins should be rejected

### 6.7 DISTINCT simplification test

Remove `DISTINCT K` when `U(input, K)` is proven.

Remove aggregate-local `DISTINCT` only when:

- the aggregate is duplicate-insensitive by catalog rule
- all arguments are the same after normalization

Fallback:

- if duplicate sensitivity is unknown, keep DISTINCT

### 6.8 Fixpoint and termination

Recommended bounds:

- `max_rewrite_passes_per_block = 8`
- `max_new_predicates_per_block = 64`
- `max_shape_changing_rewrites_per_block = 32`

Terminate when:

- structural hash of the logical tree is unchanged for one full phase cycle

Fallback:

- if a bound is exceeded, stop rewriting, preserve current tree, and emit a trace event indicating bounded termination

### 6.9 Benefit heuristics

Use legality-first, benefit-second ordering.

Simple benefit score:

`benefit = access_unlock + partition_prune_unlock + fanout_reduction + column_reduction - risk_penalty`

Where:

- `access_unlock` is high when a pushed predicate can become an index or partition filter
- `fanout_reduction` is high when an early filter reduces join input size
- `column_reduction` is high when many wide columns become dead
- `risk_penalty` is zero for proven-safe rewrites and non-zero only as a scheduling tie-breaker

Inference:

- First-wave ScratchBird should not reject a legal rewrite because its estimated benefit is small if the rewrite unlocks a materially better access path or partition pruning opportunity.

## 7. ScratchBird Contract Draft

### 7.1 Placement

Introduce a new planner stage:

- `ResolvedQuery`
- `LogicalRewriteManager`
- `PhysicalPlanner`

The rewrite layer should consume resolved ScratchBird algebra, not raw parser AST nodes.

### 7.2 Core state

Recommended first-pass structures:

```text
enum class RewritePhase {
  Normalize,
  DeriveProperties,
  LocalPushdown,
  RelationalRewrite,
  Cleanup
};

struct PropertySet {
  ColumnSet output_columns;
  ColumnSet required_columns;
  EquivalenceClasses eq_classes;
  NullabilityMap nullability;
  UniqueKeySet unique_keys;
  FunctionalDependencySet fds;
  JoinDomainId join_domain;
  bool duplicate_preserving;
  bool has_outer_refs;
};

struct ProofArtifact {
  ProofKind kind;
  LogicalNodeId subject;
  LogicalNodeId witness;
  std::string summary;
};

struct RewriteTraceEvent {
  std::string rule_name;
  RewritePhase phase;
  LogicalFingerprint before_fp;
  LogicalFingerprint after_fp;
  std::vector<ProofArtifactId> proofs_used;
  std::string reject_reason;
};
```

### 7.3 Rule API

Recommended rule contract:

```text
class LogicalRewriteRule {
 public:
  virtual std::string name() const = 0;
  virtual RewritePhase phase() const = 0;
  virtual MatchSet match(const LogicalNode &, const RewriteContext &) const = 0;
  virtual LegalityResult check(const Match &, const RewriteContext &) const = 0;
  virtual ApplyResult apply(const Match &, RewriteContext &) const = 0;
};
```

`LegalityResult` should include:

- accepted or rejected
- proof artifacts consumed
- proof artifacts produced
- explicit rejection reason

### 7.4 Rewrite contract rules

- Every rewrite must declare whether it changes duplicates, ordering, null introduction, or lateral binding.
- Every rewrite must operate on canonical logical operators, not SQL syntax forms.
- Volatile, nondeterministic, and side-effecting expressions are hard barriers unless a rule explicitly proves otherwise.
- Outer joins, semijoins, antijoins, and correlated references must carry domain metadata.
- Any rewrite that depends on uniqueness, null rejection, or implication must emit a proof artifact.
- Rewrite traces must be observable in EXPLAIN or planner diagnostics so later lanes can explain why a rewrite fired or was rejected.

### 7.5 Initial logical operators to support

ScratchBird should have explicit logical operator forms for:

- filter
- projection
- aggregate
- distinct
- join with typed join kinds
- semijoin
- antijoin
- derived-table or subquery boundary
- empty result

Inference:

- A dedicated `Apply` or dependent-join operator may become necessary for full correlated-subquery support, but first-wave Lane B can proceed without exposing a full arbitrary-`Apply` framework if it scopes initial decorrelation to EXISTS, IN, and simple correlated forms.

### 7.6 Non-negotiable semantic contract

- MGA-visible row contents must be preserved.
- NULL truth tables must be preserved exactly.
- Duplicate behavior must be preserved exactly unless the rewrite is itself a duplicate-removing operator such as DISTINCT or aggregate and the change is already part of the query semantics.
- Rewrite proofs must not depend on MVCC or WAL-specific donor assumptions.

## 8. Validation and Benchmark Packet

### 8.1 Correctness corpus categories

The validation corpus should include both positive and negative cases for every rule family.

Required categories:

- simple scan predicate pushdown
- predicate duplication across inner-join equalities
- blocked pushdown across outer joins
- projection pruning with join keys, order keys, and hidden proof dependencies
- transitive inference across three or more joined relations
- contradiction detection and empty-result pruning
- trivial derived-table flattening
- correlated EXISTS to semijoin
- correlated NOT EXISTS to antijoin
- IN and NOT IN cases with nullable outer and inner expressions
- outer-to-inner demotion via null-rejecting predicates
- removable left joins
- non-removable joins where uniqueness is absent
- aggregate pushdown across key-preserving joins
- aggregate pushdown rejection across fanout joins
- DISTINCT elimination from already-unique inputs
- duplicate grouping-key removal
- blocked rewrites due to volatile or exception-throwing expressions

### 8.2 Metamorphic and differential testing

Recommended validation modes:

- original SQL versus rewritten logical plan must produce identical results
- randomized data generation must vary NULL density, duplicate density, and key skew
- each rule should have “fire” and “do not fire” pairs
- logical fingerprints before and after rewrite should be stored for regression debugging

Differential comparison ideas:

- run selected SQL corpora against ScratchBird, PostgreSQL, DuckDB, and MySQL
- compare result sets first
- compare logical rewrite opportunities second
- treat donor output as advisory only, not as semantic authority

### 8.3 Observability checks

Each rule should be testable via a rewrite trace:

- rule name
- preconditions checked
- proofs used
- rejection reason if blocked

This is required to make later Lane K observability work practical.

### 8.4 Benchmark shapes

Performance-oriented corpus should include:

- star-schema selective filters
- wide fact tables where projection pruning matters
- correlated subqueries that should become semijoins
- left joins with post-filters that should demote to inner joins
- key-preserving dimension joins that should be removable
- aggregate-over-join cases where early grouping is safe

### 8.5 Acceptance criteria for first wave

- no semantic regressions on a NULL-heavy and duplicate-heavy randomized corpus
- measurable reduction in average logical-tree width from projection pruning
- measurable increase in partition-pruning and index-predicate opportunities after rewrite
- rewrite traces show deterministic pass ordering
- bounded rewrite execution time with no fixpoint loops

## 9. Adopt/Adapt/Reject/Defer Matrix

### Adopt

- PostgreSQL-style equivalence classes and join-domain-aware transitive reasoning
- PostgreSQL-style predicate implication and refutation discipline, especially immutability gating
- DuckDB-style ordered pass manager with explicit rule registration
- DuckDB-style remove-unused-columns and column-lifetime trimming ideas
- MySQL-style semijoin-candidate checklist
- MySQL-style explicit outer-to-inner simplification as a rewrite stage

### Adapt

- DuckDB filter pull-up and pushdown rules, adapted to ScratchBird algebra and MGA semantics
- PostgreSQL join elimination, adapted to ScratchBird proof artifacts and available uniqueness metadata
- DuckDB aggregate and DISTINCT simplifications, adapted to ScratchBird function catalog semantics
- MySQL joined-derived rewrites for blocked semijoin conversions
- MySQL semijoin deduplication logic, adapted as an optional downstream transformation rather than a required first semantic rewrite

### Reject

- parser-AST-specific rewrite logic as the long-term rewrite substrate
- folding or predicate implication over volatile or nondeterministic functions
- pushing predicates across outer joins without explicit null and join-domain proofs
- donor-specific assumptions tied to WAL, MVCC visibility, or engine-specific executor markers

### Defer

- full memo-integrated rewrite saturation
- arbitrary scalar-subquery unnesting beyond a bounded first-wave subset
- aggressive aggregate pushdown through outer joins and many-to-many joins
- generalized FK-driven join elimination for complex multi-join nests
- Top-N and LIMIT-aware rewrites
- runtime adaptive rewrite based on executor feedback

Inference:

- If Lane A later chooses a memo-first architecture, most items in Adopt and Adapt remain valid; only their execution substrate changes.

## 10. Open Questions and Integration Dependencies

Cross-lane dependencies:

- Lane A must decide whether Lane B first targets a standalone pass manager or a memo-connected logical layer. This draft assumes a standalone staged pass manager is acceptable first.
- Lane C and Lane D must define what uniqueness, FD, nullability, and selectivity metadata are trustworthy enough for rewrite proofs.
- Lane K must define how rewrite traces and proof artifacts are exposed through EXPLAIN and planner diagnostics.

Open semantic questions:

- What is the canonical ScratchBird function-volatility and side-effect catalog for built-ins and user-defined routines
- Should first-wave decorrelation include scalar subqueries, or only EXISTS, NOT EXISTS, IN, and simple correlated derived-table forms
- What proof sources are acceptable for join elimination: declared constraints only, inferred uniqueness only, or statistics-backed confidence proofs
- Does ScratchBird want an explicit dependent-join or `Apply` logical node, or should the first wave keep unresolved correlated forms outside the main rewrite set
- How should rule legality interact with current join legality classification in the semantic analyzer

Open implementation questions:

- Whether required columns for proof-only purposes should be carried in a dedicated dependency channel or folded into ordinary required-column sets
- Whether rewrite traces are stored inline on logical nodes or in a sidecar planner journal
- Whether contradiction detection should start with integer or scalar ranges only, then grow by type family

Unresolved evidence gaps:

- Full literature extraction for aggregate pushdown and advanced decorrelation was not completed in this first pass.
- The exact current integration status of `cse.cpp` in the main select-planning path remains unconfirmed.

Inference:

- None of the unresolved gaps above block a first implementation wave focused on safe canonical rewrites, semijoin or antijoin conversion, and proof-aware pushdown.

## 11. Recommended Next-Step Specification Tasks

Recommended follow-on specs, in priority order:

1. Draft `logical_rewrite_architecture` with pass phases, rule API, proof artifacts, and trace schema.
2. Draft `equivalence_classes_and_predicate_implication` with join-domain handling and contradiction detection.
3. Draft `subquery_decorrelation_and_semi_anti_rewrites` with explicit legality tables for EXISTS, NOT EXISTS, IN, and NOT IN.
4. Draft `outer_join_demotion_and_join_elimination` with uniqueness, null-rejection, and duplicate-preservation proofs.
5. Draft `projection_pruning_and_column_liveness` with required-column propagation formulas.
6. Draft `aggregate_pushdown_and_distinct_simplification` with first-wave safe subsets only.
7. Draft `rewrite_validation_corpus` with semantic regression suites, metamorphic tests, and donor-comparison harnesses.
8. Draft `rewrite_observability_contract` jointly with Lane K so every rule has explainable proof and rejection output.

Implementation sequencing recommendation:

- Ship normalization, property derivation, predicate pushdown, projection pruning, equivalence classes, and semijoin or antijoin conversion first.
- Ship outer-join demotion and conservative join elimination second.
- Ship aggregate pushdown and broader DISTINCT simplification only after uniqueness and fanout proofs are stable.
