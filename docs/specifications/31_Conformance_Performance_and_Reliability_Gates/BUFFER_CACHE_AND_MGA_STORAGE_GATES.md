# Buffer Cache and MGA Storage Gates

## Purpose
Define mandatory gate suites for segmented buffer policy, dirty-page handling,
checkpoint interaction, writeback, MGA storage behavior, and local concurrency.

## Gate Suites
### G6-BC-01 Scan resistance
- mixed OLTP and scan workloads
- acceptance: working set survives scan pressure within stated degradation bound

### G6-BC-02 Domain isolation
- simultaneous pressure from metadata, OLTP, scan, version-chain, and temp
  workloads
- acceptance: `critical_system` and `version_undo` reservations survive without
  emergency breach

### G6-BC-03 Admission and ghost history
- one-touch scan pages mixed with repeated point-lookups
- acceptance: scan pages remain ring-only or probationary, hot pages produce
  observable ghost hits when undersized and recover after policy adjustment

### G6-BC-04 Prefetch fairness
- speculative prefetch mixed with demand reads and temp spill
- acceptance: prefetch debt remains bounded, useless prefetch is observable, and
  durable protected residency is not monopolized

### G7-BC-01 Dirty-generation correctness
- inject concurrent dirties during checkpoint capture
- acceptance: checkpoint completes only for the captured generation set

### G7-BC-02 Writeback failure behavior
- inject flush and fsync failures on required pages
- acceptance: no false-clean frame publication and no illegal commit ack

### G7-BC-03 Disk-full reserve behavior
- exhaust ordinary free space and then reserve space
- acceptance: reserve pages used only for critical metadata and incident paths

### G7-BC-04 Restart ordering
- crash during grouped writeback, commit fence, and checkpoint publication
- acceptance: transaction truth and checkpoint markers remain legal and restart
  classifies correctly

### G7-BC-05 Local concurrency and ownership
- parallel unrelated misses, evictions, flushes, and scan traffic
- acceptance: unrelated misses do not serialize behind one global victim path
  and frame ownership transitions remain legal

## Required Evidence
- dirty-page generation snapshots
- checkpoint status and history rows
- writeback-incident rows
- frame-state transition traces
- domain occupancy and ghost-hit snapshots
- prefetch debt and usefulness snapshots
- contention and ownership handoff traces
- restart and integrity report

## 2026-03-28 Audit Normalization Update

- Section `31` is normalized to the code-backed `partial` standard.
- Current gate authority is bounded to the shipped engine and driver gate entry points, especially `ScratchBird/docs/TEST.md`, `tests/conformance/public_beta/run_required_public_beta_gate.sh`, `tests/compatibility/*`, engine unit/integration/benchmark/stress suites, and driver build or implementation-gate reports under `ScratchBird-driver/docs/`.
- The required public-beta gate is the strongest current section-local release-gate authority, but it is still a bounded gate script and category set rather than proof of a fully unified enterprise certification framework.
- Compatibility manifests, benchmark suites, driver build matrices, and implementation gate reports are current evidence surfaces; they are not universal proof that every numbered section `31` gate is live, mandatory, and fully replayable.
- Performance, optimization, and scorecard language is bounded to the current benchmark or readiness evidence, not a completed cross-platform SLO certification program.
- Cluster gameday, operator runbook, replication, upgrade or rollback orchestration, full forensic shadow gating, and broad platform certification language remain bounded, checklist-oriented, or `target_state_only` unless direct gate scripts and replayable evidence bundles exist.
- Evidence artifact matrices and phase-dependency matrices are treated as planning or inventory surfaces unless matched by executed gate runners and preserved result artifacts.
- MGA recovery remains state-based and not WAL/redo replay; replay language in this section must stay compatible with current recovery audits.
