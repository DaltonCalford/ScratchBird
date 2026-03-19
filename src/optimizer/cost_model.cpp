/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/core/debug.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace scratchbird::optimizer
{
    namespace
    {
        struct SpillEstimate
        {
            uint64_t working_set_bytes = 0;
            uint64_t budget_bytes = 0;
            bool spill_expected = false;
            uint32_t spill_passes = 0;
            uint64_t spill_bytes = 0;
            uint64_t initial_runs = 0;
            uint32_t merge_fanout = 0;
            double io_cost = 0.0;
            double cpu_cost = 0.0;
        };

        auto safeBudgetBytes(uint64_t requested) -> uint64_t
        {
            constexpr uint64_t MIN_BUDGET_BYTES = 64 * 1024;
            return std::max<uint64_t>(MIN_BUDGET_BYTES, requested);
        }

        auto ceilDivU64(uint64_t value, uint64_t divisor) -> uint64_t
        {
            if (divisor == 0)
            {
                return 0;
            }
            return (value + divisor - 1) / divisor;
        }

        auto estimateSpill(const CostParameters &params,
                           uint64_t working_set_bytes,
                           uint64_t budget_bytes,
                           uint64_t touched_rows,
                           double cpu_penalty_scale = 1.0) -> SpillEstimate
        {
            SpillEstimate spill;
            spill.working_set_bytes = working_set_bytes;
            spill.budget_bytes = safeBudgetBytes(budget_bytes);
            if (working_set_bytes == 0 || working_set_bytes <= spill.budget_bytes)
            {
                return spill;
            }

            spill.spill_expected = true;
            spill.spill_bytes = working_set_bytes;

            const uint64_t page_size =
                std::max<uint64_t>(1024, params.planner_page_size_bytes);
            const uint64_t budget_pages =
                std::max<uint64_t>(2, spill.budget_bytes / page_size);
            spill.merge_fanout =
                static_cast<uint32_t>(std::max<uint64_t>(2, budget_pages));
            spill.initial_runs = std::max<uint64_t>(
                2, ceilDivU64(working_set_bytes, spill.budget_bytes));
            uint64_t remaining_runs = spill.initial_runs;
            while (remaining_runs > 1)
            {
                remaining_runs =
                    ceilDivU64(remaining_runs,
                               std::max<uint64_t>(2, spill.merge_fanout));
                ++spill.spill_passes;
            }
            const double spill_pages =
                static_cast<double>(ceilDivU64(working_set_bytes, page_size));
            spill.io_cost =
                spill_pages * params.spill_page_cost *
                static_cast<double>(1 + (2 * spill.spill_passes));
            spill.cpu_cost =
                static_cast<double>(touched_rows) * params.spill_cpu_tuple_cost *
                static_cast<double>(std::max<uint32_t>(1, spill.spill_passes)) *
                cpu_penalty_scale;
            return spill;
        }

        auto safeRowWidthBytes(const CostParameters &params) -> uint64_t
        {
            return std::max<uint64_t>(16, params.default_row_width_bytes);
        }

        auto calibratedHeapRowsPerPage(const CostParameters &params,
                                       uint64_t row_width_bytes,
                                       uint64_t num_pages,
                                       uint64_t num_rows) -> double
        {
            if (params.calibrated_heap_rows_per_page > 0.0)
            {
                return std::max(1.0, params.calibrated_heap_rows_per_page);
            }
            if (num_pages > 0 && num_rows > 0)
            {
                return std::max(
                    1.0,
                    static_cast<double>(num_rows) /
                        static_cast<double>(num_pages));
            }
            const uint64_t page_size =
                std::max<uint64_t>(1024, params.planner_page_size_bytes);
            return std::max(
                1.0,
                static_cast<double>(page_size) /
                    static_cast<double>(std::max<uint64_t>(16, row_width_bytes)));
        }

        auto calibratedIndexEntriesPerPage(const CostParameters &params,
                                           uint64_t row_width_bytes,
                                           uint64_t index_pages,
                                           uint64_t index_tuples) -> double
        {
            if (params.calibrated_index_entries_per_page > 0.0)
            {
                return std::max(1.0, params.calibrated_index_entries_per_page);
            }
            if (index_pages > 0 && index_tuples > 0)
            {
                return std::max(
                    1.0,
                    static_cast<double>(index_tuples) /
                        static_cast<double>(index_pages));
            }
            const uint64_t page_size =
                std::max<uint64_t>(1024, params.planner_page_size_bytes);
            return std::max(
                1.0,
                static_cast<double>(page_size) /
                    static_cast<double>(std::max<uint64_t>(16, row_width_bytes / 2)));
        }

        auto calibratedProbePages(const CostParameters &params,
                                  uint64_t fallback_height) -> double
        {
            if (params.calibrated_avg_probe_pages > 0.0)
            {
                return std::max(1.0, params.calibrated_avg_probe_pages);
            }
            return static_cast<double>(std::max<uint64_t>(1, fallback_height));
        }

        auto calibratedVisibilityTupleCost(const CostParameters &params) -> double
        {
            return std::max(0.0, params.calibrated_visibility_tuple_cost);
        }

        auto applyResourceEstimate(CostEstimate &cost,
                                   const SpillEstimate &spill) -> void
        {
            cost.memory_bytes = spill.working_set_bytes;
            cost.memory_budget_bytes = spill.budget_bytes;
            cost.spill_expected = spill.spill_expected;
            cost.spill_passes = spill.spill_passes;
            cost.spill_bytes = spill.spill_bytes;
            if (spill.working_set_bytes == 0)
            {
                cost.resource_governance_outcome = "NO_MEMORY_GOVERNANCE";
            }
            else if (spill.spill_expected)
            {
                cost.resource_governance_outcome = "SPILL_EXPECTED";
            }
            else
            {
                cost.resource_governance_outcome = "IN_MEMORY";
            }
        }

        auto profileSignatureSuffix(const CostParameters &params) -> std::string
        {
            uint64_t hash = 1469598103934665603ULL;
            auto mix = [&](double value) {
                const auto scaled = static_cast<uint64_t>(std::llround(value * 1000000.0));
                hash ^= scaled;
                hash *= 1099511628211ULL;
            };
            auto mix_u64 = [&](uint64_t value) {
                hash ^= value;
                hash *= 1099511628211ULL;
            };

            mix(params.seq_page_cost);
            mix(params.random_page_cost);
            mix(params.cpu_tuple_cost);
            mix(params.cpu_index_tuple_cost);
            mix(params.cpu_operator_cost);
            mix(params.sort_mem_cost);
            mix_u64(params.work_mem_bytes);
            mix_u64(params.planner_page_size_bytes);
            mix(params.spill_page_cost);
            mix(params.spill_cpu_tuple_cost);
            mix(params.hash_mem_multiplier);
            mix(params.merge_mem_multiplier);
            mix(params.aggregate_mem_multiplier);
            mix_u64(params.default_row_width_bytes);
            mix_u64(params.hash_tuple_overhead_bytes);
            mix_u64(params.sort_tuple_overhead_bytes);
            mix(params.calibrated_heap_rows_per_page);
            mix(params.calibrated_index_entries_per_page);
            mix(params.calibrated_avg_probe_pages);
            mix(params.calibrated_duplicate_density);
            mix(params.calibrated_dead_fraction);
            mix(params.calibrated_false_positive_ratio);
            mix(params.calibrated_visibility_tuple_cost);
            mix(params.parallel_setup_cost);
            mix(params.parallel_tuple_cost);
            mix(params.effective_cache_size);

            std::ostringstream out;
            out << std::hex << hash;
            return out.str();
        }

        auto deriveFormulaProfile(const CostParameters &params) -> CostFormulaProfile
        {
            CostFormulaProfile profile;
            profile.profile_id =
                "sb_cost_formula/heap_btree/mixed_oltp/" +
                profileSignatureSuffix(params);
            profile.profile_version = 1;
            profile.calibration_profile_id =
                "sb_cost_calibration/fixed_seed_alpha_v1";
            profile.storage_profile = "heap_btree";
            profile.workload_profile = "mixed_oltp";
            profile.parameters = params;
            return profile;
        }

        auto normalizeFormulaProfile(CostFormulaProfile profile) -> CostFormulaProfile
        {
            if (profile.profile_id.empty())
            {
                profile.profile_id =
                    "sb_cost_formula/heap_btree/mixed_oltp/" +
                    profileSignatureSuffix(profile.parameters);
            }
            if (profile.profile_version == 0)
            {
                profile.profile_version = 1;
            }
            if (profile.calibration_profile_id.empty())
            {
                profile.calibration_profile_id =
                    "sb_cost_calibration/fixed_seed_alpha_v1";
            }
            if (profile.storage_profile.empty())
            {
                profile.storage_profile = "heap_btree";
            }
            if (profile.workload_profile.empty())
            {
                profile.workload_profile = "mixed_oltp";
            }
            return profile;
        }

        auto normalizeProfileComponent(const std::string &value) -> std::string
        {
            std::string normalized;
            normalized.reserve(value.size());
            bool previous_underscore = false;
            for (unsigned char ch : value)
            {
                if (std::isalnum(ch))
                {
                    normalized.push_back(static_cast<char>(std::tolower(ch)));
                    previous_underscore = false;
                }
                else if (!previous_underscore)
                {
                    normalized.push_back('_');
                    previous_underscore = true;
                }
            }
            while (!normalized.empty() && normalized.front() == '_')
            {
                normalized.erase(normalized.begin());
            }
            while (!normalized.empty() && normalized.back() == '_')
            {
                normalized.pop_back();
            }
            return normalized.empty() ? std::string("unknown") : normalized;
        }

        auto confidencePenaltyMultiplier(const std::string &confidence_class) -> double
        {
            const std::string normalized =
                normalizeProfileComponent(confidence_class);
            if (normalized == "high")
            {
                return 1.00;
            }
            if (normalized == "medium")
            {
                return 1.05;
            }
            if (normalized == "low")
            {
                return 1.15;
            }
            if (normalized == "invalid")
            {
                return 1.30;
            }
            return 1.10;
        }

        auto initializeCostEvidence(CostEstimate &cost,
                                    const CostFormulaProfile &profile,
                                    const std::string &operator_name) -> void
        {
            cost.operator_name = operator_name;
            cost.formula_profile_id = profile.profile_id;
            cost.formula_profile_version = profile.profile_version;
            cost.calibration_profile_id = profile.calibration_profile_id;
            cost.storage_profile = profile.storage_profile;
            cost.workload_profile = profile.workload_profile;
            cost.resource_governance_outcome = "NO_MEMORY_GOVERNANCE";
            cost.input_estimates.clear();
            cost.expanded_terms.clear();
        }

        auto appendInputEstimate(CostEstimate &cost,
                                 const std::string &name,
                                 double value,
                                 const std::string &unit) -> void
        {
            cost.input_estimates.push_back(
                CostInputEstimate{name, value, unit});
        }

        auto appendFormulaTerm(CostEstimate &cost,
                               const std::string &name,
                               double coefficient,
                               double input_value,
                               double contribution,
                               const std::string &unit) -> void
        {
            cost.expanded_terms.push_back(CostFormulaTerm{
                name,
                coefficient,
                input_value,
                contribution,
                unit});
        }
    } // namespace

    CostModel::CostModel(const CostParameters &params)
        : params_(params),
          formula_profile_(deriveFormulaProfile(params))
    {
        DEBUG_LOG_DB("CostModel created with seq_page_cost=" +
                     std::to_string(params_.seq_page_cost) +
                     ", random_page_cost=" + std::to_string(params_.random_page_cost));
    }

    CostModel::CostModel(const CostFormulaProfile &profile)
        : params_(profile.parameters),
          formula_profile_(normalizeFormulaProfile(profile))
    {
        params_ = formula_profile_.parameters;
        DEBUG_LOG_DB("CostModel created with profile=" + formula_profile_.profile_id +
                     " version=" + std::to_string(formula_profile_.profile_version));
    }

    auto deriveIndexFamilyFormulaProfile(
        const CostParameters &params,
        const IndexFamilyCostCalibrationInput &input) -> CostFormulaProfile
    {
        CostFormulaProfile profile = deriveFormulaProfile(params);
        const std::string family_token =
            normalizeProfileComponent(input.planner_family);
        const std::string metrics_token =
            normalizeProfileComponent(input.metrics_type_name);
        const std::string confidence_token =
            normalizeProfileComponent(input.metrics_confidence_class);
        const uint32_t version =
            std::max<uint32_t>(1, input.family_metrics_version);

        profile.profile_id = "sb_cost_formula/index_family/" + family_token +
                             "/" + metrics_token + "/v" +
                             std::to_string(version) + "/" + confidence_token;
        profile.profile_version = version;
        profile.calibration_profile_id =
            "sb_cost_calibration/index_family/" + metrics_token + "/" +
            family_token + "/v" + std::to_string(version);
        profile.storage_profile =
            metrics_token == "unknown" ? family_token : metrics_token;
        if (input.ordered_output)
        {
            profile.workload_profile = "ordered_read";
        }
        else if (input.covering_index)
        {
            profile.workload_profile = "covering_lookup";
        }
        else if (input.requires_recheck)
        {
            profile.workload_profile = "recheck_filter";
        }
        else
        {
            profile.workload_profile = "mixed_oltp";
        }

        const double bloat_ratio = std::clamp(input.bloat_ratio, 0.0, 1.0);
        const double recheck_ratio =
            std::clamp(input.recheck_ratio_est, 0.0, 1.0);
        const double coverage_fraction =
            std::clamp(input.coverage_fraction, 0.0, 1.0);
        const double dead_fraction =
            std::clamp(input.dead_fraction, 0.0, 1.0);
        const double duplicate_density =
            std::clamp(input.duplicate_density, 0.0, 1.0);
        const double false_positive_ratio =
            std::clamp(input.false_positive_ratio, 0.0, 1.0);
        const double avg_range_pages_per_row =
            std::max(0.0, input.avg_range_pages_per_row);
        const double prefix_selectivity =
            std::clamp(input.prefix_selectivity, 0.0, 1.0);
        const double skip_group_count =
            std::max(0.0, input.skip_group_count);
        const double prefetchable_page_fraction =
            std::clamp(input.prefetchable_page_fraction, 0.0, 1.0);
        const double secondary_lookup_fraction =
            std::clamp(input.secondary_lookup_fraction, 0.0, 1.0);
        const double cluster_locality_gain_est =
            std::clamp(input.cluster_locality_gain_est, 0.0, 1.0);
        const double early_stop_gain_est =
            std::clamp(input.early_stop_gain_est, 0.0, 1.0);
        const double overflow_chain_depth =
            std::max(0.0, input.overflow_chain_depth);
        const double run_count =
            std::max(0.0, input.run_count);
        const double level_count =
            std::max(0.0, input.level_count);
        const double tombstone_fraction =
            std::clamp(input.tombstone_fraction, 0.0, 1.0);
        const double l0_run_count =
            std::max(0.0, input.l0_run_count);
        const double sort_avoidance_gain_est =
            std::clamp(input.sort_avoidance_gain_est, 0.0, 1.0);
        const double confidence_penalty =
            confidencePenaltyMultiplier(input.metrics_confidence_class);
        const bool is_skip_scan =
            family_token.find("skip_scan") != std::string::npos;
        const bool is_hash_family =
            family_token.find("hash") != std::string::npos;
        const bool is_lsm_family =
            family_token.find("lsm") != std::string::npos;
        const bool is_ordered_family =
            input.ordered_output ||
            family_token.find("btree") != std::string::npos ||
            is_lsm_family;

        if (input.row_width_bytes > 0)
        {
            profile.parameters.default_row_width_bytes = input.row_width_bytes;
        }
        profile.parameters.calibrated_heap_rows_per_page =
            std::max(0.0, input.heap_rows_per_page);
        profile.parameters.calibrated_index_entries_per_page =
            std::max(0.0, input.index_entries_per_page);
        profile.parameters.calibrated_avg_probe_pages =
            std::max(0.0, input.avg_probe_pages);
        profile.parameters.calibrated_duplicate_density = duplicate_density;
        profile.parameters.calibrated_dead_fraction = dead_fraction;
        profile.parameters.calibrated_false_positive_ratio =
            false_positive_ratio;
        profile.parameters.calibrated_visibility_tuple_cost =
            profile.parameters.cpu_operator_cost *
            (0.10 + (dead_fraction * 0.40) + (recheck_ratio * 0.30) +
             (false_positive_ratio * 0.20));
        profile.parameters.calibrated_avg_range_pages_per_row =
            avg_range_pages_per_row;
        profile.parameters.calibrated_prefix_selectivity =
            prefix_selectivity;
        profile.parameters.calibrated_skip_group_count =
            skip_group_count;
        profile.parameters.calibrated_prefetchable_page_fraction =
            prefetchable_page_fraction;
        profile.parameters.calibrated_secondary_lookup_fraction =
            secondary_lookup_fraction;
        profile.parameters.calibrated_cluster_locality_gain_est =
            cluster_locality_gain_est;
        profile.parameters.calibrated_early_stop_gain_est =
            early_stop_gain_est;
        profile.parameters.calibrated_overflow_chain_depth =
            overflow_chain_depth;
        profile.parameters.calibrated_run_count =
            run_count;
        profile.parameters.calibrated_level_count =
            level_count;
        profile.parameters.calibrated_tombstone_fraction =
            tombstone_fraction;
        profile.parameters.calibrated_l0_run_count =
            l0_run_count;
        profile.parameters.calibrated_sort_avoidance_gain_est =
            sort_avoidance_gain_est;

        profile.parameters.random_page_cost *=
            (1.0 + (bloat_ratio * 0.75)) * confidence_penalty;
        profile.parameters.cpu_index_tuple_cost *= 1.0 + (recheck_ratio * 0.50);
        profile.parameters.cpu_tuple_cost *=
            1.0 + ((1.0 - coverage_fraction) * 0.25) + (recheck_ratio * 0.35);
        profile.parameters.cpu_operator_cost *= 1.0 + (duplicate_density * 0.10);
        if (input.covering_index || coverage_fraction >= 0.999)
        {
            profile.parameters.cpu_tuple_cost *= 0.85;
            profile.parameters.calibrated_visibility_tuple_cost *= 0.5;
        }
        if (is_ordered_family)
        {
            profile.parameters.random_page_cost *= std::clamp(
                1.0 + (std::min(avg_range_pages_per_row, 16.0) * 0.03) -
                    (prefetchable_page_fraction * 0.12) -
                    (cluster_locality_gain_est * 0.12),
                0.55,
                2.50);
            profile.parameters.cpu_tuple_cost *= std::clamp(
                1.0 + (secondary_lookup_fraction * 0.20) -
                    (early_stop_gain_est * 0.10),
                0.60,
                2.50);
            profile.parameters.cpu_operator_cost *= std::clamp(
                1.0 + ((1.0 - prefix_selectivity) * 0.08) -
                    (sort_avoidance_gain_est * 0.08),
                0.60,
                2.50);
            profile.parameters.sort_mem_cost *= std::clamp(
                1.0 - (sort_avoidance_gain_est * 0.15),
                0.50,
                1.00);
        }
        if (is_skip_scan)
        {
            profile.parameters.random_page_cost *= std::clamp(
                1.0 + (std::min(skip_group_count, 64.0) * 0.015),
                1.00,
                2.50);
            profile.parameters.cpu_operator_cost *= std::clamp(
                1.0 + (std::min(skip_group_count, 64.0) * 0.010),
                1.00,
                2.00);
        }
        if (is_hash_family)
        {
            profile.parameters.cpu_operator_cost *= std::clamp(
                1.0 + (overflow_chain_depth * 0.15) +
                    (secondary_lookup_fraction * 0.10),
                1.00,
                3.00);
            profile.parameters.cpu_tuple_cost *= std::clamp(
                1.0 + (secondary_lookup_fraction * 0.15),
                1.00,
                2.00);
        }
        if (is_lsm_family)
        {
            profile.parameters.random_page_cost *= std::clamp(
                1.0 + (run_count * 0.04) +
                    (std::log2(1.0 + level_count) * 0.08) +
                    (l0_run_count * 0.04) +
                    (tombstone_fraction * 0.20),
                1.00,
                3.50);
            profile.parameters.cpu_index_tuple_cost *= std::clamp(
                1.0 + (run_count * 0.02) + (tombstone_fraction * 0.20),
                1.00,
                2.50);
        }
        if (input.ordered_output || std::abs(input.correlation) > 0.8)
        {
            profile.parameters.random_page_cost *= 0.90;
        }
        if (input.requires_recheck)
        {
            profile.parameters.cpu_operator_cost *=
                1.0 + std::max(0.25, recheck_ratio);
        }
        return normalizeFormulaProfile(profile);
    }

    auto CostModel::costSeqScan(uint64_t num_pages, uint64_t num_tuples,
                                 double qual_cost, core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating sequential scan cost: pages=" + std::to_string(num_pages) +
                     ", tuples=" + std::to_string(num_tuples));

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "SEQ_SCAN");
        const uint64_t row_width_bytes = safeRowWidthBytes(params_);
        const double heap_rows_per_page = calibratedHeapRowsPerPage(
            params_, row_width_bytes, num_pages, num_tuples);
        const double visibility_cpu_cost =
            static_cast<double>(num_tuples) * calibratedVisibilityTupleCost(params_);
        appendInputEstimate(cost, "num_pages", static_cast<double>(num_pages), "pages");
        appendInputEstimate(cost, "num_tuples", static_cast<double>(num_tuples), "rows");
        appendInputEstimate(cost, "qual_cost", qual_cost, "cpu_per_row");
        appendInputEstimate(cost,
                            "row_width_bytes",
                            static_cast<double>(row_width_bytes),
                            "bytes");
        appendInputEstimate(cost,
                            "heap_rows_per_page",
                            heap_rows_per_page,
                            "rows_per_page");

        // Sequential scan has no startup cost (no index to traverse, no setup needed)
        cost.startup_cost = 0.0;

        // Disk cost: read all pages sequentially
        // This is the dominant cost for large tables
        double disk_cost = static_cast<double>(num_pages) * params_.seq_page_cost;

        // CPU cost: process all tuples + evaluate WHERE clause
        // cpu_tuple_cost = cost of materializing one tuple
        // qual_cost = cost of evaluating WHERE predicates per tuple
        double cpu_cost = static_cast<double>(num_tuples) * params_.cpu_tuple_cost +
                          static_cast<double>(num_tuples) * qual_cost;
        appendFormulaTerm(cost,
                          "run.disk.sequential_pages",
                          params_.seq_page_cost,
                          static_cast<double>(num_pages),
                          disk_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.cpu.tuple_scan",
                          params_.cpu_tuple_cost,
                          static_cast<double>(num_tuples),
                          static_cast<double>(num_tuples) * params_.cpu_tuple_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.cpu.qual_eval",
                          qual_cost,
                          static_cast<double>(num_tuples),
                          static_cast<double>(num_tuples) * qual_cost,
                          "cost");
        if (visibility_cpu_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "run.cpu.mga_visibility",
                              calibratedVisibilityTupleCost(params_),
                              static_cast<double>(num_tuples),
                              visibility_cpu_cost,
                              "cost");
        }

        cost.run_cost = disk_cost + cpu_cost + visibility_cpu_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = num_tuples;
        cost.row_width_bytes = row_width_bytes;

        DEBUG_LOG_DB("SeqScan cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost));

        return cost;
    }

    auto CostModel::costIndexScan(uint64_t index_height, uint64_t index_pages,
                                   uint64_t index_tuples, uint64_t heap_pages,
                                   uint64_t heap_tuples, double qual_cost,
                                   double correlation, core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating index scan cost: index_height=" + std::to_string(index_height) +
                     ", index_pages=" + std::to_string(index_pages) +
                     ", heap_pages=" + std::to_string(heap_pages) +
                     ", correlation=" + std::to_string(correlation));

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "INDEX_SCAN");
        const uint64_t row_width_bytes = safeRowWidthBytes(params_);
        const double heap_rows_per_page = calibratedHeapRowsPerPage(
            params_, row_width_bytes, heap_pages, heap_tuples);
        const double index_entries_per_page = calibratedIndexEntriesPerPage(
            params_, row_width_bytes, index_pages, index_tuples);
        const double traversal_pages =
            calibratedProbePages(params_, index_height);
        const double duplicate_density =
            std::clamp(params_.calibrated_duplicate_density, 0.0, 1.0);
        const double avg_range_pages_per_row =
            std::max(0.0, params_.calibrated_avg_range_pages_per_row);
        const double prefix_selectivity =
            std::clamp(params_.calibrated_prefix_selectivity, 0.0, 1.0);
        const double skip_group_count =
            std::max(0.0, params_.calibrated_skip_group_count);
        const double prefetchable_page_fraction =
            std::clamp(params_.calibrated_prefetchable_page_fraction, 0.0, 1.0);
        const double secondary_lookup_fraction =
            std::clamp(params_.calibrated_secondary_lookup_fraction, 0.0, 1.0);
        const double cluster_locality_gain_est =
            std::clamp(params_.calibrated_cluster_locality_gain_est, 0.0, 1.0);
        const double early_stop_gain_est =
            std::clamp(params_.calibrated_early_stop_gain_est, 0.0, 1.0);
        const double overflow_chain_depth =
            std::max(0.0, params_.calibrated_overflow_chain_depth);
        const double run_count =
            std::max(0.0, params_.calibrated_run_count);
        const double level_count =
            std::max(0.0, params_.calibrated_level_count);
        const double tombstone_fraction =
            std::clamp(params_.calibrated_tombstone_fraction, 0.0, 1.0);
        const double l0_run_count =
            std::max(0.0, params_.calibrated_l0_run_count);
        const double sort_avoidance_gain_est =
            std::clamp(params_.calibrated_sort_avoidance_gain_est, 0.0, 1.0);
        const double visibility_cpu_cost =
            static_cast<double>(heap_tuples) * calibratedVisibilityTupleCost(params_);
        const bool is_skip_scan =
            formula_profile_.profile_id.find("skip_scan") != std::string::npos;
        const bool is_hash_family =
            formula_profile_.profile_id.find("hash") != std::string::npos;
        const bool is_lsm_family =
            formula_profile_.profile_id.find("lsm") != std::string::npos;
        const bool ordered_family =
            formula_profile_.workload_profile == "ordered_read" ||
            formula_profile_.profile_id.find("btree") != std::string::npos ||
            is_lsm_family;
        appendInputEstimate(cost, "index_height", static_cast<double>(index_height), "levels");
        appendInputEstimate(cost, "index_pages", static_cast<double>(index_pages), "pages");
        appendInputEstimate(cost, "index_tuples", static_cast<double>(index_tuples), "rows");
        appendInputEstimate(cost, "heap_pages", static_cast<double>(heap_pages), "pages");
        appendInputEstimate(cost, "heap_tuples", static_cast<double>(heap_tuples), "rows");
        appendInputEstimate(cost, "qual_cost", qual_cost, "cpu_per_row");
        appendInputEstimate(cost, "correlation", correlation, "ratio");
        appendInputEstimate(cost,
                            "row_width_bytes",
                            static_cast<double>(row_width_bytes),
                            "bytes");
        appendInputEstimate(cost,
                            "heap_rows_per_page",
                            heap_rows_per_page,
                            "rows_per_page");
        appendInputEstimate(cost,
                            "index_entries_per_page",
                            index_entries_per_page,
                            "rows_per_page");
        appendInputEstimate(cost,
                            "avg_probe_pages",
                            traversal_pages,
                            "pages");
        appendInputEstimate(cost,
                            "avg_range_pages_per_row",
                            avg_range_pages_per_row,
                            "pages_per_row");
        appendInputEstimate(cost,
                            "prefix_selectivity",
                            prefix_selectivity,
                            "ratio");
        appendInputEstimate(cost,
                            "skip_group_count",
                            skip_group_count,
                            "groups");
        appendInputEstimate(cost,
                            "prefetchable_page_fraction",
                            prefetchable_page_fraction,
                            "ratio");
        appendInputEstimate(cost,
                            "secondary_lookup_fraction",
                            secondary_lookup_fraction,
                            "ratio");
        appendInputEstimate(cost,
                            "cluster_locality_gain_est",
                            cluster_locality_gain_est,
                            "ratio");
        appendInputEstimate(cost,
                            "early_stop_gain_est",
                            early_stop_gain_est,
                            "ratio");
        appendInputEstimate(cost,
                            "overflow_chain_depth",
                            overflow_chain_depth,
                            "levels");
        appendInputEstimate(cost,
                            "run_count",
                            run_count,
                            "runs");
        appendInputEstimate(cost,
                            "level_count",
                            level_count,
                            "levels");
        appendInputEstimate(cost,
                            "tombstone_fraction",
                            tombstone_fraction,
                            "ratio");
        appendInputEstimate(cost,
                            "l0_run_count",
                            l0_run_count,
                            "runs");
        appendInputEstimate(cost,
                            "sort_avoidance_gain_est",
                            sort_avoidance_gain_est,
                            "ratio");

        // Startup cost: traverse B-tree from root to first matching entry
        const double traversal_io_cost =
            traversal_pages *
            effectiveRandomPageCost(std::max<uint64_t>(1, index_pages));
        const double traversal_cpu_cost =
            traversal_pages * params_.cpu_operator_cost;
        cost.startup_cost = traversal_io_cost + traversal_cpu_cost;
        appendFormulaTerm(cost,
                          "startup.index_traversal_io",
                          effectiveRandomPageCost(std::max<uint64_t>(1, index_pages)),
                          traversal_pages,
                          traversal_io_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "startup.index_traversal_cpu",
                          params_.cpu_operator_cost,
                          traversal_pages,
                          traversal_cpu_cost,
                          "cost");

        // Index scan cost: read index pages and process index tuples
        const double effective_index_pages = std::max(
            static_cast<double>(index_pages),
            std::ceil(static_cast<double>(std::max<uint64_t>(1, index_tuples)) /
                      index_entries_per_page) *
                (1.0 + (duplicate_density * 0.25)));
        double index_io_cost =
            effective_index_pages * params_.random_page_cost;
        if (ordered_family && avg_range_pages_per_row > 0.0)
        {
            const double ordered_range_penalty =
                std::min(avg_range_pages_per_row, 8.0) * 0.15;
            index_io_cost += ordered_range_penalty * params_.random_page_cost;
            appendFormulaTerm(cost,
                              "run.index_range_penalty",
                              params_.random_page_cost,
                              ordered_range_penalty,
                              ordered_range_penalty * params_.random_page_cost,
                              "cost");
        }
        if (ordered_family && prefetchable_page_fraction > 0.0)
        {
            const double prefetch_gain =
                index_io_cost * std::min(0.35, prefetchable_page_fraction * 0.20);
            index_io_cost -= prefetch_gain;
            appendFormulaTerm(cost,
                              "run.index_prefetch_gain",
                              -params_.random_page_cost,
                              std::min(0.35, prefetchable_page_fraction * 0.20) *
                                  effective_index_pages,
                              -prefetch_gain,
                              "cost");
        }
        double index_cpu_cost = static_cast<double>(index_tuples) * params_.cpu_index_tuple_cost;
        if (is_lsm_family && run_count > 0.0)
        {
            const double merge_penalty =
                params_.cpu_index_tuple_cost *
                ((run_count * 0.50) +
                 (std::log2(1.0 + level_count) * 0.75) +
                 (l0_run_count * 0.50));
            index_cpu_cost += merge_penalty;
            appendFormulaTerm(cost,
                              "run.lsm_merge_penalty",
                              params_.cpu_index_tuple_cost,
                              (run_count * 0.50) +
                                  (std::log2(1.0 + level_count) * 0.75) +
                                  (l0_run_count * 0.50),
                              merge_penalty,
                              "cost");
        }
        if (is_hash_family && overflow_chain_depth > 0.0)
        {
            const double overflow_penalty =
                params_.cpu_operator_cost * overflow_chain_depth;
            index_cpu_cost += overflow_penalty;
            appendFormulaTerm(cost,
                              "run.hash_overflow_penalty",
                              params_.cpu_operator_cost,
                              overflow_chain_depth,
                              overflow_penalty,
                              "cost");
        }
        appendFormulaTerm(cost,
                          "run.index_io",
                          params_.random_page_cost,
                          effective_index_pages,
                          index_io_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.index_tuple_cpu",
                          params_.cpu_index_tuple_cost,
                          static_cast<double>(index_tuples),
                          index_cpu_cost,
                          "cost");

        // Heap fetch cost: depends on physical ordering correlation
        // If index and heap are well-correlated, heap access is more sequential
        // If poorly correlated, heap access is random

        const double sequential_heap_pages =
            std::ceil(static_cast<double>(heap_tuples) / heap_rows_per_page);
        const double random_heap_pages =
            std::min(static_cast<double>(heap_tuples),
                     static_cast<double>(heap_pages));
        const double correlation_weight =
            std::clamp(std::abs(correlation), 0.0, 1.0);
        double effective_heap_pages =
            (sequential_heap_pages * correlation_weight) +
            (random_heap_pages * (1.0 - correlation_weight));
        effective_heap_pages *= (1.0 - (duplicate_density * 0.20));
        effective_heap_pages *=
            std::clamp(1.0 - (cluster_locality_gain_est * 0.25),
                       0.50,
                       1.00);
        effective_heap_pages = std::max(1.0, effective_heap_pages);

        // Apply cache effect to heap I/O cost
        double effective_random_cost = effectiveRandomPageCost(heap_pages);
        double heap_io_cost = effective_heap_pages * effective_random_cost;
        if (secondary_lookup_fraction > 0.0)
        {
            const double secondary_lookup_penalty =
                heap_io_cost * std::min(0.50, secondary_lookup_fraction * 0.20);
            heap_io_cost += secondary_lookup_penalty;
            appendFormulaTerm(cost,
                              "run.secondary_lookup_penalty",
                              effective_random_cost,
                              effective_heap_pages *
                                  std::min(0.50, secondary_lookup_fraction * 0.20),
                              secondary_lookup_penalty,
                              "cost");
        }
        if (ordered_family && early_stop_gain_est > 0.0)
        {
            const double early_stop_gain =
                heap_io_cost * std::min(0.30, early_stop_gain_est * 0.15);
            heap_io_cost -= early_stop_gain;
            appendFormulaTerm(cost,
                              "run.early_stop_gain",
                              -effective_random_cost,
                              effective_heap_pages *
                                  std::min(0.30, early_stop_gain_est * 0.15),
                              -early_stop_gain,
                              "cost");
        }
        appendInputEstimate(cost, "effective_heap_pages", effective_heap_pages, "pages");
        appendInputEstimate(cost,
                            "effective_random_page_cost",
                            effective_random_cost,
                            "cost_per_page");

        // CPU cost for heap tuples
        double heap_cpu_cost = static_cast<double>(heap_tuples) * params_.cpu_tuple_cost +
                               static_cast<double>(heap_tuples) * qual_cost;
        if (is_skip_scan && skip_group_count > 0.0)
        {
            const double skip_restart_penalty =
                params_.cpu_operator_cost *
                std::min(skip_group_count, 128.0);
            cost.startup_cost += skip_restart_penalty;
            appendFormulaTerm(cost,
                              "startup.skip_restart_penalty",
                              params_.cpu_operator_cost,
                              std::min(skip_group_count, 128.0),
                              skip_restart_penalty,
                              "cost");
        }
        if (ordered_family && sort_avoidance_gain_est > 0.0)
        {
            const double sort_avoid_gain =
                heap_cpu_cost * std::min(0.25, sort_avoidance_gain_est * 0.10);
            heap_cpu_cost -= sort_avoid_gain;
            appendFormulaTerm(cost,
                              "run.sort_avoidance_gain",
                              -params_.cpu_tuple_cost,
                              static_cast<double>(heap_tuples) *
                                  std::min(0.25, sort_avoidance_gain_est * 0.10),
                              -sort_avoid_gain,
                              "cost");
        }
        if (is_lsm_family && tombstone_fraction > 0.0)
        {
            const double tombstone_penalty =
                static_cast<double>(heap_tuples) *
                params_.cpu_operator_cost *
                std::min(1.0, tombstone_fraction);
            heap_cpu_cost += tombstone_penalty;
            appendFormulaTerm(cost,
                              "run.lsm_tombstone_penalty",
                              params_.cpu_operator_cost,
                              static_cast<double>(heap_tuples) *
                                  std::min(1.0, tombstone_fraction),
                              tombstone_penalty,
                              "cost");
        }
        if (ordered_family && prefix_selectivity > 0.0 &&
            prefix_selectivity < 1.0)
        {
            const double prefix_gain =
                index_cpu_cost *
                std::min(0.20, (1.0 - prefix_selectivity) * 0.10);
            index_cpu_cost -= prefix_gain;
            appendFormulaTerm(cost,
                              "run.prefix_selectivity_gain",
                              -params_.cpu_index_tuple_cost,
                              static_cast<double>(index_tuples) *
                                  std::min(0.20,
                                           (1.0 - prefix_selectivity) * 0.10),
                              -prefix_gain,
                              "cost");
        }
        appendFormulaTerm(cost,
                          "run.heap_io",
                          effective_random_cost,
                          effective_heap_pages,
                          heap_io_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.heap_tuple_cpu",
                          params_.cpu_tuple_cost,
                          static_cast<double>(heap_tuples),
                          static_cast<double>(heap_tuples) * params_.cpu_tuple_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.heap_qual_cpu",
                          qual_cost,
                          static_cast<double>(heap_tuples),
                          static_cast<double>(heap_tuples) * qual_cost,
                          "cost");
        if (visibility_cpu_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "run.heap_visibility_cpu",
                              calibratedVisibilityTupleCost(params_),
                              static_cast<double>(heap_tuples),
                              visibility_cpu_cost,
                              "cost");
        }

        cost.run_cost = index_io_cost + index_cpu_cost + heap_io_cost +
                        heap_cpu_cost + visibility_cpu_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = heap_tuples;
        cost.row_width_bytes = row_width_bytes;

        DEBUG_LOG_DB("IndexScan cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     " (index_io=" + std::to_string(index_io_cost) +
                     ", heap_io=" + std::to_string(heap_io_cost) + ")");

        return cost;
    }

    auto CostModel::costIndexOnlyScan(uint64_t index_height,
                                      uint64_t index_pages,
                                      uint64_t index_tuples,
                                      double qual_cost,
                                      double correlation,
                                      core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating index-only scan cost: index_height=" +
                     std::to_string(index_height) +
                     ", index_pages=" + std::to_string(index_pages) +
                     ", index_tuples=" + std::to_string(index_tuples) +
                     ", correlation=" + std::to_string(correlation));

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "INDEX_ONLY_SCAN");
        const uint64_t row_width_bytes = safeRowWidthBytes(params_);
        const double traversal_pages =
            calibratedProbePages(params_, index_height);
        const double index_entries_per_page = calibratedIndexEntriesPerPage(
            params_, row_width_bytes, index_pages, index_tuples);
        const double visibility_cpu_cost =
            static_cast<double>(index_tuples) *
            std::max(params_.cpu_tuple_cost * 0.05,
                     calibratedVisibilityTupleCost(params_) * 0.50);
        appendInputEstimate(cost, "index_height", static_cast<double>(index_height), "levels");
        appendInputEstimate(cost, "index_pages", static_cast<double>(index_pages), "pages");
        appendInputEstimate(cost, "index_tuples", static_cast<double>(index_tuples), "rows");
        appendInputEstimate(cost, "qual_cost", qual_cost, "cpu_per_row");
        appendInputEstimate(cost, "correlation", correlation, "ratio");
        appendInputEstimate(cost,
                            "row_width_bytes",
                            static_cast<double>(row_width_bytes),
                            "bytes");
        appendInputEstimate(cost,
                            "index_entries_per_page",
                            index_entries_per_page,
                            "rows_per_page");
        appendInputEstimate(cost,
                            "avg_probe_pages",
                            traversal_pages,
                            "pages");
        const double traversal_io_cost =
            traversal_pages *
            effectiveRandomPageCost(std::max<uint64_t>(1, index_pages));
        const double traversal_cpu_cost =
            traversal_pages * params_.cpu_operator_cost;
        cost.startup_cost = traversal_io_cost + traversal_cpu_cost;
        appendFormulaTerm(cost,
                          "startup.index_traversal_io",
                          effectiveRandomPageCost(std::max<uint64_t>(1, index_pages)),
                          traversal_pages,
                          traversal_io_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "startup.index_traversal_cpu",
                          params_.cpu_operator_cost,
                          traversal_pages,
                          traversal_cpu_cost,
                          "cost");

        const double locality_discount =
            std::abs(correlation) > 0.8 ? 0.65 : (std::abs(correlation) > 0.4 ? 0.8 : 1.0);
        const double effective_index_pages = std::max(
            static_cast<double>(index_pages),
            std::ceil(static_cast<double>(std::max<uint64_t>(1, index_tuples)) /
                      index_entries_per_page));
        const double index_io_cost =
            effective_index_pages * params_.random_page_cost * locality_discount;
        const double index_cpu_cost =
            static_cast<double>(index_tuples) * params_.cpu_index_tuple_cost;
        const double qual_cpu_cost =
            static_cast<double>(index_tuples) * qual_cost;
        appendFormulaTerm(cost,
                          "run.index_io",
                          params_.random_page_cost * locality_discount,
                          effective_index_pages,
                          index_io_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.index_tuple_cpu",
                          params_.cpu_index_tuple_cost,
                          static_cast<double>(index_tuples),
                          index_cpu_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.visibility_cpu",
                          visibility_cpu_cost /
                              static_cast<double>(std::max<uint64_t>(1, index_tuples)),
                          static_cast<double>(index_tuples),
                          visibility_cpu_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.qual_cpu",
                          qual_cost,
                          static_cast<double>(index_tuples),
                          qual_cpu_cost,
                          "cost");

        cost.run_cost =
            index_io_cost + index_cpu_cost + visibility_cpu_cost + qual_cpu_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = index_tuples;
        cost.row_width_bytes = row_width_bytes;
        return cost;
    }

    auto CostModel::costBitmapScan(uint64_t num_indexes,
                                   uint64_t total_index_pages,
                                   uint64_t heap_pages,
                                   uint64_t heap_tuples,
                                   double qual_cost,
                                   const std::string &bitmap_op,
                                   core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating bitmap scan cost: indexes=" + std::to_string(num_indexes) +
                     ", total_index_pages=" + std::to_string(total_index_pages) +
                     ", heap_pages=" + std::to_string(heap_pages) +
                     ", tuples=" + std::to_string(heap_tuples) +
                     ", bitmap_op=" + bitmap_op);

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "BITMAP_INDEX_SCAN");
        const uint64_t row_width_bytes = safeRowWidthBytes(params_);
        const double heap_rows_per_page = calibratedHeapRowsPerPage(
            params_, row_width_bytes, heap_pages, heap_tuples);
        const double false_positive_ratio =
            std::clamp(params_.calibrated_false_positive_ratio, 0.0, 1.0);
        const double visibility_cpu_cost =
            static_cast<double>(heap_tuples) *
            calibratedVisibilityTupleCost(params_) * 0.50;
        appendInputEstimate(cost, "num_indexes", static_cast<double>(num_indexes), "count");
        appendInputEstimate(cost,
                            "total_index_pages",
                            static_cast<double>(total_index_pages),
                            "pages");
        appendInputEstimate(cost, "heap_pages", static_cast<double>(heap_pages), "pages");
        appendInputEstimate(cost, "heap_tuples", static_cast<double>(heap_tuples), "rows");
        appendInputEstimate(cost, "qual_cost", qual_cost, "cpu_per_row");
        appendInputEstimate(cost,
                            "row_width_bytes",
                            static_cast<double>(row_width_bytes),
                            "bytes");
        appendInputEstimate(cost,
                            "heap_rows_per_page",
                            heap_rows_per_page,
                            "rows_per_page");
        appendInputEstimate(cost,
                            "false_positive_ratio",
                            false_positive_ratio,
                            "ratio");
        const double bitmap_build_cost =
            static_cast<double>(heap_tuples) *
            (params_.cpu_index_tuple_cost + params_.cpu_operator_cost * 0.5);
        const double bitmap_merge_cost =
            static_cast<double>(std::max<uint64_t>(1, num_indexes - 1)) *
            static_cast<double>(heap_tuples) * params_.cpu_operator_cost * 0.35;

        cost.startup_cost = bitmap_build_cost + bitmap_merge_cost;
        appendFormulaTerm(cost,
                          "startup.bitmap_build",
                          params_.cpu_index_tuple_cost + params_.cpu_operator_cost * 0.5,
                          static_cast<double>(heap_tuples),
                          bitmap_build_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "startup.bitmap_merge",
                          params_.cpu_operator_cost * 0.35,
                          static_cast<double>(std::max<uint64_t>(1, num_indexes - 1)) *
                              static_cast<double>(heap_tuples),
                          bitmap_merge_cost,
                          "cost");

        const double index_io_cost =
            static_cast<double>(total_index_pages) * params_.random_page_cost;
        const double candidate_heap_pages = std::max(
            static_cast<double>(heap_pages),
            std::ceil(static_cast<double>(heap_tuples) / heap_rows_per_page));
        const double heap_io_cost =
            candidate_heap_pages *
            std::min(params_.random_page_cost,
                     params_.seq_page_cost + effectiveRandomPageCost(heap_pages) * 0.5);
        const double qual_cpu_cost =
            static_cast<double>(heap_tuples) * (params_.cpu_tuple_cost + qual_cost);
        const double false_positive_cost =
            static_cast<double>(heap_tuples) *
            params_.cpu_operator_cost * false_positive_ratio;
        appendFormulaTerm(cost,
                          "run.index_io",
                          params_.random_page_cost,
                          static_cast<double>(total_index_pages),
                          index_io_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.heap_io",
                          candidate_heap_pages == 0 ? 0.0
                                                     : heap_io_cost /
                                                           candidate_heap_pages,
                          candidate_heap_pages,
                          heap_io_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.heap_tuple_cpu",
                          params_.cpu_tuple_cost + qual_cost,
                          static_cast<double>(heap_tuples),
                          qual_cpu_cost,
                          "cost");
        if (visibility_cpu_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "run.heap_visibility_cpu",
                              (calibratedVisibilityTupleCost(params_) * 0.50),
                              static_cast<double>(heap_tuples),
                              visibility_cpu_cost,
                              "cost");
        }
        if (false_positive_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "run.bitmap_false_positive_cpu",
                              params_.cpu_operator_cost,
                              static_cast<double>(heap_tuples) *
                                  false_positive_ratio,
                              false_positive_cost,
                              "cost");
        }

        const double base_run_cost =
            index_io_cost + heap_io_cost + qual_cpu_cost + visibility_cpu_cost +
            false_positive_cost;
        cost.run_cost = base_run_cost;
        if (bitmap_op == "OR")
        {
            const double or_penalty_ratio =
                0.04 + (false_positive_ratio * 0.50) +
                (std::clamp(params_.calibrated_duplicate_density, 0.0, 1.0) *
                 0.10);
            cost.run_cost *= 1.0 + or_penalty_ratio;
            appendFormulaTerm(cost,
                              "run.bitmap_or_penalty",
                              or_penalty_ratio,
                              base_run_cost,
                              cost.run_cost - base_run_cost,
                              "cost");
        }
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = heap_tuples;
        cost.row_width_bytes = row_width_bytes;
        return cost;
    }

    auto CostModel::costSummaryScan(uint64_t summary_pages_read,
                                    uint64_t candidate_heap_pages,
                                    uint64_t candidate_rows,
                                    double qual_cost,
                                    double unsummarized_range_fraction,
                                    double summary_staleness_fraction,
                                    core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating summary scan cost: summary_pages=" +
                     std::to_string(summary_pages_read) +
                     ", heap_pages=" + std::to_string(candidate_heap_pages) +
                     ", rows=" + std::to_string(candidate_rows));

        const double unsummarized_penalty =
            std::clamp(unsummarized_range_fraction, 0.0, 1.0);
        const double staleness_penalty =
            std::clamp(summary_staleness_fraction, 0.0, 1.0);

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "SUMMARY_SCAN");
        cost.row_width_bytes = safeRowWidthBytes(params_);
        appendInputEstimate(cost,
                            "summary_pages_read",
                            static_cast<double>(summary_pages_read),
                            "pages");
        appendInputEstimate(cost,
                            "candidate_heap_pages",
                            static_cast<double>(candidate_heap_pages),
                            "pages");
        appendInputEstimate(cost,
                            "candidate_rows",
                            static_cast<double>(candidate_rows),
                            "rows");
        appendInputEstimate(cost,
                            "qual_cost",
                            qual_cost,
                            "cpu_per_row");
        appendInputEstimate(cost,
                            "unsummarized_range_fraction",
                            unsummarized_penalty,
                            "ratio");
        appendInputEstimate(cost,
                            "summary_staleness_fraction",
                            staleness_penalty,
                            "ratio");

        cost.startup_cost =
            static_cast<double>(summary_pages_read) *
            params_.cpu_operator_cost * 0.25;
        appendFormulaTerm(cost,
                          "startup.summary_probe",
                          params_.cpu_operator_cost * 0.25,
                          static_cast<double>(summary_pages_read),
                          cost.startup_cost,
                          "cost");

        const double summary_io_cost =
            static_cast<double>(summary_pages_read) *
            params_.seq_page_cost * 0.60;
        const double heap_io_cost =
            static_cast<double>(candidate_heap_pages) * params_.seq_page_cost;
        const double tuple_cpu_cost =
            static_cast<double>(candidate_rows) * params_.cpu_tuple_cost;
        const double qual_cpu_cost =
            static_cast<double>(candidate_rows) * qual_cost;
        const double unsummarized_cost =
            static_cast<double>(candidate_heap_pages) *
            params_.random_page_cost * unsummarized_penalty * 0.50;
        const double stale_cost =
            static_cast<double>(candidate_rows) *
            params_.cpu_operator_cost * staleness_penalty;

        appendFormulaTerm(cost,
                          "run.summary_io",
                          params_.seq_page_cost * 0.60,
                          static_cast<double>(summary_pages_read),
                          summary_io_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.heap_candidate_io",
                          params_.seq_page_cost,
                          static_cast<double>(candidate_heap_pages),
                          heap_io_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.heap_tuple_cpu",
                          params_.cpu_tuple_cost,
                          static_cast<double>(candidate_rows),
                          tuple_cpu_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.recheck_cpu",
                          qual_cost,
                          static_cast<double>(candidate_rows),
                          qual_cpu_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.unsummarized_penalty",
                          params_.random_page_cost * 0.50,
                          static_cast<double>(candidate_heap_pages) *
                              unsummarized_penalty,
                          unsummarized_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.summary_staleness_penalty",
                          params_.cpu_operator_cost,
                          static_cast<double>(candidate_rows) *
                              staleness_penalty,
                          stale_cost,
                          "cost");

        cost.run_cost = summary_io_cost + heap_io_cost + tuple_cpu_cost +
                        qual_cpu_cost + unsummarized_cost + stale_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = candidate_rows;
        return cost;
    }

    auto CostModel::costBitmapStorageScan(uint64_t bitmap_pages_read,
                                          uint64_t candidate_heap_pages,
                                          uint64_t candidate_rows,
                                          double qual_cost,
                                          double lossy_container_fraction,
                                          double false_positive_ratio,
                                          core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating bitmap storage scan cost: bitmap_pages=" +
                     std::to_string(bitmap_pages_read) +
                     ", heap_pages=" + std::to_string(candidate_heap_pages) +
                     ", rows=" + std::to_string(candidate_rows));

        const double lossy_fraction =
            std::clamp(lossy_container_fraction, 0.0, 1.0);
        const double false_positive_fraction =
            std::clamp(false_positive_ratio, 0.0, 1.0);

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "BITMAP_STORAGE_SCAN");
        cost.row_width_bytes = safeRowWidthBytes(params_);
        appendInputEstimate(cost,
                            "bitmap_pages_read",
                            static_cast<double>(bitmap_pages_read),
                            "pages");
        appendInputEstimate(cost,
                            "candidate_heap_pages",
                            static_cast<double>(candidate_heap_pages),
                            "pages");
        appendInputEstimate(cost,
                            "candidate_rows",
                            static_cast<double>(candidate_rows),
                            "rows");
        appendInputEstimate(cost,
                            "qual_cost",
                            qual_cost,
                            "cpu_per_row");
        appendInputEstimate(cost,
                            "lossy_container_fraction",
                            lossy_fraction,
                            "ratio");
        appendInputEstimate(cost,
                            "false_positive_ratio",
                            false_positive_fraction,
                            "ratio");

        cost.startup_cost =
            static_cast<double>(bitmap_pages_read) *
            params_.cpu_operator_cost * 0.35;
        appendFormulaTerm(cost,
                          "startup.bitmap_probe",
                          params_.cpu_operator_cost * 0.35,
                          static_cast<double>(bitmap_pages_read),
                          cost.startup_cost,
                          "cost");

        const double bitmap_io_cost =
            static_cast<double>(bitmap_pages_read) *
            params_.random_page_cost * 0.75;
        const double heap_io_cost =
            static_cast<double>(candidate_heap_pages) *
            params_.seq_page_cost * 0.90;
        const double bitmap_cpu_cost =
            static_cast<double>(candidate_rows) *
            params_.cpu_index_tuple_cost;
        const double visibility_cpu_cost =
            static_cast<double>(candidate_rows) *
            params_.cpu_tuple_cost * 0.25;
        const double qual_cpu_cost =
            static_cast<double>(candidate_rows) * qual_cost;
        const double lossy_penalty_cost =
            static_cast<double>(candidate_rows) *
            params_.cpu_operator_cost *
            (lossy_fraction + false_positive_fraction);

        appendFormulaTerm(cost,
                          "run.bitmap_io",
                          params_.random_page_cost * 0.75,
                          static_cast<double>(bitmap_pages_read),
                          bitmap_io_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.heap_candidate_io",
                          params_.seq_page_cost * 0.90,
                          static_cast<double>(candidate_heap_pages),
                          heap_io_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.bitmap_cpu",
                          params_.cpu_index_tuple_cost,
                          static_cast<double>(candidate_rows),
                          bitmap_cpu_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.visibility_cpu",
                          params_.cpu_tuple_cost * 0.25,
                          static_cast<double>(candidate_rows),
                          visibility_cpu_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.recheck_cpu",
                          qual_cost,
                          static_cast<double>(candidate_rows),
                          qual_cpu_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.lossy_penalty",
                          params_.cpu_operator_cost,
                          static_cast<double>(candidate_rows) *
                              (lossy_fraction + false_positive_fraction),
                          lossy_penalty_cost,
                          "cost");

        cost.run_cost = bitmap_io_cost + heap_io_cost + bitmap_cpu_cost +
                        visibility_cpu_cost + qual_cpu_cost +
                        lossy_penalty_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = candidate_rows;
        return cost;
    }

    auto CostModel::costColumnstoreScan(uint64_t bytes_read_est,
                                        uint64_t rows_materialized,
                                        double qual_cost,
                                        double column_bytes_pruned_ratio,
                                        double late_materialization_gain_est,
                                        double delta_fraction,
                                        core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating columnstore scan cost: bytes_read=" +
                     std::to_string(bytes_read_est) +
                     ", rows=" + std::to_string(rows_materialized));

        const double bytes_pruned_ratio =
            std::clamp(column_bytes_pruned_ratio, 0.0, 1.0);
        const double late_materialization_gain =
            std::clamp(late_materialization_gain_est, 0.0, 1.0);
        const double delta_penalty =
            std::clamp(delta_fraction, 0.0, 1.0);
        const uint64_t bytes_per_page =
            std::max<uint64_t>(1024, params_.planner_page_size_bytes);
        const uint64_t column_pages_read =
            std::max<uint64_t>(1, ceilDivU64(bytes_read_est, bytes_per_page));

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "COLUMNSTORE_SCAN");
        cost.row_width_bytes = safeRowWidthBytes(params_);
        appendInputEstimate(cost,
                            "bytes_read_est",
                            static_cast<double>(bytes_read_est),
                            "bytes");
        appendInputEstimate(cost,
                            "rows_materialized",
                            static_cast<double>(rows_materialized),
                            "rows");
        appendInputEstimate(cost,
                            "qual_cost",
                            qual_cost,
                            "cpu_per_row");
        appendInputEstimate(cost,
                            "column_bytes_pruned_ratio",
                            bytes_pruned_ratio,
                            "ratio");
        appendInputEstimate(cost,
                            "late_materialization_gain_est",
                            late_materialization_gain,
                            "ratio");
        appendInputEstimate(cost,
                            "delta_fraction",
                            delta_penalty,
                            "ratio");

        cost.startup_cost =
            static_cast<double>(column_pages_read) *
            params_.cpu_operator_cost * 0.20;
        appendFormulaTerm(cost,
                          "startup.column_projection",
                          params_.cpu_operator_cost * 0.20,
                          static_cast<double>(column_pages_read),
                          cost.startup_cost,
                          "cost");

        const double column_io_cost =
            static_cast<double>(column_pages_read) *
            params_.seq_page_cost * 0.75;
        const double reconstruct_cpu_cost =
            static_cast<double>(rows_materialized) *
            params_.cpu_tuple_cost *
            std::max(0.25, 1.0 - (late_materialization_gain * 0.50));
        const double qual_cpu_cost =
            static_cast<double>(rows_materialized) * qual_cost;
        const double delta_cost =
            static_cast<double>(column_pages_read) *
            params_.random_page_cost * delta_penalty * 0.35;
        const double prune_credit =
            column_io_cost *
            std::clamp(bytes_pruned_ratio * late_materialization_gain,
                       0.0,
                       0.75);

        appendFormulaTerm(cost,
                          "run.column_io",
                          params_.seq_page_cost * 0.75,
                          static_cast<double>(column_pages_read),
                          column_io_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.reconstruct_cpu",
                          params_.cpu_tuple_cost *
                              std::max(0.25,
                                       1.0 -
                                           (late_materialization_gain * 0.50)),
                          static_cast<double>(rows_materialized),
                          reconstruct_cpu_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.qual_cpu",
                          qual_cost,
                          static_cast<double>(rows_materialized),
                          qual_cpu_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.delta_penalty",
                          params_.random_page_cost * 0.35,
                          static_cast<double>(column_pages_read) *
                              delta_penalty,
                          delta_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.prune_credit",
                          -params_.seq_page_cost * 0.75,
                          static_cast<double>(column_pages_read) *
                              std::clamp(bytes_pruned_ratio *
                                             late_materialization_gain,
                                         0.0,
                                         0.75),
                          -prune_credit,
                          "cost");

        cost.run_cost = std::max(
            0.0,
            column_io_cost + reconstruct_cpu_cost + qual_cpu_cost +
                delta_cost - prune_credit);
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = rows_materialized;
        return cost;
    }

    auto CostModel::costLSMScan(uint64_t num_levels, uint64_t avg_sstables_per_level,
                                 uint64_t index_tuples, uint64_t heap_pages,
                                 uint64_t heap_tuples, double qual_cost,
                                 double correlation, core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating LSM scan cost: num_levels=" + std::to_string(num_levels) +
                     ", avg_sstables_per_level=" + std::to_string(avg_sstables_per_level) +
                     ", heap_pages=" + std::to_string(heap_pages) +
                     ", correlation=" + std::to_string(correlation));

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "LSM_SCAN");
        const uint64_t row_width_bytes = safeRowWidthBytes(params_);
        const double heap_rows_per_page = calibratedHeapRowsPerPage(
            params_, row_width_bytes, heap_pages, heap_tuples);
        const double avg_probe_pages =
            std::max(1.0, calibratedProbePages(params_, num_levels));
        const double false_positive_ratio =
            params_.calibrated_false_positive_ratio > 0.0
                ? std::clamp(params_.calibrated_false_positive_ratio, 0.0, 1.0)
                : 0.30;
        const double visibility_cpu_cost =
            static_cast<double>(heap_tuples) * calibratedVisibilityTupleCost(params_);
        appendInputEstimate(cost, "num_levels", static_cast<double>(num_levels), "count");
        appendInputEstimate(cost,
                            "avg_sstables_per_level",
                            static_cast<double>(avg_sstables_per_level),
                            "count");
        appendInputEstimate(cost, "index_tuples", static_cast<double>(index_tuples), "rows");
        appendInputEstimate(cost, "heap_pages", static_cast<double>(heap_pages), "pages");
        appendInputEstimate(cost, "heap_tuples", static_cast<double>(heap_tuples), "rows");
        appendInputEstimate(cost, "qual_cost", qual_cost, "cpu_per_row");
        appendInputEstimate(cost, "correlation", correlation, "ratio");
        appendInputEstimate(cost,
                            "row_width_bytes",
                            static_cast<double>(row_width_bytes),
                            "bytes");
        appendInputEstimate(cost,
                            "heap_rows_per_page",
                            heap_rows_per_page,
                            "rows_per_page");
        appendInputEstimate(cost,
                            "avg_probe_pages",
                            avg_probe_pages,
                            "pages");
        appendInputEstimate(cost,
                            "false_positive_ratio",
                            false_positive_ratio,
                            "ratio");

        // Startup cost: LSM-Tree read path
        // 1. Check memtable (in-memory, very cheap)
        double memtable_cost = 10.0 * params_.cpu_operator_cost;  // ~10 comparisons for RB-tree lookup

        // 2. Check immutable memtable (in-memory, very cheap)
        double immutable_memtable_cost = 10.0 * params_.cpu_operator_cost;

        // 3. Bloom filter checks for each SSTable level (CPU only, no I/O if negative)
        // Each Bloom filter check is ~3-5 hash computations
        uint64_t total_sstables = num_levels * avg_sstables_per_level;
        double bloom_filter_cost = static_cast<double>(total_sstables) * 4.0 * params_.cpu_operator_cost;

        cost.startup_cost = memtable_cost + immutable_memtable_cost + bloom_filter_cost;
        appendFormulaTerm(cost,
                          "startup.memtable_lookup",
                          params_.cpu_operator_cost,
                          10.0,
                          memtable_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "startup.immutable_memtable_lookup",
                          params_.cpu_operator_cost,
                          10.0,
                          immutable_memtable_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "startup.bloom_filter_checks",
                          4.0 * params_.cpu_operator_cost,
                          static_cast<double>(total_sstables),
                          bloom_filter_cost,
                          "cost");

        // Index scan cost: read SSTable pages
        // Key insight: LSM-Tree may need to read from multiple SSTables per level
        // Each SSTable requires random page reads (B-Tree within SSTable)
        // Bloom filters reduce false positives, assume 30% false positive rate
        double expected_sstable_reads =
            static_cast<double>(total_sstables) * false_positive_ratio;

        // Each SSTable read: 1-2 pages for internal B-Tree (small index within SSTable)
        uint64_t estimated_index_pages = static_cast<uint64_t>(
            std::ceil(expected_sstable_reads * avg_probe_pages));

        double index_io_cost = static_cast<double>(estimated_index_pages) * params_.random_page_cost;
        double index_cpu_cost = static_cast<double>(index_tuples) * params_.cpu_index_tuple_cost;
        appendInputEstimate(cost,
                            "estimated_index_pages",
                            static_cast<double>(estimated_index_pages),
                            "pages");

        // OPT-M7: Additional CPU cost for k-way merge if range scan
        // K-way merge is needed when range scans must merge results from multiple SSTables
        // Cost: O(n * log(k)) where n = tuples, k = number of SSTables to merge
        // For point lookups (index_tuples <= 1), no merge needed
        double merge_cost = 0.0;
        if (index_tuples > 1 && total_sstables > 1)
        {
            // K-way merge CPU cost: each tuple requires log(k) comparisons to find next
            // in the merge heap, where k = number of overlapping SSTables
            double log_k = std::log2(static_cast<double>(total_sstables));
            merge_cost = static_cast<double>(index_tuples) * log_k * params_.cpu_operator_cost;

            DEBUG_LOG_DB("LSM merge cost for " + std::to_string(index_tuples) +
                         " tuples across " + std::to_string(total_sstables) +
                         " SSTables: " + std::to_string(merge_cost));
        }

        // Heap fetch cost: same as B-Tree
        const double sequential_heap_pages =
            std::ceil(static_cast<double>(heap_tuples) / heap_rows_per_page);
        const double random_heap_pages =
            std::min(static_cast<double>(heap_tuples),
                     static_cast<double>(heap_pages));
        const double correlation_weight =
            std::clamp(std::abs(correlation), 0.0, 1.0);
        double effective_heap_pages =
            (sequential_heap_pages * correlation_weight) +
            (random_heap_pages * (1.0 - correlation_weight));
        effective_heap_pages = std::max(1.0, effective_heap_pages);

        double effective_random_cost = effectiveRandomPageCost(heap_pages);
        double heap_io_cost = effective_heap_pages * effective_random_cost;
        double heap_cpu_cost = static_cast<double>(heap_tuples) * params_.cpu_tuple_cost +
                               static_cast<double>(heap_tuples) * qual_cost;
        appendInputEstimate(cost, "effective_heap_pages", effective_heap_pages, "pages");
        appendInputEstimate(cost,
                            "effective_random_page_cost",
                            effective_random_cost,
                            "cost_per_page");
        appendFormulaTerm(cost,
                          "run.sstable_io",
                          params_.random_page_cost,
                          static_cast<double>(estimated_index_pages),
                          index_io_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.index_tuple_cpu",
                          params_.cpu_index_tuple_cost,
                          static_cast<double>(index_tuples),
                          index_cpu_cost,
                          "cost");
        if (merge_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "run.sstable_merge_cpu",
                              params_.cpu_operator_cost,
                              static_cast<double>(index_tuples) *
                                  std::log2(static_cast<double>(total_sstables)),
                              merge_cost,
                              "cost");
        }
        appendFormulaTerm(cost,
                          "run.heap_io",
                          effective_random_cost,
                          effective_heap_pages,
                          heap_io_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.heap_tuple_cpu",
                          params_.cpu_tuple_cost + qual_cost,
                          static_cast<double>(heap_tuples),
                          heap_cpu_cost,
                          "cost");
        if (visibility_cpu_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "run.heap_visibility_cpu",
                              calibratedVisibilityTupleCost(params_),
                              static_cast<double>(heap_tuples),
                              visibility_cpu_cost,
                              "cost");
        }

        cost.run_cost = index_io_cost + index_cpu_cost + merge_cost +
                        heap_io_cost + heap_cpu_cost + visibility_cpu_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = heap_tuples;
        cost.row_width_bytes = row_width_bytes;

        DEBUG_LOG_DB("LSMScan cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     " (index_io=" + std::to_string(index_io_cost) +
                     ", merge=" + std::to_string(merge_cost) +
                     ", heap_io=" + std::to_string(heap_io_cost) +
                     ", bloom_checks=" + std::to_string(bloom_filter_cost) + ")");

        return cost;
    }

    auto CostModel::effectiveRandomPageCost(uint64_t table_pages) const -> double
    {
        if (table_pages == 0)
        {
            return params_.random_page_cost;
        }

        // Calculate cache hit ratio
        // If entire table fits in cache (effective_cache_size > table_pages),
        // cache hit ratio = 1.0 (all accesses are cache hits)
        double cache_hit_ratio = std::min(1.0, params_.effective_cache_size /
                                                   static_cast<double>(table_pages));

        // Blend random and sequential costs based on cache hit ratio
        // When cache_hit_ratio = 1.0 (fully cached), cost = seq_page_cost (fast)
        // When cache_hit_ratio = 0.0 (not cached), cost = random_page_cost (slow)
        double effective_cost = params_.random_page_cost * (1.0 - cache_hit_ratio) +
                                params_.seq_page_cost * cache_hit_ratio;

        DEBUG_LOG_DB("Effective random page cost for " + std::to_string(table_pages) +
                     " pages: " + std::to_string(effective_cost) +
                     " (cache_hit_ratio=" + std::to_string(cache_hit_ratio) + ")");

        return effective_cost;
    }

    auto CostModel::operatorCost(const std::string &op) const -> double
    {
        // Simple comparison operators: cheapest
        if (op == "=" || op == "!=" || op == "<" || op == ">" ||
            op == "<=" || op == ">=")
        {
            return params_.cpu_operator_cost;
        }

        // Arithmetic operators: cheap
        if (op == "+" || op == "-")
        {
            return params_.cpu_operator_cost;
        }

        // Multiplication and division: slightly more expensive
        if (op == "*" || op == "/" || op == "%")
        {
            return params_.cpu_operator_cost * 2.0;
        }

        // String operations: expensive (pattern matching)
        if (op == "LIKE" || op == "ILIKE" || op == "~" || op == "~*")
        {
            return params_.cpu_operator_cost * 10.0;
        }

        // IN operator: cost depends on list size, but estimate moderate cost
        if (op == "IN" || op == "NOT IN")
        {
            return params_.cpu_operator_cost * 5.0;
        }

        // BETWEEN: two comparisons
        if (op == "BETWEEN")
        {
            return params_.cpu_operator_cost * 2.0;
        }

        // IS NULL / IS NOT NULL: very cheap (just a flag check)
        if (op == "IS NULL" || op == "IS NOT NULL")
        {
            return params_.cpu_operator_cost * 0.5;
        }

        // Logical operators: combine costs of subexpressions
        if (op == "AND" || op == "OR" || op == "NOT")
        {
            return params_.cpu_operator_cost * 0.5;
        }

        // String functions: moderately expensive
        if (op == "substr" || op == "substring" || op == "concat" ||
            op == "lower" || op == "upper" || op == "trim")
        {
            return params_.cpu_operator_cost * 5.0;
        }

        // Aggregate functions: expensive (but amortized over groups)
        if (op == "sum" || op == "avg" || op == "count" ||
            op == "min" || op == "max")
        {
            return params_.cpu_operator_cost * 3.0;
        }

        // Math functions: moderately expensive
        if (op == "abs" || op == "round" || op == "floor" || op == "ceil" ||
            op == "sqrt" || op == "exp" || op == "log" || op == "pow")
        {
            return params_.cpu_operator_cost * 3.0;
        }

        // Default: use base cpu_operator_cost
        DEBUG_LOG_DB("Unknown operator '" + op + "', using default cost");
        return params_.cpu_operator_cost;
    }

    auto CostModel::costNestedLoopJoin(const CostEstimate& outer_cost,
                                       const CostEstimate& inner_cost,
                                       uint64_t outer_rows,
                                       uint64_t inner_rows,
                                       double selectivity,
                                       parser::JoinType join_type,
                                       core::ErrorContext* ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating nested loop join cost: outer_rows=" + std::to_string(outer_rows) +
                     ", inner_rows=" + std::to_string(inner_rows) +
                     ", selectivity=" + std::to_string(selectivity));

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "NESTED_LOOP_JOIN");
        const uint64_t outer_row_width =
            outer_cost.row_width_bytes == 0
                ? safeRowWidthBytes(params_)
                : outer_cost.row_width_bytes;
        const uint64_t inner_row_width =
            inner_cost.row_width_bytes == 0
                ? safeRowWidthBytes(params_)
                : inner_cost.row_width_bytes;
        appendInputEstimate(cost, "outer_rows", static_cast<double>(outer_rows), "rows");
        appendInputEstimate(cost, "inner_rows", static_cast<double>(inner_rows), "rows");
        appendInputEstimate(cost, "selectivity", selectivity, "ratio");
        appendInputEstimate(cost,
                            "outer_row_width_bytes",
                            static_cast<double>(outer_row_width),
                            "bytes");
        appendInputEstimate(cost,
                            "inner_row_width_bytes",
                            static_cast<double>(inner_row_width),
                            "bytes");

        // Startup cost: just the outer relation startup
        // Inner relation is re-scanned for each outer row, so no one-time startup
        cost.startup_cost = outer_cost.startup_cost;
        if (cost.startup_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "startup.outer_startup",
                              1.0,
                              outer_cost.startup_cost,
                              cost.startup_cost,
                              "cost");
        }

        // Run cost breakdown:
        // 1. Scan outer relation completely
        double outer_scan_cost = outer_cost.run_cost;

        // 2. For each outer row, scan inner relation (inner is re-scanned outer_rows times)
        //    Note: inner_cost.total_cost includes both startup and run for each inner scan
        double inner_scan_cost = static_cast<double>(outer_rows) * inner_cost.total_cost;

        // 3. CPU cost of evaluating join condition and materializing output
        //    Join produces: outer_rows * inner_rows * selectivity output rows
        uint64_t output_rows = 0;
        if (join_type == parser::JoinType::CROSS)
        {
            output_rows = outer_rows * inner_rows;
        }
        else
        {
            output_rows = static_cast<uint64_t>(
                static_cast<double>(outer_rows) * static_cast<double>(inner_rows) * selectivity);
        }
        if (outer_rows > 0 && inner_rows > 0 && output_rows == 0 &&
            join_type != parser::JoinType::LEFT &&
            join_type != parser::JoinType::RIGHT &&
            join_type != parser::JoinType::FULL)
        {
            output_rows = 1;
        }
        if (join_type == parser::JoinType::LEFT)
        {
            output_rows = std::max<uint64_t>(outer_rows, output_rows);
        }
        else if (join_type == parser::JoinType::RIGHT)
        {
            output_rows = std::max<uint64_t>(inner_rows, output_rows);
        }
        else if (join_type == parser::JoinType::FULL)
        {
            output_rows = std::max<uint64_t>(std::max<uint64_t>(outer_rows, inner_rows),
                                             output_rows);
        }

        // Cost of evaluating join condition for each combination
        // (before selectivity filtering)
        double join_qual_cost = static_cast<double>(outer_rows) *
                               static_cast<double>(inner_rows) *
                               params_.cpu_operator_cost;

        // Cost of materializing output tuples (after selectivity filtering)
        const double output_row_scale =
            std::clamp(static_cast<double>(outer_row_width + inner_row_width) /
                           static_cast<double>(safeRowWidthBytes(params_)),
                       1.0,
                       8.0);
        double output_cost =
            static_cast<double>(output_rows) * params_.cpu_tuple_cost *
            output_row_scale;

        appendFormulaTerm(cost,
                          "run.outer_scan",
                          1.0,
                          outer_scan_cost,
                          outer_scan_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.inner_rescan",
                          static_cast<double>(outer_rows),
                          inner_cost.total_cost,
                          inner_scan_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.join_qual",
                          params_.cpu_operator_cost,
                          static_cast<double>(outer_rows) *
                              static_cast<double>(inner_rows),
                          join_qual_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.output_materialization",
                          params_.cpu_tuple_cost,
                          static_cast<double>(output_rows),
                          output_cost,
                          "cost");
        cost.run_cost = outer_scan_cost + inner_scan_cost + join_qual_cost + output_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = output_rows;
        cost.row_width_bytes = outer_row_width + inner_row_width;

        DEBUG_LOG_DB("NestedLoopJoin cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     ", output_rows=" + std::to_string(cost.rows) +
                     " (outer_scan=" + std::to_string(outer_scan_cost) +
                     ", inner_scan=" + std::to_string(inner_scan_cost) +
                     ", join_qual=" + std::to_string(join_qual_cost) + ")");

        return cost;
    }

    auto CostModel::costHashJoin(const CostEstimate& outer_cost,
                                 const CostEstimate& inner_cost,
                                 uint64_t outer_rows,
                                 uint64_t inner_rows,
                                 double selectivity,
                                 parser::JoinType join_type,
                                 core::ErrorContext* ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating hash join cost: outer_rows=" + std::to_string(outer_rows) +
                     ", inner_rows=" + std::to_string(inner_rows) +
                     ", selectivity=" + std::to_string(selectivity));

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "HASH_JOIN");
        const uint64_t outer_row_width =
            outer_cost.row_width_bytes == 0
                ? safeRowWidthBytes(params_)
                : outer_cost.row_width_bytes;
        const uint64_t inner_row_width =
            inner_cost.row_width_bytes == 0
                ? safeRowWidthBytes(params_)
                : inner_cost.row_width_bytes;
        appendInputEstimate(cost, "outer_rows", static_cast<double>(outer_rows), "rows");
        appendInputEstimate(cost, "inner_rows", static_cast<double>(inner_rows), "rows");
        appendInputEstimate(cost, "selectivity", selectivity, "ratio");
        appendInputEstimate(cost,
                            "outer_row_width_bytes",
                            static_cast<double>(outer_row_width),
                            "bytes");
        appendInputEstimate(cost,
                            "inner_row_width_bytes",
                            static_cast<double>(inner_row_width),
                            "bytes");

        // Hash join phases:
        // Phase 1 (Build): Scan outer relation and build hash table
        // Phase 2 (Probe): Scan inner relation and probe hash table

        // Build phase cost:
        // 1. Scan outer relation completely
        double outer_scan_cost = outer_cost.total_cost;

        // 2. Build hash table from outer rows
        //    Hash insertion: compute hash + insert into bucket
        //    Factor of 2.0 represents hash function computation + insertion overhead
        const double build_row_scale =
            std::clamp(static_cast<double>(outer_row_width) /
                           static_cast<double>(safeRowWidthBytes(params_)),
                       0.5,
                       4.0);
        const double HASH_BUILD_FACTOR = 1.25 + (build_row_scale * 0.75);
        double hash_build_cost = static_cast<double>(outer_rows) *
                                params_.cpu_tuple_cost * HASH_BUILD_FACTOR;

        const uint64_t hash_row_width =
            outer_row_width + params_.hash_tuple_overhead_bytes;
        const SpillEstimate spill = estimateSpill(
            params_,
            outer_rows * hash_row_width,
            static_cast<uint64_t>(static_cast<double>(params_.work_mem_bytes) *
                                  params_.hash_mem_multiplier),
            outer_rows + inner_rows,
            1.25);

        // Startup cost = build entire hash table
        cost.startup_cost =
            outer_scan_cost + hash_build_cost + spill.io_cost * 0.5 + spill.cpu_cost * 0.5;
        appendFormulaTerm(cost,
                          "startup.outer_scan",
                          1.0,
                          outer_scan_cost,
                          outer_scan_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "startup.hash_build_cpu",
                          params_.cpu_tuple_cost * HASH_BUILD_FACTOR,
                          static_cast<double>(outer_rows),
                          hash_build_cost,
                          "cost");
        if (spill.io_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "startup.hash_spill_io",
                              0.5,
                              spill.io_cost,
                              spill.io_cost * 0.5,
                              "cost");
        }
        if (spill.cpu_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "startup.hash_spill_cpu",
                              0.5,
                              spill.cpu_cost,
                              spill.cpu_cost * 0.5,
                              "cost");
        }

        // Probe phase cost:
        // 1. Scan inner relation completely
        double inner_scan_cost = inner_cost.total_cost;

        // 2. Probe hash table for each inner row
        //    Hash lookup: compute hash + traverse bucket chain
        //    Factor of 1.5 represents hash function computation + lookup overhead
        const double probe_row_scale =
            std::clamp(static_cast<double>(inner_row_width) /
                           static_cast<double>(safeRowWidthBytes(params_)),
                       0.5,
                       4.0);
        const double HASH_PROBE_FACTOR = 1.0 + (probe_row_scale * 0.50);
        double hash_probe_cost = static_cast<double>(inner_rows) *
                                params_.cpu_tuple_cost * HASH_PROBE_FACTOR;

        // 3. CPU cost of evaluating join condition and materializing output
        uint64_t output_rows = 0;
        if (join_type == parser::JoinType::CROSS)
        {
            output_rows = outer_rows * inner_rows;
        }
        else
        {
            output_rows = static_cast<uint64_t>(
                static_cast<double>(outer_rows) * static_cast<double>(inner_rows) * selectivity);
        }
        if (outer_rows > 0 && inner_rows > 0 && output_rows == 0 &&
            join_type != parser::JoinType::LEFT &&
            join_type != parser::JoinType::RIGHT &&
            join_type != parser::JoinType::FULL)
        {
            output_rows = 1;
        }
        if (join_type == parser::JoinType::LEFT)
        {
            output_rows = std::max<uint64_t>(outer_rows, output_rows);
        }
        else if (join_type == parser::JoinType::RIGHT)
        {
            output_rows = std::max<uint64_t>(inner_rows, output_rows);
        }
        else if (join_type == parser::JoinType::FULL)
        {
            output_rows = std::max<uint64_t>(std::max<uint64_t>(outer_rows, inner_rows),
                                             output_rows);
        }

        // For hash join, we only evaluate join condition for matching hash buckets
        // (much cheaper than nested loop which evaluates for all combinations)
        // Estimate: evaluate for ~10% of combinations (hash collisions + matches)
        double join_qual_cost = static_cast<double>(outer_rows) *
                               static_cast<double>(inner_rows) *
                               selectivity * 10.0 *  // hash collision factor
                               params_.cpu_operator_cost;

        // Cost of materializing output tuples
        double output_cost = static_cast<double>(output_rows) * params_.cpu_tuple_cost;

        appendFormulaTerm(cost,
                          "run.inner_scan",
                          1.0,
                          inner_scan_cost,
                          inner_scan_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.hash_probe_cpu",
                          params_.cpu_tuple_cost * HASH_PROBE_FACTOR,
                          static_cast<double>(inner_rows),
                          hash_probe_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.join_qual",
                          params_.cpu_operator_cost,
                          static_cast<double>(outer_rows) *
                              static_cast<double>(inner_rows) * selectivity * 10.0,
                          join_qual_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.output_materialization",
                          params_.cpu_tuple_cost,
                          static_cast<double>(output_rows),
                          output_cost,
                          "cost");
        if (spill.io_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "run.hash_spill_io",
                              0.5,
                              spill.io_cost,
                              spill.io_cost * 0.5,
                              "cost");
        }
        if (spill.cpu_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "run.hash_spill_cpu",
                              0.5,
                              spill.cpu_cost,
                              spill.cpu_cost * 0.5,
                              "cost");
        }
        cost.run_cost = inner_scan_cost + hash_probe_cost + join_qual_cost +
                        output_cost + spill.io_cost * 0.5 + spill.cpu_cost * 0.5;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = output_rows;
        cost.row_width_bytes = outer_row_width + inner_row_width;
        applyResourceEstimate(cost, spill);

        DEBUG_LOG_DB("HashJoin cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     ", output_rows=" + std::to_string(cost.rows) +
                     " (hash_build=" + std::to_string(hash_build_cost) +
                     ", hash_probe=" + std::to_string(hash_probe_cost) +
                     ", join_qual=" + std::to_string(join_qual_cost) +
                     ", spill=" + std::to_string(cost.spill_expected ? 1 : 0) + ")");

        return cost;
    }

    auto CostModel::costMergeJoin(const CostEstimate& outer_cost,
                                  const CostEstimate& inner_cost,
                                  uint64_t outer_rows,
                                  uint64_t inner_rows,
                                  double selectivity,
                                  bool outer_presorted,
                                  bool inner_presorted,
                                  parser::JoinType join_type,
                                  core::ErrorContext* ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating merge join cost: outer_rows=" + std::to_string(outer_rows) +
                     ", inner_rows=" + std::to_string(inner_rows) +
                     ", selectivity=" + std::to_string(selectivity) +
                     ", outer_presorted=" + std::to_string(outer_presorted) +
                     ", inner_presorted=" + std::to_string(inner_presorted));

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "MERGE_JOIN");
        const uint64_t outer_row_width =
            outer_cost.row_width_bytes == 0
                ? safeRowWidthBytes(params_)
                : outer_cost.row_width_bytes;
        const uint64_t inner_row_width =
            inner_cost.row_width_bytes == 0
                ? safeRowWidthBytes(params_)
                : inner_cost.row_width_bytes;
        appendInputEstimate(cost, "outer_rows", static_cast<double>(outer_rows), "rows");
        appendInputEstimate(cost, "inner_rows", static_cast<double>(inner_rows), "rows");
        appendInputEstimate(cost, "selectivity", selectivity, "ratio");
        appendInputEstimate(cost,
                            "outer_presorted",
                            outer_presorted ? 1.0 : 0.0,
                            "flag");
        appendInputEstimate(cost,
                            "inner_presorted",
                            inner_presorted ? 1.0 : 0.0,
                            "flag");
        appendInputEstimate(cost,
                            "outer_row_width_bytes",
                            static_cast<double>(outer_row_width),
                            "bytes");
        appendInputEstimate(cost,
                            "inner_row_width_bytes",
                            static_cast<double>(inner_row_width),
                            "bytes");

        CostEstimate outer_sort_cost;
        CostEstimate inner_sort_cost;
        if (!outer_presorted)
        {
            outer_sort_cost = costSort(outer_rows, outer_row_width, 1, ctx);
        }
        if (!inner_presorted)
        {
            inner_sort_cost = costSort(inner_rows, inner_row_width, 1, ctx);
        }

        cost.startup_cost =
            outer_cost.total_cost +
            inner_cost.total_cost +
            outer_sort_cost.total_cost +
            inner_sort_cost.total_cost;
        appendFormulaTerm(cost,
                          "startup.outer_input",
                          1.0,
                          outer_cost.total_cost,
                          outer_cost.total_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "startup.inner_input",
                          1.0,
                          inner_cost.total_cost,
                          inner_cost.total_cost,
                          "cost");
        if (outer_sort_cost.total_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "startup.outer_sort",
                              1.0,
                              outer_sort_cost.total_cost,
                              outer_sort_cost.total_cost,
                              "cost");
        }
        if (inner_sort_cost.total_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "startup.inner_sort",
                              1.0,
                              inner_sort_cost.total_cost,
                              inner_sort_cost.total_cost,
                              "cost");
        }

        uint64_t output_rows = 0;
        if (join_type == parser::JoinType::CROSS)
        {
            output_rows = outer_rows * inner_rows;
        }
        else
        {
            output_rows = static_cast<uint64_t>(
                static_cast<double>(outer_rows) * static_cast<double>(inner_rows) * selectivity);
        }
        if (outer_rows > 0 && inner_rows > 0 && output_rows == 0 &&
            join_type != parser::JoinType::LEFT &&
            join_type != parser::JoinType::RIGHT &&
            join_type != parser::JoinType::FULL)
        {
            output_rows = 1;
        }
        if (join_type == parser::JoinType::LEFT)
        {
            output_rows = std::max<uint64_t>(outer_rows, output_rows);
        }
        else if (join_type == parser::JoinType::RIGHT)
        {
            output_rows = std::max<uint64_t>(inner_rows, output_rows);
        }
        else if (join_type == parser::JoinType::FULL)
        {
            output_rows = std::max<uint64_t>(std::max<uint64_t>(outer_rows, inner_rows),
                                             output_rows);
        }

        constexpr double MERGE_COMPARE_FACTOR = 1.2;
        double merge_compare_cost =
            static_cast<double>(outer_rows + inner_rows) *
            params_.cpu_operator_cost *
            MERGE_COMPARE_FACTOR;
        double output_cost = static_cast<double>(output_rows) * params_.cpu_tuple_cost;

        const uint64_t merge_buffer_bytes =
            std::max<uint64_t>(1, outer_rows + inner_rows) *
            std::max<uint64_t>(16,
                               ((outer_row_width + inner_row_width) / 2) +
                                   (params_.hash_tuple_overhead_bytes / 2));
        const SpillEstimate merge_buffer = estimateSpill(
            params_,
            merge_buffer_bytes,
            static_cast<uint64_t>(static_cast<double>(params_.work_mem_bytes) *
                                  params_.merge_mem_multiplier),
            outer_rows + inner_rows,
            0.25);

        appendFormulaTerm(cost,
                          "run.merge_compare_cpu",
                          params_.cpu_operator_cost * MERGE_COMPARE_FACTOR,
                          static_cast<double>(outer_rows + inner_rows),
                          merge_compare_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.output_materialization",
                          params_.cpu_tuple_cost,
                          static_cast<double>(output_rows),
                          output_cost,
                          "cost");
        if (merge_buffer.io_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "run.merge_buffer_io",
                              1.0,
                              merge_buffer.io_cost,
                              merge_buffer.io_cost,
                              "cost");
        }
        if (merge_buffer.cpu_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "run.merge_buffer_cpu",
                              1.0,
                              merge_buffer.cpu_cost,
                              merge_buffer.cpu_cost,
                              "cost");
        }
        cost.run_cost = merge_compare_cost + output_cost +
                        merge_buffer.io_cost + merge_buffer.cpu_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = output_rows;
        cost.memory_bytes = std::max<uint64_t>(
            merge_buffer.working_set_bytes,
            std::max<uint64_t>(outer_sort_cost.memory_bytes, inner_sort_cost.memory_bytes));
        cost.memory_budget_bytes = std::max<uint64_t>(
            merge_buffer.budget_bytes,
            std::max<uint64_t>(outer_sort_cost.memory_budget_bytes,
                               inner_sort_cost.memory_budget_bytes));
        cost.spill_expected = outer_sort_cost.spill_expected ||
                              inner_sort_cost.spill_expected ||
                              merge_buffer.spill_expected;
        cost.spill_passes = std::max<uint32_t>(
            merge_buffer.spill_passes,
            std::max<uint32_t>(outer_sort_cost.spill_passes,
                               inner_sort_cost.spill_passes));
        cost.spill_bytes =
            merge_buffer.spill_bytes + outer_sort_cost.spill_bytes +
            inner_sort_cost.spill_bytes;
        cost.row_width_bytes = outer_row_width + inner_row_width;
        cost.resource_governance_outcome = cost.spill_expected
            ? "SPILL_EXPECTED"
            : (cost.memory_bytes > 0 ? "IN_MEMORY" : "NO_MEMORY_GOVERNANCE");

        DEBUG_LOG_DB("MergeJoin cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     ", output_rows=" + std::to_string(cost.rows) +
                     " (outer_sort=" + std::to_string(outer_sort_cost.total_cost) +
                     ", inner_sort=" + std::to_string(inner_sort_cost.total_cost) +
                     ", merge_compare=" + std::to_string(merge_compare_cost) +
                     ", spill=" + std::to_string(cost.spill_expected ? 1 : 0) + ")");

        return cost;
    }

    auto CostModel::costAggregate(uint64_t input_rows,
                                  uint64_t num_groups,
                                  uint64_t num_aggregates,
                                  core::ErrorContext* ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating aggregate cost: input_rows=" + std::to_string(input_rows) +
                     ", num_groups=" + std::to_string(num_groups) +
                     ", num_aggregates=" + std::to_string(num_aggregates));

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "HASH_AGGREGATE");
        cost.row_width_bytes = safeRowWidthBytes(params_);
        appendInputEstimate(cost, "input_rows", static_cast<double>(input_rows), "rows");
        appendInputEstimate(cost, "num_groups", static_cast<double>(num_groups), "rows");
        appendInputEstimate(cost,
                            "num_aggregates",
                            static_cast<double>(num_aggregates),
                            "count");

        // Hash-based aggregation uses a hash table to group rows
        // For simple aggregation (no GROUP BY), num_groups = 1

        // Startup cost: build hash table and compute aggregates
        // Each input row is hashed and inserted into hash table
        // Factor of 2.0 represents hash computation + hash table insertion
        constexpr double HASH_AGG_FACTOR = 2.0;
        double hash_build_cost = static_cast<double>(input_rows) *
                                params_.cpu_tuple_cost * HASH_AGG_FACTOR;

        const uint64_t aggregate_row_width =
            params_.default_row_width_bytes +
            (std::max<uint64_t>(1, num_aggregates) * sizeof(double)) +
            params_.hash_tuple_overhead_bytes;
        const SpillEstimate spill = estimateSpill(
            params_,
            std::max<uint64_t>(1, num_groups) * aggregate_row_width,
            static_cast<uint64_t>(static_cast<double>(params_.work_mem_bytes) *
                                  params_.aggregate_mem_multiplier),
            input_rows,
            1.1);

        cost.startup_cost = hash_build_cost + spill.io_cost * 0.5 + spill.cpu_cost * 0.5;
        appendFormulaTerm(cost,
                          "startup.hash_build_cpu",
                          params_.cpu_tuple_cost * HASH_AGG_FACTOR,
                          static_cast<double>(input_rows),
                          hash_build_cost,
                          "cost");
        if (spill.io_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "startup.aggregate_spill_io",
                              0.5,
                              spill.io_cost,
                              spill.io_cost * 0.5,
                              "cost");
        }
        if (spill.cpu_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "startup.aggregate_spill_cpu",
                              0.5,
                              spill.cpu_cost,
                              spill.cpu_cost * 0.5,
                              "cost");
        }

        // Run cost: finalize aggregates for each group
        // For each group, we need to compute final aggregate values
        // (e.g., AVG = sum/count, finalize accumulators)
        double finalize_cost = static_cast<double>(num_groups) *
                              static_cast<double>(num_aggregates) *
                              params_.cpu_operator_cost;

        // Output cost: materialize result rows
        double output_cost = static_cast<double>(num_groups) * params_.cpu_tuple_cost;

        appendFormulaTerm(cost,
                          "run.finalize_cpu",
                          params_.cpu_operator_cost,
                          static_cast<double>(num_groups) *
                              static_cast<double>(num_aggregates),
                          finalize_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.output_materialization",
                          params_.cpu_tuple_cost,
                          static_cast<double>(num_groups),
                          output_cost,
                          "cost");
        if (spill.io_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "run.aggregate_spill_io",
                              0.5,
                              spill.io_cost,
                              spill.io_cost * 0.5,
                              "cost");
        }
        if (spill.cpu_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "run.aggregate_spill_cpu",
                              0.5,
                              spill.cpu_cost,
                              spill.cpu_cost * 0.5,
                              "cost");
        }
        cost.run_cost = finalize_cost + output_cost + spill.io_cost * 0.5 + spill.cpu_cost * 0.5;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = num_groups;
        applyResourceEstimate(cost, spill);

        DEBUG_LOG_DB("Aggregate cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     ", output_rows=" + std::to_string(cost.rows) +
                     " (hash_build=" + std::to_string(hash_build_cost) +
                     ", finalize=" + std::to_string(finalize_cost) + ")");

        return cost;
    }

    auto CostModel::costGroupAggregate(uint64_t input_rows,
                                       uint64_t num_groups,
                                       uint64_t num_aggregates,
                                       core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating group aggregate cost: input_rows=" +
                     std::to_string(input_rows) + ", num_groups=" +
                     std::to_string(num_groups) + ", num_aggregates=" +
                     std::to_string(num_aggregates));

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "GROUP_AGGREGATE");
        cost.row_width_bytes = safeRowWidthBytes(params_);
        appendInputEstimate(cost, "input_rows", static_cast<double>(input_rows), "rows");
        appendInputEstimate(cost, "num_groups", static_cast<double>(num_groups), "rows");
        appendInputEstimate(cost,
                            "num_aggregates",
                            static_cast<double>(num_aggregates),
                            "count");
        const double transition_cost =
            static_cast<double>(input_rows) *
            (params_.cpu_tuple_cost * 0.35 +
             static_cast<double>(std::max<uint64_t>(1, num_aggregates)) *
                 params_.cpu_operator_cost * 0.15);
        const double group_boundary_cost =
            static_cast<double>(input_rows) * params_.cpu_operator_cost * 0.25;
        const double finalize_cost =
            static_cast<double>(num_groups) *
            static_cast<double>(num_aggregates) *
            params_.cpu_operator_cost;
        const double output_cost =
            static_cast<double>(num_groups) * params_.cpu_tuple_cost;

        cost.startup_cost = static_cast<double>(input_rows) *
                            params_.cpu_tuple_cost * 0.05;
        appendFormulaTerm(cost,
                          "startup.group_stream_setup",
                          params_.cpu_tuple_cost * 0.05,
                          static_cast<double>(input_rows),
                          cost.startup_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.transition_cpu",
                          1.0,
                          transition_cost,
                          transition_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.group_boundary_cpu",
                          1.0,
                          group_boundary_cost,
                          group_boundary_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.finalize_cpu",
                          1.0,
                          finalize_cost,
                          finalize_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.output_materialization",
                          params_.cpu_tuple_cost,
                          static_cast<double>(num_groups),
                          output_cost,
                          "cost");
        cost.run_cost = transition_cost + group_boundary_cost + finalize_cost +
                        output_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = num_groups;
        cost.memory_budget_bytes = safeBudgetBytes(params_.work_mem_bytes);

        return cost;
    }

    auto CostModel::costDistinct(uint64_t input_rows,
                                 uint64_t num_distinct_rows,
                                 uint64_t num_distinct_keys,
                                 bool presorted,
                                 core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating distinct cost: input_rows=" +
                     std::to_string(input_rows) + ", distinct_rows=" +
                     std::to_string(num_distinct_rows) + ", keys=" +
                     std::to_string(num_distinct_keys) + ", presorted=" +
                     std::to_string(presorted ? 1 : 0));

        CostEstimate cost;
        initializeCostEvidence(cost,
                               formula_profile_,
                               presorted ? "STREAM_DISTINCT" : "HASH_DISTINCT");
        cost.row_width_bytes = safeRowWidthBytes(params_);
        appendInputEstimate(cost, "input_rows", static_cast<double>(input_rows), "rows");
        appendInputEstimate(cost,
                            "num_distinct_rows",
                            static_cast<double>(num_distinct_rows),
                            "rows");
        appendInputEstimate(cost,
                            "num_distinct_keys",
                            static_cast<double>(num_distinct_keys),
                            "count");
        appendInputEstimate(cost, "presorted", presorted ? 1.0 : 0.0, "flag");
        if (input_rows == 0)
        {
            return cost;
        }

        if (presorted)
        {
            cost.startup_cost = static_cast<double>(input_rows) *
                                params_.cpu_tuple_cost * 0.05;
            cost.run_cost =
                static_cast<double>(input_rows) *
                    (params_.cpu_tuple_cost * 0.2 +
                     static_cast<double>(std::max<uint64_t>(1, num_distinct_keys)) *
                         params_.cpu_operator_cost * 0.25) +
                static_cast<double>(num_distinct_rows) * params_.cpu_tuple_cost;
            cost.total_cost = cost.startup_cost + cost.run_cost;
            cost.rows = num_distinct_rows;
            cost.memory_budget_bytes = safeBudgetBytes(params_.work_mem_bytes);
            appendFormulaTerm(cost,
                              "startup.stream_distinct_setup",
                              params_.cpu_tuple_cost * 0.05,
                              static_cast<double>(input_rows),
                              cost.startup_cost,
                              "cost");
            appendFormulaTerm(cost,
                              "run.stream_distinct_cpu",
                              1.0,
                              cost.run_cost - static_cast<double>(num_distinct_rows) *
                                  params_.cpu_tuple_cost,
                              cost.run_cost - static_cast<double>(num_distinct_rows) *
                                  params_.cpu_tuple_cost,
                              "cost");
            appendFormulaTerm(cost,
                              "run.output_materialization",
                              params_.cpu_tuple_cost,
                              static_cast<double>(num_distinct_rows),
                              static_cast<double>(num_distinct_rows) *
                                  params_.cpu_tuple_cost,
                              "cost");
            return cost;
        }

        constexpr double HASH_DISTINCT_FACTOR = 1.6;
        const uint64_t distinct_row_width =
            params_.default_row_width_bytes +
            std::max<uint64_t>(1, num_distinct_keys) * sizeof(uint64_t) +
            params_.hash_tuple_overhead_bytes;
        const SpillEstimate spill = estimateSpill(
            params_,
            std::max<uint64_t>(1, num_distinct_rows) * distinct_row_width,
            params_.work_mem_bytes,
            input_rows,
            1.0);

        cost.startup_cost =
            static_cast<double>(input_rows) * params_.cpu_tuple_cost *
                HASH_DISTINCT_FACTOR +
            spill.io_cost * 0.5 + spill.cpu_cost * 0.5;
        appendFormulaTerm(cost,
                          "startup.hash_distinct_cpu",
                          params_.cpu_tuple_cost * HASH_DISTINCT_FACTOR,
                          static_cast<double>(input_rows),
                          static_cast<double>(input_rows) * params_.cpu_tuple_cost *
                              HASH_DISTINCT_FACTOR,
                          "cost");
        if (spill.io_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "startup.distinct_spill_io",
                              0.5,
                              spill.io_cost,
                              spill.io_cost * 0.5,
                              "cost");
        }
        if (spill.cpu_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "startup.distinct_spill_cpu",
                              0.5,
                              spill.cpu_cost,
                              spill.cpu_cost * 0.5,
                              "cost");
        }
        cost.run_cost =
            static_cast<double>(num_distinct_rows) *
                (params_.cpu_tuple_cost +
                 static_cast<double>(std::max<uint64_t>(1, num_distinct_keys)) *
                     params_.cpu_operator_cost * 0.2) +
            spill.io_cost * 0.5 + spill.cpu_cost * 0.5;
        appendFormulaTerm(cost,
                          "run.distinct_finalize_cpu",
                          1.0,
                          static_cast<double>(num_distinct_rows) *
                              (params_.cpu_tuple_cost +
                               static_cast<double>(std::max<uint64_t>(1, num_distinct_keys)) *
                                   params_.cpu_operator_cost * 0.2),
                          static_cast<double>(num_distinct_rows) *
                              (params_.cpu_tuple_cost +
                               static_cast<double>(std::max<uint64_t>(1, num_distinct_keys)) *
                                   params_.cpu_operator_cost * 0.2),
                          "cost");
        if (spill.io_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "run.distinct_spill_io",
                              0.5,
                              spill.io_cost,
                              spill.io_cost * 0.5,
                              "cost");
        }
        if (spill.cpu_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "run.distinct_spill_cpu",
                              0.5,
                              spill.cpu_cost,
                              spill.cpu_cost * 0.5,
                              "cost");
        }
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = num_distinct_rows;
        applyResourceEstimate(cost, spill);
        return cost;
    }

    auto CostModel::costSort(uint64_t num_rows,
                            uint64_t row_width,
                            uint64_t num_sort_keys,
                            core::ErrorContext* ctx)
        -> CostEstimate
    {
        return costSort(num_rows, row_width, num_sort_keys, 0, ctx);
    }

    auto CostModel::costSort(uint64_t num_rows,
                             uint64_t row_width,
                             uint64_t num_sort_keys,
                             uint64_t top_n_count,
                             core::ErrorContext* ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating sort cost: num_rows=" + std::to_string(num_rows) +
                     ", row_width=" + std::to_string(row_width) +
                     ", num_sort_keys=" + std::to_string(num_sort_keys) +
                     ", top_n=" + std::to_string(top_n_count));

        CostEstimate cost;
        initializeCostEvidence(cost,
                               formula_profile_,
                               top_n_count > 0 && top_n_count < num_rows
                                   ? "TOPN_SORT"
                                   : "SORT");
        cost.row_width_bytes = row_width;
        appendInputEstimate(cost, "num_rows", static_cast<double>(num_rows), "rows");
        appendInputEstimate(cost, "row_width", static_cast<double>(row_width), "bytes");
        appendInputEstimate(cost,
                            "num_sort_keys",
                            static_cast<double>(num_sort_keys),
                            "count");
        appendInputEstimate(cost, "top_n_count", static_cast<double>(top_n_count), "rows");

        if (num_rows == 0)
        {
            // Empty input, no cost
            cost.startup_cost = 0.0;
            cost.run_cost = 0.0;
            cost.total_cost = 0.0;
            cost.rows = 0;
            return cost;
        }

        const bool bounded_top_n =
            top_n_count > 0 && top_n_count < num_rows;
        const uint64_t sort_work_rows = bounded_top_n ? top_n_count : num_rows;

        // In-memory quicksort or bounded heap-style top-N sort.
        // Top-N still touches all input rows, but maintains only N rows in sort memory.
        double num_comparisons = bounded_top_n
            ? static_cast<double>(num_rows) *
                  std::log2(static_cast<double>(std::max<uint64_t>(2, top_n_count)))
            : static_cast<double>(num_rows) *
                  std::log2(static_cast<double>(num_rows));

        // Cost per comparison: evaluate all sort key expressions
        double comparison_cost = static_cast<double>(num_sort_keys) * params_.cpu_operator_cost;

        // Total sort cost
        double sort_cost = num_comparisons * comparison_cost;

        // Memory cost: if data doesn't fit in memory, external sort is needed
        // For now, assume in-memory sort (external sort would add I/O cost)
        uint64_t memory_bytes =
            sort_work_rows *
            std::max<uint64_t>(16, row_width + params_.sort_tuple_overhead_bytes);
        const SpillEstimate spill = estimateSpill(
            params_,
            memory_bytes,
            static_cast<uint64_t>(static_cast<double>(params_.work_mem_bytes) *
                                  params_.merge_mem_multiplier),
            num_rows,
            bounded_top_n ? 0.4 : 1.0);
        double memory_cost = static_cast<double>(
                                 std::min<uint64_t>(memory_bytes, spill.budget_bytes)) *
                             params_.sort_mem_cost;

        // Startup cost: perform the sort
        cost.startup_cost = sort_cost + memory_cost + spill.io_cost + spill.cpu_cost;
        appendFormulaTerm(cost,
                          "startup.sort_compare_cpu",
                          comparison_cost,
                          num_comparisons,
                          sort_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "startup.sort_memory",
                          params_.sort_mem_cost,
                          static_cast<double>(std::min<uint64_t>(memory_bytes, spill.budget_bytes)),
                          memory_cost,
                          "cost");
        appendInputEstimate(cost,
                            "spill_initial_runs",
                            static_cast<double>(spill.initial_runs),
                            "count");
        appendInputEstimate(cost,
                            "spill_merge_fanout",
                            static_cast<double>(spill.merge_fanout),
                            "count");
        if (spill.io_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "startup.sort_spill_io",
                              1.0,
                              spill.io_cost,
                              spill.io_cost,
                              "cost");
        }
        if (spill.cpu_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "startup.sort_spill_cpu",
                              1.0,
                              spill.cpu_cost,
                              spill.cpu_cost,
                              "cost");
        }

        // Run cost: output sorted rows (sequential scan)
        cost.run_cost = static_cast<double>(num_rows) * params_.cpu_tuple_cost;
        appendFormulaTerm(cost,
                          "run.output_materialization",
                          params_.cpu_tuple_cost,
                          static_cast<double>(num_rows),
                          cost.run_cost,
                          "cost");

        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = num_rows;
        applyResourceEstimate(cost, spill);

        DEBUG_LOG_DB("Sort cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     ", output_rows=" + std::to_string(cost.rows) +
                     " (sort=" + std::to_string(sort_cost) +
                     ", comparisons=" + std::to_string(num_comparisons) +
                     ", memory=" + std::to_string(memory_cost) +
                     ", spill=" + std::to_string(cost.spill_expected ? 1 : 0) +
                     ", bounded_top_n=" + std::to_string(bounded_top_n ? 1 : 0) + ")");

        return cost;
    }

    auto CostModel::costGather(const CostEstimate &input_cost,
                               uint64_t input_rows,
                               uint32_t workers_planned,
                               bool leader_participates,
                               core::ErrorContext *ctx)
        -> CostEstimate
    {
        (void)ctx;
        DEBUG_LOG_DB("Estimating gather cost: input_rows=" +
                     std::to_string(input_rows) +
                     ", workers=" + std::to_string(workers_planned) +
                     ", leader=" + std::to_string(leader_participates ? 1 : 0));

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "GATHER");
        cost.row_width_bytes = input_cost.row_width_bytes;
        appendInputEstimate(cost,
                            "input_rows",
                            static_cast<double>(input_rows),
                            "rows");
        appendInputEstimate(cost,
                            "workers_planned",
                            static_cast<double>(workers_planned),
                            "workers");
        appendInputEstimate(cost,
                            "leader_participates",
                            leader_participates ? 1.0 : 0.0,
                            "flag");

        const uint64_t rows = input_rows == 0 ? input_cost.rows : input_rows;
        const double participants = std::max(
            1.0,
            static_cast<double>(workers_planned) +
                (leader_participates ? 1.0 : 0.0));
        const double parallelized_startup_cost =
            input_cost.startup_cost / participants;
        const double parallelized_run_cost = input_cost.run_cost / participants;
        const double tuple_transfer_cost =
            static_cast<double>(rows) * params_.parallel_tuple_cost;

        cost.startup_cost =
            parallelized_startup_cost + params_.parallel_setup_cost;
        appendFormulaTerm(cost,
                          "startup.parallelized_child_startup",
                          1.0 / participants,
                          input_cost.startup_cost,
                          parallelized_startup_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "startup.parallel_setup",
                          1.0,
                          params_.parallel_setup_cost,
                          params_.parallel_setup_cost,
                          "cost");

        cost.run_cost = parallelized_run_cost + tuple_transfer_cost;
        appendFormulaTerm(cost,
                          "run.parallelized_child",
                          1.0 / participants,
                          input_cost.run_cost,
                          parallelized_run_cost,
                          "cost");
        appendFormulaTerm(cost,
                          "run.parallel_tuple_transfer",
                          params_.parallel_tuple_cost,
                          static_cast<double>(rows),
                          tuple_transfer_cost,
                          "cost");

        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = rows;
        return cost;
    }

    auto CostModel::costGatherMerge(const CostEstimate &input_cost,
                                    uint64_t input_rows,
                                    uint64_t num_order_keys,
                                    uint32_t workers_planned,
                                    bool leader_participates,
                                    core::ErrorContext *ctx)
        -> CostEstimate
    {
        (void)ctx;
        DEBUG_LOG_DB("Estimating gather merge cost: input_rows=" +
                     std::to_string(input_rows) +
                     ", order_keys=" + std::to_string(num_order_keys) +
                     ", workers=" + std::to_string(workers_planned));

        CostEstimate cost = costGather(input_cost,
                                       input_rows,
                                       workers_planned,
                                       leader_participates,
                                       nullptr);
        cost.operator_name = "GATHER_MERGE";
        const uint64_t rows = cost.rows;
        const double participants = std::max(
            2.0,
            static_cast<double>(workers_planned) +
                (leader_participates ? 1.0 : 0.0));
        const double merge_comparisons =
            static_cast<double>(rows) *
            std::log2(participants) *
            static_cast<double>(std::max<uint64_t>(1, num_order_keys));
        const double merge_cost =
            merge_comparisons * params_.cpu_operator_cost;
        appendInputEstimate(cost,
                            "num_order_keys",
                            static_cast<double>(num_order_keys),
                            "count");
        appendFormulaTerm(cost,
                          "run.gather_merge_compare_cpu",
                          params_.cpu_operator_cost,
                          merge_comparisons,
                          merge_cost,
                          "cost");
        cost.run_cost += merge_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        return cost;
    }

    auto CostModel::costLimit(uint64_t input_rows,
                             int64_t limit_count,
                             int64_t offset_count,
                             core::ErrorContext* ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating limit cost: input_rows=" + std::to_string(input_rows) +
                     ", limit=" + std::to_string(limit_count) +
                     ", offset=" + std::to_string(offset_count));

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "LIMIT");
        cost.row_width_bytes = safeRowWidthBytes(params_);
        appendInputEstimate(cost, "input_rows", static_cast<double>(input_rows), "rows");
        appendInputEstimate(cost, "limit_count", static_cast<double>(limit_count), "rows");
        appendInputEstimate(cost, "offset_count", static_cast<double>(offset_count), "rows");

        // Handle offset
        uint64_t offset = 0;
        if (offset_count > 0)
        {
            offset = static_cast<uint64_t>(offset_count);
        }

        // OFFSET requires scanning and discarding rows
        double offset_cost = static_cast<double>(offset) * params_.cpu_tuple_cost;
        cost.startup_cost = offset_cost;
        if (offset_cost > 0.0)
        {
            appendFormulaTerm(cost,
                              "startup.offset_skip_cpu",
                              params_.cpu_tuple_cost,
                              static_cast<double>(offset),
                              offset_cost,
                              "cost");
        }

        // Calculate output rows after offset
        uint64_t rows_after_offset = (offset >= input_rows) ? 0 : (input_rows - offset);

        // Handle limit
        uint64_t output_rows = rows_after_offset;
        if (limit_count >= 0)
        {
            output_rows = std::min(rows_after_offset, static_cast<uint64_t>(limit_count));
        }

        // Run cost: materialize limited output rows
        // LIMIT allows early termination, so we only process output_rows
        cost.run_cost = static_cast<double>(output_rows) * params_.cpu_tuple_cost;
        appendFormulaTerm(cost,
                          "run.output_materialization",
                          params_.cpu_tuple_cost,
                          static_cast<double>(output_rows),
                          cost.run_cost,
                          "cost");

        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = output_rows;
        cost.memory_budget_bytes = safeBudgetBytes(params_.work_mem_bytes);

        DEBUG_LOG_DB("Limit cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     ", output_rows=" + std::to_string(cost.rows) +
                     " (offset_rows=" + std::to_string(offset) + ")");

        return cost;
    }

    auto CostModel::costWindow(uint64_t input_rows,
                               uint64_t row_width,
                               uint64_t num_partition_keys,
                               uint64_t num_order_keys,
                               uint64_t num_window_functions,
                               bool input_presorted,
                               core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating window cost: input_rows=" + std::to_string(input_rows) +
                     ", row_width=" + std::to_string(row_width) +
                     ", partition_keys=" + std::to_string(num_partition_keys) +
                     ", order_keys=" + std::to_string(num_order_keys) +
                     ", funcs=" + std::to_string(num_window_functions) +
                     ", presorted=" + std::to_string(input_presorted ? 1 : 0));

        CostEstimate cost;
        initializeCostEvidence(cost, formula_profile_, "WINDOW");
        cost.row_width_bytes = row_width;
        appendInputEstimate(cost, "input_rows", static_cast<double>(input_rows), "rows");
        appendInputEstimate(cost, "row_width", static_cast<double>(row_width), "bytes");
        appendInputEstimate(cost,
                            "num_partition_keys",
                            static_cast<double>(num_partition_keys),
                            "count");
        appendInputEstimate(cost,
                            "num_order_keys",
                            static_cast<double>(num_order_keys),
                            "count");
        appendInputEstimate(cost,
                            "num_window_functions",
                            static_cast<double>(num_window_functions),
                            "count");
        appendInputEstimate(cost,
                            "input_presorted",
                            input_presorted ? 1.0 : 0.0,
                            "flag");
        if (input_rows == 0)
        {
            return cost;
        }

        const bool requires_sort =
            !input_presorted && (num_partition_keys > 0 || num_order_keys > 0);
        double sort_cost = 0.0;
        if (requires_sort)
        {
            const auto sort = costSort(input_rows,
                                       row_width,
                                       std::max<uint64_t>(1, num_partition_keys + num_order_keys),
                                       ctx);
            sort_cost = sort.total_cost;
            cost.memory_bytes = sort.memory_bytes;
            cost.memory_budget_bytes = sort.memory_budget_bytes;
            cost.spill_expected = sort.spill_expected;
            cost.spill_passes = sort.spill_passes;
            cost.spill_bytes = sort.spill_bytes;
            appendFormulaTerm(cost,
                              "startup.window_sort",
                              1.0,
                              sort.total_cost,
                              sort.total_cost,
                              "cost");
        }

        const double partition_cpu =
            static_cast<double>(input_rows) *
            static_cast<double>(std::max<uint64_t>(1, num_partition_keys)) *
            params_.cpu_operator_cost * 0.5;
        const double function_cpu =
            static_cast<double>(input_rows) *
            static_cast<double>(std::max<uint64_t>(1, num_window_functions)) *
            (params_.cpu_operator_cost + params_.cpu_tuple_cost * 0.5);

        cost.startup_cost = sort_cost;
        appendFormulaTerm(cost,
                          "run.partition_cpu",
                          1.0,
                          partition_cpu,
                          partition_cpu,
                          "cost");
        appendFormulaTerm(cost,
                          "run.window_function_cpu",
                          1.0,
                          function_cpu,
                          function_cpu,
                          "cost");
        cost.run_cost = partition_cpu + function_cpu;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = input_rows;
        if (cost.memory_budget_bytes == 0)
        {
            cost.memory_budget_bytes = safeBudgetBytes(params_.work_mem_bytes);
        }
        if (cost.resource_governance_outcome.empty() ||
            cost.resource_governance_outcome == "NO_MEMORY_GOVERNANCE")
        {
            cost.resource_governance_outcome =
                cost.spill_expected ? "SPILL_EXPECTED" : "IN_MEMORY";
        }
        return cost;
    }

    auto CostModel::costWindow(uint64_t input_rows,
                               uint64_t row_width,
                               uint64_t num_partition_keys,
                               uint64_t num_order_keys,
                               uint64_t num_window_functions,
                               core::ErrorContext *ctx)
        -> CostEstimate
    {
        return costWindow(input_rows,
                          row_width,
                          num_partition_keys,
                          num_order_keys,
                          num_window_functions,
                          false,
                          ctx);
    }

} // namespace scratchbird::optimizer
