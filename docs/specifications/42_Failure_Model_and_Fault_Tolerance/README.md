# Section 42 Failure Model and Fault Tolerance

Status: current_authority

This section owns the canonical ScratchBird MGA-native failure model: truth
source precedence, fault classification, fail-closed and degraded-mode rules,
process or storage or network boundary handling, operator intervention
expectations, derivative evidence lanes, and explicit fault-tolerance limits.

Where current runtime support is narrower than an HA platform, that narrower
scope is expressed as explicit fail-closed or non-guarantee language inside the
section files. This section is not a generic high-availability placeholder.

## Governing model

- ScratchBird failure handling is MGA-native.
- Durable database state plus transaction inventory are the source of truth.
- Recovery is state reconciliation, not log replay.
- Updates and deletes are new transaction-stamped versions with lineage to
  earlier versions.
- Local immutable evidence and logical shadow capture are evidence lanes.
- `wal_after` export and remote archive delivery are derivative downstream
  lanes, not recovery authority.
- WAL is not core Alpha recovery truth.

## Section scope

- failure model and fault classification
- truth-source precedence and derivative evidence boundaries
- fail-closed and degraded-mode rules
- node, process, storage, and network failure boundary
- operator intervention and recovery boundary
- fault-tolerance non-guarantees

<!-- AUTO-GENERATED:FILE-LIST:START -->
- [BETA2_DISTRIBUTED_ATOMIC_COORDINATION_AND_PREPARED_BRANCH_MODEL.md](BETA2_DISTRIBUTED_ATOMIC_COORDINATION_AND_PREPARED_BRANCH_MODEL.md)
- [BETA2_FAILOVER_SESSION_CONTINUITY_AND_RECOVERY_CLASSIFICATION_MODEL.md](BETA2_FAILOVER_SESSION_CONTINUITY_AND_RECOVERY_CLASSIFICATION_MODEL.md)
- [BETA2_SHARD_FAILURE_REBALANCE_AND_OWNERSHIP_RECOVERY_CLASSIFICATION_MODEL.md](BETA2_SHARD_FAILURE_REBALANCE_AND_OWNERSHIP_RECOVERY_CLASSIFICATION_MODEL.md)
- [CLUSTER_IDENTITY_KEY_SHARD_AND_WRITE_FENCE_FAILURE_MODEL.md](CLUSTER_IDENTITY_KEY_SHARD_AND_WRITE_FENCE_FAILURE_MODEL.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md](FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md)
- [FAIL_CLOSED_AND_DEGRADED_MODE_RULES.md](FAIL_CLOSED_AND_DEGRADED_MODE_RULES.md)
- [FAULT_TOLERANCE_NON_GUARANTEES.md](FAULT_TOLERANCE_NON_GUARANTEES.md)
- [NODE_PROCESS_STORAGE_AND_NETWORK_FAILURE_BOUNDARY.md](NODE_PROCESS_STORAGE_AND_NETWORK_FAILURE_BOUNDARY.md)
- [OPERATOR_INTERVENTION_AND_RECOVERY_BOUNDARY.md](OPERATOR_INTERVENTION_AND_RECOVERY_BOUNDARY.md)
- `SECTION_CLOSURE_MATRIX.csv`
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
