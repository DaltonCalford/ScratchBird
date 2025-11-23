# ScratchBird - Comprehensive Improvement Opportunities

**Date:** November 23, 2025
**Purpose:** Detailed catalog of all identified improvement opportunities organized by priority and subsystem

---

## TABLE OF CONTENTS

1. [Critical Issues (P0)](#critical-issues-p0)
2. [High Priority (P1)](#high-priority-p1)
3. [Medium Priority (P2)](#medium-priority-p2)
4. [Low Priority / Enhancements (P3)](#low-priority-enhancements-p3)
5. [Performance Optimizations](#performance-optimizations)
6. [Testing Improvements](#testing-improvements)
7. [Documentation Improvements](#documentation-improvements)

---

## CRITICAL ISSUES (P0)

**Total Items:** 8
**Estimated Effort:** 50-70 hours
**Target:** Alpha 1 completion

### P0-1: Password Policy Enforcement
**Component:** Security / Authentication
**Severity:** HIGH (CWE-521)
**Effort:** 8-10 hours

**Current State:**
- No password strength validation
- No minimum length requirement
- No complexity requirements
- No dictionary checks
- No password history

**Files:**
- `/home/user/ScratchBird/src/core/password_hash.cpp:101-114`
- `/home/user/ScratchBird/include/scratchbird/core/password_hash.h`

**Implementation:**
```cpp
struct PasswordPolicy {
    size_t min_length = 8;
    size_t max_length = 72;
    bool require_uppercase = true;
    bool require_lowercase = true;
    bool require_digit = true;
    bool require_symbol = true;
    bool check_common_passwords = true;
    size_t history_count = 5;
};

Status validatePasswordPolicy(const std::string& password,
                              const PasswordPolicy& policy,
                              ErrorContext* ctx);
```

**Impact:** Prevents weak passwords, meets compliance requirements (PCI-DSS, SOC 2)

---

### P0-2: Failed Login Tracking & Account Lockout
**Component:** Security / Authentication
**Severity:** HIGH (CWE-307)
**Effort:** 8-10 hours

**Current State:**
- No brute-force protection
- Unlimited login attempts
- No rate limiting
- No CAPTCHA support

**Files:**
- `/home/user/ScratchBird/src/core/auth_provider.cpp:23-103`
- `/home/user/ScratchBird/include/scratchbird/core/auth_provider.h:23-33`

**Implementation:**
```cpp
struct LoginAttemptTracker {
    std::unordered_map<std::string, FailedAttempts> attempts_;
    std::mutex mutex_;

    bool isAccountLocked(const std::string& username);
    void recordFailedAttempt(const std::string& username);
    void resetAttempts(const std::string& username);
};

struct FailedAttempts {
    uint32_t count = 0;
    uint64_t first_attempt_time = 0;
    uint64_t last_attempt_time = 0;
    uint64_t lockout_until = 0;
};
```

**Configuration:**
- Max failed attempts: 5
- Lockout duration: 15 minutes (exponential backoff)
- Reset window: 60 minutes

**Impact:** Prevents online password guessing attacks

---

### P0-3: Security Audit Logging
**Component:** Security / Audit
**Severity:** HIGH (CWE-778)
**Effort:** 12-15 hours

**Current State:**
- No audit log system
- Security events not logged
- No forensic trail
- Compliance failures (SOC 2, PCI-DSS)

**Missing Events:**
- User login success/failure
- Permission grants/revokes
- User/role creation/deletion
- Password changes
- Privilege escalation (SET ROLE)
- RLS policy violations
- Failed permission checks

**Implementation:**
```cpp
enum class AuditEventType {
    LOGIN_SUCCESS,
    LOGIN_FAILURE,
    LOGOUT,
    PASSWORD_CHANGE,
    USER_CREATED,
    USER_DELETED,
    ROLE_GRANTED,
    ROLE_REVOKED,
    PERMISSION_GRANTED,
    PERMISSION_REVOKED,
    PERMISSION_DENIED,
    RLS_VIOLATION,
    SUPERUSER_ACCESS,
    DDL_OPERATION
};

struct AuditEvent {
    uint64_t event_id;
    AuditEventType event_type;
    ID user_id;
    std::string username;
    std::string ip_address;  // Future: network layer
    std::string details;  // JSON format
    bool success;
    uint64_t timestamp;
};

class AuditLogger {
    void logEvent(const AuditEvent& event);
    void queryAuditLog(const AuditQuery& query,
                      std::vector<AuditEvent>& out);
};
```

**Files to Create:**
- `/home/user/ScratchBird/include/scratchbird/core/audit_logger.h`
- `/home/user/ScratchBird/src/core/audit_logger.cpp`
- New catalog table: `pg_audit_log`

**Impact:** Enables security incident response, meets compliance requirements

---

### P0-4: Arithmetic Overflow Checking
**Component:** Expression Evaluator
**Severity:** HIGH (Correctness)
**Effort:** 4 hours

**Current State:**
```cpp
// executor.cpp:158 - UNSAFE
return TypedValue::makeInt64(left.getInt64() + right.getInt64());
// INT64_MAX + 1 = undefined behavior
```

**Fix:**
```cpp
int64_t a = left.getInt64();
int64_t b = right.getInt64();
int64_t result;

if (__builtin_add_overflow(a, b, &result)) {
    error("Integer overflow in addition");
}
return TypedValue::makeInt64(result);
```

**Files:**
- `/home/user/ScratchBird/src/sblr/expression_evaluator.cpp:156-183`

**Operations to Fix:**
- ADD
- SUBTRACT
- MULTIPLY
- MODULO (also add division by zero check)

**Impact:** Prevents data corruption, undefined behavior

---

### P0-5: NaN/Infinity Handling
**Component:** Type Conversions
**Severity:** HIGH (Security/Correctness)
**Effort:** 2 hours

**Current State:**
```cpp
// type_conversions.cpp:488
float val = static_cast<float>(float_val);  // No validation!
```

**Fix:**
```cpp
if (std::isnan(float_val) || std::isinf(float_val)) {
    SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE,
        "Cannot convert NaN/Infinity to integer");
    return std::nullopt;
}
```

**Files:**
- `/home/user/ScratchBird/src/core/type_conversions.cpp:488-502`

**Also Add:**
- Mathematical function NaN/Infinity checks
- Comparison operator NaN handling

**Impact:** Prevents undefined behavior, improves IEEE 754 compliance

---

### P0-6: GIN Parallel Operations MGA Bug
**Component:** GIN Index
**Severity:** HIGH (Transaction Isolation)
**Effort:** 2 hours

**Current State:**
```cpp
// gin_index.cpp:3437
// TODO: findAllParallel should accept current_xid
// Parallel queries may return invisible tuples!
```

**Fix:**
```cpp
// Change signature:
Status findAllParallel(const std::vector<Key>& keys,
                      TransactionId current_xid,  // ADD THIS
                      std::vector<std::vector<TID>>* results,
                      ErrorContext* ctx);

// Update visibility check in parallel worker:
if (isVersionVisible(entry.xmin, current_xid)) {
    // Include result
}
```

**Files:**
- `/home/user/ScratchBird/src/core/gin_index.cpp:3437, 3523`
- `/home/user/ScratchBird/include/scratchbird/core/gin_index.h`

**Impact:** Fixes ACID isolation violation

---

### P0-7: Catalog Sequence Operations
**Component:** Catalog Manager
**Severity:** HIGH (Functionality)
**Effort:** 3-4 hours

**Current State:**
```cpp
// catalog_manager.cpp:8151
std::optional<SequenceInfo> getSequence(const std::string& name) {
    return std::nullopt;  // STUBBED!
}
```

**Impact:** IDENTITY columns broken

**Fix:** Implement full CRUD:
- `getSequence()` - Read from catalog
- `updateSequence()` - Update next value
- `getNextSequenceValue()` - Atomic increment

**Files:**
- `/home/user/ScratchBird/src/core/catalog_manager.cpp:8151-8157`

---

### P0-8: Charset/Collation Read Operations
**Component:** Catalog Manager
**Severity:** MEDIUM-HIGH (Functionality)
**Effort:** 3-4 hours

**Current State:**
- CREATE works, but all READ operations stubbed
- Cannot query character sets
- Cannot validate collations

**Files:**
- `/home/user/ScratchBird/src/core/catalog_manager.cpp:3342-3440`

**Fix:** Implement:
- `getCharset()`
- `getCollation()`
- `listCharsets()`
- `listCollations()`

---

## HIGH PRIORITY (P1)

**Total Items:** 15
**Estimated Effort:** 80-120 hours
**Target:** Beta 1

### P1-1: TRY/EXCEPT Exception Handling
**Component:** PSQL Executor
**Effort:** 10-15 hours

**Current State:**
- Infrastructure exists (ExceptionFrame, exception_stack_)
- RAISE throws C++ exceptions
- No TRY/EXCEPT execution logic

**Implementation Needed:**
- `executeTryStatement()`
- `executeExceptHandler()`
- Exception catch and dispatch
- Exception name matching
- SQLSTATE mapping

**Files:**
- `/home/user/ScratchBird/src/sblr/executor.cpp:15090-15114`

---

### P1-2: XID Wraparound Prevention
**Component:** Transaction Manager
**Effort:** 3-5 hours

**Issue:** Database stops accepting transactions at wraparound

**Fix:** Implement autovacuum trigger at 90% of MAX_SAFE_XID

**Files:**
- `/home/user/ScratchBird/src/core/transaction_manager.cpp:232-241`

---

### P1-3: SQLSTATE Error Codes
**Component:** Error Handling
**Effort:** 6-8 hours

**Implementation:**
```cpp
enum class SQLState {
    SUCCESS = 0,
    SYNTAX_ERROR_42601,
    UNIQUE_VIOLATION_23505,
    FK_VIOLATION_23503,
    CHECK_VIOLATION_23514,
    NOT_NULL_VIOLATION_23502,
    FK_NO_MATCH_23503,
    // ... full PostgreSQL SQLSTATE coverage
};
```

**Impact:** Programmatic error handling, PostgreSQL compatibility

---

### P1-4: Cursor Operations (Full Implementation)
**Component:** PSQL Executor
**Effort:** 20-25 hours

**Missing:**
- DECLARE CURSOR
- OPEN cursor
- FETCH (NEXT/PRIOR/FIRST/LAST/ABSOLUTE/RELATIVE)
- CLOSE cursor
- FOR SELECT loops

**Opcodes to Define:**
- `EXT_CURSOR_DECLARE`
- `EXT_CURSOR_OPEN`
- `EXT_CURSOR_FETCH`
- `EXT_CURSOR_CLOSE`

---

### P1-5: Stored Procedure Invocation
**Component:** PSQL Executor
**Effort:** 15-20 hours

**Missing:**
- Complete function execution
- Parameter binding (IN/OUT/INOUT)
- Return value handling
- SQL SECURITY DEFINER/INVOKER

**Files:**
- `/home/user/ScratchBird/src/sblr/executor.cpp:14521+`

---

### P1-6: Foreign Key Completion
**Component:** Constraint Enforcement
**Effort:** 15-20 hours

**Missing Actions:**
- CASCADE UPDATE
- SET NULL
- SET DEFAULT

**Blocker:** Requires tuple modification API

**Files:**
- `/home/user/ScratchBird/src/sblr/executor.cpp:15366-15478`

---

### P1-7: TIP Binary Search Optimization
**Component:** Transaction Manager
**Effort:** 4-6 hours

**Current:** O(N) linear search within TIP page (up to 32K entries)

**Improvement:** O(log N) binary search

**Impact:** 10-100x speedup for transaction state lookups

**Files:**
- `/home/user/ScratchBird/src/core/transaction_manager.cpp:1102-1128`

---

### P1-8: Index-Based FK Lookups
**Component:** Constraint Enforcement
**Effort:** 10-15 hours

**Current:** O(N) table scan to find parent/child rows

**Improvement:** Use index for O(log N) lookup

**Impact:** 100-1000x speedup for FK checks

**Files:**
- `/home/user/ScratchBird/src/sblr/executor.cpp` (FK enforcement logic)

---

### P1-9: Constraints Table Implementation
**Component:** Catalog System
**Effort:** 8-10 hours

**Current:** Page allocated but zero operations

**Needed:** Store CHECK, PRIMARY KEY, UNIQUE metadata

**Impact:** Cannot persist constraint definitions

---

### P1-10: Statistics Table & ANALYZE
**Component:** Catalog System
**Effort:** 20-30 hours

**Current:** Statistics table unused

**Needed:**
- ANALYZE command implementation
- Histogram collection
- n_distinct estimation
- Correlation statistics

**Impact:** Query optimizer has no cost data

---

### P1-11: Bulk Loading for Indexes
**Component:** Index System
**Effort:** 15-20 hours per index type

**Current:** O(N log N) individual inserts

**Improvement:** O(N) bottom-up construction

**Impact:** 3-5x faster initial index build

**Indexes:**
- B-Tree (highest priority)
- Hash
- GIN
- Bitmap

---

### P1-12: Session Timeout Implementation
**Component:** Security / Session Management
**Effort:** 4-6 hours

**Missing:**
- Idle timeout
- Max session lifetime
- Automatic session cleanup

**Files:**
- `/home/user/ScratchBird/include/scratchbird/core/catalog_manager.h:741-753`

---

### P1-13: MERGE Statement Completion
**Component:** SQL Executor
**Effort:** 8-10 hours

**Missing:**
- Full matching logic with ON condition
- INSERT logic for unmatched source rows
- DELETE logic for unmatched target rows

**Files:**
- `/home/user/ScratchBird/src/sblr/executor.cpp:5532-5570`

---

### P1-14: RETURNING Clause Implementation
**Component:** SQL Executor
**Effort:** 6-8 hours

**Current:** Opcode exists but implementation incomplete

**Files:**
- `/home/user/ScratchBird/src/sblr/executor.cpp:4398`

---

### P1-15: Multi-Geometry Functions
**Component:** Spatial Functions
**Effort:** 6-8 hours

**Missing (8 functions):**
- ST_MULTIPOINT
- ST_MULTILINESTRING
- ST_MULTIPOLYGON
- ST_GEOMETRYCOLLECTION
- ST_COLLECT
- ST_GEOMETRYN
- ST_NUMGEOMETRIES
- ST_DUMP

**Status:** Opcodes defined, not implemented

---

## MEDIUM PRIORITY (P2)

**Total Items:** 25
**Estimated Effort:** 100-150 hours
**Target:** Beta 2

### Performance Optimizations

#### P2-1: Page Table Lock Partitioning
**Component:** Buffer Pool
**Effort:** 8-10 hours

**Current:** Single mutex for entire page table

**Improvement:** Partition into N buckets with separate locks

**Impact:** Reduced lock contention under high concurrency

---

#### P2-2: Dirty Page Counter
**Component:** Buffer Pool
**Effort:** 2-3 hours

**Current:** O(N) scan to count dirty pages

**Improvement:** Atomic counter updated on flag changes

**Impact:** O(1) dirty page count for background writer

---

#### P2-3: TOAST Chunk Prefetching
**Component:** TOAST System
**Effort:** 6-8 hours

**Current:** Sequential chunk reads (many random I/Os)

**Improvement:** Batch read all chunks in single range scan

**Impact:** 5-10x faster for large TOAST values

---

#### P2-4: Permission Cache TTL Reduction
**Component:** Security
**Effort:** 1 hour

**Current:** 60-second TTL creates race window

**Improvement:** Reduce to 5-10 seconds

**Impact:** Smaller TOCTOU window

---

#### P2-5: Hash Index Directory Resize
**Component:** Hash Index
**Effort:** 10-12 hours

**Current:** Directory expansion blocks all writes

**Improvement:** Concurrent resize with fine-grained locking

**Impact:** No write stalls during resize

---

### Feature Completeness

#### P2-6: GENERATED Columns
**Component:** Constraint System
**Effort:** 25-30 hours

**Missing:**
- STORED variant (computed once, stored physically)
- VIRTUAL variant (computed on SELECT)
- Dependency tracking

---

#### P2-7: Deferred Constraints
**Component:** Constraint System
**Effort:** 20-25 hours

**Missing:**
- DEFERRABLE support
- INITIALLY DEFERRED/IMMEDIATE
- Deferred check accumulation
- Commit-time validation

---

#### P2-8: Statement-Level Triggers
**Component:** Trigger System
**Effort:** 20-25 hours

**Missing:**
- FOR EACH STATEMENT firing
- Transition table support (OLD TABLE, NEW TABLE)

---

#### P2-9: Window Function Frames
**Component:** SQL Executor
**Effort:** 20-30 hours

**Missing:**
- ROWS BETWEEN ... AND ...
- RANGE BETWEEN ... AND ...
- LAG/LEAD offset handling

---

#### P2-10: Statistical Aggregate Functions
**Component:** SQL Executor
**Effort:** 8-10 hours

**Current:** Stubs only

**Needed:**
- Proper mean accumulation
- Sum of squares calculation
- Correlation calculation

---

### Testing & Quality

#### P2-11: Edge Case Test Suite
**Effort:** 8-10 hours

**Coverage:**
- NaN handling
- Infinity handling
- Overflow/underflow
- Precision loss
- Character encoding edge cases

---

#### P2-12: Concurrent Transaction Tests
**Effort:** 10-12 hours

**Coverage:**
- Concurrent INSERT/UPDATE/DELETE
- Snapshot isolation verification
- Deadlock detection
- Lock escalation

---

#### P2-13: Performance Benchmark Suite
**Effort:** 15-20 hours

**Benchmarks:**
- TPC-H queries
- Index creation time
- Transaction throughput (TPS)
- Aggregate performance

---

#### P2-14: Constraint Enforcement Tests
**Effort:** 6-8 hours

**Coverage:**
- CHECK constraint violations
- FK constraint violations
- UNIQUE constraint violations
- IDENTITY column behavior

---

### Code Quality

#### P2-15: Role Cycle Detection
**Component:** Security
**Effort:** 4-6 hours

**Issue:** No cycle detection in role grants

**Fix:** DFS-based cycle detection before grant

---

#### P2-16: Policy Expression Validation
**Component:** Security / RLS
**Effort:** 4-6 hours

**Issue:** Policies validated at execution time only

**Fix:** Validate at CREATE POLICY time

---

#### P2-17: Error Message Context
**Component:** Error Handling
**Effort:** 8-10 hours

**Improvement:**
- Include constraint names in violation messages
- Add violating values to error context
- Provide suggested fixes

---

## LOW PRIORITY / ENHANCEMENTS (P3)

**Total Items:** 20+
**Estimated Effort:** 200+ hours
**Target:** Post-Beta

### P3-1: Password Expiration
**Component:** Security
**Effort:** 6-8 hours

**Feature:** 90-day password rotation policy

---

### P3-2: Multi-Factor Authentication (MFA)
**Component:** Security
**Effort:** 40-50 hours

**Blocker:** Requires Alpha 3 (network layer)

**Methods:**
- TOTP (time-based one-time passwords)
- WebAuthn support

---

### P3-3: IP Whitelisting/Blacklisting
**Component:** Security
**Effort:** 8-10 hours

**Blocker:** Requires Alpha 3 (network layer)

---

### P3-4: Certificate-Based Authentication
**Component:** Security
**Effort:** 20-25 hours

**Blocker:** Requires Alpha 3 (network layer)

**Feature:** X.509 client certificates

---

### P3-5: DECIMAL Fixed-Point Implementation
**Component:** Data Types
**Effort:** 16-20 hours

**Current:** DECIMAL stored as string (slow)

**Improvement:** int128_t with scale tracking

**Impact:** 10-100x faster arithmetic

---

### P3-6: SIMD Vector Operations
**Component:** Data Types
**Effort:** 12-16 hours

**Current:** Scalar operations

**Improvement:** AVX2/AVX-512 for dot product, L2 distance

**Impact:** 4-8x faster vector queries

---

### P3-7: Columnstore Enhancements
**Component:** Indexes
**Effort:** 30-40 hours

**Missing:**
- Dictionary encoding
- Vectorized execution
- Additional compression types

---

### P3-8: HNSW Quantization
**Component:** Indexes
**Effort:** 12-16 hours

**Feature:**
- Product quantization
- Scalar quantization

**Impact:** 4-8x memory reduction for vector indexes

---

### P3-9: LSM Compression
**Component:** Indexes
**Effort:** 4-6 hours

**Missing:** SSTable compression (Zstd integration)

**Impact:** 50-70% space savings

---

### P3-10: Bitmap RLE Compression
**Component:** Indexes
**Effort:** 6-8 hours

**Missing:** Run-length encoding for sequential TIDs

**Impact:** 50%+ space savings

---

### P3-11: TIP Compaction
**Component:** Transaction Manager
**Effort:** 10-12 hours

**Issue:** TIP pages grow unbounded

**Fix:** Consolidate old committed/aborted transactions

---

### P3-12: Catalog B-Tree Indexes
**Component:** Catalog System
**Effort:** 15-20 hours

**Current:** Linear scan through catalog pages

**Improvement:** B-tree indexes on catalog tables

**Impact:** 100-1000x faster catalog lookups

---

### P3-13: View Security Context
**Component:** Security
**Effort:** 6-8 hours

**Feature:** SECURITY DEFINER vs SECURITY INVOKER

---

### P3-14: Partition Pruning
**Component:** Query Planner
**Effort:** 30-40 hours

**Feature:**
- Table partitioning (RANGE, LIST, HASH)
- Predicate-based pruning
- Partition-wise joins

---

### P3-15: Materialized View Rewriting
**Component:** Query Planner
**Effort:** 40-50 hours

**Feature:** Automatic MV selection for queries

---

### P3-16: Telemetry Export
**Component:** Observability
**Effort:** 15-20 hours

**Feature:** Prometheus/OpenMetrics integration

---

### P3-17: Structured Logging
**Component:** Observability
**Effort:** 10-12 hours

**Improvement:** JSON logging format

---

### P3-18: Query Profiler
**Component:** Observability
**Effort:** 20-25 hours

**Feature:** EXPLAIN ANALYZE with timing

---

### P3-19: Common Subexpression Elimination
**Component:** Query Optimizer
**Effort:** 15-20 hours

**Feature:** CSE in WHERE clauses

---

### P3-20: Join Ordering Optimization
**Component:** Query Optimizer
**Effort:** 25-30 hours

**Feature:** Cost-based join ordering

---

## SUMMARY BY CATEGORY

| Category | P0 | P1 | P2 | P3 | Total |
|----------|----|----|----|----|-------|
| **Security** | 3 | 3 | 2 | 4 | 12 |
| **Correctness** | 3 | 2 | 0 | 0 | 5 |
| **Performance** | 0 | 3 | 5 | 8 | 16 |
| **Features** | 2 | 7 | 4 | 7 | 20 |
| **Testing** | 0 | 0 | 4 | 0 | 4 |
| **Quality** | 0 | 0 | 3 | 1 | 4 |
| **Total** | 8 | 15 | 18 | 20 | 61 |

**Estimated Effort:**
- P0: 50-70 hours
- P1: 80-120 hours
- P2: 100-150 hours
- P3: 200+ hours
- **Grand Total:** 430-540 hours (11-14 weeks)

---

**Document Generated:** November 23, 2025
**Next Update:** After Alpha 1 completion
