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
 * V3 MIGRATION STATUS: PENDING
 *
 * The join ordering optimizer uses V3 Expression types for join conditions.
 * It provides cost-based join ordering optimization using dynamic programming
 * for small join counts and greedy optimization for larger queries.
 *
 * Join reordering is only applied to reorderable inner/cross join graphs.
 * Outer/natural/other barrier joins fall back to input-order planning so
 * legality constraints are preserved even off the main execution path.
 */

#ifndef SCRATCHBIRD_OPTIMIZER_JOIN_ORDERING_H
#define SCRATCHBIRD_OPTIMIZER_JOIN_ORDERING_H

#include "scratchbird/optimizer/path.h"
#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/optimizer/selectivity_estimator.h"
#include "scratchbird/optimizer/join_legality.h"
#include "scratchbird/parser/shared_types.h"      // For JoinType (shared parser enums)
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/types.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <limits>

namespace scratchbird::parser::v3 {
    class Expression;
}

namespace scratchbird::optimizer
{

inline constexpr const char* kJoinSearchContractId = "sb_join_search/v1";
inline constexpr const char* kJoinSearchPropertySignatureContractId =
    "sb_join_search_property_signature/v1";
inline constexpr const char* kJoinSearchFrontierMode =
    "SUBSET_PROPERTY_DOMINANCE_FRONTIER";

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
    std::vector<std::shared_ptr<Path>> candidate_paths;
    std::vector<std::string> candidate_labels;
    std::shared_ptr<Path> best_path;
    uint64_t best_path_rows = 0;
    uint64_t rows;
    uint64_t width;
};

struct JoinSearchEntry
{
    std::shared_ptr<Path> best_path;
    double cost = std::numeric_limits<double>::max();
    uint64_t rows = 0;
    uint64_t relation_set_mask = 0;
};

using JoinSearchFrontier = std::vector<JoinSearchEntry>;

/**
 * JoinEdge - Represents a join condition between two relations (V3 types)
 */
struct JoinEdge
{
    size_t left_rel_idx;
    size_t right_rel_idx;
    parser::JoinType join_type;
    parser::v3::Expression* join_condition;
    double selectivity;
    size_t source_join_index = std::numeric_limits<size_t>::max();
    JoinLegalityClass legality_class = JoinLegalityClass::INNER_REORDERABLE;
    bool reorderable = true;
    bool requires_original_order = false;
    bool natural_barrier = false;
    bool using_barrier = false;
    bool equi_join = false;
    bool has_key_metadata = false;
    std::string left_order_key_text;
    std::string right_order_key_text;
};

enum class JoinSearchStrategy
{
    AUTO,
    EXHAUSTIVE_DP,
    BOUNDED_DP,
    HYPERGRAPH_GREEDY,
    HEURISTIC_GREEDY,
    INPUT_ORDER
};

enum class JoinMethodPolicy
{
    AUTO,
    NESTED_LOOP_ONLY,
    HASH_ONLY,
    MERGE_ONLY
};

struct JoinPlanningControls
{
    JoinSearchStrategy strategy = JoinSearchStrategy::AUTO;
    JoinMethodPolicy method_policy = JoinMethodPolicy::AUTO;
    bool disallow_temp_spill = false;
    size_t max_exhaustive_relations = 8;
    size_t max_bounded_dp_relations = 12;
    size_t max_pair_evaluations = 64;
    size_t max_states_considered = 256;
    size_t fallback_prune_level = 1;
};

struct JoinPlanningTelemetry
{
    JoinSearchStrategy requested_strategy = JoinSearchStrategy::AUTO;
    JoinSearchStrategy selected_strategy = JoinSearchStrategy::AUTO;
    std::string search_contract_id = kJoinSearchContractId;
    std::string property_signature_contract_id =
        kJoinSearchPropertySignatureContractId;
    std::string frontier_retention_mode = kJoinSearchFrontierMode;
    std::string strategy_source = "AUTO_POLICY";
    size_t exhaustive_join_limit = 0;
    size_t bounded_dp_join_limit = 0;
    size_t max_pair_evaluations = 0;
    size_t max_states_considered = 0;
    size_t fallback_prune_level = 0;
    size_t considered_state_count = 0;
    size_t pruned_state_count = 0;
    size_t pair_evaluation_count = 0;
    size_t base_candidate_path_count = 0;
    size_t retained_frontier_entry_count = 0;
    size_t dominated_state_count = 0;
    size_t max_frontier_width = 0;
    std::string fallback_reason;
    std::string fallback_threshold_name;
    size_t fallback_threshold_value = 0;
};

auto joinSearchPropertySignature(const AccessPathDescriptor& descriptor)
    -> std::string;

/**
 * JoinOrderingOptimizer - Finds optimal join order (V3)
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

    size_t addRelation(const core::ID& table_id,
                      const std::string& table_name,
                      const std::string& alias,
                      const std::vector<std::shared_ptr<Path>>& candidate_paths);

    size_t addJoinEdge(size_t left_idx, size_t right_idx,
                       parser::JoinType join_type,
                       parser::v3::Expression* join_condition,
                       size_t source_join_index =
                           std::numeric_limits<size_t>::max(),
                       JoinLegalityClass legality_class =
                           JoinLegalityClass::INNER_REORDERABLE,
                       bool reorderable = true,
                       bool requires_original_order = false,
                       bool natural_barrier = false,
                       bool using_barrier = false,
                       bool equi_join = false,
                       bool has_key_metadata = false,
                       const std::string& left_order_key_text = {},
                       const std::string& right_order_key_text = {});

    void setJoinSelectivity(size_t edge_idx, double selectivity);

    std::shared_ptr<Path> optimize(core::ErrorContext* ctx = nullptr);
    std::shared_ptr<Path> optimizeGreedy(core::ErrorContext* ctx = nullptr);
    std::shared_ptr<Path> optimizeHypergraphGreedy(core::ErrorContext* ctx = nullptr);

    size_t numRelations() const { return relations_.size(); }
    size_t numJoinEdges() const { return join_edges_.size(); }

    void setPlanningControls(const JoinPlanningControls& controls)
    {
        controls_ = controls;
    }
    const JoinPlanningControls& planningControls() const { return controls_; }
    JoinSearchStrategy lastStrategyUsed() const { return last_strategy_used_; }
    const JoinPlanningTelemetry& lastTelemetry() const { return telemetry_; }

    void clear();

private:
    using RelationSet = uint64_t;
    using DPEntry = JoinSearchEntry;
    using DPFrontier = JoinSearchFrontier;

    // DP helper methods
    DPFrontier generateSubsetPlans(RelationSet subset, core::ErrorContext* ctx);
    DPFrontier enumerateJoinAlternatives(const DPEntry& left_entry,
                                         const DPEntry& right_entry,
                                         const JoinEdge& edge,
                                         core::ErrorContext* ctx,
                                         bool preserve_orientation = false);
    DPEntry costJoin(const DPEntry& left_entry, const DPEntry& right_entry,
                     const JoinEdge& edge, core::ErrorContext* ctx,
                     bool preserve_orientation = false);
    DPEntry costCrossJoin(const DPEntry& left_entry, const DPEntry& right_entry,
                          core::ErrorContext* ctx);
    std::shared_ptr<Path> optimizeBoundedDP(core::ErrorContext* ctx);
    std::shared_ptr<Path> optimizePreservingInputOrder(core::ErrorContext* ctx);
    JoinSearchStrategy chooseHeuristicStrategy() const;
    void resetTelemetry();
    void recordFallback(const std::string& reason,
                        const std::string& threshold_name,
                        size_t threshold_value);
    void pushFrontierEntry(DPFrontier& frontier, DPEntry entry);
    auto frontierBestPath(const DPFrontier& frontier) const -> std::shared_ptr<Path>;
    auto frontierBestEntry(const DPFrontier& frontier) const -> const DPEntry*;
    auto baseFrontierForRelation(const RelationInfo& relation,
                                size_t relation_index) -> DPFrontier;
    auto frontierSignature(const DPEntry& entry) const -> std::string;
    auto frontierBucketSignature(const DPEntry& entry) const -> std::string;
    auto frontierEntryDominates(const DPEntry& left,
                                const DPEntry& right) const -> bool;
    std::vector<size_t> findConnectingEdges(RelationSet left_set, RelationSet right_set) const;
    bool hasJoinReorderBarrier() const;
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
    JoinPlanningControls controls_;
    JoinSearchStrategy last_strategy_used_ = JoinSearchStrategy::AUTO;
    JoinPlanningTelemetry telemetry_;
    std::vector<RelationInfo> relations_;
    std::vector<JoinEdge> join_edges_;
    std::unordered_map<RelationSet, DPFrontier> dp_table_;
};

} // namespace scratchbird::optimizer

#endif // SCRATCHBIRD_OPTIMIZER_JOIN_ORDERING_H
