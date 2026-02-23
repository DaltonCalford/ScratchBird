#include <gtest/gtest.h>

#include "scratchbird/optimizer/join_ordering.h"
#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/optimizer/selectivity_estimator.h"
#include "scratchbird/optimizer/path.h"

using namespace scratchbird;

namespace {

std::shared_ptr<optimizer::SeqScanPath> makeSeqScan(const core::ID& table_id,
                                                    const std::string& name,
                                                    uint64_t pages,
                                                    uint64_t rows,
                                                    optimizer::CostModel& cost_model)
{
    auto cost = cost_model.costSeqScan(pages, rows, 0.0, nullptr);
    return std::make_shared<optimizer::SeqScanPath>(table_id, name, pages, rows, 0.0, cost);
}

core::ID makeId(uint8_t b0)
{
    core::ID id{};
    id.bytes[0] = b0;
    return id;
}

}  // namespace

TEST(JoinOrderingOptimizerTest, SingleRelationReturnsBestPath)
{
    optimizer::CostModel cost_model;
    optimizer::SelectivityEstimator selectivity_estimator(nullptr);
    optimizer::JoinOrderingOptimizer optimizer(cost_model, selectivity_estimator);

    auto path = makeSeqScan(makeId(1), "t1", 10, 100, cost_model);
    optimizer.addRelation(makeId(1), "t1", "t1", path);

    auto result = optimizer.optimize();
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result.get(), path.get());
}

TEST(JoinOrderingOptimizerTest, ChoosesHashJoinForSelectiveLargeJoin)
{
    optimizer::CostModel cost_model;
    optimizer::SelectivityEstimator selectivity_estimator(nullptr);
    optimizer::JoinOrderingOptimizer optimizer(cost_model, selectivity_estimator);

    auto path_a = makeSeqScan(makeId(1), "a", 100, 10000, cost_model);
    auto path_b = makeSeqScan(makeId(2), "b", 100, 12000, cost_model);

    size_t a_idx = optimizer.addRelation(makeId(1), "a", "a", path_a);
    size_t b_idx = optimizer.addRelation(makeId(2), "b", "b", path_b);

    optimizer.addJoinEdge(a_idx, b_idx, parser::JoinType::INNER, nullptr);
    optimizer.setJoinSelectivity(0, 0.01);

    auto result = optimizer.optimize();
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type(), optimizer::PathType::HASH_JOIN);
}

