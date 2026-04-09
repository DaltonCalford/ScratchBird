# Lock Provider API

Namespace: `scratchbird::core`

Types:
- `enum class StorageLockResult { LOCKED, LOCK_CONFLICT, IO_ERROR }`
- `class StorageLockProvider`
  - `StorageLockResult tryLockExclusive(int fd, int* lock_errno = nullptr) const`
  - `bool unlock(int fd, int* lock_errno = nullptr) const`
- `const StorageLockProvider& getStorageLockProvider()`

Integration:
- Database create/open lock acquisition.
- Daemon PID file lock acquisition and unlock on remove.
