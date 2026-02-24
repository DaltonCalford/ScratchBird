# Issues

- task_id: `ENG-MYSQL-002`
- gate: `ENG-MYSQL-GATE-02`
- blocker_class: `build reproducibility`
- observed_behavior: `Command failed (timeout): cd build_codex2/mysql-test && perl mysql-test-run.pl --suite=main --do-test=select --force`
- expected_behavior: `row should complete and close gate with required evidence`
- evidence_path: `/home/dcalford/CliWork/ScratchBird/artifacts/baseline/mysql/p6s1w2/eng-mysql-002-tests`
- proposed_options:
  1. Install missing toolchain/dependency and rerun this row.
  2. Increase timeout and rerun if failure was timeout-related.
  3. Provide explicit skip/waiver policy for this gate.
