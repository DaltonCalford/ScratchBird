/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * join_ordering.cpp - Dynamic Programming Join Order Optimizer Implementation
 *
 * P3-20: Cost-based join ordering optimization using dynamic programming.
 *
 * V3 MIGRATION: Uses V3 Expression types for join conditions.
 */

#include "scratchbird/optimizer/join_ordering.h"
#include "scratchbird/optimizer/join_legality.h"
#include "scratchbird/core/debug.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <sstream>

namespace scratchbird::optimizer
{
namespace
{
    constexpr double kFrontierDominanceEpsilon = 1e-9;

    auto appendUniqueTag(std::vector<std::string>& tags,
                         const std::string& tag) -> void
    {
        if (tag.empty())
        {
            return;
        }
        if (std::find(tags.begin(), tags.end(), tag) == tags.end())
        {
            tags.push_back(tag);
        }
    }

    auto descriptorProvidesMergeJoinOrder(const AccessPathDescriptor& descriptor,
                                          const std::string& order_key_text)
        -> bool
    {
        if (order_key_text.empty() || !descriptor.order_complete ||
            descriptor.ordering_keys.empty())
        {
            return false;
        }
        return descriptor.ordering_keys.front().expression_text == order_key_text;
    }

    auto pathProvidesMergeJoinOrder(const std::shared_ptr<Path>& path,
                                    const std::string& order_key_text) -> bool
    {
        if (!path)
        {
            return false;
        }
        return descriptorProvidesMergeJoinOrder(path->accessDescriptor(),
                                                order_key_text);
    }

    auto quantizePropertyScore(double value) -> int64_t
    {
        if (!std::isfinite(value))
        {
            return 0;
        }
        return static_cast<int64_t>(std::llround(value * 1000.0));
    }

    auto appendSignatureText(std::ostringstream& out, const std::string& text) -> void
    {
        out << text.size() << '#' << text << ';';
    }

    auto accessPathExactnessStrength(AccessPathExactnessClass value) -> int
    {
        switch (value)
        {
            case AccessPathExactnessClass::EXACT_KEY:
                return 5;
            case AccessPathExactnessClass::EXACT_ROW:
                return 4;
            case AccessPathExactnessClass::CANDIDATE_REGION:
                return 3;
            case AccessPathExactnessClass::LOWER_BOUND_ORDERED:
                return 2;
            case AccessPathExactnessClass::APPROX_TOPK:
                return 1;
            case AccessPathExactnessClass::UNKNOWN:
            default:
                return 0;
        }
    }

    auto weakerExactness(AccessPathExactnessClass left,
                         AccessPathExactnessClass right)
        -> AccessPathExactnessClass
    {
        return accessPathExactnessStrength(left) <=
                   accessPathExactnessStrength(right)
               ? left
               : right;
    }

    auto accessPathVisibilityStrength(AccessPathVisibilityEnforcement value)
        -> int
    {
        switch (value)
        {
            case AccessPathVisibilityEnforcement::INDEX_NATIVE:
                return 3;
            case AccessPathVisibilityEnforcement::HYBRID:
                return 2;
            case AccessPathVisibilityEnforcement::POST_FILTER:
                return 1;
            case AccessPathVisibilityEnforcement::UNKNOWN:
            default:
                return 0;
        }
    }

    auto weakerVisibility(AccessPathVisibilityEnforcement left,
                          AccessPathVisibilityEnforcement right)
        -> AccessPathVisibilityEnforcement
    {
        return accessPathVisibilityStrength(left) <=
                   accessPathVisibilityStrength(right)
               ? left
               : right;
    }

    auto accessPathQueryabilityStrength(AccessPathQueryabilityState value)
        -> int
    {
        switch (value)
        {
            case AccessPathQueryabilityState::QUERYABLE:
                return 3;
            case AccessPathQueryabilityState::LIMITED:
                return 2;
            case AccessPathQueryabilityState::INVALID:
                return 1;
            case AccessPathQueryabilityState::UNKNOWN:
            default:
                return 0;
        }
    }

    auto weakerQueryability(AccessPathQueryabilityState left,
                            AccessPathQueryabilityState right)
        -> AccessPathQueryabilityState
    {
        return accessPathQueryabilityStrength(left) <=
                   accessPathQueryabilityStrength(right)
               ? left
               : right;
    }

    auto sameOrderingKey(const AccessPathDescriptor::OrderingKey& left,
                         const AccessPathDescriptor::OrderingKey& right) -> bool
    {
        return left.expression_text == right.expression_text &&
               left.descending == right.descending &&
               left.nulls_first == right.nulls_first &&
               left.comparator_family == right.comparator_family;
    }

    auto orderingPrefixMatches(
        const std::vector<AccessPathDescriptor::OrderingKey>& left_keys,
        const std::vector<AccessPathDescriptor::OrderingKey>& right_keys,
        size_t required_prefix) -> bool
    {
        if (required_prefix == 0)
        {
            return true;
        }
        if (left_keys.size() < required_prefix ||
            right_keys.size() < required_prefix)
        {
            return false;
        }
        for (size_t index = 0; index < required_prefix; ++index)
        {
            if (!sameOrderingKey(left_keys[index], right_keys[index]))
            {
                return false;
            }
        }
        return true;
    }

    auto orderingDominates(const AccessPathDescriptor& left,
                           const AccessPathDescriptor& right) -> bool
    {
        const size_t required_prefix =
            std::min<size_t>(right.ordered_prefix_length,
                             right.ordering_keys.size());
        if (!right.ordered_output && required_prefix == 0 &&
            right.ordering_keys.empty())
        {
            return left.interesting_order_score + kFrontierDominanceEpsilon >=
                   right.interesting_order_score;
        }

        const size_t available_prefix =
            std::min<size_t>(left.ordered_prefix_length,
                             left.ordering_keys.size());
        if (!orderingPrefixMatches(left.ordering_keys,
                                   right.ordering_keys,
                                   required_prefix))
        {
            return false;
        }
        if (available_prefix < required_prefix)
        {
            return false;
        }
        if (right.ordered_output && !left.ordered_output)
        {
            return false;
        }
        if (right.order_complete && !left.order_complete)
        {
            return false;
        }
        return left.interesting_order_score + kFrontierDominanceEpsilon >=
               right.interesting_order_score;
    }

    auto requiredOuterDominates(const std::vector<size_t>& left,
                                const std::vector<size_t>& right) -> bool
    {
        std::vector<size_t> left_sorted = left;
        std::vector<size_t> right_sorted = right;
        std::sort(left_sorted.begin(), left_sorted.end());
        left_sorted.erase(std::unique(left_sorted.begin(), left_sorted.end()),
                          left_sorted.end());
        std::sort(right_sorted.begin(), right_sorted.end());
        right_sorted.erase(
            std::unique(right_sorted.begin(), right_sorted.end()),
            right_sorted.end());
        return std::includes(right_sorted.begin(),
                             right_sorted.end(),
                             left_sorted.begin(),
                             left_sorted.end());
    }

    auto mergeRequiredOuter(const AccessPathDescriptor* left,
                            const AccessPathDescriptor* right)
        -> std::vector<size_t>
    {
        std::vector<size_t> merged;
        if (left != nullptr)
        {
            merged = left->required_outer_relation_indexes;
        }
        if (right != nullptr)
        {
            merged.insert(merged.end(),
                          right->required_outer_relation_indexes.begin(),
                          right->required_outer_relation_indexes.end());
        }
        std::sort(merged.begin(), merged.end());
        merged.erase(std::unique(merged.begin(), merged.end()), merged.end());
        return merged;
    }

    auto mergeFamilyTags(const AccessPathDescriptor* left,
                         const AccessPathDescriptor* right)
        -> std::vector<std::string>
    {
        std::vector<std::string> merged;
        if (left != nullptr)
        {
            merged = left->family_tags;
        }
        if (right != nullptr)
        {
            merged.insert(merged.end(),
                          right->family_tags.begin(),
                          right->family_tags.end());
        }
        std::sort(merged.begin(), merged.end());
        merged.erase(std::unique(merged.begin(), merged.end()), merged.end());
        return merged;
    }

    auto mergeMetricsConfidenceClass(const AccessPathDescriptor* left,
                                     const AccessPathDescriptor* right)
        -> std::string
    {
        const std::string left_value =
            left != nullptr ? left->metrics_confidence_class : std::string();
        const std::string right_value =
            right != nullptr ? right->metrics_confidence_class : std::string();
        if (left_value.empty())
        {
            return right_value;
        }
        if (right_value.empty() || right_value == left_value)
        {
            return left_value;
        }
        return "MIXED";
    }

    auto mergeCoverageFraction(const AccessPathDescriptor* left,
                               const AccessPathDescriptor* right) -> double
    {
        const double left_value =
            left != nullptr ? left->coverage_fraction : 0.0;
        const double right_value =
            right != nullptr ? right->coverage_fraction : 0.0;
        if (left == nullptr)
        {
            return right_value;
        }
        if (right == nullptr)
        {
            return left_value;
        }
        return std::min(left_value, right_value);
    }

    auto mergeParallelTextProperty(const std::string& left_value,
                                   const std::string& right_value,
                                   const std::string& mixed_value)
        -> std::string
    {
        if (left_value.empty())
        {
            return right_value;
        }
        if (right_value.empty() || right_value == left_value)
        {
            return left_value;
        }
        return mixed_value;
    }

    auto parallelOrderPreservationStrength(const std::string& value) -> int
    {
        if (value == "TOTAL_ORDER")
        {
            return 4;
        }
        if (value == "ORDERED_PREFIX")
        {
            return 3;
        }
        if (value == "PARTIAL_ORDER")
        {
            return 2;
        }
        if (value == "UNORDERED")
        {
            return 1;
        }
        return 0;
    }

    auto weakerParallelOrderPreservation(const std::string& left_value,
                                         const std::string& right_value)
        -> std::string
    {
        if (left_value.empty())
        {
            return right_value;
        }
        if (right_value.empty())
        {
            return left_value;
        }
        return parallelOrderPreservationStrength(left_value) <=
                   parallelOrderPreservationStrength(right_value)
               ? left_value
               : right_value;
    }

    auto mergeExchangeTopologyId(const AccessPathDescriptor* left,
                                 const AccessPathDescriptor* right)
        -> std::string
    {
        const std::string left_value =
            left != nullptr ? left->exchange_topology_id : std::string();
        const std::string right_value =
            right != nullptr ? right->exchange_topology_id : std::string();
        if (left_value.empty())
        {
            return right_value;
        }
        if (right_value.empty() || right_value == left_value)
        {
            return left_value;
        }
        return left_value + "+" + right_value;
    }

    auto mergeGatherDecisionReason(const AccessPathDescriptor* left,
                                   const AccessPathDescriptor* right)
        -> std::string
    {
        const std::string left_value =
            left != nullptr ? left->gather_decision_reason : std::string();
        const std::string right_value =
            right != nullptr ? right->gather_decision_reason : std::string();
        if (left_value.empty())
        {
            return right_value;
        }
        if (right_value.empty() || right_value == left_value)
        {
            return left_value;
        }
        return left_value + " | " + right_value;
    }

    auto combineJoinDescriptorCommon(const AccessPathDescriptor* left,
                                     const AccessPathDescriptor* right,
                                     AccessPathDescriptor& out) -> void
    {
        out.required_outer_relation_indexes = mergeRequiredOuter(left, right);
        out.family_tags = mergeFamilyTags(left, right);
        out.parameterized =
            (left != nullptr && left->parameterized) ||
            (right != nullptr && right->parameterized);
        out.requires_recheck =
            (left != nullptr && left->requires_recheck) ||
            (right != nullptr && right->requires_recheck);

        const auto left_exactness =
            left != nullptr ? left->exactness_class
                            : AccessPathExactnessClass::UNKNOWN;
        const auto right_exactness =
            right != nullptr ? right->exactness_class
                             : AccessPathExactnessClass::UNKNOWN;
        out.exactness_class = weakerExactness(left_exactness, right_exactness);

        const auto left_visibility =
            left != nullptr ? left->visibility_enforcement
                            : AccessPathVisibilityEnforcement::UNKNOWN;
        const auto right_visibility =
            right != nullptr ? right->visibility_enforcement
                             : AccessPathVisibilityEnforcement::UNKNOWN;
        out.visibility_enforcement =
            weakerVisibility(left_visibility, right_visibility);

        const auto left_queryability =
            left != nullptr ? left->queryability_state
                            : AccessPathQueryabilityState::UNKNOWN;
        const auto right_queryability =
            right != nullptr ? right->queryability_state
                             : AccessPathQueryabilityState::UNKNOWN;
        out.queryability_state =
            weakerQueryability(left_queryability, right_queryability);

        out.coverage_fraction = mergeCoverageFraction(left, right);
        out.family_metrics_version = std::max(
            left != nullptr ? left->family_metrics_version : 0U,
            right != nullptr ? right->family_metrics_version : 0U);
        out.metrics_confidence_class =
            mergeMetricsConfidenceClass(left, right);
        out.parallel_aware =
            (left != nullptr && left->parallel_aware) ||
            (right != nullptr && right->parallel_aware);
        out.parallel_enabled =
            (left != nullptr && left->parallel_enabled) ||
            (right != nullptr && right->parallel_enabled);
        out.parallel_workers_planned = std::max(
            left != nullptr ? left->parallel_workers_planned : 0U,
            right != nullptr ? right->parallel_workers_planned : 0U);
        out.parallel_stage = mergeParallelTextProperty(
            left != nullptr ? left->parallel_stage : std::string(),
            right != nullptr ? right->parallel_stage : std::string(),
            "MIXED_PARALLEL_STAGE");
        out.parallel_distribution_mode = mergeParallelTextProperty(
            left != nullptr ? left->parallel_distribution_mode : std::string(),
            right != nullptr ? right->parallel_distribution_mode : std::string(),
            "MIXED_DISTRIBUTION");
        out.parallel_order_preservation = weakerParallelOrderPreservation(
            left != nullptr ? left->parallel_order_preservation : std::string(),
            right != nullptr ? right->parallel_order_preservation
                             : std::string());
        out.exchange_topology_id = mergeExchangeTopologyId(left, right);
        out.gather_decision_reason = mergeGatherDecisionReason(left, right);
    }
} // namespace

JoinOrderingOptimizer::JoinOrderingOptimizer(CostModel& cost_model,
                                             SelectivityEstimator& selectivity_estimator)
    : cost_model_(cost_model),
      selectivity_estimator_(selectivity_estimator)
{
}

void JoinOrderingOptimizer::resetTelemetry()
{
    telemetry_ = JoinPlanningTelemetry{};
    telemetry_.requested_strategy = controls_.strategy;
    telemetry_.selected_strategy = controls_.strategy;
    telemetry_.search_contract_id = kJoinSearchContractId;
    telemetry_.property_signature_contract_id =
        kJoinSearchPropertySignatureContractId;
    telemetry_.frontier_retention_mode = kJoinSearchFrontierMode;
    telemetry_.strategy_source =
        controls_.strategy == JoinSearchStrategy::AUTO
            ? "AUTO_POLICY"
            : "EXPLICIT_POLICY";
    telemetry_.exhaustive_join_limit =
        std::min(MAX_DP_RELATIONS, std::max<size_t>(1, controls_.max_exhaustive_relations));
    telemetry_.bounded_dp_join_limit = std::min(
        MAX_DP_RELATIONS,
        std::max(telemetry_.exhaustive_join_limit,
                 std::max<size_t>(1, controls_.max_bounded_dp_relations)));
    telemetry_.max_pair_evaluations =
        std::max<size_t>(1, controls_.max_pair_evaluations);
    telemetry_.max_states_considered =
        std::max<size_t>(1, controls_.max_states_considered);
    telemetry_.fallback_prune_level = controls_.fallback_prune_level;
}

void JoinOrderingOptimizer::recordFallback(const std::string& reason,
                                           const std::string& threshold_name,
                                           size_t threshold_value)
{
    telemetry_.fallback_reason = reason;
    telemetry_.fallback_threshold_name = threshold_name;
    telemetry_.fallback_threshold_value = threshold_value;
}

JoinSearchStrategy JoinOrderingOptimizer::chooseHeuristicStrategy() const
{
    if (relations_.size() <= 2)
    {
        return JoinSearchStrategy::HEURISTIC_GREEDY;
    }

    const double max_edges = static_cast<double>(
        (relations_.size() * (relations_.size() - 1)) / 2);
    const double density = max_edges > 0.0
        ? static_cast<double>(join_edges_.size()) / max_edges
        : 1.0;

    if (controls_.fallback_prune_level >= 2 || density < 0.45)
    {
        return JoinSearchStrategy::HEURISTIC_GREEDY;
    }

    return JoinSearchStrategy::HYPERGRAPH_GREEDY;
}

auto joinSearchPropertySignature(const AccessPathDescriptor& descriptor)
    -> std::string
{
    std::ostringstream key;
    key << static_cast<uint32_t>(descriptor.family_kind) << ':';
    appendSignatureText(key, descriptor.family);
    appendSignatureText(key, descriptor.path_name);
    key << static_cast<uint32_t>(descriptor.exactness_class) << ':'
        << static_cast<uint32_t>(descriptor.visibility_enforcement) << ':'
        << static_cast<uint32_t>(descriptor.queryability_state) << ':'
        << descriptor.family_metrics_version << ':'
        << static_cast<uint32_t>(descriptor.requires_recheck ? 1U : 0U) << ':'
        << static_cast<uint32_t>(descriptor.parameterized ? 1U : 0U) << ':'
        << static_cast<uint32_t>(descriptor.parallel_aware ? 1U : 0U) << ':'
        << static_cast<uint32_t>(descriptor.parallel_enabled ? 1U : 0U) << ':'
        << descriptor.parallel_workers_planned << ':'
        << static_cast<uint32_t>(descriptor.gather_merge ? 1U : 0U) << ':'
        << static_cast<uint32_t>(descriptor.ordered_output ? 1U : 0U) << ':'
        << descriptor.ordered_prefix_length << ':'
        << static_cast<uint32_t>(descriptor.order_complete ? 1U : 0U) << ':'
        << quantizePropertyScore(descriptor.coverage_fraction) << ':'
        << quantizePropertyScore(descriptor.interesting_order_score) << ':';
    appendSignatureText(key, descriptor.parallel_stage);
    appendSignatureText(key, descriptor.parallel_distribution_mode);
    appendSignatureText(key, descriptor.parallel_order_preservation);
    appendSignatureText(key, descriptor.exchange_topology_id);
    appendSignatureText(key, descriptor.metrics_confidence_class);

    key << descriptor.ordering_keys.size() << ':';
    for (const auto& ordering_key : descriptor.ordering_keys)
    {
        appendSignatureText(key, ordering_key.expression_text);
        key << static_cast<uint32_t>(ordering_key.descending ? 1U : 0U) << ':'
            << static_cast<uint32_t>(ordering_key.nulls_first ? 1U : 0U) << ':';
        appendSignatureText(key, ordering_key.comparator_family);
    }

    auto family_tags = descriptor.family_tags;
    std::sort(family_tags.begin(), family_tags.end());
    key << family_tags.size() << ':';
    for (const auto& tag : family_tags)
    {
        appendSignatureText(key, tag);
    }

    key << descriptor.required_outer_relation_indexes.size() << ':';
    for (size_t relation_index : descriptor.required_outer_relation_indexes)
    {
        key << relation_index << ',';
    }
    return key.str();
}

auto JoinOrderingOptimizer::frontierSignature(const DPEntry& entry) const
    -> std::string
{
    if (!entry.best_path)
    {
        return "NULL";
    }

    std::ostringstream key;
    key << entry.relation_set_mask << '|'
        << joinSearchPropertySignature(entry.best_path->accessDescriptor());
    return key.str();
}

auto JoinOrderingOptimizer::frontierBucketSignature(const DPEntry& entry) const
    -> std::string
{
    if (!entry.best_path)
    {
        return "NULL";
    }

    const auto& descriptor = entry.best_path->accessDescriptor();
    std::ostringstream key;
    key << entry.relation_set_mask << ':'
        << static_cast<uint32_t>(descriptor.family_kind) << ':';
    appendSignatureText(key, descriptor.family);
    appendSignatureText(key, descriptor.path_name);
    key << descriptor.family_metrics_version << ':'
        << static_cast<uint32_t>(descriptor.parallel_aware ? 1U : 0U) << ':'
        << static_cast<uint32_t>(descriptor.parallel_enabled ? 1U : 0U) << ':'
        << descriptor.parallel_workers_planned << ':'
        << static_cast<uint32_t>(descriptor.gather_merge ? 1U : 0U) << ':';
    appendSignatureText(key, descriptor.parallel_stage);
    appendSignatureText(key, descriptor.parallel_distribution_mode);
    appendSignatureText(key, descriptor.parallel_order_preservation);
    appendSignatureText(key, descriptor.exchange_topology_id);
    appendSignatureText(key, descriptor.metrics_confidence_class);
    key << descriptor.ordering_keys.size() << ':';
    for (const auto& ordering_key : descriptor.ordering_keys)
    {
        appendSignatureText(key, ordering_key.expression_text);
        key << static_cast<uint32_t>(ordering_key.descending ? 1U : 0U) << ':'
            << static_cast<uint32_t>(ordering_key.nulls_first ? 1U : 0U) << ':';
        appendSignatureText(key, ordering_key.comparator_family);
    }
    auto family_tags = descriptor.family_tags;
    std::sort(family_tags.begin(), family_tags.end());
    key << family_tags.size() << ':';
    for (const auto& tag : family_tags)
    {
        appendSignatureText(key, tag);
    }
    return key.str();
}

auto JoinOrderingOptimizer::frontierEntryDominates(const DPEntry& left,
                                                   const DPEntry& right) const
    -> bool
{
    if (!left.best_path || !right.best_path)
    {
        return false;
    }
    if (frontierBucketSignature(left) != frontierBucketSignature(right))
    {
        return false;
    }
    if (left.cost > right.cost + kFrontierDominanceEpsilon)
    {
        return false;
    }
    if (left.rows > right.rows)
    {
        return false;
    }

    const auto& left_descriptor = left.best_path->accessDescriptor();
    const auto& right_descriptor = right.best_path->accessDescriptor();
    if (accessPathExactnessStrength(left_descriptor.exactness_class) <
        accessPathExactnessStrength(right_descriptor.exactness_class))
    {
        return false;
    }
    if (accessPathVisibilityStrength(left_descriptor.visibility_enforcement) <
        accessPathVisibilityStrength(right_descriptor.visibility_enforcement))
    {
        return false;
    }
    if (accessPathQueryabilityStrength(left_descriptor.queryability_state) <
        accessPathQueryabilityStrength(right_descriptor.queryability_state))
    {
        return false;
    }
    if (left_descriptor.requires_recheck &&
        !right_descriptor.requires_recheck)
    {
        return false;
    }
    if (left_descriptor.coverage_fraction + kFrontierDominanceEpsilon <
        right_descriptor.coverage_fraction)
    {
        return false;
    }
    if (!requiredOuterDominates(left_descriptor.required_outer_relation_indexes,
                                right_descriptor.required_outer_relation_indexes))
    {
        return false;
    }
    if (!orderingDominates(left_descriptor, right_descriptor))
    {
        return false;
    }
    return true;
}

void JoinOrderingOptimizer::pushFrontierEntry(DPFrontier& frontier, DPEntry entry)
{
    if (!entry.best_path)
    {
        return;
    }

    if (entry.cost == std::numeric_limits<double>::max())
    {
        entry.cost = entry.best_path->totalCost();
    }
    if (entry.rows == 0)
    {
        entry.rows = entry.best_path->rows();
    }

    const std::string entry_signature = frontierSignature(entry);
    for (auto it = frontier.begin(); it != frontier.end();)
    {
        const bool same_signature =
            frontierSignature(*it) == entry_signature;
        const bool existing_dominates =
            frontierEntryDominates(*it, entry) ||
            (same_signature &&
             it->cost <= entry.cost + kFrontierDominanceEpsilon &&
             it->rows <= entry.rows);
        if (existing_dominates)
        {
            ++telemetry_.dominated_state_count;
            ++telemetry_.pruned_state_count;
            return;
        }
        const bool entry_dominates =
            frontierEntryDominates(entry, *it) ||
            (same_signature &&
             entry.cost <= it->cost + kFrontierDominanceEpsilon &&
             entry.rows <= it->rows);
        if (entry_dominates)
        {
            ++telemetry_.dominated_state_count;
            ++telemetry_.pruned_state_count;
            it = frontier.erase(it);
            continue;
        }
        ++it;
    }

    frontier.push_back(std::move(entry));
    std::sort(frontier.begin(),
              frontier.end(),
              [](const DPEntry& left, const DPEntry& right) {
                  if (left.cost == right.cost)
                  {
                      return left.rows < right.rows;
                  }
                  return left.cost < right.cost;
              });
    ++telemetry_.retained_frontier_entry_count;
    telemetry_.max_frontier_width =
        std::max(telemetry_.max_frontier_width, frontier.size());
}

auto JoinOrderingOptimizer::frontierBestEntry(const DPFrontier& frontier) const
    -> const DPEntry*
{
    if (frontier.empty())
    {
        return nullptr;
    }
    return &frontier.front();
}

auto JoinOrderingOptimizer::frontierBestPath(const DPFrontier& frontier) const
    -> std::shared_ptr<Path>
{
    const auto* best_entry = frontierBestEntry(frontier);
    return best_entry != nullptr ? best_entry->best_path : nullptr;
}

auto JoinOrderingOptimizer::baseFrontierForRelation(const RelationInfo& relation,
                                                    size_t relation_index)
    -> DPFrontier
{
    DPFrontier frontier;
    for (const auto& candidate_path : relation.candidate_paths)
    {
        if (!candidate_path)
        {
            continue;
        }
        DPEntry entry;
        entry.best_path = candidate_path;
        entry.cost = candidate_path->totalCost();
        entry.rows = candidate_path->rows();
        entry.relation_set_mask = 1ULL << relation_index;
        pushFrontierEntry(frontier, std::move(entry));
    }

    if (frontier.empty() && relation.best_path)
    {
        DPEntry entry;
        entry.best_path = relation.best_path;
        entry.cost = relation.best_path->totalCost();
        entry.rows = relation.best_path->rows();
        entry.relation_set_mask = 1ULL << relation_index;
        pushFrontierEntry(frontier, std::move(entry));
    }
    return frontier;
}

size_t JoinOrderingOptimizer::addRelation(const core::ID& table_id,
                                          const std::string& table_name,
                                          const std::string& alias,
                                          std::shared_ptr<Path> best_path)
{
    std::vector<std::shared_ptr<Path>> bundle;
    if (best_path)
    {
        bundle.push_back(std::move(best_path));
    }
    return addRelation(table_id, table_name, alias, bundle);
}

size_t JoinOrderingOptimizer::addRelation(const core::ID& table_id,
                                          const std::string& table_name,
                                          const std::string& alias,
                                          const std::vector<std::shared_ptr<Path>>& candidate_paths)
{
    RelationInfo info;
    info.table_id = table_id;
    info.table_name = table_name;
    info.alias = alias;
    info.candidate_paths = candidate_paths;
    auto frontier = baseFrontierForRelation(info, relations_.size());
    telemetry_.base_candidate_path_count += frontier.size();
    if (const auto* best_entry = frontierBestEntry(frontier))
    {
        info.best_path = best_entry->best_path;
        info.best_path_rows = best_entry->rows;
        info.rows = best_entry->rows;
    }
    else
    {
        info.rows = 1000;
    }
    info.width = 100;  // Default row width estimate

    size_t idx = relations_.size();
    relations_.push_back(std::move(info));

    DEBUG_LOG_DB("JoinOrdering: Added relation " + std::to_string(idx) +
                 " (" + table_name + "), rows=" + std::to_string(info.rows));

    return idx;
}

size_t JoinOrderingOptimizer::addJoinEdge(size_t left_idx, size_t right_idx,
                                          parser::JoinType join_type,
                                          parser::v3::Expression* join_condition,
                                          size_t source_join_index,
                                          JoinLegalityClass legality_class,
                                          bool reorderable,
                                          bool requires_original_order,
                                          bool natural_barrier,
                                          bool using_barrier,
                                          bool equi_join,
                                          bool has_key_metadata,
                                          const std::string& left_order_key_text,
                                          const std::string& right_order_key_text)
{
    if (legality_class == JoinLegalityClass::INNER_REORDERABLE &&
        reorderable &&
        !requires_original_order)
    {
        const auto derived = classifyJoinLegality(join_type, false, false);
        legality_class = derived.legality_class;
        reorderable = derived.reorderable;
        requires_original_order = derived.requires_original_order;
    }

    JoinEdge edge;
    edge.left_rel_idx = left_idx;
    edge.right_rel_idx = right_idx;
    edge.join_type = join_type;
    edge.join_condition = join_condition;
    edge.selectivity = 0.1;  // Default selectivity estimate
    edge.source_join_index = source_join_index;
    edge.legality_class = legality_class;
    edge.reorderable = reorderable;
    edge.requires_original_order = requires_original_order;
    edge.natural_barrier = natural_barrier;
    edge.using_barrier = using_barrier;
    edge.equi_join = equi_join;
    edge.has_key_metadata = has_key_metadata;
    edge.left_order_key_text = left_order_key_text;
    edge.right_order_key_text = right_order_key_text;

    join_edges_.push_back(std::move(edge));

    DEBUG_LOG_DB("JoinOrdering: Added join edge between " +
                 std::to_string(left_idx) + " and " + std::to_string(right_idx));

    return join_edges_.size() - 1;
}

void JoinOrderingOptimizer::setJoinSelectivity(size_t edge_idx, double selectivity)
{
    if (edge_idx < join_edges_.size())
    {
        join_edges_[edge_idx].selectivity = selectivity;
    }
}

void JoinOrderingOptimizer::clear()
{
    relations_.clear();
    join_edges_.clear();
    dp_table_.clear();
    last_strategy_used_ = controls_.strategy;
    resetTelemetry();
}

std::shared_ptr<Path> JoinOrderingOptimizer::optimize(core::ErrorContext* ctx)
{
    resetTelemetry();

    if (relations_.empty())
    {
        DEBUG_LOG_DB("JoinOrdering: No relations to optimize");
        return nullptr;
    }

    // Single relation - just return its best path
    if (relations_.size() == 1)
    {
        DEBUG_LOG_DB("JoinOrdering: Single relation, returning best path");
        last_strategy_used_ = JoinSearchStrategy::AUTO;
        telemetry_.selected_strategy = last_strategy_used_;
        return frontierBestPath(baseFrontierForRelation(relations_[0], 0));
    }

    if (hasJoinReorderBarrier())
    {
        DEBUG_LOG_DB("JoinOrdering: Reorder barrier detected, preserving input join order");
        last_strategy_used_ = JoinSearchStrategy::INPUT_ORDER;
        telemetry_.selected_strategy = last_strategy_used_;
        if (controls_.strategy != JoinSearchStrategy::INPUT_ORDER)
        {
            recordFallback("JOIN_REORDER_BARRIER", "JOIN_REORDER_BARRIER", 1);
        }
        return optimizePreservingInputOrder(ctx);
    }

    if (controls_.strategy == JoinSearchStrategy::INPUT_ORDER)
    {
        DEBUG_LOG_DB("JoinOrdering: Explicit input-order strategy requested");
        last_strategy_used_ = JoinSearchStrategy::INPUT_ORDER;
        telemetry_.selected_strategy = last_strategy_used_;
        return optimizePreservingInputOrder(ctx);
    }

    const size_t exhaustive_cap = telemetry_.exhaustive_join_limit;
    const size_t bounded_cap = telemetry_.bounded_dp_join_limit;

    if (controls_.strategy == JoinSearchStrategy::HYPERGRAPH_GREEDY)
    {
        DEBUG_LOG_DB("JoinOrdering: Explicit hypergraph-greedy strategy requested");
        last_strategy_used_ = JoinSearchStrategy::HYPERGRAPH_GREEDY;
        telemetry_.selected_strategy = last_strategy_used_;
        telemetry_.strategy_source = "EXPLICIT_POLICY";
        return optimizeHypergraphGreedy(ctx);
    }

    if (controls_.strategy == JoinSearchStrategy::HEURISTIC_GREEDY)
    {
        DEBUG_LOG_DB("JoinOrdering: Explicit heuristic-greedy strategy requested");
        last_strategy_used_ = JoinSearchStrategy::HEURISTIC_GREEDY;
        telemetry_.selected_strategy = last_strategy_used_;
        telemetry_.strategy_source = "EXPLICIT_POLICY";
        return optimizeGreedy(ctx);
    }

    if (controls_.strategy == JoinSearchStrategy::EXHAUSTIVE_DP)
    {
        if (relations_.size() <= exhaustive_cap)
        {
            last_strategy_used_ = JoinSearchStrategy::EXHAUSTIVE_DP;
            telemetry_.selected_strategy = last_strategy_used_;
            telemetry_.strategy_source = "EXPLICIT_POLICY";
        }
        else
        {
            recordFallback("EXHAUSTIVE_JOIN_LIMIT",
                           "EXHAUSTIVE_JOIN_LIMIT",
                           exhaustive_cap);
        }
    }

    if ((controls_.strategy == JoinSearchStrategy::EXHAUSTIVE_DP &&
         relations_.size() <= exhaustive_cap) ||
        (controls_.strategy == JoinSearchStrategy::AUTO &&
         relations_.size() <= exhaustive_cap))
    {
        last_strategy_used_ = JoinSearchStrategy::EXHAUSTIVE_DP;
        telemetry_.selected_strategy = last_strategy_used_;
    }

    if (last_strategy_used_ == JoinSearchStrategy::EXHAUSTIVE_DP)
    {
        DEBUG_LOG_DB("JoinOrdering: Starting exhaustive DP optimization for " +
                     std::to_string(relations_.size()) + " relations");

        dp_table_.clear();
        for (size_t i = 0; i < relations_.size(); ++i)
        {
            RelationSet singleton = 1ULL << i;
            dp_table_[singleton] = baseFrontierForRelation(relations_[i], i);
        }

        RelationSet full_set = (1ULL << relations_.size()) - 1;
        for (size_t size = 2; size <= relations_.size(); ++size)
        {
            RelationSet subset = (1ULL << size) - 1;
            while (subset <= full_set)
            {
                if (countBits(subset) == size)
                {
                    ++telemetry_.considered_state_count;
                    DPFrontier frontier = generateSubsetPlans(subset, ctx);
                    if (!frontier.empty())
                    {
                        dp_table_[subset] = std::move(frontier);
                    }
                }

                if (subset == 0) break;
                RelationSet c = subset & -static_cast<int64_t>(subset);
                RelationSet r = subset + c;
                subset = (((r ^ subset) >> 2) / c) | r;
                if (subset > full_set) break;
            }
        }

        auto it = dp_table_.find(full_set);
        if (it != dp_table_.end())
        {
            return frontierBestPath(it->second);
        }

        recordFallback("EXHAUSTIVE_PLAN_INCOMPLETE", "FULL_SET", relations_.size());
    }

    if ((controls_.strategy == JoinSearchStrategy::BOUNDED_DP &&
         relations_.size() > bounded_cap) ||
        (controls_.strategy == JoinSearchStrategy::AUTO &&
         relations_.size() > bounded_cap) ||
        (controls_.strategy == JoinSearchStrategy::EXHAUSTIVE_DP &&
         relations_.size() > bounded_cap))
    {
        if (telemetry_.fallback_reason.empty())
        {
            recordFallback("BOUNDED_DP_JOIN_LIMIT",
                           "BOUNDED_DP_JOIN_LIMIT",
                           bounded_cap);
        }

        const auto heuristic_strategy = chooseHeuristicStrategy();
        last_strategy_used_ = heuristic_strategy;
        telemetry_.selected_strategy = last_strategy_used_;
        return heuristic_strategy == JoinSearchStrategy::HEURISTIC_GREEDY
            ? optimizeGreedy(ctx)
            : optimizeHypergraphGreedy(ctx);
    }

    if (controls_.strategy == JoinSearchStrategy::BOUNDED_DP ||
        controls_.strategy == JoinSearchStrategy::AUTO ||
        controls_.strategy == JoinSearchStrategy::EXHAUSTIVE_DP)
    {
        last_strategy_used_ = JoinSearchStrategy::BOUNDED_DP;
        telemetry_.selected_strategy = last_strategy_used_;
        return optimizeBoundedDP(ctx);
    }

    last_strategy_used_ = chooseHeuristicStrategy();
    telemetry_.selected_strategy = last_strategy_used_;
    return last_strategy_used_ == JoinSearchStrategy::HEURISTIC_GREEDY
        ? optimizeGreedy(ctx)
        : optimizeHypergraphGreedy(ctx);
}

std::shared_ptr<Path> JoinOrderingOptimizer::optimizeBoundedDP(
    core::ErrorContext* ctx)
{
    dp_table_.clear();
    for (size_t i = 0; i < relations_.size(); ++i)
    {
        RelationSet singleton = 1ULL << i;
        dp_table_[singleton] = baseFrontierForRelation(relations_[i], i);
    }

    RelationSet full_set = (1ULL << relations_.size()) - 1;
    size_t pair_budget_remaining = telemetry_.max_pair_evaluations;
    const size_t state_limit = telemetry_.max_states_considered;

    for (size_t size = 2; size <= relations_.size(); ++size)
    {
        RelationSet subset = (1ULL << size) - 1;
        while (subset <= full_set)
        {
            if (countBits(subset) == size)
            {
                if (telemetry_.considered_state_count >= state_limit)
                {
                    recordFallback("MAX_STATES_CONSIDERED",
                                   "MAX_STATES_CONSIDERED",
                                   state_limit);
                    const auto heuristic_strategy = chooseHeuristicStrategy();
                    last_strategy_used_ = heuristic_strategy;
                    telemetry_.selected_strategy = last_strategy_used_;
                    return heuristic_strategy == JoinSearchStrategy::HEURISTIC_GREEDY
                        ? optimizeGreedy(ctx)
                        : optimizeHypergraphGreedy(ctx);
                }

                ++telemetry_.considered_state_count;
                const size_t pair_count_before = telemetry_.pair_evaluation_count;
                DPFrontier frontier = generateSubsetPlans(subset, ctx);
                const size_t pair_delta =
                    telemetry_.pair_evaluation_count - pair_count_before;
                if (pair_delta > pair_budget_remaining)
                {
                    pair_budget_remaining = 0;
                }
                else
                {
                    pair_budget_remaining -= pair_delta;
                }
                if (!frontier.empty())
                {
                    dp_table_[subset] = std::move(frontier);
                }
                if (pair_budget_remaining == 0 && subset != full_set)
                {
                    recordFallback("MAX_PAIR_EVALUATIONS",
                                   "MAX_PAIR_EVALUATIONS",
                                   telemetry_.max_pair_evaluations);
                    const auto heuristic_strategy = chooseHeuristicStrategy();
                    last_strategy_used_ = heuristic_strategy;
                    telemetry_.selected_strategy = last_strategy_used_;
                    return heuristic_strategy == JoinSearchStrategy::HEURISTIC_GREEDY
                        ? optimizeGreedy(ctx)
                        : optimizeHypergraphGreedy(ctx);
                }
            }

            if (subset == 0) break;
            RelationSet c = subset & -static_cast<int64_t>(subset);
            RelationSet r = subset + c;
            subset = (((r ^ subset) >> 2) / c) | r;
            if (subset > full_set) break;
        }
    }

    auto it = dp_table_.find(full_set);
    if (it != dp_table_.end())
    {
        last_strategy_used_ = JoinSearchStrategy::BOUNDED_DP;
        telemetry_.selected_strategy = last_strategy_used_;
        return frontierBestPath(it->second);
    }

    if (telemetry_.fallback_reason.empty())
    {
        recordFallback("BOUNDED_DP_INCOMPLETE", "FULL_SET", relations_.size());
    }
    const auto heuristic_strategy = chooseHeuristicStrategy();
    last_strategy_used_ = heuristic_strategy;
    telemetry_.selected_strategy = last_strategy_used_;
    return heuristic_strategy == JoinSearchStrategy::HEURISTIC_GREEDY
        ? optimizeGreedy(ctx)
        : optimizeHypergraphGreedy(ctx);
}

std::shared_ptr<Path> JoinOrderingOptimizer::optimizeGreedy(core::ErrorContext* ctx)
{
    if (relations_.empty())
    {
        return nullptr;
    }

    if (relations_.size() == 1)
    {
        return frontierBestPath(baseFrontierForRelation(relations_[0], 0));
    }

    if (hasJoinReorderBarrier())
    {
        DEBUG_LOG_DB("JoinOrdering: Greedy path disabled by reorder barrier, preserving input join order");
        last_strategy_used_ = JoinSearchStrategy::INPUT_ORDER;
        telemetry_.selected_strategy = last_strategy_used_;
        return optimizePreservingInputOrder(ctx);
    }

    DEBUG_LOG_DB("JoinOrdering: Starting greedy optimization");
    last_strategy_used_ = JoinSearchStrategy::HEURISTIC_GREEDY;
    telemetry_.selected_strategy = last_strategy_used_;

    // Track which relations have been joined
    std::vector<bool> joined(relations_.size(), false);

    // Start with the relation that has the smallest estimated rows
    size_t best_start = 0;
    uint64_t min_rows = std::numeric_limits<uint64_t>::max();
    for (size_t i = 0; i < relations_.size(); ++i)
    {
        const auto frontier = baseFrontierForRelation(relations_[i], i);
        const auto* best_entry = frontierBestEntry(frontier);
        if (best_entry != nullptr && best_entry->rows < min_rows)
        {
            min_rows = best_entry->rows;
            best_start = i;
        }
    }
    if (min_rows == std::numeric_limits<uint64_t>::max())
    {
        const auto frontier = baseFrontierForRelation(relations_[0], 0);
        const auto* best_entry = frontierBestEntry(frontier);
        min_rows = best_entry != nullptr ? best_entry->rows : 0;
    }

    joined[best_start] = true;
    auto current_frontier =
        baseFrontierForRelation(relations_[best_start], best_start);
    const auto* current_best = frontierBestEntry(current_frontier);
    if (current_best == nullptr)
    {
        return nullptr;
    }
    DPEntry current_entry = *current_best;

    DEBUG_LOG_DB("JoinOrdering: Greedy start with " + relations_[best_start].table_name);

    // Greedily add remaining relations
    for (size_t step = 1; step < relations_.size(); ++step)
    {
        // Find the next best relation to join
        double best_cost = std::numeric_limits<double>::max();
        uint64_t best_rows = std::numeric_limits<uint64_t>::max();
        size_t best_rel = 0;
        size_t best_frontier_size = 0;
        DPFrontier best_frontier;
        size_t round_candidates = 0;

        for (size_t i = 0; i < relations_.size(); ++i)
        {
            if (joined[i]) continue;
            const auto relation_frontier =
                baseFrontierForRelation(relations_[i], i);
            if (relation_frontier.empty())
            {
                continue;
            }

            DPFrontier candidate_frontier;

            // Find join edge connecting current set to this relation
            for (size_t e = 0; e < join_edges_.size(); ++e)
            {
                const JoinEdge& edge = join_edges_[e];
                bool connects = false;

                // Check if this edge connects to relation i
                if ((edge.left_rel_idx == i && !joined[edge.left_rel_idx]) ||
                    (edge.right_rel_idx == i && !joined[edge.right_rel_idx]))
                {
                    // Check if other side is in current set
                    if ((edge.left_rel_idx == i && joined[edge.right_rel_idx]) ||
                        (edge.right_rel_idx == i && joined[edge.left_rel_idx]))
                    {
                        connects = true;
                    }
                }

                if (!connects) continue;
                bool evaluated_edge = false;
                for (const auto& left_entry : current_frontier)
                for (const auto& relation_entry : relation_frontier)
                {
                    ++telemetry_.pair_evaluation_count;
                    ++round_candidates;
                    const auto join_entry = costJoin(left_entry,
                                                     relation_entry,
                                                     edge,
                                                     ctx,
                                                     true);
                    pushFrontierEntry(candidate_frontier, join_entry);
                    evaluated_edge = true;
                }
                if (evaluated_edge)
                {
                    continue;
                }
            }

            if (!candidate_frontier.empty())
            {
                const auto* candidate_best = frontierBestEntry(candidate_frontier);
                if (candidate_best != nullptr &&
                    (candidate_best->cost < best_cost ||
                     (candidate_best->cost == best_cost &&
                      candidate_best->rows < best_rows) ||
                     (candidate_best->cost == best_cost &&
                      candidate_best->rows == best_rows &&
                      candidate_frontier.size() > best_frontier_size)))
                {
                    best_cost = candidate_best->cost;
                    best_rows = candidate_best->rows;
                    best_rel = i;
                    best_frontier_size = candidate_frontier.size();
                    best_frontier = std::move(candidate_frontier);
                }
            }
        }

        if (best_frontier.empty())
        {
            // No connecting edge found - bridge the next disconnected component
            // with the cheapest explicit cross join.
            for (size_t i = 0; i < relations_.size(); ++i)
            {
                if (joined[i]) continue;
                const auto relation_frontier =
                    baseFrontierForRelation(relations_[i], i);
                DPFrontier candidate_frontier;
                for (const auto& left_entry : current_frontier)
                {
                    for (const auto& relation_entry : relation_frontier)
                    {
                        ++telemetry_.pair_evaluation_count;
                        ++round_candidates;
                        const auto join_entry =
                            costCrossJoin(left_entry, relation_entry, ctx);
                        pushFrontierEntry(candidate_frontier, join_entry);
                    }
                }

                if (candidate_frontier.empty())
                {
                    continue;
                }

                const auto* candidate_best = frontierBestEntry(candidate_frontier);
                if (candidate_best != nullptr &&
                    (candidate_best->cost < best_cost ||
                     (candidate_best->cost == best_cost &&
                      candidate_best->rows < best_rows) ||
                     (candidate_best->cost == best_cost &&
                      candidate_best->rows == best_rows &&
                      candidate_frontier.size() > best_frontier_size)))
                {
                    best_cost = candidate_best->cost;
                    best_rows = candidate_best->rows;
                    best_rel = i;
                    best_frontier_size = candidate_frontier.size();
                    best_frontier = std::move(candidate_frontier);
                }
            }
        }

        if (!best_frontier.empty())
        {
            joined[best_rel] = true;
            current_frontier = std::move(best_frontier);
            current_best = frontierBestEntry(current_frontier);
            if (current_best == nullptr)
            {
                return nullptr;
            }
            current_entry = *current_best;

            DEBUG_LOG_DB("JoinOrdering: Greedy added " + relations_[best_rel].table_name +
                         ", total cost=" + std::to_string(current_entry.cost));
        }

        telemetry_.considered_state_count += round_candidates;
    }

    return frontierBestPath(current_frontier);
}

std::shared_ptr<Path> JoinOrderingOptimizer::optimizeHypergraphGreedy(
    core::ErrorContext* ctx)
{
    if (relations_.empty())
    {
        return nullptr;
    }

    if (relations_.size() == 1)
    {
        return frontierBestPath(baseFrontierForRelation(relations_[0], 0));
    }

    if (hasJoinReorderBarrier())
    {
        last_strategy_used_ = JoinSearchStrategy::INPUT_ORDER;
        telemetry_.selected_strategy = last_strategy_used_;
        return optimizePreservingInputOrder(ctx);
    }

    struct Component
    {
        RelationSet relation_set = 0;
        DPFrontier frontier;
    };

    std::vector<Component> components;
    components.reserve(relations_.size());
    for (size_t i = 0; i < relations_.size(); ++i)
    {
        Component component;
        component.relation_set = 1ULL << i;
        component.frontier = baseFrontierForRelation(relations_[i], i);
        components.push_back(std::move(component));
    }

    const size_t max_pair_evaluations =
        std::max<size_t>(1, controls_.max_pair_evaluations);

    while (components.size() > 1)
    {
        bool found_connected_merge = false;
        size_t best_left = 0;
        size_t best_right = 0;
        DPFrontier best_frontier;
        double best_cost = std::numeric_limits<double>::max();
        size_t best_frontier_size = 0;
        size_t pair_evaluations = 0;
        size_t round_candidates = 0;

        for (size_t left = 0; left < components.size(); ++left)
        {
            for (size_t right = left + 1; right < components.size(); ++right)
            {
                if (pair_evaluations >= max_pair_evaluations)
                {
                    break;
                }
                ++pair_evaluations;
                ++telemetry_.pair_evaluation_count;

                const auto connecting_edges =
                    findConnectingEdges(components[left].relation_set,
                                        components[right].relation_set);
                if (connecting_edges.empty())
                {
                    continue;
                }

                for (size_t edge_idx : connecting_edges)
                {
                    DPFrontier candidate_frontier;
                    for (const auto& left_entry : components[left].frontier)
                    {
                        for (const auto& right_entry : components[right].frontier)
                        {
                            ++round_candidates;
                            ++telemetry_.pair_evaluation_count;
                            pushFrontierEntry(candidate_frontier,
                                              costJoin(left_entry,
                                                       right_entry,
                                                       join_edges_[edge_idx],
                                                       ctx));
                        }
                    }
                    const auto* best_entry = frontierBestEntry(candidate_frontier);
                    if (best_entry == nullptr)
                    {
                        continue;
                    }
                    if (best_entry->cost > best_cost ||
                        (best_entry->cost == best_cost &&
                         candidate_frontier.size() <= best_frontier_size))
                    {
                        continue;
                    }
                    best_cost = best_entry->cost;
                    best_frontier_size = candidate_frontier.size();
                    best_frontier = std::move(candidate_frontier);
                    best_left = left;
                    best_right = right;
                    found_connected_merge = true;
                }
            }
            if (pair_evaluations >= max_pair_evaluations)
            {
                break;
            }
        }

        if (!found_connected_merge)
        {
            best_frontier.clear();
            best_cost = std::numeric_limits<double>::max();
            best_frontier_size = 0;
            for (size_t left = 0; left < components.size(); ++left)
            {
                for (size_t right = left + 1; right < components.size(); ++right)
                {
                    DPFrontier candidate_frontier;
                    for (const auto& left_entry : components[left].frontier)
                    {
                        for (const auto& right_entry : components[right].frontier)
                        {
                            ++telemetry_.pair_evaluation_count;
                            ++round_candidates;
                            pushFrontierEntry(candidate_frontier,
                                              costCrossJoin(left_entry,
                                                            right_entry,
                                                            ctx));
                        }
                    }
                    const auto* best_entry = frontierBestEntry(candidate_frontier);
                    if (best_entry == nullptr)
                    {
                        continue;
                    }
                    if (best_entry->cost > best_cost ||
                        (best_entry->cost == best_cost &&
                         candidate_frontier.size() <= best_frontier_size))
                    {
                        continue;
                    }
                    best_cost = best_entry->cost;
                    best_frontier_size = candidate_frontier.size();
                    best_frontier = std::move(candidate_frontier);
                    best_left = left;
                    best_right = right;
                }
            }
        }

        if (best_frontier.empty() || best_left == best_right)
        {
            recordFallback("HEURISTIC_NO_CONNECTED_CANDIDATE",
                           "MAX_PAIR_EVALUATIONS",
                           max_pair_evaluations);
            return optimizeGreedy(ctx);
        }

        telemetry_.considered_state_count += round_candidates;

        Component merged;
        merged.relation_set =
            components[best_left].relation_set | components[best_right].relation_set;
        merged.frontier = std::move(best_frontier);

        if (best_left > best_right)
        {
            std::swap(best_left, best_right);
        }
        components.erase(components.begin() + static_cast<std::ptrdiff_t>(best_right));
        components.erase(components.begin() + static_cast<std::ptrdiff_t>(best_left));
        components.push_back(std::move(merged));
    }

    last_strategy_used_ = JoinSearchStrategy::HYPERGRAPH_GREEDY;
    telemetry_.selected_strategy = last_strategy_used_;
    return frontierBestPath(components.front().frontier);
}

std::shared_ptr<Path> JoinOrderingOptimizer::optimizePreservingInputOrder(
    core::ErrorContext* ctx)
{
    if (relations_.empty())
    {
        return nullptr;
    }

    if (relations_.size() == 1)
    {
        return frontierBestPath(baseFrontierForRelation(relations_[0], 0));
    }

    std::vector<bool> joined(relations_.size(), false);
    joined[0] = true;
    DPFrontier current_frontier = baseFrontierForRelation(relations_[0], 0);

    while (true)
    {
        bool made_progress = false;
        size_t round_candidates = 0;
        for (const auto &edge : join_edges_)
        {
            if (edge.left_rel_idx >= joined.size() || edge.right_rel_idx >= joined.size())
            {
                continue;
            }

            size_t candidate_rel = std::numeric_limits<size_t>::max();

            if (joined[edge.left_rel_idx] && !joined[edge.right_rel_idx])
            {
                candidate_rel = edge.right_rel_idx;
            }
            else if (joined[edge.right_rel_idx] && !joined[edge.left_rel_idx])
            {
                if (!edge.reorderable)
                {
                    continue;
                }
                candidate_rel = edge.left_rel_idx;
            }
            else
            {
                continue;
            }

            const auto relation_frontier =
                baseFrontierForRelation(relations_[candidate_rel],
                                       candidate_rel);
            DPFrontier next_frontier;
            for (const auto& left_entry : current_frontier)
            {
                for (const auto& relation_entry : relation_frontier)
                {
                    ++telemetry_.pair_evaluation_count;
                    ++round_candidates;
                    pushFrontierEntry(next_frontier,
                                      costJoin(left_entry,
                                               relation_entry,
                                               edge,
                                               ctx,
                                               true));
                }
            }

            if (next_frontier.empty())
            {
                continue;
            }

            joined[candidate_rel] = true;
            current_frontier = std::move(next_frontier);
            made_progress = true;
            break;
        }

        if (!made_progress)
        {
            size_t best_rel = std::numeric_limits<size_t>::max();
            DPFrontier best_frontier;
            double best_cost = std::numeric_limits<double>::max();
            size_t best_frontier_size = 0;
            for (size_t i = 0; i < relations_.size(); ++i)
            {
                if (joined[i])
                {
                    continue;
                }

                const auto relation_frontier =
                    baseFrontierForRelation(relations_[i], i);
                DPFrontier candidate_frontier;
                for (const auto& left_entry : current_frontier)
                {
                    for (const auto& relation_entry : relation_frontier)
                    {
                        ++telemetry_.pair_evaluation_count;
                        ++round_candidates;
                        pushFrontierEntry(candidate_frontier,
                                          costCrossJoin(left_entry,
                                                        relation_entry,
                                                        ctx));
                    }
                }
                const auto* best_entry = frontierBestEntry(candidate_frontier);
                if (best_entry == nullptr)
                {
                    continue;
                }
                if (best_entry->cost > best_cost ||
                    (best_entry->cost == best_cost &&
                     candidate_frontier.size() <= best_frontier_size))
                {
                    continue;
                }
                best_cost = best_entry->cost;
                best_frontier_size = candidate_frontier.size();
                best_frontier = std::move(candidate_frontier);
                best_rel = i;
            }

            if (best_frontier.empty() || best_rel == std::numeric_limits<size_t>::max())
            {
                break;
            }

            joined[best_rel] = true;
            current_frontier = std::move(best_frontier);
        }

        telemetry_.considered_state_count += round_candidates;

        if (std::all_of(joined.begin(), joined.end(), [](bool value) { return value; }))
        {
            break;
        }
    }

    return frontierBestPath(current_frontier);
}

JoinOrderingOptimizer::DPFrontier JoinOrderingOptimizer::generateSubsetPlans(
    RelationSet subset, core::ErrorContext* ctx)
{
    DPFrontier best_frontier;

    // Try all ways to partition subset into two non-empty subsets
    // such that there's a join edge connecting them
    std::vector<RelationSet> subsets = enumerateSubsets(subset);

    for (RelationSet left_set : subsets)
    {
        // Skip empty set and full set
        if (left_set == 0 || left_set == subset) continue;

        RelationSet right_set = subset & ~left_set;
        if (right_set == 0) continue;

        // Check if we have plans for both subsets
        auto left_it = dp_table_.find(left_set);
        auto right_it = dp_table_.find(right_set);
        if (left_it == dp_table_.end() || right_it == dp_table_.end()) continue;

        // Find join edges connecting these sets
        std::vector<size_t> connecting_edges = findConnectingEdges(left_set, right_set);

        if (connecting_edges.empty())
        {
            for (const auto& left_entry : left_it->second)
            {
                for (const auto& right_entry : right_it->second)
                {
                    ++telemetry_.pair_evaluation_count;
                    pushFrontierEntry(best_frontier,
                                      costCrossJoin(left_entry,
                                                    right_entry,
                                                    ctx));
                }
            }
            continue;
        }

        // Try each connecting edge
        for (size_t edge_idx : connecting_edges)
        {
            for (const auto& left_entry : left_it->second)
            {
                for (const auto& right_entry : right_it->second)
                {
                    ++telemetry_.pair_evaluation_count;
                    pushFrontierEntry(best_frontier,
                                      costJoin(left_entry,
                                               right_entry,
                                               join_edges_[edge_idx],
                                               ctx));
                }
            }
        }
    }

    return best_frontier;
}

JoinOrderingOptimizer::DPEntry JoinOrderingOptimizer::costJoin(
    const DPEntry& left_entry,
    const DPEntry& right_entry,
    const JoinEdge& edge,
    core::ErrorContext* ctx,
    bool preserve_orientation)
{
    DPEntry result;

    const uint64_t left_rows = left_entry.rows;
    const uint64_t right_rows = right_entry.rows;
    const double selectivity = edge.selectivity;
    const auto* left_descriptor =
        left_entry.best_path != nullptr
            ? &left_entry.best_path->accessDescriptor()
            : nullptr;
    const auto* right_descriptor =
        right_entry.best_path != nullptr
            ? &right_entry.best_path->accessDescriptor()
            : nullptr;
    const bool parameterized_inner =
        right_descriptor != nullptr && right_descriptor->parameterized;
    JoinLegalityDescriptor method_legality;
    method_legality.legality_class = edge.legality_class;
    method_legality.reorderable = edge.reorderable;
    method_legality.requires_original_order = edge.requires_original_order;
    method_legality.natural_barrier = edge.natural_barrier;
    method_legality.using_barrier = edge.using_barrier;
    method_legality.semi_duplicate_semantics =
        edge.legality_class == JoinLegalityClass::SEMI_BARRIER;
    method_legality.anti_duplicate_semantics =
        edge.legality_class == JoinLegalityClass::ANTI_BARRIER;
    method_legality.lateral_dependency = parameterized_inner;
    const bool left_contains_edge_left =
        edge.left_rel_idx < MAX_DP_RELATIONS &&
        (left_entry.relation_set_mask & (1ULL << edge.left_rel_idx)) != 0;
    const std::string outer_order_key =
        left_contains_edge_left ? edge.left_order_key_text
                                : edge.right_order_key_text;
    const std::string inner_order_key =
        left_contains_edge_left ? edge.right_order_key_text
                                : edge.left_order_key_text;
    const bool outer_presorted =
        edge.has_key_metadata &&
        pathProvidesMergeJoinOrder(left_entry.best_path, outer_order_key);
    const bool inner_presorted =
        edge.has_key_metadata &&
        pathProvidesMergeJoinOrder(right_entry.best_path, inner_order_key);

    uint64_t join_rows = static_cast<uint64_t>(
        static_cast<double>(left_rows) *
        static_cast<double>(right_rows) * selectivity);
    if (join_rows == 0)
    {
        join_rows = 1;
    }

    const auto nested_legality =
        evaluateNestedLoopLegality(method_legality, parameterized_inner);
    const bool allow_nested_loop =
        nested_legality.legal &&
        controls_.method_policy != JoinMethodPolicy::HASH_ONLY &&
        controls_.method_policy != JoinMethodPolicy::MERGE_ONLY;
    CostEstimate nl_cost{};
    if (allow_nested_loop)
    {
        nl_cost = cost_model_.costNestedLoopJoin(
            left_entry.best_path ? left_entry.best_path->cost() : CostEstimate{},
            right_entry.best_path ? right_entry.best_path->cost() : CostEstimate{},
            left_rows,
            right_rows,
            selectivity,
            edge.join_type,
            ctx);
    }
    else
    {
        nl_cost.total_cost = std::numeric_limits<double>::max();
    }

    const auto hash_legality =
        evaluateHashJoinLegality(method_legality,
                                 edge.join_type,
                                 edge.equi_join,
                                 edge.has_key_metadata,
                                 parameterized_inner);
    CostEstimate hash_cost{};
    bool allow_hash =
        hash_legality.legal &&
        controls_.method_policy != JoinMethodPolicy::NESTED_LOOP_ONLY &&
        controls_.method_policy != JoinMethodPolicy::MERGE_ONLY;
    if (allow_hash)
    {
        hash_cost = cost_model_.costHashJoin(
            left_entry.best_path ? left_entry.best_path->cost() : CostEstimate{},
            right_entry.best_path ? right_entry.best_path->cost() : CostEstimate{},
            left_rows,
            right_rows,
            selectivity,
            edge.join_type,
            ctx);
        if (controls_.disallow_temp_spill && hash_cost.spill_expected)
        {
            allow_hash = false;
            hash_cost.total_cost = std::numeric_limits<double>::max();
        }
    }
    else
    {
        hash_cost.total_cost = std::numeric_limits<double>::max();
    }

    const auto merge_legality =
        evaluateMergeJoinLegality(method_legality,
                                  edge.join_type,
                                  edge.equi_join,
                                  edge.has_key_metadata,
                                  parameterized_inner,
                                  outer_presorted,
                                  inner_presorted);
    CostEstimate merge_cost{};
    bool allow_merge =
        merge_legality.legal &&
        controls_.method_policy != JoinMethodPolicy::NESTED_LOOP_ONLY &&
        controls_.method_policy != JoinMethodPolicy::HASH_ONLY;
    if (allow_merge)
    {
        merge_cost = cost_model_.costMergeJoin(
            left_entry.best_path ? left_entry.best_path->cost() : CostEstimate{},
            right_entry.best_path ? right_entry.best_path->cost() : CostEstimate{},
            left_rows,
            right_rows,
            selectivity,
            outer_presorted,
            inner_presorted,
            edge.join_type,
            ctx);
        if (controls_.disallow_temp_spill && merge_cost.spill_expected)
        {
            allow_merge = false;
            merge_cost.total_cost = std::numeric_limits<double>::max();
        }
    }
    else
    {
        merge_cost.total_cost = std::numeric_limits<double>::max();
    }

    enum class ChosenJoinMethod
    {
        NESTED_LOOP,
        HASH_JOIN,
        MERGE_JOIN
    };

    const bool merge_requires_explicit_sort =
        merge_legality.requires_sort_outer || merge_legality.requires_sort_inner;

    if (!allow_nested_loop && !allow_hash && !allow_merge)
    {
        if (ctx != nullptr)
        {
            std::string reject_reason;
            if (controls_.method_policy == JoinMethodPolicy::HASH_ONLY)
            {
                reject_reason = hash_legality.legal
                    ? "JOIN_METHOD_SPILL_DISALLOWED: HASH_JOIN requires temp spill under current work_mem policy"
                    : joinMethodRejectReason(JoinMethodFamily::HASH_JOIN,
                                             hash_legality.reject_code);
            }
            else if (controls_.method_policy == JoinMethodPolicy::MERGE_ONLY)
            {
                reject_reason = merge_legality.legal
                    ? "JOIN_METHOD_SPILL_DISALLOWED: MERGE_JOIN requires temp spill under current work_mem policy"
                    : joinMethodRejectReason(JoinMethodFamily::MERGE_JOIN,
                                             merge_legality.reject_code);
            }
            if (!reject_reason.empty())
            {
                SET_ERROR_CONTEXT(ctx,
                                  core::Status::INVALID_ARGUMENT,
                                  reject_reason.c_str());
            }
        }
        return result;
    }

    ChosenJoinMethod chosen_method = ChosenJoinMethod::NESTED_LOOP;
    if (!allow_nested_loop)
    {
        chosen_method = allow_hash ? ChosenJoinMethod::HASH_JOIN
                                   : ChosenJoinMethod::MERGE_JOIN;
    }
    double chosen_cost =
        chosen_method == ChosenJoinMethod::HASH_JOIN
            ? hash_cost.total_cost
            : chosen_method == ChosenJoinMethod::MERGE_JOIN
                  ? merge_cost.total_cost
                  : nl_cost.total_cost;
    if (allow_hash && hash_cost.total_cost < chosen_cost)
    {
        chosen_method = ChosenJoinMethod::HASH_JOIN;
        chosen_cost = hash_cost.total_cost;
    }
    if (allow_merge &&
        (controls_.method_policy == JoinMethodPolicy::MERGE_ONLY ||
         (!allow_hash && merge_cost.total_cost < chosen_cost) ||
         (!merge_requires_explicit_sort &&
          controls_.method_policy != JoinMethodPolicy::AUTO &&
          merge_cost.total_cost < chosen_cost)))
    {
        chosen_method = ChosenJoinMethod::MERGE_JOIN;
        chosen_cost = merge_cost.total_cost;
    }

    result.rows = join_rows;
    result.relation_set_mask =
        left_entry.relation_set_mask | right_entry.relation_set_mask;

    if (chosen_method == ChosenJoinMethod::HASH_JOIN)
    {
        result.cost = hash_cost.total_cost;
        std::shared_ptr<Path> build_path = left_entry.best_path;
        std::shared_ptr<Path> probe_path = right_entry.best_path;
        if (edge.join_type == parser::JoinType::INNER && !preserve_orientation)
        {
            build_path = (left_rows < right_rows)
                ? left_entry.best_path : right_entry.best_path;
            probe_path = (left_rows < right_rows)
                ? right_entry.best_path : left_entry.best_path;
        }

        std::vector<parser::v3::Expression*> hash_keys_outer;
        std::vector<parser::v3::Expression*> hash_keys_inner;
        result.best_path = std::make_shared<HashJoinPath>(
            edge.join_type,
            build_path,
            probe_path,
            edge.join_condition,
            hash_keys_outer,
            hash_keys_inner,
            selectivity,
            hash_cost);
        AccessPathDescriptor descriptor;
        descriptor.family = "HASH_JOIN";
        descriptor.path_name = "HASH_JOIN";
        combineJoinDescriptorCommon(left_descriptor, right_descriptor, descriptor);
        descriptor.ordered_output = false;
        descriptor.ordered_prefix_length = 0;
        descriptor.ordering_keys.clear();
        descriptor.order_complete = false;
        descriptor.interesting_order_score = 0.0;
        descriptor.gather_merge = false;
        descriptor.parallel_order_preservation = "UNORDERED";
        appendUniqueTag(descriptor.family_tags, "HASH_JOIN");
        if (descriptor.parallel_enabled || descriptor.parallel_aware)
        {
            descriptor.parallel_stage = "HASH_JOIN";
        }
        result.best_path->setAccessDescriptor(std::move(descriptor));
    }
    else if (chosen_method == ChosenJoinMethod::MERGE_JOIN)
    {
        result.cost = merge_cost.total_cost;
        std::vector<parser::v3::Expression*> merge_keys_outer;
        std::vector<parser::v3::Expression*> merge_keys_inner;
        result.best_path = std::make_shared<MergeJoinPath>(
            edge.join_type,
            left_entry.best_path,
            right_entry.best_path,
            edge.join_condition,
            merge_keys_outer,
            merge_keys_inner,
            selectivity,
            outer_presorted,
            inner_presorted,
            merge_cost);
        AccessPathDescriptor descriptor;
        descriptor.family = "MERGE_JOIN";
        descriptor.path_name =
            (merge_legality.requires_sort_outer ||
             merge_legality.requires_sort_inner)
                ? "MERGE_JOIN[SORT_TO_MERGE]"
                : "MERGE_JOIN";
        combineJoinDescriptorCommon(left_descriptor, right_descriptor, descriptor);
        descriptor.ordered_output = true;
        descriptor.order_complete = true;
        descriptor.ordered_prefix_length = outer_order_key.empty() ? 0 : 1;
        if (!outer_order_key.empty())
        {
            AccessPathDescriptor::OrderingKey ordering_key;
            ordering_key.expression_text = outer_order_key;
            descriptor.ordering_keys.push_back(std::move(ordering_key));
        }
        descriptor.interesting_order_score =
            descriptor.ordered_prefix_length > 0
                ? static_cast<double>(descriptor.ordered_prefix_length)
                : 1.0;
        descriptor.gather_merge = false;
        descriptor.parallel_order_preservation = "TOTAL_ORDER";
        appendUniqueTag(descriptor.family_tags, "MERGE_JOIN");
        if (merge_legality.requires_sort_outer ||
            merge_legality.requires_sort_inner)
        {
            appendUniqueTag(descriptor.family_tags, "SORT_TO_MERGE");
        }
        if (merge_legality.requires_sort_outer)
        {
            appendUniqueTag(descriptor.family_tags, "SORT_OUTER");
        }
        if (merge_legality.requires_sort_inner)
        {
            appendUniqueTag(descriptor.family_tags, "SORT_INNER");
        }
        if (outer_presorted)
        {
            appendUniqueTag(descriptor.family_tags, "OUTER_PRESORTED");
        }
        if (inner_presorted)
        {
            appendUniqueTag(descriptor.family_tags, "INNER_PRESORTED");
        }
        if (descriptor.parallel_enabled || descriptor.parallel_aware)
        {
            descriptor.parallel_stage = "MERGE_JOIN";
        }
        result.best_path->setAccessDescriptor(std::move(descriptor));
    }
    else
    {
        result.cost = nl_cost.total_cost;
        result.best_path = std::make_shared<NestedLoopJoinPath>(
            edge.join_type,
            left_entry.best_path,
            right_entry.best_path,
            edge.join_condition,
            selectivity,
            nl_cost);
        AccessPathDescriptor descriptor;
        descriptor.family = parameterized_inner ? "PARAMETERIZED_NESTED_LOOP"
                                                : "NESTED_LOOP_JOIN";
        descriptor.path_name = descriptor.family;
        combineJoinDescriptorCommon(left_descriptor, right_descriptor, descriptor);
        if (left_descriptor != nullptr)
        {
            descriptor.ordered_output = left_descriptor->ordered_output;
            descriptor.ordered_prefix_length =
                left_descriptor->ordered_prefix_length;
            descriptor.ordering_keys = left_descriptor->ordering_keys;
            descriptor.order_complete = left_descriptor->order_complete;
            descriptor.interesting_order_score =
                left_descriptor->interesting_order_score;
            descriptor.gather_merge = left_descriptor->gather_merge;
            descriptor.parallel_stage = left_descriptor->parallel_stage;
            descriptor.parallel_distribution_mode =
                left_descriptor->parallel_distribution_mode;
            descriptor.parallel_order_preservation =
                left_descriptor->parallel_order_preservation;
            descriptor.exchange_topology_id =
                left_descriptor->exchange_topology_id;
            descriptor.gather_decision_reason =
                left_descriptor->gather_decision_reason;
        }
        appendUniqueTag(descriptor.family_tags,
                        parameterized_inner ? "PARAMETERIZED_NESTED_LOOP"
                                            : "NESTED_LOOP_JOIN");
        result.best_path->setAccessDescriptor(std::move(descriptor));
    }

    return result;
}

JoinOrderingOptimizer::DPEntry JoinOrderingOptimizer::costCrossJoin(
    const DPEntry& left_entry,
    const DPEntry& right_entry,
    core::ErrorContext* ctx)
{
    DPEntry result;

    const uint64_t left_rows = left_entry.rows;
    const uint64_t right_rows = right_entry.rows;
    const double selectivity = 1.0;

    CostEstimate nl_cost = cost_model_.costNestedLoopJoin(
        left_entry.best_path ? left_entry.best_path->cost() : CostEstimate{},
        right_entry.best_path ? right_entry.best_path->cost() : CostEstimate{},
        left_rows,
        right_rows,
        selectivity,
        parser::JoinType::CROSS,
        ctx);

    result.cost = nl_cost.total_cost;
    result.rows = left_rows * right_rows;
    result.relation_set_mask =
        left_entry.relation_set_mask | right_entry.relation_set_mask;
    result.best_path = std::make_shared<NestedLoopJoinPath>(
        parser::JoinType::CROSS,
        left_entry.best_path,
        right_entry.best_path,
        nullptr,
        selectivity,
        nl_cost);
    AccessPathDescriptor descriptor;
    descriptor.family = "NESTED_LOOP_JOIN";
    descriptor.path_name = "NESTED_LOOP_JOIN";
    const auto* left_descriptor =
        left_entry.best_path != nullptr
            ? &left_entry.best_path->accessDescriptor()
            : nullptr;
    const auto* right_descriptor =
        right_entry.best_path != nullptr
            ? &right_entry.best_path->accessDescriptor()
            : nullptr;
    combineJoinDescriptorCommon(left_descriptor, right_descriptor, descriptor);
    if (left_descriptor != nullptr)
    {
        descriptor.ordered_output = left_descriptor->ordered_output;
        descriptor.ordered_prefix_length = left_descriptor->ordered_prefix_length;
        descriptor.ordering_keys = left_descriptor->ordering_keys;
        descriptor.order_complete = left_descriptor->order_complete;
        descriptor.interesting_order_score =
            left_descriptor->interesting_order_score;
        descriptor.gather_merge = left_descriptor->gather_merge;
        descriptor.parallel_stage = left_descriptor->parallel_stage;
        descriptor.parallel_distribution_mode =
            left_descriptor->parallel_distribution_mode;
        descriptor.parallel_order_preservation =
            left_descriptor->parallel_order_preservation;
        descriptor.exchange_topology_id =
            left_descriptor->exchange_topology_id;
        descriptor.gather_decision_reason =
            left_descriptor->gather_decision_reason;
    }
    result.best_path->setAccessDescriptor(std::move(descriptor));
    return result;
}

std::vector<size_t> JoinOrderingOptimizer::findConnectingEdges(
    RelationSet left_set, RelationSet right_set) const
{
    std::vector<size_t> result;

    for (size_t i = 0; i < join_edges_.size(); ++i)
    {
        const JoinEdge& edge = join_edges_[i];

        bool left_in_left = hasBit(left_set, edge.left_rel_idx);
        bool left_in_right = hasBit(right_set, edge.left_rel_idx);
        bool right_in_left = hasBit(left_set, edge.right_rel_idx);
        bool right_in_right = hasBit(right_set, edge.right_rel_idx);

        // Edge connects the two sets if one endpoint is in left and other in right
        if ((left_in_left && right_in_right) || (left_in_right && right_in_left))
        {
            result.push_back(i);
        }
    }

    return result;
}

bool JoinOrderingOptimizer::isConnected(RelationSet set) const
{
    if (set == 0) return true;
    if (countBits(set) == 1) return true;

    // BFS to check connectivity
    std::vector<std::vector<size_t>> adj = buildAdjacencyList();

    // Find first set bit
    size_t start = lowestBit(set);

    std::vector<bool> visited(relations_.size(), false);
    std::queue<size_t> q;
    q.push(start);
    visited[start] = true;
    size_t count = 1;

    while (!q.empty())
    {
        size_t u = q.front();
        q.pop();

        for (size_t v : adj[u])
        {
            if (!visited[v] && hasBit(set, v))
            {
                visited[v] = true;
                count++;
                q.push(v);
            }
        }
    }

    return count == countBits(set);
}

bool JoinOrderingOptimizer::hasJoinReorderBarrier() const
{
    return std::any_of(join_edges_.begin(),
                       join_edges_.end(),
                       [](const JoinEdge &edge) {
                           return !edge.reorderable || edge.requires_original_order;
                       });
}

std::vector<std::vector<size_t>> JoinOrderingOptimizer::buildAdjacencyList() const
{
    std::vector<std::vector<size_t>> adj(relations_.size());

    for (const JoinEdge& edge : join_edges_)
    {
        adj[edge.left_rel_idx].push_back(edge.right_rel_idx);
        adj[edge.right_rel_idx].push_back(edge.left_rel_idx);
    }

    return adj;
}

std::string JoinOrderingOptimizer::setToString(RelationSet set) const
{
    std::string result = "{";
    bool first = true;
    for (size_t i = 0; i < relations_.size(); ++i)
    {
        if (hasBit(set, i))
        {
            if (!first) result += ", ";
            result += relations_[i].table_name;
            first = false;
        }
    }
    result += "}";
    return result;
}

size_t JoinOrderingOptimizer::countBits(RelationSet set)
{
    size_t count = 0;
    while (set)
    {
        count += set & 1;
        set >>= 1;
    }
    return count;
}

size_t JoinOrderingOptimizer::lowestBit(RelationSet set)
{
    if (set == 0) return 0;
    size_t idx = 0;
    while ((set & 1) == 0)
    {
        set >>= 1;
        idx++;
    }
    return idx;
}

std::vector<JoinOrderingOptimizer::RelationSet> JoinOrderingOptimizer::enumerateSubsets(
    RelationSet set)
{
    std::vector<RelationSet> result;

    // Enumerate all subsets of 'set' using bit manipulation
    // For a set S, iterate through all subsets by starting at S and
    // repeatedly computing (subset - 1) & set
    RelationSet subset = set;
    do
    {
        result.push_back(subset);
        subset = (subset - 1) & set;
    } while (subset != set);

    result.push_back(0);  // Include empty set
    return result;
}

} // namespace scratchbird::optimizer
