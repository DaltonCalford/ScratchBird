/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Query result cache tests (SBLR executor path).
 */

#include <gtest/gtest.h>
#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_result_cache.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/domain_manager.h"
#include "unit/test_user_helpers.h"
#include <filesystem>
#include <sstream>
#include <thread>
#include <chrono>

using namespace scratchbird::sblr;
using namespace scratchbird::core;

static std::string generateUniqueDbPath() {
    std::ostringstream oss;
    oss << "/tmp/test_query_result_cache_"
        << std::this_thread::get_id() << "_"
        << std::chrono::steady_clock::now().time_since_epoch().count()
        << ".sbdb";
    return oss.str();
}

class QueryResultCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = generateUniqueDbPath();
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database";

        status = db_.open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database";

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr) << "CatalogManager is null";

        CatalogManager::SchemaInfo public_schema_info;
        status = catalog_->getSchema("public", public_schema_info, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to resolve public schema: " << ctx.message;
        public_schema_id_ = public_schema_info.schema_id;

        EnsureUser(catalog_, "test_user");

        compiler_ = std::make_unique<QueryCompilerV2>(&db_);
        executor_ = std::make_unique<Executor>(&db_);

        status = db_.connect(connection_ctx_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create connection: " << ctx.message;
        connection_ctx_->setCurrentSchemaId(public_schema_id_);
        auto system_user_id = catalog_->getSystemUserId(&ctx);
        connection_ctx_->setCurrentUser(system_user_id, true);
        ConnectionContext::setCurrent(connection_ctx_.get());
        executor_->setConnectionContext(connection_ctx_.get());

        auto& cache = QueryResultCacheManager::getInstance();
        cache.setEnabled(true);
        cache.invalidateAll();
        cache.resetStatistics();
    }

    void TearDown() override {
        auto& cache = QueryResultCacheManager::getInstance();
        cache.invalidateAll();
        cache.resetStatistics();

        compiler_.reset();
        executor_.reset();
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.close();
        std::filesystem::remove(test_db_path_);
        std::filesystem::remove(test_db_path_ + "-lock");
    }

    ExecutionResult compileAndExecute(const std::string& sql) {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success()) {
            std::string errors;
            for (const auto& err : compile_result.errors()) {
                errors += err + "\n";
            }
            return ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(compile_result.bytecode());
    }

    std::string test_db_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    ID public_schema_id_;
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
};

TEST_F(QueryResultCacheTest, SelectCachesAndReusesResult) {
    ASSERT_TRUE(compileAndExecute("CREATE TABLE t (id INT, value INT)").success());
    ASSERT_TRUE(compileAndExecute("INSERT INTO t VALUES (1, 10)").success());

    auto& cache = QueryResultCacheManager::getInstance();
    cache.invalidateAll();
    cache.resetStatistics();

    auto compile_result = compiler_->compile("SELECT value FROM t WHERE id = 1");
    ASSERT_TRUE(compile_result.success()) << "Compilation failed";
    const auto& bytecode = compile_result.bytecode();
    ASSERT_GT(bytecode.size(), 2u);
    EXPECT_TRUE(bytecode[2] == static_cast<uint8_t>(Opcode::SELECT) ||
                bytecode[2] == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));

    auto first = executor_->execute(bytecode);
    ASSERT_TRUE(first.success());
    ASSERT_TRUE(first.hasResultSet());
    ASSERT_EQ(first.resultSet()->rowCount(), 1u);
    EXPECT_EQ(first.resultSet()->getValue(0, 0).toInt64(), 10);

    auto stats_after_first = cache.getStatistics();
    EXPECT_EQ(stats_after_first.misses, 1u);
    EXPECT_EQ(stats_after_first.hits, 0u);
    EXPECT_EQ(stats_after_first.insertions, 1u);

    auto second = executor_->execute(bytecode);
    ASSERT_TRUE(second.success());
    ASSERT_TRUE(second.hasResultSet());
    ASSERT_EQ(second.resultSet()->rowCount(), 1u);
    EXPECT_EQ(second.resultSet()->getValue(0, 0).toInt64(), 10);

    auto stats_after_second = cache.getStatistics();
    EXPECT_EQ(stats_after_second.hits, 1u);
    EXPECT_EQ(stats_after_second.misses, 1u);
}

TEST_F(QueryResultCacheTest, DmlInvalidatesCachedResults) {
    ASSERT_TRUE(compileAndExecute("CREATE TABLE t (id INT, value INT)").success());
    ASSERT_TRUE(compileAndExecute("INSERT INTO t VALUES (1, 10)").success());

    auto& cache = QueryResultCacheManager::getInstance();
    cache.invalidateAll();
    cache.resetStatistics();

    auto compile_result = compiler_->compile("SELECT value FROM t WHERE id = 1");
    ASSERT_TRUE(compile_result.success()) << "Compilation failed";
    const auto& bytecode = compile_result.bytecode();
    ASSERT_GT(bytecode.size(), 2u);
    EXPECT_TRUE(bytecode[2] == static_cast<uint8_t>(Opcode::SELECT) ||
                bytecode[2] == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));

    auto first = executor_->execute(bytecode);
    ASSERT_TRUE(first.success());
    ASSERT_TRUE(first.hasResultSet());

    auto stats_after_first = cache.getStatistics();
    EXPECT_EQ(stats_after_first.misses, 1u);
    EXPECT_EQ(stats_after_first.insertions, 1u);

    ASSERT_TRUE(compileAndExecute("UPDATE t SET value = 12 WHERE id = 1").success());

    auto stats_after_update = cache.getStatistics();
    EXPECT_EQ(stats_after_update.invalidations, 1u);

    auto second = executor_->execute(bytecode);
    ASSERT_TRUE(second.success());
    ASSERT_TRUE(second.hasResultSet());
    ASSERT_EQ(second.resultSet()->rowCount(), 1u);
    EXPECT_EQ(second.resultSet()->getValue(0, 0).toInt64(), 12);

    auto stats_after_second = cache.getStatistics();
    EXPECT_EQ(stats_after_second.misses, 2u);
    EXPECT_EQ(stats_after_second.hits, 0u);
}
