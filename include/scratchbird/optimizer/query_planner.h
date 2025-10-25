#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/optimizer/path.h"
#include "scratchbird/optimizer/plan_node.h"
#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/optimizer/selectivity_estimator.h"
#include "scratchbird/parser/ast.h"
#include <memory>
#include <vector>

namespace scratchbird::optimizer
{

    /**
     * QueryPlanner - Cost-based query planner
     *
     * Generates multiple execution paths for a query, estimates costs,
     * and selects the cheapest path.
     *
     * Planning Pipeline:
     *   1. Receive AST from parser
     *   2. Generate all feasible paths (SeqScan, IndexScans)
     *   3. Estimate cost for each path using CostModel + Statistics
     *   4. Select cheapest path
     *   5. Convert to PlanNode tree
     *   6. Return PlanNode to bytecode generator
     *
     * Current Scope (Phase 1):
     * - Single-table SELECT queries
     * - WHERE clause with simple predicates
     * - SeqScan and IndexScan paths
     * - No joins, no aggregation, no sorting
     *
     * Phase 1, Task 1.3
     */
    class QueryPlanner
    {
    public:
        /**
         * Constructor
         *
         * @param db Database instance
         * @param cost_model Cost model for path costing
         * @param stats_manager Statistics manager for cardinality estimation
         */
        QueryPlanner(core::Database *db,
                     const CostModel &cost_model,
                     StatisticsManager *stats_manager)
            : db_(db),
              cost_model_(cost_model),
              stats_manager_(stats_manager),
              selectivity_estimator_(stats_manager)
        {
        }

        /**
         * planQuery - Generate execution plan for SELECT statement
         *
         * Planning algorithm:
         *   1. Generate sequential scan path
         *   2. Generate index scan paths (one per applicable index)
         *   3. Estimate costs for all paths
         *   4. Select cheapest path
         *   5. Convert to PlanNode
         *
         * @param select_stmt SELECT statement AST
         * @param ctx Error context
         * @return Execution plan (PlanNode tree) or nullptr on error
         *
         * Phase 1, Task 1.3.3
         */
        auto planQuery(const parser::SelectStmt *select_stmt,
                       core::ErrorContext *ctx = nullptr)
            -> std::shared_ptr<PlanNode>;

        /**
         * planAnalyze - Generate execution plan for ANALYZE statement
         *
         * Creates a simple plan that triggers statistics collection.
         *
         * @param analyze_stmt ANALYZE statement AST
         * @param ctx Error context
         * @return Execution plan or nullptr on error
         *
         * Phase 1, Task 1.3.3
         */
        auto planAnalyze(const parser::AnalyzeStmt *analyze_stmt,
                         core::ErrorContext *ctx = nullptr)
            -> std::shared_ptr<PlanNode>;

    private:
        /**
         * generatePaths - Generate all feasible paths for single-table query
         *
         * Generates:
         * - One SeqScanPath (always feasible)
         * - One IndexScanPath per applicable index
         *
         * Index applicability rules:
         * - Index column matches WHERE clause column
         * - Operator is compatible with index (=, <, >, <=, >=, BETWEEN)
         * - For B-tree: supports all comparison operators
         *
         * @param select_stmt SELECT statement
         * @param table_id Table ID
         * @param paths Output vector of generated paths
         * @param ctx Error context
         * @return Status
         *
         * Phase 1, Task 1.3.4
         */
        auto generatePaths(const parser::SelectStmt *select_stmt,
                           const core::ID &table_id,
                           std::vector<std::shared_ptr<Path>> &paths,
                           core::ErrorContext *ctx)
            -> core::Status;

        /**
         * generateSeqScanPath - Generate sequential scan path
         *
         * Cost calculation:
         *   1. Get table statistics (num_pages, num_rows)
         *   2. Estimate selectivity of WHERE clause
         *   3. Calculate qual_cost (sum of operator costs)
         *   4. Call cost_model.costSeqScan()
         *
         * @param select_stmt SELECT statement
         * @param table_id Table ID
         * @param table_name Table name
         * @param ctx Error context
         * @return SeqScanPath or nullptr
         *
         * Phase 1, Task 1.3.4
         */
        auto generateSeqScanPath(const parser::SelectStmt *select_stmt,
                                 const core::ID &table_id,
                                 const std::string &table_name,
                                 core::ErrorContext *ctx)
            -> std::shared_ptr<SeqScanPath>;

        /**
         * generateIndexScanPaths - Generate index scan paths
         *
         * For each index on table:
         *   1. Check if index is applicable to WHERE clause
         *   2. If applicable, estimate index scan cost
         *   3. Create IndexScanPath
         *
         * Index applicability:
         * - Index column must appear in WHERE clause
         * - Operator must be index-compatible (=, <, >, <=, >=, BETWEEN)
         * - For multi-column indexes, first column must match (Phase 1 limitation)
         *
         * @param select_stmt SELECT statement
         * @param table_id Table ID
         * @param table_name Table name
         * @param paths Output vector to append index paths
         * @param ctx Error context
         * @return Status
         *
         * Phase 1, Task 1.3.4
         */
        auto generateIndexScanPaths(const parser::SelectStmt *select_stmt,
                                    const core::ID &table_id,
                                    const std::string &table_name,
                                    std::vector<std::shared_ptr<Path>> &paths,
                                    core::ErrorContext *ctx)
            -> core::Status;

        /**
         * isIndexApplicable - Check if index can be used for WHERE clause
         *
         * Rules:
         * - Index column matches WHERE predicate column
         * - Operator is index-compatible
         * - B-tree indexes support: =, <, >, <=, >=, BETWEEN
         *
         * @param index_id Index ID
         * @param select_stmt SELECT statement
         * @param ctx Error context
         * @return true if index is applicable
         *
         * Phase 1, Task 1.3.4
         */
        auto isIndexApplicable(const core::ID &index_id,
                               const parser::SelectStmt *select_stmt,
                               core::ErrorContext *ctx) const
            -> bool;

        /**
         * selectCheapestPath - Select path with lowest total cost
         *
         * Comparison criteria (in order):
         * 1. Total cost (lower is better)
         * 2. If costs are equal, prefer index scan (provides ordering)
         *
         * @param paths Vector of candidate paths
         * @return Cheapest path or nullptr if empty
         *
         * Phase 1, Task 1.3.5
         */
        auto selectCheapestPath(const std::vector<std::shared_ptr<Path>> &paths) const
            -> std::shared_ptr<Path>;

        /**
         * pathToPlanNode - Convert Path to PlanNode
         *
         * Creates appropriate PlanNode subclass:
         * - SeqScanPath → SeqScanNode
         * - IndexScanPath → IndexScanNode
         *
         * @param path Path to convert
         * @param ctx Error context
         * @return PlanNode or nullptr
         *
         * Phase 1, Task 1.3.5
         */
        auto pathToPlanNode(const std::shared_ptr<Path> &path,
                            core::ErrorContext *ctx)
            -> std::shared_ptr<PlanNode>;

        /**
         * estimateSelectivity - Estimate selectivity of WHERE clause
         *
         * Returns fraction of rows passing WHERE clause (0.0 to 1.0).
         *
         * For Phase 1, uses simple heuristics:
         * - Equality (=): 1.0 / n_distinct
         * - Range (<, >, <=, >=): 0.33 (default guess)
         * - BETWEEN: 0.10 (default guess)
         * - No WHERE clause: 1.0 (all rows)
         *
         * Phase 2 will use histogram-based estimation (Task 1.4).
         *
         * @param select_stmt SELECT statement
         * @param table_id Table ID
         * @param ctx Error context
         * @return Selectivity estimate (0.0 to 1.0)
         *
         * Phase 1, Task 1.3.4
         */
        auto estimateSelectivity(const parser::SelectStmt *select_stmt,
                                 const core::ID &table_id,
                                 core::ErrorContext *ctx) const
            -> double;

        /**
         * calculateQualCost - Calculate cost of WHERE clause evaluation
         *
         * Sums cpu_operator_cost for each predicate in WHERE clause.
         *
         * Example:
         *   WHERE age > 25 AND name = 'Alice'
         *   qual_cost = operatorCost(">") + operatorCost("=")
         *             = 0.0025 + 0.0025 = 0.005
         *
         * @param select_stmt SELECT statement
         * @return Qualification cost
         *
         * Phase 1, Task 1.3.4
         */
        auto calculateQualCost(const parser::SelectStmt *select_stmt) const
            -> double;

    private:
        core::Database *db_;
        CostModel cost_model_;
        StatisticsManager *stats_manager_;
        SelectivityEstimator selectivity_estimator_;
    };

} // namespace scratchbird::optimizer
