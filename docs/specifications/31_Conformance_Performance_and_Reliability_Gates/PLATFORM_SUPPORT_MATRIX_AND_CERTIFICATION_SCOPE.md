# Platform Support Matrix and Certification Scope

## Purpose
Define the authoritative platform-support matrix for ScratchBird and the exact
runtime environments that may claim certification under section 31.

## Scope
- operating systems
- CPU architectures
- filesystem classes
- certification versus development-only profiles

## Hard Invariants
1. A platform may be useful for development without being certified for release.
2. Certification claims must be explicit by OS, architecture, and storage
   substrate.
3. Unsupported platform combinations must fail the release gate even if they
   compile.

## Platform Matrix
| Platform class | Status | Notes |
| --- | --- | --- |
| Linux x86_64 with local journaled filesystem | certified target | default Alpha certification lane |
| Linux arm64 with local filesystem | preview | requires explicit gate evidence |
| macOS development hosts | development support | no production certification claim by default |
| Windows development hosts | development support | no production certification claim by default |
| network filesystem for primary database files | unsupported for certification | explicit exception requires human approval |

## Certification Requirements
Certified platform claims require:
- clean build and test on the target platform
- durability and recovery gates on the target storage substrate
- timing and worker-model validation on the target scheduler model
- documented unsupported filesystem or kernel features

## Cross-Section References
- `TEST_OWNERSHIP_EXCLUSION_AND_FLAKE_POLICY.md`
- `FEATURE_LIFECYCLE_AND_CROSS_VERSION_COMPATIBILITY_MATRIX.md`
- `../25_Runtime_Modes/ENGINE_THREAD_WORKER_AND_TASK_MODEL.md`
- `../25_Runtime_Modes/ENGINE_TIME_SOURCE_AND_ORDERING_DISCIPLINE.md`

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
