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
 * join_ordering.h - Dynamic Programming Join Order Optimizer
 *
 * V2 MIGRATION STATUS: COMPLETE
 *
 * The join ordering optimizer uses V2 ResolvedExpression types for join conditions.
 * It provides cost-based join ordering optimization using dynamic programming
 * for small join counts and greedy optimization for larger queries.
 *
 * NOTE: This optimizer is NOT in the main client execution path.
 * The main execution path is:
 *   ServerSession → QueryCompilerV2 → BytecodeGeneratorV2 → Executor
 *
 * QueryPlanner uses JoinOrderingOptimizer for EXPLAIN functionality.
 */

#ifndef SCRATCHBIRD_OPTIMIZER_JOIN_ORDERING_H
#define SCRATCHBIRD_OPTIMIZER_JOIN_ORDERING_H

#include "scratchbird/optimizer/path.h"
#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/optimizer/selectivity_estimator.h"
#include "scratchbird/parser/shared_types.h"      // For JoinType (shared between V1/V2)
#include "scratchbird/sblr/resolved_ast_v2.h"     // For V2 ResolvedExpression
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/types.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <limits>

namespace scratchbird::optimizer
{

// Forward declarations
class QueryPlanner;

/**
 * RelationInfo - Information about a base relation (table) in a join query
 */
struct RelationInfo
{
    core::ID table_id;
    std::string table_name;
    std::string alias;
    std::shared_ptr<Path> best_path;
    uint64_t rows;
    uint64_t width;
};

/**
 * JoinEdge - Represents a join condition between two relations (V2 types)
 */
struct JoinEdge
{
    size_t left_rel_idx;
    size_t right_rel_idx;
    parser::JoinType join_type;
    parser::v2::ResolvedExpression* join_condition;  // V2 type
    double selectivity;
};

/**
 * JoinOrderingOptimizer - Finds optimal join order (V2 Complete)
 *
 * Uses dynamic programming for small queries (≤12 relations) and
 * greedy optimization for larger queries. Returns NestedLoopJoinPath
 * or HashJoinPath based on cost estimates.
 */
class JoinOrderingOptimizer
{
public:
    static constexpr size_t MAX_DP_RELATIONS = 12;

    JoinOrderingOptimizer(CostModel& cost_model,
                         SelectivityEstimator& selectivity_estimator);

    size_t addRelation(const core::ID& table_id,
                      const std::string& table_name,
                      const std::string& alias,
                      std::shared_ptr<Path> best_path);

    void addJoinEdge(size_t left_idx, size_t right_idx,
                    parser::JoinType join_type,
                    parser::v2::ResolvedExpression* join_condition);  // V2 type

    void setJoinSelectivity(size_t edge_idx, double selectivity);

    std::shared_ptr<Path> optimize(core::ErrorContext* ctx = nullptr);
    std::shared_ptr<Path> optimizeGreedy(core::ErrorContext* ctx = nullptr);

    size_t numRelations() const { return relations_.size(); }
    size_t numJoinEdges() const { return join_edges_.size(); }

    void clear();

private:
    using RelationSet = uint64_t;

    struct DPEntry
    {
        std::shared_ptr<Path> best_path;
        double cost;
        uint64_t rows;

        DPEntry() : cost(std::numeric_limits<double>::max()), rows(0) {}
    };

    // DP helper methods
    DPEntry generateSubsetPlan(RelationSet subset, core::ErrorContext* ctx);
    DPEntry costJoin(const DPEntry& left_entry, const DPEntry& right_entry,
                     const JoinEdge& edge, core::ErrorContext* ctx);
    std::vector<size_t> findConnectingEdges(RelationSet left_set, RelationSet right_set) const;
    bool isConnected(RelationSet set) const;
    std::vector<std::vector<size_t>> buildAdjacencyList() const;
    std::string setToString(RelationSet set) const;

    // Bit manipulation helpers
    static size_t countBits(RelationSet set);
    static size_t lowestBit(RelationSet set);
    static bool hasBit(RelationSet set, size_t idx) { return (set & (1ULL << idx)) != 0; }
    static std::vector<RelationSet> enumerateSubsets(RelationSet set);

    CostModel& cost_model_;
    SelectivityEstimator& selectivity_estimator_;
    std::vector<RelationInfo> relations_;
    std::vector<JoinEdge> join_edges_;
    std::unordered_map<RelationSet, DPEntry> dp_table_;
};

} // namespace scratchbird::optimizer

#endif // SCRATCHBIRD_OPTIMIZER_JOIN_ORDERING_H
