#include <gtest/gtest.h>

#include "scratchbird/optimizer/mv_rewriter.h"
#include "scratchbird/optimizer/cost_model.h"

using namespace scratchbird;

namespace {

optimizer::QueryPattern makePattern(const std::vector<std::string>& tables,
                                    const std::vector<std::string>& selects,
                                    bool is_aggregate,
                                    const std::vector<std::string>& group_by = {},
                                    const std::vector<std::string>& aggregates = {})
{
    optimizer::QueryPattern pattern;
    pattern.table_names = tables;
    pattern.select_columns = selects;
    pattern.is_aggregate = is_aggregate;
    pattern.group_by_columns = group_by;
    pattern.aggregates = aggregates;
    pattern.computeHash();
    return pattern;
}

}  // namespace

TEST(MVRewriterTest, SubsumptionRequiresAllTablesAndColumns)
{
    optimizer::CostModel cost_model;
    optimizer::MVRewriter rewriter(nullptr, cost_model, nullptr);

    auto mv_pattern = makePattern({"t1", "t2"}, {"t1.id", "t2.name"}, false);
    auto query_pattern = makePattern({"t1"}, {"t1.id"}, false);

    EXPECT_TRUE(rewriter.checkSubsumption(mv_pattern, query_pattern));

    auto missing_table = makePattern({"t2"}, {"t1.id"}, false);
    EXPECT_FALSE(rewriter.checkSubsumption(missing_table, query_pattern));

    auto missing_column = makePattern({"t1"}, {"t1.id"}, false);
    auto query_needs_more = makePattern({"t1"}, {"t1.id", "t1.extra"}, false);
    EXPECT_FALSE(rewriter.checkSubsumption(missing_column, query_needs_more));
}

TEST(MVRewriterTest, SubsumptionRequiresAggregateAndGroupByMatch)
{
    optimizer::CostModel cost_model;
    optimizer::MVRewriter rewriter(nullptr, cost_model, nullptr);

    auto mv_pattern = makePattern({"sales"}, {"region", "SUM(amount)"}, true,
                                  {"region"}, {"SUM"});
    auto query_pattern = makePattern({"sales"}, {"region", "SUM(amount)"}, true,
                                     {"region"}, {"SUM"});

    EXPECT_TRUE(rewriter.checkSubsumption(mv_pattern, query_pattern));

    auto query_group_mismatch = makePattern({"sales"}, {"region", "SUM(amount)"}, true,
                                            {"region", "month"}, {"SUM"});
    EXPECT_FALSE(rewriter.checkSubsumption(mv_pattern, query_group_mismatch));

    auto mv_not_aggregate = makePattern({"sales"}, {"region", "amount"}, false);
    EXPECT_FALSE(rewriter.checkSubsumption(mv_not_aggregate, query_pattern));
}

TEST(QueryPatternTest, HashIgnoresSelectColumnOrder)
{
    auto pattern_a = makePattern({"t1"}, {"b", "a", "c"}, false);
    auto pattern_b = makePattern({"t1"}, {"c", "b", "a"}, false);

    EXPECT_EQ(pattern_a.pattern_hash, pattern_b.pattern_hash);
}
