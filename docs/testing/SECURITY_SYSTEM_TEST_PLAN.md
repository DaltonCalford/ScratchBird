# ScratchBird Security System Test Plan
## Comprehensive Testing Strategy for Phase 2

### Document Information
- **Version:** 1.0
- **Date:** November 10, 2025
- **Phase:** Security System Phase 2
- **Status:** Test Plan Ready for Implementation

---

## Table of Contents
1. [Overview](#overview)
2. [Test Categories](#test-categories)
3. [Unit Tests](#unit-tests)
4. [Integration Tests](#integration-tests)
5. [Security Tests](#security-tests)
6. [Performance Tests](#performance-tests)
7. [Test Data](#test-data)
8. [Expected Results](#expected-results)
9. [Test Execution](#test-execution)

---

## Overview

### Testing Objectives
1. **Verify Correctness:** All security SQL statements execute correctly
2. **Verify Security:** Permission checks properly deny unauthorized access
3. **Verify Integration:** Security system integrates correctly with catalog manager
4. **Verify Performance:** Permission checks don't significantly impact performance
5. **Verify Error Handling:** Appropriate error messages for all failure cases

### Test Coverage Goals
- **Parser Layer:** 100% coverage of all 13 security statement types
- **Bytecode Layer:** 100% coverage of all 13 opcodes
- **Executor Layer:** 100% coverage of all 13 executor functions
- **Permission Checks:** 100% coverage of all 7 DML/DDL operations
- **Error Paths:** 100% coverage of all error conditions

### Testing Tools
- **Unit Tests:** Google Test (gtest) framework
- **Integration Tests:** SQL script-based testing
- **Performance Tests:** Custom benchmarking harness
- **Security Tests:** Penetration testing scenarios

---

## Test Categories

### 1. Unit Tests (Parser, Bytecode, Executor)
- Test each component in isolation
- Mock dependencies
- Fast execution (< 1 second total)

### 2. Integration Tests (End-to-End)
- Test complete SQL statement execution
- Real database and catalog manager
- Verify actual behavior

### 3. Security Tests (Permission Enforcement)
- Test permission denial scenarios
- Test privilege escalation
- Test superuser bypass

### 4. Performance Tests (Overhead Measurement)
- Measure permission check overhead
- Compare with/without security
- Identify bottlenecks

### 5. Regression Tests
- Ensure changes don't break existing functionality
- Test backward compatibility
- Verify no performance degradation

---

## Unit Tests

### Parser Tests

#### Test Suite: SecurityParserTest

**Test Cases:**

```cpp
TEST(SecurityParserTest, ParseCreateUser_BasicSyntax) {
    // Test: CREATE USER alice WITH PASSWORD 'secret';
    // Verify: AST node created with correct fields
}

TEST(SecurityParserTest, ParseCreateUser_Superuser) {
    // Test: CREATE USER admin WITH PASSWORD 'pass' SUPERUSER;
    // Verify: is_superuser flag set correctly
}

TEST(SecurityParserTest, ParseCreateUser_NoPassword) {
    // Test: CREATE USER service;
    // Verify: has_password = false
}

TEST(SecurityParserTest, ParseAlterUser_Password) {
    // Test: ALTER USER alice WITH PASSWORD 'newpass';
    // Verify: change_password = true, new password captured
}

TEST(SecurityParserTest, ParseAlterUser_SuperuserFlag) {
    // Test: ALTER USER alice WITH SUPERUSER;
    // Verify: change_superuser = true, is_superuser = true
}

TEST(SecurityParserTest, ParseDropUser_Basic) {
    // Test: DROP USER alice;
    // Verify: username captured, if_exists = false, cascade = false
}

TEST(SecurityParserTest, ParseDropUser_IfExists) {
    // Test: DROP USER IF EXISTS alice;
    // Verify: if_exists = true
}

TEST(SecurityParserTest, ParseDropUser_Cascade) {
    // Test: DROP USER alice CASCADE;
    // Verify: cascade = true
}

TEST(SecurityParserTest, ParseCreateRole) {
    // Test: CREATE ROLE admin;
    // Verify: role name captured
}

TEST(SecurityParserTest, ParseDropRole) {
    // Test: DROP ROLE admin CASCADE;
    // Verify: role name, cascade flag
}

TEST(SecurityParserTest, ParseGrantRole) {
    // Test: GRANT admin TO alice;
    // Verify: role name, user name
}

TEST(SecurityParserTest, ParseRevokeRole) {
    // Test: REVOKE admin FROM alice CASCADE;
    // Verify: role name, user name, cascade
}

TEST(SecurityParserTest, ParseGrantPrivilege_Single) {
    // Test: GRANT SELECT ON TABLE users TO alice;
    // Verify: privilege, object type, object name, grantee
}

TEST(SecurityParserTest, ParseGrantPrivilege_Multiple) {
    // Test: GRANT SELECT, INSERT, UPDATE ON TABLE users TO alice;
    // Verify: multiple privileges captured
}

TEST(SecurityParserTest, ParseGrantPrivilege_All) {
    // Test: GRANT ALL ON TABLE users TO alice;
    // Verify: ALL privilege
}

TEST(SecurityParserTest, ParseGrantPrivilege_WithGrantOption) {
    // Test: GRANT SELECT ON TABLE users TO alice WITH GRANT OPTION;
    // Verify: with_grant_option = true
}

TEST(SecurityParserTest, ParseGrantPrivilege_ToPublic) {
    // Test: GRANT SELECT ON TABLE users TO PUBLIC;
    // Verify: grantee_type = PUBLIC
}

TEST(SecurityParserTest, ParseRevokePrivilege) {
    // Test: REVOKE SELECT ON TABLE users FROM alice;
    // Verify: privilege, object, grantee
}

TEST(SecurityParserTest, ParseRevokePrivilege_Cascade) {
    // Test: REVOKE SELECT ON TABLE users FROM alice CASCADE;
    // Verify: cascade = true
}

TEST(SecurityParserTest, ParseSetRole) {
    // Test: SET ROLE admin;
    // Verify: role name, is_reset = false
}

TEST(SecurityParserTest, ParseResetRole) {
    // Test: RESET ROLE;
    // Verify: is_reset = true
}

TEST(SecurityParserTest, ParseSetSessionAuthorization) {
    // Test: SET SESSION AUTHORIZATION alice;
    // Verify: username, is_reset = false
}

TEST(SecurityParserTest, ParseResetSessionAuthorization) {
    // Test: RESET SESSION AUTHORIZATION;
    // Verify: is_reset = true
}

// Error handling tests
TEST(SecurityParserTest, ParseCreateUser_MissingPassword) {
    // Test: CREATE USER alice WITH PASSWORD;
    // Verify: Syntax error
}

TEST(SecurityParserTest, ParseGrantPrivilege_InvalidPrivilege) {
    // Test: GRANT INVALID ON TABLE users TO alice;
    // Verify: Syntax error
}
```

**Total Parser Tests:** ~30 test cases

### Bytecode Tests

#### Test Suite: SecurityBytecodeTest

**Test Cases:**

```cpp
TEST(SecurityBytecodeTest, GenerateCreateUserBytecode) {
    // Parse: CREATE USER alice WITH PASSWORD 'secret' SUPERUSER;
    // Generate bytecode
    // Verify: Correct opcode, flags, string encoding
}

TEST(SecurityBytecodeTest, GenerateAlterUserBytecode) {
    // Parse: ALTER USER alice WITH PASSWORD 'new' NOSUPERUSER;
    // Verify: Flags correctly encode changes
}

TEST(SecurityBytecodeTest, GenerateDropUserBytecode) {
    // Parse: DROP USER alice IF EXISTS CASCADE;
    // Verify: Flags encode if_exists and cascade
}

TEST(SecurityBytecodeTest, GenerateGrantPrivilegeBytecode_Single) {
    // Parse: GRANT SELECT ON TABLE users TO alice;
    // Verify: Privilege bitmask, object type, grantee type
}

TEST(SecurityBytecodeTest, GenerateGrantPrivilegeBytecode_Multiple) {
    // Parse: GRANT SELECT, INSERT ON TABLE users TO alice;
    // Verify: Bitmask combines privileges correctly
}

TEST(SecurityBytecodeTest, BytecodeRoundtrip_CreateUser) {
    // Parse → Generate → Execute
    // Verify: Information preserved through pipeline
}

TEST(SecurityBytecodeTest, BytecodeSizeOptimization) {
    // Measure bytecode sizes
    // Verify: Within expected ranges (< 20 bytes per statement)
}
```

**Total Bytecode Tests:** ~15 test cases

### Executor Tests

#### Test Suite: SecurityExecutorTest

**Test Cases:**

```cpp
TEST(SecurityExecutorTest, ExecuteCreateUser_Basic) {
    // Execute: CREATE USER alice WITH PASSWORD 'secret';
    // Verify: User exists in catalog with hashed password
}

TEST(SecurityExecutorTest, ExecuteCreateUser_Superuser) {
    // Execute: CREATE USER admin WITH PASSWORD 'pass' SUPERUSER;
    // Verify: User created with superuser flag
}

TEST(SecurityExecutorTest, ExecuteAlterUser_Password) {
    // Setup: Create user alice
    // Execute: ALTER USER alice WITH PASSWORD 'new';
    // Verify: Password changed in catalog
}

TEST(SecurityExecutorTest, ExecuteAlterUser_SuperuserFlag) {
    // Setup: Create user alice (not superuser)
    // Execute: ALTER USER alice WITH SUPERUSER;
    // Verify: Superuser flag updated
    // Note: May fail due to API limitation (TODO in code)
}

TEST(SecurityExecutorTest, ExecuteDropUser_Basic) {
    // Setup: Create user alice
    // Execute: DROP USER alice;
    // Verify: User removed from catalog
}

TEST(SecurityExecutorTest, ExecuteDropUser_IfExists_UserExists) {
    // Setup: Create user alice
    // Execute: DROP USER IF EXISTS alice;
    // Verify: User removed, no error
}

TEST(SecurityExecutorTest, ExecuteDropUser_IfExists_UserMissing) {
    // Execute: DROP USER IF EXISTS nonexistent;
    // Verify: No error, silent success
}

TEST(SecurityExecutorTest, ExecuteDropUser_WithoutIfExists_UserMissing) {
    // Execute: DROP USER nonexistent;
    // Verify: Error "User 'nonexistent' not found"
}

TEST(SecurityExecutorTest, ExecuteCreateRole) {
    // Execute: CREATE ROLE admin;
    // Verify: Role exists in catalog
}

TEST(SecurityExecutorTest, ExecuteDropRole) {
    // Setup: Create role admin
    // Execute: DROP ROLE admin;
    // Verify: Role removed from catalog
}

TEST(SecurityExecutorTest, ExecuteGrantRole) {
    // Setup: Create user alice, create role admin
    // Execute: GRANT admin TO alice;
    // Verify: Role membership recorded in catalog
}

TEST(SecurityExecutorTest, ExecuteRevokeRole) {
    // Setup: Create user alice, role admin, grant role
    // Execute: REVOKE admin FROM alice;
    // Verify: Role membership removed
}

TEST(SecurityExecutorTest, ExecuteCreateGroup) {
    // Execute: CREATE GROUP developers;
    // Verify: Group exists with type LOCAL
}

TEST(SecurityExecutorTest, ExecuteDropGroup) {
    // Setup: Create group developers
    // Execute: DROP GROUP developers;
    // Verify: Group removed
}

TEST(SecurityExecutorTest, ExecuteGrantPrivilege_Table_User) {
    // Setup: Create user alice, table users
    // Execute: GRANT SELECT ON TABLE users TO alice;
    // Verify: Permission recorded in catalog
}

TEST(SecurityExecutorTest, ExecuteGrantPrivilege_MultiplePrivileges) {
    // Execute: GRANT SELECT, INSERT, UPDATE ON TABLE users TO alice;
    // Verify: All three privileges granted
}

TEST(SecurityExecutorTest, ExecuteGrantPrivilege_ToRole) {
    // Setup: Create role readonly
    // Execute: GRANT SELECT ON TABLE users TO readonly;
    // Verify: Permission granted to role
}

TEST(SecurityExecutorTest, ExecuteGrantPrivilege_ToPublic) {
    // Execute: GRANT SELECT ON TABLE users TO PUBLIC;
    // Verify: Permission granted to PUBLIC
}

TEST(SecurityExecutorTest, ExecuteRevokePrivilege) {
    // Setup: Grant SELECT to alice
    // Execute: REVOKE SELECT ON TABLE users FROM alice;
    // Verify: Permission removed
}

TEST(SecurityExecutorTest, ExecuteSetRole) {
    // Setup: Create role admin, grant to current user
    // Execute: SET ROLE admin;
    // Verify: Active role set in session (when implemented)
}

TEST(SecurityExecutorTest, ExecuteResetRole) {
    // Setup: SET ROLE admin
    // Execute: RESET ROLE;
    // Verify: Active role cleared
}

TEST(SecurityExecutorTest, ExecuteSetSessionAuthorization) {
    // Setup: Create user alice, current user is superuser
    // Execute: SET SESSION AUTHORIZATION alice;
    // Verify: Effective user changed to alice
}

TEST(SecurityExecutorTest, ExecuteResetSessionAuthorization) {
    // Setup: SET SESSION AUTHORIZATION alice
    // Execute: RESET SESSION AUTHORIZATION;
    // Verify: Effective user restored to connection user
}
```

**Total Executor Tests:** ~30 test cases

### Permission Check Tests

#### Test Suite: PermissionCheckTest

**Test Cases:**

```cpp
TEST(PermissionCheckTest, CheckPermission_Placeholder) {
    // Call checkPermission() directly
    // Verify: Returns true (placeholder implementation)
}

TEST(PermissionCheckTest, ExecuteSelect_WithPermission) {
    // Setup: Grant SELECT to current user
    // Execute: SELECT * FROM users;
    // Verify: Query succeeds
}

TEST(PermissionCheckTest, ExecuteSelect_WithoutPermission) {
    // Setup: No SELECT privilege
    // Execute: SELECT * FROM restricted;
    // Verify: Error "Permission denied: SELECT on table restricted"
    // Note: Will pass until connection context integrated
}

TEST(PermissionCheckTest, ExecuteInsert_WithPermission) {
    // Setup: Grant INSERT to current user
    // Execute: INSERT INTO users VALUES (...);
    // Verify: Insert succeeds
}

TEST(PermissionCheckTest, ExecuteInsert_WithoutPermission) {
    // Setup: No INSERT privilege
    // Execute: INSERT INTO restricted VALUES (...);
    // Verify: Error "Permission denied: INSERT on table restricted"
}

TEST(PermissionCheckTest, ExecuteUpdate_WithPermission) {
    // Execute: UPDATE users SET ... WHERE ...;
    // Verify: Update succeeds
}

TEST(PermissionCheckTest, ExecuteDelete_WithPermission) {
    // Execute: DELETE FROM users WHERE ...;
    // Verify: Delete succeeds
}

TEST(PermissionCheckTest, ExecuteCreateTable_WithPermission) {
    // Setup: Grant CREATE on schema
    // Execute: CREATE TABLE new_table (...);
    // Verify: Table created
}

TEST(PermissionCheckTest, ExecuteDropTable_WithPermission) {
    // Setup: User owns table or has permission
    // Execute: DROP TABLE old_table;
    // Verify: Table dropped
}

TEST(PermissionCheckTest, ExecuteAlterTable_WithPermission) {
    // Execute: ALTER TABLE users ADD COLUMN ...;
    // Verify: Column added
}
```

**Total Permission Tests:** ~15 test cases

---

## Integration Tests

### End-to-End Workflow Tests

#### Test 1: Complete User Lifecycle

```sql
-- Test ID: INT_USER_001
-- Description: Create, modify, use, and delete user

-- Step 1: Create user
CREATE USER testuser WITH PASSWORD 'testpass';

-- Step 2: Verify user exists (query catalog)
-- Expected: User exists, password hashed

-- Step 3: Alter user password
ALTER USER testuser WITH PASSWORD 'newpass';

-- Step 4: Verify password changed
-- Expected: Password hash changed

-- Step 5: Alter user to superuser
ALTER USER testuser WITH SUPERUSER;

-- Step 6: Verify superuser flag
-- Expected: is_superuser = true

-- Step 7: Drop user
DROP USER testuser;

-- Step 8: Verify user deleted
-- Expected: User not found

-- Cleanup: None needed
```

#### Test 2: Role-Based Access Control

```sql
-- Test ID: INT_RBAC_001
-- Description: Complete RBAC workflow

-- Step 1: Create tables
CREATE TABLE sensitive_data (id INT, value VARCHAR(100));
CREATE TABLE public_data (id INT, value VARCHAR(100));

-- Step 2: Create users
CREATE USER alice WITH PASSWORD 'alice_pass';
CREATE USER bob WITH PASSWORD 'bob_pass';

-- Step 3: Create roles
CREATE ROLE data_reader;
CREATE ROLE data_writer;

-- Step 4: Grant privileges to roles
GRANT SELECT ON TABLE sensitive_data TO data_reader;
GRANT SELECT ON TABLE public_data TO data_reader;
GRANT SELECT, INSERT, UPDATE, DELETE ON TABLE sensitive_data TO data_writer;
GRANT SELECT, INSERT, UPDATE, DELETE ON TABLE public_data TO data_writer;

-- Step 5: Grant roles to users
GRANT data_reader TO alice;
GRANT data_writer TO bob;

-- Step 6: Test alice (reader) access
-- As alice:
SELECT * FROM sensitive_data;  -- Should succeed
SELECT * FROM public_data;     -- Should succeed
INSERT INTO sensitive_data VALUES (1, 'test');  -- Should fail

-- Step 7: Test bob (writer) access
-- As bob:
SELECT * FROM sensitive_data;  -- Should succeed
INSERT INTO sensitive_data VALUES (1, 'test');  -- Should succeed
UPDATE sensitive_data SET value = 'changed' WHERE id = 1;  -- Should succeed
DELETE FROM sensitive_data WHERE id = 1;  -- Should succeed

-- Step 8: Revoke role from alice
REVOKE data_reader FROM alice;

-- Step 9: Test alice access after revocation
-- As alice:
SELECT * FROM sensitive_data;  -- Should fail

-- Cleanup
DROP USER alice;
DROP USER bob;
DROP ROLE data_reader;
DROP ROLE data_writer;
DROP TABLE sensitive_data;
DROP TABLE public_data;
```

#### Test 3: Privilege Cascading

```sql
-- Test ID: INT_CASCADE_001
-- Description: Test CASCADE behavior

-- Step 1: Create users
CREATE USER owner WITH PASSWORD 'pass';
CREATE USER user1 WITH PASSWORD 'pass';
CREATE USER user2 WITH PASSWORD 'pass';

-- Step 2: Grant privilege with GRANT OPTION
-- As owner:
GRANT SELECT ON TABLE data TO user1 WITH GRANT OPTION;

-- Step 3: User1 grants to user2
-- As user1:
GRANT SELECT ON TABLE data TO user2;

-- Step 4: Verify user2 has access
-- As user2:
SELECT * FROM data;  -- Should succeed

-- Step 5: Revoke from user1 with CASCADE
-- As owner:
REVOKE SELECT ON TABLE data FROM user1 CASCADE;

-- Step 6: Verify user2 lost access (transitive revoke)
-- As user2:
SELECT * FROM data;  -- Should fail

-- Cleanup
DROP USER owner CASCADE;
DROP USER user1 CASCADE;
DROP USER user2 CASCADE;
```

#### Test 4: Session Management

```sql
-- Test ID: INT_SESSION_001
-- Description: Test SET ROLE and SET SESSION AUTHORIZATION

-- Step 1: Create role with privileges
CREATE ROLE elevated;
GRANT ALL ON TABLE restricted TO elevated;

-- Step 2: Create user with no privileges
CREATE USER lowpriv WITH PASSWORD 'pass';
GRANT elevated TO lowpriv;

-- Step 3: As lowpriv, test without role
-- As lowpriv:
SELECT * FROM restricted;  -- Should fail (no direct privilege)

-- Step 4: Set role and test
SET ROLE elevated;
SELECT * FROM restricted;  -- Should succeed (role has privilege)

-- Step 5: Reset role
RESET ROLE;
SELECT * FROM restricted;  -- Should fail again

-- Step 6: As superuser, test SET SESSION AUTHORIZATION
-- As superuser:
SET SESSION AUTHORIZATION lowpriv;
SELECT * FROM restricted;  -- Should fail (acting as lowpriv)

-- Step 7: Reset
RESET SESSION AUTHORIZATION;
SELECT * FROM restricted;  -- Should succeed (back to superuser)

-- Cleanup
DROP USER lowpriv;
DROP ROLE elevated;
```

**Total Integration Tests:** ~20 test scenarios

---

## Security Tests

### Penetration Testing Scenarios

#### Test 1: SQL Injection in Security Statements

```sql
-- Test ID: SEC_INJ_001
-- Description: Attempt SQL injection in usernames

-- Attempt 1: Username with single quote
CREATE USER 'alice'' OR ''1''=''1' WITH PASSWORD 'pass';
-- Expected: Syntax error or username treated as literal

-- Attempt 2: Username with semicolon
CREATE USER 'alice; DROP TABLE users;' WITH PASSWORD 'pass';
-- Expected: Username treated as literal, no command execution

-- Attempt 3: Password with SQL
CREATE USER alice WITH PASSWORD 'pass''; DROP TABLE users; --';
-- Expected: Password treated as literal string
```

#### Test 2: Privilege Escalation Attempts

```sql
-- Test ID: SEC_ESC_001
-- Description: Attempt to escalate privileges

-- Setup
CREATE USER attacker WITH PASSWORD 'pass';
-- Attacker has no privileges

-- Attempt 1: Create superuser (should fail for non-superuser)
-- As attacker:
CREATE USER backdoor WITH PASSWORD 'pass' SUPERUSER;
-- Expected: Error "Permission denied: CREATE USER (superuser only)"

-- Attempt 2: Grant self superuser (should fail)
-- As attacker:
ALTER USER attacker WITH SUPERUSER;
-- Expected: Error "Permission denied: ALTER USER (superuser only)"

-- Attempt 3: Grant privileges without permission
-- As attacker:
GRANT ALL ON TABLE sensitive TO attacker;
-- Expected: Error "Permission denied: GRANT (owner or superuser only)"
```

#### Test 3: Authentication Bypass Attempts

```sql
-- Test ID: SEC_AUTH_001
-- Description: Attempt to bypass authentication

-- Attempt 1: Empty password
CREATE USER nopass WITH PASSWORD '';
-- Expected: User created but can't login with empty password

-- Attempt 2: NULL password
CREATE USER nullpass;
-- Expected: User created, no password authentication possible

-- Attempt 3: Very long password (buffer overflow test)
CREATE USER longpass WITH PASSWORD '<10000 character string>';
-- Expected: Password truncated or rejected
```

#### Test 4: Permission Denial Verification

```sql
-- Test ID: SEC_DENY_001
-- Description: Verify all permission denials work

-- Test each DML operation without privilege
-- Expected: Each should fail with "Permission denied" error

SELECT * FROM restricted;   -- No SELECT
INSERT INTO restricted VALUES (...);  -- No INSERT
UPDATE restricted SET ...;  -- No UPDATE
DELETE FROM restricted;     -- No DELETE
TRUNCATE restricted;        -- No TRUNCATE (when implemented)
```

**Total Security Tests:** ~15 test scenarios

---

## Performance Tests

### Benchmark 1: Permission Check Overhead

```cpp
// Measure SELECT performance with/without permission checks

Benchmark: SELECT_WithPermissionCheck
Setup: Table with 10,000 rows
Operation: SELECT * FROM table;
Iterations: 1,000
Measure: Average execution time

Benchmark: SELECT_NoPermissionCheck
Setup: Disable checkPermission() (return true immediately)
Operation: SELECT * FROM table;
Iterations: 1,000
Measure: Average execution time

Compare: Overhead = WithCheck - NoCheck
Target: < 5% overhead
```

### Benchmark 2: Role Lookup Performance

```cpp
Benchmark: RolePrivilegeCheck
Setup: User with 10 roles, each role with 50 privileges
Operation: Check permission on object
Iterations: 10,000
Measure: Average lookup time
Target: < 1ms per check
```

### Benchmark 3: Cascading Revoke Performance

```cpp
Benchmark: CascadeRevoke
Setup: Tree of privilege grants (depth 5, fanout 10)
Operation: REVOKE ... CASCADE at root
Measure: Total revocation time
Target: < 100ms for 50 revocations
```

**Total Performance Tests:** ~10 benchmarks

---

## Test Data

### Standard Test Users

```sql
-- Superuser
CREATE USER test_admin WITH PASSWORD 'admin_pass' SUPERUSER;

-- Regular users
CREATE USER test_alice WITH PASSWORD 'alice_pass';
CREATE USER test_bob WITH PASSWORD 'bob_pass';
CREATE USER test_charlie WITH PASSWORD 'charlie_pass';

-- Service account (no password)
CREATE USER test_service;
```

### Standard Test Roles

```sql
CREATE ROLE test_readonly;
CREATE ROLE test_readwrite;
CREATE ROLE test_admin_role;
```

### Standard Test Tables

```sql
CREATE TABLE test_public_data (
    id INT PRIMARY KEY,
    value VARCHAR(100)
);

CREATE TABLE test_restricted_data (
    id INT PRIMARY KEY,
    secret VARCHAR(100)
);

CREATE TABLE test_audit_log (
    id INT PRIMARY KEY,
    timestamp BIGINT,
    action VARCHAR(100),
    username VARCHAR(100)
);
```

---

## Expected Results

### Success Criteria

**Parser Tests:**
- ✅ All 30 parser tests pass
- ✅ All security SQL syntaxes correctly parsed
- ✅ All error cases properly detected

**Bytecode Tests:**
- ✅ All 15 bytecode tests pass
- ✅ Bytecode correctly encodes all information
- ✅ Bytecode size within expected ranges

**Executor Tests:**
- ✅ All 30 executor tests pass
- ✅ All operations correctly modify catalog
- ✅ All error conditions properly handled

**Integration Tests:**
- ✅ All 20 integration tests pass
- ✅ End-to-end workflows work correctly
- ✅ Permission enforcement verified

**Security Tests:**
- ✅ All 15 security tests pass
- ✅ No SQL injection vulnerabilities
- ✅ No privilege escalation possible
- ✅ All permission denials work

**Performance Tests:**
- ✅ Permission check overhead < 5%
- ✅ Role lookup < 1ms per check
- ✅ CASCADE operations complete in reasonable time

---

## Test Execution

### Phase 1: Unit Tests (Week 1)
1. Implement parser tests
2. Implement bytecode tests
3. Implement executor tests
4. Run and fix failures
5. Achieve 100% test pass rate

### Phase 2: Integration Tests (Week 2)
1. Set up test database
2. Implement integration test scripts
3. Run end-to-end scenarios
4. Fix discovered issues
5. Document test results

### Phase 3: Security Tests (Week 3)
1. Implement security test scenarios
2. Run penetration tests
3. Fix vulnerabilities
4. Re-test until all pass
5. Security audit

### Phase 4: Performance Tests (Week 4)
1. Implement benchmarks
2. Measure baseline performance
3. Optimize hotspots
4. Re-measure
5. Document performance characteristics

### Continuous Integration
- Run unit tests on every commit
- Run integration tests nightly
- Run security tests weekly
- Run performance tests before releases

---

## Test Automation

### Test Runner Script

```bash
#!/bin/bash
# run_security_tests.sh

echo "Running ScratchBird Security System Tests..."

# Unit tests
echo "1. Running unit tests..."
./build/scratchbird_tests --gtest_filter=Security*
UNIT_RESULT=$?

# Integration tests
echo "2. Running integration tests..."
./scripts/run_integration_tests.sh
INTEGRATION_RESULT=$?

# Security tests
echo "3. Running security tests..."
./scripts/run_security_tests.sh
SECURITY_RESULT=$?

# Report results
echo ""
echo "========== TEST RESULTS =========="
echo "Unit Tests: $([ $UNIT_RESULT -eq 0 ] && echo 'PASS' || echo 'FAIL')"
echo "Integration Tests: $([ $INTEGRATION_RESULT -eq 0 ] && echo 'PASS' || echo 'FAIL')"
echo "Security Tests: $([ $SECURITY_RESULT -eq 0 ] && echo 'PASS' || echo 'FAIL')"
echo "=================================="

# Exit with failure if any test failed
[ $UNIT_RESULT -eq 0 ] && [ $INTEGRATION_RESULT -eq 0 ] && [ $SECURITY_RESULT -eq 0 ]
exit $?
```

---

## Conclusion

This comprehensive test plan covers all aspects of the security system:
- **90+ individual test cases** across all layers
- **~20 integration scenarios** for end-to-end verification
- **~15 security tests** for penetration testing
- **~10 performance benchmarks** for optimization

**Estimated Testing Effort:** 40-60 hours total
**Target Completion:** 4 weeks (Phase 1-4)
**Success Criteria:** All tests passing with < 5% performance overhead

---

**Document Version:** 1.0
**Last Updated:** November 10, 2025
**Next Review:** After connection context integration
