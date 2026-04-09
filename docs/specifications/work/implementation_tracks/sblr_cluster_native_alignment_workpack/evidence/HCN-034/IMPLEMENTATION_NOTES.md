# Implementation Notes - HCN-034

Code paths:
- `include/scratchbird/core/cluster_write_safety.h`
  - Added domain control-plane event and manifest validation data models.
- `src/core/cluster_write_safety.cpp`
  - Added event-type string conversion.
  - Added canonical definition hash helper.
  - Added event append pipeline with monotonic epoch guard.
  - Added manifest export and join validation logic.
- `tests/unit/test_domain_control_plane_replication.cpp`
  - Added contract tests for event flow, join mismatch detection, and epoch/hash validation.

Behavioral notes:
- Event append rejects regressed `cluster_config_epoch` or `schema_epoch`.
- `DROP` removes domain hash projection from local manifest.
- Non-drop events require a non-empty `definition_hash`.
- Join validation always returns `Status::OK` and reports mismatch detail through structured reason strings.
