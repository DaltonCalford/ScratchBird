<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# Row-Level Security (RLS)

[Prev](./01_permission_model_and_grants.md) | [Next](./03_column_security_and_masking_patterns.md) | [Topic README](./README.md) | [Security Hardening README](../../README.md)

## Coverage and Evidence Status

Status: Complete

## Synopsis

Row-Level Security (RLS) restricts which rows users can see or modify based on policy predicates. Each query is automatically modified to include the policy conditions.

## How RLS Works

```
Without RLS:
┌─────────────┐
│ SELECT *    │ ──► Returns all rows
│ FROM orders │
└─────────────┘

With RLS:
┌─────────────┐
│ SELECT *    │ ──► Automatically transformed:
│ FROM orders │     SELECT * FROM orders 
└─────────────┘     WHERE user_id = current_user_id()
                    Returns only user's rows
```

## Enabling RLS

```sql
-- Enable RLS on table
ALTER TABLE orders ENABLE ROW LEVEL SECURITY;

-- Force RLS for table owner too
ALTER TABLE orders FORCE ROW LEVEL SECURITY;

-- Disable RLS
ALTER TABLE orders DISABLE ROW LEVEL SECURITY;
```

## Creating Policies

### Syntax

```sql
CREATE POLICY policy_name ON table_name
    [ AS { PERMISSIVE | RESTRICTIVE } ]
    [ FOR { ALL | SELECT | INSERT | UPDATE | DELETE } ]
    [ TO { role_name | PUBLIC | CURRENT_USER | SESSION_USER } [, ...] ]
    [ USING ( using_expression ) ]
    [ WITH CHECK ( check_expression ) ];
```

### Parameters

| Parameter | Description |
|-----------|-------------|
| `AS PERMISSIVE` | Rows visible if ANY permissive policy allows (default) |
| `AS RESTRICTIVE` | Rows visible only if ALL restrictive policies allow |
| `FOR` | Which operations policy applies to |
| `TO` | Which roles policy applies to |
| `USING` | Filter for SELECT/UPDATE/DELETE |
| `WITH CHECK` | Validation for INSERT/UPDATE |

## Policy Types

### SELECT Policy

Controls which rows can be read.

```sql
-- Users see only their own orders
CREATE POLICY user_orders_policy ON orders
    FOR SELECT
    TO app_user
    USING (user_id = current_user_id());

-- Managers see their team's orders
CREATE POLICY manager_orders_policy ON orders
    FOR SELECT
    TO manager_role
    USING (team_id IN (
        SELECT team_id FROM user_teams 
        WHERE user_id = current_user_id()
    ));
```

### INSERT Policy

Controls which rows can be inserted.

```sql
-- Users can only insert their own orders
CREATE POLICY user_insert_policy ON orders
    FOR INSERT
    TO app_user
    WITH CHECK (user_id = current_user_id());
```

### UPDATE Policy

Controls which rows can be updated.

```sql
-- Complex update policy
CREATE POLICY user_update_policy ON orders
    FOR UPDATE
    TO app_user
    USING (user_id = current_user_id())
    WITH CHECK (user_id = current_user_id());
```

### DELETE Policy

Controls which rows can be deleted.

```sql
-- Users can delete their own draft orders
CREATE POLICY user_delete_policy ON orders
    FOR DELETE
    TO app_user
    USING (user_id = current_user_id() AND status = 'draft');
```

### ALL Policy

Single policy for all operations.

```sql
CREATE POLICY tenant_isolation ON orders
    FOR ALL
    TO app_user
    USING (tenant_id = current_setting('app.current_tenant')::INTEGER)
    WITH CHECK (tenant_id = current_setting('app.current_tenant')::INTEGER);
```

## Complete Examples

### Multi-Tenant Application

```sql
-- Enable RLS
ALTER TABLE customers ENABLE ROW LEVEL SECURITY;
ALTER TABLE orders ENABLE ROW LEVEL SECURITY;

-- Tenant isolation policy
CREATE POLICY tenant_customers ON customers
    FOR ALL
    TO app_user
    USING (tenant_id = current_setting('app.tenant_id')::INTEGER);

CREATE POLICY tenant_orders ON orders
    FOR ALL  
    TO app_user
    USING (tenant_id = current_setting('app.tenant_id')::INTEGER);

-- Bypass for admin
CREATE POLICY admin_all_customers ON customers
    FOR ALL
    TO admin_role
    USING (true);

CREATE POLICY admin_all_orders ON orders
    FOR ALL
    TO admin_role  
    USING (true);
```

### User-Owned Data

```sql
-- Documents table
CREATE TABLE documents (
    id UUID PRIMARY KEY,
    owner_id UUID REFERENCES users(id),
    content TEXT,
    is_public BOOLEAN DEFAULT FALSE
);

ALTER TABLE documents ENABLE ROW LEVEL SECURITY;

-- Owner sees all their documents
CREATE POLICY owner_documents ON documents
    FOR ALL
    TO app_user
    USING (owner_id = current_user_id());

-- Public sees only public documents  
CREATE POLICY public_documents ON documents
    FOR SELECT
    TO PUBLIC
    USING (is_public = TRUE);

-- Shared documents
CREATE POLICY shared_documents ON documents
    FOR SELECT
    TO app_user
    USING (id IN (
        SELECT document_id FROM document_shares
        WHERE shared_with = current_user_id()
    ));
```

### Hierarchical Access

```sql
-- Employees table
CREATE TABLE employees (
    id UUID PRIMARY KEY,
    name TEXT,
    manager_id UUID REFERENCES employees(id),
    salary DECIMAL(10,2),
    department TEXT
);

ALTER TABLE employees ENABLE ROW LEVEL SECURITY;

-- Employees see themselves
CREATE POLICY self_employee ON employees
    FOR SELECT
    TO employee_role
    USING (id = current_user_id());

-- Managers see their reports
CREATE POLICY manager_reports ON employees
    FOR SELECT
    TO manager_role
    USING (id IN (
        WITH RECURSIVE reports AS (
            SELECT id FROM employees 
            WHERE manager_id = current_user_id()
            UNION ALL
            SELECT e.id FROM employees e
            JOIN reports r ON e.manager_id = r.id
        )
        SELECT id FROM reports
        UNION SELECT current_user_id()
    ));

-- HR sees all
CREATE POLICY hr_all ON employees
    FOR ALL
    TO hr_role
    USING (true);
```

## Policy Combination

### Multiple Policies on Same Table

```sql
-- Policy 1: Users see own data
CREATE POLICY p1 ON orders FOR SELECT TO app_user 
    USING (user_id = current_user_id());

-- Policy 2: Users see public orders
CREATE POLICY p2 ON orders FOR SELECT TO app_user 
    USING (is_public = TRUE);

-- Result: OR combination (PERMISSIVE default)
-- Users see: own orders OR public orders
```

### PERMISSIVE vs RESTRICTIVE

```sql
-- PERMISSIVE (OR logic)
CREATE POLICY p1 ON table1 FOR SELECT USING (condition1);
CREATE POLICY p2 ON table1 FOR SELECT USING (condition2);
-- Result: rows where condition1 OR condition2

-- RESTRICTIVE (AND logic)
CREATE POLICY r1 ON table1 FOR SELECT AS RESTRICTIVE USING (condition1);
CREATE POLICY r2 ON table1 FOR SELECT AS RESTRICTIVE USING (condition2);
-- Result: rows where condition1 AND condition2

-- Combined
CREATE POLICY p1 ON table1 FOR SELECT USING (permissive_condition);
CREATE POLICY r1 ON table1 FOR SELECT AS RESTRICTIVE USING (restrictive_condition);
-- Result: permissive_condition AND restrictive_condition
```

## Bypassing RLS

### BYPASSRLS Attribute

```sql
-- Create user that bypasses RLS
CREATE USER admin WITH BYPASSRLS;

-- Table owner bypasses unless FORCE RLS
ALTER TABLE orders FORCE ROW LEVEL SECURITY;
```

### Policy for All Roles

```sql
-- Policy that applies to everyone including owner
CREATE POLICY universal ON orders FOR ALL USING (true);
```

## Performance Considerations

### Index Requirements

RLS policies often benefit from indexes:

```sql
-- Policy: USING (user_id = current_user_id())
-- Needs index:
CREATE INDEX idx_orders_user_id ON orders(user_id);

-- Policy: USING (tenant_id = ...)
CREATE INDEX idx_orders_tenant ON orders(tenant_id);
```

### Policy Function Optimization

```sql
-- Mark function as stable/immutable
CREATE OR REPLACE FUNCTION get_current_tenant() RETURNS INTEGER AS $$
BEGIN
    RETURN current_setting('app.tenant_id')::INTEGER;
END;
$$ LANGUAGE plpgsql STABLE;  -- Important: STABLE not VOLATILE
```

## Common Patterns

### Soft Delete with RLS

```sql
ALTER TABLE orders ENABLE ROW LEVEL SECURITY;

-- Hide deleted rows
CREATE POLICY active_orders ON orders
    FOR ALL
    TO app_user
    USING (deleted_at IS NULL);

-- Admin sees all including deleted
CREATE POLICY admin_orders ON orders
    FOR ALL
    TO admin_role
    USING (true);
```

### Time-Based Access

```sql
-- Only show current records
CREATE POLICY current_records ON contracts
    FOR SELECT
    TO app_user
    USING (effective_date <= NOW() AND (end_date IS NULL OR end_date > NOW()));
```

### Audit Logging with RLS

```sql
-- Prevent modification of historical records
CREATE POLICY no_historical_update ON audit_log
    FOR UPDATE
    TO app_user
    USING (created_at > NOW() - INTERVAL '1 day');
```

## Validation

### Test RLS Policies

```sql
-- Test as specific user
SET ROLE app_user;
SET app.tenant_id = '123';

-- Should return only tenant 123 rows
SELECT * FROM orders;

-- Reset
RESET ROLE;
```

### Policy Catalog Query

```sql
-- View all policies
SELECT schemaname, tablename, policyname, permissive, roles, cmd, qual, with_check
FROM pg_policies
WHERE tablename = 'orders';
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `policy_violation` | WITH CHECK expression fails on INSERT/UPDATE |
| `insufficient_privilege` | No policy allows access |
| `undefined_policy` | Policy doesn't exist |

## Completion Checklist

- [x] Policy syntax documented
- [x] Policy types explained
- [x] Multiple examples provided
- [x] PERMISSIVE/RESTRICTIVE explained
- [x] Performance considerations
- [x] Common patterns
- [x] Validation methods

## See Also

- [Column-Level Security](03_column_security_and_masking_patterns.md)
- [Domain Security](04_domain_visibility_and_policy_enforcement.md)
- [CREATE POLICY](../../language_reference/syntax_guide/ddl/security_objects/10_create_policy.md)
