/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/optimizer/multi_track_classifier.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace scratchbird::optimizer
{
    struct Histogram128
    {
        std::array<double, 129> boundaries{};
        bool initialized = false;
    };

    struct SelectivityResult
    {
        bool ok = false;
        double value = 0.0;
        std::string error_code;
        std::string error_message;
    };

    struct PlanHashAttribute
    {
        enum class Kind : uint8_t
        {
            STRING,
            NUMERIC,
            I64,
            U64,
            BOOL
        };

        std::string key;
        Kind kind = Kind::STRING;
        std::string string_value;
        double numeric_value = 0.0;
        int64_t i64_value = 0;
        uint64_t u64_value = 0;
        bool bool_value = false;

        static auto makeString(const std::string &key, const std::string &value)
            -> PlanHashAttribute;
        static auto makeNumeric(const std::string &key, double value)
            -> PlanHashAttribute;
        static auto makeI64(const std::string &key, int64_t value)
            -> PlanHashAttribute;
        static auto makeU64(const std::string &key, uint64_t value)
            -> PlanHashAttribute;
        static auto makeBool(const std::string &key, bool value)
            -> PlanHashAttribute;
    };

    struct PlanHashNode
    {
        std::string node_symbol;
        std::vector<PlanHashAttribute> attributes;
        std::vector<PlanHashNode> children;
    };

    struct PlanHashResult
    {
        bool ok = false;
        std::string plan_hash;
        std::string serialized;
        std::string error_code;
        std::string error_message;
    };

    struct VNextPlanCandidateInput
    {
        std::optional<QueryTrack> track_symbol;
        std::optional<uint64_t> base_cardinality;
        std::optional<double> post_filter_selectivity;
        std::optional<uint64_t> avg_row_bytes;
        std::optional<uint32_t> operator_count;
        std::optional<uint32_t> bridge_count;
        std::optional<uint64_t> bridge_shuffle_bytes;
        std::optional<double> read_amplification_factor;
        std::optional<double> latency_budget_ms;
        std::optional<uint64_t> memory_budget_bytes;
        std::optional<uint64_t> page_size_bytes;

        std::optional<uint32_t> join_reorder_candidate_count;
        std::optional<uint64_t> estimated_document_fetches;
        std::optional<uint64_t> pruned_chunk_count;
        std::optional<uint64_t> zone_map_pruned_row_groups;
        std::optional<uint64_t> postings_blocks_visited;
        std::optional<uint64_t> ann_expansion_count;
        std::optional<uint32_t> cross_track_predicate_count;

        bool violates_isolation_constraints = false;
        std::optional<PlanHashNode> plan_root;
    };

    struct VNextPlanScoreResult
    {
        bool ok = false;
        std::string error_code;
        std::string error_message;

        QueryTrack track_symbol = QueryTrack::RELATIONAL_TRACK;
        uint64_t estimated_rows = 0;
        double estimated_scan_bytes = 0.0;
        double estimated_io_pages = 0.0;
        uint64_t estimated_memory_peak_bytes = 0;
        double predicted_latency_ms = 0.0;
        double total_cost = 0.0;
        int64_t total_cost_uCost = 0;
        uint32_t bridge_count = 0;
        std::string plan_hash;
    };

    struct VNextPlanSelectionResult
    {
        bool ok = false;
        std::string error_code;
        std::string error_message;
        size_t selected_index = 0;
        std::vector<VNextPlanScoreResult> scores;
    };

    class VNextPlanSelection
    {
    public:
        static constexpr int64_t kEpsilonEqualCostUCost = 1000;

        static auto makeUniformHistogram(double min_value, double max_value)
            -> Histogram128;

        static auto computeRangeSelectivity(const std::optional<Histogram128> &histogram,
                                            double lower,
                                            double upper,
                                            bool histogram_required)
            -> SelectivityResult;

        static auto combineIndependentSelectivities(
            const std::vector<double> &selectivities)
            -> SelectivityResult;

        static auto computeSameColumnIntersectionSelectivity(
            const std::optional<Histogram128> &histogram,
            double lower_a,
            double upper_a,
            double lower_b,
            double upper_b,
            bool histogram_required)
            -> SelectivityResult;

        static auto buildPlanHash(const PlanHashNode &root) -> PlanHashResult;

        static auto scoreCandidate(const VNextPlanCandidateInput &candidate)
            -> VNextPlanScoreResult;

        static auto selectBestPlan(const std::vector<VNextPlanCandidateInput> &candidates)
            -> VNextPlanSelectionResult;
    };

} // namespace scratchbird::optimizer

