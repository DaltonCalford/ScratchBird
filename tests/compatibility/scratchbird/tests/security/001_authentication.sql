-- ScratchBird Native Test: Authentication & Authorization
-- Test ID: security_001
-- Description: User management, roles, and permissions

-- Connect as SYSDBA
\c test_security SYSDBA;

-- Test User Creation
CREATE USER alice WITH PASSWORD 'password123';
CREATE USER bob WITH PASSWORD 'securepass456';
CREATE USER charlie WITH PASSWORD 'mypass789';

-- List users
SELECT username FROM sb_users WHERE username IN ('alice', 'bob', 'charlie') ORDER BY username;

-- Test Role Creation
CREATE ROLE admin_role;
CREATE ROLE user_role;
CREATE ROLE readonly_role;

-- List roles
SELECT role_name FROM sb_roles WHERE role_name LIKE '%_role' ORDER BY role_name;

-- Grant roles to users
GRANT admin_role TO alice;
GRANT user_role TO bob;
GRANT readonly_role TO charlie;

-- Create test table
CREATE TABLE sensitive_data (
    id INTEGER PRIMARY KEY,
    data VARCHAR(100),
    owner VARCHAR(50)
);

-- Insert test data
INSERT INTO sensitive_data VALUES (1, 'Public Information', 'system');
INSERT INTO sensitive_data VALUES (2, 'Confidential Data', 'system');
INSERT INTO sensitive_data VALUES (3, 'Top Secret', 'system');

-- Test Table Permissions
-- Grant SELECT to readonly_role
GRANT SELECT ON sensitive_data TO readonly_role;

-- Grant SELECT, INSERT, UPDATE to user_role
GRANT SELECT, INSERT, UPDATE ON sensitive_data TO user_role;

-- Grant ALL to admin_role
GRANT ALL ON sensitive_data TO admin_role;

-- Test Column-Level Permissions
CREATE TABLE employee_records (
    emp_id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    salary DECIMAL(10, 2),
    ssn VARCHAR(11)
);

-- Grant SELECT on specific columns only
GRANT SELECT (emp_id, name) ON employee_records TO readonly_role;
GRANT SELECT ON employee_records TO admin_role;

-- Test REVOKE
GRANT DELETE ON sensitive_data TO user_role;
SELECT privilege_type FROM sb_table_privileges
WHERE table_name = 'sensitive_data' AND grantee = 'user_role'
ORDER BY privilege_type;

REVOKE DELETE ON sensitive_data FROM user_role;
SELECT privilege_type FROM sb_table_privileges
WHERE table_name = 'sensitive_data' AND grantee = 'user_role'
ORDER BY privilege_type;

-- Test WITH GRANT OPTION
GRANT SELECT ON sensitive_data TO alice WITH GRANT OPTION;

-- Alice can now grant SELECT to others (tested in alice's session)

-- Test Schema Permissions
CREATE SCHEMA finance;
CREATE TABLE finance.accounts (
    account_id INTEGER PRIMARY KEY,
    account_name VARCHAR(100)
);

GRANT USAGE ON SCHEMA finance TO user_role;
GRANT SELECT ON finance.accounts TO user_role;

-- Test Ownership
-- Only table owner or SYSDBA can ALTER/DROP
CREATE USER dave WITH PASSWORD 'davepass';
GRANT admin_role TO dave;

-- Dave creates a table (dave becomes owner)
-- \c test_security dave;
-- CREATE TABLE dave_table (id INTEGER PRIMARY KEY);

-- Bob cannot drop dave's table (even with admin_role)
-- \c test_security bob;
-- DROP TABLE dave_table; -- Should fail

-- Test Row-Level Security (if supported)
CREATE TABLE department_data (
    dept_id INTEGER,
    dept_name VARCHAR(100),
    manager VARCHAR(50)
);

INSERT INTO department_data VALUES (10, 'Engineering', 'alice');
INSERT INTO department_data VALUES (20, 'Sales', 'bob');
INSERT INTO department_data VALUES (30, 'Marketing', 'charlie');

-- Enable RLS on table
ALTER TABLE department_data ENABLE ROW LEVEL SECURITY;

-- Create policy: users can only see their own department
CREATE POLICY dept_isolation ON department_data
    FOR SELECT
    USING (manager = CURRENT_USER);

GRANT SELECT ON department_data TO alice;
GRANT SELECT ON department_data TO bob;
GRANT SELECT ON department_data TO charlie;

-- Test as alice (should only see dept 10)
-- \c test_security alice;
-- SELECT * FROM department_data; -- Should show only Engineering

-- Test as bob (should only see dept 20)
-- \c test_security bob;
-- SELECT * FROM department_data; -- Should show only Sales

-- Test Password Change
ALTER USER alice WITH PASSWORD 'newpassword123';

-- Test User Disable/Enable
ALTER USER charlie ACCOUNT LOCK;
-- charlie cannot login now

ALTER USER charlie ACCOUNT UNLOCK;
-- charlie can login again

-- Test CASCADE on User Drop
CREATE TABLE alice_data (id INTEGER);
GRANT SELECT ON alice_data TO alice;

-- Drop user with CASCADE
DROP USER alice CASCADE;
-- All alice's objects and grants are removed

-- Verify alice is gone
SELECT COUNT(*) AS alice_count FROM sb_users WHERE username = 'alice';

-- Test Superuser Privileges
-- Only SYSDBA can create users
-- \c test_security bob;
-- CREATE USER eve WITH PASSWORD 'evepass'; -- Should fail

-- Re-connect as SYSDBA
\c test_security SYSDBA;

-- Cleanup
DROP POLICY IF EXISTS dept_isolation ON department_data;
DROP TABLE department_data;
DROP TABLE finance.accounts CASCADE;
DROP SCHEMA finance CASCADE;
DROP TABLE employee_records;
DROP TABLE sensitive_data;
DROP ROLE admin_role;
DROP ROLE user_role;
DROP ROLE readonly_role;
DROP USER bob CASCADE;
DROP USER charlie CASCADE;
DROP USER dave CASCADE;

-- Drop database
DROP DATABASE test_security;
