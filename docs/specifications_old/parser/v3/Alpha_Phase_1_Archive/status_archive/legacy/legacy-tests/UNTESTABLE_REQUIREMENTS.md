# Untestable Requirements for Alpha 1.01.2

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


## Overview

This document lists requirements and recommendations from Agent B's security review that cannot be directly tested in the unit test framework, but should still be implemented by Agent A.

## Priority 1 (Important)

### 1. Thread Safety Documentation
**Requirement**: Add comprehensive thread safety documentation to all public interfaces.

**Why it can't be tested**: Documentation is not executable code.

**Implementation needed**:
- Add thread safety notes to class headers
- Document that current implementation is single-threaded
- Specify which classes have internal synchronization (PageManager, BufferPool)
- Note that Database class requires external synchronization

**Example**:
```cpp
/**
 * @class Database
 * @brief Main database interface
 * 
 * Thread Safety: NOT thread-safe. All methods require external synchronization
 * when accessed from multiple threads. The Database object should be used by
 * a single thread or protected by external mutex.
 */
```

## Priority 2 (Nice to have)

### 1. Debug Logging
**Requirement**: Add debug logging for troubleshooting page management operations.

**Why it can't be tested**: Logging is typically compile-time controlled and doesn't affect functionality.

**Implementation needed**:
- Add SCRATCHBIRD_DEBUG macro
- Log page allocations/frees with page IDs
- Log buffer pool hits/misses
- Log eviction decisions
- Make logging compile-time optional

**Example**:
```cpp
#ifdef SCRATCHBIRD_DEBUG
    fprintf(stderr, "[PageManager] Allocated page %u, free pages: %u/%u\n", 
            page_id, free_pages_, total_pages_);
#endif
```

### 2. fsync() for FSM Durability
**Requirement**: Ensure FSM updates are durable with explicit fsync.

**Why it can't be tested**: fsync behavior is OS-dependent and can't be reliably tested in unit tests.

**Implementation needed**:
- Add fsync() after FSM bitmap updates
- Consider batching FSM updates to reduce fsync overhead
- Make fsync optional via configuration

**Code location**: `page_manager.cpp` - after writing FSM page

### 3. Comprehensive FSM Corruption Detection
**Requirement**: Add checksums or additional validation to FSM pages.

**Why it can't be fully tested**: While we can test detection of corrupt data, we can't test all possible corruption scenarios.

**Implementation needed**:
- Add CRC32 checksum to FSM page (similar to regular pages)
- Validate free page count matches bitmap
- Check for impossible states (e.g., page allocated beyond file size)
- Add periodic FSM consistency checks

### 4. File Size Limit Documentation
**Requirement**: Document maximum database size limits.

**Why it can't be tested**: Cannot actually create TB-sized files in tests.

**Implementation needed**:
- Add constants for maximum database sizes
- Document in README or design docs
- Add runtime checks before allocation would exceed limits

**Limits by page size**:
- 8KB pages:  ~32TB (4,294,967,295 pages)
- 16KB pages: ~64TB
- 32KB pages: ~128TB

## Testing Limitations

### 1. Actual Concurrent Access
The current design is single-threaded. While we have mutexes in place, we cannot properly test true concurrent access patterns without risking test flakiness.

### 2. File System Limits
We cannot test actual file system limits (file size, number of files, etc.) as this would require:
- Root access
- Massive disk space
- Platform-specific behavior

### 3. OS-Specific Behavior
Items that vary by operating system:
- fsync() effectiveness
- File locking behavior  
- Signal handling during I/O
- Memory mapping (future feature)

## Recommendations for Agent A

1. **Add Thread Annotations**: Consider using thread safety annotations if available:
   ```cpp
   class SCRATCHBIRD_CAPABILITY("mutex") Database {
       // ...
   };
   ```

2. **Add Debug Build Target**: Create a debug build configuration:
   ```cmake
   option(SCRATCHBIRD_DEBUG "Enable debug logging" OFF)
   if(SCRATCHBIRD_DEBUG)
       add_compile_definitions(SCRATCHBIRD_DEBUG)
   endif()
   ```

3. **Add Design Documentation**: Create DESIGN.md documenting:
   - Thread safety model
   - File size limits
   - Durability guarantees
   - Performance characteristics

4. **Consider Future Features**:
   - Add page checksums to FSM page
   - Add version number to FSM for upgrades
   - Plan for multi-threaded access in future versions

## Summary

While these items cannot be directly tested in unit tests, they are important for production quality:

- **Documentation**: Critical for correct usage
- **Debug logging**: Essential for troubleshooting
- **fsync() usage**: Important for durability
- **Design limits**: Must be documented for users

Agent A should implement these items even though Agent C cannot create tests for them. The implementation should follow the patterns established in the existing codebase.

---

*Generated by Agent C - Test Verification Code Generator*
*For: Alpha 1.01.2 Page Management*
*Date: Requirements Documentation Complete*
