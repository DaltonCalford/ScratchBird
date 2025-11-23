# SQL Object Permissions & Ownership Chains
**Date:** November 10, 2025
**Status:** Design Document
**Priority:** CRITICAL - Required for stored procedures/functions

---

## Problem Statement

**Current Security Model:**
- Permissions granted to: USER, ROLE, GROUP, PUBLIC
- Missing: Permissions granted to SQL objects (procedures, functions, views)

**User Request:**
```sql
-- Grant table permission to a procedure
GRANT SELECT ON TABLE employees TO PROCEDURE get_employee_salary;

-- User only needs permission to execute the procedure
GRANT EXECUTE ON PROCEDURE get_employee_salary TO alice;

-- Alice can now call the procedure, which internally queries employees table
-- Alice does NOT need direct SELECT permission on employees!
```

**This is called:** Ownership chaining, definer rights, or SQL object security context

---

## SQL Standard: SECURITY Clause

### SQL:2016 Standard

**Procedure/Function Definition:**
```sql
CREATE PROCEDURE get_employee_salary(emp_id INT)
    LANGUAGE SQL
    SQL SECURITY DEFINER  -- Execute with definer's privileges (default)
    -- or --
    SQL SECURITY INVOKER  -- Execute with caller's privileges
BEGIN
    SELECT salary FROM employees WHERE id = emp_id;
END;
```

**View Definition:**
```sql
CREATE VIEW employee_salaries
    SQL SECURITY DEFINER  -- Query uses definer's privileges
AS SELECT employee_id, name, salary FROM employees;
```

---

## Proposed Design: Two-Level Security Model

### Level 1: SQL Object Context (Definer Rights)

**Execution Context Stack:**
```
User alice executes: CALL get_department_summary(10)
  ↓
Executor pushes security context:
  [User: alice] → [Procedure: get_department_summary (owner: admin)]
    ↓
Procedure calls: SELECT * FROM employees WHERE dept_id = ?
      ↓
    Permission check uses PROCEDURE's owner (admin), not alice!
      ↓
    Procedure calls: CALL calculate_statistics(...)
        ↓
      Pushes another context:
        [Procedure: calculate_statistics (owner: analyst)]
          ↓
        Check uses analyst's permissions
```

**Key Concept:** Security context is a **stack**, not a single value!

---

## Implementation

### Phase 1: Catalog Schema Extensions

#### 1.1: Extend pg_procedures/pg_functions

```sql
ALTER TABLE sys.pg_procedures ADD COLUMN security_type UINT8 NOT NULL DEFAULT 0;
-- 0 = DEFINER (default)
-- 1 = INVOKER

ALTER TABLE sys.pg_procedures ADD COLUMN owner_id UUID NOT NULL;
-- User who created the procedure (definer)

ALTER TABLE sys.pg_functions ADD COLUMN security_type UINT8 NOT NULL DEFAULT 0;
ALTER TABLE sys.pg_functions ADD COLUMN owner_id UUID NOT NULL;
```

#### 1.2: Extend pg_views

```sql
ALTER TABLE sys.pg_views ADD COLUMN security_type UINT8 NOT NULL DEFAULT 0;
ALTER TABLE sys.pg_views ADD COLUMN owner_id UUID NOT NULL;
```

#### 1.3: New Table: pg_object_permissions

```sql
CREATE TABLE sys.pg_object_permissions (
    permission_id      UUID PRIMARY KEY,        -- UUIDv7
    object_id          UUID NOT NULL,           -- Table, View, Sequence, etc.
    object_type        UINT8 NOT NULL,          -- TABLE=1, VIEW=2, SEQUENCE=3, etc.

    -- NEW: Grantee can be a SQL object!
    grantee_id         UUID NOT NULL,           -- User, Role, Group, PUBLIC, or SQL OBJECT
    grantee_type       UINT8 NOT NULL,          -- USER=1, ROLE=2, GROUP=3, PUBLIC=4,
                                                 -- PROCEDURE=5, FUNCTION=6, VIEW=7, TRIGGER=8

    privileges         UINT32 NOT NULL,         -- Bitmask: SELECT=1, INSERT=2, UPDATE=4, etc.
    grantor_id         UUID NOT NULL,           -- User who granted this
    grant_option       BOOLEAN NOT NULL,        -- Can re-grant?
    created_at         TIMESTAMP NOT NULL,

    -- Soft delete (MGA compliance)
    deleted_at         TIMESTAMP,
    deleted_by         UUID,

    INDEX idx_object_perms_object (object_id, object_type),
    INDEX idx_object_perms_grantee (grantee_id, grantee_type),
    INDEX idx_object_perms_lookup (object_id, grantee_id, grantee_type)
);
```

**Key Addition:** `grantee_type` now includes SQL objects (PROCEDURE=5, FUNCTION=6, VIEW=7, TRIGGER=8)

---

### Phase 2: Security Context Stack

#### 2.1: Extend ConnectionContext

```cpp
namespace scratchbird::core {

class ConnectionContext {
public:
    // Security context for SQL objects
    enum class SecurityContextType : uint8_t {
        USER = 0,           // Normal user execution
        PROCEDURE = 1,      // Inside stored procedure
        FUNCTION = 2,       // Inside function
        VIEW = 3,           // Inside view query
        TRIGGER = 4         // Inside trigger
    };

    struct SecurityContext {
        SecurityContextType type;
        ID object_id;           // Procedure/Function/View/Trigger ID
        ID owner_id;            // Definer (owner) of the object
        uint8_t security_type;  // DEFINER=0, INVOKER=1
    };

private:
    // Security context stack (for nested procedure calls)
    std::vector<SecurityContext> security_context_stack_;

public:
    // Push security context when entering SQL object
    void pushSecurityContext(SecurityContextType type,
                            const ID& object_id,
                            const ID& owner_id,
                            uint8_t security_type);

    // Pop security context when exiting SQL object
    void popSecurityContext();

    // Get effective user for permission checks
    // Returns owner_id if in DEFINER context, or current_user_id if in INVOKER context
    const ID& getEffectiveUserId() const;

    // Get current execution context
    const SecurityContext* getCurrentSecurityContext() const;

    // Check if currently executing inside a SQL object
    bool isInSQLObjectContext() const {
        return !security_context_stack_.empty();
    }
};

} // namespace scratchbird::core
```

#### 2.2: Implementation

```cpp
void ConnectionContext::pushSecurityContext(SecurityContextType type,
                                           const ID& object_id,
                                           const ID& owner_id,
                                           uint8_t security_type)
{
    SecurityContext ctx;
    ctx.type = type;
    ctx.object_id = object_id;
    ctx.owner_id = owner_id;
    ctx.security_type = security_type;

    security_context_stack_.push_back(ctx);

    LOG_DEBUG(SECURITY, "Pushed security context: type=%d, object=%s, owner=%s, security_type=%d",
              static_cast<int>(type), object_id.toString().c_str(),
              owner_id.toString().c_str(), security_type);
}

void ConnectionContext::popSecurityContext()
{
    if (security_context_stack_.empty()) {
        LOG_ERROR(SECURITY, "Attempted to pop empty security context stack");
        return;
    }

    auto ctx = security_context_stack_.back();
    security_context_stack_.pop_back();

    LOG_DEBUG(SECURITY, "Popped security context: type=%d, object=%s",
              static_cast<int>(ctx.type), ctx.object_id.toString().c_str());
}

const ID& ConnectionContext::getEffectiveUserId() const
{
    // If not in SQL object context, use current user
    if (security_context_stack_.empty()) {
        return current_user_id_;
    }

    // Get top of security context stack
    const auto& ctx = security_context_stack_.back();

    // DEFINER: Use object owner's ID
    if (ctx.security_type == 0) {
        return ctx.owner_id;
    }

    // INVOKER: Use current user's ID
    return current_user_id_;
}

const ConnectionContext::SecurityContext* ConnectionContext::getCurrentSecurityContext() const
{
    if (security_context_stack_.empty()) {
        return nullptr;
    }
    return &security_context_stack_.back();
}
```

---

### Phase 3: Permission Checking Integration

#### 3.1: Update SecurityAnalyzer

```cpp
class SecurityAnalyzer {
    bool checkPermission(const core::ID& object_id, Privilege privilege) {
        // 1. Get effective user ID (considers security context stack)
        const core::ID& effective_user_id = conn_ctx_->getEffectiveUserId();

        // 2. If in SQL object context, check object permissions first
        if (conn_ctx_->isInSQLObjectContext()) {
            auto* ctx = conn_ctx_->getCurrentSecurityContext();

            // Check if the SQL object has direct permission
            bool has_perm = false;
            catalog_->hasPermission(
                ctx->object_id,  // The procedure/function/view ID
                object_id,       // The table/object being accessed
                PermissionObjectType::TABLE,
                privilege,
                has_perm,
                nullptr);

            if (has_perm) {
                // SQL object has permission - ALLOW!
                LOG_DEBUG(SECURITY, "Permission granted via SQL object context: object=%s, privilege=%d",
                          ctx->object_id.toString().c_str(), static_cast<int>(privilege));
                return true;
            }

            // SQL object doesn't have permission, check owner/definer
            if (ctx->security_type == 0) {  // DEFINER
                // Check definer's permissions
                catalog_->hasPermission(
                    ctx->owner_id,  // Definer's user ID
                    object_id,
                    PermissionObjectType::TABLE,
                    privilege,
                    has_perm,
                    nullptr);

                if (has_perm) {
                    LOG_DEBUG(SECURITY, "Permission granted via definer rights: owner=%s",
                              ctx->owner_id.toString().c_str());
                    return true;
                }

                // Definer doesn't have permission - DENY
                return false;
            }
        }

        // 3. Standard user permission check (includes roles, groups, PUBLIC)
        return checkUserPermission(effective_user_id, object_id, privilege);
    }
};
```

#### 3.2: Permission Check Priority Order

```
When checking permission on object X:

1. Superuser bypass (always allow)
   ↓
2. If in SQL object context:
   a. Check SQL object's direct permissions (e.g., procedure has SELECT on table)
      → If YES: ALLOW
   b. If SQL SECURITY DEFINER:
      → Check definer's permissions (owner of procedure)
      → ALLOW if definer has permission, DENY otherwise
   c. If SQL SECURITY INVOKER:
      → Fall through to step 3
   ↓
3. Check user's direct permissions
   ↓
4. Check user's role permissions
   ↓
5. Check user's group permissions
   ↓
6. Check PUBLIC permissions
   ↓
7. DENY (no permission found)
```

---

### Phase 4: Executor Integration

#### 4.1: Procedure/Function Execution

```cpp
void Executor::executeCallProcedure() {
    // Decode bytecode
    std::string proc_name = readString();
    std::vector<Value> args = readArguments();

    // Look up procedure
    core::CatalogManager::ProcedureInfo proc_info;
    db_->catalog_manager()->getProcedureByName(proc_name, proc_info, &err_ctx);

    // Check EXECUTE permission on procedure
    if (!checkPermission(proc_info.procedure_id, Privilege::EXECUTE)) {
        error("Permission denied: EXECUTE on procedure " + proc_name);
    }

    // PUSH SECURITY CONTEXT
    conn_ctx_->pushSecurityContext(
        ConnectionContext::SecurityContextType::PROCEDURE,
        proc_info.procedure_id,
        proc_info.owner_id,
        proc_info.security_type);

    try {
        // Execute procedure body (PSQL bytecode)
        executePSQLBytecode(proc_info.body_bytecode);

        // POP SECURITY CONTEXT
        conn_ctx_->popSecurityContext();
    }
    catch (...) {
        // Ensure context is popped even on error
        conn_ctx_->popSecurityContext();
        throw;
    }
}
```

#### 4.2: View Execution

```cpp
void Executor::executeSelectFromView() {
    // Look up view definition
    core::CatalogManager::ViewInfo view_info;
    db_->catalog_manager()->getView(view_name, view_info, &err_ctx);

    // Check SELECT permission on view
    if (!checkPermission(view_info.view_id, Privilege::SELECT)) {
        error("Permission denied: SELECT on view " + view_name);
    }

    // PUSH SECURITY CONTEXT for view query
    conn_ctx_->pushSecurityContext(
        ConnectionContext::SecurityContextType::VIEW,
        view_info.view_id,
        view_info.owner_id,
        view_info.security_type);

    try {
        // Execute view query (stored as SQL or bytecode)
        executeViewQuery(view_info.query_bytecode);

        // POP SECURITY CONTEXT
        conn_ctx_->popSecurityContext();
    }
    catch (...) {
        conn_ctx_->popSecurityContext();
        throw;
    }
}
```

---

## SQL Syntax Extensions

### 1. CREATE PROCEDURE/FUNCTION with SECURITY clause

```sql
-- Definer rights (default) - procedure executes with owner's permissions
CREATE PROCEDURE get_employee_salary(emp_id INT)
    LANGUAGE SQL
    SQL SECURITY DEFINER
AS
BEGIN
    SELECT salary FROM employees WHERE id = emp_id;
END;

-- Invoker rights - procedure executes with caller's permissions
CREATE PROCEDURE get_my_salary()
    LANGUAGE SQL
    SQL SECURITY INVOKER
AS
BEGIN
    SELECT salary FROM employees WHERE id = CURRENT_USER_ID();
END;
```

### 2. CREATE VIEW with SECURITY clause

```sql
-- Definer rights view
CREATE VIEW manager_salaries
    SQL SECURITY DEFINER
AS SELECT * FROM employees WHERE is_manager = TRUE;

-- Invoker rights view
CREATE VIEW my_profile
    SQL SECURITY INVOKER
AS SELECT * FROM employees WHERE id = CURRENT_USER_ID();
```

### 3. GRANT permissions to SQL objects

```sql
-- Grant table permission to procedure
GRANT SELECT ON TABLE employees TO PROCEDURE get_employee_salary;

-- Grant multiple permissions to function
GRANT SELECT, INSERT ON TABLE audit_log TO FUNCTION log_access;

-- Grant permission to view (for nested views)
GRANT SELECT ON TABLE sensitive_data TO VIEW public_summary;

-- Grant permission to trigger
GRANT INSERT ON TABLE audit_trail TO TRIGGER employee_audit_trigger;
```

### 4. Query permissions granted to objects

```sql
-- Show what permissions a procedure has
SELECT * FROM information_schema.object_privileges
WHERE grantee_type = 'PROCEDURE' AND grantee_name = 'get_employee_salary';
```

---

## Example Scenarios

### Scenario 1: Secure Payroll Procedure

```sql
-- 1. Admin creates procedure with definer rights
CREATE PROCEDURE get_employee_salary(emp_id INT)
    SQL SECURITY DEFINER  -- Executes with admin's privileges
AS
BEGIN
    -- Admin has SELECT on employees
    SELECT salary FROM employees WHERE id = emp_id;
END;

-- 2. Grant table permission to procedure
GRANT SELECT ON TABLE employees TO PROCEDURE get_employee_salary;

-- 3. Grant EXECUTE to regular user
GRANT EXECUTE ON PROCEDURE get_employee_salary TO alice;

-- 4. Alice executes procedure
CALL get_employee_salary(123);

-- Security check flow:
-- ✓ Alice has EXECUTE on procedure (granted in step 3)
-- ✓ Procedure has SELECT on employees (granted in step 2)
-- ✓ Query succeeds!

-- 5. Alice tries direct access
SELECT salary FROM employees WHERE id = 123;
-- ✗ Permission denied: Alice doesn't have SELECT on employees!
```

### Scenario 2: Nested Procedure Calls

```sql
-- Procedure 1: Internal calculation (owned by analyst)
CREATE PROCEDURE calculate_bonus(emp_id INT)
    SQL SECURITY DEFINER
AS
BEGIN
    SELECT salary * 0.15 FROM employees WHERE id = emp_id;
END;

GRANT SELECT ON TABLE employees TO PROCEDURE calculate_bonus;

-- Procedure 2: Public interface (owned by manager)
CREATE PROCEDURE get_total_compensation(emp_id INT)
    SQL SECURITY DEFINER
AS
BEGIN
    DECLARE bonus DECIMAL;
    CALL calculate_bonus(emp_id) INTO bonus;
    SELECT salary + bonus FROM employees WHERE id = emp_id;
END;

GRANT SELECT ON TABLE employees TO PROCEDURE get_total_compensation;
GRANT EXECUTE ON PROCEDURE calculate_bonus TO PROCEDURE get_total_compensation;

-- User alice executes
GRANT EXECUTE ON PROCEDURE get_total_compensation TO alice;
CALL get_total_compensation(123);

-- Security context stack:
-- [User: alice]
--   → [Procedure: get_total_compensation (owner: manager)]
--       → [Procedure: calculate_bonus (owner: analyst)]
--
-- Each procedure can only access what it has permission for!
```

### Scenario 3: View with Definer Rights

```sql
-- Admin creates view showing all salaries
CREATE VIEW all_salaries
    SQL SECURITY DEFINER  -- Uses admin's permissions
AS SELECT employee_id, name, salary FROM employees;

-- Grant SELECT on view to alice (NOT on underlying table)
GRANT SELECT ON VIEW all_salaries TO alice;

-- Alice queries view
SELECT * FROM all_salaries;  -- ✓ Succeeds!

-- Alice tries direct table access
SELECT * FROM employees;  -- ✗ Denied!
```

---

## Implementation Timeline

### Phase 1: Catalog Schema (2-3 hours)
- Add `security_type` and `owner_id` columns to pg_procedures, pg_functions, pg_views
- Create `pg_object_permissions` table
- Extend `grantee_type` enum to include SQL objects

### Phase 2: Security Context Stack (3-4 hours)
- Implement `pushSecurityContext()` / `popSecurityContext()` in ConnectionContext
- Implement `getEffectiveUserId()`
- Add logging for security context changes

### Phase 3: Permission Checking (2-3 hours)
- Update `SecurityAnalyzer::checkPermission()` to check SQL object permissions
- Implement definer vs invoker rights logic
- Add object permission priority order

### Phase 4: Executor Integration (3-5 hours)
- Update procedure/function execution to push/pop security context
- Update view execution to push/pop security context
- Add trigger execution security context (future)
- Exception safety (ensure context popped on errors)

### Phase 5: SQL Parser (2-3 hours)
- Parse `SQL SECURITY DEFINER/INVOKER` clause
- Parse `GRANT ... TO PROCEDURE/FUNCTION/VIEW`
- Update AST nodes

### Phase 6: Bytecode Generation (2-3 hours)
- Encode security_type in procedure/function/view definitions
- Encode object grants in GRANT/REVOKE bytecode

**Total Estimated Time:** 14-21 hours

---

## Benefits

### Security Benefits
1. **Least Privilege** - Users only need EXECUTE on procedures, not direct table access
2. **Encapsulation** - Business logic can enforce security rules
3. **Audit Trail** - All access goes through defined procedures
4. **Separation of Concerns** - Procedure owner != caller

### Architecture Benefits
1. **Ownership Chaining** - Automatic permission cascading
2. **Flexible Security** - Choose DEFINER or INVOKER per-object
3. **Performance** - Security context is a simple stack push/pop (O(1))
4. **Standard Compliance** - Matches SQL:2016 specification

### Development Benefits
1. **Simplified Permissions** - Grant EXECUTE once, not SELECT/INSERT/UPDATE on every table
2. **Centralized Security** - Business rules in procedures, not scattered in application code
3. **Testing** - Easy to test procedure security independently

---

## Comparison with Other Databases

### PostgreSQL
```sql
-- PostgreSQL syntax (identical to our proposal)
CREATE FUNCTION get_salary(emp_id INT)
    SECURITY DEFINER  -- Uses function owner's privileges
    RETURNS DECIMAL
AS $$ ... $$;
```

### SQL Server
```sql
-- SQL Server uses EXECUTE AS
CREATE PROCEDURE get_salary @emp_id INT
WITH EXECUTE AS OWNER  -- Equivalent to SECURITY DEFINER
AS
BEGIN
    SELECT salary FROM employees WHERE id = @emp_id;
END;
```

### Oracle
```sql
-- Oracle uses AUTHID
CREATE PROCEDURE get_salary(emp_id NUMBER)
    AUTHID DEFINER  -- Default, uses procedure owner's privileges
AS
BEGIN
    SELECT salary INTO result FROM employees WHERE id = emp_id;
END;
```

**Our design matches SQL standard and PostgreSQL syntax!**

---

## Testing Strategy

### Unit Tests

1. **Security Context Stack**
   - Push/pop operations
   - Nested contexts
   - Exception safety

2. **Permission Resolution**
   - Object permissions
   - Definer permissions
   - Invoker permissions
   - Priority order

### Integration Tests

1. **Procedure Execution**
   - Definer rights procedure
   - Invoker rights procedure
   - Nested procedure calls
   - Permission denied scenarios

2. **View Execution**
   - Definer rights view
   - Invoker rights view
   - Nested views
   - View + procedure interaction

3. **Complex Scenarios**
   - Multi-level procedure nesting
   - Circular dependencies
   - Permission revocation during execution

---

## Open Questions

1. **Circular Dependencies**
   - Q: What if procedure A calls procedure B, which calls A?
   - A: Stack depth limit (e.g., 32 levels), detect cycles

2. **Permission Changes During Execution**
   - Q: What if permissions are revoked while procedure is executing?
   - A: Use transaction snapshot - permissions checked at procedure start

3. **Security Context for Triggers**
   - Q: Should triggers use definer or invoker rights?
   - A: Standard says DEFINER, but make it configurable

4. **Anonymous Blocks**
   - Q: What security context for `DO $$ ... $$` blocks?
   - A: Always INVOKER (no owner, uses caller's permissions)

---

## Success Criteria

### Functional Requirements
- ✅ Users can grant permissions to procedures/functions/views
- ✅ Procedures can execute with owner's privileges (DEFINER)
- ✅ Procedures can execute with caller's privileges (INVOKER)
- ✅ Security context stack handles nested calls correctly
- ✅ Permission checks use correct effective user ID

### Performance Requirements
- ✅ Security context push/pop < 1μs
- ✅ No performance degradation for procedures without special permissions
- ✅ Nested procedure calls maintain O(1) context switching

### Compatibility Requirements
- ✅ Matches SQL:2016 standard syntax
- ✅ Compatible with PostgreSQL SECURITY DEFINER/INVOKER
- ✅ Backward compatible (existing procedures use DEFINER by default)

---

## Documentation Deliverables

1. **User Guide**
   - How to create SECURITY DEFINER/INVOKER procedures
   - How to grant permissions to SQL objects
   - Best practices for secure procedure design

2. **Security Guide**
   - Definer vs invoker rights explained
   - Ownership chaining concepts
   - Security implications and risks

3. **Migration Guide**
   - Converting from user-based permissions to object-based
   - Examples of common patterns

---

**Document Version:** 1.0
**Last Updated:** November 10, 2025
**Status:** Design Complete - Ready for Implementation
**Priority:** CRITICAL - Required for stored procedures
**Estimated Implementation Time:** 14-21 hours
