### DDL: Row-Level Security (RLS) Policies

**What it is**

Row-Level Security (RLS) provides fine-grained access control at the row level, allowing different users to see different subsets of data in the same table. RLS policies define which rows are visible (SELECT), insertable (INSERT), updatable (UPDATE), or deletable (DELETE) for specific users or roles. This security mechanism operates transparently, automatically filtering data based on the current user's context.

**Why it matters**

- **Multi-tenancy**: Isolate tenant data in shared tables
- **Compliance**: Enforce data privacy regulations (GDPR, HIPAA)
- **Security**: Implement need-to-know access patterns
- **Simplification**: Eliminate complex view hierarchies
- **Transparency**: Applications don't need security logic

**How to use it**

Enable RLS on a table, create policies that define row visibility and modification rules using boolean expressions, then grant table-level permissions. The database automatically applies policies based on the current user, filtering results and preventing unauthorized modifications.

## Enabling Row-Level Security

```sql
-- Enable RLS on table
ALTER TABLE customers ENABLE ROW LEVEL SECURITY;

-- Disable RLS
ALTER TABLE customers DISABLE ROW LEVEL SECURITY;

-- Force RLS for table owners
ALTER TABLE sensitive_data FORCE ROW LEVEL SECURITY;

-- Check RLS status
SELECT 
    schemaname,
    tablename,
    rowsecurity,
    forcerowsecurity
FROM pg_tables
WHERE tablename = 'customers';
```

## CREATE POLICY

### Basic Syntax

```sql
-- Simple policy
CREATE POLICY policy_name ON table_name
    FOR operation
    TO roles
    USING (boolean_expression)
    WITH CHECK (boolean_expression);

-- All operations policy
CREATE POLICY tenant_isolation ON orders
    USING (tenant_id = current_setting('app.tenant_id')::INT);

-- SELECT policy
CREATE POLICY view_own_records ON customers
    FOR SELECT
    TO app_users
    USING (user_id = current_user_id());

-- INSERT policy
CREATE POLICY insert_own_records ON customers
    FOR INSERT
    TO app_users
    WITH CHECK (user_id = current_user_id());

-- UPDATE policy
CREATE POLICY update_own_records ON customers
    FOR UPDATE
    TO app_users
    USING (user_id = current_user_id())
    WITH CHECK (user_id = current_user_id());

-- DELETE policy
CREATE POLICY delete_own_records ON customers
    FOR DELETE
    TO app_users
    USING (user_id = current_user_id());
```

### Multi-Tenant Isolation

```sql
-- Set up multi-tenant table
ALTER TABLE projects ENABLE ROW LEVEL SECURITY;

-- Tenant isolation policy
CREATE POLICY tenant_isolation ON projects
    USING (tenant_id = current_setting('app.current_tenant')::UUID);

-- Admin bypass policy
CREATE POLICY admin_all ON projects
    TO admin_role
    USING (true);

-- Usage in application
SET app.current_tenant = '123e4567-e89b-12d3-a456-426614174000';
SELECT * FROM projects;  -- Only shows tenant's projects
```

### Department-Based Access

```sql
-- Employee table with department isolation
ALTER TABLE employees ENABLE ROW LEVEL SECURITY;

-- Employees see their department
CREATE POLICY same_department ON employees
    FOR SELECT
    USING (department_id IN (
        SELECT department_id 
        FROM employees 
        WHERE email = current_user
    ));

-- Managers see their subordinates
CREATE POLICY manager_view ON employees
    FOR SELECT
    TO managers
    USING (
        manager_id = (SELECT employee_id FROM employees WHERE email = current_user)
        OR employee_id = (SELECT employee_id FROM employees WHERE email = current_user)
    );

-- HR sees all
CREATE POLICY hr_full_access ON employees
    TO hr_role
    USING (true);
```

### Time-Based Access

```sql
-- Documents with expiration
ALTER TABLE documents ENABLE ROW LEVEL SECURITY;

-- Active documents only
CREATE POLICY active_documents ON documents
    FOR SELECT
    USING (
        (expires_at IS NULL OR expires_at > CURRENT_TIMESTAMP)
        AND (effective_date IS NULL OR effective_date <= CURRENT_TIMESTAMP)
    );

-- Archive access for specific role
CREATE POLICY archive_access ON documents
    FOR SELECT
    TO archivists
    USING (true);  -- See all including expired
```

## Complex Policy Examples

### Hierarchical Access

```sql
-- Organization hierarchy
ALTER TABLE org_data ENABLE ROW LEVEL SECURITY;

-- See own and subordinate data
CREATE POLICY hierarchical_access ON org_data
    FOR ALL
    USING (
        org_unit_id IN (
            WITH RECURSIVE org_tree AS (
                -- User's direct org unit
                SELECT org_unit_id
                FROM user_assignments
                WHERE user_id = current_user_id()
                
                UNION
                
                -- Subordinate org units
                SELECT o.org_unit_id
                FROM org_units o
                JOIN org_tree ot ON o.parent_id = ot.org_unit_id
            )
            SELECT org_unit_id FROM org_tree
        )
    );
```

### Geographic Restrictions

```sql
-- Regional data access
ALTER TABLE sales_data ENABLE ROW LEVEL SECURITY;

-- Region-based access
CREATE POLICY regional_access ON sales_data
    USING (
        region_id IN (
            SELECT region_id
            FROM user_regions
            WHERE user_id = current_user_id()
            AND valid_from <= CURRENT_DATE
            AND (valid_to IS NULL OR valid_to >= CURRENT_DATE)
        )
    );

-- Country-level override
CREATE POLICY country_manager ON sales_data
    TO country_managers
    USING (
        country_code = (
            SELECT country_code
            FROM user_roles
            WHERE user_id = current_user_id()
            AND role = 'country_manager'
        )
    );
```

### Data Classification

```sql
-- Classified documents
ALTER TABLE classified_docs ENABLE ROW LEVEL SECURITY;

-- Classification-based access
CREATE POLICY classification_access ON classified_docs
    USING (
        classification_level <= (
            SELECT MAX(clearance_level)
            FROM user_clearances
            WHERE user_id = current_user_id()
            AND status = 'active'
            AND expires_at > CURRENT_TIMESTAMP
        )
    );

-- Need-to-know basis
CREATE POLICY need_to_know ON classified_docs
    USING (
        classification_level <= 2  -- Unclassified/Confidential
        OR document_id IN (
            SELECT document_id
            FROM document_access_list
            WHERE user_id = current_user_id()
            AND (expires_at IS NULL OR expires_at > CURRENT_TIMESTAMP)
        )
    );
```

## ALTER POLICY

```sql
-- Rename policy
ALTER POLICY old_name ON table_name RENAME TO new_name;

-- Change roles
ALTER POLICY policy_name ON table_name TO new_roles;

-- Change USING clause
ALTER POLICY policy_name ON table_name 
    USING (new_boolean_expression);

-- Change WITH CHECK clause
ALTER POLICY policy_name ON table_name
    WITH CHECK (new_boolean_expression);

-- Complete alteration
ALTER POLICY tenant_isolation ON projects
    TO authenticated_users
    USING (tenant_id = get_current_tenant())
    WITH CHECK (tenant_id = get_current_tenant());
```

## DROP POLICY

```sql
-- Drop specific policy
DROP POLICY policy_name ON table_name;

-- Drop if exists
DROP POLICY IF EXISTS old_policy ON table_name;

-- Drop all policies (must drop individually)
DO $$
DECLARE
    pol RECORD;
BEGIN
    FOR pol IN 
        SELECT policyname 
        FROM pg_policies 
        WHERE tablename = 'target_table'
    LOOP
        EXECUTE format('DROP POLICY %I ON target_table', pol.policyname);
    END LOOP;
END $$;
```

## Policy Combinations

### Permissive vs Restrictive

```sql
-- Permissive policies (OR logic) - default
CREATE POLICY policy1 ON table_name AS PERMISSIVE
    USING (condition1);

CREATE POLICY policy2 ON table_name AS PERMISSIVE
    USING (condition2);
-- Row visible if condition1 OR condition2

-- Restrictive policies (AND logic)
CREATE POLICY must_be_active ON users AS RESTRICTIVE
    USING (status = 'active');

CREATE POLICY must_be_verified ON users AS RESTRICTIVE
    USING (email_verified = true);
-- Row visible only if BOTH conditions true
```

### Combining Operations

```sql
-- Different policies for different operations
ALTER TABLE tasks ENABLE ROW LEVEL SECURITY;

-- Anyone can view public tasks
CREATE POLICY view_public ON tasks
    FOR SELECT
    USING (is_public = true);

-- Users can view their private tasks
CREATE POLICY view_own ON tasks
    FOR SELECT
    USING (owner_id = current_user_id() AND is_public = false);

-- Only owners can update
CREATE POLICY update_own ON tasks
    FOR UPDATE
    USING (owner_id = current_user_id())
    WITH CHECK (owner_id = current_user_id());

-- Only owners can delete, but not completed tasks
CREATE POLICY delete_own ON tasks
    FOR DELETE
    USING (owner_id = current_user_id() AND status != 'completed');

-- Anyone can insert, but must own it
CREATE POLICY insert_tasks ON tasks
    FOR INSERT
    WITH CHECK (owner_id = current_user_id());
```

## Testing and Debugging

### Testing Policies

```sql
-- Test as different user
SET ROLE test_user;
SELECT * FROM protected_table;  -- Should see filtered results
RESET ROLE;

-- Force RLS for superuser testing
ALTER TABLE protected_table FORCE ROW LEVEL SECURITY;

-- Check visible rows for user
CREATE FUNCTION test_rls_visibility(
    test_user TEXT,
    table_name TEXT
) RETURNS TABLE (visible_count BIGINT) AS $$
BEGIN
    EXECUTE format('SET ROLE %I', test_user);
    RETURN QUERY EXECUTE format('SELECT COUNT(*) FROM %I', table_name);
    RESET ROLE;
END;
$$ LANGUAGE plpgsql;
```

### Debugging Policies

```sql
-- View all policies
SELECT 
    schemaname,
    tablename,
    policyname,
    permissive,
    roles,
    cmd,
    qual,
    with_check
FROM pg_policies
WHERE tablename = 'your_table'
ORDER BY policyname;

-- Explain plan with RLS
EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM protected_table
WHERE some_column = 'value';
-- Look for "Filter" nodes showing RLS conditions

-- Audit policy effectiveness
CREATE VIEW rls_audit AS
SELECT 
    current_user AS checking_user,
    t.tablename,
    COUNT(*) FILTER (WHERE p.policyname IS NOT NULL) AS policy_count,
    COUNT(*) FILTER (WHERE p.permissive = 'PERMISSIVE') AS permissive_count,
    COUNT(*) FILTER (WHERE p.permissive = 'RESTRICTIVE') AS restrictive_count
FROM pg_tables t
LEFT JOIN pg_policies p ON t.tablename = p.tablename
WHERE t.rowsecurity = true
GROUP BY t.tablename;
```

## Performance Considerations

### Indexing for Policies

```sql
-- Create indexes on policy columns
CREATE INDEX idx_tenant_id ON orders(tenant_id);
CREATE INDEX idx_user_id ON documents(user_id);
CREATE INDEX idx_department ON employees(department_id);

-- Composite indexes for complex policies
CREATE INDEX idx_classification_user 
    ON classified_docs(classification_level, document_id);

-- Partial indexes for common cases
CREATE INDEX idx_active_public 
    ON tasks(owner_id) 
    WHERE is_public = false;
```

### Policy Optimization

```sql
-- Avoid expensive subqueries
-- Bad: Subquery executed for each row
CREATE POLICY slow_policy ON large_table
    USING (id IN (SELECT id FROM complex_calculation()));

-- Good: Use JOIN or EXISTS
CREATE POLICY fast_policy ON large_table
    USING (EXISTS (
        SELECT 1 FROM user_permissions up
        WHERE up.table_id = large_table.id
        AND up.user_id = current_user_id()
    ));

-- Cache user context
CREATE FUNCTION get_user_departments() RETURNS INT[] AS $$
BEGIN
    RETURN ARRAY(
        SELECT department_id 
        FROM user_departments 
        WHERE user_id = current_user_id()
    );
END;
$$ LANGUAGE plpgsql STABLE;

CREATE POLICY dept_policy ON data
    USING (department_id = ANY(get_user_departments()));
```

## Best Practices

1. **Policy Design**
   - Keep policies simple and readable
   - Use functions for complex logic
   - Document policy intent

2. **Security**
   - Always use FORCE ROW LEVEL SECURITY for sensitive tables
   - Test policies thoroughly
   - Audit policy changes

3. **Performance**
   - Index policy predicate columns
   - Monitor query plans
   - Cache user context when possible

4. **Maintenance**
   - Version control policy definitions
   - Test policy changes in staging
   - Document policy dependencies

## Implementation Details

**Parser** (`src/engine/parser_ddl.cpp`):
- `parse_ddl_rls_policy`: CREATE/ALTER/DROP POLICY
- Handles USING and WITH CHECK clauses
- Parses permissive/restrictive modes

**Executor**:
- Applies RLS filters during query execution
- Combines multiple policies
- Handles policy exceptions

**Security Context**:
- Tracks current user
- Manages session variables
- Evaluates policy expressions

**Code Anchors**:
- RLS policy parser: `src/engine/parser_ddl.cpp` (parse_ddl_rls_policy)
- Policy evaluation: `src/engine/rls_executor.cpp`
- Security context: `src/engine/security_context.cpp`
- AST definitions: `include/scratchbird/engine/ast.h`

## See also

- [Roles & Grants](./ddl-roles-users-grants.md) - Table-level permissions
- [Views](./ddl-views.md) - Alternative access control
- [Session](./session-and-transaction.md) - Setting session variables
- [Functions](./psql-routines-and-triggers.md) - Policy helper functions
- [Performance](./explain-analyze.md) - Analyzing RLS impact