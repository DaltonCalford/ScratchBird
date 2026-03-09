#include <gtest/gtest.h>

#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/optimizer/join_legality.h"
#include "scratchbird/optimizer/join_ordering.h"
#include "scratchbird/optimizer/path.h"
#include "scratchbird/optimizer/selectivity_estimator.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace scratchbird;
using namespace scratchbird::optimizer;

namespace
{
    auto makeSeqScanPath(const std::string &table_name,
                         uint64_t rows,
                         double total_cost) -> std::shared_ptr<Path>
    {
        CostEstimate cost;
        cost.startup_cost = 0.0;
        cost.run_cost = total_cost;
        cost.total_cost = total_cost;
        cost.rows = rows;
        return std::make_shared<SeqScanPath>(core::ID{},
                                             table_name,
                                             std::max<uint64_t>(1, rows / 32),
                                             rows,
                                             0.0,
                                             cost);
    }

    auto containsCrossJoin(const std::shared_ptr<Path> &path) -> bool
    {
        if (!path)
        {
            return false;
        }
        if (auto nested = std::dynamic_pointer_cast<NestedLoopJoinPath>(path))
        {
            return nested->joinType() == parser::JoinType::CROSS ||
                   containsCrossJoin(nested->outerPath()) ||
                   containsCrossJoin(nested->innerPath());
        }
        if (auto hash = std::dynamic_pointer_cast<HashJoinPath>(path))
        {
            return hash->joinType() == parser::JoinType::CROSS ||
                   containsCrossJoin(hash->outerPath()) ||
                   containsCrossJoin(hash->innerPath());
        }
        if (auto merge = std::dynamic_pointer_cast<MergeJoinPath>(path))
        {
            return merge->joinType() == parser::JoinType::CROSS ||
                   containsCrossJoin(merge->outerPath()) ||
                   containsCrossJoin(merge->innerPath());
        }
        return false;
    }

    auto isJoinPath(const std::shared_ptr<Path> &path) -> bool
    {
        return std::dynamic_pointer_cast<NestedLoopJoinPath>(path) != nullptr ||
               std::dynamic_pointer_cast<HashJoinPath>(path) != nullptr ||
               std::dynamic_pointer_cast<MergeJoinPath>(path) != nullptr;
    }

    auto isBushyTopJoin(const std::shared_ptr<Path> &path) -> bool
    {
        if (auto nested = std::dynamic_pointer_cast<NestedLoopJoinPath>(path))
        {
            return isJoinPath(nested->outerPath()) && isJoinPath(nested->innerPath());
        }
        if (auto hash = std::dynamic_pointer_cast<HashJoinPath>(path))
        {
            return isJoinPath(hash->outerPath()) && isJoinPath(hash->innerPath());
        }
        if (auto merge = std::dynamic_pointer_cast<MergeJoinPath>(path))
        {
            return isJoinPath(merge->outerPath()) && isJoinPath(merge->innerPath());
        }
        return false;
    }

    auto leafTableNames(const std::shared_ptr<Path> &path) -> std::vector<std::string>
    {
        if (!path)
        {
            return {};
        }
        if (auto seq = std::dynamic_pointer_cast<SeqScanPath>(path))
        {
            return {seq->tableName()};
        }
        if (auto nested = std::dynamic_pointer_cast<NestedLoopJoinPath>(path))
        {
            auto names = leafTableNames(nested->outerPath());
            auto inner = leafTableNames(nested->innerPath());
            names.insert(names.end(), inner.begin(), inner.end());
            return names;
        }
        if (auto hash = std::dynamic_pointer_cast<HashJoinPath>(path))
        {
            auto names = leafTableNames(hash->outerPath());
            auto inner = leafTableNames(hash->innerPath());
            names.insert(names.end(), inner.begin(), inner.end());
            return names;
        }
        if (auto merge = std::dynamic_pointer_cast<MergeJoinPath>(path))
        {
            auto names = leafTableNames(merge->outerPath());
            auto inner = leafTableNames(merge->innerPath());
            names.insert(names.end(), inner.begin(), inner.end());
            return names;
        }
        return {};
    }

    auto leftmostLeafTable(const std::shared_ptr<Path> &path) -> std::string
    {
        const auto names = leafTableNames(path);
        return names.empty() ? std::string() : names.front();
    }
} // namespace

TEST(JoinOrderingOptimizerTest, DynamicProgrammingPlansDisconnectedGraphWithCrossJoin)
{
    CostModel cost_model;
    SelectivityEstimator selectivity_estimator(nullptr, nullptr);
    JoinOrderingOptimizer optimizer(cost_model, selectivity_estimator);

    optimizer.addRelation(core::ID{}, "users", "users", makeSeqScanPath("users", 100, 12.0));
    optimizer.addRelation(core::ID{}, "products", "products", makeSeqScanPath("products", 10, 2.0));
    optimizer.addRelation(core::ID{}, "audit", "audit", makeSeqScanPath("audit", 5, 1.0));
    optimizer.addJoinEdge(0, 1, parser::JoinType::INNER, nullptr);
    optimizer.setJoinSelectivity(0, 0.1);

    auto plan = optimizer.optimize();
    ASSERT_NE(plan, nullptr);
    EXPECT_TRUE(containsCrossJoin(plan));
}

TEST(JoinOrderingOptimizerTest, GreedyFallbackMarksDisconnectedJoinAsCrossJoin)
{
    CostModel cost_model;
    SelectivityEstimator selectivity_estimator(nullptr, nullptr);
    JoinOrderingOptimizer optimizer(cost_model, selectivity_estimator);

    optimizer.addRelation(core::ID{},
                          "left_rel",
                          "left_rel",
                          makeSeqScanPath("left_rel", 50, 5.0));
    optimizer.addRelation(core::ID{},
                          "right_rel",
                          "right_rel",
                          makeSeqScanPath("right_rel", 20, 2.0));

    auto plan = optimizer.optimizeGreedy();
    ASSERT_NE(plan, nullptr);

    auto top_join = std::dynamic_pointer_cast<NestedLoopJoinPath>(plan);
    ASSERT_NE(top_join, nullptr);
    EXPECT_EQ(top_join->joinType(), parser::JoinType::CROSS);
}

TEST(JoinOrderingOptimizerTest, LeftOuterJoinKeepsPreservedLeftRelationAsOuterOperand)
{
    CostModel cost_model;
    SelectivityEstimator selectivity_estimator(nullptr, nullptr);
    JoinOrderingOptimizer optimizer(cost_model, selectivity_estimator);

    optimizer.addRelation(core::ID{}, "users", "users", makeSeqScanPath("users", 2000, 200.0));
    optimizer.addRelation(core::ID{}, "products", "products", makeSeqScanPath("products", 5, 1.0));
    optimizer.addJoinEdge(0, 1, parser::JoinType::LEFT, nullptr);
    optimizer.setJoinSelectivity(0, 0.1);

    auto plan = optimizer.optimize();
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(leftmostLeafTable(plan), "users");
}

TEST(JoinOrderingOptimizerTest, HypergraphGreedyCanBuildBushyJoinTree)
{
    CostModel cost_model;
    SelectivityEstimator selectivity_estimator(nullptr, nullptr);
    JoinOrderingOptimizer optimizer(cost_model, selectivity_estimator);

    JoinPlanningControls controls;
    controls.strategy = JoinSearchStrategy::HYPERGRAPH_GREEDY;
    controls.max_pair_evaluations = 32;
    optimizer.setPlanningControls(controls);

    optimizer.addRelation(core::ID{}, "a", "a", makeSeqScanPath("a", 10, 1.0));
    optimizer.addRelation(core::ID{}, "b", "b", makeSeqScanPath("b", 10, 1.0));
    optimizer.addRelation(core::ID{}, "c", "c", makeSeqScanPath("c", 10, 1.0));
    optimizer.addRelation(core::ID{}, "d", "d", makeSeqScanPath("d", 10, 1.0));

    optimizer.addJoinEdge(0, 1, parser::JoinType::INNER, nullptr);
    optimizer.setJoinSelectivity(0, 0.01);
    optimizer.addJoinEdge(2, 3, parser::JoinType::INNER, nullptr);
    optimizer.setJoinSelectivity(1, 0.01);
    optimizer.addJoinEdge(1, 2, parser::JoinType::INNER, nullptr);
    optimizer.setJoinSelectivity(2, 0.5);

    auto plan = optimizer.optimize();
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(optimizer.lastStrategyUsed(), JoinSearchStrategy::HYPERGRAPH_GREEDY);
    EXPECT_TRUE(isBushyTopJoin(plan));
}

TEST(JoinOrderingOptimizerTest, ExhaustiveCapCanForceHeuristicSearch)
{
    CostModel cost_model;
    SelectivityEstimator selectivity_estimator(nullptr, nullptr);
    JoinOrderingOptimizer optimizer(cost_model, selectivity_estimator);

    JoinPlanningControls controls;
    controls.strategy = JoinSearchStrategy::AUTO;
    controls.max_exhaustive_relations = 2;
    controls.max_pair_evaluations = 16;
    optimizer.setPlanningControls(controls);

    optimizer.addRelation(core::ID{}, "r1", "r1", makeSeqScanPath("r1", 100, 10.0));
    optimizer.addRelation(core::ID{}, "r2", "r2", makeSeqScanPath("r2", 50, 5.0));
    optimizer.addRelation(core::ID{}, "r3", "r3", makeSeqScanPath("r3", 25, 2.5));
    optimizer.addJoinEdge(0, 1, parser::JoinType::INNER, nullptr);
    optimizer.addJoinEdge(1, 2, parser::JoinType::INNER, nullptr);

    auto plan = optimizer.optimize();
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(optimizer.lastStrategyUsed(), JoinSearchStrategy::HYPERGRAPH_GREEDY);
}

TEST(JoinOrderingOptimizerTest, InputOrderControlOverridesReordering)
{
    CostModel cost_model;
    SelectivityEstimator selectivity_estimator(nullptr, nullptr);
    JoinOrderingOptimizer optimizer(cost_model, selectivity_estimator);

    JoinPlanningControls controls;
    controls.strategy = JoinSearchStrategy::INPUT_ORDER;
    optimizer.setPlanningControls(controls);

    optimizer.addRelation(core::ID{}, "r1", "r1", makeSeqScanPath("r1", 500, 50.0));
    optimizer.addRelation(core::ID{}, "r2", "r2", makeSeqScanPath("r2", 5, 1.0));
    optimizer.addRelation(core::ID{}, "r3", "r3", makeSeqScanPath("r3", 10, 2.0));
    optimizer.addJoinEdge(0, 1, parser::JoinType::INNER, nullptr);
    optimizer.addJoinEdge(1, 2, parser::JoinType::INNER, nullptr);

    auto plan = optimizer.optimize();
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(optimizer.lastStrategyUsed(), JoinSearchStrategy::INPUT_ORDER);
    EXPECT_EQ(leftmostLeafTable(plan), "r1");
}

TEST(JoinLegalityClassificationTest, SemiAndAntiDescriptorsFailClosedForReordering)
{
    const auto semi = makeSemiJoinLegality();
    EXPECT_EQ(semi.legality_class, JoinLegalityClass::SEMI_BARRIER);
    EXPECT_FALSE(semi.reorderable);
    EXPECT_TRUE(semi.requires_original_order);

    const auto anti = makeAntiJoinLegality();
    EXPECT_EQ(anti.legality_class, JoinLegalityClass::ANTI_BARRIER);
    EXPECT_FALSE(anti.reorderable);
    EXPECT_TRUE(anti.requires_original_order);
}
