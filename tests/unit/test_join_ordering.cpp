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

    auto containsLeafPathIdentity(const std::shared_ptr<Path> &root,
                                  const std::shared_ptr<Path> &needle) -> bool
    {
        if (!root || !needle)
        {
            return false;
        }
        if (root == needle)
        {
            return true;
        }
        if (auto nested = std::dynamic_pointer_cast<NestedLoopJoinPath>(root))
        {
            return containsLeafPathIdentity(nested->outerPath(), needle) ||
                   containsLeafPathIdentity(nested->innerPath(), needle);
        }
        if (auto hash = std::dynamic_pointer_cast<HashJoinPath>(root))
        {
            return containsLeafPathIdentity(hash->outerPath(), needle) ||
                   containsLeafPathIdentity(hash->innerPath(), needle);
        }
        if (auto merge = std::dynamic_pointer_cast<MergeJoinPath>(root))
        {
            return containsLeafPathIdentity(merge->outerPath(), needle) ||
                   containsLeafPathIdentity(merge->innerPath(), needle);
        }
        return false;
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

TEST(JoinOrderingOptimizerTest,
     CandidateBundlesCanPickNonPrimaryBasePathForJoinSearch)
{
    CostModel cost_model;
    SelectivityEstimator selectivity_estimator(nullptr, nullptr);
    JoinOrderingOptimizer optimizer(cost_model, selectivity_estimator);

    auto broad_scan = makeSeqScanPath("users", 5000, 5.0);
    auto selective_ordered = makeSeqScanPath("users", 5, 15.0);
    AccessPathDescriptor ordered_descriptor = selective_ordered->accessDescriptor();
    ordered_descriptor.family = "BTREE_ORDERED_SCAN";
    ordered_descriptor.path_name = "BTREE_ORDERED_SCAN";
    ordered_descriptor.ordered_output = true;
    ordered_descriptor.ordered_prefix_length = 1;
    ordered_descriptor.interesting_order_score = 1.0;
    selective_ordered->setAccessDescriptor(std::move(ordered_descriptor));

    optimizer.addRelation(core::ID{},
                          "users",
                          "users",
                          std::vector<std::shared_ptr<Path>>{
                              broad_scan,
                              selective_ordered});
    optimizer.addRelation(core::ID{},
                          "orders",
                          "orders",
                          makeSeqScanPath("orders", 10000, 100.0));
    optimizer.addJoinEdge(0, 1, parser::JoinType::INNER, nullptr);
    optimizer.setJoinSelectivity(0, 0.0001);

    auto plan = optimizer.optimize();
    ASSERT_NE(plan, nullptr);
    EXPECT_TRUE(containsLeafPathIdentity(plan, selective_ordered));
    EXPECT_FALSE(containsLeafPathIdentity(plan, broad_scan));
}

TEST(JoinOrderingPropertySignatureTest,
     SignatureDistinguishesOrderingCoverageAndQueryability)
{
    AccessPathDescriptor descriptor;
    descriptor.family = "BTREE_ORDERED_SCAN";
    descriptor.path_name = "BTREE_ORDERED_SCAN";
    descriptor.family_kind = PlannerAccessFamily::BTREE_ORDERED_SCAN;
    descriptor.exactness_class = AccessPathExactnessClass::EXACT_KEY;
    descriptor.visibility_enforcement = AccessPathVisibilityEnforcement::HYBRID;
    descriptor.queryability_state = AccessPathQueryabilityState::QUERYABLE;
    descriptor.ordered_output = true;
    descriptor.ordered_prefix_length = 1;
    descriptor.order_complete = true;
    descriptor.coverage_fraction = 0.50;
    descriptor.interesting_order_score = 0.80;
    descriptor.ordering_keys.push_back(
        {"users.last_name", false, false, "STRING"});

    auto alternate_order = descriptor;
    alternate_order.ordering_keys[0].expression_text = "users.created_at";

    auto higher_coverage = descriptor;
    higher_coverage.coverage_fraction = 1.0;

    auto limited = descriptor;
    limited.queryability_state = AccessPathQueryabilityState::LIMITED;

    EXPECT_NE(joinSearchPropertySignature(descriptor),
              joinSearchPropertySignature(alternate_order));
    EXPECT_NE(joinSearchPropertySignature(descriptor),
              joinSearchPropertySignature(higher_coverage));
    EXPECT_NE(joinSearchPropertySignature(descriptor),
              joinSearchPropertySignature(limited));
}

TEST(JoinOrderingOptimizerTest,
     GreedySearchRetainsFrontierStatesToChooseNonPrimaryBasePath)
{
    CostModel cost_model;
    SelectivityEstimator selectivity_estimator(nullptr, nullptr);
    JoinOrderingOptimizer optimizer(cost_model, selectivity_estimator);

    JoinPlanningControls controls;
    controls.strategy = JoinSearchStrategy::HEURISTIC_GREEDY;
    optimizer.setPlanningControls(controls);

    auto broad_scan = makeSeqScanPath("users", 5000, 5.0);
    auto selective_ordered = makeSeqScanPath("users", 5, 15.0);
    AccessPathDescriptor ordered_descriptor = selective_ordered->accessDescriptor();
    ordered_descriptor.family = "BTREE_ORDERED_SCAN";
    ordered_descriptor.path_name = "BTREE_ORDERED_SCAN";
    ordered_descriptor.family_kind = PlannerAccessFamily::BTREE_ORDERED_SCAN;
    ordered_descriptor.ordered_output = true;
    ordered_descriptor.ordered_prefix_length = 1;
    ordered_descriptor.interesting_order_score = 1.0;
    selective_ordered->setAccessDescriptor(std::move(ordered_descriptor));

    optimizer.addRelation(core::ID{},
                          "users",
                          "users",
                          std::vector<std::shared_ptr<Path>>{
                              broad_scan,
                              selective_ordered});
    optimizer.addRelation(core::ID{},
                          "orders",
                          "orders",
                          makeSeqScanPath("orders", 10000, 100.0));
    optimizer.addJoinEdge(0, 1, parser::JoinType::INNER, nullptr);
    optimizer.setJoinSelectivity(0, 0.0001);

    auto plan = optimizer.optimize();
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(optimizer.lastStrategyUsed(), JoinSearchStrategy::HEURISTIC_GREEDY);
    EXPECT_TRUE(containsLeafPathIdentity(plan, selective_ordered));
    EXPECT_FALSE(containsLeafPathIdentity(plan, broad_scan));
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
    controls.max_bounded_dp_relations = 2;
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

TEST(JoinOrderingOptimizerTest, AutoUsesBoundedDpBetweenExhaustiveAndHeuristicThresholds)
{
    CostModel cost_model;
    SelectivityEstimator selectivity_estimator(nullptr, nullptr);
    JoinOrderingOptimizer optimizer(cost_model, selectivity_estimator);

    JoinPlanningControls controls;
    controls.strategy = JoinSearchStrategy::AUTO;
    controls.max_exhaustive_relations = 2;
    controls.max_bounded_dp_relations = 3;
    controls.max_pair_evaluations = 32;
    controls.max_states_considered = 32;
    optimizer.setPlanningControls(controls);

    optimizer.addRelation(core::ID{}, "r1", "r1", makeSeqScanPath("r1", 100, 10.0));
    optimizer.addRelation(core::ID{}, "r2", "r2", makeSeqScanPath("r2", 50, 5.0));
    optimizer.addRelation(core::ID{}, "r3", "r3", makeSeqScanPath("r3", 25, 2.5));
    optimizer.addJoinEdge(0, 1, parser::JoinType::INNER, nullptr);
    optimizer.addJoinEdge(1, 2, parser::JoinType::INNER, nullptr);

    auto plan = optimizer.optimize();
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(optimizer.lastStrategyUsed(), JoinSearchStrategy::BOUNDED_DP);
    EXPECT_TRUE(optimizer.lastTelemetry().fallback_reason.empty());
}

TEST(JoinOrderingOptimizerTest, BoundedDpBudgetFallbackPublishesThreshold)
{
    CostModel cost_model;
    SelectivityEstimator selectivity_estimator(nullptr, nullptr);
    JoinOrderingOptimizer optimizer(cost_model, selectivity_estimator);

    JoinPlanningControls controls;
    controls.strategy = JoinSearchStrategy::BOUNDED_DP;
    controls.max_exhaustive_relations = 2;
    controls.max_bounded_dp_relations = 4;
    controls.max_states_considered = 1;
    controls.max_pair_evaluations = 8;
    controls.fallback_prune_level = 2;
    optimizer.setPlanningControls(controls);

    optimizer.addRelation(core::ID{}, "r1", "r1", makeSeqScanPath("r1", 100, 10.0));
    optimizer.addRelation(core::ID{}, "r2", "r2", makeSeqScanPath("r2", 50, 5.0));
    optimizer.addRelation(core::ID{}, "r3", "r3", makeSeqScanPath("r3", 25, 2.5));
    optimizer.addJoinEdge(0, 1, parser::JoinType::INNER, nullptr);
    optimizer.addJoinEdge(1, 2, parser::JoinType::INNER, nullptr);

    auto plan = optimizer.optimize();
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(optimizer.lastStrategyUsed(), JoinSearchStrategy::HEURISTIC_GREEDY);
    EXPECT_EQ(optimizer.lastTelemetry().fallback_reason, "MAX_STATES_CONSIDERED");
    EXPECT_EQ(optimizer.lastTelemetry().fallback_threshold_name,
              "MAX_STATES_CONSIDERED");
    EXPECT_EQ(optimizer.lastTelemetry().fallback_threshold_value, 1u);
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

TEST(JoinOrderingOptimizerTest, NonReorderableInnerBarrierForcesInputOrderPath)
{
    CostModel cost_model;
    SelectivityEstimator selectivity_estimator(nullptr, nullptr);
    JoinOrderingOptimizer optimizer(cost_model, selectivity_estimator);

    optimizer.addRelation(core::ID{}, "r1", "r1", makeSeqScanPath("r1", 500, 50.0));
    optimizer.addRelation(core::ID{}, "r2", "r2", makeSeqScanPath("r2", 5, 1.0));
    optimizer.addRelation(core::ID{}, "r3", "r3", makeSeqScanPath("r3", 10, 2.0));
    optimizer.addJoinEdge(0,
                          1,
                          parser::JoinType::INNER,
                          nullptr,
                          0,
                          JoinLegalityClass::USING_BARRIER,
                          false,
                          true);
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

TEST(JoinMethodLegalityTest, HashAndMergeRequireResolvedJoinKeyMetadata)
{
    auto descriptor = classifyJoinLegality(parser::JoinType::INNER, false, false);

    const auto hash_legality = evaluateHashJoinLegality(descriptor,
                                                        parser::JoinType::INNER,
                                                        true,
                                                        false,
                                                        false);
    EXPECT_FALSE(hash_legality.legal);
    EXPECT_EQ(hash_legality.reject_code,
              JoinMethodRejectCode::MISSING_KEY_METADATA);

    const auto merge_legality = evaluateMergeJoinLegality(descriptor,
                                                          parser::JoinType::INNER,
                                                          true,
                                                          false,
                                                          false,
                                                          false,
                                                          false);
    EXPECT_FALSE(merge_legality.legal);
    EXPECT_EQ(merge_legality.reject_code,
              JoinMethodRejectCode::MISSING_KEY_METADATA);
}

TEST(JoinMethodLegalityTest, MergeJoinCanBeEnabledByExplicitSorts)
{
    auto descriptor = classifyJoinLegality(parser::JoinType::INNER, false, false);

    const auto merge_legality = evaluateMergeJoinLegality(descriptor,
                                                          parser::JoinType::INNER,
                                                          true,
                                                          true,
                                                          false,
                                                          false,
                                                          false);
    EXPECT_TRUE(merge_legality.legal);
    EXPECT_TRUE(merge_legality.requires_sort_outer);
    EXPECT_TRUE(merge_legality.requires_sort_inner);
}

TEST(JoinMethodLegalityTest, ParameterizedInnerPublishesDedicatedNestedLoopFamily)
{
    auto descriptor = classifyJoinLegality(parser::JoinType::LEFT, false, false);
    descriptor.lateral_dependency = true;

    const auto nested_legality =
        evaluateNestedLoopLegality(descriptor, true);
    EXPECT_TRUE(nested_legality.legal);
    EXPECT_EQ(nested_legality.family,
              JoinMethodFamily::PARAMETERIZED_NESTED_LOOP);

    const auto hash_legality = evaluateHashJoinLegality(descriptor,
                                                        parser::JoinType::LEFT,
                                                        true,
                                                        true,
                                                        true);
    EXPECT_FALSE(hash_legality.legal);
    EXPECT_EQ(hash_legality.reject_code,
              JoinMethodRejectCode::PARAMETERIZED_INNER_REQUIRES_NESTED_LOOP);
}
