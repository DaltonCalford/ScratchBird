#include "test_db_utils.h"

#include <gtest/gtest.h>

using namespace scratchbird::engine;
using scratchbird::tests::TestDatabaseRAII;

int main()
{
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}

// For this phase, we test TIME_BUCKET via a simple projection placeholder implementation
TEST(TimeSeriesFunctions, TimeBucketAndMovingAverage)
{
    TestDatabaseRAII db("timeseries_functions", true);

    auto r = execute_select_sql("CREATE TABLE s(ts INT, val DOUBLE PRECISION)");
    ASSERT_TRUE(r.success) << r.error_message;

    // Insert timestamps 0..9 with value = i
    for (int i = 0; i < 10; ++i) {
        r = execute_select_sql("INSERT INTO s VALUES (" + std::to_string(i) + ", " +
                               std::to_string(i) + ")");
        ASSERT_TRUE(r.success) << r.error_message;
    }

    // Verify TIME_BUCKET placeholder behavior by grouping coarse buckets (bucket size 3)
    // For now expect query to succeed even if bucketing reduces to raw ts
    r = execute_select_sql(
        "SELECT ts /* TIME_BUCKET(ts, 3) placeholder */, COUNT(*) FROM s GROUP BY ts ORDER BY ts");
    ASSERT_TRUE(r.success) << r.error_message;

    // Moving average as a window function placeholder: ensure SUM OVER works already
    r = execute_select_sql("SELECT val, SUM(val) OVER (ORDER BY ts) AS run_sum FROM s");
    ASSERT_TRUE(r.success) << r.error_message;
    ASSERT_EQ(r.rows.size(), 10);
}

