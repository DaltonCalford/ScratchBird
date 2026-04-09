# Implementation Notes - HCN-022

Code paths:
- `include/scratchbird/core/catalog_manager.h`
  - `SessionInfo` extended with cluster/schema/security epoch pin fields.
  - new `SessionEpochValidation` structure and epoch pin APIs added.
- `src/core/catalog_manager.cpp`
  - `createSession` pins cluster/schema/security epochs.
  - session record serialization/deserialization includes new fields.
  - implemented `setSessionEpochPins` + `validateSessionEpochPins`.
- `include/scratchbird/core/cluster_write_safety.h`
  - introduced generic epoch validation enums and helpers.
- `src/core/cluster_write_safety.cpp`
  - implemented `validateSessionEpochPins(...)` policy behavior.

Safety properties:
- stale session context is never silently accepted.
- mismatch outcomes are deterministic and externally traceable.
