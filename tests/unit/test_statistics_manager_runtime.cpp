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
#include "scratchbird/optimizer/selectivity_estimator.h"
#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/parser/ast_v3.h"
#include "scratchbird/parser/lexer_v3.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

#include <algorithm>
#include <cmath>
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

    auto encodeLengthPrefixedValue(const std::string &text) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> out(sizeof(uint32_t) + text.size());
        const uint32_t len = static_cast<uint32_t>(text.size());
        std::memcpy(out.data(), &len, sizeof(len));
        if (!text.empty())
        {
            std::memcpy(out.data() + sizeof(uint32_t), text.data(), text.size());
        }
        return out;
    }

    template <typename T>
    auto encodeScalarValue(T value) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> out(sizeof(T));
        std::memcpy(out.data(), &value, sizeof(T));
        return out;
    }

    auto makeColumnRefExpr(parser::v3::StringPool &pool,
                           const std::string &name)
        -> std::unique_ptr<parser::v3::ColumnRefExpr>
    {
        auto expr = std::make_unique<parser::v3::ColumnRefExpr>();
        expr->column = parser::v3::ColumnRef(pool.intern(name));
        return expr;
    }

    auto makeStringLiteralExpr(parser::v3::StringPool &pool,
                               const std::string &value)
        -> std::unique_ptr<parser::v3::LiteralExpr>
    {
        auto expr = std::make_unique<parser::v3::LiteralExpr>();
        expr->literal_type = parser::v3::LiteralType::STRING;
        expr->string_value = pool.intern(value);
        return expr;
    }

    auto makeParameterExpr(uint32_t index) -> std::unique_ptr<parser::v3::ParameterExpr>
    {
        auto expr = std::make_unique<parser::v3::ParameterExpr>();
        expr->is_named = false;
        expr->index = index;
        return expr;
    }

    auto makeBinaryExpr(parser::v3::BinaryOp op,
                        parser::v3::Expression *left,
                        parser::v3::Expression *right)
        -> std::unique_ptr<parser::v3::BinaryExpr>
    {
        auto expr = std::make_unique<parser::v3::BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = right;
        return expr;
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

TEST_F(StatisticsManagerRuntimeTest, LongStringStatsPersistWithoutTruncationAndEstimateEquality)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE TABLE stats_long_strings (payload VARCHAR(1024))").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON stats_long_strings TO PUBLIC").success());

    const std::string prefix(300, 'a');
    const std::string long_a = prefix + "A_tail";
    const std::string long_b = prefix + "B_tail";
    for (int i = 0; i < 60; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO stats_long_strings (payload) VALUES ('" + long_a + "')")
                        .success());
    }
    for (int i = 0; i < 40; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO stats_long_strings (payload) VALUES ('" + long_b + "')")
                        .success());
    }

    CatalogManager::TableInfo table_info;
    std::vector<CatalogManager::ColumnInfo> columns;
    ASSERT_TRUE(lookupTable("stats_long_strings", table_info, columns));

    ErrorContext stats_ctx;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 1.0f, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    const ID payload_column = findColumnId(columns, "payload");
    ASSERT_NE(payload_column, ID{});

    ASSERT_TRUE(reopenDatabase());
    ASSERT_TRUE(lookupTable("stats_long_strings", table_info, columns));

    const ID reopened_payload_column = findColumnId(columns, "payload");
    ASSERT_NE(reopened_payload_column, ID{});

    ColumnStatistics payload_stats;
    ASSERT_EQ(db_->statistics_manager()->getColumnStatistics(table_info.table_id,
                                                             reopened_payload_column,
                                                             payload_stats,
                                                             &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    EXPECT_EQ(payload_stats.comparator_family, StatisticsComparatorFamily::STRING);
    EXPECT_EQ(payload_stats.value_encoding,
              StatisticsValueEncoding::PLAIN_LENGTH_PREFIXED);
    ASSERT_GE(payload_stats.mcv_list.size(), 2u);

    const auto encoded_a = encodeLengthPrefixedValue(long_a);
    const auto encoded_b = encodeLengthPrefixedValue(long_b);
    bool found_a = false;
    bool found_b = false;
    for (const auto &mcv : payload_stats.mcv_list)
    {
        if (mcv.value_data == encoded_a)
        {
            found_a = true;
            EXPECT_NEAR(mcv.frequency, 0.60, 0.01);
            EXPECT_GT(mcv.value_data.size(), 256u);
        }
        else if (mcv.value_data == encoded_b)
        {
            found_b = true;
            EXPECT_NEAR(mcv.frequency, 0.40, 0.01);
            EXPECT_GT(mcv.value_data.size(), 256u);
        }
    }
    EXPECT_TRUE(found_a);
    EXPECT_TRUE(found_b);

    SelectivityEstimator estimator(db_->statistics_manager(), db_.get());
    EXPECT_NEAR(estimator.estimateEquality(table_info.table_id,
                                           reopened_payload_column,
                                           encoded_a,
                                           &stats_ctx),
                0.60,
                0.01);
    EXPECT_NEAR(estimator.estimateEquality(table_info.table_id,
                                           reopened_payload_column,
                                           encoded_b,
                                           &stats_ctx),
                0.40,
                0.01);
}

TEST_F(StatisticsManagerRuntimeTest, TypedNumericHistogramRangeEstimationTracksActualDistribution)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE TABLE stats_numbers (value INTEGER)").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON stats_numbers TO PUBLIC").success());

    for (int i = 1; i <= 100; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO stats_numbers (value) VALUES (" + std::to_string(i) +
                               ")")
                        .success());
    }

    CatalogManager::TableInfo table_info;
    std::vector<CatalogManager::ColumnInfo> columns;
    ASSERT_TRUE(lookupTable("stats_numbers", table_info, columns));

    ErrorContext stats_ctx;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 1.0f, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    const ID value_column = findColumnId(columns, "value");
    ASSERT_NE(value_column, ID{});

    ColumnStatistics value_stats;
    ASSERT_EQ(db_->statistics_manager()->getColumnStatistics(table_info.table_id,
                                                             value_column,
                                                             value_stats,
                                                             &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    EXPECT_EQ(value_stats.comparator_family,
              StatisticsComparatorFamily::SIGNED_INTEGER);
    ASSERT_FALSE(value_stats.histogram_buckets.empty());

    SelectivityEstimator estimator(db_->statistics_manager(), db_.get());
    const double ge_fifty =
        estimator.estimateRange(table_info.table_id,
                                value_column,
                                ">=",
                                encodeScalarValue<int32_t>(50),
                                &stats_ctx);
    const double lt_twenty =
        estimator.estimateRange(table_info.table_id,
                                value_column,
                                "<",
                                encodeScalarValue<int32_t>(20),
                                &stats_ctx);

    EXPECT_NEAR(ge_fifty, 0.51, 0.08);
    EXPECT_NEAR(lt_twenty, 0.19, 0.08);
}

TEST_F(StatisticsManagerRuntimeTest, MultivariateStatisticsPersistMcvNDistinctAndDependencies)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE TABLE stats_pairs (country VARCHAR(16), state VARCHAR(16))").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON stats_pairs TO PUBLIC").success());

    for (int i = 0; i < 60; ++i)
    {
        ASSERT_TRUE(
            executeSQL("INSERT INTO stats_pairs (country, state) VALUES ('USA', 'CA')").success());
    }
    for (int i = 0; i < 40; ++i)
    {
        ASSERT_TRUE(
            executeSQL("INSERT INTO stats_pairs (country, state) VALUES ('USA', 'WA')").success());
    }
    for (int i = 0; i < 20; ++i)
    {
        ASSERT_TRUE(
            executeSQL("INSERT INTO stats_pairs (country, state) VALUES ('CAN', 'BC')").success());
    }

    CatalogManager::TableInfo table_info;
    std::vector<CatalogManager::ColumnInfo> columns;
    ASSERT_TRUE(lookupTable("stats_pairs", table_info, columns));

    ErrorContext stats_ctx;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 1.0f, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    const ID country_column = findColumnId(columns, "country");
    const ID state_column = findColumnId(columns, "state");
    ASSERT_NE(country_column, ID{});
    ASSERT_NE(state_column, ID{});

    MultivariateStatistics pair_stats;
    ASSERT_EQ(db_->statistics_manager()->getMultivariateStatistics(
                  table_info.table_id, {country_column, state_column}, pair_stats, &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    EXPECT_EQ(pair_stats.ndistinct.num_distinct, 3u);
    EXPECT_EQ(pair_stats.column_ids.size(), 2u);
    ASSERT_GE(pair_stats.mcv_list.size(), 3u);

    const auto encoded_usa = encodeLengthPrefixedValue("USA");
    const auto encoded_can = encodeLengthPrefixedValue("CAN");
    const auto encoded_ca = encodeLengthPrefixedValue("CA");
    const auto encoded_wa = encodeLengthPrefixedValue("WA");
    const auto encoded_bc = encodeLengthPrefixedValue("BC");

    bool found_usa_ca = false;
    bool found_usa_wa = false;
    bool found_can_bc = false;
    for (const auto &entry : pair_stats.mcv_list)
    {
        ASSERT_EQ(entry.values.size(), 2u);
        if (entry.values[0] == encoded_can && entry.values[1] == encoded_bc)
        {
            found_can_bc = true;
            EXPECT_NEAR(entry.frequency, 20.0 / 120.0, 0.02);
        }
        else if (entry.values[0] == encoded_usa && entry.values[1] == encoded_ca)
        {
            found_usa_ca = true;
            EXPECT_NEAR(entry.frequency, 0.50, 0.02);
        }
        else if (entry.values[0] == encoded_usa && entry.values[1] == encoded_wa)
        {
            found_usa_wa = true;
            EXPECT_NEAR(entry.frequency, 40.0 / 120.0, 0.02);
        }
    }
    EXPECT_TRUE(found_usa_ca);
    EXPECT_TRUE(found_usa_wa);
    EXPECT_TRUE(found_can_bc);

    ASSERT_EQ(pair_stats.dependencies.size(), 2u);
    double country_to_state = 0.0;
    double state_to_country = 0.0;
    for (const auto &dependency : pair_stats.dependencies)
    {
        ASSERT_EQ(dependency.determinant_column_ids.size(), 1u);
        ASSERT_EQ(dependency.dependent_column_ids.size(), 1u);
        if (dependency.determinant_column_ids[0] == country_column)
        {
            country_to_state = dependency.strength;
        }
        else if (dependency.determinant_column_ids[0] == state_column)
        {
            state_to_country = dependency.strength;
        }
    }
    EXPECT_NEAR(country_to_state, 80.0 / 120.0, 0.02);
    EXPECT_NEAR(state_to_country, 1.0, 0.001);

    ASSERT_TRUE(reopenDatabase());
    ASSERT_TRUE(lookupTable("stats_pairs", table_info, columns));

    const ID reopened_country = findColumnId(columns, "country");
    const ID reopened_state = findColumnId(columns, "state");
    ASSERT_NE(reopened_country, ID{});
    ASSERT_NE(reopened_state, ID{});

    MultivariateStatistics reopened_stats;
    ASSERT_EQ(db_->statistics_manager()->getMultivariateStatistics(
                  table_info.table_id, {reopened_country, reopened_state}, reopened_stats, &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    EXPECT_EQ(reopened_stats.ndistinct.num_distinct, 3u);
    ASSERT_EQ(reopened_stats.dependencies.size(), 2u);
}

TEST_F(StatisticsManagerRuntimeTest, MultivariateStatisticsCarryPairwiseCorrelationForNumericPairs)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE TABLE stats_numeric_pairs (id INTEGER, score INTEGER)").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON stats_numeric_pairs TO PUBLIC").success());

    for (int i = 1; i <= 200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO stats_numeric_pairs (id, score) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(i * 7) + ")")
                        .success());
    }

    CatalogManager::TableInfo table_info;
    std::vector<CatalogManager::ColumnInfo> columns;
    ASSERT_TRUE(lookupTable("stats_numeric_pairs", table_info, columns));

    ErrorContext stats_ctx;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 1.0f, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    const ID id_column = findColumnId(columns, "id");
    const ID score_column = findColumnId(columns, "score");
    ASSERT_NE(id_column, ID{});
    ASSERT_NE(score_column, ID{});

    MultivariateStatistics numeric_stats;
    ASSERT_EQ(db_->statistics_manager()->getMultivariateStatistics(
                  table_info.table_id, {id_column, score_column}, numeric_stats, &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    ASSERT_EQ(numeric_stats.pairwise_correlations.size(), 1u);
    EXPECT_GT(std::abs(numeric_stats.pairwise_correlations.front().coefficient), 0.99);
}

TEST_F(StatisticsManagerRuntimeTest, DependencyAwareConjunctiveEqualityUsesMultivariateStats)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE TABLE stats_pairs (country VARCHAR(16), state VARCHAR(16))").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON stats_pairs TO PUBLIC").success());

    for (int i = 0; i < 60; ++i)
    {
        ASSERT_TRUE(
            executeSQL("INSERT INTO stats_pairs (country, state) VALUES ('USA', 'CA')").success());
    }
    for (int i = 0; i < 40; ++i)
    {
        ASSERT_TRUE(
            executeSQL("INSERT INTO stats_pairs (country, state) VALUES ('USA', 'WA')").success());
    }
    for (int i = 0; i < 20; ++i)
    {
        ASSERT_TRUE(
            executeSQL("INSERT INTO stats_pairs (country, state) VALUES ('CAN', 'BC')").success());
    }

    CatalogManager::TableInfo table_info;
    std::vector<CatalogManager::ColumnInfo> columns;
    ASSERT_TRUE(lookupTable("stats_pairs", table_info, columns));

    ErrorContext stats_ctx;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 1.0f, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    SelectivityEstimator estimator(db_->statistics_manager(), db_.get());
    parser::v3::StringPool pool;
    auto country_col = makeColumnRefExpr(pool, "country");
    auto usa_literal = makeStringLiteralExpr(pool, "USA");
    auto country_eq =
        makeBinaryExpr(parser::v3::BinaryOp::EQ, country_col.get(), usa_literal.get());
    auto state_col = makeColumnRefExpr(pool, "state");
    auto ca_literal = makeStringLiteralExpr(pool, "CA");
    auto state_eq =
        makeBinaryExpr(parser::v3::BinaryOp::EQ, state_col.get(), ca_literal.get());
    auto and_expr =
        makeBinaryExpr(parser::v3::BinaryOp::AND, country_eq.get(), state_eq.get());

    const double selectivity =
        estimator.estimateWhereClause(and_expr.get(), table_info.table_id, &pool, &stats_ctx);
    EXPECT_NEAR(selectivity, 60.0 / 120.0, 0.02);
}

TEST_F(StatisticsManagerRuntimeTest, ParameterBoundConjunctiveEqualityUsesMultivariateStats)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE TABLE stats_pairs (country VARCHAR(16), state VARCHAR(16))").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON stats_pairs TO PUBLIC").success());

    for (int i = 0; i < 60; ++i)
    {
        ASSERT_TRUE(
            executeSQL("INSERT INTO stats_pairs (country, state) VALUES ('USA', 'CA')").success());
    }
    for (int i = 0; i < 40; ++i)
    {
        ASSERT_TRUE(
            executeSQL("INSERT INTO stats_pairs (country, state) VALUES ('USA', 'WA')").success());
    }
    for (int i = 0; i < 20; ++i)
    {
        ASSERT_TRUE(
            executeSQL("INSERT INTO stats_pairs (country, state) VALUES ('CAN', 'BC')").success());
    }

    CatalogManager::TableInfo table_info;
    std::vector<CatalogManager::ColumnInfo> columns;
    ASSERT_TRUE(lookupTable("stats_pairs", table_info, columns));

    ErrorContext stats_ctx;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 1.0f, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    SelectivityEstimator estimator(db_->statistics_manager(), db_.get());
    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "USA"});
    bindings.positional.push_back({false, "WA"});
    estimator.setParameterBindings(&bindings);

    parser::v3::StringPool pool;
    auto country_col = makeColumnRefExpr(pool, "country");
    auto country_param = makeParameterExpr(1);
    auto country_eq =
        makeBinaryExpr(parser::v3::BinaryOp::EQ, country_col.get(), country_param.get());
    auto state_col = makeColumnRefExpr(pool, "state");
    auto state_param = makeParameterExpr(2);
    auto state_eq =
        makeBinaryExpr(parser::v3::BinaryOp::EQ, state_col.get(), state_param.get());
    auto and_expr =
        makeBinaryExpr(parser::v3::BinaryOp::AND, country_eq.get(), state_eq.get());

    const double selectivity =
        estimator.estimateWhereClause(and_expr.get(), table_info.table_id, &pool, &stats_ctx);
    EXPECT_NEAR(selectivity, 40.0 / 120.0, 0.02);
}

TEST_F(StatisticsManagerRuntimeTest, EquiJoinSelectivityUsesMcvOverlapInsteadOfUniformFallback)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE TABLE join_left (value VARCHAR(16))").success());
    ASSERT_TRUE(executeSQL("CREATE TABLE join_right (value VARCHAR(16))").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON join_left TO PUBLIC").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON join_right TO PUBLIC").success());

    for (int i = 0; i < 80; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO join_left (value) VALUES ('A')").success());
    }
    for (int i = 0; i < 20; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO join_left (value) VALUES ('B')").success());
    }
    for (int i = 0; i < 50; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO join_right (value) VALUES ('A')").success());
    }
    for (int i = 0; i < 50; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO join_right (value) VALUES ('C')").success());
    }

    CatalogManager::TableInfo left_table;
    CatalogManager::TableInfo right_table;
    std::vector<CatalogManager::ColumnInfo> left_columns;
    std::vector<CatalogManager::ColumnInfo> right_columns;
    ASSERT_TRUE(lookupTable("join_left", left_table, left_columns));
    ASSERT_TRUE(lookupTable("join_right", right_table, right_columns));

    ErrorContext stats_ctx;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(left_table.table_id, 1.0f, &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(right_table.table_id, 1.0f, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    const ID left_value_column = findColumnId(left_columns, "value");
    const ID right_value_column = findColumnId(right_columns, "value");
    ASSERT_NE(left_value_column, ID{});
    ASSERT_NE(right_value_column, ID{});

    SelectivityEstimator estimator(db_->statistics_manager(), db_.get());
    const double selectivity =
        estimator.estimateEquiJoinSelectivity(left_value_column,
                                              right_value_column,
                                              left_table.table_id,
                                              right_table.table_id,
                                              &stats_ctx);
    EXPECT_NEAR(selectivity, 0.40, 0.05);
}

TEST_F(StatisticsManagerRuntimeTest, MultiColumnJoinSelectivityUsesMultivariateOverlap)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE TABLE join_pairs_left (country VARCHAR(16), state VARCHAR(16))")
            .success());
    ASSERT_TRUE(
        executeSQL("CREATE TABLE join_pairs_right (country VARCHAR(16), state VARCHAR(16))")
            .success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON join_pairs_left TO PUBLIC").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON join_pairs_right TO PUBLIC").success());

    for (int i = 0; i < 60; ++i)
    {
        ASSERT_TRUE(executeSQL(
                        "INSERT INTO join_pairs_left (country, state) VALUES ('USA', 'CA')")
                        .success());
        ASSERT_TRUE(executeSQL(
                        "INSERT INTO join_pairs_right (country, state) VALUES ('USA', 'CA')")
                        .success());
    }
    for (int i = 0; i < 40; ++i)
    {
        ASSERT_TRUE(executeSQL(
                        "INSERT INTO join_pairs_left (country, state) VALUES ('USA', 'WA')")
                        .success());
        ASSERT_TRUE(executeSQL(
                        "INSERT INTO join_pairs_right (country, state) VALUES ('USA', 'WA')")
                        .success());
    }
    for (int i = 0; i < 20; ++i)
    {
        ASSERT_TRUE(executeSQL(
                        "INSERT INTO join_pairs_left (country, state) VALUES ('CAN', 'BC')")
                        .success());
        ASSERT_TRUE(executeSQL(
                        "INSERT INTO join_pairs_right (country, state) VALUES ('CAN', 'BC')")
                        .success());
    }

    CatalogManager::TableInfo left_table;
    CatalogManager::TableInfo right_table;
    std::vector<CatalogManager::ColumnInfo> left_columns;
    std::vector<CatalogManager::ColumnInfo> right_columns;
    ASSERT_TRUE(lookupTable("join_pairs_left", left_table, left_columns));
    ASSERT_TRUE(lookupTable("join_pairs_right", right_table, right_columns));

    ErrorContext stats_ctx;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(left_table.table_id, 1.0f, &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(right_table.table_id, 1.0f, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    SelectivityEstimator estimator(db_->statistics_manager(), db_.get());
    parser::v3::StringPool pool;
    auto left_country = makeColumnRefExpr(pool, "country");
    auto right_country = makeColumnRefExpr(pool, "country");
    auto country_eq =
        makeBinaryExpr(parser::v3::BinaryOp::EQ, left_country.get(), right_country.get());
    auto left_state = makeColumnRefExpr(pool, "state");
    auto right_state = makeColumnRefExpr(pool, "state");
    auto state_eq =
        makeBinaryExpr(parser::v3::BinaryOp::EQ, left_state.get(), right_state.get());
    auto and_expr =
        makeBinaryExpr(parser::v3::BinaryOp::AND, country_eq.get(), state_eq.get());

    const double selectivity =
        estimator.estimateJoinSelectivity(and_expr.get(),
                                          left_table.table_id,
                                          right_table.table_id,
                                          &pool,
                                          &stats_ctx);
    EXPECT_NEAR(selectivity, (0.50 * 0.50) + ((40.0 / 120.0) * (40.0 / 120.0)) +
                                 ((20.0 / 120.0) * (20.0 / 120.0)),
                0.03);
}

TEST_F(StatisticsManagerRuntimeTest, LowConfidenceStatisticsTriggerSampledRefreshDuringEstimation)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE TABLE stats_sampling (category VARCHAR(16))").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON stats_sampling TO PUBLIC").success());

    for (int i = 0; i < 900; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO stats_sampling (category) VALUES ('hot')").success());
    }
    for (int i = 0; i < 100; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO stats_sampling (category) VALUES ('cold')").success());
    }

    CatalogManager::TableInfo table_info;
    std::vector<CatalogManager::ColumnInfo> columns;
    ASSERT_TRUE(lookupTable("stats_sampling", table_info, columns));

    ErrorContext stats_ctx;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 0.01f, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    const ID category_column = findColumnId(columns, "category");
    ASSERT_NE(category_column, ID{});

    ColumnStatistics before_stats;
    ASSERT_EQ(db_->statistics_manager()->getColumnStatistics(table_info.table_id,
                                                             category_column,
                                                             before_stats,
                                                             &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    EXPECT_EQ(before_stats.confidence_class, StatisticsConfidenceClass::LOW);

    SelectivityEstimator estimator(db_->statistics_manager(), db_.get());
    const double selectivity =
        estimator.estimateEquality(table_info.table_id,
                                   category_column,
                                   encodeLengthPrefixedValue("hot"),
                                   &stats_ctx);

    ColumnStatistics after_stats;
    ASSERT_EQ(db_->statistics_manager()->getColumnStatistics(table_info.table_id,
                                                             category_column,
                                                             after_stats,
                                                             &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    EXPECT_GT(after_stats.sample_size, before_stats.sample_size);
    EXPECT_NE(after_stats.confidence_class, StatisticsConfidenceClass::LOW);
    EXPECT_GT(selectivity, 0.70);
}
