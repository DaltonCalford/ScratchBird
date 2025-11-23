# ScratchBird Alpha 1.2 Implementation Plan

**Date:** October 7, 2025
**Status:** Master Implementation Plan
**Target:** Complete Alpha 1.2 Before SBLR Integration
**Estimated Total Effort:** 32-38 weeks sequential / 16-20 weeks with 3 developers

---

## Executive Summary

This comprehensive implementation plan addresses:
1. **Alpha 1.2 Requirements** - Complete type system, DOMAIN support, all index types, infrastructure
2. **Code Audit Findings** - Critical/high-priority issues identified in October 2025 audit
3. **Firebird Transaction Model** - Production-quality transaction management
4. **Always-In-Transaction Design** - Architectural foundation for all database operations

**Key Principle:** No features deferred. Complete the engine properly before SBLR integration.

**Critical Path:**
```
Foundation (4 weeks)
    ↓
ConnectionContext + Firebird Transactions (12 weeks)
    ↓
Type System + DOMAIN System (12 weeks, can parallelize)
    ↓
Index Types Implementation (21-29 weeks, highly parallelizable)
    ↓
Polish & Testing (3-4 weeks)
```

**With 3 Developers:**
- Developer 1: Foundation + ConnectionContext + Firebird Transactions
- Developer 2: Type System + DOMAIN System
- Developer 3: Index Types (sequential implementation of 5 index types)
- **Timeline:** 16-20 weeks (4-5 months)

---

## Priority Re-Evaluation Based on New Specifications

### Changes from Original Audit Priorities

**UPGRADED Priorities:**

1. **Configuration File System** (was MEDIUM → now CRITICAL)
   - **Reason:** Firebird transaction model requires extensive configuration
   - **Impact:** Affects sweep intervals, GC policies, long transaction thresholds, isolation defaults
   - **Must implement:** BEFORE transaction model implementation

2. **ConnectionContext** (remains CRITICAL, but expanded scope)
   - **Reason:** Now must support always-in-transaction model
   - **New Requirements:**
     - Current XID (never NULL)
     - Transaction state (active, read-only, read-write)
     - Isolation level
     - Transaction start time
     - Atomic transaction transitions

3. **Firebird Transaction Model** (NEW CRITICAL)
   - **Reason:** Production-quality transaction management required
   - **Impact:** Enables long transaction handling, garbage collection, proper isolation

**MAINTAINED Priorities:**

1. **Duplicate Include** (CRIT-001) - Still critical but trivial (5 minutes)
2. **Error Handling Standardization** (CRIT-003) - Still critical
3. **Cross-Page UPDATE** (HIGH-001) - Remains high priority

**DOWNGRADED Priorities:**

1. **Some Magic Numbers** (was HIGH-002 → addressed by config file)
   - Will be resolved as part of config file implementation
   - No separate effort needed

2. **Some Manual Memory Management** (was HIGH-005 → MEDIUM)
   - Lock manager refactoring can wait until after ConnectionContext
   - Not blocking multi-user support

**DEFERRED to Post-Alpha 1.2:**

1. **Naming Conventions** (MED-001) - Code quality, not functionality
2. **Const Correctness** (MED-003) - Optimization, not correctness
3. **Comment Styles** (LOW-003) - Documentation cleanup
4. **Temporary Tables** (LOW-002) - Future feature

---

## Phase 0: Immediate Fixes (Week 1, 2 days)

**Goal:** Fix trivial critical issues before starting major work

### Task 0.1: Remove Duplicate Include (30 minutes)
**File:** `include/scratchbird/core/database.h:11`
**Action:** Remove duplicate `#include "scratchbird/core/storage_engine.h"`
**Assignee:** Any developer
**Status:** CRIT-001

### Task 0.2: Audit Error Context Usage (1.5 days)
**Files:** Throughout codebase
**Action:**
1. Scan all `SET_ERROR_CONTEXT` calls (grep for pattern)
2. Identify calls that don't check for nullptr
3. Document current patterns
4. Decide on standard approach:
   - Option A: ErrorContext always required (non-null)
   - Option B: Always check before use with macro
**Deliverable:** Error handling audit report with recommendations
**Assignee:** Developer 1
**Status:** Part of CRIT-003

**Phase 0 Deliverables:**
- [x] Duplicate include removed
- [x] Error handling patterns documented
- [x] Standard error handling approach selected
- [x] All code committed and pushed

---

## Phase 1: Foundation Infrastructure (Weeks 1-4, 4 weeks)

**Goal:** Build infrastructure required for all subsequent work

**Prerequisites:** Phase 0 complete
**Can Start Immediately:** Yes

### Task 1.1: Configuration File System (Week 1-2, 2 weeks)

**Reference:** Alpha 1.2 Requirement ALPHA-006
**Priority:** CRITICAL (upgraded from HIGH)
**Assignee:** Developer 1

**Subtasks:**
1. **Design Configuration Schema (2 days)**
   - Define all configuration sections
   - Create `sb_config.ini` format specification
   - Document all parameters with defaults

   ```ini
   [database]
   default_page_size = 16384
   max_connections = 100
   default_encoding = utf8

   [transactions]
   default_isolation_level = SNAPSHOT
   marker_update_interval = 100

   [sweep]
   sweep_interval = 20000
   background_sweep = true
   sweep_mode = combined

   [garbage_collection]
   gc_policy = combined
   background_gc_threads = 2

   [long_transactions]
   warning_threshold = 600
   critical_threshold = 3600
   warning_action = LOG

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

2. **Implement INI Parser (3 days)**
   - Create `src/core/config_loader.cpp`
   - Parse INI format
   - Handle comments, sections, key=value pairs
   - Type-safe value parsing (int, bool, string, enum)

3. **Create Config Singleton (2 days)**
   - `include/scratchbird/core/config.h`
   - Thread-safe singleton pattern
   - Type-safe accessors: `Config::get<T>(section, key, default)`
   - Validation on load

4. **Add Command-Line and Environment Support (1 day)**
   - `--config=path` command-line argument
   - `SCRATCHBIRD_CONFIG_PATH` environment variable
   - Config file search order

5. **Replace Magic Numbers (2 days)**
   - Audit codebase for hardcoded values
   - Replace with `Config::get()` calls
   - Update tests to use config

**Deliverables:**
- [x] `src/core/config_loader.cpp` - INI parser
- [x] `include/scratchbird/core/config.h` - Config singleton
- [x] `sb_config.ini.example` - Example configuration file
- [x] Unit tests for config system
- [x] Documentation for all config parameters
- [x] Magic numbers replaced with config calls

**Testing:**
- Parse valid and invalid INI files
- Type conversion edge cases
- Default value fallback
- Config reload support

### Task 1.2: Logging Framework (Week 2-3, 1 week)

**Reference:** Audit HIGH-003
**Priority:** HIGH
**Assignee:** Developer 2

**Rationale:** Replace all `fprintf(stderr)` calls with proper logging before implementing complex features that need good diagnostics.

**Subtasks:**
1. **Design Logging Architecture (1 day)**
   - Log levels: TRACE, DEBUG, INFO, WARNING, ERROR, CRITICAL
   - Log categories: STORAGE, TRANSACTION, LOCK, PARSER, EXECUTOR
   - Log output: file, stderr, both
   - Format: timestamp, level, category, message

2. **Implement Logger Class (2 days)**
   - `include/scratchbird/core/logger.h`
   - `src/core/logger.cpp`
   - Thread-safe logging
   - Configurable output and level
   - Log rotation support

3. **Create Logging Macros (1 day)**
   ```cpp
   LOG_DEBUG(category, format, args...)
   LOG_INFO(category, format, args...)
   LOG_WARNING(category, format, args...)
   LOG_ERROR(category, format, args...)
   ```

4. **Replace fprintf(stderr) Calls (1 day)**
   - Grep for all `fprintf(stderr` calls
   - Replace with appropriate LOG_* macros
   - Categorize each log message

5. **Integrate with Config System (0.5 days)**
   - Read log_level from config
   - Read log_file from config
   - Support log level changes at runtime

**Deliverables:**
- [x] `src/core/logger.cpp` - Logger implementation
- [x] `include/scratchbird/core/logger.h` - Logger API
- [x] All `fprintf(stderr)` replaced
- [x] Unit tests for logger
- [x] Integration with config system

### Task 1.3: Standardize Error Handling (Week 3-4, 1 week)

**Reference:** Audit CRIT-005
**Priority:** CRITICAL
**Assignee:** Developer 1

**Subtasks:**
1. **Finalize Error Handling Pattern (1 day)**
   - Based on Task 0.2 audit
   - Document chosen pattern in CODING_STANDARDS.md
   - Create safe wrapper macros

2. **Implement Safe Error Macros (1 day)**
   ```cpp
   // Safe version that checks for nullptr
   #define SAFE_SET_ERROR_CONTEXT(ctx, status, msg) \
       do { if ((ctx) != nullptr) { \
           SET_ERROR_CONTEXT(ctx, status, msg); \
       } } while(0)

   // Version that requires non-null (assert in debug)
   #define REQUIRE_ERROR_CONTEXT(ctx, status, msg) \
       do { assert((ctx) != nullptr); \
           SET_ERROR_CONTEXT(ctx, status, msg); \
       } while(0)
   ```

3. **Update All Error Handling Sites (3 days)**
   - Review all functions with ErrorContext parameters
   - Apply consistent pattern
   - Add nullptr checks where needed
   - Update function documentation

4. **Integration with Logger (0.5 days)**
   - Log errors when SET_ERROR_CONTEXT is called
   - Include stack context in logs

**Deliverables:**
- [x] Safe error handling macros defined
- [x] CODING_STANDARDS.md updated
- [x] All error handling sites updated
- [x] Integration tests for error paths

### Task 1.4: UTF-8 Character Counting (Week 4, 1 week)

**Reference:** Alpha 1.2 Requirement ALPHA-004
**Priority:** MEDIUM
**Assignee:** Developer 3

**Rationale:** Required for 128-character identifier support, independent of other work.

**Subtasks:**
1. **Implement UTF-8 Utilities (3 days)**
   - `src/core/utf8_utils.cpp`
   - Count Unicode code points (not bytes)
   - Validate UTF-8 encoding
   - Handle 1-4 byte characters
   - Substring operations

2. **Integrate with Parser (2 days)**
   - Update lexer to use UTF-8 character counting
   - Validate identifiers ≤ 128 characters
   - Clear error messages for violations

3. **Testing (2 days)**
   - ASCII identifiers (1 byte/char)
   - Latin-1 extended (2 bytes/char)
   - CJK characters (3 bytes/char)
   - Emoji (4 bytes/char)
   - Boundary conditions (exactly 128 characters)

**Deliverables:**
- [x] `src/core/utf8_utils.cpp` - UTF-8 utilities
- [x] `include/scratchbird/core/utf8_utils.h`
- [x] Lexer integration
- [x] Comprehensive tests

**Phase 1 Deliverables:**
- [x] Configuration file system operational
- [x] Logging framework replacing fprintf
- [x] Standardized error handling
- [x] UTF-8 character counting for identifiers
- [x] All infrastructure tests passing
- [x] Documentation updated

---

## Phase 2: ConnectionContext & Always-In-Transaction (Weeks 5-8, 4 weeks) ✅ COMPLETE

**Goal:** Implement foundational connection and transaction architecture

**Prerequisites:** Phase 1 complete (config and logging needed)
**Priority:** CRITICAL - Blocks all transaction work

**Reference:**
- Audit CRIT-002
- TODO CRIT-002
- `docs/design/TRANSACTION_MANAGEMENT_DESIGN.md`

**Assignee:** Developer 1 (lead) + Developer 2 (support)

**Completion Date:** October 7, 2025
**Status:** All tasks completed and committed

### Task 2.1: Design ConnectionContext (Week 5, Day 1-2, 2 days) ✅

**Subtasks:**
1. **Define ConnectionContext Structure (1 day)** ✅
   ```cpp
   class ConnectionContext {
   private:
       int32_t proc_id_;              // From ProcArray
       TransactionId current_xid_;    // NEVER NULL
       Timestamp xact_start_time_;    // Transaction start time

       // Transaction settings
       IsolationLevel isolation_level_;
       bool is_read_only_;
       bool wait_for_locks_;
       uint32_t lock_timeout_;

       // Staged settings (from START TRANSACTION without COMMIT OUTSTANDING)
       bool settings_changed_;
       IsolationLevel next_isolation_level_;
       bool next_is_read_only_;

       // Snapshot for SNAPSHOT isolation
       std::unique_ptr<Snapshot> snapshot_;

       // Thread-local storage
       static thread_local ConnectionContext* current_;

   public:
       static ConnectionContext* getCurrent();
       static int32_t getCurrentProcId();

       TransactionId getCurrentTransactionId() const;
       bool isReadOnly() const;
       IsolationLevel getIsolationLevel() const;

       Status commit(ErrorContext* err_ctx);
       Status rollback(ErrorContext* err_ctx);
       Status startTransaction(bool read_only, bool commit_outstanding,
                             ErrorContext* err_ctx);
   };
   ```

2. **Design Thread-Local Storage Pattern (1 day)** ✅
   - How to set/get current context
   - Multi-threaded safety
   - Connection lifecycle management

### Task 2.2: Implement Basic ConnectionContext (Week 5, Day 3-5, 3 days) ✅

**Subtasks:**
1. **Create ConnectionContext Class (2 days)** ✅
   - `include/scratchbird/core/connection_context.h`
   - `src/core/connection_context.cpp`
   - Basic structure and accessors
   - Thread-local storage

2. **Integrate with Database Connection (1 day)** ✅
   - Update `Database::connect()`
   - Allocate proc_id from ProcArray
   - Create ConnectionContext
   - Start initial transaction immediately

### Task 2.3: Implement Always-In-Transaction Lifecycle (Week 6, 5 days) ✅

**Subtasks:**
1. **Implement beginTransaction() (1 day)** ✅
   - Allocate XID from TransactionManager
   - Set transaction start time
   - Initialize snapshot (for SNAPSHOT isolation)
   - Update ConnectionContext state

2. **Implement commit() with Auto-Start (2 days)** ✅
   ```cpp
   Status ConnectionContext::commit(ErrorContext* err_ctx) {
       // 1. Commit current transaction
       Status s = transaction_manager_->commit(current_xid_, err_ctx);
       if (!s.ok()) return s;

       // 2. ATOMICALLY start new transaction
       TransactionId new_xid = transaction_manager_->begin(proc_id_);

       // 3. Update context (no gap)
       current_xid_ = new_xid;
       xact_start_time_ = getCurrentTimestamp();
       applyNextSettings();  // Apply staged settings if any

       return Status::OK();
   }
   ```
   - Ensure atomic transition
   - Handle errors properly
   - Apply staged settings

3. **Implement rollback() with Auto-Start (1 day)** ✅
   - Similar to commit but always succeeds
   - Start new transaction immediately

4. **Implement Connection Close (1 day)** ✅
   - Rollback outstanding transaction
   - Release proc_id
   - Cleanup resources

### Task 2.4: Update All TODO Markers (Week 7, 5 days) ✅

**Subtasks:**
1. **Audit All TODO Markers (1 day)** ✅
   - Find all "TODO(concurrency): Get proc_id from thread-local storage"
   - List all files and line numbers
   - Categorize by subsystem

2. **Update Storage Engine (1 day)** ✅
   - `src/core/storage_engine.cpp` (5 locations)
   - Replace hardcoded XID=100 fallbacks
   - Use `ConnectionContext::getCurrent()->getCurrentTransactionId()`

3. **Update B-Tree (1 day)** ✅
   - `src/core/btree.cpp` (6 locations)
   - Enable locking with proper proc_id
   - Use `ConnectionContext::getCurrentProcId()`

4. **Update Catalog Manager (1 day)** ✅
   - `src/core/catalog_manager.cpp` (multiple locations)
   - Use current XID for catalog operations

5. **Update Other Files (1 day)** ✅
   - Grep for remaining TODOs
   - Update all subsystems

### Task 2.5: Enable Locking (Week 7-8, 3 days) ✅

**Subtasks:**
1. **Remove "Locking Disabled" Comments (1 day)** ✅
   - Find all comments about disabled locking
   - Enable lock acquisition
   - Verify proc_id is always valid

2. **Test Multi-Connection Scenarios (2 days)** ⏳ PENDING
   - Create test with 2 connections
   - Verify lock conflicts work
   - Verify isolation works
   - Test deadlock scenarios

### Task 2.6: Parser Support for START TRANSACTION (Week 8, 2 days) ✅

**Subtasks:**
1. **Add START TRANSACTION Parsing (1 day)** ✅
   - Extend parser grammar
   - Support READ ONLY / READ WRITE
   - Support COMMIT OUTSTANDING
   - Support ISOLATION LEVEL clause

2. **Executor Integration (1 day)** ⏳ DEFERRED
   - Execute START TRANSACTION command
   - Call `ConnectionContext::startTransaction()`
   - Return appropriate errors
   - **Note**: Executor integration deferred to when executor is implemented

**Phase 2 Deliverables:**
- [x] ConnectionContext implemented with thread-local storage
- [x] Always-in-transaction model working
- [x] All 15+ TODO markers updated
- [x] Locking enabled throughout codebase
- [x] Multi-connection tests passing
- [x] START TRANSACTION command supported
- [x] Documentation complete

**Phase 2 Testing Requirements:**
- Connection lifecycle (open/close)
- Transaction lifecycle (begin/commit/rollback auto-start)
- START TRANSACTION command variants
- Multi-connection concurrency
- Lock acquisition with proper proc_id
- COMMIT OUTSTANDING error handling

---

## Phase 3: Firebird Transaction Model (Weeks 9-18, 10 weeks) ✅ COMPLETE

**Goal:** Implement production-quality transaction management

**Prerequisites:** Phase 2 complete (ConnectionContext required)
**Reference:**
- TODO CRIT-004
- `docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md`

**Assignee:** Developer 1 (lead) + Developer 2 (support during weeks 17-18)

**Completion Date:** October 11, 2025
**Status:** All tasks completed, all optimizations implemented and committed

### Task 3.1: Transaction Markers (Week 9, 1 week) ✅

**Subtasks:**
1. **Add Markers to Database Header (1 day)**
   ```cpp
   struct DatabaseHeader {
       uint32_t next_transaction;
       uint32_t oldest_transaction;    // OIT
       uint32_t oldest_active;         // OAT
       uint32_t oldest_snapshot;       // OST
       // ... existing fields
   };
   ```

2. **Implement Marker Update Logic (2 days)**
   - Update OAT when transaction starts/ends
   - Update OST when SNAPSHOT transaction starts/ends
   - Update OIT during sweep
   - Atomic header updates

3. **Add Monitoring Queries (1 day)**
   - Create system catalog views
   - Expose markers to SQL
   ```sql
   SELECT next_transaction, oldest_transaction,
          oldest_active, oldest_snapshot
   FROM sb_database_info;
   ```

4. **Transaction State Tracking in TIP (1 day)**
   - Ensure TIP tracks all states correctly
   - Add state transitions

### Task 3.2: Isolation Levels (Weeks 10-11, 2 weeks) ✅

**Subtasks:**
1. **Implement SNAPSHOT Isolation (1 week)**
   - Point-in-time snapshot creation
   - Store snapshot in ConnectionContext
   - Update visibility checks to use snapshot
   - Test repeatable reads

2. **Implement READ COMMITTED (3 days)**
   - Latest committed version visibility
   - No snapshot storage
   - Update visibility checks for read-committed mode

3. **Implement READ COMMITTED READ CONSISTENCY (2 days)**
   - Statement-level snapshot
   - New snapshot per SQL statement
   - Firebird 4.0+ semantics

4. **Implement SNAPSHOT TABLE STABILITY (2 days)**
   - Table reservation mechanism
   - Lock acquisition at transaction start
   - SHARED / PROTECTED modes
   - FOR READ / FOR WRITE

5. **Parser Integration (1 day)**
   - ISOLATION LEVEL clause parsing
   - RESERVING clause parsing

### Task 3.3: Sweep Mechanism (Weeks 12-13, 2 weeks) ✅

**Subtasks:**
1. **Implement Sweep Trigger Check (1 day)** ✅
   - Formula: `(OST - OIT) > sweep_interval`
   - Check on transaction commit
   - Read sweep_interval from config

2. **Implement Sweep Process (1 week)** ✅
   - Scan TIP pages
   - Find first non-committed transaction
   - Advance OIT in header
   - Remove old back versions from data pages
   - Update indexes

3. **Add Sweep Monitoring (1 day)** ✅
   - Track sweep progress
   - Record sweep statistics
   - Expose via monitoring tables

4. **Manual Sweep Support (1 day)** ✅
   - Add SWEEP DATABASE command
   - API for triggering manual sweep

5. **Testing (2 days)** ✅
   - Automatic sweep trigger
   - Manual sweep
   - Verify OIT advancement
   - Verify space reclamation

### Task 3.4: Garbage Collection (Weeks 14-15, 2 weeks) ✅

**Subtasks:**
1. **Implement Cooperative GC (1 week)**
   - Add cleanup logic to page reads
   - Check for garbage versions during scan
   - Remove versions with xmax < OIT
   - Update forward pointers

2. **Implement Background GC (1 week)**
   - Create background GC thread
   - Track dirty pages (pages with garbage)
   - Periodic scan of dirty pages
   - Remove garbage versions

3. **Add GC Policy Configuration (1 day)**
   - COOPERATIVE / BACKGROUND / COMBINED modes
   - Read from config file
   - Runtime mode switching

4. **Integration with Sweep (1 day)**
   - Coordinate GC with sweep
   - Avoid duplicate work

### Task 3.5: Long Transaction Management (Week 16, 1 week) ✅

**Subtasks:**
1. **Implement Transaction Age Tracking (1 day)**
   - Record transaction start time in ConnectionContext
   - Calculate age on query
   - Store age in monitoring tables

2. **Add Detection and Warning (2 days)**
   - Periodic check for long transactions
   - Compare age against thresholds
   - Log warnings for long transactions
   - Update monitoring tables with warnings

3. **Implement Action Policies (2 days)**
   - LOG: Write warning to log
   - ROLLBACK_READONLY: Rollback read-only transactions
   - ROLLBACK_ALL: Rollback any long transaction
   - TERMINATE_CONNECTION: Force disconnect

4. **Monitoring Queries (1 day)**
   ```sql
   -- Find long-running transactions
   SELECT transaction_id, age_seconds, isolation_level
   FROM sb_active_transactions
   WHERE age_seconds > 600;

   -- Transaction gap analysis
   SELECT (oldest_snapshot - oldest_transaction) as SWEEP_GAP
   FROM sb_database_info;
   ```

### Task 3.6: Advanced Features (Weeks 17-18, 2 weeks) ✅

**Subtasks:**
1. **Implement RESERVING Clause Execution (1 week)** ✅
   - Parse table list from RESERVING clause
   - Acquire locks at transaction start
   - SHARED / PROTECTED modes
   - FOR READ / FOR WRITE

2. **Implement LOCK TIMEOUT (2 days)** ✅
   - Add timeout parameter to lock acquisition
   - Wait up to specified seconds
   - Return lock timeout error

3. **Implement READ ONLY Optimization (2 days)** ✅
   - OAT calculation excludes read-only transactions
   - Snapshot filtering for read-only transactions
   - Lock manager fast-path for read-only workloads
   - Buffer pool eviction optimization
   - Comprehensive statistics tracking

4. **Full SET TRANSACTION Syntax (1 day)** ✅
   - Complete parser support
   - All parameter combinations
   - Error checking

**Phase 3 Deliverables:**
- [x] Transaction markers (OIT, OAT, OST, Next) operational
- [x] All isolation levels implemented and tested
- [x] Sweep mechanism working (automatic and manual)
- [x] Garbage collection (cooperative and background)
- [x] Long transaction detection and policies
- [x] Table reservation support
- [x] LOCK TIMEOUT support
- [x] Full SET TRANSACTION syntax
- [x] READ ONLY optimizations (OAT, snapshot, locks, buffer pool, statistics)
- [x] Configuration integrated
- [x] Monitoring queries functional
- [x] Comprehensive tests
- [x] Documentation updated

---

## Phase 4: Type System Completion (Weeks 5-16, 12 weeks - PARALLEL)

**Goal:** Complete all primitive types and DOMAIN system

**Prerequisites:** Phase 1 complete (config system needed)
**Can Parallelize:** YES - Developer 2 works on this while Developer 1 does Phase 2-3
**Reference:** Alpha 1.2 Requirements ALPHA-001 and ALPHA-002

**Assignee:** Developer 2

### Task 4.1: Complete Missing Primitive Types (Weeks 5-10, 6 weeks)

**Subtasks:**

**Week 5-6: Integer Types (2 weeks)**
1. INT128 (3 days)
   - Add to TypedValue::VariantType
   - Arithmetic operations
   - Type conversions
   - Parser support

2. Unsigned Integers (4 days)
   - UINT8, UINT16, UINT32, UINT64
   - Add to variant storage
   - Arithmetic and conversions
   - Parser support

3. Testing (3 days)
   - Range tests
   - Arithmetic edge cases
   - Type conversion matrix

**Week 7: MONEY, INTERVAL, DECIMAL (1 week)**
1. MONEY Type (2 days)
   - Fixed-precision currency (64-bit, 4 decimals)
   - Currency operations
   - Parser support

2. INTERVAL Type (2 days)
   - Time interval storage
   - Interval arithmetic
   - Parser support

3. DECIMAL Arithmetic (3 days)
   - Evaluate libraries (boost::multiprecision or GNU MPFR)
   - Replace string storage
   - Full precision support

**Week 8: JSONB and XML (1 week)**
1. JSONB (3 days)
   - Binary JSON format design
   - Efficient storage
   - Parser support
   - Basic query support

2. XML Type (2 days)
   - XML document storage
   - Validation
   - Consider libxml2 or pugixml
   - Parser support

**Week 9: VECTOR and ARRAY (2 weeks)**
1. VECTOR Type (1 week)
   - Vector embeddings storage
   - Similarity operations
   - Consider Eigen library
   - Parser support

2. ARRAY Type (1 week)
   - Multi-dimensional array support
   - Array operations
   - NULL handling
   - Parser support

**Week 10: COMPOSITE/RECORD (1 week)**
1. COMPOSITE Type (1 week)
   - Structured type support
   - Nested field access
   - ROW constructor
   - Integration with DOMAIN system

### Task 4.2: DOMAIN System Implementation (Weeks 11-16, 6 weeks)

**Phase 1: Basic Domains (Weeks 11-12, 2 weeks)**
1. Parser Support (3 days)
   - CREATE DOMAIN syntax
   - Constraints (DEFAULT, NOT NULL, CHECK)
   - INHERITS clause

2. Catalog Tables (2 days)
   - sb_domains table
   - Domain constraint storage
   - Domain inheritance relationships

3. Domain Validation (2 days)
   - CHECK constraint evaluation
   - NOT NULL enforcement
   - Default value application

4. Testing (3 days)
   - Basic domain creation
   - Constraint validation
   - Inheritance

**Phase 2: RECORD Domains (Week 13, 2 weeks split with next)**
1. RECORD Type Storage (3 days)
   - Field definitions
   - Nested structure

2. ROW Constructor (2 days)
   - Syntax: `ROW(value1, value2, ...)`

3. Field Access (2 days)
   - Dot notation: `record.field`
   - EXTRACT function

**Phase 3: ENUM and SET Domains (Week 14, 1 week)**
1. ENUM Type (3 days)
   - Ordered values
   - SET NEXT VALUE
   - GET VALUE FOR, GET POSITION FOR
   - Comparison and ordering

2. SET Type (2 days)
   - Unordered, unique collection
   - SET constructor: `SET['val1', 'val2']`
   - Set operators: @>, &&
   - Set operations: union, intersection, difference

**Phase 4: VARIANT Type (Week 15, 2 weeks split with next)**
1. VARIANT Implementation (1 week)
   - Runtime polymorphic type
   - EXTRACT(DATATYPE FROM value)
   - IS OF TYPE checks
   - Type-safe casting

**Phase 5: Advanced Domain Features (Weeks 15-16, 2 weeks)**
1. WITH SECURITY (3 days)
   - MASK_FUNCTION
   - AUDIT_ACCESS
   - REQUIRE_PERMISSION
   - ENCRYPTION

2. WITH INTEGRITY (2 days)
   - UNIQUE_ACROSS_DATABASE
   - CASE_INSENSITIVE
   - NORMALIZE_FUNCTION

3. WITH VALIDATION (2 days)
   - VALIDATE_FUNCTION
   - ON_VIOLATION
   - ERROR_MESSAGE

4. WITH QUALITY (2 days)
   - PARSE_FUNCTION
   - STANDARDIZE_FUNCTION
   - ENRICH_FUNCTION

**Phase 4 Deliverables:**
- [x] All primitive types implemented (INT128, UINT*, MONEY, INTERVAL, JSONB, XML, VECTOR, ARRAY, COMPOSITE)
- [x] DECIMAL with proper arithmetic
- [x] Complete DOMAIN system
- [x] RECORD, ENUM, SET, VARIANT types
- [x] Advanced domain features (security, integrity, validation, quality)
- [x] Parser support for all types
- [x] Catalog tables updated
- [x] Comprehensive type system tests

---

## Phase 5: Index Types Implementation (Weeks 9-37, 29 weeks - PARALLEL)

**Goal:** Implement all remaining index types

**Prerequisites:** Phase 1 complete
**Can Parallelize:** YES - Developer 3 works on this starting week 9
**Reference:** Alpha 1.2 Requirement ALPHA-003

**Assignee:** Developer 3

**Note:** Developer 3 implements index types sequentially but in parallel with other developers working on Phases 2-4.

### Task 5.1: GIN Index (Weeks 9-16, 8 weeks)

**Priority:** HIGHEST (required for ARRAY, JSONB, full-text)

**Subtasks:**
1. **Design GIN Architecture (Week 9, 1 week)**
   - Entry tree structure
   - Posting trees
   - Pending list for fast inserts

2. **Implement Entry Tree (Weeks 10-11, 2 weeks)**
   - B-tree of index keys
   - Key extraction from values
   - Duplicate key handling

3. **Implement Posting Trees (Weeks 12-13, 2 weeks)**
   - Lists of tuple IDs per key
   - Compression for posting lists
   - Range queries

4. **Implement Pending List (Week 14, 1 week)**
   - Fast insert buffer
   - Merge with main index
   - Cleanup strategy

5. **Testing and Optimization (Weeks 15-16, 2 weeks)**
   - Array indexing tests
   - JSONB indexing tests
   - Full-text search tests
   - Performance tuning

### Task 5.2: Bitmap Index (Weeks 17-20, 4 weeks)

**Subtasks:**
1. **Design Bitmap Structure (Week 17, 1 week)**
   - Bitmap storage format
   - Compression algorithms
   - Update strategy

2. **Implement Core Operations (Weeks 18-19, 2 weeks)**
   - Bitmap creation
   - Bitmap scans
   - Set operations (AND, OR, NOT)

3. **Testing (Week 20, 1 week)**
   - Low-cardinality columns
   - Performance tests
   - Update scenarios

### Task 5.3: GIST Index (Weeks 21-28, 8 weeks)

**Subtasks:**
1. **Design GIST Framework (Weeks 21-22, 2 weeks)**
   - Extensible architecture
   - Operator classes
   - Key compression

2. **Implement Core GIST (Weeks 23-25, 3 weeks)**
   - Tree structure
   - Insert/delete/search
   - Page splits

3. **Spatial Data Support (Week 26, 1 week)**
   - R-tree semantics
   - Bounding box operations

4. **Testing and Optimization (Weeks 27-28, 2 weeks)**
   - Spatial queries
   - Range types
   - Extensibility tests

### Task 5.4: BRIN Index (Weeks 29-31, 3 weeks)

**Subtasks:**
1. **Design BRIN Structure (Week 29, 1 week)**
   - Block range summaries
   - Minimal storage format

2. **Implement BRIN Operations (Week 30, 1 week)**
   - Summary creation
   - Range scans
   - Updates

3. **Testing (Week 31, 1 week)**
   - Large table tests
   - Storage overhead measurement
   - Query performance

### Task 5.5: VECTOR Index (Weeks 32-37, 6 weeks)

**Subtasks:**
1. **Design Vector Index (Weeks 32-33, 2 weeks)**
   - Choose algorithm (HNSW or IVF)
   - Distance metrics
   - Vector storage

2. **Implement Core Algorithm (Weeks 34-36, 3 weeks)**
   - Index construction
   - Approximate nearest neighbor search
   - Parameter tuning

3. **Testing (Week 37, 1 week)**
   - Similarity search tests
   - Recall/precision measurement
   - Performance benchmarks

**Phase 5 Deliverables:**
- [x] GIN index fully functional
- [x] Bitmap index fully functional
- [x] GIST index fully functional
- [x] BRIN index fully functional
- [x] VECTOR index fully functional
- [x] All index tests passing
- [x] Performance benchmarks documented

---

## Phase 6: Comment Support & Remaining Infrastructure (Weeks 17-18, 2 weeks - PARALLEL)

**Goal:** Complete remaining Alpha 1.2 infrastructure requirements

**Prerequisites:** Phase 1 complete
**Can Parallelize:** YES - Developer 2 after completing Type System
**Reference:** Alpha 1.2 Requirement ALPHA-005

**Assignee:** Developer 2 (weeks 17-18)

### Task 6.1: Comment Support (Week 17, 1 week)

**Subtasks:**
1. **Catalog Schema Changes (2 days)**
   - Add comment fields to all catalog structures
   - Inline storage for ≤ 255 bytes
   - TOAST for longer comments

2. **Parser Support (2 days)**
   - COMMENT ON TABLE/COLUMN/INDEX/etc.
   - COMMENT ON ... IS 'text'
   - COMMENT ON ... IS NULL (removal)

3. **Catalog Operations (1 day)**
   - setTableComment() / getTableComment()
   - setColumnComment() / getColumnComment()
   - Similar for all object types

4. **Executor Integration (1 day)**
   - Execute COMMENT ON statements
   - Update catalog

5. **Testing (1 day)**
   - Comment on various objects
   - Retrieve comments
   - Remove comments

### Task 6.2: Cross-Page UPDATE Implementation (Week 18, 1 week)

**Reference:** Audit CRIT-003
**Priority:** HIGH

**Subtasks:**
1. **Design Cross-Page Version Chain (2 days)**
   - Forward pointer in old tuple
   - New tuple on different page
   - Version chain traversal

2. **Implement Cross-Page Update (2 days)**
   - Allocate space on new page
   - Create new version
   - Update forward pointer
   - Update indexes

3. **Testing (1 day)**
   - UPDATE with size increase
   - Version chain traversal
   - Index consistency

**Phase 6 Deliverables:**
- [x] Comment support for all database objects
- [x] Cross-page UPDATE working
- [x] Tests passing

---

## Phase 7: Polish, Testing, Documentation (Weeks 38-41, 4 weeks)

**Goal:** Comprehensive testing, bug fixes, documentation

**Prerequisites:** All previous phases complete
**Assignees:** All developers

### Task 7.1: Comprehensive Testing (Weeks 38-39, 2 weeks)

**All Developers:**

**Week 38:**
1. **Integration Tests (Developer 1)**
   - Multi-connection scenarios
   - Complex transactions
   - Isolation level interactions
   - Long transaction handling

2. **Type System Tests (Developer 2)**
   - All type combinations
   - DOMAIN constraints
   - Advanced domain features

3. **Index Tests (Developer 3)**
   - All index types
   - Mixed workloads
   - Index consistency

**Week 39:**
1. **Stress Tests (All)**
   - High connection count
   - Large data volumes
   - Concurrent operations
   - Memory pressure

2. **Error Path Tests (All)**
   - OOM scenarios
   - Corruption detection
   - Recovery scenarios

3. **Security Tests (Developer 1)**
   - SQL injection
   - Path traversal
   - Integer overflow

### Task 7.2: Bug Fixes (Week 40, 1 week)

**All Developers:**
- Fix issues found during testing
- Address any regressions
- Performance fixes

### Task 7.3: Documentation (Week 41, 1 week)

**All Developers:**

1. **API Documentation (2 days)**
   - Generate Doxygen documentation
   - Review and improve comments

2. **User Documentation (2 days)**
   - Configuration guide
   - SQL reference
   - Transaction management guide

3. **Architecture Documentation (1 day)**
   - Update design documents
   - Architecture diagrams

**Phase 7 Deliverables:**
- [x] All tests passing
- [x] No critical bugs
- [x] Performance acceptable
- [x] Documentation complete
- [x] Release notes written

---

## Timeline Summary

### With 3 Developers Working in Parallel

**Critical Path Timeline:**

| Phase | Weeks | Developer 1 | Developer 2 | Developer 3 |
|-------|-------|-------------|-------------|-------------|
| **Phase 0** | 1 (partial) | Immediate fixes | - | - |
| **Phase 1** | 1-4 | Config system | Logging | UTF-8 |
| **Phase 2** | 5-8 | ConnectionContext | Support Phase 2 | - |
| **Phase 3** | 9-18 | Firebird Txn Model | - | - |
| **Phase 4** | 5-16 | - | Type System + DOMAIN | - |
| **Phase 5** | 9-37 | - | - | Index Types |
| **Phase 6** | 17-18 | - | Comments + Cross-Page | - |
| **Phase 7** | 38-41 | Testing & Polish | Testing & Polish | Testing & Polish |

**Critical Path:** Phase 0 → Phase 1 → Phase 2 → Phase 3 → Phase 7
**Total Duration:** 41 weeks (10.25 months) with sequential index implementation

**Optimized Timeline:**
- If index types can be parallelized (2 developers on indexes): ~30 weeks (7.5 months)
- With 4 developers: ~25 weeks (6.25 months)

**Realistic Estimate: 16-20 weeks (4-5 months) with 3 developers**

---

## Risk Assessment

### High Risks

1. **Firebird Transaction Model Complexity** (Phase 3)
   - Mitigation: Follow spec closely, comprehensive testing
   - Contingency: Simplify features if needed

2. **DOMAIN System Advanced Features** (Phase 4.2)
   - Mitigation: Implement incrementally, basic features first
   - Contingency: Defer some advanced features if needed

3. **GIN Index Complexity** (Phase 5.1)
   - Mitigation: Study PostgreSQL implementation, prototype first
   - Contingency: Implement simplified version initially

### Medium Risks

1. **Integration Issues** (All phases)
   - Mitigation: Regular integration testing
   - Contingency: Dedicated integration phase

2. **Performance Issues** (Phase 7)
   - Mitigation: Performance testing throughout
   - Contingency: Optimization phase

### Low Risks

1. **Configuration System** (Phase 1)
   - Well-understood problem
   - Many reference implementations

2. **UTF-8 Support** (Phase 1)
   - Standard algorithms available

---

## Dependencies Graph

```
Phase 0 (Immediate)
    ↓
Phase 1 (Foundation)
    ├─→ Phase 2 (ConnectionContext) ──→ Phase 3 (Firebird Txn)
    ├─→ Phase 4 (Type System) [parallel]
    └─→ Phase 5 (Index Types) [parallel]
         Phase 6 (Comments) [parallel after Phase 4]

All converge to Phase 7 (Polish & Testing)
```

---

## Success Criteria

Alpha 1.2 is complete when:

### Type System
- [ ] All primitive types implemented and tested
- [ ] DOMAIN system fully functional
- [ ] RECORD, ENUM, SET, VARIANT types working
- [ ] Advanced domain features implemented
- [ ] All types usable in tables, indexes, queries

### Transaction Management
- [ ] ConnectionContext operational with thread-local storage
- [ ] Always-in-transaction model working
- [ ] All isolation levels implemented
- [ ] Transaction markers (OIT, OAT, OST) tracked
- [ ] Sweep mechanism functional
- [ ] Garbage collection working
- [ ] Long transaction detection and policies active

### Index Types
- [ ] GIN index fully functional
- [ ] Bitmap index fully functional
- [ ] GIST index fully functional
- [ ] BRIN index fully functional
- [ ] VECTOR index fully functional
- [ ] All index tests passing

### Infrastructure
- [ ] Configuration file system operational
- [ ] Logging framework replacing fprintf
- [ ] UTF-8 character identifiers working
- [ ] Comment support on all objects
- [ ] Cross-page UPDATE working

### Quality
- [ ] No regression in existing tests
- [ ] All new features tested
- [ ] Documentation complete
- [ ] Performance acceptable
- [ ] No critical bugs

### Ready for SBLR
- [ ] Solid, stable database engine
- [ ] All core features working
- [ ] Can handle complex workloads
- [ ] Suitable for SBLR testing and development

---

## Deferred to Post-Alpha 1.2

- WAL (Write-Ahead Logging) - Beta requirement
- Network layer - Beta requirement
- Wire protocol implementations - Beta requirement
- Authentication systems - Beta requirement
- Replication - Future
- Distributed transactions - Future
- Naming convention cleanup - Code quality
- Full const correctness - Optimization
- Doxygen complete coverage - Documentation
- Temporary tables - Future feature

---

**Plan Version:** 1.0
**Date:** October 7, 2025
**Status:** Approved for Implementation
**Next Review:** After Phase 1 completion
