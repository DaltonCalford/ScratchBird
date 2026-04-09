# Test Ownership Exclusion and Flake Policy

## Purpose
Define the authoritative policy for test ownership, temporary exclusions,
flake handling, and promotion rules for section-31 evidence.

## Scope
- subsystem test ownership
- exclusion and quarantine policy
- flake tracking and retirement
- evidence-promotion rules

## Hard Invariants
1. A skipped or quarantined test is never invisible; it must remain owner-routed
   and time-bounded.
2. Flaky tests may not be silently removed from release truth.
3. Section-31 evidence may include exclusions only when the exclusion record is
   explicit, justified, and approved under this policy.

## Required Ownership Model
Every gate-relevant suite must have:
- an owning subsystem
- an owning implementation area
- an owner contact or team label
- an escalation path for persistent failures

## Exclusion Classes
| Class | Meaning |
| --- | --- |
| `infra_blocked` | environment defect outside subsystem correctness |
| `known_flake` | non-deterministic failure under active remediation |
| `feature_not_certified` | feature exists but is outside current certification scope |
| `platform_out_of_scope` | platform is not in the current support matrix |

## Flake Rules
1. A test enters flake status only after repeated reproduction and owner review.
2. Every flake entry must carry:
   - creation date
   - owner
   - subsystem
   - last reproduction date
   - planned retirement condition
3. Chronic flakes block promotion when they touch certified paths.

## Evidence Rules
1. Gate evidence must declare excluded and quarantined tests explicitly.
2. Aggregate pass summaries are invalid if they hide excluded required suites.
3. A subsystem may not claim completed implementation while its required tests
   remain indefinitely quarantined.

## Cross-Section References
- `PLATFORM_SUPPORT_MATRIX_AND_CERTIFICATION_SCOPE.md`
- `TEST_CONTRACT.md`
- `../30_Client_Tooling/README.md`

## 2026-03-28 Audit Normalization Update

- Section `31` is normalized to the code-backed `partial` standard.
- Current gate authority is bounded to the shipped engine and driver gate entry points, especially `ScratchBird/docs/TEST.md`, `tests/conformance/public_beta/run_required_public_beta_gate.sh`, `tests/compatibility/*`, engine unit/integration/benchmark/stress suites, and driver build or implementation-gate reports under `ScratchBird-driver/docs/`.
- The required public-beta gate is the strongest current section-local release-gate authority, but it is still a bounded gate script and category set rather than proof of a fully unified enterprise certification framework.
- Compatibility manifests, benchmark suites, driver build matrices, and implementation gate reports are current evidence surfaces; they are not universal proof that every numbered section `31` gate is live, mandatory, and fully replayable.
- Performance, optimization, and scorecard language is bounded to the current benchmark or readiness evidence, not a completed cross-platform SLO certification program.
- Cluster gameday, operator runbook, replication, upgrade or rollback orchestration, full forensic shadow gating, and broad platform certification language remain bounded, checklist-oriented, or `target_state_only` unless direct gate scripts and replayable evidence bundles exist.
- Evidence artifact matrices and phase-dependency matrices are treated as planning or inventory surfaces unless matched by executed gate runners and preserved result artifacts.
- MGA recovery remains state-based and not WAL/redo replay; replay language in this section must stay compatible with current recovery audits.
