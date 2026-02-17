#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "scratchbird/core/telemetry.h"
#include "scratchbird/optimizer/vnext_plan_selection.h"

using scratchbird::optimizer::PlanHashAttribute;
using scratchbird::optimizer::PlanHashNode;
using scratchbird::optimizer::QueryTrack;
using scratchbird::optimizer::VNextPlanCandidateInput;
using scratchbird::optimizer::VNextPlanSelection;

namespace
{
auto metricCounterValue(const std::string& metric_name,
                        const std::vector<std::string>& labels) -> double
{
    auto* metric = scratchbird::core::MetricsRegistry::getInstance().get(metric_name);
    if (metric == nullptr)
    {
        return 0.0;
    }
    auto* counter = dynamic_cast<scratchbird::core::Counter*>(metric);
    if (counter == nullptr)
    {
        return 0.0;
    }
    return counter->get(labels);
}

auto makePlanNode(const std::string &name, const std::string &label) -> PlanHashNode
{
    PlanHashNode node;
    node.node_symbol = name;
    node.attributes.push_back(PlanHashAttribute::makeString("label", label));
    node.attributes.push_back(PlanHashAttribute::makeNumeric("cost_hint", 1.5));
    return node;
}

auto makeBaseCandidate(QueryTrack track) -> VNextPlanCandidateInput
{
    VNextPlanCandidateInput candidate;
    candidate.track_symbol = track;
    candidate.base_cardinality = 1000;
    candidate.post_filter_selectivity = 0.50;
    candidate.avg_row_bytes = 100;
    candidate.operator_count = 5;
    candidate.bridge_count = 0;
    candidate.bridge_shuffle_bytes = 0;
    candidate.read_amplification_factor = 1.0;
    candidate.latency_budget_ms = 100.0;
    candidate.memory_budget_bytes = 1024ULL * 1024ULL * 1024ULL;
    candidate.page_size_bytes = 8192;
    candidate.plan_root = makePlanNode("SCAN", "base");
    return candidate;
}
} // namespace

TEST(OptimizerVNextPlanSelectionTest, ScoreCandidateUsesCanonicalFormula)
{
    VNextPlanCandidateInput candidate = makeBaseCandidate(QueryTrack::RELATIONAL_TRACK);
    candidate.base_cardinality = 1000;
    candidate.post_filter_selectivity = 0.25;
    candidate.avg_row_bytes = 100;
    candidate.operator_count = 10;
    candidate.bridge_count = 1;
    candidate.bridge_shuffle_bytes = 2ULL * 1024ULL * 1024ULL;
    candidate.read_amplification_factor = 1.2;
    candidate.latency_budget_ms = 10.0;
    candidate.memory_budget_bytes = 20000;
    candidate.page_size_bytes = 8192;
    candidate.join_reorder_candidate_count = 5;
    candidate.plan_root = makePlanNode("SCAN", "fixture");

    auto result = VNextPlanSelection::scoreCandidate(candidate);
    ASSERT_TRUE(result.ok) << result.error_code << ": " << result.error_message;
    EXPECT_EQ(250U, result.estimated_rows);
    EXPECT_EQ(10000U, result.estimated_memory_peak_bytes);
    EXPECT_NEAR(4.8, result.estimated_io_pages, 1e-12);
    EXPECT_NEAR(3.228760, result.predicted_latency_ms, 1e-12);
    EXPECT_NEAR(3.934200, result.total_cost, 1e-12);
    EXPECT_EQ(3934200, result.total_cost_uCost);
}

TEST(OptimizerVNextPlanSelectionTest, MissingRequiredInputRejectsWithOPT0302)
{
    VNextPlanCandidateInput candidate = makeBaseCandidate(QueryTrack::RELATIONAL_TRACK);
    candidate.page_size_bytes.reset();

    auto result = VNextPlanSelection::scoreCandidate(candidate);
    ASSERT_FALSE(result.ok);
    EXPECT_EQ("OPT_0302", result.error_code);
}

TEST(OptimizerVNextPlanSelectionTest, InvalidNumericRejectsWithOPT0306)
{
    VNextPlanCandidateInput candidate = makeBaseCandidate(QueryTrack::RELATIONAL_TRACK);
    candidate.read_amplification_factor = std::numeric_limits<double>::quiet_NaN();

    auto result = VNextPlanSelection::scoreCandidate(candidate);
    ASSERT_FALSE(result.ok);
    EXPECT_EQ("OPT_0306", result.error_code);
}

TEST(OptimizerVNextPlanSelectionTest, HistogramInterpolationUses128BucketContract)
{
    const auto histogram = VNextPlanSelection::makeUniformHistogram(0.0, 128.0);
    auto result =
        VNextPlanSelection::computeRangeSelectivity(histogram, 16.0, 48.0, true);
    ASSERT_TRUE(result.ok) << result.error_code << ": " << result.error_message;
    EXPECT_NEAR(0.25, result.value, 1e-12);
}

TEST(OptimizerVNextPlanSelectionTest, SameColumnIntersectionSelectivityIsDeterministic)
{
    const auto histogram = VNextPlanSelection::makeUniformHistogram(0.0, 128.0);
    auto result = VNextPlanSelection::computeSameColumnIntersectionSelectivity(
        histogram, 10.0, 90.0, 40.0, 60.0, true);
    ASSERT_TRUE(result.ok) << result.error_code << ": " << result.error_message;
    EXPECT_NEAR(20.0 / 128.0, result.value, 1e-12);
}

TEST(OptimizerVNextPlanSelectionTest, MissingRequiredHistogramRejectsWithOPT0302)
{
    auto result = VNextPlanSelection::computeRangeSelectivity(std::nullopt, 0.0, 1.0, true);
    ASSERT_FALSE(result.ok);
    EXPECT_EQ("OPT_0302", result.error_code);
}

TEST(OptimizerVNextPlanSelectionTest, PlanHashIsStableAndFormatsNumericsAtSixDecimals)
{
    PlanHashNode root;
    root.node_symbol = "ROOT";
    root.attributes.push_back(PlanHashAttribute::makeString("b", "z"));
    root.attributes.push_back(PlanHashAttribute::makeNumeric("a", 1.5));
    root.children.push_back(makePlanNode("CHILD", "x"));

    auto first = VNextPlanSelection::buildPlanHash(root);
    auto second = VNextPlanSelection::buildPlanHash(root);
    ASSERT_TRUE(first.ok);
    ASSERT_TRUE(second.ok);
    EXPECT_EQ(first.plan_hash, second.plan_hash);
    EXPECT_EQ(first.serialized, second.serialized);
    EXPECT_NE(std::string::npos, first.serialized.find("a=1.500000"));
    EXPECT_NE(std::string::npos, first.serialized.find("ROOT|1|2|a=1.500000|b=z"));
}

TEST(OptimizerVNextPlanSelectionTest, DuplicatePlanHashKeysRejectWithOPT0307)
{
    PlanHashNode root;
    root.node_symbol = "ROOT";
    root.attributes.push_back(PlanHashAttribute::makeString("dup", "x"));
    root.attributes.push_back(PlanHashAttribute::makeString("dup", "y"));

    auto result = VNextPlanSelection::buildPlanHash(root);
    ASSERT_FALSE(result.ok);
    EXPECT_EQ("OPT_0307", result.error_code);
}

TEST(OptimizerVNextPlanSelectionTest, SelectionFiltersIsolationViolationsBeforeChoosing)
{
    VNextPlanCandidateInput filtered = makeBaseCandidate(QueryTrack::RELATIONAL_TRACK);
    filtered.base_cardinality = 1;
    filtered.violates_isolation_constraints = true;
    filtered.plan_root = makePlanNode("SCAN", "filtered");

    VNextPlanCandidateInput usable = makeBaseCandidate(QueryTrack::RELATIONAL_TRACK);
    usable.base_cardinality = 1000;
    usable.plan_root = makePlanNode("SCAN", "usable");
    const auto usable_score = VNextPlanSelection::scoreCandidate(usable);
    ASSERT_TRUE(usable_score.ok);

    auto result = VNextPlanSelection::selectBestPlan({filtered, usable});
    ASSERT_TRUE(result.ok) << result.error_code << ": " << result.error_message;
    ASSERT_EQ(1U, result.scores.size());
    EXPECT_EQ(0U, result.selected_index);
    EXPECT_EQ(usable_score.plan_hash, result.scores[result.selected_index].plan_hash);
}

TEST(OptimizerVNextPlanSelectionTest, EqualCostPlansTieBreakByLexicalPlanHash)
{
    VNextPlanCandidateInput first = makeBaseCandidate(QueryTrack::RELATIONAL_TRACK);
    first.plan_root = makePlanNode("SCAN", "aaa");

    VNextPlanCandidateInput second = makeBaseCandidate(QueryTrack::RELATIONAL_TRACK);
    second.plan_root = makePlanNode("SCAN", "zzz");

    auto result = VNextPlanSelection::selectBestPlan({first, second});
    ASSERT_TRUE(result.ok) << result.error_code << ": " << result.error_message;
    ASSERT_EQ(2U, result.scores.size());

    const size_t expected =
        (result.scores[0].plan_hash < result.scores[1].plan_hash) ? 0U : 1U;
    EXPECT_EQ(expected, result.selected_index);
}

TEST(OptimizerVNextPlanSelectionTest, MissingPlanHashInEqualCostTieRejectsWithOPT0307)
{
    VNextPlanCandidateInput first = makeBaseCandidate(QueryTrack::RELATIONAL_TRACK);
    first.plan_root.reset();

    VNextPlanCandidateInput second = makeBaseCandidate(QueryTrack::RELATIONAL_TRACK);
    second.plan_root.reset();

    auto result = VNextPlanSelection::selectBestPlan({first, second});
    ASSERT_FALSE(result.ok);
    EXPECT_EQ("OPT_0307", result.error_code);
}

TEST(OptimizerVNextPlanSelectionTest, MetricsEmissionTracksScoreAndSelectionOutcomes)
{
    const std::string metric = "scratchbird_vnext_optimizer_events_total";
    const double score_ok_before = metricCounterValue(metric, {"plan_score", "ok", "NONE"});
    const double score_reject_before = metricCounterValue(metric, {"plan_score", "reject", "OPT_0302"});
    const double select_ok_before = metricCounterValue(metric, {"plan_select", "ok", "NONE"});
    const double select_reject_before = metricCounterValue(metric, {"plan_select", "reject", "OPT_0301"});

    VNextPlanCandidateInput valid = makeBaseCandidate(QueryTrack::RELATIONAL_TRACK);
    auto valid_score = VNextPlanSelection::scoreCandidate(valid);
    ASSERT_TRUE(valid_score.ok);

    VNextPlanCandidateInput invalid = makeBaseCandidate(QueryTrack::RELATIONAL_TRACK);
    invalid.page_size_bytes.reset();
    auto invalid_score = VNextPlanSelection::scoreCandidate(invalid);
    ASSERT_FALSE(invalid_score.ok);
    EXPECT_EQ("OPT_0302", invalid_score.error_code);

    auto selection_ok = VNextPlanSelection::selectBestPlan({valid});
    ASSERT_TRUE(selection_ok.ok);

    auto selection_reject = VNextPlanSelection::selectBestPlan({});
    ASSERT_FALSE(selection_reject.ok);
    EXPECT_EQ("OPT_0301", selection_reject.error_code);

    EXPECT_EQ(score_ok_before + 2.0, metricCounterValue(metric, {"plan_score", "ok", "NONE"}));
    EXPECT_EQ(score_reject_before + 1.0,
              metricCounterValue(metric, {"plan_score", "reject", "OPT_0302"}));
    EXPECT_EQ(select_ok_before + 1.0, metricCounterValue(metric, {"plan_select", "ok", "NONE"}));
    EXPECT_EQ(select_reject_before + 1.0,
              metricCounterValue(metric, {"plan_select", "reject", "OPT_0301"}));
}
