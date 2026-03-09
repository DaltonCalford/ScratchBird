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
 * QueryPlanner - Cost-based Query Planner Implementation
 *
 * V3 implementation:
 * - Resolves SELECT statements against catalog metadata
 * - Chooses base access paths using statistics and cost model estimates
 * - Builds left-deep join plans with deterministic join-method selection
 * - Preserves outer/natural join order and fails closed to nested loops there
 */

#include "scratchbird/optimizer/query_planner.h"

#include "scratchbird/core/debug.h"
#include <algorithm>
#include <limits>
#include <optional>
#include <sstream>

namespace scratchbird::optimizer
{
    namespace
    {
        struct BaseAccessChoice
        {
            size_t relation_index = 0;
            std::shared_ptr<Path> path;
            std::shared_ptr<PlanNode> plan;
            RuntimePlanRelation runtime_relation;
            double selectivity = 1.0;
        };

        struct JoinDecision
        {
            bool valid = false;
            size_t join_index = 0;
            size_t candidate_relation_index = 0;
            std::shared_ptr<Path> path;
            std::shared_ptr<PlanNode> plan;
            RuntimePlanJoinStep runtime_join;
            double selectivity = 1.0;
            double total_cost = std::numeric_limits<double>::max();
            uint64_t rows = 0;
        };

        auto planJoinTypeToString(parser::JoinType join_type) -> std::string
        {
            switch (join_type)
            {
                case parser::JoinType::INNER: return "INNER";
                case parser::JoinType::LEFT: return "LEFT";
                case parser::JoinType::RIGHT: return "RIGHT";
                case parser::JoinType::FULL: return "FULL";
                case parser::JoinType::CROSS: return "CROSS";
                case parser::JoinType::NATURAL: return "NATURAL";
                case parser::JoinType::NATURAL_LEFT: return "NATURAL_LEFT";
                case parser::JoinType::NATURAL_RIGHT: return "NATURAL_RIGHT";
                case parser::JoinType::NATURAL_FULL: return "NATURAL_FULL";
            }
            return "INNER";
        }

        auto displayRelationName(const ResolvedRelation &relation) -> std::string
        {
            if (!relation.alias.empty() &&
                !core::IdentifierUtils::namesMatch(relation.alias,
                                                   false,
                                                   relation.table_info.table_name,
                                                   false))
            {
                if (!relation.table_info.table_name.empty())
                {
                    return relation.alias + " (" + relation.table_info.table_name + ")";
                }
                if (!relation.table_path.empty())
                {
                    return relation.alias + " (" + relation.table_path + ")";
                }
                return relation.alias;
            }
            if (!relation.alias.empty())
            {
                return relation.alias;
            }
            if (!relation.table_info.table_name.empty())
            {
                return relation.table_info.table_name;
            }
            return relation.table_path.empty() ? "<derived>" : relation.table_path;
        }

        auto relationLookupName(const ResolvedRelation &relation) -> std::string
        {
            if (!relation.alias.empty())
            {
                return relation.alias;
            }
            if (!relation.table_info.table_name.empty())
            {
                return relation.table_info.table_name;
            }
            return relation.table_path;
        }

        auto stablePlanHash(const std::string &text) -> std::string
        {
            uint64_t hash = 1469598103934665603ULL;
            for (unsigned char ch : text)
            {
                hash ^= static_cast<uint64_t>(ch);
                hash *= 1099511628211ULL;
            }
            std::ostringstream out;
            out << std::hex << hash;
            return out.str();
        }

        auto isZeroId(const core::ID &id) -> bool
        {
            return id == core::ID{};
        }

        auto relationOutputRows(uint64_t base_rows, double selectivity) -> uint64_t
        {
            if (base_rows == 0)
            {
                return 0;
            }
            uint64_t rows = static_cast<uint64_t>(
                static_cast<double>(base_rows) * std::max(0.0, std::min(1.0, selectivity)));
            if (rows == 0)
            {
                rows = 1;
            }
            return rows;
        }

        auto toRuntimePlanNode(const std::shared_ptr<PlanNode> &plan) -> RuntimePlanNode
        {
            RuntimePlanNode node;
            if (!plan)
            {
                return node;
            }

            node.startup_cost = plan->startupCost();
            node.total_cost = plan->totalCost();
            node.estimated_rows = plan->rows();

            switch (plan->type())
            {
                case PlanNodeType::SEQ_SCAN: {
                    auto seq = std::dynamic_pointer_cast<SeqScanNode>(plan);
                    node.node_type = "SeqScan";
                    if (seq)
                    {
                        node.table_path = seq->tableName();
                    }
                    break;
                }
                case PlanNodeType::INDEX_SCAN: {
                    auto idx = std::dynamic_pointer_cast<IndexScanNode>(plan);
                    node.node_type = "IndexScan";
                    if (idx)
                    {
                        node.table_path = idx->tableName();
                        node.index_name = idx->indexName();
                        node.condition_text = idx->indexCond();
                    }
                    break;
                }
                case PlanNodeType::INDEX_ONLY_SCAN: {
                    auto idx = std::dynamic_pointer_cast<IndexOnlyScanNode>(plan);
                    node.node_type = "IndexOnlyScan";
                    if (idx)
                    {
                        node.table_path = idx->tableName();
                        node.index_name = idx->indexName();
                        node.condition_text = idx->indexCond();
                    }
                    break;
                }
                case PlanNodeType::BITMAP_INDEX_SCAN: {
                    auto bitmap = std::dynamic_pointer_cast<BitmapIndexScanNode>(plan);
                    node.node_type = "BitmapIndexScan";
                    if (bitmap)
                    {
                        node.table_path = bitmap->tableName();
                        node.detail_text = bitmap->bitmapOp();
                        if (!bitmap->indexNames().empty())
                        {
                            node.index_name = bitmap->indexNames().front();
                        }
                    }
                    break;
                }
                case PlanNodeType::NESTED_LOOP_JOIN: {
                    auto join = std::dynamic_pointer_cast<NestedLoopJoinNode>(plan);
                    node.node_type = "NestedLoopJoin";
                    if (join)
                    {
                        node.join_type = planJoinTypeToString(join->joinType());
                        node.condition_text = join->joinCondString();
                        node.children.push_back(toRuntimePlanNode(join->outerPlan()));
                        node.children.push_back(toRuntimePlanNode(join->innerPlan()));
                    }
                    break;
                }
                case PlanNodeType::HASH_JOIN: {
                    auto join = std::dynamic_pointer_cast<HashJoinNode>(plan);
                    node.node_type = "HashJoin";
                    if (join)
                    {
                        node.join_type = planJoinTypeToString(join->joinType());
                        node.condition_text = join->joinCondString();
                        node.children.push_back(toRuntimePlanNode(join->outerPlan()));
                        node.children.push_back(toRuntimePlanNode(join->innerPlan()));
                    }
                    break;
                }
                case PlanNodeType::AGGREGATE: {
                    auto aggregate = std::dynamic_pointer_cast<AggregateNode>(plan);
                    node.node_type = "Aggregate";
                    if (aggregate)
                    {
                        node.detail_text =
                            aggregate->isSimpleAggregation() ? "SIMPLE" : "GROUP";
                        node.children.push_back(toRuntimePlanNode(aggregate->childPlan()));
                    }
                    break;
                }
                case PlanNodeType::SORT: {
                    auto sort = std::dynamic_pointer_cast<SortNode>(plan);
                    node.node_type = "Sort";
                    if (sort)
                    {
                        node.detail_text =
                            std::to_string(sort->orderByItems().size()) + " keys";
                        node.children.push_back(toRuntimePlanNode(sort->childPlan()));
                    }
                    break;
                }
                case PlanNodeType::LIMIT: {
                    auto limit = std::dynamic_pointer_cast<LimitNode>(plan);
                    node.node_type = "Limit";
                    if (limit)
                    {
                        std::ostringstream detail;
                        detail << "limit=" << limit->limitCount();
                        if (limit->offsetCount() >= 0)
                        {
                            detail << " offset=" << limit->offsetCount();
                        }
                        node.detail_text = detail.str();
                        node.children.push_back(toRuntimePlanNode(limit->childPlan()));
                    }
                    break;
                }
                case PlanNodeType::WINDOW: {
                    auto window = std::dynamic_pointer_cast<WindowNode>(plan);
                    node.node_type = "Window";
                    if (window)
                    {
                        node.detail_text =
                            std::to_string(window->windowFunctions().size()) + " funcs";
                        node.children.push_back(toRuntimePlanNode(window->child()));
                    }
                    break;
                }
                default:
                    node.node_type = "PlanNode";
                    for (const auto &child : plan->children())
                    {
                        node.children.push_back(toRuntimePlanNode(child));
                    }
                    break;
            }

            return node;
        }

        auto isJoinReorderSafe(const ResolvedSelectQuery &resolved) -> bool
        {
            if (!resolved.all_joins_inner)
            {
                return false;
            }
            for (const auto &join : resolved.joins)
            {
                if (join.natural || !join.using_columns.empty())
                {
                    return false;
                }
            }
            return true;
        }

        auto makeScanPredicateLiteralBytes(const ResolvedScanPredicate &predicate,
                                           std::vector<uint8_t> &bytes_out) -> bool
        {
            if (predicate.literal_kind == "STRING")
            {
                bytes_out.assign(predicate.literal_text.begin(), predicate.literal_text.end());
                return true;
            }
            if (predicate.literal_kind == "INTEGER")
            {
                try
                {
                    int64_t value = std::stoll(predicate.literal_text);
                    bytes_out.resize(sizeof(value));
                    std::memcpy(bytes_out.data(), &value, sizeof(value));
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }
            if (predicate.literal_kind == "FLOAT")
            {
                try
                {
                    double value = std::stod(predicate.literal_text);
                    bytes_out.resize(sizeof(value));
                    std::memcpy(bytes_out.data(), &value, sizeof(value));
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }
            if (predicate.literal_kind == "BOOLEAN")
            {
                bytes_out = {static_cast<uint8_t>(predicate.literal_text == "TRUE" ? 1 : 0)};
                return true;
            }
            if (predicate.literal_kind == "NULL")
            {
                bytes_out.clear();
                return true;
            }
            return false;
        }

        auto isAggregateFunctionName(const std::string &name) -> bool
        {
            const std::string upper = core::IdentifierUtils::toUpper(name);
            return upper == "COUNT" || upper == "SUM" || upper == "AVG" ||
                   upper == "MIN" || upper == "MAX" || upper == "STDDEV" ||
                   upper == "STDDEV_POP" || upper == "STDDEV_SAMP" ||
                   upper == "VARIANCE" || upper == "VAR_POP" || upper == "VAR_SAMP" ||
                   upper == "COVAR_POP" || upper == "COVAR_SAMP" || upper == "CORR";
        }

        auto collectExpressionColumns(const parser::v3::Expression *expr,
                                      const parser::v3::StringPool &pool,
                                      std::vector<std::pair<std::string, std::string>> &out) -> void
        {
            if (expr == nullptr)
            {
                return;
            }

            switch (expr->kind())
            {
                case parser::v3::ASTKind::ColumnRefExpr: {
                    const auto *column_ref = static_cast<const parser::v3::ColumnRefExpr *>(expr);
                    if (column_ref->column.column_name == parser::v3::StringPool::INVALID_ID)
                    {
                        return;
                    }
                    std::string qualifier;
                    if (column_ref->column.has_table_qualifier &&
                        !column_ref->column.table_path.components.empty())
                    {
                        qualifier =
                            pool.get(column_ref->column.table_path.components.back());
                    }
                    out.push_back(
                        {qualifier, std::string(pool.get(column_ref->column.column_name))});
                    return;
                }
                case parser::v3::ASTKind::BinaryExpr: {
                    const auto *binary = static_cast<const parser::v3::BinaryExpr *>(expr);
                    collectExpressionColumns(binary->left, pool, out);
                    collectExpressionColumns(binary->right, pool, out);
                    return;
                }
                case parser::v3::ASTKind::UnaryExpr: {
                    const auto *unary = static_cast<const parser::v3::UnaryExpr *>(expr);
                    collectExpressionColumns(unary->operand, pool, out);
                    return;
                }
                case parser::v3::ASTKind::CastExpr: {
                    const auto *cast_expr = static_cast<const parser::v3::CastExpr *>(expr);
                    collectExpressionColumns(cast_expr->expr, pool, out);
                    return;
                }
                case parser::v3::ASTKind::FunctionCallExpr: {
                    const auto *func = static_cast<const parser::v3::FunctionCallExpr *>(expr);
                    for (const auto *arg : func->arguments)
                    {
                        collectExpressionColumns(arg, pool, out);
                    }
                    if (func->filter != nullptr)
                    {
                        collectExpressionColumns(func->filter, pool, out);
                    }
                    return;
                }
                case parser::v3::ASTKind::LikeExpr: {
                    const auto *like_expr = static_cast<const parser::v3::LikeExpr *>(expr);
                    collectExpressionColumns(like_expr->expr, pool, out);
                    collectExpressionColumns(like_expr->pattern, pool, out);
                    return;
                }
                case parser::v3::ASTKind::BetweenExpr: {
                    const auto *between_expr = static_cast<const parser::v3::BetweenExpr *>(expr);
                    collectExpressionColumns(between_expr->expr, pool, out);
                    collectExpressionColumns(between_expr->low, pool, out);
                    collectExpressionColumns(between_expr->high, pool, out);
                    return;
                }
                default:
                    return;
            }
        }

        auto collectRequiredColumnIds(const ResolvedSelectQuery &resolved,
                                      size_t relation_index,
                                      std::vector<core::ID> &column_ids_out,
                                      std::vector<parser::v3::FunctionCallExpr *> &aggregates_out,
                                      std::vector<parser::v3::FunctionCallExpr *> &windows_out)
            -> void
        {
            column_ids_out.clear();
            aggregates_out.clear();
            windows_out.clear();

            if (resolved.stmt == nullptr || resolved.string_pool == nullptr ||
                relation_index >= resolved.relations.size())
            {
                return;
            }

            const auto &relation = resolved.relations[relation_index];
            const auto &pool = *resolved.string_pool;
            std::vector<std::pair<std::string, std::string>> refs;

            auto add_column_id = [&](const std::string &qualifier,
                                     const std::string &column_name) {
                for (const auto &column : relation.columns)
                {
                    if (!core::IdentifierUtils::namesMatch(column.column_name,
                                                           false,
                                                           column_name,
                                                           false))
                    {
                        continue;
                    }
                    if (!qualifier.empty())
                    {
                        const bool matches_alias =
                            !relation.alias.empty() &&
                            core::IdentifierUtils::namesMatch(relation.alias,
                                                              false,
                                                              qualifier,
                                                              false);
                        const bool matches_table =
                            !relation.table_info.table_name.empty() &&
                            core::IdentifierUtils::namesMatch(relation.table_info.table_name,
                                                              false,
                                                              qualifier,
                                                              false);
                        const bool matches_path =
                            !relation.table_path.empty() &&
                            core::IdentifierUtils::namesMatch(relation.table_path,
                                                              false,
                                                              qualifier,
                                                              false);
                        if (!matches_alias && !matches_table && !matches_path)
                        {
                            continue;
                        }
                    }
                    if (std::find(column_ids_out.begin(),
                                  column_ids_out.end(),
                                  column.column_id) == column_ids_out.end())
                    {
                        column_ids_out.push_back(column.column_id);
                    }
                }
            };

            auto scan_expr = [&](const parser::v3::Expression *expr,
                                 auto &&scan_expr_ref) -> void {
                if (expr == nullptr)
                {
                    return;
                }
                collectExpressionColumns(expr, pool, refs);
                if (expr->kind() == parser::v3::ASTKind::FunctionCallExpr)
                {
                    const auto *func = static_cast<const parser::v3::FunctionCallExpr *>(expr);
                    if (!func->function_path.components.empty())
                    {
                        const std::string func_name =
                            std::string(pool.get(func->function_path.components.back()));
                        if (isAggregateFunctionName(func_name))
                        {
                            aggregates_out.push_back(
                                const_cast<parser::v3::FunctionCallExpr *>(func));
                        }
                    }
                    if (func->is_window)
                    {
                        windows_out.push_back(
                            const_cast<parser::v3::FunctionCallExpr *>(func));
                    }
                    for (const auto *arg : func->arguments)
                    {
                        scan_expr_ref(arg, scan_expr_ref);
                    }
                    if (func->filter != nullptr)
                    {
                        scan_expr_ref(func->filter, scan_expr_ref);
                    }
                    return;
                }

                if (expr->kind() == parser::v3::ASTKind::BinaryExpr)
                {
                    const auto *binary = static_cast<const parser::v3::BinaryExpr *>(expr);
                    scan_expr_ref(binary->left, scan_expr_ref);
                    scan_expr_ref(binary->right, scan_expr_ref);
                }
                else if (expr->kind() == parser::v3::ASTKind::UnaryExpr)
                {
                    const auto *unary = static_cast<const parser::v3::UnaryExpr *>(expr);
                    scan_expr_ref(unary->operand, scan_expr_ref);
                }
                else if (expr->kind() == parser::v3::ASTKind::CastExpr)
                {
                    const auto *cast_expr = static_cast<const parser::v3::CastExpr *>(expr);
                    scan_expr_ref(cast_expr->expr, scan_expr_ref);
                }
            };

            for (const auto *item : resolved.stmt->items)
            {
                if (item == nullptr)
                {
                    continue;
                }
                if (item->item_type == parser::v3::SelectItem::Type::STAR)
                {
                    for (const auto &column : relation.columns)
                    {
                        column_ids_out.push_back(column.column_id);
                    }
                    continue;
                }
                if (item->item_type == parser::v3::SelectItem::Type::TABLE_STAR)
                {
                    for (const auto &column : relation.columns)
                    {
                        column_ids_out.push_back(column.column_id);
                    }
                    continue;
                }
                scan_expr(item->expr, scan_expr);
            }

            for (const auto *expr : resolved.stmt->group_by)
            {
                scan_expr(expr, scan_expr);
            }
            if (resolved.stmt->having != nullptr)
            {
                scan_expr(resolved.stmt->having, scan_expr);
            }
            for (const auto *order_by : resolved.stmt->order_by)
            {
                if (order_by != nullptr)
                {
                    scan_expr(order_by->expr, scan_expr);
                }
            }
            if (relation.local_predicate.has_value() &&
                relation.local_predicate->expression != nullptr)
            {
                scan_expr(relation.local_predicate->expression, scan_expr);
            }

            for (const auto &[qualifier, column_name] : refs)
            {
                add_column_id(qualifier, column_name);
            }
        }

        auto predicateSupportsExactLookup(const ResolvedScanPredicate &predicate) -> bool
        {
            return predicate.kind == ResolvedPredicateKind::EQUALITY;
        }

        auto predicateCorrelationHint(const ResolvedScanPredicate &predicate) -> double
        {
            switch (predicate.kind)
            {
                case ResolvedPredicateKind::EQUALITY:
                    return 0.9;
                case ResolvedPredicateKind::LIKE_PREFIX:
                    return 0.6;
                case ResolvedPredicateKind::RANGE:
                    return 0.35;
                default:
                    return 0.0;
            }
        }

        auto extractConstantInt64(const parser::v3::Expression *expr,
                                  int64_t &value_out) -> bool
        {
            if (expr == nullptr)
            {
                return false;
            }
            if (expr->kind() == parser::v3::ASTKind::LiteralExpr)
            {
                const auto *literal = static_cast<const parser::v3::LiteralExpr *>(expr);
                if (literal->literal_type == parser::v3::LiteralType::INTEGER)
                {
                    value_out = literal->int_value;
                    return true;
                }
            }
            return false;
        }
    } // namespace

    auto QueryPlanner::planQuery(const parser::v3::SelectStmt *select_stmt,
                                 core::ErrorContext *ctx,
                                 core::ConnectionContext *conn_ctx)
        -> std::shared_ptr<PlanNode>
    {
        conn_ctx_ = conn_ctx;
        if (select_stmt == nullptr)
        {
            return nullptr;
        }

        DEBUG_LOG_DB("QueryPlanner::planQuery requires string pool context; "
                     "use buildSelectPlan for V3 execution planning");
        return nullptr;
    }

    auto QueryPlanner::planAnalyze(const parser::v3::AnalyzeStmt *analyze_stmt,
                                   core::ErrorContext *ctx)
        -> std::shared_ptr<PlanNode>
    {
        (void)analyze_stmt;
        (void)ctx;
        return nullptr;
    }

    auto QueryPlanner::buildSelectPlan(const parser::v3::SelectStmt *select_stmt,
                                       const parser::v3::StringPool &pool,
                                       PlannedSelectQuery &planned_out,
                                       core::ErrorContext *ctx,
                                       core::ConnectionContext *conn_ctx,
                                       const core::ID &current_schema_id)
        -> core::Status
    {
        planned_out = PlannedSelectQuery{};
        conn_ctx_ = conn_ctx;
        if (select_stmt == nullptr)
        {
            return core::Status::INVALID_ARGUMENT;
        }

        V3SemanticAnalyzer analyzer(db_, stats_manager_);
        core::Status status = analyzer.resolveSelect(select_stmt,
                                                    pool,
                                                    planned_out.resolved_query,
                                                    conn_ctx,
                                                    current_schema_id,
                                                    ctx);
        if (status != core::Status::OK)
        {
            return status;
        }

        if (planned_out.resolved_query.relations.empty())
        {
            RuntimePlan plan;
            plan.version = 1;
            plan.explain_text = "Result";
            plan.plan_hash = stablePlanHash(plan.explain_text);
            plan.root.node_type = "Result";
            planned_out.runtime_plan = std::move(plan);
            return core::Status::OK;
        }

        std::vector<parser::v3::FunctionCallExpr *> query_aggregates;
        std::vector<parser::v3::FunctionCallExpr *> query_windows;

        auto appendUniqueColumnId = [](std::vector<core::ID> &ids, const core::ID &column_id) {
            if (isZeroId(column_id) ||
                std::find(ids.begin(), ids.end(), column_id) != ids.end())
            {
                return;
            }
            ids.push_back(column_id);
        };

        auto appendUniqueFunction =
            [](std::vector<parser::v3::FunctionCallExpr *> &functions,
               parser::v3::FunctionCallExpr *function) {
                if (function == nullptr ||
                    std::find(functions.begin(), functions.end(), function) != functions.end())
                {
                    return;
                }
                functions.push_back(function);
            };

        std::vector<BaseAccessChoice> access_choices(planned_out.resolved_query.relations.size());
        for (size_t relation_index = 0;
             relation_index < planned_out.resolved_query.relations.size();
             ++relation_index)
        {
            const auto &relation = planned_out.resolved_query.relations[relation_index];
            BaseAccessChoice choice;
            choice.relation_index = relation_index;

            std::vector<core::ID> required_column_ids;
            std::vector<parser::v3::FunctionCallExpr *> relation_aggregates;
            std::vector<parser::v3::FunctionCallExpr *> relation_windows;
            collectRequiredColumnIds(planned_out.resolved_query,
                                     relation_index,
                                     required_column_ids,
                                     relation_aggregates,
                                     relation_windows);
            if (relation_index == 0)
            {
                query_aggregates = relation_aggregates;
                query_windows = relation_windows;
            }

            for (const auto &join : planned_out.resolved_query.joins)
            {
                if (join.left_relation_index == relation_index)
                {
                    appendUniqueColumnId(required_column_ids, join.left_hash_column_id);
                }
                if (join.right_relation_index == relation_index)
                {
                    appendUniqueColumnId(required_column_ids, join.right_hash_column_id);
                }
            }

            std::vector<ResolvedScanPredicate> predicates = relation.local_predicates;
            if (predicates.empty() && relation.local_predicate.has_value())
            {
                predicates.push_back(*relation.local_predicate);
            }

            const auto buildPredicateText =
                [&](const std::optional<size_t> &preferred_index = std::nullopt) -> std::string {
                    if (predicates.empty())
                    {
                        return {};
                    }

                    std::ostringstream out;
                    const std::string joiner =
                        core::IdentifierUtils::toUpper(relation.predicate_combination) == "OR"
                            ? " OR "
                            : " AND ";
                    bool first = true;
                    auto emit_predicate = [&](size_t index) {
                        if (index >= predicates.size())
                        {
                            return;
                        }
                        if (!first)
                        {
                            out << joiner;
                        }
                        out << predicates[index].predicate_text;
                        first = false;
                    };

                    if (preferred_index.has_value())
                    {
                        emit_predicate(*preferred_index);
                    }
                    for (size_t index = 0; index < predicates.size(); ++index)
                    {
                        if (preferred_index.has_value() && *preferred_index == index)
                        {
                            continue;
                        }
                        emit_predicate(index);
                    }
                    return out.str();
                };

            auto makeRuntimePredicate = [](const ResolvedScanPredicate &predicate)
                -> RuntimePlanIndexPredicate {
                RuntimePlanIndexPredicate out;
                out.valid = true;
                out.index_name = predicate.matched_index.index_name;
                out.index_id_text = predicate.matched_index.index_id.toString();
                out.column_name = predicate.column_name;
                out.operator_name = predicate.operator_name;
                out.literal_kind = predicate.literal_kind;
                out.literal_text = predicate.literal_text;
                return out;
            };

            auto estimatePredicateSelectivity =
                [&](const ResolvedScanPredicate &predicate) -> double {
                    if (!relation.resolved ||
                        isZeroId(relation.table_info.table_id) ||
                        predicate.expression == nullptr)
                    {
                        return 1.0;
                    }

                    double estimated = selectivity_estimator_.estimateWhereClause(
                        predicate.expression,
                        relation.table_info.table_id,
                        &pool,
                        ctx);
                    if (estimated <= 0.0)
                    {
                        estimated = 0.01;
                    }
                    return std::max(0.0001, std::min(1.0, estimated));
                };

            const std::string relation_name = displayRelationName(relation);
            double qual_cost = 0.0;
            const bool predicate_or =
                core::IdentifierUtils::toUpper(relation.predicate_combination) == "OR";
            double selectivity = predicate_or ? 0.0 : 1.0;
            for (const auto &predicate : predicates)
            {
                qual_cost += cost_model_.operatorCost(predicate.operator_name);
                const double predicate_selectivity = estimatePredicateSelectivity(predicate);
                if (predicate_or)
                {
                    selectivity = 1.0 - ((1.0 - selectivity) * (1.0 - predicate_selectivity));
                }
                else
                {
                    selectivity *= predicate_selectivity;
                }
            }
            if (!predicates.empty())
            {
                selectivity = std::max(0.0001, std::min(1.0, selectivity));
            }

            const uint64_t base_rows =
                relation.estimated_rows == 0 ? 1000 : relation.estimated_rows;
            const uint64_t base_pages =
                std::max<uint64_t>(1, relation.estimated_pages == 0 ? 10 : relation.estimated_pages);
            const uint64_t seq_rows = relationOutputRows(base_rows, selectivity);
            CostEstimate best_cost =
                cost_model_.costSeqScan(base_pages, seq_rows, qual_cost, ctx);
            std::shared_ptr<Path> best_path;
            auto seq_path = std::make_shared<SeqScanPath>(relation.table_info.table_id,
                                                          relation_name,
                                                          base_pages,
                                                          seq_rows,
                                                          qual_cost,
                                                          best_cost);
            if (relation.local_predicate.has_value())
            {
                seq_path->setWhereExpr(relation.local_predicate->expression);
            }
            best_path = seq_path;

            std::shared_ptr<PlanNode> best_plan;
            auto seq_plan = std::make_shared<SeqScanNode>(relation.table_info.table_id,
                                                          relation_name);
            seq_plan->setQualCost(qual_cost);
            seq_plan->setFilter(buildPredicateText());
            seq_plan->setCost(best_cost.startup_cost, best_cost.total_cost, best_cost.rows);
            best_plan = seq_plan;

            std::string best_scan_kind = "SEQ_SCAN";
            RuntimePlanIndexPredicate best_runtime_predicate;
            std::vector<RuntimePlanIndexPredicate> best_runtime_predicates;
            std::string best_bitmap_op;
            bool best_covering_index = false;
            bool best_exact_key_lookup = false;
            core::CatalogManager::IndexInfo matched_index{};

            auto indexCoversColumns =
                [&](const core::CatalogManager::IndexInfo &index) -> bool {
                    for (const auto &column_id : required_column_ids)
                    {
                        const bool in_keys =
                            std::find(index.column_ids.begin(),
                                      index.column_ids.end(),
                                      column_id) != index.column_ids.end();
                        const bool in_include =
                            std::find(index.include_column_ids.begin(),
                                      index.include_column_ids.end(),
                                      column_id) != index.include_column_ids.end();
                        if (!in_keys && !in_include)
                        {
                            return false;
                        }
                    }
                    return true;
                };

            for (size_t predicate_index = 0; predicate_index < predicates.size(); ++predicate_index)
            {
                const auto &predicate = predicates[predicate_index];
                if (!predicate.has_index_match)
                {
                    continue;
                }

                const auto &index = predicate.matched_index;
                const double predicate_selectivity =
                    std::min(selectivity, estimatePredicateSelectivity(predicate));
                const uint64_t expected_rows =
                    relationOutputRows(base_rows, predicate_selectivity);
                const uint64_t index_pages =
                    std::max<uint64_t>(1, base_pages / 8);
                const uint64_t heap_pages =
                    std::max<uint64_t>(1,
                                       static_cast<uint64_t>(static_cast<double>(base_pages) *
                                                             predicate_selectivity));
                const double correlation = predicateCorrelationHint(predicate);
                const bool covering_index = indexCoversColumns(index);
                const bool exact_key_lookup =
                    index.index_type == core::CatalogManager::IndexType::BTREE &&
                    predicateSupportsExactLookup(predicate);

                CostEstimate candidate_cost{};
                std::string scan_kind = "INDEX_SCAN";
                if (index.index_type == core::CatalogManager::IndexType::LSM)
                {
                    candidate_cost = cost_model_.costLSMScan(3,
                                                             2,
                                                             expected_rows,
                                                             heap_pages,
                                                             expected_rows,
                                                             qual_cost,
                                                             correlation,
                                                             ctx);
                    scan_kind = "LSM_SCAN";
                }
                else if (covering_index)
                {
                    candidate_cost = cost_model_.costIndexOnlyScan(3,
                                                                   index_pages,
                                                                   expected_rows,
                                                                   qual_cost,
                                                                   correlation,
                                                                   ctx);
                    scan_kind = "INDEX_ONLY_SCAN";
                }
                else
                {
                    candidate_cost = cost_model_.costIndexScan(3,
                                                               index_pages,
                                                               expected_rows,
                                                               heap_pages,
                                                               expected_rows,
                                                               qual_cost,
                                                               correlation,
                                                               ctx);
                }

                if (candidate_cost.total_cost > best_cost.total_cost)
                {
                    continue;
                }

                matched_index = index;
                best_cost = candidate_cost;
                best_scan_kind = scan_kind;
                best_runtime_predicate = makeRuntimePredicate(predicate);
                best_runtime_predicates = {best_runtime_predicate};
                best_bitmap_op.clear();
                best_covering_index = covering_index;
                best_exact_key_lookup = exact_key_lookup;

                if (scan_kind == "INDEX_ONLY_SCAN")
                {
                    best_path = std::make_shared<IndexOnlyScanPath>(relation.table_info.table_id,
                                                                    relation_name,
                                                                    index.index_id,
                                                                    index.index_name,
                                                                    3,
                                                                    index_pages,
                                                                    expected_rows,
                                                                    qual_cost,
                                                                    correlation,
                                                                    candidate_cost);
                    auto index_plan =
                        std::make_shared<IndexOnlyScanNode>(relation.table_info.table_id,
                                                            relation_name,
                                                            index.index_id,
                                                            index.index_name);
                    index_plan->setIndexCond(predicate.predicate_text);
                    index_plan->setFilter(buildPredicateText(predicate_index));
                    index_plan->setCost(candidate_cost.startup_cost,
                                        candidate_cost.total_cost,
                                        candidate_cost.rows);
                    best_plan = index_plan;
                }
                else
                {
                    best_path = std::make_shared<IndexScanPath>(relation.table_info.table_id,
                                                                relation_name,
                                                                index.index_id,
                                                                index.index_name,
                                                                3,
                                                                index_pages,
                                                                expected_rows,
                                                                heap_pages,
                                                                expected_rows,
                                                                qual_cost,
                                                                correlation,
                                                                candidate_cost);
                    auto index_plan = std::make_shared<IndexScanNode>(relation.table_info.table_id,
                                                                      relation_name,
                                                                      index.index_id,
                                                                      index.index_name);
                    index_plan->setIndexQualCost(qual_cost);
                    index_plan->setHeapQualCost(qual_cost);
                    index_plan->setCorrelation(correlation);
                    index_plan->setIndexCond(predicate.predicate_text);
                    index_plan->setFilter(buildPredicateText(predicate_index));
                    index_plan->setCost(candidate_cost.startup_cost,
                                        candidate_cost.total_cost,
                                        candidate_cost.rows);
                    best_plan = index_plan;
                }
            }

            std::vector<RuntimePlanIndexPredicate> bitmap_predicates;
            std::vector<core::ID> bitmap_index_ids;
            std::vector<std::string> bitmap_index_names;
            std::vector<std::string> bitmap_predicate_texts;
            bool bitmap_exact_lookup = !predicates.empty();
            size_t bitmap_exact_predicate_count = 0;
            uint64_t bitmap_total_index_pages = 0;
            for (const auto &predicate : predicates)
            {
                if (!predicate.has_index_match ||
                    predicate.matched_index.index_type != core::CatalogManager::IndexType::BTREE)
                {
                    bitmap_exact_lookup = false;
                    continue;
                }
                if (std::find(bitmap_index_ids.begin(),
                              bitmap_index_ids.end(),
                              predicate.matched_index.index_id) != bitmap_index_ids.end())
                {
                    continue;
                }
                bitmap_index_ids.push_back(predicate.matched_index.index_id);
                bitmap_index_names.push_back(predicate.matched_index.index_name);
                bitmap_predicates.push_back(makeRuntimePredicate(predicate));
                bitmap_predicate_texts.push_back(predicate.predicate_text);
                bitmap_total_index_pages += std::max<uint64_t>(1, base_pages / 8);
                const bool predicate_exact_lookup = predicateSupportsExactLookup(predicate);
                bitmap_exact_lookup = bitmap_exact_lookup && predicate_exact_lookup;
                if (predicate_exact_lookup)
                {
                    ++bitmap_exact_predicate_count;
                }
            }

            if (bitmap_index_ids.size() > 1)
            {
                const uint64_t bitmap_rows =
                    relationOutputRows(base_rows, selectivity);
                const uint64_t bitmap_heap_pages =
                    std::max<uint64_t>(1,
                                       static_cast<uint64_t>(static_cast<double>(base_pages) *
                                                             selectivity));
                CostEstimate bitmap_cost =
                    cost_model_.costBitmapScan(bitmap_index_ids.size(),
                                               std::max<uint64_t>(1, bitmap_total_index_pages),
                                               bitmap_heap_pages,
                                               bitmap_rows,
                                               qual_cost,
                                               predicate_or ? "OR" : "AND",
                                               ctx);
                const bool prefer_exact_bitmap =
                    !predicate_or &&
                    bitmap_exact_lookup &&
                    bitmap_exact_predicate_count == bitmap_predicates.size();
                if (bitmap_cost.total_cost <= best_cost.total_cost || prefer_exact_bitmap)
                {
                    best_cost = bitmap_cost;
                    best_scan_kind = "BITMAP_INDEX_SCAN";
                    best_runtime_predicates = bitmap_predicates;
                    best_runtime_predicate =
                        bitmap_predicates.empty() ? RuntimePlanIndexPredicate{}
                                                  : bitmap_predicates.front();
                    best_bitmap_op = predicate_or ? "OR" : "AND";
                    best_covering_index = false;
                    best_exact_key_lookup = bitmap_exact_lookup;
                    matched_index = {};

                    best_path = std::make_shared<BitmapIndexScanPath>(
                        relation.table_info.table_id,
                        relation_name,
                        bitmap_index_ids,
                        bitmap_index_names,
                        best_bitmap_op,
                        std::max<uint64_t>(1, bitmap_total_index_pages),
                        bitmap_heap_pages,
                        bitmap_rows,
                        qual_cost,
                        bitmap_cost);

                    auto bitmap_plan = std::make_shared<BitmapIndexScanNode>(
                        relation.table_info.table_id,
                        relation_name,
                        bitmap_index_ids,
                        bitmap_index_names,
                        best_bitmap_op);
                    bitmap_plan->setIndexConds(bitmap_predicate_texts);
                    bitmap_plan->setFilter(buildPredicateText());
                    bitmap_plan->setCost(bitmap_cost.startup_cost,
                                         bitmap_cost.total_cost,
                                         bitmap_cost.rows);
                    best_plan = bitmap_plan;
                }
            }

            choice.path = best_path;
            choice.plan = best_plan;
            choice.selectivity = selectivity;
            choice.runtime_relation.source_relation_index = relation_index;
            choice.runtime_relation.table_path = relation.table_path;
            choice.runtime_relation.physical_table_path = relation.physical_table_path;
            choice.runtime_relation.alias = relation.alias;
            choice.runtime_relation.table_id_text =
                relation.resolved ? relation.table_info.table_id.toString() : std::string();
            choice.runtime_relation.scan_kind = best_scan_kind;
            choice.runtime_relation.bitmap_op = best_bitmap_op;
            choice.runtime_relation.covering_index = best_covering_index;
            choice.runtime_relation.exact_key_lookup = best_exact_key_lookup;
            choice.runtime_relation.flattened_derived = relation.flattened_derived;
            choice.runtime_relation.startup_cost = best_cost.startup_cost;
            choice.runtime_relation.total_cost = best_cost.total_cost;
            choice.runtime_relation.estimated_rows = best_cost.rows;
            if (!best_runtime_predicates.empty())
            {
                choice.runtime_relation.index_predicates = best_runtime_predicates;
                choice.runtime_relation.index_predicate = best_runtime_predicates.front();
            }
            if (!best_runtime_predicate.index_name.empty())
            {
                choice.runtime_relation.index_name = best_runtime_predicate.index_name;
                choice.runtime_relation.index_id_text = best_runtime_predicate.index_id_text;
            }
            else if (!isZeroId(matched_index.index_id))
            {
                choice.runtime_relation.index_name = matched_index.index_name;
                choice.runtime_relation.index_id_text = matched_index.index_id.toString();
            }
            access_choices[relation_index] = std::move(choice);
        }

        auto estimateJoinSelectivityFor =
            [&](const ResolvedJoin &join) -> double {
                if (!join.equi_join ||
                    !join.has_hash_column_ids ||
                    join.left_relation_index >= planned_out.resolved_query.relations.size() ||
                    join.right_relation_index >= planned_out.resolved_query.relations.size())
                {
                    const auto &left_relation =
                        planned_out.resolved_query.relations[join.left_relation_index];
                    const auto &right_relation =
                        planned_out.resolved_query.relations[join.right_relation_index];
                    return selectivity_estimator_.estimateJoinSelectivity(
                        join.condition,
                        left_relation.table_info.table_id,
                        right_relation.table_info.table_id,
                        &pool,
                        ctx);
                }

                const auto &left_relation =
                    planned_out.resolved_query.relations[join.left_relation_index];
                const auto &right_relation =
                    planned_out.resolved_query.relations[join.right_relation_index];
                return selectivity_estimator_.estimateEquiJoinSelectivity(
                    join.left_hash_column_id,
                    join.right_hash_column_id,
                    left_relation.table_info.table_id,
                    right_relation.table_info.table_id,
                    ctx);
            };

        auto buildJoinDecision =
            [&](const std::shared_ptr<Path> &current_path,
                const std::shared_ptr<PlanNode> &current_plan,
                uint64_t current_rows,
                size_t candidate_relation_index,
                const ResolvedJoin &join,
                bool candidate_is_right_relation) -> JoinDecision {
                JoinDecision decision;
                decision.valid = true;
                decision.join_index = join.source_join_index;
                decision.candidate_relation_index = candidate_relation_index;
                decision.selectivity = estimateJoinSelectivityFor(join);
                if (decision.selectivity <= 0.0)
                {
                    decision.selectivity = 0.01;
                }

                const auto &candidate_access = access_choices[candidate_relation_index];
                const auto &candidate_relation =
                    planned_out.resolved_query.relations[candidate_relation_index];
                const auto join_type = join.join_type;
                CostEstimate nl_cost = cost_model_.costNestedLoopJoin(
                    current_path ? current_path->cost() : CostEstimate{},
                    candidate_access.path ? candidate_access.path->cost() : CostEstimate{},
                    current_rows,
                    candidate_access.path ? candidate_access.path->rows() : 0,
                    decision.selectivity,
                    join_type,
                    ctx);

                bool allow_hash =
                    (join_type == parser::JoinType::INNER || join_type == parser::JoinType::LEFT) &&
                    join.equi_join;
                CostEstimate hash_cost{};
                if (allow_hash)
                {
                    hash_cost = cost_model_.costHashJoin(
                        current_path ? current_path->cost() : CostEstimate{},
                        candidate_access.path ? candidate_access.path->cost() : CostEstimate{},
                        current_rows,
                        candidate_access.path ? candidate_access.path->rows() : 0,
                        decision.selectivity,
                        join_type,
                        ctx);
                }
                else
                {
                    hash_cost.total_cost = std::numeric_limits<double>::max();
                }

                bool use_hash = allow_hash && hash_cost.total_cost < nl_cost.total_cost;
                decision.total_cost = use_hash ? hash_cost.total_cost : nl_cost.total_cost;
                decision.rows = use_hash ? hash_cost.rows : nl_cost.rows;
                decision.runtime_join.source_join_index = join.source_join_index;
                decision.runtime_join.right_relation_index = candidate_relation_index;
                decision.runtime_join.join_type = planJoinTypeToString(join_type);
                decision.runtime_join.natural = join.natural;
                decision.runtime_join.using_columns = join.using_columns;
                decision.runtime_join.condition_text = join.condition_text;
                decision.runtime_join.startup_cost =
                    use_hash ? hash_cost.startup_cost : nl_cost.startup_cost;
                decision.runtime_join.total_cost =
                    use_hash ? hash_cost.total_cost : nl_cost.total_cost;
                decision.runtime_join.estimated_rows =
                    use_hash ? hash_cost.rows : nl_cost.rows;
                decision.runtime_join.method = use_hash ? "HASH_JOIN" : "NESTED_LOOP";

                if (join.equi_join)
                {
                    decision.runtime_join.has_hash_keys = true;
                    if (candidate_is_right_relation)
                    {
                        decision.runtime_join.left_hash_key.qualifier =
                            join.left_hash_qualifier;
                        decision.runtime_join.left_hash_key.column_name =
                            join.left_hash_column;
                        decision.runtime_join.right_hash_key.qualifier =
                            join.right_hash_qualifier;
                        decision.runtime_join.right_hash_key.column_name =
                            join.right_hash_column;
                    }
                    else
                    {
                        decision.runtime_join.left_hash_key.qualifier =
                            join.right_hash_qualifier;
                        decision.runtime_join.left_hash_key.column_name =
                            join.right_hash_column;
                        decision.runtime_join.right_hash_key.qualifier =
                            join.left_hash_qualifier;
                        decision.runtime_join.right_hash_key.column_name =
                            join.left_hash_column;
                    }
                }

                if (use_hash)
                {
                    std::vector<parser::v3::Expression *> hash_keys_outer;
                    std::vector<parser::v3::Expression *> hash_keys_inner;
                    auto hash_plan = std::make_shared<HashJoinNode>(join_type,
                                                                   current_plan,
                                                                   candidate_access.plan,
                                                                   const_cast<parser::v3::Expression *>(
                                                                       join.condition),
                                                                   hash_keys_outer,
                                                                   hash_keys_inner);
                    hash_plan->setJoinCondString(join.condition_text);
                    hash_plan->setCost(hash_cost.startup_cost,
                                       hash_cost.total_cost,
                                       hash_cost.rows);
                    decision.plan = hash_plan;
                    decision.path = std::make_shared<HashJoinPath>(
                        join_type,
                        current_path,
                        candidate_access.path,
                        const_cast<parser::v3::Expression *>(join.condition),
                        hash_keys_outer,
                        hash_keys_inner,
                        decision.selectivity,
                        hash_cost);
                }
                else
                {
                    auto nested_plan = std::make_shared<NestedLoopJoinNode>(
                        join_type,
                        current_plan,
                        candidate_access.plan,
                        const_cast<parser::v3::Expression *>(join.condition));
                    nested_plan->setJoinCondString(join.condition_text);
                    nested_plan->setCost(nl_cost.startup_cost,
                                         nl_cost.total_cost,
                                         nl_cost.rows);
                    decision.plan = nested_plan;
                    decision.path = std::make_shared<NestedLoopJoinPath>(
                        join_type,
                        current_path,
                        candidate_access.path,
                        const_cast<parser::v3::Expression *>(join.condition),
                        decision.selectivity,
                        nl_cost);
                }

                if (!decision.runtime_join.has_hash_keys)
                {
                    decision.runtime_join.left_hash_key.qualifier = relationLookupName(
                        planned_out.resolved_query.relations[join.left_relation_index]);
                    decision.runtime_join.right_hash_key.qualifier = relationLookupName(
                        candidate_relation);
                }

                return decision;
            };

        std::vector<size_t> relation_order;
        std::vector<RuntimePlanRelation> runtime_relations;
        std::vector<RuntimePlanJoinStep> runtime_joins;
        std::shared_ptr<Path> current_path;
        std::shared_ptr<PlanNode> current_plan;
        uint64_t current_rows = 0;

        if (planned_out.resolved_query.joins.empty())
        {
            relation_order.push_back(0);
            runtime_relations.push_back(access_choices[0].runtime_relation);
            current_path = access_choices[0].path;
            current_plan = access_choices[0].plan;
            current_rows = current_path ? current_path->rows() : 0;
        }
        else if (isJoinReorderSafe(planned_out.resolved_query))
        {
            planned_out.reordered_relations = true;
            std::vector<bool> joined(planned_out.resolved_query.relations.size(), false);
            size_t start_relation = 0;
            double best_start_cost = std::numeric_limits<double>::max();
            for (size_t relation_index = 0;
                 relation_index < access_choices.size();
                 ++relation_index)
            {
                if (!access_choices[relation_index].path)
                {
                    continue;
                }
                if (access_choices[relation_index].path->totalCost() < best_start_cost)
                {
                    best_start_cost = access_choices[relation_index].path->totalCost();
                    start_relation = relation_index;
                }
            }

            joined[start_relation] = true;
            relation_order.push_back(start_relation);
            runtime_relations.push_back(access_choices[start_relation].runtime_relation);
            current_path = access_choices[start_relation].path;
            current_plan = access_choices[start_relation].plan;
            current_rows = current_path ? current_path->rows() : 0;

            while (relation_order.size() < planned_out.resolved_query.relations.size())
            {
                JoinDecision best_join;
                for (const auto &join : planned_out.resolved_query.joins)
                {
                    const bool left_joined = joined[join.left_relation_index];
                    const bool right_joined = joined[join.right_relation_index];
                    if (left_joined == right_joined)
                    {
                        continue;
                    }

                    const bool candidate_is_right = !right_joined;
                    const size_t candidate_relation_index =
                        candidate_is_right ? join.right_relation_index : join.left_relation_index;
                    auto decision = buildJoinDecision(current_path,
                                                      current_plan,
                                                      current_rows,
                                                      candidate_relation_index,
                                                      join,
                                                      candidate_is_right);
                    if (!best_join.valid || decision.total_cost < best_join.total_cost)
                    {
                        best_join = std::move(decision);
                    }
                }

                if (!best_join.valid)
                {
                    size_t candidate = 0;
                    for (; candidate < joined.size(); ++candidate)
                    {
                        if (!joined[candidate])
                        {
                            break;
                        }
                    }
                    if (candidate >= joined.size())
                    {
                        break;
                    }

                    ResolvedJoin cross_join;
                    cross_join.source_join_index = std::numeric_limits<size_t>::max();
                    cross_join.left_relation_index = relation_order.back();
                    cross_join.right_relation_index = candidate;
                    cross_join.join_type = parser::JoinType::CROSS;
                    cross_join.condition = nullptr;
                    cross_join.condition_text.clear();
                    auto decision = buildJoinDecision(current_path,
                                                      current_plan,
                                                      current_rows,
                                                      candidate,
                                                      cross_join,
                                                      true);
                    best_join = std::move(decision);
                }

                joined[best_join.candidate_relation_index] = true;
                relation_order.push_back(best_join.candidate_relation_index);
                runtime_relations.push_back(
                    access_choices[best_join.candidate_relation_index].runtime_relation);
                runtime_joins.push_back(best_join.runtime_join);
                current_path = best_join.path;
                current_plan = best_join.plan;
                current_rows = best_join.rows;
            }
        }
        else
        {
            relation_order.push_back(0);
            runtime_relations.push_back(access_choices[0].runtime_relation);
            current_path = access_choices[0].path;
            current_plan = access_choices[0].plan;
            current_rows = current_path ? current_path->rows() : 0;

            for (const auto &join : planned_out.resolved_query.joins)
            {
                const size_t candidate_relation_index = join.right_relation_index;
                auto decision = buildJoinDecision(current_path,
                                                  current_plan,
                                                  current_rows,
                                                  candidate_relation_index,
                                                  join,
                                                  true);
                if (!decision.valid)
                {
                    return core::Status::INTERNAL_ERROR;
                }
                if (join.join_type == parser::JoinType::RIGHT ||
                    join.join_type == parser::JoinType::FULL ||
                    join.natural ||
                    !join.using_columns.empty())
                {
                    decision.runtime_join.method = "NESTED_LOOP";
                    decision.runtime_join.has_hash_keys = false;
                }
                relation_order.push_back(candidate_relation_index);
                runtime_relations.push_back(access_choices[candidate_relation_index].runtime_relation);
                runtime_joins.push_back(decision.runtime_join);
                current_path = decision.path;
                current_plan = decision.plan;
                current_rows = decision.rows;
            }
        }

        auto estimateRowWidth = [&](const std::vector<size_t> &relations) -> uint64_t {
            uint64_t width = 0;
            if (relations.empty())
            {
                return 64;
            }
            for (size_t relation_index : relations)
            {
                if (relation_index >= planned_out.resolved_query.relations.size())
                {
                    continue;
                }
                width += static_cast<uint64_t>(
                    std::max<size_t>(1, planned_out.resolved_query.relations[relation_index].columns.size()) * 16);
            }
            return std::max<uint64_t>(64, width);
        };

        const uint64_t row_width = estimateRowWidth(relation_order);

        if (!query_aggregates.empty() ||
            !select_stmt->group_by.empty() ||
            select_stmt->having != nullptr)
        {
            uint64_t estimated_groups = select_stmt->group_by.empty()
                ? 1
                : std::max<uint64_t>(1, current_rows / 10);
            CostEstimate aggregate_cost = cost_model_.costAggregate(current_rows,
                                                                    estimated_groups,
                                                                    query_aggregates.size(),
                                                                    ctx);
            current_path = std::make_shared<AggregatePath>(current_path,
                                                           select_stmt->group_by,
                                                           query_aggregates,
                                                           select_stmt->having,
                                                           estimated_groups,
                                                           aggregate_cost,
                                                           select_stmt->grouping_type,
                                                           select_stmt->grouping_sets);
            auto aggregate_plan = std::make_shared<AggregateNode>(current_plan,
                                                                  select_stmt->group_by,
                                                                  query_aggregates,
                                                                  select_stmt->having,
                                                                  select_stmt->grouping_type,
                                                                  select_stmt->grouping_sets);
            aggregate_plan->setCost(aggregate_cost.startup_cost,
                                    aggregate_cost.total_cost,
                                    aggregate_cost.rows);
            current_plan = aggregate_plan;
            current_rows = aggregate_cost.rows;
        }

        if (!query_windows.empty())
        {
            uint64_t partition_keys = 0;
            uint64_t order_keys = 0;
            std::vector<WindowNode::WindowFunction> window_functions;
            window_functions.reserve(query_windows.size());
            for (auto *func : query_windows)
            {
                if (func == nullptr)
                {
                    continue;
                }
                const auto *window_spec = func->window;
                if (window_spec != nullptr)
                {
                    partition_keys += window_spec->partition_by.size();
                    order_keys += window_spec->order_by.size();
                }
                parser::WindowFunc window_type = parser::WindowFunc::ROW_NUMBER;
                if (!func->function_path.components.empty())
                {
                    const std::string upper_name =
                        core::IdentifierUtils::toUpper(
                            std::string(pool.get(func->function_path.components.back())));
                    if (upper_name == "RANK")
                    {
                        window_type = parser::WindowFunc::RANK;
                    }
                    else if (upper_name == "DENSE_RANK")
                    {
                        window_type = parser::WindowFunc::DENSE_RANK;
                    }
                    else if (upper_name == "LAG")
                    {
                        window_type = parser::WindowFunc::LAG;
                    }
                    else if (upper_name == "LEAD")
                    {
                        window_type = parser::WindowFunc::LEAD;
                    }
                    else if (upper_name == "FIRST_VALUE")
                    {
                        window_type = parser::WindowFunc::FIRST_VALUE;
                    }
                    else if (upper_name == "LAST_VALUE")
                    {
                        window_type = parser::WindowFunc::LAST_VALUE;
                    }
                    else if (upper_name == "NTILE")
                    {
                        window_type = parser::WindowFunc::NTILE;
                    }
                    else if (upper_name == "PERCENT_RANK")
                    {
                        window_type = parser::WindowFunc::PERCENT_RANK;
                    }
                    else if (upper_name == "CUME_DIST")
                    {
                        window_type = parser::WindowFunc::CUME_DIST;
                    }
                }
                std::string output_column =
                    func->function_path.components.empty()
                        ? std::string("window")
                        : std::string(pool.get(func->function_path.components.back()));
                window_functions.emplace_back(window_type,
                                              func->arguments,
                                              func->window,
                                              output_column);
            }

            CostEstimate window_cost = cost_model_.costWindow(current_rows,
                                                              row_width,
                                                              partition_keys,
                                                              order_keys,
                                                              window_functions.size(),
                                                              ctx);
            current_path = std::make_shared<WindowPath>(current_path,
                                                        query_windows,
                                                        window_cost);
            auto window_plan = std::make_shared<WindowNode>(current_plan, window_functions);
            window_plan->setCost(window_cost.startup_cost,
                                 window_cost.total_cost,
                                 window_cost.rows);
            current_plan = window_plan;
            current_rows = window_cost.rows;
        }

        if (!select_stmt->order_by.empty())
        {
            CostEstimate sort_cost = cost_model_.costSort(current_rows,
                                                          row_width,
                                                          select_stmt->order_by.size(),
                                                          ctx);
            current_path = std::make_shared<SortPath>(current_path,
                                                      select_stmt->order_by,
                                                      row_width,
                                                      sort_cost);
            auto sort_plan = std::make_shared<SortNode>(current_plan, select_stmt->order_by);
            sort_plan->setCost(sort_cost.startup_cost,
                               sort_cost.total_cost,
                               sort_cost.rows);
            current_plan = sort_plan;
            current_rows = sort_cost.rows;
        }

        int64_t limit_count = -1;
        int64_t offset_count = -1;
        bool has_limit = false;
        bool has_offset = false;
        if (select_stmt->limit != nullptr)
        {
            has_limit = extractConstantInt64(select_stmt->limit, limit_count);
        }
        else if (select_stmt->fetch_row_count != nullptr)
        {
            has_limit = extractConstantInt64(select_stmt->fetch_row_count, limit_count);
        }
        if (select_stmt->offset != nullptr)
        {
            has_offset = extractConstantInt64(select_stmt->offset, offset_count);
        }

        if (has_limit || has_offset)
        {
            CostEstimate limit_cost = cost_model_.costLimit(current_rows,
                                                            has_limit ? limit_count : -1,
                                                            has_offset ? offset_count : -1,
                                                            ctx);
            current_path = std::make_shared<LimitPath>(current_path,
                                                       has_limit ? limit_count : -1,
                                                       has_offset ? offset_count : -1,
                                                       limit_cost);
            auto limit_plan = std::make_shared<LimitNode>(current_plan,
                                                          has_limit ? limit_count : -1,
                                                          has_offset ? offset_count : -1);
            limit_plan->setCost(limit_cost.startup_cost,
                                limit_cost.total_cost,
                                limit_cost.rows);
            current_plan = limit_plan;
            current_rows = limit_cost.rows;
        }

        planned_out.root_plan = current_plan;
        planned_out.runtime_plan.version = 1;
        planned_out.runtime_plan.relations = std::move(runtime_relations);
        planned_out.runtime_plan.join_steps = std::move(runtime_joins);
        planned_out.runtime_plan.root = toRuntimePlanNode(current_plan);
        planned_out.runtime_plan.explain_text =
            current_plan ? current_plan->toString() : std::string("Result");

        std::ostringstream hash_seed;
        hash_seed << planned_out.runtime_plan.explain_text << '|';
        for (const auto &relation : planned_out.runtime_plan.relations)
        {
            hash_seed << relation.source_relation_index << ':'
                      << relation.scan_kind << ':'
                      << relation.index_name << ':'
                      << relation.bitmap_op << ':'
                      << relation.total_cost << '|';
            for (const auto &predicate : relation.index_predicates)
            {
                hash_seed << predicate.index_id_text << ':'
                          << predicate.column_name << ':'
                          << predicate.operator_name << ':'
                          << predicate.literal_text << '|';
            }
        }
        for (const auto &join : planned_out.runtime_plan.join_steps)
        {
            hash_seed << join.source_join_index << ':'
                      << join.method << ':'
                      << join.total_cost << '|';
        }
        planned_out.runtime_plan.plan_hash = stablePlanHash(hash_seed.str());
        return core::Status::OK;
    }

} // namespace scratchbird::optimizer
