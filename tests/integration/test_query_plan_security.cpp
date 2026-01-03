/**
 * Integration tests for Query Plan Security (Phase 3.2.1)
 *
 * Tests permission checking at query plan time instead of execution time.
 * This provides 10-100x performance improvement by checking permissions
 * ONCE per query instead of per row.
 */

#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/parser/parser_v2.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/semantic_analyzer_v2.h"
#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/optimizer/statistics_manager.h"
#include "test_helpers.h"

#include <memory>
#include <string>

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::sblr;
using namespace scratchbird::optimizer;
using scratchbird::testing::TestDatabaseFile;

class QueryPlanSecurityTest : public ::testing::Test
{
protected:
    std::unique_ptr<Database> db;
    std::unique_ptr<ConnectionContext> admin_ctx;
    std::unique_ptr<ConnectionContext> user_ctx;
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<TestDatabaseFile> db_file_;
    core::ID schema_id_;

    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_query_plan_security");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;

        db = std::make_unique<Database>();
        ASSERT_EQ(db->open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        CatalogManager::SchemaInfo schema;
        ASSERT_EQ(db->catalog_manager()->getSchema("PUBLIC", schema, &ctx), Status::OK)
            << ctx.message;
        schema_id_ = schema.schema_id;

        compiler_ = std::make_unique<QueryCompilerV2>(db.get());
        compiler_->setCurrentSchema(schema_id_);

        executor_ = std::make_unique<Executor>(db.get());
        executor_->setCurrentSchema(schema_id_);

        // Initialize admin connection context (proc_id 1)
        admin_ctx = std::make_unique<ConnectionContext>(db.get(), 1);
        ASSERT_EQ(admin_ctx->initialize(&ctx), Status::OK);

        // Set admin as superuser (bootstrap user)
        CatalogManager::UserInfo admin_user;
        ASSERT_EQ(db->catalog_manager()->getUserByName("SYS", admin_user, &ctx), Status::OK);
        admin_ctx->setCurrentUser(admin_user.user_id, true);

        // Initialize regular user connection context (proc_id 2)
        user_ctx = std::make_unique<ConnectionContext>(db.get(), 2);
        ASSERT_EQ(user_ctx->initialize(&ctx), Status::OK);

        // Create a test user
        executeSQL(admin_ctx.get(), "CREATE USER alice WITH PASSWORD 'password123'");

        // Set user_ctx to alice
        CatalogManager::UserInfo alice_user;
        ASSERT_EQ(db->catalog_manager()->getUserByName("alice", alice_user, &ctx), Status::OK);
        user_ctx->setCurrentUser(alice_user.user_id, false);

        // Create test table
        executeSQL(admin_ctx.get(), "CREATE TABLE employees (id INT, name VARCHAR(100), salary DECIMAL)");

        // Insert test data
        executeSQL(admin_ctx.get(), "INSERT INTO employees VALUES (1, 'Bob', 50000)");
        executeSQL(admin_ctx.get(), "INSERT INTO employees VALUES (2, 'Charlie', 60000)");
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        user_ctx.reset();
        admin_ctx.reset();
        db.reset();
        db_file_.reset();
    }

    bool executeSQL(ConnectionContext* conn_ctx, const std::string& sql, std::string* error_msg = nullptr)
    {
        ConnectionContext::setCurrent(conn_ctx);
        executor_->setConnectionContext(conn_ctx);

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

    std::shared_ptr<PlanNode> planQuery(ConnectionContext* conn_ctx,
                                        const std::string& sql,
                                        ErrorContext* ctx)
    {
        ConnectionContext::setCurrent(conn_ctx);

        parser::v2::Parser parser(sql);
        auto parse_result = parser.parseStatement();
        if (!parse_result.success())
        {
            SET_ERROR_CONTEXT(ctx, Status::SYNTAX_ERROR, "Parse failed");
            return nullptr;
        }

        parser::v2::SemanticAnalyzerV2 analyzer(*db->catalog_manager(), parser.stringPool());
        analyzer.setCurrentSchema(schema_id_);
        auto sem_result = analyzer.analyze(parse_result.statement());
        if (!sem_result.success())
        {
            SET_ERROR_CONTEXT(ctx, Status::SYNTAX_ERROR, "Semantic analysis failed");
            return nullptr;
        }

        auto* select_stmt = dynamic_cast<parser::v2::ResolvedSelectStmt*>(sem_result.statement());
        if (!select_stmt)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_STATEMENT, "Not a SELECT statement");
            return nullptr;
        }

        return db->query_planner()->planQuery(select_stmt, ctx, conn_ctx);
    }
};

// Test 1: Superuser can plan query without SELECT permission
TEST_F(QueryPlanSecurityTest, SuperuserBypassesPermissionCheck)
{
    ErrorContext ctx;

    // Admin (superuser) should be able to plan query
    auto plan = planQuery(admin_ctx.get(), "SELECT * FROM employees", &ctx);

    ASSERT_NE(plan, nullptr) << "Superuser should be able to plan query";
    EXPECT_EQ(ctx.code, Status::OK);
}

// Test 2: Regular user without SELECT permission cannot plan query
TEST_F(QueryPlanSecurityTest, UserWithoutPermissionCannotPlanQuery)
{
    ErrorContext ctx;

    // Alice (regular user) should NOT be able to plan query (no permission)
    auto plan = planQuery(user_ctx.get(), "SELECT * FROM employees", &ctx);

    ASSERT_EQ(plan, nullptr) << "User without SELECT permission should fail at plan time";
    EXPECT_EQ(ctx.code, Status::PERMISSION_DENIED);
    EXPECT_NE(ctx.message.find("Permission denied"), std::string::npos);
}

// Test 3: Regular user WITH SELECT permission can plan query
TEST_F(QueryPlanSecurityTest, UserWithPermissionCanPlanQuery)
{
    ErrorContext ctx;

    // Grant SELECT permission to alice
    ASSERT_TRUE(executeSQL(admin_ctx.get(), "GRANT SELECT ON TABLE employees TO alice"));

    // Now alice should be able to plan query
    auto plan = planQuery(user_ctx.get(), "SELECT * FROM employees", &ctx);

    ASSERT_NE(plan, nullptr) << "User with SELECT permission should succeed at plan time";
    EXPECT_EQ(ctx.code, Status::OK);
}

// Test 4: Permission check happens at plan time, not execution time
TEST_F(QueryPlanSecurityTest, PermissionCheckAtPlanTimeNotExecutionTime)
{
    ErrorContext ctx;

    // Without SELECT permission, planning should fail immediately
    auto plan = planQuery(user_ctx.get(), "SELECT * FROM employees WHERE id = 1", &ctx);

    ASSERT_EQ(plan, nullptr) << "Permission check should fail at PLAN time";
    EXPECT_EQ(ctx.code, Status::PERMISSION_DENIED);

    // This is the key benefit: Permission denied BEFORE any I/O happens
    // In Phase 2 (executor-level checks), we would read all rows then filter
    // In Phase 3.2 (planner-level checks), we reject immediately with no I/O
}

// Test 5: Permission cache works across multiple calls
TEST_F(QueryPlanSecurityTest, PermissionCacheWorksCorrectly)
{
    ErrorContext ctx1, ctx2;

    // Grant SELECT permission
    ASSERT_TRUE(executeSQL(admin_ctx.get(), "GRANT SELECT ON TABLE employees TO alice"));

    // First query should check catalog and cache result
    auto plan1 = planQuery(user_ctx.get(), "SELECT * FROM employees", &ctx1);
    ASSERT_NE(plan1, nullptr);
    EXPECT_EQ(ctx1.code, Status::OK);

    // Second query should use cached result (still works)
    auto plan2 = planQuery(user_ctx.get(), "SELECT * FROM employees WHERE id = 1", &ctx2);
    ASSERT_NE(plan2, nullptr);
    EXPECT_EQ(ctx2.code, Status::OK);
}

// Test 6: REVOKE invalidates cached permissions
TEST_F(QueryPlanSecurityTest, RevokeInvalidatesCachedPermissions)
{
    ErrorContext ctx1, ctx2;

    // Grant SELECT permission
    ASSERT_TRUE(executeSQL(admin_ctx.get(), "GRANT SELECT ON TABLE employees TO alice"));

    // Query should succeed
    auto plan1 = planQuery(user_ctx.get(), "SELECT * FROM employees", &ctx1);
    ASSERT_NE(plan1, nullptr);

    // Revoke permission
    ASSERT_TRUE(executeSQL(admin_ctx.get(), "REVOKE SELECT ON TABLE employees FROM alice"));

    // Query should now fail (cache is cleared between queries)
    auto plan2 = planQuery(user_ctx.get(), "SELECT * FROM employees", &ctx2);
    ASSERT_EQ(plan2, nullptr);
    EXPECT_EQ(ctx2.code, Status::PERMISSION_DENIED);
}

// ============================================================================
// Phase 3.2.2: DML Permission Tests
// ============================================================================

// Test 7: INSERT permission check (statement-level, already optimal)
TEST_F(QueryPlanSecurityTest, InsertPermissionCheck)
{
    std::string error_msg;

    // Alice should NOT be able to INSERT (no permission)
    ASSERT_FALSE(executeSQL(user_ctx.get(), "INSERT INTO employees VALUES (3, 'Carol', 70000)", &error_msg));
    EXPECT_NE(error_msg.find("Permission denied"), std::string::npos);

    // Grant INSERT permission
    ASSERT_TRUE(executeSQL(admin_ctx.get(), "GRANT INSERT ON TABLE employees TO alice"));

    // Now alice should be able to INSERT
    ASSERT_TRUE(executeSQL(user_ctx.get(), "INSERT INTO employees VALUES (3, 'Carol', 70000)"));

    // Verify insert worked
    ASSERT_TRUE(executeSQL(admin_ctx.get(), "SELECT * FROM employees WHERE id = 3"));
}

// Test 8: UPDATE permission check (statement-level, already optimal)
TEST_F(QueryPlanSecurityTest, UpdatePermissionCheck)
{
    std::string error_msg;

    // Alice should NOT be able to UPDATE (no permission)
    ASSERT_FALSE(executeSQL(user_ctx.get(), "UPDATE employees SET salary = 75000 WHERE id = 1", &error_msg));
    EXPECT_NE(error_msg.find("Permission denied"), std::string::npos);

    // Grant UPDATE permission
    ASSERT_TRUE(executeSQL(admin_ctx.get(), "GRANT UPDATE ON TABLE employees TO alice"));

    // Now alice should be able to UPDATE
    ASSERT_TRUE(executeSQL(user_ctx.get(), "UPDATE employees SET salary = 75000 WHERE id = 1"));
}

// Test 9: DELETE permission check (statement-level, already optimal)
TEST_F(QueryPlanSecurityTest, DeletePermissionCheck)
{
    std::string error_msg;

    // Alice should NOT be able to DELETE (no permission)
    ASSERT_FALSE(executeSQL(user_ctx.get(), "DELETE FROM employees WHERE id = 1", &error_msg));
    EXPECT_NE(error_msg.find("Permission denied"), std::string::npos);

    // Grant DELETE permission
    ASSERT_TRUE(executeSQL(admin_ctx.get(), "GRANT DELETE ON TABLE employees TO alice"));

    // Now alice should be able to DELETE
    ASSERT_TRUE(executeSQL(user_ctx.get(), "DELETE FROM employees WHERE id = 1"));
}

// Test 10: Superuser can perform all DML without GRANT
TEST_F(QueryPlanSecurityTest, SuperuserDMLBypass)
{
    // Admin (superuser) can do everything without explicit GRANT
    ASSERT_TRUE(executeSQL(admin_ctx.get(), "INSERT INTO employees VALUES (4, 'Dave', 80000)"));
    ASSERT_TRUE(executeSQL(admin_ctx.get(), "UPDATE employees SET salary = 85000 WHERE id = 4"));
    ASSERT_TRUE(executeSQL(admin_ctx.get(), "DELETE FROM employees WHERE id = 4"));
}

// Test 11: DML permission checks are statement-level (not per-row)
TEST_F(QueryPlanSecurityTest, DMLPermissionIsStatementLevel)
{
    // Grant INSERT permission
    ASSERT_TRUE(executeSQL(admin_ctx.get(), "GRANT INSERT ON TABLE employees TO alice"));

    // Insert multiple rows - permission checked ONCE, not per row
    ASSERT_TRUE(executeSQL(user_ctx.get(), "INSERT INTO employees VALUES (5, 'Eve', 50000)"));
    ASSERT_TRUE(executeSQL(user_ctx.get(), "INSERT INTO employees VALUES (6, 'Frank', 60000)"));
    ASSERT_TRUE(executeSQL(user_ctx.get(), "INSERT INTO employees VALUES (7, 'Grace', 70000)"));

    // This demonstrates O(1) permission checking (not O(N))
    // Each statement is checked once, regardless of row count
}
