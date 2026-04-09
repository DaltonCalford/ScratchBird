# Index Governance and Scorecard Contract

## Purpose
Define the canonical governance objects, contradiction controls, and readiness
scorecards required before any ScratchBird index family becomes planner-visible
or release-claimable.

## Scope
- capability registry
- contradiction log
- defer register
- scorecard schema
- governance closure rules

## Hard Invariants
1. No index family is planner-visible by default without a complete capability
   object, lifecycle contract, metrics packet, and benchmark mapping.
2. Executor routing alone is not sufficient to claim product support.
3. Donor-family names may not imply separate storage identities without a real
   ScratchBird runtime class.
4. Research completion is not release completion; contradiction closure and gate
   evidence are mandatory.
5. No shipped index family may be treated as planner-secondary, advisory-only,
   or intentionally ignored once it is exposed as runtime-supported.

## Required Governance Artifacts

### `IndexFamilyCapabilityRegistry`
Must record, for every exposed family or alias:

- runtime class
- planner family
- canonical path set
- exactness class per path
- recheck model per path
- lifecycle model
- metrics packet type
- benchmark suite mapping

### `IndexLifecycleAndReclaimContract`
Must bind every family to:

- publication model
- retirement model
- reclaim-horizon owner
- evidence required before physical deletion

### `IndexMetricsAndCalibrationRegistry`
Must record:

- shared metrics packet
- family-specific packet version
- confidence-class rules
- calibration profile identity

### `IndexBenchmarkRegistry`
Must record:

- benchmark suite ids
- dataset descriptors
- seed-control rules
- required outputs
- artifact retention rules

### `IndexScorecardSnapshot`
Must record, per family and per cross-family lane:

- contract completeness
- MGA correctness
- planner integration
- metrics completeness
- primary-class parity
- benchmark coverage
- contradiction status
- release state

### `IndexContradictionLog`
Must classify every mismatch as one of:

- `spec_gap`
- `code_gap`
- `naming_collision`
- `overstated_surface`
- `pending_dependency`

Each contradiction entry must also record:

- owner
- target section
- closure action
- reopen condition if deferred

### `IndexDeferredRegister`
Must list every non-shipped optional family, advanced alias mode, or future
planner feature with:

- explicit rationale
- required reopening evidence
- no-silent-promotion rule

## Scorecard Schema
Allowed states:

- `Green`
- `Yellow`
- `Red`
- `Pending`

Required columns:

- `contract_completeness`
- `mga_correctness`
- `planner_integration`
- `metrics_completeness`
- `primary_class_parity`
- `benchmark_coverage`
- `contradiction_status`
- `release_state`

## Closure Rules
1. Any `Red` contradiction on a planner-visible family blocks release.
2. Any family without a complete capability registry row remains non-releasable.
3. Any family without a benchmark mapping remains non-releasable.
4. Any deferred feature must remain visible in the defer register until its
   reopen condition is met and scored again.
5. Any shipped family with `primary_class_parity != Green` remains
   non-releasable as a full optimizer-supported family.

## Primary-class parity rule

Every shipped index family is a primary optimizer class.

Therefore the governance scorecard must explicitly prove that each shipped
family has:

1. planner-family lowering
2. queryability handling
3. family-native or explicitly bounded metrics packet support
4. deterministic cost-input normalization
5. benchmark mapping
6. no silent omission from planner candidate enumeration

Any shipped family missing one of these is not allowed to hide behind generic
“index support” wording.

## Contradiction Review Method
1. Freeze authoritative inputs from section 18, section 20, and section 31.
2. Normalize every family to one comparison schema:
   - runtime class
   - alias lowering
   - lifecycle model
   - planner paths
   - metrics packet
   - benchmark suites
3. Compare canonical spec, live implementation, gate evidence, and defer
   register state.
4. Classify every mismatch in the contradiction log.
5. Assign closure owner.
6. Rerun scorecard generation before any release claim changes.

During contradiction review, each shipped family must be classified explicitly
as one of:

- `primary_class_complete`
- `primary_class_partial`
- `implemented_but_planner_secondary`
- `alias_surface_limited`
- `deferred_non_shipped`

`implemented_but_planner_secondary` is a release-blocking contradiction state.

## Cross-Section References
- `../18_Index_Framework/INDEX_RUNTIME_TAXONOMY_AND_ALIAS_LOWERING.md`
- `../18_Index_Framework/INDEX_MGA_PUBLICATION_AND_RECLAIM.md`
- `../18_Index_Framework/INDEX_FAMILY_METRICS_AND_CALIBRATION.md`
- `../18_Index_Framework/INDEX_PLANNER_PATH_TAXONOMY_AND_EXACTNESS.md`
- `BTREE_HARDENING_AND_CRASH_SAFE_INDEX_GATES.md`

## 2026-03-28 Audit Normalization Update

- Section `31` is normalized to the code-backed `partial` standard.
- Current gate authority is bounded to the shipped engine and driver gate entry points, especially `ScratchBird/docs/TEST.md`, `tests/conformance/public_beta/run_required_public_beta_gate.sh`, `tests/compatibility/*`, engine unit/integration/benchmark/stress suites, and driver build or implementation-gate reports under `ScratchBird-driver/docs/`.
- The required public-beta gate is the strongest current section-local release-gate authority, but it is still a bounded gate script and category set rather than proof of a fully unified enterprise certification framework.
- Compatibility manifests, benchmark suites, driver build matrices, and implementation gate reports are current evidence surfaces; they are not universal proof that every numbered section `31` gate is live, mandatory, and fully replayable.
- Performance, optimization, and scorecard language is bounded to the current benchmark or readiness evidence, not a completed cross-platform SLO certification program.
- Cluster gameday, operator runbook, replication, upgrade or rollback orchestration, full forensic shadow gating, and broad platform certification language remain bounded, checklist-oriented, or `target_state_only` unless direct gate scripts and replayable evidence bundles exist.
- Evidence artifact matrices and phase-dependency matrices are treated as planning or inventory surfaces unless matched by executed gate runners and preserved result artifacts.
- MGA recovery remains state-based and not WAL/redo replay; replay language in this section must stay compatible with current recovery audits.
