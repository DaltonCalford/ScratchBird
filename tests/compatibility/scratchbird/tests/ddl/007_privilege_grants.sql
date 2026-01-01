-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DDL - Privilege and Grant Operations
-- Description: Comprehensive GRANT, REVOKE privilege testing
-- ============================================================================

-- Privilege DDL: Access control and permissions
-- GRANT: Grant privileges to roles
-- REVOKE: Revoke privileges from roles
-- Table, schema, database, function privileges
-- Default privileges, role membership

-- Create test database
CREATE DATABASE test_privilege_grants_db;
USE test_privilege_grants_db;

-- ============================================================================
-- Section 1: Basic Table Privileges - GRANT SELECT
-- ============================================================================

CREATE ROLE test_reader;

CREATE TABLE test_basic_privileges (
    id SERIAL PRIMARY KEY,
    data TEXT
);

INSERT INTO test_basic_privileges (data) VALUES ('Test data');

-- Grant SELECT privilege
GRANT SELECT ON test_basic_privileges TO test_reader;

-- Query table privileges
SELECT
    grantee,
    privilege_type,
    is_grantable
FROM information_schema.table_privileges
WHERE table_name = 'test_basic_privileges'
  AND grantee = 'test_reader'
ORDER BY privilege_type;

-- ============================================================================
-- Section 2: Multiple Table Privileges - INSERT, UPDATE, DELETE
-- ============================================================================

CREATE ROLE test_writer;

CREATE TABLE test_multiple_privileges (
    id SERIAL PRIMARY KEY,
    value INT
);

-- Grant multiple privileges
GRANT SELECT, INSERT, UPDATE, DELETE ON test_multiple_privileges TO test_writer;

-- Query privileges
SELECT
    grantee,
    privilege_type
FROM information_schema.table_privileges
WHERE table_name = 'test_multiple_privileges'
  AND grantee = 'test_writer'
ORDER BY privilege_type;

-- ============================================================================
-- Section 3: GRANT ALL PRIVILEGES
-- ============================================================================

CREATE ROLE test_full_access;

CREATE TABLE test_all_privileges (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Grant all privileges on table
GRANT ALL PRIVILEGES ON test_all_privileges TO test_full_access;

-- Query all granted privileges
SELECT
    grantee,
    privilege_type
FROM information_schema.table_privileges
WHERE table_name = 'test_all_privileges'
  AND grantee = 'test_full_access'
ORDER BY privilege_type;

-- ============================================================================
-- Section 4: GRANT with WITH GRANT OPTION
-- ============================================================================

CREATE ROLE test_grantor;
CREATE ROLE test_sub_grantee;

CREATE TABLE test_grant_option (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Grant with ability to re-grant
GRANT SELECT ON test_grant_option TO test_grantor WITH GRANT OPTION;

-- Query grant option
SELECT
    grantee,
    privilege_type,
    is_grantable
FROM information_schema.table_privileges
WHERE table_name = 'test_grant_option'
  AND grantee = 'test_grantor';

-- test_grantor could now grant SELECT to test_sub_grantee

-- ============================================================================
-- Section 5: REVOKE Privileges
-- ============================================================================

CREATE ROLE test_revoke_user;

CREATE TABLE test_revoke_privileges (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Grant privileges
GRANT SELECT, INSERT, UPDATE ON test_revoke_privileges TO test_revoke_user;

-- Verify granted
SELECT privilege_type
FROM information_schema.table_privileges
WHERE table_name = 'test_revoke_privileges'
  AND grantee = 'test_revoke_user'
ORDER BY privilege_type;

-- Revoke specific privilege
REVOKE INSERT ON test_revoke_privileges FROM test_revoke_user;

-- Verify revoked
SELECT privilege_type
FROM information_schema.table_privileges
WHERE table_name = 'test_revoke_privileges'
  AND grantee = 'test_revoke_user'
ORDER BY privilege_type;

-- ============================================================================
-- Section 6: REVOKE CASCADE
-- ============================================================================

CREATE ROLE test_cascade_grantor;
CREATE ROLE test_cascade_grantee;

CREATE TABLE test_revoke_cascade (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Grant with grant option
GRANT SELECT ON test_revoke_cascade TO test_cascade_grantor WITH GRANT OPTION;

-- test_cascade_grantor grants to test_cascade_grantee
-- (simulated - would need to SET ROLE)

-- Revoke from original grantor (cascades to sub-grantees)
REVOKE SELECT ON test_revoke_cascade FROM test_cascade_grantor CASCADE;

-- Verify revoked
SELECT COUNT(*) AS privilege_count
FROM information_schema.table_privileges
WHERE table_name = 'test_revoke_cascade'
  AND grantee IN ('test_cascade_grantor', 'test_cascade_grantee');

-- ============================================================================
-- Section 7: Schema Privileges
-- ============================================================================

CREATE SCHEMA test_schema_privileges;
CREATE ROLE test_schema_user;

-- Grant USAGE on schema (required to access objects)
GRANT USAGE ON SCHEMA test_schema_privileges TO test_schema_user;

-- Grant CREATE (can create objects in schema)
GRANT CREATE ON SCHEMA test_schema_privileges TO test_schema_user;

-- Query schema privileges
SELECT
    grantee,
    privilege_type
FROM information_schema.schema_privileges
WHERE schema_name = 'test_schema_privileges'
  AND grantee = 'test_schema_user'
ORDER BY privilege_type;

-- ============================================================================
-- Section 8: Database Privileges
-- ============================================================================

CREATE ROLE test_db_user;

-- Grant CONNECT on database
GRANT CONNECT ON DATABASE test_privilege_grants_db TO test_db_user;

-- Grant CREATE (can create schemas)
GRANT CREATE ON DATABASE test_privilege_grants_db TO test_db_user;

-- Grant TEMPORARY (can create temp tables)
GRANT TEMPORARY ON DATABASE test_privilege_grants_db TO test_db_user;

-- Query database privileges
SELECT
    grantee,
    privilege_type
FROM information_schema.database_privileges
WHERE grantee = 'test_db_user'
ORDER BY privilege_type;

-- ============================================================================
-- Section 9: Sequence Privileges
-- ============================================================================

CREATE SEQUENCE test_privilege_sequence;
CREATE ROLE test_sequence_user;

-- Grant USAGE on sequence (can use nextval, currval)
GRANT USAGE ON SEQUENCE test_privilege_sequence TO test_sequence_user;

-- Grant SELECT (can see sequence value)
GRANT SELECT ON SEQUENCE test_privilege_sequence TO test_sequence_user;

-- Grant UPDATE (can use setval)
GRANT UPDATE ON SEQUENCE test_privilege_sequence TO test_sequence_user;

-- Query sequence privileges
SELECT
    grantee,
    privilege_type
FROM information_schema.usage_privileges
WHERE object_name = 'test_privilege_sequence'
  AND grantee = 'test_sequence_user';

-- ============================================================================
-- Section 10: Function/Procedure Privileges
-- ============================================================================

CREATE FUNCTION test_privilege_function() RETURNS INT AS $$
BEGIN
    RETURN 42;
END;
$$ LANGUAGE plpgsql;

CREATE ROLE test_function_user;

-- Grant EXECUTE on function
GRANT EXECUTE ON FUNCTION test_privilege_function() TO test_function_user;

-- Query function privileges
SELECT
    grantee,
    privilege_type
FROM information_schema.routine_privileges
WHERE routine_name = 'test_privilege_function'
  AND grantee = 'test_function_user';

-- ============================================================================
-- Section 11: Column-Level Privileges
-- ============================================================================

CREATE ROLE test_column_user;

CREATE TABLE test_column_privileges (
    id SERIAL PRIMARY KEY,
    public_data TEXT,
    private_data TEXT,
    secret_data TEXT
);

-- Grant SELECT on specific columns only
GRANT SELECT (id, public_data) ON test_column_privileges TO test_column_user;

-- Grant UPDATE on specific column
GRANT UPDATE (public_data) ON test_column_privileges TO test_column_user;

-- Query column privileges
SELECT
    grantee,
    privilege_type,
    column_name
FROM information_schema.column_privileges
WHERE table_name = 'test_column_privileges'
  AND grantee = 'test_column_user'
ORDER BY column_name, privilege_type;

-- ============================================================================
-- Section 12: GRANT to PUBLIC
-- ============================================================================

CREATE TABLE test_public_access (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Grant to all users (PUBLIC)
GRANT SELECT ON test_public_access TO PUBLIC;

-- Query privileges
SELECT
    grantee,
    privilege_type
FROM information_schema.table_privileges
WHERE table_name = 'test_public_access'
  AND grantee = 'PUBLIC';

-- ============================================================================
-- Section 13: Default Privileges
-- ============================================================================

CREATE ROLE test_default_priv_owner;
CREATE ROLE test_default_priv_user;

-- Set default privileges for future tables
ALTER DEFAULT PRIVILEGES FOR ROLE test_default_priv_owner
GRANT SELECT, INSERT ON TABLES TO test_default_priv_user;

-- Set default for sequences
ALTER DEFAULT PRIVILEGES FOR ROLE test_default_priv_owner
GRANT USAGE ON SEQUENCES TO test_default_priv_user;

-- Set default for functions
ALTER DEFAULT PRIVILEGES FOR ROLE test_default_priv_owner
GRANT EXECUTE ON FUNCTIONS TO test_default_priv_user;

-- Query default privileges
SELECT
    defaclrole::regrole AS grantor,
    defaclnamespace,
    defaclobjtype AS object_type,
    defaclacl AS privileges
FROM pg_default_acl
WHERE defaclrole = 'test_default_priv_owner'::regrole;

-- ============================================================================
-- Section 14: Privileges on Multiple Tables
-- ============================================================================

CREATE ROLE test_multi_table_user;

CREATE TABLE test_multi_table1 (id INT);
CREATE TABLE test_multi_table2 (id INT);
CREATE TABLE test_multi_table3 (id INT);

-- Grant on all tables in schema
GRANT SELECT ON ALL TABLES IN SCHEMA public TO test_multi_table_user;

-- Count granted privileges
SELECT COUNT(*) AS tables_with_access
FROM information_schema.table_privileges
WHERE grantee = 'test_multi_table_user'
  AND privilege_type = 'SELECT'
  AND table_schema = 'public';

-- ============================================================================
-- Section 15: Privileges on All Sequences
-- ============================================================================

CREATE ROLE test_multi_sequence_user;

CREATE SEQUENCE test_seq1;
CREATE SEQUENCE test_seq2;
CREATE SEQUENCE test_seq3;

-- Grant on all sequences in schema
GRANT USAGE ON ALL SEQUENCES IN SCHEMA public TO test_multi_sequence_user;

-- Verify grants
SELECT COUNT(*) AS sequences_with_access
FROM information_schema.usage_privileges
WHERE grantee = 'test_multi_sequence_user'
  AND object_schema = 'public';

-- ============================================================================
-- Section 16: Row Level Security (RLS) Policies
-- ============================================================================

CREATE TABLE test_rls_table (
    id SERIAL PRIMARY KEY,
    user_id INT,
    data TEXT
);

CREATE ROLE test_rls_user;

-- Enable RLS
ALTER TABLE test_rls_table ENABLE ROW LEVEL SECURITY;

-- Create policy (users see only their rows)
CREATE POLICY test_rls_policy ON test_rls_table
    FOR SELECT
    TO test_rls_user
    USING (user_id = current_setting('app.current_user_id')::INT);

-- Grant table access
GRANT SELECT ON test_rls_table TO test_rls_user;

-- Query policies
SELECT
    tablename,
    policyname,
    permissive,
    roles,
    cmd AS command,
    qual AS using_expression
FROM pg_policies
WHERE tablename = 'test_rls_table';

-- ============================================================================
-- Section 17: GRANT OPTION Tracking
-- ============================================================================

CREATE TABLE test_grant_tracking (
    grantor VARCHAR(100),
    grantee VARCHAR(100),
    privilege VARCHAR(50),
    grantable BOOLEAN
);

-- Populate with current grants
INSERT INTO test_grant_tracking
SELECT
    grantor,
    grantee,
    privilege_type,
    is_grantable::BOOLEAN
FROM information_schema.table_privileges
WHERE table_schema = 'public'
  AND grantee NOT IN ('PUBLIC', 'postgres')
ORDER BY table_name, grantee, privilege_type;

SELECT * FROM test_grant_tracking ORDER BY grantor, grantee, privilege;

-- ============================================================================
-- Section 18: Privilege Inheritance through Roles
-- ============================================================================

CREATE ROLE test_parent_priv_role;
CREATE ROLE test_child_priv_role INHERIT;

CREATE TABLE test_inherited_privileges (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Grant to parent role
GRANT SELECT, INSERT ON test_inherited_privileges TO test_parent_priv_role;

-- Add child to parent role
GRANT test_parent_priv_role TO test_child_priv_role;

-- Child role inherits privileges (INHERIT attribute)

-- ============================================================================
-- Section 19: Revoking Default Privileges
-- ============================================================================

CREATE ROLE test_default_revoke_user;

-- Revoke default PUBLIC privileges
REVOKE ALL ON ALL TABLES IN SCHEMA public FROM PUBLIC;
REVOKE ALL ON DATABASE test_privilege_grants_db FROM PUBLIC;

-- Grant specific access
GRANT CONNECT ON DATABASE test_privilege_grants_db TO test_default_revoke_user;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_privilege_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_privilege_best_practices VALUES
    (1, 'Follow principle of least privilege (grant minimum needed)'),
    (2, 'Grant privileges to roles, not individual users'),
    (3, 'Use schemas to organize and control access'),
    (4, 'Grant schema USAGE before granting table privileges'),
    (5, 'Use DEFAULT PRIVILEGES for consistent permissions'),
    (6, 'Revoke PUBLIC access when appropriate'),
    (7, 'Use column-level privileges for sensitive data'),
    (8, 'WITH GRANT OPTION sparingly (privilege escalation risk)'),
    (9, 'REVOKE CASCADE to remove dependent grants'),
    (10, 'Use Row Level Security (RLS) for multi-tenant data'),
    (11, 'Regular audit of granted privileges'),
    (12, 'Document privilege requirements per role'),
    (13, 'Test application with minimum required privileges'),
    (14, 'Use GRANT ALL only when truly needed'),
    (15, 'Separate read-only and read-write roles'),
    (16, 'Grant EXECUTE on functions separately'),
    (17, 'Use INHERIT for automatic privilege inheritance'),
    (18, 'Monitor privilege grants in production'),
    (19, 'Revoke privileges before dropping roles'),
    (20, 'Consider security implications of PUBLIC grants');

SELECT id, guideline FROM test_privilege_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_privilege_best_practices;
DROP TABLE test_inherited_privileges;
DROP ROLE test_child_priv_role;
DROP ROLE test_parent_priv_role;
DROP TABLE test_grant_tracking;
DROP POLICY test_rls_policy ON test_rls_table;
DROP TABLE test_rls_table;
DROP ROLE test_rls_user;
DROP SEQUENCE test_seq3;
DROP SEQUENCE test_seq2;
DROP SEQUENCE test_seq1;
DROP ROLE test_multi_sequence_user;
DROP TABLE test_multi_table3;
DROP TABLE test_multi_table2;
DROP TABLE test_multi_table1;
DROP ROLE test_multi_table_user;
DROP ROLE test_default_priv_user;
DROP ROLE test_default_priv_owner;
DROP TABLE test_public_access;
DROP TABLE test_column_privileges;
DROP ROLE test_column_user;
DROP FUNCTION test_privilege_function();
DROP ROLE test_function_user;
DROP SEQUENCE test_privilege_sequence;
DROP ROLE test_sequence_user;
DROP ROLE test_db_user;
DROP SCHEMA test_schema_privileges CASCADE;
DROP ROLE test_schema_user;
DROP TABLE test_revoke_cascade;
DROP ROLE test_cascade_grantee;
DROP ROLE test_cascade_grantor;
DROP TABLE test_revoke_privileges;
DROP ROLE test_revoke_user;
DROP TABLE test_grant_option;
DROP ROLE test_sub_grantee;
DROP ROLE test_grantor;
DROP TABLE test_all_privileges;
DROP ROLE test_full_access;
DROP TABLE test_multiple_privileges;
DROP ROLE test_writer;
DROP TABLE test_basic_privileges;
DROP ROLE test_reader;
DROP ROLE test_default_revoke_user;

DROP DATABASE test_privilege_grants_db;

-- End of privilege and grant tests
