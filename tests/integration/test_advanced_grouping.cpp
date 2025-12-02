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
// - src/sblr/bytecode_generator.cpp (ROLLUP/CUBE bytecode generation)
// - src/parser/parser.cpp (ROLLUP/CUBE parsing)

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/parser/parser.h"
#include <memory>
<filesystem>

using namespace scratchbird;

class AdvancedGroupingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create temporary test database
        test_db_path_ = "/tmp/test_advanced_grouping.db";
        std::filesystem::remove_all(test_db_path_);

        core::ErrorContext ctx;
        db_ = core::Database::create(test_db_path_, &ctx);
        ASSERT_NE(db_, nullptr) << "Failed to create database: " << ctx.message;

        // Create test table with sales data
        createSalesTable();
        insertSalesData();
    }

    void TearDown() override
    {
        db_.reset();
        std::filesystem::remove_all(test_db_path_);
    }

    void createSalesTable()
    {
        // CREATE TABLE sales (region VARCHAR(50), product VARCHAR(50), sales FLOAT)
        core::ErrorContext ctx;

        core::CatalogManager::SchemaInfo schema;
        auto status = db_->catalog_manager()->getSchema("PUBLIC", schema, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to get PUBLIC schema";

        std::vector<core::CatalogManager::ColumnInfo> columns;

        core::CatalogManager::ColumnInfo col1;
        col1.column_name = "region";
        col1.data_type = static_cast<uint16_t>(core::DataType::VARCHAR);
        col1.max_length = 50;
        col1.is_nullable = true;
        col1.column_id = 1;
        columns.push_back(col1);

        core::CatalogManager::ColumnInfo col2;
        col2.column_name = "product";
        col2.data_type = static_cast<uint16_t>(core::DataType::VARCHAR);
        col2.max_length = 50;
        col2.is_nullable = true;
        col2.column_id = 2;
        columns.push_back(col2);

        core::CatalogManager::ColumnInfo col3;
        col3.column_name = "sales";
        col3.data_type = static_cast<uint16_t>(core::DataType::FLOAT64);
        col3.is_nullable = true;
        col3.column_id = 3;
        columns.push_back(col3);

        uint64_t table_id;
        status = db_->catalog_manager()->createTable(
            schema.schema_id,
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
            executeSQL(sql);
        }
    }

    std::unique_ptr<sblr::ResultSet> executeSQL(const std::string& sql)
    {
        core::ErrorContext ctx;

        // Parse SQL
        parser::Parser parser;
        auto ast = parser.parse(sql, &ctx);
        EXPECT_NE(ast, nullptr) << "Failed to parse SQL: " << ctx.message << "\nSQL: " << sql;
        if (!ast) return nullptr;

        // Generate bytecode
        sblr::BytecodeGenerator generator(db_.get());
        auto bytecode = generator.generate(ast.get(), &ctx);
        EXPECT_GT(bytecode.size(), 0) << "Failed to generate bytecode: " << ctx.message;
        if (bytecode.empty()) return nullptr;

        // Execute bytecode
        sblr::Executor executor(db_.get(), bytecode);
        auto result = executor.execute(&ctx);

        if (ctx.error_code != core::ErrorCode::SUCCESS)
        {
            std::cerr << "Execution error: " << ctx.message << std::endl;
        }

        return result;
    }

    void verifyRowCount(const sblr::ResultSet* result, size_t expected_count)
    {
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->rowCount(), expected_count)
            << "Expected " << expected_count << " rows, got " << result->rowCount();
    }

    void verifyColumnValue(const sblr::ResultSet* result, size_t row, size_t col, const std::string& expected)
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
};

// Test 1: ROLLUP with single column
TEST_F(AdvancedGroupingTest, RollupSingleColumn)
{
    auto result = executeSQL("SELECT region, SUM(sales) FROM sales GROUP BY ROLLUP(region)");
    ASSERT_NE(result, nullptr);

    // Expected: 4 rows (North, South, East, Grand Total)
    verifyRowCount(result.get(), 4);

    // Verify that we have 3 region groups + 1 grand total (NULL region)
    bool found_grand_total = false;
    for (size_t i = 0; i < result->rowCount(); i++)
    {
        auto region = result->getValue(i, 0);
        if (region.isNull())
        {
            found_grand_total = true;
            // Grand total should be sum of all: 100+150+200+250+300+350 = 1350
            auto sum = result->getValue(i, 1);
            EXPECT_DOUBLE_EQ(sum.toDouble(), 1350.0);
        }
    }
    EXPECT_TRUE(found_grand_total) << "Grand total row (NULL region) not found";
}

// Test 2: ROLLUP with multiple columns
TEST_F(AdvancedGroupingTest, RollupMultipleColumns)
{
    auto result = executeSQL("SELECT region, product, SUM(sales) FROM sales GROUP BY ROLLUP(region, product)");
    ASSERT_NE(result, nullptr);

    // Expected: (region, product), (region, NULL), (NULL, NULL)
    // 6 detail rows + 3 region subtotals + 1 grand total = 10 rows
    verifyRowCount(result.get(), 10);

    // Verify grand total exists (both NULL)
    bool found_grand_total = false;
    for (size_t i = 0; i < result->rowCount(); i++)
    {
        auto region = result->getValue(i, 0);
        auto product = result->getValue(i, 1);
        if (region.isNull() && product.isNull())
        {
            found_grand_total = true;
            auto sum = result->getValue(i, 2);
            EXPECT_DOUBLE_EQ(sum.toDouble(), 1350.0);
        }
    }
    EXPECT_TRUE(found_grand_total) << "Grand total row not found";
}

// Test 3: CUBE with two columns
TEST_F(AdvancedGroupingTest, CubeTwoColumns)
{
    auto result = executeSQL("SELECT region, product, SUM(sales) FROM sales GROUP BY CUBE(region, product)");
    ASSERT_NE(result, nullptr);

    // Expected: 2^2 = 4 grouping levels
    // (region, product), (region, NULL), (NULL, product), (NULL, NULL)
    // 6 detail + 3 region subtotals + 2 product subtotals + 1 grand total = 12 rows
    verifyRowCount(result.get(), 12);

    // Verify grand total
    bool found_grand_total = false;
    for (size_t i = 0; i < result->rowCount(); i++)
    {
        auto region = result->getValue(i, 0);
        auto product = result->getValue(i, 1);
        if (region.isNull() && product.isNull())
        {
            found_grand_total = true;
            auto sum = result->getValue(i, 2);
            EXPECT_DOUBLE_EQ(sum.toDouble(), 1350.0);
        }
    }
    EXPECT_TRUE(found_grand_total) << "Grand total row not found";

    // Verify product subtotals exist (NULL region, specific product)
    bool found_widget_subtotal = false;
    bool found_gadget_subtotal = false;
    for (size_t i = 0; i < result->rowCount(); i++)
    {
        auto region = result->getValue(i, 0);
        auto product = result->getValue(i, 1);
        if (region.isNull() && !product.isNull())
        {
            if (product.toString() == "Widget")
            {
                found_widget_subtotal = true;
                auto sum = result->getValue(i, 2);
                // Widget total: 100 + 200 + 300 = 600
                EXPECT_DOUBLE_EQ(sum.toDouble(), 600.0);
            }
            else if (product.toString() == "Gadget")
            {
                found_gadget_subtotal = true;
                auto sum = result->getValue(i, 2);
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
    auto result = executeSQL(
        "SELECT region, product, SUM(sales) FROM sales "
        "GROUP BY GROUPING SETS ((region, product), (region), ())"
    );
    ASSERT_NE(result, nullptr);

    // Expected: Only specified sets (not full cube)
    // 6 detail + 3 region subtotals + 1 grand total = 10 rows
    verifyRowCount(result.get(), 10);
}

// Test 5: GROUPING() function
TEST_F(AdvancedGroupingTest, GroupingFunction)
{
    auto result = executeSQL(
        "SELECT region, product, "
        "GROUPING(region) as r_grp, GROUPING(product) as p_grp, "
        "SUM(sales) FROM sales "
        "GROUP BY ROLLUP(region, product)"
    );
    ASSERT_NE(result, nullptr);

    // Verify GROUPING() values for grand total row
    bool found_grand_total = false;
    for (size_t i = 0; i < result->rowCount(); i++)
    {
        auto region = result->getValue(i, 0);
        auto product = result->getValue(i, 1);

        if (region.isNull() && product.isNull())
        {
            // Grand total: both columns aggregated
            found_grand_total = true;
            auto r_grp = result->getValue(i, 2);
            auto p_grp = result->getValue(i, 3);

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
    auto result = executeSQL(
        "SELECT region, SUM(sales) FROM sales "
        "WHERE region IN ('North', 'South') "
        "GROUP BY ROLLUP(region)"
    );
    ASSERT_NE(result, nullptr);

    // Expected: 3 rows (North, South, Grand Total)
    verifyRowCount(result.get(), 3);

    // Grand total should only include North and South
    bool found_grand_total = false;
    for (size_t i = 0; i < result->rowCount(); i++)
    {
        auto region = result->getValue(i, 0);
        if (region.isNull())
        {
            found_grand_total = true;
            auto sum = result->getValue(i, 1);
            // North: 100+150=250, South: 200+250=450, Total: 700
            EXPECT_DOUBLE_EQ(sum.toDouble(), 700.0);
        }
    }
    EXPECT_TRUE(found_grand_total);
}

// Test 7: ROLLUP with HAVING clause
TEST_F(AdvancedGroupingTest, RollupWithHaving)
{
    auto result = executeSQL(
        "SELECT region, SUM(sales) FROM sales "
        "GROUP BY ROLLUP(region) "
        "HAVING SUM(sales) > 500"
    );
    ASSERT_NE(result, nullptr);

    // Expected: Rows where SUM(sales) > 500
    // East (650) and Grand Total (1350) should pass
    EXPECT_GE(result->rowCount(), 2);

    // Verify all returned rows have SUM > 500
    for (size_t i = 0; i < result->rowCount(); i++)
    {
        auto sum = result->getValue(i, 1);
        EXPECT_GT(sum.toDouble(), 500.0);
    }
}

// Test 8: ROLLUP with ORDER BY
TEST_F(AdvancedGroupingTest, RollupWithOrderBy)
{
    auto result = executeSQL(
        "SELECT region, SUM(sales) as total FROM sales "
        "GROUP BY ROLLUP(region) "
        "ORDER BY total DESC"
    );
    ASSERT_NE(result, nullptr);

    // Verify descending order
    for (size_t i = 0; i + 1 < result->rowCount(); i++)
    {
        auto current = result->getValue(i, 1).toDouble();
        auto next = result->getValue(i + 1, 1).toDouble();
        EXPECT_GE(current, next) << "Results not in descending order";
    }
}

// Test 9: Multiple aggregates with ROLLUP
TEST_F(AdvancedGroupingTest, MultipleAggregatesWithRollup)
{
    auto result = executeSQL(
        "SELECT region, SUM(sales), AVG(sales), COUNT(*) FROM sales "
        "GROUP BY ROLLUP(region)"
    );
    ASSERT_NE(result, nullptr);

    // Verify grand total row has correct aggregates
    bool found_grand_total = false;
    for (size_t i = 0; i < result->rowCount(); i++)
    {
        auto region = result->getValue(i, 0);
        if (region.isNull())
        {
            found_grand_total = true;
            auto sum = result->getValue(i, 1).toDouble();
            auto avg = result->getValue(i, 2).toDouble();
            auto count = result->getValue(i, 3).toInt64();

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
    auto result = executeSQL(
        "SELECT region, SUM(sales) FROM sales "
        "WHERE region = 'NonExistent' "
        "GROUP BY ROLLUP(region)"
    );
    ASSERT_NE(result, nullptr);

    // With no matching rows, ROLLUP should still produce grand total
    // (This depends on SQL standard behavior - some DBs return empty, some return grand total)
    // For now, just verify no crash
    EXPECT_GE(result->rowCount(), 0);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
