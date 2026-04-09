# Performance SLO and Benchmark Method (Alpha)

## Purpose
Define benchmark methodology and SLO gate thresholds.

## Baseline Environment
- 8 vCPU
- 32 GB RAM
- NVMe SSD
- Linux x86_64

## Required SLOs
- p95 point-select latency <= 20 ms
- p95 single-row update latency <= 30 ms
- p95 commit latency <= 25 ms
- zero data loss and zero catalog corruption across crash/restart suites

## Benchmark Workloads
1. OLTP point select/update mix
2. write-heavy mutation mix
3. concurrent transaction commit stress
4. index-heavy query mix
5. service channel mixed load
6. OLTP plus analytical scan-resistance mix
7. deep version-chain and TOAST prefetch mix
8. append-hotspot and grouped-writeback mix
9. B-tree duplicate-heavy and MGA-heavy lookup/update mix
10. B-tree split/separator/compressed-search regression mix

## Method Rules
1. fixed seed data generation.
2. warmup period before sample collection.
3. measure p50/p95/p99 and throughput.
4. run each benchmark 10 times.

## Acceptance Criteria
- all required SLOs met across 10-run aggregate.
- no run may exceed 1% failed operations.

## Reporting Format
Required output fields:
- benchmark_id
- config hash
- workload seed
- sample count
- latency stats
- throughput stats
- error counts

## Artifact Path
Store benchmark bundles under:
- `docs/specifications/work/implementation_tracks/performance/`

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
