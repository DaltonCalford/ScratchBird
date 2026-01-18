#pragma once

/**
 * QueryPlanner - Cost-based Query Planner
 *
 * V2 MIGRATION STATUS: COMPLETE
 *
 * This optimizer generates execution plans using V2 resolved AST types.
 * It supports:
 * - Single table SELECT with WHERE, GROUP BY, ORDER BY, LIMIT/OFFSET
 * - JOIN queries using JoinOrderingOptimizer
 * - ANALYZE statements
 *
 * NOTE: The main client execution path is:
 *   ServerSession → QueryCompilerV2 → BytecodeGeneratorV2 → Executor
 *
 * QueryPlanner is primarily used for EXPLAIN functionality.
 */

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/optimizer/path.h"
#include "scratchbird/optimizer/plan_node.h"
#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/optimizer/selectivity_estimator.h"
#include "scratchbird/parser/shared_types.h"      // For JoinType, GroupingType, etc.
#include "scratchbird/sblr/resolved_ast_v2.h"     // For V2 ResolvedSelectStmt, ResolvedExpression, etc.
#include <memory>
#include <vector>
#include <unordered_set>

namespace scratchbird::optimizer
{

    /**
     * QueryPlanner - Cost-based query planner (V2 Complete)
     *
     * Generates execution plans using V2 resolved AST types.
     * Supports single-table queries, JOINs, aggregation, sorting, and LIMIT.
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
              selectivity_estimator_(stats_manager),
              conn_ctx_(nullptr)
        {
        }

        /**
         * planQuery - Generate execution plan for SELECT statement (V2 API)
         *
         * @param select_stmt SELECT statement (V2 Resolved AST)
         * @param ctx Error context
         * @param conn_ctx Connection context
         * @return Execution plan node tree, or nullptr on error
         */
        auto planQuery(const parser::v2::ResolvedSelectStmt *select_stmt,
                       core::ErrorContext *ctx = nullptr,
                       core::ConnectionContext *conn_ctx = nullptr)
            -> std::shared_ptr<PlanNode>;

        /**
         * planAnalyze - Generate execution plan for ANALYZE statement (V2 API)
         *
         * @param analyze_stmt ANALYZE statement (V2 Resolved AST)
         * @param ctx Error context
         * @return Execution plan node tree, or nullptr on error
         */
        auto planAnalyze(const parser::v2::ResolvedAnalyzeStmt *analyze_stmt,
                         core::ErrorContext *ctx = nullptr)
            -> std::shared_ptr<PlanNode>;

    private:
        /**
         * Plan a JOIN query using JoinOrderingOptimizer
         */
        std::shared_ptr<PlanNode> planJoinQuery(
            const parser::v2::ResolvedSelectStmt* select_stmt,
            core::ErrorContext* ctx);

        /**
         * Check SELECT permissions for all FROM tables before planning.
         */
        bool checkSelectPermissions(
            const parser::v2::ResolvedSelectStmt* select_stmt,
            core::ErrorContext* ctx);

        /**
         * Wrap base plan with GROUP BY, ORDER BY, LIMIT/OFFSET nodes
         */
        std::shared_ptr<PlanNode> wrapWithClauses(
            std::shared_ptr<PlanNode> base_plan,
            const parser::v2::ResolvedSelectStmt* select_stmt);

        core::Database *db_;
        CostModel cost_model_;
        StatisticsManager *stats_manager_;
        SelectivityEstimator selectivity_estimator_;
        core::ConnectionContext *conn_ctx_;
        mutable std::unordered_set<std::string> expanding_views_;
    };

} // namespace scratchbird::optimizer
