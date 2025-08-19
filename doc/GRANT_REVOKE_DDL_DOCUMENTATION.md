# ScratchBird GRANT/REVOKE DDL - Complete Reference Documentation

## Overview

**GRANT and REVOKE statements** are fundamental DDL statements in ScratchBird that manage privileges and permissions for database objects and operations. These statements form the foundation of ScratchBird's comprehensive security model, providing fine-grained access control for all database resources including objects, schemas, operations, and administrative functions.

### Privilege Management Features

ScratchBird provides one of the most comprehensive privilege management systems available in SQL databases:

- **Object Privileges**: Control access to specific database objects (tables, views, procedures, etc.)
- **System Privileges**: Control database administration and DDL operations
- **Schema Privileges**: Hierarchical schema-level permissions (ScratchBird enhancement)
- **Role Management**: Role-based access control with inheritance and administration
- **Advanced Options**: Grant options, grantor specification, and privilege inheritance

### ScratchBird Enhancements

ScratchBird extends standard SQL privilege management with:

| Feature | Standard SQL | ScratchBird Enhancement |
|---------|-------------|------------------------|
| **Schema Privileges** | Basic schema support | Hierarchical schema permissions with up to 3-level nesting |
| **Object Types** | Standard objects | Extended object types (packages, exceptions, database links) |
| **DDL Privileges** | Limited DDL control | Comprehensive DDL privilege management |
| **Grant Granularity** | Object-level | Column-level and schema-level granularity |
| **Administrative Control** | Basic roles | Advanced role hierarchy with system privileges |

---

## GRANT Statement Syntax

The GRANT statement assigns privileges to users and roles for specific database objects or operations.

### Basic GRANT Syntax

```sql
-- Object privileges
GRANT privilege_list ON object_name TO grantee_list [WITH GRANT OPTION] [GRANTED BY grantor]

-- Role assignment
GRANT role_list TO grantee_list [WITH ADMIN OPTION] [GRANTED BY grantor]

-- System privileges
GRANT system_privilege_list TO grantee_list [WITH ADMIN OPTION] [GRANTED BY grantor]
```

### Privilege Types

#### **Data Manipulation Privileges**
- **SELECT** - Query data from tables and views
- **INSERT** - Add new records to tables
- **UPDATE** - Modify existing records (with optional column specification)
- **DELETE** - Remove records from tables
- **REFERENCES** - Create foreign key constraints (with optional column specification)
- **ALL** - All applicable data manipulation privileges

#### **Execution Privileges**
- **EXECUTE** - Execute procedures, functions, and packages

#### **Usage Privileges**
- **USAGE** - Use exceptions, sequences, generators, schemas, domains, character sets, collations

#### **DDL Privileges**
- **CREATE** - Create objects in schemas or database
- **ALTER** - Modify object definitions (with ANY option for cross-schema operations)
- **DROP** - Remove objects (with ANY option for cross-schema operations)

---

## Object Privilege Management

Object privileges control access to specific database objects.

### Table and View Privileges

#### Basic Table Privileges
```sql
-- Grant SELECT privilege on table
GRANT SELECT ON customers TO user1;

-- Grant multiple privileges
GRANT SELECT, INSERT, UPDATE, DELETE ON customers TO sales_team;

-- Grant all privileges on table
GRANT ALL ON customers TO admin_user;

-- Grant with grant option
GRANT SELECT, UPDATE ON customers TO team_lead WITH GRANT OPTION;
```

#### Column-Level Privileges
```sql
-- Grant UPDATE on specific columns
GRANT UPDATE (first_name, last_name, email) ON customers TO hr_staff;

-- Grant REFERENCES on specific columns
GRANT REFERENCES (customer_id) ON customers TO app_user;

-- Multiple column privileges
GRANT SELECT ON customers TO readonly_user;
GRANT UPDATE (phone, address, city, state) ON customers TO contact_updater;
```

#### View Privileges
```sql
-- Grant privileges on views
GRANT SELECT ON customer_summary_view TO report_users;
GRANT SELECT ON finance.accounting.monthly_reports TO finance_analysts;

-- Grant with hierarchical schema qualification
GRANT SELECT ON enterprise.sales.customer_analytics_view TO regional_managers;
```

### Procedure and Function Privileges

#### Stored Procedure Privileges
```sql
-- Grant EXECUTE on stored procedures
GRANT EXECUTE ON calculate_commission TO sales_staff;
GRANT EXECUTE ON process_monthly_billing TO billing_team;

-- Grant with hierarchical schema
GRANT EXECUTE ON finance.accounting.close_period TO accounting_managers;

-- Grant with grant option for administrative procedures
GRANT EXECUTE ON admin.user_management.reset_password TO help_desk WITH GRANT OPTION;
```

#### User-Defined Function Privileges
```sql
-- Grant EXECUTE on functions
GRANT EXECUTE ON format_currency TO report_generators;
GRANT EXECUTE ON calculate_tax_rate TO payroll_system;

-- Grant on function with schema qualification
GRANT EXECUTE ON utilities.string_functions.clean_phone_number TO data_processors;

-- Package function access through package privileges
GRANT EXECUTE ON math_utilities TO scientific_apps;
```

### Package Privileges

#### Package-Level Access Control
```sql
-- Grant EXECUTE on entire package
GRANT EXECUTE ON accounting_utilities TO finance_team;

-- Grant on hierarchical schema packages
GRANT EXECUTE ON enterprise.hr.employee_management TO hr_administrators;

-- Grant with administrative rights
GRANT EXECUTE ON system.admin_tools TO database_administrators WITH GRANT OPTION;

-- Package access for application roles
GRANT EXECUTE ON api.customer_services TO web_application;
GRANT EXECUTE ON api.inventory_management TO mobile_application;
```

### Exception and Generator Privileges

#### Exception Usage Rights
```sql
-- Grant USAGE on exceptions
GRANT USAGE ON insufficient_balance TO banking_applications;
GRANT USAGE ON data_validation_error TO input_processors;

-- Grant on schema-qualified exceptions
GRANT USAGE ON finance.accounting.period_closed_exception TO accounting_software;

-- Grant with grant option
GRANT USAGE ON custom_business_rules.validation_failed TO app_developers WITH GRANT OPTION;
```

#### Sequence and Generator Access
```sql
-- Grant USAGE on sequences
GRANT USAGE ON customer_id_seq TO order_processing;
GRANT USAGE ON invoice_number_generator TO billing_system;

-- Grant on schema-qualified generators
GRANT USAGE ON enterprise.global.entity_id_generator TO all_applications;

-- Administrative access to sequences
GRANT ALL ON SEQUENCE customer_seq TO sequence_administrators;
```

---

## Schema Privilege Management (ScratchBird Enhancement)

ScratchBird's hierarchical schema system provides advanced schema-level privilege management.

### Schema-Level Privileges

#### Schema Usage Rights
```sql
-- Grant USAGE on schema (allows object access within schema)
GRANT USAGE ON SCHEMA finance TO finance_users;
GRANT USAGE ON SCHEMA finance.accounting TO accounting_staff;
GRANT USAGE ON SCHEMA finance.accounting.reports TO report_viewers;

-- Grant USAGE with hierarchical wildcards
GRANT USAGE ON SCHEMA finance.* TO finance_department;
GRANT USAGE ON SCHEMA enterprise.americas.* TO americas_regional_staff;
```

#### Schema Creation Rights
```sql
-- Grant CREATE privileges within schema
GRANT CREATE ON SCHEMA development TO developers;
GRANT CREATE TABLE ON SCHEMA finance.accounting TO accounting_managers;
GRANT CREATE PROCEDURE ON SCHEMA utilities TO procedure_developers;

-- Grant comprehensive DDL rights
GRANT CREATE, ALTER, DROP ON SCHEMA finance.accounting TO accounting_administrators;

-- Grant with hierarchical schema support
GRANT CREATE ON SCHEMA enterprise.*.development TO development_teams;
```

### Hierarchical Schema Permissions

#### Multi-Level Schema Access
```sql
-- Grant access to nested schema hierarchies
GRANT USAGE ON SCHEMA company.division.department TO department_staff;
GRANT CREATE TABLE ON SCHEMA company.division.department.projects TO project_managers;

-- Grant with inheritance to child schemas
GRANT USAGE ON SCHEMA finance.* TO finance_department;
-- This grants access to finance.accounting, finance.budgeting, finance.reporting, etc.

-- Specific nested schema access
GRANT SELECT ON SCHEMA finance.accounting.* TO auditors;
GRANT INSERT, UPDATE ON SCHEMA finance.accounting.transactions.* TO transaction_processors;
```

#### Schema Wildcard Patterns
```sql
-- Grant access using wildcard patterns
GRANT USAGE ON SCHEMA *.development TO all_developers;
GRANT USAGE ON SCHEMA enterprise.*.testing TO qa_teams;
GRANT CREATE TABLE ON SCHEMA company.*.temp TO temporary_workspace_users;

-- Comprehensive schema access patterns
GRANT ALL ON SCHEMA company.division.* TO division_administrators;
GRANT SELECT ON SCHEMA *.reporting.* TO business_intelligence_team;
```

---

## Role Management and Assignment

Roles provide a powerful way to group privileges and assign them to users collectively.

### Role Assignment

#### Basic Role Assignment
```sql
-- Assign single role to user
GRANT sales_representative TO john_doe;
GRANT accounting_clerk TO jane_smith;

-- Assign multiple roles to user
GRANT sales_representative, customer_service TO multi_role_user;

-- Assign role to multiple users
GRANT report_viewer TO analyst1, analyst2, analyst3;
```

#### Administrative Role Assignment
```sql
-- Grant role with administrative privileges
GRANT department_manager TO team_lead WITH ADMIN OPTION;
GRANT finance_administrator TO senior_accountant WITH ADMIN OPTION;

-- Grant default role assignment
GRANT DEFAULT sales_representative TO new_sales_employee;

-- Combination of regular and default roles
GRANT sales_representative TO employee_user;
GRANT DEFAULT customer_service TO employee_user;
```

### Role Hierarchy and Inheritance

#### Hierarchical Role Structure
```sql
-- Create role hierarchy through assignment
-- Base roles
GRANT employee TO all_staff_members;

-- Department roles (inherit from employee)
GRANT finance_user TO finance_department_staff;
GRANT sales_user TO sales_department_staff;
GRANT it_user TO it_department_staff;

-- Management roles (inherit from department roles)
GRANT finance_manager TO accounting_supervisors;
GRANT sales_manager TO sales_supervisors;

-- Executive roles (inherit from management roles)  
GRANT finance_director TO cfo, finance_vp;
GRANT sales_director TO coo, sales_vp;
```

#### System Privilege Roles
```sql
-- Grant system administration roles
GRANT database_administrator TO dba_team;
GRANT user_manager TO hr_administrators;
GRANT backup_operator TO operations_staff;

-- Grant system privileges through roles
-- (System privileges are typically assigned to roles, then roles to users)
CREATE ROLE system_admin;
-- System privileges would be granted to system_admin role
GRANT system_admin TO senior_dba WITH ADMIN OPTION;
```

---

## System Privilege Management

System privileges control database administration and DDL operations.

### Database-Level Privileges

#### Database Administration
```sql
-- Grant database-level DDL privileges
GRANT CREATE, ALTER, DROP DATABASE TO database_administrators;

-- Grant backup and restore privileges
GRANT BACKUP DATABASE TO backup_operators;
GRANT RESTORE DATABASE TO disaster_recovery_team;

-- Grant monitoring and maintenance privileges
GRANT MONITOR DATABASE TO performance_analysts;
GRANT MAINTENANCE DATABASE TO maintenance_crew;
```

#### User and Security Management
```sql
-- Grant user management privileges
GRANT USER_MANAGEMENT TO hr_administrators;
GRANT ROLE_MANAGEMENT TO security_administrators;

-- Grant authentication management
GRANT AUTHENTICATION_MANAGEMENT TO identity_administrators;
GRANT PRIVILEGE_MANAGEMENT TO privilege_administrators;
```

### DDL Object Management

#### Schema-Level DDL Privileges
```sql
-- Grant DDL privileges for specific object types
GRANT CREATE TABLE TO table_designers;
GRANT CREATE PROCEDURE TO procedure_developers;
GRANT CREATE FUNCTION TO function_developers;
GRANT CREATE PACKAGE TO package_architects;

-- Grant ALTER privileges
GRANT ALTER ANY TABLE TO schema_administrators;
GRANT ALTER ANY PROCEDURE TO procedure_maintainers;

-- Grant DROP privileges
GRANT DROP ANY VIEW TO view_administrators;
GRANT DROP ANY SEQUENCE TO sequence_managers;
```

#### Cross-Schema DDL Privileges
```sql
-- Grant privileges to work across schemas
GRANT ALTER ANY TABLE TO cross_schema_administrators;
GRANT DROP ANY PROCEDURE TO cleanup_administrators;
GRANT CREATE ANY INDEX TO performance_optimizers;

-- Grant comprehensive DDL management
GRANT CREATE ANY, ALTER ANY, DROP ANY TO senior_developers;
```

---

## Advanced GRANT Options and Features

### WITH GRANT OPTION

The WITH GRANT OPTION allows grantees to grant the same privileges to other users.

#### Privilege Delegation
```sql
-- Grant with delegation capability
GRANT SELECT, INSERT ON customers TO team_lead WITH GRANT OPTION;

-- Now team_lead can grant these privileges to team members
-- (executed by team_lead)
GRANT SELECT ON customers TO team_member1;
GRANT SELECT, INSERT ON customers TO team_member2;

-- Grant role with administrative delegation
GRANT finance_user TO finance_manager WITH ADMIN OPTION;

-- Now finance_manager can assign finance_user role to others
-- (executed by finance_manager)  
GRANT finance_user TO new_finance_employee;
```

#### Cascading Privileges
```sql
-- Create privilege hierarchy through grant options
GRANT SELECT ON sensitive_data TO senior_analyst WITH GRANT OPTION;

-- Senior analyst delegates limited access
-- (executed by senior_analyst)
GRANT SELECT ON sensitive_data TO junior_analyst;

-- Junior analyst cannot further delegate (no grant option received)
```

### GRANTED BY Clause

The GRANTED BY clause specifies who is granting the privilege, useful for auditing and administrative control.

#### Administrative Privilege Assignment
```sql
-- Grant privileges on behalf of object owner
GRANT SELECT ON employees TO hr_staff GRANTED BY hr_manager;

-- Grant with explicit grantor for auditing
GRANT EXECUTE ON payroll_procedures TO payroll_team GRANTED BY payroll_supervisor;

-- System administrator granting on behalf of users
GRANT ALL ON user_tables TO backup_service GRANTED BY system_administrator;
```

#### Privilege Transfer and Auditing
```sql
-- Transfer privileges with proper attribution
GRANT UPDATE ON customer_data TO data_steward GRANTED BY data_owner;

-- Administrative override with documentation
GRANT EMERGENCY_ACCESS TO incident_responder GRANTED BY security_administrator;

-- Audit trail through explicit grantor specification
GRANT TEMPORARY_ELEVATED_ACCESS TO contractor GRANTED BY project_manager;
```

---

## REVOKE Statement Syntax

The REVOKE statement removes previously granted privileges from users and roles.

### Basic REVOKE Syntax

```sql
-- Revoke object privileges
REVOKE privilege_list ON object_name FROM grantee_list [GRANTED BY grantor]

-- Revoke role assignment
REVOKE role_list FROM grantee_list [GRANTED BY grantor]

-- Revoke grant option only
REVOKE GRANT OPTION FOR privilege_list ON object_name FROM grantee_list

-- Revoke admin option only
REVOKE ADMIN OPTION FOR role_list FROM grantee_list
```

### Object Privilege Revocation

#### Basic Privilege Removal
```sql
-- Revoke specific privileges
REVOKE SELECT ON customers FROM former_employee;
REVOKE INSERT, UPDATE, DELETE ON customers FROM demoted_user;

-- Revoke all privileges
REVOKE ALL ON customers FROM terminated_contractor;

-- Revoke from multiple grantees
REVOKE SELECT ON sensitive_data FROM user1, user2, user3;
```

#### Column-Level Privilege Revocation
```sql
-- Revoke column-specific privileges
REVOKE UPDATE (salary, bonus) ON employees FROM non_hr_user;
REVOKE REFERENCES (customer_id) ON customers FROM external_app;

-- Revoke while maintaining other privileges
REVOKE UPDATE (credit_limit) ON customers FROM sales_rep;
-- (sales_rep retains other UPDATE privileges on other columns)
```

### Role Revocation

#### Role Removal
```sql
-- Revoke roles from users
REVOKE sales_representative FROM transferred_employee;
REVOKE finance_user, report_viewer FROM role_change_user;

-- Revoke administrative privileges
REVOKE department_manager FROM demoted_supervisor;
```

#### Administrative Option Revocation
```sql
-- Revoke admin option while maintaining role
REVOKE ADMIN OPTION FOR finance_manager FROM former_admin;
-- (User retains finance_manager role but cannot assign it to others)

-- Revoke role entirely
REVOKE finance_manager FROM terminated_manager;
```

### Grant Option Revocation

#### Selective Grant Option Removal
```sql
-- Revoke grant option while maintaining privilege
REVOKE GRANT OPTION FOR SELECT ON customers FROM team_lead;
-- (team_lead retains SELECT privilege but cannot grant it to others)

-- Revoke grant option for specific privileges
REVOKE GRANT OPTION FOR INSERT, UPDATE ON customers FROM former_supervisor;

-- Complete privilege removal
REVOKE SELECT, INSERT, UPDATE ON customers FROM former_supervisor;
```

### Cascade Effects and Dependencies

#### Understanding Revocation Cascading
```sql
-- Original grants with delegation
GRANT SELECT ON customers TO manager WITH GRANT OPTION;
-- Manager delegates to subordinate
GRANT SELECT ON customers TO subordinate GRANTED BY manager;

-- Revoking from manager cascades to subordinate
REVOKE SELECT ON customers FROM manager;
-- This automatically revokes SELECT from subordinate as well

-- Prevent cascading by revoking grant option first
REVOKE GRANT OPTION FOR SELECT ON customers FROM manager;
-- subordinate retains SELECT privilege
-- Then optionally revoke from manager
REVOKE SELECT ON customers FROM manager;
```

---

## Permission Analysis and Monitoring

Understanding and monitoring the current privilege state is crucial for security management.

### Querying Current Privileges

#### User Privilege Analysis
```sql
-- View all privileges for a specific user
SELECT 
    p.RDB$USER as GRANTEE,
    p.RDB$RELATION_NAME as OBJECT_NAME,
    p.RDB$OBJECT_TYPE as OBJECT_TYPE,
    p.RDB$PRIVILEGE as PRIVILEGE,
    p.RDB$GRANT_OPTION as HAS_GRANT_OPTION,
    p.RDB$GRANTOR as GRANTED_BY
FROM RDB$USER_PRIVILEGES p
WHERE p.RDB$USER = 'SPECIFIC_USER'
ORDER BY p.RDB$RELATION_NAME, p.RDB$PRIVILEGE;

-- View role assignments for user
SELECT 
    p.RDB$USER as USER_NAME,
    p.RDB$RELATION_NAME as ROLE_NAME,
    p.RDB$GRANT_OPTION as HAS_ADMIN_OPTION,
    p.RDB$GRANTOR as GRANTED_BY
FROM RDB$USER_PRIVILEGES p
WHERE p.RDB$USER = 'SPECIFIC_USER'
  AND p.RDB$OBJECT_TYPE = 13  -- Role object type
ORDER BY p.RDB$RELATION_NAME;
```

#### Object Privilege Analysis
```sql
-- View all users with privileges on specific object
SELECT 
    p.RDB$USER as GRANTEE,
    p.RDB$PRIVILEGE as PRIVILEGE,
    p.RDB$GRANT_OPTION as HAS_GRANT_OPTION,
    p.RDB$GRANTOR as GRANTED_BY,
    p.RDB$FIELD_NAME as COLUMN_NAME
FROM RDB$USER_PRIVILEGES p
WHERE p.RDB$RELATION_NAME = 'CUSTOMERS'
  AND p.RDB$OBJECT_TYPE = 0  -- Table object type
ORDER BY p.RDB$USER, p.RDB$PRIVILEGE;

-- Analyze schema-level privileges
SELECT 
    p.RDB$USER as GRANTEE,
    p.RDB$RELATION_NAME as SCHEMA_NAME,
    p.RDB$PRIVILEGE as PRIVILEGE,
    p.RDB$GRANT_OPTION as HAS_GRANT_OPTION
FROM RDB$USER_PRIVILEGES p
WHERE p.RDB$OBJECT_TYPE = 38  -- Schema object type
ORDER BY p.RDB$RELATION_NAME, p.RDB$USER;
```

### Comprehensive Privilege Reports

#### Security Audit Report
```sql
-- Generate comprehensive security audit
SELECT 
    'USER_PRIVILEGE' as PRIVILEGE_TYPE,
    p.RDB$USER as GRANTEE,
    p.RDB$RELATION_NAME as OBJECT_NAME,
    CASE p.RDB$OBJECT_TYPE
        WHEN 0 THEN 'TABLE'
        WHEN 1 THEN 'VIEW'
        WHEN 5 THEN 'PROCEDURE'
        WHEN 7 THEN 'EXCEPTION'
        WHEN 13 THEN 'ROLE'
        WHEN 14 THEN 'GENERATOR'
        WHEN 15 THEN 'FUNCTION'
        WHEN 18 THEN 'PACKAGE'
        WHEN 38 THEN 'SCHEMA'
        ELSE 'OTHER'
    END as OBJECT_TYPE,
    p.RDB$PRIVILEGE as PRIVILEGE,
    p.RDB$GRANT_OPTION as HAS_GRANT_OPTION,
    p.RDB$GRANTOR as GRANTED_BY
FROM RDB$USER_PRIVILEGES p
WHERE p.RDB$USER NOT IN ('SYSDBA', 'RDB$ADMIN')  -- Exclude system users
ORDER BY p.RDB$USER, p.RDB$RELATION_NAME, p.RDB$PRIVILEGE;

-- Role membership analysis
SELECT 
    'ROLE_MEMBERSHIP' as PRIVILEGE_TYPE,
    p.RDB$USER as USER_NAME,
    p.RDB$RELATION_NAME as ROLE_NAME,
    p.RDB$GRANT_OPTION as HAS_ADMIN_OPTION,
    p.RDB$GRANTOR as GRANTED_BY
FROM RDB$USER_PRIVILEGES p
WHERE p.RDB$OBJECT_TYPE = 13  -- Role assignments
ORDER BY p.RDB$RELATION_NAME, p.RDB$USER;
```

#### Privilege Summary by Object
```sql
-- Summarize privileges by object type
SELECT 
    CASE p.RDB$OBJECT_TYPE
        WHEN 0 THEN 'TABLE'
        WHEN 1 THEN 'VIEW'
        WHEN 5 THEN 'PROCEDURE'
        WHEN 15 THEN 'FUNCTION'
        WHEN 18 THEN 'PACKAGE'
        WHEN 38 THEN 'SCHEMA'
        ELSE 'OTHER'
    END as OBJECT_TYPE,
    COUNT(DISTINCT p.RDB$RELATION_NAME) as OBJECT_COUNT,
    COUNT(*) as TOTAL_PRIVILEGES,
    COUNT(CASE WHEN p.RDB$GRANT_OPTION = 1 THEN 1 END) as GRANT_OPTIONS,
    COUNT(DISTINCT p.RDB$USER) as UNIQUE_GRANTEES
FROM RDB$USER_PRIVILEGES p
WHERE p.RDB$USER NOT IN ('SYSDBA', 'RDB$ADMIN')
GROUP BY p.RDB$OBJECT_TYPE
ORDER BY TOTAL_PRIVILEGES DESC;
```

---

## Troubleshooting and Common Issues

### Common GRANT/REVOKE Errors

#### Insufficient Privileges Error
```sql
-- Error: "no permission for INSERT/WRITE access to TABLE CUSTOMERS"
-- Solution: Grant appropriate privilege
GRANT INSERT ON customers TO user_name;

-- Error: "no permission for ALTER access to PROCEDURE procedure_name"  
-- Solution: Grant DDL privilege or object ownership
GRANT ALTER ANY PROCEDURE TO user_name;
-- OR
GRANT ALTER ON PROCEDURE procedure_name TO user_name;
```

#### Object Not Found Error
```sql
-- Error: "Table CUSTOMERS not found"
-- Check object exists and user has USAGE on schema
GRANT USAGE ON SCHEMA schema_name TO user_name;

-- For hierarchical schemas, ensure parent schema access
GRANT USAGE ON SCHEMA finance TO user_name;
GRANT USAGE ON SCHEMA finance.accounting TO user_name;
```

#### Circular Grant Dependencies
```sql
-- Error: "Circular grant dependency detected"
-- Occurs when trying to grant privileges that would create circular references

-- Example problematic sequence:
-- GRANT role1 TO role2;
-- GRANT role2 TO role3;  
-- GRANT role3 TO role1;  -- This creates circular dependency

-- Solution: Redesign role hierarchy to eliminate circular references
```

### Permission Debugging

#### Diagnosing Access Issues
```sql
-- Check if user exists
SELECT RDB$USER_NAME FROM RDB$USERS WHERE RDB$USER_NAME = 'USERNAME';

-- Check current user's privileges
SELECT CURRENT_USER;
SELECT RDB$GET_CONTEXT('SECURITY', 'ACTIVE_ROLES');

-- Verify object existence and ownership
SELECT 
    r.RDB$RELATION_NAME,
    r.RDB$OWNER_NAME,
    r.RDB$RELATION_TYPE
FROM RDB$RELATIONS r
WHERE r.RDB$RELATION_NAME = 'OBJECT_NAME';
```

#### Schema Access Diagnosis
```sql
-- Check schema hierarchy access
SELECT RDB$GET_CONTEXT('SCHEMA', 'ACCESSIBLE_SCHEMAS');
SELECT RDB$GET_CONTEXT('SCHEMA', 'CURRENT_PATH');

-- Verify hierarchical schema permissions
SELECT 
    p.RDB$USER,
    p.RDB$RELATION_NAME as SCHEMA_NAME,
    p.RDB$PRIVILEGE
FROM RDB$USER_PRIVILEGES p
WHERE p.RDB$OBJECT_TYPE = 38  -- Schema privileges
  AND p.RDB$USER = CURRENT_USER
ORDER BY p.RDB$RELATION_NAME;
```

### Security Best Practices

#### Principle of Least Privilege
```sql
-- Good: Grant minimal necessary privileges
GRANT SELECT ON customer_summary_view TO report_user;
GRANT EXECUTE ON generate_monthly_report TO report_user;

-- Avoid: Excessive privilege grants
-- GRANT ALL ON ALL TABLES TO report_user;  -- Too broad

-- Better: Use roles for privilege grouping
CREATE ROLE report_generator;
GRANT SELECT ON customer_summary_view TO report_generator;
GRANT SELECT ON sales_summary_view TO report_generator;
GRANT EXECUTE ON generate_monthly_report TO report_generator;
GRANT report_generator TO report_user;
```

#### Regular Privilege Audits
```sql
-- Create audit procedure for regular review
CREATE PROCEDURE audit_user_privileges(username VARCHAR(63))
AS
DECLARE VARIABLE privilege_count INTEGER;
DECLARE VARIABLE grant_option_count INTEGER;
BEGIN
    -- Count total privileges
    SELECT COUNT(*) FROM RDB$USER_PRIVILEGES 
    WHERE RDB$USER = :username INTO :privilege_count;
    
    -- Count grant options
    SELECT COUNT(*) FROM RDB$USER_PRIVILEGES 
    WHERE RDB$USER = :username AND RDB$GRANT_OPTION = 1 
    INTO :grant_option_count;
    
    -- Log audit results
    INSERT INTO privilege_audit_log (
        audit_date, username, privilege_count, grant_option_count
    ) VALUES (
        CURRENT_TIMESTAMP, :username, :privilege_count, :grant_option_count
    );
END;

-- Regular privilege cleanup
CREATE PROCEDURE cleanup_inactive_user_privileges
AS
BEGIN
    -- Revoke privileges from inactive users
    FOR SELECT RDB$USER_NAME FROM RDB$USERS 
        WHERE RDB$ACTIVE = 0  -- Inactive users
        INTO :username
    DO
    BEGIN
        -- Revoke all privileges from inactive user
        EXECUTE STATEMENT 'REVOKE ALL ON ALL FROM ' || :username;
    END
END;
```

---

## Performance Considerations

### Grant/Revoke Performance

#### Batch Operations
```sql
-- Efficient: Single statement with multiple grantees
GRANT SELECT ON customers TO user1, user2, user3, user4, user5;

-- Inefficient: Multiple individual grants
-- GRANT SELECT ON customers TO user1;
-- GRANT SELECT ON customers TO user2;
-- GRANT SELECT ON customers TO user3;
-- GRANT SELECT ON customers TO user4;
-- GRANT SELECT ON customers TO user5;

-- Efficient: Role-based privilege management
CREATE ROLE customer_readers;
GRANT SELECT ON customers TO customer_readers;
GRANT customer_readers TO user1, user2, user3, user4, user5;
```

#### Privilege Checking Performance
```sql
-- Efficient: Use roles for grouping similar privileges
CREATE ROLE finance_base;
GRANT SELECT ON finance_tables TO finance_base;
GRANT finance_base TO finance_users;

-- Inefficient: Individual grants to many users
-- GRANT SELECT ON finance_table1 TO user1;
-- GRANT SELECT ON finance_table1 TO user2;
-- ... (repeated for many users and tables)

-- Efficient: Schema-level privileges for related objects
GRANT USAGE ON SCHEMA finance.accounting TO accounting_team;
GRANT SELECT ON SCHEMA finance.accounting.* TO accounting_team;
```

### Security Impact on Performance

#### Index and Query Plan Considerations
```sql
-- Security views can impact performance
-- Consider creating optimized security views
CREATE VIEW secure_customer_view AS
SELECT customer_id, customer_name, phone, email
FROM customers 
WHERE customer_id IN (
    SELECT customer_id FROM customer_access_control
    WHERE user_name = CURRENT_USER
);

-- Index supporting security predicates
CREATE INDEX idx_customer_access_user ON customer_access_control(user_name, customer_id);
```

#### Monitoring Privilege Check Performance
```sql
-- Monitor privilege checking overhead
SELECT 
    RDB$GET_CONTEXT('PERFORMANCE', 'SECURITY_CHECKS') as security_checks,
    RDB$GET_CONTEXT('PERFORMANCE', 'PRIVILEGE_CACHE_HITS') as cache_hits,
    RDB$GET_CONTEXT('PERFORMANCE', 'PRIVILEGE_CACHE_MISSES') as cache_misses;

-- Calculate privilege check efficiency
SELECT 
    CAST(RDB$GET_CONTEXT('PERFORMANCE', 'PRIVILEGE_CACHE_HITS') AS FLOAT) /
    (CAST(RDB$GET_CONTEXT('PERFORMANCE', 'PRIVILEGE_CACHE_HITS') AS FLOAT) + 
     CAST(RDB$GET_CONTEXT('PERFORMANCE', 'PRIVILEGE_CACHE_MISSES') AS FLOAT)) * 100 
    as cache_hit_ratio_percent;
```

---

## Integration with ScratchBird Features

### Database Link Privileges

ScratchBird's database links require specific privilege management:

```sql
-- Grant database link privileges
GRANT CONNECT ON DATABASE LINK finance_link TO finance_users;
GRANT QUERY ON DATABASE LINK reporting_link TO report_generators;

-- Grant database link administration
GRANT CREATE DATABASE LINK TO link_administrators;
GRANT ALTER DATABASE LINK TO link_managers;
GRANT DROP DATABASE LINK TO link_administrators;

-- Schema-aware database link privileges
GRANT USAGE ON DATABASE LINK hr_link.employee_schema TO hr_applications;
```

### Spatial Data Privileges

For ScratchBird's spatial data features:

```sql
-- Grant spatial function privileges
GRANT EXECUTE ON spatial_functions TO gis_applications;
GRANT USAGE ON SPATIAL_REFERENCE_SYSTEMS TO mapping_software;

-- Grant spatial index management
GRANT CREATE SPATIAL INDEX TO spatial_administrators;
GRANT ALTER SPATIAL INDEX TO performance_tuners;
```

### Vector and AI/ML Privileges

For ScratchBird's AI/ML vector operations:

```sql
-- Grant vector operation privileges
GRANT EXECUTE ON vector_functions TO ai_applications;
GRANT USAGE ON VECTOR_MODELS TO machine_learning_apps;

-- Grant vector index management
GRANT CREATE VECTOR INDEX TO ml_administrators;
GRANT VECTOR_SIMILARITY_SEARCH TO ai_query_engines;
```

---

