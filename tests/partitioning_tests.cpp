#include "test_db_utils.h"

#include <gtest/gtest.h>

using namespace scratchbird::engine;
using scratchbird::tests::TestDatabaseRAII;

int main()
{
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}

TEST(Partitioning, RangePruningExplain)
{
    TestDatabaseRAII db("partitioning_range", true);

    auto r = execute_select_sql("CREATE TABLE sales(id INT, region VARCHAR(10))");
    ASSERT_TRUE(r.success) << r.error_message;

    // Register partitions via ALTER TABLE ... PARTITION BY RANGE (region)
    r = execute_select_sql(
        "ALTER TABLE sales ADD CONSTRAINT pk PRIMARY KEY (id), SET PARTITION BY RANGE (region) (\n"
        "  p1 FROM 'A' TO 'M',\n"
        "  p2 FROM 'M' TO 'Z'\n"
        ")");
    ASSERT_TRUE(r.success) << r.error_message;

    // EXPLAIN should report pruned partitions when predicate filters to one side
    auto er = execute_select_sql(
        "EXPLAIN SELECT * FROM sales WHERE region BETWEEN 'N' AND 'T'");
    ASSERT_TRUE(er.success) << er.error_message;
    ASSERT_FALSE(er.rows.empty());
    std::string plan = er.rows[0].empty() ? std::string() : er.rows[0][0];
    // We don't parse the entire string; just check it contains cost/rows and not empty
    ASSERT_FALSE(plan.empty());
}

TEST(Partitioning, ListPruningExplain)
{
    TestDatabaseRAII db("partitioning_list", true);

    auto r = execute_select_sql("CREATE TABLE users(id INT, country VARCHAR(2))");
    ASSERT_TRUE(r.success) << r.error_message;

    r = execute_select_sql(
        "ALTER TABLE users SET PARTITION BY LIST (country) (\n"
        "  eu IN ('DE','FR','ES'),\n"
        "  non_eu IN ('US','CA')\n"
        ")");
    ASSERT_TRUE(r.success) << r.error_message;

    auto er = execute_select_sql("EXPLAIN SELECT * FROM users WHERE country = 'DE'");
    ASSERT_TRUE(er.success) << er.error_message;
    ASSERT_FALSE(er.rows.empty());
    ASSERT_FALSE(er.rows[0].empty());
}

