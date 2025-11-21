# Alpha Advanced Security Implementation Plan
**Date:** November 10, 2025 (Updated with SQL Object Permissions)
**Phase:** Security System Phase 3 - Advanced Features
**Status:** Planning Complete - Ready for Implementation

---

## Overview

This document provides a logical implementation order for advanced security features required for the Alpha release. The order is based on dependencies, complexity, and incremental value delivery.

**MAJOR UPDATES (Nov 10, 2025):**
1. ✅ Added **Query Plan Security Integration** (Phase 3.0) - 10-100x performance improvement
2. ✅ Added **Group Membership Caching** - Critical for complete permission checking
3. ✅ Added **SQL Object Permissions** (Phase 3.1) - Definer rights, ownership chaining
4. 📝 See `/docs/planning/QUERY_PLAN_SECURITY_INTEGRATION.md` for query plan design
5. 📝 See `/docs/planning/SQL_OBJECT_PERMISSIONS_DESIGN.md` for object permissions design

**Total Timeline:** 50-73 hours for critical path (was 30-43 hours)
- Query Plan Security adds 11-17 hours but provides 10-100x speedup
- SQL Object Permissions adds 14-21 hours but is **required for stored procedures**

---

## Implementation Order

> **ARCHITECTURAL REVISION (Nov 10, 2025):**
> Based on analysis, implementing **Query Plan Security Integration** provides superior performance and architecture.
> See `/docs/planning/QUERY_PLAN_SECURITY_INTEGRATION.md` for detailed design.

### **Phase 3.0: Query Plan Security Framework** (NEW - HIGHEST PRIORITY!)
**Duration:** 11-17 hours
**Rationale:** Move security checks to query planning phase for 10-100x performance improvement

#### 3.0.1: Security Analyzer Component (4-5 hours)
**Priority:** CRITICAL - Foundation for all security features
**Complexity:** MEDIUM

**Implementation:**
- Create `SecurityAnalyzer` class in parser layer
- Analyze queries at planning time (before bytecode generation)
- Cache user's roles and groups for entire query
- Validate all permissions ONCE during planning (not per-row)

**Benefits:**
- ✅ Fail fast - permission errors before execution
- ✅ 10-100x faster than runtime checks
- ✅ RLS policies compiled once (not per-row)
- ✅ Column filtering determined at plan time

**Files to Create:**
- `include/scratchbird/parser/security_analyzer.h` (~100 lines)
- `src/parser/security_analyzer.cpp` (~300 lines)

---

#### 3.0.2: Group Membership Caching (2-3 hours)
**Priority:** CRITICAL - Required for complete permission checking
**Complexity:** LOW

**Implementation:**
```cpp
// In ConnectionContext
class ConnectionContext {
private:
    std::vector<ID> cached_role_ids_;
    std::vector<ID> cached_group_ids_;
    bool security_cache_loaded_ = false;

public:
    const std::vector<ID>& getUserRoles();
    const std::vector<ID>& getUserGroups();  // NEW!
};
```

**Why Critical:**
- Groups used in permission checks (USER, ROLE, GROUP, PUBLIC)
- Without caching: catalog lookup for every permission check
- With caching: O(1) lookup from vector

**Files to Modify:**
- `include/scratchbird/core/connection_context.h` (+25 lines)
- `src/core/connection_context.cpp` (+60 lines)
- `include/scratchbird/core/catalog_manager.h` - Add `getUserGroups()` (+5 lines)
- `src/core/catalog_manager.cpp` - Implement `getUserGroups()` (+40 lines)

---

#### 3.0.3: Security Plan Encoding (2-3 hours)
**Priority:** HIGH
**Complexity:** MEDIUM

**Implementation:**
- Define `SecurityPlan` structure
- Encode SecurityPlan into bytecode
- Decode SecurityPlan in Executor
- Pre-compile RLS policies during planning

**Files to Modify:**
- `include/scratchbird/sblr/opcodes.h` - Add SECURITY_PLAN opcode (+5 lines)
- `src/sblr/bytecode_generator.cpp` - Encode SecurityPlan (+80 lines)
- `src/sblr/executor.cpp` - Decode SecurityPlan (+40 lines)

---

#### 3.0.4: Executor Integration (3-4 hours)
**Priority:** HIGH
**Complexity:** MEDIUM

**Changes:**
- Remove per-row permission checks
- Use pre-validated SecurityPlan from bytecode
- Apply pre-compiled RLS policies
- Use pre-filtered column lists

**Performance Impact:**
- Current: 60,001 security operations for 10K row query
- New: 10,008 security operations (75x faster!)

**Files to Modify:**
- `src/sblr/executor.cpp` - Use SecurityPlan (~120 lines modified)

---

---

### **Phase 3.1: SQL Object Permissions (CRITICAL - DO SECOND!)**
**Duration:** 14-21 hours
**Rationale:** Required for stored procedures, views, and triggers; enables ownership chaining

> **See:** `/docs/planning/SQL_OBJECT_PERMISSIONS_DESIGN.md` for complete design

#### 3.1.1: Catalog Schema for Object Permissions (2-3 hours)
**Priority:** CRITICAL - Foundation for definer rights
**Complexity:** LOW

**Implementation:**
1. Extend `pg_procedures`, `pg_functions`, `pg_views` with:
   - `security_type` (DEFINER=0, INVOKER=1)
   - `owner_id` (UUID of creator/definer)

2. Create `pg_object_permissions` table:
   - Allows granting permissions TO procedures/functions/views/triggers
   - Extends `grantee_type` enum: PROCEDURE=5, FUNCTION=6, VIEW=7, TRIGGER=8

**SQL Example:**
```sql
-- Grant table permission to procedure
GRANT SELECT ON TABLE employees TO PROCEDURE get_employee_salary;

-- User only needs EXECUTE permission
GRANT EXECUTE ON PROCEDURE get_employee_salary TO alice;
```

**Files to Modify:**
- `include/scratchbird/core/catalog_manager.h` (+40 lines)
- `src/core/catalog_manager.cpp` - Bootstrap new columns and table (+80 lines)

---

#### 3.1.2: Security Context Stack (3-4 hours)
**Priority:** CRITICAL - Core mechanism for ownership chaining
**Complexity:** MEDIUM

**Implementation:**
```cpp
class ConnectionContext {
    struct SecurityContext {
        SecurityContextType type;  // PROCEDURE, FUNCTION, VIEW, TRIGGER
        ID object_id;              // SQL object UUID
        ID owner_id;               // Definer (owner) UUID
        uint8_t security_type;     // DEFINER or INVOKER
    };

    std::vector<SecurityContext> security_context_stack_;

    void pushSecurityContext(...);   // Enter SQL object
    void popSecurityContext();       // Exit SQL object
    const ID& getEffectiveUserId();  // Returns owner_id if DEFINER, current_user_id if INVOKER
};
```

**Key Concept:** Permission checks use **effective user ID** from top of stack

**Files to Modify:**
- `include/scratchbird/core/connection_context.h` (+50 lines)
- `src/core/connection_context.cpp` (+100 lines)

---

#### 3.1.3: Permission Checking with Object Context (2-3 hours)
**Priority:** CRITICAL
**Complexity:** MEDIUM

**Permission Check Priority:**
```
1. Superuser bypass
2. If in SQL object context:
   a. Check SQL object's direct permissions
      → GRANT SELECT ON table TO PROCEDURE proc
   b. If SECURITY DEFINER:
      → Check object owner's permissions
   c. If SECURITY INVOKER:
      → Fall through to user permissions
3. Check user's permissions
4. Check role permissions
5. Check group permissions
6. Check PUBLIC permissions
7. DENY
```

**Files to Modify:**
- `src/parser/security_analyzer.cpp` - Update permission checking (+80 lines)
- `src/sblr/executor.cpp` - Update checkPermission() (+40 lines)

---

#### 3.1.4: Executor Integration (3-5 hours)
**Priority:** CRITICAL
**Complexity:** MEDIUM

**Changes:**
- Procedure execution: push/pop security context
- Function execution: push/pop security context
- View execution: push/pop security context
- Exception safety: ensure context popped on errors

**Example:**
```cpp
void Executor::executeCallProcedure() {
    // Look up procedure
    ProcedureInfo proc_info = ...;

    // Check EXECUTE permission
    if (!checkPermission(proc_info.procedure_id, Privilege::EXECUTE)) {
        error("Permission denied: EXECUTE on procedure");
    }

    // PUSH security context
    conn_ctx_->pushSecurityContext(
        SecurityContextType::PROCEDURE,
        proc_info.procedure_id,
        proc_info.owner_id,
        proc_info.security_type);

    try {
        // Execute procedure body
        executePSQLBytecode(proc_info.body_bytecode);
        conn_ctx_->popSecurityContext();
    }
    catch (...) {
        conn_ctx_->popSecurityContext();  // Ensure cleanup
        throw;
    }
}
```

**Files to Modify:**
- `src/sblr/executor.cpp` - Update procedure/function/view execution (+150 lines)

---

#### 3.1.5: SQL Parser for SECURITY Clause (2-3 hours)
**Priority:** HIGH
**Complexity:** LOW

**New Syntax:**
```sql
-- Create procedure with definer rights (default)
CREATE PROCEDURE get_salary(emp_id INT)
    SQL SECURITY DEFINER
AS BEGIN
    SELECT salary FROM employees WHERE id = emp_id;
END;

-- Create procedure with invoker rights
CREATE PROCEDURE get_my_salary()
    SQL SECURITY INVOKER
AS BEGIN
    SELECT salary FROM employees WHERE id = CURRENT_USER_ID();
END;

-- Grant to SQL objects
GRANT SELECT ON TABLE employees TO PROCEDURE get_employee_salary;
GRANT INSERT ON TABLE audit_log TO FUNCTION log_access;
```

**Files to Modify:**
- `src/parser/parser.cpp` - Parse SQL SECURITY clause (+60 lines)
- `src/parser/parser.cpp` - Parse GRANT TO PROCEDURE/FUNCTION/VIEW (+40 lines)
- `include/scratchbird/parser/ast.h` - Add security fields to AST nodes (+15 lines)

---

#### 3.1.6: Bytecode Generation (2-3 hours)
**Priority:** HIGH
**Complexity:** LOW

**Changes:**
- Encode `security_type` in procedure/function/view definitions
- Encode object grants in GRANT/REVOKE bytecode
- Include `owner_id` in procedure metadata

**Files to Modify:**
- `src/sblr/bytecode_generator.cpp` - Encode security metadata (+60 lines)
- `include/scratchbird/sblr/opcodes.h` - Extend opcodes if needed (+5 lines)

---

### **Phase 3.2: Performance Foundations** (OPTIONAL - Lower Priority Now)
**Duration:** 3-5 hours (reduced from 5-8 hours)
**Rationale:** Query plan integration handles most caching; this is just cleanup

#### 3.4.1: Permission Result Caching (OPTIONAL - 3-5 hours)
**Priority:** LOW - Plan-time checks make this less critical
**Complexity:** LOW

**Implementation:**
```cpp
// In ConnectionContext
class ConnectionContext {
private:
    // Cache user's role memberships (loaded once per transaction)
    std::vector<ID> cached_role_ids_;
    bool roles_cached_ = false;

public:
    // Load roles once, reuse for entire transaction
    const std::vector<ID>& getUserRoles();
};
```

**Why First:**
- Prerequisite for role permission checking
- Dramatically improves SET ROLE performance
- Simple to implement (just a cache wrapper)
- Zero breaking changes

**Files to Modify:**
- `include/scratchbird/core/connection_context.h` (+10 lines)
- `src/core/connection_context.cpp` (+25 lines)
- Clear cache on GRANT ROLE / REVOKE ROLE

---

#### 3.1.2: Permission Result Caching (3-5 hours)
**Priority:** HIGH - Improves all permission checks
**Complexity:** MEDIUM

**Implementation:**
```cpp
// In ConnectionContext
class ConnectionContext {
private:
    // Cache permission check results
    struct PermissionCacheKey {
        ID object_id;
        uint8_t object_type;
        uint32_t privilege;

        bool operator==(const PermissionCacheKey&) const;
    };

    struct PermissionCacheKeyHash {
        size_t operator()(const PermissionCacheKey& k) const;
    };

    std::unordered_map<PermissionCacheKey, bool, PermissionCacheKeyHash> permission_cache_;

public:
    void invalidatePermissionCache();  // Called on GRANT/REVOKE
};
```

**Why Second:**
- Speeds up repeated permission checks (e.g., multiple rows in same table)
- Simple LRU or TTL-based invalidation
- Can limit cache size (e.g., 1000 entries max)

**Files to Modify:**
- `include/scratchbird/core/connection_context.h` (+30 lines)
- `src/core/connection_context.cpp` (+60 lines)
- `src/sblr/executor.cpp` - Update `checkPermission()` to use cache (+15 lines)

---

#### 3.1.3: Bulk Permission Checks (Optional, 2-3 hours)
**Priority:** MEDIUM - Nice optimization but not critical
**Complexity:** LOW

**Implementation:**
```cpp
// In CatalogManager
auto checkPermissionBulk(
    const std::vector<ID>& object_ids,
    PermissionObjectType object_type,
    const ID& user_id,
    const ID& active_role_id,
    Privilege privilege,
    std::vector<bool>& results_out,
    ErrorContext* ctx = nullptr) -> Status;
```

**Why Third:**
- Only beneficial for bulk operations (e.g., SELECT from 100 tables)
- Can be deferred if time is limited
- Significant optimization for complex queries

**Files to Modify:**
- `include/scratchbird/core/catalog_manager.h` (+5 lines)
- `src/core/catalog_manager.cpp` (+40 lines)
- `src/sblr/executor.cpp` - Optional usage in multi-table queries (+20 lines)

---

### **Phase 3.3: Column-Level Permissions** (High Priority, Medium Complexity)
**Duration:** 10-15 hours
**Rationale:** Simpler than RLS, provides immediate value for sensitive data

#### 3.4.1: Catalog Schema Extension (2-3 hours)
**Priority:** HIGH
**Complexity:** LOW

**New Table: `pg_column_permissions`**
```sql
CREATE TABLE sys.pg_column_permissions (
    permission_id      UUID PRIMARY KEY,        -- UUIDv7
    table_id           UUID NOT NULL,           -- References pg_tables
    column_name        VARCHAR(128) NOT NULL,   -- Column being protected
    grantee_id         UUID NOT NULL,           -- User, Role, Group, or PUBLIC
    grantee_type       UINT8 NOT NULL,          -- USER=1, ROLE=2, GROUP=3, PUBLIC=4
    privileges         UINT32 NOT NULL,         -- Bitmask: SELECT=1, UPDATE=2, INSERT=4, REFERENCES=8
    grantor_id         UUID NOT NULL,           -- User who granted this
    grant_option       BOOLEAN NOT NULL,        -- Can re-grant?
    created_at         TIMESTAMP NOT NULL,

    -- Soft delete (MGA compliance)
    deleted_at         TIMESTAMP,
    deleted_by         UUID,

    INDEX idx_column_perms_table (table_id),
    INDEX idx_column_perms_grantee (grantee_id),
    INDEX idx_column_perms_lookup (table_id, column_name, grantee_id)
);
```

**Files to Create/Modify:**
- `include/scratchbird/core/catalog_manager.h` - Add ColumnPermissionInfo struct (+20 lines)
- `src/core/catalog_manager.cpp` - Bootstrap table (+30 lines)

---

#### 3.4.2: Column Permission CRUD Operations (3-4 hours)
**Priority:** HIGH
**Complexity:** MEDIUM

**New CatalogManager Methods:**
```cpp
// Grant column-level permission
auto grantColumnPermission(
    const ID& table_id,
    const std::string& column_name,
    const ID& grantee_id,
    GranteeType grantee_type,
    uint32_t privileges,
    bool grant_option,
    const ID& grantor_id,
    ErrorContext* ctx = nullptr) -> Status;

// Revoke column-level permission
auto revokeColumnPermission(
    const ID& table_id,
    const std::string& column_name,
    const ID& grantee_id,
    GranteeType grantee_type,
    uint32_t privileges,
    ErrorContext* ctx = nullptr) -> Status;

// Check if user has permission on specific column
auto hasColumnPermission(
    const ID& user_id,
    const ID& table_id,
    const std::string& column_name,
    Privilege privilege,
    bool& has_perm_out,
    ErrorContext* ctx = nullptr) -> Status;

// Get all columns user can access on a table
auto getAccessibleColumns(
    const ID& user_id,
    const ID& table_id,
    Privilege privilege,
    std::vector<std::string>& columns_out,
    ErrorContext* ctx = nullptr) -> Status;
```

**Files to Modify:**
- `include/scratchbird/core/catalog_manager.h` (+30 lines)
- `src/core/catalog_manager.cpp` (+150 lines)

---

#### 3.4.3: SQL Parser Extensions (2-3 hours)
**Priority:** HIGH
**Complexity:** LOW

**New SQL Syntax:**
```sql
-- Grant column-level SELECT
GRANT SELECT (salary, bonus) ON TABLE employees TO alice;

-- Grant multiple privileges on columns
GRANT SELECT, UPDATE (address, phone) ON TABLE customers TO support_role;

-- Revoke column permissions
REVOKE UPDATE (salary) ON TABLE employees FROM bob;
```

**Files to Modify:**
- `src/parser/parser.cpp` - Parse column list in GRANT/REVOKE (+80 lines)
- `include/scratchbird/parser/ast.h` - Add column_names to GrantStmt/RevokeStmt (+5 lines)

---

#### 3.4.4: Bytecode & Executor Integration (3-5 hours)
**Priority:** HIGH
**Complexity:** MEDIUM

**Changes:**
1. Extend GRANT/REVOKE bytecode to include column lists
2. Update `executeGrant()` / `executeRevoke()` to handle columns
3. **Critical:** Update `executeSelect()` to filter columns based on permissions

**Column Filtering Algorithm:**
```cpp
void Executor::executeSelect() {
    // ... existing code ...

    // Get table info
    TableInfo table_info = /* ... */;

    // Get accessible columns for current user
    std::vector<std::string> accessible_cols;
    db_->catalog_manager()->getAccessibleColumns(
        getCurrentUserID(), table_info.table_id,
        Privilege::SELECT, accessible_cols, &err_ctx);

    // Filter SELECT list to only accessible columns
    for (const auto& col : requested_columns) {
        if (std::find(accessible_cols.begin(), accessible_cols.end(), col) == accessible_cols.end()) {
            error("Permission denied: SELECT on column " + col);
        }
    }

    // If SELECT *, use accessible_cols instead of all columns
    if (select_star) {
        requested_columns = accessible_cols;
    }
}
```

**Files to Modify:**
- `include/scratchbird/sblr/opcodes.h` - Extend GRANT/REVOKE opcodes (+2 lines)
- `src/sblr/bytecode_generator.cpp` - Encode column lists (+40 lines)
- `src/sblr/executor.cpp` - Column permission checking (+120 lines)

---

### **Phase 3.4: Row-Level Security (RLS)** (High Priority, High Complexity)
**Duration:** 15-20 hours
**Rationale:** Most powerful feature but complex; build on column permissions

#### 3.4.1: RLS Policy Catalog Schema (3-4 hours)
**Priority:** HIGH
**Complexity:** MEDIUM

**New Table: `pg_row_security_policies`**
```sql
CREATE TABLE sys.pg_row_security_policies (
    policy_id          UUID PRIMARY KEY,        -- UUIDv7
    policy_name        VARCHAR(128) NOT NULL,   -- User-friendly name
    table_id           UUID NOT NULL,           -- References pg_tables
    command_type       UINT8 NOT NULL,          -- ALL=0, SELECT=1, INSERT=2, UPDATE=3, DELETE=4
    permissive         BOOLEAN NOT NULL,        -- Permissive (OR) vs Restrictive (AND)
    roles              UUID[] NOT NULL,         -- Applies to these roles (empty = PUBLIC)

    -- USING clause (for SELECT, UPDATE, DELETE)
    using_expression   TEXT,                    -- SQL predicate (e.g., "user_id = current_user_id()")

    -- WITH CHECK clause (for INSERT, UPDATE)
    check_expression   TEXT,                    -- SQL predicate for new/modified rows

    enabled            BOOLEAN NOT NULL DEFAULT TRUE,
    created_at         TIMESTAMP NOT NULL,
    created_by         UUID NOT NULL,

    -- Soft delete
    deleted_at         TIMESTAMP,
    deleted_by         UUID,

    INDEX idx_rls_table (table_id),
    UNIQUE (table_id, policy_name)
);
```

**Files to Create/Modify:**
- `include/scratchbird/core/catalog_manager.h` - Add RLSPolicyInfo struct (+30 lines)
- `src/core/catalog_manager.cpp` - Bootstrap table (+40 lines)

---

#### 3.4.2: RLS Policy CRUD Operations (3-4 hours)
**Priority:** HIGH
**Complexity:** MEDIUM

**New CatalogManager Methods:**
```cpp
struct RLSPolicyInfo {
    ID policy_id;
    std::string policy_name;
    ID table_id;
    uint8_t command_type;  // ALL, SELECT, INSERT, UPDATE, DELETE
    bool permissive;
    std::vector<ID> role_ids;
    std::string using_expression;
    std::string check_expression;
    bool enabled;
};

auto createRLSPolicy(const RLSPolicyInfo& policy, ErrorContext* ctx = nullptr) -> Status;
auto getRLSPolicy(const ID& policy_id, RLSPolicyInfo& policy_out, ErrorContext* ctx = nullptr) -> Status;
auto getRLSPoliciesByTable(const ID& table_id, std::vector<RLSPolicyInfo>& policies_out, ErrorContext* ctx = nullptr) -> Status;
auto updateRLSPolicy(const ID& policy_id, const RLSPolicyInfo& policy, ErrorContext* ctx = nullptr) -> Status;
auto deleteRLSPolicy(const ID& policy_id, ErrorContext* ctx = nullptr) -> Status;
auto enableRLS(const ID& table_id, ErrorContext* ctx = nullptr) -> Status;
auto disableRLS(const ID& table_id, ErrorContext* ctx = nullptr) -> Status;
```

**Files to Modify:**
- `include/scratchbird/core/catalog_manager.h` (+50 lines)
- `src/core/catalog_manager.cpp` (+200 lines)

---

#### 3.4.3: SQL Parser for RLS Policies (2-3 hours)
**Priority:** HIGH
**Complexity:** MEDIUM

**New SQL Syntax:**
```sql
-- Enable RLS on table
ALTER TABLE employees ENABLE ROW LEVEL SECURITY;

-- Create permissive policy
CREATE POLICY employee_access ON employees
    FOR SELECT
    USING (department = current_user_department() OR is_manager());

-- Create restrictive policy
CREATE POLICY manager_only ON employees
    FOR UPDATE
    USING (is_manager())
    WITH CHECK (salary <= old.salary * 1.1);  -- Max 10% raise

-- Policy for specific roles
CREATE POLICY tenant_isolation ON data
    FOR ALL
    TO tenant_user, tenant_admin
    USING (tenant_id = current_tenant_id());

-- Drop policy
DROP POLICY employee_access ON employees;

-- Disable RLS
ALTER TABLE employees DISABLE ROW LEVEL SECURITY;
```

**Files to Modify:**
- `src/parser/parser.cpp` - Parse CREATE/DROP POLICY, ALTER TABLE RLS (+150 lines)
- `include/scratchbird/parser/ast.h` - Add RLS statement nodes (+40 lines)

---

#### 3.4.4: RLS Execution Engine (7-9 hours)
**Priority:** HIGH
**Complexity:** VERY HIGH

**Core Challenge:** Inject WHERE clause predicates into queries

**Implementation Strategy:**

1. **Fetch Applicable Policies** (1 hour)
```cpp
// In Executor::executeSelect()
std::vector<RLSPolicyInfo> policies;
db_->catalog_manager()->getRLSPoliciesByTable(table_id, policies, &err_ctx);

// Filter policies applicable to current user's roles
std::vector<RLSPolicyInfo> applicable;
for (const auto& policy : policies) {
    if (policyAppliesTo(policy, getCurrentUserID(), getUserRoles())) {
        applicable.push_back(policy);
    }
}
```

2. **Compile Policy Expressions** (3-4 hours)
```cpp
// Parse policy USING expressions into AST
auto policy_ast = parser.parseExpression(policy.using_expression);

// Convert to bytecode (reuse expression evaluator)
auto policy_bytecode = generator.generateExpression(policy_ast);
```

3. **Combine Policies** (2-3 hours)
```cpp
// Combine permissive policies with OR
// Combine restrictive policies with AND
// Final predicate: (permissive1 OR permissive2 OR ...) AND (restrictive1 AND restrictive2 AND ...)

std::vector<Expression> permissive_exprs;
std::vector<Expression> restrictive_exprs;

for (const auto& policy : applicable) {
    auto expr = compilePolicy(policy.using_expression);
    if (policy.permissive) {
        permissive_exprs.push_back(expr);
    } else {
        restrictive_exprs.push_back(expr);
    }
}

Expression combined = combineWithOr(permissive_exprs);
combined = combineWithAnd(combined, restrictive_exprs);
```

4. **Inject into Query Execution** (1-2 hours)
```cpp
// Add RLS predicate to WHERE clause
if (has_rls_policies) {
    // Evaluate RLS predicate for each row
    for (each row) {
        bool visible = evaluateExpression(rls_predicate, row);
        if (!visible) continue;  // Skip this row

        // ... normal processing ...
    }
}
```

**Files to Modify:**
- `src/sblr/executor.cpp` - RLS predicate evaluation (+250 lines)
- `src/sblr/bytecode_generator.cpp` - Policy compilation (+80 lines)
- `include/scratchbird/sblr/executor.h` - Helper methods (+30 lines)

**Performance Consideration:**
- RLS predicates evaluated per-row (can be expensive)
- Consider compiling policies to native code (future optimization)
- Use indexes when RLS predicate matches indexed columns

---

### **Phase 3.5: Policy-Based Access Control** (Medium Priority, Medium Complexity)
**Duration:** 8-12 hours
**Rationale:** Advanced features for enterprise use cases

#### 3.5.1: Time-Based Permissions (3-4 hours)
**Priority:** MEDIUM
**Complexity:** LOW

**Catalog Extension:**
```sql
ALTER TABLE sys.pg_permissions ADD COLUMN valid_from TIMESTAMP;
ALTER TABLE sys.pg_permissions ADD COLUMN valid_until TIMESTAMP;
```

**Implementation:**
```cpp
// In checkPermission()
if (permission.valid_from.has_value() && current_time < permission.valid_from) {
    return false;  // Not yet valid
}
if (permission.valid_until.has_value() && current_time > permission.valid_until) {
    return false;  // Expired
}
```

**SQL Syntax:**
```sql
GRANT SELECT ON TABLE sales TO analyst
    VALID FROM '2025-01-01' UNTIL '2025-12-31';
```

**Files to Modify:**
- `include/scratchbird/core/catalog_manager.h` - Add time fields (+5 lines)
- `src/core/catalog_manager.cpp` - Time validation (+20 lines)
- `src/parser/parser.cpp` - Parse VALID FROM/UNTIL (+40 lines)
- `src/sblr/executor.cpp` - Time checking in permission grants (+15 lines)

---

#### 3.5.2: IP-Based Restrictions (2-3 hours)
**Priority:** LOW
**Complexity:** LOW

**ConnectionContext Extension:**
```cpp
class ConnectionContext {
private:
    std::string client_ip_address_;

public:
    void setClientIP(const std::string& ip);
    const std::string& getClientIP() const;
};
```

**Catalog Extension:**
```sql
ALTER TABLE sys.pg_users ADD COLUMN allowed_ips INET[];
```

**Validation:**
```cpp
// During authentication
auto user_info = getUserByName(username);
if (!user_info.allowed_ips.empty()) {
    if (!isIPAllowed(client_ip, user_info.allowed_ips)) {
        return Status::PERMISSION_DENIED;
    }
}
```

**Files to Modify:**
- `include/scratchbird/core/connection_context.h` (+5 lines)
- `src/core/connection_context.cpp` (+10 lines)
- `include/scratchbird/core/catalog_manager.h` - Add IP list field (+2 lines)
- `src/core/catalog_manager.cpp` - IP validation (+30 lines)

---

#### 3.5.3: Custom Policy Functions (3-5 hours)
**Priority:** MEDIUM
**Complexity:** MEDIUM

**Concept:** Allow users to register C++ functions as policy evaluators

**Example:**
```sql
-- Create custom policy function
CREATE FUNCTION is_business_hours() RETURNS BOOLEAN
    LANGUAGE INTERNAL AS 'check_business_hours';

-- Use in RLS policy
CREATE POLICY business_hours_only ON sensitive_data
    USING (is_business_hours());
```

**Implementation:**
```cpp
// Policy function registry
class PolicyFunctionRegistry {
    using PolicyFunc = std::function<bool(const ExecutionContext&)>;
    std::unordered_map<std::string, PolicyFunc> functions_;

public:
    void registerFunction(const std::string& name, PolicyFunc func);
    bool evaluateFunction(const std::string& name, const ExecutionContext& ctx);
};

// Example built-in functions
registry.registerFunction("is_business_hours", [](const auto& ctx) {
    auto now = std::chrono::system_clock::now();
    auto hour = /* extract hour */;
    return hour >= 9 && hour < 17;  // 9 AM - 5 PM
});

registry.registerFunction("current_user_department", [](const auto& ctx) {
    return ctx.conn_ctx->getCurrentUserDepartment();
});
```

**Files to Create:**
- `include/scratchbird/core/policy_functions.h` (+50 lines)
- `src/core/policy_functions.cpp` (+100 lines)

**Files to Modify:**
- `src/sblr/executor.cpp` - Call policy functions during RLS evaluation (+40 lines)

---

## Summary Timeline

### Critical Path (Required for Alpha)

| Phase | Feature | Duration | Cumulative |
|-------|---------|----------|------------|
| **Phase 3.0: Query Plan Security** | | **11-17 hours** | **11-17 hours** |
| 3.0.1 | Security Analyzer Component | 4-5 hours | 4-5 hours |
| 3.0.2 | Group Membership Caching | 2-3 hours | 6-8 hours |
| 3.0.3 | Security Plan Encoding | 2-3 hours | 8-11 hours |
| 3.0.4 | Executor Integration | 3-4 hours | 11-15 hours |
| **Phase 3.1: SQL Object Permissions** | | **14-21 hours** | **25-38 hours** |
| 3.1.1 | Catalog Schema for Object Perms | 2-3 hours | 13-18 hours |
| 3.1.2 | Security Context Stack | 3-4 hours | 16-22 hours |
| 3.1.3 | Permission Checking Integration | 2-3 hours | 18-25 hours |
| 3.1.4 | Executor Integration | 3-5 hours | 21-30 hours |
| 3.1.5 | SQL Parser for SECURITY | 2-3 hours | 23-33 hours |
| 3.1.6 | Bytecode Generation | 2-3 hours | 25-36 hours |
| **Phase 3.3: Column Permissions** | | **10-15 hours** | **35-53 hours** |
| 3.3.1 | Column Permission Schema | 2-3 hours | 27-39 hours |
| 3.3.2 | Column Permission CRUD | 3-4 hours | 30-43 hours |
| 3.3.3 | Column SQL Parser | 2-3 hours | 32-46 hours |
| 3.3.4 | Column Executor Integration | 3-5 hours | 35-51 hours |
| **Phase 3.4: Row-Level Security** | | **15-20 hours** | **50-73 hours** |
| 3.4.1 | RLS Catalog Schema | 3-4 hours | 38-55 hours |
| 3.4.2 | RLS Policy CRUD | 3-4 hours | 41-59 hours |
| 3.4.3 | RLS SQL Parser | 2-3 hours | 43-62 hours |
| 3.4.4 | RLS Execution Engine | 7-9 hours | 50-71 hours |

**Total Critical Path:** 50-73 hours (6-9 days with 1 developer, 3-5 days with 2 developers)

### Optional Enhancements

| Phase | Feature | Duration |
|-------|---------|----------|
| 3.2.1 | Permission Result Caching | 3-5 hours |
| 3.2.2 | Bulk Permission Checks | 2-3 hours |
| 3.5.1 | Time-Based Permissions | 3-4 hours |
| 3.5.2 | IP-Based Restrictions | 2-3 hours |
| 3.5.3 | Custom Policy Functions | 3-5 hours |

**Total Optional:** 13-20 hours (2-3 days)

---

## Recommended Implementation Order

### Week 1: Query Plan Security & SQL Object Permissions
**Days 1-2:**
1. ✅ Security Analyzer Component (3.0.1) - 4-5 hours
2. ✅ Group Membership Caching (3.0.2) - 2-3 hours
3. ✅ Security Plan Encoding (3.0.3) - 2-3 hours

**Days 3-5:**
4. ✅ Executor Integration (3.0.4) - 3-4 hours
5. ✅ SQL Object Permissions Catalog (3.1.1) - 2-3 hours
6. ✅ Security Context Stack (3.1.2) - 3-4 hours
7. Testing and bug fixes

### Week 2: Complete SQL Object Permissions & Start Column Permissions
**Days 1-2:**
8. ✅ Permission Checking Integration (3.1.3) - 2-3 hours
9. ✅ SQL Object Executor Integration (3.1.4) - 3-5 hours
10. ✅ SQL Parser for SECURITY (3.1.5) - 2-3 hours
11. ✅ Bytecode Generation (3.1.6) - 2-3 hours

**Days 3-5:**
12. ✅ Column Permission Schema (3.3.1) - 2-3 hours
13. ✅ Column Permission CRUD (3.3.2) - 3-4 hours
14. ✅ Column SQL Parser (3.3.3) - 2-3 hours
15. Start Column Executor Integration (3.3.4)

### Week 3: Row-Level Security
**Days 1-3:**
16. Complete Column Executor Integration (3.3.4) - 3-5 hours
17. RLS catalog and CRUD (3.4.1, 3.4.2) - 6-8 hours
18. RLS SQL parser (3.4.3) - 2-3 hours

**Days 4-5:**
19. RLS execution engine (3.4.4) - COMPLEX - 7-9 hours
20. Integration testing

### Week 4 (Optional): Policy-Based Access Control & Polish
**Days 1-2:**
21. Time-based permissions (3.5.1) - 3-4 hours
22. IP restrictions (3.5.2) - 2-3 hours
23. Custom policy functions (3.5.3) - 3-5 hours

**Days 3-5:**
24. Comprehensive testing
25. Performance tuning
26. Documentation

---

## Risk Assessment

### High Risk Items

1. **RLS Execution Engine (3.3.4)** - HIGHEST RISK
   - **Risk:** Complex predicate injection, performance impact
   - **Mitigation:** Start with simple policies, add complexity incrementally
   - **Fallback:** Implement basic RLS first, defer advanced features

2. **Column Permission Filtering**
   - **Risk:** Breaking existing queries, SELECT * handling
   - **Mitigation:** Extensive testing, backward compatibility checks
   - **Fallback:** Make column permissions opt-in per table

3. **Performance Impact**
   - **Risk:** RLS per-row evaluation can be slow
   - **Mitigation:** Caching, index-aware policy compilation
   - **Fallback:** Allow disabling RLS per query (`SET row_security = OFF`)

### Medium Risk Items

1. **Policy Expression Compilation**
   - Need to reuse existing expression evaluator
   - May need to extend for special functions (current_user_id, etc.)

2. **Permission Cache Invalidation**
   - Must invalidate on GRANT/REVOKE
   - Consider distributed systems (future)

### Low Risk Items

1. Time-based permissions - straightforward timestamp comparison
2. IP restrictions - simple string matching
3. Caching - well-understood patterns

---

## Testing Strategy

### Unit Tests (Per Feature)

1. **Caching Tests**
   - Cache hit/miss ratios
   - Cache invalidation correctness
   - Memory usage limits

2. **Column Permission Tests**
   - Grant/revoke operations
   - SELECT filtering
   - UPDATE validation
   - Error messages

3. **RLS Tests**
   - Policy creation/deletion
   - Permissive vs restrictive combination
   - Role-specific policies
   - Performance benchmarks

### Integration Tests

1. **Multi-tenant Scenario**
   - Create 3 tenants with RLS policies
   - Verify complete isolation
   - Test cross-tenant queries (should fail)

2. **Complex Permission Hierarchy**
   - User + Role + Group + Column + Row permissions
   - Verify correct precedence
   - Test permission changes

3. **Performance Tests**
   - 1M rows with RLS policies
   - Measure query latency
   - Compare with/without RLS

---

## Success Criteria

### Phase 3.1 (Caching)
- ✅ Cache hit rate > 80% for repeated permission checks
- ✅ Role lookup reduced from O(N) to O(1) per transaction
- ✅ No memory leaks in cache implementation

### Phase 3.2 (Column Permissions)
- ✅ Users can only see permitted columns
- ✅ SELECT * respects column permissions
- ✅ Backward compatible with existing queries

### Phase 3.3 (RLS)
- ✅ Policies correctly filter rows
- ✅ Performance degradation < 20% with simple policies
- ✅ Multi-tenant isolation verified

### Phase 3.4 (Policy-Based)
- ✅ Time-based permissions enforce validity windows
- ✅ IP restrictions block unauthorized connections
- ✅ Custom functions integrate seamlessly

---

## Dependencies

### External Dependencies
- None - all features use existing infrastructure

### Internal Dependencies
```
Caching (3.1)
  ↓
Column Permissions (3.2)
  ↓
RLS (3.3) ← Can be implemented in parallel with 3.4
  ↓
Policy-Based (3.4)
```

**Critical Path:** Must complete in order 3.1 → 3.2 → 3.3
**Parallel Work:** 3.4 can be done alongside 3.3 if resources available

---

## Open Questions

1. **RLS Performance Tuning**
   - Q: Should we compile policies to native code for performance?
   - A: Defer to post-alpha, use interpreted evaluation initially

2. **Column Permission Granularity**
   - Q: Support column permissions on JOINs across tables?
   - A: Yes, check permissions on all tables in query

3. **RLS Bypass for Debugging**
   - Q: Allow superusers to bypass RLS for debugging?
   - A: Yes, add `SET row_security = OFF` (superuser only)

4. **Policy Function Security**
   - Q: How to prevent malicious policy functions?
   - A: Only allow built-in functions initially, sandbox custom functions later

---

## Documentation Deliverables

1. **User Guide Updates**
   - Column permission examples
   - RLS policy cookbook
   - Performance tuning guide

2. **API Documentation**
   - CatalogManager new methods
   - Policy function API

3. **Migration Guide**
   - Upgrading from Phase 2 to Phase 3
   - Breaking changes (if any)

---

**Document Version:** 1.0
**Last Updated:** November 10, 2025
**Status:** Planning Complete - Ready for Implementation
**Estimated Total Time:** 40-58 hours (critical + optional)
