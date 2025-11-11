/*
 * Security System Phase 3.3 Integration Test
 * Tests end-to-end functionality of column-level permissions
 *
 * Covers:
 * - Column permission CRUD operations (grantColumnPermission, revokeColumnPermission)
 * - hasColumnPermission() checking
 * - getAccessibleColumns() retrieval
 * - Permission cache invalidation
 * - GRANT/REVOKE SQL syntax parsing
 * - Bytecode generation for column-level permissions
 */

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/lexer.h"
#include "scratchbird/parser/ast.h"
#include "scratchbird/parser/semantic_analyzer.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include <filesystem>
#include <memory>

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::parser;
using namespace scratchbird::sblr;

class SecurityPhase3_3Test : public ::testing::Test
{
protected:
    std::unique_ptr<Database> db;
    std::string test_db_path;

    void SetUp() override
    {
        // Create temporary test database
        test_db_path = "/tmp/test_security_phase3_3.db";

        // Remove old test database if exists
        if (std::filesystem::exists(test_db_path))
        {
            std::filesystem::remove_all(test_db_path);
        }

        ErrorContext ctx;
        auto status = Database::create(test_db_path, 8192, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database: " << ctx.message;

        db = std::make_unique<Database>();
        status = db->open(test_db_path, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database: " << ctx.message;
    }

    void TearDown() override
    {
        db.reset();
        if (std::filesystem::exists(test_db_path))
        {
            std::filesystem::remove_all(test_db_path);
        }
    }

    // Helper to get or create a test user
    ID getOrCreateUser(const std::string& username)
    {
        CatalogManager::UserInfo user_info;
        ErrorContext ctx;
        auto status = db->catalog_manager()->getUserByName(username, user_info, &ctx);

        if (status == Status::OK)
        {
            return user_info.user_id;
        }

        // Create user
        ID user_id;
        status = db->catalog_manager()->createUser(username, "test123", false, user_id, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to create user: " << ctx.message;
        return user_id;
    }

    // Helper to get or create a test table
    ID getOrCreateTable(const std::string& table_name)
    {
        // Get public schema
        CatalogManager::SchemaInfo schema_info;
        ErrorContext ctx;
        auto status = db->catalog_manager()->getSchema("public", schema_info, &ctx);
        EXPECT_EQ(status, Status::OK);

        // Check if table exists
        CatalogManager::TableInfo table_info;
        status = db->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info, &ctx);

        if (status == Status::OK)
        {
            return table_info.table_id;
        }

        // Create table
        ID table_id;
        status = db->catalog_manager()->createTable(
            schema_info.schema_id, table_name, ID{}, // owner_id empty for now
            CatalogManager::TableType::ORDINARY, table_id, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to create table: " << ctx.message;
        return table_id;
    }
};

// Test 1: Grant column permission
TEST_F(SecurityPhase3_3Test, GrantColumnPermission)
{
    // Setup
    ID user_id = getOrCreateUser("alice");
    ID table_id = getOrCreateTable("employees");
    ID grantor_id = db->catalog_manager()->getCurrentUserID();

    // Grant SELECT on single column
    ErrorContext ctx;
    auto status = db->catalog_manager()->grantColumnPermission(
        table_id, "salary",
        user_id, CatalogManager::GranteeType::USER,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT),
        false, grantor_id, &ctx);

    ASSERT_EQ(status, Status::OK) << "Failed to grant column permission: " << ctx.message;

    // Verify permission exists
    bool has_perm = false;
    status = db->catalog_manager()->hasColumnPermission(
        user_id, table_id, "salary",
        CatalogManager::Privilege::SELECT,
        has_perm, &ctx);

    ASSERT_EQ(status, Status::OK);
    EXPECT_TRUE(has_perm);
}

// Test 2: Grant multiple column permissions
TEST_F(SecurityPhase3_3Test, GrantMultipleColumnPermissions)
{
    // Setup
    ID user_id = getOrCreateUser("bob");
    ID table_id = getOrCreateTable("employees");
    ID grantor_id = db->catalog_manager()->getCurrentUserID();

    // Grant SELECT on multiple columns
    ErrorContext ctx;
    uint32_t select_priv = static_cast<uint32_t>(CatalogManager::Privilege::SELECT);

    auto status = db->catalog_manager()->grantColumnPermission(
        table_id, "id", user_id, CatalogManager::GranteeType::USER,
        select_priv, false, grantor_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = db->catalog_manager()->grantColumnPermission(
        table_id, "name", user_id, CatalogManager::GranteeType::USER,
        select_priv, false, grantor_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify both permissions exist
    bool has_id = false;
    status = db->catalog_manager()->hasColumnPermission(
        user_id, table_id, "id",
        CatalogManager::Privilege::SELECT, has_id, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_TRUE(has_id);

    bool has_name = false;
    status = db->catalog_manager()->hasColumnPermission(
        user_id, table_id, "name",
        CatalogManager::Privilege::SELECT, has_name, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_TRUE(has_name);

    // Verify other column has no permission
    bool has_salary = false;
    status = db->catalog_manager()->hasColumnPermission(
        user_id, table_id, "salary",
        CatalogManager::Privilege::SELECT, has_salary, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_FALSE(has_salary);
}

// Test 3: Revoke column permission
TEST_F(SecurityPhase3_3Test, RevokeColumnPermission)
{
    // Setup
    ID user_id = getOrCreateUser("charlie");
    ID table_id = getOrCreateTable("employees");
    ID grantor_id = db->catalog_manager()->getCurrentUserID();
    ErrorContext ctx;
    uint32_t select_priv = static_cast<uint32_t>(CatalogManager::Privilege::SELECT);

    // Grant then revoke
    auto status = db->catalog_manager()->grantColumnPermission(
        table_id, "id", user_id, CatalogManager::GranteeType::USER,
        select_priv, false, grantor_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = db->catalog_manager()->grantColumnPermission(
        table_id, "name", user_id, CatalogManager::GranteeType::USER,
        select_priv, false, grantor_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Revoke one column
    status = db->catalog_manager()->revokeColumnPermission(
        table_id, "id", user_id, CatalogManager::GranteeType::USER,
        select_priv, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify id permission removed
    bool has_id = true;
    status = db->catalog_manager()->hasColumnPermission(
        user_id, table_id, "id",
        CatalogManager::Privilege::SELECT, has_id, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_FALSE(has_id);

    // Verify name permission still exists
    bool has_name = false;
    status = db->catalog_manager()->hasColumnPermission(
        user_id, table_id, "name",
        CatalogManager::Privilege::SELECT, has_name, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_TRUE(has_name);
}

// Test 4: getAccessibleColumns returns correct columns
TEST_F(SecurityPhase3_3Test, GetAccessibleColumns)
{
    // Setup
    ID user_id = getOrCreateUser("dave");
    ID table_id = getOrCreateTable("employees");
    ID grantor_id = db->catalog_manager()->getCurrentUserID();
    ErrorContext ctx;
    uint32_t select_priv = static_cast<uint32_t>(CatalogManager::Privilege::SELECT);

    // Grant SELECT on specific columns
    auto status = db->catalog_manager()->grantColumnPermission(
        table_id, "id", user_id, CatalogManager::GranteeType::USER,
        select_priv, false, grantor_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = db->catalog_manager()->grantColumnPermission(
        table_id, "name", user_id, CatalogManager::GranteeType::USER,
        select_priv, false, grantor_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Get accessible columns
    std::vector<std::string> accessible_columns;
    status = db->catalog_manager()->getAccessibleColumns(
        user_id, table_id, CatalogManager::Privilege::SELECT,
        accessible_columns, &ctx);

    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(accessible_columns.size(), 2);
    EXPECT_TRUE(std::find(accessible_columns.begin(), accessible_columns.end(), "id") != accessible_columns.end());
    EXPECT_TRUE(std::find(accessible_columns.begin(), accessible_columns.end(), "name") != accessible_columns.end());
}

// Test 5: Multiple privilege types on same column
TEST_F(SecurityPhase3_3Test, MultiplePrivilegesOnColumn)
{
    // Setup
    ID user_id = getOrCreateUser("eve");
    ID table_id = getOrCreateTable("users");
    ID grantor_id = db->catalog_manager()->getCurrentUserID();
    ErrorContext ctx;

    // Grant both SELECT and UPDATE on email column
    auto status = db->catalog_manager()->grantColumnPermission(
        table_id, "email", user_id, CatalogManager::GranteeType::USER,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT),
        false, grantor_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = db->catalog_manager()->grantColumnPermission(
        table_id, "email", user_id, CatalogManager::GranteeType::USER,
        static_cast<uint32_t>(CatalogManager::Privilege::UPDATE),
        false, grantor_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify both permissions
    bool has_select = false;
    status = db->catalog_manager()->hasColumnPermission(
        user_id, table_id, "email",
        CatalogManager::Privilege::SELECT, has_select, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_TRUE(has_select);

    bool has_update = false;
    status = db->catalog_manager()->hasColumnPermission(
        user_id, table_id, "email",
        CatalogManager::Privilege::UPDATE, has_update, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_TRUE(has_update);
}

// Test 6: SQL parsing - GRANT with single column
TEST_F(SecurityPhase3_3Test, ParseGrantSingleColumn)
{
    std::string sql = "GRANT SELECT (salary) ON TABLE employees TO alice";

    Lexer lexer(sql);
    ASTArena arena;
    Parser parser(lexer, arena);
    auto parse_result = parser.parseStatement();

    ASSERT_TRUE(parse_result.success());
    ASSERT_NE(parse_result.statement(), nullptr);

    auto* grant = dynamic_cast<GrantPrivilegeStmt*>(parse_result.statement());
    ASSERT_NE(grant, nullptr);

    // Check column list
    EXPECT_TRUE(grant->hasColumnList());
    ASSERT_EQ(grant->columnNames().size(), 1);
}

// Test 7: SQL parsing - GRANT with multiple columns
TEST_F(SecurityPhase3_3Test, ParseGrantMultipleColumns)
{
    std::string sql = "GRANT SELECT (id, name, email) ON TABLE users TO bob";

    Lexer lexer(sql);
    ASTArena arena;
    Parser parser(lexer, arena);
    auto parse_result = parser.parseStatement();

    ASSERT_TRUE(parse_result.success());
    ASSERT_NE(parse_result.statement(), nullptr);

    auto* grant = dynamic_cast<GrantPrivilegeStmt*>(parse_result.statement());
    ASSERT_NE(grant, nullptr);

    // Check column list
    EXPECT_TRUE(grant->hasColumnList());
    EXPECT_EQ(grant->columnNames().size(), 3);
}

// Test 8: SQL parsing - REVOKE with columns
TEST_F(SecurityPhase3_3Test, ParseRevokeColumns)
{
    std::string sql = "REVOKE SELECT (salary, bonus) ON TABLE employees FROM alice";

    Lexer lexer(sql);
    ASTArena arena;
    Parser parser(lexer, arena);
    auto parse_result = parser.parseStatement();

    ASSERT_TRUE(parse_result.success());
    ASSERT_NE(parse_result.statement(), nullptr);

    auto* revoke = dynamic_cast<RevokePrivilegeStmt*>(parse_result.statement());
    ASSERT_NE(revoke, nullptr);

    // Check column list
    EXPECT_TRUE(revoke->hasColumnList());
    EXPECT_EQ(revoke->columnNames().size(), 2);
}

// Test 9: Semantic analysis - reject column privileges on non-TABLE
TEST_F(SecurityPhase3_3Test, SemanticRejectColumnOnNonTable)
{
    std::string sql = "GRANT SELECT (id) ON ROLE testrole TO alice";

    Lexer lexer(sql);
    ASTArena arena;
    Parser parser(lexer, arena);
    auto parse_result = parser.parseStatement();

    ASSERT_TRUE(parse_result.success());

    // Semantic analysis should fail
    SemanticAnalyzer analyzer(lexer.stringPool());
    auto semantic_result = analyzer.analyze(parse_result.statement());

    EXPECT_FALSE(semantic_result.success());
}

// Test 10: Semantic analysis - reject invalid privilege types for columns
TEST_F(SecurityPhase3_3Test, SemanticRejectInvalidColumnPrivileges)
{
    std::string sql = "GRANT DELETE (id) ON TABLE test TO alice";

    Lexer lexer(sql);
    ASTArena arena;
    Parser parser(lexer, arena);
    auto parse_result = parser.parseStatement();

    ASSERT_TRUE(parse_result.success());

    // Semantic analysis should fail (DELETE not allowed on columns)
    SemanticAnalyzer analyzer(lexer.stringPool());
    auto semantic_result = analyzer.analyze(parse_result.statement());

    EXPECT_FALSE(semantic_result.success());
}

// Test 11: Bytecode generation - GRANT with columns
TEST_F(SecurityPhase3_3Test, BytecodeGenerationGrantColumns)
{
    std::string sql = "GRANT SELECT (id, name) ON TABLE employees TO alice";

    Lexer lexer(sql);
    ASTArena arena;
    Parser parser(lexer, arena);
    auto parse_result = parser.parseStatement();

    ASSERT_TRUE(parse_result.success());

    // Semantic analysis
    SemanticAnalyzer analyzer(lexer.stringPool());
    auto semantic_result = analyzer.analyze(parse_result.statement());
    ASSERT_TRUE(semantic_result.success());

    // Generate bytecode
    BytecodeGenerator generator(lexer.stringPool(), db.get());
    auto bytecode_result = generator.generate(parse_result.statement());

    EXPECT_TRUE(bytecode_result.success()) << "Bytecode generation failed";
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
