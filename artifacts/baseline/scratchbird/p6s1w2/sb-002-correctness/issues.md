# Issues

- task_id: `SB-002`
- gate: `SB-GATE-02`
- blocker_class: `dependency`
- observed_behavior: `Dependency blocked: SB-001`
- expected_behavior: `row should complete and close gate with required evidence`
- evidence_path: `/home/dcalford/CliWork/ScratchBird/artifacts/baseline/scratchbird/p6s1w2/sb-002-correctness`
- proposed_options:
  1. Install missing toolchain/dependency and rerun this row.
  2. Increase timeout and rerun if failure was timeout-related.
  3. Provide explicit skip/waiver policy for this gate.
