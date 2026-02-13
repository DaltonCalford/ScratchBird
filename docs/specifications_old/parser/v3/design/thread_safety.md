# ScratchBird Thread Safety Documentation

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


## Alpha Phase Design: Single-Threaded

For the Alpha release (1.01.x), ScratchBird is designed as a **single-threaded** database engine. This design decision simplifies the implementation while still preparing the codebase for future multi-threading support.

## Current Thread Safety Architecture

### Components with Mutex Protection

The following components have mutex protection in place, preparing for future multi-threaded operation:

1. **PageManager** (`page_manager.cpp`)
   - All public methods protected by `std::mutex`
   - Thread-safe bitmap operations
   - Atomic page allocation/deallocation

2. **BufferPool** (`buffer_pool.cpp`)
   - All public methods protected by `std::mutex`
   - Thread-safe LRU list management
   - Atomic pin/unpin operations

### Components Requiring External Synchronization

1. **Database** (`database.cpp`)
   - No internal mutex protection
   - File I/O operations are not synchronized
   - Caller must ensure single-threaded access

## Usage Requirements

### Single Process, Single Thread
- Only one Database instance should be opened per database file
- All database operations must be performed from a single thread
- No concurrent access from multiple processes is supported

### File Locking
- Advisory file locking via `flock(fd, LOCK_EX | LOCK_NB)`
- Prevents accidental multi-process access
- Does not prevent deliberate concurrent access

## Known Limitations

### Race Conditions (if used multi-threaded)
1. **Database::read_page/write_page**
   - Direct file I/O without synchronization
   - Could result in torn reads/writes

2. **Multiple Database Objects**
   - Opening same file from multiple Database instances
   - Would bypass file locking protection

3. **Signal Handlers**
   - I/O operations could be interrupted
   - No signal-safe guarantees

## Future Multi-Threading Considerations

### Phase 2 Requirements
When multi-threading support is added:

1. Add Database-level synchronization
2. Implement read/write locks for pages
3. Add transaction isolation
4. Implement MVCC or similar concurrency control

### Preparation Already in Place
- Mutex protection in PageManager and BufferPool
- Clean separation of concerns
- No global state

## Debug Assertions

When compiled with `DEBUG` or `SCRATCHBIRD_DEBUG` defined:
- Thread ownership assertions can be added
- Race condition detection can be implemented
- Lock order verification can be enforced

## Best Practices for Alpha Users

1. **Single-Threaded Usage**
   ```cpp
   // Good - single thread
   Database db;
   db.open("mydb.db");
   // All operations from same thread
   ```

2. **Avoid Concurrent Access**
   ```cpp
   // Bad - multiple threads
   std::thread t1([&db]() { db.read_page(...); });
   std::thread t2([&db]() { db.write_page(...); });
   ```

3. **Proper Cleanup**
   ```cpp
   // Always close before opening elsewhere
   db.close();
   // Now safe to open in another context
   ```

## Error Handling

If concurrent access is attempted:
- File locking will prevent multiple processes
- Single process with multiple threads: undefined behavior
- Use external synchronization if needed

## Summary

ScratchBird Alpha is intentionally single-threaded to focus on correctness and simplicity. The mutex infrastructure is in place for future expansion, but users must ensure single-threaded access for Alpha releases.
