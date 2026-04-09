# Benchmark And Load Shape Inputs

## Purpose

This file freezes the benchmark and load-shape authorities that the downstream
Beta 1 plans must inherit.

## Benchmark And Gate Input Authorities

- `../specifications/31_Conformance_Performance_and_Reliability_Gates/SCRATCHBIRD_BENCHMARKS_PROJECT_AND_MATRIX_MODEL.md`
- `../specifications/31_Conformance_Performance_and_Reliability_Gates/FULL_CLEAN_BUILD_TEST_AND_BENCHMARK_ARTIFACT_MODEL.md`
- `../specifications/31_Conformance_Performance_and_Reliability_Gates/PUBLIC_BETA_REQUIRED_GATE_EXECUTION_AND_FAILURE_MODEL.md`
- `../specifications/31_Conformance_Performance_and_Reliability_Gates/PUBLIC_BETA_REQUIRED_GATE_CATEGORY_AND_STEP_MODEL.md`
- `../TEST.md`
- `tests/conformance/public_beta/run_required_public_beta_gate.sh`

## Required Downstream Consumption

- `04-Access_Methods_Indexes_Optimizer_Memory` must consume optimizer,
  index-parity, and memory benchmark or load-shape authorities
- `05-Service_Stack_LocalIPC_Wire_Listeners_Manager` must consume wire,
  session, listener, and manager scale or restart-shape authorities
- `07-Backup_Restore_Migration_Cloud_Beta1_Ops` must consume backup, restore,
  export, and cloud-operability load-shape authorities
- `08-Tooling_Drivers_Benchmarks_Gates_Release` must own the final benchmark,
  gate, artifact, and release-evidence closure model

## Beta 1 Baseline Categories

The required public-beta gate currently establishes the minimum Beta 1 category
baseline:

- `wire_protocol`
- `transaction_semantics`
- `security_enforcement`
- `end_to_end_sql`
- `modal_nosql`
- `cluster_infra`

Generated downstream plans must preserve those categories even when their own
work goes deeper than the current gate.
