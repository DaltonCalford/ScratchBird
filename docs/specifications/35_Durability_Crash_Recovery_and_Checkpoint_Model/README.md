# Section 35 Durability Crash Recovery and Checkpoint Model

Status: current_authority_with_reconstructed_expansion
Current implementation state: partial, explicitly MGA-centered and anti-WAL.

This section owns the canonical ScratchBird durability, crash recovery, checkpoint, partial-write, and corruption-containment model in ScratchBird-native terms.

## Section scope

- durability model and correctness boundary
- startup recovery flow
- checkpoint and dirty-state model
- partial-write and corruption containment
- background maintenance and recovery interaction
- incident classification and fail-closed rules

<!-- AUTO-GENERATED:FILE-LIST:START -->
- [BACKGROUND_MAINTENANCE_AND_RECOVERY_INTERACTION.md](BACKGROUND_MAINTENANCE_AND_RECOVERY_INTERACTION.md)
- [CHECKPOINT_AND_DIRTY_STATE_MODEL.md](CHECKPOINT_AND_DIRTY_STATE_MODEL.md)
- [CHECKPOINT_BOUND_DELTA_RECONCILIATION_AND_MAINTENANCE_MARKERS.md](CHECKPOINT_BOUND_DELTA_RECONCILIATION_AND_MAINTENANCE_MARKERS.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [DURABILITY_MODEL_AND_CORRECTNESS_BOUNDARY.md](DURABILITY_MODEL_AND_CORRECTNESS_BOUNDARY.md)
- [INCIDENT_CLASSIFICATION_AND_FAIL_CLOSED_RULES.md](INCIDENT_CLASSIFICATION_AND_FAIL_CLOSED_RULES.md)
- [PARTIAL_WRITE_AND_CORRUPTION_CONTAINMENT.md](PARTIAL_WRITE_AND_CORRUPTION_CONTAINMENT.md)
- `SECTION_CLOSURE_MATRIX.csv`
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [STARTUP_RECOVERY_FLOW.md](STARTUP_RECOVERY_FLOW.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
- [TRANSACTION_DURABILITY_RECOVERY_OWNERSHIP_AND_MGA_ALIGNMENT_MODEL.md](TRANSACTION_DURABILITY_RECOVERY_OWNERSHIP_AND_MGA_ALIGNMENT_MODEL.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
