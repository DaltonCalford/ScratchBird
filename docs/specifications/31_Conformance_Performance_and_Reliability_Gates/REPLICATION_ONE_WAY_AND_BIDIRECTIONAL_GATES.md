# Replication One-Way and Bi-Directional Gates (Alpha)

## Purpose
Define conformance/reliability gates for replication specifications in sections `24`, `28`, `29`, and `30`.

## Gate Suite `G11_REPLICATION_CONFORMANCE`

### `G11-A` Channel State Machine Conformance
- one-way state transitions follow canonical graph and reject forbidden transitions.
- bi-directional transitions enforce `FENCED` recovery workflow.
- optimistic version guard (`EXPECT VERSION`) enforced for all mutations.

Pass criteria:
- 100% deterministic transition outcomes and error codes across replayed runs.

### `G11-B` Cursor Protocol Integrity
- cursor frame ordering (`HELLO/OFFER/BATCH_BEGIN/CHANGE/BATCH_END/ACK|NACK`) validation.
- malformed frame rejection determinism.
- monotonic sequence enforcement per stream.

Pass criteria:
- zero accepted malformed streams and zero sequence regressions.

### `G11-C` Apply Ordering and Retry Determinism
- ascending `source_commit_seq` apply order.
- transaction-atomic apply per batch.
- retry backoff and dead-letter transitions match policy.

Pass criteria:
- no out-of-order apply and no nondeterministic retry schedule.

### `G11-D` Conflict and Loop-Prevention Behavior
- bi-directional loop-prevention using origin progress vectors.
- conflict classification and resolution policy determinism.
- conflict persistence before terminal apply status.

Pass criteria:
- no replication loops and no unresolved conflict status mismatches.

### `G11-E` Split-Brain Fence and Recovery
- split-brain detection fences channel and blocks unsafe controls.
- fence clear requires approval token and convergence validation.
- fenced channel cannot transition directly to `STREAMING`.

Pass criteria:
- zero fence bypasses and deterministic recovery transitions.

### `G11-F` SQL and Tooling Surface Conformance
- native SQL replication control commands parse and map to canonical control ops.
- fixed result schemas for `SHOW REPLICATION ...` commands.
- CLI mappings for `sb_admin replication ...` and `sb_replctl ...` are 1:1.

Pass criteria:
- zero command-surface drift and stable result schemas.

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
