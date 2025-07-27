# ScratchBird USER - Complete DDL Documentation

**Version**: Alpha 0.6.0  
**Implementation Date**: July 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  
**Documentation Type**: User Guide & Technical Reference

---

## Overview

USER objects in ScratchBird provide comprehensive user account management for database authentication and authorization. Users represent individual database accounts with configurable properties including names, passwords, roles, and schema associations. The user management system integrates with ScratchBird's security architecture to provide fine-grained access control and user administration capabilities.

### Key Features and Capabilities

- **User Account Creation**: Create individual database user accounts with unique identifiers
- **Personal Information Management**: Store first name, middle name, and last name
- **Password Authentication**: Secure password-based authentication with configurable policies
- **Role-Based Security**: Integration with role-based access control system
- **Admin Role Assignment**: Grant administrative privileges to users
- **Account Status Control**: Enable/disable user accounts (ACTIVE/INACTIVE)
- **Home Schema Assignment**: Default schema association for user sessions
- **Plugin-Based Authentication**: Support for external authentication providers
- **Comment and Description**: Administrative notes and user descriptions

### ScratchBird-Specific Enhancements

1. **Extended User Properties**: First, middle, and last name fields for complete user identity
2. **Home Schema Integration**: Automatic schema context assignment with hierarchical support
3. **Plugin Authentication System**: Extensible authentication through external plugins
4. **Enhanced System Catalog**: Comprehensive SEC$USERS virtual table with rich metadata
5. **DDL Trigger Integration**: Support for DDL triggers on user operations
6. **Advanced Permission Model**: Granular user privilege management
7. **Current User Management**: Special ALTER CURRENT USER syntax for self-management
8. **Comment System**: Built-in user comment and description management

---

## DDL Syntax Reference

### CREATE USER

Creates a new database user account with specified properties and credentials.

#### **Basic Syntax**
```sql
CREATE USER [IF NOT EXISTS] username
    [PASSWORD 'password']
    [FIRSTNAME 'first_name']
    [MIDDLENAME 'middle_name']
    [LASTNAME 'last_name']
    [GRANT ADMIN ROLE | REVOKE ADMIN ROLE]
    [ACTIVE | INACTIVE]
    [HOME SCHEMA schema_name];
```

#### **Complete Syntax with All Options**
```sql
CREATE USER [IF NOT EXISTS] username
    [PASSWORD 'user_password']
    [FIRSTNAME 'user_first_name']
    [MIDDLENAME 'user_middle_name']
    [LASTNAME 'user_last_name']
    [GRANT ADMIN ROLE | REVOKE ADMIN ROLE]
    [ACTIVE | INACTIVE]
    [HOME SCHEMA [catalog.]schema.subschema]
    [USING PLUGIN plugin_name];
```

#### **Parameters**

- **IF NOT EXISTS**: Skip creation if user already exists (no error)
- **username**: User identifier (63 characters max, case-insensitive)
- **PASSWORD**: Authentication password (encrypted storage)
- **FIRSTNAME/MIDDLENAME/LASTNAME**: Personal name components (optional)
- **GRANT/REVOKE ADMIN ROLE**: Administrative privilege assignment
- **ACTIVE/INACTIVE**: Account status (default: ACTIVE)
- **HOME SCHEMA**: Default schema for user sessions
- **USING PLUGIN**: External authentication plugin specification

---

## CREATE USER Examples

### **Basic User Creation**

#### **Simple User Accounts**
```sql
-- Create basic user with password
CREATE USER john_doe PASSWORD 'secure123';

-- Create user with personal information
CREATE USER jane_smith 
    PASSWORD 'mypassword'
    FIRSTNAME 'Jane'
    LASTNAME 'Smith';

-- Create user with full name details
CREATE USER robert_johnson
    PASSWORD 'strongpass'
    FIRSTNAME 'Robert'
    MIDDLENAME 'Michael'
    LASTNAME 'Johnson';
```

#### **Administrative Users**
```sql
-- Create administrator with admin role
CREATE USER db_admin
    PASSWORD 'admin_secret'
    FIRSTNAME 'Database'
    LASTNAME 'Administrator'
    GRANT ADMIN ROLE;

-- Create backup administrator
CREATE USER backup_admin
    PASSWORD 'backup123'
    FIRSTNAME 'Backup'
    LASTNAME 'Administrator'
    GRANT ADMIN ROLE
    ACTIVE;
```

### **Users with Schema Assignments**

#### **Home Schema Configuration**
```sql
-- Create user with default schema
CREATE USER finance_user
    PASSWORD 'finance123'
    FIRSTNAME 'Finance'
    LASTNAME 'User'
    HOME SCHEMA finance;

-- Create user with hierarchical schema
CREATE USER accounting_clerk
    PASSWORD 'accounting456'
    FIRSTNAME 'Accounting'
    LASTNAME 'Clerk'
    HOME SCHEMA enterprise.finance.accounting;

-- Create user with deep schema hierarchy
CREATE USER regional_analyst
    PASSWORD 'analyst789'
    FIRSTNAME 'Regional'
    LASTNAME 'Analyst'
    HOME SCHEMA company.regions.northamerica.sales.reports;
```

### **Application-Specific Users**

#### **System Service Accounts**
```sql
-- Create application service user
CREATE USER app_service
    PASSWORD 'service_secret'
    FIRSTNAME 'Application'
    LASTNAME 'Service'
    HOME SCHEMA application
    ACTIVE;

-- Create reporting service user
CREATE USER report_service
    PASSWORD 'report_access'
    FIRSTNAME 'Report'
    LASTNAME 'Service'
    HOME SCHEMA enterprise.reports
    ACTIVE;

-- Create batch processing user
CREATE USER batch_processor
    PASSWORD 'batch123'
    FIRSTNAME 'Batch'
    LASTNAME 'Processor'
    HOME SCHEMA processing.batch
    GRANT ADMIN ROLE;
```

#### **Business Role Users**
```sql
-- Create sales manager
CREATE USER sales_manager
    PASSWORD 'sales_mgr123'
    FIRSTNAME 'Sales'
    LASTNAME 'Manager'
    HOME SCHEMA enterprise.sales
    ACTIVE;

-- Create HR coordinator
CREATE USER hr_coordinator
    PASSWORD 'hr_coord456'
    FIRSTNAME 'Human Resources'
    LASTNAME 'Coordinator'
    HOME SCHEMA enterprise.hr.administration;

-- Create financial analyst
CREATE USER financial_analyst
    PASSWORD 'finance_analyst'
    FIRSTNAME 'Financial'
    LASTNAME 'Analyst'
    HOME SCHEMA enterprise.finance.analysis;
```

### **Plugin-Based Authentication**

#### **External Authentication Users**
```sql
-- Create user with LDAP authentication
CREATE USER ldap_user
    FIRSTNAME 'LDAP'
    LASTNAME 'User'
    HOME SCHEMA enterprise
    USING PLUGIN Srp256;

-- Create user with Active Directory authentication
CREATE USER ad_user
    FIRSTNAME 'Active Directory'
    LASTNAME 'User'
    HOME SCHEMA corporate
    USING PLUGIN Win_Sspi;

-- Create user with custom plugin
CREATE USER oauth_user
    FIRSTNAME 'OAuth'
    LASTNAME 'User'
    HOME SCHEMA external_users
    USING PLUGIN CustomOAuth;
```

### **Account Status Management**

#### **Active and Inactive Users**
```sql
-- Create active user (default)
CREATE USER active_user
    PASSWORD 'active123'
    FIRSTNAME 'Active'
    LASTNAME 'User'
    ACTIVE;

-- Create initially inactive user
CREATE USER temp_user
    PASSWORD 'temp456'
    FIRSTNAME 'Temporary'
    LASTNAME 'User'
    INACTIVE;

-- Create seasonal user (inactive initially)
CREATE USER seasonal_worker
    PASSWORD 'seasonal789'
    FIRSTNAME 'Seasonal'
    LASTNAME 'Worker'
    INACTIVE
    HOME SCHEMA temporary;
```

### **Conditional User Creation**

#### **IF NOT EXISTS Usage**
```sql
-- Safe user creation for deployment scripts
CREATE USER IF NOT EXISTS default_user
    PASSWORD 'default123'
    HOME SCHEMA public;

-- Safe application user creation
CREATE USER IF NOT EXISTS app_user
    PASSWORD 'application_pass'
    FIRSTNAME 'Application'
    LASTNAME 'User'
    HOME SCHEMA application
    ACTIVE;

-- Safe admin user creation
CREATE USER IF NOT EXISTS system_admin
    PASSWORD 'sys_admin_pass'
    FIRSTNAME 'System'
    LASTNAME 'Administrator'
    GRANT ADMIN ROLE
    HOME SCHEMA administration;
```

---

## ALTER USER

Modifies existing user account properties including passwords, names, roles, and schema assignments.

### **ALTER USER Syntax**
```sql
ALTER USER username [SET]
    [PASSWORD 'new_password']
    [FIRSTNAME 'new_first_name']
    [MIDDLENAME 'new_middle_name']
    [LASTNAME 'new_last_name']
    [GRANT ADMIN ROLE | REVOKE ADMIN ROLE]
    [ACTIVE | INACTIVE]
    [HOME SCHEMA new_schema_name];
```

### **ALTER USER Examples**

#### **Password Management**
```sql
-- Change user password
ALTER USER john_doe PASSWORD 'new_secure_password';

-- Update password with SET keyword
ALTER USER jane_smith SET PASSWORD 'updated_password123';

-- Change admin password
ALTER USER db_admin SET PASSWORD 'new_admin_secret';
```

#### **Personal Information Updates**
```sql
-- Update first name
ALTER USER john_doe FIRSTNAME 'Jonathan';

-- Update full name information
ALTER USER jane_smith 
    FIRSTNAME 'Jane Elizabeth'
    MIDDLENAME 'Marie'
    LASTNAME 'Smith-Johnson';

-- Update multiple name fields
ALTER USER robert_johnson SET
    FIRSTNAME 'Robert'
    MIDDLENAME 'Michael'
    LASTNAME 'Johnson-Williams';
```

#### **Administrative Role Management**
```sql
-- Grant admin role to existing user
ALTER USER finance_user GRANT ADMIN ROLE;

-- Revoke admin role from user
ALTER USER former_admin REVOKE ADMIN ROLE;

-- Grant admin role with other updates
ALTER USER team_lead SET
    GRANT ADMIN ROLE
    HOME SCHEMA management;
```

#### **Account Status Changes**
```sql
-- Activate user account
ALTER USER temp_user ACTIVE;

-- Deactivate user account
ALTER USER terminated_user INACTIVE;

-- Reactivate seasonal user with updates
ALTER USER seasonal_worker SET
    ACTIVE
    PASSWORD 'new_season_pass'
    HOME SCHEMA current_projects;
```

#### **Home Schema Updates**
```sql
-- Change user's home schema
ALTER USER finance_user HOME SCHEMA enterprise.finance.newdept;

-- Update home schema to hierarchical structure
ALTER USER regional_user SET HOME SCHEMA company.regions.europe.sales;

-- Move user to different schema hierarchy
ALTER USER project_manager SET
    HOME SCHEMA enterprise.projects.development
    ACTIVE;
```

#### **Comprehensive User Updates**
```sql
-- Complete user profile update
ALTER USER john_doe SET
    PASSWORD 'updated_secure_pass'
    FIRSTNAME 'John'
    MIDDLENAME 'David'
    LASTNAME 'Doe-Wilson'
    GRANT ADMIN ROLE
    HOME SCHEMA enterprise.administration;

-- Update service account properties
ALTER USER app_service SET
    PASSWORD 'new_service_secret'
    ACTIVE
    HOME SCHEMA application.production;

-- Update user with role and schema changes
ALTER USER business_analyst SET
    FIRSTNAME 'Senior Business'
    LASTNAME 'Analyst'
    HOME SCHEMA enterprise.analysis.advanced
    GRANT ADMIN ROLE;
```

---

## ALTER CURRENT USER

Special syntax for users to modify their own account properties without requiring administrative privileges.

### **ALTER CURRENT USER Syntax**
```sql
ALTER CURRENT USER [SET]
    [PASSWORD 'new_password']
    [FIRSTNAME 'new_first_name']
    [MIDDLENAME 'new_middle_name']
    [LASTNAME 'new_last_name'];
```

### **ALTER CURRENT USER Examples**

#### **Self-Service Password Changes**
```sql
-- Change own password
ALTER CURRENT USER PASSWORD 'my_new_password';

-- Update password with SET
ALTER CURRENT USER SET PASSWORD 'secure_new_pass';
```

#### **Personal Information Self-Management**
```sql
-- Update own first name
ALTER CURRENT USER FIRSTNAME 'NewFirstName';

-- Update complete name information
ALTER CURRENT USER SET
    FIRSTNAME 'Updated'
    MIDDLENAME 'Middle'
    LASTNAME 'NewLastName';

-- Update name and password together
ALTER CURRENT USER SET
    PASSWORD 'new_personal_pass'
    FIRSTNAME 'John'
    LASTNAME 'Smith-Updated';
```

---

## DROP USER

Removes user accounts from the database. Users can only be dropped if they don't own any database objects or have active sessions.

### **DROP USER Syntax**
```sql
-- Standard user removal
DROP USER [IF EXISTS] username;

-- User removal with plugin specification
DROP USER [IF EXISTS] username USING PLUGIN plugin_name;
```

### **DROP USER Examples**

#### **Basic User Removal**
```sql
-- Drop specific user
DROP USER john_doe;

-- Safe user removal
DROP USER IF EXISTS temp_user;

-- Drop inactive user
DROP USER IF EXISTS terminated_employee;
```

#### **Plugin-Specific User Removal**
```sql
-- Drop user from specific plugin
DROP USER external_user USING PLUGIN Srp256;

-- Drop LDAP user
DROP USER ldap_employee USING PLUGIN CustomLDAP;

-- Safe plugin user removal
DROP USER IF EXISTS oauth_user USING PLUGIN OAuth2Plugin;
```

#### **Administrative User Cleanup**
```sql
-- Drop temporary admin user
DROP USER IF EXISTS temp_admin;

-- Remove test users
DROP USER IF EXISTS test_user1;
DROP USER IF EXISTS test_user2;
DROP USER IF EXISTS demo_user;

-- Clean up service accounts
DROP USER IF EXISTS old_service_user;
```

#### **Dependency Checking Before Drop**
```sql
-- Check for user-owned objects before dropping
SELECT 
    RDB$RELATION_NAME as OBJECT_NAME,
    'TABLE' as OBJECT_TYPE
FROM RDB$RELATIONS
WHERE RDB$OWNER_NAME = 'USERNAME_TO_DROP'

UNION ALL

SELECT 
    RDB$PROCEDURE_NAME as OBJECT_NAME,
    'PROCEDURE' as OBJECT_TYPE
FROM RDB$PROCEDURES
WHERE RDB$OWNER_NAME = 'USERNAME_TO_DROP'

UNION ALL

SELECT 
    RDB$FUNCTION_NAME as OBJECT_NAME,
    'FUNCTION' as OBJECT_TYPE
FROM RDB$FUNCTIONS
WHERE RDB$OWNER_NAME = 'USERNAME_TO_DROP';

-- Drop user after confirming no dependencies
DROP USER username_to_drop;
```

---

## RECREATE USER

Combines DROP and CREATE operations in a single atomic transaction.

### **RECREATE USER Syntax**
```sql
RECREATE USER username
    [PASSWORD 'password']
    [FIRSTNAME 'first_name']
    [MIDDLENAME 'middle_name']
    [LASTNAME 'last_name']
    [GRANT ADMIN ROLE | REVOKE ADMIN ROLE]
    [ACTIVE | INACTIVE]
    [HOME SCHEMA schema_name]
    [USING PLUGIN plugin_name];
```

### **RECREATE USER Examples**

#### **User Account Recreation**
```sql
-- Recreate user with new properties
RECREATE USER john_doe
    PASSWORD 'completely_new_pass'
    FIRSTNAME 'John'
    MIDDLENAME 'Robert'
    LASTNAME 'Doe'
    HOME SCHEMA users;

-- Recreate admin user
RECREATE USER system_admin
    PASSWORD 'new_admin_secret'
    FIRSTNAME 'System'
    LASTNAME 'Administrator'
    GRANT ADMIN ROLE
    HOME SCHEMA administration
    ACTIVE;

-- Recreate service user with plugin
RECREATE USER app_service
    PASSWORD 'service_password'
    FIRSTNAME 'Application'
    LASTNAME 'Service'
    HOME SCHEMA application
    USING PLUGIN Srp256;
```

---

## User Session and Schema Management

### **Home Schema Functionality**

#### **Setting User Home Schemas**
```sql
-- Set home schema for specific user (admin operation)
SET HOME SCHEMA FOR USER finance_user TO enterprise.finance;

-- Set home schema with hierarchical path
SET HOME SCHEMA FOR USER analyst TO company.analysis.financial;

-- Set home schema for current session
SET HOME SCHEMA enterprise.projects;
```

#### **Home Schema in User Sessions**
```sql
-- When user connects, automatic schema context
-- User: finance_user with HOME SCHEMA enterprise.finance
-- Tables can be referenced without schema qualification:
SELECT * FROM accounts;  -- Resolves to enterprise.finance.accounts

-- Explicit schema override still works:
SELECT * FROM enterprise.hr.employees;  -- Different schema access
```

### **Current User Context**

#### **User Information Queries**
```sql
-- Get current user information
SELECT USER FROM RDB$DATABASE;

-- Get current connection user details
SELECT 
    SEC$USER_NAME,
    SEC$FIRST_NAME,
    SEC$LAST_NAME,
    SEC$ACTIVE,
    SEC$ADMIN
FROM SEC$USERS 
WHERE SEC$USER_NAME = USER;

-- Get current user's home schema
SELECT HOME_SCHEMA FROM SEC$USERS WHERE SEC$USER_NAME = USER;
```

---

## System Catalog Integration

ScratchBird stores user information in the SEC$USERS virtual system table.

### **Querying User Information**

#### **List All Users**
```sql
-- Show all database users
SELECT 
    SEC$USER_NAME as USERNAME,
    SEC$FIRST_NAME as FIRST_NAME,
    SEC$MIDDLE_NAME as MIDDLE_NAME,
    SEC$LAST_NAME as LAST_NAME,
    CASE SEC$ACTIVE
        WHEN TRUE THEN 'ACTIVE'
        WHEN FALSE THEN 'INACTIVE'
        ELSE 'UNKNOWN'
    END as STATUS,
    CASE SEC$ADMIN
        WHEN TRUE THEN 'YES'
        WHEN FALSE THEN 'NO'
        ELSE 'UNKNOWN'
    END as ADMIN_ROLE,
    SEC$PLUGIN as PLUGIN,
    SEC$HOME_SCHEMA as HOME_SCHEMA,
    SEC$COMMENT as COMMENTS
FROM SEC$USERS
ORDER BY USERNAME;
```

#### **Active Users Query**
```sql
-- Show only active users
SELECT 
    SEC$USER_NAME as USERNAME,
    SEC$FIRST_NAME || 
    CASE WHEN SEC$MIDDLE_NAME IS NOT NULL THEN ' ' || SEC$MIDDLE_NAME ELSE '' END ||
    CASE WHEN SEC$LAST_NAME IS NOT NULL THEN ' ' || SEC$LAST_NAME ELSE '' END as FULL_NAME,
    SEC$HOME_SCHEMA as HOME_SCHEMA,
    SEC$PLUGIN as AUTH_PLUGIN
FROM SEC$USERS
WHERE SEC$ACTIVE = TRUE
ORDER BY FULL_NAME;
```

#### **Administrative Users Query**
```sql
-- Show users with admin privileges
SELECT 
    SEC$USER_NAME as ADMIN_USER,
    SEC$FIRST_NAME || ' ' || SEC$LAST_NAME as FULL_NAME,
    CASE SEC$ACTIVE
        WHEN TRUE THEN 'ACTIVE'
        ELSE 'INACTIVE'
    END as STATUS,
    SEC$HOME_SCHEMA as HOME_SCHEMA,
    SEC$COMMENT as NOTES
FROM SEC$USERS
WHERE SEC$ADMIN = TRUE
ORDER BY STATUS DESC, ADMIN_USER;
```

### **User Authentication Analysis**

#### **Users by Authentication Plugin**
```sql
-- Group users by authentication method
SELECT 
    COALESCE(SEC$PLUGIN, 'DEFAULT') as AUTH_PLUGIN,
    COUNT(*) as USER_COUNT,
    COUNT(CASE WHEN SEC$ACTIVE = TRUE THEN 1 END) as ACTIVE_COUNT,
    COUNT(CASE WHEN SEC$ADMIN = TRUE THEN 1 END) as ADMIN_COUNT
FROM SEC$USERS
GROUP BY SEC$PLUGIN
ORDER BY USER_COUNT DESC;
```

#### **Users by Home Schema**
```sql
-- Analyze users by home schema assignment
SELECT 
    COALESCE(SEC$HOME_SCHEMA, 'NO_SCHEMA') as HOME_SCHEMA,
    COUNT(*) as USER_COUNT,
    STRING_AGG(SEC$USER_NAME, ', ' ORDER BY SEC$USER_NAME) as USERS
FROM SEC$USERS
WHERE SEC$ACTIVE = TRUE
GROUP BY SEC$HOME_SCHEMA
ORDER BY USER_COUNT DESC;
```

### **User Activity and Security Analysis**

#### **Incomplete User Profiles**
```sql
-- Find users with incomplete profile information
SELECT 
    SEC$USER_NAME as USERNAME,
    CASE 
        WHEN SEC$FIRST_NAME IS NULL THEN 'Missing First Name; '
        ELSE ''
    END ||
    CASE 
        WHEN SEC$LAST_NAME IS NULL THEN 'Missing Last Name; '
        ELSE ''
    END ||
    CASE 
        WHEN SEC$HOME_SCHEMA IS NULL THEN 'No Home Schema; '
        ELSE ''
    END as MISSING_INFO
FROM SEC$USERS
WHERE SEC$FIRST_NAME IS NULL 
   OR SEC$LAST_NAME IS NULL 
   OR SEC$HOME_SCHEMA IS NULL
ORDER BY USERNAME;
```

#### **User Security Audit**
```sql
-- Security audit of user accounts
SELECT 
    SEC$USER_NAME as USERNAME,
    CASE SEC$ACTIVE
        WHEN TRUE THEN 'ACTIVE'
        ELSE 'INACTIVE'  
    END as STATUS,
    CASE SEC$ADMIN
        WHEN TRUE THEN 'ADMIN'
        ELSE 'REGULAR'
    END as PRIVILEGE_LEVEL,
    COALESCE(SEC$PLUGIN, 'INTERNAL') as AUTH_METHOD,
    SEC$HOME_SCHEMA as DEFAULT_SCHEMA,
    CASE 
        WHEN SEC$COMMENT IS NOT NULL THEN 'DOCUMENTED'
        ELSE 'NO_COMMENTS'
    END as DOCUMENTATION_STATUS
FROM SEC$USERS
ORDER BY PRIVILEGE_LEVEL DESC, STATUS, USERNAME;
```

---

## User Comments and Documentation

### **Adding Comments to Users**

#### **COMMENT ON USER Syntax**
```sql
COMMENT ON USER username IS 'comment_text';
```

#### **User Comment Examples**
```sql
-- Add comment to user account
COMMENT ON USER john_doe IS 'Finance department analyst, hired January 2025';

-- Document admin user
COMMENT ON USER db_admin IS 'Primary database administrator, on-call rotation';

-- Comment on service account
COMMENT ON USER app_service IS 'Application service account for production system';

-- Update user documentation
COMMENT ON USER finance_manager IS 'Finance department manager, reports approval authority';
```

### **Viewing User Comments**
```sql
-- Show users with their comments
SELECT 
    SEC$USER_NAME as USERNAME,
    SEC$FIRST_NAME || ' ' || SEC$LAST_NAME as FULL_NAME,
    SEC$COMMENT as DESCRIPTION
FROM SEC$USERS
WHERE SEC$COMMENT IS NOT NULL
ORDER BY USERNAME;
```

---

## User Permissions and Security

### **User Privilege Management**

#### **Database Object Ownership**
```sql
-- Find objects owned by specific user
SELECT 
    'TABLE' as OBJECT_TYPE,
    RDB$RELATION_NAME as OBJECT_NAME,
    RDB$OWNER_NAME as OWNER
FROM RDB$RELATIONS
WHERE RDB$OWNER_NAME = 'JOHN_DOE'

UNION ALL

SELECT 
    'PROCEDURE' as OBJECT_TYPE,
    RDB$PROCEDURE_NAME as OBJECT_NAME,
    RDB$OWNER_NAME as OWNER
FROM RDB$PROCEDURES  
WHERE RDB$OWNER_NAME = 'JOHN_DOE'

UNION ALL

SELECT 
    'FUNCTION' as OBJECT_TYPE,
    RDB$FUNCTION_NAME as OBJECT_NAME,
    RDB$OWNER_NAME as OWNER
FROM RDB$FUNCTIONS
WHERE RDB$OWNER_NAME = 'JOHN_DOE'

ORDER BY OBJECT_TYPE, OBJECT_NAME;
```

#### **User Permission Analysis**
```sql
-- Analyze user permissions across database objects
SELECT 
    p.RDB$USER as USERNAME,
    p.RDB$RELATION_NAME as OBJECT_NAME,
    p.RDB$PRIVILEGE as PERMISSION,
    CASE p.RDB$GRANT_OPTION
        WHEN 1 THEN 'YES'
        ELSE 'NO'
    END as CAN_GRANT,
    p.RDB$GRANTOR as GRANTED_BY
FROM RDB$USER_PRIVILEGES p
JOIN SEC$USERS u ON p.RDB$USER = u.SEC$USER_NAME
WHERE u.SEC$ACTIVE = TRUE
  AND p.RDB$OBJECT_TYPE = 0  -- Tables
ORDER BY USERNAME, OBJECT_NAME, PERMISSION;
```

### **Role Assignment to Users**

#### **User Role Analysis**
```sql
-- Show roles assigned to users
SELECT 
    p.RDB$USER as USERNAME,
    p.RDB$RELATION_NAME as ROLE_NAME,
    p.RDB$GRANTOR as GRANTED_BY,
    CASE p.RDB$GRANT_OPTION
        WHEN 1 THEN 'WITH ADMIN OPTION'
        ELSE 'REGULAR'
    END as GRANT_TYPE
FROM RDB$USER_PRIVILEGES p
JOIN SEC$USERS u ON p.RDB$USER = u.SEC$USER_NAME
WHERE p.RDB$OBJECT_TYPE = 13  -- Roles
  AND u.SEC$ACTIVE = TRUE
ORDER BY USERNAME, ROLE_NAME;
```

---

## Advanced User Management

### **User Account Automation**

#### **Bulk User Creation Scripts**
```sql
-- Create multiple users with consistent properties
EXECUTE BLOCK AS
DECLARE username VARCHAR(63);
DECLARE counter INTEGER = 1;
BEGIN
    WHILE (counter <= 10) DO
    BEGIN
        username = 'user_' || LPAD(counter, 3, '0');
        
        EXECUTE STATEMENT 'CREATE USER ' || username || 
                         ' PASSWORD ''temp_pass_' || counter || ''' ' ||
                         'FIRSTNAME ''User'' ' ||
                         'LASTNAME ''Number ' || counter || ''' ' ||
                         'HOME SCHEMA users ' ||
                         'ACTIVE';
        
        counter = counter + 1;
    END
END;
```

#### **User Status Management Scripts**
```sql
-- Bulk activate users
EXECUTE BLOCK AS
DECLARE username VARCHAR(63);
BEGIN
    FOR SELECT SEC$USER_NAME FROM SEC$USERS 
        WHERE SEC$ACTIVE = FALSE AND SEC$USER_NAME LIKE 'temp_%'
        INTO :username
    DO
    BEGIN
        EXECUTE STATEMENT 'ALTER USER ' || username || ' ACTIVE';
    END
END;

-- Bulk deactivate inactive users
EXECUTE BLOCK AS
DECLARE username VARCHAR(63);
BEGIN
    FOR SELECT SEC$USER_NAME FROM SEC$USERS 
        WHERE SEC$COMMENT LIKE '%terminated%'
        INTO :username
    DO
    BEGIN
        EXECUTE STATEMENT 'ALTER USER ' || username || ' INACTIVE';
    END
END;
```

### **User Migration and Maintenance**

#### **User Profile Migration**
```sql
-- Migrate users to new schema structure
EXECUTE BLOCK AS
DECLARE username VARCHAR(63);
DECLARE old_schema VARCHAR(63);
DECLARE new_schema VARCHAR(63);
BEGIN
    FOR SELECT SEC$USER_NAME, SEC$HOME_SCHEMA FROM SEC$USERS 
        WHERE SEC$HOME_SCHEMA LIKE 'old_structure%'
        INTO :username, :old_schema
    DO
    BEGIN
        -- Map old schema to new structure
        new_schema = REPLACE(old_schema, 'old_structure', 'enterprise.new_structure');
        
        EXECUTE STATEMENT 'ALTER USER ' || username || 
                         ' HOME SCHEMA ' || new_schema;
    END
END;
```

#### **User Cleanup and Maintenance**
```sql
-- Clean up inactive users older than specified period
CREATE PROCEDURE cleanup_inactive_users(
    inactive_days INTEGER DEFAULT 90
)
AS
DECLARE username VARCHAR(63);
DECLARE last_activity TIMESTAMP;
BEGIN
    -- This would require additional audit table for last activity
    -- For demonstration, we'll use creation-based cleanup
    
    FOR SELECT SEC$USER_NAME FROM SEC$USERS 
        WHERE SEC$ACTIVE = FALSE 
          AND SEC$COMMENT LIKE '%cleanup_eligible%'
        INTO :username
    DO
    BEGIN
        -- Log the cleanup action
        INSERT INTO user_cleanup_log (username, cleanup_date, reason)
        VALUES (:username, CURRENT_TIMESTAMP, 'Inactive user cleanup');
        
        -- Drop the user
        EXECUTE STATEMENT 'DROP USER IF EXISTS ' || username;
    END
END;
```

---

## Error Handling and Troubleshooting

### **Common User Management Errors**

#### **User Creation Errors**
```sql
-- Error: User already exists
CREATE USER existing_user PASSWORD 'pass';
-- Solution: Use IF NOT EXISTS or ALTER USER

-- Error: Invalid username characters
CREATE USER 'user-with-hyphens' PASSWORD 'pass';
-- Solution: Use valid SQL identifiers (letters, numbers, underscores)

-- Error: Password too simple (if policy enforced)
CREATE USER new_user PASSWORD '123';
-- Solution: Use complex password meeting policy requirements
```

#### **User Modification Errors**
```sql
-- Error: User does not exist
ALTER USER nonexistent_user PASSWORD 'newpass';
-- Solution: Create user first or verify username

-- Error: Cannot modify plugin user properties
ALTER USER external_user PASSWORD 'localpass';
-- Solution: Manage password through external plugin system

-- Error: Invalid schema name
ALTER USER test_user HOME SCHEMA nonexistent.schema;
-- Solution: Create schema first or verify schema name
```

#### **User Deletion Errors**
```sql
-- Error: User owns database objects
DROP USER object_owner;
-- Solution: Transfer ownership or drop objects first

-- Error: User has active sessions
DROP USER active_user;
-- Solution: Terminate user sessions first

-- Error: Plugin user requires plugin specification
DROP USER plugin_user;
-- Solution: Use USING PLUGIN clause with correct plugin name
```

### **User Troubleshooting Queries**

#### **User Account Diagnostics**
```sql
-- Check user account status and configuration
SELECT 
    SEC$USER_NAME as USERNAME,
    SEC$ACTIVE as IS_ACTIVE,
    SEC$ADMIN as IS_ADMIN,
    SEC$PLUGIN as AUTH_PLUGIN,
    SEC$HOME_SCHEMA as HOME_SCHEMA,
    CASE 
        WHEN SEC$FIRST_NAME IS NULL AND SEC$LAST_NAME IS NULL THEN 'NO_NAME_INFO'
        WHEN SEC$FIRST_NAME IS NULL THEN 'MISSING_FIRST_NAME'
        WHEN SEC$LAST_NAME IS NULL THEN 'MISSING_LAST_NAME'
        ELSE 'COMPLETE_NAME'
    END as NAME_STATUS
FROM SEC$USERS
WHERE SEC$USER_NAME = 'PROBLEM_USER';
```

#### **User Permission Diagnostics**
```sql
-- Diagnose user permission issues
SELECT 
    'OWNED_OBJECTS' as CATEGORY,
    COUNT(*) as COUNT
FROM RDB$RELATIONS
WHERE RDB$OWNER_NAME = 'PROBLEM_USER'

UNION ALL

SELECT 
    'GRANTED_PERMISSIONS' as CATEGORY,
    COUNT(*) as COUNT
FROM RDB$USER_PRIVILEGES  
WHERE RDB$USER = 'PROBLEM_USER'

UNION ALL

SELECT 
    'ROLE_MEMBERSHIPS' as CATEGORY,
    COUNT(*) as COUNT
FROM RDB$USER_PRIVILEGES
WHERE RDB$USER = 'PROBLEM_USER'
  AND RDB$OBJECT_TYPE = 13;
```

### **User Recovery Procedures**

#### **Password Reset Recovery**
```sql
-- Admin password reset for user
ALTER USER locked_out_user PASSWORD 'temporary_password123';

-- Force user to change password on next login (via application logic)
COMMENT ON USER locked_out_user IS 'Password reset required - expires ' || CURRENT_DATE;
```

#### **Account Recovery Scripts**
```sql
-- Recover accidentally deactivated user
ALTER USER recovered_user SET
    ACTIVE
    PASSWORD 'recovery_password'
    COMMENT 'Account recovered on ' || CURRENT_DATE;

-- Restore admin privileges to user
ALTER USER former_admin SET
    GRANT ADMIN ROLE
    ACTIVE
    COMMENT 'Admin privileges restored';
```

---

## Best Practices

### **User Management Guidelines**

1. **Secure Passwords**: Enforce strong password policies through application logic
2. **Regular Audits**: Periodically review user accounts and permissions
3. **Principle of Least Privilege**: Grant minimum necessary permissions
4. **Documentation**: Use comments to document user purposes and roles
5. **Home Schema Assignment**: Assign appropriate default schemas for user context
6. **Account Lifecycle**: Implement processes for user creation, modification, and removal

### **Recommended User Management Patterns**

#### **User Naming Conventions**
```sql
-- Service accounts
CREATE USER svc_application PASSWORD 'service_pass' HOME SCHEMA application;
CREATE USER svc_backup PASSWORD 'backup_pass' HOME SCHEMA maintenance;
CREATE USER svc_reporting PASSWORD 'report_pass' HOME SCHEMA reports;

-- Human users with consistent naming
CREATE USER john_doe PASSWORD 'user_pass' FIRSTNAME 'John' LASTNAME 'Doe';
CREATE USER jane_smith PASSWORD 'user_pass' FIRSTNAME 'Jane' LASTNAME 'Smith';

-- Administrative accounts
CREATE USER admin_db PASSWORD 'admin_pass' GRANT ADMIN ROLE;
CREATE USER admin_security PASSWORD 'security_pass' GRANT ADMIN ROLE;
```

#### **Schema-Based User Organization**
```sql
-- Department-based users
CREATE USER finance_analyst PASSWORD 'fin_pass' HOME SCHEMA enterprise.finance;
CREATE USER hr_coordinator PASSWORD 'hr_pass' HOME SCHEMA enterprise.hr;
CREATE USER sales_manager PASSWORD 'sales_pass' HOME SCHEMA enterprise.sales;

-- Project-based users  
CREATE USER proj_alpha_lead PASSWORD 'proj_pass' HOME SCHEMA projects.alpha;
CREATE USER proj_beta_dev PASSWORD 'dev_pass' HOME SCHEMA projects.beta.development;

-- Environment-based users
CREATE USER prod_operator PASSWORD 'prod_pass' HOME SCHEMA production;
CREATE USER test_user PASSWORD 'test_pass' HOME SCHEMA testing;
CREATE USER dev_user PASSWORD 'dev_pass' HOME SCHEMA development;
```

#### **User Security Templates**
```sql
-- Standard user template
CREATE PROCEDURE create_standard_user(
    username VARCHAR(63),
    first_name VARCHAR(32),
    last_name VARCHAR(32), 
    department_schema VARCHAR(63),
    initial_password VARCHAR(100)
)
AS
BEGIN
    EXECUTE STATEMENT 'CREATE USER ' || username ||
                     ' PASSWORD ''' || initial_password || ''' ' ||
                     'FIRSTNAME ''' || first_name || ''' ' ||
                     'LASTNAME ''' || last_name || ''' ' ||
                     'HOME SCHEMA ' || department_schema || ' ' ||
                     'ACTIVE';
                     
    -- Add standard documentation
    EXECUTE STATEMENT 'COMMENT ON USER ' || username || 
                     ' IS ''Standard user created ' || CURRENT_DATE || 
                     ', Department: ' || department_schema || '''';
END;

-- Admin user template
CREATE PROCEDURE create_admin_user(
    username VARCHAR(63),
    first_name VARCHAR(32),
    last_name VARCHAR(32),
    admin_password VARCHAR(100)
)
AS
BEGIN
    EXECUTE STATEMENT 'CREATE USER ' || username ||
                     ' PASSWORD ''' || admin_password || ''' ' ||
                     'FIRSTNAME ''' || first_name || ''' ' ||
                     'LASTNAME ''' || last_name || ''' ' ||
                     'GRANT ADMIN ROLE ' ||
                     'HOME SCHEMA administration ' ||
                     'ACTIVE';
                     
    EXECUTE STATEMENT 'COMMENT ON USER ' || username || 
                     ' IS ''Administrator account created ' || CURRENT_DATE || '''';
END;
```

---

## Implementation Details

### **Primary Implementation Files**

#### **Parser and Grammar**
- **File**: `src/dsql/parse.y:8335-8410`
- **Grammar Rules**:
  - `create_user_clause` (lines 8335-8345): CREATE USER syntax
  - `alter_user_clause` (lines 8347-8357): ALTER USER syntax
  - `alter_cur_user_clause` (lines 8359-8369): ALTER CURRENT USER syntax
  - `user_fixed_list` (lines 8394-8410): User property specifications
- **Functionality**: Parsing CREATE/ALTER/DROP USER with all properties

#### **DDL Node Classes**
- **File**: `src/dsql/DdlNodes.h:2435-2559`
- **Classes**:
  - `CreateAlterUserNode` (lines 2435-2508): User creation/modification
  - `DropUserNode` (lines 2511-2545): User removal with plugin support
  - `RecreateUserNode` (lines 2549-2559): Atomic recreation operations

#### **System Catalog Integration**
- **File**: `src/jrd/relations.h:708-719`
- **Table**: `SEC$USERS` - Virtual table for user information
- **Fields**:
  - `f_sec_user_name`: Username
  - `f_sec_first_name`, `f_sec_middle_name`, `f_sec_last_name`: Personal names
  - `f_sec_active`: Account status (active/inactive)
  - `f_sec_admin`: Administrative privileges flag
  - `f_sec_comment`: User description/comments
  - `f_sec_plugin`: Authentication plugin name
  - `f_sec_home_schema`: Default schema assignment

#### **User Authentication Integration**
- **File**: `src/dsql/parse.y:4605-4607`
- **Functionality**: DDL trigger support for user operations (CREATE/ALTER/DROP)
- Authentication plugin system integration
- Password encryption and validation

### **Core Classes and Functions**

#### **CreateAlterUserNode Methods**
- `dsqlPass()`: DSQL compilation and user validation
- `execute()`: User creation/modification in security database
- `checkPermission()`: Security validation for user operations
- User property management (names, password, admin role, home schema)

#### **User Security Integration**
- Plugin-based authentication system
- Password encryption and storage
- Admin role privilege management
- Home schema resolution and validation

### **Storage Structures**

Users are stored in the SEC$USERS virtual system table with:

- **Identity**: Username (case-insensitive, unique)
- **Personal Information**: First, middle, and last names
- **Authentication**: Password hash and plugin information
- **Authorization**: Admin role flag and privilege tracking
- **Context**: Home schema assignment for default database context
- **Status**: Active/inactive account status control
- **Documentation**: Comment field for administrative notes

---

## Administrative Operations

### **User Backup and Restore**

#### **Backup Considerations**
- User definitions are included in database security backups
- Personal information and role assignments are preserved
- Home schema assignments are maintained across backup/restore
- Plugin authentication settings require plugin availability

#### **Restore Procedures**
```sql
-- Verify users after restore
SELECT 
    SEC$USER_NAME,
    SEC$ACTIVE,
    SEC$ADMIN,
    SEC$HOME_SCHEMA,
    SEC$PLUGIN
FROM SEC$USERS
ORDER BY SEC$USER_NAME;

-- Test user authentication (application-level verification required)
-- Verify home schema functionality
SET HOME SCHEMA FOR USER test_user TO test_schema;
```

### **User Maintenance**

#### **Regular Maintenance Tasks**
```sql
-- Monitor user account status
CREATE VIEW user_account_summary AS
SELECT 
    COUNT(*) as TOTAL_USERS,
    COUNT(CASE WHEN SEC$ACTIVE = TRUE THEN 1 END) as ACTIVE_USERS,
    COUNT(CASE WHEN SEC$ADMIN = TRUE THEN 1 END) as ADMIN_USERS,
    COUNT(CASE WHEN SEC$PLUGIN IS NOT NULL THEN 1 END) as PLUGIN_USERS,
    COUNT(CASE WHEN SEC$HOME_SCHEMA IS NOT NULL THEN 1 END) as SCHEMA_ASSIGNED_USERS
FROM SEC$USERS;

-- Check for users without complete information
SELECT COUNT(*) as INCOMPLETE_PROFILES
FROM SEC$USERS
WHERE SEC$FIRST_NAME IS NULL OR SEC$LAST_NAME IS NULL;
```

---

## Migration from Other Database Systems

### **From PostgreSQL Users**
PostgreSQL user/role system translates to ScratchBird users:
```sql
-- PostgreSQL
CREATE USER john_doe WITH PASSWORD 'mypassword';

-- ScratchBird equivalent
CREATE USER john_doe PASSWORD 'mypassword';
```

### **From Oracle Users**
Oracle user accounts can be migrated to ScratchBird:
```sql
-- Oracle
CREATE USER finance_user IDENTIFIED BY password 
DEFAULT TABLESPACE finance_data;

-- ScratchBird approach
CREATE USER finance_user PASSWORD 'password' 
HOME SCHEMA finance;
```

### **From SQL Server Logins**
SQL Server logins translate to ScratchBird users:
```sql
-- SQL Server
CREATE LOGIN john_doe WITH PASSWORD = 'mypassword';

-- ScratchBird approach  
CREATE USER john_doe PASSWORD 'mypassword';
```

---

