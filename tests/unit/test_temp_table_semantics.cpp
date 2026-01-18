/**
 * Temp table placement + lifecycle tests.
 */

#include <gtest/gtest.h>
#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "unit/test_user_helpers.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <thread>

using namespace scratchbird::sblr;
using namespace scratchbird::core;

static std::string makeUniquePath(const std::string& prefix, const std::string& suffix)
{
    std::ostringstream oss;
    oss << "/tmp/" << prefix << "_"
        << std::this_thread::get_id() << "_"
        << std::chrono::steady_clock::now().time_since_epoch().count()
        << suffix;
    return oss.str();
}

class TempTableExecutorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_path_ = makeUniquePath("test_temp_tables", ".sbdb");
        std::filesystem::remove(db_path_);

        ErrorContext ctx;
        auto status = Database::create(db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database";

        status = db_.open(db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database";

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        CatalogManager::SchemaInfo public_schema_info;
        status = catalog_->getSchema("public", public_schema_info, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to resolve public schema";
        public_schema_id_ = public_schema_info.schema_id;

        compiler_ = std::make_unique<QueryCompilerV2>(&db_);
        executor_ = std::make_unique<Executor>(&db_);
    }

    void TearDown() override
    {
        clearConnection();
        compiler_.reset();
        executor_.reset();
        db_.close();
        std::filesystem::remove(db_path_);
        std::filesystem::remove(db_path_ + "-lock");
    }

    ExecutionResult compileAndExecute(const std::string& sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            std::string errors;
            for (const auto& err : compile_result.errors())
            {
                errors += err + "\n";
            }
            return ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(compile_result.bytecode());
    }

    std::unique_ptr<ConnectionContext> connectAs(const std::string& username, bool is_superuser = false)
    {
        EnsureUser(catalog_, username, public_schema_id_, is_superuser);

        ErrorContext ctx;
        CatalogManager::UserInfo user_info;
        auto status = catalog_->getUserByName(username, user_info, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to resolve user: " << ctx.message;

        std::unique_ptr<ConnectionContext> connection;
        status = db_.connect(connection, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to create connection";

        connection->setCurrentSchemaId(public_schema_id_);
        connection->setCurrentUser(user_info.user_id, is_superuser);
        ConnectionContext::setCurrent(connection.get());
        executor_->setConnectionContext(connection.get());
        return connection;
    }

    void clearConnection()
    {
        if (executor_)
        {
            executor_->setConnectionContext(nullptr);
        }
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
    }

    std::string db_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    ID public_schema_id_;
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
};

TEST_F(TempTableExecutorTest, TempTableUsesUserTempSchemaIfHomeExists)
{
    ErrorContext ctx;
    ID user_schema_id;
    auto status = catalog_->createSchemaPath("users.alice",
                                             CatalogManager::SchemaType::USER_HOME,
                                             user_schema_id,
                                             &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    connection_ctx_ = connectAs("alice");

    auto create_result = compileAndExecute("CREATE TEMP TABLE temp_user (id INT)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    CatalogManager::SchemaInfo temp_schema;
    status = catalog_->getSchema("users.alice.temp", temp_schema, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::TableInfo table_info;
    status = catalog_->getTable(temp_schema.schema_id, "temp_user", table_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(table_info.temp_metadata_scope, CatalogManager::TempMetadataScope::SESSION);
    EXPECT_EQ(table_info.temp_schema_id, temp_schema.schema_id);
}

TEST_F(TempTableExecutorTest, TempTableUsesPublicTempSchemaWhenNoUserHome)
{
    connection_ctx_ = connectAs("bob");

    auto create_result = compileAndExecute("CREATE TEMP TABLE temp_public (id INT)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    ErrorContext ctx;
    CatalogManager::SchemaInfo temp_schema;
    auto status = catalog_->getSchema("public.temp", temp_schema, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::TableInfo table_info;
    status = catalog_->getTable(temp_schema.schema_id, "temp_public", table_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(table_info.temp_metadata_scope, CatalogManager::TempMetadataScope::SESSION);
    EXPECT_EQ(table_info.temp_schema_id, temp_schema.schema_id);
}

TEST_F(TempTableExecutorTest, TempTableOnCommitPreserveRows)
{
    ErrorContext ctx;
    ID user_schema_id;
    auto status = catalog_->createSchemaPath("users.alice",
                                             CatalogManager::SchemaType::USER_HOME,
                                             user_schema_id,
                                             &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    connection_ctx_ = connectAs("alice");

    ASSERT_TRUE(compileAndExecute(
        "CREATE TEMP TABLE temp_preserve (id INT) ON COMMIT PRESERVE ROWS").success());
    auto insert_result = compileAndExecute("INSERT INTO temp_preserve VALUES (1)");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();
    auto commit_result = compileAndExecute("COMMIT");
    ASSERT_TRUE(commit_result.success()) << commit_result.error();

    auto select_result = compileAndExecute("SELECT * FROM temp_preserve");
    ASSERT_TRUE(select_result.hasResultSet()) << select_result.error();
    EXPECT_EQ(select_result.resultSet()->rowCount(), 1u);
}

TEST_F(TempTableExecutorTest, TempTableOnCommitDeleteRows)
{
    ErrorContext ctx;
    ID user_schema_id;
    auto status = catalog_->createSchemaPath("users.alice",
                                             CatalogManager::SchemaType::USER_HOME,
                                             user_schema_id,
                                             &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    connection_ctx_ = connectAs("alice");

    ASSERT_TRUE(compileAndExecute(
        "CREATE TEMP TABLE temp_delete (id INT) ON COMMIT DELETE ROWS").success());

    CatalogManager::SchemaInfo temp_schema;
    status = catalog_->getSchema("users.alice.temp", temp_schema, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::TableInfo table_info;
    status = catalog_->getTable(temp_schema.schema_id, "temp_delete", table_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(table_info.temp_on_commit, CatalogManager::TempOnCommitAction::DELETE_ROWS);

    auto insert_result = compileAndExecute("INSERT INTO temp_delete VALUES (1)");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();
    auto commit_result = compileAndExecute("COMMIT");
    ASSERT_TRUE(commit_result.success()) << commit_result.error();

    auto select_result = compileAndExecute("SELECT * FROM temp_delete");
    ASSERT_TRUE(select_result.hasResultSet()) << select_result.error();
    EXPECT_EQ(select_result.resultSet()->rowCount(), 0u);
}

TEST_F(TempTableExecutorTest, TempTableSessionCleanupDropsMetadata)
{
    ErrorContext ctx;
    ID user_schema_id;
    auto status = catalog_->createSchemaPath("users.alice",
                                             CatalogManager::SchemaType::USER_HOME,
                                             user_schema_id,
                                             &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    connection_ctx_ = connectAs("alice");
    auto session_id = connection_ctx_->effectiveSessionId();

    ASSERT_TRUE(compileAndExecute(
        "CREATE TEMP TABLE temp_session (id INT) ON COMMIT PRESERVE ROWS").success());
    auto insert_result = compileAndExecute("INSERT INTO temp_session VALUES (1)");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();
    auto commit_result = compileAndExecute("COMMIT");
    ASSERT_TRUE(commit_result.success()) << commit_result.error();

    clearConnection();

    std::vector<CatalogManager::TableInfo> tables;
    status = catalog_->listTemporaryTablesForSession(session_id, tables, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    auto it = std::find_if(tables.begin(), tables.end(),
                           [](const CatalogManager::TableInfo& info)
                           {
                               return info.table_name == "temp_session";
                           });
    EXPECT_EQ(it, tables.end()) << "Temp table metadata should be dropped on session end";
}
