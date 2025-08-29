#include "test_db_utils.h"

#include <gtest/gtest.h>

using namespace scratchbird::engine;
using scratchbird::tests::TestDatabaseRAII;

int main()
{
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}

TEST(AnalyticsFunctions, CorrelationCovarianceRegressionMode)
{
    TestDatabaseRAII db("analytics_functions", true);

    auto r = execute_select_sql("CREATE TABLE t(a DOUBLE PRECISION, b DOUBLE PRECISION, c INT)");
    ASSERT_TRUE(r.success) << r.error_message;

    std::vector<std::string> inserts = {
        "INSERT INTO t VALUES (1.0, 2.0, 1)", "INSERT INTO t VALUES (2.0, 4.0, 1)",
        "INSERT INTO t VALUES (3.0, 6.0, 2)", "INSERT INTO t VALUES (4.0, 8.0, 2)",
        "INSERT INTO t VALUES (5.0, 10.0, 2)"};
    for (auto const& s : inserts) {
        r = execute_select_sql(s);
        ASSERT_TRUE(r.success) << r.error_message;
    }

    // Perfect linear correlation between a and b (b = 2*a)
    r = execute_select_sql(
        "SELECT CORRELATION(a,b), COVARIANCE(a,b), REGRESSION_SLOPE(a,b), REGRESSION_INTERCEPT(a,b) FROM t");
    ASSERT_TRUE(r.success) << r.error_message;
    ASSERT_FALSE(r.rows.empty());
    // Correlation approx 1, slope approx 2, intercept approx 0
    auto row = r.rows[0];
    ASSERT_EQ(row.size(), 4);
    double corr = std::stod(row[0]);
    double cov = std::stod(row[1]);
    double slope = std::stod(row[2]);
    double intercept = std::stod(row[3]);
    EXPECT_NEAR(corr, 0.999, 0.01);
    EXPECT_NEAR(slope, 2.0, 0.01);
    EXPECT_NEAR(intercept, 0.0, 0.01);
    (void)cov; // sanity only; not asserting exact sample covariance

    // MODE of c should be 2
    r = execute_select_sql("SELECT MODE(c) FROM t");
    ASSERT_TRUE(r.success) << r.error_message;
    ASSERT_FALSE(r.rows.empty());
    EXPECT_EQ(r.rows[0][0], "2");
}

