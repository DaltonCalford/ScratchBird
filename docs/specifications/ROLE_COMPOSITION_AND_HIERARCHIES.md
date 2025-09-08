# Role Composition and Hierarchical Role Management

## Overview

ScratchBird implements role composition through the standard SQL `GRANT ROLE TO ROLE` mechanism, creating powerful, dynamic permission hierarchies. Grouped roles automatically inherit and aggregate permissions from their member roles, with changes propagating through the hierarchy.

## Basic Role Composition

### Creating Composite Roles

```sql
-- Create base roles with specific permissions
CREATE ROLE read_only;
CREATE ROLE data_entry;
CREATE ROLE report_writer;
CREATE ROLE schema_designer;
CREATE ROLE security_admin;

-- Grant specific permissions to base roles
GRANT SELECT ON ALL TABLES IN SCHEMA public TO read_only;
GRANT INSERT, UPDATE ON ALL TABLES IN SCHEMA public TO data_entry;
GRANT CREATE VIEW, CREATE FUNCTION TO report_writer;
GRANT CREATE TABLE, CREATE INDEX, ALTER TABLE TO schema_designer;
GRANT CREATE ROLE, ALTER ROLE, DROP ROLE TO security_admin;

-- Create composite roles by granting roles to roles
CREATE ROLE analyst;
GRANT read_only TO analyst;
GRANT report_writer TO analyst;

CREATE ROLE developer;
GRANT read_only TO developer;
GRANT data_entry TO developer;
GRANT schema_designer TO developer;

CREATE ROLE admin;
GRANT developer TO admin;  -- Inherits all developer permissions
GRANT security_admin TO admin;  -- Plus security permissions

-- Create a superuser role that combines everything
CREATE ROLE super_admin;
GRANT admin TO super_admin;
GRANT ALL ROLES TO super_admin;  -- Special syntax for all roles
```

### Dynamic Permission Inheritance

```sql
-- Changes to base roles automatically propagate
GRANT DELETE ON ALL TABLES IN SCHEMA public TO data_entry;
-- Now all roles that include data_entry (developer, admin, super_admin) 
-- automatically have DELETE permission

-- Revoke from base role affects all composite roles
REVOKE UPDATE ON sensitive_table FROM data_entry;
-- developer and admin lose UPDATE on sensitive_table

-- Add new role to existing composite
CREATE ROLE audit_viewer;
GRANT SELECT ON audit_logs TO audit_viewer;
GRANT audit_viewer TO analyst;  -- Analysts can now view audit logs
```

## Hierarchical Role Structures

### Multi-Level Hierarchies

```sql
-- Department-based hierarchy
CREATE ROLE company;

-- Level 1: Departments
CREATE ROLE sales_dept;
CREATE ROLE engineering_dept;
CREATE ROLE finance_dept;
GRANT sales_dept, engineering_dept, finance_dept TO company;

-- Level 2: Teams within departments
CREATE ROLE sales_team_us;
CREATE ROLE sales_team_eu;
CREATE ROLE sales_team_asia;
GRANT sales_team_us, sales_team_eu, sales_team_asia TO sales_dept;

CREATE ROLE backend_team;
CREATE ROLE frontend_team;
CREATE ROLE devops_team;
GRANT backend_team, frontend_team, devops_team TO engineering_dept;

-- Level 3: Specific roles within teams
CREATE ROLE sales_rep;
CREATE ROLE sales_manager;
GRANT sales_rep TO sales_team_us;
GRANT sales_manager TO sales_team_us;
GRANT sales_rep TO sales_manager;  -- Managers have all rep permissions

-- Level 4: Individual users
GRANT sales_rep TO 'john.doe';
GRANT sales_manager TO 'jane.smith';
```

### Role Inheritance Paths

```sql
-- View effective permissions through inheritance
CREATE VIEW role_inheritance_tree AS
WITH RECURSIVE role_tree AS (
    -- Base roles (no parents)
    SELECT 
        r.role_name,
        NULL::VARCHAR AS parent_role,
        0 AS level,
        ARRAY[r.role_name] AS path
    FROM sys.roles r
    WHERE NOT EXISTS (
        SELECT 1 FROM sys.role_members rm 
        WHERE rm.member_role = r.role_name
    )
    
    UNION ALL
    
    -- Recursive: roles with parents
    SELECT 
        rm.member_role AS role_name,
        rm.role_name AS parent_role,
        rt.level + 1,
        rt.path || rm.member_role
    FROM sys.role_members rm
    JOIN role_tree rt ON rm.role_name = rt.role_name
)
SELECT * FROM role_tree
ORDER BY level, role_name;

-- Check effective permissions for a role
CREATE FUNCTION get_effective_permissions(role_name VARCHAR)
RETURNS TABLE(permission_type VARCHAR, object_name VARCHAR, source_role VARCHAR)
AS $$
WITH RECURSIVE role_hierarchy AS (
    -- Start with the specified role
    SELECT role_name AS current_role, 0 AS level
    
    UNION
    
    -- Include all roles granted to this role
    SELECT 
        rm.role_name AS current_role,
        rh.level + 1
    FROM sys.role_members rm
    JOIN role_hierarchy rh ON rm.member_role = rh.current_role
)
SELECT DISTINCT
    p.permission_type,
    p.object_name,
    p.grantee AS source_role
FROM role_hierarchy rh
JOIN sys.permissions p ON p.grantee = rh.current_role
ORDER BY permission_type, object_name;
$$ LANGUAGE sql;
```

## Conditional Role Composition

### Time-Based Role Membership

```sql
-- Grant role membership with time constraints
GRANT developer TO contractor_role
    VALID FROM '2024-01-01' TO '2024-12-31';

-- Temporary elevation
GRANT admin TO developer
    VALID FOR INTERVAL '4 hours'
    WITH REASON 'Emergency production fix';

-- Scheduled role changes
CREATE ROLE on_call_admin;
GRANT admin TO on_call_admin
    WHEN time_between('17:00', '09:00')  -- After hours only
    OR day_of_week() IN ('Saturday', 'Sunday');  -- Weekends
```

### Contextual Role Activation

```sql
-- Role only active in specific contexts
CREATE ROLE production_admin;
GRANT admin TO production_admin
    WHEN current_database() = 'production'
    AND client_ip() IN ('10.0.1.0/24');

-- Role with approval workflow
CREATE ROLE sensitive_data_access;
GRANT data_analyst TO sensitive_data_access
    WHEN EXISTS (
        SELECT 1 FROM approval_queue
        WHERE role_name = 'sensitive_data_access'
        AND user_name = CURRENT_USER
        AND approved = TRUE
        AND expires_at > CURRENT_TIMESTAMP
    );
```

## Role Composition Policies

### Separation of Duties

```sql
-- Define mutually exclusive roles
CREATE ROLE POLICY separation_of_duties AS
    EXCLUSIVE ROLES (approver, requester)  -- Can't have both
    EXCLUSIVE ROLES (auditor, developer)   -- Audit independence
    EXCLUSIVE ROLES (trader, risk_manager); -- Trading floor rules

-- Attempting to grant conflicting roles fails
GRANT requester TO john;  -- OK
GRANT approver TO john;   -- ERROR: Violates separation_of_duties policy

-- Composite roles respect exclusivity
CREATE ROLE financial_controller;
GRANT approver TO financial_controller;
GRANT requester TO financial_controller;  -- ERROR: Exclusive roles
```

### Maximum Permission Policies

```sql
-- Limit permission accumulation
CREATE ROLE POLICY max_permissions AS
    MAX_SCHEMAS 5,           -- Can access at most 5 schemas
    MAX_TABLES 100,          -- Can access at most 100 tables
    MAX_SENSITIVE_TABLES 10, -- Limited sensitive data access
    MAX_ADMIN_RIGHTS 3;      -- Limited admin capabilities

-- Role composition respects limits
CREATE ROLE data_scientist;
GRANT team1_analyst TO data_scientist;  -- Access to 30 tables
GRANT team2_analyst TO data_scientist;  -- Access to 40 tables
GRANT team3_analyst TO data_scientist;  -- Access to 50 tables
-- Total would be 120 tables - exceeds MAX_TABLES policy
-- ERROR: Role composition would exceed MAX_TABLES limit
```

## Dynamic Role Management

### Role Templates

```sql
-- Create role templates for consistency
CREATE ROLE TEMPLATE standard_employee AS (
    GRANT read_only,
    GRANT company_resources,
    GRANT self_service_hr,
    SET statement_timeout = '5 minutes',
    SET work_mem = '256MB'
);

-- Create roles from template
CREATE ROLE new_employee FROM TEMPLATE standard_employee;
CREATE ROLE contractor FROM TEMPLATE standard_employee
    WITHOUT company_resources;  -- Exclude specific grants
```

### Role Cloning and Modification

```sql
-- Clone existing role with modifications
CREATE ROLE team_lead AS CLONE OF developer WITH (
    ADDITIONAL GRANTS (approve_pull_requests, manage_team),
    REVOKE schema_designer,  -- Don't need this permission
    SET statement_timeout = '10 minutes'  -- Longer timeout
);

-- Bulk role operations
DO $$
DECLARE
    r RECORD;
BEGIN
    -- Grant new permission to all developer roles
    FOR r IN SELECT role_name FROM sys.roles 
             WHERE role_name LIKE '%_developer' LOOP
        EXECUTE format('GRANT code_review TO %I', r.role_name);
    END LOOP;
END $$;
```

## Role Composition Monitoring

### Audit Role Changes

```sql
-- Track role composition changes
CREATE TABLE role_composition_audit (
    audit_id UUID DEFAULT gen_random_uuid(),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    action VARCHAR(10),  -- GRANT, REVOKE
    parent_role VARCHAR(100),
    child_role VARCHAR(100),
    granted_by VARCHAR(100),
    reason TEXT,
    expires_at TIMESTAMP
);

-- Trigger to audit role grants
CREATE TRIGGER audit_role_grants
AFTER INSERT OR DELETE ON sys.role_members
FOR EACH ROW EXECUTE FUNCTION audit_role_composition();
```

### Role Usage Analytics

```sql
-- Analyze role composition effectiveness
CREATE VIEW role_composition_stats AS
SELECT 
    parent_role,
    COUNT(DISTINCT child_role) AS child_count,
    COUNT(DISTINCT user_name) AS total_users,
    array_agg(DISTINCT child_role) AS child_roles,
    MAX(last_used) AS last_active
FROM sys.role_members rm
LEFT JOIN sys.role_usage ru ON rm.role_name = ru.role_name
GROUP BY parent_role;

-- Find unused role compositions
SELECT 
    role_name,
    'Unused composite role' AS issue
FROM sys.roles r
WHERE EXISTS (
    SELECT 1 FROM sys.role_members 
    WHERE role_name = r.role_name
)
AND NOT EXISTS (
    SELECT 1 FROM sys.role_usage
    WHERE role_name = r.role_name
    AND last_used > CURRENT_DATE - INTERVAL '90 days'
);
```

## Best Practices Examples

### Example 1: Application Role Hierarchy

```sql
-- Base permissions
CREATE ROLE app_read;
GRANT SELECT ON schema app TO app_read;

CREATE ROLE app_write;
GRANT INSERT, UPDATE, DELETE ON schema app TO app_write;

CREATE ROLE app_execute;
GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA app TO app_execute;

-- Service roles (combine base permissions)
CREATE ROLE app_service_readonly;
GRANT app_read TO app_service_readonly;

CREATE ROLE app_service_standard;
GRANT app_read TO app_service_standard;
GRANT app_write TO app_service_standard;
GRANT app_execute TO app_service_standard;

CREATE ROLE app_service_admin;
GRANT app_service_standard TO app_service_admin;
GRANT CREATE, ALTER, DROP ON SCHEMA app TO app_service_admin;

-- Environment-specific roles
CREATE ROLE app_dev;
GRANT app_service_admin TO app_dev;

CREATE ROLE app_staging;
GRANT app_service_standard TO app_staging;

CREATE ROLE app_production;
GRANT app_service_readonly TO app_production;
-- Production writes through different mechanism
```

### Example 2: Multi-Tenant Role Structure

```sql
-- Tenant isolation with role composition
CREATE ROLE tenant_base;
GRANT USAGE ON SCHEMA public TO tenant_base;

-- Per-tenant roles
CREATE ROLE tenant_001;
GRANT tenant_base TO tenant_001;
GRANT ALL ON SCHEMA tenant_001 TO tenant_001;

CREATE ROLE tenant_002;
GRANT tenant_base TO tenant_002;
GRANT ALL ON SCHEMA tenant_002 TO tenant_002;

-- Tenant admin can manage multiple tenants
CREATE ROLE multi_tenant_admin;
GRANT tenant_001 TO multi_tenant_admin;
GRANT tenant_002 TO multi_tenant_admin;
-- Admin can now access both tenant schemas

-- Support role can read all tenants
CREATE ROLE support;
DO $$
DECLARE
    tenant RECORD;
BEGIN
    FOR tenant IN SELECT role_name FROM sys.roles 
                  WHERE role_name LIKE 'tenant_%' LOOP
        EXECUTE format('GRANT %I TO support', tenant.role_name);
    END LOOP;
END $$;
```

### Example 3: Compliance-Driven Roles

```sql
-- Compliance requirements as roles
CREATE ROLE pci_compliant;
CREATE ROLE gdpr_compliant;
CREATE ROLE hipaa_compliant;

-- Grant specific compliance permissions
GRANT SELECT ON pci_audit_log TO pci_compliant;
GRANT EXECUTE ON gdpr_data_functions TO gdpr_compliant;
GRANT SELECT ON hipaa_protected_data TO hipaa_compliant WITH (
    REQUIRE ENCRYPTION,
    AUDIT ALL ACCESS,
    MASK SENSITIVE FIELDS
);

-- Application roles inherit compliance requirements
CREATE ROLE payment_processor;
GRANT app_service_standard TO payment_processor;
GRANT pci_compliant TO payment_processor;  -- Must be PCI compliant

CREATE ROLE healthcare_app;
GRANT app_service_standard TO healthcare_app;
GRANT hipaa_compliant TO healthcare_app;  -- Must be HIPAA compliant

CREATE ROLE european_service;
GRANT app_service_standard TO european_service;
GRANT gdpr_compliant TO european_service;  -- Must be GDPR compliant

-- Super role for compliance officers
CREATE ROLE compliance_officer;
GRANT pci_compliant TO compliance_officer;
GRANT gdpr_compliant TO compliance_officer;
GRANT hipaa_compliant TO compliance_officer;
GRANT READ ONLY ON ALL TABLES TO compliance_officer;
```

This role composition system provides maximum flexibility while maintaining security and auditability!