#include <gtest/gtest.h>

#include "scratchbird/optimizer/cost_model.h"

#include <algorithm>
#include <string>

using scratchbird::optimizer::CostEstimate;
using scratchbird::optimizer::CostFormulaProfile;
using scratchbird::optimizer::CostModel;
using scratchbird::optimizer::CostParameters;
using scratchbird::optimizer::IndexFamilyCostCalibrationInput;
using scratchbird::optimizer::deriveIndexFamilyFormulaProfile;

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

    auto findInputEstimate(const CostEstimate &cost, const std::string &name)
        -> const scratchbird::optimizer::CostInputEstimate *
    {
        const auto it = std::find_if(
            cost.input_estimates.begin(),
            cost.input_estimates.end(),
            [&](const scratchbird::optimizer::CostInputEstimate &estimate) {
                return estimate.name == name;
            });
        return it == cost.input_estimates.end() ? nullptr : &(*it);
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

TEST(CostModelTest, CostEstimatesCarryGovernedFormulaProfileAndExpandedTerms)
{
    auto model = defaultModel();

    const auto sort = model.costSort(3200, 64, 2);

    EXPECT_FALSE(sort.formula_profile_id.empty());
    EXPECT_GT(sort.formula_profile_version, 0u);
    EXPECT_FALSE(sort.calibration_profile_id.empty());
    EXPECT_EQ(sort.storage_profile, "heap_btree");
    EXPECT_EQ(sort.workload_profile, "mixed_oltp");
    EXPECT_FALSE(sort.resource_governance_outcome.empty());
    EXPECT_FALSE(sort.input_estimates.empty());
    EXPECT_FALSE(sort.expanded_terms.empty());

    double summed_terms = 0.0;
    for (const auto &term : sort.expanded_terms)
    {
        summed_terms += term.contribution;
        EXPECT_FALSE(term.name.empty());
        EXPECT_FALSE(term.unit.empty());
    }
    EXPECT_NEAR(summed_terms, sort.total_cost, 1e-9);
}

TEST(CostModelTest, ExplicitFormulaProfilesDriveCostIdentityAndCoefficientChoices)
{
    CostFormulaProfile oltp_profile;
    oltp_profile.profile_id = "sb_cost_formula/test_oltp";
    oltp_profile.profile_version = 7;
    oltp_profile.calibration_profile_id = "sb_cost_calibration/test_oltp";
    oltp_profile.storage_profile = "heap_btree";
    oltp_profile.workload_profile = "oltp";
    oltp_profile.parameters.seq_page_cost = 1.0;
    oltp_profile.parameters.random_page_cost = 4.0;
    oltp_profile.parameters.cpu_tuple_cost = 0.01;

    CostFormulaProfile analytics_profile = oltp_profile;
    analytics_profile.profile_id = "sb_cost_formula/test_analytics";
    analytics_profile.calibration_profile_id = "sb_cost_calibration/test_analytics";
    analytics_profile.workload_profile = "analytics";
    analytics_profile.parameters.random_page_cost = 2.0;
    analytics_profile.parameters.cpu_tuple_cost = 0.02;

    CostModel oltp_model(oltp_profile);
    CostModel analytics_model(analytics_profile);

    const auto oltp_index =
        oltp_model.costIndexScan(3, 12, 500, 64, 500, oltp_model.operatorCost("="), 0.3);
    const auto analytics_index =
        analytics_model.costIndexScan(3,
                                      12,
                                      500,
                                      64,
                                      500,
                                      analytics_model.operatorCost("="),
                                      0.3);

    EXPECT_EQ(oltp_index.formula_profile_id, "sb_cost_formula/test_oltp");
    EXPECT_EQ(oltp_index.formula_profile_version, 7u);
    EXPECT_EQ(oltp_index.calibration_profile_id, "sb_cost_calibration/test_oltp");
    EXPECT_EQ(analytics_index.formula_profile_id, "sb_cost_formula/test_analytics");
    EXPECT_EQ(analytics_index.calibration_profile_id,
              "sb_cost_calibration/test_analytics");
    EXPECT_NE(analytics_index.total_cost, oltp_index.total_cost);
}

TEST(CostModelTest, DerivedIndexFamilyProfilesEncodeFamilyIdentityAndAdjustCoefficients)
{
    CostParameters params;
    params.seq_page_cost = 1.0;
    params.random_page_cost = 4.0;
    params.cpu_tuple_cost = 0.01;
    params.cpu_index_tuple_cost = 0.005;
    params.cpu_operator_cost = 0.0025;

    IndexFamilyCostCalibrationInput input;
    input.planner_family = "BTREE_ORDERED_SCAN";
    input.metrics_type_name = "ORDERED_EXACT";
    input.family_metrics_version = 7;
    input.metrics_confidence_class = "HIGH";
    input.correlation = 0.92;
    input.bloat_ratio = 0.40;
    input.recheck_ratio_est = 0.25;
    input.coverage_fraction = 1.0;
    input.ordered_output = true;
    input.covering_index = true;
    input.row_width_bytes = 192;
    input.heap_rows_per_page = 28.0;
    input.index_entries_per_page = 144.0;
    input.dead_fraction = 0.18;
    input.duplicate_density = 0.35;
    input.avg_probe_pages = 4.0;

    const CostFormulaProfile profile =
        deriveIndexFamilyFormulaProfile(params, input);
    CostModel model(profile);
    const CostEstimate index_only =
        model.costIndexOnlyScan(2, 8, 64, model.operatorCost("="), 0.92);

    EXPECT_EQ(profile.profile_id,
              "sb_cost_formula/index_family/btree_ordered_scan/ordered_exact/v7/high");
    EXPECT_EQ(profile.profile_version, 7u);
    EXPECT_EQ(profile.calibration_profile_id,
              "sb_cost_calibration/index_family/ordered_exact/btree_ordered_scan/v7");
    EXPECT_EQ(profile.storage_profile, "ordered_exact");
    EXPECT_EQ(profile.workload_profile, "ordered_read");
    EXPECT_EQ(profile.parameters.default_row_width_bytes, 192u);
    EXPECT_DOUBLE_EQ(profile.parameters.calibrated_heap_rows_per_page, 28.0);
    EXPECT_DOUBLE_EQ(profile.parameters.calibrated_index_entries_per_page, 144.0);
    EXPECT_DOUBLE_EQ(profile.parameters.calibrated_avg_probe_pages, 4.0);
    EXPECT_DOUBLE_EQ(profile.parameters.calibrated_dead_fraction, 0.18);
    EXPECT_DOUBLE_EQ(profile.parameters.calibrated_duplicate_density, 0.35);
    EXPECT_GT(profile.parameters.calibrated_visibility_tuple_cost, 0.0);
    EXPECT_LT(profile.parameters.random_page_cost, params.random_page_cost * 1.5);
    EXPECT_LT(profile.parameters.cpu_tuple_cost, params.cpu_tuple_cost * 1.5);
    EXPECT_GT(profile.parameters.cpu_index_tuple_cost, params.cpu_index_tuple_cost);
    EXPECT_EQ(index_only.formula_profile_id, profile.profile_id);
    EXPECT_EQ(index_only.calibration_profile_id, profile.calibration_profile_id);
    EXPECT_EQ(index_only.row_width_bytes, 192u);
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

TEST(CostModelTest, TopNSortCostsLessThanFullSort)
{
    auto model = defaultModel();

    const auto full_sort = model.costSort(3200, 64, 2);
    const auto top_n_sort = model.costSort(3200, 64, 2, 25, nullptr);

    EXPECT_EQ(full_sort.rows, top_n_sort.rows);
    EXPECT_LT(top_n_sort.total_cost, full_sort.total_cost);
    EXPECT_LT(top_n_sort.startup_cost, full_sort.startup_cost);
}

TEST(CostModelTest, GatherCostPublishesParallelTermsAndCanBeatSerialRunCost)
{
    auto model = defaultModel();
    const auto serial = model.costSort(16000, 64, 2);
    const auto gather = model.costGather(serial, 16000, 4, true);

    EXPECT_EQ(gather.rows, 16000u);
    EXPECT_EQ(gather.operator_name, "GATHER");
    EXPECT_FALSE(gather.expanded_terms.empty());
    EXPECT_GT(gather.startup_cost, serial.startup_cost);
    EXPECT_GT(gather.run_cost, 0.0);
    EXPECT_DOUBLE_EQ(gather.total_cost, gather.startup_cost + gather.run_cost);
}

TEST(CostModelTest, GatherMergeCostsMoreThanGatherBecauseOfMergeFanIn)
{
    auto model = defaultModel();
    const auto serial = model.costSort(16000, 64, 2);
    const auto gather = model.costGather(serial, 16000, 4, true);
    const auto gather_merge = model.costGatherMerge(serial, 16000, 2, 4, true);

    EXPECT_EQ(gather_merge.rows, gather.rows);
    EXPECT_EQ(gather_merge.operator_name, "GATHER_MERGE");
    EXPECT_GT(gather_merge.total_cost, gather.total_cost);
    EXPECT_FALSE(gather_merge.expanded_terms.empty());
}

TEST(CostModelTest, SortCostMarksSpillWhenWorkMemTooSmall)
{
    CostParameters small_params;
    small_params.seq_page_cost = 1.0;
    small_params.random_page_cost = 4.0;
    small_params.cpu_tuple_cost = 0.01;
    small_params.cpu_index_tuple_cost = 0.005;
    small_params.cpu_operator_cost = 0.0025;
    small_params.effective_cache_size = 128.0;
    small_params.work_mem_bytes = 64 * 1024;

    CostParameters large_params = small_params;
    large_params.work_mem_bytes = 16 * 1024 * 1024;

    CostModel spill_model(small_params);
    CostModel in_memory_model(large_params);

    const auto spilled = spill_model.costSort(8192, 256, 2);
    const auto in_memory = in_memory_model.costSort(8192, 256, 2);

    EXPECT_TRUE(spilled.spill_expected);
    EXPECT_GT(spilled.spill_passes, 0u);
    EXPECT_GT(spilled.spill_bytes, 0u);
    EXPECT_GT(spilled.total_cost, in_memory.total_cost);
    EXPECT_LT(spilled.memory_budget_bytes, spilled.memory_bytes);
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

TEST(CostModelTest, MergeJoinComparisonBeatsHashJoinWhenInputsAreAlreadyOrdered)
{
    auto model = defaultModel();

    const auto outer =
        model.costIndexOnlyScan(3, 2, 256, 0.0, 1.0);
    const auto inner =
        model.costIndexOnlyScan(3, 2, 256, 0.0, 1.0);

    const auto hash =
        model.costHashJoin(outer, inner, 256, 256, 0.01, scratchbird::parser::JoinType::INNER);
    const auto merge =
        model.costMergeJoin(outer,
                            inner,
                            256,
                            256,
                            0.01,
                            true,
                            true,
                            scratchbird::parser::JoinType::INNER);

    EXPECT_EQ(hash.rows, merge.rows);
    EXPECT_LT(merge.total_cost, hash.total_cost);
}

TEST(CostModelTest, HashJoinComparisonBeatsMergeJoinWhenInputsRequireSorts)
{
    auto model = defaultModel();

    const auto outer = model.costSeqScan(32, 1000, 0.0);
    const auto inner = model.costSeqScan(32, 1000, 0.0);

    const auto hash =
        model.costHashJoin(outer, inner, 1000, 1000, 0.01, scratchbird::parser::JoinType::INNER);
    const auto merge =
        model.costMergeJoin(outer,
                            inner,
                            1000,
                            1000,
                            0.01,
                            false,
                            false,
                            scratchbird::parser::JoinType::INNER);

    EXPECT_EQ(hash.rows, merge.rows);
    EXPECT_LT(hash.total_cost, merge.total_cost);
}

TEST(CostModelTest, HashJoinComparisonBeatsMergeJoinForTinyUnorderedInputs)
{
    auto model = defaultModel();

    const auto outer = model.costSeqScan(1, 3, 0.0);
    const auto inner = model.costSeqScan(1, 3, 0.0);

    const auto hash =
        model.costHashJoin(outer, inner, 3, 3, 0.1, scratchbird::parser::JoinType::INNER);
    const auto merge =
        model.costMergeJoin(outer,
                            inner,
                            3,
                            3,
                            0.1,
                            false,
                            false,
                            scratchbird::parser::JoinType::INNER);

    EXPECT_EQ(hash.rows, merge.rows);
    EXPECT_LT(hash.total_cost, merge.total_cost);
}

TEST(CostModelTest, HashJoinComparisonBeatsMergeJoinForDuplicateHeavyUnorderedInputs)
{
    auto model = defaultModel();

    const auto outer = model.costSeqScan(32, 1000, 0.0);
    const auto inner = model.costSeqScan(32, 1000, 0.0);

    const auto hash =
        model.costHashJoin(outer,
                           inner,
                           1000,
                           1000,
                           2.0 / 9.0,
                           scratchbird::parser::JoinType::INNER);
    const auto merge =
        model.costMergeJoin(outer,
                            inner,
                            1000,
                            1000,
                            2.0 / 9.0,
                            false,
                            false,
                            scratchbird::parser::JoinType::INNER);

    EXPECT_EQ(hash.rows, merge.rows);
    EXPECT_LT(hash.total_cost, merge.total_cost);
}

TEST(CostModelTest, HashJoinCostMarksSpillWhenBuildSideExceedsBudget)
{
    CostParameters small_params;
    small_params.seq_page_cost = 1.0;
    small_params.random_page_cost = 4.0;
    small_params.cpu_tuple_cost = 0.01;
    small_params.cpu_index_tuple_cost = 0.005;
    small_params.cpu_operator_cost = 0.0025;
    small_params.effective_cache_size = 128.0;
    small_params.work_mem_bytes = 64 * 1024;

    CostParameters large_params = small_params;
    large_params.work_mem_bytes = 16 * 1024 * 1024;

    CostModel spill_model(small_params);
    CostModel in_memory_model(large_params);

    const auto outer = spill_model.costSeqScan(256, 20000, 0.0);
    const auto inner = spill_model.costSeqScan(256, 20000, 0.0);
    const auto spilled = spill_model.costHashJoin(
        outer, inner, 20000, 20000, 0.01, scratchbird::parser::JoinType::INNER);

    const auto outer_large = in_memory_model.costSeqScan(256, 20000, 0.0);
    const auto inner_large = in_memory_model.costSeqScan(256, 20000, 0.0);
    const auto in_memory = in_memory_model.costHashJoin(
        outer_large,
        inner_large,
        20000,
        20000,
        0.01,
        scratchbird::parser::JoinType::INNER);

    EXPECT_TRUE(spilled.spill_expected);
    EXPECT_GT(spilled.spill_passes, 0u);
    EXPECT_GT(spilled.spill_bytes, 0u);
    EXPECT_GT(spilled.total_cost, in_memory.total_cost);
    EXPECT_LT(spilled.memory_budget_bytes, spilled.memory_bytes);
}

TEST(CostModelTest, PhysicalCalibrationChangesIndexTraversalAndVisibilityCosts)
{
    CostParameters params;
    params.seq_page_cost = 1.0;
    params.random_page_cost = 4.0;
    params.cpu_tuple_cost = 0.01;
    params.cpu_index_tuple_cost = 0.005;
    params.cpu_operator_cost = 0.0025;

    CostModel baseline_model(params);
    const auto baseline =
        baseline_model.costIndexScan(3, 6, 240, 24, 240, baseline_model.operatorCost("="), 0.15);

    IndexFamilyCostCalibrationInput input;
    input.planner_family = "BTREE_ORDERED_SCAN";
    input.metrics_type_name = "ORDERED_EXACT";
    input.family_metrics_version = 9;
    input.metrics_confidence_class = "HIGH";
    input.correlation = 0.15;
    input.row_width_bytes = 224;
    input.heap_rows_per_page = 12.0;
    input.index_entries_per_page = 40.0;
    input.dead_fraction = 0.30;
    input.duplicate_density = 0.40;
    input.avg_probe_pages = 5.0;

    CostModel calibrated_model(deriveIndexFamilyFormulaProfile(params, input));
    const auto calibrated =
        calibrated_model.costIndexScan(3, 6, 240, 24, 240, calibrated_model.operatorCost("="), 0.15);

    EXPECT_EQ(calibrated.row_width_bytes, 224u);
    EXPECT_GT(calibrated.startup_cost, baseline.startup_cost);
    EXPECT_GT(calibrated.total_cost, baseline.total_cost);

    const auto *row_width = findInputEstimate(calibrated, "row_width_bytes");
    const auto *heap_rows = findInputEstimate(calibrated, "heap_rows_per_page");
    const auto *probe_pages = findInputEstimate(calibrated, "avg_probe_pages");
    ASSERT_NE(row_width, nullptr);
    ASSERT_NE(heap_rows, nullptr);
    ASSERT_NE(probe_pages, nullptr);
    EXPECT_DOUBLE_EQ(row_width->value, 224.0);
    EXPECT_DOUBLE_EQ(heap_rows->value, 12.0);
    EXPECT_DOUBLE_EQ(probe_pages->value, 5.0);
}

TEST(CostModelTest, WiderRowsIncreaseHashJoinSpillAndCost)
{
    CostParameters params;
    params.seq_page_cost = 1.0;
    params.random_page_cost = 4.0;
    params.cpu_tuple_cost = 0.01;
    params.cpu_index_tuple_cost = 0.005;
    params.cpu_operator_cost = 0.0025;
    params.work_mem_bytes = 128 * 1024;

    CostFormulaProfile narrow_profile;
    narrow_profile.parameters = params;
    narrow_profile.parameters.default_row_width_bytes = 64;
    narrow_profile.profile_id = "sb_cost_formula/test_narrow";
    narrow_profile.calibration_profile_id = "sb_cost_calibration/test_narrow";

    CostFormulaProfile wide_profile = narrow_profile;
    wide_profile.parameters.default_row_width_bytes = 512;
    wide_profile.profile_id = "sb_cost_formula/test_wide";
    wide_profile.calibration_profile_id = "sb_cost_calibration/test_wide";

    CostModel narrow_model(narrow_profile);
    CostModel wide_model(wide_profile);

    const auto narrow_outer = narrow_model.costSeqScan(256, 12000, 0.0);
    const auto narrow_inner = narrow_model.costSeqScan(256, 12000, 0.0);
    const auto wide_outer = wide_model.costSeqScan(256, 12000, 0.0);
    const auto wide_inner = wide_model.costSeqScan(256, 12000, 0.0);

    const auto narrow_hash = narrow_model.costHashJoin(
        narrow_outer, narrow_inner, 12000, 12000, 0.01, scratchbird::parser::JoinType::INNER);
    const auto wide_hash = wide_model.costHashJoin(
        wide_outer, wide_inner, 12000, 12000, 0.01, scratchbird::parser::JoinType::INNER);

    EXPECT_TRUE(narrow_hash.spill_expected);
    EXPECT_TRUE(wide_hash.spill_expected);
    EXPECT_GT(wide_hash.memory_bytes, narrow_hash.memory_bytes);
    EXPECT_GT(wide_hash.total_cost, narrow_hash.total_cost);
}

TEST(CostModelTest, SortSpillPublishesMergeFanoutEvidence)
{
    CostParameters params;
    params.seq_page_cost = 1.0;
    params.random_page_cost = 4.0;
    params.cpu_tuple_cost = 0.01;
    params.cpu_index_tuple_cost = 0.005;
    params.cpu_operator_cost = 0.0025;
    params.work_mem_bytes = 64 * 1024;

    CostModel model(params);
    const auto spilled = model.costSort(16384, 256, 3);

    EXPECT_TRUE(spilled.spill_expected);
    EXPECT_GT(spilled.spill_passes, 0u);

    const auto *initial_runs = findInputEstimate(spilled, "spill_initial_runs");
    const auto *merge_fanout = findInputEstimate(spilled, "spill_merge_fanout");
    ASSERT_NE(initial_runs, nullptr);
    ASSERT_NE(merge_fanout, nullptr);
    EXPECT_GT(initial_runs->value, 1.0);
    EXPECT_GE(merge_fanout->value, 2.0);
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
