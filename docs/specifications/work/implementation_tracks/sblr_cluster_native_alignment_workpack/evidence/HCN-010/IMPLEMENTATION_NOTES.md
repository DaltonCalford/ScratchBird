# Implementation Notes - HCN-010

Code paths:
- `include/scratchbird/core/database.h`
  - `DatabaseHeader` expanded with cluster identity fields.
  - Public getters and `set_cluster_identity(...)` API added.
- `src/core/database.cpp`
  - `init_header_page(...)` initializes cluster identity to zero defaults.
  - `set_cluster_identity(...)` updates in-memory header + page-0 via buffer pool and syncs.

Safety notes:
- Header checksum is recomputed after identity mutation.
- Existing open path does not hard-validate header lower-bound to struct size, enabling tail-extension tolerance.
