# Phase-Gate Dependency Matrix

## Purpose
Provide a deterministic execution-order matrix for gate dependencies so implementation and validation can be run without sequencing ambiguity.

## Scope
- Alpha baseline gates.
- P1 optimization-track gates.
- P2 optimization-track gates.

## Rules
1. A downstream gate cannot start until all required upstream gates pass.
2. If any upstream gate regresses, all dependent downstream gates return to `blocked`.
3. P2 is never eligible until all P1 gate families pass.
4. Determinism gates are blocking gates, not advisory checks.

## Phase Matrix

| Phase Id | Phase Name | Required Gate Sets | Depends On | Blocks If Failing | Exit Criteria |
| --- | --- | --- | --- | --- | --- |
| `PH0` | Baseline Parser/Engine/Wire Readiness | `T23-A..T23-J`, `T26-A..T26-G`, `T28-A..T28-L`, `T25 required tests`, `T31-G1..T31-G9` | none | all later phases | all baseline suites pass with required evidence |
| `PH1` | P1 Distributed Read and Telemetry + Operational Readiness | `T23-K`, `T25-P1-*`, `T25-CLOCK-*`, `T25-SLO-*`, `T26-H`, `T28-M`, `T31-G10-01..03`, `T31-G12-01..07` | `PH0` | `PH2` | all P1 suites and game-day runbooks pass with replayable evidence |
| `PH2` | P2 Cost-Aware Scheduling and Stability | `T23-L`, `T25-P2-*`, `T28-N`, `T31-G10-04..06` | `PH1` | release approval for optimization track | all P2 suites pass with replayable evidence |

## Gate Family Dependency Map

| Dependency Id | Upstream Gate | Downstream Gate | Dependency Type | Blocking Rule | Evidence Requirement |
| --- | --- | --- | --- | --- | --- |
| `D-001` | `T28-L` | `T23-A..T23-J` | parser->engine normalization | fail in parser normalization blocks engine plan/cache validation | parser SBLR artifacts with normalization hash evidence |
| `D-002` | `T23-A..T23-J` | `T26-A..T26-G` | engine->wire result/error conformance | fail in engine result semantics blocks wire conformance signoff | engine result-shape and error mapping evidence |
| `D-003` | `T26-A..T26-G` | `T31-G3` | protocol integration | wire conformance failure blocks orchestration gates | frame/message transcript bundle |
| `D-004` | `T23-K` | `T31-G10-01` | P1 distributed-read correctness | any `T23-K` fail blocks GP1-A pass | distributed-read execution traces |
| `D-005` | `T25-P1-*` | `T31-G10-01` | runtime consistency/repair | any runtime P1 fail blocks GP1-A pass | cluster routing and repair evidence |
| `D-006` | `T26-H` | `T31-G10-02`, `T31-G10-03` | wire telemetry and event transport | any wire P1 fail blocks GP1-B/C | telemetry/event frame replay logs |
| `D-007` | `T28-M` | `T31-G10-03` | parser telemetry security | parser P1 telemetry/redaction fail blocks GP1-C | parser redaction trace and role-visibility evidence |
| `D-008` | `T31-G10-01..03` | `PH2` start | phase barrier | any GP1 failure blocks all GP2 execution | GP1 gate summary bundle |
| `D-009` | `T23-L` | `T31-G10-04` | deterministic plan choice | any tie-break determinism failure blocks GP2-A | repeated-run deterministic selection evidence |
| `D-010` | `T25-P2-*` | `T31-G10-05`, `T31-G10-06` | runtime scheduler safety | runtime P2 safety fail blocks GP2-B/C | placement/rebalance/rollback evidence |
| `D-011` | `T28-N` | `T31-G10-04` | parser hint stability | parser P2 hint instability blocks GP2-A | canonical hint translation and shape-key evidence |
| `D-012` | `T19-PKI-*` | `T31-G12-01` | PKI lifecycle correctness | any PKI lifecycle failure blocks PKI game-day pass | cert rotation/revocation/anchor rollover evidence |
| `D-013` | `T25-CLOCK-*` | `T31-G12-02` | clock-skew policy correctness | any clock policy failure blocks clock game-day pass | clock state transition and fence evidence |
| `D-014` | `T25-SLO-*` | `T31-G12-05` | SLO and autoscale policy correctness | any SLO policy failure blocks overload game-day pass | burn-rate, tuning, and autoscale action evidence |

## Re-Run Triggers

| Trigger Id | Trigger Condition | Required Re-Run Scope |
| --- | --- | --- |
| `R-001` | change in capability profile version | `T28-A..N`, then dependent `T23-*`, then `T31-G10` |
| `R-002` | change in formula/scheduler policy version | `T23-J..L`, `T25-P2-*`, `T31-G10-04..06` |
| `R-003` | change in distributed read/repair policy version | `T23-K`, `T25-P1-*`, `T26-H`, `T28-M`, `T31-G10-01..03` |
| `R-004` | wire message schema/version change | `T26-A..H`, `T31-G3`, `T31-G10` |
| `R-005` | telemetry/redaction policy change | `T26-H`, `T28-M`, `T31-G10-03` |
| `R-006` | PKI policy/certificate/trust-anchor change | `T19-PKI-*`, `T31-G12-01` |
| `R-007` | clock policy or source topology change | `T25-CLOCK-*`, `T31-G12-02` |
| `R-008` | SLO/autoscale/admission tuning policy change | `T25-SLO-*`, `T31-G12-05` |

## Execution Procedure (Low-Capability Agent Safe)
1. Execute all `PH0` suites.
2. If `PH0` pass: execute `PH1` suites.
3. If `PH1` pass: execute `PH2` suites.
4. If any suite fails: mark dependent phases `blocked`.
5. Apply required re-run scope from trigger table after any policy/profile/schema change.

## Cross-Section Links
- `23_SBLR_VM_Compiler_and_Executor/TEST_CONTRACT.md`
- `25_Runtime_Modes/TEST_CONTRACT.md`
- `26_Native_Wire_Protocol/TEST_CONTRACT.md`
- `28_Parser_Implementations/TEST_CONTRACT.md`
- `31_Conformance_Performance_and_Reliability_Gates/P1_P2_OPTIMIZATION_GATE_PROFILE.md`
- `31_Conformance_Performance_and_Reliability_Gates/GATE_EVIDENCE_ARTIFACT_MATRIX.md`

## 2026-03-28 Audit Normalization Update

- Section `31` is normalized to the code-backed `partial` standard.
- Current gate authority is bounded to the shipped engine and driver gate entry points, especially `ScratchBird/docs/TEST.md`, `tests/conformance/public_beta/run_required_public_beta_gate.sh`, `tests/compatibility/*`, engine unit/integration/benchmark/stress suites, and driver build or implementation-gate reports under `ScratchBird-driver/docs/`.
- The required public-beta gate is the strongest current section-local release-gate authority, but it is still a bounded gate script and category set rather than proof of a fully unified enterprise certification framework.
- Compatibility manifests, benchmark suites, driver build matrices, and implementation gate reports are current evidence surfaces; they are not universal proof that every numbered section `31` gate is live, mandatory, and fully replayable.
- Performance, optimization, and scorecard language is bounded to the current benchmark or readiness evidence, not a completed cross-platform SLO certification program.
- Cluster gameday, operator runbook, replication, upgrade or rollback orchestration, full forensic shadow gating, and broad platform certification language remain bounded, checklist-oriented, or `target_state_only` unless direct gate scripts and replayable evidence bundles exist.
- Evidence artifact matrices and phase-dependency matrices are treated as planning or inventory surfaces unless matched by executed gate runners and preserved result artifacts.
- MGA recovery remains state-based and not WAL/redo replay; replay language in this section must stay compatible with current recovery audits.
