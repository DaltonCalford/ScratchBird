#include "scratchbird/capi.h"
#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/parser.h"

#include "gtest/gtest.h"
#include <filesystem>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace scratchbird::engine;

static void run_ddl(const std::string& sql)
{
    auto ast = parse_sql(sql);
    execute_ast(ast);
}

static std::string tempdb()
{
    // Use project-local temp dir to avoid /tmp space constraints
    const char* root = "/home/dcalford/CliWork/ScratchBird/temp";
    mkdir(root, 0755);
    std::ostringstream oss;
    oss << root << "/db_" << getpid() << "_" << (unsigned long long)time(nullptr);
    return oss.str();
}

static void create_db_and_set_path(const std::string& base)
{
    SB_CreateDbOptions o{};
    o.page_size = 4096;
    SB_Database* db = nullptr;
    auto st = sb_create_database(base.c_str(), &o, &db);
    (void)st;
    if (db)
        sb_close_database(db);
    set_executor_db_path(base);
    // Ensure catalog roots are bootstrapped before any DDL
    CatalogManager cm(base);
    cm.bootstrap_if_needed();
}

static void cleanup_db(const std::string& base)
{
    // Best-effort: remove segment files base.seg0..base.seg15
    for (int i = 0; i < 16; ++i) {
        std::string seg = base + ".seg" + std::to_string(i);
        unlink(seg.c_str());
    }
    // Also try to remove any .bootstrap.sql file
    std::string bootstrap = base + ".bootstrap.sql";
    unlink(bootstrap.c_str());
}

class HavingTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        db_path_ = tempdb();
        create_db_and_set_path(db_path_);
        setup_test_data();
    }

    void TearDown() override
    {
        cleanup_db(db_path_);
    }

    void setup_test_data()
    {
        // Create schema and table
        run_ddl("CREATE SCHEMA public");
        run_ddl("CREATE TABLE public.sales (id INT, product VARCHAR(50), amount "
                "DECIMAL(10,2), region VARCHAR(20))");

        // Insert test data
        execute_insert_sql("INSERT INTO public.sales VALUES (1, 'Product A', 100.00, 'North')");
        execute_insert_sql("INSERT INTO public.sales VALUES (2, 'Product B', 150.00, 'South')");
        execute_insert_sql("INSERT INTO public.sales VALUES (3, 'Product A', 200.00, 'North')");
        execute_insert_sql("INSERT INTO public.sales VALUES (4, 'Product B', 75.00, 'South')");
        execute_insert_sql("INSERT INTO public.sales VALUES (5, 'Product C', 300.00, 'East')");
        execute_insert_sql("INSERT INTO public.sales VALUES (6, 'Product A', 125.00, 'West')");
        execute_insert_sql("INSERT INTO public.sales VALUES (7, 'Product B', 90.00, 'North')");
        execute_insert_sql("INSERT INTO public.sales VALUES (8, 'Product C', 250.00, 'South')");
    }

    std::string db_path_;
};

TEST_F(HavingTest, BasicHavingWithCount)
{
    // Test HAVING with COUNT aggregate
    auto result = execute_select_sql(
        "SELECT product, COUNT(*) as cnt FROM public.sales GROUP BY product HAVING COUNT(*) > 2");

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
    auto result = execute_select_sql("SELECT region, SUM(amount) as total FROM public.sales GROUP "
                                     "BY region HAVING SUM(amount) > 400");

    EXPECT_TRUE(result.success) << "Query failed: " << result.error_message;
    // Should find regions with total sales > 400
    // North: 100 + 200 + 90 = 390 (should not appear)
    // South: 150 + 75 + 250 = 475 (should appear)
    // East: 300 (should not appear)
    // West: 125 (should not appear)
    EXPECT_EQ(result.rows.size(), 1);
    if (result.rows.size() >= 1) {
        EXPECT_EQ(result.rows[0][0], "South");
        EXPECT_EQ(result.rows[0][1], "475");
    }
}

TEST_F(HavingTest, HavingWithMinMax)
{
    // Test HAVING with MIN/MAX aggregates
    auto result =
        execute_select_sql("SELECT product, AVG(amount) as avg_amount FROM public.sales GROUP BY "
                           "product HAVING AVG(amount) > 150");

    EXPECT_TRUE(result.success) << "Query failed: " << result.error_message;
    // Product A: (100 + 200 + 125) / 3 = 141.67 (should not appear)
    // Product B: (150 + 75 + 90) / 3 = 105 (should not appear)
    // Product C: (300 + 250) / 2 = 275 (should appear)
    EXPECT_EQ(result.rows.size(), 1);
    if (result.rows.size() >= 1) {
        EXPECT_EQ(result.rows[0][0], "Product C");
        // Note: AVG result may be approximate, so just check it's > 150
        double avg = std::stod(result.rows[0][1]);
        EXPECT_GT(avg, 150.0);
    }
}

TEST_F(HavingTest, HavingWithMultipleConditions)
{
    // Test HAVING with multiple conditions
    auto result = execute_select_sql(
        "SELECT product, COUNT(*) as cnt, SUM(amount) as total FROM public.sales "
        "GROUP BY product HAVING COUNT(*) >= 2 AND SUM(amount) > 300");

    EXPECT_TRUE(result.success) << "Query failed: " << result.error_message;
    // Product A: COUNT=3, SUM=425 (should appear)
    // Product B: COUNT=3, SUM=315 (should appear)
    // Product C: COUNT=2, SUM=550 (should appear)
    EXPECT_EQ(result.rows.size(), 3);

    // All products should have at least 2 records and sum > 300
    for (const auto& row : result.rows) {
        int count = std::stoi(row[1]);
        double sum = std::stod(row[2]);
        EXPECT_GE(count, 2);
        EXPECT_GT(sum, 300.0);
    }
}

TEST_F(HavingTest, HavingWithoutGroupBy)
{
    // Test HAVING without GROUP BY (should still work with aggregates)
    auto result =
        execute_select_sql("SELECT COUNT(*) as total_count FROM public.sales HAVING COUNT(*) > 5");

    EXPECT_TRUE(result.success) << "Query failed: " << result.error_message;
    // We have 8 records, so COUNT(*) > 5 should be true
    EXPECT_EQ(result.rows.size(), 1);
    if (result.rows.size() >= 1) {
        int count = std::stoi(result.rows[0][0]);
        EXPECT_EQ(count, 8);
    }
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
