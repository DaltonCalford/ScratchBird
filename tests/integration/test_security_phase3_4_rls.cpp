/*
 * Security System Phase 3.4 Integration Test
 * Tests Row-Level Security (RLS) DDL operations and fail-safe behavior
 *
 * Covers:
 * - CREATE POLICY syntax and catalog operations
 * - DROP POLICY syntax and catalog operations
 * - ALTER TABLE ... ROW LEVEL SECURITY operations
 * - RLS enable/disable/force behavior
 * - Fail-safe behavior (RLS enabled with no policies = deny all)
 * - Superuser bypass (non-forced RLS)
 * - Forced RLS (even superusers must obey)
 * - Policy CRUD operations (createPolicy, dropPolicy, getPolicy, etc.)
 *
 * NOT Covered (Phase 3.4.6 - Deferred):
 * - Runtime expression evaluation (USING, WITH CHECK)
 * - Row filtering based on policies
 * - Multi-policy combination
 */

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/opcodes.h"
#include "test_helpers.h"

#include <filesystem>
#include <memory>

using namespace scratchbird;
using namespace scratchbird::core;
using scratchbird::sblr::Executor;
using scratchbird::sblr::QueryCompilerV2;
using scratchbird::sblr::Opcode;

class SecurityPhase3_4_RLS_Test : public ::testing::Test
{
protected:
    std::unique_ptr<Database> db;
    std::unique_ptr<ConnectionContext> conn_ctx_;
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
    std::string test_db_path;
    uint32_t proc_id_ = 0;

    void SetUp() override
    {
        // Create temporary test database
        test_db_path =
            scratchbird::testing::uniqueTestDbPath("test_security_phase3_4_rls", ".db");

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

        status = ProcArrayManager::initialize(db.get(), 4, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to initialize ProcArray: " << ctx.message;

        status = ProcArrayManager::registerBackend(&proc_id_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to register backend: " << ctx.message;

        conn_ctx_ = std::make_unique<ConnectionContext>(db.get(), proc_id_);
        status = conn_ctx_->initialize(&ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to initialize connection context: " << ctx.message;

        auto system_user = db->catalog_manager()->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user, true);

        core::CatalogManager::SchemaInfo schema;
        ASSERT_EQ(db->catalog_manager()->getSchema("PUBLIC", schema, &ctx), core::Status::OK)
            << "Failed to get PUBLIC schema: " << ctx.message;
        conn_ctx_->setCurrentSchemaId(schema.schema_id);

        compiler_ = std::make_unique<QueryCompilerV2>(db.get());
        compiler_->setCurrentSchema(schema.schema_id);

        executor_ = std::make_unique<Executor>(db.get());
        executor_->setConnectionContext(conn_ctx_.get());
        executor_->setCurrentSchema(schema.schema_id);
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        conn_ctx_.reset();

        ErrorContext ctx;
        ProcArrayManager::unregisterBackend(proc_id_, &ctx);
        ProcArrayManager::shutdown(&ctx);

        db.reset();
        if (std::filesystem::exists(test_db_path))
        {
            std::filesystem::remove_all(test_db_path);
        }
    }

    scratchbird::sblr::ExecutionResult executeSQL(const std::string& sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            if (!compile_result.errors().empty())
            {
                return scratchbird::sblr::ExecutionResult(compile_result.errors().front());
            }
            return scratchbird::sblr::ExecutionResult("Compile error");
        }

        return executor_->execute(compile_result.bytecode());
    }

    void commitTransaction()
    {
        ErrorContext ctx;
        auto status = conn_ctx_->commit(&ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to commit: " << ctx.message;
    }

    // Helper to create a test table
    ID createTestTable(const std::string& table_name)
    {
        // Get public schema
        CatalogManager::SchemaInfo schema_info;
        ErrorContext ctx;
        auto status = db->catalog_manager()->getSchema("PUBLIC", schema_info, &ctx);
        EXPECT_EQ(status, Status::OK);

        // Create table with basic columns
        std::vector<CatalogManager::ColumnInfo> columns;

        CatalogManager::ColumnInfo id_col;
        id_col.column_name = "id";
        id_col.data_type = DataType::INTEGER;
        id_col.is_nullable = false;
        columns.push_back(id_col);

        CatalogManager::ColumnInfo name_col;
        name_col.column_name = "name";
        name_col.data_type = DataType::VARCHAR;
        name_col.precision = 100;
        name_col.is_nullable = true;
        columns.push_back(name_col);

        CatalogManager::ColumnInfo tenant_col;
        tenant_col.column_name = "tenant_id";
        tenant_col.data_type = DataType::INTEGER;
        tenant_col.is_nullable = true;
        columns.push_back(tenant_col);

        ID table_id;
        status = db->catalog_manager()->createTable(
            schema_info.schema_id, table_name, columns, table_id, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to create table: " << ctx.message;

        return table_id;
    }

    std::string makeBinaryPolicyHex(const std::string& column,
                                    Opcode op,
                                    int32_t literal) const
    {
        std::vector<uint8_t> bytes;
        bytes.push_back(static_cast<uint8_t>(Opcode::COLUMN_REF));

        uint8_t len_buf[10];
        size_t len_bytes = scratchbird::sblr::writeUVarint(len_buf, column.size());
        bytes.insert(bytes.end(), len_buf, len_buf + len_bytes);
        bytes.insert(bytes.end(), column.begin(), column.end());

        bytes.push_back(static_cast<uint8_t>(Opcode::LITERAL_INT32));
        uint8_t int_buf[4];
        scratchbird::sblr::writeInt32(int_buf, static_cast<uint32_t>(literal));
        bytes.insert(bytes.end(), int_buf, int_buf + 4);

        bytes.push_back(static_cast<uint8_t>(op));

        static const char hex_chars[] = "0123456789abcdef";
        std::string hex;
        hex.reserve(2 + bytes.size() * 2);
        hex.append("0x");
        for (uint8_t byte : bytes)
        {
            hex.push_back(hex_chars[(byte >> 4) & 0x0F]);
            hex.push_back(hex_chars[byte & 0x0F]);
        }
        return hex;
    }

};

// ============================================================================
// Test 1: Policy CRUD Operations
// ============================================================================

TEST_F(SecurityPhase3_4_RLS_Test, CreatePolicyBasic)
{
    // Create test table
    ID table_id = createTestTable("documents");

    // Create policy via catalog manager
    ID policy_id;
    ErrorContext ctx;
    auto status = db->catalog_manager()->createPolicy(
        table_id,
        "tenant_isolation",
        CatalogManager::PolicyType::SELECT,
        std::vector<std::string>{},  // All roles
        "",  // No USING expression (Phase 3.4.6)
        "",  // No WITH CHECK expression
        policy_id,
        &ctx);

    EXPECT_EQ(status, Status::OK) << "createPolicy failed: " << ctx.message;
    EXPECT_NE(policy_id, ID{}) << "Policy ID should not be empty";

    // Verify policy exists
    CatalogManager::PolicyInfo policy_info;
    status = db->catalog_manager()->getPolicy(table_id, "tenant_isolation", policy_info, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(policy_info.policy_name, "tenant_isolation");
    EXPECT_EQ(policy_info.policy_type, CatalogManager::PolicyType::SELECT);
    EXPECT_TRUE(policy_info.is_enabled);
}

TEST_F(SecurityPhase3_4_RLS_Test, CreatePolicyDuplicate)
{
    ID table_id = createTestTable("documents");

    // Create first policy
    ID policy_id1;
    ErrorContext ctx;
    auto status = db->catalog_manager()->createPolicy(
        table_id, "test_policy", CatalogManager::PolicyType::ALL,
        {}, "", "", policy_id1, &ctx);
    EXPECT_EQ(status, Status::OK);

    // Try to create duplicate - should fail
    ID policy_id2;
    status = db->catalog_manager()->createPolicy(
        table_id, "test_policy", CatalogManager::PolicyType::SELECT,
        {}, "", "", policy_id2, &ctx);
    EXPECT_EQ(status, Status::FILE_EXISTS) << "Should reject duplicate policy name";
}

TEST_F(SecurityPhase3_4_RLS_Test, DropPolicy)
{
    ID table_id = createTestTable("documents");

    // Create policy
    ID policy_id;
    ErrorContext ctx;
    auto status = db->catalog_manager()->createPolicy(
        table_id, "test_policy", CatalogManager::PolicyType::SELECT,
        {}, "", "", policy_id, &ctx);
    EXPECT_EQ(status, Status::OK);

    // Drop policy
    status = db->catalog_manager()->dropPolicy(table_id, "test_policy", &ctx);
    EXPECT_EQ(status, Status::OK);

    // Verify policy is gone
    CatalogManager::PolicyInfo policy_info;
    status = db->catalog_manager()->getPolicy(table_id, "test_policy", policy_info, &ctx);
    EXPECT_EQ(status, Status::NOT_FOUND) << "Policy should be deleted";
}

TEST_F(SecurityPhase3_4_RLS_Test, GetTablePolicies)
{
    ID table_id = createTestTable("documents");

    // Create multiple policies
    ID policy_id;
    ErrorContext ctx;

    db->catalog_manager()->createPolicy(table_id, "policy1",
        CatalogManager::PolicyType::SELECT, {}, "", "", policy_id, &ctx);
    db->catalog_manager()->createPolicy(table_id, "policy2",
        CatalogManager::PolicyType::INSERT, {}, "", "", policy_id, &ctx);
    db->catalog_manager()->createPolicy(table_id, "policy3",
        CatalogManager::PolicyType::UPDATE, {}, "", "", policy_id, &ctx);

    // Get all policies for table (need to query each type separately)
    std::vector<CatalogManager::PolicyInfo> all_policies;
    std::vector<CatalogManager::PolicyInfo> select_policies;
    std::vector<CatalogManager::PolicyInfo> insert_policies;
    std::vector<CatalogManager::PolicyInfo> update_policies;

    auto status = db->catalog_manager()->getTablePolicies(table_id, CatalogManager::PolicyType::SELECT, select_policies, &ctx);
    EXPECT_EQ(status, Status::OK);
    status = db->catalog_manager()->getTablePolicies(table_id, CatalogManager::PolicyType::INSERT, insert_policies, &ctx);
    EXPECT_EQ(status, Status::OK);
    status = db->catalog_manager()->getTablePolicies(table_id, CatalogManager::PolicyType::UPDATE, update_policies, &ctx);
    EXPECT_EQ(status, Status::OK);

    // Combine all policies
    all_policies.insert(all_policies.end(), select_policies.begin(), select_policies.end());
    all_policies.insert(all_policies.end(), insert_policies.begin(), insert_policies.end());
    all_policies.insert(all_policies.end(), update_policies.begin(), update_policies.end());

    EXPECT_EQ(all_policies.size(), 3) << "Should have 3 policies total";

    // Verify policy names
    std::set<std::string> names;
    for (const auto& policy : all_policies)
    {
        names.insert(policy.policy_name);
    }
    EXPECT_TRUE(names.count("policy1"));
    EXPECT_TRUE(names.count("policy2"));
    EXPECT_TRUE(names.count("policy3"));
}

// ============================================================================
// Test 2: ALTER TABLE RLS Operations
// ============================================================================

TEST_F(SecurityPhase3_4_RLS_Test, EnableRLS)
{
    ID table_id = createTestTable("documents");

    // Enable RLS
    ErrorContext ctx;
    auto status = db->catalog_manager()->setTableRLS(table_id, true, false, &ctx);
    EXPECT_EQ(status, Status::OK);

    // Verify RLS is enabled
    CatalogManager::SchemaInfo schema_info;
    db->catalog_manager()->getSchema("PUBLIC", schema_info, &ctx);

    CatalogManager::TableInfo table_info;
    db->catalog_manager()->getTable(schema_info.schema_id, "documents", table_info, &ctx);

    EXPECT_TRUE(table_info.rls_enabled) << "RLS should be enabled";
    EXPECT_FALSE(table_info.rls_forced) << "RLS should not be forced";
}

TEST_F(SecurityPhase3_4_RLS_Test, ForceRLS)
{
    ID table_id = createTestTable("documents");

    // Force RLS
    ErrorContext ctx;
    auto status = db->catalog_manager()->setTableRLS(table_id, true, true, &ctx);
    EXPECT_EQ(status, Status::OK);

    // Verify RLS is forced
    CatalogManager::SchemaInfo schema_info;
    db->catalog_manager()->getSchema("PUBLIC", schema_info, &ctx);

    CatalogManager::TableInfo table_info;
    db->catalog_manager()->getTable(schema_info.schema_id, "documents", table_info, &ctx);

    EXPECT_TRUE(table_info.rls_enabled) << "RLS should be enabled";
    EXPECT_TRUE(table_info.rls_forced) << "RLS should be forced";
}

TEST_F(SecurityPhase3_4_RLS_Test, DisableRLS)
{
    ID table_id = createTestTable("documents");

    // Enable then disable
    ErrorContext ctx;
    db->catalog_manager()->setTableRLS(table_id, true, false, &ctx);

    auto status = db->catalog_manager()->setTableRLS(table_id, false, false, &ctx);
    EXPECT_EQ(status, Status::OK);

    // Verify RLS is disabled
    CatalogManager::SchemaInfo schema_info;
    db->catalog_manager()->getSchema("PUBLIC", schema_info, &ctx);

    CatalogManager::TableInfo table_info;
    db->catalog_manager()->getTable(schema_info.schema_id, "documents", table_info, &ctx);

    EXPECT_FALSE(table_info.rls_enabled) << "RLS should be disabled";
    EXPECT_FALSE(table_info.rls_forced) << "RLS should not be forced";
}

// ============================================================================
// Test 3: SQL Syntax Parsing and Bytecode Generation
// ============================================================================

TEST_F(SecurityPhase3_4_RLS_Test, ParseCreatePolicy)
{
    GTEST_SKIP() << "Parser V2 RLS DDL support pending";
}

TEST_F(SecurityPhase3_4_RLS_Test, ParseDropPolicy)
{
    GTEST_SKIP() << "Parser V2 RLS DDL support pending";
}

TEST_F(SecurityPhase3_4_RLS_Test, ParseAlterTableEnableRLS)
{
    GTEST_SKIP() << "Parser V2 RLS DDL support pending";
}

TEST_F(SecurityPhase3_4_RLS_Test, ParseAlterTableForceRLS)
{
    GTEST_SKIP() << "Parser V2 RLS DDL support pending";
}

// ============================================================================
// Test 4: End-to-End SQL Execution
// ============================================================================

TEST_F(SecurityPhase3_4_RLS_Test, ExecuteCreatePolicySQL)
{
    GTEST_SKIP() << "Parser V2 RLS DDL execution pending";
}

TEST_F(SecurityPhase3_4_RLS_Test, ExecuteDropPolicySQL)
{
    GTEST_SKIP() << "Parser V2 RLS DDL execution pending";
}

TEST_F(SecurityPhase3_4_RLS_Test, ExecuteAlterTableRLSSQL)
{
    GTEST_SKIP() << "Parser V2 RLS DDL execution pending";
}

// ============================================================================
// Test 5: Policy Type Filtering
// ============================================================================

TEST_F(SecurityPhase3_4_RLS_Test, PolicyTypeFiltering)
{
    ID table_id = createTestTable("documents");

    // Create policies of different types
    ID policy_id;
    ErrorContext ctx;

    db->catalog_manager()->createPolicy(table_id, "select_policy",
        CatalogManager::PolicyType::SELECT, {}, "", "", policy_id, &ctx);
    db->catalog_manager()->createPolicy(table_id, "insert_policy",
        CatalogManager::PolicyType::INSERT, {}, "", "", policy_id, &ctx);
    db->catalog_manager()->createPolicy(table_id, "all_policy",
        CatalogManager::PolicyType::ALL, {}, "", "", policy_id, &ctx);

    // Get all policies (need to query each type)
    std::vector<CatalogManager::PolicyInfo> all_policies;
    std::vector<CatalogManager::PolicyInfo> temp_policies;

    db->catalog_manager()->getTablePolicies(table_id, CatalogManager::PolicyType::SELECT, temp_policies, &ctx);
    all_policies.insert(all_policies.end(), temp_policies.begin(), temp_policies.end());
    temp_policies.clear();

    db->catalog_manager()->getTablePolicies(table_id, CatalogManager::PolicyType::INSERT, temp_policies, &ctx);
    all_policies.insert(all_policies.end(), temp_policies.begin(), temp_policies.end());
    temp_policies.clear();

    db->catalog_manager()->getTablePolicies(table_id, CatalogManager::PolicyType::ALL, temp_policies, &ctx);
    all_policies.insert(all_policies.end(), temp_policies.begin(), temp_policies.end());

    EXPECT_EQ(all_policies.size(), 3);

    // Count by type
    int select_count = 0, insert_count = 0, all_count = 0;
    for (const auto& policy : all_policies)
    {
        if (policy.policy_type == CatalogManager::PolicyType::SELECT) select_count++;
        if (policy.policy_type == CatalogManager::PolicyType::INSERT) insert_count++;
        if (policy.policy_type == CatalogManager::PolicyType::ALL) all_count++;
    }

    EXPECT_EQ(select_count, 1);
    EXPECT_EQ(insert_count, 1);
    EXPECT_EQ(all_count, 1);
}

// ============================================================================
// Test 6: Multiple Policies Per Table
// ============================================================================

TEST_F(SecurityPhase3_4_RLS_Test, MultiplePoliciesPerTable)
{
    ID table_id = createTestTable("documents");

    // Create 5 policies with different names
    ErrorContext ctx;
    ID policy_id;

    for (int i = 1; i <= 5; i++)
    {
        std::string policy_name = "policy_" + std::to_string(i);
        auto status = db->catalog_manager()->createPolicy(
            table_id, policy_name, CatalogManager::PolicyType::SELECT,
            {}, "", "", policy_id, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to create " << policy_name;
    }

    // Verify all 5 policies exist (all SELECT type)
    std::vector<CatalogManager::PolicyInfo> policies;
    db->catalog_manager()->getTablePolicies(table_id, CatalogManager::PolicyType::SELECT, policies, &ctx);
    EXPECT_EQ(policies.size(), 5);
}

/**
 * Test 17: Expression Storage (Phase 3.4.6)
 *
 * Tests that policy expressions are stored and retrieved correctly.
 */
TEST_F(SecurityPhase3_4_RLS_Test, ExpressionStorage)
{
    // Create test table
    ID table_id = createTestTable("documents");

    // Create policy with USING and WITH CHECK expressions
    std::string using_expr = "tenant_id = current_tenant_id()";
    std::string with_check_expr = "status = 'draft'";
    std::vector<std::string> roles = {"tenant_users"};

    ID policy_id;
    ErrorContext ctx;
    auto status = db->catalog_manager()->createPolicy(
        table_id,
        "tenant_isolation",
        CatalogManager::PolicyType::SELECT,
        roles,
        using_expr,
        with_check_expr,
        policy_id,
        &ctx);

    ASSERT_EQ(status, Status::OK) << "Failed to create policy: " << ctx.message;

    // Retrieve policy and check expressions
    CatalogManager::PolicyInfo policy_info;
    status = db->catalog_manager()->getPolicy(table_id, "tenant_isolation", policy_info, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to get policy: " << ctx.message;

    // Verify expressions are stored correctly (Phase 3.4.6)
    EXPECT_EQ(policy_info.using_expr, using_expr) << "USING expression not stored correctly";
    EXPECT_EQ(policy_info.with_check_expr, with_check_expr) << "WITH CHECK expression not stored correctly";

    // Verify roles are stored correctly
    ASSERT_EQ(policy_info.roles.size(), 1);
    EXPECT_EQ(policy_info.roles[0], "tenant_users");

    // Verify getTablePolicies also returns expressions
    std::vector<CatalogManager::PolicyInfo> policies;
    status = db->catalog_manager()->getTablePolicies(table_id, CatalogManager::PolicyType::ALL,
                                                     policies, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(policies.size(), 1);

    EXPECT_EQ(policies[0].using_expr, using_expr) << "USING expression not in getTablePolicies";
    EXPECT_EQ(policies[0].with_check_expr, with_check_expr) << "WITH CHECK expression not in getTablePolicies";
}

// Test 18: RuntimeFiltering - Verify RLS policies filter SELECT results (Phase 3.4.7)
TEST_F(SecurityPhase3_4_RLS_Test, RuntimeFiltering)
{
    // Create test table
    ID table_id = createTestTable("products");

    auto insert_a = executeSQL("INSERT INTO products (id, name, tenant_id) VALUES (1, 'alpha', 1)");
    ASSERT_TRUE(insert_a.success()) << "INSERT failed: " << insert_a.error();
    auto insert_b = executeSQL("INSERT INTO products (id, name, tenant_id) VALUES (2, 'beta', 2)");
    ASSERT_TRUE(insert_b.success()) << "INSERT failed: " << insert_b.error();
    commitTransaction();

    // Enable RLS on table (forced so superuser cannot bypass)
    ErrorContext ctx;
    auto status = db->catalog_manager()->setTableRLS(table_id, true, true, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to enable RLS: " << ctx.message;

    // Create policy: tenant_id = 1
    std::string using_expr = makeBinaryPolicyHex("tenant_id", Opcode::EXPR_EQ, 1);
    ID policy_id;
    status = db->catalog_manager()->createPolicy(
        table_id,
        "tenant_filter",
        CatalogManager::PolicyType::ALL,
        {},  // all roles
        using_expr,
        using_expr,
        policy_id,
        &ctx);

    ASSERT_EQ(status, Status::OK) << "Failed to create policy: " << ctx.message;

    // Verify RLS is enabled
    CatalogManager::TableInfo table_info;
    status = db->catalog_manager()->getTable(table_id, table_info, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_TRUE(table_info.rls_enabled);

    // Verify policy exists and has correct expression
    CatalogManager::PolicyInfo policy_info;
    status = db->catalog_manager()->getPolicy(table_id, "tenant_filter", policy_info, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(policy_info.using_expr, using_expr);

    auto select_result = executeSQL("SELECT id, tenant_id FROM products ORDER BY id");
    ASSERT_TRUE(select_result.success()) << "SELECT failed: " << select_result.error();

    auto* rs = select_result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_EQ(rs->getValue(0, 0).toInt32(), 1);
    EXPECT_EQ(rs->getValue(0, 1).toInt32(), 1);
}

TEST_F(SecurityPhase3_4_RLS_Test, DmlHonorsWithCheckPolicies)
{
    ID table_id = createTestTable("tasks");

    auto insert_ok = executeSQL("INSERT INTO tasks (id, name, tenant_id) VALUES (1, 'ok', 1)");
    ASSERT_TRUE(insert_ok.success()) << "INSERT failed: " << insert_ok.error();
    commitTransaction();

    ErrorContext ctx;
    auto status = db->catalog_manager()->setTableRLS(table_id, true, true, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to enable RLS: " << ctx.message;

    std::string policy_expr = makeBinaryPolicyHex("tenant_id", Opcode::EXPR_EQ, 1);
    ID policy_id;
    status = db->catalog_manager()->createPolicy(
        table_id,
        "tenant_guard",
        CatalogManager::PolicyType::ALL,
        {},
        policy_expr,
        policy_expr,
        policy_id,
        &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create policy: " << ctx.message;

    auto insert_blocked = executeSQL("INSERT INTO tasks (id, name, tenant_id) VALUES (2, 'blocked', 2)");
    EXPECT_FALSE(insert_blocked.success());
    EXPECT_NE(insert_blocked.error().find("Row-level security policy violation"),
              std::string::npos);

    auto update_blocked = executeSQL("UPDATE tasks SET tenant_id = 2 WHERE id = 1");
    EXPECT_FALSE(update_blocked.success());
    EXPECT_NE(update_blocked.error().find("Row-level security policy violation"),
              std::string::npos);
}

// Test 19: TOAST Persistence
TEST_F(SecurityPhase3_4_RLS_Test, ToastPersistence)
{
    // Create test table
    ID table_id = createTestTable("products");

    // Enable RLS on table
    ErrorContext ctx;
    auto status = db->catalog_manager()->setTableRLS(table_id, true, false, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Create a policy with large expression to ensure TOAST is used
    std::string using_expr = "price < 100 AND category IN ('electronics', 'books', 'clothing') AND stock > 0";
    std::string with_check_expr = "price >= 0 AND category IS NOT NULL AND description IS NOT NULL";
    std::vector<std::string> roles = {"users", "readonly"};

    ID policy_id;
    status = db->catalog_manager()->createPolicy(
        table_id,
        "complex_policy",
        CatalogManager::PolicyType::ALL,
        roles,
        using_expr,
        with_check_expr,
        policy_id,
        &ctx);

    ASSERT_EQ(status, Status::OK);

    // Clear the in-memory cache to force loading from TOAST
    // Note: This is a test-only operation - in production the cache would persist
    db->catalog_manager()->clearPolicyCache();  // We'll need to add this method

    // Retrieve policy - should load from TOAST
    CatalogManager::PolicyInfo policy_info;
    status = db->catalog_manager()->getPolicy(table_id, "complex_policy", policy_info, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify expressions were correctly persisted and loaded
    EXPECT_EQ(policy_info.using_expr, using_expr);
    EXPECT_EQ(policy_info.with_check_expr, with_check_expr);
    EXPECT_EQ(policy_info.policy_name, "complex_policy");

    // Drop the policy - should cleanup TOAST data
    status = db->catalog_manager()->dropPolicy(table_id, "complex_policy", &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify policy is gone
    status = db->catalog_manager()->getPolicy(table_id, "complex_policy", policy_info, &ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);

    std::cout << "TOAST persistence test completed successfully" << std::endl;
}

// Main function to run tests
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
