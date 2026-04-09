# Canonical Equivalence Model and Diff Classification (Alpha/Beta)

## Purpose
Define the canonical equivalence rules used by migration validation, dual execution, and compatibility conformance. This model is the mandatory comparison contract for pass/fail decisions.

## Scope
- Result-shape and value normalization.
- Error and side-effect equivalence.
- Diff classes, severity, and gate impact.
- Deterministic comparator behavior and replay requirements.

## Hard Invariants
1. Comparator decisions must be deterministic for identical inputs.
2. Normalization rules are profile-versioned and immutable per run.
3. Unsupported comparison context is a hard reject, not best-effort fallback.
4. Diff class taxonomy is closed and versioned.
5. Gate outcomes derive from diff severity policy, not ad-hoc operator judgment.

## Equivalence Levels
| Level | Meaning |
| --- | --- |
| `E0_EXACT` | Byte-identical values and row ordering. |
| `E1_CANONICAL` | Canonically equivalent after normalization rules. |
| `E2_POLICY_ACCEPTED` | Explicit policy allows bounded differences. |
| `E3_NON_EQUIVALENT` | Differences exceed policy; failure class. |

Cutover eligibility requires all required sample sets at `E0` or `E1`, unless a specific `E2` allowance exists.

## Normalization Dimensions
### 1. Shape Normalization
- Column count and column order are normalized by resolved projection map.
- Column names compare case-insensitively when profile policy allows.
- Hidden/system columns excluded unless explicitly requested.

### 2. Type Normalization
- Numeric families map to canonical numeric domain with explicit precision/scale.
- Temporal values normalize to UTC instant + original zone metadata.
- Boolean and tri-state forms normalize to canonical boolean/null.
- Binary values normalize by exact byte sequence (no text fallback).

### 3. Value Normalization
- String comparison follows explicit collation policy.
- Trailing-space behavior follows profile-specific text normalization policy.
- Floating comparisons use explicit tolerance policy only when allowed.
- JSON/document values normalize using canonical key order and number format.

### 4. Ordering Normalization
- If SQL includes deterministic ordering, row order must match.
- If ordering unspecified and policy permits, comparator may sort by canonical row digest.
- Any implicit order normalization must be recorded in diff evidence.

### 5. Error Normalization
- Compare error class family, state/code, and transaction outcome impact.
- Text message equality is advisory; code-family equality is normative.

## Side-Effect Equivalence
For mutation statements, comparator must include:
1. affected row count equivalence policy,
2. generated key/identity equivalence,
3. trigger side-effect equivalence where visible,
4. transaction outcome (commit/rollback/error) equivalence.

## Diff Class Taxonomy
| Class | Meaning | Default Severity |
| --- | --- | --- |
| `DIFF_NONE` | Equivalent under selected level | `info` |
| `DIFF_SHAPE` | Column/result shape mismatch | `critical` |
| `DIFF_TYPE` | Canonical type mismatch | `error` |
| `DIFF_VALUE` | Canonical value mismatch | `error` |
| `DIFF_ORDER` | Row order mismatch under required order | `warn` |
| `DIFF_CARDINALITY` | Row count mismatch | `critical` |
| `DIFF_ERROR` | Error code/outcome mismatch | `critical` |
| `DIFF_SIDE_EFFECT` | Mutation side-effect mismatch | `critical` |
| `DIFF_POLICY_REJECT` | Comparison blocked by policy guard | `error` |

## Gate Policy Mapping
1. Any `critical` diff blocks cutover and fails required conformance gate.
2. `error` diffs require triage and explicit waiver policy to proceed.
3. `warn` diffs are reportable but non-blocking unless threshold exceeded.
4. `info` diffs never block.

## Comparator Algorithm
1. Load comparison profile/version and seeds.
2. Validate input envelopes and result-shape metadata.
3. Normalize both sides using profile-locked rules.
4. Compute canonical digest per row and result set.
5. Evaluate equivalence level and diff class.
6. Emit comparator decision artifact with rationale fields.
7. Persist deterministic replay command and checksums.

## Deterministic Error Classes
| Code | Condition |
| --- | --- |
| `EQ_PROFILE_NOT_FOUND` | comparator profile/version missing |
| `EQ_SHAPE_UNSUPPORTED` | result shape cannot be normalized |
| `EQ_COLLATION_UNMAPPED` | required collation mapping unavailable |
| `EQ_POLICY_FORBIDS_NORMALIZATION` | requested normalization disabled by policy |
| `EQ_INPUT_CORRUPT` | compare payload integrity failure |

## Evidence Contract
Required per compare batch:
- `comparator_manifest.json`
- `profile_lock.json`
- `shape_map.csv`
- `diff_rows.csv`
- `diff_summary.json`
- `replay_command.txt`
- `checksums.sha256`

## Test Contract
Required tests:
1. repeatability under same seed/profile.
2. deterministic classification per diff class.
3. collation/timezone normalization correctness.
4. strict ordered vs unordered comparison behavior.
5. error/outcome equivalence handling.
6. policy reject paths and deterministic failures.

## Cross-Section References
- `29_Listener_and_Server_Orchestration/DUAL_EXECUTION_MIRROR_AND_AUDIT_RUNTIME.md`
- `31_Conformance_Performance_and_Reliability_Gates/EVIDENCE_ARTIFACTS_AND_REPLAY_REQUIREMENTS.md`
- `21_V3_Dialect_Surface/NATIVE_SUPERSET_COMPATIBILITY_MATRIX.md`


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
