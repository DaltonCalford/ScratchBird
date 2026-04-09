# Implementation Notes - HCN-042

Code paths:
- `include/scratchbird/core/observability_contract.h`
- `src/core/observability_contract.cpp`
- `include/scratchbird/core/telemetry.h`
- `src/core/telemetry.cpp`
- `tests/unit/test_health_readiness_contract.cpp`

Contract behavior:
- Readiness requires liveness + database/catalog/epoch/listener/control-plane/lease/shard-map all healthy.
- `/healthz` is liveness-focused; `/readyz` is readiness-focused.
- Both endpoints include deterministic component-level detail payloads.
