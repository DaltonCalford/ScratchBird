/**
 * Cache integration tests (hit/miss ratios on repeated workloads).
 */

#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/pool/statement_cache.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/query_result_cache.h"
#include "test_helpers.h"

#include <chrono>
#include <memory>
#include <string>

using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::pool::DatabaseStatementCache;
using scratchbird::pool::StatementCacheConfig;
using scratchbird::pool::StatementMetadata;
using scratchbird::pool::StatementType;
using scratchbird::sblr::Executor;
using scratchbird::sblr::QueryCompilerV2;
using scratchbird::sblr::QueryResultCacheManager;
using scratchbird::testing::TestDatabaseFile;

class CacheIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("test_cache_integration");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK)
            << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), Status::OK)
            << "Failed to open database: " << ctx.message;

        auto status = scratchbird::core::ProcArrayManager::initialize(db_.get(), 10, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to initialize ProcArray: " << ctx.message;

        status = scratchbird::core::ProcArrayManager::registerBackend(&proc_id_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to register backend: " << ctx.message;

        conn_ctx_ = std::make_unique<scratchbird::core::ConnectionContext>(db_.get(), proc_id_);
        status = conn_ctx_->initialize(&ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to initialize connection context: " << ctx.message;

        scratchbird::core::CatalogManager::SchemaInfo schema;
        ASSERT_EQ(db_->catalog_manager()->getSchema("PUBLIC", schema, &ctx), Status::OK)
            << "Failed to get PUBLIC schema: " << ctx.message;
        schema_id_ = schema.schema_id;

        compiler_ = std::make_unique<QueryCompilerV2>(db_.get());
        compiler_->setCurrentSchema(schema_id_);

        executor_ = std::make_unique<Executor>(db_.get());
        executor_->setConnectionContext(conn_ctx_.get());
        executor_->setCurrentSchema(schema_id_);
        conn_ctx_->setCurrentSchemaId(schema_id_);
        scratchbird::core::ConnectionContext::setCurrent(conn_ctx_.get());

        auto& result_cache = QueryResultCacheManager::getInstance();
        result_cache.setEnabled(true);
        result_cache.invalidateAll();
        result_cache.resetStatistics();
    }

    void TearDown() override {
        executor_.reset();
        compiler_.reset();
        scratchbird::core::ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();

        ErrorContext ctx;
        scratchbird::core::ProcArrayManager::unregisterBackend(proc_id_, &ctx);
        scratchbird::core::ProcArrayManager::shutdown(&ctx);

        db_.reset();
        db_file_.reset();
    }

    scratchbird::sblr::ExecutionResult executeSQL(const std::string& sql) {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success()) {
            if (!compile_result.errors().empty()) {
                return scratchbird::sblr::ExecutionResult(compile_result.errors().front());
            }
            return scratchbird::sblr::ExecutionResult("Compile error");
        }
        return executor_->execute(compile_result.bytecode());
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<scratchbird::core::ConnectionContext> conn_ctx_;
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
    scratchbird::core::ID schema_id_{};
    uint32_t proc_id_{0};
};

TEST_F(CacheIntegrationTest, ResultCacheHitRatioOnRepeatedSelects) {
    ASSERT_TRUE(executeSQL("CREATE TABLE cache_items (id INT, value INT)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO cache_items VALUES (1, 10)").success());

    auto& cache = QueryResultCacheManager::getInstance();
    cache.invalidateAll();
    cache.resetStatistics();

    auto first = executeSQL("SELECT value FROM cache_items WHERE id = 1");
    ASSERT_TRUE(first.success());
    ASSERT_TRUE(first.hasResultSet());

    auto second = executeSQL("SELECT value FROM cache_items WHERE id = 1");
    ASSERT_TRUE(second.success());
    ASSERT_TRUE(second.hasResultSet());

    auto stats = cache.getStatistics();
    EXPECT_GE(stats.misses, 1u);
    EXPECT_GE(stats.hits, 1u);
    double total = static_cast<double>(stats.hits + stats.misses);
    double hit_ratio = total > 0.0 ? static_cast<double>(stats.hits) / total : 0.0;
    EXPECT_GT(hit_ratio, 0.0);
}

TEST_F(CacheIntegrationTest, StatementCacheHitRatioOnRepeatedLookups) {
    StatementCacheConfig config;
    config.max_statements = 128;
    config.min_statement_size = 1;
    DatabaseStatementCache stmt_cache("test_cache_integration", config);

    const std::string sql = "SELECT value FROM cache_items WHERE id = 1";
    auto miss = stmt_cache.get(sql);
    EXPECT_EQ(miss, nullptr);

    StatementMetadata meta;
    meta.statement_type = StatementType::SELECT;
    meta.referenced_tables = {"cache_items"};
    auto statement = std::make_shared<scratchbird::pool::CachedStatement>(sql, meta);
    ASSERT_TRUE(stmt_cache.put(statement));

    auto hit = stmt_cache.get(sql);
    ASSERT_NE(hit, nullptr);

    const auto& stats = stmt_cache.statistics();
    EXPECT_GE(stats.total_misses, 1u);
    EXPECT_GE(stats.total_hits, 1u);
    EXPECT_GT(stats.hit_ratio, 0.0);
}
