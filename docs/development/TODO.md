# ScratchBird Development TODO

**Last Updated:** October 11, 2025
**Based On:**
- [Code Audit Report](../audit/after_transaction_work.md) (October 6, 2025)
- [Documentation Audit Report](../audit/after_transaction_documentation_work.md) (October 11, 2025)
- [Alpha 1.2 Requirements](../issues/ALPHA_1_2_REQUIREMENTS.md)

This document tracks actual implementation work needed based on comprehensive code analysis. Items are prioritized by severity and impact.

**Major Updates (October 11, 2025):**
- ✅ **Phase 2 Complete:** ConnectionContext with thread-local storage implemented
- ✅ **Phase 3 Complete:** Firebird Transaction Model fully operational
- 🔥 **New Critical Issues:** 5 critical issues identified in code audit (CRIT-001 through CRIT-005)

---

## 🎯 ALPHA 1.2 RELEASE ITEMS (New Priority)

**Target:** Complete core functionality before SBLR stage
**Estimated Effort:** 20-30 weeks with 2-3 developers (parallelizable)
**Key Principle:** No features deferred - complete the engine properly before SBLR integration
**Full Requirements:** See [ALPHA_1_2_REQUIREMENTS.md](../issues/ALPHA_1_2_REQUIREMENTS.md)

**Already Complete:**
- ✅ B-Tree Index (~2,256 lines, tests passing)
- ✅ Hash Index (~2,254 lines, tests passing)

---

### ALPHA-001: Complete All Missing Primitive Data Types 🔥 CRITICAL
**Files:** `src/core/types.cpp`, `include/scratchbird/core/types.h`
**Effort:** 4-6 weeks
**Impact:** Full type system support per specification
**Priority:** CRITICAL

**Currently Missing:**
- ❌ INT128 - 128-bit signed integer
- ❌ UINT8, UINT16, UINT32, UINT64 - Unsigned integers
- ❌ MONEY - Fixed-precision currency type
- ❌ INTERVAL - Time interval type
- ❌ JSONB - Binary JSON format (optimized)
- ❌ XML - XML document type
- ❌ VECTOR - Vector embeddings for similarity search
- ❌ ARRAY - Multi-dimensional arrays
- ❌ COMPOSITE/RECORD - Structured types
- ⚠️ DECIMAL - Currently string storage, needs proper arithmetic

**NO DEFERRALS:** All types must be implemented per user requirements.

**Requirements:**
1. **INT128 and Unsigned Integers (1 week)**
   - Add to TypedValue::VariantType storage
   - Implement arithmetic operations
   - Add type conversions

2. **MONEY Type (2 days)**
   - Fixed-precision currency type
   - Currency-specific operations

3. **INTERVAL Type (3 days)**
   - Time interval storage and arithmetic
   - Support for various interval units

4. **DECIMAL Arithmetic (1 week)**
   - Replace string storage with proper arithmetic
   - Consider boost::multiprecision or GNU MPFR

5. **JSONB (1 week)**
   - Binary JSON format for efficient storage/querying
   - Index support via GIN

6. **XML Type (1 week)**
   - XML document storage and validation
   - XPath query support (basic)
   - Consider libxml2 or pugixml

7. **VECTOR Type (1 week)**
   - Vector embeddings storage
   - Similarity operations
   - Consider Eigen library or custom implementation

8. **ARRAY Type (1-2 weeks)**
   - Multi-dimensional array support
   - Array operations and indexing
   - NULL handling in arrays

9. **COMPOSITE/RECORD Type (1 week)**
   - Structured type support
   - Nested field access
   - Integration with DOMAIN system

**Status:** ❌ Not Started

---

### ALPHA-002: Implement Complete DOMAIN System 🎯 CRITICAL
**Files:** New `src/core/domain_manager.cpp`, catalog tables, parser extensions
**Effort:** 12 weeks total
**Impact:** Advanced type system with constraints, security, validation
**Priority:** CRITICAL

**Phase 1: Basic DOMAIN Support (2 weeks)**
- Parser support for CREATE DOMAIN
- Catalog tables for domain storage
- Domain constraint validation
- Domain inheritance (INHERITS clause)

**Phase 2: RECORD Domains (2 weeks)**
- RECORD type storage and operations
- ROW constructor syntax
- Dot notation field access
- EXTRACT function for fields

**Phase 3: ENUM Domains (1 week)**
- ENUM type with ordered values
- SET NEXT VALUE operation
- GET VALUE FOR, GET POSITION FOR operations
- Enum comparison and ordering

**Phase 4: SET Domains (1 week)**
- SET type storage (unordered, unique)
- SET constructor syntax
- Set operators: @> (contains), && (overlaps)
- Set operations: union, intersection, difference

**Phase 5: VARIANT Type (2 weeks)**
- Runtime polymorphic type
- EXTRACT(DATATYPE FROM value)
- IS OF TYPE checks
- Type-safe casting

**Phase 6: Advanced Domain Features (4 weeks)**
- WITH SECURITY (masking, encryption, audit, permissions)
- WITH INTEGRITY (uniqueness, normalization)
- WITH VALIDATION (custom validation functions)
- WITH QUALITY (parse, standardize, enrich functions)

**Status:** ❌ Not Started

---

### ALPHA-003: Implement All Missing Index Types 📊 CRITICAL
**Files:** New index implementation files
**Effort:** 21-29 weeks (can be parallelized across developers)
**Impact:** Complete index support per specification
**Priority:** CRITICAL

**Missing Index Types:**

1. **GIN (Generalized Inverted Index) - 6-8 weeks** 🔥 HIGHEST PRIORITY
   - For array indexing, full-text search, JSON
   - Entry tree + posting trees architecture
   - Pending list for fast inserts
   - Critical for ARRAY, JSONB, full-text features

2. **Bitmap Index - 3-4 weeks**
   - For low-cardinality columns
   - Bitmap compression algorithms
   - Set operations on bitmaps (AND, OR, NOT)

3. **GIST (Generalized Search Tree) - 6-8 weeks**
   - Extensible framework for custom types
   - Support for spatial data
   - Support for range types
   - Extensibility API

4. **BRIN (Block Range Index) - 2-3 weeks**
   - For very large tables
   - Minimal storage overhead
   - Range-based indexing

5. **VECTOR Index - 4-6 weeks**
   - For similarity search (HNSW or IVF algorithm)
   - Required for VECTOR type
   - Approximate nearest neighbor search

**Parallelization:** Multiple developers can work on different index types simultaneously.

**Status:** ❌ Not Started

---

### ALPHA-004: UTF-8 Character Counting for Identifiers ⚠️ MEDIUM PRIORITY
**Files:** New `src/core/utf8_utils.cpp`, `src/parser/lexer.cpp`
**Effort:** 1 week
**Impact:** SQL standard compliance, international identifier support
**Priority:** MEDIUM

**Issue:**
- Current: 128-byte buffers (byte-based, not character-based)
- Required: 128 UTF-8 characters (1-4 bytes per character)

**Requirements:**
1. UTF-8 character counting utilities
2. Parser validation (128 characters, not bytes)
3. Proper handling of multibyte characters
4. Testing with various scripts (CJK, emoji, etc.)

**Status:** ❌ Not Started

---

### ALPHA-005: Add Comment Support for Database Objects 💬 MEDIUM PRIORITY
**Files:** `src/core/catalog_manager.cpp`, `src/parser/parser.cpp`
**Effort:** 1 week
**Impact:** Self-documenting database schemas
**Priority:** MEDIUM

**Requirements:**
1. Catalog schema changes (add comment fields)
2. Parser support for COMMENT ON syntax
3. Catalog operations (set/get/remove comments)
4. Support for all object types (schemas, tables, columns, indexes, etc.)

**SQL Syntax:**
```sql
COMMENT ON TABLE schema.table IS 'Description';
COMMENT ON COLUMN schema.table.column IS 'Description';
COMMENT ON INDEX schema.index IS 'Description';
```

**Status:** ❌ Not Started

---

### ALPHA-006: Implement Configuration File System (sb_config.ini) 📋 HIGH PRIORITY
**Files:** New `src/core/config_loader.cpp`, `include/scratchbird/core/config.h`
**Effort:** 1-2 weeks
**Impact:** Production readiness, eliminates hardcoded magic numbers
**Priority:** HIGH

**Requirements:**
1. INI file parser
2. Config singleton with type-safe accessors
3. Command-line argument support
4. Environment variable support
5. Replace all hardcoded magic numbers
6. Configuration validation

**Configuration Sections:**
```ini
[database]
default_page_size = 16384
max_connections = 100

[memory]
buffer_pool_size = 128
transaction_cache_size = 10000

[storage]
toast_chunk_size = 8192
compression_enabled = true

[btree]
min_fanout = 32
max_fanout = 256

[logging]
log_level = INFO
log_file = scratchbird.log
```

**Status:** ❌ Not Started

---

**Alpha 1.2 Total Effort Summary:**
- Foundation: 3-4 weeks (config, UTF-8, comments)
- Type System: 10-12 weeks (primitive types + DOMAIN system)
- Index Types: 21-29 weeks (parallelizable across developers)
- **With 2-3 developers working in parallel: 20-30 weeks (5-7.5 months)**

---

## ✅ Completed Critical Items (Phase 2 & 3)

### ✅ CRIT-002-COMPLETE: ConnectionContext with Thread-Local Storage
**Completion Date:** October 7, 2025 (Phase 2)
**Files Created:**
- `src/core/connection_context.cpp` (1,286 lines)
- `include/scratchbird/core/connection_context.h` (295 lines)

**Delivered:**
- ✅ ConnectionContext class with thread-local storage
- ✅ Always-in-transaction model implemented
- ✅ All 15+ locking TODO markers resolved
- ✅ Locking operational throughout codebase
- ✅ Parser support for transaction statements
- ✅ Multi-connection support enabled

**Documentation:** See `docs/planning/implemented/PHASE_2_COMPLETE.md`

---

### ✅ CRIT-004-COMPLETE: Firebird Transaction Model
**Completion Date:** October 11, 2025 (Phase 3)
**Files Created:**
- `src/core/sweep_manager.cpp` (548 lines)
- `src/core/long_transaction_monitor.cpp` (412 lines)

**Delivered:**
- ✅ Transaction markers (OIT, OAT, OST, NEXT)
- ✅ All four isolation levels (READ UNCOMMITTED, READ COMMITTED, REPEATABLE READ, SERIALIZABLE)
- ✅ Sweep mechanism with auto-sweep
- ✅ Garbage collection
- ✅ Long transaction monitoring with action policies
- ✅ READ ONLY transaction optimizations
- ✅ Monitoring queries (MON_ACTIVE_TRANSACTIONS, etc.)

**Documentation:** See `docs/planning/implemented/PHASE_3_COMPLETE.md`

---

## 🔥 Critical Priority (From October 6 Audit)

**Source:** [Code Audit Report](../audit/after_transaction_work.md)

### ✅ CRIT-001-COMPLETE: Deadlock Detection Implementation
**File:** `src/core/lock_manager.cpp`
**Completion Date:** October 12, 2025
**Effort:** 2 weeks (including testing and bug fixes)
**Impact:** Deadlocks are now detected and resolved automatically
**Priority:** COMPLETED
**Audit Reference:** after_transaction_work.md lines 67-115

**Delivered:**
1. ✅ Wait-graph construction from lock wait queues (lines 500-568)
2. ✅ Cycle detection algorithm using DFS (lines 570-613)
3. ✅ Deadlock victim selection policy - youngest transaction/highest XID (lines 615-655)
4. ✅ Deadlock abort logic with full rollback and cleanup (lines 657-710)
5. ✅ Deadlock statistics tracking (stats_.deadlocks_detected)
6. ✅ Comprehensive deadlock tests (tests/unit/test_deadlock_detection.cpp)

**Implementation Details:**
- `buildWaitGraph()`: Constructs wait-for graph by scanning lock manager state
- `findAllCycles()`: Detects all cycles in wait-for graph using DFS
- `selectVictim()`: Selects youngest transaction (highest XID) as victim
- `abortTransaction()`: Performs full rollback via TransactionManager

**Bug Fixed:**
- Fixed critical bug in `acquireLock()` (lines 124-133) that allowed conflicting locks
  to be granted incorrectly for self-conflicting modes (EXCLUSIVE, ACCESS_EXCLUSIVE)

**Test Coverage:**
- Simple 2-process deadlocks
- Multi-process cycles (3+ processes)
- Victim selection verification
- Statistics tracking
- Mixed lock modes
- Edge cases (no false positives)

**Status:** ✅ COMPLETED

---

### ✅ CRIT-002-COMPLETE: Cross-Page Tuple Updates Implementation
**File:** `src/core/storage_engine.cpp:708-834`
**Completion Date:** October 12, 2025
**Effort:** 1 day (implementation + testing)
**Impact:** UPDATE now works when tuples grow beyond page capacity
**Priority:** COMPLETED
**Audit Reference:** after_transaction_work.md lines 117-159

**Delivered:**
1. ✅ Cross-page version chain mechanism (lines 708-834)
2. ✅ HOT (Heap-Only Tuple) updates on same page (already existed in HeapPage::updateTuple)
3. ✅ Cross-page chain for large updates with proper version linking
4. ✅ Tuple relocation logic with proper locking
5. ✅ TOAST interaction handled correctly (via HeapPage::insertTuple)
6. ✅ Comprehensive test suite (tests/unit/test_cross_page_updates.cpp)

**Implementation Details:**
- When HeapPage::updateTuple returns PAGE_FULL:
  - Finds or allocates new page with sufficient free space
  - Inserts new tuple version on new page
  - Acquires lock on new tuple location
  - Re-pins old page and updates version chain pointer
  - Marks old tuple with HEAP_MOVED flag
  - Returns new location to caller

**Version Chain Structure:**
- Old tuple: xmax set, next_version_tid points to new page, HEAP_MOVED flag set
- New tuple: xmin = old xmax, next_version_tid = 0 (latest), self-referencing ctid
- Works seamlessly with existing MVCC visibility logic

**Test Coverage:**
- Basic cross-page updates when page is full
- Version chain links verified across pages
- Multiple updates creating proper chains
- HOT update vs cross-page update behavior
- MVCC visibility with cross-page updates
- Large tuple updates (6KB+)
- Error handling for invalid updates

**Index Updates (Completed October 12, 2025):**
- ✅ Index entries are automatically updated when tuples relocate across pages
- ✅ Both BTree and Hash indexes supported
- ✅ Helper functions `buildIndexKey()` and `updateIndexesForRelocation()` implemented (lines 1004-1190)
- ✅ Graceful handling when tables have no indexes or when index updates fail
- ⚠️ Note: Key extraction currently uses simplified tuple parsing (TODO at line 1006-1009 for future enhancement)

**Status:** ✅ COMPLETED (including index updates)

---

### CRIT-003: Fix Lock Manager Memory Safety 🔥 CRITICAL
**File:** `src/core/lock_manager.cpp:63-90`
**Effort:** 1 week
**Impact:** Manual memory management creates leak/corruption risk
**Priority:** CRITICAL
**Audit Reference:** after_transaction_work.md lines 161-208

**Current Issues:**
- Manual `new`/`delete` for Lock and LockRequest objects
- Prone to leaks if shutdown isn't called properly
- No exception safety
- Complex ownership model hard to maintain

**Requirements:**
1. Replace `Lock*` with `std::unique_ptr<Lock>` or `std::shared_ptr<Lock>`
2. Replace `LockRequest*` with `std::unique_ptr<LockRequest>`
3. Implement proper ownership model
4. Add exception safety (RAII)
5. Use `std::unordered_map` with smart pointer values
6. Add leak detection in debug builds
7. Verify no memory leaks with valgrind/sanitizers

**Memory Model:**
```cpp
// Before (unsafe):
Lock* lock = new Lock(resource_id);
locks_[resource_id] = lock;

// After (safe):
auto lock = std::make_unique<Lock>(resource_id);
locks_[resource_id] = std::move(lock);
```

**Status:** ❌ Not Started

---

### CRIT-004: Fix TIP Page Logic 🔥 HIGH
**File:** `src/core/tip_page.cpp`
**Effort:** 3-5 days
**Impact:** Transaction state persistence issues
**Priority:** HIGH
**Audit Reference:** after_transaction_work.md lines 210-256

**Current Issues:**
- Incorrect transaction state values written to TIP
- Magic constant `0xFF` used instead of proper TransactionState enum
- State retrieval may return incorrect values
- No validation of TIP page consistency

**Requirements:**
1. Use proper TransactionState enum values throughout
2. Remove magic constant `0xFF` usage
3. Add TIP page consistency validation
4. Implement TIP page recovery logic
5. Add comprehensive unit tests for all transaction states
6. Add integration tests for TIP persistence across restarts

**Fix Required:**
```cpp
// Before (incorrect):
tip_data[offset] = 0xFF;  // Magic number!

// After (correct):
tip_data[offset] = static_cast<uint8_t>(TransactionState::IN_PROGRESS);
```

**Status:** ❌ Not Started

---

### CRIT-005: Standardize Error Handling Pattern ⚠️ MEDIUM-HIGH
**Files:** Throughout codebase
**Effort:** 1 week
**Impact:** Prevents crashes, improves debugging
**Priority:** MEDIUM-HIGH
**Audit Reference:** after_transaction_work.md lines 258-303

**Current Issues:**
- Some functions check `if (ctx != nullptr)` before SET_ERROR_CONTEXT
- Others assume ctx is valid (will crash if nullptr)
- Inconsistent across subsystems
- No documented policy

**Requirements:**
1. Audit all `SET_ERROR_CONTEXT` calls for nullptr safety (100+ locations)
2. Choose standard pattern:
   - **Option A (Recommended):** Always check before use: `if (ctx) SET_ERROR_CONTEXT(...)`
   - Option B: Always require ErrorContext (never nullptr) - change signatures
3. Create safe wrapper macro: `SAFE_SET_ERROR_CONTEXT(ctx, ...)`
4. Update all error handling call sites
5. Document pattern in CODING_STANDARDS.md
6. Add static analysis check if possible

**Recommended Pattern:**
```cpp
// Safe macro (Option A):
#define SAFE_SET_ERROR_CONTEXT(ctx, ...) \
    do { if (ctx) { SET_ERROR_CONTEXT(ctx, __VA_ARGS__); } } while(0)

// Usage:
SAFE_SET_ERROR_CONTEXT(ctx, "Failed to acquire lock");
```

**Status:** ❌ Not Started

---

## High Priority (Major Features/Fixes)

### ✅ HIGH-001-COMPLETE: Cross-Page Tuple Updates (Now CRIT-002)
**File:** `src/core/storage_engine.cpp:708-840`
**Completion Date:** October 12, 2025
**Status:** ✅ COMPLETED (see CRIT-002-COMPLETE above for full details)

---

### HIGH-002: Fix Buffer Pool Eviction Safety
**File:** `src/core/buffer_pool.cpp:283-322`
**Effort:** 2-3 days
**Impact:** Prevents memory corruption

**Requirements:**
1. Add bounds checking for `frame_index` before access
2. Add assertion that `page_id` exists in `page_table_` before erasing
3. Add consistency check in debug builds
4. Add unit tests for edge cases

**Status:** ❌ Not Started

---

### HIGH-003: Move Magic Numbers to Configuration
**Files:** Throughout codebase
**Effort:** 1 week
**Impact:** Improves maintainability, tunability

**Requirements:**
1. Create comprehensive list of all hardcoded constants
2. Add to `include/scratchbird/core/config.h`
3. Replace all usages:
   - `uint32_t last_page = 100;` in storage_engine.cpp
   - `header->max_connections = 1;` in database.cpp
   - Other magic numbers
4. Document meaning of each constant

**Examples:**
```cpp
constexpr uint32_t MAX_HEAP_SCAN_PAGES = 100;
constexpr uint32_t DEFAULT_MAX_CONNECTIONS = 1;
constexpr uint32_t MAX_CHAIN_LENGTH = 100;
```

**Status:** ❌ Not Started

---

### HIGH-004: Implement Logging Framework
**Files:** Multiple (replacing fprintf(stderr) calls)
**Effort:** 1 week
**Impact:** Production-ready logging

**Requirements:**
1. Design logging levels (DEBUG, INFO, WARN, ERROR, FATAL)
2. Extend existing `DEBUG_LOG` from debug.h
3. Replace all `fprintf(stderr, ...)` with proper logging
4. Add log file output
5. Add configuration for log levels
6. Thread-safe logging

**Current Issues:**
- ~20+ fprintf(stderr) calls throughout code
- No structured logging
- No log files
- No log rotation

**Status:** ❌ Not Started

---

### HIGH-005: Convert Lock Manager to RAII
**File:** `src/core/lock_manager.cpp:63-90`
**Effort:** 2-3 days
**Impact:** Prevents memory leaks

**Requirements:**
1. Replace `Lock*` with `std::unique_ptr<Lock>`
2. Replace `LockRequest*` with `std::unique_ptr<LockRequest>`
3. Implement proper ownership model
4. Add exception safety
5. Add leak detection in debug builds

**Current Issues:**
- Manual `new`/`delete` for locks
- Prone to leaks if shutdown isn't called
- No exception safety

**Status:** ❌ Not Started

---

## Medium Priority (Code Quality)

### MED-001: Standardize Naming Conventions
**Files:** Throughout codebase
**Effort:** 2-3 weeks
**Impact:** Consistency, readability

**Requirements:**
1. Choose standard style (C++ Core Guidelines recommended)
2. Create/update `.clang-format` configuration
3. Run `clang-format` on entire codebase
4. Document style in CODING_STANDARDS.md
5. Add pre-commit hook for formatting

**Current Issues:**
- Mixed snake_case, camelCase, PascalCase
- Inconsistent enum naming
- Mix of styles across subsystems

**Status:** ❌ Not Started

---

### MED-002: Add Const Correctness
**Files:** Multiple headers and implementations
**Effort:** 1-2 weeks
**Impact:** Correctness, optimization

**Requirements:**
1. Audit all getter methods
2. Add `const` qualifiers where appropriate
3. Use `mutable` for internal caches if needed
4. Update method signatures throughout
5. Propagate const-correctness through call chains

**Status:** ❌ Not Started

---

### MED-003: Remove Commented Debug Code
**File:** `src/core/transaction_manager.cpp:54-64` and others
**Effort:** 2-3 days
**Impact:** Code cleanliness

**Requirements:**
1. Find all commented-out fprintf/printf calls
2. Convert useful ones to DEBUG_LOG
3. Remove obsolete commented code
4. Clean up dead code paths

**Status:** ❌ Not Started

---

### MED-004: Audit Type Casts for Safety
**Files:** Throughout codebase (676+ casts found)
**Effort:** 2-3 weeks
**Impact:** Memory safety, type safety

**Requirements:**
1. Review all `const_cast` usage (1 found - audit it)
2. Review all `reinterpret_cast` for alignment issues
3. Add static assertions for struct sizes
4. Consider using `std::byte*` instead of `uint8_t*`
5. Document ownership model for page buffers
6. Add alignment checks

**Status:** ❌ Not Started

---

### MED-005: Fix Page Size Mismatch Handling
**File:** `src/core/heap_page.cpp:65-70`
**Effort:** 1 day
**Impact:** Corruption detection

**Requirements:**
1. Add logging for page size corrections
2. Consider returning error instead of silent fix
3. Add metric for page size mismatches
4. Add tests for corrupted page sizes

**Current Behavior:** Silently corrects mismatches

**Status:** ❌ Not Started

---

## Low Priority (Future Enhancements)

### LOW-001: Convert TODO Markers to GitHub Issues
**Files:** 42+ locations
**Effort:** 1-2 days
**Impact:** Project management

**Requirements:**
1. Create GitHub issues for each TODO
2. Add issue numbers to TODO comments
3. Prioritize based on impact
4. Link to project roadmap

**Status:** ❌ Not Started

---

### LOW-002: Implement Temporary Tables or Remove Enum
**File:** `src/core/catalog_manager.cpp:64`
**Effort:** Variable (or 5 minutes to remove)
**Impact:** Feature completeness or code cleanup

**Current State:** `TEMPORARY = 2` enum defined but never used

**Options:**
1. Implement temporary table support
2. Remove unused enum value

**Status:** ❌ Not Started

---

### LOW-003: Standardize Comment Style
**Files:** Throughout codebase
**Effort:** 1 week
**Impact:** Documentation consistency

**Requirements:**
1. Adopt Doxygen for public APIs
2. Use `//` for implementation comments
3. Remove or standardize block comments
4. Generate HTML documentation with Doxygen
5. Add Doxygen configuration file

**Status:** ❌ Not Started

---

### LOW-004: Complete Unicode Support
**File:** `src/core/timezone.cpp:517`
**Effort:** 2-4 weeks
**Impact:** I18N completeness

**Requirements:**
1. Integrate ICU library
2. Implement full UCA (Unicode Collation Algorithm)
3. Add locale-specific comparisons
4. Test with various Unicode edge cases

**Status:** ❌ Not Started

---

## Parser/Lexer Improvements (Stage 1.2 Features)

### PARSER-001: Implement JOIN Support
**Files:** `src/parser/parser.cpp`, `src/parser/ast.cpp`
**Effort:** 2-3 weeks
**Impact:** SQL completeness

**Requirements:**
- INNER JOIN
- LEFT/RIGHT/FULL OUTER JOIN
- CROSS JOIN
- Join condition parsing
- Multi-table queries

**Test Failures:** 5+ tests

**Status:** ❌ Not Started

---

### PARSER-002: Implement Subquery Support
**Files:** `src/parser/parser.cpp`
**Effort:** 2-3 weeks
**Impact:** SQL expressiveness

**Requirements:**
- Scalar subqueries
- Subqueries in WHERE clause
- Subqueries in FROM clause
- Correlated subqueries

**Test Failures:** 3+ tests

**Status:** ❌ Not Started

---

### PARSER-003: Implement Constraint Parsing
**Files:** `src/parser/parser.cpp`
**Effort:** 1-2 weeks
**Impact:** Data integrity

**Requirements:**
- PRIMARY KEY
- FOREIGN KEY
- UNIQUE
- CHECK constraints
- NOT NULL

**Test Failures:** 4+ tests

**Status:** ❌ Not Started

---

### PARSER-004: Implement Table Aliases
**Files:** `src/parser/parser.cpp`
**Effort:** 3-5 days
**Impact:** SQL usability

**Requirements:**
- Table aliases in FROM clause
- Column references with aliases
- AS keyword support

**Test Failures:** 2+ tests

**Status:** ❌ Not Started

---

### LEXER-001: Fix Edge Cases
**Files:** `src/parser/lexer.cpp`
**Effort:** 1 week
**Impact:** Robustness

**Requirements:**
- Unicode edge cases
- Escape sequence handling
- Numeric literal edge cases
- String delimiter handling

**Test Failures:** 6+ tests

**Status:** ❌ Not Started

---

### LEXER-002: Add Security Validations
**Files:** `src/parser/lexer.cpp`
**Effort:** 1 week
**Impact:** Security

**Requirements:**
- Input size limits
- Deeply nested structure detection
- Invalid character handling
- DOS prevention

**Test Failures:** 3+ tests

**Status:** ❌ Not Started

---

### LEXER-003: Implement Error Recovery
**Files:** `src/parser/lexer.cpp`
**Effort:** 1 week
**Impact:** User experience

**Requirements:**
- Graceful error handling
- Error reporting with context
- Recovery from invalid tokens
- Multiple error reporting

**Test Failures:** 3+ tests

**Status:** ❌ Not Started

---

## Future Features (Beta Stage)

### FUTURE-001: Implement Write-Ahead Logging (WAL)
**Effort:** 6-8 weeks
**Impact:** Crash recovery, durability

**Requirements:**
- WAL file format design
- Log record types
- Checkpoint mechanism
- Recovery protocol
- Integration with transaction manager

**Current State:** Not implemented ("No WAL in Alpha")

**Status:** ❌ Not Started (Beta requirement)

---

### FUTURE-002: Implement B-Tree Page Merging
**File:** `src/core/btree_vacuum.cpp:328`
**Effort:** 2-3 weeks
**Impact:** Space reclamation

**Requirements:**
- Page merge algorithm
- Update parent pointers
- Handle underflow
- Add tests

**Current State:** TODO marker

**Status:** ❌ Not Started

---

### FUTURE-003: Complete Deadlock Detection
**File:** `src/core/lock_manager.cpp`
**Effort:** 2-3 weeks
**Impact:** Concurrency robustness

**Requirements:**
- Implement `buildWaitGraph()`
- Cycle detection algorithm
- Deadlock victim selection
- Add comprehensive tests

**Current State:** Stub function

**Status:** ❌ Not Started

---

### FUTURE-004: Implement Network Layer
**Effort:** 4-6 weeks
**Impact:** Multi-user access

**Requirements:**
- Socket management
- Protocol selection (PostgreSQL, MySQL, etc.)
- Authentication integration
- Connection pooling

**Current State:** Not implemented

**Status:** ❌ Not Started

---

### FUTURE-005: Add Hash Index Overflow Pages
**File:** `src/core/hash_index.cpp`
**Effort:** 2-3 weeks
**Impact:** Hash index scalability

**Requirements:**
- Overflow page allocation
- Chain management
- Update hash directory
- Add tests

**Current State:** Count returns 0

**Status:** ❌ Not Started

---

## Testing Improvements

### TEST-001: Add Concurrency Tests
**Effort:** 2-3 weeks
**Impact:** Multi-user validation

**Requirements:**
- Multi-connection stress tests
- Lock conflict scenarios
- Deadlock testing
- Race condition detection

**Blocked By:** CRIT-002 (ConnectionContext)

**Status:** ❌ Not Started

---

### TEST-002: Add Error Path Tests
**Effort:** 1-2 weeks
**Impact:** Robustness

**Requirements:**
- OOM simulation
- Disk full scenarios
- Corruption recovery
- Partial failure handling

**Status:** ❌ Not Started

---

### TEST-003: Add Boundary Tests
**Effort:** 1 week
**Impact:** Edge case coverage

**Requirements:**
- Maximum page size scenarios
- XID wraparound testing
- TOAST threshold boundaries
- Buffer pool exhaustion

**Status:** ❌ Not Started

---

### TEST-004: Add Security Tests
**Effort:** 1-2 weeks
**Impact:** Security validation

**Requirements:**
- SQL injection attempts
- Path traversal attempts
- Integer overflow scenarios
- Buffer overflow detection

**Status:** ❌ Not Started

---

## Documentation Tasks

### DOC-001: Update All Specification Documents
**Effort:** 1-2 weeks
**Impact:** Accuracy

**Requirements:**
- Mark unimplemented features clearly
- Update implementation percentages
- Add "Status" sections to each spec
- Cross-reference with audit report

**Status:** ✅ In Progress

---

### DOC-002: Create Architecture Diagrams
**Effort:** 1 week
**Impact:** Understanding

**Requirements:**
- Component interaction diagrams
- Data flow diagrams
- Transaction lifecycle
- Query execution pipeline

**Status:** ❌ Not Started

---

### DOC-003: Generate API Documentation
**Effort:** 3-5 days
**Impact:** Developer experience

**Requirements:**
- Set up Doxygen
- Add missing documentation comments
- Generate HTML docs
- Host on GitHub Pages

**Status:** ❌ Not Started

---

## Summary Statistics

**Last Updated:** October 11, 2025

**Phase 2 & 3 Completion:**
- ✅ ConnectionContext implementation (Phase 2)
- ✅ Firebird Transaction Model (Phase 3)
- ✅ 2 major critical items resolved
- 🔥 5 new critical issues identified from audit

**Current TODO Items:**
**Total Items:** 45+
**✅ Completed Critical:** 4 (ConnectionContext, Firebird Transaction Model, Deadlock Detection, Cross-Page Updates)
**🔥 Critical:** 3 (CRIT-003 through CRIT-005 from audit - BLOCKING)
**High:** 4
**Medium:** 5
**Low:** 4
**Alpha 1.2 Items:** 6 (Type system, DOMAIN, indexes, config, UTF-8, comments)
**Parser/Lexer:** 9
**Future/Beta:** 5
**Testing:** 4
**Documentation:** 3

**Estimated Remaining Effort:**
- **Critical fixes:** 1-2 weeks (CRIT-003 through CRIT-005) ⬇️ Reduced from 2-4 weeks
- **Alpha 1.2 completion:** 20-30 weeks (parallelizable with 2-3 developers)
- **Parser/Lexer:** 8-12 weeks
- **Future/Beta features:** 16-24 weeks

**Priority Order (Post Phase 2 & 3):**
1. ✅ ~~Fix CRIT-001 (Deadlock detection - 1-2 weeks)~~ **COMPLETED** 🎉
2. ✅ ~~Fix CRIT-002 (Cross-page updates - 1 day)~~ **COMPLETED** 🎉
3. Fix CRIT-003 (Lock manager memory safety - 1 week) 🔥 CRITICAL
4. Fix CRIT-004 (TIP page logic - 3-5 days)
5. Fix CRIT-005 (Error handling standardization - 1 week)
6. Alpha 1.2 items (Type system, DOMAIN, indexes - 20-30 weeks with 2-3 developers)
7. Parser/Lexer improvements (8-12 weeks)
8. Future features for Beta (WAL, network layer - 16-24 weeks)

**Progress Update:**
- **Before Phase 2 & 3:** ~60-70% of core functionality
- **After Phase 2 & 3:** ~75-80% of core functionality
- **Remaining for Alpha 1.2:** Type system completion, advanced indexes, configuration system

---

**Note:** This TODO list is based on actual code analysis, not on documentation or comments. All issues are verified to exist in the codebase as of October 11, 2025.

**See Also:**
- [Code Audit Report](../audit/after_transaction_work.md) (October 6, 2025) - Critical issues
- [Documentation Audit Report](../audit/after_transaction_documentation_work.md) (October 11, 2025) - Doc updates needed
- [Current Status](../status/CURRENT_STATUS.md) - Implementation status (updated Oct 11, 2025)
- [Phase 2 Complete](../planning/implemented/PHASE_2_COMPLETE.md) - ConnectionContext completion
- [Phase 3 Complete](../planning/implemented/PHASE_3_COMPLETE.md) - Firebird Transaction Model completion
- [Coding Standards](CODING_STANDARDS.md) - Development guidelines
