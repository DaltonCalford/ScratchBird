/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/optimizer/vnext_plan_selection.h"
#include "scratchbird/core/vnext_metrics_event_model.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <numeric>
#include <openssl/sha.h>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace scratchbird::optimizer
{
    namespace
    {
        constexpr size_t kHistogramBucketCount = 128;
        constexpr double kCostScale = 1000000.0;
        constexpr double kBytesPerMB = 1048576.0;

        struct TrackCoefficients
        {
            double c_io_page = 0.0;
            double c_cpu_row = 0.0;
            double c_cpu_op = 0.0;
            double memory_multiplier = 0.0;
            double latency_base_ms = 0.0;
        };

        auto coeffForTrack(QueryTrack track) -> TrackCoefficients
        {
            switch (track)
            {
                case QueryTrack::RELATIONAL_TRACK: return {0.004000, 0.000060, 0.150000, 0.40, 2.0};
                case QueryTrack::DOCUMENT_TRACK: return {0.005000, 0.000080, 0.180000, 0.45, 3.0};
                case QueryTrack::TIMESERIES_TRACK: return {0.003000, 0.000050, 0.120000, 0.35, 2.5};
                case QueryTrack::COLUMNAR_TRACK: return {0.002500, 0.000040, 0.110000, 0.30, 2.0};
                case QueryTrack::SEARCH_TRACK: return {0.004500, 0.000090, 0.210000, 0.50, 4.0};
                case QueryTrack::VECTOR_TRACK: return {0.006000, 0.000120, 0.300000, 0.55, 5.0};
                case QueryTrack::HYBRID_TRACK: return {0.005500, 0.000100, 0.260000, 0.60, 6.0};
            }
            return {};
        }

        auto toFixed6(double value) -> std::string
        {
            std::ostringstream out;
            out << std::fixed << std::setprecision(6) << value;
            return out.str();
        }

        auto isFinite(double value) -> bool
        {
            return std::isfinite(value);
        }

        auto hasMissingRequiredInput(const VNextPlanCandidateInput &candidate) -> bool
        {
            return !candidate.track_symbol || !candidate.base_cardinality ||
                   !candidate.post_filter_selectivity || !candidate.avg_row_bytes ||
                   !candidate.operator_count || !candidate.bridge_count ||
                   !candidate.bridge_shuffle_bytes || !candidate.read_amplification_factor ||
                   !candidate.latency_budget_ms || !candidate.memory_budget_bytes ||
                   !candidate.page_size_bytes;
        }

        auto invalidScore(const std::string &code, const std::string &message)
            -> VNextPlanScoreResult
        {
            VNextPlanScoreResult out;
            out.ok = false;
            out.error_code = code;
            out.error_message = message;
            core::VNextMetricsEventModel::recordOptimizerEvent(
                "plan_score", "reject", code);
            return out;
        }

        auto invalidSelectivity(const std::string &code, const std::string &message)
            -> SelectivityResult
        {
            SelectivityResult out;
            out.ok = false;
            out.error_code = code;
            out.error_message = message;
            core::VNextMetricsEventModel::recordOptimizerEvent(
                "plan_selectivity", "reject", code);
            return out;
        }

        auto invalidPlanHash(const std::string &code, const std::string &message)
            -> PlanHashResult
        {
            PlanHashResult out;
            out.ok = false;
            out.error_code = code;
            out.error_message = message;
            core::VNextMetricsEventModel::recordOptimizerEvent(
                "plan_hash", "reject", code);
            return out;
        }

        auto appendNodeRecord(const PlanHashNode &node,
                              std::vector<std::string> &records,
                              std::string &error_message) -> bool
        {
            if (node.node_symbol.empty())
            {
                error_message = "Plan hash serialization requires non-empty node_symbol";
                return false;
            }

            std::vector<const PlanHashAttribute *> sorted_attrs;
            sorted_attrs.reserve(node.attributes.size());
            for (const PlanHashAttribute &attr : node.attributes)
            {
                sorted_attrs.push_back(&attr);
            }
            std::sort(sorted_attrs.begin(), sorted_attrs.end(),
                      [](const PlanHashAttribute *lhs, const PlanHashAttribute *rhs)
                      {
                          return lhs->key < rhs->key;
                      });

            for (size_t i = 0; i < sorted_attrs.size(); ++i)
            {
                if (sorted_attrs[i]->key.empty())
                {
                    error_message =
                        "Plan hash serialization requires non-empty attribute keys";
                    return false;
                }
                if (i > 0 && sorted_attrs[i - 1]->key == sorted_attrs[i]->key)
                {
                    error_message =
                        "Plan hash serialization forbids duplicate attribute keys";
                    return false;
                }
            }

            std::ostringstream record;
            record << node.node_symbol << "|" << node.children.size() << "|"
                   << sorted_attrs.size();

            for (const PlanHashAttribute *attr : sorted_attrs)
            {
                std::string value;
                switch (attr->kind)
                {
                    case PlanHashAttribute::Kind::STRING:
                        value = attr->string_value;
                        break;
                    case PlanHashAttribute::Kind::NUMERIC:
                        if (!isFinite(attr->numeric_value))
                        {
                            error_message =
                                "Plan hash serialization requires finite numeric attributes";
                            return false;
                        }
                        value = toFixed6(attr->numeric_value);
                        break;
                    case PlanHashAttribute::Kind::I64:
                        value = std::to_string(attr->i64_value);
                        break;
                    case PlanHashAttribute::Kind::U64:
                        value = std::to_string(attr->u64_value);
                        break;
                    case PlanHashAttribute::Kind::BOOL:
                        value = attr->bool_value ? "true" : "false";
                        break;
                }
                record << "|" << attr->key << "=" << value;
            }
            records.push_back(record.str());

            for (const PlanHashNode &child : node.children)
            {
                if (!appendNodeRecord(child, records, error_message))
                {
                    return false;
                }
            }
            return true;
        }
    } // namespace

    auto PlanHashAttribute::makeString(const std::string &key, const std::string &value)
        -> PlanHashAttribute
    {
        PlanHashAttribute out;
        out.key = key;
        out.kind = Kind::STRING;
        out.string_value = value;
        return out;
    }

    auto PlanHashAttribute::makeNumeric(const std::string &key, double value)
        -> PlanHashAttribute
    {
        PlanHashAttribute out;
        out.key = key;
        out.kind = Kind::NUMERIC;
        out.numeric_value = value;
        return out;
    }

    auto PlanHashAttribute::makeI64(const std::string &key, int64_t value)
        -> PlanHashAttribute
    {
        PlanHashAttribute out;
        out.key = key;
        out.kind = Kind::I64;
        out.i64_value = value;
        return out;
    }

    auto PlanHashAttribute::makeU64(const std::string &key, uint64_t value)
        -> PlanHashAttribute
    {
        PlanHashAttribute out;
        out.key = key;
        out.kind = Kind::U64;
        out.u64_value = value;
        return out;
    }

    auto PlanHashAttribute::makeBool(const std::string &key, bool value)
        -> PlanHashAttribute
    {
        PlanHashAttribute out;
        out.key = key;
        out.kind = Kind::BOOL;
        out.bool_value = value;
        return out;
    }

    auto VNextPlanSelection::makeUniformHistogram(double min_value, double max_value)
        -> Histogram128
    {
        Histogram128 out;
        if (!isFinite(min_value) || !isFinite(max_value))
        {
            return out;
        }
        if (max_value < min_value)
        {
            std::swap(min_value, max_value);
        }

        const double step =
            (max_value - min_value) / static_cast<double>(kHistogramBucketCount);
        for (size_t i = 0; i <= kHistogramBucketCount; ++i)
        {
            out.boundaries[i] = min_value + (step * static_cast<double>(i));
        }
        out.initialized = true;
        return out;
    }

    auto VNextPlanSelection::computeRangeSelectivity(
        const std::optional<Histogram128> &histogram,
        double lower,
        double upper,
        bool histogram_required) -> SelectivityResult
    {
        if (!histogram || !histogram->initialized)
        {
            if (histogram_required)
            {
                return invalidSelectivity(
                    "OPT_0302",
                    "Missing required histogram for predicate-bearing column");
            }
            return {true, 1.0, "", ""};
        }

        if (!isFinite(lower) || !isFinite(upper))
        {
            return invalidSelectivity("OPT_0306", "Invalid numeric range bounds");
        }
        if (upper < lower)
        {
            std::swap(lower, upper);
        }

        for (size_t i = 0; i < histogram->boundaries.size(); ++i)
        {
            if (!isFinite(histogram->boundaries[i]))
            {
                return invalidSelectivity("OPT_0306",
                                          "Histogram contains invalid numeric boundary");
            }
            if (i > 0 && histogram->boundaries[i] < histogram->boundaries[i - 1])
            {
                return invalidSelectivity("OPT_0306",
                                          "Histogram boundaries must be monotonic");
            }
        }

        const double min_bound = histogram->boundaries.front();
        const double max_bound = histogram->boundaries.back();
        const double clamped_lower = std::max(lower, min_bound);
        const double clamped_upper = std::min(upper, max_bound);
        if (!(clamped_upper > clamped_lower))
        {
            return {true, 0.0, "", ""};
        }

        double selectivity = 0.0;
        const double per_bucket_weight = 1.0 / static_cast<double>(kHistogramBucketCount);
        for (size_t i = 0; i < kHistogramBucketCount; ++i)
        {
            const double bucket_low = histogram->boundaries[i];
            const double bucket_high = histogram->boundaries[i + 1];
            const double bucket_width = bucket_high - bucket_low;
            if (!(bucket_width > 0.0))
            {
                continue;
            }

            const double overlap_low = std::max(clamped_lower, bucket_low);
            const double overlap_high = std::min(clamped_upper, bucket_high);
            if (!(overlap_high > overlap_low))
            {
                continue;
            }
            const double overlap_fraction = (overlap_high - overlap_low) / bucket_width;
            selectivity += overlap_fraction * per_bucket_weight;
        }

        selectivity = std::clamp(selectivity, 0.0, 1.0);
        return {true, selectivity, "", ""};
    }

    auto VNextPlanSelection::combineIndependentSelectivities(
        const std::vector<double> &selectivities) -> SelectivityResult
    {
        double combined = 1.0;
        for (double value : selectivities)
        {
            if (!isFinite(value) || value < 0.0 || value > 1.0)
            {
                return invalidSelectivity("OPT_0306",
                                          "Invalid selectivity value for multiplication");
            }
            combined *= value;
        }
        combined = std::clamp(combined, 0.0, 1.0);
        return {true, combined, "", ""};
    }

    auto VNextPlanSelection::computeSameColumnIntersectionSelectivity(
        const std::optional<Histogram128> &histogram,
        double lower_a,
        double upper_a,
        double lower_b,
        double upper_b,
        bool histogram_required) -> SelectivityResult
    {
        if (upper_a < lower_a)
        {
            std::swap(lower_a, upper_a);
        }
        if (upper_b < lower_b)
        {
            std::swap(lower_b, upper_b);
        }

        const double lower = std::max(lower_a, lower_b);
        const double upper = std::min(upper_a, upper_b);
        if (!(upper > lower))
        {
            return {true, 0.0, "", ""};
        }
        return computeRangeSelectivity(histogram, lower, upper, histogram_required);
    }

    auto VNextPlanSelection::buildPlanHash(const PlanHashNode &root) -> PlanHashResult
    {
        std::vector<std::string> records;
        std::string error_message;
        if (!appendNodeRecord(root, records, error_message))
        {
            return invalidPlanHash("OPT_0307", error_message);
        }

        PlanHashResult out;
        out.ok = true;
        for (size_t i = 0; i < records.size(); ++i)
        {
            if (i > 0)
            {
                out.serialized.push_back('\n');
            }
            out.serialized.append(records[i]);
        }

        unsigned char digest[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char *>(out.serialized.data()),
               out.serialized.size(), digest);

        std::ostringstream hex;
        hex << std::hex << std::nouppercase << std::setfill('0');
        for (unsigned char byte : digest)
        {
            hex << std::setw(2) << static_cast<unsigned int>(byte);
        }
        out.plan_hash = hex.str();
        return out;
    }

    auto VNextPlanSelection::scoreCandidate(const VNextPlanCandidateInput &candidate)
        -> VNextPlanScoreResult
    {
        if (hasMissingRequiredInput(candidate))
        {
            return invalidScore("OPT_0302", "Missing required cost-model input");
        }

        const double selectivity = *candidate.post_filter_selectivity;
        if (!isFinite(selectivity) || selectivity < 0.0 || selectivity > 1.0)
        {
            return invalidScore("OPT_0302", "Invalid post_filter_selectivity range");
        }

        const double read_amplification = *candidate.read_amplification_factor;
        const double latency_budget_ms = *candidate.latency_budget_ms;
        if (!isFinite(read_amplification) || !isFinite(latency_budget_ms))
        {
            return invalidScore("OPT_0306", "Invalid numeric cost-model state");
        }
        if (read_amplification <= 0.0 || *candidate.page_size_bytes == 0)
        {
            return invalidScore("OPT_0302", "Invalid page size or read amplification");
        }

        const QueryTrack track = *candidate.track_symbol;
        const TrackCoefficients coeff = coeffForTrack(track);

        const double rows_double = std::ceil(
            static_cast<double>(*candidate.base_cardinality) * selectivity);
        if (!isFinite(rows_double))
        {
            return invalidScore("OPT_0306", "Invalid numeric state while deriving row count");
        }
        const uint64_t estimated_rows = static_cast<uint64_t>(
            std::max(1.0, rows_double));
        const double estimated_scan_bytes =
            static_cast<double>(estimated_rows) * static_cast<double>(*candidate.avg_row_bytes);
        const double estimated_io_pages =
            std::ceil(estimated_scan_bytes / static_cast<double>(*candidate.page_size_bytes)) *
            read_amplification;
        const double memory_peak_double =
            std::ceil(estimated_scan_bytes * coeff.memory_multiplier);

        if (!isFinite(estimated_scan_bytes) || !isFinite(estimated_io_pages) ||
            !isFinite(memory_peak_double) || memory_peak_double < 0.0 ||
            memory_peak_double > static_cast<double>(std::numeric_limits<uint64_t>::max()))
        {
            return invalidScore("OPT_0306", "Invalid numeric state while deriving cost inputs");
        }
        const uint64_t estimated_memory_peak_bytes =
            static_cast<uint64_t>(memory_peak_double);

        constexpr double c_mem_over_budget_per_mb = 0.250000;
        constexpr double c_latency_over_budget_per_ms = 0.100000;
        constexpr double c_bridge_fixed = 2.000000;
        constexpr double c_bridge_per_mb = 0.150000;

        const uint32_t bridge_count = *candidate.bridge_count;
        const double io_cost = estimated_io_pages * coeff.c_io_page;
        const double cpu_cost =
            (static_cast<double>(estimated_rows) * coeff.c_cpu_row) +
            (static_cast<double>(*candidate.operator_count) * coeff.c_cpu_op);
        const double memory_over_mb = std::max(
            0.0, (static_cast<double>(estimated_memory_peak_bytes) -
                  static_cast<double>(*candidate.memory_budget_bytes)) /
                     kBytesPerMB);
        const double memory_cost = memory_over_mb * c_mem_over_budget_per_mb;
        const double bridge_cost =
            (static_cast<double>(bridge_count) * c_bridge_fixed) +
            ((static_cast<double>(*candidate.bridge_shuffle_bytes) / kBytesPerMB) *
             c_bridge_per_mb);
        const double predicted_latency_ms =
            coeff.latency_base_ms + (io_cost * 0.30) + (cpu_cost * 0.20) + (bridge_cost * 0.40);
        const double latency_cost =
            std::max(0.0, predicted_latency_ms - latency_budget_ms) *
            c_latency_over_budget_per_ms;

        double total_cost = io_cost + cpu_cost + memory_cost + bridge_cost + latency_cost;
        switch (track)
        {
            case QueryTrack::RELATIONAL_TRACK:
                total_cost +=
                    0.020000 * static_cast<double>(
                                   candidate.join_reorder_candidate_count.value_or(0));
                break;
            case QueryTrack::DOCUMENT_TRACK:
                total_cost +=
                    0.000003 *
                    static_cast<double>(candidate.estimated_document_fetches.value_or(0));
                break;
            case QueryTrack::TIMESERIES_TRACK:
                total_cost -=
                    0.000002 * static_cast<double>(candidate.pruned_chunk_count.value_or(0));
                total_cost = std::max(0.0, total_cost);
                break;
            case QueryTrack::COLUMNAR_TRACK:
                total_cost -= 0.0000015 *
                              static_cast<double>(
                                  candidate.zone_map_pruned_row_groups.value_or(0));
                total_cost = std::max(0.0, total_cost);
                break;
            case QueryTrack::SEARCH_TRACK:
                total_cost +=
                    0.000004 *
                    static_cast<double>(candidate.postings_blocks_visited.value_or(0));
                break;
            case QueryTrack::VECTOR_TRACK:
                total_cost +=
                    0.000005 * static_cast<double>(candidate.ann_expansion_count.value_or(0));
                break;
            case QueryTrack::HYBRID_TRACK:
                total_cost +=
                    0.100000 *
                    static_cast<double>(candidate.cross_track_predicate_count.value_or(0));
                break;
        }

        if (!isFinite(total_cost) || !isFinite(predicted_latency_ms))
        {
            return invalidScore("OPT_0306", "Invalid numeric cost-model state");
        }

        const double scaled_u_cost = total_cost * kCostScale;
        if (!isFinite(scaled_u_cost) ||
            scaled_u_cost < static_cast<double>(std::numeric_limits<int64_t>::min()) ||
            scaled_u_cost > static_cast<double>(std::numeric_limits<int64_t>::max()))
        {
            return invalidScore("OPT_0306", "Cost value exceeds signed 64-bit uCost range");
        }
        const int64_t total_cost_u_cost = static_cast<int64_t>(std::llround(scaled_u_cost));

        VNextPlanScoreResult out;
        out.ok = true;
        out.track_symbol = track;
        out.estimated_rows = estimated_rows;
        out.estimated_scan_bytes = estimated_scan_bytes;
        out.estimated_io_pages = estimated_io_pages;
        out.estimated_memory_peak_bytes = estimated_memory_peak_bytes;
        out.predicted_latency_ms = predicted_latency_ms;
        out.total_cost = total_cost;
        out.total_cost_uCost = total_cost_u_cost;
        out.bridge_count = bridge_count;

        if (candidate.plan_root.has_value())
        {
            PlanHashResult hash_result = buildPlanHash(*candidate.plan_root);
            if (!hash_result.ok)
            {
                return invalidScore(hash_result.error_code, hash_result.error_message);
            }
            out.plan_hash = std::move(hash_result.plan_hash);
        }
        core::VNextMetricsEventModel::recordOptimizerEvent(
            "plan_score", "ok", "NONE");
        return out;
    }

    auto VNextPlanSelection::selectBestPlan(
        const std::vector<VNextPlanCandidateInput> &candidates) -> VNextPlanSelectionResult
    {
        VNextPlanSelectionResult out;
        if (candidates.empty())
        {
            out.error_code = "OPT_0301";
            out.error_message = "No legal plan candidates available";
            core::VNextMetricsEventModel::recordOptimizerEvent(
                "plan_select", "reject", out.error_code);
            return out;
        }

        for (const VNextPlanCandidateInput &candidate : candidates)
        {
            if (candidate.violates_isolation_constraints)
            {
                continue;
            }
            VNextPlanScoreResult score = scoreCandidate(candidate);
            if (!score.ok)
            {
                out.error_code = score.error_code;
                out.error_message = score.error_message;
                core::VNextMetricsEventModel::recordOptimizerEvent(
                    "plan_select", "reject", out.error_code);
                return out;
            }
            out.scores.push_back(std::move(score));
        }

        if (out.scores.empty())
        {
            out.error_code = "OPT_0301";
            out.error_message = "No legal plan candidates available after isolation filtering";
            core::VNextMetricsEventModel::recordOptimizerEvent(
                "plan_select", "reject", out.error_code);
            return out;
        }

        int64_t min_cost = out.scores.front().total_cost_uCost;
        for (const VNextPlanScoreResult &score : out.scores)
        {
            min_cost = std::min(min_cost, score.total_cost_uCost);
        }

        std::vector<size_t> tie_indices;
        for (size_t i = 0; i < out.scores.size(); ++i)
        {
            const int64_t delta = std::llabs(out.scores[i].total_cost_uCost - min_cost);
            if (delta <= kEpsilonEqualCostUCost)
            {
                tie_indices.push_back(i);
            }
        }

        if (tie_indices.size() > 1)
        {
            for (size_t index : tie_indices)
            {
                if (out.scores[index].plan_hash.empty())
                {
                    out.error_code = "OPT_0307";
                    out.error_message =
                        "Tie-break input missing plan_hash for equal-cost candidate";
                    core::VNextMetricsEventModel::recordOptimizerEvent(
                        "plan_select", "reject", out.error_code);
                    return out;
                }
            }

            std::sort(tie_indices.begin(), tie_indices.end(),
                      [&out](size_t lhs, size_t rhs)
                      {
                          const VNextPlanScoreResult &a = out.scores[lhs];
                          const VNextPlanScoreResult &b = out.scores[rhs];
                          if (a.bridge_count != b.bridge_count)
                          {
                              return a.bridge_count < b.bridge_count;
                          }
                          if (a.estimated_memory_peak_bytes != b.estimated_memory_peak_bytes)
                          {
                              return a.estimated_memory_peak_bytes < b.estimated_memory_peak_bytes;
                          }
                          if (a.plan_hash != b.plan_hash)
                          {
                              return a.plan_hash < b.plan_hash;
                          }
                          return lhs < rhs;
                      });
        }

        out.ok = true;
        out.selected_index = tie_indices.front();
        core::VNextMetricsEventModel::recordOptimizerEvent(
            "plan_select", "ok", "NONE");
        return out;
    }

} // namespace scratchbird::optimizer
