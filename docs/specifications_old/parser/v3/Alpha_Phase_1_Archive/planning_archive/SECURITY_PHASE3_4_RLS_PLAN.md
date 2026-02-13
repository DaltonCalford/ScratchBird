# Security Phase 3.4 - Row-Level Security (RLS) - Implementation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Status**: 📋 Planning
**Estimated Time**: 15-20 hours
**Complexity**: High
**Priority**: Medium-High
**Prerequisites**: Phase 3.3 (Column-Level Permissions) ✅

---

## Overview

Phase 3.4 implements Row-Level Security (RLS), allowing administrators to define policies that filter which rows users can access, modify, or delete. This is the final piece of fine-grained access control (column-level ✅ + row-level = complete).

---

## What is Row-Level Security?

**Definition**: RLS restricts which rows a user can access based on policies that evaluate predicates (WHERE conditions) at query execution time.

**Example Use Cases**:
1. Multi-tenant applications - Users only see their own data
2. Hierarchical permissions - Managers see their reports' data
3. Time-based access - Users only see current data
4. Attribute-based access - Users with specific attributes see specific rows

**SQL Example**:
```sql
-- Setup
CREATE TABLE orders (
    order_id INT,
    user_id INT,
    amount DECIMAL,
    created_at TIMESTAMP
);

-- Create policy: Users can only see their own orders
CREATE POLICY orders_user_policy ON orders
    FOR SELECT
    USING (user_id = current_user_id());

-- Enable RLS on table
ALTER TABLE orders ENABLE ROW LEVEL SECURITY;

-- Now when user 42 queries:
SELECT * FROM orders;
-- Automatically becomes:
SELECT * FROM orders WHERE user_id = 42;
```

---

## Design Goals

1. **PostgreSQL Compatibility**: Follow PostgreSQL's RLS syntax and semantics
2. **Performance**: Minimize overhead for tables without RLS
3. **Security**: Policies cannot be bypassed (except by superuser/table owner)
4. **Flexibility**: Support multiple policies per table with combining logic
5. **Query Planner Integration**: Push policies down for optimization
6. **Audit Trail**: Track policy creation, modification, deletion

---

## PostgreSQL RLS Feature Set

### Core Features (Phase 3.4 Target)
1. ✅ `CREATE POLICY` - Define row-level security policy
2. ✅ `DROP POLICY` - Remove policy
3. ✅ `ALTER TABLE ... ENABLE/DISABLE ROW LEVEL SECURITY` - Toggle RLS on table
4. ✅ `ALTER TABLE ... FORCE ROW LEVEL SECURITY` - Apply to table owners too
5. ✅ Policy types: `FOR SELECT`, `FOR INSERT`, `FOR UPDATE`, `FOR DELETE`, `FOR ALL`
6. ✅ Policy clauses: `USING (condition)` for visibility, `WITH CHECK (condition)` for modifications
7. ✅ Policy roles: `TO role_list` - apply to specific roles
8. ✅ Multiple policies per table with AND/OR combining

### Advanced Features (Phase 3.5+)
- `ALTER POLICY` - Modify existing policy
- Permissive vs Restrictive policies
- Policy dependency tracking
- Policy debugging tools

---

## Implementation Phases

### Phase 3.4.1: Catalog Schema (2-3 hours)
**Goal**: Create pg_policies catalog table

**Schema Design**:
```sql
CREATE TABLE pg_policies (
    policy_id UUID PRIMARY KEY,           -- Policy UUID
    table_id UUID NOT NULL,               -- Table this policy applies to
    policy_name VARCHAR(63) NOT NULL,     -- Policy name (unique per table)
    policy_type UINT8 NOT NULL,           -- SELECT=1, INSERT=2, UPDATE=3, DELETE=4, ALL=0
    roles TEXT[],                         -- Roles this policy applies to (empty = all)
    using_expr TEXT NOT NULL,             -- USING clause (for SELECT/UPDATE/DELETE)
    with_check_expr TEXT,                 -- WITH CHECK clause (for INSERT/UPDATE)
    is_enabled BOOLEAN DEFAULT TRUE,      -- Policy can be disabled
    created_time TIMESTAMP NOT NULL,
    modified_time TIMESTAMP,
    UNIQUE(table_id, policy_name)
);

CREATE INDEX idx_policies_table ON pg_policies(table_id);
```

**Catalog Manager Extensions**:
```cpp
struct PolicyInfo {
    ID policy_id;
    ID table_id;
    std::string policy_name;
    enum PolicyType { ALL = 0, SELECT = 1, INSERT = 2, UPDATE = 3, DELETE = 4 };
    PolicyType policy_type;
    std::vector<std::string> roles;  // Empty = all roles
    std::string using_expr;          // Parsed expression tree (serialized)
    std::string with_check_expr;     // Parsed expression tree (serialized)
    bool is_enabled;
    Timestamp created_time;
    Timestamp modified_time;
};
```

**Table Metadata Extensions**:
```cpp
// Add to pg_tables (or separate table):
struct TableRLSInfo {
    ID table_id;
    bool rls_enabled;        -- ALTER TABLE ... ENABLE ROW LEVEL SECURITY
    bool rls_forced;         -- ALTER TABLE ... FORCE ROW LEVEL SECURITY
};
```

**Deliverables**:
- pg_policies table definition
- TableRLSInfo storage
- Bootstrap catalog creation

---

### Phase 3.4.2: Policy CRUD Operations (3-4 hours)
**Goal**: Implement catalog operations for policies

**CatalogManager Methods**:
```cpp
class CatalogManager {
public:
    // Policy CRUD
    auto createPolicy(const ID& table_id, const std::string& policy_name,
                     PolicyType type, const std::vector<std::string>& roles,
                     const std::string& using_expr, const std::string& with_check_expr,
                     ID& policy_id_out, ErrorContext* ctx) -> Status;

    auto dropPolicy(const ID& table_id, const std::string& policy_name,
                   ErrorContext* ctx) -> Status;

    auto getPolicy(const ID& table_id, const std::string& policy_name,
                  PolicyInfo& info_out, ErrorContext* ctx) -> Status;

    auto getTablePolicies(const ID& table_id, PolicyType type,
                         std::vector<PolicyInfo>& policies_out,
                         ErrorContext* ctx) -> Status;

    // RLS enable/disable
    auto setTableRLS(const ID& table_id, bool enabled, bool forced,
                    ErrorContext* ctx) -> Status;

    auto getTableRLS(const ID& table_id, bool& enabled_out, bool& forced_out,
                    ErrorContext* ctx) -> Status;

    // Policy evaluation
    auto getPoliciesForUser(const ID& table_id, const ID& user_id,
                           PolicyType type, std::vector<PolicyInfo>& policies_out,
                           ErrorContext* ctx) -> Status;
};
```

**Implementation Notes**:
- Policy names must be unique per table (not globally)
- Empty roles vector = policy applies to all roles
- USING expression is required, WITH CHECK is optional
- Policies can be disabled without dropping

**Estimated**: ~150-200 lines

---

### Phase 3.4.3: SQL Parser Extensions (3-4 hours)
**Goal**: Parse CREATE POLICY, DROP POLICY, ALTER TABLE ... ROW LEVEL SECURITY

**New AST Nodes**:
```cpp
class CreatePolicyStmt : public Statement {
    StringPool::StringId policy_name_;
    StringPool::StringId table_name_;
    PolicyType type_;                           // FOR SELECT/INSERT/UPDATE/DELETE/ALL
    std::vector<StringPool::StringId> roles_;   // TO clause
    Expression* using_expr_;                    // USING (condition)
    Expression* with_check_expr_;               // WITH CHECK (condition)
};

class DropPolicyStmt : public Statement {
    StringPool::StringId policy_name_;
    StringPool::StringId table_name_;
    bool if_exists_;
};

// Extend AlterTableStmt
class AlterTableStmt : public Statement {
    // ... existing fields ...
    enum AlterType {
        // ... existing types ...
        ENABLE_RLS,
        DISABLE_RLS,
        FORCE_RLS,
        NO_FORCE_RLS
    };
};
```

**Parser Changes**:
```cpp
// In Parser::parseStatement()
if (match(TokenType::KW_CREATE)) {
    if (match(TokenType::KW_POLICY)) {
        return parseCreatePolicy();
    }
}

// In Parser::parseAlterTable()
if (match(TokenType::KW_ENABLE)) {
    if (match(TokenType::KW_ROW)) {
        consume(TokenType::KW_LEVEL);
        consume(TokenType::KW_SECURITY);
        return AlterTableStmt{ENABLE_RLS, ...};
    }
}
```

**Semantic Validation**:
- Policy name must be unique per table
- USING expression must be boolean
- WITH CHECK expression must be boolean
- Table must exist
- User must have sufficient privileges

**Supported Syntax**:
```sql
-- Create policy
CREATE POLICY policy_name ON table_name
    [FOR {SELECT | INSERT | UPDATE | DELETE | ALL}]
    [TO role_list]
    USING (condition)
    [WITH CHECK (condition)];

-- Drop policy
DROP POLICY [IF EXISTS] policy_name ON table_name;

-- Enable/disable RLS
ALTER TABLE table_name ENABLE ROW LEVEL SECURITY;
ALTER TABLE table_name DISABLE ROW LEVEL SECURITY;
ALTER TABLE table_name FORCE ROW LEVEL SECURITY;
ALTER TABLE table_name NO FORCE ROW LEVEL SECURITY;
```

**Estimated**: ~180-220 lines

---

### Phase 3.4.4: Bytecode & Executor (2-3 hours)
**Goal**: Generate bytecode and execute policy DDL statements

**New Opcodes**:
```cpp
enum class ExtendedOpcode : uint8_t {
    // ... existing opcodes ...
    EXT_CREATE_POLICY = 0x30,
    EXT_DROP_POLICY = 0x31,
    EXT_ALTER_TABLE_RLS = 0x32,
};
```

**Bytecode Format**:
```
CREATE_POLICY:
  EXTENDED_OPCODE (1 byte)
  EXT_CREATE_POLICY (1 byte)
  policy_name (StringPool ID)
  table_name (StringPool ID)
  policy_type (1 byte)
  role_count (uint32_t)
  role_1..role_N (StringPool IDs)
  using_expr (serialized expression tree)
  with_check_expr (serialized expression tree or null)

DROP_POLICY:
  EXTENDED_OPCODE (1 byte)
  EXT_DROP_POLICY (1 byte)
  policy_name (StringPool ID)
  table_name (StringPool ID)
  if_exists (1 byte)

ALTER_TABLE_RLS:
  EXTENDED_OPCODE (1 byte)
  EXT_ALTER_TABLE_RLS (1 byte)
  table_name (StringPool ID)
  rls_operation (1 byte) -- ENABLE=1, DISABLE=2, FORCE=3, NO_FORCE=4
```

**Executor Methods**:
```cpp
void Executor::executeCreatePolicy();
void Executor::executeDropPolicy();
void Executor::executeAlterTableRLS();
```

**Estimated**: ~100-150 lines

---

### Phase 3.4.5: Query Planner Integration (4-6 hours)
**Goal**: Inject policy predicates into query plans

**This is the most complex phase** - requires modifying the query planner to:
1. Detect when RLS is enabled on accessed tables
2. Load applicable policies for current user
3. Combine policy USING expressions with AND/OR logic
4. Inject combined predicate into WHERE clause
5. Optimize predicate pushdown

**Query Planner Changes**:
```cpp
class QueryPlanner {
    // Check if RLS applies to this query
    bool needsRLSCheck(const ID& table_id, OperationType op_type) {
        // Skip RLS for:
        // 1. Superusers (unless FORCE RLS)
        // 2. Table owners (unless FORCE RLS)
        // 3. Tables with RLS disabled
        // 4. System tables

        bool rls_enabled, rls_forced;
        catalog_->getTableRLS(table_id, rls_enabled, rls_forced);

        if (!rls_enabled) return false;
        if (is_superuser && !rls_forced) return false;
        if (is_table_owner && !rls_forced) return false;

        return true;
    }

    // Get applicable policies and build combined predicate
    Expression* buildRLSPredicate(const ID& table_id, OperationType op_type) {
        std::vector<PolicyInfo> policies;
        catalog_->getPoliciesForUser(table_id, current_user_id,
                                     mapOperationType(op_type), policies);

        if (policies.empty()) {
            // No policies = no access (restrictive default)
            return makeLiteralBool(false);
        }

        // Combine policies with OR (permissive)
        // Multiple policies allow access if ANY policy allows
        Expression* combined = nullptr;
        for (const auto& policy : policies) {
            Expression* policy_expr = parseExpression(policy.using_expr);
            if (combined == nullptr) {
                combined = policy_expr;
            } else {
                combined = makeOrExpression(combined, policy_expr);
            }
        }

        return combined;
    }

    // Inject RLS predicate into plan
    void applyRLS(QueryPlan& plan) {
        for (auto& node : plan.scan_nodes) {
            if (needsRLSCheck(node.table_id, SELECT)) {
                Expression* rls_predicate = buildRLSPredicate(node.table_id, SELECT);

                // Combine with existing WHERE clause
                if (node.filter != nullptr) {
                    node.filter = makeAndExpression(node.filter, rls_predicate);
                } else {
                    node.filter = rls_predicate;
                }
            }
        }
    }
};
```

**Key Challenges**:
1. **Expression Parsing**: Policy expressions stored as strings must be parsed back into AST
2. **Predicate Combination**: Multiple policies require OR logic (any policy allows access)
3. **Optimization**: RLS predicates should be pushed down to scan nodes
4. **Security**: Predicates must not be bypassed by query optimization

**Estimated**: ~200-300 lines

---

### Phase 3.4.6: Executor DML Integration (3-4 hours)
**Goal**: Apply WITH CHECK policies to INSERT/UPDATE

**Executor Changes**:
```cpp
void Executor::executeInsert() {
    // ... existing code ...

    // Check if table has RLS enabled
    bool rls_enabled, rls_forced;
    db_->catalog_manager()->getTableRLS(table_id, rls_enabled, rls_forced);

    if (rls_enabled && needsRLSCheck(table_id, INSERT)) {
        // Get INSERT policies with WITH CHECK clauses
        std::vector<PolicyInfo> policies;
        db_->catalog_manager()->getPoliciesForUser(
            table_id, current_user_id, PolicyType::INSERT, policies);

        // Evaluate WITH CHECK for each inserted row
        for (const auto& row : rows_to_insert) {
            bool allowed = false;
            for (const auto& policy : policies) {
                if (!policy.with_check_expr.empty()) {
                    if (evaluateWithCheck(policy.with_check_expr, row)) {
                        allowed = true;
                        break;  // OR logic - any policy allows
                    }
                }
            }

            if (!allowed) {
                error("New row violates row-level security policy");
            }
        }
    }

    // ... continue with insert ...
}

void Executor::executeUpdate() {
    // Similar logic for UPDATE:
    // 1. USING clause determines which rows can be updated (visibility)
    // 2. WITH CHECK clause determines if new values are allowed
}
```

**Key Points**:
- INSERT: Only WITH CHECK matters (row doesn't exist yet)
- UPDATE: USING determines visibility, WITH CHECK validates new values
- DELETE: Only USING matters (WITH CHECK not applicable)
- Error message must not leak information about filtered rows

**Estimated**: ~150-200 lines

---

### Phase 3.4.7: Testing (2-3 hours)
**Goal**: Comprehensive integration tests

**Test Cases**:
1. **Basic Policy Creation**: CREATE POLICY with USING clause
2. **Policy Application**: SELECT with policy filters rows correctly
3. **Multiple Policies**: OR logic combines multiple policies
4. **Role-Specific Policies**: TO clause restricts policy to specific roles
5. **INSERT Policies**: WITH CHECK clause validates inserted rows
6. **UPDATE Policies**: USING + WITH CHECK combination
7. **DELETE Policies**: USING clause filters deletable rows
8. **RLS Enable/Disable**: ALTER TABLE ... ROW LEVEL SECURITY
9. **Force RLS**: Table owners still subject to RLS
10. **Superuser Bypass**: Superusers bypass RLS (unless forced)
11. **Policy Denial**: Error when no policy allows access
12. **Performance**: RLS overhead measurement

**Test File**: `tests/integration/test_security_phase3_4_rls.cpp`

**Estimated**: ~500-600 lines

---

## Technical Challenges

### Challenge 1: Expression Storage & Retrieval
**Problem**: Policy USING/WITH CHECK expressions must be stored as strings and parsed back

**Solution Options**:
1. **Store AST as JSON**: Serialize expression tree to JSON format
2. **Store SQL String**: Store original SQL, re-parse on load
3. **Store Bytecode**: Store compiled bytecode, faster but less portable

**Recommendation**: Store SQL string (option 2) for simplicity and debuggability. Cache parsed expressions in memory.

### Challenge 2: Policy Combination Logic
**Problem**: Multiple policies require OR logic, but query optimizer may not handle complex ORs well

**Solution**:
- Combine policies at planner level before optimization
- Use predicate pushdown to move combined filter close to scan
- Consider policy-specific indexes (future optimization)

### Challenge 3: Security vs Performance
**Problem**: RLS adds overhead to every query on protected tables

**Solution**:
- Fast path for tables without RLS (check flag first)
- Cache parsed policy expressions
- Superuser bypass optimization
- Consider policy compilation to bytecode (future)

### Challenge 4: Error Messages
**Problem**: RLS errors must not leak information about filtered rows

**Generic Error**: "Permission denied: Row-level security policy violation"
**Bad Error**: "Row with id=42 violates policy orders_user_policy"

**Solution**: Always use generic error messages for RLS violations.

---

## Performance Considerations

### Overhead Analysis

**Table without RLS** (95% of tables):
- Check: Is RLS enabled? (O(1) - metadata flag)
- Overhead: ~5 μs
- **No additional cost**

**Table with RLS, no applicable policies** (rare):
- Check: Is RLS enabled? Yes
- Check: Get policies for user (O(log N) catalog lookup)
- Result: No access (restrictive default)
- Overhead: ~100 μs

**Table with RLS, 1 policy**:
- Check: Is RLS enabled? Yes
- Load: Get policies for user
- Parse: Parse USING expression (cached)
- Inject: Add WHERE clause to plan
- Execute: Filtered scan
- Overhead: ~200-500 μs (parsing) + scan cost

**Table with RLS, 5 policies**:
- Same as above but OR 5 expressions together
- Overhead: ~500-1000 μs (parsing) + scan cost

### Optimization Strategies

1. **Cache Parsed Expressions**: Parse policy expressions once, cache in memory
2. **Predicate Pushdown**: Move RLS filters close to table scans
3. **Index Utilization**: Use indexes on policy predicate columns
4. **Superuser Bypass**: Skip RLS checks for superusers (unless forced)
5. **Owner Bypass**: Skip RLS checks for table owners (unless forced)

---

## Security Properties

### 1. Cannot Be Bypassed
- RLS predicates added at planner level (before optimization)
- Query optimizer cannot remove security predicates
- No SQL injection risk (expressions are pre-parsed)

### 2. Fail-Safe Defaults
- No policies = no access (restrictive)
- Invalid policy expression = deny access
- Missing roles = apply to all roles

### 3. Audit Trail
- All policy creation/modification logged
- Policy evaluation can be logged (future)
- Track which policies applied to each query (future)

### 4. Privilege Requirements
- Only superusers can create/drop policies
- Only superusers can enable/disable RLS on tables
- Table owners affected by FORCE RLS

---

## SQL Examples

### Example 1: Multi-Tenant Application
```sql
-- Setup
CREATE TABLE documents (
    doc_id INT PRIMARY KEY,
    tenant_id INT NOT NULL,
    title VARCHAR(200),
    content TEXT
);

-- Create policy: Users only see their tenant's documents
CREATE POLICY tenant_isolation ON documents
    FOR ALL
    USING (tenant_id = current_tenant_id());

-- Enable RLS
ALTER TABLE documents ENABLE ROW LEVEL SECURITY;

-- Now queries are automatically filtered:
SELECT * FROM documents;
-- Becomes: SELECT * FROM documents WHERE tenant_id = current_tenant_id();
```

### Example 2: Hierarchical Permissions
```sql
-- Setup
CREATE TABLE employees (
    emp_id INT PRIMARY KEY,
    name VARCHAR(100),
    manager_id INT,
    salary INT
);

-- Policy 1: Employees can see their own record
CREATE POLICY employees_self ON employees
    FOR SELECT
    USING (emp_id = current_user_id());

-- Policy 2: Managers can see their reports
CREATE POLICY employees_manager ON employees
    FOR SELECT
    USING (manager_id = current_user_id());

-- Policy 3: HR can see all employees
CREATE POLICY employees_hr ON employees
    FOR ALL
    TO hr_role
    USING (true);

ALTER TABLE employees ENABLE ROW LEVEL SECURITY;
```

### Example 3: Time-Based Access
```sql
-- Setup
CREATE TABLE financial_records (
    record_id INT PRIMARY KEY,
    fiscal_year INT,
    amount DECIMAL,
    created_at TIMESTAMP
);

-- Policy: Users only see records from last 2 years
CREATE POLICY recent_records ON financial_records
    FOR SELECT
    USING (created_at >= NOW() - INTERVAL '2 years');

-- Policy: CFO sees all records
CREATE POLICY cfo_all_records ON financial_records
    FOR ALL
    TO cfo_role
    USING (true);

ALTER TABLE financial_records ENABLE ROW LEVEL SECURITY;
```

### Example 4: INSERT with WITH CHECK
```sql
-- Setup
CREATE TABLE orders (
    order_id INT PRIMARY KEY,
    user_id INT NOT NULL,
    amount DECIMAL,
    status VARCHAR(20)
);

-- Policy: Users can only insert orders for themselves
CREATE POLICY orders_insert ON orders
    FOR INSERT
    WITH CHECK (user_id = current_user_id() AND amount > 0);

-- Policy: Users can only update their own orders (and only if pending)
CREATE POLICY orders_update ON orders
    FOR UPDATE
    USING (user_id = current_user_id() AND status = 'pending')
    WITH CHECK (user_id = current_user_id() AND status IN ('pending', 'cancelled'));

ALTER TABLE orders ENABLE ROW LEVEL SECURITY;
```

---

## Success Criteria

Phase 3.4 is complete when:

- [ ] pg_policies catalog table created
- [ ] Policy CRUD operations implemented
- [ ] CREATE POLICY / DROP POLICY syntax parsed
- [ ] ALTER TABLE ... ROW LEVEL SECURITY syntax parsed
- [ ] Bytecode generation for policy DDL
- [ ] Executor implements policy DDL
- [ ] Query planner injects RLS predicates for SELECT
- [ ] Executor applies WITH CHECK for INSERT/UPDATE
- [ ] 12+ integration tests pass
- [ ] Documentation complete
- [ ] Code compiles with no errors
- [ ] Performance overhead measured

---

## Estimated Timeline

| Phase | Task | Time | Lines |
|-------|------|------|-------|
| 3.4.1 | Catalog Schema | 2-3h | ~50 |
| 3.4.2 | Policy CRUD | 3-4h | ~200 |
| 3.4.3 | SQL Parser | 3-4h | ~220 |
| 3.4.4 | Bytecode/Executor DDL | 2-3h | ~150 |
| 3.4.5 | Query Planner Integration | 4-6h | ~300 |
| 3.4.6 | Executor DML Integration | 3-4h | ~200 |
| 3.4.7 | Testing | 2-3h | ~600 |
| **Total** | **Phase 3.4** | **19-27h** | **~1720** |

**Realistic Estimate**: 20-25 hours over 3-4 sessions

---

## Files to Modify

### Phase 3.4.1-3.4.2 (Catalog)
1. `include/scratchbird/core/catalog_manager.h` - Add PolicyInfo struct, method declarations
2. `src/core/catalog_manager.cpp` - Implement policy CRUD methods

### Phase 3.4.3 (Parser)
3. `include/scratchbird/parser/ast.h` - Add CreatePolicyStmt, DropPolicyStmt, extend AlterTableStmt
4. `src/parser/parser.cpp` - Parse CREATE POLICY, DROP POLICY, ALTER TABLE RLS
5. `src/parser/semantic_analyzer.cpp` - Validate policy statements

### Phase 3.4.4 (Bytecode)
6. `include/scratchbird/sblr/opcodes.h` - Add new opcodes
7. `src/sblr/bytecode_generator.cpp` - Generate bytecode for policy statements
8. `src/sblr/executor.cpp` - Execute policy DDL statements

### Phase 3.4.5-3.4.6 (Integration)
9. `include/scratchbird/optimizer/query_planner.h` - Add RLS methods
10. `src/optimizer/query_planner.cpp` - Inject RLS predicates
11. `src/sblr/executor.cpp` (DML methods) - Apply WITH CHECK clauses

### Phase 3.4.7 (Testing)
12. `tests/integration/test_security_phase3_4_rls.cpp` - Integration tests

**Total**: ~12 files modified/created

---

## Risks & Mitigation

### Risk 1: Query Optimizer Complexity
**Risk**: RLS predicate injection may break query optimizer assumptions

**Mitigation**:
- Inject predicates early in planning (before optimization)
- Mark RLS predicates as non-removable
- Extensive testing with complex queries

### Risk 2: Performance Degradation
**Risk**: RLS overhead may be unacceptable for high-throughput queries

**Mitigation**:
- Fast path for tables without RLS
- Cache parsed expressions
- Measure overhead and optimize hot paths

### Risk 3: Expression Parsing Complexity
**Risk**: Parsing stored policy expressions back into AST may be error-prone

**Mitigation**:
- Store original SQL strings (easier to debug)
- Comprehensive validation during CREATE POLICY
- Cache parsed expressions to avoid repeated parsing

### Risk 4: Security Bypass
**Risk**: Query optimizer or executor bug could bypass RLS

**Mitigation**:
- Add security predicates at planner level (can't be optimized away)
- Mark predicates as security-critical
- Extensive security testing

---

## Future Enhancements (Phase 3.5+)

1. **ALTER POLICY** - Modify existing policies
2. **Restrictive Policies** - AND logic (must satisfy all policies)
3. **Policy Debugging** - EXPLAIN shows which policies applied
4. **Policy Dependencies** - Track objects referenced by policies
5. **Policy Performance** - Compile policies to bytecode
6. **Policy Audit Log** - Track policy evaluations
7. **Default Policies** - Apply policies to all tables in schema

---

## Conclusion

Phase 3.4 (Row-Level Security) is a complex but essential feature for fine-grained access control. The implementation follows PostgreSQL's design, ensuring compatibility and leveraging proven security patterns.

**Complexity**: High (requires query planner integration)
**Value**: Very High (completes fine-grained access control)
**Risk**: Medium (query optimizer integration is tricky)

**Recommendation**: Proceed with Phase 3.4 after Phase 3.3 is stable and tested.

---

**Document Created**: November 11, 2025
**Status**: Planning - Ready for Implementation
**Estimated Start**: After Phase 3.3 stabilization
**Estimated Duration**: 20-25 hours (3-4 sessions)

