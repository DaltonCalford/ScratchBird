# Gate Framework and Stage Policy (Alpha)

## Purpose
Define mandatory gate model, execution order, and promotion policy for Alpha implementation.

## Gate Execution Order
1. `G1_SCHEMA_AND_STATIC_VALIDATION`
2. `G2_UNIT_AND_COMPONENT`
3. `G3_PROTOCOL_AND_HANDSHAKE_CONFORMANCE`
4. `G4_VM_COMPILER_EXECUTOR_CONFORMANCE`
5. `G5_SYSTEM_INTEGRATION`
6. `G6_PERFORMANCE`
7. `G7_RELIABILITY_AND_CHAOS`
8. `G8_SECURITY_AND_AUDIT`
9. `G9_RELEASE_CANDIDATE_SIGNOFF`

No stage skipping is allowed.

## Pass/Fail Rules
A gate passes only when:
1. all required tests in that gate pass.
2. failure-rate threshold is met.
3. evidence artifacts are complete and checksum-stable.

Default repetition policy:
- each gate suite runs 10 repetitions unless overridden.

Failure-rate threshold:
- max 1% failure rate per gate suite.

## Promotion Policy
- promotion to next gate requires current gate pass.
- regression in already-passed gate blocks release candidate.
- release channels progress in order:
  `canary -> beta -> stable -> lts`.
- an `lts` designation is valid only for a release line that has already met
  the `stable` gate and published the required support/deprecation artifacts.
- at most one LTS line may be active at a time, with a planned overlap window
  before the older LTS line reaches end-of-support.
- a build marked `lts` must fail release-candidate signoff if its published
  support horizon or deprecation windows are weaker than the stable lane.
- a build marked `beta` or `stable` must not claim LTS status in runtime
  banners, runbooks, or evidence bundles.

## Lifecycle Notice Policy
- beta-lane removals require at least one minor-release boundary and at least
  90 days published notice before removal from a supported line.
- stable-lane removals require at least two minor-release boundaries and at
  least 180 days published notice.
- LTS removals or end-of-support require at least two minor-release boundaries
  and at least 365 days published notice.
- promotion or deprecation claims are not complete until the notice policy is
  both documented and emitted by the runtime version surface.

## Freeze Rules
Before `G9_RELEASE_CANDIDATE_SIGNOFF`:
1. no unresolved critical defects.
2. no open protocol-contract mismatches.
3. no unresolved verifier correctness failures.

## Evidence Storage Rule
All gate artifacts must be written under:
- `docs/specifications/work/implementation_tracks/`
- `docs/specifications/work/findings/`

No gate outputs are stored in numbered section directories.

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
