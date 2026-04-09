# Implementation Notes - HCN-012

Code paths:
- `include/scratchbird/core/storage_lock_provider.h`
- `src/core/storage_lock_provider.cpp`
- `src/core/database.cpp`
- `src/server/daemon.cpp`
- `tests/unit/test_storage_lock_provider.cpp`

Platform behavior:
- POSIX: `flock(LOCK_EX|LOCK_NB)` and `flock(LOCK_UN)`.
- Windows: `_locking(_LK_NBLCK/_LK_UNLCK)` with seek reset.

Result mapping:
- `LOCKED`
- `LOCK_CONFLICT`
- `IO_ERROR`
