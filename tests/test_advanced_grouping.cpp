/**
 * Advanced Grouping Test Suite
 * Tests for ROLLUP, CUBE, GROUPING SETS, and GROUPING() function
 *
 * This test suite validates the complete implementation of SQL advanced
 * grouping features following PostgreSQL/SQL standard semantics.
 *
 * Features Tested:
 * - ROLLUP: Hierarchical grouping with subtotals and grand totals
 * - CUBE: All-combinations grouping
 * - GROUPING SETS: Explicit grouping set specification
 * - GROUPING(): Function to identify aggregated columns
 * - Integration with HAVING, ORDER BY, and multiple aggregates
 *
 * Created: November 24, 2025
 * Status: Ready for execution when test infrastructure is fixed
 */

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/parser/lexer.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/sblr/executor.h"
#include <filesystem>
#include <memory>

using namespace scratchbird;

class AdvancedGroupingTest : public ::testing::Test {
protected:
    std::unique_ptr<core::Database> db_;
    const std::string db_path_ = "/tmp/test_advanced_grouping_db";

    void SetUp() override {
        // Clean up any existing database
        std::filesystem::remove_all(db_path_);

        // Create database
        core::ErrorContext ctx;
        auto status = core::Database::create(db_path_, 16384, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to create database: " << ctx.message;

        // Open database
        db_ = std::make_unique<core::Database>();
        status = db_->open(db_path_, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to open database: " << ctx.message;

        // Create test table and insert sample data
        createTestData();
    }

    void TearDown() override {
        db_.reset();
        std::filesystem::remove_all(db_path_);
    }

    void createTestData() {
        // Create sales table: region, product, sales
        executeSQL("CREATE TABLE sales (region VARCHAR(50), product VARCHAR(50), sales INTEGER)");

        // Insert test data: 3 regions × 2 products = 6 rows
        executeSQL("INSERT INTO sales VALUES ('North', 'Widget', 100)");
        executeSQL("INSERT INTO sales VALUES ('North', 'Gadget', 150)");
        executeSQL("INSERT INTO sales VALUES ('South', 'Widget', 200)");
        executeSQL("INSERT INTO sales VALUES ('South', 'Gadget', 250)");
        executeSQL("INSERT INTO sales VALUES ('West', 'Widget', 300)");
        executeSQL("INSERT INTO sales VALUES ('West', 'Gadget', 350)");
    }

    void executeSQL(const std::string& sql) {
        parser::Lexer lexer(sql);
        parser::ASTArena arena;
        parser::Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "Parse error for: " << sql;

        sblr::BytecodeGenerator generator(lexer.stringPool(), db_.get());
        auto bytecode = generator.generate(result.statement());
        ASSERT_TRUE(bytecode.success()) << "Bytecode generation error for: " << sql;

        sblr::Executor executor(db_.get());
        auto exec_result = executor.execute(bytecode.bytecode());
        ASSERT_TRUE(exec_result.success()) << "Execution error for: " << sql
            << " - " << exec_result.error();
    }

    std::vector<std::vector<std::string>> queryResults(const std::string& sql) {
        parser::Lexer lexer(sql);
        parser::ASTArena arena;
        parser::Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        if (!result.success()) {
            ADD_FAILURE() << "Parse error for: " << sql;
            return {};
        }

        sblr::BytecodeGenerator generator(lexer.stringPool(), db_.get());
        auto bytecode = generator.generate(result.statement());
        if (!bytecode.success()) {
            ADD_FAILURE() << "Bytecode generation error for: " << sql;
            return {};
        }

        sblr::Executor executor(db_.get());
        auto exec_result = executor.execute(bytecode.bytecode());
        if (!exec_result.success()) {
            ADD_FAILURE() << "Execution error for: " << sql << " - " << exec_result.error();
            return {};
        }

        // Convert result to string vectors for easy comparison
        std::vector<std::vector<std::string>> rows;
        auto& result_set = exec_result.resultSet();

        for (const auto& row : result_set.rows()) {
            std::vector<std::string> string_row;
            for (const auto& value : row) {
                if (value.isNull()) {
                    string_row.push_back("NULL");
                } else if (value.isInteger()) {
                    string_row.push_back(std::to_string(value.toInt64()));
                } else if (value.isString()) {
                    string_row.push_back(value.toString());
                } else {
                    string_row.push_back(value.toString());
                }
            }
            rows.push_back(string_row);
        }

        return rows;
    }
};

/**
 * Test ROLLUP with single column
 *
 * ROLLUP(region) produces:
 * - Detail rows grouped by region
 * - Grand total row (region = NULL)
 *
 * Expected output:
 * North  | 250
 * South  | 450
 * West   | 650
 * NULL   | 1350  (grand total)
 */
TEST_F(AdvancedGroupingTest, ROLLUP_SingleColumn) {
    std::string query =
        "SELECT region, SUM(sales) as total_sales "
        "FROM sales "
        "GROUP BY ROLLUP(region) "
        "ORDER BY region NULLS LAST";

    auto results = queryResults(query);

    ASSERT_EQ(results.size(), 4) << "Expected 4 rows (3 regions + 1 grand total)";

    // Check detail rows
    EXPECT_EQ(results[0][0], "North");
    EXPECT_EQ(results[0][1], "250");

    EXPECT_EQ(results[1][0], "South");
    EXPECT_EQ(results[1][1], "450");

    EXPECT_EQ(results[2][0], "West");
    EXPECT_EQ(results[2][1], "650");

    // Check grand total row
    EXPECT_EQ(results[3][0], "NULL");
    EXPECT_EQ(results[3][1], "1350");
}

/**
 * Test ROLLUP with multiple columns
 *
 * ROLLUP(region, product) produces:
 * - Detail rows: GROUP BY region, product
 * - Subtotal rows: GROUP BY region (product = NULL)
 * - Grand total row: (region = NULL, product = NULL)
 *
 * Expected output:
 * North  | Gadget | 150
 * North  | Widget | 100
 * North  | NULL   | 250   (North subtotal)
 * South  | Gadget | 250
 * South  | Widget | 200
 * South  | NULL   | 450   (South subtotal)
 * West   | Gadget | 350
 * West   | Widget | 300
 * West   | NULL   | 650   (West subtotal)
 * NULL   | NULL   | 1350  (Grand total)
 */
TEST_F(AdvancedGroupingTest, ROLLUP_MultipleColumns) {
    std::string query =
        "SELECT region, product, SUM(sales) as total_sales "
        "FROM sales "
        "GROUP BY ROLLUP(region, product) "
        "ORDER BY region NULLS LAST, product NULLS LAST";

    auto results = queryResults(query);

    ASSERT_EQ(results.size(), 10) << "Expected 10 rows (6 detail + 3 subtotals + 1 grand total)";

    // North detail
    EXPECT_EQ(results[0][0], "North");
    EXPECT_EQ(results[0][1], "Gadget");
    EXPECT_EQ(results[0][2], "150");

    EXPECT_EQ(results[1][0], "North");
    EXPECT_EQ(results[1][1], "Widget");
    EXPECT_EQ(results[1][2], "100");

    // North subtotal
    EXPECT_EQ(results[2][0], "North");
    EXPECT_EQ(results[2][1], "NULL");
    EXPECT_EQ(results[2][2], "250");

    // South detail
    EXPECT_EQ(results[3][0], "South");
    EXPECT_EQ(results[3][1], "Gadget");
    EXPECT_EQ(results[3][2], "250");

    EXPECT_EQ(results[4][0], "South");
    EXPECT_EQ(results[4][1], "Widget");
    EXPECT_EQ(results[4][2], "200");

    // South subtotal
    EXPECT_EQ(results[5][0], "South");
    EXPECT_EQ(results[5][1], "NULL");
    EXPECT_EQ(results[5][2], "450");

    // West detail
    EXPECT_EQ(results[6][0], "West");
    EXPECT_EQ(results[6][1], "Gadget");
    EXPECT_EQ(results[6][2], "350");

    EXPECT_EQ(results[7][0], "West");
    EXPECT_EQ(results[7][1], "Widget");
    EXPECT_EQ(results[7][2], "300");

    // West subtotal
    EXPECT_EQ(results[8][0], "West");
    EXPECT_EQ(results[8][1], "NULL");
    EXPECT_EQ(results[8][2], "650");

    // Grand total
    EXPECT_EQ(results[9][0], "NULL");
    EXPECT_EQ(results[9][1], "NULL");
    EXPECT_EQ(results[9][2], "1350");
}

/**
 * Test CUBE with two columns
 *
 * CUBE(region, product) produces all combinations:
 * - (region, product) - detail rows
 * - (region)          - region subtotals
 * - (product)         - product subtotals
 * - ()                - grand total
 *
 * Total rows: 6 detail + 3 region + 2 product + 1 grand = 12 rows
 */
TEST_F(AdvancedGroupingTest, CUBE_TwoColumns) {
    std::string query =
        "SELECT region, product, SUM(sales) as total_sales "
        "FROM sales "
        "GROUP BY CUBE(region, product) "
        "ORDER BY region NULLS LAST, product NULLS LAST";

    auto results = queryResults(query);

    // CUBE produces 2^n grouping sets where n is number of columns
    // CUBE(region, product) = 2^2 = 4 grouping sets:
    // - (region, product): 6 rows
    // - (region): 3 rows
    // - (product): 2 rows
    // - (): 1 row
    // Total: 12 rows
    ASSERT_EQ(results.size(), 12) << "Expected 12 rows for CUBE(region, product)";

    // Verify grand total exists
    bool found_grand_total = false;
    for (const auto& row : results) {
        if (row[0] == "NULL" && row[1] == "NULL") {
            EXPECT_EQ(row[2], "1350") << "Grand total should be 1350";
            found_grand_total = true;
        }
    }
    EXPECT_TRUE(found_grand_total) << "CUBE should produce grand total row";

    // Verify product subtotals exist (region = NULL, product != NULL)
    int product_subtotals = 0;
    for (const auto& row : results) {
        if (row[0] == "NULL" && row[1] != "NULL") {
            product_subtotals++;
        }
    }
    EXPECT_EQ(product_subtotals, 2) << "CUBE should produce 2 product subtotals (Widget, Gadget)";
}

/**
 * Test GROUPING SETS with explicit sets
 *
 * GROUPING SETS ((region), (product)) produces exactly 2 grouping sets:
 * - GROUP BY region
 * - GROUP BY product
 *
 * No grand total or detail rows
 */
TEST_F(AdvancedGroupingTest, GROUPING_SETS_Explicit) {
    std::string query =
        "SELECT region, product, SUM(sales) as total_sales "
        "FROM sales "
        "GROUP BY GROUPING SETS ((region), (product)) "
        "ORDER BY region NULLS LAST, product NULLS LAST";

    auto results = queryResults(query);

    // 3 region groups + 2 product groups = 5 rows
    ASSERT_EQ(results.size(), 5) << "Expected 5 rows (3 region groups + 2 product groups)";

    // Product groups should have region = NULL
    int product_groups = 0;
    for (const auto& row : results) {
        if (row[0] == "NULL") {
            EXPECT_NE(row[1], "NULL") << "Product should not be NULL in product grouping";
            product_groups++;
        }
    }
    EXPECT_EQ(product_groups, 2) << "Should have 2 product groups";

    // Region groups should have product = NULL
    int region_groups = 0;
    for (const auto& row : results) {
        if (row[1] == "NULL") {
            EXPECT_NE(row[0], "NULL") << "Region should not be NULL in region grouping";
            region_groups++;
        }
    }
    EXPECT_EQ(region_groups, 3) << "Should have 3 region groups";
}

/**
 * Test GROUPING() function basic usage
 *
 * GROUPING(column) returns:
 * - 0 when column is part of the grouping (detail row)
 * - 1 when column is aggregated (subtotal/total row)
 *
 * This allows distinguishing between:
 * - Real NULL values in data
 * - NULL values generated by aggregation
 */
TEST_F(AdvancedGroupingTest, GROUPING_Function_BasicUsage) {
    std::string query =
        "SELECT region, SUM(sales) as total_sales, GROUPING(region) as is_total "
        "FROM sales "
        "GROUP BY ROLLUP(region) "
        "ORDER BY region NULLS LAST";

    auto results = queryResults(query);

    ASSERT_EQ(results.size(), 4) << "Expected 4 rows";

    // Detail rows should have GROUPING(region) = 0
    EXPECT_EQ(results[0][0], "North");
    EXPECT_EQ(results[0][2], "0") << "Detail row should have GROUPING(region) = 0";

    EXPECT_EQ(results[1][0], "South");
    EXPECT_EQ(results[1][2], "0") << "Detail row should have GROUPING(region) = 0";

    EXPECT_EQ(results[2][0], "West");
    EXPECT_EQ(results[2][2], "0") << "Detail row should have GROUPING(region) = 0";

    // Grand total row should have GROUPING(region) = 1
    EXPECT_EQ(results[3][0], "NULL");
    EXPECT_EQ(results[3][2], "1") << "Grand total row should have GROUPING(region) = 1";
}

/**
 * Test ROLLUP with HAVING clause
 *
 * Verifies that HAVING clause correctly filters grouped results
 * from ROLLUP operations
 */
TEST_F(AdvancedGroupingTest, ROLLUP_WithHAVING) {
    std::string query =
        "SELECT region, SUM(sales) as total_sales "
        "FROM sales "
        "GROUP BY ROLLUP(region) "
        "HAVING SUM(sales) > 500 "
        "ORDER BY region NULLS LAST";

    auto results = queryResults(query);

    // Only West (650) and grand total (1350) should pass the filter
    ASSERT_EQ(results.size(), 2) << "Expected 2 rows (West and grand total)";

    EXPECT_EQ(results[0][0], "West");
    EXPECT_EQ(results[0][1], "650");

    EXPECT_EQ(results[1][0], "NULL");
    EXPECT_EQ(results[1][1], "1350");
}

/**
 * Test CUBE with ORDER BY
 *
 * Verifies that ORDER BY correctly sorts CUBE results,
 * including proper NULL handling
 */
TEST_F(AdvancedGroupingTest, CUBE_WithORDERBY) {
    std::string query =
        "SELECT region, product, SUM(sales) as total_sales "
        "FROM sales "
        "GROUP BY CUBE(region, product) "
        "ORDER BY total_sales DESC";

    auto results = queryResults(query);

    ASSERT_EQ(results.size(), 12) << "Expected 12 rows";

    // First row should be grand total (highest sum)
    EXPECT_EQ(results[0][2], "1350") << "Highest total should be grand total";

    // Verify descending order
    for (size_t i = 1; i < results.size(); i++) {
        int64_t prev_sales = std::stoll(results[i - 1][2]);
        int64_t curr_sales = std::stoll(results[i][2]);
        EXPECT_GE(prev_sales, curr_sales) << "Results should be in descending order";
    }
}

/**
 * Test GROUPING SETS with multiple aggregate functions
 *
 * Verifies that multiple aggregate functions work correctly
 * with GROUPING SETS
 */
TEST_F(AdvancedGroupingTest, GROUPING_SETS_WithMultipleAggregates) {
    std::string query =
        "SELECT region, "
        "       SUM(sales) as total_sales, "
        "       AVG(sales) as avg_sales, "
        "       COUNT(*) as num_products "
        "FROM sales "
        "GROUP BY GROUPING SETS ((region), ()) "
        "ORDER BY region NULLS LAST";

    auto results = queryResults(query);

    // 3 regions + 1 grand total = 4 rows
    ASSERT_EQ(results.size(), 4) << "Expected 4 rows (3 regions + grand total)";

    // Check North aggregates
    EXPECT_EQ(results[0][0], "North");
    EXPECT_EQ(results[0][1], "250");      // SUM
    EXPECT_EQ(results[0][2], "125");      // AVG (250/2)
    EXPECT_EQ(results[0][3], "2");        // COUNT

    // Check grand total aggregates
    EXPECT_EQ(results[3][0], "NULL");
    EXPECT_EQ(results[3][1], "1350");     // SUM
    EXPECT_EQ(results[3][2], "225");      // AVG (1350/6)
    EXPECT_EQ(results[3][3], "6");        // COUNT
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
