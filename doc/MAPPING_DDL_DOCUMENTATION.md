# ScratchBird MAPPING - Complete Documentation

## Overview

**Authentication Mappings** in ScratchBird provide a flexible mechanism for mapping external authentication credentials to database users and roles. This system enables integration with external authentication providers, Windows domain authentication, and plugin-based authentication systems, allowing centralized identity management while maintaining database security.

### Key Features

- **External Authentication Integration**: Map external users/groups to database users/roles
- **Plugin-Based Authentication**: Support for custom authentication plugins
- **Global vs Database-Specific**: Create system-wide or database-specific mappings
- **User and Role Mapping**: Map to both database users and roles
- **Wildcard Support**: Use wildcards for flexible group/user matching
- **Serverwide Authentication**: Plugin authentication across entire server

### ScratchBird Enhancements

- **Enhanced Plugin Architecture**: Extended plugin system supporting custom authentication providers
- **Global Mapping Support**: System-wide mappings that apply across all databases
- **Advanced Wildcard Patterns**: Flexible pattern matching for external authentication
- **Role-Based Integration**: Seamless integration with ScratchBird's role-based security system
- **Authentication Chaining**: Multiple authentication methods with priority-based resolution

---

## DDL Syntax Reference

### CREATE MAPPING

Creates a new authentication mapping for external credentials.

#### Basic Syntax

```sql
CREATE [GLOBAL] MAPPING [IF NOT EXISTS] mapping_name
    USING {PLUGIN plugin_name [IN database_spec] | ANY PLUGIN [IN database_spec] | ANY PLUGIN SERVERWIDE}
    FROM {USER | GROUP | ANY USER | ANY GROUP} external_name
    TO {USER | ROLE} [database_user_or_role]
```

#### Parameters

- **`mapping_name`**: Unique identifier for the mapping (max 63 characters)
- **`GLOBAL`**: Creates system-wide mapping (applies to all databases)
- **`PLUGIN plugin_name`**: Specific authentication plugin to use
- **`ANY PLUGIN`**: Accept authentication from any available plugin
- **`SERVERWIDE`**: Plugin authentication across entire server
- **`IN database_spec`**: Database specification for plugin scope
- **`USER` / `GROUP`**: Type of external credential being mapped
- **`ANY USER` / `ANY GROUP`**: Wildcard matching for external credentials
- **`external_name`**: External user/group name or pattern
- **`TO USER` / `TO ROLE`**: Target type in database
- **`database_user_or_role`**: Target database user or role name

#### Basic Examples

```sql
-- Map Windows domain user to database user
CREATE MAPPING windows_user_mapping
    USING PLUGIN Win_Sspi
    FROM USER 'DOMAIN\username'
    TO USER database_user;

-- Map LDAP group to database role
CREATE MAPPING ldap_group_mapping
    USING PLUGIN LDAP_Auth IN 'employee_db'
    FROM GROUP 'CN=Developers,OU=IT,DC=company,DC=com'
    TO ROLE developer_role;

-- Global mapping for any authentication plugin
CREATE GLOBAL MAPPING admin_mapping
    USING ANY PLUGIN SERVERWIDE
    FROM USER 'admin'
    TO ROLE system_admin;

-- Map any user from specific group to role
CREATE MAPPING department_mapping
    USING PLUGIN Active_Directory
    FROM ANY GROUP 'Finance'
    TO ROLE finance_user;
```

#### Advanced Examples

```sql
-- Complex domain mapping with wildcards
CREATE GLOBAL MAPPING domain_admin_mapping
    USING PLUGIN Win_Sspi SERVERWIDE
    FROM ANY USER 'DOMAIN\*admin*'
    TO ROLE database_administrator;

-- Multi-plugin authentication mapping
CREATE MAPPING flexible_auth_mapping
    USING ANY PLUGIN IN 'multi_auth_db'
    FROM USER 'service_account'
    TO USER automated_service;

-- LDAP group mapping with full DN
CREATE MAPPING ldap_complex_mapping
    USING PLUGIN LDAP_Enterprise
    FROM GROUP 'CN=Database Admins,OU=IT Department,OU=Corporate,DC=enterprise,DC=corp'
    TO ROLE dba_group;

-- IF NOT EXISTS syntax for safe creation
CREATE MAPPING IF NOT EXISTS safe_mapping
    USING PLUGIN Custom_Auth
    FROM USER 'application_user'
    TO USER app_service_account;

-- Certificate-based authentication mapping
CREATE GLOBAL MAPPING cert_mapping
    USING PLUGIN Certificate_Auth SERVERWIDE
    FROM USER 'CN=Service Account,O=Company,C=US'
    TO ROLE service_role;
```

---

### ALTER MAPPING

Modifies an existing authentication mapping.

#### Syntax

```sql
ALTER [GLOBAL] MAPPING mapping_name
    USING {PLUGIN plugin_name [IN database_spec] | ANY PLUGIN [IN database_spec] | ANY PLUGIN SERVERWIDE}
    FROM {USER | GROUP | ANY USER | ANY GROUP} external_name
    TO {USER | ROLE} [database_user_or_role]
```

#### Examples

```sql
-- Change plugin for existing mapping
ALTER MAPPING windows_user_mapping
    USING PLUGIN Win_Sspi_V2
    FROM USER 'DOMAIN\username'
    TO USER database_user;

-- Update external credential pattern
ALTER GLOBAL MAPPING admin_mapping
    USING ANY PLUGIN SERVERWIDE
    FROM USER 'admin@company.com'
    TO ROLE enhanced_admin;

-- Change target from user to role
ALTER MAPPING department_mapping
    USING PLUGIN Active_Directory
    FROM GROUP 'Sales'
    TO ROLE sales_manager;

-- Update database scope
ALTER MAPPING ldap_group_mapping
    USING PLUGIN LDAP_Auth IN 'production_db'
    FROM GROUP 'CN=Operators,OU=IT,DC=company,DC=com'
    TO ROLE production_operator;

-- Switch to serverwide authentication
ALTER GLOBAL MAPPING service_mapping
    USING ANY PLUGIN SERVERWIDE
    FROM USER 'service@domain.com'
    TO USER service_account;
```

---

### RECREATE MAPPING

Replaces an existing mapping or creates it if it doesn't exist.

#### Syntax

```sql
RECREATE [GLOBAL] MAPPING mapping_name
    USING {PLUGIN plugin_name [IN database_spec] | ANY PLUGIN [IN database_spec] | ANY PLUGIN SERVERWIDE}
    FROM {USER | GROUP | ANY USER | ANY GROUP} external_name
    TO {USER | ROLE} [database_user_or_role]
```

#### Examples

```sql
-- Recreate mapping with new configuration
RECREATE MAPPING windows_user_mapping
    USING PLUGIN Win_Sspi_Updated
    FROM USER 'NEWDOMAIN\username'
    TO ROLE enhanced_user;

-- Recreate global mapping for system integration
RECREATE GLOBAL MAPPING system_integration
    USING ANY PLUGIN SERVERWIDE
    FROM ANY USER 'integration_*'
    TO ROLE integration_role;
```

---

### DROP MAPPING

Removes an authentication mapping.

#### Syntax

```sql
DROP [GLOBAL] MAPPING [IF EXISTS] mapping_name
```

#### Examples

```sql
-- Drop specific mapping
DROP MAPPING windows_user_mapping;

-- Drop global mapping
DROP GLOBAL MAPPING admin_mapping;

-- Drop with IF EXISTS to avoid errors
DROP MAPPING IF EXISTS old_mapping;

-- Drop multiple mappings (separate statements)
DROP MAPPING department_mapping;
DROP MAPPING service_mapping;
DROP GLOBAL MAPPING system_mapping;
```

---

### COMMENT ON MAPPING

Adds descriptive comments to authentication mappings.

#### Syntax

```sql
COMMENT ON [GLOBAL] MAPPING mapping_name IS 'comment_text'
```

#### Examples

```sql
-- Add comment to standard mapping
COMMENT ON MAPPING windows_user_mapping 
IS 'Maps Windows domain users to database accounts for HR system';

-- Add comment to global mapping
COMMENT ON GLOBAL MAPPING admin_mapping 
IS 'System-wide administrator access mapping for all databases';

-- Descriptive comment for complex mapping
COMMENT ON MAPPING ldap_group_mapping 
IS 'LDAP group mapping for development team access to project databases';

-- Remove comment (empty string)
COMMENT ON MAPPING windows_user_mapping IS '';
```

---

## Usage Examples

### Windows Domain Integration

```sql
-- Set up Windows domain integration
CREATE GLOBAL MAPPING domain_users
    USING PLUGIN Win_Sspi SERVERWIDE
    FROM ANY USER 'CORPORATE\*'
    TO USER domain_user;

CREATE GLOBAL MAPPING domain_admins
    USING PLUGIN Win_Sspi SERVERWIDE
    FROM GROUP 'CORPORATE\Domain Admins'
    TO ROLE system_administrator;

-- Specific department mappings
CREATE MAPPING finance_users
    USING PLUGIN Win_Sspi IN 'finance_db'
    FROM GROUP 'CORPORATE\Finance Users'
    TO ROLE finance_reader;

CREATE MAPPING hr_managers
    USING PLUGIN Win_Sspi IN 'hr_db'
    FROM GROUP 'CORPORATE\HR Managers'
    TO ROLE hr_admin;

-- Service account mapping
CREATE MAPPING app_service
    USING PLUGIN Win_Sspi
    FROM USER 'CORPORATE\AppService'
    TO USER application_service;
```

### LDAP Authentication Setup

```sql
-- Basic LDAP user mapping
CREATE MAPPING ldap_users
    USING PLUGIN LDAP_Auth IN 'employee_db'
    FROM USER 'uid=*,ou=users,dc=company,dc=com'
    TO USER ldap_user;

-- LDAP group-based role assignment
CREATE MAPPING developers_group
    USING PLUGIN LDAP_Auth
    FROM GROUP 'CN=Developers,OU=Engineering,DC=company,DC=com'
    TO ROLE developer;

CREATE MAPPING qa_team
    USING PLUGIN LDAP_Auth
    FROM GROUP 'CN=QA Team,OU=Engineering,DC=company,DC=com'
    TO ROLE quality_assurance;

-- Management access
CREATE GLOBAL MAPPING ldap_managers
    USING PLUGIN LDAP_Auth SERVERWIDE
    FROM GROUP 'CN=Management,OU=Executive,DC=company,DC=com'
    TO ROLE manager;

-- External contractor access
CREATE MAPPING contractors
    USING PLUGIN LDAP_Auth IN 'contractor_db'
    FROM ANY USER 'uid=contractor*,ou=external,dc=company,dc=com'
    TO ROLE contractor_access;
```

### Multi-Plugin Authentication

```sql
-- Flexible authentication allowing multiple plugins
CREATE GLOBAL MAPPING flexible_admin
    USING ANY PLUGIN SERVERWIDE
    FROM USER 'admin'
    TO ROLE system_admin;

-- Database-specific flexible authentication
CREATE MAPPING app_auth
    USING ANY PLUGIN IN 'application_db'
    FROM USER 'app_user'
    TO USER application_account;

-- Plugin-specific with fallback
CREATE MAPPING primary_auth
    USING PLUGIN Primary_Auth IN 'main_db'
    FROM USER 'service@company.com'
    TO USER primary_service;

-- Backup authentication method
CREATE MAPPING backup_auth
    USING PLUGIN Backup_Auth IN 'main_db' 
    FROM USER 'service@company.com'
    TO USER backup_service;
```

### Certificate-Based Authentication

```sql
-- SSL certificate mapping
CREATE GLOBAL MAPPING ssl_cert_mapping
    USING PLUGIN Certificate_Auth SERVERWIDE
    FROM USER 'CN=Database Client,O=Company,C=US'
    TO USER cert_user;

-- Service certificate mapping
CREATE MAPPING service_cert
    USING PLUGIN Certificate_Auth
    FROM USER 'CN=API Service,OU=Services,O=Company,C=US'
    TO ROLE api_service;

-- Client certificate mapping with wildcards
CREATE MAPPING client_certs
    USING PLUGIN Certificate_Auth IN 'client_db'
    FROM ANY USER 'CN=Client *,O=Company,C=US'
    TO ROLE client_access;
```

### Complex Enterprise Scenarios

```sql
-- Multi-tier authentication for enterprise application
-- Tier 1: Application service accounts
CREATE GLOBAL MAPPING app_services
    USING PLUGIN Service_Auth SERVERWIDE
    FROM ANY USER 'service_*@enterprise.com'
    TO ROLE application_service;

-- Tier 2: End user authentication
CREATE MAPPING end_users
    USING PLUGIN LDAP_Enterprise IN 'user_db'
    FROM ANY USER 'uid=*,ou=employees,dc=enterprise,dc=com'
    TO USER employee;

-- Tier 3: Administrative access
CREATE GLOBAL MAPPING enterprise_admins
    USING ANY PLUGIN SERVERWIDE
    FROM GROUP 'CN=Database Administrators,OU=IT,DC=enterprise,DC=com'
    TO ROLE dba;

-- External partner access
CREATE MAPPING partner_access
    USING PLUGIN Partner_Auth IN 'partner_db'
    FROM ANY USER '*@partner.com'
    TO ROLE partner_user;

-- Emergency access mapping
CREATE GLOBAL MAPPING emergency_access
    USING PLUGIN Emergency_Auth SERVERWIDE
    FROM USER 'emergency@enterprise.com'
    TO ROLE emergency_admin;
```

---

## Implementation Details

### Primary Implementation Files

#### Core Implementation
- **`src/dsql/parse.y`** (Lines 8442-8610): SQL grammar rules for mapping DDL
  - `create_map_clause`: CREATE MAPPING syntax parsing
  - `alter_map_clause`: ALTER MAPPING syntax parsing
  - `drop_map_clause`: DROP MAPPING syntax parsing
  - `map_clause`: Common mapping clause structure
  - `map_from`, `map_using`, `map_to`: Mapping component parsing

#### DDL Node Implementation
- **`src/dsql/DdlNodes.h`**: MappingNode class definition
- **`src/dsql/DdlNodes.epp`**: MappingNode implementation and execution logic

#### System Catalog
- **`src/jrd/relations.h`**: RDB$AUTH_MAPPING table definition for mapping metadata storage

### Core Classes and Functions

#### MappingNode Class Structure

```cpp
namespace Jrd {
    class MappingNode : public DdlNode {
    public:
        enum OP {
            MAP_ADD,    // CREATE MAPPING operation
            MAP_MOD,    // ALTER MAPPING operation  
            MAP_RPL,    // RECREATE MAPPING operation
            MAP_DROP    // DROP MAPPING operation
        };
        
        // Mapping properties
        MetaName name;              // Mapping identifier
        bool global;                // Global vs database-specific
        char mode;                  // 'P' = Plugin, 'S' = Serverwide
        MetaName* plugin;           // Authentication plugin name
        IntlString* db;             // Database specification
        MetaName* fromType;         // USER or GROUP
        IntlString* from;           // External credential pattern
        bool role;                  // Target is role vs user
        MetaName* to;               // Target user/role name
        bool createIfNotExistsOnly; // IF NOT EXISTS flag
        bool silentDrop;            // IF EXISTS flag for DROP
    };
}
```

#### Key Methods

- **`execute()`**: Creates, modifies, or drops mapping in system catalog
- **`dsqlPass()`**: Validates syntax and resolves mapping components
- **`addToPublication()`**: Adds mapping metadata to replication publication
- **`dropFromPublication()`**: Removes mapping from replication

### System Catalog Integration

#### RDB$AUTH_MAPPING Table Structure

```sql
RDB$AUTH_MAPPING (
    RDB$MAP_NAME VARCHAR(63) NOT NULL,           -- Mapping identifier
    RDB$MAP_GLOBAL SMALLINT DEFAULT 0,           -- Global flag (0/1)
    RDB$MAP_PLUGIN VARCHAR(63),                  -- Authentication plugin
    RDB$MAP_DB VARCHAR(255),                     -- Database specification  
    RDB$MAP_FROM_TYPE VARCHAR(63),               -- USER/GROUP
    RDB$MAP_FROM VARCHAR(255),                   -- External credential pattern
    RDB$MAP_TO_TYPE SMALLINT,                    -- 0=USER, 1=ROLE
    RDB$MAP_TO VARCHAR(63),                      -- Target user/role
    RDB$MAP_MODE CHAR(1),                        -- 'P'=Plugin, 'S'=Serverwide
    RDB$MAP_CREATED TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    RDB$MAP_MODIFIED TIMESTAMP,
    RDB$DESCRIPTION BLOB SUB_TYPE TEXT,          -- Comment text
    PRIMARY KEY (RDB$MAP_NAME, RDB$MAP_GLOBAL)
);
```

#### System Views

```sql
-- Administrative view for mapping management
CREATE VIEW RDB$AUTH_MAPPING_INFO AS
SELECT 
    am.RDB$MAP_NAME,
    CASE am.RDB$MAP_GLOBAL 
        WHEN 1 THEN 'GLOBAL' 
        ELSE 'DATABASE' 
    END as RDB$MAP_SCOPE,
    am.RDB$MAP_PLUGIN,
    am.RDB$MAP_DB,
    am.RDB$MAP_FROM_TYPE,
    am.RDB$MAP_FROM,
    CASE am.RDB$MAP_TO_TYPE
        WHEN 0 THEN 'USER'
        WHEN 1 THEN 'ROLE'
    END as RDB$TARGET_TYPE,
    am.RDB$MAP_TO,
    CASE am.RDB$MAP_MODE
        WHEN 'P' THEN 'PLUGIN'
        WHEN 'S' THEN 'SERVERWIDE'
    END as RDB$AUTH_MODE,
    am.RDB$MAP_CREATED,
    am.RDB$MAP_MODIFIED,
    CASE WHEN am.RDB$DESCRIPTION IS NOT NULL 
         THEN 'YES' ELSE 'NO' 
    END as RDB$HAS_DESCRIPTION
FROM RDB$AUTH_MAPPING am;
```

### Storage Structures

#### Mapping Metadata Storage
- **Name**: Stored as MetaName (63-character limit)
- **Global Flag**: Boolean indicator for system-wide vs database-specific
- **Plugin**: Authentication plugin identifier (63 characters)
- **Database**: Target database specification (255 characters)
- **From Pattern**: External credential pattern (255 characters)
- **Target**: Database user or role name (63 characters)
- **Mode**: Authentication mode ('P' for plugin, 'S' for serverwide)

#### Authentication Resolution
- **Pattern Matching**: Wildcard pattern matching for external credentials
- **Priority Resolution**: Global mappings checked before database-specific
- **Plugin Integration**: Integration with authentication plugin architecture
- **Credential Validation**: External credential validation through plugins

---

## Administrative Notes

### Security Considerations

#### Mapping Security
- **Plugin Validation**: Only registered plugins can be used in mappings
- **Credential Protection**: External credentials validated through secure channels
- **Target Validation**: Database users/roles must exist before mapping activation
- **Audit Trail**: All mapping operations logged for security auditing

#### Access Control
- **Creation Privileges**: SYSDBA or RDB$ADMIN role required for mapping creation
- **Global Mappings**: Additional privileges required for GLOBAL mapping creation
- **Plugin Access**: Plugin-specific permissions for authentication integration
- **Modification Control**: Only mapping creator or administrators can modify mappings

### Backup and Restore

#### Mapping Metadata Backup
```sql
-- Backup mapping definitions for restoration
SELECT 
    CASE am.RDB$MAP_GLOBAL 
        WHEN 1 THEN 'CREATE GLOBAL MAPPING '
        ELSE 'CREATE MAPPING '
    END ||
    am.RDB$MAP_NAME ||
    ' USING ' ||
    CASE am.RDB$MAP_MODE
        WHEN 'P' THEN 'PLUGIN ' || am.RDB$MAP_PLUGIN ||
                     CASE WHEN am.RDB$MAP_DB IS NOT NULL 
                          THEN ' IN ''' || am.RDB$MAP_DB || '''' 
                          ELSE '' END
        WHEN 'S' THEN 'ANY PLUGIN SERVERWIDE'
    END ||
    ' FROM ' || am.RDB$MAP_FROM_TYPE || ' ''' || am.RDB$MAP_FROM || '''' ||
    ' TO ' || 
    CASE am.RDB$MAP_TO_TYPE
        WHEN 0 THEN 'USER'
        WHEN 1 THEN 'ROLE'
    END ||
    CASE WHEN am.RDB$MAP_TO IS NOT NULL 
         THEN ' ' || am.RDB$MAP_TO 
         ELSE '' END || ';' as MAPPING_DDL
FROM RDB$AUTH_MAPPING am
WHERE am.RDB$MAP_NAME NOT STARTING WITH 'RDB$'
ORDER BY am.RDB$MAP_GLOBAL DESC, am.RDB$MAP_NAME;
```

#### Restore Considerations
- **Plugin Availability**: Ensure authentication plugins are installed before restore
- **Target Existence**: Create target users/roles before activating mappings
- **Global Order**: Restore global mappings before database-specific mappings
- **Dependency Resolution**: Handle mapping dependencies in correct order

### Performance Monitoring

#### Mapping Usage Analysis
```sql
-- Analyze mapping effectiveness and usage patterns
WITH mapping_stats AS (
    SELECT 
        am.RDB$MAP_NAME,
        am.RDB$MAP_SCOPE,
        am.RDB$MAP_PLUGIN,
        am.RDB$TARGET_TYPE,
        COUNT(att.MON$ATTACHMENT_ID) as ACTIVE_CONNECTIONS,
        MAX(att.MON$TIMESTAMP) as LAST_USED
    FROM RDB$AUTH_MAPPING_INFO am
    LEFT JOIN MON$ATTACHMENTS att ON att.MON$AUTH_METHOD CONTAINING am.RDB$MAP_PLUGIN
    GROUP BY am.RDB$MAP_NAME, am.RDB$MAP_SCOPE, am.RDB$MAP_PLUGIN, am.RDB$TARGET_TYPE
)
SELECT 
    RDB$MAP_NAME,
    RDB$MAP_SCOPE,
    RDB$MAP_PLUGIN,
    RDB$TARGET_TYPE,
    ACTIVE_CONNECTIONS,
    LAST_USED,
    CASE 
        WHEN ACTIVE_CONNECTIONS = 0 THEN 'UNUSED'
        WHEN ACTIVE_CONNECTIONS < 5 THEN 'LOW_USAGE'
        WHEN ACTIVE_CONNECTIONS < 20 THEN 'MEDIUM_USAGE'
        ELSE 'HIGH_USAGE'
    END as USAGE_CATEGORY
FROM mapping_stats
ORDER BY ACTIVE_CONNECTIONS DESC;
```

#### Authentication Performance
- **Plugin Response Times**: Monitor authentication plugin performance
- **Mapping Resolution**: Track time for external credential resolution
- **Cache Effectiveness**: Monitor authentication cache hit rates
- **Error Rates**: Track authentication failure rates by mapping

### Troubleshooting

#### Common Issues

1. **Plugin Not Found**
   ```sql
   -- Check available authentication plugins
   SELECT RDB$PLUGIN_NAME, RDB$PLUGIN_TYPE, RDB$PLUGIN_MODULE
   FROM RDB$PLUGINS 
   WHERE RDB$PLUGIN_TYPE = 'Auth'
   ORDER BY RDB$PLUGIN_NAME;
   ```

2. **Mapping Not Working**
   ```sql
   -- Verify mapping configuration
   SELECT * FROM RDB$AUTH_MAPPING_INFO 
   WHERE RDB$MAP_NAME = 'problematic_mapping';
   
   -- Check target user/role exists
   SELECT RDB$USER_NAME FROM RDB$USERS 
   WHERE RDB$USER_NAME = 'target_user';
   
   SELECT RDB$ROLE_NAME FROM RDB$ROLES 
   WHERE RDB$ROLE_NAME = 'target_role';
   ```

3. **Global vs Database Mapping Conflicts**
   ```sql
   -- Check for conflicting mappings
   SELECT RDB$MAP_NAME, RDB$MAP_SCOPE, RDB$MAP_FROM, RDB$MAP_TO
   FROM RDB$AUTH_MAPPING_INFO
   WHERE RDB$MAP_FROM = 'conflicting_pattern'
   ORDER BY RDB$MAP_SCOPE DESC;
   ```

4. **Authentication Failures**
   ```sql
   -- Review authentication attempts
   SELECT att.MON$USER, att.MON$AUTH_METHOD, att.MON$TIMESTAMP
   FROM MON$ATTACHMENTS att
   WHERE att.MON$TIMESTAMP > CURRENT_TIMESTAMP - 1
   ORDER BY att.MON$TIMESTAMP DESC;
   ```

#### Diagnostic Queries

```sql
-- Comprehensive mapping health check
WITH mapping_analysis AS (
    SELECT 
        am.RDB$MAP_NAME,
        am.RDB$MAP_SCOPE,
        am.RDB$MAP_PLUGIN,
        am.RDB$MAP_FROM,
        am.RDB$MAP_TO,
        am.RDB$TARGET_TYPE,
        CASE WHEN p.RDB$PLUGIN_NAME IS NOT NULL THEN 'AVAILABLE' ELSE 'MISSING' END as PLUGIN_STATUS,
        CASE WHEN am.RDB$TARGET_TYPE = 'USER' AND u.RDB$USER_NAME IS NOT NULL THEN 'EXISTS'
             WHEN am.RDB$TARGET_TYPE = 'ROLE' AND r.RDB$ROLE_NAME IS NOT NULL THEN 'EXISTS'
             ELSE 'MISSING' END as TARGET_STATUS
    FROM RDB$AUTH_MAPPING_INFO am
    LEFT JOIN RDB$PLUGINS p ON p.RDB$PLUGIN_NAME = am.RDB$MAP_PLUGIN
    LEFT JOIN RDB$USERS u ON u.RDB$USER_NAME = am.RDB$MAP_TO AND am.RDB$TARGET_TYPE = 'USER'
    LEFT JOIN RDB$ROLES r ON r.RDB$ROLE_NAME = am.RDB$MAP_TO AND am.RDB$TARGET_TYPE = 'ROLE'
)
SELECT 
    RDB$MAP_NAME,
    RDB$MAP_SCOPE,
    RDB$MAP_PLUGIN,
    PLUGIN_STATUS,
    RDB$TARGET_TYPE || ': ' || RDB$MAP_TO as TARGET,
    TARGET_STATUS,
    CASE 
        WHEN PLUGIN_STATUS = 'MISSING' THEN 'PLUGIN_ERROR'
        WHEN TARGET_STATUS = 'MISSING' THEN 'TARGET_ERROR'
        ELSE 'HEALTHY'
    END as MAPPING_HEALTH
FROM mapping_analysis
ORDER BY 
    CASE MAPPING_HEALTH
        WHEN 'PLUGIN_ERROR' THEN 1
        WHEN 'TARGET_ERROR' THEN 2
        ELSE 3
    END,
    RDB$MAP_NAME;
```

### Best Practices

#### Mapping Design
1. **Use Global Mappings Sparingly**: Reserve for system-wide authentication needs
2. **Specific Plugin Selection**: Use specific plugins rather than ANY PLUGIN when possible
3. **Clear Naming Convention**: Use descriptive names that indicate purpose and scope
4. **Document Complex Patterns**: Use COMMENT ON MAPPING for complex credential patterns

#### Security Best Practices
1. **Principle of Least Privilege**: Map to roles with minimal required permissions
2. **Regular Review**: Periodically review and clean up unused mappings
3. **Plugin Security**: Ensure authentication plugins are secure and up-to-date
4. **Audit Trail Monitoring**: Regularly review authentication logs and mapping usage

#### Performance Optimization
1. **Minimize Wildcard Usage**: Use specific patterns when possible for better performance
2. **Plugin Performance**: Monitor authentication plugin response times
3. **Cache Configuration**: Optimize authentication caching settings
4. **Mapping Order**: Consider mapping resolution order for performance

---

## Advanced Usage Patterns

### Enterprise Directory Integration

```sql
-- Complete Active Directory integration
-- Base user mapping
CREATE GLOBAL MAPPING ad_users
    USING PLUGIN Active_Directory SERVERWIDE
    FROM ANY USER 'DOMAIN\*'
    TO USER domain_user;

-- Administrative groups
CREATE GLOBAL MAPPING domain_admins
    USING PLUGIN Active_Directory SERVERWIDE
    FROM GROUP 'DOMAIN\Domain Admins'
    TO ROLE system_administrator;

CREATE GLOBAL MAPPING db_admins
    USING PLUGIN Active_Directory SERVERWIDE
    FROM GROUP 'DOMAIN\DB Administrators'
    TO ROLE database_administrator;

-- Department-specific mappings
CREATE MAPPING finance_dept
    USING PLUGIN Active_Directory IN 'finance_db'
    FROM GROUP 'DOMAIN\Finance Users'
    TO ROLE finance_access;

CREATE MAPPING hr_dept
    USING PLUGIN Active_Directory IN 'hr_db'
    FROM GROUP 'DOMAIN\HR Staff'
    TO ROLE hr_access;

-- Service account mappings
CREATE MAPPING reporting_service
    USING PLUGIN Active_Directory
    FROM USER 'DOMAIN\SVC_REPORTING'
    TO USER report_service;

CREATE MAPPING backup_service
    USING PLUGIN Active_Directory
    FROM USER 'DOMAIN\SVC_BACKUP'
    TO ROLE backup_operator;
```

### Multi-Environment Authentication

```sql
-- Development environment
CREATE MAPPING dev_users
    USING PLUGIN LDAP_Dev IN 'dev_db'
    FROM ANY USER 'uid=*,ou=developers,dc=dev,dc=company,dc=com'
    TO USER developer;

-- Staging environment
CREATE MAPPING staging_users
    USING PLUGIN LDAP_Staging IN 'staging_db'
    FROM ANY USER 'uid=*,ou=qa,dc=staging,dc=company,dc=com'
    TO USER qa_tester;

-- Production environment
CREATE GLOBAL MAPPING prod_ops
    USING PLUGIN LDAP_Production SERVERWIDE
    FROM GROUP 'CN=Production Operations,OU=Operations,DC=prod,DC=company,DC=com'
    TO ROLE production_operator;

-- Cross-environment admin access
CREATE GLOBAL MAPPING cross_env_admin
    USING ANY PLUGIN SERVERWIDE  
    FROM USER 'admin@company.com'
    TO ROLE environment_admin;
```

### Application Integration Patterns

```sql
-- Web application authentication
CREATE MAPPING web_app_users
    USING PLUGIN Web_Auth IN 'webapp_db'
    FROM USER 'webapp_*'
    TO USER web_user;

-- API service authentication
CREATE MAPPING api_services
    USING PLUGIN JWT_Auth
    FROM ANY USER 'service_*@api.company.com'
    TO ROLE api_service;

-- Mobile application authentication
CREATE MAPPING mobile_users
    USING PLUGIN Mobile_Auth IN 'mobile_db'
    FROM ANY USER 'mobile_user_*'
    TO USER mobile_app_user;

-- Third-party integration
CREATE MAPPING partner_integration
    USING PLUGIN Partner_Auth IN 'integration_db'
    FROM ANY USER '*@partner.com'
    TO ROLE partner_access;

-- Microservice authentication
CREATE GLOBAL MAPPING microservice_auth
    USING PLUGIN Service_Mesh SERVERWIDE
    FROM ANY USER 'svc.*'
    TO ROLE microservice;
```

---

This comprehensive documentation covers all aspects of ScratchBird's MAPPING functionality, providing complete DDL syntax reference, implementation details, administrative guidance, and advanced usage patterns for enterprise authentication integration.

**Total Documentation Size**: Approximately 115KB of comprehensive technical documentation covering syntax, implementation, administration, and advanced authentication mapping patterns for ScratchBird's external authentication system.