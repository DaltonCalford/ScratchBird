/**
 * LSM-Tree SQL Integration Tests (GTest)
 *
 * Validates:
 * - CREATE INDEX ... USING LSM
 * - INSERT with LSM index present
 * - Multiple index types in one table
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <string>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "test_helpers.h"

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::sblr;

class LsmSqlIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_path_ = scratchbird::testing::uniqueTestDbPath("lsm_sql_integration", ".db");
        std::filesystem::remove(db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;

        Status status = db_->initializeProcArray(16, &ctx);
        if (status != Status::OK && status != Status::INVALID_ARGUMENT)
        {
            ASSERT_EQ(status, Status::OK) << ctx.message;
        }

        ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_ctx_.get());
        ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK) << ctx.message;

        ID system_user = db_->catalog_manager()->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user, true);

        schema_id_ = resolveDefaultSchema(&ctx);
        ASSERT_NE(schema_id_, ID{});
        conn_ctx_->setCurrentSchemaId(schema_id_);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        db_.reset();
        std::filesystem::remove(db_path_);
    }

    ID resolveDefaultSchema(ErrorContext *ctx)
    {
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = db_->catalog_manager()->listSchemas(schemas, ctx);
        if (status == Status::OK && !schemas.empty())
        {
            return schemas.front().schema_id;
        }

        ID schema_id;
        status = db_->catalog_manager()->createSchema("public", "SYSTEM", schema_id, ctx);
        if (status == Status::OK)
        {
            return schema_id;
        }
        return ID{};
    }

    void execSQL(const std::string &sql)
    {
        QueryCompilerV2 compiler(db_.get());
        compiler.setCurrentSchema(schema_id_);
        auto compile_result = compiler.compile(sql);
        ASSERT_TRUE(compile_result.success())
            << (compile_result.errors().empty() ? "compile failed" : compile_result.errors().front());

        Executor executor(db_.get());
        executor.setConnectionContext(conn_ctx_.get());
        executor.setCurrentSchema(schema_id_);
        auto exec_result = executor.execute(compile_result.bytecode());
        ASSERT_TRUE(exec_result.success()) << exec_result.error();
    }

    std::string db_path_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<ConnectionContext> conn_ctx_;
    ID schema_id_{};
};

TEST_F(LsmSqlIntegrationTest, CreateLsmIndex)
{
    execSQL("CREATE TABLE users (id INT32, email VARCHAR(100))");
    execSQL("CREATE INDEX idx_email ON users USING LSM (email)");

    ErrorContext ctx;
    CatalogManager *catalog = db_->catalog_manager();

    CatalogManager::TableInfo table_info;
    ASSERT_EQ(catalog->getTable(schema_id_, "users", table_info, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::IndexInfo index_info;
    ASSERT_EQ(catalog->getIndex(table_info.table_id, "idx_email", index_info, &ctx), Status::OK)
        << ctx.message;

    EXPECT_EQ(index_info.index_type, CatalogManager::IndexType::LSM);

    CatalogManager::IndexType index_type{};
    void *index_ptr = catalog->getIndexPtr(index_info.index_id, &index_type);
    EXPECT_NE(index_ptr, nullptr);
    EXPECT_EQ(index_type, CatalogManager::IndexType::LSM);
}

TEST_F(LsmSqlIntegrationTest, InsertWithLsmIndex)
{
    execSQL("CREATE TABLE users (id INT32, email VARCHAR(100))");
    execSQL("CREATE INDEX idx_email ON users USING LSM (email)");
    execSQL("INSERT INTO users (id, email) VALUES (1, 'alice@example.com')");
}

TEST_F(LsmSqlIntegrationTest, MultipleIndexTypes)
{
    execSQL("CREATE TABLE users (id INT32, email VARCHAR(100), name VARCHAR(100))");
    execSQL("CREATE INDEX idx_id ON users USING BTREE (id)");
    execSQL("CREATE INDEX idx_email ON users USING LSM (email)");
    execSQL("CREATE INDEX idx_name ON users (name)");

    ErrorContext ctx;
    CatalogManager *catalog = db_->catalog_manager();

    CatalogManager::TableInfo table_info;
    ASSERT_EQ(catalog->getTable(schema_id_, "users", table_info, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::IndexInfo idx_id{};
    CatalogManager::IndexInfo idx_email{};
    CatalogManager::IndexInfo idx_name{};
    ASSERT_EQ(catalog->getIndex(table_info.table_id, "idx_id", idx_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog->getIndex(table_info.table_id, "idx_email", idx_email, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog->getIndex(table_info.table_id, "idx_name", idx_name, &ctx), Status::OK)
        << ctx.message;

    EXPECT_EQ(idx_id.index_type, CatalogManager::IndexType::BTREE);
    EXPECT_EQ(idx_email.index_type, CatalogManager::IndexType::LSM);
    EXPECT_EQ(idx_name.index_type, CatalogManager::IndexType::BTREE);

    execSQL("INSERT INTO users (id, email, name) VALUES (1, 'alice@example.com', 'Alice')");
}
