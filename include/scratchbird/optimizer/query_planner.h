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

/**
 * QueryPlanner - Cost-based Query Planner
 *
 * V3 MIGRATION STATUS: PENDING
 *
 * This optimizer is awaiting a V3 resolver that produces resolved query
 * structures with catalog IDs and fully-qualified references.
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
#include <memory>
#include <vector>
#include <unordered_set>

namespace scratchbird::parser::v3 {
    class AnalyzeStmt;
    class SelectStmt;
}

namespace scratchbird::optimizer
{

    /**
     * QueryPlanner - Cost-based query planner (V3 pending)
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
         * planQuery - Generate execution plan for SELECT statement (V3 AST)
         *
         * @param select_stmt SELECT statement (V3 AST)
         * @param ctx Error context
         * @param conn_ctx Connection context
         * @return Execution plan node tree, or nullptr on error
         */
        auto planQuery(const parser::v3::SelectStmt *select_stmt,
                       core::ErrorContext *ctx = nullptr,
                       core::ConnectionContext *conn_ctx = nullptr)
            -> std::shared_ptr<PlanNode>;

        /**
         * planAnalyze - Generate execution plan for ANALYZE statement (V3 AST)
         *
         * @param analyze_stmt ANALYZE statement (V3 AST)
         * @param ctx Error context
         * @return Execution plan node tree, or nullptr on error
         */
        auto planAnalyze(const parser::v3::AnalyzeStmt *analyze_stmt,
                         core::ErrorContext *ctx = nullptr)
            -> std::shared_ptr<PlanNode>;

    private:
        core::Database *db_;
        CostModel cost_model_;
        StatisticsManager *stats_manager_;
        SelectivityEstimator selectivity_estimator_;
        core::ConnectionContext *conn_ctx_;
        mutable std::unordered_set<std::string> expanding_views_;
    };

} // namespace scratchbird::optimizer
