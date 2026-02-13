# Phase Security/Hardening Review (AI B)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


- Phase ID: ALPHA-1.01
- Commit: cursor/initialize-and-verify-database-core-components-2a50
- Date: 2024-12-31
- Reviewer: AI B

## Scope
- Features covered: Database file creation and opening, system catalog initialization, page validation
- Interfaces touched: Database class, PageHeader structure, CRC32C checksum, UUID v7 generation
- Files/paths: src/core/database.cpp, include/scratchbird/core/{database.h,ondisk.h,status.h,uuidv7.h}

## Findings

### Correctness risks:
- **P0**: Missing OOM checks - `new uint8_t[]` allocations in database.cpp (lines 53, 238) do not check for allocation failure
- **P1**: Error enum mismatch - Status enum missing `OOM` error code despite MEMORY_MANAGEMENT.md requirement
- **P1**: Non-idempotent system catalog - No check if catalog already exists before initializing

### Concurrency/thread-safety:
- **P1**: `flock()` used for locking but test shows double-open still possible (test_lock_conflict.cpp confirms this is known)
- **P2**: No documentation of thread safety level per THREAD_SAFETY.md requirements

### Memory/ownership:
- **P0**: Memory leak on error path - If allocation at line 238 fails (throws), fd_ remains open without cleanup
- **P1**: `write_page()` casts away const to update checksum, violating const correctness

### I/O and file integrity:
- **P1**: Short read handling incomplete - `open()` method doesn't handle partial header reads gracefully
- **P2**: No `O_SYNC` or `O_DSYNC` flags for durability on critical writes

### Input validation:
- **P1**: No path traversal protection - accepts any path including "../" sequences
- **P2**: Database name hardcoded to "scratchbird.db" instead of using actual filename

### Authentication/authorization:
- File permissions set to 0644 (world-readable) - appropriate for Alpha but should be noted

### Denial of service vectors:
- **P1**: No maximum path length validation
- **P2**: No protection against extremely large page_size values in corrupted headers

### Logging/telemetry/privacy:
- No error context population as required by ERROR_HANDLING.md specification

## Recommendations

### Priority 0 (must-fix):
1. Add OOM checks after all `new` allocations:
   ```cpp
   uint8_t* page_buffer = new(std::nothrow) uint8_t[page_size];
   if (!page_buffer) return Status::OOM;
   ```
2. Add Status::OOM to the Status enum per ERROR_HANDLING.md
3. Fix memory leak in `open()` - use RAII or ensure cleanup on all paths

### Priority 1 (should-fix):
1. Implement error context population per ERROR_HANDLING.md
2. Add idempotency check for system catalog initialization
3. Validate paths for traversal attacks (reject ".." components)
4. Fix const-correctness in `write_page()`
5. Handle short reads properly in `open()`
6. Use actual database filename instead of hardcoded "scratchbird.db"

### Priority 2 (could-fix):
1. Document thread safety levels in header files
2. Add path length validation
3. Consider using `O_SYNC` for critical writes
4. Add upper bound validation for page_size from corrupted files

## Test Coverage Assessment

### Existing test coverage (good):
- Multiple page sizes (8K, 16K, 32K)
- Invalid page size rejection
- Checksum validation
- UUID v7 generation and monotonicity
- File exists error
- Corrupted magic detection
- Short file read handling

### Missing test cases (recommended):
1. **P1**: OOM simulation test (once OOM handling added)
2. **P1**: Path traversal attack test (e.g., "../../../etc/passwd")
3. **P1**: Concurrent open attempts with proper locking
4. **P2**: Extremely long path test
5. **P2**: System catalog re-initialization test (idempotency)
6. **P2**: Page write with concurrent reader test

## Change Requests (if any)
- CR-001: Add OOM error code to Status enum → docs/change_requests/CR-001.md
- CR-002: Clarify file locking requirements for Alpha → docs/change_requests/CR-002.md

## Sign-off
- Block/Proceed: **BLOCK**
- Rationale: Priority 0 issues (missing OOM checks and memory leak) must be addressed before proceeding. These represent potential crashes and resource leaks in production use. The implementation is otherwise solid and well-aligned with specifications, requiring only minor adjustments after fixing critical issues.
