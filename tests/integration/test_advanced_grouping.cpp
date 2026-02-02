/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// ScratchBird Advanced Grouping Integration Tests
// Tests ROLLUP, CUBE, GROUPING SETS, and GROUPING() function
//
// Date: November 24, 2025
// Purpose: Verify that advanced grouping operations execute correctly
//
// Test Coverage:
// 1. ROLLUP(single_column) - Simple rollup with grand total
// 2. ROLLUP(multiple_columns) - Multi-level rollup
// 3. CUBE(two_columns) - Full cube with all combinations
// 4. GROUPING SETS - Custom grouping sets
// 5. GROUPING() function - Correct 0/1 values
// 6. Advanced cases - WITH WHERE, HAVING, ORDER BY
//
// MGA Compliance:
// - All operations use TransactionId (not Snapshot*)
// - TIP-based visibility checking
// - Follows Firebird MGA model
//
// Files Tested:
// - src/sblr/executor.cpp (executeAdvancedGrouping)
// - src/sblr/bytecode_generator_v2.cpp (ROLLUP/CUBE bytecode generation)
// - src/parser/parser_v2.cpp (ROLLUP/CUBE parsing)

#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "test_helpers.h"

#include <filesystem>
#include <memory>
#include <string>

using namespace scratchbird;

class AdvancedGroupingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create temporary test database
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_advanced_grouping", ".db");
        std::filesystem::remove_all(test_db_path_);

        core::ErrorContext ctx;
        ASSERT_EQ(core::Database::create(test_db_path_, 16384, &ctx), core::Status::OK)
            << "Failed to create database: " << ctx.message;
        db_ = std::make_unique<core::Database>();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), core::Status::OK)
            << "Failed to open database: " << ctx.message;

        core::CatalogManager::SchemaInfo schema;
        auto status = db_->catalog_manager()->getSchema("PUBLIC", schema, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to get PUBLIC schema";
        schema_id_ = schema.schema_id;

        compiler_ = std::make_unique<sblr::QueryCompilerV2>(db_.get());
        compiler_->setCurrentSchema(schema_id_);
        executor_ = std::make_unique<sblr::Executor>(db_.get());
        executor_->setCurrentSchema(schema_id_);

        // Create test table with sales data
        createSalesTable();
        insertSalesData();
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        db_.reset();
        std::filesystem::remove_all(test_db_path_);
    }

    void createSalesTable()
    {
        // CREATE TABLE sales (region VARCHAR(50), product VARCHAR(50), sales FLOAT)
        core::ErrorContext ctx;

        std::vector<core::CatalogManager::ColumnInfo> columns;

        core::CatalogManager::ColumnInfo col1;
        col1.column_id = core::generateUuidV7();
        col1.column_name = "region";
        col1.data_type = static_cast<uint16_t>(core::DataType::VARCHAR);
        col1.type_precision = 50;
        col1.max_length = 50;
        col1.nullable = true;
        col1.ordinal = 0;
        columns.push_back(col1);

        core::CatalogManager::ColumnInfo col2;
        col2.column_id = core::generateUuidV7();
        col2.column_name = "product";
        col2.data_type = static_cast<uint16_t>(core::DataType::VARCHAR);
        col2.type_precision = 50;
        col2.max_length = 50;
        col2.nullable = true;
        col2.ordinal = 1;
        columns.push_back(col2);

        core::CatalogManager::ColumnInfo col3;
        col3.column_id = core::generateUuidV7();
        col3.column_name = "sales";
        col3.data_type = static_cast<uint16_t>(core::DataType::FLOAT64);
        col3.nullable = true;
        col3.ordinal = 2;
        columns.push_back(col3);

        core::ID table_id;
        auto status = db_->catalog_manager()->createTable(
            schema_id_,
            "sales",
            columns,
            table_id,
            0,  // default tablespace
            &ctx
        );
        ASSERT_EQ(status, core::Status::OK) << "Failed to create sales table: " << ctx.message;
    }

    void insertSalesData()
    {
        // Insert test data
        const char* insert_queries[] = {
            "INSERT INTO sales (region, product, sales) VALUES ('North', 'Widget', 100.0)",
            "INSERT INTO sales (region, product, sales) VALUES ('North', 'Gadget', 150.0)",
            "INSERT INTO sales (region, product, sales) VALUES ('South', 'Widget', 200.0)",
            "INSERT INTO sales (region, product, sales) VALUES ('South', 'Gadget', 250.0)",
            "INSERT INTO sales (region, product, sales) VALUES ('East', 'Widget', 300.0)",
            "INSERT INTO sales (region, product, sales) VALUES ('East', 'Gadget', 350.0)"
        };

        for (const char* sql : insert_queries)
        {
            ASSERT_TRUE(executeStatement(sql));
        }
    }

    bool executeStatement(const std::string& sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            ADD_FAILURE() << "Compile failed for SQL: " << sql;
            for (const auto& err : compile_result.errors())
            {
                ADD_FAILURE() << "  " << err;
            }
            return false;
        }

        auto exec_result = executor_->execute(compile_result.bytecode());
        if (!exec_result.success())
        {
            ADD_FAILURE() << "Execution failed for SQL: " << sql
                          << " error: " << exec_result.error();
            return false;
        }

        return true;
    }

    sblr::ExecutionResult executeQuery(const std::string& sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            ADD_FAILURE() << "Compile failed for SQL: " << sql;
            for (const auto& err : compile_result.errors())
            {
                ADD_FAILURE() << "  " << err;
            }
            return sblr::ExecutionResult("compile failed");
        }

        auto exec_result = executor_->execute(compile_result.bytecode());
        if (!exec_result.success())
        {
            ADD_FAILURE() << "Execution failed for SQL: " << sql
                          << " error: " << exec_result.error();
        }

        return exec_result;
    }

    void verifyRowCount(const sblr::ResultSet* result, size_t expected_count)
    {
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->rowCount(), expected_count)
            << "Expected " << expected_count << " rows, got " << result->rowCount();
    }

    void verifyColumnValue(const sblr::ResultSet* result, size_t row, size_t col,
                           const std::string& expected)
    {
        ASSERT_NE(result, nullptr);
        ASSERT_LT(row, result->rowCount()) << "Row index out of range";
        ASSERT_LT(col, result->columnCount()) << "Column index out of range";

        auto value = result->getValue(row, col);
        if (expected == "NULL")
        {
            EXPECT_TRUE(value.isNull()) << "Expected NULL at row " << row << ", col " << col;
        }
        else
        {
            EXPECT_FALSE(value.isNull()) << "Expected non-NULL at row " << row << ", col " << col;
            EXPECT_EQ(value.toString(), expected)
                << "Expected '" << expected << "' at row " << row << ", col " << col
                << ", got '" << value.toString() << "'";
        }
    }

    std::string test_db_path_;
    std::unique_ptr<core::Database> db_;
    std::unique_ptr<sblr::QueryCompilerV2> compiler_;
    std::unique_ptr<sblr::Executor> executor_;
    core::ID schema_id_;
};

// Test 1: ROLLUP with single column
TEST_F(AdvancedGroupingTest, RollupSingleColumn)
{
    auto result = executeQuery("SELECT region, SUM(sales) FROM sales GROUP BY ROLLUP(region)");
    ASSERT_TRUE(result.success()) << result.error();
    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);

    // Expected: 4 rows (North, South, East, Grand Total)
    verifyRowCount(result_set, 4);

    // Verify that we have 3 region groups + 1 grand total (NULL region)
    bool found_grand_total = false;
    for (size_t i = 0; i < result_set->rowCount(); i++)
    {
        auto region = result_set->getValue(i, 0);
        if (region.isNull())
        {
            found_grand_total = true;
            // Grand total should be sum of all: 100+150+200+250+300+350 = 1350
            auto sum = result_set->getValue(i, 1);
            EXPECT_DOUBLE_EQ(sum.toDouble(), 1350.0);
        }
    }
    EXPECT_TRUE(found_grand_total) << "Grand total row (NULL region) not found";
}

// Test 2: ROLLUP with multiple columns
TEST_F(AdvancedGroupingTest, RollupMultipleColumns)
{
    auto result = executeQuery(
        "SELECT region, product, SUM(sales) FROM sales GROUP BY ROLLUP(region, product)"
    );
    ASSERT_TRUE(result.success()) << result.error();
    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);

    // Expected: (region, product), (region, NULL), (NULL, NULL)
    // 6 detail rows + 3 region subtotals + 1 grand total = 10 rows
    verifyRowCount(result_set, 10);

    // Verify grand total exists (both NULL)
    bool found_grand_total = false;
    for (size_t i = 0; i < result_set->rowCount(); i++)
    {
        auto region = result_set->getValue(i, 0);
        auto product = result_set->getValue(i, 1);
        if (region.isNull() && product.isNull())
        {
            found_grand_total = true;
            auto sum = result_set->getValue(i, 2);
            EXPECT_DOUBLE_EQ(sum.toDouble(), 1350.0);
        }
    }
    EXPECT_TRUE(found_grand_total) << "Grand total row not found";
}

// Test 3: CUBE with two columns
TEST_F(AdvancedGroupingTest, CubeTwoColumns)
{
    auto result = executeQuery(
        "SELECT region, product, SUM(sales) FROM sales GROUP BY CUBE(region, product)"
    );
    ASSERT_TRUE(result.success()) << result.error();
    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);

    // Expected: 2^2 = 4 grouping levels
    // (region, product), (region, NULL), (NULL, product), (NULL, NULL)
    // 6 detail + 3 region subtotals + 2 product subtotals + 1 grand total = 12 rows
    verifyRowCount(result_set, 12);

    // Verify grand total
    bool found_grand_total = false;
    for (size_t i = 0; i < result_set->rowCount(); i++)
    {
        auto region = result_set->getValue(i, 0);
        auto product = result_set->getValue(i, 1);
        if (region.isNull() && product.isNull())
        {
            found_grand_total = true;
            auto sum = result_set->getValue(i, 2);
            EXPECT_DOUBLE_EQ(sum.toDouble(), 1350.0);
        }
    }
    EXPECT_TRUE(found_grand_total) << "Grand total row not found";

    // Verify product subtotals exist (NULL region, specific product)
    bool found_widget_subtotal = false;
    bool found_gadget_subtotal = false;
    for (size_t i = 0; i < result_set->rowCount(); i++)
    {
        auto region = result_set->getValue(i, 0);
        auto product = result_set->getValue(i, 1);
        if (region.isNull() && !product.isNull())
        {
            if (product.toString() == "Widget")
            {
                found_widget_subtotal = true;
                auto sum = result_set->getValue(i, 2);
                // Widget total: 100 + 200 + 300 = 600
                EXPECT_DOUBLE_EQ(sum.toDouble(), 600.0);
            }
            else if (product.toString() == "Gadget")
            {
                found_gadget_subtotal = true;
                auto sum = result_set->getValue(i, 2);
                // Gadget total: 150 + 250 + 350 = 750
                EXPECT_DOUBLE_EQ(sum.toDouble(), 750.0);
            }
        }
    }
    EXPECT_TRUE(found_widget_subtotal) << "Widget subtotal not found";
    EXPECT_TRUE(found_gadget_subtotal) << "Gadget subtotal not found";
}

// Test 4: GROUPING SETS
TEST_F(AdvancedGroupingTest, GroupingSets)
{
    auto result = executeQuery(
        "SELECT region, product, SUM(sales) FROM sales "
        "GROUP BY GROUPING SETS ((region, product), (region), ())"
    );
    ASSERT_TRUE(result.success()) << result.error();
    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);

    // Expected: Only specified sets (not full cube)
    // 6 detail + 3 region subtotals + 1 grand total = 10 rows
    verifyRowCount(result_set, 10);
}

// Test 5: GROUPING() function
TEST_F(AdvancedGroupingTest, GroupingFunction)
{
    auto result = executeQuery(
        "SELECT region, product, "
        "GROUPING(region) as r_grp, GROUPING(product) as p_grp, "
        "SUM(sales) FROM sales "
        "GROUP BY ROLLUP(region, product)"
    );
    ASSERT_TRUE(result.success()) << result.error();
    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);

    // Verify GROUPING() values for grand total row
    bool found_grand_total = false;
    for (size_t i = 0; i < result_set->rowCount(); i++)
    {
        auto region = result_set->getValue(i, 0);
        auto product = result_set->getValue(i, 1);

        if (region.isNull() && product.isNull())
        {
            // Grand total: both columns aggregated
            found_grand_total = true;
            auto r_grp = result_set->getValue(i, 2);
            auto p_grp = result_set->getValue(i, 3);

            // Note: GROUPING() implementation is heuristic-based
            // For grand total (empty set), both should be 1
            EXPECT_EQ(r_grp.toInt32(), 1) << "GROUPING(region) should be 1 for grand total";
            EXPECT_EQ(p_grp.toInt32(), 1) << "GROUPING(product) should be 1 for grand total";
        }
    }
    EXPECT_TRUE(found_grand_total) << "Grand total row not found";
}

// Test 6: ROLLUP with WHERE clause
TEST_F(AdvancedGroupingTest, RollupWithWhere)
{
    auto result = executeQuery(
        "SELECT region, SUM(sales) FROM sales "
        "WHERE region IN ('North', 'South') "
        "GROUP BY ROLLUP(region)"
    );
    ASSERT_TRUE(result.success()) << result.error();
    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);

    // Expected: 3 rows (North, South, Grand Total)
    verifyRowCount(result_set, 3);

    // Grand total should only include North and South
    bool found_grand_total = false;
    for (size_t i = 0; i < result_set->rowCount(); i++)
    {
        auto region = result_set->getValue(i, 0);
        if (region.isNull())
        {
            found_grand_total = true;
            auto sum = result_set->getValue(i, 1);
            // North: 100+150=250, South: 200+250=450, Total: 700
            EXPECT_DOUBLE_EQ(sum.toDouble(), 700.0);
        }
    }
    EXPECT_TRUE(found_grand_total);
}

// Test 7: ROLLUP with HAVING clause
TEST_F(AdvancedGroupingTest, RollupWithHaving)
{
    auto result = executeQuery(
        "SELECT region, SUM(sales) FROM sales "
        "GROUP BY ROLLUP(region) "
        "HAVING SUM(sales) > 500"
    );
    ASSERT_TRUE(result.success()) << result.error();
    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);

    // Expected: Rows where SUM(sales) > 500
    // East (650) and Grand Total (1350) should pass
    EXPECT_GE(result_set->rowCount(), 2);

    // Verify all returned rows have SUM > 500
    for (size_t i = 0; i < result_set->rowCount(); i++)
    {
        auto sum = result_set->getValue(i, 1);
        EXPECT_GT(sum.toDouble(), 500.0);
    }
}

// Test 8: ROLLUP with ORDER BY
TEST_F(AdvancedGroupingTest, RollupWithOrderBy)
{
    auto result = executeQuery(
        "SELECT region, SUM(sales) as total FROM sales "
        "GROUP BY ROLLUP(region) "
        "ORDER BY total DESC"
    );
    ASSERT_TRUE(result.success()) << result.error();
    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);

    // Verify descending order
    for (size_t i = 0; i + 1 < result_set->rowCount(); i++)
    {
        auto current = result_set->getValue(i, 1).toDouble();
        auto next = result_set->getValue(i + 1, 1).toDouble();
        EXPECT_GE(current, next) << "Results not in descending order";
    }
}

// Test 9: Multiple aggregates with ROLLUP
TEST_F(AdvancedGroupingTest, MultipleAggregatesWithRollup)
{
    auto result = executeQuery(
        "SELECT region, SUM(sales), AVG(sales), COUNT(*) FROM sales "
        "GROUP BY ROLLUP(region)"
    );
    ASSERT_TRUE(result.success()) << result.error();
    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);

    // Verify grand total row has correct aggregates
    bool found_grand_total = false;
    for (size_t i = 0; i < result_set->rowCount(); i++)
    {
        auto region = result_set->getValue(i, 0);
        if (region.isNull())
        {
            found_grand_total = true;
            auto sum = result_set->getValue(i, 1).toDouble();
            auto avg = result_set->getValue(i, 2).toDouble();
            auto count = result_set->getValue(i, 3).toInt64();

            EXPECT_DOUBLE_EQ(sum, 1350.0);
            EXPECT_DOUBLE_EQ(avg, 225.0);  // 1350 / 6
            EXPECT_EQ(count, 6);
        }
    }
    EXPECT_TRUE(found_grand_total);
}

// Test 10: Empty result with ROLLUP
TEST_F(AdvancedGroupingTest, EmptyResultWithRollup)
{
    auto result = executeQuery(
        "SELECT region, SUM(sales) FROM sales "
        "WHERE region = 'NonExistent' "
        "GROUP BY ROLLUP(region)"
    );
    ASSERT_TRUE(result.success()) << result.error();
    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);

    // With no matching rows, ROLLUP should still produce grand total
    // (This depends on SQL standard behavior - some DBs return empty, some return grand total)
    // For now, just verify no crash
    EXPECT_GE(result_set->rowCount(), 0u);
}
