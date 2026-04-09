# SBLR VM, Compiler, and Executor Gates (Alpha)

## Purpose
Define conformance gates for sections `22` and `23`.

## Gate Suite `G4_VM_COMPILER_EXECUTOR_CONFORMANCE`

## Required Gate Groups

### `G4-A` Container and Verifier
- section-22 container decode/encode round-trip.
- verifier stable code behavior (`SBLR-E-*`).
- feature-to-opcode mapping completeness checks.

Pass criteria:
- 100% pass for all verifier-negative tests.

### `G4-B` Bind and Plan Build
- UUID binding correctness.
- permission enforcement at bind phase.
- stale epoch invalidation behavior.

Pass criteria:
- zero false-accept on unauthorized bind attempts.

### `G4-C` VM Execution Semantics
- register typing and null semantics.
- deterministic output with fixed snapshots.
- transaction visibility correctness under MGA.

Pass criteria:
- deterministic output hash identical across repeated runs.

### `G4-D` Optimizer
- pass ordering fixed.
- semantic equivalence pre/post optimization.
- deterministic join tie-break behavior.

Pass criteria:
- no semantic drift cases.

### `G4-E` Native Compilation
- hotness promotion behavior.
- artifact build/load on all Alpha target platforms.
- fallback to VM on artifact failure.

Pass criteria:
- all platform targets produce loadable artifacts or deterministic fallback.

### `G4-F` Cache and Invalidation
- cache key correctness.
- dependency invalidation correctness.
- corruption eviction/rebuild path.

Pass criteria:
- zero stale-plan execution after invalidation trigger.

## Required Evidence
For each gate group produce:
- `gate_id`
- commit hash
- test manifest
- pass/fail counts
- failure details
- deterministic hash of result bundle


## 2026-03-28 Audit Normalization Update

- Section `31` is normalized to the code-backed `partial` standard.
- Current gate authority is bounded to the shipped engine and driver gate entry points, especially `ScratchBird/docs/TEST.md`, `tests/conformance/public_beta/run_required_public_beta_gate.sh`, `tests/compatibility/*`, engine unit/integration/benchmark/stress suites, and driver build or implementation-gate reports under `ScratchBird-driver/docs/`.
- The required public-beta gate is the strongest current section-local release-gate authority, but it is still a bounded gate script and category set rather than proof of a fully unified enterprise certification framework.
- Compatibility manifests, benchmark suites, driver build matrices, and implementation gate reports are current evidence surfaces; they are not universal proof that every numbered section `31` gate is live, mandatory, and fully replayable.
- Performance, optimization, and scorecard language is bounded to the current benchmark or readiness evidence, not a completed cross-platform SLO certification program.
- Cluster gameday, operator runbook, replication, upgrade or rollback orchestration, full forensic shadow gating, and broad platform certification language remain bounded, checklist-oriented, or `target_state_only` unless direct gate scripts and replayable evidence bundles exist.
- Evidence artifact matrices and phase-dependency matrices are treated as planning or inventory surfaces unless matched by executed gate runners and preserved result artifacts.
- MGA recovery remains state-based and not WAL/redo replay; replay language in this section must stay compatible with current recovery audits.
