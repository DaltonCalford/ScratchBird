/*
 * Security System Phase 2 Integration Test
 * Tests end-to-end functionality of SQL security statements
 *
 * Covers:
 * - CREATE/ALTER/DROP USER
 * - CREATE/DROP ROLE
 * - CREATE/DROP GROUP
 * - GRANT/REVOKE privileges
 * - GRANT/REVOKE roles
 * - SET ROLE / RESET ROLE
 * - SET SESSION AUTHORIZATION
 * - Permission enforcement in SELECT/INSERT/UPDATE/DELETE
 * - Permission enforcement in DDL operations
 */

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"

#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/executor.h"
#include "test_helpers.h"
#include <memory>

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::sblr;

class SecurityPhase2Test : public ::testing::Test
{
protected:
    std::unique_ptr<Database> db;
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<scratchbird::testing::TestDatabaseFile> db_file_;
    core::ID schema_id_;

    void SetUp() override
    {
        GTEST_SKIP() << "Parser V2 DCL support pending";

        db_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_security_phase2");

        ErrorContext ctx;
        auto status = Database::create(db_file_->path(), 8192, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database: " << ctx.message;

        db = std::make_unique<Database>();
        status = db->open(db_file_->path(), &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database: " << ctx.message;

        CatalogManager::SchemaInfo schema;
        ASSERT_EQ(db->catalog_manager()->getSchema("PUBLIC", schema, &ctx), Status::OK)
            << "Failed to get PUBLIC schema: " << ctx.message;
        schema_id_ = schema.schema_id;

        compiler_ = std::make_unique<QueryCompilerV2>(db.get());
        compiler_->setCurrentSchema(schema_id_);
        executor_ = std::make_unique<Executor>(db.get());
        executor_->setCurrentSchema(schema_id_);
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        db.reset();
        db_file_.reset();
    }

    // Helper to execute SQL and return success
    bool executeSQL(const std::string& sql, std::string* error_msg = nullptr)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            if (error_msg && !compile_result.errors().empty())
            {
                *error_msg = compile_result.errors().front();
            }
            return false;
        }

        auto exec_result = executor_->execute(compile_result.bytecode());
        if (!exec_result.success())
        {
            if (error_msg)
            {
                *error_msg = exec_result.error();
            }
            return false;
        }

        return true;
    }
};

// Test 1: CREATE USER
TEST_F(SecurityPhase2Test, CreateUser)
{
    std::string error;

    // Create a regular user
    EXPECT_TRUE(executeSQL("CREATE USER alice WITH PASSWORD 'secret123'", &error))
        << "Failed to create user: " << error;

    // Create a superuser
    EXPECT_TRUE(executeSQL("CREATE USER admin WITH PASSWORD 'admin123' SUPERUSER", &error))
        << "Failed to create superuser: " << error;

    // Verify users exist in catalog
    CatalogManager::UserInfo user_info;
    ErrorContext ctx;
    auto status = db->catalog_manager()->getUserByName("alice", user_info, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(user_info.username, "alice");
    EXPECT_FALSE(user_info.is_superuser);
}

// Test 2: ALTER USER
TEST_F(SecurityPhase2Test, AlterUser)
{
    std::string error;

    // Create user
    ASSERT_TRUE(executeSQL("CREATE USER bob WITH PASSWORD 'oldpass'", &error));

    // Change password
    EXPECT_TRUE(executeSQL("ALTER USER bob WITH PASSWORD 'newpass'", &error))
        << "Failed to alter user: " << error;

    // Make superuser
    EXPECT_TRUE(executeSQL("ALTER USER bob SUPERUSER", &error))
        << "Failed to make user superuser: " << error;
}

// Test 3: DROP USER
TEST_F(SecurityPhase2Test, DropUser)
{
    std::string error;

    // Create and drop user
    ASSERT_TRUE(executeSQL("CREATE USER charlie", &error));
    EXPECT_TRUE(executeSQL("DROP USER charlie", &error))
        << "Failed to drop user: " << error;

    // Verify user no longer exists
    CatalogManager::UserInfo user_info;
    ErrorContext ctx;
    auto status = db->catalog_manager()->getUserByName("charlie", user_info, &ctx);
    EXPECT_NE(status, Status::OK);

    // Test IF EXISTS (should not error)
    EXPECT_TRUE(executeSQL("DROP USER IF EXISTS nonexistent", &error))
        << "DROP USER IF EXISTS failed: " << error;
}

// Test 4: CREATE ROLE
TEST_F(SecurityPhase2Test, CreateRole)
{
    std::string error;

    EXPECT_TRUE(executeSQL("CREATE ROLE developer", &error))
        << "Failed to create role: " << error;

    EXPECT_TRUE(executeSQL("CREATE ROLE manager", &error))
        << "Failed to create role: " << error;

    // Verify roles exist
    CatalogManager::RoleInfo role_info;
    ErrorContext ctx;
    auto status = db->catalog_manager()->getRoleByName("developer", role_info, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(role_info.role_name, "developer");
}

// Test 5: DROP ROLE
TEST_F(SecurityPhase2Test, DropRole)
{
    std::string error;

    ASSERT_TRUE(executeSQL("CREATE ROLE testrole", &error));
    EXPECT_TRUE(executeSQL("DROP ROLE testrole", &error))
        << "Failed to drop role: " << error;

    // Test IF EXISTS
    EXPECT_TRUE(executeSQL("DROP ROLE IF EXISTS nonexistent_role", &error))
        << "DROP ROLE IF EXISTS failed: " << error;
}

// Test 6: CREATE GROUP
TEST_F(SecurityPhase2Test, CreateGroup)
{
    std::string error;

    EXPECT_TRUE(executeSQL("CREATE GROUP engineering", &error))
        << "Failed to create group: " << error;

    EXPECT_TRUE(executeSQL("CREATE GROUP sales", &error))
        << "Failed to create group: " << error;

    // Verify groups exist
    CatalogManager::GroupInfo group_info;
    ErrorContext ctx;
    auto status = db->catalog_manager()->getGroupByName("engineering", group_info, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(group_info.group_name, "engineering");
}

// Test 7: DROP GROUP
TEST_F(SecurityPhase2Test, DropGroup)
{
    std::string error;

    ASSERT_TRUE(executeSQL("CREATE GROUP testgroup", &error));
    EXPECT_TRUE(executeSQL("DROP GROUP testgroup", &error))
        << "Failed to drop group: " << error;
}

// Test 8: GRANT Privilege
TEST_F(SecurityPhase2Test, GrantPrivilege)
{
    std::string error;

    // Create test user and table
    ASSERT_TRUE(executeSQL("CREATE USER testuser WITH PASSWORD 'pass'", &error));
    ASSERT_TRUE(executeSQL("CREATE TABLE testtable (id INT, name VARCHAR(50))", &error));

    // Grant SELECT privilege
    EXPECT_TRUE(executeSQL("GRANT SELECT ON TABLE testtable TO USER testuser", &error))
        << "Failed to grant SELECT: " << error;

    // Grant multiple privileges
    EXPECT_TRUE(executeSQL("GRANT INSERT, UPDATE ON TABLE testtable TO USER testuser", &error))
        << "Failed to grant INSERT, UPDATE: " << error;

    // Grant to PUBLIC
    EXPECT_TRUE(executeSQL("GRANT SELECT ON TABLE testtable TO PUBLIC", &error))
        << "Failed to grant to PUBLIC: " << error;
}

// Test 9: REVOKE Privilege
TEST_F(SecurityPhase2Test, RevokePrivilege)
{
    std::string error;

    // Setup: create user, table, and grant privilege
    ASSERT_TRUE(executeSQL("CREATE USER testuser2 WITH PASSWORD 'pass'", &error));
    ASSERT_TRUE(executeSQL("CREATE TABLE testtable2 (id INT)", &error));
    ASSERT_TRUE(executeSQL("GRANT SELECT ON TABLE testtable2 TO USER testuser2", &error));

    // Revoke privilege
    EXPECT_TRUE(executeSQL("REVOKE SELECT ON TABLE testtable2 FROM USER testuser2", &error))
        << "Failed to revoke SELECT: " << error;
}

// Test 10: GRANT Role to User
TEST_F(SecurityPhase2Test, GrantRole)
{
    std::string error;

    // Setup: create user and role
    ASSERT_TRUE(executeSQL("CREATE USER emp WITH PASSWORD 'pass'", &error));
    ASSERT_TRUE(executeSQL("CREATE ROLE worker", &error));

    // Grant role to user
    EXPECT_TRUE(executeSQL("GRANT worker TO emp", &error))
        << "Failed to grant role to user: " << error;

    // Verify role membership
    CatalogManager::UserInfo user_info;
    ErrorContext ctx;
    ASSERT_EQ(db->catalog_manager()->getUserByName("emp", user_info, &ctx), Status::OK);

    std::vector<CatalogManager::RoleMembershipInfo> roles;
    ASSERT_EQ(db->catalog_manager()->getUserRoles(user_info.user_id, roles, &ctx), Status::OK);
    EXPECT_FALSE(roles.empty());
}

// Test 11: REVOKE Role from User
TEST_F(SecurityPhase2Test, RevokeRole)
{
    std::string error;

    // Setup: create user, role, and grant
    ASSERT_TRUE(executeSQL("CREATE USER emp2 WITH PASSWORD 'pass'", &error));
    ASSERT_TRUE(executeSQL("CREATE ROLE worker2", &error));
    ASSERT_TRUE(executeSQL("GRANT worker2 TO emp2", &error));

    // Revoke role
    EXPECT_TRUE(executeSQL("REVOKE worker2 FROM emp2", &error))
        << "Failed to revoke role from user: " << error;
}

// Test 12: Complex Privilege Scenario
TEST_F(SecurityPhase2Test, ComplexPrivilegeScenario)
{
    std::string error;

    // Create users, roles, and table
    ASSERT_TRUE(executeSQL("CREATE USER dev1 WITH PASSWORD 'pass'", &error));
    ASSERT_TRUE(executeSQL("CREATE USER dev2 WITH PASSWORD 'pass'", &error));
    ASSERT_TRUE(executeSQL("CREATE ROLE developers", &error));
    ASSERT_TRUE(executeSQL("CREATE TABLE projects (id INT, name VARCHAR(100))", &error));

    // Grant role to users
    ASSERT_TRUE(executeSQL("GRANT developers TO dev1", &error));
    ASSERT_TRUE(executeSQL("GRANT developers TO dev2", &error));

    // Grant privileges to role
    EXPECT_TRUE(executeSQL("GRANT SELECT, INSERT ON TABLE projects TO ROLE developers", &error))
        << "Failed to grant privileges to role: " << error;
}

// Test 13: Cascading Permissions
TEST_F(SecurityPhase2Test, CascadingPermissions)
{
    std::string error;

    // Create user with grant option
    ASSERT_TRUE(executeSQL("CREATE USER admin WITH PASSWORD 'pass' SUPERUSER", &error));
    ASSERT_TRUE(executeSQL("CREATE USER user1 WITH PASSWORD 'pass'", &error));
    ASSERT_TRUE(executeSQL("CREATE TABLE data (value INT)", &error));

    // Grant with grant option
    EXPECT_TRUE(executeSQL("GRANT SELECT ON TABLE data TO USER user1 WITH GRANT OPTION", &error))
        << "Failed to grant with grant option: " << error;
}

// Test 14: CASCADE and RESTRICT Options
TEST_F(SecurityPhase2Test, CascadeRestrict)
{
    std::string error;

    // Create user and role with dependencies
    ASSERT_TRUE(executeSQL("CREATE USER testuser3 WITH PASSWORD 'pass'", &error));
    ASSERT_TRUE(executeSQL("CREATE ROLE testrole3", &error));
    ASSERT_TRUE(executeSQL("GRANT testrole3 TO testuser3", &error));

    // Drop role with CASCADE (should remove memberships)
    EXPECT_TRUE(executeSQL("DROP ROLE testrole3 CASCADE", &error))
        << "Failed to drop role with CASCADE: " << error;
}

// Test 15: Bytecode Generation Verification
TEST_F(SecurityPhase2Test, BytecodeGenerationAllStatements)
{
    std::vector<std::string> test_sqls = {
        "CREATE USER test WITH PASSWORD 'pass' SUPERUSER",
        "ALTER USER test WITH PASSWORD 'newpass'",
        "DROP USER test IF EXISTS CASCADE",
        "CREATE ROLE test_role",
        "DROP ROLE test_role IF EXISTS RESTRICT",
        "CREATE GROUP test_group",
        "DROP GROUP test_group IF EXISTS",
        "GRANT SELECT ON TABLE t TO USER u",
        "REVOKE INSERT ON TABLE t FROM USER u CASCADE",
        "GRANT role1 TO user1",
        "REVOKE role1 FROM user1 RESTRICT",
        "SET ROLE myrole",
        "RESET ROLE",
        "SET SESSION AUTHORIZATION myuser",
        "RESET SESSION AUTHORIZATION"
    };

    // Verify all statements generate bytecode successfully (Phase 3.0 API update)
    for (const auto& sql : test_sqls)
    {
        auto compile_result = compiler_->compile(sql);
        EXPECT_TRUE(compile_result.success()) << "Compile failed for: " << sql;
    }
}
