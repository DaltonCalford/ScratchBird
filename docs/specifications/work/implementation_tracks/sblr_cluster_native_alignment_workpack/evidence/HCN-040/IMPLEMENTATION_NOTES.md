# Implementation Notes - HCN-040

Code paths:
- `include/scratchbird/core/observability_contract.h`
- `src/core/observability_contract.cpp`
- `src/core/telemetry.cpp`
- `tests/unit/test_observability_metric_contract.cpp`

Key contract behavior:
- `MetricContractPolicy::auditRegistry(...)` scans canonical samples and emits deterministic violations.
- `MetricContractPolicy::registerSbObsBaselineMetrics(...)` registers required `sb_engine_*` and `sb_cluster_*` metrics.
- Telemetry initialization now registers canonical SB-OBS metrics in parallel with legacy `scratchbird_*` families to avoid abrupt compatibility breaks.
