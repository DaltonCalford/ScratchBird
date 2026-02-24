# Issues

- task_id: `ENG-MONGODB-001`
- gate: `ENG-MONGODB-GATE-01`
- blocker_class: `build reproducibility`
- observed_behavior: `Command failed (timeout): bazel build install-mongod`
- expected_behavior: `row should complete and close gate with required evidence`
- evidence_path: `/home/dcalford/CliWork/ScratchBird/artifacts/baseline/mongodb/p6s1w1/eng-mongodb-001-build`
- proposed_options:
  1. Install missing toolchain/dependency and rerun this row.
  2. Increase timeout and rerun if failure was timeout-related.
  3. Provide explicit skip/waiver policy for this gate.
