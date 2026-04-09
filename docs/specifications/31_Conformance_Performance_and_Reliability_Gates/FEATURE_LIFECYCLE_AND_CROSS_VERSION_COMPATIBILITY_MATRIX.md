# Feature Lifecycle and Cross Version Compatibility Matrix

## Purpose
Provide one authoritative lifecycle and compatibility contract for ScratchBird
features across syntax, semantics, protocol, storage, and operational scope.

## Scope
- lifecycle states
- compatibility axes
- promotion and deprecation rules
- release and downgrade claims

## Hard Invariants
1. Feature lifecycle state must be explicit and user-visible.
2. A feature may not claim stable compatibility on one axis while remaining
   undefined on another required axis.
3. Experimental or preview features may not silently widen storage or protocol
   compatibility guarantees.

## Lifecycle States
| State | Meaning |
| --- | --- |
| `design_only` | specified but not implementation-complete |
| `experimental` | implementation may exist but is non-certifiable |
| `preview` | bounded support with explicit gate requirements |
| `stable` | certified for the declared scope |
| `deprecated` | still supported but planned for removal |
| `removed` | no longer supported |

## Compatibility Axes
- syntax and parser acceptance
- execution semantics
- client or wire protocol behavior
- catalog metadata shape
- on-disk storage compatibility
- operational and backup compatibility

## Matrix Rules
1. Every feature promoted beyond `experimental` must declare compatibility on
   all applicable axes.
2. Storage-affecting features must update section 05 format inventory before
   promotion to `preview` or `stable`.
3. Protocol-affecting features must update section 30 gate and compatibility
   claims before promotion to `stable`.

## Promotion Requirements
- normative specification complete
- implementation present
- required tests and evidence passing
- unsupported behavior documented

## Cross-Section References
- `PLATFORM_SUPPORT_MATRIX_AND_CERTIFICATION_SCOPE.md`
- `TEST_CONTRACT.md`
- `../05_Page_Taxonomy_and_Binary_Layouts/ON_DISK_FORMAT_INVENTORY_AND_VERSION_MANIFEST.md`
- `../30_Client_Tooling/README.md`

## 2026-03-28 Audit Normalization Update

- Section `31` is normalized to explicit implementation-driving authority classes.
- Current gate authority is bounded to the shipped engine and driver gate entry points, especially `ScratchBird/docs/TEST.md`, `tests/conformance/public_beta/run_required_public_beta_gate.sh`, `tests/compatibility/*`, engine unit/integration/benchmark/stress suites, and driver build or implementation-gate reports under `ScratchBird-driver/docs/`.
- The required public-beta gate is the strongest current section-local release-gate authority, but it is still a bounded gate script and category set rather than proof of a fully unified enterprise certification framework.
- Compatibility manifests, benchmark suites, driver build matrices, implementation gate reports, and the system compatibility manifest are current evidence surfaces; they are not universal proof that every numbered section `31` gate is live, mandatory, and fully replayable.
- Performance, optimization, and scorecard language is bounded to the current benchmark or readiness evidence, not a completed cross-platform SLO certification program.
- Cluster gameday, operator runbook, replication, upgrade or rollback orchestration, full forensic shadow gating, and broad platform certification language remain bounded, checklist-oriented, or `target_state_only` unless direct gate scripts and replayable evidence bundles exist.
- Evidence artifact matrices and phase-dependency matrices are treated as planning or inventory surfaces unless matched by executed gate runners and preserved result artifacts.
- MGA recovery remains state-based and not WAL/redo replay; replay language in this section must stay compatible with current recovery audits.
