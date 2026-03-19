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
#include "scratchbird/optimizer/plan_payload.h"
#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/optimizer/parameter_bindings.h"
#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/optimizer/selectivity_estimator.h"
#include "scratchbird/optimizer/v3_semantic_analyzer.h"
#include "scratchbird/parser/shared_types.h"      // For JoinType, GroupingType, etc.
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace scratchbird::parser::v3 {
    class Statement;
    class ExplainStmt;
    class AnalyzeStmt;
    class SelectStmt;
}

namespace scratchbird::optimizer
{
    enum class PlannerStatementKind : uint8_t
    {
        UNKNOWN = 0,
        SELECT = 1,
        EXPLAIN = 2,
        ANALYZE = 3,
        UPDATE_DRIVER = 4,
        DELETE_DRIVER = 5,
        MERGE_DRIVER = 6,
        MAINTENANCE = 7,
        ADVISOR_WHAT_IF = 8,
    };

    struct PlannedSelectQuery
    {
        std::shared_ptr<PlanNode> root_plan;
        RuntimePlan runtime_plan;
        ResolvedSelectQuery resolved_query;
        bool reordered_relations = false;
        bool disconnected_join_graph = false;
    };

    struct StatementPlanRequest
    {
        PlannerStatementKind statement_kind = PlannerStatementKind::UNKNOWN;
        std::string normalized_statement_id;
        const parser::v3::Statement *normalized_statement_payload = nullptr;
        const parser::v3::StringPool *string_pool = nullptr;
        std::string string_pool_snapshot_id;
        const ParameterBindings *parameter_bindings = nullptr;
        core::ID current_schema_id = core::ID{};
        std::string catalog_snapshot_id;
        std::string statistics_snapshot_id;
        std::string family_metrics_snapshot_id;
        std::string security_snapshot_id;
        std::string planner_policy_snapshot_id;
        std::string artifact_mode = "RUNTIME_PLAN";
        std::string diagnostics_mode = "STANDARD";
        std::string cache_mode = "DEFAULT";
        std::string reuse_mode = "DEFAULT";
        std::string storage_layer_shape = "ROW_STORE_MGA";
        std::string publication_state_snapshot_id;
        std::string collector_specialization_request;
        std::string execution_intent_class = "EXECUTE";
        std::string continuation_context;
        std::string what_if_context;
    };

    struct StatementPlanResult
    {
        core::Status status_code = core::Status::INVALID_ARGUMENT;
        std::string normalized_request_digest;
        std::string plan_hash;
        RuntimePlan runtime_plan;
        std::shared_ptr<PlanNode> root_plan;
        std::string diagnostics_payload_json;
        std::string chosen_reuse_mode;
        std::vector<std::string> invalidation_dependencies;
        std::vector<std::string> compatibility_version_identifiers;
        std::string storage_layer_shape = "ROW_STORE_MGA";
        std::string publication_state_summary;
        std::string collector_specialization_id;
        std::string execution_intent_class;
        std::string continuation_token_contract;
        std::string rewrite_before_search_contract_id =
            kRewriteBeforeSearchContractId;
        std::string rewrite_before_search_owner_pass_id =
            "P01_SEMANTIC_NORMALIZE";
        std::string rewrite_before_search_terminal_pass_id =
            "P07_FILTER_PUSH_DOWN";
        bool rewrite_before_search_frozen = false;
        std::string tagging_contract_id = kAccessPathTaggingContractId;
        std::string tagging_owner_pass_id = "P08_ACCESS_PATH_ANNOTATE";
        std::string join_search_owner_pass_id = "P09_JOIN_ORDER_PLAN";
        std::string result_shape_finalize_pass_id =
            "P10_RESULT_SHAPE_FINALIZE";
        std::vector<std::string> fallback_and_rejection_stream;
        PlannedSelectQuery planned_select;
    };

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
              selectivity_estimator_(stats_manager, db),
              conn_ctx_(nullptr)
        {
        }

        auto planStatement(const StatementPlanRequest &request,
                           StatementPlanResult &result_out,
                           core::ErrorContext *ctx = nullptr,
                           core::ConnectionContext *conn_ctx = nullptr)
            -> core::Status;

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

        auto buildSelectPlan(const parser::v3::SelectStmt *select_stmt,
                             const parser::v3::StringPool &pool,
                             PlannedSelectQuery &planned_out,
                             core::ErrorContext *ctx = nullptr,
                             core::ConnectionContext *conn_ctx = nullptr,
                             const core::ID &current_schema_id = core::ID{},
                             const ParameterBindings *parameter_bindings = nullptr)
            -> core::Status;

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
        auto buildSelectPlanImpl(const parser::v3::SelectStmt *select_stmt,
                                 const parser::v3::StringPool &pool,
                                 PlannedSelectQuery &planned_out,
                                 core::ErrorContext *ctx = nullptr,
                                 core::ConnectionContext *conn_ctx = nullptr,
                                 const core::ID &current_schema_id = core::ID{},
                                 const ParameterBindings *parameter_bindings = nullptr)
            -> core::Status;

        core::Database *db_;
        CostModel cost_model_;
        StatisticsManager *stats_manager_;
        SelectivityEstimator selectivity_estimator_;
        core::ConnectionContext *conn_ctx_;
        mutable std::unordered_set<std::string> expanding_views_;
    };

} // namespace scratchbird::optimizer
