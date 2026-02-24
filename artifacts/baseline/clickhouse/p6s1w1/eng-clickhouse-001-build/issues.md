# Issues

- task_id: `ENG-CLICKHOUSE-001`
- gate: `ENG-CLICKHOUSE-GATE-01`
- blocker_class: `build reproducibility`
- observed_behavior: `CMake configure did not complete within current timeout budget; command log shows normal compiler/toolchain detection and git hash discovery before timeout (no deterministic CMake error yet)`
- expected_behavior: `row should complete and close gate with required evidence`
- evidence_path: `/home/dcalford/CliWork/ScratchBird/artifacts/baseline/clickhouse/p6s1w1/eng-clickhouse-001-build`
- proposed_options:
  1. Increase configure timeout budget for ClickHouse baseline build phase.
  2. Re-run with unscaled/default timeout to capture terminal configure outcome.
  3. Keep downstream ClickHouse rows blocked until build gate closes.
