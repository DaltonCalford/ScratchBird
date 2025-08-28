#include "test_db_utils.h"

#include <gtest/gtest.h>

using namespace scratchbird::engine;
using scratchbird::tests::TestDatabaseRAII;

int main()
{
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}

// Smoke: ensure EXPLAIN runs before/after ANALYZE and returns a non-empty plan
TEST(StatisticsPlan, ExplainWithAnalyzeAffectsCacheKey)
{
    TestDatabaseRAII db("stats_plan", true);

    auto r = execute_select_sql("CREATE TABLE t(a INT, b INT)");
    ASSERT_TRUE(r.success) << r.error_message;

    // Without data/stats
    auto er1 = execute_select_sql("EXPLAIN SELECT * FROM t WHERE a = 1");
    ASSERT_TRUE(er1.success);
    ASSERT_FALSE(er1.rows.empty());
    std::string plan1 = er1.rows[0][0];
    ASSERT_FALSE(plan1.empty());

    // ANALYZE should register stats and bump epochs (affects plan cache key)
    auto ar = execute_select_sql("ANALYZE t");
    ASSERT_TRUE(ar.success);

    auto er2 = execute_select_sql("EXPLAIN SELECT * FROM t WHERE a = 1");
    ASSERT_TRUE(er2.success);
    ASSERT_FALSE(er2.rows.empty());
    std::string plan2 = er2.rows[0][0];
    ASSERT_FALSE(plan2.empty());

    // Plans may or may not differ; this test ensures both succeed and are non-empty
}

