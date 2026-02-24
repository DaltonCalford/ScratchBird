# Issues

- task_id: `ENG-CASSANDRA-002`
- gate: `ENG-CASSANDRA-GATE-02`
- blocker_class: `build reproducibility`
- observed_behavior: `ant test timed out after build/test startup and multiple audit suites had already passed; run was still active in junit-timeout execution phase`
- expected_behavior: `row should complete and close gate with required evidence`
- evidence_path: `/home/dcalford/CliWork/ScratchBird/artifacts/baseline/cassandra/p6s1w2/eng-cassandra-002-tests`
- proposed_options:
  1. Increase timeout budget for Cassandra test gate.
  2. If full suite is out of cycle scope, define a deterministic smoke target subset for gate closure.
  3. Keep downstream Cassandra rows blocked until this gate is closed.
