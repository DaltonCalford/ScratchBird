#include <gtest/gtest.h>

#include "scratchbird/optimizer/cost_model.h"

using scratchbird::optimizer::CostEstimate;
using scratchbird::optimizer::CostModel;
using scratchbird::optimizer::CostParameters;

namespace
{
    auto defaultModel() -> CostModel
    {
        CostParameters params;
        params.seq_page_cost = 1.0;
        params.random_page_cost = 4.0;
        params.cpu_tuple_cost = 0.01;
        params.cpu_index_tuple_cost = 0.005;
        params.cpu_operator_cost = 0.0025;
        params.effective_cache_size = 128.0;
        return CostModel(params);
    }
} // namespace

TEST(CostModelTest, AccessMethodCostsProduceDeterministicOutputs)
{
    auto model = defaultModel();

    const auto seq = model.costSeqScan(32, 3200, model.operatorCost("="));
    const auto index =
        model.costIndexScan(3, 4, 80, 8, 80, model.operatorCost("="), 0.9);
    const auto index_only =
        model.costIndexOnlyScan(3, 4, 80, model.operatorCost("="), 0.9);
    const auto bitmap =
        model.costBitmapScan(2, 6, 4, 80, model.operatorCost("="), "AND");
    const auto lsm = model.costLSMScan(3, 2, 80, 8, 80, model.operatorCost("="), 0.1);
    const auto aggregate = model.costAggregate(3200, 16, 2);
    const auto sort = model.costSort(3200, 64, 2);
    const auto limit = model.costLimit(3200, 25, 10);
    const auto window = model.costWindow(3200, 64, 1, 1, 2);

    EXPECT_EQ(seq.rows, 3200u);
    EXPECT_EQ(index.rows, 80u);
    EXPECT_EQ(index_only.rows, 80u);
    EXPECT_EQ(bitmap.rows, 80u);
    EXPECT_EQ(lsm.rows, 80u);
    EXPECT_EQ(aggregate.rows, 16u);
    EXPECT_EQ(sort.rows, 3200u);
    EXPECT_EQ(limit.rows, 25u);
    EXPECT_EQ(window.rows, 3200u);

    EXPECT_GT(seq.total_cost, 0.0);
    EXPECT_GT(index.total_cost, seq.startup_cost);
    EXPECT_LT(index_only.total_cost, index.total_cost);
    EXPECT_GT(bitmap.total_cost, 0.0);
    EXPECT_GT(lsm.total_cost, 0.0);
    EXPECT_GT(aggregate.total_cost, 0.0);
    EXPECT_GT(sort.total_cost, aggregate.total_cost);
    EXPECT_GT(limit.total_cost, 0.0);
    EXPECT_GT(window.total_cost, 0.0);
}

TEST(CostModelTest, EffectiveRandomPageCostUsesCacheModel)
{
    CostParameters params;
    params.seq_page_cost = 1.0;
    params.random_page_cost = 4.0;
    params.effective_cache_size = 100.0;
    CostModel model(params);

    EXPECT_DOUBLE_EQ(model.effectiveRandomPageCost(0), 4.0);
    EXPECT_DOUBLE_EQ(model.effectiveRandomPageCost(100), 1.0);
    EXPECT_NEAR(model.effectiveRandomPageCost(200), 2.5, 1e-9);
}

TEST(CostModelTest, HashJoinComparisonBeatsNestedLoopForLargeEquiJoin)
{
    auto model = defaultModel();

    const auto outer = model.costSeqScan(128, 12000, 0.0);
    const auto inner = model.costSeqScan(128, 12000, 0.0);

    const auto nested =
        model.costNestedLoopJoin(outer, inner, 12000, 12000, 0.01, scratchbird::parser::JoinType::INNER);
    const auto hash =
        model.costHashJoin(outer, inner, 12000, 12000, 0.01, scratchbird::parser::JoinType::INNER);

    EXPECT_EQ(nested.rows, hash.rows);
    EXPECT_LT(hash.total_cost, nested.total_cost);
}

TEST(CostModelTest, OuterJoinPaddingKeepsOutputRowsAtLeastInputSide)
{
    auto model = defaultModel();

    CostEstimate outer_cost(0.0, 10.0, 10);
    CostEstimate inner_cost(0.0, 10.0, 20);

    const auto left =
        model.costNestedLoopJoin(outer_cost, inner_cost, 10, 20, 0.0, scratchbird::parser::JoinType::LEFT);
    const auto right =
        model.costNestedLoopJoin(outer_cost, inner_cost, 10, 20, 0.0, scratchbird::parser::JoinType::RIGHT);
    const auto full =
        model.costHashJoin(outer_cost, inner_cost, 10, 20, 0.0, scratchbird::parser::JoinType::FULL);

    EXPECT_EQ(left.rows, 10u);
    EXPECT_EQ(right.rows, 20u);
    EXPECT_EQ(full.rows, 20u);
}
