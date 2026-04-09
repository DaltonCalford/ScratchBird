# P1/P2 Optimization Gate Profile

## Purpose
Define explicit release-gate profile for P1 and P2 optimization capabilities so they can be validated as a parallel track without weakening Alpha correctness gates.

## Scope
- P1 distributed-read, cache layering, and telemetry contracts.
- P2 cost-aware scheduling and deterministic tie-break contracts.

## Hard Invariants
1. P1 and P2 gates are additive; they do not replace Alpha correctness gates.
2. Determinism gates are mandatory for any optimization-track promotion.
3. Failing optimization gates cannot be waived by performance-only gains.

## Gate Families

### GP1-A Distributed Read Correctness
- Validate consistency policy transport and enforcement.
- Validate speculative retry bounds and reconciliation behavior.
- Validate repair mode execution and persistence evidence.

### GP1-B Two-Layer Cache Correctness
- Validate L1/L2 admission rules.
- Validate invalidation triggers and stale-read prevention.
- Validate cache-key determinism and cross-policy isolation.

### GP1-C Telemetry Integrity and Security
- Validate complete telemetry field capture.
- Validate correlation id continuity across parser/engine/wire.
- Validate redaction and policy-governed exposure controls.

### GP2-A Deterministic Plan Selection
- Validate equal-cost tie-break determinism.
- Validate reproducibility of selection from persisted scoring inputs.
- Validate tie-break metadata visibility in diagnostics.

### GP2-B Cost-Aware Placement and Scheduler Safety
- Validate deterministic node scoring and placement.
- Validate queue fairness and starvation bounds.
- Validate rebalance safety checks and rollback behavior.

### GP2-C Stability Under Change
- Validate behavior under autoscaling events.
- Validate behavior under partial node failures.
- Validate policy version roll-forward and rollback paths.

## Evidence Requirements
- Required artifacts per run:
  - request/response transcript
  - scheduler decision trace
  - cache admission/invalidation trace
  - telemetry capture bundle
  - deterministic replay transcript.
- Evidence must include:
  - policy ids and versions
  - correlation ids
  - snapshot ids
  - pass/fail verdict with reason code.

## Acceptance Thresholds
- Determinism:
  - identical test seeds and inputs produce identical verdicts and comparable traces.
- Reliability:
  - no incorrect results under injected retries, repair, or rebalance events.
- Performance drift:
  - no regression beyond configured threshold for protected workloads.
- Security:
  - zero redaction policy violations.

## Failure Classification
- `BLOCKER`: correctness, security, or determinism failure.
- `MAJOR`: reliability degradation beyond threshold.
- `MINOR`: non-blocking metric drift without correctness impact.

## Advancement Rules
1. All `BLOCKER` failures must be resolved.
2. `MAJOR` failures require explicit risk acceptance entry.
3. `MINOR` failures require tracked remediation plan.
4. P2 promotion requires P1 gate family full pass first.

## Cross-Section Links
- `23_SBLR_VM_Compiler_and_Executor/NORMATIVE_P1_DISTRIBUTED_READ_CACHE_AND_TELEMETRY_CHECKLIST.md`
- `23_SBLR_VM_Compiler_and_Executor/NORMATIVE_P2_COST_AWARE_SCHEDULER_AND_TIEBREAK_CHECKLIST.md`
- `25_Runtime_Modes/NORMATIVE_P1_CLUSTER_READ_CONSISTENCY_AND_REPAIR_CHECKLIST.md`
- `25_Runtime_Modes/NORMATIVE_P2_CLUSTER_COST_AWARE_PLACEMENT_AND_SCHEDULING_CHECKLIST.md`
- `26_Native_Wire_Protocol/NORMATIVE_P1_WIRE_DISTRIBUTED_READ_AND_TELEMETRY_CHECKLIST.md`
- `28_Parser_Implementations/NORMATIVE_P1_PARSER_DISTRIBUTED_POLICY_AND_TELEMETRY_CHECKLIST.md`
- `28_Parser_Implementations/NORMATIVE_P2_PARSER_PLAN_STABILITY_AND_HINTS_CHECKLIST.md`

## 2026-03-28 Audit Normalization Update

- Section `31` is normalized to the code-backed `partial` standard.
- Current gate authority is bounded to the shipped engine and driver gate entry points, especially `ScratchBird/docs/TEST.md`, `tests/conformance/public_beta/run_required_public_beta_gate.sh`, `tests/compatibility/*`, engine unit/integration/benchmark/stress suites, and driver build or implementation-gate reports under `ScratchBird-driver/docs/`.
- The required public-beta gate is the strongest current section-local release-gate authority, but it is still a bounded gate script and category set rather than proof of a fully unified enterprise certification framework.
- Compatibility manifests, benchmark suites, driver build matrices, and implementation gate reports are current evidence surfaces; they are not universal proof that every numbered section `31` gate is live, mandatory, and fully replayable.
- Performance, optimization, and scorecard language is bounded to the current benchmark or readiness evidence, not a completed cross-platform SLO certification program.
- Cluster gameday, operator runbook, replication, upgrade or rollback orchestration, full forensic shadow gating, and broad platform certification language remain bounded, checklist-oriented, or `target_state_only` unless direct gate scripts and replayable evidence bundles exist.
- Evidence artifact matrices and phase-dependency matrices are treated as planning or inventory surfaces unless matched by executed gate runners and preserved result artifacts.
- MGA recovery remains state-based and not WAL/redo replay; replay language in this section must stay compatible with current recovery audits.

## 2026-03-28 Hardening Promotion Update

- Section `31` now carries explicit bounded authority for the required public-beta gate, compatibility manifests, current engine benchmark and stress evidence, and bounded driver build or gate evidence.
- Required gate and stage-policy language is now tied to the current engine gate entry points rather than a fully unified enterprise certification framework.
- Evidence and replay language is now bounded to current manifest, marker, and preserved result surfaces.
- Reliability and recovery language is now anchored to MGA/state-based recovery audits and current executed gate steps rather than a universal replay-certification regime.
- Protocol, handshake, client-tooling, driver, and platform gate language is now explicitly shared or bounded where current proof lives in neighboring sections or driver repo artifacts.
- Cluster gameday, replication, upgrade, rollback, operator-runbook, forensic shadow, and canonical diff-classification narratives remain bounded or `target_state_only` unless dedicated executed gate bundles and preserved artifacts exist.
