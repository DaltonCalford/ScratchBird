# Issues

- task_id: `ENG-OPENSEARCH-003`
- gate: `ENG-OPENSEARCH-GATE-03`
- blocker_class: `dependency`
- observed_behavior: `Dependency blocked: ENG-OPENSEARCH-002, SB-006`
- expected_behavior: `row should complete and close gate with required evidence`
- evidence_path: `/home/dcalford/CliWork/ScratchBird/artifacts/baseline/opensearch/p6s1w3/eng-opensearch-003-compare`
- proposed_options:
  1. Install missing toolchain/dependency and rerun this row.
  2. Increase timeout and rerun if failure was timeout-related.
  3. Provide explicit skip/waiver policy for this gate.
