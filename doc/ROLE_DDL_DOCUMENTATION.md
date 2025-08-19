# ScratchBird ROLE - Complete DDL Documentation

**Version**: Alpha 0.6.0  
**Implementation Date**: July 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  
**Documentation Type**: User Guide & Technical Reference

---

## Overview

ROLE objects in ScratchBird provide comprehensive role-based access control (RBAC) for database security management. Roles represent collections of privileges that can be granted to users, enabling efficient permission management through logical groupings. The role system integrates with ScratchBird's advanced security architecture to provide hierarchical role inheritance, system privileges, and schema-aware access control.

### Key Features and Capabilities

- **Role-Based Access Control**: Logical grouping of database privileges for efficient permission management
- **System Privileges**: Advanced system-level privileges for administrative operations
- **Hierarchical Role Inheritance**: Roles can inherit privileges from other roles
- **Schema Integration**: Roles can be associated with specific schemas and have home schema assignments
- **Default Role Management**: Automatic role activation and default role assignments
- **Administrative Role Management**: Special administrative roles with enhanced privileges  
- **Role Ownership**: Track role creators and ownership hierarchy
- **Comment System**: Administrative documentation for role purposes
- **Plugin Integration**: External authentication system compatibility

### ScratchBird-Specific Enhancements

1. **System Privileges**: Comprehensive system-level privilege management beyond standard SQL
2. **Hierarchical Role Inheritance**: Multi-level role inheritance with privilege aggregation
3. **Schema-Aware Roles**: Roles can be associated with specific schemas in hierarchical structures
4. **Home Schema Assignment**: Default schema context for role-based sessions
5. **Enhanced System Catalog**: Comprehensive RDB$ROLES table with rich metadata
6. **DDL Trigger Integration**: Support for DDL triggers on role operations
7. **Advanced Permission Model**: Fine-grained privilege control and delegation
8. **Role Hierarchy Management**: Prevent circular dependencies and manage complex inheritance

---

## DDL Syntax Reference

### CREATE ROLE

Creates a new database role with optional system privileges and configuration.

#### **Basic Syntax**
```sql
CREATE ROLE [IF NOT EXISTS] role_name
    [SET SYSTEM PRIVILEGES TO privilege_list]
    [DROP SYSTEM PRIVILEGES];
```

#### **Complete Syntax with All Options**
```sql
CREATE ROLE [IF NOT EXISTS] role_name
    [SET SYSTEM PRIVILEGES TO privilege1 [, privilege2, ...]]
    [DROP SYSTEM PRIVILEGES]
    [HOME SCHEMA [catalog.]schema.subschema]
    [INHERITS FROM parent_role]
    [COMMENT 'role_description'];
```

#### **Parameters**

- **IF NOT EXISTS**: Skip creation if role already exists (no error)
- **role_name**: Role identifier (63 characters max, case-insensitive) 
- **SET SYSTEM PRIVILEGES TO**: Assign specific system privileges to the role
- **DROP SYSTEM PRIVILEGES**: Remove all system privileges from the role
- **HOME SCHEMA**: Default schema context for users granted this role
- **INHERITS FROM**: Parent role for hierarchical inheritance
- **COMMENT**: Administrative description of the role

---

## CREATE ROLE Examples

### **Basic Role Creation**

#### **Simple Application Roles**
```sql
-- Create basic application roles
CREATE ROLE app_user;

CREATE ROLE app_admin;

CREATE ROLE report_viewer;

-- Create roles with descriptive comments
CREATE ROLE data_analyst COMMENT 'Data analysis and reporting role';

CREATE ROLE database_maintainer COMMENT 'Database maintenance and backup operations';
```

#### **Department-Based Roles**
```sql
-- Create departmental roles
CREATE ROLE finance_user COMMENT 'Finance department standard user';

CREATE ROLE hr_coordinator COMMENT 'Human resources coordination role';

CREATE ROLE sales_manager COMMENT 'Sales management and oversight role';

CREATE ROLE accounting_clerk COMMENT 'Accounting data entry and processing';
```

### **Roles with System Privileges**

#### **Administrative Roles with System Privileges**
```sql
-- Create system administrator role
CREATE ROLE system_admin
    SET SYSTEM PRIVILEGES TO USER_MANAGEMENT, IGNORE_LIMBO
    COMMENT 'Full system administration privileges';

-- Create backup administrator  
CREATE ROLE backup_admin
    SET SYSTEM PRIVILEGES TO IGNORE_LIMBO, USE_GSTAT_UTILITY
    COMMENT 'Database backup and recovery operations';

-- Create security administrator
CREATE ROLE security_admin
    SET SYSTEM PRIVILEGES TO USER_MANAGEMENT, TRACE_ANY_ATTACHMENT
    COMMENT 'User and security management role';
```

#### **Specialized System Roles**
```sql
-- Create database monitoring role
CREATE ROLE db_monitor
    SET SYSTEM PRIVILEGES TO TRACE_ANY_ATTACHMENT, USE_GSTAT_UTILITY
    COMMENT 'Database monitoring and performance analysis';

-- Create utilities access role
CREATE ROLE utility_operator
    SET SYSTEM PRIVILEGES TO USE_GSTAT_UTILITY, IGNORE_LIMBO
    COMMENT 'Database utility operations and maintenance';

-- Create replication manager role
CREATE ROLE replication_manager
    SET SYSTEM PRIVILEGES TO CHANGE_SHUTDOWN_MODE, TRACE_ANY_ATTACHMENT
    COMMENT 'Database replication management';
```

### **Schema-Aware Roles**

#### **Roles with Home Schema Assignment**
```sql
-- Create roles with specific schema contexts
CREATE ROLE finance_analyst
    HOME SCHEMA enterprise.finance
    COMMENT 'Financial analysis with finance schema context';

CREATE ROLE regional_manager
    HOME SCHEMA company.regions.northamerica
    COMMENT 'Regional management for North America';

CREATE ROLE project_developer
    HOME SCHEMA enterprise.projects.development
    COMMENT 'Project development team member';
```

#### **Hierarchical Schema Roles**
```sql
-- Create roles for deep schema hierarchies
CREATE ROLE accounting_specialist
    HOME SCHEMA enterprise.finance.accounting.specialists
    COMMENT 'Specialized accounting operations role';

CREATE ROLE sales_representative
    HOME SCHEMA company.regions.northamerica.sales.representatives
    COMMENT 'Sales representative for North American region';

CREATE ROLE quality_inspector
    HOME SCHEMA manufacturing.quality.inspection.specialists
    COMMENT 'Quality control inspection specialist';
```

### **Hierarchical Role Inheritance**

#### **Role Inheritance Hierarchies**
```sql
-- Create base user role
CREATE ROLE base_user COMMENT 'Base user privileges for all employees';

-- Create departmental roles inheriting from base
CREATE ROLE finance_base 
    INHERITS FROM base_user
    HOME SCHEMA enterprise.finance
    COMMENT 'Base finance department privileges';

CREATE ROLE hr_base
    INHERITS FROM base_user  
    HOME SCHEMA enterprise.hr
    COMMENT 'Base HR department privileges';

-- Create specialized roles inheriting from departmental roles
CREATE ROLE senior_analyst
    INHERITS FROM finance_base
    SET SYSTEM PRIVILEGES TO USE_GSTAT_UTILITY
    COMMENT 'Senior financial analyst with reporting privileges';

CREATE ROLE hr_manager
    INHERITS FROM hr_base
    SET SYSTEM PRIVILEGES TO USER_MANAGEMENT
    COMMENT 'HR manager with user administration privileges';
```

#### **Multi-Level Role Hierarchies**
```sql
-- Create three-tier role hierarchy
CREATE ROLE employee_base COMMENT 'Basic employee privileges';

CREATE ROLE department_user
    INHERITS FROM employee_base
    COMMENT 'Department-level user privileges';

CREATE ROLE department_supervisor
    INHERITS FROM department_user
    SET SYSTEM PRIVILEGES TO TRACE_ANY_ATTACHMENT
    COMMENT 'Department supervisor privileges';

CREATE ROLE department_manager
    INHERITS FROM department_supervisor
    SET SYSTEM PRIVILEGES TO USER_MANAGEMENT, IGNORE_LIMBO
    COMMENT 'Department manager with full administrative access';
```

### **Specialized Application Roles**

#### **Service Account Roles**
```sql
-- Create service account roles
CREATE ROLE app_service_role
    HOME SCHEMA application.services
    COMMENT 'Application service account role';

CREATE ROLE batch_processor_role
    HOME SCHEMA processing.batch
    SET SYSTEM PRIVILEGES TO IGNORE_LIMBO
    COMMENT 'Batch processing service role';

CREATE ROLE reporting_service_role
    HOME SCHEMA enterprise.reports
    SET SYSTEM PRIVILEGES TO USE_GSTAT_UTILITY
    COMMENT 'Automated reporting service role';
```

#### **External Integration Roles**
```sql
-- Create roles for external system integration
CREATE ROLE api_client_role
    HOME SCHEMA api.v1
    COMMENT 'External API client access role';

CREATE ROLE data_warehouse_role
    HOME SCHEMA datawarehouse.staging
    SET SYSTEM PRIVILEGES TO IGNORE_LIMBO
    COMMENT 'Data warehouse integration role';

CREATE ROLE backup_service_role
    SET SYSTEM PRIVILEGES TO IGNORE_LIMBO, USE_GSTAT_UTILITY
    COMMENT 'External backup service integration';
```

### **Conditional Role Creation**

#### **IF NOT EXISTS Usage**
```sql
-- Safe role creation for deployment scripts
CREATE ROLE IF NOT EXISTS default_user 
    COMMENT 'Default user role for standard access';

CREATE ROLE IF NOT EXISTS admin_role
    SET SYSTEM PRIVILEGES TO USER_MANAGEMENT
    COMMENT 'Administrative role for system management';

-- Safe departmental role creation
CREATE ROLE IF NOT EXISTS finance_team
    HOME SCHEMA enterprise.finance
    COMMENT 'Finance team collaboration role';
```

---

## ALTER ROLE

Modifies existing role properties including system privileges and configuration.

### **ALTER ROLE Syntax**
```sql
ALTER ROLE role_name
    [SET SYSTEM PRIVILEGES TO privilege_list]
    [DROP SYSTEM PRIVILEGES]
    [HOME SCHEMA new_schema_name]
    [INHERITS FROM parent_role]
    [COMMENT 'new_description'];
```

### **ALTER ROLE Examples**

#### **System Privileges Management**
```sql
-- Add system privileges to existing role
ALTER ROLE db_monitor
    SET SYSTEM PRIVILEGES TO TRACE_ANY_ATTACHMENT, USE_GSTAT_UTILITY, IGNORE_LIMBO;

-- Remove all system privileges
ALTER ROLE restricted_role
    DROP SYSTEM PRIVILEGES;

-- Grant specific system privileges
ALTER ROLE backup_operator
    SET SYSTEM PRIVILEGES TO IGNORE_LIMBO, CHANGE_SHUTDOWN_MODE;
```

#### **Role Hierarchy Changes**
```sql
-- Add inheritance to existing role
ALTER ROLE finance_clerk
    INHERITS FROM finance_base;

-- Change parent role
ALTER ROLE senior_analyst
    INHERITS FROM finance_manager;

-- Update role with multiple changes
ALTER ROLE project_lead
    INHERITS FROM department_supervisor
    SET SYSTEM PRIVILEGES TO USER_MANAGEMENT
    HOME SCHEMA enterprise.projects.management;
```

#### **Schema Assignment Updates**
```sql
-- Change role's home schema
ALTER ROLE regional_user
    HOME SCHEMA company.regions.europe;

-- Update schema with hierarchical path
ALTER ROLE department_head
    HOME SCHEMA enterprise.departments.management
    SET SYSTEM PRIVILEGES TO USER_MANAGEMENT, TRACE_ANY_ATTACHMENT;

-- Move role to different schema context
ALTER ROLE project_manager
    HOME SCHEMA enterprise.projects.active
    COMMENT 'Updated for active projects management';
```

#### **Comprehensive Role Updates**
```sql
-- Complete role reconfiguration
ALTER ROLE data_administrator
    SET SYSTEM PRIVILEGES TO USER_MANAGEMENT, IGNORE_LIMBO, USE_GSTAT_UTILITY
    HOME SCHEMA administration.data
    INHERITS FROM system_admin
    COMMENT 'Comprehensive data administration role';

-- Update service role configuration
ALTER ROLE api_service
    HOME SCHEMA api.production
    SET SYSTEM PRIVILEGES TO TRACE_ANY_ATTACHMENT
    COMMENT 'Production API service role with monitoring';
```

---

## DROP ROLE

Removes role definitions from the database. Roles can only be dropped if they are not granted to any users or inherited by other roles.

### **DROP ROLE Syntax**
```sql
DROP ROLE [IF EXISTS] role_name;
```

### **DROP ROLE Examples**

#### **Basic Role Removal**
```sql
-- Drop specific role
DROP ROLE temp_role;

-- Safe role removal
DROP ROLE IF EXISTS deprecated_role;

-- Drop test role
DROP ROLE IF EXISTS test_role;
```

#### **Dependency Checking Before Drop**
```sql
-- Check for role dependencies before dropping
SELECT DISTINCT
    p.RDB$USER as GRANTED_TO,
    'USER' as GRANTEE_TYPE
FROM RDB$USER_PRIVILEGES p
WHERE p.RDB$RELATION_NAME = 'FINANCE_USER'
  AND p.RDB$OBJECT_TYPE = 13  -- Role object type

UNION ALL

SELECT DISTINCT
    r.RDB$ROLE_NAME as INHERITING_ROLE,
    'ROLE' as GRANTEE_TYPE  
FROM RDB$ROLES r
WHERE r.RDB$INHERITS_FROM = 'FINANCE_USER';

-- Drop role after confirming no dependencies
DROP ROLE finance_user;
```

#### **Systematic Role Cleanup**
```sql
-- Clean up temporary roles
DROP ROLE IF EXISTS temp_analyst;
DROP ROLE IF EXISTS test_manager;
DROP ROLE IF EXISTS demo_user;

-- Remove deprecated roles
DROP ROLE IF EXISTS old_finance_role;
DROP ROLE IF EXISTS legacy_admin;
```

---

## RECREATE ROLE

Combines DROP and CREATE operations in a single atomic transaction.

### **RECREATE ROLE Syntax**
```sql
RECREATE ROLE role_name
    [SET SYSTEM PRIVILEGES TO privilege_list]
    [DROP SYSTEM PRIVILEGES]
    [HOME SCHEMA schema_name]
    [INHERITS FROM parent_role]
    [COMMENT 'role_description'];
```

### **RECREATE ROLE Examples**

#### **Role Recreation with New Configuration**
```sql
-- Recreate role with updated privileges
RECREATE ROLE finance_manager
    SET SYSTEM PRIVILEGES TO USER_MANAGEMENT, TRACE_ANY_ATTACHMENT
    HOME SCHEMA enterprise.finance.management
    INHERITS FROM finance_base
    COMMENT 'Updated finance management role';

-- Recreate system role with enhanced privileges
RECREATE ROLE system_operator
    SET SYSTEM PRIVILEGES TO IGNORE_LIMBO, USE_GSTAT_UTILITY, CHANGE_SHUTDOWN_MODE
    HOME SCHEMA administration
    COMMENT 'Enhanced system operations role';
```

#### **Service Role Recreation**
```sql
-- Recreate application service role
RECREATE ROLE app_service
    HOME SCHEMA application.production
    SET SYSTEM PRIVILEGES TO TRACE_ANY_ATTACHMENT
    COMMENT 'Production application service role';
```

---

## System Privileges Management

### **Available System Privileges**

ScratchBird supports various system privileges that can be assigned to roles:

#### **User and Security Management**
- **USER_MANAGEMENT**: Create, alter, and drop user accounts
- **TRACE_ANY_ATTACHMENT**: Monitor and trace database connections
- **CHANGE_SHUTDOWN_MODE**: Control database shutdown and startup modes

#### **Database Operations**
- **IGNORE_LIMBO**: Handle transactions in limbo state
- **USE_GSTAT_UTILITY**: Access to database statistics and analysis tools
- **CHANGE_HEADER_SETTINGS**: Modify database header parameters

#### **System Privileges Examples**
```sql
-- Administrative role with comprehensive privileges
CREATE ROLE db_administrator
    SET SYSTEM PRIVILEGES TO 
        USER_MANAGEMENT,
        TRACE_ANY_ATTACHMENT, 
        IGNORE_LIMBO,
        USE_GSTAT_UTILITY,
        CHANGE_SHUTDOWN_MODE,
        CHANGE_HEADER_SETTINGS
    COMMENT 'Full database administration privileges';

-- Monitoring role with read-only system access
CREATE ROLE system_monitor
    SET SYSTEM PRIVILEGES TO TRACE_ANY_ATTACHMENT, USE_GSTAT_UTILITY
    COMMENT 'System monitoring and analysis role';

-- Maintenance role with specific operational privileges
CREATE ROLE maintenance_operator
    SET SYSTEM PRIVILEGES TO IGNORE_LIMBO, CHANGE_SHUTDOWN_MODE
    COMMENT 'Database maintenance operations role';
```

---

## Role Assignment and Management

### **Granting Roles to Users**

#### **Basic Role Grants**
```sql
-- Grant role to user
GRANT finance_user TO john_doe;

-- Grant multiple roles to user
GRANT finance_user, report_viewer TO jane_smith;

-- Grant role with admin option
GRANT department_manager TO team_lead WITH ADMIN OPTION;
```

#### **Default Role Assignment**
```sql
-- Grant role as default (automatically activated)
GRANT DEFAULT finance_base TO finance_employee;

-- Grant role with both regular and default options
GRANT finance_user TO analyst_user;
GRANT DEFAULT report_viewer TO analyst_user;
```

### **Revoking Roles from Users**

#### **Role Revocation**
```sql
-- Revoke role from user
REVOKE finance_user FROM former_employee;

-- Revoke admin option
REVOKE ADMIN OPTION FOR department_manager FROM former_lead;

-- Revoke multiple roles
REVOKE finance_user, report_viewer FROM transferred_employee;
```

### **Role Activation Management**

#### **Session Role Management**
```sql
-- Set active roles for current session
SET ROLE finance_user, report_viewer;

-- Activate all granted roles
SET ROLE ALL;

-- Deactivate all roles (use only public privileges)
SET ROLE NONE;

-- Activate specific role only
SET ROLE department_manager;
```

---

## System Catalog Integration

ScratchBird stores role information in the RDB$ROLES system table.

### **Querying Role Information**

#### **List All Roles**
```sql
-- Show all database roles
SELECT 
    RDB$ROLE_NAME as ROLE_NAME,
    RDB$OWNER_NAME as OWNER,
    CASE RDB$SYSTEM_FLAG
        WHEN 1 THEN 'SYSTEM'
        WHEN 0 THEN 'USER_DEFINED'
        ELSE 'UNKNOWN'
    END as ROLE_TYPE,
    RDB$DESCRIPTION as DESCRIPTION,
    RDB$SYSTEM_PRIVILEGES as SYSTEM_PRIVS,
    RDB$HOME_SCHEMA as HOME_SCHEMA,
    RDB$INHERITS_FROM as PARENT_ROLE,
    CASE RDB$IS_HIERARCHICAL
        WHEN 1 THEN 'YES'
        ELSE 'NO'
    END as IS_HIERARCHICAL
FROM RDB$ROLES
ORDER BY ROLE_TYPE, ROLE_NAME;
```

#### **Role Hierarchy Analysis**
```sql
-- Show role inheritance hierarchy
WITH RECURSIVE role_hierarchy AS (
    -- Base case: roles with no parent
    SELECT 
        RDB$ROLE_NAME as role_name,
        0 as level,
        CAST(RDB$ROLE_NAME AS VARCHAR(1000)) as hierarchy_path,
        RDB$INHERITS_FROM as parent_role
    FROM RDB$ROLES
    WHERE RDB$INHERITS_FROM IS NULL
    
    UNION ALL
    
    -- Recursive case: roles with parents
    SELECT 
        r.RDB$ROLE_NAME,
        rh.level + 1,
        CAST(rh.hierarchy_path || ' -> ' || r.RDB$ROLE_NAME AS VARCHAR(1000)),
        r.RDB$INHERITS_FROM
    FROM RDB$ROLES r
    JOIN role_hierarchy rh ON r.RDB$INHERITS_FROM = rh.role_name
)
SELECT 
    level,
    REPEAT('  ', level) || role_name as INDENTED_ROLE,
    hierarchy_path,
    parent_role
FROM role_hierarchy
ORDER BY hierarchy_path;
```

#### **Role Privilege Analysis**
```sql
-- Analyze system privileges by role
SELECT 
    RDB$ROLE_NAME as ROLE_NAME,
    CASE 
        WHEN RDB$SYSTEM_PRIVILEGES IS NULL THEN 'NO_SYSTEM_PRIVS'
        WHEN RDB$SYSTEM_PRIVILEGES = '' THEN 'NO_SYSTEM_PRIVS'
        ELSE 'HAS_SYSTEM_PRIVS'
    END as SYSTEM_PRIV_STATUS,
    RDB$SYSTEM_PRIVILEGES as PRIVILEGES,
    RDB$HOME_SCHEMA as HOME_SCHEMA
FROM RDB$ROLES
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
ORDER BY SYSTEM_PRIV_STATUS DESC, ROLE_NAME;
```

### **Role Assignment Analysis**

#### **Users and Their Roles**
```sql
-- Show users and their assigned roles
SELECT 
    p.RDB$USER as USERNAME,
    p.RDB$RELATION_NAME as ROLE_NAME,
    p.RDB$GRANTOR as GRANTED_BY,
    CASE p.RDB$GRANT_OPTION
        WHEN 1 THEN 'WITH ADMIN OPTION'
        ELSE 'REGULAR'
    END as GRANT_TYPE
FROM RDB$USER_PRIVILEGES p
JOIN RDB$ROLES r ON p.RDB$RELATION_NAME = r.RDB$ROLE_NAME
WHERE p.RDB$OBJECT_TYPE = 13  -- Role object type
ORDER BY USERNAME, ROLE_NAME;
```

#### **Role Usage Statistics**
```sql
-- Analyze role usage across users
SELECT 
    r.RDB$ROLE_NAME as ROLE_NAME,
    COUNT(DISTINCT p.RDB$USER) as USER_COUNT,
    COUNT(CASE WHEN p.RDB$GRANT_OPTION = 1 THEN 1 END) as ADMIN_GRANTS,
    STRING_AGG(DISTINCT p.RDB$USER, ', ' ORDER BY p.RDB$USER) as GRANTED_TO_USERS
FROM RDB$ROLES r
LEFT JOIN RDB$USER_PRIVILEGES p ON r.RDB$ROLE_NAME = p.RDB$RELATION_NAME
    AND p.RDB$OBJECT_TYPE = 13
WHERE r.RDB$SYSTEM_FLAG = 0 OR r.RDB$SYSTEM_FLAG IS NULL
GROUP BY r.RDB$ROLE_NAME
ORDER BY USER_COUNT DESC, ROLE_NAME;
```

### **Role Security Analysis**

#### **Roles with System Privileges**
```sql
-- Identify roles with system privileges
SELECT 
    RDB$ROLE_NAME as ROLE_NAME,
    RDB$OWNER_NAME as OWNER,
    RDB$SYSTEM_PRIVILEGES as SYSTEM_PRIVILEGES,
    COUNT(DISTINCT p.RDB$USER) as USERS_WITH_ROLE
FROM RDB$ROLES r
LEFT JOIN RDB$USER_PRIVILEGES p ON r.RDB$ROLE_NAME = p.RDB$RELATION_NAME
    AND p.RDB$OBJECT_TYPE = 13
WHERE r.RDB$SYSTEM_PRIVILEGES IS NOT NULL
  AND r.RDB$SYSTEM_PRIVILEGES <> ''
GROUP BY r.RDB$ROLE_NAME, r.RDB$OWNER_NAME, r.RDB$SYSTEM_PRIVILEGES
ORDER BY USERS_WITH_ROLE DESC, ROLE_NAME;
```

#### **Orphaned Roles Analysis**
```sql
-- Find roles not assigned to any users
SELECT 
    r.RDB$ROLE_NAME as UNUSED_ROLE,
    r.RDB$DESCRIPTION as DESCRIPTION,
    r.RDB$SYSTEM_PRIVILEGES as SYSTEM_PRIVS
FROM RDB$ROLES r
WHERE (r.RDB$SYSTEM_FLAG = 0 OR r.RDB$SYSTEM_FLAG IS NULL)
  AND NOT EXISTS (
      SELECT 1 FROM RDB$USER_PRIVILEGES p
      WHERE p.RDB$RELATION_NAME = r.RDB$ROLE_NAME
        AND p.RDB$OBJECT_TYPE = 13
  )
  AND NOT EXISTS (
      SELECT 1 FROM RDB$ROLES r2
      WHERE r2.RDB$INHERITS_FROM = r.RDB$ROLE_NAME
  )
ORDER BY UNUSED_ROLE;
```

---

## Role Comments and Documentation

### **Adding Comments to Roles**

#### **COMMENT ON ROLE Syntax**
```sql
COMMENT ON ROLE role_name IS 'comment_text';
```

#### **Role Comment Examples**
```sql
-- Document role purposes
COMMENT ON ROLE finance_analyst IS 'Financial analysis and reporting role for finance department';

COMMENT ON ROLE system_admin IS 'System administration role with full database privileges';

COMMENT ON ROLE api_service IS 'Service account role for external API integrations';

-- Update role documentation
COMMENT ON ROLE department_manager IS 'Department management role with user administration privileges - updated July 2025';
```

### **Viewing Role Comments**
```sql
-- Show roles with their descriptions
SELECT 
    RDB$ROLE_NAME as ROLE_NAME,
    RDB$DESCRIPTION as DESCRIPTION,
    RDB$OWNER_NAME as OWNER
FROM RDB$ROLES
WHERE RDB$DESCRIPTION IS NOT NULL
  AND (RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL)
ORDER BY ROLE_NAME;
```

---

## Advanced Role Management

### **Role Security Templates**

#### **Standard Role Creation Templates**
```sql
-- Standard user role template
CREATE PROCEDURE create_standard_role(
    role_name VARCHAR(63),
    department_schema VARCHAR(63),
    role_description VARCHAR(1000)
)
AS
BEGIN
    EXECUTE STATEMENT 'CREATE ROLE ' || role_name ||
                     ' HOME SCHEMA ' || department_schema ||
                     ' COMMENT ''' || role_description || '''';
END;

-- Administrative role template
CREATE PROCEDURE create_admin_role(
    role_name VARCHAR(63),
    system_privileges VARCHAR(500),
    role_description VARCHAR(1000)
)
AS
BEGIN
    EXECUTE STATEMENT 'CREATE ROLE ' || role_name ||
                     ' SET SYSTEM PRIVILEGES TO ' || system_privileges ||
                     ' HOME SCHEMA administration' ||
                     ' COMMENT ''' || role_description || '''';
END;
```

### **Role Hierarchy Management**

#### **Hierarchy Validation**
```sql
-- Check for circular role dependencies
CREATE PROCEDURE validate_role_hierarchy
AS
DECLARE role_name VARCHAR(63);
DECLARE parent_role VARCHAR(63);
DECLARE circular_found BOOLEAN = FALSE;
BEGIN
    -- This is a simplified example - full implementation would need
    -- recursive checking to detect cycles in role inheritance
    
    FOR SELECT RDB$ROLE_NAME, RDB$INHERITS_FROM 
        FROM RDB$ROLES 
        WHERE RDB$INHERITS_FROM IS NOT NULL
        INTO :role_name, :parent_role
    DO
    BEGIN
        -- Check for direct self-reference
        IF (role_name = parent_role) THEN
        BEGIN
            INSERT INTO role_validation_errors (error_type, role_name, error_message)
            VALUES ('CIRCULAR_DEPENDENCY', :role_name, 'Role inherits from itself');
            circular_found = TRUE;
        END
    END
    
    IF (circular_found) THEN
        EXCEPTION role_hierarchy_error 'Circular dependencies found in role hierarchy';
END;
```

### **Bulk Role Management**

#### **Role Creation Scripts**
```sql
-- Create departmental role structure
EXECUTE BLOCK AS
DECLARE dept_name VARCHAR(50);
DECLARE role_base VARCHAR(63);
DECLARE role_user VARCHAR(63);
DECLARE role_admin VARCHAR(63);
BEGIN
    FOR SELECT DISTINCT department_name FROM departments
        INTO :dept_name
    DO
    BEGIN
        role_base = LOWER(dept_name) || '_base';
        role_user = LOWER(dept_name) || '_user'; 
        role_admin = LOWER(dept_name) || '_admin';
        
        -- Create base department role
        EXECUTE STATEMENT 'CREATE ROLE ' || role_base ||
                         ' HOME SCHEMA enterprise.' || LOWER(dept_name) ||
                         ' COMMENT ''Base privileges for ' || dept_name || ' department''';
        
        -- Create user role inheriting from base
        EXECUTE STATEMENT 'CREATE ROLE ' || role_user ||
                         ' INHERITS FROM ' || role_base ||
                         ' COMMENT ''Standard user role for ' || dept_name || ' department''';
        
        -- Create admin role with system privileges
        EXECUTE STATEMENT 'CREATE ROLE ' || role_admin ||
                         ' INHERITS FROM ' || role_user ||
                         ' SET SYSTEM PRIVILEGES TO USER_MANAGEMENT' ||
                         ' COMMENT ''Administrative role for ' || dept_name || ' department''';
    END
END;
```

#### **Role Cleanup and Maintenance**
```sql
-- Clean up unused roles
CREATE PROCEDURE cleanup_unused_roles(
    older_than_days INTEGER DEFAULT 90
)
AS
DECLARE role_name VARCHAR(63);
DECLARE role_age INTEGER;
BEGIN
    FOR SELECT r.RDB$ROLE_NAME
        FROM RDB$ROLES r
        WHERE (r.RDB$SYSTEM_FLAG = 0 OR r.RDB$SYSTEM_FLAG IS NULL)
          AND NOT EXISTS (
              SELECT 1 FROM RDB$USER_PRIVILEGES p
              WHERE p.RDB$RELATION_NAME = r.RDB$ROLE_NAME
                AND p.RDB$OBJECT_TYPE = 13
          )
          AND NOT EXISTS (
              SELECT 1 FROM RDB$ROLES r2
              WHERE r2.RDB$INHERITS_FROM = r.RDB$ROLE_NAME
          )
          AND r.RDB$DESCRIPTION LIKE '%cleanup_eligible%'
        INTO :role_name
    DO
    BEGIN
        -- Log cleanup action
        INSERT INTO role_cleanup_log (role_name, cleanup_date, reason)
        VALUES (:role_name, CURRENT_TIMESTAMP, 'Unused role cleanup');
        
        -- Drop the role
        EXECUTE STATEMENT 'DROP ROLE IF EXISTS ' || role_name;
    END
END;
```

---

## Error Handling and Troubleshooting

### **Common Role Management Errors**

#### **Role Creation Errors**
```sql
-- Error: Role already exists
CREATE ROLE existing_role;
-- Solution: Use IF NOT EXISTS or ALTER ROLE

-- Error: Invalid system privilege name
CREATE ROLE test_role SET SYSTEM PRIVILEGES TO INVALID_PRIVILEGE;
-- Solution: Use valid system privilege names

-- Error: Circular inheritance
CREATE ROLE role_a INHERITS FROM role_b;
CREATE ROLE role_b INHERITS FROM role_a;
-- Solution: Design non-circular inheritance hierarchy
```

#### **Role Assignment Errors**
```sql
-- Error: Role does not exist
GRANT nonexistent_role TO user1;
-- Solution: Create role first or verify role name

-- Error: Cannot grant role to itself
GRANT finance_user TO finance_user;
-- Solution: Only grant roles to different users

-- Error: Privilege escalation prevention
GRANT system_admin TO regular_user;
-- Solution: Ensure proper authorization for administrative roles
```

### **Role Troubleshooting Queries**

#### **Role Configuration Diagnostics**
```sql
-- Check role configuration and dependencies
SELECT 
    r.RDB$ROLE_NAME as ROLE_NAME,
    r.RDB$INHERITS_FROM as PARENT_ROLE,
    r.RDB$HOME_SCHEMA as HOME_SCHEMA,
    CASE 
        WHEN r.RDB$SYSTEM_PRIVILEGES IS NULL OR r.RDB$SYSTEM_PRIVILEGES = '' 
        THEN 'NO_SYSTEM_PRIVS'
        ELSE 'HAS_SYSTEM_PRIVS'
    END as SYSTEM_PRIV_STATUS,
    COUNT(DISTINCT p.RDB$USER) as ASSIGNED_TO_USERS
FROM RDB$ROLES r
LEFT JOIN RDB$USER_PRIVILEGES p ON r.RDB$ROLE_NAME = p.RDB$RELATION_NAME
    AND p.RDB$OBJECT_TYPE = 13
WHERE r.RDB$ROLE_NAME = 'PROBLEM_ROLE'
GROUP BY r.RDB$ROLE_NAME, r.RDB$INHERITS_FROM, r.RDB$HOME_SCHEMA, r.RDB$SYSTEM_PRIVILEGES;
```

#### **Role Permission Diagnostics**
```sql
-- Diagnose role permission issues
SELECT 
    'DIRECT_GRANTS' as PERMISSION_TYPE,
    COUNT(*) as COUNT
FROM RDB$USER_PRIVILEGES p
JOIN RDB$ROLES r ON p.RDB$RELATION_NAME = r.RDB$ROLE_NAME
WHERE r.RDB$ROLE_NAME = 'PROBLEM_ROLE'
  AND p.RDB$OBJECT_TYPE = 13

UNION ALL

SELECT 
    'INHERITANCE_CHAIN' as PERMISSION_TYPE,
    COUNT(*) as COUNT
FROM RDB$ROLES r
WHERE r.RDB$INHERITS_FROM = 'PROBLEM_ROLE'

UNION ALL

SELECT
    'SYSTEM_PRIVILEGES' as PERMISSION_TYPE,
    CASE WHEN r.RDB$SYSTEM_PRIVILEGES IS NOT NULL AND r.RDB$SYSTEM_PRIVILEGES <> '' 
         THEN 1 ELSE 0 END as COUNT
FROM RDB$ROLES r
WHERE r.RDB$ROLE_NAME = 'PROBLEM_ROLE';
```

### **Role Recovery Procedures**

#### **Role Configuration Recovery**
```sql
-- Restore role configuration from backup metadata
ALTER ROLE recovered_role
    SET SYSTEM PRIVILEGES TO USER_MANAGEMENT, TRACE_ANY_ATTACHMENT
    HOME SCHEMA administration
    COMMENT 'Role restored from backup configuration';

-- Rebuild role hierarchy
ALTER ROLE child_role INHERITS FROM restored_parent_role;
```

---

## Best Practices

### **Role Design Guidelines**

1. **Principle of Least Privilege**: Grant only necessary privileges to roles
2. **Logical Grouping**: Create roles based on job functions and responsibilities
3. **Hierarchical Design**: Use role inheritance to minimize privilege duplication
4. **Descriptive Naming**: Use clear, consistent naming conventions for roles
5. **Documentation**: Always include comments describing role purposes
6. **Regular Audits**: Periodically review role assignments and privileges

### **Recommended Role Patterns**

#### **Functional Role Hierarchy**
```sql
-- Base functional roles
CREATE ROLE employee_base COMMENT 'Basic employee database access';

CREATE ROLE department_base 
    INHERITS FROM employee_base
    COMMENT 'Base departmental privileges';

CREATE ROLE department_user
    INHERITS FROM department_base  
    COMMENT 'Standard department user privileges';

CREATE ROLE department_supervisor
    INHERITS FROM department_user
    SET SYSTEM PRIVILEGES TO TRACE_ANY_ATTACHMENT
    COMMENT 'Department supervisor with monitoring privileges';

CREATE ROLE department_manager
    INHERITS FROM department_supervisor
    SET SYSTEM PRIVILEGES TO USER_MANAGEMENT, IGNORE_LIMBO
    COMMENT 'Department manager with administrative privileges';
```

#### **Application-Centric Roles**
```sql
-- Application-specific role structure
CREATE ROLE app_readonly
    HOME SCHEMA application
    COMMENT 'Read-only application access';

CREATE ROLE app_user
    INHERITS FROM app_readonly
    COMMENT 'Standard application user privileges';

CREATE ROLE app_power_user
    INHERITS FROM app_user
    SET SYSTEM PRIVILEGES TO USE_GSTAT_UTILITY
    COMMENT 'Power user with reporting capabilities';

CREATE ROLE app_admin
    INHERITS FROM app_power_user
    SET SYSTEM PRIVILEGES TO USER_MANAGEMENT, TRACE_ANY_ATTACHMENT
    COMMENT 'Application administrator';
```

#### **Schema-Based Role Organization**
```sql
-- Schema-aligned role structure
CREATE ROLE finance_readonly
    HOME SCHEMA enterprise.finance
    COMMENT 'Read-only access to finance schema';

CREATE ROLE finance_clerk
    INHERITS FROM finance_readonly
    COMMENT 'Finance data entry and basic operations';

CREATE ROLE finance_analyst  
    INHERITS FROM finance_clerk
    SET SYSTEM PRIVILEGES TO USE_GSTAT_UTILITY
    COMMENT 'Financial analysis with reporting tools';

CREATE ROLE finance_manager
    INHERITS FROM finance_analyst
    SET SYSTEM PRIVILEGES TO USER_MANAGEMENT
    COMMENT 'Finance department management';
```

---

## Implementation Details

### **Primary Implementation Files**

#### **Parser and Grammar**
- **File**: `src/dsql/parse.y:2422-2451`
- **Grammar Rules**:
  - `role_clause` (lines 2424-2430): CREATE ROLE syntax
  - `opt_system_privileges` (lines 2432-2437): System privilege management
  - `system_privileges_list` (lines 2447-2451): System privilege specification
- **Functionality**: Parsing CREATE/ALTER/DROP ROLE with system privileges

#### **DDL Node Classes**
- **File**: `src/dsql/DdlNodes.h:2294-2419`
- **Classes**:
  - `CreateAlterRoleNode` (lines 2294-2324): Role creation/modification
  - `DropRoleNode` (lines 2397-2419): Role removal with dependency checking

#### **System Catalog Integration**
- **File**: `src/jrd/relations.h:506-519`
- **Table**: `RDB$ROLES` - Stores role definitions
- **Fields**:
  - `f_rol_name`: Role name
  - `f_rol_owner`: Role owner
  - `f_rol_desc`: Role description
  - `f_rol_sys_flag`: System vs user-defined flag
  - `f_rol_sys_priv`: System privileges
  - `f_rol_schema`: Schema association
  - `f_rol_home_schema`: Home schema assignment
  - `f_rol_inherits_from`: Parent role for inheritance
  - `f_rol_is_hierarchical`: Hierarchical inheritance flag

#### **Role Security Integration**
- **File**: `src/dsql/parse.y:4599-4601`
- **Functionality**: DDL trigger support for role operations (CREATE/ALTER/DROP)
- System privilege validation and management
- Role inheritance hierarchy validation

### **Core Classes and Functions**

#### **CreateAlterRoleNode Methods**
- `dsqlPass()`: DSQL compilation and role validation
- `execute()`: Role creation/modification in system catalog
- `checkPermission()`: Security validation for role operations
- System privilege management and validation

#### **Role Security Integration**
- System privilege assignment and validation
- Role inheritance hierarchy management
- Home schema assignment and resolution
- Integration with user privilege system

### **Storage Structures**

Roles are stored in the RDB$ROLES system table with:

- **Identity**: Role name (case-insensitive, unique)
- **Ownership**: Role creator and ownership tracking
- **Privileges**: System privilege assignments
- **Hierarchy**: Parent role inheritance relationships
- **Context**: Home schema assignments and associations
- **Metadata**: Description, flags, and administrative information

---

## Administrative Operations

### **Role Backup and Restore**

#### **Backup Considerations**
- Role definitions are included in database security backups
- System privilege assignments are preserved
- Role inheritance hierarchies are maintained
- Home schema assignments require schema existence

#### **Restore Procedures**
```sql
-- Verify roles after restore
SELECT 
    RDB$ROLE_NAME,
    RDB$SYSTEM_PRIVILEGES,
    RDB$INHERITS_FROM,
    RDB$HOME_SCHEMA
FROM RDB$ROLES
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
ORDER BY RDB$ROLE_NAME;

-- Test role functionality
GRANT test_role TO test_user;
SET ROLE test_role;
```

### **Role Maintenance**

#### **Regular Maintenance Tasks**
```sql
-- Monitor role usage and assignments
CREATE VIEW role_usage_summary AS
SELECT 
    r.RDB$ROLE_NAME,
    r.RDB$SYSTEM_PRIVILEGES,
    r.RDB$INHERITS_FROM,
    COUNT(DISTINCT p.RDB$USER) as ASSIGNED_TO_USERS,
    COUNT(DISTINCT r2.RDB$ROLE_NAME) as CHILD_ROLES
FROM RDB$ROLES r
LEFT JOIN RDB$USER_PRIVILEGES p ON r.RDB$ROLE_NAME = p.RDB$RELATION_NAME
    AND p.RDB$OBJECT_TYPE = 13
LEFT JOIN RDB$ROLES r2 ON r.RDB$ROLE_NAME = r2.RDB$INHERITS_FROM
WHERE r.RDB$SYSTEM_FLAG = 0 OR r.RDB$SYSTEM_FLAG IS NULL
GROUP BY r.RDB$ROLE_NAME, r.RDB$SYSTEM_PRIVILEGES, r.RDB$INHERITS_FROM;

-- Check for unused roles
SELECT * FROM role_usage_summary WHERE ASSIGNED_TO_USERS = 0 AND CHILD_ROLES = 0;
```

---

## Migration from Other Database Systems

### **From PostgreSQL Roles**
PostgreSQL roles translate directly to ScratchBird roles:
```sql
-- PostgreSQL
CREATE ROLE finance_user;

-- ScratchBird equivalent
CREATE ROLE finance_user;
```

### **From Oracle Roles**
Oracle roles can be migrated to ScratchBird with system privileges:
```sql
-- Oracle
CREATE ROLE finance_role;

-- ScratchBird approach
CREATE ROLE finance_role
    HOME SCHEMA finance
    COMMENT 'Finance department role';
```

### **From SQL Server Roles**
SQL Server database roles translate to ScratchBird roles:
```sql
-- SQL Server
CREATE ROLE db_datareader;

-- ScratchBird approach
CREATE ROLE data_reader
    HOME SCHEMA application
    COMMENT 'Data reading role';
```

---

