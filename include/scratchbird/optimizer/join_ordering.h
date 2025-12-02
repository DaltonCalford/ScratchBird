/**
 * join_ordering.h - Dynamic Programming Join Order Optimizer
 *
 * P3-20: Cost-based join ordering optimization using dynamic programming.
 * Given N tables to join, finds the optimal join order that minimizes
 * total query execution cost.
 *
 * Algorithm: Bottom-up dynamic programming (similar to Selinger et al.)
 * - Enumerate all 2^N subsets of tables
 * - For each subset, find optimal way to construct it from smaller subsets
 * - Memoize results to avoid recomputation
 *
 * Time complexity: O(3^N) for N tables
 * Space complexity: O(2^N) for memoization table
 *
 * Practical limits:
 * - N <= 12 tables: DP is fast (< 1 second)
 * - N > 12 tables: Falls back to greedy heuristic
 */

#ifndef SCRATCHBIRD_OPTIMIZER_JOIN_ORDERING_H
#define SCRATCHBIRD_OPTIMIZER_JOIN_ORDERING_H

#include "scratchbird/optimizer/path.h"
#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/optimizer/selectivity_estimator.h"
#include "scratchbird/parser/ast.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/types.h"  // For core::ID
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <bitset>
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
    std::string alias;  // Table alias (if any)
    std::shared_ptr<Path> best_path;  // Best access path for this relation
    uint64_t rows;  // Estimated row count
    uint64_t width;  // Estimated row width in bytes
};

/**
 * JoinEdge - Represents a join condition between two relations
 */
struct JoinEdge
{
    size_t left_rel_idx;   // Index of left relation
    size_t right_rel_idx;  // Index of right relation
    parser::JoinType join_type;
    parser::Expression* join_condition;
    double selectivity;  // Estimated join selectivity (0.0 - 1.0)
};

/**
 * JoinOrderingOptimizer - Finds optimal join order using dynamic programming
 *
 * Usage:
 *   JoinOrderingOptimizer optimizer(cost_model, selectivity_estimator);
 *   optimizer.addRelation(table_id, table_name, best_path);
 *   optimizer.addJoinEdge(0, 1, INNER, condition);
 *   auto best_plan = optimizer.optimize(ctx);
 */
class JoinOrderingOptimizer
{
public:
    // Maximum number of relations for DP (2^12 = 4096 subsets)
    static constexpr size_t MAX_DP_RELATIONS = 12;

    /**
     * Constructor
     * @param cost_model Reference to cost model for costing join plans
     * @param selectivity_estimator Reference to selectivity estimator
     */
    JoinOrderingOptimizer(CostModel& cost_model,
                         SelectivityEstimator& selectivity_estimator);

    /**
     * Add a base relation to the join graph
     * @param table_id Table ID in catalog
     * @param table_name Table name
     * @param alias Table alias (optional)
     * @param best_path Best access path for this relation
     * @return Index of the added relation
     */
    size_t addRelation(const core::ID& table_id,
                      const std::string& table_name,
                      const std::string& alias,
                      std::shared_ptr<Path> best_path);

    /**
     * Add a join edge between two relations
     * @param left_idx Index of left relation
     * @param right_idx Index of right relation
     * @param join_type Type of join (INNER, LEFT, RIGHT, FULL)
     * @param join_condition Join condition expression
     */
    void addJoinEdge(size_t left_idx, size_t right_idx,
                    parser::JoinType join_type,
                    parser::Expression* join_condition);

    /**
     * Set estimated selectivity for a join edge
     * @param edge_idx Index of the join edge
     * @param selectivity Selectivity value (0.0 - 1.0)
     */
    void setJoinSelectivity(size_t edge_idx, double selectivity);

    /**
     * Find optimal join order using dynamic programming
     * @param ctx Error context for reporting errors
     * @return Best join path, or nullptr if optimization fails
     */
    std::shared_ptr<Path> optimize(core::ErrorContext* ctx = nullptr);

    /**
     * Find join order using greedy heuristic (fallback for large queries)
     * @param ctx Error context for reporting errors
     * @return Greedy join path, or nullptr if optimization fails
     */
    std::shared_ptr<Path> optimizeGreedy(core::ErrorContext* ctx = nullptr);

    /**
     * Get the number of relations in the join graph
     */
    size_t numRelations() const { return relations_.size(); }

    /**
     * Get the number of join edges
     */
    size_t numJoinEdges() const { return join_edges_.size(); }

    /**
     * Clear all relations and edges for reuse
     */
    void clear();

private:
    // Bitmask type for representing subsets of relations
    using RelationSet = uint64_t;

    // Entry in the DP memoization table
    struct DPEntry
    {
        std::shared_ptr<Path> best_path;
        double cost;
        uint64_t rows;

        DPEntry() : cost(std::numeric_limits<double>::max()), rows(0) {}
    };

    // References to external components
    CostModel& cost_model_;
    SelectivityEstimator& selectivity_estimator_;

    // Join graph
    std::vector<RelationInfo> relations_;
    std::vector<JoinEdge> join_edges_;

    // DP memoization table: subset -> best plan for that subset
    std::unordered_map<RelationSet, DPEntry> dp_table_;

    // Build adjacency list for join graph
    std::vector<std::vector<size_t>> buildAdjacencyList() const;

    // Check if a relation set is connected (forms a valid join)
    bool isConnected(RelationSet set) const;

    // Find all join edges connecting two relation sets
    std::vector<size_t> findConnectingEdges(RelationSet left_set,
                                            RelationSet right_set) const;

    // Cost a specific join between two subsets
    DPEntry costJoin(const DPEntry& left_entry,
                     const DPEntry& right_entry,
                     const JoinEdge& edge,
                     core::ErrorContext* ctx);

    // Generate best join path for a subset of relations
    DPEntry generateSubsetPlan(RelationSet subset, core::ErrorContext* ctx);

    // Helper: Convert RelationSet to string for debugging
    std::string setToString(RelationSet set) const;

    // Helper: Check if a bit is set
    static bool hasBit(RelationSet set, size_t idx) { return (set & (1ULL << idx)) != 0; }

    // Helper: Set a bit
    static RelationSet setBit(RelationSet set, size_t idx) { return set | (1ULL << idx); }

    // Helper: Count bits (popcount)
    static size_t countBits(RelationSet set);

    // Helper: Get lowest set bit index
    static size_t lowestBit(RelationSet set);

    // Helper: Enumerate all subsets of a given set
    static std::vector<RelationSet> enumerateSubsets(RelationSet set);
};

} // namespace scratchbird::optimizer

#endif // SCRATCHBIRD_OPTIMIZER_JOIN_ORDERING_H
