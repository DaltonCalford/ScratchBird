# Implementation Notes - HCN-041

Code paths:
- `include/scratchbird/core/observability_contract.h`
- `src/core/observability_contract.cpp`
- `tests/unit/test_observability_sql_views.cpp`

Contract details:
- Runtime rows use canonical snapshot samples and deterministic label JSON encoding.
- Health rows are sorted by component key for deterministic output.
- Cluster shard/snapshot rows are deterministic by `(db_uuid, shard_id, ...)` ordering.
