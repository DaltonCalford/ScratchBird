# Specification: RLS Policy Enforcement

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/rls |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp` (RLS records)
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp` (RLS enforcement)
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp` (RLS policy parsing)
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/integration/test_security_phase3_4_rls.cpp`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/integration/test_security_phase3_5_rls_dml.cpp`

## Synopsis

This specification defines Row-Level Security (RLS) policy enforcement in ScratchBird, including policy definition, predicate generation, query rewriting integration, and enforcement modes (RESTRICTIVE/PERMISSIVE). RLS enables fine-grained access control at the row level based on user attributes and session context.

## Scope

### In Scope

- RLS policy creation and storage
- Policy predicate evaluation
- Query rewriting for RLS
- FORCE ROW LEVEL SECURITY option
- RESTRICTIVE vs PERMISSIVE policy modes
- Bypass RLS privileges (BYPASSRLS)

### Out of Scope

- Authentication flows (see `authentication_flow.md`)
- Authorization model (see `authorization_model.md`)
- Column-Level Security (see `cls_column_masking.md`)
- Audit logging of RLS decisions

## Background

Row-Level Security (RLS) allows tables to have security policies that restrict which rows can be accessed or modified. Key concepts:

1. **Policy**: A rule combining a USING expression (for reads) and WITH CHECK expression (for writes)
2. **Predicate**: The SQL condition generated from policy evaluation
3. **Query Rewriting**: RLS predicates are injected into queries automatically
4. **Enforcement Modes**: PERMISSIVE (OR-combined) vs RESTRICTIVE (AND-combined)

## Specification

### Data Structures

```cpp
// RLS Policy Record (catalog table)
struct RlsPolicyRecord {
    ID policy_id;              // UUIDv7
    ID table_id;               // Target table
    std::string policy_name;   // Unique per table
    
    // Policy definition
    ID using_expression_id;    // Query ID for USING clause (0 if not applicable)
    ID with_check_expression_id; // Query ID for WITH CHECK clause
    
    // Policy applicability
    uint32_t applicable_commands; // Bitmask: ALL=1, SELECT=2, INSERT=4, UPDATE=8, DELETE=16
    
    // Role filtering
    std::vector<ID> to_roles;  // Empty = public (all roles)
    bool to_public = false;    // True if applies to all roles
    
    // Enforcement mode
    bool is_restrictive = false; // false=PERMISSIVE (default), true=RESTRICTIVE
    
    // Status
    bool is_enabled = true;
    
    // Metadata
    ID owner_id;               // Policy creator
    // ... timestamps
};
```

```cpp
// RLS Context for query execution
struct RlsContext {
    ID user_id;
    ID session_id;
    std::vector<ID> role_ids;
    
    // Session variables available to RLS expressions
    std::map<std::string, Value> session_variables;
    
    // Cached predicates per table
    std::map<ID, RlsPredicate> cached_predicates;
};

// RLS Predicate - the generated filter expression
struct RlsPredicate {
    ID table_id;
    CommandType command;       // SELECT, INSERT, UPDATE, DELETE
    
    // The actual predicate expression (AST or serialized)
    ExprNode* predicate_expr;
    std::string predicate_sql; // For EXPLAIN/debugging
    
    // Source policies
    std::vector<ID> source_policies;
    
    // Evaluation timestamp
    std::chrono::system_clock::time_point generated_at;
};
```

### Policy Command Bitmask

```cpp
enum RlsCommand : uint32_t {
    RLS_ALL    = 1,     // Applies to all commands
    RLS_SELECT = 2,     // Applies to SELECT
    RLS_INSERT = 4,     // Applies to INSERT
    RLS_UPDATE = 8,     // Applies to UPDATE
    RLS_DELETE = 16     // Applies to DELETE
};
```

### Interface Contracts

#### Function: `CatalogManager::createRlsPolicy()`

```cpp
// Source: catalog_manager.cpp (policy creation)
Status CatalogManager::createRlsPolicy(
    const std::string& policy_name,
    const ID& table_id,
    const std::string& using_expr_sql,      // Optional
    const std::string& with_check_expr_sql, // Optional
    uint32_t applicable_commands,
    const std::vector<std::string>& to_roles,
    bool is_restrictive,
    ID& out_policy_id,
    ErrorContext* ctx);
```

**Preconditions:**
- Table exists and user has ALTER privilege
- Policy name unique per table
- USING or WITH CHECK expression provided (at least one)
- User has CREATE RLS POLICY privilege

**Postconditions:**
- Policy record created in catalog
- Table marked as having RLS policies
- Policy epoch incremented for cache invalidation

**Error Handling:**
- `PERMISSION_DENIED`: User lacks ALTER privilege on table
- `DUPLICATE`: Policy with same name exists on table
- `INVALID_ARGUMENT`: Neither USING nor WITH CHECK provided
- `SYNTAX_ERROR`: Expression parsing failed

#### Function: `RlsEnforcer::getPredicateForTable()`

```cpp
// Source: sblr/executor.cpp (RLS enforcement point)
RlsPredicate RlsEnforcer::getPredicateForTable(
    const RlsContext& ctx,
    const ID& table_id,
    CommandType command);
```

**Preconditions:**
- RLS context initialized with user/roles
- Table has RLS enabled
- User does not have BYPASSRLS privilege

**Postconditions:**
- Returns combined predicate for all applicable policies
- RESTRICTIVE policies AND-combined
- PERMISSIVE policies OR-combined

**Algorithm:**
1. Check if user has BYPASSRLS privilege → return empty predicate
2. Fetch all enabled policies for table
3. Filter policies by applicable command
4. Filter policies by role membership (to_roles)
5. Separate RESTRICTIVE and PERMISSIVE policies
6. Generate predicates:
   - For USING: applicable for SELECT, UPDATE, DELETE
   - For WITH CHECK: applicable for INSERT, UPDATE
7. Combine:
   - RESTRICTIVE: `R1 AND R2 AND ...`
   - PERMISSIVE: `(P1 OR P2 OR ...) OR (no permissive policies → false)`
   - Final: `RESTRICTIVE_COMBINED AND PERMISSIVE_COMBINED`

#### Function: `Executor::applyRlsPredicates()`

```cpp
// Source: sblr/executor.cpp (query plan modification)
Status Executor::applyRlsPredicates(
    QueryPlan& plan,
    const RlsContext& ctx,
    ErrorContext* err_ctx);
```

**Preconditions:**
- Query plan is in logical/physical form
- RLS context is valid

**Postconditions:**
- Scan nodes on RLS tables have predicate filters added
- DML nodes have WITH CHECK constraints added

**Algorithm:**
```
For each scan_node in plan.table_scans:
    If table_has_rls_enabled(scan_node.table_id):
        predicate = rls_enforcer_.getPredicateForTable(
            ctx, scan_node.table_id, SELECT)
        
        If predicate is not empty:
            // Add to scan filter
            scan_node.filter = AND(scan_node.filter, predicate)
            
            // Mark as security quarantine for EXPLAIN
            scan_node.is_rls_filtered = true

For each dml_node in plan.dml_nodes:
    If table_has_rls_enabled(dml_node.table_id):
        // Get WITH CHECK predicate for INSERT/UPDATE
        with_check = rls_enforcer_.getWithCheckPredicate(
            ctx, dml_node.table_id, dml_node.command)
        
        // Get USING predicate for UPDATE/DELETE
        if dml_node.command in (UPDATE, DELETE):
            using_pred = rls_enforcer_.getUsingPredicate(
                ctx, dml_node.table_id, dml_node.command)
            dml_node.target_filter = using_pred
        
        // Add WITH CHECK for INSERT/UPDATE
        if dml_node.command in (INSERT, UPDATE):
            dml_node.with_check_constraint = with_check
```

### Algorithms

#### Algorithm: RLS Policy Evaluation

```
Input:  user_id, roles[], table_id, command, session_vars
Output: Combined predicate expression

1. CHECK BYPASSRLS
   if user_has_privilege(user_id, BYPASSRLS):
       return EMPTY_PREDICATE  // RLS disabled for this user

2. FETCH APPLICABLE POLICIES
   policies = catalog.getRlsPoliciesForTable(table_id)
   
   applicable = []
   for policy in policies:
       if not policy.is_enabled:
           continue
       if not (policy.applicable_commands & command):
           continue
       if not policy.to_public:
           if not intersects(policy.to_roles, roles):
               continue
       applicable.append(policy)

3. SEPARATE BY MODE
   restrictive = [p for p in applicable if p.is_restrictive]
   permissive  = [p for p in applicable if not p.is_restrictive]

4. GENERATE RESTRICTIVE PREDICATE
   restrictive_exprs = []
   for policy in restrictive:
       expr = getPolicyExpression(policy, command)  // USING or WITH CHECK
       if expr is not None:
           restrictive_exprs.append(expr)
   
   if restrictive_exprs is empty:
       restrictive_pred = TRUE
   else:
       restrictive_pred = AND(restrictive_exprs)

5. GENERATE PERMISSIVE PREDICATE
   permissive_exprs = []
   for policy in permissive:
       expr = getPolicyExpression(policy, command)
       if expr is not None:
           permissive_exprs.append(expr)
   
   if permissive_exprs is empty:
       // No permissive policies → deny all (unless there are restrictive only)
       if restrictive is not empty:
           permissive_pred = TRUE  // Restrictive policies alone
       else:
           permissive_pred = FALSE  // No policies → deny
   else:
       permissive_pred = OR(permissive_exprs)

6. COMBINE
   final_pred = AND(restrictive_pred, permissive_pred)
   
   return RlsPredicate{
       table_id = table_id,
       command = command,
       predicate_expr = final_pred,
       source_policies = applicable.policy_ids
   }
```

#### Algorithm: Policy Expression Generation

```
Input:  policy, command
Output: Expression node for the command

1. DETERMINE EXPRESSION TYPE
   switch command:
       case SELECT:
       case DELETE:
           // Only USING applies
           return policy.using_expression_id ? 
                  loadExpression(policy.using_expression_id) : NULL
                  
       case INSERT:
           // Only WITH CHECK applies
           return policy.with_check_expression_id ?
                  loadExpression(policy.with_check_expression_id) : NULL
                  
       case UPDATE:
           // Both USING (for which rows to update) and 
           // WITH CHECK (for new row values) apply
           using_expr = policy.using_expression_id ?
                        loadExpression(policy.using_expression_id) : NULL
           check_expr = policy.with_check_expression_id ?
                        loadExpression(policy.with_check_expression_id) : NULL
                        
           if using_expr and check_expr:
               // Different expressions - executor handles separately
               return {using_for_read=true, with_check_for_write=true}
           else if using_expr:
               return using_expr
           else if check_expr:
               return check_expr
           else:
               return NULL
```

### RLS Policy Combination Logic

```
Policy Combination Truth Table

RESTRICTIVE policies: R1, R2
PERMISSIVE policies: P1, P2

Case 1: Both types present
Final = (R1 AND R2) AND (P1 OR P2)

Case 2: Only RESTRICTIVE
Final = R1 AND R2  (all rows must satisfy restrictive policies)

Case 3: Only PERMISSIVE  
Final = P1 OR P2  (rows satisfying any permissive policy)

Case 4: No policies
Final = FALSE  (deny all - table has RLS but no applicable policies)
```

### Decision Trees

```
RLS Enforcement Decision
│
├─ Table has RLS enabled? ──No──► No predicate needed
│
├─ User has BYPASSRLS? ──Yes──► No predicate needed
│
├─ Table owner and not FORCE RLS? ──Yes──► No predicate needed
│
└─ Generate predicate:
   ├─ Fetch all policies for table
   │   └─ Filter: enabled AND applicable_command AND (public OR role_match)
   │
   ├─ Separate RESTRICTIVE and PERMISSIVE
   │
   ├─ Build RESTRICTIVE predicate (AND)
   │   └─ For each: Get USING (SELECT/UPDATE/DELETE) or WITH CHECK (INSERT/UPDATE)
   │
   ├─ Build PERMISSIVE predicate (OR)
   │   └─ Same expression selection logic
   │
   └─ Combine: RESTRICTIVE AND PERMISSIVE
```

## Invariants

1. **Policy Isolation**: RLS predicates cannot reference other tables' row data directly
   - Verification: Expression validator in parser

2. **Owner Override**: Table owners bypass RLS unless FORCE ROW LEVEL SECURITY is set
   - Verification: Check in `getPredicateForTable()` before policy evaluation

3. **Command-Specific Application**: Policies only apply to their specified commands
   - Verification: `applicable_commands` bitmask check

4. **Role-Based Filtering**: Policies apply only to specified roles or PUBLIC
   - Verification: Role intersection check in policy selection

5. **No Policy Bypass Without Privilege**: BYPASSRLS is the only way to skip RLS
   - Verification: Privilege check before predicate generation

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `PERMISSION_DENIED` | User lacks ALTER to create policy | Request appropriate privilege |
| `PERMISSION_DENIED` | RLS prevents access to row | Modify query or request policy change |
| `CONSTRAINT_VIOLATION` | WITH CHECK violation on INSERT/UPDATE | Modify values to satisfy policy |
| `INVALID_ARGUMENT` | Invalid policy expression | Fix expression syntax |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `test_security_phase3_4_rls.cpp` | RLS policy creation and SELECT filtering |
| `test_security_phase3_5_rls_dml.cpp` | RLS for INSERT, UPDATE, DELETE |

## Related Specifications

- `authentication_flow.md` - User identity for RLS context
- `authorization_model.md` - Table privileges for RLS management
- `rls_policy_syntax.md` - CREATE/ALTER/DROP POLICY syntax
- `rls_performance.md` - RLS optimization
- `cls_column_masking.md` - Column-level security complements RLS
- `audit_logging.md` - RLS policy violation logging

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| RLS | Row-Level Security - row access control |
| USING | Policy expression for read operations |
| WITH CHECK | Policy expression validating new rows |
| PERMISSIVE | Policy mode - OR-combined with other permissive |
| RESTRICTIVE | Policy mode - AND-combined with all policies |
| BYPASSRLS | Privilege to skip RLS evaluation |
| FORCE RLS | Table option requiring RLS even for owner |

### Example RLS Policies

```sql
-- Simple user isolation
CREATE POLICY user_isolation ON orders
    USING (user_id = current_user_id());

-- Manager can see all, employees see own
CREATE POLICY employee_view ON sales
    USING (employee_id = current_user_id());
    
CREATE POLICY manager_view ON sales
    USING (current_user_role() = 'manager');

-- RESTRICTIVE policy for data classification
CREATE POLICY classified_restriction ON documents
    AS RESTRICTIVE
    USING (classification_level <= user_clearance_level());

-- WITH CHECK for data integrity
CREATE POLICY insert_check ON orders
    WITH CHECK (created_at >= CURRENT_DATE - INTERVAL '1 day');
```

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
