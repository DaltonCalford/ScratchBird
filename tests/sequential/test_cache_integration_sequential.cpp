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
 * Cache Integration Sequential Test Suite
 * 
 * This test wraps all cache integration tests to run them sequentially,
 * avoiding parallel execution conflicts on the shared QueryResultCacheManager singleton.
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/pool/statement_cache.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/query_result_cache.h"
#include "test_helpers.h"

using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::core::ID;
using scratchbird::pool::DatabaseStatementCache;
using scratchbird::pool::StatementCacheConfig;
using scratchbird::pool::StatementMetadata;
using scratchbird::pool::StatementType;
using scratchbird::sblr::Executor;
using scratchbird::sblr::QueryCompilerV3;
using scratchbird::sblr::QueryResultCacheManager;
using scratchbird::testing::TestDatabaseFile;

/**
 * Sequential Cache Integration Test Suite
 * All tests run in a single test case to ensure sequential execution
 */
TEST(CacheIntegrationSequentialSuite, AllTests)
{
    static std::atomic<uint64_t> test_counter{0};
    uint64_t counter = test_counter.fetch_add(1);
    std::string table_suffix = std::to_string(counter);

    // ===== Test 1: ResultCacheHitRatioOnRepeatedSelects =====
    {
        SCOPED_TRACE("ResultCacheHitRatioOnRepeatedSelects");
        
        auto db_file = std::make_unique<TestDatabaseFile>("test_cache_integration");
        ErrorContext ctx;
        
        ASSERT_EQ(Database::create(db_file->path(), 16384, &ctx), Status::OK);
        
        auto db = std::make_unique<Database>();
        ASSERT_EQ(db->open(db_file->path(), &ctx), Status::OK);
        
        auto status = scratchbird::core::ProcArrayManager::initialize(db.get(), 10, &ctx);
        ASSERT_EQ(status, Status::OK);
        
        uint32_t proc_id = 0;
        status = scratchbird::core::ProcArrayManager::registerBackend(&proc_id, &ctx);
        ASSERT_EQ(status, Status::OK);
        
        auto conn_ctx = std::make_unique<scratchbird::core::ConnectionContext>(db.get(), proc_id);
        status = conn_ctx->initialize(&ctx);
        ASSERT_EQ(status, Status::OK);
        
        scratchbird::core::CatalogManager::SchemaInfo schema;
        ASSERT_EQ(db->catalog_manager()->getSchema("PUBLIC", schema, &ctx), Status::OK);
        ID schema_id = schema.schema_id;
        
        auto compiler = std::make_unique<QueryCompilerV3>(db.get());
        compiler->setCurrentSchema(schema_id);
        
        auto executor = std::make_unique<Executor>(db.get());
        executor->setConnectionContext(conn_ctx.get());
        executor->setCurrentSchema(schema_id);
        conn_ctx->setCurrentSchemaId(schema_id);
        scratchbird::core::ConnectionContext::setCurrent(conn_ctx.get());
        
        auto& result_cache = QueryResultCacheManager::getInstance();
        result_cache.setEnabled(true);
        result_cache.invalidateAll();
        
        // Use unique table name
        std::string table_name = "cache_items_" + table_suffix;
        
        auto executeSQL = [&](const std::string& sql) -> scratchbird::sblr::ExecutionResult {
            auto compile_result = compiler->compile(sql);
            if (!compile_result.success()) {
                if (!compile_result.errors().empty()) {
                    return scratchbird::sblr::ExecutionResult(compile_result.errors().front());
                }
                return scratchbird::sblr::ExecutionResult("Compile error");
            }
            return executor->execute(compile_result.bytecode());
        };
        
        ASSERT_TRUE(executeSQL("CREATE TABLE " + table_name + " (id INT, value INT)").success());
        ASSERT_TRUE(executeSQL("INSERT INTO " + table_name + " VALUES (1, 10)").success());
        
        auto& cache = QueryResultCacheManager::getInstance();
        cache.invalidateAll();
        
        auto baseline_stats = cache.getStatistics();
        uint64_t start_hits = baseline_stats.hits;
        uint64_t start_misses = baseline_stats.misses;
        
        auto first = executeSQL("SELECT value FROM " + table_name + " WHERE id = 1");
        ASSERT_TRUE(first.success());
        ASSERT_TRUE(first.hasResultSet());
        
        auto second = executeSQL("SELECT value FROM " + table_name + " WHERE id = 1");
        ASSERT_TRUE(second.success());
        ASSERT_TRUE(second.hasResultSet());
        
        auto stats = cache.getStatistics();
        uint64_t new_hits = stats.hits > start_hits ? stats.hits - start_hits : 0;
        uint64_t new_misses = stats.misses > start_misses ? stats.misses - start_misses : 0;
        
        EXPECT_GE(new_misses, 1u);
        if (new_hits + new_misses >= 2) {
            double hit_ratio = static_cast<double>(new_hits) / (new_hits + new_misses);
            EXPECT_GT(hit_ratio, 0.0);
        }
        
        scratchbird::core::ConnectionContext::setCurrent(nullptr);
        scratchbird::core::ProcArrayManager::unregisterBackend(proc_id, &ctx);
        scratchbird::core::ProcArrayManager::shutdown(&ctx);
    }

    // ===== Test 2: StatementCacheHitRatioOnRepeatedLookups =====
    {
        SCOPED_TRACE("StatementCacheHitRatioOnRepeatedLookups");
        
        StatementCacheConfig config;
        config.max_statements = 128;
        config.min_statement_size = 1;
        // Use unique cache name to avoid interference
        DatabaseStatementCache stmt_cache("test_cache_integration_" + table_suffix, config);
        
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
}
