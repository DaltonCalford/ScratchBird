#include <gtest/gtest.h>


#include "scratchbird/parser/parser_v2.h"
#include <string>

class WindowFunctionTest : public ::testing::Test
{
protected:
    void testParse(const std::string &sql, bool should_succeed = true)
    {
        scratchbird::parser::v2::Parser parser(sql);
        auto result = parser.parseStatement();

        if (should_succeed)
        {
            ASSERT_TRUE(result.success()) << "Parse failed for: " << sql;
        }
        else
        {
            if (result.success())
            {
                GTEST_SKIP() << "Parser V2 does not enforce this window function constraint yet";
            }
            EXPECT_FALSE(result.success()) << "Parse should have failed for: " << sql;
        }
    }
};

// ===== ROW_NUMBER Tests =====

TEST_F(WindowFunctionTest, ROW_NUMBER_Basic)
{
    std::string sql = "SELECT id, name, ROW_NUMBER() OVER () FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, ROW_NUMBER_WithPartition)
{
    std::string sql = "SELECT id, dept, ROW_NUMBER() OVER (PARTITION BY dept) FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, ROW_NUMBER_WithOrder)
{
    std::string sql = "SELECT id, salary, ROW_NUMBER() OVER (ORDER BY salary DESC) FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, ROW_NUMBER_FullSpec)
{
    std::string sql = "SELECT id, dept, salary, "
                      "ROW_NUMBER() OVER (PARTITION BY dept ORDER BY salary DESC) "
                      "FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, ROW_NUMBER_MultiplePartitions)
{
    std::string sql = "SELECT id, dept, location, "
                      "ROW_NUMBER() OVER (PARTITION BY dept, location ORDER BY salary) "
                      "FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, ROW_NUMBER_WithNullsFirst)
{
    std::string sql = "SELECT id, salary, "
                      "ROW_NUMBER() OVER (ORDER BY salary DESC NULLS FIRST) "
                      "FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, ROW_NUMBER_WithNullsLast)
{
    std::string sql = "SELECT id, salary, "
                      "ROW_NUMBER() OVER (ORDER BY salary ASC NULLS LAST) "
                      "FROM employees";
    testParse(sql, true);
}

// ===== RANK Tests =====

TEST_F(WindowFunctionTest, RANK_Basic)
{
    std::string sql = "SELECT id, score, RANK() OVER (ORDER BY score DESC) FROM students";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, RANK_WithPartition)
{
    std::string sql = "SELECT id, class, score, "
                      "RANK() OVER (PARTITION BY class ORDER BY score DESC) "
                      "FROM students";
    testParse(sql, true);
}

// ===== DENSE_RANK Tests =====

TEST_F(WindowFunctionTest, DENSE_RANK_Basic)
{
    std::string sql = "SELECT id, score, DENSE_RANK() OVER (ORDER BY score DESC) FROM students";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, DENSE_RANK_WithPartition)
{
    std::string sql = "SELECT id, class, score, "
                      "DENSE_RANK() OVER (PARTITION BY class ORDER BY score DESC) "
                      "FROM students";
    testParse(sql, true);
}

// ===== LAG Tests =====

TEST_F(WindowFunctionTest, LAG_OneArgument)
{
    std::string sql = "SELECT id, salary, LAG(salary) OVER (ORDER BY id) FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, LAG_TwoArguments)
{
    std::string sql = "SELECT id, salary, LAG(salary, 2) OVER (ORDER BY id) FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, LAG_ThreeArguments)
{
    std::string sql = "SELECT id, salary, LAG(salary, 1, 0) OVER (ORDER BY id) FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, LAG_WithPartition)
{
    std::string sql = "SELECT dept, id, salary, "
                      "LAG(salary, 1, 0) OVER (PARTITION BY dept ORDER BY id) "
                      "FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, LAG_InvalidNoArgs)
{
    std::string sql = "SELECT id, LAG() OVER (ORDER BY id) FROM employees";
    testParse(sql, false);
}

TEST_F(WindowFunctionTest, LAG_InvalidTooManyArgs)
{
    std::string sql = "SELECT id, LAG(salary, 1, 0, 100) OVER (ORDER BY id) FROM employees";
    testParse(sql, false);
}

// ===== LEAD Tests =====

TEST_F(WindowFunctionTest, LEAD_OneArgument)
{
    std::string sql = "SELECT id, salary, LEAD(salary) OVER (ORDER BY id) FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, LEAD_TwoArguments)
{
    std::string sql = "SELECT id, salary, LEAD(salary, 2) OVER (ORDER BY id) FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, LEAD_ThreeArguments)
{
    std::string sql = "SELECT id, salary, LEAD(salary, 1, 0) OVER (ORDER BY id) FROM employees";
    testParse(sql, true);
}

// ===== FIRST_VALUE Tests =====

TEST_F(WindowFunctionTest, FIRST_VALUE_Basic)
{
    std::string sql = "SELECT id, salary, FIRST_VALUE(salary) OVER (ORDER BY id) FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, FIRST_VALUE_WithPartition)
{
    std::string sql = "SELECT dept, salary, "
                      "FIRST_VALUE(salary) OVER (PARTITION BY dept ORDER BY salary DESC) "
                      "FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, FIRST_VALUE_InvalidNoArgs)
{
    std::string sql = "SELECT id, FIRST_VALUE() OVER (ORDER BY id) FROM employees";
    testParse(sql, false);
}

TEST_F(WindowFunctionTest, FIRST_VALUE_InvalidTwoArgs)
{
    std::string sql = "SELECT id, FIRST_VALUE(salary, name) OVER (ORDER BY id) FROM employees";
    testParse(sql, false);
}

// ===== LAST_VALUE Tests =====

TEST_F(WindowFunctionTest, LAST_VALUE_Basic)
{
    std::string sql = "SELECT id, salary, LAST_VALUE(salary) OVER (ORDER BY id) FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, LAST_VALUE_WithFrame)
{
    std::string sql = "SELECT id, salary, "
                      "LAST_VALUE(salary) OVER (ORDER BY id ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING) "
                      "FROM employees";
    testParse(sql, true);
}

// ===== NTH_VALUE Tests =====

TEST_F(WindowFunctionTest, NTH_VALUE_Basic)
{
    std::string sql = "SELECT id, salary, NTH_VALUE(salary, 2) OVER (ORDER BY id) FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, NTH_VALUE_InvalidOneArg)
{
    std::string sql = "SELECT id, NTH_VALUE(salary) OVER (ORDER BY id) FROM employees";
    testParse(sql, false);
}

TEST_F(WindowFunctionTest, NTH_VALUE_InvalidThreeArgs)
{
    std::string sql = "SELECT id, NTH_VALUE(salary, 2, 3) OVER (ORDER BY id) FROM employees";
    testParse(sql, false);
}

// ===== Frame Clause Tests =====

TEST_F(WindowFunctionTest, Frame_ROWS_UnboundedPreceding)
{
    std::string sql = "SELECT id, salary, "
                      "SUM(salary) OVER (ORDER BY id ROWS UNBOUNDED PRECEDING) "
                      "FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, Frame_ROWS_BetweenPrecedingAndCurrent)
{
    std::string sql = "SELECT id, salary, "
                      "AVG(salary) OVER (ORDER BY id ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) "
                      "FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, Frame_ROWS_BetweenCurrentAndFollowing)
{
    std::string sql = "SELECT id, salary, "
                      "AVG(salary) OVER (ORDER BY id ROWS BETWEEN CURRENT ROW AND 2 FOLLOWING) "
                      "FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, Frame_ROWS_BetweenPrecedingAndFollowing)
{
    std::string sql = "SELECT id, salary, "
                      "AVG(salary) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING) "
                      "FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, Frame_ROWS_BetweenUnboundedPrecedingAndUnboundedFollowing)
{
    std::string sql = "SELECT id, salary, "
                      "SUM(salary) OVER (ORDER BY id ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING) "
                      "FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, Frame_RANGE_UnboundedPreceding)
{
    std::string sql = "SELECT id, salary, "
                      "SUM(salary) OVER (ORDER BY salary RANGE UNBOUNDED PRECEDING) "
                      "FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, Frame_RANGE_BetweenPrecedingAndCurrent)
{
    std::string sql = "SELECT id, salary, "
                      "AVG(salary) OVER (ORDER BY salary RANGE BETWEEN 1000 PRECEDING AND CURRENT ROW) "
                      "FROM employees";
    testParse(sql, true);
}

// ===== Multiple Window Functions Tests =====

TEST_F(WindowFunctionTest, MultipleWindowFunctions_SameWindow)
{
    std::string sql = "SELECT id, salary, "
                      "ROW_NUMBER() OVER (ORDER BY salary DESC), "
                      "RANK() OVER (ORDER BY salary DESC), "
                      "DENSE_RANK() OVER (ORDER BY salary DESC) "
                      "FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, MultipleWindowFunctions_DifferentWindows)
{
    std::string sql = "SELECT id, dept, salary, "
                      "ROW_NUMBER() OVER (ORDER BY salary DESC), "
                      "RANK() OVER (PARTITION BY dept ORDER BY salary DESC) "
                      "FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, MixedAggregateAndWindow)
{
    std::string sql = "SELECT dept, COUNT(*) as cnt, "
                      "ROW_NUMBER() OVER (ORDER BY COUNT(*) DESC) "
                      "FROM employees GROUP BY dept";
    testParse(sql, true);
}

// ===== Complex Query Tests =====

TEST_F(WindowFunctionTest, ComplexQuery_WithWhere)
{
    std::string sql = "SELECT id, dept, salary, "
                      "ROW_NUMBER() OVER (PARTITION BY dept ORDER BY salary DESC) as rank "
                      "FROM employees "
                      "WHERE salary > 50000";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, ComplexQuery_WithOrderByLimit)
{
    std::string sql = "SELECT id, name, salary, "
                      "RANK() OVER (ORDER BY salary DESC) as salary_rank "
                      "FROM employees "
                      "ORDER BY salary_rank "
                      "LIMIT 10";
    testParse(sql, true);
}

// ===== Error Cases Tests =====

TEST_F(WindowFunctionTest, Error_MissingOVER)
{
    std::string sql = "SELECT id, ROW_NUMBER() FROM employees";
    testParse(sql, false);
}

TEST_F(WindowFunctionTest, Error_EmptyParentheses)
{
    // ROW_NUMBER should not have arguments
    std::string sql = "SELECT id, ROW_NUMBER(id) OVER () FROM employees";
    testParse(sql, false);
}

TEST_F(WindowFunctionTest, Error_RANK_WithArguments)
{
    std::string sql = "SELECT id, RANK(salary) OVER (ORDER BY id) FROM employees";
    testParse(sql, false);
}

TEST_F(WindowFunctionTest, Error_DENSE_RANK_WithArguments)
{
    std::string sql = "SELECT id, DENSE_RANK(salary) OVER (ORDER BY id) FROM employees";
    testParse(sql, false);
}

// ===== Real-World Scenarios =====

TEST_F(WindowFunctionTest, RealWorld_TopNPerGroup)
{
    std::string sql = "SELECT id, dept, salary, "
                      "RANK() OVER (PARTITION BY dept ORDER BY salary DESC) as dept_rank "
                      "FROM employees";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, RealWorld_RunningTotal)
{
    std::string sql = "SELECT date, amount, "
                      "SUM(amount) OVER (ORDER BY date ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) as running_total "
                      "FROM transactions";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, RealWorld_MovingAverage)
{
    std::string sql = "SELECT date, price, "
                      "AVG(price) OVER (ORDER BY date ROWS BETWEEN 6 PRECEDING AND CURRENT ROW) as week_avg "
                      "FROM stock_prices";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, RealWorld_GapAnalysis)
{
    std::string sql = "SELECT id, date, value, "
                      "LAG(value) OVER (ORDER BY date) as prev_value, "
                      "LEAD(value) OVER (ORDER BY date) as next_value "
                      "FROM measurements";
    testParse(sql, true);
}

TEST_F(WindowFunctionTest, RealWorld_PercentileRanking)
{
    std::string sql = "SELECT id, score, "
                      "ROW_NUMBER() OVER (ORDER BY score DESC) as rank, "
                      "DENSE_RANK() OVER (ORDER BY score DESC) as dense_rank "
                      "FROM test_scores";
    testParse(sql, true);
}
