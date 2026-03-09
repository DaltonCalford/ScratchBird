/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/table_stats_manager.h"
#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::optimizer;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

namespace
{
    auto findColumnId(const std::vector<CatalogManager::ColumnInfo>& columns,
                      const std::string& name) -> ID
    {
        for (const auto& column : columns)
        {
            if (core::IdentifierUtils::namesMatch(column.column_name, false, name, false))
            {
                return column.column_id;
            }
        }
        return {};
    }
}

class StatisticsManagerRuntimeTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.reset();
        db_file_.reset();
    }

    bool createDatabase()
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_statistics_manager_runtime");

        ErrorContext ctx;
        Status status = Database::create(db_file_->path(), 16384, &ctx);
        if (status != Status::OK)
        {
            return false;
        }

        db_ = std::make_unique<Database>();
        status = db_->open(db_file_->path(), &ctx);
        if (status != Status::OK)
        {
            return false;
        }

        auto* catalog = db_->catalog_manager();
        if (catalog == nullptr)
        {
            return false;
        }

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
        executor_ = std::make_unique<Executor>(db_.get());

        CatalogManager::SchemaInfo public_schema;
        status = catalog->getSchema("public", public_schema, &ctx);
        if (status != Status::OK)
        {
            return false;
        }
        public_schema_id_ = public_schema.schema_id;

        status = db_->connect(connection_ctx_, &ctx);
        if (status != Status::OK)
        {
            return false;
        }
        connection_ctx_->setCurrentSchemaId(public_schema_id_);
        const auto system_user_id = catalog->getSystemUserId(&ctx);
        if (system_user_id == ID{})
        {
            return false;
        }
        connection_ctx_->setCurrentUser(system_user_id, true);
        ConnectionContext::setCurrent(connection_ctx_.get());
        executor_->setConnectionContext(connection_ctx_.get());
        return true;
    }

    bool reopenDatabase()
    {
        executor_.reset();
        compiler_.reset();
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.reset();

        ErrorContext ctx;
        db_ = std::make_unique<Database>();
        Status status = db_->open(db_file_->path(), &ctx);
        if (status != Status::OK)
        {
            return false;
        }

        auto* catalog = db_->catalog_manager();
        if (catalog == nullptr)
        {
            return false;
        }

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
        executor_ = std::make_unique<Executor>(db_.get());

        CatalogManager::SchemaInfo public_schema;
        status = catalog->getSchema("public", public_schema, &ctx);
        if (status != Status::OK)
        {
            return false;
        }
        public_schema_id_ = public_schema.schema_id;

        status = db_->connect(connection_ctx_, &ctx);
        if (status != Status::OK)
        {
            return false;
        }
        connection_ctx_->setCurrentSchemaId(public_schema_id_);
        const auto system_user_id = catalog->getSystemUserId(&ctx);
        if (system_user_id == ID{})
        {
            return false;
        }
        connection_ctx_->setCurrentUser(system_user_id, true);
        ConnectionContext::setCurrent(connection_ctx_.get());
        executor_->setConnectionContext(connection_ctx_.get());
        return true;
    }

    ExecutionResult executeSQL(const std::string& sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            return ExecutionResult("Compilation failed");
        }
        return executor_->execute(compile_result.bytecode());
    }

    bool lookupTable(const std::string& table_name,
                     CatalogManager::TableInfo& table_info,
                     std::vector<CatalogManager::ColumnInfo>& columns)
    {
        ErrorContext ctx;
        if (db_->catalog_manager()->getTable(public_schema_id_, table_name, table_info, &ctx) !=
            Status::OK)
        {
            return false;
        }
        return db_->catalog_manager()->getColumns(table_info.table_id, columns, &ctx) ==
            Status::OK;
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
    ID public_schema_id_{};
};

TEST_F(StatisticsManagerRuntimeTest, AutoAnalyzeBuildsCorrelationAndExpressionStatsAndDropClearsThem)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE TABLE stats_users (id INTEGER, score INTEGER, city VARCHAR(64))")
            .success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON stats_users TO PUBLIC").success());

    for (int i = 1; i <= 256; ++i)
    {
        const std::string city = (i % 2 == 0) ? "Seattle" : "Austin";
        ASSERT_TRUE(executeSQL("INSERT INTO stats_users (id, score, city) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(i * 10) + ", '" +
                               city + "')")
                        .success());
    }

    CatalogManager::TableInfo table_info;
    std::vector<CatalogManager::ColumnInfo> columns;
    ASSERT_TRUE(lookupTable("stats_users", table_info, columns));

    const ID id_column = findColumnId(columns, "id");
    const ID score_column = findColumnId(columns, "score");
    ASSERT_NE(id_column, ID{});
    ASSERT_NE(score_column, ID{});

    ColumnStatistics id_stats;
    ErrorContext stats_ctx;
    ASSERT_EQ(db_->statistics_manager()->getColumnStatistics(
                  table_info.table_id, id_column, id_stats, &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    EXPECT_GT(id_stats.last_analyzed_time, 0u);
    EXPECT_GT(id_stats.sample_size, 0u);

    if (auto* table_stats_manager = db_->table_stats_manager())
    {
        TableStatsSnapshot snapshot;
        ASSERT_TRUE(table_stats_manager->snapshotForTable(table_info.table_id, snapshot));
        EXPECT_GT(snapshot.autoanalyze_count, 0u);
    }

    ColumnCorrelationStatistics correlation;
    ASSERT_EQ(db_->statistics_manager()->getColumnCorrelation(
                  table_info.table_id, id_column, score_column, correlation, &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    EXPECT_GT(correlation.coefficient, 0.90);

    ExpressionStatistics expression_stats;
    ASSERT_EQ(db_->statistics_manager()->getExpressionStatistics(
                  table_info.table_id, "LOWER(city)", expression_stats, &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    EXPECT_FALSE(expression_stats.stats.mcv_list.empty());
    EXPECT_GT(expression_stats.stats.num_distinct, 0u);

    ASSERT_TRUE(reopenDatabase());
    ASSERT_TRUE(lookupTable("stats_users", table_info, columns));

    const ID reopened_id_column = findColumnId(columns, "id");
    const ID reopened_score_column = findColumnId(columns, "score");
    ASSERT_NE(reopened_id_column, ID{});
    ASSERT_NE(reopened_score_column, ID{});

    ColumnCorrelationStatistics persisted_correlation;
    ASSERT_EQ(db_->statistics_manager()->getColumnCorrelation(
                  table_info.table_id,
                  reopened_id_column,
                  reopened_score_column,
                  persisted_correlation,
                  &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    EXPECT_GT(persisted_correlation.coefficient, 0.90);

    ExpressionStatistics persisted_expression_stats;
    ASSERT_EQ(db_->statistics_manager()->getExpressionStatistics(
                  table_info.table_id,
                  "LOWER(city)",
                  persisted_expression_stats,
                  &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    EXPECT_GT(persisted_expression_stats.stats.num_distinct, 0u);

    ASSERT_EQ(db_->statistics_manager()->dropStatistics(table_info.table_id, &stats_ctx), Status::OK)
        << stats_ctx.message;

    ExpressionStatistics missing_expression_stats;
    EXPECT_NE(db_->statistics_manager()->getExpressionStatistics(
                  table_info.table_id,
                  "LOWER(city)",
                  missing_expression_stats,
                  &stats_ctx),
              Status::OK);
}
