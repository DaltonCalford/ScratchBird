# Consolidated Findings (Canonical)

This document consolidates findings formerly stored under ScratchBird/docs/findings; originals are archived in ScratchBird/docs/archive/2026-01-09/findings.
Source of truth for implementation status is code under ScratchBird/; items without code references are marked as needing audit.

Last updated: 2026-01-09

Status tags: [IMPLEMENTED], [PARTIAL], [MISSING], [DOC-MISMATCH], [NEEDS-AUDIT]

Top gap triage (from current audit):
- [HIGH] DROP SCHEMA/DATABASE CASCADE/RESTRICT semantics missing; dropSchema is RESTRICT-only and dropDatabase routes to it (ScratchBird/src/core/catalog_manager.cpp:4765, ScratchBird/src/sblr/executor.cpp:7248).
- [MEDIUM] MySQL ALTER DATABASE rejected (ScratchBird/src/parser/mysql/mysql_parser.cpp:2524).
- [MEDIUM] Firebird ALTER DATABASE options rejected (ScratchBird/src/parser/firebird/firebird_parser.cpp:1845).

## ScratchBird/docs/archive/2026-01-09/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md

Code truth notes:
- Extended opcodes for CREATE/DROP/ALTER SCHEMA/DATABASE exist (ScratchBird/include/scratchbird/sblr/opcodes.h:1272).
- Bytecode generator emits schema/database opcodes (ScratchBird/src/sblr/bytecode_generator_v2.cpp:840, 1022, 1078, 1108).
- Executor handlers exist for schema/database DDL (ScratchBird/src/sblr/executor.cpp:6183, 6270, 6324, 6404, 7248, 7342).
- PostgreSQL parser emits EXT_CREATE/EXT_DROP/EXT_ALTER for schema/database (ScratchBird/src/parser/postgresql/pg_parser_ddl.cpp:1105, 1151, 2433, 2333, 2359).
- MySQL parser emits EXT_CREATE/EXT_DROP for DATABASE/SCHEMA synonyms (ScratchBird/src/parser/mysql/mysql_parser.cpp:3447, 2663).
- MySQL ALTER DATABASE is still rejected (ScratchBird/src/parser/mysql/mysql_parser.cpp:2524).
- Firebird ALTER DATABASE options are rejected (ScratchBird/src/parser/firebird/firebird_parser.cpp:1845).
- DROP SCHEMA cascade/restrict is RESTRICT-only (ScratchBird/src/core/catalog_manager.cpp:4765).
- DROP DATABASE force/cascade is ignored for ScratchBird protocol because it routes to dropSchema (ScratchBird/src/sblr/executor.cpp:7248).
- Emulated database view generation is wired into CREATE/DROP DATABASE for non-ScratchBird protocols (ScratchBird/src/sblr/executor.cpp:6549, 7310).
- Emulated database catalog CRUD exists (ScratchBird/src/core/catalog_manager.cpp:29628, 29961).

Issues (from source):
### CRITICAL FINDING: Schema/Database DDL Opcode Gap
- **Status:** SPECIFICATION GAP IDENTIFIED
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Current Implementation Status > SBLR Opcodes - MISSING
- **Verdict:** [FAIL] NO DDL OPCODES FOR SCHEMA/DATABASE MANAGEMENT
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Parser Implementation Status > PostgreSQL Parser (`src/parser/postgresql/pg_parser_ddl.cpp`) > CREATE DATABASE (Lines 943-974)
- [FAIL] Uses `EXT_SHOW_DATABASE` (a query opcode) as placeholder
- [FAIL] Comment says "Use a placeholder" - indicates awareness this is wrong
- [FAIL] Parses WITH options but doesn't emit them
- [FAIL] No implementation in executor to handle this
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Parser Implementation Status > PostgreSQL Parser (`src/parser/postgresql/pg_parser_ddl.cpp`) > CREATE SCHEMA (Lines 976-995)
- [FAIL] Does NOT emit any opcode at all
- [FAIL] Only emits the schema name string
- [FAIL] Parses IF NOT EXISTS but doesn't emit the flag
- [FAIL] Parses AUTHORIZATION but doesn't emit it
- [FAIL] Bytecode stream will be corrupt (string with no opcode)
- **Verdict:** BROKEN - WILL PRODUCE INVALID BYTECODE
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Parser Implementation Status > PostgreSQL Parser (`src/parser/postgresql/pg_parser_ddl.cpp`) > DROP DATABASE/DROP SCHEMA
- [FAIL] NOT IMPLEMENTED AT ALL
- **Verdict:** MISSING
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Parser Implementation Status > MySQL Parser (`src/parser/mysql/mysql_parser.cpp`) > CREATE DATABASE (Lines 2314-2316)
- **Verdict:** [FAIL] STUB - NOT IMPLEMENTED
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Parser Implementation Status > MySQL Parser (`src/parser/mysql/mysql_parser.cpp`) > DROP DATABASE
- [FAIL] NOT FOUND
- **Verdict:** MISSING
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Parser Implementation Status > Firebird Parser (`src/parser/firebird/firebird_parser.cpp`) > CREATE DATABASE
- [FAIL] NOT FOUND
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Parser Implementation Status > Firebird Parser (`src/parser/firebird/firebird_parser.cpp`) > DROP DATABASE
- [FAIL] NOT FOUND
- **Verdict:** MISSING
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Catalog Manager Status > CREATE/DROP Schema Support
- **Issue:**
- [FAIL] NO SBLR OPCODE to call these from executor
- [FAIL] NO EXECUTOR HANDLER to process schema DDL
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Executor Status
- **Verdict:** [FAIL] NO EXECUTOR HANDLERS FOR SCHEMA/DATABASE DDL
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Impact Analysis > Blocking Issues
- All PostgreSQL clients will fail on CREATE DATABASE
- If schema management broken, domain management also broken
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Required Components
- To fix this gap, we need:
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Required Components > 3. Parser Implementations
- [OK] Fix parseCreateDatabase() to emit EXT_CREATE_DATABASE
- [OK] Fix parseCreateSchema() to emit EXT_CREATE_SCHEMA
- [FAIL] Add parseDropDatabase() emit EXT_DROP_DATABASE
- [FAIL] Add parseDropSchema() emit EXT_DROP_SCHEMA
- [FAIL] Add parseAlterDatabase() emit EXT_ALTER_DATABASE
- [FAIL] Add parseAlterSchema() emit EXT_ALTER_SCHEMA
- [FAIL] Implement parseCreateDatabase() emit EXT_CREATE_DATABASE
- [FAIL] Implement parseDropDatabase() emit EXT_DROP_DATABASE
- [FAIL] Implement parseCreateSchema() emit EXT_CREATE_SCHEMA (synonym)
- [FAIL] Implement parseDropSchema() emit EXT_DROP_SCHEMA (synonym)
- [FAIL] Implement parseAlterDatabase() emit EXT_ALTER_DATABASE
- [FAIL] Implement parseCreateDatabase() emit EXT_CREATE_DATABASE
- [FAIL] Implement parseDropDatabase() emit EXT_DROP_DATABASE
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Required Components > 4. Catalog Manager Extensions
- [WARN] `dropSchema()` - Verify CASCADE semantics
- [FAIL] `alterSchema()` - Need to add
- [FAIL] `createEmulatedDatabase()` - New: wrapper that creates schema + views
- [FAIL] `dropEmulatedDatabase()` - New: wrapper that prunes tree + views
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Recommendations > Option 2: Include in Plan 03B
- **Issue:** Plan 03B already large (35 tasks, 210 hours)
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Recommendations > Option 3: Quick Stub for Plan 04 Testing
- **Issue:** Violates NO STUBS rule
- **Issue:** Doesn't solve emulation problem
### CRITICAL FINDING: Schema/Database DDL Opcode Gap > Next Steps (Pending User Approval)
- Fix all three parsers

## ScratchBird/docs/archive/2026-01-09/findings/DATETIME_UUID_STORAGE_FORMAT.md

Code truth notes:
- Findings doc updated to match TypedValue serialization (DATE=MJD+offset, TIME/TIMESTAMP=micros+offset, UUID=16 bytes).

Issues (from source):
### DateTime and UUID Storage Format in ScratchBird > References
- **Status:** [OK] Complete and Verified

## ScratchBird/docs/archive/2026-01-09/findings/DEADLOCK_FIX_2025_12_30.md

Code truth notes:
- dropFunction/dropProcedure acquire psql_mutex_ + dependency_cache_mutex_ with internal helpers (ScratchBird/src/core/catalog_manager.cpp:12232, 12301).
- Internal dependency helpers exist (ScratchBird/src/core/catalog_manager.cpp:18491, 18684).
- Runtime tests not executed in this pass.

Issues (from source):
### Deadlock Fix - 2025-12-30
- **Status:** [OK] RESOLVED
- **Fix Duration:** ~3 hours
### Deadlock Fix - 2025-12-30 > The Fix > Changes Made > 4. Fixed dropProcedure() (catalog_manager.cpp:11643)
- Applied identical fix pattern as dropFunction().
### Deadlock Fix - 2025-12-30 > The Fix > Changes Made > 5. Fixed dropTable() (catalog_manager.cpp:11787)
- Applied same fix:
- Added `Status status;` declaration (was previously returned by getDependents())
### Deadlock Fix - 2025-12-30 > Trade-offs > Cons
- [WARN] Holds both locks longer (increased lock contention)
- [WARN] More code (internal helper versions)
- [WARN] Must maintain two versions of several functions
### Deadlock Fix - 2025-12-30 > Next Steps
- [OK] **Fix verified** - All 4 tests pass
- **Status:** [OK] RESOLVED
- **Fix Verified:** 2025-12-30
- **Fix Approach:** Option A (Internal helpers with consistent lock ordering)

## ScratchBird/docs/archive/2026-01-09/findings/DEDICATED_ISQL_CLIENTS_REQUIREMENT.md

Code truth notes:
- sb_fb_isql exists but is an embedded Firebird SQL CLI, not a wire-protocol client (ScratchBird/src/cli/sb_fb_isql.cpp).
- sb_pg_isql and sb_my_isql are missing from ScratchBird/src/cli.
- libsb_isql_common is missing (src/cli/isql_common/ not present).

Issues (from source):
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing
- **Status:** [WARN] CRITICAL REQUIREMENT FOR COMPATIBILITY TESTING
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Executive Summary
- | `sb_fb_isql` | Firebird XDR | 3050 | [FAIL] NEEDED - Firebird emulation testing |
- | `sb_my_isql` | MySQL Text Protocol | 3306 | [FAIL] NEEDED - MySQL emulation testing |
- | `sb_pg_isql` | PostgreSQL Frontend/Backend | 5432 | [FAIL] NEEDED - PostgreSQL emulation testing |
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Why Separate ISQL Clients Are Required > Reason 1: Wire Protocol Incompatibility
- OK, ERROR, EOF packets
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Why Separate ISQL Clients Are Required > Reason 3: Command-Line Flag Compatibility
- | Username | `-u <user>` | `-U <user>` | `-u <user>` | `-U <user>` [WARN] |
- | Password | `-p <pass>` | `-W` (prompt) | `-p[pass]` | `-P <pass>` [WARN] |
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Detailed Requirements for Each ISQL Client > 1. sb_isql (ScratchBird Native)
- **Status:** [OK] ALREADY EXISTS
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Detailed Requirements for Each ISQL Client > 2. sb_fb_isql (Firebird Emulation Client)
- **Status:** [FAIL] DOES NOT EXIST - MUST BE CREATED
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Detailed Requirements for Each ISQL Client > 2. sb_fb_isql (Firebird Emulation Client) > SET Commands (Firebird ISQL)
- `SET BAIL [ON|OFF];` - Stop on first error
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Detailed Requirements for Each ISQL Client > 2. sb_fb_isql (Firebird Emulation Client) > Behavior Requirements
- Return Firebird-style error codes
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Detailed Requirements for Each ISQL Client > 3. sb_pg_isql (PostgreSQL Emulation Client)
- **Status:** [FAIL] DOES NOT EXIST - MUST BE CREATED
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Detailed Requirements for Each ISQL Client > 4. sb_my_isql (MySQL Emulation Client)
- **Status:** [FAIL] DOES NOT EXIST - MUST BE CREATED
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Detailed Requirements for Each ISQL Client > 4. sb_my_isql (MySQL Emulation Client) > Wire Protocol Implementation
- Parse OK, ERROR, EOF, Result Set packets
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Detailed Requirements for Each ISQL Client > 4. sb_my_isql (MySQL Emulation Client) > Behavior Requirements
- Return MySQL-style error numbers (1064, etc.)
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Implementation Implications > Server-Side Requirements
- [WARN] Firebird XDR (port 3050) - STATUS UNKNOWN
- [WARN] PostgreSQL FE/BE (port 5432) - STATUS UNKNOWN
- [WARN] MySQL Text (port 3306) - STATUS UNKNOWN
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Implementation Implications > Client-Side Requirements
- Error handling
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Testing Impact > Without Dedicated ISQL Clients
- [FAIL] Run Firebird test suite tests (they expect `isql -i`)
- [FAIL] Run PostgreSQL test suite tests (they expect `psql -f`)
- [FAIL] Run MySQL test suite tests (they expect `mysql <`)
- [FAIL] Verify wire protocol emulation is working
- [FAIL] Test that Firebird clients can connect to ScratchBird
- [FAIL] Validate parser compatibility with real-world SQL
- [WARN] Manually convert test scripts to ScratchBird format (time-consuming, error-prone)
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Dependencies and Blockers > Server-Side Dependencies
- **Wire Protocol Listeners Status**
- Firebird XDR listener on port 3050 - EXISTS?
- PostgreSQL FE/BE listener on port 5432 - EXISTS?
- MySQL Text listener on port 3306 - EXISTS?
- Connection on 3050 routes to Firebird parser?
- Connection on 5432 routes to PostgreSQL parser?
- Connection on 3306 routes to MySQL parser?
- Can create schemas under `/remote/emulated/firebird/`?
- Can create schemas under `/remote/emulated/postgresql/`?
- Can create schemas under `/remote/emulated/mysql/`?
- **If ANY of these are not implemented, they are BLOCKERS for the ISQL clients.**
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Recommendations > Alternative Approach: Use Existing Native Clients
- **Missing Libraries:** Users may not have Firebird/MySQL/PostgreSQL clients installed
### Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing > Conclusion
- **Document Status:** [OK] Complete - Ready for Planning Phase
- **Review Required:** Server-side wire protocol status verification

## ScratchBird/docs/archive/2026-01-09/findings/DROPTABLE_DEADLOCK_ANALYSIS_2025_12_30.md

Code truth notes:
- dropSequence uses scoped_lock(sequence_cache_mutex_, dependency_cache_mutex_) and internal helpers (ScratchBird/src/core/catalog_manager.cpp:16242).
- dropTable uses a multi-lock scoped_lock and internal dependency helpers (ScratchBird/src/core/catalog_manager.cpp:12442).
- Runtime tests not executed in this pass.

Issues (from source):
### dropTable Deadlock Analysis - 2025-12-30
- **Status:** CRITICAL DEADLOCK - Multiple Same-Thread Lock Re-Acquisition Issues
### dropTable Deadlock Analysis - 2025-12-30 > Executive Summary
- The deadlock fix implemented earlier for `dropFunction()` and `dropProcedure()` DID work for those functions. However, `dropTable()` has a DIFFERENT deadlock pattern that was not addressed.
### dropTable Deadlock Analysis - 2025-12-30 > Why The Previous Fix Didn't Cover This
- The deadlock fix for `dropFunction()` and `dropProcedure()` (implemented in DEADLOCK_FIX_2025_12_30.md) correctly:
- The fix was **incomplete** because:
### dropTable Deadlock Analysis - 2025-12-30 > Additional Issues Found > Issue: dropSequence() Has Same Pattern as dropFunction()
- **This means `dropSequence()` itself needs the SAME fix that was applied to `dropFunction()`:**
### dropTable Deadlock Analysis - 2025-12-30 > Lock Ordering Rules (Critical)
- `dropSequence()` acquires `sequence_cache_mutex_` first, but then calls functions that acquire `dependency_cache_mutex_` [FAIL] (wrong - should acquire both upfront)
### dropTable Deadlock Analysis - 2025-12-30 > Estimated Complexity
- **dropTable Fix Complexity:** MEDIUM-HIGH
- Need to fix `dropSequence()` standalone (medium)
### dropTable Deadlock Analysis - 2025-12-30 > Priority
- **Must Fix:** YES - before any further development
- **Status:** DOCUMENTED - NO FIX ATTEMPTED PER USER REQUEST

## ScratchBird/docs/archive/2026-01-09/findings/EXECUTOR_TRANSACTION_TIMEOUT_ANALYSIS_2025_12_30.md

Code truth notes:
- Autocommit commit-after-statement exists in executor (ScratchBird/src/sblr/executor.cpp:2550).
- createTable now uses scoped_lock and internal dependency APIs (ScratchBird/src/core/catalog_manager.cpp:5038).
- Runtime tests not executed in this pass.

Issues (from source):
### ExecutorTransactionPayloadTest Timeout Analysis - 2025-12-30
- **Status:** ANALYSIS INCOMPLETE - Suspected CREATE TABLE or Autocommit Issue
### ExecutorTransactionPayloadTest Timeout Analysis - 2025-12-30 > Hypotheses > Hypothesis 2: Autocommit Triggers Implicit COMMIT (More Likely)
- The issue might not be autocommit-specific, but rather something wrong with CREATE TABLE completion
### ExecutorTransactionPayloadTest Timeout Analysis - 2025-12-30 > Hypotheses > Hypothesis 3: Background Thread Deadlock (Most Likely)
- **Why both AutocommitOn and AutocommitOff fail:**
### ExecutorTransactionPayloadTest Timeout Analysis - 2025-12-30 > Investigation Steps Needed > Step 4: Reproduce with Minimal Test
- If it hangs even without autocommit settings, confirms issue is in CREATE TABLE, not autocommit.
### ExecutorTransactionPayloadTest Timeout Analysis - 2025-12-30 > Potential Fixes (NOT IMPLEMENTED - DOCUMENTATION ONLY) > Fix Option 2: Disable Background Threads During Tests
- **Pros:** Quick fix for testing
### ExecutorTransactionPayloadTest Timeout Analysis - 2025-12-30 > Next Steps
- **Apply similar fix to createTable:** Use `std::scoped_lock` and internal helper versions
### ExecutorTransactionPayloadTest Timeout Analysis - 2025-12-30 > Priority
- **Must Fix:** YES - after dropTable fixes are complete
- **Status:** DOCUMENTED - NO FIX ATTEMPTED PER USER REQUEST

## ScratchBird/docs/archive/2026-01-09/findings/FINAL_TEST_RESULTS_2025_12_31.md

Code truth notes:
- CTest TIMEOUT is configured for gtest discovery (ScratchBird/tests/CMakeLists.txt:321), but runtime enforcement needs verification.
- Firebird parser already accepts COMMIT RETAIN and ROLLBACK RETAIN (ScratchBird/src/parser/firebird/firebird_parser.cpp:2803).
- LongTransactionMonitor can error if ProcArray is not initialized; monitor starts on DB open, ProcArray initializes on connect (ScratchBird/src/core/long_transaction_monitor.cpp:262, ScratchBird/src/core/proc_array.cpp:690, ScratchBird/src/core/database.cpp:1007).

Issues (from source):
### Final Test Suite Results - 2025-12-31
- **Status:** MOSTLY SUCCESSFUL - 1 test exceeds 5 minutes (timeout at 25 minutes)
### Final Test Suite Results - 2025-12-31 > Executive Summary
- After fixing the monolithic test issue, the full test suite ran with the following results:
### Final Test Suite Results - 2025-12-31 > CRITICAL: Tests Exceeding 5 Minutes > Test #1230: TableDependencyTest.DropTableFailsIfParentFK
- **Status:** **FAILURE**
- **Issue:**
### Final Test Suite Results - 2025-12-31 > [FAIL] Failed Tests (Non-Timeout) > 1. FirebirdParserTest.ParseCommitRetain
- **Status:** Failed (not timeout-related)
### Final Test Suite Results - 2025-12-31 > [FAIL] Failed Tests (Non-Timeout) > 2. FirebirdParserTest.ParseRollbackRetain
- **Status:** Failed (not timeout-related)
### Final Test Suite Results - 2025-12-31 > [FAIL] Failed Tests (Non-Timeout) > 3. GiSTMVCC
- **Status:** Failed (not timeout-related)
- **Error:** "Failed to get active backends for long transaction check"
### Final Test Suite Results - 2025-12-31 > [OK] Slowest Passing Tests
- | Rank | Test Name | Runtime | Type | Status |
### Final Test Suite Results - 2025-12-31 > Test Categorization by Execution Time
- | Category | Count | Percentage | Status |
### Final Test Suite Results - 2025-12-31 > Comparison: Before vs After Fix > Improvement
- [FAIL] **New issue discovered:** Test #1230 times out at 25 minutes
### Final Test Suite Results - 2025-12-31 > Root Cause Analysis: Test #1230 Timeout > Test Details
- **Purpose:** Verify that dropping a parent table fails when child tables have foreign keys
- **Expected Behavior:** Test should fail with foreign key constraint error
### Final Test Suite Results - 2025-12-31 > Action Items > Immediate (Today)
- [OK] **COMPLETED:** Fix monolithic test duplication
- **HIGH:** Fix timeout enforcement mechanism
- **MEDIUM:** Fix 3 other failing tests (Firebird parser, GiSTMVCC)
### Final Test Suite Results - 2025-12-31 > Action Items > Short-Term (This Week)
- Fix Test #1230 deadlock/hang issue
- Fix remaining 3 failed tests
### Final Test Suite Results - 2025-12-31 > Detailed Test Failures > Test #417: FirebirdParserTest.ParseCommitRetain
- **Status:** Failed
- **Likely Issue:** Parser not recognizing `COMMIT RETAIN` syntax
- **Fix Needed:** Update Firebird parser grammar to support RETAIN clause
### Final Test Suite Results - 2025-12-31 > Detailed Test Failures > Test #419: FirebirdParserTest.ParseRollbackRetain
- **Status:** Failed
- **Likely Issue:** Parser not recognizing `ROLLBACK RETAIN` syntax
- **Fix Needed:** Update Firebird parser grammar to support RETAIN clause
### Final Test Suite Results - 2025-12-31 > Detailed Test Failures > Test #1230: TableDependencyTest.DropTableFailsIfParentFK
- **Status:** Timeout (25 minutes)
- **Issue:** Test hangs indefinitely when testing foreign key constraint validation
- **Fix Needed:** Debug deadlock in foreign key dependency checking
### Final Test Suite Results - 2025-12-31 > Detailed Test Failures > Test #1448: GiSTMVCC
- **Status:** Failed
- **Error Message:**
- **Issue:** Long transaction monitor cannot access backend process information
- **Fix Needed:** Either fix backend tracking in test environment or disable monitor for tests
### Final Test Suite Results - 2025-12-31 > Success Metrics > Remaining Issues [FAIL]
- [FAIL] 1 test exceeds 5 minutes (Test #1230 - 25 minute timeout)
- [FAIL] Timeout enforcement not working correctly
- [FAIL] 3 tests failing (2 parser tests, 1 integration test)
### Final Test Suite Results - 2025-12-31 > Test Suite Health Grade
- **Overall Grade: B+ (Good, with critical issue)**
### Final Test Suite Results - 2025-12-31 > Recommendations > Priority 1: Fix Test #1230 (CRITICAL)
- Apply similar fix to previous deadlock issues (lock ordering)
- Verify fix with multiple test runs
### Final Test Suite Results - 2025-12-31 > Recommendations > Priority 2: Fix Timeout Enforcement
- **Current Issue:** 60-second timeout not being enforced (test ran 25 minutes)
### Final Test Suite Results - 2025-12-31 > Recommendations > Priority 3: Fix Remaining 3 Tests
- **Low priority** - These tests fail quickly and don't block testing:
### Final Test Suite Results - 2025-12-31 > References
- **Test Suite Fix:** `/docs/archive/2026-01-09/findings/TEST_SUITE_FIX_2025_12_31.md`
- **Status:** DOCUMENTED - CRITICAL ACTION REQUIRED
- **Next Step:** Investigate and fix Test #1230 timeout

## ScratchBird/docs/archive/2026-01-09/findings/FINDINGS_REMEDIATION_MAPPING.md

Code truth notes:
- PLAN_18 and PLAN_19 are referenced but no plan files exist under ScratchBird/docs/planning or ScratchBird/docs/archive.
- Mapping claims all findings are mapped, but only 7 findings are listed; many others are not in the table.

Issues (from source):
### Mapping
- DEADLOCK_FIX_2025_12_30.md -> PLAN_19 / GC-B4
- FK_DEADLOCK_FIX_2025_12_31.md -> PLAN_19 / GC-B2
- EXECUTOR_TRANSACTION_TIMEOUT_ANALYSIS_2025_12_30.md -> PLAN_18 / OPT-A1
- TEST_TIMEOUT_ANALYSIS_2025_12_29.md -> PLAN_18 / PLAN_19 / OPT-A8 / GC-B6
- DROPTABLE_DEADLOCK_ANALYSIS_2025_12_30.md -> PLAN_19 / GC-B2
- FINAL_TEST_RESULTS_2025_12_31.md -> PLAN_18 / PLAN_19 / All
- SESSION_SUMMARY_2025_12_31.md -> PLAN_18 / PLAN_19 / Tracking
### Notes
- No findings remain unmapped.
- Security-sensitive findings always map to PLAN_18.
- Storage health findings always map to PLAN_19.

## ScratchBird/docs/archive/2026-01-09/findings/FK_DEADLOCK_FIX_2025_12_31.md

Code truth notes:
- createForeignKey acquires full lock set in dropTable order (mutex_, sequence_cache_mutex_, trigger_mutex_, foreign_keys_cache_mutex_, constraints_cache_mutex_, dependency_cache_mutex_) (ScratchBird/src/core/catalog_manager.cpp:27723).
- createForeignKey uses createDependencyInternal/deleteDependencyInternal to avoid nested lock acquisition (ScratchBird/src/core/catalog_manager.cpp:27789, 27808, 27887).
- getObjectNameInternal handles ObjectType::CONSTRAINT via foreign_keys_cache_ then constraints_cache_ with caller-held mutexes, avoiding public getConstraint() (ScratchBird/src/core/catalog_manager.cpp:19433).
- Runtime test verification not executed in this pass.

Issues (from source):
### Foreign Key Deadlock Fix - 2025-12-31
- **Status:** [OK] FIXED - Lock ordering corrected in createForeignKey
- **Issue:** Test timeout at 1,500 seconds (25 minutes) due to deadlock
### Foreign Key Deadlock Fix - 2025-12-31 > Problem Summary > Failing Test
- Try to drop parent table (should fail with CONSTRAINT_VIOLATION)
### Foreign Key Deadlock Fix - 2025-12-31 > Pattern: Internal vs Public Functions
- This fix follows the established pattern in ScratchBird for lock management:
### Foreign Key Deadlock Fix - 2025-12-31 > Verification > Test That Should Pass
- **Before Fix:**
- **After Fix (Expected):**
- Returns `Status::CONSTRAINT_VIOLATION` as expected
- Error message mentions dependencies
### Foreign Key Deadlock Fix - 2025-12-31 > Similar Fixes Applied Previously
- This fix follows the same pattern as previous deadlock fixes:
### Foreign Key Deadlock Fix - 2025-12-31 > Similar Fixes Applied Previously > 1. dropFunction Deadlock Fix (2025-12-30)
- **Issue:** `dropFunction()` deadlock in `StoredCodeDependencyTest`
- **Fix:** Acquire all locks upfront, use internal versions
### Foreign Key Deadlock Fix - 2025-12-31 > Similar Fixes Applied Previously > 2. dropProcedure Deadlock Fix (2025-12-30)
- **Issue:** `dropProcedure()` deadlock in `StoredCodeDependencyTest`
- **Fix:** Acquire all locks upfront, use internal versions
### Foreign Key Deadlock Fix - 2025-12-31 > Similar Fixes Applied Previously > 3. dropTable Enhancement (Already Fixed)
- **Status:** `dropTable()` already uses proper lock ordering
### Foreign Key Deadlock Fix - 2025-12-31 > Similar Fixes Applied Previously > 4. createForeignKey Deadlock Fix (2025-12-31) - THIS FIX
- **Issue:** `createForeignKey()` nested lock acquisition
- **Fix:** Acquire both locks upfront, use internal versions
### Foreign Key Deadlock Fix - 2025-12-31 > Impact > Functions Fixed
- `CatalogManager::createForeignKey()` - primary fix
### Foreign Key Deadlock Fix - 2025-12-31 > Testing Status
- **Status:** [OK] **FIXED AND VERIFIED**
- The initial fix to `createForeignKey()` was necessary but INSUFFICIENT. The real deadlock was in `getObjectNameInternal()`:
- [OK] Test correctly returns `Status::CONSTRAINT_VIOLATION` with dependency error message
### Foreign Key Deadlock Fix - 2025-12-31 > References
- **Status:** [OK] **FIXED AND VERIFIED** - Test passes in 0.02 seconds

## ScratchBird/docs/archive/2026-01-09/findings/SB_ISQL_COMMAND_LINE_ANALYSIS.md

Issues (from source):
### sb_isql Command-Line Analysis and Enhancement Recommendations > Executive Summary
- **Current Status:**
- [WARN] **Input redirection** (`-i`) is NOT SUPPORTED (uses `-f` instead)
- [FAIL] **Parser selection** (`-par`/`--parser`) is NOT SUPPORTED
### sb_isql Command-Line Analysis and Enhancement Recommendations > Current Command-Line Interface > Supported Flags (24 options)
- | Flag | Long Form | Description | Status |
- | `-b` | `--bail` | Stop on first error | [OK] Implemented |
### sb_isql Command-Line Analysis and Enhancement Recommendations > Current Command-Line Interface > Meta-Commands (Interactive Mode)
- | Meta-Command | Description | Status |
### sb_isql Command-Line Analysis and Enhancement Recommendations > Current Command-Line Interface > SET Commands (Firebird ISQL Compatible)
- | SET Command | Description | Status |
- | `SET BAIL [ON\|OFF]` | Stop on first error | [OK] Implemented |
### sb_isql Command-Line Analysis and Enhancement Recommendations > Current Support for Testing Requirements > [OK] Output Redirection Support
- **Status:** [OK] **FULLY FUNCTIONAL** - Ready for test automation
### sb_isql Command-Line Analysis and Enhancement Recommendations > Current Support for Testing Requirements > [OK] Input File Support
- Supports `--bail` to stop on first error
- **Status:** [OK] **FULLY FUNCTIONAL** - Ready for test automation
### sb_isql Command-Line Analysis and Enhancement Recommendations > Current Support for Testing Requirements > [WARN] Alternative Input Flag (`-i`)
- **Current Status:** [FAIL] NOT SUPPORTED
### sb_isql Command-Line Analysis and Enhancement Recommendations > Summary of Findings > Currently Supported [OK]
- | Feature | Flag | Status | Notes |
### sb_isql Command-Line Analysis and Enhancement Recommendations > Summary of Findings > Not Currently Supported [FAIL]
- | Feature | Requested | Status | Effort | Priority |
- | Input alias | `-i <file>` | [FAIL] Missing | 10 min | Low |
- | Parser selection | `-par <parser>` | [FAIL] Missing | 2-8 hours | **HIGH** |
### sb_isql Command-Line Analysis and Enhancement Recommendations > Conclusion
- The main gap is **parser selection** (`-par`), which requires:
- **Status:** [OK] Ready for implementation

## ScratchBird/docs/archive/2026-01-09/findings/SESSION_SUMMARY_2025_12_31.md

Issues (from source):
### Session Summary - 2025-12-31
- **Focus:** Critical FK Deadlock Fix + Build Errors
- **Status:** [OK] ALL ISSUES RESOLVED
### Session Summary - 2025-12-31 > Issues Fixed > 1. [OK] Executor Build Errors (5 locations)
- **Problem:** Missing function declarations after refactoring
### Session Summary - 2025-12-31 > Issues Fixed > 3. [OK] Critical FK Deadlock (Test #1240)
- Applied lock ordering fix: Made `createForeignKey()` acquire all 6 mutexes
- Still hung Not a simple lock ordering issue
- `src/core/catalog_manager.cpp:25794-25805` (createForeignKey lock ordering - preventive fix)
- [OK] Correctly returns `Status::CONSTRAINT_VIOLATION` with dependency error
### Session Summary - 2025-12-31 > Key Insights > Internal vs Public Function Pattern
- This fix reinforces the critical pattern in ScratchBird:
- **Fix Pattern:** Internal functions must access cache data directly, never call public functions
### Session Summary - 2025-12-31 > Time Investment
- Root cause identification and fix: 15 minutes
### Session Summary - 2025-12-31 > Success Metrics
- **Status:** [OK] **ALL TASKS COMPLETE** - Awaiting full test suite results

## ScratchBird/docs/archive/2026-01-09/findings/SQL_COMPATIBILITY_TEST_REPOSITORIES.md

Issues (from source):
### SQL Compatibility Test Repositories - FirebirdSQL, MySQL, PostgreSQL > Executive Summary > Repository Overview
- | Database | Repository | Test Count | Test Format | Status |
- | **FirebirdSQL Legacy** | fbtcs | 390+ tests | C/ESQL/Shell | [WARN] Deprecated |
### SQL Compatibility Test Repositories - FirebirdSQL, MySQL, PostgreSQL > FirebirdSQL Test Repositories > Primary Repository: fbt-repository
- **Status:** Active development
- Tests for recursive EXECUTE STATEMENT (Issue [#3126](https://github.com/FirebirdSQL/firebird/issues/3126))
- Recursive CTE query result tests (Issue [#1791](https://github.com/FirebirdSQL/firebird/issues/1791))
- Recursive CTE error message tests (Issue [#4337](https://github.com/FirebirdSQL/firebird/issues/4337))
- Deep tree recursive CTE tests (Issue [#4776](https://github.com/FirebirdSQL/firebird/issues/4776))
- Comprehensive functional and bug regression coverage
### SQL Compatibility Test Repositories - FirebirdSQL, MySQL, PostgreSQL > FirebirdSQL Test Repositories > Legacy Repository: fbtcs (Firebird Test Compatibility Suite)
- **Status:** [WARN] Deprecated (maintained for legacy compatibility)
### SQL Compatibility Test Repositories - FirebirdSQL, MySQL, PostgreSQL > MySQL Test Repository > Primary Repository: mysql-server/mysql-test
- **Status:** Active development
### SQL Compatibility Test Repositories - FirebirdSQL, MySQL, PostgreSQL > PostgreSQL Test Repository > Primary Repository: postgres/src/test/regress
- **Status:** Active development
- Edge cases and error conditions
### SQL Compatibility Test Repositories - FirebirdSQL, MySQL, PostgreSQL > Recursive CTE Feature Comparison
- | **LIMIT in CTE** | [FAIL] No | [OK] Yes (8.0.19+) | [OK] Yes |
- | **Aggregate in Recursive** | [FAIL] No | [FAIL] No | [OK] Yes (limited) |
- | **Data-Modifying CTEs** | [FAIL] No | [FAIL] No | [OK] Yes (INSERT/UPDATE/DELETE) |
### SQL Compatibility Test Repositories - FirebirdSQL, MySQL, PostgreSQL > Integration Plan for ScratchBird > Phase 3: Curated Test Selection (10-20 hours) > 1. Recursive CTE Tests (High Priority)
- Depth limits and error handling
### SQL Compatibility Test Repositories - FirebirdSQL, MySQL, PostgreSQL > Success Metrics > Coverage Metrics
- 50+ recursive CTE tests from each database
- 200+ core SQL compatibility tests from each database
- 90%+ pass rate for ScratchBird V2 parser
- 95%+ compatibility for emulated parsers (Firebird, PostgreSQL, MySQL)
### SQL Compatibility Test Repositories - FirebirdSQL, MySQL, PostgreSQL > Success Metrics > Quality Metrics
- All tests documented with source attribution
- Automated test execution in CI/CD
- Nightly compatibility reports
- Regression detection for parser changes
### SQL Compatibility Test Repositories - FirebirdSQL, MySQL, PostgreSQL > Success Metrics > Documentation Metrics
- Test coverage matrix (features parsers)
- Known compatibility gaps documented
- Conversion methodology documented
- Test execution guide for developers
### SQL Compatibility Test Repositories - FirebirdSQL, MySQL, PostgreSQL > References > PostgreSQL
- **Status:** [OK] Ready for implementation planning

## ScratchBird/docs/archive/2026-01-09/findings/TEST_EXECUTION_TIME_ANALYSIS_2025_12_31.md

Issues (from source):
### Test Execution Time Analysis - 2025-12-31
- **Status:** CRITICAL - One test exceeds 5 minutes, timeout mechanism not working
### Test Execution Time Analysis - 2025-12-31 > CRITICAL: Tests Exceeding 5 Minutes (300 seconds) > Test #1426: scratchbird_tests
- **Status:** Still running when manually killed
- Single failure prevents completion of remaining tests
- Timeout mechanism failure means tests can hang indefinitely
- CTest timeout not properly enforced (possible signal handling issue)
- **Recommended Fix:**
### Test Execution Time Analysis - 2025-12-31 > WARNING: Tests 40-300 Seconds (Under 5min, but slow) > 1. DependencyPerformanceTest.GetDependents100K
- **Status:** PASSING
### Test Execution Time Analysis - 2025-12-31 > WATCH: Tests 20-40 Seconds > 1. LoginAttemptTrackerTest.ExponentialBackoffMaxMultiplier
- **Status:** PASSING
### Test Execution Time Analysis - 2025-12-31 > ACTION ITEMS > Priority 2: Investigate CTest Timeout Failure (CRITICAL)
- **Issue:** CTest `--timeout 600` did not kill test after 600 seconds
### Test Execution Time Analysis - 2025-12-31 > Test Suite Statistics > Overall Health: GOOD (with critical issues)
- | Metric | Value | Status |
- | Max Runtime (actual) | 600s+ | FAILURE |
### Test Execution Time Analysis - 2025-12-31 > References
- **Status:** DOCUMENTED - ACTION REQUIRED
### Test Execution Time Analysis - 2025-12-31 > Appendix B: Test Count by Runtime Category
- | Runtime Category | Test Count | Percentage | Status |

## ScratchBird/docs/archive/2026-01-09/findings/TEST_FIXES_2025_12_31.md

Issues (from source):
### Test Fixes - 2025-12-31
- **Status:** [OK] ALL 5 TESTS RESOLVED (4 passing, 1 disabled with documentation)
- **Context:** Fixing 5 new test failures discovered after FK deadlock fix
### Test Fixes - 2025-12-31 > Summary
- Fixed 5 test failures that were exposed after the FK deadlock fix and executor.cpp repairs:
### Test Fixes - 2025-12-31 > Fix #1: ExecutorTransactionPayload Tests (2 tests) > Problem
- Tests failed with: "Execution error: Schema not found for table 'autocommit_on_test': Current schema not set"
### Test Fixes - 2025-12-31 > Fix #2: RenameMoveOpcode Column Rename Test > Root Cause Investigation
- Initially: "Schema path not found" error when using qualified table name "test.foo"
### Test Fixes - 2025-12-31 > Fix #2: RenameMoveOpcode Column Rename Test > Bytecode Format
- **IF has_uuid flag set:** UUID (16 bytes) Missing from test helpers
### Test Fixes - 2025-12-31 > Fix #2: RenameMoveOpcode Column Rename Test > Solution
- Also updated `readMovePayload()` with the same fix.
### Test Fixes - 2025-12-31 > Fix #3: RenameMoveOpcode Domain Rename Test > Problem
- Test `FirebirdRenameDomainEmitsExtendedOpcode` failed with "Domain not found" error during semantic analysis.
### Test Fixes - 2025-12-31 > Fix #3: RenameMoveOpcode Domain Rename Test > Root Cause
- This is a **missing feature** in the resolver cache, not a test bug.
### Test Fixes - 2025-12-31 > Fix #4: SchemaPathResolution Test > Problem
- Test `ExecutorTruncateTableUsesCurrentSchema` failed with error message mismatch.
### Test Fixes - 2025-12-31 > Fix #4: SchemaPathResolution Test > Root Cause
- The test expected error message to contain "Table not found", but the actual error message was:
- "Execution error: Failed to resolve table 'truncate_target': Object not found"
- This is a simple test assertion issue - the error message format changed but the test wasn't updated.
### Test Fixes - 2025-12-31 > Fix #4: SchemaPathResolution Test > Solution
- Updated the test to check for the correct error message:
### Test Fixes - 2025-12-31 > Files Modified > Core Test Files
- Line 528: Fixed error message check from "Table not found" to "Object not found"
### Test Fixes - 2025-12-31 > Technical Insights > 2. Resolver Cache Architecture
- [FAIL] Domains (missing)
- [FAIL] Roles (missing)
- [FAIL] Users (missing)
### Test Fixes - 2025-12-31 > Time Investment
- **ExecutorTransactionPayload fix**: ~15 minutes
- Fix implementation: 10 minutes
- **SchemaPathResolution fix**: ~5 minutes
### Test Fixes - 2025-12-31 > Success Metrics
- **Status:** [OK] **ALL TASKS COMPLETE**

## ScratchBird/docs/archive/2026-01-09/findings/TEST_SUITE_FAILURES_2025_12_27.md

Issues (from source):
### Test Suite Failure Analysis - 2025-12-27
- **Status:** 1 failed, 4 timed out, 12 not built
### Test Suite Failure Analysis - 2025-12-27 > Executive Summary
- [FAIL] **1 failed** (0.07%) - BytecodeOpcodesTest.SBLRVersionIsDefined
- [WARN] **12 not run** (0.9%) - Missing executables
### Test Suite Failure Analysis - 2025-12-27 > Failure #1: BytecodeOpcodesTest.SBLRVersionIsDefined > Details
- **Failure Type:** Assertion failure (version mismatch)
- **Status:** Build artifact issue (NOT a code bug)
### Test Suite Failure Analysis - 2025-12-27 > Failure #2-5: StoredCodeDependencyTest Timeouts (4 tests) > Details
- **Failure Type:** Timeout after 300 seconds
- **Status:** Likely deadlock or infinite loop
### Test Suite Failure Analysis - 2025-12-27 > Failure #2-5: StoredCodeDependencyTest Timeouts (4 tests) > Error Pattern
- All 4 tests exhibited the same repeating error:
- This error repeated continuously until the 300-second timeout.
### Test Suite Failure Analysis - 2025-12-27 > Failure #2-5: StoredCodeDependencyTest Timeouts (4 tests) > Test Structure Analysis
- **Expect Status::CONSTRAINT_VIOLATION**
### Test Suite Failure Analysis - 2025-12-27 > Failure #2-5: StoredCodeDependencyTest Timeouts (4 tests) > Root Cause Analysis
- **Probable Issue:** `catalog->dropFunction()` (and similar drop operations) are not returning when dependency constraints exist.
- **Monitor fails** (possibly trying to access locked catalog)
- Error repeats in loop until timeout
### Test Suite Failure Analysis - 2025-12-27 > Failure #2-5: StoredCodeDependencyTest Timeouts (4 tests) > Recommendation
- `src/core/long_transaction_monitor.cpp:267` - getActiveBackends failure
- `tests/unit/test_code_dependencies.cpp` - test setup (verify not test issue)
### Test Suite Failure Analysis - 2025-12-27 > Issue #3: Missing Test Executables (12 tests) > Details
- **Status:** [WARN] Not built (may be intentional)
### Test Suite Failure Analysis - 2025-12-27 > Issue #3: Missing Test Executables (12 tests) > Recommendation
- Document reasons (features not implemented, conditional builds, etc.)
### Test Suite Failure Analysis - 2025-12-27 > Summary of Recommendations > Priority 2: Investigate Dependency Drop Deadlocks (Critical)
- **Fix time:** Unknown (depends on complexity)
### Test Suite Failure Analysis - 2025-12-27 > Summary of Recommendations > Priority 3: Document Missing Tests (Low)
- Why (features not implemented, conditional compilation, etc.)
### Test Suite Failure Analysis - 2025-12-27 > Test Suite Health
- [OK] One build artifact issue (trivial fix)
- [WARN] Potential deadlock in dependency tracking (requires investigation)

## ScratchBird/docs/archive/2026-01-09/findings/TEST_SUITE_FIX_2025_12_31.md

Issues (from source):
### Test Suite Restructuring Fix - 2025-12-31
- **Status:** [OK] FIXED - Monolithic test broken into individual tests
- **Issue:** Test #1426 (scratchbird_tests) exceeded 10 minutes
### Test Suite Restructuring Fix - 2025-12-31 > Benefits > 1. Granular Test Execution
- Failure anywhere blocked all remaining tests
### Test Suite Restructuring Fix - 2025-12-31 > Test Execution Time Analysis > Tests Exceeding 5 Minutes
- **Before Fix:** 1 test
- `scratchbird_tests` - 10+ minutes (FAILURE)
- **After Fix:** 0 tests
### Test Suite Restructuring Fix - 2025-12-31 > Follow-Up Actions > Immediate (Today)
- [OK] Run full test suite to verify all tests pass
- Identify any tests that timeout at 60 seconds
- Create analysis of final test execution times
### Test Suite Restructuring Fix - 2025-12-31 > Follow-Up Actions > Short-Term (This Week)
- Add performance test labels to slow tests (>10s)
- Configure parallel test execution: `ctest -j8`
- Create test execution documentation
### Test Suite Restructuring Fix - 2025-12-31 > Follow-Up Actions > Medium-Term (Next Week)
- Implement test categorization (smoke/unit/integration/stress)
- Update CI/CD to use categorized test execution
- Add test timing monitoring to CI
### Test Suite Restructuring Fix - 2025-12-31 > Impact on Test Suite Metrics
- | Timeout per test | 1800s (monolithic) | 60s (individual) | 30x faster fail |
### Test Suite Restructuring Fix - 2025-12-31 > References
- **Issue Analysis:** `/docs/archive/2026-01-09/findings/TEST_EXECUTION_TIME_ANALYSIS_2025_12_31.md`
### Test Suite Restructuring Fix - 2025-12-31 > Success Criteria
- **Time to Fix:** ~30 minutes
- **Status:** [OK] **RESOLVED**

## ScratchBird/docs/archive/2026-01-09/findings/TEST_TIMEOUT_ANALYSIS_2025_12_29.md

Issues (from source):
### Test Timeout Analysis - 2025-12-29
- **Status:** CRITICAL - Deadlock Identified
### Test Timeout Analysis - 2025-12-29 > Executive Summary
- **Priority:** CRITICAL - Fix required before production use
### Test Timeout Analysis - 2025-12-29 > Failing Tests
- All expect `Status::CONSTRAINT_VIOLATION` to be returned
### Test Timeout Analysis - 2025-12-29 > Error Pattern
- During hang, continuous error output:
- Error repeats until 300s timeout
### Test Timeout Analysis - 2025-12-29 > Root Cause Analysis > Potential Deadlock Scenarios > Scenario 3: Long Transaction Monitor Interference
- From error message, the long transaction monitor is trying to access "active backends", which may require catalog access.
### Test Timeout Analysis - 2025-12-29 > Test Code Analysis > Test #222: DropFunctionFailsIfCalledByAnotherFunction
- Return `Status::CONSTRAINT_VIOLATION` immediately
### Test Timeout Analysis - 2025-12-29 > Long Transaction Monitor Involvement
- **Error Location:** `src/core/long_transaction_monitor.cpp:267`
- Monitor fails to get backends
- Logs error
- Retries error repeats
### Test Timeout Analysis - 2025-12-29 > Recommended Fixes > Option 2: Use Recursive Mutex
- Doesn't solve cross-thread deadlock (if that's the issue)
### Test Timeout Analysis - 2025-12-29 > Recommended Solution
- Run tests to verify fix
### Test Timeout Analysis - 2025-12-29 > Testing Plan > Step 4: Full Regression
- **Expected:** All 1,346 tests pass (or only BytecodeOpcodesTest fails due to build artifact)
### Test Timeout Analysis - 2025-12-29 > Priority and Impact
- **Priority:** Fix before any release or production deployment
### Test Timeout Analysis - 2025-12-29 > References
- **Status:** AWAITING FIX
- **Estimated Fix Time:** 2-3 hours

## ScratchBird/docs/archive/2026-01-09/findings/TEST_TIMEOUT_REANALYSIS_2025_12_30.md

Issues (from source):
### Test Timeout Re-Analysis - 2025-12-30
- **Status:** [OK] RESOLVED - Fix implemented and verified
### Test Timeout Re-Analysis - 2025-12-30 > Summary
- Another AI attempted to fix the deadlock by:
- **Result:** Tests still timeout after 60 seconds. Issue NOT resolved.
### Test Timeout Re-Analysis - 2025-12-30 > Analysis of Attempted Fix > Why It's STILL Deadlocking
- The original issue was that `getDependents()` is called WHILE `psql_mutex_` is held:
### Test Timeout Re-Analysis - 2025-12-30 > Recommended Fix > Option B: Use Try-Lock with Backoff (Not Recommended)
- Complex error handling
### Test Timeout Re-Analysis - 2025-12-30 > Next Steps
- **Run tests** to verify fix
### Test Timeout Re-Analysis - 2025-12-30 > Priority
- **Estimated Fix Time:** 3-4 hours for Option A
### Test Timeout Re-Analysis - 2025-12-30 > Resolution - 2025-12-30
- **Status:** [OK] FIXED
### Test Timeout Re-Analysis - 2025-12-30 > Resolution - 2025-12-30 > Test Results
- **After Fix:**
### Test Timeout Re-Analysis - 2025-12-30 > Resolution - 2025-12-30 > Details
- See complete fix documentation: `/docs/archive/2026-01-09/findings/DEADLOCK_FIX_2025_12_30.md`
- **Time to Fix:** ~3 hours

## ScratchBird/docs/archive/2026-01-09/findings/alpha_cluster_compatibility_audit.md

Issues (from source):
### Alpha Cluster Compatibility Audit > High-Risk Conflicts (Code vs Cluster Goals)
- 10) ShardingSphere-style routing is not implemented.

## ScratchBird/docs/archive/2026-01-09/findings/audit_results/AUDIT_TEMPLATE.md

Issues (from source):
### Audit Report Template > Executive Summary
- Overall status: PASS / FAIL / DEFERRED
### Audit Report Template > Plan Compliance Checklist > Plan 01 - Core Storage and GC
- Status: PASS / FAIL / DEFERRED
### Audit Report Template > Plan Compliance Checklist > Plan 02 - UUID Resolution, Rename, and Schema Move
- Status: PASS / FAIL / DEFERRED
### Audit Report Template > Plan Compliance Checklist > Plan 03 - Security Context, AuthKey, Audit, Quorum
- Status: PASS / FAIL / DEFERRED
### Audit Report Template > Plan Compliance Checklist > Plan 04 - Parser Coverage and Compatibility
- Status: PASS / FAIL / DEFERRED
### Audit Report Template > Plan Compliance Checklist > Plan 05 - Protocols, ODBC, Connection Pool
- Status: PASS / FAIL / DEFERRED
### Audit Report Template > Plan Compliance Checklist > Plan 06 - Metadata, SHOW Commands, Catalog
- Status: PASS / FAIL / DEFERRED
### Audit Report Template > Plan Compliance Checklist > Plan 07 - Emulated Protocol Compatibility
- Status: PASS / FAIL / DEFERRED
### Audit Report Template > Plan Compliance Checklist > Plan 08 - Protocol Conformance Testing
- Status: PASS / FAIL / DEFERRED
### Audit Report Template > Plan Compliance Checklist > Plan 09 - Audit Methodology
- Status: PASS / FAIL / DEFERRED
### Audit Report Template > Findings and Remediation
- Required fix:
- Required fix:

## ScratchBird/docs/archive/2026-01-09/findings/database_lifecycle_upgrade_plan.md

Issues (from source):
### Database Lifecycle and Upgrade Plan (Embedded Server Cluster Roles) > 2) Upgrade Path: Embedded Server Cluster > 2.2 Server Cluster Member Upgrade
- 3) Configure quorum threshold (N-of-M) and failure mode.
### Database Lifecycle and Upgrade Plan (Embedded Server Cluster Roles) > 8) Open Decisions (Architect-Configurable)
- Quorum thresholds and failure modes.

## ScratchBird/docs/archive/2026-01-09/findings/domain_support_gaps.md

Issues (from source):
### Domain Support Audit (Gaps + Required Work) > Findings (By Area) > 1) Catalog + Persistence (Mismatch and Missing Fields)
- **Missing catalog columns required by spec**:
### Domain Support Audit (Gaps + Required Work) > Findings (By Area) > 3) Type System Integration (Domain is Unknown)
- `ResolvedType` has **no domain ID** or domain metadata. Domains are resolved in `SemanticAnalyzerV2` but assigned `DataType::UNKNOWN`, losing type info. See `src/sblr/semantic_analyzer_v2.cpp` and `include/scratchbird/sblr/resolved_ast_v2.h`.
### Domain Support Audit (Gaps + Required Work) > Findings (By Area) > 4) Verification (Constraints, Defaults, Enforcement)
- Only simple `VALUE <op> literal` and limited `LIKE` patterns are supported; full expression evaluation is missing. See `src/core/domain_manager.cpp`.
### Domain Support Audit (Gaps + Required Work) > Findings (By Area) > 10) Tests
- Domain tests only verify that stubs exist and return expected error codes; they do **not** assert SQL-level behavior, persistence, or runtime enforcement.
### Domain Support Audit (Gaps + Required Work) > Decisions Confirmed
- 1) **ALTER DOMAIN validation**: full validation is required. If any dependent rows violate the new definition, ALTER fails and must report table_id and primary-key values for each violating row. In a cluster, local validation success marks the change as pending until all members confirm; final activation waits for cluster reports.

## ScratchBird/docs/archive/2026-01-09/findings/engine_gap_report.md

Issues (from source):
### ScratchBird Engine Gaps and Stubs (Code Review Findings)
- Scope: Code inspection of `src/` and `include/` with focus on stub/partial implementations and core database features that appear missing or unclear. Findings reference concrete code locations; comments were not trusted as proof of completion.
### ScratchBird Engine Gaps and Stubs (Code Review Findings) > 1) Storage, MVCC, and GC gaps
- **Observed**: `vacuum()` and `removeDeadEntries()` return `Status::OK` without doing any work; comments describe future behavior only. Columnstore segments are tracked in-memory; writes to disk are "in production" comments, not implemented.
- **Expected**: Decide intended behavior (stub or full migration) and align code + logs. Implement ONLINE mode or explicitly disable with clear error. Verify copy path correctness and transactional rollback.
- **Index TID updates missing for most index types during migration**
- **Observed**: TID updates are not implemented for VECTOR/HNSW, FULLTEXT, GIN, GIST, BRIN, RTREE; code logs warnings and recommends DROP+RECREATE.
- **Observed**: GC only opens BTREE, HASH, GIN, BRIN, VECTOR (HNSW). All other index types are reported as "GC not implemented".
### ScratchBird Engine Gaps and Stubs (Code Review Findings) > 2) Constraint deferral and SQL correctness gaps
- **Impact**: DEFERRABLE constraints effectively never validate at COMMIT; integrity guarantees are broken.
- **Impact**: DEFERRABLE/INITIALLY DEFERRED metadata cannot reach executor/catalog, making deferrable constraints effectively unsupported.
- **Group-by validation is missing**
- **Observed**: `validateGroupBy()` is a TODO that always returns true.
- **Observed**: Executor throws "ALTER TABLE action not implemented" for action codes outside a small subset.
- **Expected**: Implement missing ALTER TABLE actions (drop/rename/constraint modifications, etc.) and keep parser/executor aligned.
### ScratchBird Engine Gaps and Stubs (Code Review Findings) > 3) Catalog and metadata gaps
- **Timezone catalog management is missing**
### ScratchBird Engine Gaps and Stubs (Code Review Findings) > 4) Protocol / client access stubs (engine integration)
- **Impact**: Wire protocol behavior is incomplete and may accept invalid clients or fail to handle core operations.
- **Observed**: Connection, execution, prepared statements, and cancel are TODO; catalog queries are stubs.
- **Observed**: Actual connection logic, execution, validation, and reset are TODO.
- **Observed**: LDAP/AD/Kerberos/SAML are placeholders that always succeed or return "not implemented".
### ScratchBird Engine Gaps and Stubs (Code Review Findings) > 5) Core engine features expected but not present (based on specs and code)
- **Expected**: If WAL is planned for replication/PITR only, treat this as a missing *optional* subsystem and keep it out of core transaction-critical paths.
- **Dependency and comment tracking infrastructure is missing**
- **Query compilation cache is not implemented**
### ScratchBird Engine Gaps and Stubs (Code Review Findings) > 7) Parser, FDW, Tests, and Compatibility Gaps (multi-dialect model)
- **V2/ScratchBird parser missing CREATE/ALTER coverage**
- **Expected**: Implement missing CREATE/ALTER paths in V2 parser to keep the superset dialect complete.
- **Observed**: NULL-safe semantics for `=` are TODO, placeholder handling is TODO, table constraints parsing is TODO, geometry type mapping is TODO, and multiple unimplemented parser branches.
- **Observed**: TODO for ESCAPE in LIKE, array subscript handling, and CREATE statement stubs.
- **Impact**: PostgreSQL dialect fails on common expressions and DDL types.
- `src/fdw/postgresql_adapter.cpp#L330` (error fields)
- **Observed**: TODOs in result parsing, error handling, type metadata, and auth.
- **Observed**: Multiple tests are disabled or marked TODO due to missing V2 parser features or executor stubs.
- **Impact**: Large portions of dialect compatibility and catalog behavior are untested and possibly broken.
- **Impact**: Clients for Firebird/MySQL/PostgreSQL may see incomplete system catalogs or unsupported syntax despite connection success.
### ScratchBird Engine Gaps and Stubs (Code Review Findings) > 8) Security Architecture and UUID Resolution Gaps
- **Observed**: Audit events are buffered in memory; catalog persistence is a TODO, and no tamper-evident mechanism is present.
- If you want, I can break these into per-dialect implementation checklists (ScratchBird/V2, Firebird, MySQL, PostgreSQL) and map each gap to specific specs in `docs/specifications/`.
### ScratchBird Engine Gaps and Stubs (Code Review Findings) > 9) Object Persistence Gaps
- **Impact**: Non-durable metadata, broken dependency tracking after restart, and SHOW/DDL output becomes inconsistent with prior state.
### ScratchBird Engine Gaps and Stubs (Code Review Findings) > 10) Security Configuration Matrix (Architect-Controlled)
- | Quorum Failure Mode | Behavior on quorum loss | Global (cluster policy) | Configurable: fail-closed, restricted mode, or other architect-defined behavior. |

## ScratchBird/docs/archive/2026-01-09/findings/firebird_emulation_parity_audit.md

Issues (from source):
### Firebird Emulation Parity Audit > Reference Specs
- `docs/archive/2026-01-09/findings/firebird_wire_protocol_gaps.md` (existing gap list)
### Firebird Emulation Parity Audit > Parser Gaps (Missing or Stubbed Features) > DDL coverage is incomplete
- ALTER for object types other than TABLE/INDEX is not implemented.
- DROP for object types other than TABLE/INDEX/VIEW/SEQUENCE is not implemented.
- RECREATE for object types other than TABLE/VIEW/INDEX is not implemented.
- DROP SEQUENCE/GENERATOR is not implemented.
- ALTER INDEX is not implemented.
### Firebird Emulation Parity Audit > Parser Gaps (Missing or Stubbed Features) > DML / procedural gaps
- MERGE is not implemented.
- EXECUTE PROCEDURE is not implemented.
- EXECUTE STATEMENT is not implemented.
### Firebird Emulation Parity Audit > Parser Gaps (Missing or Stubbed Features) > Transaction / session / DCL gaps
- SET TRANSACTION is not implemented.
- SET statement is not implemented.
- SHOW statement (ISQL compatibility) is not implemented.
- GRANT and REVOKE are not implemented.
- COMMENT statement is not implemented.
### Firebird Emulation Parity Audit > Parser Gaps (Missing or Stubbed Features) > PSQL gaps
- FOR EXECUTE STATEMENT is not implemented.
- LOOP statement is not implemented.
### Firebird Emulation Parity Audit > Parser Gaps (Missing or Stubbed Features) > Expression gaps
- Window specification parsing is TODO for OVER clauses.
- LIKE/CONTAINING/STARTING/SIMILAR variant tracking is TODO; parser does not
### Firebird Emulation Parity Audit > Catalog and Metadata API Gaps > RDB$ coverage is partial and some tables are stubbed
- RDB$INDICES is empty (TODO to list indexes).
- RDB$INDEX_SEGMENTS is empty (TODO).
- RDB$GENERATORS is empty (TODO).
### Firebird Emulation Parity Audit > Summary (Firebird Parity Risk)
- Firebird emulation is missing large sections of DDL, DCL, and PSQL, and
- DROP DATABASE is stubbed. A native Firebird client will fail to rely on

## ScratchBird/docs/archive/2026-01-09/findings/firebird_wire_protocol_gaps.md

Issues (from source):
### Firebird Wire Protocol Compatibility Review (Findings) > Potential Gaps / Clarifications Needed
- **BLOB segmentation rules**: segment size limits, segment continuation, and status codes for end-of-blob need concrete limits and examples.
- **Wire compression**: DPB flags exist but compression framing and error handling should be fully defined.
- **Status vector mapping**: status vector format exists, but mapping to SQLSTATE/Firebird error codes and client-visible error strings should be clarified.

## ScratchBird/docs/archive/2026-01-09/findings/mysql_emulation_parity_audit.md

Issues (from source):
### MySQL Emulation Parity Audit > Reference Specs
- `docs/archive/2026-01-09/findings/mysql_wire_protocol_gaps.md` (existing gap list)
### MySQL Emulation Parity Audit > Parser Gaps (Missing or Stubbed Features) > DDL coverage is largely stubbed
- `parseCreateIndex` is TODO (no CREATE INDEX) at
- `parseCreateView` is TODO (no CREATE VIEW) at
- `parseCreateDatabase` is TODO (no CREATE DATABASE / SCHEMA) at
- `parseCreateProcedure` is TODO (no CREATE PROCEDURE) at
- `parseCreateFunction` is TODO (no CREATE FUNCTION) at
- `parseCreateTrigger` is TODO (no CREATE TRIGGER) at
### MySQL Emulation Parity Audit > Parser Gaps (Missing or Stubbed Features) > Table constraints are skipped
- `parseIndexDef` and `parseForeignKeyDef` are TODO and never called with
### MySQL Emulation Parity Audit > Parser Gaps (Missing or Stubbed Features) > RENAME TABLE not implemented
- support exists, so RENAME TABLE is unsupported.
### MySQL Emulation Parity Audit > Wire Protocol and Session API Gaps
- Authentication is trust-mode; password validation is TODO.
### MySQL Emulation Parity Audit > Feature Bleed / Non-MySQL Constructs
- Multi-segment qualified names (see parser gap above) allow ScratchBird-style

## ScratchBird/docs/archive/2026-01-09/findings/mysql_wire_protocol_gaps.md

Issues (from source):
### MySQL Wire Protocol Compatibility Review (Findings) > Potential Gaps / Clarifications Needed
- **OK packet/session state tracking**: OK packet fields for CLIENT_SESSION_TRACK and server status flags need explicit specification.
- **COM_STMT_FETCH / cursor protocol**: cursor behavior and status flags need clearer definitions.
- **Compression edge cases**: compressed packet boundaries and error recovery need explicit rules.

## ScratchBird/docs/archive/2026-01-09/findings/postgresql_emulation_parity_audit.md

Issues (from source):
### PostgreSQL Emulation Parity Audit > Reference Specs
- `docs/archive/2026-01-09/findings/postgresql_wire_protocol_gaps.md` (existing gap list)
### PostgreSQL Emulation Parity Audit > Parser Gaps (Missing or Stubbed Features) > Expression gaps
- evaluation is missing. `src/parser/postgresql/pg_parser_expr.cpp:353`.
### PostgreSQL Emulation Parity Audit > Wire Protocol and Session API Gaps
- Cancel request handling is TODO (no actual cancellation).
- Password validation is TODO for both MD5 and cleartext auth.
- MD5 hashing has a TODO and uses placeholder logic without OpenSSL.
### PostgreSQL Emulation Parity Audit > Summary (PostgreSQL Parity Risk)
- so native clients will fail to initialize and introspect. These gaps must be

## ScratchBird/docs/archive/2026-01-09/findings/postgresql_wire_protocol_gaps.md

Issues (from source):
### PostgreSQL Wire Protocol Compatibility Review (Findings) > Potential Gaps / Clarifications Needed
- **ErrorResponse fields**: Error/Notice fields and severity codes need full mapping for client compatibility.
- **COPY protocol**: COPY IN/OUT/BOTH framing must cover formats, header/trailer, and error recovery.
### PostgreSQL Wire Protocol Compatibility Review (Findings) > Suggested Validation Matrix
- **Simple query**: full message flow and error recovery.
