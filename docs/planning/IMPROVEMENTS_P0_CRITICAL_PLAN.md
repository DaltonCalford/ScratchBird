# Alpha 1 - Critical Issues (P0) Implementation Plan

**Created:** November 23, 2025
**Priority:** P0 - CRITICAL
**Estimated Effort:** 50-70 hours
**Target:** Alpha 1 completion (MANDATORY)
**Dependencies:** None - can start immediately

---

## OVERVIEW

This plan covers 8 critical issues that must be resolved for Alpha 1 completion. These items address security vulnerabilities, correctness bugs, and functionality gaps that would prevent production deployment.

**Execution Strategy:** Items P0-1 through P0-3 are security-related and can be assigned to one agent. Items P0-4 through P0-8 are correctness/functionality issues and can be assigned to a second agent for parallel execution.

---

## AGENT 1: SECURITY CRITICAL ISSUES

**Items:** P0-1, P0-2, P0-3
**Total Effort:** 28-35 hours
**Focus:** Authentication, authorization, audit logging

---

### P0-1: Password Policy Enforcement

**Severity:** HIGH (CWE-521)
**Effort:** 8-10 hours
**Component:** Security / Authentication

#### Current State
- No password strength validation
- No minimum length requirement
- No complexity requirements
- No dictionary checks
- No password history tracking

#### Implementation Plan

**Phase 1: Policy Structure (2 hours)**

Create `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/password_policy.h`:

```cpp
#ifndef SCRATCHBIRD_CORE_PASSWORD_POLICY_H
#define SCRATCHBIRD_CORE_PASSWORD_POLICY_H

#include <string>
#include <vector>
#include "status.h"
#include "error_context.h"

namespace scratchbird {
namespace core {

struct PasswordPolicy {
    size_t min_length = 8;
    size_t max_length = 72;  // BCrypt limit
    bool require_uppercase = true;
    bool require_lowercase = true;
    bool require_digit = true;
    bool require_symbol = true;
    bool check_common_passwords = true;
    size_t history_count = 5;  // Prevent password reuse
};

// Validates password against policy
Status validatePasswordPolicy(
    const std::string& password,
    const PasswordPolicy& policy,
    ErrorContext* ctx
);

// Checks if password is in common password list
bool isCommonPassword(const std::string& password);

// Loads common password dictionary (top 10,000)
Status loadCommonPasswordDictionary(ErrorContext* ctx);

}}  // namespace scratchbird::core

#endif  // SCRATCHBIRD_CORE_PASSWORD_POLICY_H
```

**Phase 2: Implementation (4 hours)**

Create `/home/dcalford/CliWork/ScratchBird/src/core/password_policy.cpp`:

```cpp
#include "scratchbird/core/password_policy.h"
#include <cctype>
#include <unordered_set>
#include <algorithm>

namespace scratchbird {
namespace core {

// Common password dictionary (lazy-loaded)
static std::unordered_set<std::string> common_passwords_;
static bool dictionary_loaded_ = false;

Status loadCommonPasswordDictionary(ErrorContext* ctx) {
    if (dictionary_loaded_) {
        return Status::OK;
    }

    // Top 10,000 common passwords
    // TODO: Load from data/common_passwords.txt
    common_passwords_ = {
        "password", "123456", "123456789", "12345678", "12345",
        "1234567", "qwerty", "abc123", "111111", "password1",
        "admin", "letmein", "welcome", "monkey", "dragon",
        // ... (embed top 100-1000 for now)
    };

    dictionary_loaded_ = true;
    return Status::OK;
}

bool isCommonPassword(const std::string& password) {
    if (!dictionary_loaded_) {
        loadCommonPasswordDictionary(nullptr);
    }

    std::string lowercase = password;
    std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(), ::tolower);
    return common_passwords_.count(lowercase) > 0;
}

Status validatePasswordPolicy(
    const std::string& password,
    const PasswordPolicy& policy,
    ErrorContext* ctx
) {
    // Length check
    if (password.length() < policy.min_length) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Password must be at least " + std::to_string(policy.min_length) + " characters");
        return Status::INVALID_ARGUMENT;
    }

    if (password.length() > policy.max_length) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Password must be at most " + std::to_string(policy.max_length) + " characters");
        return Status::INVALID_ARGUMENT;
    }

    // Complexity checks
    bool has_uppercase = false;
    bool has_lowercase = false;
    bool has_digit = false;
    bool has_symbol = false;

    for (char c : password) {
        if (std::isupper(c)) has_uppercase = true;
        else if (std::islower(c)) has_lowercase = true;
        else if (std::isdigit(c)) has_digit = true;
        else has_symbol = true;
    }

    if (policy.require_uppercase && !has_uppercase) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Password must contain at least one uppercase letter");
        return Status::INVALID_ARGUMENT;
    }

    if (policy.require_lowercase && !has_lowercase) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Password must contain at least one lowercase letter");
        return Status::INVALID_ARGUMENT;
    }

    if (policy.require_digit && !has_digit) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Password must contain at least one digit");
        return Status::INVALID_ARGUMENT;
    }

    if (policy.require_symbol && !has_symbol) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Password must contain at least one symbol");
        return Status::INVALID_ARGUMENT;
    }

    // Common password check
    if (policy.check_common_passwords && isCommonPassword(password)) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Password is too common - please choose a stronger password");
        return Status::INVALID_ARGUMENT;
    }

    return Status::OK;
}

}}  // namespace scratchbird::core
```

**Phase 3: Integration (2 hours)**

Update `/home/dcalford/CliWork/ScratchBird/src/core/password_hash.cpp`:

```cpp
#include "scratchbird/core/password_policy.h"

Status hashPassword(const std::string& password, std::string& hash_out, ErrorContext* ctx) {
    // Validate password policy BEFORE hashing
    PasswordPolicy policy;  // Use default policy
    Status status = validatePasswordPolicy(password, policy, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Existing BCrypt logic...
}
```

**Phase 4: Testing (2 hours)**

Create `/home/dcalford/CliWork/ScratchBird/tests/unit/test_password_policy.cpp`:

```cpp
#include <gtest/gtest.h>
#include "scratchbird/core/password_policy.h"

TEST(PasswordPolicy, MinimumLength) {
    PasswordPolicy policy;
    policy.min_length = 8;

    ErrorContext ctx;
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("short", policy, &ctx));
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("LongEnough123!", policy, &ctx));
}

TEST(PasswordPolicy, ComplexityRequirements) {
    PasswordPolicy policy;

    ErrorContext ctx;
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("alllowercase123!", policy, &ctx));
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("ALLUPPERCASE123!", policy, &ctx));
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("NoDigitsHere!", policy, &ctx));
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("NoSymbols123", policy, &ctx));
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("ValidPass123!", policy, &ctx));
}

TEST(PasswordPolicy, CommonPasswordRejection) {
    PasswordPolicy policy;

    ErrorContext ctx;
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("Password123!", policy, &ctx));
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("Admin123!", policy, &ctx));
}
```

---

### P0-2: Failed Login Tracking & Account Lockout

**Severity:** HIGH (CWE-307)
**Effort:** 8-10 hours
**Component:** Security / Authentication

#### Current State
- No brute-force protection
- Unlimited login attempts
- No rate limiting
- No account lockout mechanism

#### Implementation Plan

**Phase 1: Tracking Structure (2 hours)**

Create `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/login_attempt_tracker.h`:

```cpp
#ifndef SCRATCHBIRD_CORE_LOGIN_ATTEMPT_TRACKER_H
#define SCRATCHBIRD_CORE_LOGIN_ATTEMPT_TRACKER_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdint>

namespace scratchbird {
namespace core {

struct FailedAttempts {
    uint32_t count = 0;
    uint64_t first_attempt_time = 0;  // Unix timestamp (ms)
    uint64_t last_attempt_time = 0;   // Unix timestamp (ms)
    uint64_t lockout_until = 0;       // Unix timestamp (ms), 0 = not locked
};

struct LockoutPolicy {
    uint32_t max_attempts = 5;        // Max failed attempts before lockout
    uint64_t reset_window_ms = 3600000;  // 1 hour (reset counter)
    uint64_t base_lockout_ms = 900000;   // 15 minutes (first lockout)
    bool exponential_backoff = true;   // Double lockout time on repeat
    uint32_t max_lockout_multiplier = 8; // Max 2 hours (15min * 8)
};

class LoginAttemptTracker {
public:
    LoginAttemptTracker(const LockoutPolicy& policy = LockoutPolicy());

    // Check if account is currently locked
    bool isAccountLocked(const std::string& username);

    // Record a failed login attempt
    void recordFailedAttempt(const std::string& username);

    // Record successful login (resets counter)
    void recordSuccessfulLogin(const std::string& username);

    // Get lockout time remaining (0 if not locked)
    uint64_t getLockoutTimeRemaining(const std::string& username);

    // Clear all tracking (admin reset)
    void clearAttempts(const std::string& username);

    // Cleanup old entries (periodic maintenance)
    void cleanupExpiredEntries();

private:
    LockoutPolicy policy_;
    std::unordered_map<std::string, FailedAttempts> attempts_;
    std::mutex mutex_;

    uint64_t getCurrentTimeMs() const;
};

}}  // namespace scratchbird::core

#endif  // SCRATCHBIRD_CORE_LOGIN_ATTEMPT_TRACKER_H
```

**Phase 2: Implementation (4 hours)**

Create `/home/dcalford/CliWork/ScratchBird/src/core/login_attempt_tracker.cpp` with full implementation.

**Phase 3: Integration (2 hours)**

Update `/home/dcalford/CliWork/ScratchBird/src/core/auth_provider.cpp`:

```cpp
#include "scratchbird/core/login_attempt_tracker.h"

// Add as member variable
LoginAttemptTracker login_tracker_;

Status AuthProvider::authenticate(const std::string& username,
                                  const std::string& password,
                                  ErrorContext* ctx) {
    // Check if account is locked FIRST
    if (login_tracker_.isAccountLocked(username)) {
        uint64_t remaining = login_tracker_.getLockoutTimeRemaining(username);
        SET_ERROR_CONTEXT(ctx, Status::PERMISSION_DENIED,
            "Account locked due to too many failed attempts. Try again in " +
            std::to_string(remaining / 60000) + " minutes");
        return Status::PERMISSION_DENIED;
    }

    // Existing authentication logic...

    if (password_matches) {
        login_tracker_.recordSuccessfulLogin(username);
        return Status::OK;
    } else {
        login_tracker_.recordFailedAttempt(username);
        SET_ERROR_CONTEXT(ctx, Status::PERMISSION_DENIED, "Invalid credentials");
        return Status::PERMISSION_DENIED;
    }
}
```

**Phase 4: Testing (2 hours)**

Test lockout behavior, exponential backoff, reset logic.

---

### P0-3: Security Audit Logging

**Severity:** HIGH (CWE-778)
**Effort:** 12-15 hours
**Component:** Security / Audit

#### Current State
- No audit log system
- Security events not logged
- No forensic trail
- Compliance failures (SOC 2, PCI-DSS)

#### Implementation Plan

**Phase 1: Audit Event Structure (3 hours)**

Create `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/audit_logger.h`:

```cpp
#ifndef SCRATCHBIRD_CORE_AUDIT_LOGGER_H
#define SCRATCHBIRD_CORE_AUDIT_LOGGER_H

#include <string>
#include <vector>
#include <mutex>
#include "status.h"
#include "types.h"

namespace scratchbird {
namespace core {

enum class AuditEventType {
    // Authentication
    LOGIN_SUCCESS,
    LOGIN_FAILURE,
    LOGOUT,
    PASSWORD_CHANGE,
    PASSWORD_RESET,
    ACCOUNT_LOCKED,

    // Authorization
    PERMISSION_GRANTED,
    PERMISSION_REVOKED,
    PERMISSION_DENIED,
    ROLE_GRANTED,
    ROLE_REVOKED,

    // User Management
    USER_CREATED,
    USER_DELETED,
    USER_MODIFIED,
    ROLE_CREATED,
    ROLE_DELETED,

    // Data Access
    RLS_VIOLATION,
    COLUMN_ACCESS_DENIED,

    // Privilege Escalation
    SUPERUSER_ACCESS,
    SET_ROLE,

    // DDL Operations
    DDL_CREATE,
    DDL_ALTER,
    DDL_DROP,

    // System Events
    DATABASE_STARTUP,
    DATABASE_SHUTDOWN,
    BACKUP_STARTED,
    BACKUP_COMPLETED
};

struct AuditEvent {
    uint64_t event_id;           // Sequential ID
    AuditEventType event_type;
    ID user_id;                  // User who performed action
    std::string username;
    ID target_user_id;           // User affected by action (if applicable)
    std::string target_username;
    std::string object_type;     // TABLE, ROLE, USER, etc.
    std::string object_name;     // Name of affected object
    std::string details;         // JSON format for structured data
    bool success;                // true = action succeeded
    uint64_t timestamp;          // Unix timestamp (ms)
    std::string ip_address;      // Future: network layer
    std::string application_name; // Future: network layer
};

struct AuditQuery {
    uint64_t start_time = 0;     // Filter by time range
    uint64_t end_time = 0;
    std::optional<ID> user_id;   // Filter by user
    std::optional<AuditEventType> event_type; // Filter by event type
    std::string object_name;     // Filter by object
    std::optional<bool> success; // Filter by success/failure
    uint32_t limit = 100;        // Max results
    uint32_t offset = 0;         // Pagination
};

class AuditLogger {
public:
    AuditLogger();
    ~AuditLogger();

    // Log an audit event
    Status logEvent(const AuditEvent& event, ErrorContext* ctx);

    // Query audit log
    Status queryAuditLog(const AuditQuery& query,
                        std::vector<AuditEvent>& out,
                        ErrorContext* ctx);

    // Get total event count
    uint64_t getTotalEventCount();

    // Flush to disk (async buffer)
    Status flush(ErrorContext* ctx);

private:
    uint64_t next_event_id_;
    std::mutex mutex_;
    // TODO: Catalog table integration
};

}}  // namespace scratchbird::core

#endif  // SCRATCHBIRD_CORE_AUDIT_LOGGER_H
```

**Phase 2: Catalog Table Schema (2 hours)**

Add to catalog schema:

```sql
CREATE TABLE pg_audit_log (
    event_id BIGINT PRIMARY KEY,
    event_type INTEGER NOT NULL,
    event_timestamp BIGINT NOT NULL,
    user_id UUID NOT NULL,
    username VARCHAR(128) NOT NULL,
    target_user_id UUID,
    target_username VARCHAR(128),
    object_type VARCHAR(64),
    object_name VARCHAR(256),
    details TEXT,  -- JSON format
    success BOOLEAN NOT NULL,
    ip_address VARCHAR(45),  -- IPv6 compatible
    application_name VARCHAR(128)
);

CREATE INDEX idx_audit_timestamp ON pg_audit_log(event_timestamp);
CREATE INDEX idx_audit_user ON pg_audit_log(user_id);
CREATE INDEX idx_audit_type ON pg_audit_log(event_type);
```

**Phase 3: Implementation (5 hours)**

Implement `audit_logger.cpp` with catalog integration.

**Phase 4: Integration Points (2 hours)**

Add audit logging to:
- `auth_provider.cpp` - login/logout events
- `catalog_manager.cpp` - user/role creation/deletion
- `executor.cpp` - GRANT/REVOKE statements
- `security_manager.cpp` - permission checks

**Phase 5: Testing (2 hours)**

Verify all security events are logged correctly.

---

## AGENT 2: CORRECTNESS & FUNCTIONALITY CRITICAL ISSUES

**Items:** P0-4, P0-5, P0-6, P0-7, P0-8
**Total Effort:** 14-18 hours
**Focus:** Arithmetic correctness, MGA compliance, catalog CRUD

---

### P0-4: Arithmetic Overflow Checking

**Severity:** HIGH (Correctness)
**Effort:** 4 hours
**Component:** Expression Evaluator

#### Current State
```cpp
// executor.cpp:158 - UNSAFE
return TypedValue::makeInt64(left.getInt64() + right.getInt64());
// INT64_MAX + 1 = undefined behavior
```

#### Implementation Plan

**Phase 1: Overflow Detection Functions (1 hour)**

Add to `/home/dcalford/CliWork/ScratchBird/src/sblr/expression_evaluator.cpp`:

```cpp
// Safe arithmetic operations using compiler intrinsics
inline bool safeAdd(int64_t a, int64_t b, int64_t* result) {
    return !__builtin_add_overflow(a, b, result);
}

inline bool safeSubtract(int64_t a, int64_t b, int64_t* result) {
    return !__builtin_sub_overflow(a, b, result);
}

inline bool safeMultiply(int64_t a, int64_t b, int64_t* result) {
    return !__builtin_mul_overflow(a, b, result);
}

inline bool safeDivide(int64_t a, int64_t b, int64_t* result, ErrorContext* ctx) {
    if (b == 0) {
        SET_ERROR_CONTEXT(ctx, Status::DIVISION_BY_ZERO, "Division by zero");
        return false;
    }
    // Check for INT64_MIN / -1 overflow
    if (a == INT64_MIN && b == -1) {
        SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Integer overflow in division");
        return false;
    }
    *result = a / b;
    return true;
}
```

**Phase 2: Update Arithmetic Operations (2 hours)**

Update all arithmetic operations in `expression_evaluator.cpp`:

```cpp
TypedValue performAdd(const TypedValue& left, const TypedValue& right, ErrorContext* ctx) {
    if (left.type == ValueType::INT64 && right.type == ValueType::INT64) {
        int64_t result;
        if (!safeAdd(left.getInt64(), right.getInt64(), &result)) {
            SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Integer overflow in addition");
            return TypedValue::makeNull();
        }
        return TypedValue::makeInt64(result);
    }
    // Existing float/double logic...
}

// Similarly for SUBTRACT, MULTIPLY, DIVIDE, MODULO
```

**Phase 3: Testing (1 hour)**

Create `/home/dcalford/CliWork/ScratchBird/tests/unit/test_arithmetic_overflow.cpp`:

```cpp
TEST(ArithmeticOverflow, Addition) {
    // INT64_MAX + 1 should error
    ErrorContext ctx;
    TypedValue left = TypedValue::makeInt64(INT64_MAX);
    TypedValue right = TypedValue::makeInt64(1);
    TypedValue result = performAdd(left, right, &ctx);
    EXPECT_EQ(Status::OUT_OF_RANGE, ctx.status);
}

TEST(ArithmeticOverflow, Multiplication) {
    ErrorContext ctx;
    TypedValue left = TypedValue::makeInt64(INT64_MAX);
    TypedValue right = TypedValue::makeInt64(2);
    TypedValue result = performMultiply(left, right, &ctx);
    EXPECT_EQ(Status::OUT_OF_RANGE, ctx.status);
}

TEST(ArithmeticOverflow, DivisionByZero) {
    ErrorContext ctx;
    TypedValue left = TypedValue::makeInt64(100);
    TypedValue right = TypedValue::makeInt64(0);
    TypedValue result = performDivide(left, right, &ctx);
    EXPECT_EQ(Status::DIVISION_BY_ZERO, ctx.status);
}
```

---

### P0-5: NaN/Infinity Handling

**Severity:** HIGH (Security/Correctness)
**Effort:** 2 hours
**Component:** Type Conversions

#### Implementation Plan

Update `/home/dcalford/CliWork/ScratchBird/src/core/type_conversions.cpp`:

```cpp
std::optional<int64_t> convertFloatToInt64(double float_val, ErrorContext* ctx) {
    // Check for NaN
    if (std::isnan(float_val)) {
        SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE,
            "Cannot convert NaN to integer");
        return std::nullopt;
    }

    // Check for infinity
    if (std::isinf(float_val)) {
        SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE,
            "Cannot convert Infinity to integer");
        return std::nullopt;
    }

    // Check range
    if (float_val > static_cast<double>(INT64_MAX) ||
        float_val < static_cast<double>(INT64_MIN)) {
        SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE,
            "Float value out of range for INT64");
        return std::nullopt;
    }

    return static_cast<int64_t>(float_val);
}

// Similarly for FLOAT → INT32, DOUBLE → INT64, etc.
```

Add NaN handling to mathematical functions:

```cpp
TypedValue executeFunction_SQRT(const TypedValue& arg, ErrorContext* ctx) {
    double val = arg.getDouble();

    if (val < 0) {
        SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Square root of negative number");
        return TypedValue::makeNull();
    }

    double result = std::sqrt(val);

    // Defensive: should not happen for valid inputs
    if (std::isnan(result) || std::isinf(result)) {
        SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Invalid mathematical result");
        return TypedValue::makeNull();
    }

    return TypedValue::makeDouble(result);
}
```

---

### P0-6: GIN Parallel Operations MGA Bug

**Severity:** HIGH (Transaction Isolation)
**Effort:** 2 hours
**Component:** GIN Index

#### Implementation Plan

**Files to Update:**
- `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/gin_index.h`
- `/home/dcalford/CliWork/ScratchBird/src/core/gin_index.cpp`

**Changes:**

```cpp
// gin_index.h - Update signature
Status findAllParallel(const std::vector<Key>& keys,
                      TransactionId current_xid,  // ADD THIS PARAMETER
                      std::vector<std::vector<TID>>* results,
                      ErrorContext* ctx);

// gin_index.cpp - Update visibility check in parallel worker
void parallelWorker(const Key& key,
                   TransactionId current_xid,  // ADD THIS
                   std::vector<TID>* results) {
    // Existing logic to find posting list...

    for (const auto& entry : posting_list) {
        // FIX: Add visibility check
        if (isVersionVisible(entry.xmin, current_xid)) {
            results->push_back(entry.tid);
        }
    }
}

// Update all call sites to pass current_xid
```

Test with concurrent transactions to verify isolation.

---

### P0-7: Catalog Sequence Operations

**Severity:** HIGH (Functionality)
**Effort:** 3-4 hours
**Component:** Catalog Manager

#### Current State
```cpp
// catalog_manager.cpp:8151 - STUBBED
std::optional<SequenceInfo> getSequence(const std::string& name) {
    return std::nullopt;  // NOT IMPLEMENTED!
}
```

#### Impact
- IDENTITY columns broken
- Sequence functions (NEXTVAL, CURRVAL) not working

#### Implementation Plan

Update `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp`:

```cpp
std::optional<SequenceInfo> CatalogManager::getSequence(const std::string& name,
                                                         ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(catalog_mutex_);

    // Read from pg_sequences catalog table
    PageNum page_num = sequences_page_;
    if (page_num == 0) {
        return std::nullopt;  // No sequences page
    }

    // Pin page
    BufferFrame* frame = buffer_pool_->pinPage(page_num, ctx);
    if (!frame) {
        return std::nullopt;
    }

    // Scan for sequence by name
    CatalogPage* page = reinterpret_cast<CatalogPage*>(frame->data);
    for (size_t i = 0; i < page->count; i++) {
        SequenceInfo* seq = &page->sequences[i];
        if (seq->name == name) {
            SequenceInfo result = *seq;
            buffer_pool_->unpinPage(page_num);
            return result;
        }
    }

    buffer_pool_->unpinPage(page_num);
    return std::nullopt;
}

Status CatalogManager::updateSequence(const SequenceInfo& seq, ErrorContext* ctx) {
    // Implementation: Update sequence in catalog page
    // Mark page dirty
    // Return status
}

int64_t CatalogManager::getNextSequenceValue(const std::string& name,
                                              ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(catalog_mutex_);

    auto seq_opt = getSequence(name, ctx);
    if (!seq_opt) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Sequence not found: " + name);
        return -1;
    }

    SequenceInfo seq = *seq_opt;

    // Atomic increment
    int64_t next_val = seq.last_value + seq.increment_by;

    // Check bounds
    if (seq.increment_by > 0 && next_val > seq.max_value) {
        if (seq.cycle) {
            next_val = seq.min_value;
        } else {
            SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Sequence exceeded maximum value");
            return -1;
        }
    }

    // Update sequence
    seq.last_value = next_val;
    Status status = updateSequence(seq, ctx);
    if (status != Status::OK) {
        return -1;
    }

    return next_val;
}
```

---

### P0-8: Charset/Collation Read Operations

**Severity:** MEDIUM-HIGH (Functionality)
**Effort:** 3-4 hours
**Component:** Catalog Manager

#### Implementation Plan

Implement missing CRUD operations in `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp`:

```cpp
std::optional<CharsetInfo> CatalogManager::getCharset(const std::string& name,
                                                       ErrorContext* ctx) {
    // Read from pg_charsets catalog table
    // Return CharsetInfo or nullopt
}

std::optional<CollationInfo> CatalogManager::getCollation(const std::string& name,
                                                           ErrorContext* ctx) {
    // Read from pg_collations catalog table
    // Return CollationInfo or nullopt
}

std::vector<CharsetInfo> CatalogManager::listCharsets(ErrorContext* ctx) {
    // Scan pg_charsets table
    // Return all charsets
}

std::vector<CollationInfo> CatalogManager::listCollations(ErrorContext* ctx) {
    // Scan pg_collations table
    // Return all collations
}
```

---

## TESTING STRATEGY

### Unit Tests
- Password policy validation (all rules)
- Login attempt tracking (lockout, exponential backoff)
- Arithmetic overflow (all operations)
- NaN/Infinity handling
- Sequence operations (increment, bounds, cycle)

### Integration Tests
- End-to-end authentication with lockout
- Audit log queries
- IDENTITY column usage with sequences
- GIN parallel queries with concurrent transactions

### Security Tests
- Brute force attack simulation
- Common password rejection
- Audit log tampering resistance

---

## COMPLETION CRITERIA

All P0 items must meet these criteria before Alpha 1 completion:

1. ✅ All unit tests passing
2. ✅ All integration tests passing
3. ✅ No security vulnerabilities (CWE checks)
4. ✅ No undefined behavior (overflow, NaN)
5. ✅ No stubbed functions in critical paths
6. ✅ Audit logging operational for all security events
7. ✅ Account lockout working correctly
8. ✅ Password policy enforced on all user creation/modification

---

## DEPENDENCIES & BLOCKERS

**None** - All P0 items can be implemented immediately with existing codebase.

---

## EXECUTION TIMELINE

### Week 1 (Agent 1)
- Days 1-2: P0-1 Password Policy
- Days 3-4: P0-2 Login Tracking
- Day 5: Start P0-3 Audit Logging

### Week 1 (Agent 2)
- Day 1: P0-4 Arithmetic Overflow
- Day 2: P0-5 NaN/Infinity + P0-6 GIN Bug
- Days 3-4: P0-7 Catalog Sequences
- Day 5: P0-8 Charset/Collation

### Week 2 (Both Agents)
- Days 1-3: Complete P0-3 Audit Logging
- Days 4-5: Integration testing, bug fixes

**Total Estimated Time:** 10 days (2 weeks) with 2 agents in parallel

---

**Document Status:** READY FOR IMPLEMENTATION
**Last Updated:** November 23, 2025
