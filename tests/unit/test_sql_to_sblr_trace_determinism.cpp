/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "scratchbird/core/database.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::sblr::QueryCompilerV3;
using scratchbird::testing::TestDatabaseFile;

class SqlToSblrTraceDeterminismTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("sql_to_sblr_trace_determinism");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
    }

    QueryCompilerV3::TraceResult trace(const std::string& sql) {
        auto result = compiler_->compileTrace(sql);
        if (!result.success()) {
            for (const auto& error : result.errors()) {
                ADD_FAILURE() << "compileTrace error: " << error;
            }
        }
        return result;
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<QueryCompilerV3> compiler_;
};

TEST_F(SqlToSblrTraceDeterminismTest, NormalizationMakesEquivalentSqlStable) {
    const std::string canonical =
        "CREATE VECTOR INDEX idx_vec ON docs (embedding) METRIC COSINE TOPK_DEFAULT 25";
    const std::string variant =
        "  CREATE   VECTOR   INDEX idx_vec  ON docs (embedding)  METRIC COSINE   TOPK_DEFAULT 25  ";

    auto trace_a = trace(canonical);
    auto trace_b = trace(variant);
    ASSERT_TRUE(trace_a.success());
    ASSERT_TRUE(trace_b.success());

    EXPECT_EQ(trace_a.digest().normalized_sql, trace_b.digest().normalized_sql);
    EXPECT_EQ(trace_a.digest().sql_hash, trace_b.digest().sql_hash);
    EXPECT_EQ(trace_a.digest().ast_hash, trace_b.digest().ast_hash);
    EXPECT_EQ(trace_a.digest().sblr_hash, trace_b.digest().sblr_hash);
    EXPECT_EQ(trace_a.digest().root_opcode_symbol, "OP_STMT_DDL_CREATE_INDEX");
}

TEST_F(SqlToSblrTraceDeterminismTest, RepeatCompileHasIdenticalDigests) {
    const std::string sql =
        "CREATE SCHEDULE sch_daily RRULE 'FREQ=DAILY;INTERVAL=1' "
        "DTSTART '2026-02-17T00:00:00' TZ 'UTC'";

    auto trace_a = trace(sql);
    auto trace_b = trace(sql);
    ASSERT_TRUE(trace_a.success());
    ASSERT_TRUE(trace_b.success());

    EXPECT_EQ(trace_a.digest().sql_hash, trace_b.digest().sql_hash);
    EXPECT_EQ(trace_a.digest().ast_hash, trace_b.digest().ast_hash);
    EXPECT_EQ(trace_a.digest().sblr_hash, trace_b.digest().sblr_hash);
    EXPECT_EQ(trace_a.digest().root_opcode_symbol, trace_b.digest().root_opcode_symbol);
}

TEST_F(SqlToSblrTraceDeterminismTest, DistinctStatementsProduceDistinctSblrDigests) {
    auto trace_index = trace("CREATE SEARCH INDEX idx_search ON docs (title, body)");
    auto trace_token =
        trace("CREATE TOKEN ifx_reader WITH SCOPE (ALLOW BUCKET 'cpu_metrics' ACTION READ)");
    ASSERT_TRUE(trace_index.success());
    ASSERT_TRUE(trace_token.success());

    EXPECT_NE(trace_index.digest().sblr_hash, trace_token.digest().sblr_hash);
    EXPECT_NE(trace_index.digest().ast_hash, trace_token.digest().ast_hash);
}

TEST_F(SqlToSblrTraceDeterminismTest, NativeExtensionCorpusCompilesToDeterministicDigests) {
    const bool dump_trace_hashes = std::getenv("SB_TRACE_DUMP") != nullptr;
    const std::vector<std::string> corpus = {
        "CREATE SEARCH INDEX idx_search ON docs (title, body)",
        "CREATE VECTOR INDEX idx_vec ON docs (embedding) METRIC COSINE TOPK_DEFAULT 25",
        "ALTER SEARCH INDEX idx_search REBUILD ONLINE",
        "CREATE MEASUREMENT cpu (host STRING, usage_user DOUBLE)",
        "ALTER MEASUREMENT cpu RETENTION '30d'",
        "CREATE SCHEDULE sch_daily RRULE 'FREQ=DAILY;INTERVAL=1' DTSTART '2026-02-17T00:00:00' TZ 'UTC'",
        "CREATE CONNECTION RULE ch_src ORDER 5 MATCH (TRANSPORT=TLS, SOURCE='10.0.0.0/8', PRINCIPAL='ch_%') "
        "REQUIRE (TLS=TLS, PROVIDER=INTERNAL) ACTION ALLOW EXPECT VERSION 1",
        "CREATE TOKEN ifx_reader WITH SCOPE (ALLOW BUCKET 'cpu_metrics' ACTION READ)",
        "CREATE QUOTA PROFILE q1 (MAX_REQUESTS_PER_SEC=1000, WINDOW_MS=1000)",
        "REVOKE TOKEN ifx_reader"};

    for (const auto& sql : corpus) {
        auto first = trace(sql);
        auto second = trace(sql);
        ASSERT_TRUE(first.success()) << sql;
        ASSERT_TRUE(second.success()) << sql;

        EXPECT_FALSE(first.digest().sql_hash.empty()) << sql;
        EXPECT_FALSE(first.digest().ast_hash.empty()) << sql;
        EXPECT_FALSE(first.digest().sblr_hash.empty()) << sql;
        EXPECT_FALSE(first.digest().root_opcode_symbol.empty()) << sql;

        EXPECT_EQ(first.digest().sql_hash, second.digest().sql_hash) << sql;
        EXPECT_EQ(first.digest().ast_hash, second.digest().ast_hash) << sql;
        EXPECT_EQ(first.digest().sblr_hash, second.digest().sblr_hash) << sql;

        if (dump_trace_hashes) {
            std::cout << first.digest().normalized_sql << ","
                      << first.digest().sql_hash << ","
                      << first.digest().ast_hash << ","
                      << first.digest().sblr_hash << ","
                      << first.digest().root_opcode_symbol << "\n";
        }
    }
}
