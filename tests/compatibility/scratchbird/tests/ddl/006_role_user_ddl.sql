-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DDL - Role and User Operations
-- Description: Comprehensive CREATE, ALTER, DROP ROLE/USER testing
-- ============================================================================

-- Role/User DDL: User and role management
-- CREATE ROLE/USER: Create roles and users
-- ALTER ROLE/USER: Modify roles and users
-- DROP ROLE/USER: Remove roles and users
-- Role attributes, membership, privileges

-- Create test database
CREATE DATABASE test_role_user_ddl_db;
USE test_role_user_ddl_db;

-- ============================================================================
-- Section 1: CREATE ROLE Basic
-- ============================================================================

-- Create basic role
CREATE ROLE test_basic_role;

-- Verify role exists
SELECT
    rolname,
    rolsuper,
    rolinherit,
    rolcreaterole,
    rolcreatedb,
    rolcanlogin,
    rolconnlimit
FROM pg_roles
WHERE rolname = 'test_basic_role';

-- ============================================================================
-- Section 2: CREATE USER vs CREATE ROLE
-- ============================================================================

-- CREATE USER (can login by default)
CREATE USER test_user1;

-- CREATE ROLE (cannot login by default)
CREATE ROLE test_role1;

-- Verify difference
SELECT
    rolname,
    rolcanlogin AS can_login
FROM pg_roles
WHERE rolname IN ('test_user1', 'test_role1')
ORDER BY rolname;

-- ============================================================================
-- Section 3: CREATE ROLE with Attributes
-- ============================================================================

-- Role with various attributes
CREATE ROLE test_role_attrs
    LOGIN
    PASSWORD 'secure_password'
    VALID UNTIL '2025-12-31'
    CONNECTION LIMIT 10
    CREATEDB
    CREATEROLE
    INHERIT
    REPLICATION
    BYPASSRLS;

-- Query role attributes
SELECT
    rolname,
    rolsuper,
    rolinherit,
    rolcreaterole,
    rolcreatedb,
    rolcanlogin,
    rolreplication,
    rolbypassrls,
    rolconnlimit,
    rolvaliduntil
FROM pg_roles
WHERE rolname = 'test_role_attrs';

-- ============================================================================
-- Section 4: CREATE ROLE IF NOT EXISTS
-- ============================================================================

-- Idempotent role creation
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_roles WHERE rolname = 'test_idempotent_role') THEN
        CREATE ROLE test_idempotent_role;
    END IF;
END $$;

-- Try again (no error)
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_roles WHERE rolname = 'test_idempotent_role') THEN
        CREATE ROLE test_idempotent_role;
    END IF;
END $$;

SELECT rolname FROM pg_roles WHERE rolname = 'test_idempotent_role';

-- ============================================================================
-- Section 5: ALTER ROLE - Rename
-- ============================================================================

CREATE ROLE test_old_role_name;

-- Rename role
ALTER ROLE test_old_role_name RENAME TO test_new_role_name;

-- Verify rename
SELECT rolname
FROM pg_roles
WHERE rolname IN ('test_old_role_name', 'test_new_role_name');

-- ============================================================================
-- Section 6: ALTER ROLE - Modify Attributes
-- ============================================================================

CREATE ROLE test_alter_role;

-- Change password
ALTER ROLE test_alter_role PASSWORD 'new_password';

-- Enable login
ALTER ROLE test_alter_role LOGIN;

-- Set connection limit
ALTER ROLE test_alter_role CONNECTION LIMIT 20;

-- Grant database creation
ALTER ROLE test_alter_role CREATEDB;

-- Set valid until
ALTER ROLE test_alter_role VALID UNTIL '2026-01-01';

-- Query updated attributes
SELECT
    rolname,
    rolcanlogin,
    rolcreatedb,
    rolconnlimit,
    rolvaliduntil
FROM pg_roles
WHERE rolname = 'test_alter_role';

-- ============================================================================
-- Section 7: Role Membership (GRANT/REVOKE)
-- ============================================================================

CREATE ROLE test_group_role;
CREATE ROLE test_member_role1;
CREATE ROLE test_member_role2;

-- Add members to group
GRANT test_group_role TO test_member_role1;
GRANT test_group_role TO test_member_role2;

-- Query role membership
SELECT
    r.rolname AS member_role,
    m.rolname AS member_of_role
FROM pg_auth_members am
JOIN pg_roles r ON r.oid = am.member
JOIN pg_roles m ON m.oid = am.roleid
WHERE m.rolname = 'test_group_role'
ORDER BY r.rolname;

-- Revoke membership
REVOKE test_group_role FROM test_member_role2;

-- Verify revocation
SELECT COUNT(*) AS member_count
FROM pg_auth_members am
JOIN pg_roles r ON r.oid = am.member
JOIN pg_roles m ON m.oid = am.roleid
WHERE m.rolname = 'test_group_role';

-- ============================================================================
-- Section 8: Role with INHERIT Attribute
-- ============================================================================

CREATE ROLE test_parent_role NOINHERIT;
CREATE ROLE test_child_role INHERIT;

GRANT test_parent_role TO test_child_role;

-- Child role with INHERIT gets parent permissions automatically
-- Child role with NOINHERIT must use SET ROLE

-- ============================================================================
-- Section 9: DROP ROLE
-- ============================================================================

CREATE ROLE test_drop_role;

-- Drop role
DROP ROLE test_drop_role;

-- Verify dropped
SELECT COUNT(*) AS role_count
FROM pg_roles
WHERE rolname = 'test_drop_role';

-- ============================================================================
-- Section 10: DROP ROLE IF EXISTS
-- ============================================================================

-- Drop if exists (no error if doesn't exist)
DROP ROLE IF EXISTS test_nonexistent_role;

-- Create and drop
CREATE ROLE test_temp_role;
DROP ROLE IF EXISTS test_temp_role;

-- Try again (no error)
DROP ROLE IF EXISTS test_temp_role;

-- ============================================================================
-- Section 11: SET ROLE and Session User
-- ============================================================================

-- Query current role and session user
SELECT current_user, session_user, current_role;

-- Create role for testing
CREATE ROLE test_switch_role LOGIN;

-- Switch to role (in application session)
-- SET ROLE test_switch_role;
-- SELECT current_role;  -- Returns test_switch_role

-- Reset to session user
-- RESET ROLE;

-- ============================================================================
-- Section 12: Role for Application User Groups
-- ============================================================================

-- Create role hierarchy for application
CREATE ROLE app_readonly;
CREATE ROLE app_readwrite;
CREATE ROLE app_admin;

-- Grant progressively more privileges
-- (actual grants would be on tables, shown for structure)

-- Create application users
CREATE ROLE app_user_readonly LOGIN IN ROLE app_readonly;
CREATE ROLE app_user_readwrite LOGIN IN ROLE app_readwrite;
CREATE ROLE app_user_admin LOGIN IN ROLE app_admin;

-- Query role hierarchy
SELECT
    r.rolname AS role,
    ARRAY_AGG(m.rolname) AS member_of
FROM pg_roles r
LEFT JOIN pg_auth_members am ON r.oid = am.member
LEFT JOIN pg_roles m ON m.oid = am.roleid
WHERE r.rolname LIKE 'app_%'
GROUP BY r.rolname
ORDER BY r.rolname;

-- ============================================================================
-- Section 13: Superuser Role
-- ============================================================================

-- Query superusers
SELECT
    rolname,
    rolsuper
FROM pg_roles
WHERE rolsuper = true
ORDER BY rolname;

-- Create superuser (requires superuser privileges)
-- CREATE ROLE test_superuser SUPERUSER;

-- ============================================================================
-- Section 14: Role Configuration Parameters
-- ============================================================================

CREATE ROLE test_config_role;

-- Set role-specific configuration
ALTER ROLE test_config_role SET search_path = public, app_schema;
ALTER ROLE test_config_role SET timezone = 'America/New_York';
ALTER ROLE test_config_role SET work_mem = '32MB';

-- Query role configuration
SELECT
    rolname,
    rolconfig
FROM pg_roles
WHERE rolname = 'test_config_role';

-- Reset configuration parameter
ALTER ROLE test_config_role RESET search_path;

-- ============================================================================
-- Section 15: Password Policy and Management
-- ============================================================================

CREATE ROLE test_password_role;

-- Set password
ALTER ROLE test_password_role PASSWORD 'initial_password';

-- Set password with expiration
ALTER ROLE test_password_role VALID UNTIL '2025-06-30';

-- Disable password expiration
ALTER ROLE test_password_role VALID UNTIL 'infinity';

-- Disable login
ALTER ROLE test_password_role NOLOGIN;

-- ============================================================================
-- Section 16: Service Account Roles
-- ============================================================================

-- Create service account (application connection)
CREATE ROLE app_service_account
    LOGIN
    PASSWORD 'service_account_password'
    CONNECTION LIMIT 20;

-- Create monitoring role (read-only system access)
CREATE ROLE monitoring_service
    LOGIN
    PASSWORD 'monitoring_password'
    CONNECTION LIMIT 5;

-- Create backup role
CREATE ROLE backup_service
    LOGIN
    PASSWORD 'backup_password'
    REPLICATION;

-- ============================================================================
-- Section 17: Query Role Information
-- ============================================================================

-- Comprehensive role information
SELECT
    r.rolname AS role_name,
    r.rolsuper AS is_superuser,
    r.rolinherit AS inherits_privileges,
    r.rolcreaterole AS can_create_roles,
    r.rolcreatedb AS can_create_databases,
    r.rolcanlogin AS can_login,
    r.rolreplication AS replication_role,
    r.rolbypassrls AS bypass_rls,
    r.rolconnlimit AS connection_limit,
    r.rolvaliduntil AS password_expires,
    ARRAY(
        SELECT m.rolname
        FROM pg_auth_members am
        JOIN pg_roles m ON m.oid = am.roleid
        WHERE am.member = r.oid
    ) AS member_of
FROM pg_roles r
WHERE r.rolname NOT LIKE 'pg_%'
ORDER BY r.rolname;

-- ============================================================================
-- Section 18: Active Sessions by Role
-- ============================================================================

-- Query active connections by role
SELECT
    usename AS role,
    application_name,
    client_addr,
    state,
    COUNT(*) AS connection_count
FROM pg_stat_activity
GROUP BY usename, application_name, client_addr, state
ORDER BY connection_count DESC, usename;

-- ============================================================================
-- Section 19: Role Dependencies
-- ============================================================================

-- Cannot drop role that owns objects or has privileges

CREATE ROLE test_owner_role;

-- Role owns database
ALTER DATABASE test_role_user_ddl_db OWNER TO test_owner_role;

-- Try to drop (would fail)
-- DROP ROLE test_owner_role;  -- ERROR: role owns objects

-- Reassign ownership first
REASSIGN OWNED BY test_owner_role TO CURRENT_USER;

-- Now can drop
DROP ROLE test_owner_role;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_role_user_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_role_user_best_practices VALUES
    (1, 'Use roles for permission groups (readonly, readwrite, admin)'),
    (2, 'CREATE USER for login accounts, CREATE ROLE for permission groups'),
    (3, 'Grant roles to users rather than individual permissions'),
    (4, 'Use INHERIT for automatic privilege inheritance'),
    (5, 'Set CONNECTION LIMIT for production roles'),
    (6, 'Use VALID UNTIL for temporary accounts'),
    (7, 'Create service accounts per application'),
    (8, 'Use strong passwords for all login roles'),
    (9, 'Minimize number of SUPERUSER roles'),
    (10, 'Use REPLICATION role only for replication purposes'),
    (11, 'Set role-specific configuration parameters'),
    (12, 'Monitor active sessions by role'),
    (13, 'Use REASSIGN OWNED before dropping roles'),
    (14, 'Document role purpose and membership'),
    (15, 'Regular audit of role privileges'),
    (16, 'Use BYPASSRLS sparingly (security risk)'),
    (17, 'Avoid granting CREATEROLE widely'),
    (18, 'Use SET ROLE for privilege escalation (audit trail)'),
    (19, 'Separate application roles from administrative roles'),
    (20, 'Test role permissions in development environment');

SELECT id, guideline FROM test_role_user_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

-- Drop all test roles
DROP TABLE test_role_user_best_practices;

DROP ROLE app_user_admin;
DROP ROLE app_user_readwrite;
DROP ROLE app_user_readonly;
DROP ROLE app_admin;
DROP ROLE app_readwrite;
DROP ROLE app_readonly;

DROP ROLE backup_service;
DROP ROLE monitoring_service;
DROP ROLE app_service_account;

DROP ROLE test_password_role;
DROP ROLE test_config_role;

DROP ROLE test_member_role1;
DROP ROLE test_child_role;
DROP ROLE test_parent_role;

DROP ROLE test_group_role;
DROP ROLE test_switch_role;
DROP ROLE test_alter_role;
DROP ROLE test_new_role_name;
DROP ROLE test_idempotent_role;
DROP ROLE test_role_attrs;
DROP ROLE test_role1;
DROP ROLE test_user1;
DROP ROLE test_basic_role;

DROP DATABASE test_role_user_ddl_db;

-- End of role and user DDL tests
