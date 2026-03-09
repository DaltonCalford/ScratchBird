# Specification: RLS Policy Syntax

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/rls/syntax |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp` (RLS parsing)
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp` (RLS records)
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/integration/test_security_phase3_4_rls.cpp`

## Synopsis

This specification defines the SQL syntax for Row-Level Security (RLS) policy management, including CREATE POLICY, ALTER POLICY, DROP POLICY, and related commands.

## Scope

### In Scope

- CREATE POLICY syntax and semantics
- ALTER POLICY syntax and semantics
- DROP POLICY syntax and semantics
- ALTER TABLE ... ENABLE/DISABLE ROW LEVEL SECURITY
- ALTER TABLE ... FORCE ROW LEVEL SECURITY

### Out of Scope

- RLS policy enforcement (see `rls_policy_enforcement.md`)
- RLS performance optimization (see `rls_performance.md`)
- Policy expression evaluation

## Background

Row-Level Security policies control which rows users can access or modify based on policy expressions evaluated at query time.

## Specification

### Data Structures

```cpp
// RLS Policy Record
struct RlsPolicyRecord {
    ID policy_id;              // UUIDv7
    ID table_id;               // Target table
    std::string policy_name;   // Unique per table
    
    // Policy definition
    ID using_expression_id;    // Query ID for USING clause
    ID with_check_expression_id; // Query ID for WITH CHECK clause
    
    // Policy applicability
    uint32_t applicable_commands; // Bitmask: ALL, SELECT, INSERT, UPDATE, DELETE
    
    // Role filtering
    std::vector<ID> to_roles;  // Empty = public
    bool to_public = false;
    
    // Enforcement mode
    bool is_restrictive = false; // false=PERMISSIVE, true=RESTRICTIVE
    
    // Status
    bool is_enabled = true;
    
    // Metadata
    ID owner_id;
    // ... timestamps
};
```

### CREATE POLICY

```sql
CREATE POLICY policy_name ON table_name
    [ AS { PERMISSIVE | RESTRICTIVE } ]
    [ FOR { ALL | SELECT | INSERT | UPDATE | DELETE } ]
    [ TO { role_name | PUBLIC | CURRENT_USER | SESSION_USER } [, ...] ]
    [ USING ( using_expression ) ]
    [ WITH CHECK ( check_expression ) ]
```

**Parameters:**

| Parameter | Description | Default |
|-----------|-------------|---------|
| `policy_name` | Unique name within table | Required |
| `table_name` | Table to apply policy | Required |
| `AS` | Policy type: PERMISSIVE or RESTRICTIVE | PERMISSIVE |
| `FOR` | Commands policy applies to | ALL |
| `TO` | Roles policy applies to | PUBLIC |
| `USING` | Expression for read operations | TRUE |
| `WITH CHECK` | Expression for write validation | USING expression |

### ALTER POLICY

```sql
ALTER POLICY policy_name ON table_name RENAME TO new_policy_name;

ALTER POLICY policy_name ON table_name
    [ TO { role_name | PUBLIC | CURRENT_USER | SESSION_USER } [, ...] ]
    [ USING ( using_expression ) ]
    [ WITH CHECK ( check_expression ) ];
```

### DROP POLICY

```sql
DROP POLICY [ IF EXISTS ] policy_name ON table_name [ CASCADE | RESTRICT ];
```

### Table RLS Control

```sql
-- Enable/disable RLS on table
ALTER TABLE table_name ENABLE ROW LEVEL SECURITY;
ALTER TABLE table_name DISABLE ROW LEVEL SECURITY;

-- Force RLS even for table owner
ALTER TABLE table_name FORCE ROW LEVEL SECURITY;
ALTER TABLE table_name NO FORCE ROW LEVEL SECURITY;
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

## Syntax Examples

### Basic Policies

```sql
-- Simple user isolation policy
CREATE POLICY user_isolation ON orders
    USING (user_id = current_user_id());

-- Per-tenant isolation
CREATE POLICY tenant_isolation ON orders
    USING (tenant_id = current_setting('app.current_tenant')::int);

-- Manager sees all employees, employees see themselves
CREATE POLICY employee_view ON employees
    USING (
        current_user_role() = 'manager' 
        OR employee_id = current_user_id()
    );
```

### Command-Specific Policies

```sql
-- Different policies for different operations
CREATE POLICY select_own_orders ON orders
    FOR SELECT
    USING (user_id = current_user_id());

CREATE POLICY insert_own_orders ON orders
    FOR INSERT
    WITH CHECK (user_id = current_user_id());

CREATE POLICY update_own_orders ON orders
    FOR UPDATE
    USING (user_id = current_user_id())
    WITH CHECK (status = 'pending');

CREATE POLICY delete_own_orders ON orders
    FOR DELETE
    USING (user_id = current_user_id() AND status = 'pending');
```

### Role-Based Policies

```sql
-- Admin sees everything
CREATE POLICY admin_all ON orders
    TO admin_role
    USING (true);

-- Manager sees own team's orders
CREATE POLICY manager_team ON orders
    TO manager_role
    USING (team_id = current_user_team_id());

-- Regular users see only own orders
CREATE POLICY user_own ON orders
    TO PUBLIC
    USING (user_id = current_user_id());
```

### RESTRICTIVE Policies

```sql
-- RESTRICTIVE policy for classified data
CREATE POLICY classified_restriction ON documents
    AS RESTRICTIVE
    USING (classification_level <= user_clearance_level());

-- RESTRICTIVE policy for data retention
CREATE POLICY retention_policy ON logs
    AS RESTRICTIVE
    USING (created_at > current_date - interval '7 years');
```

### Combining PERMISSIVE and RESTRICTIVE

```sql
-- PERMISSIVE: Users see their own or shared docs
CREATE POLICY user_documents ON documents
    AS PERMISSIVE
    USING (
        owner_id = current_user_id()
        OR is_shared = true
    );

-- RESTRICTIVE: But must meet clearance requirements
CREATE POLICY clearance_check ON documents
    AS RESTRICTIVE
    USING (clearance_required <= user_clearance());

-- Result: Documents must satisfy BOTH policies
```

### WITH CHECK Examples

```sql
-- Ensure users can only insert orders for themselves
CREATE POLICY insert_check ON orders
    FOR INSERT
    WITH CHECK (user_id = current_user_id());

-- Ensure status transitions are valid
CREATE POLICY update_check ON orders
    FOR UPDATE
    WITH CHECK (
        status IN ('pending', 'confirmed', 'shipped')
    );

-- Complex WITH CHECK
CREATE POLICY order_validation ON orders
    FOR INSERT
    WITH CHECK (
        user_id = current_user_id()
        AND created_at >= current_date - interval '1 day'
        AND total_amount > 0
    );
```

### Policy Modification

```sql
-- Rename policy
ALTER POLICY user_orders ON orders RENAME TO customer_orders;

-- Change policy expression
ALTER POLICY customer_orders ON orders
    USING (customer_id = current_user_id() AND is_active = true);

-- Change applicable roles
ALTER POLICY customer_orders ON orders
    TO customer_role, support_role;

-- Change WITH CHECK
ALTER POLICY customer_orders ON orders
    WITH CHECK (customer_id = current_user_id());
```

### Complete Multi-Tenant Example

```sql
-- Setup
CREATE TABLE orders (
    id UUID PRIMARY KEY DEFAULT gen_uuid(),
    tenant_id INT NOT NULL,
    user_id UUID NOT NULL,
    total_amount DECIMAL(10,2),
    status VARCHAR(20),
    created_at TIMESTAMP DEFAULT now()
);

-- Enable RLS
ALTER TABLE orders ENABLE ROW LEVEL SECURITY;
ALTER TABLE orders FORCE ROW LEVEL SECURITY;

-- Superadmin sees everything
CREATE POLICY admin_all ON orders
    TO admin_role
    USING (true);

-- Tenant admin sees own tenant
CREATE POLICY tenant_admin ON orders
    TO tenant_admin_role
    USING (tenant_id = current_setting('app.tenant_id')::int);

-- Users see only their own orders in their tenant
CREATE POLICY user_orders ON orders
    TO PUBLIC
    USING (
        tenant_id = current_setting('app.tenant_id')::int
        AND user_id = current_user_id()
    );

-- Users can only insert orders for themselves in their tenant
CREATE POLICY user_insert ON orders
    FOR INSERT
    TO PUBLIC
    WITH CHECK (
        tenant_id = current_setting('app.tenant_id')::int
        AND user_id = current_user_id()
    );

-- Users can only update their own pending orders
CREATE POLICY user_update ON orders
    FOR UPDATE
    TO PUBLIC
    USING (
        tenant_id = current_setting('app.tenant_id')::int
        AND user_id = current_user_id()
        AND status = 'pending'
    );
```

## Parsing Algorithm

```
Algorithm: Parse CREATE POLICY

Input: SQL statement
Output: RlsPolicyRecord

1. PARSE "CREATE POLICY"
2. EXTRACT policy_name
3. PARSE "ON"
4. EXTRACT table_name (validate exists)

5. INITIALIZE policy:
   - is_restrictive = false
   - applicable_commands = RLS_ALL
   - to_public = true
   - using_expr = NULL
   - with_check_expr = NULL

6. OPTIONAL: "AS {PERMISSIVE | RESTRICTIVE}"
   - "RESTRICTIVE" -> is_restrictive = true

7. OPTIONAL: "FOR {ALL | SELECT | INSERT | UPDATE | DELETE}"
   - "ALL" -> applicable_commands = RLS_ALL
   - "SELECT" -> applicable_commands = RLS_SELECT
   - "INSERT" -> applicable_commands = RLS_INSERT
   - "UPDATE" -> applicable_commands = RLS_UPDATE
   - "DELETE" -> applicable_commands = RLS_DELETE

8. OPTIONAL: "TO role_list"
   - Parse role names
   - "PUBLIC" -> to_public = true
   - Otherwise: lookup role IDs
   - to_public = false if specific roles

9. OPTIONAL: "USING (expression)"
   - Parse expression to AST
   - Store expression ID

10. OPTIONAL: "WITH CHECK (expression)"
    - Parse expression to AST
    - Store expression ID
    - If not specified, copy USING expression

11. VALIDATE:
    - At least one of USING or WITH CHECK must be specified
    - Policy name is unique on table
    - Roles exist

12. RETURN policy record
```

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `DUPLICATE_POLICY` | Policy name exists on table | Choose unique name |
| `UNDEFINED_TABLE` | Table doesn't exist | Create table first |
| `UNDEFINED_ROLE` | Role doesn't exist | Create role first |
| `UNDEFINED_COLUMN` | Column in expression doesn't exist | Fix expression |
| `SYNTAX_ERROR` | Invalid expression syntax | Fix expression |
| `INSUFFICIENT_PRIVILEGE` | No ALTER privilege on table | Request privilege |

## System Catalog Views

```sql
-- View all policies
SELECT 
    schemaname,
    tablename,
    policyname,
    permissive,
    roles,
    cmd,
    qual as using_expression,
    with_check
FROM pg_policies;

-- View policies for specific table
SELECT * FROM pg_policies WHERE tablename = 'orders';
```

## Invariants

1. **Unique Names**: Policy names must be unique per table
   - Verification: UNIQUE constraint on (table_id, policy_name)

2. **Valid Commands**: Applicable commands must be valid bitmask
   - Verification: Validate against enum values

3. **Expression Required**: USING or WITH CHECK must be specified
   - Verification: Check at parse time

4. **Role Existence**: All roles in TO clause must exist
   - Verification: Role lookup during creation

## Related Specifications

- `rls_policy_enforcement.md` - Policy evaluation and enforcement
- `rls_performance.md` - Performance optimization
- `authorization_model.md` - Authorization system

## Appendix

### Policy Naming Conventions

| Pattern | Use Case |
|---------|----------|
| `{action}_{resource}_{scope}` | `select_orders_own` |
| `{role}_{resource}_{access}` | `admin_orders_all` |
| `{constraint}_{resource}` | `tenant_isolation_orders` |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
