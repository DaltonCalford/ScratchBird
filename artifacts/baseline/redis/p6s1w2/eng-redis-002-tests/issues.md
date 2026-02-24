# Issues

- task_id: `ENG-REDIS-002`
- gate: `ENG-REDIS-GATE-02`
- blocker_class: `build reproducibility`
- observed_behavior: `Redis test harness was actively passing many suites when timeout SIGTERM terminated make test (no deterministic failing test assertion captured before termination)`
- expected_behavior: `row should complete and close gate with required evidence`
- evidence_path: `/home/dcalford/CliWork/ScratchBird/artifacts/baseline/redis/p6s1w2/eng-redis-002-tests`
- proposed_options:
  1. Increase timeout budget for Redis `make test`.
  2. Use deterministic smoke subset for this cycle if full suite runtime is out of scope.
  3. Keep downstream Redis rows blocked until test gate closes.
