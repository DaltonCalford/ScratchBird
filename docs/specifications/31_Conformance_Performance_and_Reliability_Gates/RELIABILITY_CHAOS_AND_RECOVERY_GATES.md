# Reliability, Chaos, and Recovery Gates

## Purpose
Define the failure-injection and recovery gate suites that certify Alpha
correctness under crash, writeback failure, disk-full pressure, checkpoint
interruption, queue rebuild, restart normalization, and repair backlog.

## Gate Suite G7
### G7-A Crash and restart matrix
- crash before data-page flush
- crash after data-page flush and before terminal inventory state
- crash after terminal inventory state and before acknowledgement
- crash during rollback terminal publication
- crash during savepoint rollback backout
- crash during checkpoint drain
- crash after checkpoint marker and before clean-shutdown publication
- acceptance: every crash window converges to one legal post-restart state

### G7-B Writeback and disk-full
- inject flush failure, fsync failure, `ENOSPC`, and reserve exhaustion
- acceptance: no illegal commit acknowledgement, correct fenced or degraded
  mode, deterministic incident publication

### G7-C Recovery and repair
- inject repairable heap and index page damage
- inject fatal control or inventory corruption
- inject stale prepared evidence and prepared-without-durable-backing contradiction
- inject torn-write or partial-write page classification paths
- acceptance: repairable classes quarantine correctly, fatal classes refuse open

### G7-D Sweep resume and rewind
- interrupt sweep before and after cursor persistence
- acceptance: restart resumes or rewinds deterministically with no illegal
  reclaim

### G7-E Buffer ownership and queue rebuild
- inject failpoints during page load ownership transfer, flush completion,
  eviction handoff, and restart queue rebuild
- acceptance: page-table and frame identity remain legal, no false-clean
  publication occurs, and derived queues rebuild deterministically

## Required Evidence
Each gate run must emit:
- failpoint seed and trigger map
- checkpoint and recovery history bundle
- writeback-incident snapshot
- sweep resume or rewind report
- ownership and queue-rebuild trace
- final integrity and service-state classification
- explicit anti-WAL proof note showing no gate verdict depends on redo replay,
  WAL distance, or LSN authority

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
