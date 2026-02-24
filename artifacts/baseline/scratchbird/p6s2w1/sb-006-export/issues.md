# Issues

- task_id: `SB-006`
- gate: `SB-GATE-06`
- blocker_class: `dependency`
- observed_behavior: `Dependency blocked: SB-003, SB-004, SB-005`
- expected_behavior: `row should complete and close gate with required evidence`
- evidence_path: `/home/dcalford/CliWork/ScratchBird/artifacts/baseline/scratchbird/p6s2w1/sb-006-export`
- proposed_options:
  1. Install missing toolchain/dependency and rerun this row.
  2. Increase timeout and rerun if failure was timeout-related.
  3. Provide explicit skip/waiver policy for this gate.
