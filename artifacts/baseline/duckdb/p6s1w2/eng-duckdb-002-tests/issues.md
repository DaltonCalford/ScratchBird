# Issues

- task_id: `ENG-DUCKDB-002`
- gate: `ENG-DUCKDB-GATE-02`
- blocker_class: `build reproducibility`
- observed_behavior: `DuckDB unit run reached test 131/4336 then was SIGTERM-terminated by timeout; failing marker points to test_capi_prepared.cpp:664 ("Test STRING LITERAL parameter type") under forced termination`
- expected_behavior: `row should complete and close gate with required evidence`
- evidence_path: `/home/dcalford/CliWork/ScratchBird/artifacts/baseline/duckdb/p6s1w2/eng-duckdb-002-tests`
- proposed_options:
  1. Increase timeout budget for `make -j2 unit` to allow progression beyond early C API suite.
  2. Re-run and confirm whether `test_capi_prepared.cpp:664` is a real failure or timeout artifact.
  3. Keep downstream DuckDB rows blocked until native test gate closes.
