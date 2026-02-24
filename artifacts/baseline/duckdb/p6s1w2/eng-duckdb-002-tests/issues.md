# Issues

- task_id: `ENG-DUCKDB-002`
- gate: `ENG-DUCKDB-GATE-02`
- blocker_class: `dependency`
- observed_behavior: `Dependency blocked: ENG-DUCKDB-001`
- expected_behavior: `row should complete and close gate with required evidence`
- evidence_path: `/home/dcalford/CliWork/ScratchBird/artifacts/baseline/duckdb/p6s1w2/eng-duckdb-002-tests`
- proposed_options:
  1. Install missing toolchain/dependency and rerun this row.
  2. Increase timeout and rerun if failure was timeout-related.
  3. Provide explicit skip/waiver policy for this gate.
