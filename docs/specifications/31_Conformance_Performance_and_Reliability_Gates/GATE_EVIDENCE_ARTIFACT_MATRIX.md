# Gate Evidence Artifact Matrix

## Purpose
Map each gate id to required evidence artifact filenames and exact output directory under `docs/specifications/work/`.

## Rules
1. Each gate writes to its own directory.
2. Filenames are mandatory and case-sensitive.
3. Re-runs overwrite files in the same gate directory for the same run-id only if explicitly allowed by runner policy; otherwise create a new run-id subdirectory.

## Matrix

| Gate Id | Phase | Required Artifact Filenames | Output Directory |
| --- | --- | --- | --- |
| `T23-A` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PLAN_DECISION_TRACE.ndjson; EXECUTION_METRICS.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T23-A/` |
| `T23-B` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PLAN_DECISION_TRACE.ndjson; EXECUTION_METRICS.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T23-B/` |
| `T23-C` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PLAN_DECISION_TRACE.ndjson; EXECUTION_METRICS.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T23-C/` |
| `T23-D` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PLAN_DECISION_TRACE.ndjson; EXECUTION_METRICS.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T23-D/` |
| `T23-E` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PLAN_DECISION_TRACE.ndjson; EXECUTION_METRICS.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T23-E/` |
| `T23-F` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PLAN_DECISION_TRACE.ndjson; EXECUTION_METRICS.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T23-F/` |
| `T23-G` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PLAN_DECISION_TRACE.ndjson; EXECUTION_METRICS.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T23-G/` |
| `T23-H` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PLAN_DECISION_TRACE.ndjson; EXECUTION_METRICS.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T23-H/` |
| `T23-I` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PLAN_DECISION_TRACE.ndjson; EXECUTION_METRICS.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T23-I/` |
| `T23-J` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PLAN_DECISION_TRACE.ndjson; EXECUTION_METRICS.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T23-J/` |
| `T25-BASE` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; NODE_ROUTING_TRACE.ndjson; CLUSTER_STATE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T25-BASE/` |
| `T26-A` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; FRAME_TRACE.ndjson; MESSAGE_DECODE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T26-A/` |
| `T26-B` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; FRAME_TRACE.ndjson; MESSAGE_DECODE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T26-B/` |
| `T26-C` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; FRAME_TRACE.ndjson; MESSAGE_DECODE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T26-C/` |
| `T26-D` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; FRAME_TRACE.ndjson; MESSAGE_DECODE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T26-D/` |
| `T26-E` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; FRAME_TRACE.ndjson; MESSAGE_DECODE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T26-E/` |
| `T26-F` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; FRAME_TRACE.ndjson; MESSAGE_DECODE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T26-F/` |
| `T26-G` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; FRAME_TRACE.ndjson; MESSAGE_DECODE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T26-G/` |
| `T28-A` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PARSER_TRANSLATION_TRACE.ndjson; SOURCE_MAP_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T28-A/` |
| `T28-B` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PARSER_TRANSLATION_TRACE.ndjson; SOURCE_MAP_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T28-B/` |
| `T28-C` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PARSER_TRANSLATION_TRACE.ndjson; SOURCE_MAP_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T28-C/` |
| `T28-D` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PARSER_TRANSLATION_TRACE.ndjson; SOURCE_MAP_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T28-D/` |
| `T28-E` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PARSER_TRANSLATION_TRACE.ndjson; SOURCE_MAP_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T28-E/` |
| `T28-F` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PARSER_TRANSLATION_TRACE.ndjson; SOURCE_MAP_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T28-F/` |
| `T28-G` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PARSER_TRANSLATION_TRACE.ndjson; SOURCE_MAP_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T28-G/` |
| `T28-H` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PARSER_TRANSLATION_TRACE.ndjson; SOURCE_MAP_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T28-H/` |
| `T28-I` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PARSER_TRANSLATION_TRACE.ndjson; SOURCE_MAP_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T28-I/` |
| `T28-J` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PARSER_TRANSLATION_TRACE.ndjson; SOURCE_MAP_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T28-J/` |
| `T28-K` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PARSER_TRANSLATION_TRACE.ndjson; SOURCE_MAP_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T28-K/` |
| `T28-L` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PARSER_TRANSLATION_TRACE.ndjson; SOURCE_MAP_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T28-L/` |
| `T31-G1` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; GATE_ROLLUP_EVIDENCE_INDEX.csv; RESOURCE_BUNDLE_FEATURE_MATRIX_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T31-G1/` |
| `T31-G2` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; GATE_ROLLUP_EVIDENCE_INDEX.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T31-G2/` |
| `T31-G3` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; GATE_ROLLUP_EVIDENCE_INDEX.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T31-G3/` |
| `T31-G4` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; GATE_ROLLUP_EVIDENCE_INDEX.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T31-G4/` |
| `T31-G5` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; GATE_ROLLUP_EVIDENCE_INDEX.csv; RESOURCE_BUNDLE_ACTIVATION_CACHE_INVALIDATION_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T31-G5/` |
| `T31-G6` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; GATE_ROLLUP_EVIDENCE_INDEX.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T31-G6/` |
| `T31-G7` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; GATE_ROLLUP_EVIDENCE_INDEX.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T31-G7/` |
| `T31-G8` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; GATE_ROLLUP_EVIDENCE_INDEX.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T31-G8/` |
| `T31-G9` | `PH0` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; GATE_ROLLUP_EVIDENCE_INDEX.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH0/T31-G9/` |
| `T23-K` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PLAN_DECISION_TRACE.ndjson; EXECUTION_METRICS.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T23-K/` |
| `T25-P1-01` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; NODE_ROUTING_TRACE.ndjson; CLUSTER_STATE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-P1-01/` |
| `T25-P1-02` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; NODE_ROUTING_TRACE.ndjson; CLUSTER_STATE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-P1-02/` |
| `T25-P1-03` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; NODE_ROUTING_TRACE.ndjson; CLUSTER_STATE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-P1-03/` |
| `T25-P1-04` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; NODE_ROUTING_TRACE.ndjson; CLUSTER_STATE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-P1-04/` |
| `T25-P1-05` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; NODE_ROUTING_TRACE.ndjson; CLUSTER_STATE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-P1-05/` |
| `T25-CLOCK-01` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; CLOCK_SAMPLE_TRACE.ndjson; CLOCK_STATE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-CLOCK-01/` |
| `T25-CLOCK-02` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; CLOCK_SAMPLE_TRACE.ndjson; CLOCK_STATE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-CLOCK-02/` |
| `T25-CLOCK-03` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; CLOCK_SAMPLE_TRACE.ndjson; CLOCK_STATE_AUDIT.csv; LEASE_FENCE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-CLOCK-03/` |
| `T25-CLOCK-04` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; CLOCK_SAMPLE_TRACE.ndjson; CLOCK_STATE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-CLOCK-04/` |
| `T25-CLOCK-05` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; NODE_ROUTING_TRACE.ndjson; CLOCK_STATE_AUDIT.csv; ADMISSION_DECISION_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-CLOCK-05/` |
| `T25-CLOCK-06` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; CONTROL_CHANNEL_FRAME_TRACE.ndjson; MONOTONICITY_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-CLOCK-06/` |
| `T25-SLO-01` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; SLO_PROFILE_BINDING_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-SLO-01/` |
| `T25-SLO-02` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; SLI_FORMULA_VALIDATION.csv; SLO_WINDOW_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-SLO-02/` |
| `T25-SLO-03` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; BURN_RATE_AUDIT.csv; THRESHOLD_TRANSITION_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-SLO-03/` |
| `T25-SLO-04` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; ADMISSION_TUNING_AUDIT.csv; SAFETY_BOUND_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-SLO-04/` |
| `T25-SLO-05` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; AUTOSCALE_ACTION_AUDIT.csv; COOLDOWN_ENFORCEMENT_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-SLO-05/` |
| `T25-SLO-06` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; SLO_EVIDENCE_IMMUTABILITY_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T25-SLO-06/` |
| `T26-H` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; FRAME_TRACE.ndjson; MESSAGE_DECODE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T26-H/` |
| `T28-M` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PARSER_TRANSLATION_TRACE.ndjson; SOURCE_MAP_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T28-M/` |
| `T31-G10-01` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; GATE_ROLLUP_EVIDENCE_INDEX.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T31-G10-01/` |
| `T31-G10-02` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; GATE_ROLLUP_EVIDENCE_INDEX.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T31-G10-02/` |
| `T31-G10-03` | `PH1` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; GATE_ROLLUP_EVIDENCE_INDEX.csv; REDACTION_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T31-G10-03/` |
| `T31-G12-01` | `PH1` | `RUN_MANIFEST.json; SCENARIO_TIMELINE.ndjson; OPERATOR_COMMAND_LOG.md; FAILURE_INJECTION_LOG.ndjson; EXPECTED_VS_OBSERVED.csv; RECOVERY_VALIDATION.csv; RUNBOOK.md; PKI_REVOCATION_PROPAGATION_AUDIT.csv; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T31-G12-01/` |
| `T31-G12-02` | `PH1` | `RUN_MANIFEST.json; SCENARIO_TIMELINE.ndjson; OPERATOR_COMMAND_LOG.md; FAILURE_INJECTION_LOG.ndjson; EXPECTED_VS_OBSERVED.csv; RECOVERY_VALIDATION.csv; RUNBOOK.md; CLOCK_SKEW_TRANSITIONS.csv; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T31-G12-02/` |
| `T31-G12-03` | `PH1` | `RUN_MANIFEST.json; SCENARIO_TIMELINE.ndjson; OPERATOR_COMMAND_LOG.md; FAILURE_INJECTION_LOG.ndjson; EXPECTED_VS_OBSERVED.csv; RECOVERY_VALIDATION.csv; RUNBOOK.md; LEADER_PROMOTION_TRACE.ndjson; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T31-G12-03/` |
| `T31-G12-04` | `PH1` | `RUN_MANIFEST.json; SCENARIO_TIMELINE.ndjson; OPERATOR_COMMAND_LOG.md; FAILURE_INJECTION_LOG.ndjson; EXPECTED_VS_OBSERVED.csv; RECOVERY_VALIDATION.csv; RUNBOOK.md; DRAIN_AND_REBALANCE_AUDIT.csv; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T31-G12-04/` |
| `T31-G12-05` | `PH1` | `RUN_MANIFEST.json; SCENARIO_TIMELINE.ndjson; OPERATOR_COMMAND_LOG.md; FAILURE_INJECTION_LOG.ndjson; EXPECTED_VS_OBSERVED.csv; RECOVERY_VALIDATION.csv; RUNBOOK.md; SLO_BURN_AND_TUNING_AUDIT.csv; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T31-G12-05/` |
| `T31-G12-06` | `PH1` | `RUN_MANIFEST.json; SCENARIO_TIMELINE.ndjson; OPERATOR_COMMAND_LOG.md; FAILURE_INJECTION_LOG.ndjson; EXPECTED_VS_OBSERVED.csv; RECOVERY_VALIDATION.csv; RUNBOOK.md; PARTITION_FENCE_RECOVERY_AUDIT.csv; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T31-G12-06/` |
| `T31-G12-07` | `PH1` | `RUN_MANIFEST.json; SCENARIO_TIMELINE.ndjson; OPERATOR_COMMAND_LOG.md; FAILURE_INJECTION_LOG.ndjson; EXPECTED_VS_OBSERVED.csv; RECOVERY_VALIDATION.csv; RUNBOOK.md; BACKUP_RESTORE_DRILL_AUDIT.csv; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH1/T31-G12-07/` |
| `T23-L` | `PH2` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PLAN_DECISION_TRACE.ndjson; EXECUTION_METRICS.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH2/T23-L/` |
| `T25-P2-01` | `PH2` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; NODE_ROUTING_TRACE.ndjson; CLUSTER_STATE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH2/T25-P2-01/` |
| `T25-P2-02` | `PH2` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; NODE_ROUTING_TRACE.ndjson; CLUSTER_STATE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH2/T25-P2-02/` |
| `T25-P2-03` | `PH2` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; NODE_ROUTING_TRACE.ndjson; CLUSTER_STATE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH2/T25-P2-03/` |
| `T25-P2-04` | `PH2` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; NODE_ROUTING_TRACE.ndjson; CLUSTER_STATE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH2/T25-P2-04/` |
| `T25-P2-05` | `PH2` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; NODE_ROUTING_TRACE.ndjson; CLUSTER_STATE_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH2/T25-P2-05/` |
| `T28-N` | `PH2` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; PARSER_TRANSLATION_TRACE.ndjson; SOURCE_MAP_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH2/T28-N/` |
| `T31-G10-04` | `PH2` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; GATE_ROLLUP_EVIDENCE_INDEX.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH2/T31-G10-04/` |
| `T31-G10-05` | `PH2` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; GATE_ROLLUP_EVIDENCE_INDEX.csv; SCHEDULER_FAIRNESS_AUDIT.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH2/T31-G10-05/` |
| `T31-G10-06` | `PH2` | `RUN_MANIFEST.json; RESULT_SUMMARY.md; GATE_ROLLUP_EVIDENCE_INDEX.csv; REPLAY_TRANSCRIPT.log; CHECKSUMS.sha256` | `docs/specifications/work/implementation_tracks/gate_evidence/PH2/T31-G10-06/` |

## Cross-Section Links
- `31_Conformance_Performance_and_Reliability_Gates/PHASE_GATE_DEPENDENCY_MATRIX.md`
- `31_Conformance_Performance_and_Reliability_Gates/P1_P2_OPTIMIZATION_GATE_PROFILE.md`

## 2026-03-28 Audit Normalization Update

- Section `31` is normalized to the code-backed `partial` standard.
- Current gate authority is bounded to the shipped engine and driver gate entry points, especially `ScratchBird/docs/TEST.md`, `tests/conformance/public_beta/run_required_public_beta_gate.sh`, `tests/compatibility/*`, engine unit/integration/benchmark/stress suites, and driver build or implementation-gate reports under `ScratchBird-driver/docs/`.
- The required public-beta gate is the strongest current section-local release-gate authority, but it is still a bounded gate script and category set rather than proof of a fully unified enterprise certification framework.
- Compatibility manifests, benchmark suites, driver build matrices, and implementation gate reports are current evidence surfaces; they are not universal proof that every numbered section `31` gate is live, mandatory, and fully replayable.
- Performance, optimization, and scorecard language is bounded to the current benchmark or readiness evidence, not a completed cross-platform SLO certification program.
- Cluster gameday, operator runbook, replication, upgrade or rollback orchestration, full forensic shadow gating, and broad platform certification language remain bounded, checklist-oriented, or `target_state_only` unless direct gate scripts and replayable evidence bundles exist.
- Evidence artifact matrices and phase-dependency matrices are treated as planning or inventory surfaces unless matched by executed gate runners and preserved result artifacts.
- MGA recovery remains state-based and not WAL/redo replay; replay language in this section must stay compatible with current recovery audits.
