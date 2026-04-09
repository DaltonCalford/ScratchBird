# Cluster Game-Day and Operator Runbook Gates

## Purpose
Define mandatory game-day gate scenarios and operator runbook validation so incident handling is deterministic, reproducible, and implementation-ready.

## Scope
- Covers production-like failure drills for cluster control-plane and data-plane operations.
- Covers runbook execution correctness, timing, safety controls, and evidence completeness.
- Covers explicit evidence packaging requirements under `docs/specifications/work/implementation_tracks/gate_evidence/`.

## Hard Invariants
1. Every game-day scenario must run from a versioned runbook with step IDs.
2. Every operator action must be logged with actor, timestamp, and command payload.
3. Safety fences must be enforced before destructive recovery steps.
4. Scenario replay with same seed and topology must produce equivalent outcome classification.
5. Missing required evidence files is an automatic gate failure.

## Required Scenario Gates

### `T31-G12-01` PKI Compromise and Emergency Revocation
Objective:
- Revoke compromised channel certificate, propagate revocation, confirm fail-closed handshakes.

Mandatory checks:
1. Revocation row persisted with approval ticket.
2. All nodes consume revocation revision within policy window.
3. Compromised cert handshakes rejected after propagation.
4. New staged replacement cert becomes active with bounded drain.

### `T31-G12-02` Clock-Skew Fence and Recovery
Objective:
- Inject hard skew on one node, confirm write fence and deterministic recovery.

Mandatory checks:
1. Node enters `HARD_SKEW` and write admission rejects.
2. Leadership leases are relinquished.
3. Recovery requires two healthy samples before un-fencing.

### `T31-G12-03` Metadata Leader Failure and Promotion
Objective:
- Kill metadata leader and verify deterministic successor promotion and catalog write continuity.

Mandatory checks:
1. Leader failure detected within timeout policy.
2. Successor selected by deterministic tie-break.
3. No split-brain catalog write window.

### `T31-G12-04` OLTP Drain/Replace and Shard Safety
Objective:
- Drain one OLTP node, replace it, rebalance safely without write correctness regression.

Mandatory checks:
1. New requests stop at drain start.
2. In-flight transactions complete or abort by timeout policy.
3. Shard leadership transfer completes before offline.
4. Replacement node joins and warms before routing activation.

### `T31-G12-05` Router Overload with SLO Burn Governance
Objective:
- Drive router/OLTP overload; verify burn-rate classification, admission tuning, and autoscale response.

Mandatory checks:
1. Burn severity transitions (`MODERATE/HIGH/CRITICAL`) follow formula thresholds.
2. Admission changes are bounded and persisted.
3. Autoscale action follows cooldown/bounds.
4. Recovery path restores normal policy levels gradually.

### `T31-G12-06` Network Partition and Recovery
Objective:
- Induce partition; validate fence behavior, degraded routing, and controlled recovery.

Mandatory checks:
1. Partition state transitions are deterministic.
2. Unsafe writes are blocked by policy.
3. Recovery clear workflow requires required approvals.

### `T31-G12-07` Backup/Restore Operational Drill
Objective:
- Run backup and restore drill and validate recovery objective compliance and audit evidence.

Mandatory checks:
1. Backup record reaches success with complete metadata.
2. Restore validation confirms data and index integrity checks.
3. Recovery timing metrics are captured and compared against policy.

## Runbook Format Contract
Each scenario must include a runbook file named RUNBOOK.md in the scenario gate directory:
- step IDs must use format `RB-<gate>-NN`.
- Every step has:
  - `preconditions`
  - `command`
  - `expected state`
  - `rollback step`
  - `evidence artifact reference`

## Mandatory Evidence Bundle
Each `T31-G12-*` gate directory must include:
- `RUN_MANIFEST.json`
- `SCENARIO_TIMELINE.ndjson`
- operator command log file named OPERATOR_COMMAND_LOG.md
- `FAILURE_INJECTION_LOG.ndjson`
- `EXPECTED_VS_OBSERVED.csv`
- `RECOVERY_VALIDATION.csv`
- runbook file named RUNBOOK.md
- `CHECKSUMS.sha256`

Scenario-specific required files:
- `T31-G12-01`: `PKI_REVOCATION_PROPAGATION_AUDIT.csv`
- `T31-G12-02`: `CLOCK_SKEW_TRANSITIONS.csv`
- `T31-G12-03`: `LEADER_PROMOTION_TRACE.ndjson`
- `T31-G12-04`: `DRAIN_AND_REBALANCE_AUDIT.csv`
- `T31-G12-05`: `SLO_BURN_AND_TUNING_AUDIT.csv`
- `T31-G12-06`: `PARTITION_FENCE_RECOVERY_AUDIT.csv`
- `T31-G12-07`: `BACKUP_RESTORE_DRILL_AUDIT.csv`

## Pass/Fail Criteria
1. All mandatory checks for scenario are true.
2. All required artifacts exist with valid checksum.
3. No unresolved critical safety violation at scenario end.
4. Recovery state equals expected stable target state.
5. Replay classification is deterministic across two repeated runs.

## Deterministic Error Classes
| Code | Condition |
| --- | --- |
| `SB-GAMEDAY-0001` | runbook step missing required fields |
| `SB-GAMEDAY-0002` | required evidence artifact missing |
| `SB-GAMEDAY-0003` | safety fence bypass detected |
| `SB-GAMEDAY-0004` | scenario recovery target not reached |
| `SB-GAMEDAY-0005` | non-deterministic outcome classification across reruns |

## Normative Implementation Checklist
1. Implement gate runner support for `T31-G12-01..07`.
2. Implement strict runbook parser enforcing required step schema.
3. Implement evidence collector with fixed filenames and checksum generation.
4. Implement replay comparator for deterministic outcome classification.
5. Fail gate immediately on missing artifact or safety-fence bypass.

## Cross-Section Links
- `19_Security_Model/PKI_LIFECYCLE_CLUSTER_CHANNELS.md`
- `25_Runtime_Modes/CLUSTER_CLOCK_DISCIPLINE_AND_SKEW_POLICY.md`
- `25_Runtime_Modes/NODE_ROLE_SLO_AND_ERROR_BUDGET_POLICY.md`
- `31_Conformance_Performance_and_Reliability_Gates/GATE_EVIDENCE_ARTIFACT_MATRIX.md`
- `31_Conformance_Performance_and_Reliability_Gates/PHASE_GATE_DEPENDENCY_MATRIX.md`

## 2026-03-28 Audit Normalization Update

The original audit-normalization note is superseded by the rebuild program.

Current rule:

- this file defines reconstructed-required game-day and runbook certification
  behavior where lost-spec intent governs the product
- any scenario not yet proven by an executed gate runner must still be treated
  as a required certification target, not dropped from canon
- implementation drift belongs in planning and gate backlogs, not as silent
  gaps in the specification
- MGA recovery remains state-based and never degrades into WAL or redo-replay
  assumptions inside these runbooks

## 2026-03-29 Rebuild Reconciliation Update

The rebuild now treats this file as:

- current authority for the mandatory evidence bundle and deterministic runbook
  format
- reconstructed-required authority for cluster game-day scenarios whose full
  gate runners or replay bundles are not yet all shipped
- the canonical home for operator drill requirements that must later align with
  sections `20`, `25`, `39`, and `42`

Canonical rule:

- missing gate automation does not void the requirement
- it creates implementation drift that must be tracked separately
