# Security Phase 3: Final Implementation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Created**: November 11, 2025
**Goal**: Complete security implementation (Phases 3.1 + 3.5)
**Estimated Time**: 38-57 hours
**Status**: ACTIVE

---

## Executive Summary

This plan completes the final two critical security phases:

**Phase 3.1: SQL Object Permissions** (14-21 hours)
- GRANT permissions to procedures/functions/views
- Security context stack for nested calls
- SQL SECURITY DEFINER/INVOKER support
- Ownership chaining

**Phase 3.5: RLS WITH CHECK Enforcement for DML** (24-36 hours)
- Fix CREATE POLICY executor expression handling
- Implement DML query planning phase
- WITH CHECK enforcement for INSERT/UPDATE
- USING enforcement for UPDATE/DELETE

Upon completion, ScratchBird will have a **complete, enterprise-grade security system**.

---

## Current State

### ✅ Already Complete

**Phase 1: Core Infrastructure** ✅
- Users, Roles, Groups, Sessions CRUD
- Permission system (GRANT/REVOKE)
- Transitive closure for nested groups/roles

**Phase 2: SQL Integration** ✅
- 13 SQL security statements
- Full parser/bytecode/executor pipeline

**Phase 3.2: Query Plan Security** ✅
- Permission checks at planning time (10-100x speedup)

**Phase 3.3: Column-Level Permissions** ✅
- GRANT SELECT(col1, col2) syntax

**Phase 3.4: Row-Level Security for SELECT** ✅
- CREATE/DROP POLICY statements
- ALTER TABLE RLS commands
- Runtime expression evaluation
- TOAST persistence for expressions

---

## Phase 3.1: SQL Object Permissions (14-21 hours)

### Overview

Enable stored procedures, functions, and views to have their own security context, allowing them to execute with specific privileges regardless of the caller's permissions.

### Use Cases

```sql
-- Procedure executes with owner privileges
CREATE PROCEDURE get_salary(employee_id INT)
SQL SECURITY DEFINER
AS
BEGIN
    SELECT salary FROM private_salaries WHERE id = employee_id;
END;

-- Grant execute permission to HR role
GRANT EXECUTE ON PROCEDURE get_salary TO ROLE hr;

-- Users in HR role can execute the procedure even if they can't
-- directly access private_salaries table
```

### Implementation Tasks

#### Task 3.1.1: Object Permissions Catalog Schema (2-3 hours)

**File**: `include/scratchbird/core/ondisk.h`

Add object permissions record structure:

```cpp
// Object Permission Record (256 bytes)
#pragma pack(push, 1)
struct ObjectPermissionRecord {
    ID permission_id;           // Permission UUID (16 bytes)
    ID object_id;              // Object UUID (procedure/function/view)
    uint8_t object_type;       // 1=PROCEDURE, 2=FUNCTION, 3=VIEW
    ID grantee_id;             // User/Role/Group UUID
    uint8_t grantee_type;      // 1=USER, 2=ROLE, 3=GROUP
    uint32_t permissions;      // Bitmask: EXECUTE=1, etc.
    uint8_t grant_option;      // WITH GRANT OPTION flag
    ID grantor_id;             // Who granted this permission
    uint64_t created_time;     // When granted
    uint8_t is_valid;          // Soft delete flag
    uint8_t reserved[199];     // Padding to 256 bytes
};
#pragma pack(pop)
```

**File**: `include/scratchbird/core/catalog_manager.h`

Add ObjectPermissionInfo struct and methods:

```cpp
struct ObjectPermissionInfo {
    ID permission_id;
    ID object_id;
    ObjectType object_type;
    ID grantee_id;
    GranteeType grantee_type;
    uint32_t permissions;
    bool grant_option;
    ID grantor_id;
    uint64_t created_time;
};

// Object permission operations
auto grantObjectPermission(const ID& object_id, ObjectType object_type,
                          const ID& grantee_id, GranteeType grantee_type,
                          uint32_t permissions, bool grant_option,
                          ID& permission_id, ErrorContext* ctx = nullptr) -> Status;

auto revokeObjectPermission(const ID& object_id, const ID& grantee_id,
                           ErrorContext* ctx = nullptr) -> Status;

auto hasObjectPermission(const ID& object_id, const ID& user_id,
                        uint32_t required_permissions,
                        ErrorContext* ctx = nullptr) -> bool;

auto getObjectPermissions(const ID& object_id,
                         std::vector<ObjectPermissionInfo>& perms_out,
                         ErrorContext* ctx = nullptr) -> Status;
```

#### Task 3.1.2: Security Context Stack (4-6 hours)

**File**: `include/scratchbird/core/connection_context.h`

Add security context stack for nested calls:

```cpp
struct SecurityContext {
    ID effective_user_id;      // Who is executing
    ID effective_role_id;      // Active role
    bool is_superuser;         // Superuser flag
    SecurityMode mode;         // DEFINER or INVOKER
    ID object_id;              // Current procedure/function/view
};

class ConnectionContext {
    // ... existing fields ...

    // Security context stack (for nested procedure calls)
    std::vector<SecurityContext> security_stack_;

public:
    // Push new security context (entering procedure)
    void pushSecurityContext(const ID& user_id, const ID& role_id,
                            bool is_superuser, SecurityMode mode,
                            const ID& object_id);

    // Pop security context (exiting procedure)
    void popSecurityContext();

    // Get current security context
    SecurityContext getCurrentSecurityContext() const;

    // Check if we're in a DEFINER context
    bool isDefinerContext() const;
};
```

**Implementation**:
- Stack-based security contexts for nested calls
- Push context when entering procedure/function/view
- Pop context when exiting
- Current security checks use top of stack

#### Task 3.1.3: SQL SECURITY DEFINER/INVOKER (3-4 hours)

**File**: `include/scratchbird/parser/ast.h`

Add SQL SECURITY clause to procedure/function definitions:

```cpp
enum class SecurityMode : uint8_t {
    INVOKER = 0,  // Execute with caller's privileges (default)
    DEFINER = 1   // Execute with owner's privileges
};

class CreateProcedureStmt : public Stmt {
    // ... existing fields ...
    SecurityMode security_mode_ = SecurityMode::INVOKER;

public:
    auto securityMode() const -> SecurityMode { return security_mode_; }
    void setSecurityMode(SecurityMode mode) { security_mode_ = mode; }
};
```

**File**: `src/parser/parser.cpp`

Parse SQL SECURITY clause:

```cpp
// CREATE PROCEDURE foo() SQL SECURITY DEFINER AS ...
if (match(TokenType::SQL) && peek().type == TokenType::SECURITY) {
    advance();  // SQL
    advance();  // SECURITY
    if (match(TokenType::DEFINER)) {
        stmt->setSecurityMode(SecurityMode::DEFINER);
    } else if (match(TokenType::INVOKER)) {
        stmt->setSecurityMode(SecurityMode::INVOKER);
    } else {
        error("Expected DEFINER or INVOKER after SQL SECURITY");
    }
}
```

#### Task 3.1.4: Ownership Chaining (3-5 hours)

**File**: `src/sblr/executor.cpp`

Implement ownership chaining in procedure execution:

```cpp
auto Executor::executeProcedure(const ProcedureInfo& proc_info) -> Status {
    ConnectionContext* ctx = ConnectionContext::getCurrentContext();
    if (!ctx) {
        return Status::INVALID_ARGUMENT;
    }

    // Check EXECUTE permission
    if (!catalog_->hasObjectPermission(proc_info.procedure_id,
                                      ctx->getCurrentUserId(),
                                      PERM_EXECUTE, nullptr)) {
        return Status::PERMISSION_DENIED;
    }

    // Push security context based on SQL SECURITY mode
    if (proc_info.security_mode == SecurityMode::DEFINER) {
        // Execute with owner's privileges
        ctx->pushSecurityContext(
            proc_info.owner_id,
            ID(),  // No role
            isUserSuperuser(proc_info.owner_id),
            SecurityMode::DEFINER,
            proc_info.procedure_id
        );
    } else {
        // Execute with caller's privileges (INVOKER)
        ctx->pushSecurityContext(
            ctx->getCurrentUserId(),
            ctx->getActiveRoleId(),
            ctx->isSuperuser(),
            SecurityMode::INVOKER,
            proc_info.procedure_id
        );
    }

    // Execute procedure body
    Status status = executeProcedureBody(proc_info);

    // Pop security context
    ctx->popSecurityContext();

    return status;
}
```

#### Task 3.1.5: Testing (2-3 hours)

**File**: `tests/integration/test_security_phase3_1_object_permissions.cpp`

Test scenarios:
1. GRANT EXECUTE on procedure
2. SQL SECURITY DEFINER execution
3. SQL SECURITY INVOKER execution
4. Ownership chaining (procedure calls procedure)
5. Permission denial for unauthorized users
6. WITH GRANT OPTION delegation

---

## Phase 3.5: RLS WITH CHECK Enforcement for DML (24-36 hours)

### Overview

Extend Row-Level Security to protect INSERT, UPDATE, and DELETE operations using USING and WITH CHECK clauses.

### Use Cases

```sql
-- Policy for SELECT (USING: which rows can be seen)
CREATE POLICY employee_select ON employees
FOR SELECT
USING (department_id = current_user_department());

-- Policy for INSERT (WITH CHECK: which rows can be inserted)
CREATE POLICY employee_insert ON employees
FOR INSERT
WITH CHECK (department_id = current_user_department());

-- Policy for UPDATE (USING: which rows can be updated, WITH CHECK: what they can become)
CREATE POLICY employee_update ON employees
FOR UPDATE
USING (department_id = current_user_department())
WITH CHECK (department_id = current_user_department());

-- Policy for DELETE (USING: which rows can be deleted)
CREATE POLICY employee_delete ON employees
FOR DELETE
USING (department_id = current_user_department());
```

### Implementation Tasks

#### Task 3.5.1: Fix CREATE POLICY Executor (2-4 hours)

**File**: `src/sblr/executor.cpp` (lines 13345-13351)

**Current code** (BROKEN):
```cpp
if (has_using_expr) {
    error("Expression evaluation for USING clause not yet implemented");
}
if (has_with_check_expr) {
    error("Expression evaluation for WITH CHECK clause not yet implemented");
}
```

**Fixed code**:
```cpp
// Expressions are stored as strings in catalog and parsed at runtime
// No validation needed here - validation happens at policy application time
// USING and WITH CHECK expressions are already handled by catalog_manager
```

Simply **remove the error calls** - expressions are handled by the catalog manager's TOAST persistence.

#### Task 3.5.2: DML Query Planning Phase (8-12 hours)

**Problem**: Currently DML bypasses the query planner entirely. Executor directly calls `storage_engine->insertTuple()` without any planning.

**Solution**: Add planning phase for DML similar to SELECT.

**File**: `include/scratchbird/optimizer/query_planner.h`

Add DML planning methods:

```cpp
class QueryPlanner {
public:
    // ... existing methods ...

    // DML planning methods (Phase 3.5)
    auto planInsert(const parser::InsertStmt* insert_stmt,
                   ConnectionContext* ctx,
                   ErrorContext* error_ctx) -> std::unique_ptr<InsertPlan>;

    auto planUpdate(const parser::UpdateStmt* update_stmt,
                   ConnectionContext* ctx,
                   ErrorContext* error_ctx) -> std::unique_ptr<UpdatePlan>;

    auto planDelete(const parser::DeleteStmt* delete_stmt,
                   ConnectionContext* ctx,
                   ErrorContext* error_ctx) -> std::unique_ptr<DeletePlan>;
};

// DML Plan nodes
struct InsertPlan {
    ID table_id;
    std::vector<parser::Expression*> with_check_policies;  // Policies to check
    bool rls_enabled;
};

struct UpdatePlan {
    ID table_id;
    parser::Expression* where_clause;  // Original WHERE + USING policies
    std::vector<parser::Expression*> with_check_policies;  // Post-update checks
    bool rls_enabled;
};

struct DeletePlan {
    ID table_id;
    parser::Expression* where_clause;  // Original WHERE + USING policies
    bool rls_enabled;
};
```

**Implementation**:

```cpp
auto QueryPlanner::planInsert(const parser::InsertStmt* insert_stmt,
                             ConnectionContext* ctx,
                             ErrorContext* error_ctx) -> std::unique_ptr<InsertPlan>
{
    auto plan = std::make_unique<InsertPlan>();

    // Get table info
    CatalogManager::TableInfo table_info;
    Status status = catalog_->getTableInfo(insert_stmt->tableName(), table_info, error_ctx);
    if (status != Status::OK) return nullptr;

    plan->table_id = table_info.table_id;
    plan->rls_enabled = table_info.rls_enabled;

    // If RLS enabled, get INSERT/ALL policies
    if (table_info.rls_enabled) {
        std::vector<CatalogManager::PolicyInfo> policies;
        status = catalog_->getTablePolicies(
            table_info.table_id,
            CatalogManager::PolicyType::INSERT,  // Also gets ALL policies
            policies,
            error_ctx);

        // Parse WITH CHECK expressions for runtime evaluation
        for (const auto& policy : policies) {
            if (!policy.with_check_expr.empty()) {
                parser::Expression* expr = parseExpressionString(
                    policy.with_check_expr, arena_, string_pool_, error_ctx);
                if (expr) {
                    plan->with_check_policies.push_back(expr);
                }
            }
        }
    }

    return plan;
}
```

#### Task 3.5.3: WITH CHECK Enforcement for INSERT (4-6 hours)

**File**: `src/sblr/executor.cpp`

Modify `executeInsert()` to check WITH CHECK policies:

```cpp
auto Executor::executeInsert(const InsertStmt* stmt) -> Status {
    // ... existing permission checks ...

    // Phase 3.5: Plan INSERT with RLS integration
    auto insert_plan = query_planner_->planInsert(stmt, conn_ctx, &ctx);
    if (!insert_plan) {
        return Status::INVALID_ARGUMENT;
    }

    // ... construct tuple from values ...

    // Phase 3.5: WITH CHECK enforcement
    if (insert_plan->rls_enabled && !insert_plan->with_check_policies.empty()) {
        // Evaluate each WITH CHECK policy against the new tuple
        bool any_policy_passed = false;

        for (auto* policy_expr : insert_plan->with_check_policies) {
            // Evaluate expression in context of new tuple
            Value result = evaluateExpression(policy_expr, tuple_data, column_offsets);

            if (result.type == DataType::BOOLEAN && result.as_bool) {
                any_policy_passed = true;
                break;  // OR semantics: any policy passing is sufficient
            }
        }

        if (!any_policy_passed) {
            SET_ERROR_CONTEXT(&ctx, Status::PERMISSION_DENIED,
                            "New row violates row-level security policy for table");
            return Status::PERMISSION_DENIED;
        }
    }

    // Insert tuple into storage
    return storage_engine_->insertTuple(table_id, tuple_data, tuple_size, &ctx);
}
```

#### Task 3.5.4: WITH CHECK Enforcement for UPDATE (4-6 hours)

**File**: `src/sblr/executor.cpp`

Modify `executeUpdate()` to check both USING and WITH CHECK:

```cpp
auto Executor::executeUpdate(const UpdateStmt* stmt) -> Status {
    // Phase 3.5: Plan UPDATE with RLS integration
    auto update_plan = query_planner_->planUpdate(stmt, conn_ctx, &ctx);
    if (!update_plan) {
        return Status::INVALID_ARGUMENT;
    }

    // Create scan with USING policies in WHERE clause
    // (only rows passing USING can be updated)
    auto scan = storage_engine_->createScan(
        update_plan->table_id,
        update_plan->where_clause,  // Original WHERE + USING policies
        &ctx);

    std::vector<TID> rows_to_update;

    // Scan for rows to update
    while (scan->next()) {
        TID tid = scan->currentTID();

        // Get old tuple data
        uint8_t* old_tuple = scan->currentTuple();

        // Apply updates to create new tuple
        uint8_t new_tuple[MAX_TUPLE_SIZE];
        applyUpdates(stmt, old_tuple, new_tuple);

        // Phase 3.5: WITH CHECK enforcement on new tuple
        if (update_plan->rls_enabled && !update_plan->with_check_policies.empty()) {
            bool any_policy_passed = false;

            for (auto* policy_expr : update_plan->with_check_policies) {
                Value result = evaluateExpression(policy_expr, new_tuple, column_offsets);

                if (result.type == DataType::BOOLEAN && result.as_bool) {
                    any_policy_passed = true;
                    break;
                }
            }

            if (!any_policy_passed) {
                // Skip this row - new values violate policy
                continue;
            }
        }

        rows_to_update.push_back(tid);
    }

    // Update all qualifying rows
    for (const TID& tid : rows_to_update) {
        storage_engine_->updateTuple(update_plan->table_id, tid, new_tuple, &ctx);
    }

    return Status::OK;
}
```

#### Task 3.5.5: USING Enforcement for DELETE (4-6 hours)

**File**: `src/sblr/executor.cpp`

Modify `executeDelete()` to enforce USING policies:

```cpp
auto Executor::executeDelete(const DeleteStmt* stmt) -> Status {
    // Phase 3.5: Plan DELETE with RLS integration
    auto delete_plan = query_planner_->planDelete(stmt, conn_ctx, &ctx);
    if (!delete_plan) {
        return Status::INVALID_ARGUMENT;
    }

    // Create scan with USING policies in WHERE clause
    // (only rows passing USING can be deleted)
    auto scan = storage_engine_->createScan(
        delete_plan->table_id,
        delete_plan->where_clause,  // Original WHERE + USING policies
        &ctx);

    std::vector<TID> rows_to_delete;

    // Scan for rows to delete
    while (scan->next()) {
        rows_to_delete.push_back(scan->currentTID());
    }

    // Delete all qualifying rows
    for (const TID& tid : rows_to_delete) {
        storage_engine_->deleteTuple(delete_plan->table_id, tid, &ctx);
    }

    return Status::OK;
}
```

#### Task 3.5.6: Integration Testing (2-4 hours)

**File**: `tests/integration/test_security_phase3_5_rls_dml.cpp`

Test scenarios:
1. INSERT with WITH CHECK - success case
2. INSERT with WITH CHECK - violation case
3. UPDATE with USING - can only update visible rows
4. UPDATE with WITH CHECK - new values must pass policy
5. DELETE with USING - can only delete visible rows
6. Multiple policies with OR semantics
7. Forced RLS for table owners

---

## Files to Create/Modify

### New Files
- `tests/integration/test_security_phase3_1_object_permissions.cpp`
- `tests/integration/test_security_phase3_5_rls_dml.cpp`
- `/docs/specifications/parser/v3/status/SECURITY_PHASE3_1_COMPLETE.md`
- `/docs/specifications/parser/v3/status/SECURITY_PHASE3_5_COMPLETE.md`
- `/docs/specifications/parser/v3/guides/OBJECT_PERMISSIONS_GUIDE.md`
- `/docs/specifications/parser/v3/guides/RLS_DML_GUIDE.md`

### Modified Files

**Phase 3.1**:
- `include/scratchbird/core/ondisk.h` - ObjectPermissionRecord
- `include/scratchbird/core/catalog_manager.h` - Object permission methods
- `src/core/catalog_manager.cpp` - Object permission CRUD
- `include/scratchbird/core/connection_context.h` - Security context stack
- `src/core/connection_context.cpp` - Stack implementation
- `include/scratchbird/parser/ast.h` - SecurityMode enum
- `src/parser/parser.cpp` - SQL SECURITY parsing
- `src/sblr/executor.cpp` - Ownership chaining

**Phase 3.5**:
- `include/scratchbird/optimizer/query_planner.h` - DML planning methods
- `src/optimizer/query_planner.cpp` - DML planning implementation
- `src/sblr/executor.cpp` - WITH CHECK/USING enforcement

---

## Success Criteria

### Phase 3.1 Complete When:
- ✅ Object permissions catalog table created and initialized
- ✅ GRANT EXECUTE ON PROCEDURE/FUNCTION/VIEW works
- ✅ REVOKE EXECUTE works
- ✅ SQL SECURITY DEFINER executes with owner privileges
- ✅ SQL SECURITY INVOKER executes with caller privileges
- ✅ Ownership chaining works for nested calls
- ✅ Security context stack handles deep nesting
- ✅ All integration tests pass

### Phase 3.5 Complete When:
- ✅ CREATE POLICY executor accepts USING/WITH CHECK expressions
- ✅ INSERT checks WITH CHECK policies
- ✅ UPDATE checks USING policies (row visibility)
- ✅ UPDATE checks WITH CHECK policies (new values)
- ✅ DELETE checks USING policies (row visibility)
- ✅ Multiple policies combine with OR semantics
- ✅ RLS enforced even for table owners (when forced)
- ✅ All integration tests pass

### Full Security System Complete When:
- ✅ All Phase 3.1 criteria met
- ✅ All Phase 3.5 criteria met
- ✅ Documentation complete
- ✅ README and PROJECT_CONTEXT updated
- ✅ No security-related TODOs remain

---

## Timeline

### Week 1 (20-25 hours)
- **Days 1-2**: Phase 3.1 Tasks 3.1.1-3.1.3 (9-13 hours)
- **Days 3-4**: Phase 3.1 Tasks 3.1.4-3.1.5 (5-8 hours)
- **Day 5**: Phase 3.5 Task 3.5.1 (2-4 hours)

### Week 2 (18-32 hours)
- **Days 1-3**: Phase 3.5 Tasks 3.5.2-3.5.3 (12-18 hours)
- **Days 4-5**: Phase 3.5 Tasks 3.5.4-3.5.6 (10-18 hours)
- **Documentation and final testing**: 4-6 hours

**Total**: 38-57 hours (5-7 working days at 8 hours/day)

---

## Next Steps

1. Begin with Phase 3.1 Task 3.1.1: Design object permissions catalog schema
2. Implement incrementally, testing each component
3. Document as you go
4. Celebrate when complete! 🎉

Upon completion, ScratchBird will have a **complete, production-ready security system** comparable to PostgreSQL.
