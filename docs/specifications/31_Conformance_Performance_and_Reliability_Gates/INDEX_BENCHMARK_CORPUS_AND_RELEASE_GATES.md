# Index Benchmark Corpus and Release Gates

## Purpose
Define the canonical benchmark corpus, evidence outputs, and phase gates that
control ScratchBird index-family promotion.

## Scope
- benchmark corpus layers
- required outputs
- index-specific phase gates
- specialized hardening gate policy

## Hard Invariants
1. Passing gates requires deterministic evidence bundles, not narrative claims.
2. Every planner-visible family must have both correctness and performance
   evidence.
3. Visibility regressions are correctness failures even when performance or
   recall looks good.
4. Families with higher-risk semantics require specialized hardening gates in
   addition to the shared gate stack.
5. Every shipped family must be benchmarked and gated as a primary optimizer
   class, not as a secondary or manually-forced fallback.

## Benchmark Corpus Layers

### 1. Correctness corpus
- snapshot visibility equivalence to sequential scan
- insert, update, delete, retire, and reclaim under old and new snapshots
- build, publish, retire, and reclaim correctness
- exactness and required-recheck truth tables

### 2. Family microbenchmark corpus
- ordered exact families: equality, selective range, long range, `ORDER BY ...
  LIMIT`, overflow debt, compaction debt
- summary and columnar families: clustered and unclustered range pruning, exact
  and lossy bitmap execution, wide and narrow projection, late materialization
- generalized and spatial families: overlap, containment, nearest search,
  partition skew, publication amplification
- text families: boolean term search, wide `OR`, phrase, proximity, ranked
  `TOP K`, write-heavy merge debt
- vector families: exact truth top-`K`, ANN recall, filtered ANN, delete debt,
  stale-training scenarios

### 3. Planner crossover corpus
- ordered scan versus sort
- `BRIN` versus sequential scan on clustered and stale layouts
- stored bitmap versus synthetic bitmap-combine versus sequential scan
- columnstore projection versus row-store reconstruction
- text score-first versus filter-plus-sort
- spatial lower-bound path versus scan fallback
- ANN approximate path versus exact fallback
- every shipped family versus the historically dominant ordered-family baseline
  on the query shapes it semantically supports

### 4. Workload and donor-reference corpus
- `TPC-H`
- `TPC-DS` where feasible
- `Star Schema Benchmark`
- `Join Order Benchmark` slices where index choice matters
- correlated-predicate synthetic workloads
- skew and heavy-hitter workloads
- plan-stability regression corpus

### 5. Release regression corpus
- deterministic reruns of all acceptance and rejection matrices
- migration and backward-compatibility checks
- full conformance sweep for planner-visible families

## Required Benchmark Outputs
Every benchmark suite must emit:

- seed and configuration hash
- formula-profile and calibration-profile ids
- p50, p95, and p99 latency
- candidate count
- recheck count
- heap fetch count
- bytes or pages read
- family-specific debt counters
- false-positive and false-negative counts
- chosen path family and cost breakdown
- admissible-but-not-chosen family list
- rejected family list with rejection reason class
- limited or degraded family list with conservative-cost reason class

## Index Gate Stack

### `IX-GATE-00` Taxonomy and alias freeze
Evidence:

- family or runtime matrix
- rejection matrix
- initial contradiction log

### `IX-GATE-01` DDL, parser, and binder contract
Evidence:

- grammar matrix
- action matrix
- alias-lowering audit

### `IX-GATE-02` Planner, emitter, and executor payload contract
Evidence:

- path taxonomy audit
- exactness and recheck field audit
- payload schema audit

### `IX-GATE-03` Family correctness matrix
Evidence:

- family implementation matrix
- validator outputs
- family correctness suites

### `IX-GATE-04` Online publish, maintenance, and reclaim
Evidence:

- lifecycle-state audit
- shadow-build or generation-swap audit
- reclaim-horizon correctness proof

### `IX-GATE-05` Metrics, health, and corruption reporting
Evidence:

- metrics packet audit
- confidence-class audit
- health-scan results
- scorecard snapshot
- primary-class parity audit

### `IX-GATE-06` Reporting and explainability
Evidence:

- `SHOW` or `EXPLAIN` contract audit
- runtime trace audit

### `IX-GATE-07` MGA and security enforcement
Evidence:

- visibility matrix
- recheck truth matrix
- policy audit

### `IX-GATE-08` Performance, migration, and full conformance
Evidence:

- performance regression report
- migration audit
- full conformance report
- cross-family no-secondary-index proof

## No-secondary-index release rule

The release program must prove that no shipped family is left in a
planner-secondary state.

That proof must include:

1. a per-family benchmark mapping
2. a per-family metrics-packet audit
3. planner candidate evidence showing the family is either:
   - admitted and ranked
   - admitted conservatively with explicit reason
   - rejected only for semantic or fail-closed reasons
4. no evidence of silent omission because another family is historically favored

If a shipped family can only be used through manual forcing, undocumented hints,
or direct executor routing, the family fails release parity.

## Specialized Hardening Gates
- `G15_BTREE_HARDENING` remains mandatory for `BTREE`
- a comparable specialized hardening gate is required before production claims
  for:
  - generalized-search families
  - ranked text families
  - ANN families

## Gate Rules
1. All acceptance and rejection matrices rerun at least three times with
   identical outputs.
2. Every gate emits manifest, summary, checksums, and evidence indexes.
3. A family may not bypass a specialized hardening gate when its semantics make
   one necessary.
4. Any `Red` scorecard state blocks release promotion.
5. Any shipped family lacking a no-secondary-index proof blocks full optimizer
   support claims.

## Cross-Section References
- `INDEX_GOVERNANCE_AND_SCORECARD_CONTRACT.md`
- `BTREE_HARDENING_AND_CRASH_SAFE_INDEX_GATES.md`
- `PERFORMANCE_SLO_AND_BENCHMARK_METHOD.md`
- `EVIDENCE_ARTIFACTS_AND_REPLAY_REQUIREMENTS.md`
- `../18_Index_Framework/TEST_CONTRACT.md`

## 2026-03-28 Audit Normalization Update

- Section `31` is normalized to the code-backed `partial` standard.
- Current gate authority is bounded to the shipped engine and driver gate entry points, especially `ScratchBird/docs/TEST.md`, `tests/conformance/public_beta/run_required_public_beta_gate.sh`, `tests/compatibility/*`, engine unit/integration/benchmark/stress suites, and driver build or implementation-gate reports under `ScratchBird-driver/docs/`.
- The required public-beta gate is the strongest current section-local release-gate authority, but it is still a bounded gate script and category set rather than proof of a fully unified enterprise certification framework.
- Compatibility manifests, benchmark suites, driver build matrices, and implementation gate reports are current evidence surfaces; they are not universal proof that every numbered section `31` gate is live, mandatory, and fully replayable.
- Performance, optimization, and scorecard language is bounded to the current benchmark or readiness evidence, not a completed cross-platform SLO certification program.
- Cluster gameday, operator runbook, replication, upgrade or rollback orchestration, full forensic shadow gating, and broad platform certification language remain bounded, checklist-oriented, or `target_state_only` unless direct gate scripts and replayable evidence bundles exist.
- Evidence artifact matrices and phase-dependency matrices are treated as planning or inventory surfaces unless matched by executed gate runners and preserved result artifacts.
- MGA recovery remains state-based and not WAL/redo replay; replay language in this section must stay compatible with current recovery audits.
