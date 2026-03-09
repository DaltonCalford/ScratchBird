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
#include <queue>
#include <cmath>

namespace scratchbird::optimizer
{

JoinOrderingOptimizer::JoinOrderingOptimizer(CostModel& cost_model,
                                             SelectivityEstimator& selectivity_estimator)
    : cost_model_(cost_model),
      selectivity_estimator_(selectivity_estimator)
{
}

size_t JoinOrderingOptimizer::addRelation(const core::ID& table_id,
                                          const std::string& table_name,
                                          const std::string& alias,
                                          std::shared_ptr<Path> best_path)
{
    RelationInfo info;
    info.table_id = table_id;
    info.table_name = table_name;
    info.alias = alias;
    info.best_path = best_path;
    info.rows = best_path ? best_path->rows() : 1000;  // Default estimate
    info.width = 100;  // Default row width estimate

    size_t idx = relations_.size();
    relations_.push_back(std::move(info));

    DEBUG_LOG_DB("JoinOrdering: Added relation " + std::to_string(idx) +
                 " (" + table_name + "), rows=" + std::to_string(info.rows));

    return idx;
}

void JoinOrderingOptimizer::addJoinEdge(size_t left_idx, size_t right_idx,
                                        parser::JoinType join_type,
                                        parser::v3::Expression* join_condition)
{
    JoinEdge edge;
    edge.left_rel_idx = left_idx;
    edge.right_rel_idx = right_idx;
    edge.join_type = join_type;
    edge.join_condition = join_condition;
    edge.selectivity = 0.1;  // Default selectivity estimate

    join_edges_.push_back(std::move(edge));

    DEBUG_LOG_DB("JoinOrdering: Added join edge between " +
                 std::to_string(left_idx) + " and " + std::to_string(right_idx));
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
}

std::shared_ptr<Path> JoinOrderingOptimizer::optimize(core::ErrorContext* ctx)
{
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
        return relations_[0].best_path;
    }

    if (hasJoinReorderBarrier())
    {
        DEBUG_LOG_DB("JoinOrdering: Reorder barrier detected, preserving input join order");
        last_strategy_used_ = JoinSearchStrategy::INPUT_ORDER;
        return optimizePreservingInputOrder(ctx);
    }

    if (controls_.strategy == JoinSearchStrategy::INPUT_ORDER)
    {
        DEBUG_LOG_DB("JoinOrdering: Explicit input-order strategy requested");
        last_strategy_used_ = JoinSearchStrategy::INPUT_ORDER;
        return optimizePreservingInputOrder(ctx);
    }

    const size_t exhaustive_cap =
        std::min(MAX_DP_RELATIONS, std::max<size_t>(1, controls_.max_exhaustive_relations));

    if (controls_.strategy == JoinSearchStrategy::HYPERGRAPH_GREEDY)
    {
        DEBUG_LOG_DB("JoinOrdering: Explicit hypergraph-greedy strategy requested");
        last_strategy_used_ = JoinSearchStrategy::HYPERGRAPH_GREEDY;
        return optimizeHypergraphGreedy(ctx);
    }

    if (controls_.strategy == JoinSearchStrategy::EXHAUSTIVE_DP &&
        relations_.size() > exhaustive_cap)
    {
        DEBUG_LOG_DB("JoinOrdering: Exhaustive DP requested but relation count exceeds cap; falling back to hypergraph-greedy");
        last_strategy_used_ = JoinSearchStrategy::HYPERGRAPH_GREEDY;
        return optimizeHypergraphGreedy(ctx);
    }

    if (controls_.strategy == JoinSearchStrategy::AUTO &&
        relations_.size() > exhaustive_cap)
    {
        DEBUG_LOG_DB("JoinOrdering: Relation count exceeds exhaustive cap (" +
                     std::to_string(exhaustive_cap) + "), using hypergraph-greedy heuristic");
        last_strategy_used_ = JoinSearchStrategy::HYPERGRAPH_GREEDY;
        return optimizeHypergraphGreedy(ctx);
    }

    last_strategy_used_ = JoinSearchStrategy::EXHAUSTIVE_DP;

    DEBUG_LOG_DB("JoinOrdering: Starting DP optimization for " +
                 std::to_string(relations_.size()) + " relations");

    // Clear memoization table
    dp_table_.clear();

    // Initialize base cases: single-relation subsets
    for (size_t i = 0; i < relations_.size(); ++i)
    {
        RelationSet singleton = 1ULL << i;
        DPEntry entry;
        entry.best_path = relations_[i].best_path;
        entry.cost = entry.best_path ? entry.best_path->totalCost() : 0.0;
        entry.rows = relations_[i].rows;
        dp_table_[singleton] = entry;

        DEBUG_LOG_DB("JoinOrdering: Base case for relation " + std::to_string(i) +
                     " (" + relations_[i].table_name + "): cost=" + std::to_string(entry.cost));
    }

    // Full set includes all relations
    RelationSet full_set = (1ULL << relations_.size()) - 1;

    // Bottom-up DP: build larger subsets from smaller ones
    // Enumerate subsets by size (2, 3, 4, ..., N)
    for (size_t size = 2; size <= relations_.size(); ++size)
    {
        DEBUG_LOG_DB("JoinOrdering: Processing subsets of size " + std::to_string(size));

        // Enumerate all subsets of given size
        // Use "Gosper's hack" to enumerate k-bit subsets of an n-bit set
        RelationSet subset = (1ULL << size) - 1;  // Start with smallest k-bit number

        while (subset <= full_set)
        {
            if (countBits(subset) == size)
            {
                // Generate best plan for this subset
                DPEntry entry = generateSubsetPlan(subset, ctx);
                if (entry.best_path)
                {
                    dp_table_[subset] = entry;
                    DEBUG_LOG_DB("JoinOrdering: Subset " + setToString(subset) +
                                 " best cost=" + std::to_string(entry.cost));
                }
            }

            // Next subset (Gosper's hack)
            if (subset == 0) break;
            RelationSet c = subset & -static_cast<int64_t>(subset);
            RelationSet r = subset + c;
            subset = (((r ^ subset) >> 2) / c) | r;
            if (subset > full_set) break;
        }
    }

    // Return best plan for full set
    auto it = dp_table_.find(full_set);
    if (it != dp_table_.end())
    {
        DEBUG_LOG_DB("JoinOrdering: DP complete, best cost=" +
                     std::to_string(it->second.cost));
        return it->second.best_path;
    }

    DEBUG_LOG_DB("JoinOrdering: DP failed to find plan for full set, using greedy fallback");
    last_strategy_used_ = JoinSearchStrategy::HYPERGRAPH_GREEDY;
    return optimizeHypergraphGreedy(ctx);
}

std::shared_ptr<Path> JoinOrderingOptimizer::optimizeGreedy(core::ErrorContext* ctx)
{
    if (relations_.empty())
    {
        return nullptr;
    }

    if (relations_.size() == 1)
    {
        return relations_[0].best_path;
    }

    if (hasJoinReorderBarrier())
    {
        DEBUG_LOG_DB("JoinOrdering: Greedy path disabled by reorder barrier, preserving input join order");
        return optimizePreservingInputOrder(ctx);
    }

    DEBUG_LOG_DB("JoinOrdering: Starting greedy optimization");

    // Track which relations have been joined
    std::vector<bool> joined(relations_.size(), false);

    // Start with the relation that has the smallest estimated rows
    size_t best_start = 0;
    uint64_t min_rows = relations_[0].rows;
    for (size_t i = 1; i < relations_.size(); ++i)
    {
        if (relations_[i].rows < min_rows)
        {
            min_rows = relations_[i].rows;
            best_start = i;
        }
    }

    joined[best_start] = true;
    std::shared_ptr<Path> current_path = relations_[best_start].best_path;
    uint64_t current_rows = relations_[best_start].rows;

    DEBUG_LOG_DB("JoinOrdering: Greedy start with " + relations_[best_start].table_name);

    // Greedily add remaining relations
    for (size_t step = 1; step < relations_.size(); ++step)
    {
        // Find the next best relation to join
        double best_cost = std::numeric_limits<double>::max();
        size_t best_rel = 0;
        std::shared_ptr<Path> best_join_path = nullptr;
        uint64_t best_join_rows = 0;

        for (size_t i = 0; i < relations_.size(); ++i)
        {
            if (joined[i]) continue;

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

                // Cost this join
                double selectivity = edge.selectivity;
                uint64_t right_rows = relations_[i].rows;
                uint64_t join_rows = static_cast<uint64_t>(
                    static_cast<double>(current_rows) *
                    static_cast<double>(right_rows) * selectivity);
                if (join_rows == 0) join_rows = 1;

                // Use nested loop join cost as estimate
                CostEstimate join_cost = cost_model_.costNestedLoopJoin(
                    current_path ? current_path->cost() : CostEstimate{},
                    relations_[i].best_path ? relations_[i].best_path->cost() : CostEstimate{},
                    current_rows,
                    right_rows,
                    selectivity,
                    edge.join_type,
                    ctx);

                if (join_cost.total_cost < best_cost)
                {
                    best_cost = join_cost.total_cost;
                    best_rel = i;
                    best_join_rows = join_rows;

                    // Create join path (V3: uses Expression* for join condition)
                    auto join_path = std::make_shared<NestedLoopJoinPath>(
                        edge.join_type,
                        current_path,
                        relations_[i].best_path,
                        edge.join_condition,
                        selectivity,
                        join_cost);
                    best_join_path = join_path;
                }
            }
        }

        if (!best_join_path)
        {
            // No connecting edge found - bridge the next disconnected component
            // with the cheapest explicit cross join.
            for (size_t i = 0; i < relations_.size(); ++i)
            {
                if (joined[i]) continue;

                double selectivity = 1.0;  // Cross join
                uint64_t right_rows = relations_[i].rows;
                uint64_t join_rows = current_rows * right_rows;

                CostEstimate join_cost = cost_model_.costNestedLoopJoin(
                    current_path ? current_path->cost() : CostEstimate{},
                    relations_[i].best_path ? relations_[i].best_path->cost() : CostEstimate{},
                    current_rows,
                    right_rows,
                    selectivity,
                    parser::JoinType::CROSS,
                    ctx);

                if (join_cost.total_cost >= best_cost)
                {
                    continue;
                }

                best_cost = join_cost.total_cost;
                best_rel = i;
                best_join_rows = join_rows;
                best_join_path = std::make_shared<NestedLoopJoinPath>(
                    parser::JoinType::CROSS,
                    current_path,
                    relations_[i].best_path,
                    nullptr,
                    selectivity,
                    join_cost);
            }
        }

        if (best_join_path)
        {
            joined[best_rel] = true;
            current_path = best_join_path;
            current_rows = best_join_rows;

            DEBUG_LOG_DB("JoinOrdering: Greedy added " + relations_[best_rel].table_name +
                         ", total cost=" + std::to_string(best_join_path->totalCost()));
        }
    }

    return current_path;
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
        return relations_[0].best_path;
    }

    if (hasJoinReorderBarrier())
    {
        last_strategy_used_ = JoinSearchStrategy::INPUT_ORDER;
        return optimizePreservingInputOrder(ctx);
    }

    struct Component
    {
        RelationSet relation_set = 0;
        DPEntry entry;
    };

    std::vector<Component> components;
    components.reserve(relations_.size());
    for (size_t i = 0; i < relations_.size(); ++i)
    {
        Component component;
        component.relation_set = 1ULL << i;
        component.entry.best_path = relations_[i].best_path;
        component.entry.cost = component.entry.best_path
            ? component.entry.best_path->totalCost()
            : 0.0;
        component.entry.rows = relations_[i].rows;
        components.push_back(std::move(component));
    }

    const size_t max_pair_evaluations =
        std::max<size_t>(1, controls_.max_pair_evaluations);

    while (components.size() > 1)
    {
        bool found_connected_merge = false;
        size_t best_left = 0;
        size_t best_right = 0;
        DPEntry best_entry;
        size_t pair_evaluations = 0;

        for (size_t left = 0; left < components.size(); ++left)
        {
            for (size_t right = left + 1; right < components.size(); ++right)
            {
                if (pair_evaluations >= max_pair_evaluations)
                {
                    break;
                }
                ++pair_evaluations;

                const auto connecting_edges =
                    findConnectingEdges(components[left].relation_set,
                                        components[right].relation_set);
                if (connecting_edges.empty())
                {
                    continue;
                }

                for (size_t edge_idx : connecting_edges)
                {
                    const auto candidate =
                        costJoin(components[left].entry,
                                 components[right].entry,
                                 join_edges_[edge_idx],
                                 ctx);
                    if (!candidate.best_path || candidate.cost >= best_entry.cost)
                    {
                        continue;
                    }
                    best_entry = candidate;
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
            best_entry = DPEntry{};
            for (size_t left = 0; left < components.size(); ++left)
            {
                for (size_t right = left + 1; right < components.size(); ++right)
                {
                    const auto candidate =
                        costCrossJoin(components[left].entry,
                                      components[right].entry,
                                      ctx);
                    if (!candidate.best_path || candidate.cost >= best_entry.cost)
                    {
                        continue;
                    }
                    best_entry = candidate;
                    best_left = left;
                    best_right = right;
                }
            }
        }

        if (!best_entry.best_path || best_left == best_right)
        {
            return optimizeGreedy(ctx);
        }

        Component merged;
        merged.relation_set =
            components[best_left].relation_set | components[best_right].relation_set;
        merged.entry = std::move(best_entry);

        if (best_left > best_right)
        {
            std::swap(best_left, best_right);
        }
        components.erase(components.begin() + static_cast<std::ptrdiff_t>(best_right));
        components.erase(components.begin() + static_cast<std::ptrdiff_t>(best_left));
        components.push_back(std::move(merged));
    }

    last_strategy_used_ = JoinSearchStrategy::HYPERGRAPH_GREEDY;
    return components.front().entry.best_path;
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
        return relations_[0].best_path;
    }

    std::vector<bool> joined(relations_.size(), false);
    joined[0] = true;
    std::shared_ptr<Path> current_path = relations_[0].best_path;
    uint64_t current_rows = relations_[0].rows;

    auto currentEntry = [&]() -> DPEntry {
        DPEntry entry;
        entry.best_path = current_path;
        entry.cost = current_path ? current_path->totalCost() : 0.0;
        entry.rows = current_rows;
        return entry;
    };

    while (true)
    {
        bool made_progress = false;
        for (const auto &edge : join_edges_)
        {
            if (edge.left_rel_idx >= joined.size() || edge.right_rel_idx >= joined.size())
            {
                continue;
            }

            const auto legality = classifyJoinLegality(edge.join_type, false, false);
            size_t candidate_rel = std::numeric_limits<size_t>::max();

            if (joined[edge.left_rel_idx] && !joined[edge.right_rel_idx])
            {
                candidate_rel = edge.right_rel_idx;
            }
            else if (joined[edge.right_rel_idx] && !joined[edge.left_rel_idx])
            {
                if (!legality.reorderable)
                {
                    continue;
                }
                candidate_rel = edge.left_rel_idx;
            }
            else
            {
                continue;
            }

            DPEntry relation_entry;
            relation_entry.best_path = relations_[candidate_rel].best_path;
            relation_entry.cost = relation_entry.best_path
                ? relation_entry.best_path->totalCost()
                : 0.0;
            relation_entry.rows = relations_[candidate_rel].rows;
            DPEntry next_entry = costJoin(currentEntry(),
                                         relation_entry,
                                         edge,
                                         ctx,
                                         true);

            if (!next_entry.best_path)
            {
                continue;
            }

            joined[candidate_rel] = true;
            current_path = next_entry.best_path;
            current_rows = next_entry.rows;
            made_progress = true;
            break;
        }

        if (!made_progress)
        {
            size_t best_rel = std::numeric_limits<size_t>::max();
            DPEntry best_entry;
            for (size_t i = 0; i < relations_.size(); ++i)
            {
                if (joined[i])
                {
                    continue;
                }

                DPEntry relation_entry;
                relation_entry.best_path = relations_[i].best_path;
                relation_entry.cost = relation_entry.best_path
                    ? relation_entry.best_path->totalCost()
                    : 0.0;
                relation_entry.rows = relations_[i].rows;
                auto join_entry = costCrossJoin(currentEntry(), relation_entry, ctx);
                if (!join_entry.best_path || join_entry.cost >= best_entry.cost)
                {
                    continue;
                }
                best_entry = std::move(join_entry);
                best_rel = i;
            }

            if (!best_entry.best_path || best_rel == std::numeric_limits<size_t>::max())
            {
                break;
            }

            joined[best_rel] = true;
            current_path = best_entry.best_path;
            current_rows = best_entry.rows;
        }

        if (std::all_of(joined.begin(), joined.end(), [](bool value) { return value; }))
        {
            break;
        }
    }

    return current_path;
}

JoinOrderingOptimizer::DPEntry JoinOrderingOptimizer::generateSubsetPlan(
    RelationSet subset, core::ErrorContext* ctx)
{
    DPEntry best_entry;

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
            DPEntry join_entry = costCrossJoin(left_it->second, right_it->second, ctx);
            if (join_entry.cost < best_entry.cost)
            {
                best_entry = std::move(join_entry);
            }
            continue;
        }

        // Try each connecting edge
        for (size_t edge_idx : connecting_edges)
        {
            DPEntry join_entry = costJoin(left_it->second, right_it->second,
                                          join_edges_[edge_idx], ctx);

            if (join_entry.cost < best_entry.cost)
            {
                best_entry = join_entry;
            }
        }
    }

    return best_entry;
}

JoinOrderingOptimizer::DPEntry JoinOrderingOptimizer::costJoin(
    const DPEntry& left_entry,
    const DPEntry& right_entry,
    const JoinEdge& edge,
    core::ErrorContext* ctx,
    bool preserve_orientation)
{
    DPEntry result;

    uint64_t left_rows = left_entry.rows;
    uint64_t right_rows = right_entry.rows;
    double selectivity = edge.selectivity;

    // Estimate join output rows
    uint64_t join_rows = static_cast<uint64_t>(
        static_cast<double>(left_rows) *
        static_cast<double>(right_rows) * selectivity);
    if (join_rows == 0) join_rows = 1;

    // Try both Nested Loop Join and Hash Join, pick cheaper one
    CostEstimate nl_cost = cost_model_.costNestedLoopJoin(
        left_entry.best_path ? left_entry.best_path->cost() : CostEstimate{},
        right_entry.best_path ? right_entry.best_path->cost() : CostEstimate{},
        left_rows,
        right_rows,
        selectivity,
        edge.join_type,
        ctx);

    CostEstimate hash_cost{};
    bool allow_hash = (edge.join_type == parser::JoinType::INNER ||
                       edge.join_type == parser::JoinType::LEFT);
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
    }
    else
    {
        hash_cost.total_cost = std::numeric_limits<double>::max();
    }

    // Choose cheaper join method
    bool use_hash = (hash_cost.total_cost < nl_cost.total_cost);

    if (use_hash)
    {
        result.cost = hash_cost.total_cost;
        result.rows = join_rows;

        // Preserve outer-join semantics by keeping the syntactic left/right
        // orientation for non-inner joins.
        std::shared_ptr<Path> build_path = left_entry.best_path;
        std::shared_ptr<Path> probe_path = right_entry.best_path;
        if (edge.join_type == parser::JoinType::INNER && !preserve_orientation)
        {
            build_path = (left_rows < right_rows)
                ? left_entry.best_path : right_entry.best_path;
            probe_path = (left_rows < right_rows)
                ? right_entry.best_path : left_entry.best_path;
        }

        // Hash keys are Expression* vectors (V3)
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
    }
    else
    {
        result.cost = nl_cost.total_cost;
        result.rows = join_rows;

        result.best_path = std::make_shared<NestedLoopJoinPath>(
            edge.join_type,
            left_entry.best_path,
            right_entry.best_path,
            edge.join_condition,
            selectivity,
            nl_cost);
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
    result.best_path = std::make_shared<NestedLoopJoinPath>(
        parser::JoinType::CROSS,
        left_entry.best_path,
        right_entry.best_path,
        nullptr,
        selectivity,
        nl_cost);
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
                           return !classifyJoinLegality(edge.join_type, false, false).reorderable;
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
