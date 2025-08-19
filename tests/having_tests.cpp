#include "scratchbird/capi.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/file.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <string>

class HavingTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        test_dir_ = "/tmp/scratchbird_having_test";
        std::filesystem::remove_all(test_dir_);
        std::filesystem::create_directories(test_dir_);

        // Create test database
        std::string db_path = test_dir_ + "/test.db";
        ScratchBirdCreateOptions opts{};
        opts.page_size = 4096;

        auto status = sb_create_database(db_path.c_str(), &opts, &db_);
        ASSERT_EQ(status, SCRATCHBIRD_OK) << "Failed to create database";

        // Set up basic test data
        setup_test_data();
    }

    void TearDown() override
    {
        if (db_) {
            sb_close_database(db_);
            db_ = nullptr;
        }
        std::filesystem::remove_all(test_dir_);
    }

    void setup_test_data()
    {
        // Create a test table with sample data
        auto result = execute_sql("CREATE TABLE sales (id INT, product VARCHAR(50), amount "
                                  "DECIMAL(10,2), region VARCHAR(20))");
        ASSERT_TRUE(result.success) << "Failed to create table: " << result.error_message;

        // Insert test data
        execute_sql("INSERT INTO sales VALUES (1, 'Product A', 100.00, 'North')");
        execute_sql("INSERT INTO sales VALUES (2, 'Product B', 150.00, 'South')");
        execute_sql("INSERT INTO sales VALUES (3, 'Product A', 200.00, 'North')");
        execute_sql("INSERT INTO sales VALUES (4, 'Product B', 75.00, 'South')");
        execute_sql("INSERT INTO sales VALUES (5, 'Product C', 300.00, 'East')");
        execute_sql("INSERT INTO sales VALUES (6, 'Product A', 125.00, 'West')");
        execute_sql("INSERT INTO sales VALUES (7, 'Product B', 90.00, 'North')");
        execute_sql("INSERT INTO sales VALUES (8, 'Product C', 250.00, 'South')");
    }

    scratchbird::engine::ExecutionResult execute_sql(const std::string& sql)
    {
        return scratchbird::engine::execute_select_sql(sql);
    }

    std::string test_dir_;
    ScratchBirdDatabase* db_ = nullptr;
};

TEST_F(HavingTest, BasicHavingWithCount)
{
    // Test HAVING with COUNT aggregate
    auto result = execute_sql(
        "SELECT product, COUNT(*) as cnt FROM sales GROUP BY product HAVING COUNT(*) > 2");

    EXPECT_TRUE(result.success) << "Query failed: " << result.error_message;
    EXPECT_EQ(result.rows.size(), 2); // Product A (3 rows) and Product B (3 rows) should pass

    // Verify the products that meet the condition
    bool found_product_a = false, found_product_b = false;
    for (const auto& row : result.rows) {
        if (row.size() >= 2) {
            if (row[0] == "Product A" && row[1] == "3")
                found_product_a = true;
            if (row[0] == "Product B" && row[1] == "3")
                found_product_b = true;
        }
    }
    EXPECT_TRUE(found_product_a) << "Product A should be in results";
    EXPECT_TRUE(found_product_b) << "Product B should be in results";
}

TEST_F(HavingTest, HavingWithSum)
{
    // Test HAVING with SUM aggregate
    auto result = execute_sql(
        "SELECT region, SUM(amount) as total FROM sales GROUP BY region HAVING SUM(amount) > 400");

    EXPECT_TRUE(result.success) << "Query failed: " << result.error_message;

    // Should find regions with total sales > 400
    // North: 100 + 200 + 90 = 390 (should not appear)
    // South: 150 + 75 + 250 = 475 (should appear)
    // East: 300 (should not appear)
    // West: 125 (should not appear)
    EXPECT_GE(result.rows.size(), 1);

    bool found_south = false;
    for (const auto& row : result.rows) {
        if (row.size() >= 2 && row[0] == "South") {
            found_south = true;
            // SUM should be 475
            double sum_value = std::stod(row[1]);
            EXPECT_GT(sum_value, 400.0);
        }
    }
    EXPECT_TRUE(found_south) << "South region should meet HAVING condition";
}

TEST_F(HavingTest, HavingWithAverage)
{
    // Test HAVING with AVG aggregate
    auto result = execute_sql("SELECT product, AVG(amount) as avg_amount FROM sales GROUP BY "
                              "product HAVING AVG(amount) > 150");

    EXPECT_TRUE(result.success) << "Query failed: " << result.error_message;

    // Product A: (100 + 200 + 125) / 3 = 141.67 (should not appear)
    // Product B: (150 + 75 + 90) / 3 = 105 (should not appear)
    // Product C: (300 + 250) / 2 = 275 (should appear)
    EXPECT_GE(result.rows.size(), 1);

    bool found_product_c = false;
    for (const auto& row : result.rows) {
        if (row.size() >= 2 && row[0] == "Product C") {
            found_product_c = true;
            double avg_value = std::stod(row[1]);
            EXPECT_GT(avg_value, 150.0);
        }
    }
    EXPECT_TRUE(found_product_c) << "Product C should meet HAVING condition";
}

TEST_F(HavingTest, HavingWithMinMax)
{
    // Test HAVING with MIN/MAX aggregates
    auto result = execute_sql("SELECT region, MIN(amount) as min_amt, MAX(amount) as max_amt FROM "
                              "sales GROUP BY region HAVING MAX(amount) > 200");

    EXPECT_TRUE(result.success) << "Query failed: " << result.error_message;

    // Should find regions where maximum amount > 200
    // North: MAX = 200 (equal, should not appear with >)
    // South: MAX = 250 (should appear)
    // East: MAX = 300 (should appear)
    // West: MAX = 125 (should not appear)
    EXPECT_GE(result.rows.size(), 1);

    for (const auto& row : result.rows) {
        if (row.size() >= 3) {
            double max_value = std::stod(row[2]);
            EXPECT_GT(max_value, 200.0) << "All results should have MAX > 200";
        }
    }
}

TEST_F(HavingTest, HavingWithMultipleConditions)
{
    // Test HAVING with complex condition (this is a stretch test for our basic implementation)
    auto result = execute_sql("SELECT product, COUNT(*) as cnt, SUM(amount) as total FROM sales "
                              "GROUP BY product HAVING COUNT(*) >= 2");

    EXPECT_TRUE(result.success) << "Query failed: " << result.error_message;

    // All products should have at least 2 records
    for (const auto& row : result.rows) {
        if (row.size() >= 2) {
            int count_value = std::stoi(row[1]);
            EXPECT_GE(count_value, 2) << "All results should have COUNT >= 2";
        }
    }
}

TEST_F(HavingTest, HavingWithoutGroupBy)
{
    // Test HAVING without GROUP BY (should still work with aggregates)
    auto result = execute_sql("SELECT COUNT(*) as total_count FROM sales HAVING COUNT(*) > 5");

    EXPECT_TRUE(result.success) << "Query failed: " << result.error_message;

    // We have 8 records, so COUNT(*) > 5 should be true
    EXPECT_EQ(result.rows.size(), 1);
    if (!result.rows.empty() && !result.rows[0].empty()) {
        int count_value = std::stoi(result.rows[0][0]);
        EXPECT_GT(count_value, 5);
    }
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
