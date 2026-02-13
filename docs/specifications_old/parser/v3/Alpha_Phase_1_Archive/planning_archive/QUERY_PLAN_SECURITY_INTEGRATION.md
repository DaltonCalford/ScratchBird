# Query Plan Security Integration - Design Document

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

**Date:** November 10, 2025
**Status:** Proposed Architecture Enhancement
**Priority:** HIGH - Significant Performance & Security Benefits

---

## Problem Statement

**Current Approach (Runtime Security Checks):**
```
Parse SQL → Generate Bytecode → Execute → Check Permissions (per row/operation)
                                    ↑
                                Runtime checks
                                (repeated for every row)
```

**Issues:**
1. ❌ Permission checks happen **during execution** (expensive)
2. ❌ Checks repeated for every row (RLS would be especially bad)
3. ❌ No early failure (parse succeeds, execute fails later)
4. ❌ Can't optimize away inaccessible columns/tables

---

## Proposed Approach: Query Plan Security Analysis

**New Flow:**
```
Parse SQL → Semantic Analysis → Security Validation → Generate Bytecode → Execute
                                      ↑
                            Check permissions ONCE
                            Fail fast if denied
                            Optimize query plan
```

---

## Design

### Phase 1: Query Plan Security Context

**New Component: `SecurityAnalyzer`**

```cpp
namespace scratchbird::parser {

class SecurityAnalyzer {
public:
    SecurityAnalyzer(core::CatalogManager* catalog,
                    core::ConnectionContext* conn_ctx);

    // Analyze query and determine security constraints
    struct SecurityPlan {
        // Required permissions validated at plan time
        std::vector<Permission> required_permissions;
        bool all_granted;

        // Column visibility (for column-level permissions)
        std::unordered_map<std::string, std::vector<std::string>> visible_columns;

        // RLS policies (compiled once, applied per-row)
        std::vector<CompiledRLSPolicy> rls_policies;

        // Early failure info
        std::string denial_reason;
    };

    // Analyze SELECT statement
    auto analyzeSelect(const SelectStmt* stmt, SecurityPlan& plan_out,
                      ErrorContext* ctx = nullptr) -> Status;

    // Analyze INSERT/UPDATE/DELETE
    auto analyzeInsert(const InsertStmt* stmt, SecurityPlan& plan_out,
                      ErrorContext* ctx = nullptr) -> Status;
    auto analyzeUpdate(const UpdateStmt* stmt, SecurityPlan& plan_out,
                      ErrorContext* ctx = nullptr) -> Status;
    auto analyzeDelete(const DeleteStmt* stmt, SecurityPlan& plan_out,
                      ErrorContext* ctx = nullptr) -> Status;

    // Analyze DDL
    auto analyzeDDL(const DDLStmt* stmt, SecurityPlan& plan_out,
                   ErrorContext* ctx = nullptr) -> Status;

private:
    core::CatalogManager* catalog_;
    core::ConnectionContext* conn_ctx_;

    // Cache for this analysis session
    std::unordered_set<core::ID> user_roles_;
    std::unordered_set<core::ID> user_groups_;
    bool is_superuser_;
};

} // namespace scratchbird::parser
```

---

## Implementation Details

### 1. Analyze SELECT Statement

```cpp
auto SecurityAnalyzer::analyzeSelect(const SelectStmt* stmt,
                                     SecurityPlan& plan_out,
                                     ErrorContext* ctx) -> Status
{
    // 1. SUPERUSER BYPASS
    if (conn_ctx_->isSuperuser()) {
        plan_out.all_granted = true;
        return Status::OK;
    }

    // 2. RESOLVE ALL TABLES IN QUERY
    std::vector<TableRef> tables = resolveTableReferences(stmt);

    // 3. CHECK TABLE-LEVEL SELECT PERMISSION (ONCE)
    for (const auto& table : tables) {
        bool has_perm = false;
        auto status = catalog_->hasPermission(
            conn_ctx_->getCurrentUserId(), table.table_id,
            PermissionObjectType::TABLE, Privilege::SELECT,
            has_perm, ctx);

        if (status != Status::OK || !has_perm) {
            // Also check roles and groups
            if (!checkRolesAndGroups(table.table_id, Privilege::SELECT)) {
                plan_out.all_granted = false;
                plan_out.denial_reason = "Permission denied: SELECT on table " + table.name;
                return Status::PERMISSION_DENIED;
            }
        }

        plan_out.required_permissions.push_back({table.table_id, Privilege::SELECT});
    }

    // 4. RESOLVE COLUMN-LEVEL PERMISSIONS
    for (const auto& table : tables) {
        std::vector<std::string> accessible_cols;
        auto status = catalog_->getAccessibleColumns(
            conn_ctx_->getCurrentUserId(), table.table_id,
            Privilege::SELECT, accessible_cols, ctx);

        if (status == Status::OK) {
            plan_out.visible_columns[table.name] = accessible_cols;
        }
    }

    // 5. VALIDATE REQUESTED COLUMNS
    for (const auto& col_ref : stmt->select_list) {
        if (col_ref.column == "*") {
            // SELECT * - will use visible_columns at execution
            continue;
        }

        const auto& visible = plan_out.visible_columns[col_ref.table];
        if (std::find(visible.begin(), visible.end(), col_ref.column) == visible.end()) {
            plan_out.all_granted = false;
            plan_out.denial_reason = "Permission denied: SELECT on column " + col_ref.column;
            return Status::PERMISSION_DENIED;
        }
    }

    // 6. COMPILE RLS POLICIES (ONCE)
    for (const auto& table : tables) {
        std::vector<RLSPolicyInfo> policies;
        auto status = catalog_->getRLSPoliciesByTable(table.table_id, policies, ctx);

        for (const auto& policy : policies) {
            if (policyAppliesTo(policy)) {
                // Compile policy expression to bytecode (ONCE)
                auto compiled = compilePolicyExpression(policy.using_expression);
                plan_out.rls_policies.push_back(compiled);
            }
        }
    }

    plan_out.all_granted = true;
    return Status::OK;
}
```

**Key Benefits:**
- ✅ **Permission checks happen ONCE during planning** (not per-row)
- ✅ **Fail fast** - Error before execution starts
- ✅ **RLS policies compiled ONCE** (not per-row)
- ✅ **Column filtering** determined at plan time

---

### 2. Integration with Bytecode Generator

```cpp
// In BytecodeGenerator
class BytecodeGenerator {
private:
    SecurityAnalyzer* security_analyzer_;

public:
    auto generate(const ASTNode* ast) -> std::vector<uint8_t> {
        // 1. Perform security analysis
        SecurityPlan security_plan;
        auto status = security_analyzer_->analyzeSelect(
            static_cast<const SelectStmt*>(ast),
            security_plan,
            &err_ctx);

        if (status != Status::OK) {
            // FAIL FAST - Permission denied before generating bytecode
            throw PermissionDeniedException(security_plan.denial_reason);
        }

        // 2. Generate optimized bytecode with security plan
        std::vector<uint8_t> bytecode;

        // Embed security plan in bytecode for executor
        encodeSecurityPlan(security_plan, bytecode);

        // Generate query execution bytecode
        // ... (existing code)

        return bytecode;
    }
};
```

---

### 3. Executor Changes

```cpp
// In Executor
void Executor::executeSelect() {
    // 1. DECODE SECURITY PLAN (created during bytecode generation)
    SecurityPlan security_plan = decodeSecurityPlan();

    // 2. NO PERMISSION CHECKS NEEDED
    // (Already validated during planning!)

    // 3. USE PRE-FILTERED COLUMN LIST
    std::vector<std::string> columns_to_select = security_plan.visible_columns[table_name];

    // 4. APPLY RLS POLICIES (compiled once, evaluate per-row)
    for (each row) {
        // Evaluate pre-compiled RLS predicates
        bool visible = evaluateRLSPolicies(security_plan.rls_policies, row);
        if (!visible) continue;

        // ... normal row processing ...
    }
}
```

**Performance Comparison:**

| Approach | Permission Checks | RLS Compilation | Column Filtering |
|----------|-------------------|-----------------|------------------|
| **Current (Runtime)** | Per-row/operation | Per-row | Per-row |
| **Proposed (Plan-time)** | Once (planning) | Once (planning) | Once (planning) |

**Example Query with 10,000 rows:**
- Current: 10,000 permission checks + 10,000 RLS compilations
- Proposed: 1 permission check + 1 RLS compilation + 10,000 RLS evaluations

**Estimated speedup: 10-100x for permission-heavy queries**

---

## Group Membership Caching

Since we're doing plan-time analysis, cache everything needed:

```cpp
class SecurityAnalyzer {
private:
    // Cached security context (loaded once per query)
    struct CachedSecurityContext {
        core::ID user_id;
        core::ID active_role_id;
        bool is_superuser;

        // Cached memberships
        std::unordered_set<core::ID> role_ids;
        std::unordered_set<core::ID> group_ids;

        // Timestamp for cache invalidation
        uint64_t cache_timestamp;
    };

    CachedSecurityContext security_ctx_;

    void loadSecurityContext() {
        // Load user info
        security_ctx_.user_id = conn_ctx_->getCurrentUserId();
        security_ctx_.active_role_id = conn_ctx_->getActiveRoleId();
        security_ctx_.is_superuser = conn_ctx_->isSuperuser();

        // Load and cache role memberships
        std::vector<core::CatalogManager::RoleMembershipInfo> roles;
        catalog_->getUserRoles(security_ctx_.user_id, roles, nullptr);
        for (const auto& role : roles) {
            security_ctx_.role_ids.insert(role.role_id);
        }

        // Load and cache group memberships (NEW!)
        std::vector<core::CatalogManager::GroupMembershipInfo> groups;
        catalog_->getUserGroups(security_ctx_.user_id, groups, nullptr);
        for (const auto& group : groups) {
            security_ctx_.group_ids.insert(group.group_id);
        }

        security_ctx_.cache_timestamp = getCurrentTimestamp();
    }

    bool checkPermission(const core::ID& object_id, Privilege privilege) {
        // 1. Check user permissions
        bool has_perm = false;
        catalog_->hasPermission(security_ctx_.user_id, object_id,
                               PermissionObjectType::TABLE, privilege, has_perm, nullptr);
        if (has_perm) return true;

        // 2. Check role permissions (from cache)
        for (const auto& role_id : security_ctx_.role_ids) {
            catalog_->hasPermission(role_id, object_id,
                                   PermissionObjectType::TABLE, privilege, has_perm, nullptr);
            if (has_perm) return true;
        }

        // 3. Check active role (if different from granted roles)
        if (security_ctx_.active_role_id != core::ID{}) {
            catalog_->hasPermission(security_ctx_.active_role_id, object_id,
                                   PermissionObjectType::TABLE, privilege, has_perm, nullptr);
            if (has_perm) return true;
        }

        // 4. Check group permissions (from cache - NEW!)
        for (const auto& group_id : security_ctx_.group_ids) {
            catalog_->hasPermission(group_id, object_id,
                                   PermissionObjectType::TABLE, privilege, has_perm, nullptr);
            if (has_perm) return true;
        }

        // 5. Check PUBLIC permissions
        core::ID public_id = /* PUBLIC constant */;
        catalog_->hasPermission(public_id, object_id,
                               PermissionObjectType::TABLE, privilege, has_perm, nullptr);
        return has_perm;
    }
};
```

---

## Revised Implementation Order

### Phase 3.0: Query Plan Security Framework (NEW - DO FIRST!)
**Duration:** 8-12 hours
**Priority:** CRITICAL - Foundation for all security features

#### 3.0.1: Security Analyzer Component (4-5 hours)
- Create `SecurityAnalyzer` class
- Implement plan-time permission validation
- Cache roles, groups, and permissions

#### 3.0.2: Security Plan Encoding (2-3 hours)
- Define `SecurityPlan` structure
- Encode/decode in bytecode
- Integrate with `BytecodeGenerator`

#### 3.0.3: Executor Integration (2-4 hours)
- Update `Executor` to use `SecurityPlan`
- Remove runtime permission checks
- Use pre-compiled RLS policies

**Files to Create:**
- `include/scratchbird/parser/security_analyzer.h` (+100 lines)
- `src/parser/security_analyzer.cpp` (+300 lines)

**Files to Modify:**
- `src/sblr/bytecode_generator.cpp` - Integrate SecurityAnalyzer (+50 lines)
- `src/sblr/executor.cpp` - Use SecurityPlan (+80 lines modified)
- `include/scratchbird/core/connection_context.h` - Add group caching (+20 lines)

---

### Updated Phase 3.1: Comprehensive Caching (Simpler Now!)
**Duration:** 3-5 hours (reduced from 5-8 hours)

#### 3.1.1: Connection Context Caching (3-5 hours)
```cpp
class ConnectionContext {
private:
    // Security context cache (loaded once per transaction)
    std::vector<ID> cached_role_ids_;
    std::vector<ID> cached_group_ids_;
    bool security_cache_loaded_ = false;

public:
    const std::vector<ID>& getUserRoles() {
        if (!security_cache_loaded_) {
            loadSecurityCache();
        }
        return cached_role_ids_;
    }

    const std::vector<ID>& getUserGroups() {
        if (!security_cache_loaded_) {
            loadSecurityCache();
        }
        return cached_group_ids_;
    }

private:
    void loadSecurityCache() {
        // Load roles
        std::vector<core::CatalogManager::RoleMembershipInfo> roles;
        db_->catalog_manager()->getUserRoles(current_user_id_, roles, nullptr);
        for (const auto& role : roles) {
            cached_role_ids_.push_back(role.role_id);
        }

        // Load groups
        std::vector<core::CatalogManager::GroupMembershipInfo> groups;
        db_->catalog_manager()->getUserGroups(current_user_id_, groups, nullptr);
        for (const auto& group : groups) {
            cached_group_ids_.push_back(group.group_id);
        }

        security_cache_loaded_ = true;
    }
};
```

**Note:** Permission result caching is LESS critical now since checks happen at plan-time, not per-row!

---

## Benefits Summary

### Performance Benefits

1. **Permission Checks: 10-100x faster**
   - Current: Check every row/operation
   - New: Check once during planning

2. **RLS: 100-1000x faster compilation**
   - Current: Compile policy expression per-row
   - New: Compile once, evaluate per-row

3. **Column Filtering: Instant**
   - Current: Check per-column per-row
   - New: Pre-filtered at plan time

4. **Group Membership: 100x faster**
   - Current: Catalog lookup per permission check
   - New: Cached for entire transaction

### Security Benefits

1. **Fail Fast** - Errors before execution starts
2. **Better Error Messages** - Know exactly which permission failed
3. **Audit Friendly** - Log permission checks at planning, not per-row
4. **Cache Consistency** - Security context loaded once per query

### Architectural Benefits

1. **Separation of Concerns** - Security analysis separate from execution
2. **Optimizer Friendly** - Can skip inaccessible tables entirely
3. **Prepared Statements** - Security plan cached with query plan
4. **Testability** - Can unit test security analysis independently

---

## Example: Performance Comparison

**Query:**
```sql
SELECT employee_id, name, salary, department
FROM employees
WHERE department = 'Engineering';
-- RLS Policy: user_id = current_user_id OR is_manager()
-- 10,000 employees, 100 in Engineering
```

### Current Approach (Runtime Checks)

```
Parse SQL ────> Generate Bytecode ────> Execute
                                         │
                                         ├─ Check SELECT permission (1 check)
                                         ├─ Scan 10,000 rows:
                                         │   ├─ Compile RLS policy (10,000 times!)
                                         │   ├─ Evaluate RLS (10,000 times)
                                         │   ├─ Check column permissions (40,000 times - 4 columns)
                                         │   └─ Filter by WHERE
                                         └─ Return 100 rows

Total checks: 1 + 10,000 + 10,000 + 40,000 = 60,001 security operations
Time: ~600ms
```

### New Approach (Plan-Time Analysis)

```
Parse SQL ────> Security Analysis ────> Generate Bytecode ────> Execute
                      │                                           │
                      ├─ Check SELECT permission (1 check)       ├─ Scan 10,000 rows:
                      ├─ Check column permissions (4 checks)     │   ├─ Evaluate RLS (10,000 times)
                      ├─ Compile RLS policy (1 compilation)      │   └─ Filter by WHERE
                      └─ Cache roles/groups (2 queries)          └─ Return 100 rows

Total checks: 1 + 4 + 1 + 2 + 10,000 = 10,008 security operations
Time: ~8ms (75x faster!)
```

---

## Migration Path

### Step 1: Implement SecurityAnalyzer (Week 1)
- Create new component
- Integrate with BytecodeGenerator
- Keep existing runtime checks as fallback

### Step 2: Add Group Caching (Week 1)
- Extend ConnectionContext
- Implement getUserGroups()

### Step 3: Update Executor (Week 1-2)
- Use SecurityPlan from bytecode
- Remove redundant runtime checks
- Benchmark performance

### Step 4: Optimize (Week 2)
- Fine-tune cache sizes
- Add prepared statement support
- Profile and optimize hot paths

---

## Open Questions

1. **Cache Invalidation:**
   - Q: When should security cache be invalidated?
   - A: Start of each transaction, or on GRANT/REVOKE in same session

2. **Prepared Statements:**
   - Q: Can we cache SecurityPlan with prepared statements?
   - A: YES - Major benefit! Parse once, validate once, execute many times

3. **Dynamic Policies:**
   - Q: What if RLS policy references current timestamp?
   - A: Re-evaluate at execution time (flag in SecurityPlan)

4. **Cross-Database Queries:**
   - Q: How to handle permissions across databases?
   - A: Validate each database separately in SecurityAnalyzer

---

## Success Metrics

### Performance Targets
- ✅ Query planning overhead < 5% for simple queries
- ✅ 10x speedup for permission-heavy queries
- ✅ 100x speedup for RLS compilation
- ✅ Group membership lookups: < 1ms per transaction

### Correctness Targets
- ✅ 100% compatibility with existing security semantics
- ✅ All permissions validated before execution
- ✅ No false positives (grant access when should deny)
- ✅ No false negatives (deny access when should grant)

---

**Document Version:** 1.0
**Last Updated:** November 10, 2025
**Status:** Proposed Architecture - Ready for Review
**Estimated Implementation Time:** 11-17 hours for query plan integration
