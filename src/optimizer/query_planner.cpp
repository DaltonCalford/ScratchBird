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
 * - Delegates multi-relation join search to the canonical join backend
 * - Materializes deterministic join methods on the backend-selected join tree
 * - Preserves outer/natural join order and bridges disconnected graphs explicitly
 */

#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/optimizer/index_family_lowering.h"
#include "scratchbird/optimizer/join_ordering.h"
#include "scratchbird/executor/parallel_executor.h"

#include "scratchbird/core/debug.h"
#include "scratchbird/core/observability_contract.h"
#include <array>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <nlohmann/json.hpp>
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
            AccessPathDescriptor access_descriptor;
            std::vector<std::string> candidate_scan_families;
            double selectivity = 1.0;
        };

        struct BaseRelationCandidateBundle
        {
            size_t relation_index = 0;
            std::vector<BaseAccessChoice> candidates;
            std::vector<std::string> candidate_scan_families;
            std::vector<std::string> candidate_family_identity_signatures;
            std::vector<std::string> candidate_family_statistics_signatures;
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
            bool runtime_filter_enabled = false;
            std::string runtime_filter_column;
            std::string runtime_filter_index_name;
            std::string runtime_filter_index_id_text;
        };

        struct MgaRelationCostSignal
        {
            bool available = false;
            double cleanup_debt_bytes = 0.0;
            double retained_dead_bytes = 0.0;
            double index_backlog_entries = 0.0;
            double same_page_update_ratio = 1.0;
            std::unordered_map<std::string, double> chain_depth_buckets;
            std::unordered_map<std::string, double> chain_scatter_buckets;
        };

        struct MgaCostingSnapshot
        {
            bool available = false;
            std::string contract_id;
            uint32_t metric_schema_version = 0;
            double commit_fence_backlog = 0.0;
            std::unordered_map<std::string, MgaRelationCostSignal> relations;
        };

        struct MgaCostPressure
        {
            bool available = false;
            double cleanup_ratio = 0.0;
            double retained_ratio = 0.0;
            double index_backlog_ratio = 0.0;
            double same_page_penalty = 0.0;
            double chain_scatter_penalty = 0.0;
            double chain_depth_penalty = 0.0;
            double fence_penalty = 0.0;
            std::string dominant_chain_scatter_bucket;
            std::string dominant_chain_depth_bucket;
            bool rewrite_equivalent = false;
        };

        struct MgaAccessGovernanceDecision
        {
            bool reject_candidate = false;
            std::string reason;
        };

        struct PlannerPartitionBound
        {
            enum class Kind : uint8_t
            {
                NONE = 0,
                LIST = 1,
                RANGE = 2,
                DEFAULT = 3
            };

            Kind kind = Kind::NONE;
            std::vector<std::vector<std::string>> list_values;
            std::vector<std::string> range_start;
            std::vector<std::string> range_end;
            bool has_start = false;
            bool has_end = false;
        };

        struct PartitionPruningResult
        {
            bool available = false;
            bool pruned = false;
            bool runtime_pruning_eligible = false;
            std::string strategy;
            std::string key_column;
            std::vector<std::string> key_columns;
            std::vector<std::string> targets;
            std::vector<std::string> pruned_targets_at_plan;
            std::vector<std::string> runtime_pruning_sources;
            std::vector<RuntimePlanIndexPredicate> predicates;
            uint64_t rows = 0;
            uint64_t pages = 0;
        };

        struct PredicateConstraint
        {
            bool valid = false;
            bool has_lower = false;
            std::string lower;
            bool lower_inclusive = true;
            bool has_upper = false;
            std::string upper;
            bool upper_inclusive = true;
        };

        enum class PartitionConstraintValueKind : uint8_t
        {
            NEGATIVE_INFINITY = 0,
            FINITE = 1,
            POSITIVE_INFINITY = 2
        };

        struct PartitionConstraintValue
        {
            PartitionConstraintValueKind kind =
                PartitionConstraintValueKind::FINITE;
            std::string value;
        };

        auto renderSchemaPath(const parser::v3::SchemaPath &path,
                              const parser::v3::StringPool &pool) -> std::string
        {
            if (path.components.empty())
            {
                return {};
            }

            std::ostringstream out;
            for (size_t i = 0; i < path.components.size(); ++i)
            {
                if (i > 0)
                {
                    out << '.';
                }
                out << pool.get(path.components[i]);
            }
            return out.str();
        }

        auto expressionToString(const parser::v3::Expression *expr,
                                const parser::v3::StringPool &pool) -> std::string
        {
            if (expr == nullptr)
            {
                return {};
            }

            switch (expr->kind())
            {
                case parser::v3::ASTKind::LiteralExpr: {
                    const auto *literal = static_cast<const parser::v3::LiteralExpr *>(expr);
                    switch (literal->literal_type)
                    {
                        case parser::v3::LiteralType::INTEGER:
                            return std::to_string(literal->int_value);
                        case parser::v3::LiteralType::FLOAT: {
                            std::ostringstream out;
                            out << literal->float_value;
                            return out.str();
                        }
                        case parser::v3::LiteralType::STRING:
                            return "'" + std::string(pool.get(literal->string_value)) + "'";
                        case parser::v3::LiteralType::BOOLEAN:
                            return literal->bool_value ? "TRUE" : "FALSE";
                        case parser::v3::LiteralType::NULL_VALUE:
                            return "NULL";
                        default:
                            return "LITERAL";
                    }
                }
                case parser::v3::ASTKind::ColumnRefExpr: {
                    const auto *column_ref =
                        static_cast<const parser::v3::ColumnRefExpr *>(expr);
                    std::ostringstream out;
                    if (column_ref->column.has_table_qualifier &&
                        !column_ref->column.table_path.components.empty())
                    {
                        out << renderSchemaPath(column_ref->column.table_path, pool)
                            << '.';
                    }
                    out << pool.get(column_ref->column.column_name);
                    return out.str();
                }
                case parser::v3::ASTKind::BinaryExpr: {
                    const auto *binary = static_cast<const parser::v3::BinaryExpr *>(expr);
                    const char *op_name = "?";
                    switch (binary->op)
                    {
                        case parser::v3::BinaryOp::EQ: op_name = "="; break;
                        case parser::v3::BinaryOp::NE: op_name = "!="; break;
                        case parser::v3::BinaryOp::LT: op_name = "<"; break;
                        case parser::v3::BinaryOp::LE: op_name = "<="; break;
                        case parser::v3::BinaryOp::GT: op_name = ">"; break;
                        case parser::v3::BinaryOp::GE: op_name = ">="; break;
                        case parser::v3::BinaryOp::AND: op_name = "AND"; break;
                        case parser::v3::BinaryOp::OR: op_name = "OR"; break;
                        case parser::v3::BinaryOp::ADD: op_name = "+"; break;
                        case parser::v3::BinaryOp::SUB: op_name = "-"; break;
                        case parser::v3::BinaryOp::MUL: op_name = "*"; break;
                        case parser::v3::BinaryOp::DIV: op_name = "/"; break;
                        case parser::v3::BinaryOp::CONCAT: op_name = "||"; break;
                        default: break;
                    }
                    return "(" + expressionToString(binary->left, pool) + " " +
                           std::string(op_name) + " " +
                           expressionToString(binary->right, pool) + ")";
                }
                case parser::v3::ASTKind::LikeExpr: {
                    const auto *like_expr = static_cast<const parser::v3::LikeExpr *>(expr);
                    return expressionToString(like_expr->expr, pool) + " LIKE " +
                           expressionToString(like_expr->pattern, pool);
                }
                case parser::v3::ASTKind::BetweenExpr: {
                    const auto *between_expr =
                        static_cast<const parser::v3::BetweenExpr *>(expr);
                    return expressionToString(between_expr->expr, pool) + " BETWEEN " +
                           expressionToString(between_expr->low, pool) + " AND " +
                           expressionToString(between_expr->high, pool);
                }
                case parser::v3::ASTKind::UnaryExpr: {
                    const auto *unary = static_cast<const parser::v3::UnaryExpr *>(expr);
                    switch (unary->op)
                    {
                        case parser::v3::UnaryOp::NOT:
                            return "NOT " + expressionToString(unary->operand, pool);
                        case parser::v3::UnaryOp::NEGATE:
                            return "-" + expressionToString(unary->operand, pool);
                        case parser::v3::UnaryOp::IS_NULL:
                            return expressionToString(unary->operand, pool) + " IS NULL";
                        case parser::v3::UnaryOp::IS_NOT_NULL:
                            return expressionToString(unary->operand, pool) + " IS NOT NULL";
                        default:
                            return "UNARY";
                    }
                }
                case parser::v3::ASTKind::FunctionCallExpr: {
                    const auto *func =
                        static_cast<const parser::v3::FunctionCallExpr *>(expr);
                    std::ostringstream out;
                    out << renderSchemaPath(func->function_path, pool) << '(';
                    for (size_t i = 0; i < func->arguments.size(); ++i)
                    {
                        if (i > 0)
                        {
                            out << ", ";
                        }
                        out << expressionToString(func->arguments[i], pool);
                    }
                    out << ')';
                    return out.str();
                }
                default:
                    return parser::v3::astKindToString(expr->kind());
            }
        }

        auto normalizeOrderingExpression(const std::string &text) -> std::string
        {
            std::string normalized;
            normalized.reserve(text.size());
            for (unsigned char ch : text)
            {
                if (std::isspace(ch) == 0)
                {
                    normalized.push_back(
                        static_cast<char>(std::toupper(ch)));
                }
            }
            return normalized;
        }

        auto makeOrderingKey(const std::string &expression_text,
                             bool descending = false,
                             bool nulls_first = false,
                             std::string comparator_family = "DEFAULT")
            -> AccessPathDescriptor::OrderingKey
        {
            AccessPathDescriptor::OrderingKey key;
            key.expression_text = expression_text;
            key.descending = descending;
            key.nulls_first = nulls_first;
            key.comparator_family = std::move(comparator_family);
            return key;
        }

        auto predicateMatchShape(ResolvedPredicateKind kind)
            -> PredicateMatchShape
        {
            switch (kind)
            {
                case ResolvedPredicateKind::EQUALITY:
                    return PredicateMatchShape::EQUALITY;
                case ResolvedPredicateKind::RANGE:
                    return PredicateMatchShape::RANGE;
                case ResolvedPredicateKind::LIKE_PREFIX:
                    return PredicateMatchShape::LIKE_PREFIX;
                case ResolvedPredicateKind::NONE:
                default:
                    return PredicateMatchShape::NONE;
            }
        }

        auto orderItemNullsFirst(const parser::v3::OrderByItem *item) -> bool
        {
            if (item == nullptr)
            {
                return false;
            }
            if (item->has_nulls_spec)
            {
                return item->nulls_first;
            }
            return !item->ascending;
        }

        auto runtimeMergeKeyExpressionText(const RuntimePlanMergeKey &key)
            -> std::string
        {
            if (!key.qualifier.empty() && !key.column_name.empty())
            {
                return key.qualifier + "." + key.column_name;
            }
            return key.column_name;
        }

        auto buildSyntheticSortOrderingKeys(const RuntimePlanMergeKey &key)
            -> std::vector<AccessPathDescriptor::OrderingKey>
        {
            std::vector<AccessPathDescriptor::OrderingKey> ordering_keys;
            const std::string expression_text = runtimeMergeKeyExpressionText(key);
            if (!expression_text.empty())
            {
                ordering_keys.push_back(makeOrderingKey(expression_text));
            }
            return ordering_keys;
        }

        auto buildSyntheticSortKeyTexts(const RuntimePlanMergeKey &key)
            -> std::vector<std::string>
        {
            std::vector<std::string> sort_keys;
            const std::string expression_text = runtimeMergeKeyExpressionText(key);
            if (!expression_text.empty())
            {
                sort_keys.push_back(expression_text);
            }
            return sort_keys;
        }

        auto orderingSatisfiesOrderBy(
            const AccessPathDescriptor &descriptor,
            const std::vector<parser::v3::OrderByItem *> &order_by_items,
            const parser::v3::StringPool &pool,
            std::string *reason_out = nullptr) -> bool
        {
            if (!descriptor.order_complete)
            {
                if (reason_out != nullptr)
                {
                    *reason_out = "input path is not marked order-complete";
                }
                return false;
            }
            if (descriptor.ordering_keys.size() < order_by_items.size())
            {
                if (reason_out != nullptr)
                {
                    *reason_out = "input ordered prefix shorter than ORDER BY list";
                }
                return false;
            }

            for (size_t i = 0; i < order_by_items.size(); ++i)
            {
                const auto *item = order_by_items[i];
                if (item == nullptr || item->expr == nullptr)
                {
                    if (reason_out != nullptr)
                    {
                        *reason_out = "ORDER BY item is missing expression payload";
                    }
                    return false;
                }

                const std::string required_expression =
                    normalizeOrderingExpression(expressionToString(item->expr, pool));
                const auto &available = descriptor.ordering_keys[i];
                if (required_expression.empty() ||
                    normalizeOrderingExpression(available.expression_text) !=
                        required_expression)
                {
                    if (reason_out != nullptr)
                    {
                        *reason_out = "ORDER BY expression mismatch at key " +
                                      std::to_string(i + 1);
                    }
                    return false;
                }
                if (available.descending != !item->ascending)
                {
                    if (reason_out != nullptr)
                    {
                        *reason_out = "ORDER BY direction mismatch at key " +
                                      std::to_string(i + 1);
                    }
                    return false;
                }
                if (item->has_nulls_spec &&
                    available.nulls_first != orderItemNullsFirst(item))
                {
                    if (reason_out != nullptr)
                    {
                        *reason_out = "ORDER BY null-order mismatch at key " +
                                      std::to_string(i + 1);
                    }
                    return false;
                }
            }
            return true;
        }

        auto orderingSatisfiesGrouping(
            const AccessPathDescriptor &descriptor,
            const std::vector<parser::v3::Expression *> &grouping_exprs,
            const parser::v3::StringPool &pool) -> bool
        {
            if (!descriptor.order_complete ||
                descriptor.ordering_keys.size() < grouping_exprs.size())
            {
                return false;
            }

            for (size_t i = 0; i < grouping_exprs.size(); ++i)
            {
                const auto *expr = grouping_exprs[i];
                if (expr == nullptr)
                {
                    return false;
                }
                const std::string required_expression =
                    normalizeOrderingExpression(expressionToString(expr, pool));
                const auto &available = descriptor.ordering_keys[i];
                if (required_expression.empty() ||
                    normalizeOrderingExpression(available.expression_text) !=
                        required_expression ||
                    available.descending)
                {
                    return false;
                }
            }
            return true;
        }

        auto orderingSatisfiesExpressions(
            const AccessPathDescriptor &descriptor,
            const std::vector<parser::v3::Expression *> &exprs,
            const parser::v3::StringPool &pool) -> bool
        {
            return orderingSatisfiesGrouping(descriptor, exprs, pool);
        }

        auto orderingSatisfiesWindow(
            const AccessPathDescriptor &descriptor,
            const std::vector<parser::v3::FunctionCallExpr *> &window_funcs,
            const parser::v3::StringPool &pool) -> bool
        {
            if (window_funcs.empty())
            {
                return true;
            }

            std::vector<parser::v3::Expression *> required_exprs;
            std::vector<bool> required_descending;
            std::vector<bool> required_nulls_first;

            bool seed = false;
            for (const auto *func : window_funcs)
            {
                if (func == nullptr || func->window == nullptr)
                {
                    continue;
                }

                std::vector<parser::v3::Expression *> candidate_exprs;
                std::vector<bool> candidate_descending;
                std::vector<bool> candidate_nulls_first;

                for (auto *partition_expr : func->window->partition_by)
                {
                    candidate_exprs.push_back(partition_expr);
                    candidate_descending.push_back(false);
                    candidate_nulls_first.push_back(false);
                }
                for (auto *order_item : func->window->order_by)
                {
                    if (order_item == nullptr)
                    {
                        continue;
                    }
                    candidate_exprs.push_back(order_item->expr);
                    candidate_descending.push_back(!order_item->ascending);
                    candidate_nulls_first.push_back(
                        orderItemNullsFirst(order_item));
                }

                if (!seed)
                {
                    required_exprs = candidate_exprs;
                    required_descending = candidate_descending;
                    required_nulls_first = candidate_nulls_first;
                    seed = true;
                    continue;
                }

                if (candidate_exprs.size() != required_exprs.size())
                {
                    return false;
                }

                for (size_t i = 0; i < candidate_exprs.size(); ++i)
                {
                    if (candidate_exprs[i] == nullptr || required_exprs[i] == nullptr)
                    {
                        return false;
                    }
                    if (normalizeOrderingExpression(
                            expressionToString(candidate_exprs[i], pool)) !=
                            normalizeOrderingExpression(
                                expressionToString(required_exprs[i], pool)) ||
                        candidate_descending[i] != required_descending[i] ||
                        candidate_nulls_first[i] != required_nulls_first[i])
                    {
                        return false;
                    }
                }
            }

            if (!seed)
            {
                return true;
            }
            if (!descriptor.order_complete ||
                descriptor.ordering_keys.size() < required_exprs.size())
            {
                return false;
            }
            for (size_t i = 0; i < required_exprs.size(); ++i)
            {
                if (required_exprs[i] == nullptr)
                {
                    return false;
                }
                const std::string required_expression =
                    normalizeOrderingExpression(
                        expressionToString(required_exprs[i], pool));
                const auto &available = descriptor.ordering_keys[i];
                if (required_expression.empty() ||
                    normalizeOrderingExpression(available.expression_text) !=
                        required_expression ||
                    available.descending != required_descending[i] ||
                    available.nulls_first != required_nulls_first[i])
                {
                    return false;
                }
            }
            return true;
        }

        auto collectDistinctExpressions(const parser::v3::SelectStmt *select_stmt)
            -> std::vector<parser::v3::Expression *>
        {
            std::vector<parser::v3::Expression *> exprs;
            if (select_stmt == nullptr)
            {
                return exprs;
            }

            if (!select_stmt->distinct_on.empty())
            {
                exprs = select_stmt->distinct_on;
                return exprs;
            }

            exprs.reserve(select_stmt->items.size());
            for (const auto *item : select_stmt->items)
            {
                if (item == nullptr ||
                    item->item_type != parser::v3::SelectItem::Type::EXPRESSION ||
                    item->expr == nullptr)
                {
                    exprs.clear();
                    return exprs;
                }
                exprs.push_back(item->expr);
            }
            return exprs;
        }

        auto isZeroId(const core::ID &id) -> bool
        {
            return id == core::ID{};
        }

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

        auto isNaturalJoinType(parser::JoinType join_type) -> bool
        {
            switch (join_type)
            {
                case parser::JoinType::NATURAL:
                case parser::JoinType::NATURAL_LEFT:
                case parser::JoinType::NATURAL_RIGHT:
                case parser::JoinType::NATURAL_FULL:
                    return true;
                default:
                    return false;
            }
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

        auto currentEpochMillis() -> uint64_t
        {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count());
        }

        auto normalizeMetricDimension(std::string value) -> std::string
        {
            auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
            value.erase(value.begin(),
                        std::find_if(value.begin(), value.end(), not_space));
            value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(),
                        value.end());
            return core::IdentifierUtils::toUpper(std::move(value));
        }

        auto parseMetricLabelsJson(const std::string &labels_json,
                                   std::unordered_map<std::string, std::string> &labels_out)
            -> bool
        {
            labels_out.clear();
            if (labels_json.empty())
            {
                return false;
            }

            try
            {
                const auto parsed = nlohmann::json::parse(labels_json);
                if (!parsed.is_object())
                {
                    return false;
                }
                for (auto it = parsed.begin(); it != parsed.end(); ++it)
                {
                    if (it.value().is_string())
                    {
                        labels_out.emplace(it.key(), it.value().get<std::string>());
                    }
                }
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        auto relationMetricLookupKeys(const ResolvedRelation &relation)
            -> std::vector<std::string>
        {
            std::vector<std::string> keys;
            auto append_key = [&keys](const std::string &value) {
                const std::string normalized = normalizeMetricDimension(value);
                if (normalized.empty() ||
                    std::find(keys.begin(), keys.end(), normalized) != keys.end())
                {
                    return;
                }
                keys.push_back(normalized);
            };

            append_key(relation.table_info.table_name);
            append_key(relation.table_path);
            append_key(relation.physical_table_path);
            append_key(relation.alias);
            return keys;
        }

        auto lookupMgaRelationCostSignal(const MgaCostingSnapshot &snapshot,
                                         const ResolvedRelation &relation)
            -> const MgaRelationCostSignal *
        {
            for (const std::string &key : relationMetricLookupKeys(relation))
            {
                const auto it = snapshot.relations.find(key);
                if (it != snapshot.relations.end() && it->second.available)
                {
                    return &it->second;
                }
            }
            return nullptr;
        }

        auto highestBucketWeight(const std::unordered_map<std::string, double> &bucket_counts,
                                 std::initializer_list<std::pair<const char *, double>> weights)
            -> std::pair<double, std::string>
        {
            double penalty = 0.0;
            std::string bucket_name;
            for (const auto &[bucket, weight] : weights)
            {
                auto it = bucket_counts.find(bucket);
                if (it != bucket_counts.end() && it->second > 0.0)
                {
                    if (weight >= penalty)
                    {
                        penalty = weight;
                        bucket_name = bucket;
                    }
                }
            }
            return {penalty, bucket_name};
        }

        auto describeMgaCostSignal(const MgaRelationCostSignal &signal,
                                   double commit_fence_backlog) -> std::string
        {
            std::ostringstream out;
            out << "cleanup_debt_bytes=" << signal.cleanup_debt_bytes
                << ", retained_dead_bytes=" << signal.retained_dead_bytes
                << ", index_backlog_entries=" << signal.index_backlog_entries
                << ", same_page_update_ratio=" << signal.same_page_update_ratio
                << ", commit_fence_backlog=" << commit_fence_backlog;
            if (!signal.chain_scatter_buckets.empty())
            {
                out << ", chain_scatter=";
                bool first = true;
                for (const auto &entry : signal.chain_scatter_buckets)
                {
                    if (!first)
                    {
                        out << ';';
                    }
                    out << entry.first << ':' << entry.second;
                    first = false;
                }
            }
            if (!signal.chain_depth_buckets.empty())
            {
                out << ", chain_depth=";
                bool first = true;
                for (const auto &entry : signal.chain_depth_buckets)
                {
                    if (!first)
                    {
                        out << ';';
                    }
                    out << entry.first << ':' << entry.second;
                    first = false;
                }
            }
            return out.str();
        }

        auto buildMgaCostPressure(const MgaRelationCostSignal *signal,
                                  double commit_fence_backlog,
                                  uint64_t base_pages,
                                  uint64_t base_rows) -> MgaCostPressure
        {
            MgaCostPressure pressure;
            if (signal == nullptr || !signal->available)
            {
                return pressure;
            }

            const double relation_bytes =
                std::max<double>(1.0, static_cast<double>(std::max<uint64_t>(1, base_pages)) * 8192.0);
            const double row_basis =
                std::max<double>(1.0, static_cast<double>(std::max<uint64_t>(1, base_rows)));

            pressure.available = true;
            pressure.cleanup_ratio =
                std::min(1.0, signal->cleanup_debt_bytes / relation_bytes);
            pressure.retained_ratio =
                std::min(1.0, signal->retained_dead_bytes / relation_bytes);
            pressure.index_backlog_ratio =
                std::min(1.0, signal->index_backlog_entries / row_basis);
            pressure.same_page_penalty =
                std::clamp(1.0 - signal->same_page_update_ratio, 0.0, 1.0);
            const auto [chain_scatter_penalty, chain_scatter_bucket] =
                highestBucketWeight(
                signal->chain_scatter_buckets,
                {{"wide", 0.80}, {"scattered", 0.45}, {"local", 0.15}, {"same_page", 0.0}});
            pressure.chain_scatter_penalty = chain_scatter_penalty;
            pressure.dominant_chain_scatter_bucket = chain_scatter_bucket;
            const auto [chain_depth_penalty, chain_depth_bucket] =
                highestBucketWeight(
                signal->chain_depth_buckets,
                {{"depth_8_plus", 0.90},
                 {"depth_4_7", 0.55},
                 {"depth_2_3", 0.20},
                 {"depth_1", 0.0}});
            pressure.chain_depth_penalty = chain_depth_penalty;
            pressure.dominant_chain_depth_bucket = chain_depth_bucket;
            pressure.fence_penalty =
                std::min(1.0, commit_fence_backlog / 64.0) * 0.20;
            pressure.rewrite_equivalent =
                (std::max(pressure.cleanup_ratio, pressure.retained_ratio) >= 0.25 ||
                 pressure.chain_depth_penalty >= 0.55 ||
                 pressure.chain_scatter_penalty >= 0.45) &&
                (pressure.index_backlog_ratio >= 0.20 ||
                 pressure.same_page_penalty >= 0.65 ||
                 commit_fence_backlog >= 64.0);
            return pressure;
        }

        auto describeMgaCostPressure(const MgaCostPressure &pressure) -> std::string
        {
            if (!pressure.available)
            {
                return "unavailable";
            }

            std::ostringstream out;
            out << "cleanup_ratio=" << pressure.cleanup_ratio
                << ", retained_ratio=" << pressure.retained_ratio
                << ", index_backlog_ratio=" << pressure.index_backlog_ratio
                << ", same_page_penalty=" << pressure.same_page_penalty
                << ", chain_scatter_penalty=" << pressure.chain_scatter_penalty;
            if (!pressure.dominant_chain_scatter_bucket.empty())
            {
                out << '[' << pressure.dominant_chain_scatter_bucket << ']';
            }
            out << ", chain_depth_penalty=" << pressure.chain_depth_penalty;
            if (!pressure.dominant_chain_depth_bucket.empty())
            {
                out << '[' << pressure.dominant_chain_depth_bucket << ']';
            }
            out << ", fence_penalty=" << pressure.fence_penalty
                << ", rewrite_equivalent="
                << (pressure.rewrite_equivalent ? "true" : "false");
            return out.str();
        }

        auto applyMgaCostPenalty(CostEstimate &cost,
                                 const MgaCostPressure &pressure,
                                 const std::string &scan_kind) -> double
        {
            if (!pressure.available)
            {
                return 0.0;
            }

            double cleanup_weight = 1.0;
            double index_weight = 0.35;
            if (scan_kind == "INDEX_SCAN" || scan_kind == "LSM_SCAN")
            {
                cleanup_weight = 0.65;
                index_weight = 0.85;
            }
            else if (scan_kind == "INDEX_ONLY_SCAN")
            {
                cleanup_weight = 0.35;
                index_weight = 1.0;
            }
            else if (scan_kind == "BRIN_SCAN" ||
                     scan_kind == "SUMMARY_FILTER_SCAN")
            {
                cleanup_weight = 0.85;
                index_weight = 0.45;
            }
            else if (scan_kind == "BITMAP_STORAGE_SCAN")
            {
                cleanup_weight = 0.80;
                index_weight = 0.80;
            }
            else if (scan_kind == "COLUMNSTORE_SCAN")
            {
                cleanup_weight = 0.50;
                index_weight = 0.65;
            }
            else if (scan_kind == "BITMAP_INDEX_SCAN")
            {
                cleanup_weight = 0.8;
                index_weight = 0.75;
            }

            const double penalty = std::min(
                4.0,
                (pressure.cleanup_ratio * 1.50 * cleanup_weight) +
                    (pressure.retained_ratio * 1.00 * cleanup_weight) +
                    (pressure.index_backlog_ratio * 0.80 * index_weight) +
                    pressure.chain_scatter_penalty +
                    pressure.chain_depth_penalty +
                    (pressure.same_page_penalty * 0.50) +
                    pressure.fence_penalty);
            if (penalty <= 0.0)
            {
                return 0.0;
            }

            cost.run_cost += penalty;
            cost.total_cost = cost.startup_cost + cost.run_cost;
            return penalty;
        }

        auto evaluateMgaAccessGovernance(
            const MgaCostPressure &pressure,
            const PlannerFamilyLoweringResult &lowering,
            const std::string &scan_kind,
            bool exact_key_lookup,
            bool ordered_output,
            uint64_t estimated_rows,
            uint64_t base_rows) -> MgaAccessGovernanceDecision
        {
            MgaAccessGovernanceDecision decision;
            if (!pressure.available || !pressure.rewrite_equivalent)
            {
                return decision;
            }

            const bool visibility_sensitive =
                lowering.visibility_enforcement !=
                    AccessPathVisibilityEnforcement::INDEX_NATIVE ||
                scan_kind == "INDEX_ONLY_SCAN" ||
                scan_kind == "BITMAP_INDEX_SCAN";
            if (!visibility_sensitive)
            {
                return decision;
            }

            const bool recheck_heavy =
                lowering.requires_recheck ||
                scan_kind == "BITMAP_INDEX_SCAN" ||
                scan_kind == "BITMAP_STORAGE_SCAN" ||
                scan_kind == "SUMMARY_FILTER_SCAN" ||
                scan_kind == "GIN_FILTER_SCAN" ||
                scan_kind == "TEXT_RECHECK_SCAN" ||
                scan_kind == "TEXT_BITMAP_SCAN";
            const uint64_t moderate_row_threshold =
                std::max<uint64_t>(8, std::max<uint64_t>(1, base_rows) / 32);
            const uint64_t broad_row_threshold =
                std::max<uint64_t>(32, std::max<uint64_t>(1, base_rows) / 8);
            const bool moderate_candidate = estimated_rows >= moderate_row_threshold;
            const bool broad_candidate = estimated_rows >= broad_row_threshold;

            if (scan_kind == "INDEX_ONLY_SCAN" &&
                !exact_key_lookup &&
                moderate_candidate &&
                !ordered_output)
            {
                decision.reject_candidate = true;
                decision.reason =
                    "rewrite-equivalent MGA churn rejects broad index-only access; " +
                    describeMgaCostPressure(pressure);
                return decision;
            }

            if ((scan_kind == "INDEX_SCAN" ||
                 scan_kind == "LSM_SCAN" ||
                 scan_kind == "BITMAP_INDEX_SCAN" ||
                 scan_kind == "BITMAP_STORAGE_SCAN") &&
                !exact_key_lookup &&
                broad_candidate &&
                !ordered_output &&
                (recheck_heavy ||
                 pressure.index_backlog_ratio >= 0.20 ||
                 pressure.chain_depth_penalty >= 0.55 ||
                 pressure.same_page_penalty >= 0.65))
            {
                decision.reject_candidate = true;
                decision.reason =
                    "rewrite-equivalent MGA churn rejects broad visibility-sensitive index path; " +
                    describeMgaCostPressure(pressure);
            }

            return decision;
        }

        auto loadMgaCostingSnapshot(core::Database *db) -> MgaCostingSnapshot
        {
            MgaCostingSnapshot snapshot;
            snapshot.contract_id = core::MgaObservabilityContract::contract_id();
            snapshot.metric_schema_version =
                core::MgaObservabilityContract::metric_schema_version();
            if (db == nullptr)
            {
                return snapshot;
            }

            std::vector<core::SqlRuntimeMetricRow> rows;
            if (core::SqlObservabilityViewBuilder::buildMgaRuntimeRows(
                    *db,
                    core::MetricsRegistry::getInstance(),
                    currentEpochMillis(),
                    rows) != core::Status::OK)
            {
                return snapshot;
            }

            const std::string db_uuid = db->uuid().toString();
            std::unordered_map<std::string, std::string> labels;
            for (const auto &row : rows)
            {
                if (!parseMetricLabelsJson(row.labels_json, labels))
                {
                    continue;
                }
                const auto db_it = labels.find("db");
                if (db_it != labels.end() && db_it->second != db_uuid)
                {
                    continue;
                }

                if (row.metric_name == "sb_buf_commit_fence_backlog")
                {
                    snapshot.commit_fence_backlog =
                        std::max(snapshot.commit_fence_backlog, row.value);
                    snapshot.available = true;
                    continue;
                }

                const auto relation_it = labels.find("relation");
                if (relation_it == labels.end())
                {
                    continue;
                }

                auto &signal =
                    snapshot.relations[normalizeMetricDimension(relation_it->second)];
                signal.available = true;
                snapshot.available = true;

                if (row.metric_name == "sb_gc_cleanup_debt_bytes")
                {
                    signal.cleanup_debt_bytes = std::max(signal.cleanup_debt_bytes, row.value);
                }
                else if (row.metric_name == "sb_mga_retained_dead_bytes")
                {
                    signal.retained_dead_bytes =
                        std::max(signal.retained_dead_bytes, row.value);
                }
                else if (row.metric_name == "sb_gc_index_backlog_entries")
                {
                    signal.index_backlog_entries =
                        std::max(signal.index_backlog_entries, row.value);
                }
                else if (row.metric_name == "sb_mga_same_page_update_ratio")
                {
                    signal.same_page_update_ratio =
                        std::clamp(row.value, 0.0, 1.0);
                }
                else if (row.metric_name == "sb_mga_chain_scatter_bucket")
                {
                    const auto bucket_it = labels.find("bucket");
                    if (bucket_it != labels.end())
                    {
                        signal.chain_scatter_buckets[bucket_it->second] += row.value;
                    }
                }
                else if (row.metric_name == "sb_mga_chain_depth_bucket")
                {
                    const auto bucket_it = labels.find("bucket");
                    if (bucket_it != labels.end())
                    {
                        signal.chain_depth_buckets[bucket_it->second] += row.value;
                    }
                }
            }

            return snapshot;
        }

        auto trimWhitespace(std::string value) -> std::string
        {
            auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
            value.erase(value.begin(),
                        std::find_if(value.begin(), value.end(), not_space));
            value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(),
                        value.end());
            return value;
        }

        auto stripStringLiteral(const std::string &input) -> std::string
        {
            if (input.size() >= 2 &&
                ((input.front() == '\'' && input.back() == '\'') ||
                 (input.front() == '"' && input.back() == '"')))
            {
                return input.substr(1, input.size() - 2);
            }
            return input;
        }

        auto normalizePartitionLiteral(const std::string &input) -> std::string
        {
            std::string trimmed = trimWhitespace(input);
            std::string upper = core::IdentifierUtils::toUpper(trimmed);
            if (upper.rfind("DATE", 0) == 0 ||
                upper.rfind("TIME", 0) == 0 ||
                upper.rfind("TIMESTAMP", 0) == 0)
            {
                const size_t quote_pos = trimmed.find('\'');
                if (quote_pos != std::string::npos)
                {
                    return stripStringLiteral(trimmed.substr(quote_pos));
                }
                const size_t space = trimmed.find(' ');
                if (space != std::string::npos)
                {
                    return stripStringLiteral(trimmed.substr(space + 1));
                }
            }
            return stripStringLiteral(trimmed);
        }

        auto parseNumericLiteral(const std::string &input,
                                 int64_t &value_out) -> bool
        {
            try
            {
                size_t index = 0;
                const std::string trimmed = trimWhitespace(input);
                const long long parsed = std::stoll(trimmed, &index, 10);
                if (index != trimmed.size())
                {
                    return false;
                }
                value_out = static_cast<int64_t>(parsed);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        auto parseDoubleLiteral(const std::string &input,
                                double &value_out) -> bool
        {
            try
            {
                size_t index = 0;
                const std::string trimmed = trimWhitespace(input);
                const double parsed = std::stod(trimmed, &index);
                if (index != trimmed.size())
                {
                    return false;
                }
                value_out = parsed;
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        auto splitTopLevel(const std::string &text) -> std::vector<std::string>
        {
            std::vector<std::string> parts;
            std::string current;
            int depth = 0;
            bool in_quote = false;
            for (size_t i = 0; i < text.size(); ++i)
            {
                const char ch = text[i];
                if (ch == '\'' && (i == 0 || text[i - 1] != '\\'))
                {
                    in_quote = !in_quote;
                    current.push_back(ch);
                    continue;
                }
                if (!in_quote)
                {
                    if (ch == '(')
                    {
                        ++depth;
                    }
                    else if (ch == ')' && depth > 0)
                    {
                        --depth;
                    }
                    else if (ch == ',' && depth == 0)
                    {
                        parts.push_back(trimWhitespace(current));
                        current.clear();
                        continue;
                    }
                }
                current.push_back(ch);
            }
            if (!current.empty())
            {
                parts.push_back(trimWhitespace(current));
            }
            return parts;
        }

        auto parsePartitionBounds(const std::string &bounds_text,
                                  PlannerPartitionBound &bound_out) -> bool
        {
            const std::string trimmed = trimWhitespace(bounds_text);
            const std::string upper = core::IdentifierUtils::toUpper(trimmed);
            if (upper.find("DEFAULT") != std::string::npos)
            {
                bound_out.kind = PlannerPartitionBound::Kind::DEFAULT;
                return true;
            }

            auto extractParenContent =
                [&](size_t start_pos, std::string &content_out) -> bool {
                    const size_t open_pos = trimmed.find('(', start_pos);
                    if (open_pos == std::string::npos)
                    {
                        return false;
                    }
                    int depth = 0;
                    for (size_t i = open_pos; i < trimmed.size(); ++i)
                    {
                        if (trimmed[i] == '(')
                        {
                            ++depth;
                        }
                        else if (trimmed[i] == ')')
                        {
                            --depth;
                            if (depth == 0)
                            {
                                content_out =
                                    trimmed.substr(open_pos + 1, i - open_pos - 1);
                                return true;
                            }
                        }
                    }
                    return false;
                };

            const size_t in_pos = upper.find("IN");
            if (in_pos != std::string::npos)
            {
                std::string content;
                if (!extractParenContent(in_pos, content))
                {
                    return false;
                }
                bound_out.kind = PlannerPartitionBound::Kind::LIST;
                for (const auto &entry : splitTopLevel(content))
                {
                    if (entry.empty())
                    {
                        continue;
                    }
                    if (entry.front() == '(' && entry.back() == ')')
                    {
                        bound_out.list_values.push_back(
                            splitTopLevel(entry.substr(1, entry.size() - 2)));
                    }
                    else
                    {
                        bound_out.list_values.push_back({entry});
                    }
                }
                return true;
            }

            const size_t from_pos = upper.find("FROM");
            const size_t to_pos = upper.find("TO");
            if (from_pos != std::string::npos && to_pos != std::string::npos)
            {
                std::string start_content;
                std::string end_content;
                if (!extractParenContent(from_pos, start_content) ||
                    !extractParenContent(to_pos, end_content))
                {
                    return false;
                }
                bound_out.kind = PlannerPartitionBound::Kind::RANGE;
                bound_out.range_start = splitTopLevel(start_content);
                bound_out.range_end = splitTopLevel(end_content);
                bound_out.has_start = !bound_out.range_start.empty();
                bound_out.has_end = !bound_out.range_end.empty();
                return true;
            }

            return false;
        }

        auto comparePartitionLiteral(const std::string &left,
                                     const std::string &right,
                                     core::DataType type) -> int
        {
            if (type == core::DataType::INT8 ||
                type == core::DataType::INT16 ||
                type == core::DataType::INT32 ||
                type == core::DataType::INT64 ||
                type == core::DataType::UINT8 ||
                type == core::DataType::UINT16 ||
                type == core::DataType::UINT32 ||
                type == core::DataType::UINT64)
            {
                int64_t left_value = 0;
                int64_t right_value = 0;
                if (parseNumericLiteral(left, left_value) &&
                    parseNumericLiteral(right, right_value))
                {
                    if (left_value < right_value) return -1;
                    if (left_value > right_value) return 1;
                    return 0;
                }
            }
            if (type == core::DataType::FLOAT32 ||
                type == core::DataType::FLOAT64 ||
                type == core::DataType::DECIMAL)
            {
                double left_value = 0.0;
                double right_value = 0.0;
                if (parseDoubleLiteral(left, left_value) &&
                    parseDoubleLiteral(right, right_value))
                {
                    if (left_value < right_value) return -1;
                    if (left_value > right_value) return 1;
                    return 0;
                }
            }

            const std::string normalized_left = normalizePartitionLiteral(left);
            const std::string normalized_right = normalizePartitionLiteral(right);
            if (normalized_left < normalized_right) return -1;
            if (normalized_left > normalized_right) return 1;
            return 0;
        }

        auto resolvePredicateLiteralText(const ResolvedScanPredicate &predicate,
                                         const ParameterBindings *parameter_bindings,
                                         std::string &literal_out) -> bool
        {
            literal_out.clear();
            if (predicate.literal_kind == "PARAMETER")
            {
                if (parameter_bindings == nullptr ||
                    predicate.literal_text.size() < 2 ||
                    predicate.literal_text.front() != '$')
                {
                    return false;
                }
                try
                {
                    const uint32_t index =
                        static_cast<uint32_t>(std::stoul(predicate.literal_text.substr(1)));
                    BoundParameterValue value;
                    if (!parameter_bindings->getPositional(index, value) || value.is_null)
                    {
                        return false;
                    }
                    literal_out = value.text;
                    return !literal_out.empty();
                }
                catch (...)
                {
                    return false;
                }
            }

            if (predicate.literal_text.empty())
            {
                return false;
            }
            literal_out = predicate.literal_text;
            return true;
        }

        auto applyPredicateConstraint(PredicateConstraint &constraint,
                                      const ResolvedScanPredicate &predicate,
                                      core::DataType type,
                                      const ParameterBindings *parameter_bindings) -> bool
        {
            std::string literal_text;
            if (!resolvePredicateLiteralText(predicate,
                                             parameter_bindings,
                                             literal_text))
            {
                return false;
            }

            auto strengthenLower =
                [&](const std::string &candidate, bool inclusive) {
                    if (!constraint.has_lower)
                    {
                        constraint.has_lower = true;
                        constraint.lower = candidate;
                        constraint.lower_inclusive = inclusive;
                        return;
                    }

                    const int cmp =
                        comparePartitionLiteral(candidate, constraint.lower, type);
                    if (cmp > 0 ||
                        (cmp == 0 && !inclusive && constraint.lower_inclusive))
                    {
                        constraint.lower = candidate;
                        constraint.lower_inclusive = inclusive;
                    }
                };

            auto strengthenUpper =
                [&](const std::string &candidate, bool inclusive) {
                    if (!constraint.has_upper)
                    {
                        constraint.has_upper = true;
                        constraint.upper = candidate;
                        constraint.upper_inclusive = inclusive;
                        return;
                    }

                    const int cmp =
                        comparePartitionLiteral(candidate, constraint.upper, type);
                    if (cmp < 0 ||
                        (cmp == 0 && !inclusive && constraint.upper_inclusive))
                    {
                        constraint.upper = candidate;
                        constraint.upper_inclusive = inclusive;
                    }
                };

            const std::string op =
                core::IdentifierUtils::toUpper(predicate.operator_name);
            if (op == "=")
            {
                strengthenLower(literal_text, true);
                strengthenUpper(literal_text, true);
                constraint.valid = true;
                return true;
            }
            if (op == ">")
            {
                strengthenLower(literal_text, false);
                constraint.valid = true;
                return true;
            }
            if (op == ">=")
            {
                strengthenLower(literal_text, true);
                constraint.valid = true;
                return true;
            }
            if (op == "<")
            {
                strengthenUpper(literal_text, false);
                constraint.valid = true;
                return true;
            }
            if (op == "<=")
            {
                strengthenUpper(literal_text, true);
                constraint.valid = true;
                return true;
            }
            return false;
        }

        auto makeConstraintTuple(const std::vector<PredicateConstraint> &constraints,
                                 bool lower_bound)
            -> std::vector<PartitionConstraintValue>
        {
            std::vector<PartitionConstraintValue> tuple;
            tuple.reserve(constraints.size());
            for (const auto &constraint : constraints)
            {
                PartitionConstraintValue entry;
                if (lower_bound)
                {
                    if (constraint.has_lower)
                    {
                        entry.kind = PartitionConstraintValueKind::FINITE;
                        entry.value = constraint.lower;
                    }
                    else
                    {
                        entry.kind =
                            PartitionConstraintValueKind::NEGATIVE_INFINITY;
                    }
                }
                else if (constraint.has_upper)
                {
                    entry.kind = PartitionConstraintValueKind::FINITE;
                    entry.value = constraint.upper;
                }
                else
                {
                    entry.kind = PartitionConstraintValueKind::POSITIVE_INFINITY;
                }
                tuple.push_back(std::move(entry));
            }
            return tuple;
        }

        auto compareConstraintTupleToLiteralTuple(
            const std::vector<PartitionConstraintValue> &left,
            const std::vector<std::string> &right,
            const std::vector<core::DataType> &types) -> int
        {
            const size_t count =
                std::min(left.size(), std::min(right.size(), types.size()));
            for (size_t i = 0; i < count; ++i)
            {
                if (left[i].kind == PartitionConstraintValueKind::NEGATIVE_INFINITY)
                {
                    return -1;
                }
                if (left[i].kind == PartitionConstraintValueKind::POSITIVE_INFINITY)
                {
                    return 1;
                }
                const int cmp =
                    comparePartitionLiteral(left[i].value, right[i], types[i]);
                if (cmp != 0)
                {
                    return cmp;
                }
            }
            if (left.size() < right.size()) return -1;
            if (left.size() > right.size()) return 1;
            return 0;
        }

        auto rangeMatchesConstraints(
            const PlannerPartitionBound &bound,
            const std::vector<PredicateConstraint> &constraints,
            const std::vector<core::DataType> &types) -> bool
        {
            bool has_any_constraint = false;
            for (const auto &constraint : constraints)
            {
                if (constraint.valid)
                {
                    has_any_constraint = true;
                    break;
                }
            }
            if (!has_any_constraint)
            {
                return true;
            }

            const auto lower_tuple = makeConstraintTuple(constraints, true);
            const auto upper_tuple = makeConstraintTuple(constraints, false);
            if (bound.has_end && !bound.range_end.empty())
            {
                if (compareConstraintTupleToLiteralTuple(lower_tuple,
                                                        bound.range_end,
                                                        types) >= 0)
                {
                    return false;
                }
            }
            if (bound.has_start && !bound.range_start.empty())
            {
                if (compareConstraintTupleToLiteralTuple(upper_tuple,
                                                        bound.range_start,
                                                        types) < 0)
                {
                    return false;
                }
            }
            return true;
        }

        auto constraintsDescribePointTuple(const std::vector<PredicateConstraint> &constraints,
                                           const std::vector<core::DataType> &types,
                                           std::vector<std::string> &tuple_out) -> bool
        {
            if (constraints.size() != types.size())
            {
                return false;
            }
            tuple_out.clear();
            tuple_out.reserve(constraints.size());
            for (size_t i = 0; i < constraints.size(); ++i)
            {
                const auto &constraint = constraints[i];
                if (!constraint.valid ||
                    !constraint.has_lower ||
                    !constraint.has_upper ||
                    !constraint.lower_inclusive ||
                    !constraint.upper_inclusive ||
                    comparePartitionLiteral(constraint.lower,
                                            constraint.upper,
                                            types[i]) != 0)
                {
                    tuple_out.clear();
                    return false;
                }
                tuple_out.push_back(constraint.lower);
            }
            return true;
        }

        auto listValueMatchesConstraint(const std::string &value,
                                        const PredicateConstraint &constraint,
                                        core::DataType type) -> bool
        {
            if (constraint.has_lower)
            {
                const int cmp =
                    comparePartitionLiteral(value, constraint.lower, type);
                if (cmp < 0 || (cmp == 0 && !constraint.lower_inclusive))
                {
                    return false;
                }
            }
            if (constraint.has_upper)
            {
                const int cmp =
                    comparePartitionLiteral(value, constraint.upper, type);
                if (cmp > 0 || (cmp == 0 && !constraint.upper_inclusive))
                {
                    return false;
                }
            }
            return true;
        }

        auto listTupleMatchesConstraints(
            const std::vector<std::string> &values,
            const std::vector<PredicateConstraint> &constraints,
            const std::vector<core::DataType> &types) -> bool
        {
            if (values.size() != constraints.size() || values.size() != types.size())
            {
                return false;
            }

            for (size_t i = 0; i < values.size(); ++i)
            {
                if (!listValueMatchesConstraint(values[i], constraints[i], types[i]))
                {
                    return false;
                }
            }
            return true;
        }

        auto rangeMatchesConstraint(const PlannerPartitionBound &bound,
                                    const PredicateConstraint &constraint,
                                    core::DataType type) -> bool
        {
            if (!constraint.valid)
            {
                return true;
            }

            if (constraint.has_lower && bound.has_end && !bound.range_end.empty())
            {
                const int cmp = comparePartitionLiteral(
                    constraint.lower,
                    bound.range_end.front(),
                    type);
                // Range partitions use an exclusive upper bound, so a predicate
                // lower bound at the partition end cannot match this child.
                if (cmp >= 0)
                {
                    return false;
                }
            }
            if (constraint.has_upper && bound.has_start && !bound.range_start.empty())
            {
                const int cmp = comparePartitionLiteral(
                    constraint.upper,
                    bound.range_start.front(),
                    type);
                if (cmp < 0 || (cmp == 0 && !constraint.upper_inclusive))
                {
                    return false;
                }
            }
            return true;
        }

        auto loadTableMetadata(core::CatalogManager *catalog,
                               const core::CatalogManager::TableInfo &table_info,
                               nlohmann::json &metadata_out,
                               core::ErrorContext *ctx) -> bool
        {
            metadata_out = nlohmann::json::object();
            if (catalog == nullptr || isZeroId(table_info.storage_params_oid))
            {
                return false;
            }

            std::string params;
            if (catalog->loadStringFromToast(table_info.storage_params_oid,
                                             0,
                                             params,
                                             ctx) != core::Status::OK ||
                params.empty())
            {
                return false;
            }

            try
            {
                metadata_out = nlohmann::json::parse(params);
                return true;
            }
            catch (...)
            {
                metadata_out = nlohmann::json::object();
                return false;
            }
        }

        auto splitSchemaComponents(const std::string &path)
            -> std::vector<std::string>
        {
            std::vector<std::string> components;
            size_t start = 0;
            while (start < path.size())
            {
                const size_t dot = path.find('.', start);
                const size_t end = dot == std::string::npos ? path.size() : dot;
                if (end > start)
                {
                    components.emplace_back(path.substr(start, end - start));
                }
                if (dot == std::string::npos)
                {
                    break;
                }
                start = dot + 1;
            }
            return components;
        }

        auto joinSchemaComponents(const std::vector<std::string> &components)
            -> std::string
        {
            std::string joined;
            for (size_t i = 0; i < components.size(); ++i)
            {
                if (i > 0)
                {
                    joined.push_back('.');
                }
                joined.append(components[i]);
            }
            return joined;
        }

        auto resolveQualifiedTable(core::CatalogManager *catalog,
                                   const core::ID &default_schema_id,
                                   const std::string &qualified_name,
                                   core::CatalogManager::TableInfo &table_info_out,
                                   core::ErrorContext *ctx) -> bool
        {
            if (catalog == nullptr)
            {
                return false;
            }

            const std::string normalized = qualified_name;
            const auto components = splitSchemaComponents(normalized);
            if (components.empty())
            {
                return false;
            }

            const std::string table_name = components.back();
            if (components.size() == 1)
            {
                return catalog->getTable(default_schema_id,
                                         table_name,
                                         table_info_out,
                                         ctx) == core::Status::OK;
            }

            std::vector<std::string> schema_components = components;
            schema_components.pop_back();
            const std::string schema_path = joinSchemaComponents(schema_components);
            core::CatalogManager::SchemaInfo schema_info;
            if (catalog->getSchema(schema_path, schema_info, ctx) != core::Status::OK)
            {
                return false;
            }
            return catalog->getTable(schema_info.schema_id,
                                     table_name,
                                     table_info_out,
                                     ctx) == core::Status::OK;
        }

        auto analyzePartitionPruning(core::CatalogManager *catalog,
                                     const ResolvedRelation &relation,
                                     const ParameterBindings *parameter_bindings)
            -> PartitionPruningResult
        {
            PartitionPruningResult result;
            if (catalog == nullptr ||
                !relation.resolved ||
                isZeroId(relation.table_info.table_id))
            {
                return result;
            }

            nlohmann::json metadata;
            core::ErrorContext metadata_ctx;
            if (!loadTableMetadata(catalog, relation.table_info, metadata, &metadata_ctx) ||
                !metadata.contains("partition") ||
                !metadata["partition"].is_object())
            {
                return result;
            }

            const auto &partition = metadata["partition"];
            result.available = true;
            if (partition.contains("strategy") && partition["strategy"].is_string())
            {
                result.strategy = partition["strategy"].get<std::string>();
            }
            if (!partition.contains("columns") || !partition["columns"].is_array() ||
                partition["columns"].empty() || !partition.contains("children") ||
                !partition["children"].is_array())
            {
                return result;
            }

            std::vector<core::DataType> partition_key_types;
            std::vector<PredicateConstraint> constraints;
            for (const auto &column_entry : partition["columns"])
            {
                if (!column_entry.is_string())
                {
                    return result;
                }
                const std::string key_column = column_entry.get<std::string>();
                result.key_columns.push_back(key_column);
                const auto column_it =
                    std::find_if(relation.columns.begin(),
                                 relation.columns.end(),
                                 [&](const core::CatalogManager::ColumnInfo &column) {
                                     return core::IdentifierUtils::namesMatch(
                                         column.column_name,
                                         false,
                                         key_column,
                                         false);
                                 });
                if (column_it == relation.columns.end())
                {
                    return result;
                }
                partition_key_types.push_back(
                    static_cast<core::DataType>(column_it->data_type));
                constraints.emplace_back();
            }
            result.key_column =
                result.key_columns.empty() ? std::string() : result.key_columns.front();

            bool has_static_constraint = false;
            for (const auto &predicate : relation.local_predicates)
            {
                const auto key_it =
                    std::find_if(result.key_columns.begin(),
                                 result.key_columns.end(),
                                 [&](const std::string &column_name) {
                                     return core::IdentifierUtils::namesMatch(
                                         predicate.column_name,
                                         false,
                                         column_name,
                                         false);
                                 });
                if (key_it == result.key_columns.end())
                {
                    continue;
                }

                result.predicates.push_back(RuntimePlanIndexPredicate{
                    true,
                    std::string(),
                    std::string(),
                    predicate.column_name,
                    predicate.operator_name,
                    predicate.literal_kind,
                    predicate.literal_text});

                const size_t key_index =
                    static_cast<size_t>(std::distance(result.key_columns.begin(),
                                                     key_it));
                const bool applied = applyPredicateConstraint(
                    constraints[key_index],
                    predicate,
                    partition_key_types[key_index],
                    parameter_bindings);
                has_static_constraint = has_static_constraint || applied;

                if (predicate.literal_kind == "PARAMETER" && !applied)
                {
                    result.runtime_pruning_eligible = true;
                    if (std::find(result.runtime_pruning_sources.begin(),
                                  result.runtime_pruning_sources.end(),
                                  "PARAMETER") ==
                        result.runtime_pruning_sources.end())
                    {
                        result.runtime_pruning_sources.push_back("PARAMETER");
                    }
                }
            }

            bool matched_any = false;
            bool has_default_child = false;
            std::string default_child_name;
            core::CatalogManager::TableInfo default_child_info;
            for (const auto &entry : partition["children"])
            {
                if (!entry.is_object() ||
                    !entry.contains("name") ||
                    !entry["name"].is_string())
                {
                    continue;
                }

                const std::string child_name = entry["name"].get<std::string>();
                PlannerPartitionBound bound;
                if (entry.contains("bounds") && entry["bounds"].is_string())
                {
                    if (!parsePartitionBounds(entry["bounds"].get<std::string>(), bound))
                    {
                        continue;
                    }
                }

                core::CatalogManager::TableInfo child_info;
                core::ErrorContext child_ctx;
                if (!resolveQualifiedTable(catalog,
                                           relation.table_info.schema_id,
                                           child_name,
                                           child_info,
                                           &child_ctx))
                {
                    continue;
                }

                if (bound.kind == PlannerPartitionBound::Kind::DEFAULT)
                {
                    has_default_child = true;
                    default_child_name = child_name;
                    default_child_info = child_info;
                    continue;
                }

                bool matches = !has_static_constraint;
                if (has_static_constraint)
                {
                    matches = false;
                    if (bound.kind == PlannerPartitionBound::Kind::LIST)
                    {
                        for (const auto &values : bound.list_values)
                        {
                            if (listTupleMatchesConstraints(values,
                                                            constraints,
                                                            partition_key_types))
                            {
                                matches = true;
                                break;
                            }
                        }
                    }
                    else if (bound.kind == PlannerPartitionBound::Kind::RANGE)
                    {
                        matches = rangeMatchesConstraints(bound,
                                                         constraints,
                                                         partition_key_types);
                    }
                }

                if (!matches)
                {
                    result.pruned_targets_at_plan.push_back(child_name);
                    continue;
                }

                result.targets.push_back(child_name);
                result.rows += child_info.row_count == 0 ? 0 : child_info.row_count;
                result.pages +=
                    std::max<uint64_t>(1,
                                       (child_info.row_count == 0
                                            ? 0
                                            : child_info.row_count) /
                                           100);
                matched_any = true;
            }

            std::vector<std::string> point_tuple;
            const bool point_tuple_known =
                constraintsDescribePointTuple(constraints,
                                             partition_key_types,
                                             point_tuple);
            if (has_default_child)
            {
                if (!point_tuple_known || !matched_any)
                {
                    result.targets.push_back(default_child_name);
                    result.rows += default_child_info.row_count == 0
                        ? 0
                        : default_child_info.row_count;
                    result.pages +=
                        std::max<uint64_t>(1,
                                           (default_child_info.row_count == 0
                                                ? 0
                                                : default_child_info.row_count) /
                                               100);
                }
                else
                {
                    result.pruned_targets_at_plan.push_back(default_child_name);
                }
            }

            result.pruned =
                has_static_constraint &&
                !result.targets.empty() &&
                !result.pruned_targets_at_plan.empty();
            return result;
        }

        auto annotateTopNSort(RuntimePlanNode &node,
                              int64_t limit_count,
                              int64_t offset_count) -> void
        {
            if (limit_count < 0)
            {
                return;
            }

            if (node.node_type == "Sort")
            {
                std::ostringstream detail;
                detail << node.detail_text;
                if (!node.detail_text.empty())
                {
                    detail << ' ';
                }
                detail << "topn=" << (limit_count + std::max<int64_t>(offset_count, 0));
                node.detail_text = detail.str();
                return;
            }

            for (auto &child : node.children)
            {
                annotateTopNSort(child, limit_count, offset_count);
            }
        }

        enum class PlannerSpillPolicy : uint8_t
        {
            ALLOW = 0,
            DISALLOW = 1
        };

        enum class PlannerJoinSearchMode : uint8_t
        {
            AUTO = 0,
            EXHAUSTIVE_DP = 1,
            BOUNDED_DP = 2,
            HYPERGRAPH_GREEDY = 3,
            HEURISTIC_GREEDY = 4,
            INPUT_ORDER = 5
        };

        enum class PlannerJoinMethodControl : uint8_t
        {
            AUTO = 0,
            NESTED_LOOP_ONLY = 1,
            HASH_ONLY = 2,
            MERGE_ONLY = 3
        };

        struct PlannerControlSurface
        {
            uint64_t work_mem_bytes = 0;
            PlannerSpillPolicy spill_policy = PlannerSpillPolicy::ALLOW;
            PlannerJoinSearchMode join_search = PlannerJoinSearchMode::AUTO;
            size_t search_depth = 0;
            size_t exhaustive_join_limit = 8;
            size_t bounded_dp_join_limit = 12;
            size_t max_states_considered = 256;
            size_t fallback_prune_level = 1;
            PlannerJoinMethodControl join_method =
                PlannerJoinMethodControl::AUTO;
            bool enable_parallel = false;
            bool enable_parallel_scan = false;
            bool enable_parallel_hash = false;
            bool enable_parallel_aggregate = false;
            bool enable_parallel_join = false;
            bool parallel_leader_participation = true;
            uint32_t max_parallel_workers = 0;
            uint32_t max_parallel_workers_per_gather = 0;
            uint32_t min_parallel_rows_per_worker = 1024;
            uint32_t min_parallel_table_scan_size = 0;
            double parallel_setup_cost = 1000.0;
            double parallel_tuple_cost = 0.1;
            std::vector<RuntimePlanControlEntry> runtime_controls;
        };

        auto plannerSpillPolicyName(PlannerSpillPolicy policy) -> const char *
        {
            return policy == PlannerSpillPolicy::DISALLOW ? "DISALLOW" : "ALLOW";
        }

        auto plannerJoinSearchName(PlannerJoinSearchMode mode) -> const char *
        {
            switch (mode)
            {
                case PlannerJoinSearchMode::EXHAUSTIVE_DP: return "EXHAUSTIVE_DP";
                case PlannerJoinSearchMode::BOUNDED_DP: return "BOUNDED_DP";
                case PlannerJoinSearchMode::HYPERGRAPH_GREEDY:
                    return "HYPERGRAPH_GREEDY";
                case PlannerJoinSearchMode::HEURISTIC_GREEDY:
                    return "HEURISTIC_GREEDY";
                case PlannerJoinSearchMode::INPUT_ORDER: return "INPUT_ORDER_ONLY";
                case PlannerJoinSearchMode::AUTO:
                default: return "AUTO";
            }
        }

        auto plannerJoinMethodName(PlannerJoinMethodControl mode) -> const char *
        {
            switch (mode)
            {
                case PlannerJoinMethodControl::NESTED_LOOP_ONLY:
                    return "NESTED_LOOP";
                case PlannerJoinMethodControl::HASH_ONLY:
                    return "HASH_JOIN";
                case PlannerJoinMethodControl::MERGE_ONLY:
                    return "MERGE_JOIN";
                case PlannerJoinMethodControl::AUTO:
                default:
                    return "AUTO";
            }
        }

        auto parsePlannerSizeBytes(const std::string &text,
                                   uint64_t &bytes_out) -> bool
        {
            std::string trimmed = trimWhitespace(text);
            if (trimmed.empty())
            {
                return false;
            }

            size_t split = 0;
            while (split < trimmed.size() &&
                   (std::isdigit(static_cast<unsigned char>(trimmed[split])) ||
                    trimmed[split] == '.'))
            {
                ++split;
            }
            if (split == 0)
            {
                return false;
            }

            double magnitude = 0.0;
            try
            {
                magnitude = std::stod(trimmed.substr(0, split));
            }
            catch (...)
            {
                return false;
            }

            std::string suffix =
                core::IdentifierUtils::toUpper(trimWhitespace(trimmed.substr(split)));
            uint64_t multiplier = 1;
            if (suffix.empty() || suffix == "B")
            {
                multiplier = 1;
            }
            else if (suffix == "K" || suffix == "KB" || suffix == "KIB")
            {
                multiplier = 1024ULL;
            }
            else if (suffix == "M" || suffix == "MB" || suffix == "MIB")
            {
                multiplier = 1024ULL * 1024ULL;
            }
            else if (suffix == "G" || suffix == "GB" || suffix == "GIB")
            {
                multiplier = 1024ULL * 1024ULL * 1024ULL;
            }
            else if (suffix == "T" || suffix == "TB" || suffix == "TIB")
            {
                multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
            }
            else
            {
                return false;
            }

            bytes_out = static_cast<uint64_t>(magnitude * static_cast<double>(multiplier));
            return bytes_out > 0;
        }

        auto readPlannerSessionSetting(core::ConnectionContext *conn_ctx,
                                       std::string &value_out,
                                       std::initializer_list<const char *> names) -> bool
        {
            if (conn_ctx == nullptr)
            {
                return false;
            }
            for (const char *name : names)
            {
                if (name != nullptr && conn_ctx->getSessionVariable(name, value_out))
                {
                    return true;
                }
            }
            return false;
        }

        auto splitPlannerDirectiveAssignments(const std::string &text)
            -> std::vector<std::string>
        {
            std::vector<std::string> parts;
            std::string current;
            bool in_quote = false;
            for (size_t i = 0; i < text.size(); ++i)
            {
                const char ch = text[i];
                if ((ch == '\'' || ch == '"') &&
                    (i == 0 || text[i - 1] != '\\'))
                {
                    in_quote = !in_quote;
                    current.push_back(ch);
                    continue;
                }
                if (!in_quote && (ch == ';' || ch == ','))
                {
                    const std::string trimmed = trimWhitespace(current);
                    if (!trimmed.empty())
                    {
                        parts.push_back(trimmed);
                    }
                    current.clear();
                    continue;
                }
                current.push_back(ch);
            }

            const std::string trimmed = trimWhitespace(current);
            if (!trimmed.empty())
            {
                parts.push_back(trimmed);
            }
            return parts;
        }

        auto parsePlannerJoinSearchMode(const std::string &value,
                                        PlannerJoinSearchMode &mode_out) -> bool
        {
            const std::string upper =
                core::IdentifierUtils::toUpper(trimWhitespace(value));
            if (upper.empty() || upper == "AUTO")
            {
                mode_out = PlannerJoinSearchMode::AUTO;
                return true;
            }
            if (upper == "EXHAUSTIVE" || upper == "EXHAUSTIVE_DP" ||
                upper == "DP")
            {
                mode_out = PlannerJoinSearchMode::EXHAUSTIVE_DP;
                return true;
            }
            if (upper == "BOUNDED" || upper == "BOUNDED_DP")
            {
                mode_out = PlannerJoinSearchMode::BOUNDED_DP;
                return true;
            }
            if (upper == "HYPERGRAPH" || upper == "HYPERGRAPH_GREEDY")
            {
                mode_out = PlannerJoinSearchMode::HYPERGRAPH_GREEDY;
                return true;
            }
            if (upper == "GREEDY" || upper == "LEFT_DEEP" ||
                upper == "HEURISTIC" || upper == "HEURISTIC_GREEDY")
            {
                mode_out = PlannerJoinSearchMode::HEURISTIC_GREEDY;
                return true;
            }
            if (upper == "INPUT_ORDER" || upper == "INPUT_ORDER_ONLY" ||
                upper == "PRESERVE")
            {
                mode_out = PlannerJoinSearchMode::INPUT_ORDER;
                return true;
            }
            return false;
        }

        auto parsePlannerJoinMethodControl(const std::string &value,
                                           PlannerJoinMethodControl &mode_out) -> bool
        {
            const std::string upper =
                core::IdentifierUtils::toUpper(trimWhitespace(value));
            if (upper.empty() || upper == "AUTO")
            {
                mode_out = PlannerJoinMethodControl::AUTO;
                return true;
            }
            if (upper == "NESTED_LOOP" || upper == "NESTED_LOOP_ONLY" ||
                upper == "NESTLOOP")
            {
                mode_out = PlannerJoinMethodControl::NESTED_LOOP_ONLY;
                return true;
            }
            if (upper == "HASH" || upper == "HASH_JOIN" || upper == "HASH_ONLY")
            {
                mode_out = PlannerJoinMethodControl::HASH_ONLY;
                return true;
            }
            if (upper == "MERGE" || upper == "MERGE_JOIN" || upper == "MERGE_ONLY")
            {
                mode_out = PlannerJoinMethodControl::MERGE_ONLY;
                return true;
            }
            return false;
        }

        auto parsePlannerSearchDepth(const std::string &value,
                                     size_t &depth_out) -> bool
        {
            const std::string trimmed = trimWhitespace(value);
            if (trimmed.empty() || core::IdentifierUtils::toUpper(trimmed) == "AUTO")
            {
                depth_out = 0;
                return true;
            }

            try
            {
                size_t index = 0;
                const unsigned long parsed = std::stoul(trimmed, &index, 10);
                if (index != trimmed.size())
                {
                    return false;
                }
                depth_out = static_cast<size_t>(parsed);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        auto parsePlannerBoolSetting(const std::string &value,
                                     bool &value_out) -> bool
        {
            const std::string upper =
                core::IdentifierUtils::toUpper(trimWhitespace(value));
            if (upper == "1" || upper == "ON" || upper == "TRUE" ||
                upper == "YES")
            {
                value_out = true;
                return true;
            }
            if (upper == "0" || upper == "OFF" || upper == "FALSE" ||
                upper == "NO" || upper.empty())
            {
                value_out = false;
                return true;
            }
            return false;
        }

        auto parsePlannerUint32Setting(const std::string &value,
                                       uint32_t &value_out) -> bool
        {
            const std::string trimmed = trimWhitespace(value);
            if (trimmed.empty())
            {
                value_out = 0;
                return true;
            }
            try
            {
                size_t index = 0;
                const unsigned long parsed = std::stoul(trimmed, &index, 10);
                if (index != trimmed.size())
                {
                    return false;
                }
                value_out = static_cast<uint32_t>(parsed);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        auto parsePlannerDoubleSetting(const std::string &value,
                                       double &value_out) -> bool
        {
            const std::string trimmed = trimWhitespace(value);
            if (trimmed.empty())
            {
                value_out = 0.0;
                return true;
            }
            try
            {
                size_t index = 0;
                value_out = std::stod(trimmed, &index);
                return index == trimmed.size();
            }
            catch (...)
            {
                return false;
            }
        }

        auto upsertPlannerControl(std::vector<RuntimePlanControlEntry> &controls,
                                  const std::string &name,
                                  const std::string &value,
                                  const std::string &source) -> void
        {
            for (auto &entry : controls)
            {
                if (entry.name == name)
                {
                    entry.value = value;
                    entry.source = source;
                    entry.enforced = true;
                    return;
                }
            }
            controls.push_back(RuntimePlanControlEntry{name, value, source, true});
        }

        auto ensurePlannerControlDefault(std::vector<RuntimePlanControlEntry> &controls,
                                         const std::string &name,
                                         const std::string &value) -> void
        {
            for (const auto &entry : controls)
            {
                if (entry.name == name)
                {
                    return;
                }
            }
            controls.push_back(RuntimePlanControlEntry{name, value, "DEFAULT", true});
        }

        auto resolvePlannerControlSurface(core::ConnectionContext *conn_ctx,
                                          const CostParameters &base_params,
                                          PlannerControlSurface &controls_out,
                                          std::string &error_out) -> bool
        {
            controls_out = PlannerControlSurface{};
            controls_out.work_mem_bytes =
                std::max<uint64_t>(64 * 1024, base_params.work_mem_bytes);

            std::string value;
            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"WORK_MEM",
                                           "OPTIMIZER.WORK_MEM",
                                           "OPTIMIZER_WORK_MEM"}))
            {
                uint64_t parsed = 0;
                if (!parsePlannerSizeBytes(value, parsed))
                {
                    error_out = "Invalid optimizer work_mem value: " + value;
                    return false;
                }
                controls_out.work_mem_bytes = std::max<uint64_t>(64 * 1024, parsed);
                upsertPlannerControl(controls_out.runtime_controls,
                                     "WORK_MEM",
                                     std::to_string(controls_out.work_mem_bytes),
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"OPTIMIZER.SPILL_POLICY",
                                           "OPTIMIZER_SPILL_POLICY",
                                           "SPILL_POLICY"}))
            {
                const std::string upper =
                    core::IdentifierUtils::toUpper(trimWhitespace(value));
                if (upper == "DISALLOW" || upper == "DENY" || upper == "NO_SPILL" ||
                    upper == "FAIL")
                {
                    controls_out.spill_policy = PlannerSpillPolicy::DISALLOW;
                }
                else if (upper == "ALLOW" || upper.empty())
                {
                    controls_out.spill_policy = PlannerSpillPolicy::ALLOW;
                }
                else
                {
                    error_out = "Invalid optimizer spill policy: " + value;
                    return false;
                }
                upsertPlannerControl(controls_out.runtime_controls,
                                     "SPILL_POLICY",
                                     plannerSpillPolicyName(controls_out.spill_policy),
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"OPTIMIZER.JOIN_SEARCH",
                                           "OPTIMIZER_JOIN_SEARCH",
                                           "JOIN_SEARCH"}))
            {
                if (!parsePlannerJoinSearchMode(value, controls_out.join_search))
                {
                    error_out = "Invalid optimizer join search mode: " + value;
                    return false;
                }
                upsertPlannerControl(controls_out.runtime_controls,
                                     "JOIN_SEARCH",
                                     plannerJoinSearchName(controls_out.join_search),
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"OPTIMIZER.SEARCH_DEPTH",
                                           "OPTIMIZER_SEARCH_DEPTH",
                                           "SEARCH_DEPTH"}))
            {
                if (!parsePlannerSearchDepth(value, controls_out.search_depth))
                {
                    error_out = "Invalid optimizer search depth: " + value;
                    return false;
                }
                upsertPlannerControl(controls_out.runtime_controls,
                                     "SEARCH_DEPTH",
                                     std::to_string(controls_out.search_depth),
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"OPTIMIZER.EXHAUSTIVE_JOIN_LIMIT",
                                           "OPTIMIZER_EXHAUSTIVE_JOIN_LIMIT",
                                           "EXHAUSTIVE_JOIN_LIMIT"}))
            {
                if (!parsePlannerSearchDepth(value,
                                             controls_out.exhaustive_join_limit))
                {
                    error_out =
                        "Invalid optimizer exhaustive join limit: " + value;
                    return false;
                }
                controls_out.exhaustive_join_limit =
                    std::max<size_t>(1, controls_out.exhaustive_join_limit);
                upsertPlannerControl(controls_out.runtime_controls,
                                     "EXHAUSTIVE_JOIN_LIMIT",
                                     std::to_string(
                                         controls_out.exhaustive_join_limit),
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"OPTIMIZER.BOUNDED_DP_JOIN_LIMIT",
                                           "OPTIMIZER_BOUNDED_DP_JOIN_LIMIT",
                                           "BOUNDED_DP_JOIN_LIMIT"}))
            {
                if (!parsePlannerSearchDepth(value,
                                             controls_out.bounded_dp_join_limit))
                {
                    error_out =
                        "Invalid optimizer bounded DP join limit: " + value;
                    return false;
                }
                controls_out.bounded_dp_join_limit = std::max<size_t>(
                    controls_out.exhaustive_join_limit,
                    std::max<size_t>(1, controls_out.bounded_dp_join_limit));
                upsertPlannerControl(controls_out.runtime_controls,
                                     "BOUNDED_DP_JOIN_LIMIT",
                                     std::to_string(
                                         controls_out.bounded_dp_join_limit),
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"OPTIMIZER.MAX_STATES_CONSIDERED",
                                           "OPTIMIZER_MAX_STATES_CONSIDERED",
                                           "MAX_STATES_CONSIDERED"}))
            {
                if (!parsePlannerSearchDepth(value,
                                             controls_out.max_states_considered))
                {
                    error_out =
                        "Invalid optimizer max states considered: " + value;
                    return false;
                }
                controls_out.max_states_considered =
                    std::max<size_t>(1, controls_out.max_states_considered);
                upsertPlannerControl(controls_out.runtime_controls,
                                     "MAX_STATES_CONSIDERED",
                                     std::to_string(
                                         controls_out.max_states_considered),
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"OPTIMIZER.FALLBACK_PRUNE_LEVEL",
                                           "OPTIMIZER_FALLBACK_PRUNE_LEVEL",
                                           "FALLBACK_PRUNE_LEVEL"}))
            {
                if (!parsePlannerSearchDepth(value,
                                             controls_out.fallback_prune_level))
                {
                    error_out =
                        "Invalid optimizer fallback prune level: " + value;
                    return false;
                }
                upsertPlannerControl(controls_out.runtime_controls,
                                     "FALLBACK_PRUNE_LEVEL",
                                     std::to_string(
                                         controls_out.fallback_prune_level),
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"OPTIMIZER.JOIN_METHOD",
                                           "OPTIMIZER_JOIN_METHOD",
                                           "JOIN_METHOD"}))
            {
                if (!parsePlannerJoinMethodControl(value, controls_out.join_method))
                {
                    error_out = "Invalid optimizer join method control: " + value;
                    return false;
                }
                upsertPlannerControl(controls_out.runtime_controls,
                                     "JOIN_METHOD",
                                     plannerJoinMethodName(controls_out.join_method),
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"ENABLE_PARALLEL",
                                           "enable_parallel"}))
            {
                if (!parsePlannerBoolSetting(value, controls_out.enable_parallel))
                {
                    error_out = "Invalid ENABLE_PARALLEL value: " + value;
                    return false;
                }
                upsertPlannerControl(controls_out.runtime_controls,
                                     "ENABLE_PARALLEL",
                                     controls_out.enable_parallel ? "ON" : "OFF",
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"ENABLE_PARALLEL_SCAN",
                                           "enable_parallel_scan"}))
            {
                if (!parsePlannerBoolSetting(value,
                                             controls_out.enable_parallel_scan))
                {
                    error_out = "Invalid ENABLE_PARALLEL_SCAN value: " + value;
                    return false;
                }
                upsertPlannerControl(controls_out.runtime_controls,
                                     "ENABLE_PARALLEL_SCAN",
                                     controls_out.enable_parallel_scan ? "ON"
                                                                       : "OFF",
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"ENABLE_PARALLEL_HASH",
                                           "enable_parallel_hash"}))
            {
                if (!parsePlannerBoolSetting(value,
                                             controls_out.enable_parallel_hash))
                {
                    error_out = "Invalid ENABLE_PARALLEL_HASH value: " + value;
                    return false;
                }
                upsertPlannerControl(controls_out.runtime_controls,
                                     "ENABLE_PARALLEL_HASH",
                                     controls_out.enable_parallel_hash ? "ON"
                                                                       : "OFF",
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"ENABLE_PARALLEL_AGGREGATE",
                                           "enable_parallel_aggregate"}))
            {
                if (!parsePlannerBoolSetting(
                        value, controls_out.enable_parallel_aggregate))
                {
                    error_out =
                        "Invalid ENABLE_PARALLEL_AGGREGATE value: " + value;
                    return false;
                }
                upsertPlannerControl(controls_out.runtime_controls,
                                     "ENABLE_PARALLEL_AGGREGATE",
                                     controls_out.enable_parallel_aggregate
                                         ? "ON"
                                         : "OFF",
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"ENABLE_PARALLEL_JOIN",
                                           "enable_parallel_join"}))
            {
                if (!parsePlannerBoolSetting(value,
                                             controls_out.enable_parallel_join))
                {
                    error_out = "Invalid ENABLE_PARALLEL_JOIN value: " + value;
                    return false;
                }
                upsertPlannerControl(controls_out.runtime_controls,
                                     "ENABLE_PARALLEL_JOIN",
                                     controls_out.enable_parallel_join ? "ON"
                                                                       : "OFF",
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"PARALLEL_LEADER_PARTICIPATION",
                                           "parallel_leader_participation"}))
            {
                if (!parsePlannerBoolSetting(
                        value, controls_out.parallel_leader_participation))
                {
                    error_out =
                        "Invalid PARALLEL_LEADER_PARTICIPATION value: " + value;
                    return false;
                }
                upsertPlannerControl(
                    controls_out.runtime_controls,
                    "PARALLEL_LEADER_PARTICIPATION",
                    controls_out.parallel_leader_participation ? "ON" : "OFF",
                    "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"MAX_PARALLEL_WORKERS",
                                           "max_parallel_workers"}))
            {
                if (!parsePlannerUint32Setting(value,
                                               controls_out.max_parallel_workers))
                {
                    error_out = "Invalid MAX_PARALLEL_WORKERS value: " + value;
                    return false;
                }
                upsertPlannerControl(controls_out.runtime_controls,
                                     "MAX_PARALLEL_WORKERS",
                                     std::to_string(
                                         controls_out.max_parallel_workers),
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"MAX_PARALLEL_WORKERS_PER_GATHER",
                                           "max_parallel_workers_per_gather"}))
            {
                if (!parsePlannerUint32Setting(
                        value, controls_out.max_parallel_workers_per_gather))
                {
                    error_out =
                        "Invalid MAX_PARALLEL_WORKERS_PER_GATHER value: " +
                        value;
                    return false;
                }
                upsertPlannerControl(
                    controls_out.runtime_controls,
                    "MAX_PARALLEL_WORKERS_PER_GATHER",
                    std::to_string(
                        controls_out.max_parallel_workers_per_gather),
                    "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"MIN_PARALLEL_ROWS_PER_WORKER",
                                           "min_parallel_rows_per_worker"}))
            {
                if (!parsePlannerUint32Setting(
                        value, controls_out.min_parallel_rows_per_worker))
                {
                    error_out =
                        "Invalid MIN_PARALLEL_ROWS_PER_WORKER value: " +
                        value;
                    return false;
                }
                upsertPlannerControl(
                    controls_out.runtime_controls,
                    "MIN_PARALLEL_ROWS_PER_WORKER",
                    std::to_string(
                        controls_out.min_parallel_rows_per_worker),
                    "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"MIN_PARALLEL_TABLE_SCAN_SIZE",
                                           "min_parallel_table_scan_size"}))
            {
                if (!parsePlannerUint32Setting(
                        value, controls_out.min_parallel_table_scan_size))
                {
                    error_out =
                        "Invalid MIN_PARALLEL_TABLE_SCAN_SIZE value: " + value;
                    return false;
                }
                upsertPlannerControl(
                    controls_out.runtime_controls,
                    "MIN_PARALLEL_TABLE_SCAN_SIZE",
                    std::to_string(
                        controls_out.min_parallel_table_scan_size),
                    "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"PARALLEL_SETUP_COST",
                                           "parallel_setup_cost"}))
            {
                if (!parsePlannerDoubleSetting(
                        value, controls_out.parallel_setup_cost))
                {
                    error_out = "Invalid PARALLEL_SETUP_COST value: " + value;
                    return false;
                }
                upsertPlannerControl(controls_out.runtime_controls,
                                     "PARALLEL_SETUP_COST",
                                     std::to_string(
                                         controls_out.parallel_setup_cost),
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"PARALLEL_TUPLE_COST",
                                           "parallel_tuple_cost"}))
            {
                if (!parsePlannerDoubleSetting(
                        value, controls_out.parallel_tuple_cost))
                {
                    error_out = "Invalid PARALLEL_TUPLE_COST value: " + value;
                    return false;
                }
                upsertPlannerControl(controls_out.runtime_controls,
                                     "PARALLEL_TUPLE_COST",
                                     std::to_string(
                                         controls_out.parallel_tuple_cost),
                                     "SESSION");
            }

            if (readPlannerSessionSetting(conn_ctx,
                                          value,
                                          {"OPTIMIZER.PLAN_DIRECTIVES",
                                           "OPTIMIZER_PLAN_DIRECTIVES",
                                           "PLAN_DIRECTIVES"}))
            {
                for (const auto &assignment : splitPlannerDirectiveAssignments(value))
                {
                    const size_t equals = assignment.find('=');
                    if (equals == std::string::npos || equals == 0 ||
                        equals + 1 >= assignment.size())
                    {
                        error_out = "Malformed optimizer directive: " + assignment;
                        return false;
                    }

                    const std::string key = core::IdentifierUtils::toUpper(
                        trimWhitespace(assignment.substr(0, equals)));
                    const std::string raw_value =
                        trimWhitespace(assignment.substr(equals + 1));

                    if (key == "WORK_MEM")
                    {
                        uint64_t parsed = 0;
                        if (!parsePlannerSizeBytes(raw_value, parsed))
                        {
                            error_out =
                                "Invalid optimizer directive WORK_MEM: " + raw_value;
                            return false;
                        }
                        controls_out.work_mem_bytes =
                            std::max<uint64_t>(64 * 1024, parsed);
                        upsertPlannerControl(controls_out.runtime_controls,
                                             "WORK_MEM",
                                             std::to_string(controls_out.work_mem_bytes),
                                             "DIRECTIVE");
                    }
                    else if (key == "SPILL_POLICY")
                    {
                        const std::string upper =
                            core::IdentifierUtils::toUpper(trimWhitespace(raw_value));
                        if (upper == "DISALLOW" || upper == "DENY" ||
                            upper == "NO_SPILL" || upper == "FAIL")
                        {
                            controls_out.spill_policy = PlannerSpillPolicy::DISALLOW;
                        }
                        else if (upper == "ALLOW" || upper.empty())
                        {
                            controls_out.spill_policy = PlannerSpillPolicy::ALLOW;
                        }
                        else
                        {
                            error_out =
                                "Invalid optimizer directive SPILL_POLICY: " + raw_value;
                            return false;
                        }
                        upsertPlannerControl(controls_out.runtime_controls,
                                             "SPILL_POLICY",
                                             plannerSpillPolicyName(
                                                 controls_out.spill_policy),
                                             "DIRECTIVE");
                    }
                    else if (key == "JOIN_SEARCH")
                    {
                        if (!parsePlannerJoinSearchMode(raw_value,
                                                        controls_out.join_search))
                        {
                            error_out =
                                "Invalid optimizer directive JOIN_SEARCH: " + raw_value;
                            return false;
                        }
                        upsertPlannerControl(controls_out.runtime_controls,
                                             "JOIN_SEARCH",
                                             plannerJoinSearchName(
                                                 controls_out.join_search),
                                             "DIRECTIVE");
                    }
                    else if (key == "SEARCH_DEPTH")
                    {
                        if (!parsePlannerSearchDepth(raw_value,
                                                     controls_out.search_depth))
                        {
                            error_out =
                                "Invalid optimizer directive SEARCH_DEPTH: " + raw_value;
                            return false;
                        }
                        upsertPlannerControl(controls_out.runtime_controls,
                                             "SEARCH_DEPTH",
                                             std::to_string(controls_out.search_depth),
                                             "DIRECTIVE");
                    }
                    else if (key == "EXHAUSTIVE_JOIN_LIMIT")
                    {
                        if (!parsePlannerSearchDepth(
                                raw_value, controls_out.exhaustive_join_limit))
                        {
                            error_out =
                                "Invalid optimizer directive EXHAUSTIVE_JOIN_LIMIT: " +
                                raw_value;
                            return false;
                        }
                        controls_out.exhaustive_join_limit = std::max<size_t>(
                            1, controls_out.exhaustive_join_limit);
                        upsertPlannerControl(controls_out.runtime_controls,
                                             "EXHAUSTIVE_JOIN_LIMIT",
                                             std::to_string(
                                                 controls_out.exhaustive_join_limit),
                                             "DIRECTIVE");
                    }
                    else if (key == "BOUNDED_DP_JOIN_LIMIT")
                    {
                        if (!parsePlannerSearchDepth(
                                raw_value, controls_out.bounded_dp_join_limit))
                        {
                            error_out =
                                "Invalid optimizer directive BOUNDED_DP_JOIN_LIMIT: " +
                                raw_value;
                            return false;
                        }
                        controls_out.bounded_dp_join_limit = std::max<size_t>(
                            controls_out.exhaustive_join_limit,
                            std::max<size_t>(1,
                                             controls_out.bounded_dp_join_limit));
                        upsertPlannerControl(controls_out.runtime_controls,
                                             "BOUNDED_DP_JOIN_LIMIT",
                                             std::to_string(
                                                 controls_out.bounded_dp_join_limit),
                                             "DIRECTIVE");
                    }
                    else if (key == "MAX_STATES_CONSIDERED")
                    {
                        if (!parsePlannerSearchDepth(
                                raw_value, controls_out.max_states_considered))
                        {
                            error_out =
                                "Invalid optimizer directive MAX_STATES_CONSIDERED: " +
                                raw_value;
                            return false;
                        }
                        controls_out.max_states_considered =
                            std::max<size_t>(1,
                                             controls_out.max_states_considered);
                        upsertPlannerControl(controls_out.runtime_controls,
                                             "MAX_STATES_CONSIDERED",
                                             std::to_string(
                                                 controls_out.max_states_considered),
                                             "DIRECTIVE");
                    }
                    else if (key == "FALLBACK_PRUNE_LEVEL")
                    {
                        if (!parsePlannerSearchDepth(
                                raw_value, controls_out.fallback_prune_level))
                        {
                            error_out =
                                "Invalid optimizer directive FALLBACK_PRUNE_LEVEL: " +
                                raw_value;
                            return false;
                        }
                        upsertPlannerControl(controls_out.runtime_controls,
                                             "FALLBACK_PRUNE_LEVEL",
                                             std::to_string(
                                                 controls_out.fallback_prune_level),
                                             "DIRECTIVE");
                    }
                    else if (key == "JOIN_METHOD")
                    {
                        if (!parsePlannerJoinMethodControl(raw_value,
                                                           controls_out.join_method))
                        {
                            error_out =
                                "Invalid optimizer directive JOIN_METHOD: " + raw_value;
                            return false;
                        }
                        upsertPlannerControl(controls_out.runtime_controls,
                                             "JOIN_METHOD",
                                             plannerJoinMethodName(
                                                 controls_out.join_method),
                                             "DIRECTIVE");
                    }
                    else if (key == "ENABLE_PARALLEL")
                    {
                        if (!parsePlannerBoolSetting(raw_value,
                                                     controls_out.enable_parallel))
                        {
                            error_out =
                                "Invalid optimizer directive ENABLE_PARALLEL: " +
                                raw_value;
                            return false;
                        }
                        upsertPlannerControl(controls_out.runtime_controls,
                                             "ENABLE_PARALLEL",
                                             controls_out.enable_parallel
                                                 ? "ON"
                                                 : "OFF",
                                             "DIRECTIVE");
                    }
                    else if (key == "ENABLE_PARALLEL_SCAN")
                    {
                        if (!parsePlannerBoolSetting(
                                raw_value, controls_out.enable_parallel_scan))
                        {
                            error_out =
                                "Invalid optimizer directive ENABLE_PARALLEL_SCAN: " +
                                raw_value;
                            return false;
                        }
                        upsertPlannerControl(
                            controls_out.runtime_controls,
                            "ENABLE_PARALLEL_SCAN",
                            controls_out.enable_parallel_scan ? "ON" : "OFF",
                            "DIRECTIVE");
                    }
                    else if (key == "ENABLE_PARALLEL_HASH")
                    {
                        if (!parsePlannerBoolSetting(
                                raw_value, controls_out.enable_parallel_hash))
                        {
                            error_out =
                                "Invalid optimizer directive ENABLE_PARALLEL_HASH: " +
                                raw_value;
                            return false;
                        }
                        upsertPlannerControl(
                            controls_out.runtime_controls,
                            "ENABLE_PARALLEL_HASH",
                            controls_out.enable_parallel_hash ? "ON" : "OFF",
                            "DIRECTIVE");
                    }
                    else if (key == "ENABLE_PARALLEL_AGGREGATE")
                    {
                        if (!parsePlannerBoolSetting(
                                raw_value,
                                controls_out.enable_parallel_aggregate))
                        {
                            error_out =
                                "Invalid optimizer directive ENABLE_PARALLEL_AGGREGATE: " +
                                raw_value;
                            return false;
                        }
                        upsertPlannerControl(
                            controls_out.runtime_controls,
                            "ENABLE_PARALLEL_AGGREGATE",
                            controls_out.enable_parallel_aggregate ? "ON"
                                                                   : "OFF",
                            "DIRECTIVE");
                    }
                    else if (key == "ENABLE_PARALLEL_JOIN")
                    {
                        if (!parsePlannerBoolSetting(
                                raw_value, controls_out.enable_parallel_join))
                        {
                            error_out =
                                "Invalid optimizer directive ENABLE_PARALLEL_JOIN: " +
                                raw_value;
                            return false;
                        }
                        upsertPlannerControl(
                            controls_out.runtime_controls,
                            "ENABLE_PARALLEL_JOIN",
                            controls_out.enable_parallel_join ? "ON" : "OFF",
                            "DIRECTIVE");
                    }
                    else if (key == "PARALLEL_LEADER_PARTICIPATION")
                    {
                        if (!parsePlannerBoolSetting(
                                raw_value,
                                controls_out.parallel_leader_participation))
                        {
                            error_out =
                                "Invalid optimizer directive PARALLEL_LEADER_PARTICIPATION: " +
                                raw_value;
                            return false;
                        }
                        upsertPlannerControl(
                            controls_out.runtime_controls,
                            "PARALLEL_LEADER_PARTICIPATION",
                            controls_out.parallel_leader_participation
                                ? "ON"
                                : "OFF",
                            "DIRECTIVE");
                    }
                    else if (key == "MAX_PARALLEL_WORKERS")
                    {
                        if (!parsePlannerUint32Setting(
                                raw_value, controls_out.max_parallel_workers))
                        {
                            error_out =
                                "Invalid optimizer directive MAX_PARALLEL_WORKERS: " +
                                raw_value;
                            return false;
                        }
                        upsertPlannerControl(controls_out.runtime_controls,
                                             "MAX_PARALLEL_WORKERS",
                                             std::to_string(
                                                 controls_out.max_parallel_workers),
                                             "DIRECTIVE");
                    }
                    else if (key == "MAX_PARALLEL_WORKERS_PER_GATHER")
                    {
                        if (!parsePlannerUint32Setting(
                                raw_value,
                                controls_out.max_parallel_workers_per_gather))
                        {
                            error_out =
                                "Invalid optimizer directive MAX_PARALLEL_WORKERS_PER_GATHER: " +
                                raw_value;
                            return false;
                        }
                        upsertPlannerControl(
                            controls_out.runtime_controls,
                            "MAX_PARALLEL_WORKERS_PER_GATHER",
                            std::to_string(
                                controls_out.max_parallel_workers_per_gather),
                            "DIRECTIVE");
                    }
                    else if (key == "MIN_PARALLEL_TABLE_SCAN_SIZE")
                    {
                        if (!parsePlannerUint32Setting(
                                raw_value,
                                controls_out.min_parallel_table_scan_size))
                        {
                            error_out =
                                "Invalid optimizer directive MIN_PARALLEL_TABLE_SCAN_SIZE: " +
                                raw_value;
                            return false;
                        }
                        upsertPlannerControl(
                            controls_out.runtime_controls,
                            "MIN_PARALLEL_TABLE_SCAN_SIZE",
                            std::to_string(
                                controls_out.min_parallel_table_scan_size),
                            "DIRECTIVE");
                    }
                    else if (key == "MIN_PARALLEL_ROWS_PER_WORKER")
                    {
                        if (!parsePlannerUint32Setting(
                                raw_value,
                                controls_out.min_parallel_rows_per_worker))
                        {
                            error_out =
                                "Invalid optimizer directive MIN_PARALLEL_ROWS_PER_WORKER: " +
                                raw_value;
                            return false;
                        }
                        upsertPlannerControl(
                            controls_out.runtime_controls,
                            "MIN_PARALLEL_ROWS_PER_WORKER",
                            std::to_string(
                                controls_out.min_parallel_rows_per_worker),
                            "DIRECTIVE");
                    }
                    else if (key == "PARALLEL_SETUP_COST")
                    {
                        if (!parsePlannerDoubleSetting(
                                raw_value, controls_out.parallel_setup_cost))
                        {
                            error_out =
                                "Invalid optimizer directive PARALLEL_SETUP_COST: " +
                                raw_value;
                            return false;
                        }
                        upsertPlannerControl(controls_out.runtime_controls,
                                             "PARALLEL_SETUP_COST",
                                             std::to_string(
                                                 controls_out.parallel_setup_cost),
                                             "DIRECTIVE");
                    }
                    else if (key == "PARALLEL_TUPLE_COST")
                    {
                        if (!parsePlannerDoubleSetting(
                                raw_value, controls_out.parallel_tuple_cost))
                        {
                            error_out =
                                "Invalid optimizer directive PARALLEL_TUPLE_COST: " +
                                raw_value;
                            return false;
                        }
                        upsertPlannerControl(controls_out.runtime_controls,
                                             "PARALLEL_TUPLE_COST",
                                             std::to_string(
                                                 controls_out.parallel_tuple_cost),
                                             "DIRECTIVE");
                    }
                    else if (key != "PLAN_PROFILE")
                    {
                        error_out = "Unsupported optimizer directive: " + key;
                        return false;
                    }
                }
            }

            controls_out.exhaustive_join_limit =
                std::max<size_t>(1, controls_out.exhaustive_join_limit);
            controls_out.bounded_dp_join_limit = std::max<size_t>(
                controls_out.exhaustive_join_limit,
                std::max<size_t>(1, controls_out.bounded_dp_join_limit));
            controls_out.max_states_considered =
                std::max<size_t>(1, controls_out.max_states_considered);

            ensurePlannerControlDefault(controls_out.runtime_controls,
                                        "WORK_MEM",
                                        std::to_string(controls_out.work_mem_bytes));
            ensurePlannerControlDefault(controls_out.runtime_controls,
                                        "SPILL_POLICY",
                                        plannerSpillPolicyName(
                                            controls_out.spill_policy));
            ensurePlannerControlDefault(controls_out.runtime_controls,
                                        "JOIN_SEARCH",
                                        plannerJoinSearchName(
                                            controls_out.join_search));
            ensurePlannerControlDefault(controls_out.runtime_controls,
                                        "SEARCH_DEPTH",
                                        std::to_string(controls_out.search_depth));
            ensurePlannerControlDefault(controls_out.runtime_controls,
                                        "EXHAUSTIVE_JOIN_LIMIT",
                                        std::to_string(
                                            controls_out.exhaustive_join_limit));
            ensurePlannerControlDefault(controls_out.runtime_controls,
                                        "BOUNDED_DP_JOIN_LIMIT",
                                        std::to_string(
                                            controls_out.bounded_dp_join_limit));
            ensurePlannerControlDefault(controls_out.runtime_controls,
                                        "MAX_STATES_CONSIDERED",
                                        std::to_string(
                                            controls_out.max_states_considered));
            ensurePlannerControlDefault(controls_out.runtime_controls,
                                        "FALLBACK_PRUNE_LEVEL",
                                        std::to_string(
                                            controls_out.fallback_prune_level));
            ensurePlannerControlDefault(controls_out.runtime_controls,
                                        "JOIN_METHOD",
                                        plannerJoinMethodName(
                                            controls_out.join_method));
            ensurePlannerControlDefault(controls_out.runtime_controls,
                                        "ENABLE_PARALLEL",
                                        controls_out.enable_parallel ? "ON"
                                                                     : "OFF");
            ensurePlannerControlDefault(
                controls_out.runtime_controls,
                "ENABLE_PARALLEL_SCAN",
                controls_out.enable_parallel_scan ? "ON" : "OFF");
            ensurePlannerControlDefault(
                controls_out.runtime_controls,
                "ENABLE_PARALLEL_HASH",
                controls_out.enable_parallel_hash ? "ON" : "OFF");
            ensurePlannerControlDefault(
                controls_out.runtime_controls,
                "ENABLE_PARALLEL_AGGREGATE",
                controls_out.enable_parallel_aggregate ? "ON" : "OFF");
            ensurePlannerControlDefault(
                controls_out.runtime_controls,
                "ENABLE_PARALLEL_JOIN",
                controls_out.enable_parallel_join ? "ON" : "OFF");
            ensurePlannerControlDefault(
                controls_out.runtime_controls,
                "PARALLEL_LEADER_PARTICIPATION",
                controls_out.parallel_leader_participation ? "ON" : "OFF");
            ensurePlannerControlDefault(
                controls_out.runtime_controls,
                "MAX_PARALLEL_WORKERS",
                std::to_string(controls_out.max_parallel_workers));
            ensurePlannerControlDefault(
                controls_out.runtime_controls,
                "MAX_PARALLEL_WORKERS_PER_GATHER",
                std::to_string(controls_out.max_parallel_workers_per_gather));
            ensurePlannerControlDefault(
                controls_out.runtime_controls,
                "MIN_PARALLEL_ROWS_PER_WORKER",
                std::to_string(controls_out.min_parallel_rows_per_worker));
            ensurePlannerControlDefault(
                controls_out.runtime_controls,
                "MIN_PARALLEL_TABLE_SCAN_SIZE",
                std::to_string(controls_out.min_parallel_table_scan_size));
            ensurePlannerControlDefault(
                controls_out.runtime_controls,
                "PARALLEL_SETUP_COST",
                std::to_string(controls_out.parallel_setup_cost));
            ensurePlannerControlDefault(
                controls_out.runtime_controls,
                "PARALLEL_TUPLE_COST",
                std::to_string(controls_out.parallel_tuple_cost));
            return true;
        }

        auto planCostEstimate(const std::shared_ptr<PlanNode> &plan) -> CostEstimate
        {
            if (!plan)
            {
                return CostEstimate{};
            }
            if (plan->costEvidence().has_value())
            {
                return *plan->costEvidence();
            }
            const double startup = plan->startupCost();
            const double total = plan->totalCost();
            return CostEstimate(startup,
                                total >= startup ? (total - startup) : 0.0,
                                plan->rows());
        }

        auto derivedJoinSelectivity(parser::JoinType join_type,
                                    uint64_t outer_rows,
                                    uint64_t inner_rows,
                                    uint64_t output_rows) -> double
        {
            if (join_type == parser::JoinType::CROSS)
            {
                return 1.0;
            }
            if (outer_rows == 0 || inner_rows == 0)
            {
                return 0.0;
            }
            const double denominator =
                static_cast<double>(outer_rows) * static_cast<double>(inner_rows);
            return std::clamp(static_cast<double>(output_rows) / denominator,
                              0.0,
                              1.0);
        }

        auto parseTopNCount(const std::string &detail_text,
                            uint64_t &top_n_out) -> bool
        {
            const std::string key = "topn=";
            const size_t pos = detail_text.find(key);
            if (pos == std::string::npos)
            {
                return false;
            }
            size_t cursor = pos + key.size();
            size_t end = cursor;
            while (end < detail_text.size() &&
                   std::isdigit(static_cast<unsigned char>(detail_text[end])))
            {
                ++end;
            }
            if (end == cursor)
            {
                return false;
            }
            try
            {
                top_n_out = static_cast<uint64_t>(
                    std::stoull(detail_text.substr(cursor, end - cursor)));
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        auto applyResourceMetadata(RuntimePlanNode &node,
                                   const CostEstimate &cost,
                                   const std::string &spill_policy_text) -> void
        {
            node.estimated_memory_bytes = cost.memory_bytes;
            node.memory_budget_bytes = cost.memory_budget_bytes;
            node.spill_expected = cost.spill_expected;
            node.spill_passes = cost.spill_passes;
            node.spill_bytes = cost.spill_bytes;
            node.spill_policy = spill_policy_text;
            node.formula_profile_id = cost.formula_profile_id;
            node.formula_profile_version = cost.formula_profile_version;
            node.calibration_profile_id = cost.calibration_profile_id;
            node.storage_profile = cost.storage_profile;
            node.workload_profile = cost.workload_profile;
            node.resource_governance_outcome = cost.resource_governance_outcome;
            node.input_estimates.clear();
            node.expanded_cost_terms.clear();
            for (const auto &input : cost.input_estimates)
            {
                node.input_estimates.push_back(RuntimePlanCostInputEstimate{
                    input.name,
                    input.value,
                    input.unit});
            }
            for (const auto &term : cost.expanded_terms)
            {
                node.expanded_cost_terms.push_back(RuntimePlanCostTerm{
                    term.name,
                    term.coefficient,
                    term.input_value,
                    term.contribution,
                    term.unit});
            }
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

        auto pathLikelyProvidesJoinOrder(const std::shared_ptr<Path> &path) -> bool
        {
            if (!path)
            {
                return false;
            }

            if (path->accessDescriptor().order_complete ||
                path->accessDescriptor().ordered_output)
            {
                return true;
            }

            switch (path->type())
            {
                case PathType::INDEX_SCAN:
                case PathType::INDEX_ONLY_SCAN:
                case PathType::SORT:
                case PathType::GATHER_MERGE:
                case PathType::MERGE_JOIN:
                    return true;
                default:
                    return false;
            }
        }

        auto appendRuntimeTrace(std::vector<RuntimePlanTraceEntry> &entries,
                                const std::string &phase,
                                const std::string &subject,
                                const std::string &candidate,
                                const std::string &verdict,
                                const std::string &reason,
                                double startup_cost,
                                double total_cost,
                                uint64_t estimated_rows) -> void
        {
            RuntimePlanTraceEntry entry;
            entry.phase = phase;
            entry.subject = subject;
            entry.candidate = candidate;
            entry.verdict = verdict;
            entry.reason = reason;
            entry.startup_cost = startup_cost;
            entry.total_cost = total_cost;
            entry.estimated_rows = estimated_rows;
            entries.push_back(std::move(entry));
        }

        auto appendStatsProvenance(
            std::vector<RuntimePlanStatisticsProvenance> &entries,
            const std::string &subject,
            const std::string &source,
            const std::string &detail) -> void
        {
            auto already_present = std::find_if(
                entries.begin(),
                entries.end(),
                [&](const RuntimePlanStatisticsProvenance &existing) {
                    return existing.subject == subject &&
                           existing.source == source &&
                           existing.detail == detail;
                });
            if (already_present != entries.end())
            {
                return;
            }

            RuntimePlanStatisticsProvenance entry;
            entry.subject = subject;
            entry.source = source;
            entry.detail = detail;
            entries.push_back(std::move(entry));
        }

        auto appendColumnStatsProvenance =
            [](std::vector<RuntimePlanStatisticsProvenance> &entries,
               const std::string &subject,
               const std::string &source,
               const std::string &detail,
               const ColumnStatistics &stats) -> void {
                RuntimePlanStatisticsProvenance entry;
                entry.subject = subject;
                entry.source = source;
                entry.detail = detail;
                entry.stats_snapshot_id = stats.stats_snapshot_id;
                entry.last_analyzed_time = stats.last_analyzed_time;
                entry.sample_ratio = static_cast<double>(stats.sample_rate);
                entry.modified_rows_since_analyze =
                    stats.modified_rows_since_analyze;
                entry.staleness_class =
                    statisticsStalenessClassName(stats.staleness_class);
                entry.confidence_class =
                    statisticsConfidenceClassName(stats.confidence_class);
                entry.auto_analyze_applied = stats.auto_analyze_applied;
                entry.auto_analyze_threshold = stats.auto_analyze_threshold;
                auto already_present = std::find_if(
                    entries.begin(),
                    entries.end(),
                    [&](const RuntimePlanStatisticsProvenance &existing) {
                        return existing.subject == entry.subject &&
                               existing.source == entry.source &&
                               existing.detail == entry.detail;
                    });
                if (already_present != entries.end())
                {
                    *already_present = entry;
                    return;
                }
                entries.push_back(std::move(entry));
            };

        auto appendTableStatsProvenance =
            [](std::vector<RuntimePlanStatisticsProvenance> &entries,
               const std::string &subject,
               const std::string &source,
               const std::string &detail,
               const TableStatistics &stats) -> void {
                RuntimePlanStatisticsProvenance entry;
                entry.subject = subject;
                entry.source = source;
                entry.detail = detail;
                entry.stats_snapshot_id = stats.stats_snapshot_id;
                entry.last_analyzed_time = stats.last_analyzed_time;
                entry.sample_ratio = 1.0;
                entry.modified_rows_since_analyze =
                    stats.modified_rows_since_analyze;
                entry.staleness_class =
                    statisticsStalenessClassName(stats.staleness_class);
                entry.confidence_class =
                    statisticsConfidenceClassName(stats.confidence_class);
                entry.auto_analyze_applied = stats.auto_analyze_applied;
                entry.auto_analyze_threshold = stats.auto_analyze_threshold;
                auto already_present = std::find_if(
                    entries.begin(),
                    entries.end(),
                    [&](const RuntimePlanStatisticsProvenance &existing) {
                        return existing.subject == entry.subject &&
                               existing.source == entry.source &&
                               existing.detail == entry.detail;
                    });
                if (already_present != entries.end())
                {
                    *already_present = entry;
                    return;
                }
                entries.push_back(std::move(entry));
            };

        auto statsPenaltyMultiplier = [](StatisticsStalenessClass staleness,
                                         StatisticsConfidenceClass confidence) -> double {
            double penalty = 1.0;
            switch (staleness)
            {
                case StatisticsStalenessClass::WARM:
                    penalty = 1.05;
                    break;
                case StatisticsStalenessClass::STALE:
                    penalty = 1.20;
                    break;
                case StatisticsStalenessClass::EXPIRED:
                    penalty = 1.40;
                    break;
                case StatisticsStalenessClass::FRESH:
                case StatisticsStalenessClass::UNKNOWN:
                default:
                    penalty = 1.0;
                    break;
            }
            if (confidence == StatisticsConfidenceClass::LOW)
            {
                penalty = std::max(penalty, 1.15);
            }
            else if (confidence == StatisticsConfidenceClass::MEDIUM)
            {
                penalty = std::max(penalty, 1.03);
            }
            return penalty;
        };

        auto statsPenaltyReason = [](const ColumnStatistics &stats,
                                     double penalty) -> std::string {
            std::ostringstream out;
            out << "staleness="
                << statisticsStalenessClassName(stats.staleness_class)
                << " confidence="
                << statisticsConfidenceClassName(stats.confidence_class)
                << " modified_rows_since_analyze="
                << stats.modified_rows_since_analyze
                << " sample_ratio=" << stats.sample_rate
                << " penalty=" << penalty;
            return out.str();
        };

        auto statsPenaltyReasonForTable = [](const TableStatistics &stats,
                                             double penalty) -> std::string {
            std::ostringstream out;
            out << "staleness="
                << statisticsStalenessClassName(stats.staleness_class)
                << " confidence="
                << statisticsConfidenceClassName(stats.confidence_class)
                << " modified_rows_since_analyze="
                << stats.modified_rows_since_analyze
                << " penalty=" << penalty;
            return out.str();
        };

        auto joinTraceSubject(const ResolvedSelectQuery &resolved,
                              const ResolvedJoin &join) -> std::string
        {
            const auto &left_relation = resolved.relations[join.left_relation_index];
            const auto &right_relation = resolved.relations[join.right_relation_index];
            return relationLookupName(left_relation) + "->" +
                   relationLookupName(right_relation);
        }

        auto accessTraceCandidateLabel(const std::string &scan_kind,
                                       const std::string &index_name,
                                       const std::string &bitmap_op) -> std::string
        {
            std::ostringstream out;
            out << scan_kind;
            if (!index_name.empty())
            {
                out << "[" << index_name << "]";
            }
            if (!bitmap_op.empty())
            {
                out << "(" << bitmap_op << ")";
            }
            return out.str();
        }

        auto predicateStatsSource(const ResolvedRelation &relation,
                                  const ResolvedScanPredicate &predicate,
                                  StatisticsManager *stats_manager,
                                  const ColumnStatistics *loaded_stats = nullptr)
            -> std::string
        {
            if (!relation.resolved || stats_manager == nullptr ||
                isZeroId(relation.table_info.table_id) ||
                isZeroId(predicate.column_id))
            {
                return "HEURISTIC_DEFAULT";
            }

            ColumnStatistics column_stats;
            if (loaded_stats != nullptr)
            {
                column_stats = *loaded_stats;
            }
            else
            {
                core::ErrorContext local_ctx;
                if (stats_manager->getColumnStatistics(relation.table_info.table_id,
                                                       predicate.column_id,
                                                       column_stats,
                                                       &local_ctx) != core::Status::OK)
                {
                    return "HEURISTIC_DEFAULT";
                }
            }
            if (column_stats.last_analyzed_time == 0)
            {
                return "HEURISTIC_DEFAULT";
            }

            std::vector<std::string> sources;
            const std::string op_upper =
                core::IdentifierUtils::toUpper(predicate.operator_name);
            if ((op_upper == "=" || op_upper == "IN") &&
                !column_stats.mcv_list.empty())
            {
                sources.push_back("MCV");
            }
            if ((op_upper == "=" || op_upper == "IN") &&
                column_stats.num_distinct > 0)
            {
                sources.push_back("NDISTINCT");
            }
            if ((op_upper == "<" || op_upper == "<=" ||
                 op_upper == ">" || op_upper == ">=" ||
                 op_upper == "BETWEEN" || op_upper == "LIKE") &&
                !column_stats.histogram_buckets.empty())
            {
                sources.push_back("HISTOGRAM");
            }
            if (sources.empty())
            {
                sources.push_back("COLUMN_STATS");
            }
            if (predicate.literal_kind == "PARAMETER")
            {
                sources.push_back("BOUND_PARAMETER");
            }

            std::ostringstream out;
            for (size_t i = 0; i < sources.size(); ++i)
            {
                if (i > 0)
                {
                    out << '+';
                }
                out << sources[i];
            }
            return out.str();
        }

        auto joinStatsSource(const ResolvedSelectQuery &resolved,
                             const ResolvedJoin &join,
                             StatisticsManager *stats_manager) -> std::string
        {
            if (!join.equi_join || !join.has_hash_column_ids || stats_manager == nullptr)
            {
                return "JOIN_HEURISTIC";
            }

            if (join.left_relation_index >= resolved.relations.size() ||
                join.right_relation_index >= resolved.relations.size())
            {
                return "JOIN_HEURISTIC";
            }

            const auto &left_relation = resolved.relations[join.left_relation_index];
            const auto &right_relation = resolved.relations[join.right_relation_index];
            if (!left_relation.resolved || !right_relation.resolved)
            {
                return "JOIN_HEURISTIC";
            }

            ColumnStatistics left_stats;
            ColumnStatistics right_stats;
            core::ErrorContext local_ctx;
            if (stats_manager->getColumnStatistics(left_relation.table_info.table_id,
                                                   join.left_hash_column_id,
                                                   left_stats,
                                                   &local_ctx) == core::Status::OK &&
                stats_manager->getColumnStatistics(right_relation.table_info.table_id,
                                                   join.right_hash_column_id,
                                                   right_stats,
                                                   &local_ctx) == core::Status::OK)
            {
                return "JOIN_COLUMN_STATS";
            }
            return "JOIN_HEURISTIC";
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
            node.parallel_aware = plan->parallelAware();
            node.parallel_enabled = plan->parallelEnabled();
            node.parallel_workers_planned = plan->workersPlanned();
            node.gather_merge = plan->gatherMerge();
            node.parallel_stage = plan->parallelStage();
            node.parallel_reason = plan->parallelReason();
            if (plan->costEvidence().has_value())
            {
                applyResourceMetadata(node, *plan->costEvidence(), std::string());
            }

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
                case PlanNodeType::MERGE_JOIN: {
                    auto join = std::dynamic_pointer_cast<MergeJoinNode>(plan);
                    node.node_type = "MergeJoin";
                    if (join)
                    {
                        node.join_type = planJoinTypeToString(join->joinType());
                        node.condition_text = join->joinCondString();
                        node.detail_text =
                            std::string(join->outerPresorted() ? "outer-presorted" : "sort-outer") +
                            "," +
                            std::string(join->innerPresorted() ? "inner-presorted" : "sort-inner");
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
                        if (!aggregate->groupingExprs().empty() &&
                            aggregate->aggregates().empty())
                        {
                            node.detail_text = "DISTINCT";
                        }
                        else
                        {
                            node.detail_text =
                                aggregate->isSimpleAggregation() ? "SIMPLE"
                                                                 : "GROUP";
                        }
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
                            std::to_string(sort->sortKeyCount()) + " keys";
                        node.children.push_back(toRuntimePlanNode(sort->childPlan()));
                    }
                    break;
                }
                case PlanNodeType::GATHER:
                case PlanNodeType::GATHER_MERGE: {
                    auto gather = std::dynamic_pointer_cast<GatherNode>(plan);
                    node.node_type =
                        plan->type() == PlanNodeType::GATHER_MERGE ? "GatherMerge"
                                                                   : "Gather";
                    if (gather)
                    {
                        std::ostringstream detail;
                        detail << "workers=" << gather->workersPlanned()
                               << " leader="
                               << (gather->leaderParticipates() ? "true"
                                                                : "false");
                        node.detail_text = detail.str();
                        node.gather_merge = gather->mergeOrdered();
                        node.children.push_back(
                            toRuntimePlanNode(gather->childPlan()));
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

        auto annotateRuntimePlanResources(RuntimePlanNode &node,
                                          const std::shared_ptr<PlanNode> &plan,
                                          CostModel &cost_model,
                                          uint64_t row_width,
                                          const std::string &spill_policy_text) -> void
        {
            if (!plan)
            {
                return;
            }

            CostEstimate resource_cost;
            bool have_resource_cost = false;

            switch (plan->type())
            {
                case PlanNodeType::NESTED_LOOP_JOIN: {
                    auto join = std::dynamic_pointer_cast<NestedLoopJoinNode>(plan);
                    if (join)
                    {
                        if (node.children.size() >= 1)
                        {
                            annotateRuntimePlanResources(node.children[0],
                                                         join->outerPlan(),
                                                         cost_model,
                                                         row_width,
                                                         spill_policy_text);
                        }
                        if (node.children.size() >= 2)
                        {
                            annotateRuntimePlanResources(node.children[1],
                                                         join->innerPlan(),
                                                         cost_model,
                                                         row_width,
                                                         spill_policy_text);
                        }
                    }
                    break;
                }
                case PlanNodeType::HASH_JOIN: {
                    auto join = std::dynamic_pointer_cast<HashJoinNode>(plan);
                    if (join)
                    {
                        if (node.children.size() >= 1)
                        {
                            annotateRuntimePlanResources(node.children[0],
                                                         join->outerPlan(),
                                                         cost_model,
                                                         row_width,
                                                         spill_policy_text);
                        }
                        if (node.children.size() >= 2)
                        {
                            annotateRuntimePlanResources(node.children[1],
                                                         join->innerPlan(),
                                                         cost_model,
                                                         row_width,
                                                         spill_policy_text);
                        }
                        const auto outer_cost = planCostEstimate(join->outerPlan());
                        const auto inner_cost = planCostEstimate(join->innerPlan());
                        resource_cost = cost_model.costHashJoin(
                            outer_cost,
                            inner_cost,
                            outer_cost.rows,
                            inner_cost.rows,
                            derivedJoinSelectivity(join->joinType(),
                                                   outer_cost.rows,
                                                   inner_cost.rows,
                                                   plan->rows()),
                            join->joinType(),
                            nullptr);
                        have_resource_cost = true;
                    }
                    break;
                }
                case PlanNodeType::MERGE_JOIN: {
                    auto join = std::dynamic_pointer_cast<MergeJoinNode>(plan);
                    if (join)
                    {
                        if (node.children.size() >= 1)
                        {
                            annotateRuntimePlanResources(node.children[0],
                                                         join->outerPlan(),
                                                         cost_model,
                                                         row_width,
                                                         spill_policy_text);
                        }
                        if (node.children.size() >= 2)
                        {
                            annotateRuntimePlanResources(node.children[1],
                                                         join->innerPlan(),
                                                         cost_model,
                                                         row_width,
                                                         spill_policy_text);
                        }
                        const auto outer_cost = planCostEstimate(join->outerPlan());
                        const auto inner_cost = planCostEstimate(join->innerPlan());
                        resource_cost = cost_model.costMergeJoin(
                            outer_cost,
                            inner_cost,
                            outer_cost.rows,
                            inner_cost.rows,
                            derivedJoinSelectivity(join->joinType(),
                                                   outer_cost.rows,
                                                   inner_cost.rows,
                                                   plan->rows()),
                            join->outerPresorted(),
                            join->innerPresorted(),
                            join->joinType(),
                            nullptr);
                        have_resource_cost = true;
                    }
                    break;
                }
                case PlanNodeType::AGGREGATE: {
                    auto aggregate = std::dynamic_pointer_cast<AggregateNode>(plan);
                    if (aggregate)
                    {
                        if (node.children.size() >= 1)
                        {
                            annotateRuntimePlanResources(node.children[0],
                                                         aggregate->childPlan(),
                                                         cost_model,
                                                         row_width,
                                                         spill_policy_text);
                        }
                        resource_cost = cost_model.costAggregate(
                            aggregate->childPlan() ? aggregate->childPlan()->rows() : plan->rows(),
                            std::max<uint64_t>(1, plan->rows()),
                            aggregate->aggregates().size(),
                            nullptr);
                        have_resource_cost = true;
                    }
                    break;
                }
                case PlanNodeType::SORT: {
                    auto sort = std::dynamic_pointer_cast<SortNode>(plan);
                    if (sort)
                    {
                        if (node.children.size() >= 1)
                        {
                            annotateRuntimePlanResources(node.children[0],
                                                         sort->childPlan(),
                                                         cost_model,
                                                         row_width,
                                                         spill_policy_text);
                        }
                        const uint64_t input_rows =
                            sort->childPlan() ? sort->childPlan()->rows() : plan->rows();
                        uint64_t top_n = 0;
                        if (parseTopNCount(node.detail_text, top_n))
                        {
                            resource_cost = cost_model.costSort(
                                input_rows,
                                row_width,
                                std::max<uint64_t>(1, sort->sortKeyCount()),
                                top_n,
                                nullptr);
                        }
                        else
                        {
                            resource_cost = cost_model.costSort(
                                input_rows,
                                row_width,
                                std::max<uint64_t>(1, sort->sortKeyCount()),
                                nullptr);
                        }
                        have_resource_cost = true;
                    }
                    break;
                }
                case PlanNodeType::GATHER:
                case PlanNodeType::GATHER_MERGE: {
                    auto gather = std::dynamic_pointer_cast<GatherNode>(plan);
                    if (gather)
                    {
                        if (node.children.size() >= 1)
                        {
                            annotateRuntimePlanResources(node.children[0],
                                                         gather->childPlan(),
                                                         cost_model,
                                                         row_width,
                                                         spill_policy_text);
                        }
                        const auto child_cost = planCostEstimate(gather->childPlan());
                        uint64_t order_keys = 1;
                        if (gather->mergeOrdered())
                        {
                            if (auto sort = std::dynamic_pointer_cast<SortNode>(
                                    gather->childPlan()))
                            {
                                order_keys = std::max<uint64_t>(
                                    1, sort->sortKeyCount());
                            }
                        }
                        resource_cost =
                            gather->mergeOrdered()
                                ? cost_model.costGatherMerge(
                                      child_cost,
                                      child_cost.rows,
                                      order_keys,
                                      gather->workersPlanned(),
                                      gather->leaderParticipates(),
                                      nullptr)
                                : cost_model.costGather(
                                      child_cost,
                                      child_cost.rows,
                                      gather->workersPlanned(),
                                      gather->leaderParticipates(),
                                      nullptr);
                        have_resource_cost = true;
                    }
                    break;
                }
                case PlanNodeType::LIMIT: {
                    auto limit = std::dynamic_pointer_cast<LimitNode>(plan);
                    if (limit)
                    {
                        if (node.children.size() >= 1)
                        {
                            annotateRuntimePlanResources(node.children[0],
                                                         limit->childPlan(),
                                                         cost_model,
                                                         row_width,
                                                         spill_policy_text);
                        }
                        const uint64_t input_rows =
                            limit->childPlan() ? limit->childPlan()->rows() : plan->rows();
                        resource_cost = cost_model.costLimit(input_rows,
                                                             limit->limitCount(),
                                                             limit->offsetCount(),
                                                             nullptr);
                        have_resource_cost = true;
                    }
                    break;
                }
                case PlanNodeType::WINDOW: {
                    auto window = std::dynamic_pointer_cast<WindowNode>(plan);
                    if (window)
                    {
                        if (node.children.size() >= 1)
                        {
                            annotateRuntimePlanResources(node.children[0],
                                                         window->child(),
                                                         cost_model,
                                                         row_width,
                                                         spill_policy_text);
                        }
                        uint64_t partition_keys = 0;
                        uint64_t order_keys = 0;
                        for (const auto &func : window->windowFunctions())
                        {
                            if (func.window_spec == nullptr)
                            {
                                continue;
                            }
                            partition_keys += func.window_spec->partition_by.size();
                            order_keys += func.window_spec->order_by.size();
                        }
                        resource_cost = cost_model.costWindow(
                            window->child() ? window->child()->rows() : plan->rows(),
                            row_width,
                            partition_keys,
                            order_keys,
                            window->windowFunctions().size(),
                            nullptr);
                        have_resource_cost = true;
                    }
                    break;
                }
                default: {
                    const auto &plan_children = plan->children();
                    for (size_t i = 0; i < node.children.size() && i < plan_children.size(); ++i)
                    {
                        annotateRuntimePlanResources(node.children[i],
                                                     plan_children[i],
                                                     cost_model,
                                                     row_width,
                                                     spill_policy_text);
                    }
                    break;
                }
            }

            if (have_resource_cost)
            {
                applyResourceMetadata(node, resource_cost, spill_policy_text);
            }
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
                                       const core::ID &current_schema_id,
                                       const ParameterBindings *parameter_bindings)
        -> core::Status
    {
        planned_out = PlannedSelectQuery{};
        conn_ctx_ = conn_ctx;
        if (select_stmt == nullptr)
        {
            return core::Status::INVALID_ARGUMENT;
        }

        CostParameters planner_params = cost_model_.parameters();
        PlannerControlSurface planner_controls;
        std::string planner_control_error;
        if (!resolvePlannerControlSurface(conn_ctx,
                                          planner_params,
                                          planner_controls,
                                          planner_control_error))
        {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::INVALID_ARGUMENT,
                              planner_control_error.c_str());
            return core::Status::INVALID_ARGUMENT;
        }
        planner_params.work_mem_bytes = planner_controls.work_mem_bytes;
        planner_params.parallel_setup_cost =
            planner_controls.parallel_setup_cost;
        planner_params.parallel_tuple_cost =
            planner_controls.parallel_tuple_cost;
        const PlannerSpillPolicy spill_policy = planner_controls.spill_policy;
        const std::string spill_policy_text = plannerSpillPolicyName(spill_policy);
        CostModel active_cost_model(planner_params);
        executor::ParallelConfig parallel_config;
        parallel_config.enable_parallel = planner_controls.enable_parallel;
        parallel_config.enable_parallel_scan =
            planner_controls.enable_parallel_scan;
        parallel_config.enable_parallel_hash =
            planner_controls.enable_parallel_hash;
        parallel_config.enable_parallel_aggregate =
            planner_controls.enable_parallel_aggregate;
        parallel_config.enable_parallel_join =
            planner_controls.enable_parallel_join;
        parallel_config.parallel_leader_participation =
            planner_controls.parallel_leader_participation;
        parallel_config.max_workers = planner_controls.max_parallel_workers;
        parallel_config.max_workers_per_gather =
            planner_controls.max_parallel_workers_per_gather;
        parallel_config.min_rows_per_worker =
            std::max<uint32_t>(1, planner_controls.min_parallel_rows_per_worker);
        parallel_config.min_pages_per_worker = 1;
        parallel_config.min_parallel_table_scan_size =
            planner_controls.min_parallel_table_scan_size;
        parallel_config.parallel_setup_cost =
            planner_controls.parallel_setup_cost;
        parallel_config.parallel_tuple_cost =
            planner_controls.parallel_tuple_cost;
        const MgaCostingSnapshot mga_costing_snapshot = loadMgaCostingSnapshot(db_);
        upsertPlannerControl(planner_controls.runtime_controls,
                             "MGA_COSTING_CONTRACT",
                             mga_costing_snapshot.contract_id,
                             "CANONICAL_METRIC_CONTRACT");
        upsertPlannerControl(planner_controls.runtime_controls,
                             "MGA_COSTING_METRIC_SCHEMA",
                             std::to_string(mga_costing_snapshot.metric_schema_version),
                             "CANONICAL_METRIC_CONTRACT");
        upsertPlannerControl(planner_controls.runtime_controls,
                             "MGA_COSTING_MODE",
                             "CANONICAL_ONLY",
                             "CANONICAL_METRIC_CONTRACT");
        upsertPlannerControl(planner_controls.runtime_controls,
                             "MGA_COSTING_ACTIVE",
                             mga_costing_snapshot.available ? "true" : "false",
                             mga_costing_snapshot.available
                                 ? "CANONICAL_METRIC_CONTRACT"
                                 : "DEFAULT");

        struct ParameterBindingScope
        {
            SelectivityEstimator &estimator;
            const ParameterBindings *previous = nullptr;

            ParameterBindingScope(SelectivityEstimator &target,
                                  const ParameterBindings *bindings)
                : estimator(target),
                  previous(target.parameterBindings())
            {
                estimator.setParameterBindings(bindings);
            }

            ~ParameterBindingScope()
            {
                estimator.setParameterBindings(previous);
            }
        } parameter_scope(selectivity_estimator_, parameter_bindings);

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

        std::vector<RuntimePlanTraceEntry> considered_paths;
        std::vector<RuntimePlanTraceEntry> rejected_paths;
        std::vector<RuntimePlanStatisticsProvenance> statistics_provenance;
        std::vector<BaseRelationCandidateBundle> access_bundles(
            planned_out.resolved_query.relations.size());
        std::vector<uint64_t> relation_row_width_bytes(
            planned_out.resolved_query.relations.size(), 0);
        int64_t preview_limit_count = 0;
        int64_t preview_offset_count = 0;
        bool query_has_limit = false;
        bool query_has_offset = false;
        if (select_stmt->limit != nullptr)
        {
            query_has_limit =
                extractConstantInt64(select_stmt->limit, preview_limit_count);
        }
        else if (select_stmt->fetch_row_count != nullptr)
        {
            query_has_limit = extractConstantInt64(select_stmt->fetch_row_count,
                                                  preview_limit_count);
        }
        if (select_stmt->offset != nullptr)
        {
            query_has_offset =
                extractConstantInt64(select_stmt->offset, preview_offset_count);
        }
        bool parallel_scan_modeled_in_search = false;
        for (size_t relation_index = 0;
             relation_index < planned_out.resolved_query.relations.size();
             ++relation_index)
        {
            const auto &relation = planned_out.resolved_query.relations[relation_index];
            BaseRelationCandidateBundle bundle;
            bundle.relation_index = relation_index;

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
            const bool allow_search_modeled_parallel_scan =
                planner_controls.enable_parallel &&
                planner_controls.enable_parallel_scan &&
                planned_out.resolved_query.relations.size() == 1 &&
                planned_out.resolved_query.joins.empty() &&
                relation_aggregates.empty() &&
                relation_windows.empty() &&
                select_stmt->group_by.empty() &&
                select_stmt->order_by.empty() &&
                !query_has_limit &&
                !query_has_offset;

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

            auto appendUniqueText =
                [](std::vector<std::string> &entries, const std::string &value) {
                    if (value.empty() ||
                        std::find(entries.begin(), entries.end(), value) !=
                            entries.end())
                    {
                        return;
                    }
                    entries.push_back(value);
                };

            auto sortAndUniqueText = [](std::vector<std::string> &entries) {
                std::sort(entries.begin(), entries.end());
                entries.erase(std::unique(entries.begin(), entries.end()),
                              entries.end());
            };

            auto candidateFamilyIdentitySignature =
                [](size_t relation_index,
                   const AccessPathDescriptor &descriptor) -> std::string {
                    const std::string family_name =
                        descriptor.family.empty() ? descriptor.path_name
                                                  : descriptor.family;
                    std::ostringstream out;
                    out << relation_index << ':'
                        << descriptor.taxonomy_version << ':'
                        << family_name << ':'
                        << static_cast<uint32_t>(descriptor.family_kind) << ':'
                        << accessPathExactnessClassName(
                               descriptor.exactness_class)
                        << ':'
                        << accessPathVisibilityEnforcementName(
                               descriptor.visibility_enforcement)
                        << ':'
                        << (descriptor.requires_recheck ? 1 : 0) << ':'
                        << accessPathQueryabilityStateName(
                               descriptor.queryability_state) << ':'
                        << descriptor.path_name;
                    return out.str();
                };

            auto candidateFamilyStatisticsSignature =
                [](size_t relation_index,
                   const AccessPathDescriptor &descriptor) -> std::string {
                    const std::string family_name =
                        descriptor.family.empty() ? descriptor.path_name
                                                  : descriptor.family;
                    std::ostringstream out;
                    out << relation_index << ':'
                        << family_name << ':'
                        << descriptor.family_metrics_version << ':'
                        << descriptor.metrics_confidence_class << ':'
                        << accessPathQueryabilityStateName(
                               descriptor.queryability_state) << ':'
                        << descriptor.formula_profile_id << '@'
                        << descriptor.formula_profile_version << ':'
                        << descriptor.calibration_profile_id;
                    return out.str();
                };

            auto normalizePlannerText = [](std::string text) -> std::string {
                std::string normalized;
                normalized.reserve(text.size());
                for (unsigned char ch : text)
                {
                    if (!std::isspace(ch))
                    {
                        normalized.push_back(
                            static_cast<char>(std::toupper(ch)));
                    }
                }
                return normalized;
            };

            auto columnNameForId = [&](const core::ID &column_id) -> std::string {
                const auto column_it =
                    std::find_if(relation.columns.begin(),
                                 relation.columns.end(),
                                 [&](const core::CatalogManager::ColumnInfo &column) {
                                     return column.column_id == column_id;
                                 });
                if (column_it == relation.columns.end())
                {
                    return {};
                }
                return column_it->column_name;
            };

            auto qualifierMatchesRelation =
                [&](const std::string &qualifier) -> bool {
                    if (qualifier.empty())
                    {
                        return true;
                    }
                    return core::IdentifierUtils::namesMatch(qualifier,
                                                             false,
                                                             relation.alias,
                                                             false) ||
                           core::IdentifierUtils::namesMatch(qualifier,
                                                             false,
                                                             relation.table_info.table_name,
                                                             false) ||
                           core::IdentifierUtils::namesMatch(qualifier,
                                                             false,
                                                             relation.table_path,
                                                             false) ||
                           core::IdentifierUtils::namesMatch(qualifier,
                                                             false,
                                                             relationLookupName(relation),
                                                             false);
                };

            auto orderedPrefixLengthForIndex =
                [&](const core::CatalogManager::IndexInfo &index) -> uint64_t {
                    if (select_stmt == nullptr || select_stmt->order_by.empty())
                    {
                        return 0;
                    }

                    uint64_t prefix = 0;
                    const size_t limit = std::min(index.column_ids.size(),
                                                  select_stmt->order_by.size());
                    for (size_t order_index = 0; order_index < limit; ++order_index)
                    {
                        const auto *order_item = select_stmt->order_by[order_index];
                        if (order_item == nullptr || order_item->expr == nullptr ||
                            order_item->expr->kind() !=
                                parser::v3::ASTKind::ColumnRefExpr)
                        {
                            break;
                        }

                        const auto *column_ref =
                            static_cast<const parser::v3::ColumnRefExpr *>(
                                order_item->expr);
                        if (column_ref->column.column_name ==
                            parser::v3::StringPool::INVALID_ID)
                        {
                            break;
                        }

                        std::string qualifier;
                        if (column_ref->column.has_table_qualifier &&
                            !column_ref->column.table_path.components.empty())
                        {
                            qualifier = pool.get(
                                column_ref->column.table_path.components.back());
                        }
                        if (!qualifierMatchesRelation(qualifier))
                        {
                            break;
                        }

                        const std::string expected_column =
                            columnNameForId(index.column_ids[order_index]);
                        if (expected_column.empty())
                        {
                            break;
                        }
                        const std::string actual_column =
                            std::string(pool.get(column_ref->column.column_name));
                        if (!core::IdentifierUtils::namesMatch(actual_column,
                                                               false,
                                                               expected_column,
                                                               false))
                        {
                            break;
                        }

                        ++prefix;
                    }

                    return prefix;
                };

            std::string relation_predicate_text = buildPredicateText();
            if (planned_out.resolved_query.relations.size() == 1 &&
                select_stmt != nullptr && select_stmt->where != nullptr)
            {
                relation_predicate_text =
                    expressionToString(select_stmt->where, pool);
            }
            const std::string normalized_relation_predicates =
                normalizePlannerText(relation_predicate_text);

            auto partialIndexMatches =
                [&](const core::CatalogManager::IndexInfo &index) -> bool {
                    if (!index.is_partial_index || index.predicate_string.empty())
                    {
                        return false;
                    }
                    const std::string normalized_index_predicate =
                        normalizePlannerText(index.predicate_string);
                    return !normalized_index_predicate.empty() &&
                           normalized_relation_predicates.find(
                               normalized_index_predicate) != std::string::npos;
                };

            auto expressionIndexMatches =
                [&](const core::CatalogManager::IndexInfo &index) -> bool {
                    if (!index.is_expression_index ||
                        index.expression_strings.empty())
                    {
                        return false;
                    }
                    for (const auto &expression_text : index.expression_strings)
                    {
                        const std::string normalized_expression =
                            normalizePlannerText(expression_text);
                        if (!normalized_expression.empty() &&
                            normalized_relation_predicates.find(
                                normalized_expression) != std::string::npos)
                        {
                            return true;
                        }
                    }
                    return false;
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
            const std::string relation_subject = "relation:" + relation_name;
            const MgaRelationCostSignal *mga_relation_signal =
                lookupMgaRelationCostSignal(mga_costing_snapshot, relation);
            core::ErrorContext stats_ctx;
            TableStatistics relation_table_stats;
            const bool have_relation_table_stats =
                relation.resolved && stats_manager_ != nullptr &&
                !isZeroId(relation.table_info.table_id) &&
                stats_manager_->getTableStatistics(relation.table_info.table_id,
                                                   relation_table_stats,
                                                   &stats_ctx) == core::Status::OK;
            double relation_stats_penalty = 1.0;
            std::string relation_stats_penalty_reason;
            auto updateRelationStatsPenalty =
                [&](double candidate_penalty, const std::string &reason) {
                    if (candidate_penalty > relation_stats_penalty)
                    {
                        relation_stats_penalty = candidate_penalty;
                        relation_stats_penalty_reason = reason;
                    }
                };
            if (have_relation_table_stats)
            {
                if (relation_table_stats.last_analyzed_time > 0 ||
                    relation_table_stats.modified_rows_since_analyze > 0)
                {
                    const double table_penalty = statsPenaltyMultiplier(
                        relation_table_stats.staleness_class,
                        relation_table_stats.confidence_class);
                    updateRelationStatsPenalty(
                        table_penalty,
                        statsPenaltyReasonForTable(relation_table_stats, table_penalty));
                }
            }
            double qual_cost = 0.0;
            const bool predicate_or =
                core::IdentifierUtils::toUpper(relation.predicate_combination) == "OR";
            double selectivity = predicate_or ? 0.0 : 1.0;
            for (const auto &predicate : predicates)
            {
                qual_cost += active_cost_model.operatorCost(predicate.operator_name);
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

            const PartitionPruningResult pruning =
                analyzePartitionPruning(db_ ? db_->catalog_manager() : nullptr,
                                        relation,
                                        parameter_bindings);
            const uint64_t unpruned_base_rows =
                relation.estimated_rows == 0 ? 1000 : relation.estimated_rows;
            const uint64_t unpruned_base_pages =
                std::max<uint64_t>(1,
                                   relation.estimated_pages == 0 ? 10
                                                                 : relation.estimated_pages);
            const uint64_t base_rows =
                pruning.pruned && pruning.rows > 0 ? pruning.rows : unpruned_base_rows;
            const uint64_t base_pages =
                pruning.pruned && pruning.pages > 0 ? pruning.pages : unpruned_base_pages;
            const MgaCostPressure relation_mga_pressure =
                buildMgaCostPressure(mga_relation_signal,
                                     mga_costing_snapshot.commit_fence_backlog,
                                     base_pages,
                                     base_rows);
            const uint64_t relation_row_width =
                have_relation_table_stats && relation_table_stats.avg_row_size > 0.0f
                    ? std::max<uint64_t>(
                          16,
                          static_cast<uint64_t>(
                              std::ceil(relation_table_stats.avg_row_size)))
                    : std::max<uint64_t>(
                          16,
                          static_cast<uint64_t>(
                              std::max<size_t>(1, relation.columns.size()) * 16));
            relation_row_width_bytes[relation_index] = relation_row_width;
            const double relation_heap_rows_per_page =
                std::max(1.0,
                         static_cast<double>(base_rows) /
                             static_cast<double>(std::max<uint64_t>(1, base_pages)));
            CostParameters relation_cost_params = planner_params;
            relation_cost_params.default_row_width_bytes = relation_row_width;
            relation_cost_params.calibrated_heap_rows_per_page =
                relation_heap_rows_per_page;
            CostModel relation_cost_model(relation_cost_params);
            const uint64_t seq_rows = relationOutputRows(base_rows, selectivity);
            if (mga_relation_signal != nullptr)
            {
                appendStatsProvenance(statistics_provenance,
                                      relation_subject,
                                      "MGA_CANONICAL_METRICS",
                                      describeMgaCostSignal(*mga_relation_signal,
                                                            mga_costing_snapshot.commit_fence_backlog));
            }
            if (have_relation_table_stats)
            {
                appendTableStatsProvenance(
                    statistics_provenance,
                    relation_subject,
                    pruning.pruned ? "PARTITION_PRUNED_STATS"
                                   : relation.estimated_rows == 0
                                         ? "CARDINALITY_FALLBACK"
                                         : "CATALOG_TABLE_STATS",
                    "base_rows=" + std::to_string(base_rows) +
                        ", pages=" + std::to_string(base_pages),
                    relation_table_stats);
            }
            else
            {
                appendStatsProvenance(
                    statistics_provenance,
                    relation_subject,
                    pruning.pruned ? "PARTITION_PRUNED_STATS"
                                   : relation.estimated_rows == 0
                                         ? "CARDINALITY_FALLBACK"
                                         : "CATALOG_TABLE_STATS",
                    "base_rows=" + std::to_string(base_rows) +
                        ", pages=" + std::to_string(base_pages));
            }
            if (pruning.available)
            {
                appendStatsProvenance(
                    statistics_provenance,
                    relation_subject,
                    pruning.pruned ? "PARTITION_PRUNING" : "PARTITION_CATALOG",
                    pruning.key_column.empty()
                        ? pruning.strategy
                        : pruning.strategy + " key=" + pruning.key_column);
                appendRuntimeTrace(considered_paths,
                                   "PARTITION_PRUNING",
                                   relation_subject,
                                   pruning.strategy.empty()
                                       ? "PARTITIONED_RELATION"
                                       : pruning.strategy,
                                   pruning.pruned ? "CHOSEN" : "CONSIDERED",
                                   pruning.pruned
                                       ? "matched " +
                                             std::to_string(pruning.targets.size()) +
                                             " partition target(s)"
                                       : "no single-target pruning proof",
                                   0.0,
                                   0.0,
                                   base_rows);
            }
            for (const auto &predicate : predicates)
            {
                ColumnStatistics predicate_stats;
                const bool have_predicate_stats =
                    relation.resolved && stats_manager_ != nullptr &&
                    !isZeroId(relation.table_info.table_id) &&
                    !isZeroId(predicate.column_id) &&
                    stats_manager_->getColumnStatistics(relation.table_info.table_id,
                                                       predicate.column_id,
                                                       predicate_stats,
                                                       &stats_ctx) == core::Status::OK;
                const std::string predicate_source =
                    have_predicate_stats
                        ? predicateStatsSource(relation,
                                               predicate,
                                               stats_manager_,
                                               &predicate_stats)
                        : predicateStatsSource(relation, predicate, stats_manager_);
                if (have_predicate_stats)
                {
                    const double predicate_penalty = statsPenaltyMultiplier(
                        predicate_stats.staleness_class,
                        predicate_stats.confidence_class);
                    updateRelationStatsPenalty(
                        predicate_penalty,
                        statsPenaltyReason(predicate_stats, predicate_penalty));
                    appendColumnStatsProvenance(statistics_provenance,
                                                relation_subject,
                                                predicate_source,
                                                predicate.predicate_text,
                                                predicate_stats);
                }
                else
                {
                    appendStatsProvenance(statistics_provenance,
                                          relation_subject,
                                          predicate_source,
                                          predicate.predicate_text);
                }
                if (!predicate.has_index_match && !relation.indexes.empty())
                {
                    appendRuntimeTrace(rejected_paths,
                                       "ACCESS_PATH",
                                       relation_subject,
                                       "INDEX_SCAN",
                                       "REJECTED",
                                       "no matching index for predicate " +
                                           predicate.predicate_text,
                                       0.0,
                                       0.0,
                                       seq_rows);
                }
            }
            CostEstimate best_cost =
                relation_cost_model.costSeqScan(base_pages, seq_rows, qual_cost, ctx);
            if (relation_stats_penalty > 1.0)
            {
                best_cost.startup_cost *= relation_stats_penalty;
                best_cost.total_cost *= relation_stats_penalty;
                appendRuntimeTrace(considered_paths,
                                   "STATS_FRESHNESS",
                                   relation_subject,
                                   "SEQ_SCAN",
                                   "ADJUSTED",
                                   relation_stats_penalty_reason,
                                   best_cost.startup_cost,
                                   best_cost.total_cost,
                                   best_cost.rows);
            }
            const double seq_mga_penalty =
                applyMgaCostPenalty(best_cost, relation_mga_pressure, "SEQ_SCAN");
            if (seq_mga_penalty > 0.0)
            {
                appendRuntimeTrace(considered_paths,
                                   "MGA_COSTING",
                                   relation_subject,
                                   "SEQ_SCAN",
                                   "ADJUSTED",
                                   "canonical MGA telemetry penalty=" +
                                       std::to_string(seq_mga_penalty) + " " +
                                       describeMgaCostPressure(
                                           relation_mga_pressure),
                                   best_cost.startup_cost,
                                   best_cost.total_cost,
                                   best_cost.rows);
            }
            appendRuntimeTrace(considered_paths,
                               "ACCESS_PATH",
                               relation_subject,
                               "SEQ_SCAN",
                               "CONSIDERED",
                               "baseline sequential scan",
                               best_cost.startup_cost,
                               best_cost.total_cost,
                               best_cost.rows);
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
            seq_plan->setCost(best_cost);
            best_plan = seq_plan;

            std::string best_scan_kind = "SEQ_SCAN";
            PlannerFamilyLoweringResult best_lowering =
                lowerSequentialPlannerFamily();
            std::string best_scan_family = best_lowering.family_name;
            std::vector<std::string> best_scan_family_tags;
            std::vector<RuntimePlanIndexPredicate> best_runtime_predicates;
            std::string best_bitmap_op;
            bool best_covering_index = false;
            bool best_exact_key_lookup = false;
            double best_coverage_fraction = 1.0;
            uint64_t best_candidate_budget = 0;
            bool best_ordered_output = false;
            uint64_t best_ordered_prefix_length = 0;
            std::vector<AccessPathDescriptor::OrderingKey> best_ordering_keys;
            bool best_order_complete = false;
            double best_interesting_order_score = 0.0;
            std::vector<size_t> best_required_outer_relation_indexes;
            std::vector<std::string> best_required_outer_relation_aliases;
            core::CatalogManager::IndexInfo matched_index{};
            std::unordered_map<std::string, IndexFamilyMetricsPacket>
                relation_index_metrics_cache;
            std::unordered_map<std::string, nlohmann::json>
                relation_index_metrics_payload_cache;
            auto metricsQueryabilityStateToAccessPath =
                [](IndexMetricsQueryabilityState state)
                    -> AccessPathQueryabilityState {
                    switch (state)
                    {
                        case IndexMetricsQueryabilityState::QUERYABLE:
                            return AccessPathQueryabilityState::QUERYABLE;
                        case IndexMetricsQueryabilityState::LIMITED:
                            return AccessPathQueryabilityState::LIMITED;
                        case IndexMetricsQueryabilityState::INVALID:
                            return AccessPathQueryabilityState::INVALID;
                        case IndexMetricsQueryabilityState::UNKNOWN:
                        default:
                            return AccessPathQueryabilityState::UNKNOWN;
                    }
                };
            auto loadIndexFamilyMetricsPacket =
                [&](const core::CatalogManager::IndexInfo &index_info,
                    IndexFamilyMetricsPacket &packet) -> bool {
                    if (isZeroId(index_info.index_id) || stats_manager_ == nullptr)
                    {
                        return false;
                    }

                    const std::string cache_key = index_info.index_id.toString();
                    auto cache_it = relation_index_metrics_cache.find(cache_key);
                    if (cache_it != relation_index_metrics_cache.end())
                    {
                        packet = cache_it->second;
                        return true;
                    }

                    core::ErrorContext metrics_ctx;
                    if (stats_manager_->getIndexFamilyMetrics(index_info.index_id,
                                                              packet,
                                                              &metrics_ctx) != core::Status::OK)
                    {
                        return false;
                    }
                    relation_index_metrics_cache.emplace(cache_key, packet);
                    return true;
                };
            auto loadIndexFamilyMetricsPayload =
                [&](const core::CatalogManager::IndexInfo &index_info,
                    const IndexFamilyMetricsPacket *packet,
                    nlohmann::json &payload) -> bool {
                    if (packet == nullptr ||
                        isZeroId(index_info.index_id) ||
                        packet->family_metrics_payload.empty())
                    {
                        return false;
                    }

                    const std::string cache_key = index_info.index_id.toString();
                    auto cache_it =
                        relation_index_metrics_payload_cache.find(cache_key);
                    if (cache_it != relation_index_metrics_payload_cache.end())
                    {
                        payload = cache_it->second;
                        return !payload.is_null();
                    }

                    payload = nlohmann::json::parse(packet->family_metrics_payload,
                                                   nullptr,
                                                   false);
                    if (payload.is_discarded() || !payload.is_object())
                    {
                        payload = nlohmann::json();
                        relation_index_metrics_payload_cache.emplace(cache_key,
                                                                     payload);
                        return false;
                    }
                    relation_index_metrics_payload_cache.emplace(cache_key,
                                                                 payload);
                    return true;
                };
            auto familyMetricDouble =
                [&](const core::CatalogManager::IndexInfo &index_info,
                    const IndexFamilyMetricsPacket *packet,
                    const std::string &metric_name,
                    double fallback_value) -> double {
                    nlohmann::json payload;
                    if (!loadIndexFamilyMetricsPayload(index_info,
                                                       packet,
                                                       payload))
                    {
                        return fallback_value;
                    }
                    const auto family_metrics_it = payload.find("family_metrics");
                    if (family_metrics_it == payload.end() ||
                        !family_metrics_it->is_object())
                    {
                        return fallback_value;
                    }
                    const auto metric_it =
                        family_metrics_it->find(metric_name);
                    if (metric_it == family_metrics_it->end() ||
                        !metric_it->is_number())
                    {
                        return fallback_value;
                    }
                    return metric_it->get<double>();
                };
            auto buildIndexFamilyCostInput =
                [&](const PlannerFamilyLoweringResult &lowering,
                    const core::CatalogManager::IndexInfo &index_info,
                    const IndexFamilyMetricsPacket *packet,
                    uint64_t row_width_bytes,
                    double heap_rows_per_page,
                    bool covering_index,
                    bool ordered_output) -> IndexFamilyCostCalibrationInput {
                    IndexFamilyCostCalibrationInput input;
                    input.planner_family = lowering.family_name;
                    input.metrics_type_name =
                        packet != nullptr
                            ? indexFamilyMetricsTypeName(
                                  packet->family_metrics_type)
                            : "UNKNOWN";
                    input.family_metrics_version =
                        packet != nullptr ? packet->family_metrics_version : 0;
                    input.metrics_confidence_class =
                        packet != nullptr
                            ? indexMetricsConfidenceClassName(
                                  packet->metrics_confidence_class)
                            : "UNKNOWN";
                    input.correlation =
                        packet != nullptr ? packet->correlation : 0.0;
                    input.bloat_ratio =
                        packet != nullptr ? packet->bloat_ratio : 0.0;
                    input.recheck_ratio_est =
                        packet != nullptr ? packet->recheck_ratio_est : 0.0;
                    input.coverage_fraction =
                        packet != nullptr
                            ? packet->coverage_fraction
                            : (covering_index ? 1.0 : 0.0);
                    input.ordered_output = ordered_output;
                    input.covering_index = covering_index;
                    input.requires_recheck = lowering.requires_recheck;
                    input.row_width_bytes = row_width_bytes;
                    input.heap_rows_per_page = heap_rows_per_page;
                    input.index_entries_per_page =
                        packet != nullptr && packet->leaf_pages > 0
                            ? std::max(
                                  1.0,
                                  static_cast<double>(std::max<uint64_t>(
                                      1, packet->live_entry_count_est)) /
                                      static_cast<double>(packet->leaf_pages))
                            : 0.0;
                    input.dead_fraction =
                        packet != nullptr ? packet->dead_fraction : 0.0;
                    input.duplicate_density = familyMetricDouble(
                        index_info,
                        packet,
                        "duplicate_density",
                        0.0);
                    input.avg_probe_pages = familyMetricDouble(
                        index_info,
                        packet,
                        "avg_probe_pages",
                        packet != nullptr
                            ? static_cast<double>(std::max<uint16_t>(1, packet->height))
                            : 0.0);
                    input.false_positive_ratio = familyMetricDouble(
                        index_info,
                        packet,
                        "bitmap_false_positive_ratio",
                        familyMetricDouble(index_info,
                                           packet,
                                           "tombstone_fraction",
                                           0.0));
                    return input;
                };
            auto makeBaseAccessChoice =
                [&](std::shared_ptr<Path> candidate_path,
                    std::shared_ptr<PlanNode> candidate_plan,
                    const CostEstimate &candidate_cost,
                    const std::string &scan_kind,
                    const std::string &scan_family,
                    const PlannerFamilyLoweringResult &lowering,
                    const std::vector<std::string> &scan_family_tags,
                    const std::vector<RuntimePlanIndexPredicate>
                        &runtime_predicates,
                    const std::string &bitmap_op,
                    bool covering_index,
                    bool exact_key_lookup,
                    bool ordered_output,
                    uint64_t ordered_prefix_length,
                    const std::vector<AccessPathDescriptor::OrderingKey>
                        &ordering_keys,
                    bool order_complete,
                    double interesting_order_score,
                    const std::vector<size_t>
                        &required_outer_relation_indexes,
                    const std::vector<std::string>
                        &required_outer_relation_aliases,
                    double coverage_fraction,
                    uint64_t candidate_budget,
                    const core::CatalogManager::IndexInfo &candidate_index,
                    const std::vector<std::string> &candidate_scan_families)
                    -> BaseAccessChoice {
                    BaseAccessChoice candidate;
                    IndexFamilyMetricsPacket index_metrics_packet;
                    const bool have_index_family_metrics =
                        loadIndexFamilyMetricsPacket(candidate_index,
                                                     index_metrics_packet);
                    AccessPathQueryabilityState effective_queryability =
                        lowering.queryability_state;
                    if (have_index_family_metrics)
                    {
                        const AccessPathQueryabilityState persisted_state =
                            metricsQueryabilityStateToAccessPath(
                                index_metrics_packet.queryability_state);
                        if (persisted_state == AccessPathQueryabilityState::INVALID ||
                            lowering.queryability_state ==
                                AccessPathQueryabilityState::INVALID)
                        {
                            effective_queryability =
                                AccessPathQueryabilityState::INVALID;
                        }
                        else if (persisted_state ==
                                     AccessPathQueryabilityState::LIMITED ||
                                 lowering.queryability_state ==
                                     AccessPathQueryabilityState::LIMITED)
                        {
                            effective_queryability =
                                AccessPathQueryabilityState::LIMITED;
                        }
                        else if (persisted_state !=
                                 AccessPathQueryabilityState::UNKNOWN)
                        {
                            effective_queryability = persisted_state;
                        }
                    }
                    candidate.relation_index = relation_index;
                    candidate.path = std::move(candidate_path);
                    candidate.plan = std::move(candidate_plan);
                    candidate.selectivity = selectivity;
                    candidate.candidate_scan_families = candidate_scan_families;
                    candidate.runtime_relation.source_relation_index =
                        relation_index;
                    candidate.runtime_relation.table_path = relation.table_path;
                    candidate.runtime_relation.physical_table_path =
                        relation.physical_table_path;
                    candidate.runtime_relation.alias = relation.alias;
                    candidate.runtime_relation.table_id_text =
                        relation.resolved
                            ? relation.table_info.table_id.toString()
                            : std::string();
                    candidate.runtime_relation.base_rows = base_rows;
                    candidate.runtime_relation.selectivity = selectivity;
                    candidate.runtime_relation.scan_kind = scan_kind;
                    candidate.runtime_relation.scan_family = scan_family;
                    candidate.runtime_relation.path_name = lowering.path_name;
                    candidate.runtime_relation.scan_family_kind =
                        lowering.family;
                    candidate.runtime_relation.taxonomy_version =
                        kPlannerFamilyTaxonomyVersion;
                    candidate.runtime_relation.scan_family_tags =
                        scan_family_tags;
                    candidate.runtime_relation.candidate_scan_families =
                        candidate_scan_families;
                    candidate.runtime_relation.exactness_class =
                        lowering.exactness_class;
                    candidate.runtime_relation.requires_recheck =
                        lowering.requires_recheck;
                    candidate.runtime_relation.coverage_fraction =
                        scan_kind == "SEQ_SCAN"
                            ? 1.0
                            : std::clamp(coverage_fraction, 0.0, 1.0);
                    candidate.runtime_relation.candidate_budget =
                        candidate_budget;
                    candidate.runtime_relation.visibility_enforcement =
                        lowering.visibility_enforcement;
                    candidate.runtime_relation.family_metrics_version =
                        have_index_family_metrics
                            ? index_metrics_packet.family_metrics_version
                            : 0;
                    candidate.runtime_relation.metrics_confidence_class =
                        have_index_family_metrics
                            ? indexMetricsConfidenceClassName(
                                  index_metrics_packet.metrics_confidence_class)
                            : (scan_kind == "SEQ_SCAN"
                                   ? (have_relation_table_stats
                                          ? statisticsConfidenceClassName(
                                                relation_table_stats
                                                    .confidence_class)
                                          : "UNKNOWN")
                                   : "LOW");
                    candidate.runtime_relation.queryability_state =
                        effective_queryability;
                    candidate.runtime_relation.bitmap_op = bitmap_op;
                    candidate.runtime_relation.covering_index =
                        covering_index;
                    candidate.runtime_relation.exact_key_lookup =
                        exact_key_lookup;
                    candidate.runtime_relation.flattened_derived =
                        relation.flattened_derived;
                    candidate.runtime_relation.lateral = relation.lateral;
                    candidate.runtime_relation.parameterized =
                        relation.lateral;
                    candidate.runtime_relation.ordered_output = ordered_output;
                    candidate.runtime_relation.ordered_prefix_length =
                        ordered_prefix_length;
                    candidate.runtime_relation.required_outer_relation_indexes =
                        required_outer_relation_indexes;
                    candidate.runtime_relation.required_outer_relation_aliases =
                        required_outer_relation_aliases;
                    candidate.runtime_relation.partition_pruned = pruning.pruned;
                    candidate.runtime_relation.partition_strategy =
                        pruning.strategy;
                    candidate.runtime_relation.partition_key_column =
                        pruning.key_column;
                    candidate.runtime_relation.partition_key_columns =
                        pruning.key_columns;
                    candidate.runtime_relation.partition_targets =
                        pruning.targets;
                    candidate.runtime_relation
                        .partition_targets_pruned_at_plan =
                        pruning.pruned_targets_at_plan;
                    candidate.runtime_relation
                        .runtime_partition_pruning_eligible =
                        pruning.runtime_pruning_eligible;
                    candidate.runtime_relation
                        .runtime_partition_pruning_sources =
                        pruning.runtime_pruning_sources;
                    candidate.runtime_relation.partition_predicates =
                        pruning.predicates;
                    candidate.runtime_relation.startup_cost =
                        candidate_cost.startup_cost;
                    candidate.runtime_relation.total_cost =
                        candidate_cost.total_cost;
                    candidate.runtime_relation.estimated_rows =
                        candidate_cost.rows;
                    candidate.runtime_relation.formula_profile_id =
                        candidate_cost.formula_profile_id;
                    candidate.runtime_relation.formula_profile_version =
                        candidate_cost.formula_profile_version;
                    candidate.runtime_relation.calibration_profile_id =
                        candidate_cost.calibration_profile_id;
                    if (!runtime_predicates.empty())
                    {
                        candidate.runtime_relation.index_predicates =
                            runtime_predicates;
                        candidate.runtime_relation.index_predicate =
                            runtime_predicates.front();
                    }
                    if (!runtime_predicates.empty() &&
                        !runtime_predicates.front().index_name.empty())
                    {
                        candidate.runtime_relation.index_name =
                            runtime_predicates.front().index_name;
                        candidate.runtime_relation.index_id_text =
                            runtime_predicates.front().index_id_text;
                    }
                    else if (!isZeroId(candidate_index.index_id))
                    {
                        candidate.runtime_relation.index_name =
                            candidate_index.index_name;
                        candidate.runtime_relation.index_id_text =
                            candidate_index.index_id.toString();
                    }

                    candidate.access_descriptor.family = scan_family;
                    candidate.access_descriptor.path_name = lowering.path_name;
                    candidate.access_descriptor.family_kind = lowering.family;
                    candidate.access_descriptor.family_tags =
                        scan_family_tags;
                    candidate.access_descriptor.taxonomy_version =
                        kPlannerFamilyTaxonomyVersion;
                    candidate.access_descriptor.exactness_class =
                        lowering.exactness_class;
                    candidate.access_descriptor.requires_recheck =
                        lowering.requires_recheck;
                    candidate.access_descriptor.coverage_fraction =
                        scan_kind == "SEQ_SCAN"
                            ? 1.0
                            : std::clamp(coverage_fraction, 0.0, 1.0);
                    candidate.access_descriptor.candidate_budget =
                        candidate_budget;
                    candidate.access_descriptor.visibility_enforcement =
                        lowering.visibility_enforcement;
                    candidate.access_descriptor.family_metrics_version =
                        candidate.runtime_relation.family_metrics_version;
                    candidate.access_descriptor.metrics_confidence_class =
                        candidate.runtime_relation.metrics_confidence_class;
                    candidate.access_descriptor.queryability_state =
                        effective_queryability;
                    candidate.access_descriptor.formula_profile_id =
                        candidate_cost.formula_profile_id;
                    candidate.access_descriptor.formula_profile_version =
                        candidate_cost.formula_profile_version;
                    candidate.access_descriptor.calibration_profile_id =
                        candidate_cost.calibration_profile_id;
                    candidate.access_descriptor.ordered_output =
                        ordered_output;
                    candidate.access_descriptor.ordered_prefix_length =
                        ordered_prefix_length;
                    candidate.access_descriptor.ordering_keys =
                        ordering_keys;
                    candidate.access_descriptor.order_complete =
                        order_complete;
                    candidate.access_descriptor.parameterized =
                        relation.lateral;
                    candidate.access_descriptor
                        .required_outer_relation_indexes =
                        required_outer_relation_indexes;
                    candidate.access_descriptor.interesting_order_score =
                        interesting_order_score;
                    candidate.access_descriptor.parallel_aware =
                        allow_search_modeled_parallel_scan &&
                        scan_kind == "SEQ_SCAN";
                    candidate.access_descriptor.parallel_stage =
                        candidate.access_descriptor.parallel_aware
                            ? "SCAN"
                            : std::string();
                    candidate.runtime_relation
                        .candidate_family_identity_signatures = {
                            candidateFamilyIdentitySignature(
                                relation_index,
                                candidate.access_descriptor)};
                    candidate.runtime_relation
                        .candidate_family_statistics_signatures = {
                            candidateFamilyStatisticsSignature(
                                relation_index,
                                candidate.access_descriptor)};
                    if (candidate.path)
                    {
                        candidate.path->setAccessDescriptor(
                            candidate.access_descriptor);
                    }
                    return candidate;
                };
            auto registerBundleCandidate =
                [&](BaseAccessChoice candidate) {
                    for (const auto &signature :
                         candidate.runtime_relation
                             .candidate_family_identity_signatures)
                    {
                        appendUniqueText(
                            bundle.candidate_family_identity_signatures,
                            signature);
                    }
                    for (const auto &signature :
                         candidate.runtime_relation
                             .candidate_family_statistics_signatures)
                    {
                        appendUniqueText(
                            bundle.candidate_family_statistics_signatures,
                            signature);
                    }
                    bundle.candidates.push_back(std::move(candidate));
                };
            auto registerParallelScanCandidate =
                [&](const BaseAccessChoice &serial_candidate) -> void {
                    if (!allow_search_modeled_parallel_scan ||
                        serial_candidate.path == nullptr ||
                        serial_candidate.plan == nullptr ||
                        serial_candidate.path->type() != PathType::SEQ_SCAN)
                    {
                        return;
                    }

                    parallel_scan_modeled_in_search = true;

                    const uint64_t parallel_rows =
                        std::max<uint64_t>(serial_candidate.runtime_relation.estimated_rows,
                                           serial_candidate.runtime_relation.base_rows);
                    const uint64_t parallel_pages =
                        std::max<uint64_t>(
                            base_pages,
                            std::max<uint64_t>(
                                1,
                                (std::max<uint64_t>(1, parallel_rows) *
                                     std::max<uint64_t>(1, relation_row_width) +
                                 planner_params.planner_page_size_bytes - 1) /
                                    planner_params.planner_page_size_bytes));
                    const auto decision = executor::evaluateParallelPlan(
                        parallel_config,
                        executor::ParallelStageKind::SCAN,
                        parallel_rows,
                        parallel_pages,
                        false,
                        false,
                        true);
                    constexpr const char *candidate_name = "PARALLEL_SEQ_SCAN";
                    if (!decision.eligible)
                    {
                        appendRuntimeTrace(rejected_paths,
                                           "PARALLEL",
                                           relation_subject,
                                           candidate_name,
                                           "REJECTED",
                                           decision.rejection_reason,
                                           serial_candidate.runtime_relation.startup_cost,
                                           serial_candidate.runtime_relation.total_cost,
                                           serial_candidate.runtime_relation.estimated_rows);
                        return;
                    }

                    CostEstimate serial_cost;
                    serial_cost.startup_cost =
                        serial_candidate.runtime_relation.startup_cost;
                    serial_cost.total_cost =
                        serial_candidate.runtime_relation.total_cost;
                    serial_cost.run_cost = std::max(
                        0.0,
                        serial_cost.total_cost - serial_cost.startup_cost);
                    serial_cost.rows =
                        serial_candidate.runtime_relation.estimated_rows;
                    CostEstimate gather_cost = relation_cost_model.costGather(
                        serial_cost,
                        std::max<uint64_t>(1, serial_cost.rows),
                        decision.workers_planned,
                        planner_controls.parallel_leader_participation,
                        ctx);
                    const double skew_penalty_cost =
                        decision.skew_penalty *
                        std::max(1.0, gather_cost.total_cost * 0.05);
                    if (skew_penalty_cost > 0.0)
                    {
                        gather_cost.run_cost += skew_penalty_cost;
                        gather_cost.total_cost =
                            gather_cost.startup_cost + gather_cost.run_cost;
                        gather_cost.expanded_terms.push_back(CostFormulaTerm{
                            "run.parallel_skew_penalty",
                            1.0,
                            skew_penalty_cost,
                            skew_penalty_cost,
                            "cost"});
                    }

                    BaseAccessChoice parallel_candidate = serial_candidate;
                    parallel_candidate.path = std::make_shared<GatherPath>(
                        serial_candidate.path,
                        decision.workers_planned,
                        planner_controls.parallel_leader_participation,
                        false,
                        gather_cost);
                    parallel_candidate.plan = std::make_shared<GatherNode>(
                        serial_candidate.plan,
                        decision.workers_planned,
                        planner_controls.parallel_leader_participation,
                        false);
                    parallel_candidate.plan->setCost(gather_cost);
                    parallel_candidate.plan->setParallelMetadata(
                        true,
                        true,
                        decision.workers_planned,
                        false,
                        "SCAN",
                        "parallel scan candidate modeled in search");

                    parallel_candidate.runtime_relation.path_name =
                        candidate_name;
                    parallel_candidate.runtime_relation.parallel_eligible = true;
                    parallel_candidate.runtime_relation.parallel_enabled = true;
                    parallel_candidate.runtime_relation.parallel_workers_planned =
                        decision.workers_planned;
                    parallel_candidate.runtime_relation.parallel_stage = "SCAN";
                    parallel_candidate.runtime_relation.parallel_rejection_reason
                        .clear();
                    parallel_candidate.runtime_relation.startup_cost =
                        gather_cost.startup_cost;
                    parallel_candidate.runtime_relation.total_cost =
                        gather_cost.total_cost;
                    parallel_candidate.runtime_relation.estimated_rows =
                        gather_cost.rows;

                    parallel_candidate.access_descriptor.path_name =
                        candidate_name;
                    parallel_candidate.access_descriptor.parallel_aware = true;
                    parallel_candidate.access_descriptor.parallel_enabled = true;
                    parallel_candidate.access_descriptor.parallel_workers_planned =
                        decision.workers_planned;
                    parallel_candidate.access_descriptor.gather_merge = false;
                    parallel_candidate.access_descriptor.parallel_stage = "SCAN";
                    if (std::find(
                            parallel_candidate.access_descriptor.family_tags.begin(),
                            parallel_candidate.access_descriptor.family_tags.end(),
                            "PARALLEL") ==
                        parallel_candidate.access_descriptor.family_tags.end())
                    {
                        parallel_candidate.access_descriptor.family_tags.push_back(
                            "PARALLEL");
                    }
                    parallel_candidate.runtime_relation.scan_family_tags =
                        parallel_candidate.access_descriptor.family_tags;
                    parallel_candidate.runtime_relation
                        .candidate_family_identity_signatures = {
                            candidateFamilyIdentitySignature(
                                relation_index,
                                parallel_candidate.access_descriptor)};
                    parallel_candidate.runtime_relation
                        .candidate_family_statistics_signatures = {
                            candidateFamilyStatisticsSignature(
                                relation_index,
                                parallel_candidate.access_descriptor)};
                    if (parallel_candidate.path)
                    {
                        parallel_candidate.path->setAccessDescriptor(
                            parallel_candidate.access_descriptor);
                    }
                    appendUniqueText(parallel_candidate.candidate_scan_families,
                                     candidate_name);
                    appendUniqueText(bundle.candidate_scan_families,
                                     candidate_name);
                    appendRuntimeTrace(considered_paths,
                                       "PARALLEL",
                                       relation_subject,
                                       candidate_name,
                                       "CONSIDERED",
                                       "workers=" +
                                           std::to_string(
                                               decision.workers_planned) +
                                           " modeled alongside serial scan",
                                       gather_cost.startup_cost,
                                       gather_cost.total_cost,
                                       gather_cost.rows);
                    registerBundleCandidate(std::move(parallel_candidate));
                };
            appendUniqueText(bundle.candidate_scan_families,
                             best_lowering.family_name);
            for (const auto &index : relation.indexes)
            {
                if (index.is_partial_index && partialIndexMatches(index))
                {
                    appendUniqueText(bundle.candidate_scan_families,
                                     "PARTIAL_INDEX_SCAN");
                }
                if (index.is_expression_index && expressionIndexMatches(index))
                {
                    appendUniqueText(bundle.candidate_scan_families,
                                     "EXPRESSION_INDEX_SCAN");
                }
            }
            registerBundleCandidate(makeBaseAccessChoice(
                seq_path,
                seq_plan,
                best_cost,
                best_scan_kind,
                best_scan_family,
                best_lowering,
                best_scan_family_tags,
                {},
                std::string(),
                false,
                false,
                false,
                0,
                {},
                false,
                0.0,
                {},
                {},
                1.0,
                0,
                core::CatalogManager::IndexInfo{},
                {best_scan_kind, best_scan_family}));
            registerParallelScanCandidate(bundle.candidates.back());

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

            auto loweringRequestForPredicate =
                [&](const core::CatalogManager::IndexInfo &index,
                    const ResolvedScanPredicate &predicate,
                    bool ordered_output,
                    bool skip_scan) -> PlannerFamilyLoweringRequest {
                    return buildPlannerFamilyLoweringRequest(
                        db_ != nullptr ? db_->catalog_manager() : nullptr,
                        index,
                        predicateMatchShape(predicate.kind),
                        predicate.operator_name,
                        ordered_output,
                        skip_scan);
                };

            auto expressionContainsAnyToken =
                [&](const parser::v3::Expression *expr,
                    std::initializer_list<std::string_view> needles) -> bool {
                    if (expr == nullptr)
                    {
                        return false;
                    }
                    const std::string normalized =
                        core::IdentifierUtils::toUpper(
                            expressionToString(expr, pool));
                    if (normalized.empty())
                    {
                        return false;
                    }
                    for (const auto needle : needles)
                    {
                        if (normalized.find(needle) != std::string::npos)
                        {
                            return true;
                        }
                    }
                    return false;
                };

            auto queryRequestsTextRanking = [&]() -> bool {
                for (const auto *item : select_stmt->items)
                {
                    if (item != nullptr && item->expr != nullptr &&
                        expressionContainsAnyToken(
                            item->expr,
                            {"TS_RANK(", "SEARCH_BM25("}))
                    {
                        return true;
                    }
                }
                for (const auto *order_item : select_stmt->order_by)
                {
                    if (order_item != nullptr && order_item->expr != nullptr &&
                        expressionContainsAnyToken(
                            order_item->expr,
                            {"TS_RANK(", "SEARCH_BM25("}))
                    {
                        return true;
                    }
                }
                return false;
            };

            auto queryRequestsVectorNearestOrder = [&]() -> bool {
                for (const auto *order_item : select_stmt->order_by)
                {
                    if (order_item != nullptr && order_item->expr != nullptr &&
                        expressionContainsAnyToken(
                            order_item->expr,
                            {"VECTOR_L2_DISTANCE(",
                             "VECTOR_COSINE_DISTANCE(",
                             "<->"}))
                    {
                        return true;
                    }
                }
                return false;
            };

            auto isTextFamilyIndex =
                [](core::CatalogManager::IndexType index_type) -> bool {
                    switch (index_type)
                    {
                        case core::CatalogManager::IndexType::FULLTEXT:
                        case core::CatalogManager::IndexType::INVERTED:
                        case core::CatalogManager::IndexType::MINHASH_LSH:
                        case core::CatalogManager::IndexType::SPARSE_INVERTED:
                        case core::CatalogManager::IndexType::SPARSE_WAND:
                        case core::CatalogManager::IndexType::TRIE:
                        case core::CatalogManager::IndexType::NGRAM:
                        case core::CatalogManager::IndexType::MONGODB_WILDCARD:
                        case core::CatalogManager::IndexType::MONGODB_ENCRYPTED_RANGE:
                        case core::CatalogManager::IndexType::NEO4J_TEXT:
                        case core::CatalogManager::IndexType::CASSANDRA_SASI:
                        case core::CatalogManager::IndexType::CASSANDRA_SAI:
                            return true;
                        default:
                            return false;
                    }
                };

            auto isAnnFamilyIndex =
                [](core::CatalogManager::IndexType index_type) -> bool {
                    switch (index_type)
                    {
                        case core::CatalogManager::IndexType::VECTOR_FLAT:
                        case core::CatalogManager::IndexType::VECTOR_BIN_FLAT:
                        case core::CatalogManager::IndexType::HNSW:
                        case core::CatalogManager::IndexType::RHNSW_PQ:
                        case core::CatalogManager::IndexType::RHNSW_SQ:
                        case core::CatalogManager::IndexType::IVF:
                        case core::CatalogManager::IndexType::IVF_FLAT:
                        case core::CatalogManager::IndexType::BIN_IVF_FLAT:
                        case core::CatalogManager::IndexType::IVF_PQ:
                        case core::CatalogManager::IndexType::IVF_SQ8:
                        case core::CatalogManager::IndexType::IVF_SQ8_HYBRID:
                        case core::CatalogManager::IndexType::ANNOY:
                        case core::CatalogManager::IndexType::NSG:
                        case core::CatalogManager::IndexType::DISKANN:
                        case core::CatalogManager::IndexType::SCANN:
                        case core::CatalogManager::IndexType::GPU_CAGRA:
                        case core::CatalogManager::IndexType::NEO4J_VECTOR:
                            return true;
                        default:
                            return false;
                    }
                };

            const bool query_requests_text_ranking = queryRequestsTextRanking();
            const bool query_requests_ann_order =
                queryRequestsVectorNearestOrder();

            auto supportsExactLookup =
                [&](const PlannerFamilyLoweringResult &lowering,
                    const ResolvedScanPredicate &predicate) -> bool {
                    return lowering.queryability_state !=
                               AccessPathQueryabilityState::INVALID &&
                           lowering.exactness_class ==
                               AccessPathExactnessClass::EXACT_KEY &&
                           predicateSupportsExactLookup(predicate);
                };

            auto indexOrderingKeys =
                [&](const core::CatalogManager::IndexInfo &index,
                    const PlannerFamilyLoweringResult &lowering)
                    -> std::vector<AccessPathDescriptor::OrderingKey> {
                    std::vector<AccessPathDescriptor::OrderingKey> keys;
                    if (!lowering.supports_ordering)
                    {
                        return keys;
                    }
                    for (const auto &column_id : index.column_ids)
                    {
                        const std::string column_name = columnNameForId(column_id);
                        if (column_name.empty())
                        {
                            break;
                        }
                        keys.push_back(makeOrderingKey(column_name));
                    }
                    return keys;
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
                const bool multicolumn_prefix_match =
                    index.column_ids.size() > 1 &&
                    !index.column_ids.empty() &&
                    index.column_ids.front() == predicate.column_id;
                const uint64_t ordered_prefix_length =
                    orderedPrefixLengthForIndex(index);
                IndexFamilyMetricsPacket costing_index_metrics_packet;
                const bool have_costing_index_metrics =
                    loadIndexFamilyMetricsPacket(index,
                                                 costing_index_metrics_packet);
                auto lowering_request =
                    loweringRequestForPredicate(index,
                                               predicate,
                                               ordered_prefix_length > 0,
                                               false);
                if (isTextFamilyIndex(index.index_type))
                {
                    const double postings_rows_est = std::max(
                        1.0,
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "avg_postings_per_term",
                                           static_cast<double>(
                                               std::max<uint64_t>(1,
                                                                  expected_rows))));
                    const double score_rows_est = std::max(
                        postings_rows_est,
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "score_rows_est",
                                           postings_rows_est));
                    lowering_request.ranking_requested =
                        query_requests_text_ranking;
                    lowering_request.corpus_stats_available =
                        have_costing_index_metrics &&
                        familyMetricDouble(index,
                                           &costing_index_metrics_packet,
                                           "avg_postings_per_term",
                                           0.0) > 0.0 &&
                        familyMetricDouble(index,
                                           &costing_index_metrics_packet,
                                           "score_rows_est",
                                           0.0) > 0.0;
                    lowering_request.candidate_bitmap_available =
                        predicate.kind == ResolvedPredicateKind::EQUALITY ||
                        postings_rows_est > 0.0;
                    lowering_request.candidate_budget = std::max<uint64_t>(
                        1,
                        static_cast<uint64_t>(std::ceil(
                            query_requests_text_ranking
                                ? score_rows_est
                                : postings_rows_est)));
                }
                else if (isAnnFamilyIndex(index.index_type))
                {
                    const double candidate_budget_default = std::max(
                        0.0,
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "candidate_budget_default",
                                           0.0));
                    const double recall_estimate = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "recall_estimate_at_k",
                                           1.0),
                        0.0,
                        1.0);
                    const double segment_coverage_fraction = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "segment_coverage_fraction",
                                           have_costing_index_metrics
                                               ? costing_index_metrics_packet
                                                     .coverage_fraction
                                               : 0.0),
                        0.0,
                        1.0);
                    lowering_request.nearest_order =
                        query_requests_ann_order;
                    lowering_request.candidate_budget =
                        query_requests_ann_order
                            ? std::max<uint64_t>(
                                  1,
                                  static_cast<uint64_t>(std::ceil(
                                      candidate_budget_default > 0.0
                                          ? candidate_budget_default
                                          : std::sqrt(static_cast<double>(
                                                std::max<uint64_t>(
                                                    1,
                                                    expected_rows))))))
                            : 0;
                    lowering_request.ann_metric_compatible =
                        query_requests_ann_order &&
                        have_costing_index_metrics;
                    lowering_request.ann_rerank_enabled =
                        query_requests_ann_order &&
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "rerank_fraction",
                                           0.0) > 0.0;
                    lowering_request.ann_exact_fallback =
                        query_requests_ann_order &&
                        index.index_type !=
                            core::CatalogManager::IndexType::VECTOR_FLAT &&
                        index.index_type !=
                            core::CatalogManager::IndexType::VECTOR_BIN_FLAT &&
                        (recall_estimate < 0.90 ||
                         segment_coverage_fraction < 0.95);
                }
                const PlannerFamilyLoweringResult lowered_candidate =
                    lowerPlannerFamily(lowering_request);
                if (lowered_candidate.queryability_state ==
                    AccessPathQueryabilityState::INVALID)
                {
                    continue;
                }
                const bool exact_key_lookup =
                    supportsExactLookup(lowered_candidate, predicate);
                const bool partial_index_match = partialIndexMatches(index);
                const bool expression_index_match =
                    expressionIndexMatches(index);
                const std::string lowered_trace_family =
                    lowered_candidate.family_name.empty()
                        ? std::string("INDEX_SCAN")
                        : lowered_candidate.family_name;
                if (index.is_partial_index && !partial_index_match)
                {
                    appendRuntimeTrace(rejected_paths,
                                       "ACCESS_PATH",
                                       relation_subject,
                                       accessTraceCandidateLabel(
                                           lowered_trace_family,
                                           index.index_name,
                                           std::string()),
                                       "REJECTED",
                                       "partial index predicate not implied by relation filter",
                                       0.0,
                                       0.0,
                                       0.0);
                    continue;
                }
                if (index.is_expression_index && !expression_index_match)
                {
                    appendRuntimeTrace(rejected_paths,
                                       "ACCESS_PATH",
                                       relation_subject,
                                       accessTraceCandidateLabel(
                                           lowered_trace_family,
                                           index.index_name,
                                           std::string()),
                                       "REJECTED",
                                       "expression index expression not present in relation filter",
                                       0.0,
                                       0.0,
                                       0.0);
                    continue;
                }
                const auto candidate_ordering_keys =
                    indexOrderingKeys(index, lowered_candidate);
                const bool candidate_order_complete =
                    lowered_candidate.supports_ordering &&
                    !candidate_ordering_keys.empty();
                const double candidate_interesting_order_score =
                    candidate_order_complete
                        ? static_cast<double>(
                              std::max<uint64_t>(ordered_prefix_length,
                                                 candidate_ordering_keys.size()))
                        : 0.0;
                const IndexFamilyCostCalibrationInput family_cost_input =
                    buildIndexFamilyCostInput(
                        lowered_candidate,
                        index,
                        have_costing_index_metrics
                            ? &costing_index_metrics_packet
                            : nullptr,
                        relation_row_width,
                        relation_heap_rows_per_page,
                        covering_index,
                        candidate_order_complete);
                CostModel candidate_cost_model(
                    deriveIndexFamilyFormulaProfile(planner_params,
                                                    family_cost_input));
                uint64_t cost_index_height = 3;
                uint64_t cost_index_pages = index_pages;
                uint64_t cost_index_tuples = expected_rows;
                uint64_t cost_heap_pages = heap_pages;
                uint64_t cost_heap_tuples = expected_rows;
                double cost_correlation = correlation;
                double cost_qual_cost = qual_cost;
                if (have_costing_index_metrics)
                {
                    const uint64_t row_count_basis =
                        std::max<uint64_t>(
                            {base_rows,
                             costing_index_metrics_packet.row_count_est,
                             costing_index_metrics_packet.live_entry_count_est});
                    const double recheck_ratio =
                        lowered_candidate.requires_recheck
                            ? std::max(0.0,
                                       costing_index_metrics_packet
                                           .recheck_ratio_est)
                            : 0.0;
                    cost_index_height = std::max<uint64_t>(
                        1, costing_index_metrics_packet.height);
                    cost_index_pages = std::max<uint64_t>(
                        1,
                        static_cast<uint64_t>(std::ceil(
                            static_cast<double>(std::max<uint64_t>(
                                1, costing_index_metrics_packet.leaf_pages)) *
                            (1.0 + std::clamp(
                                       costing_index_metrics_packet.bloat_ratio,
                                       0.0,
                                       1.0)))));
                    cost_index_tuples =
                        relationOutputRows(row_count_basis,
                                           predicate_selectivity);
                    cost_heap_tuples = relationOutputRows(
                        row_count_basis,
                        std::min(1.0,
                                 predicate_selectivity * (1.0 + recheck_ratio)));
                    if (base_rows > 0)
                    {
                        cost_heap_pages = std::max<uint64_t>(
                            1,
                            static_cast<uint64_t>(std::ceil(
                                static_cast<double>(base_pages) *
                                (static_cast<double>(cost_heap_tuples) /
                                 static_cast<double>(base_rows)))));
                    }
                    cost_correlation = costing_index_metrics_packet.correlation;
                    cost_qual_cost *= 1.0 + recheck_ratio;
                }

                CostEstimate candidate_cost{};
                std::string scan_kind = "INDEX_SCAN";
                double runtime_coverage_fraction =
                    covering_index ? 1.0 : 0.0;
                uint64_t runtime_candidate_budget = 0;
                if (lowered_candidate.family == PlannerAccessFamily::LSM_EQ_SCAN ||
                    lowered_candidate.family == PlannerAccessFamily::LSM_RANGE_SCAN ||
                    lowered_candidate.family ==
                        PlannerAccessFamily::LSM_ORDERED_RANGE_SCAN)
                {
                    candidate_cost = candidate_cost_model.costLSMScan(
                        cost_index_height,
                        std::max<uint64_t>(
                            1, std::max<uint64_t>(1, cost_index_pages) /
                                   std::max<uint64_t>(1, cost_index_height)),
                        cost_index_tuples,
                        cost_heap_pages,
                        cost_heap_tuples,
                        cost_qual_cost,
                        cost_correlation,
                        ctx);
                    scan_kind = "LSM_SCAN";
                }
                else if (lowered_candidate.family ==
                             PlannerAccessFamily::BRIN_SCAN ||
                         lowered_candidate.family ==
                             PlannerAccessFamily::SUMMARY_FILTER_SCAN)
                {
                    const double prune_ratio_est = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "prune_ratio_est",
                                           have_costing_index_metrics
                                               ? (1.0 - costing_index_metrics_packet
                                                            .dead_fraction)
                                               : (1.0 - predicate_selectivity)),
                        0.0,
                        1.0);
                    const double unsummarized_range_fraction = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "unsummarized_range_fraction",
                                           0.0),
                        0.0,
                        1.0);
                    const double summary_staleness_fraction = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "summary_staleness_fraction",
                                           0.0),
                        0.0,
                        1.0);
                    const uint64_t pages_per_range = std::max<uint64_t>(
                        1,
                        static_cast<uint64_t>(std::ceil(
                            familyMetricDouble(index,
                                               have_costing_index_metrics
                                                   ? &costing_index_metrics_packet
                                                   : nullptr,
                                               "pages_per_range",
                                               static_cast<double>(
                                                   std::max<uint64_t>(
                                                       1,
                                                       cost_index_pages))))));
                    const double candidate_fraction = std::clamp(
                        (1.0 - prune_ratio_est) + unsummarized_range_fraction +
                            summary_staleness_fraction,
                        0.001,
                        1.0);
                    cost_heap_tuples =
                        relationOutputRows(base_rows, candidate_fraction);
                    cost_heap_pages = std::max<uint64_t>(
                        1,
                        static_cast<uint64_t>(std::ceil(
                            static_cast<double>(base_pages) *
                            candidate_fraction)));
                    const uint64_t summary_pages_read = std::max<uint64_t>(
                        1,
                        static_cast<uint64_t>(std::ceil(
                            static_cast<double>(cost_heap_pages) /
                            static_cast<double>(pages_per_range))));
                    candidate_cost = candidate_cost_model.costSummaryScan(
                        summary_pages_read,
                        cost_heap_pages,
                        cost_heap_tuples,
                        cost_qual_cost,
                        unsummarized_range_fraction,
                        summary_staleness_fraction,
                        ctx);
                    scan_kind = lowered_candidate.family_name;
                    runtime_coverage_fraction =
                        have_costing_index_metrics &&
                                costing_index_metrics_packet.coverage_fraction >
                                    0.0
                            ? costing_index_metrics_packet.coverage_fraction
                            : std::clamp(1.0 - candidate_fraction, 0.0, 1.0);
                    runtime_candidate_budget = summary_pages_read;
                }
                else if (lowered_candidate.family ==
                         PlannerAccessFamily::BITMAP_STORAGE_SCAN)
                {
                    const double bitmap_density = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "bitmap_density",
                                           have_costing_index_metrics
                                               ? costing_index_metrics_packet
                                                     .coverage_fraction
                                               : predicate_selectivity),
                        0.0,
                        1.0);
                    const double bitmap_false_positive_ratio = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "bitmap_false_positive_ratio",
                                           have_costing_index_metrics
                                               ? costing_index_metrics_packet
                                                     .recheck_ratio_est
                                               : 0.10),
                        0.0,
                        1.0);
                    const double lossy_container_fraction = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "lossy_container_fraction",
                                           bitmap_false_positive_ratio),
                        0.0,
                        1.0);
                    const double candidate_fraction = std::clamp(
                        std::max(predicate_selectivity, bitmap_density) +
                            bitmap_false_positive_ratio +
                            (lossy_container_fraction * 0.10),
                        0.001,
                        1.0);
                    cost_heap_tuples =
                        relationOutputRows(base_rows, candidate_fraction);
                    cost_heap_pages = std::max<uint64_t>(
                        1,
                        static_cast<uint64_t>(std::ceil(
                            static_cast<double>(base_pages) *
                            candidate_fraction)));
                    candidate_cost =
                        candidate_cost_model.costBitmapStorageScan(
                            std::max<uint64_t>(1, cost_index_pages),
                            cost_heap_pages,
                            cost_heap_tuples,
                            cost_qual_cost,
                            lossy_container_fraction,
                            bitmap_false_positive_ratio,
                            ctx);
                    scan_kind = lowered_candidate.family_name;
                    runtime_coverage_fraction =
                        have_costing_index_metrics &&
                                costing_index_metrics_packet.coverage_fraction >
                                    0.0
                            ? costing_index_metrics_packet.coverage_fraction
                            : bitmap_density;
                    runtime_candidate_budget = cost_heap_pages;
                }
                else if (lowered_candidate.family ==
                         PlannerAccessFamily::COLUMNSTORE_SCAN)
                {
                    const double column_bytes_pruned_ratio = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "column_bytes_pruned_ratio",
                                           0.50),
                        0.0,
                        1.0);
                    const double row_groups_touched_ratio = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "row_groups_touched_ratio",
                                           have_costing_index_metrics
                                               ? costing_index_metrics_packet
                                                     .coverage_fraction
                                               : predicate_selectivity),
                        0.0,
                        1.0);
                    const double late_materialization_gain_est = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "late_materialization_gain_est",
                                           0.50),
                        0.0,
                        1.0);
                    const double projection_width_bytes = std::max(
                        1.0,
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "projection_width_bytes",
                                           16.0));
                    const double delta_fraction = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "delta_fraction",
                                           0.0),
                        0.0,
                        1.0);
                    const double candidate_fraction = std::clamp(
                        std::max(predicate_selectivity,
                                 row_groups_touched_ratio) +
                            (delta_fraction * 0.10),
                        0.001,
                        1.0);
                    cost_heap_tuples =
                        relationOutputRows(base_rows, candidate_fraction);
                    const uint64_t bytes_read_est = std::max<uint64_t>(
                        1,
                        static_cast<uint64_t>(std::ceil(
                            static_cast<double>(std::max<uint64_t>(
                                1, base_rows)) *
                            projection_width_bytes *
                            std::max(0.01, row_groups_touched_ratio) *
                            std::max(0.05,
                                     1.0 - column_bytes_pruned_ratio))));
                    candidate_cost =
                        candidate_cost_model.costColumnstoreScan(
                            bytes_read_est,
                            cost_heap_tuples,
                            cost_qual_cost,
                            column_bytes_pruned_ratio,
                            late_materialization_gain_est,
                            delta_fraction,
                            ctx);
                    scan_kind = lowered_candidate.family_name;
                    runtime_coverage_fraction =
                        have_costing_index_metrics &&
                                costing_index_metrics_packet.coverage_fraction >
                                    0.0
                            ? costing_index_metrics_packet.coverage_fraction
                            : std::clamp(1.0 - row_groups_touched_ratio,
                                         0.0,
                                         1.0);
                    runtime_candidate_budget = std::max<uint64_t>(
                        1,
                        static_cast<uint64_t>(std::ceil(
                            std::max(1.0,
                                     row_groups_touched_ratio *
                                         static_cast<double>(
                                             std::max<uint64_t>(
                                                 1,
                                                 cost_index_pages))))));
                }
                else if (lowered_candidate.family ==
                             PlannerAccessFamily::GIN_FILTER_SCAN ||
                         lowered_candidate.family ==
                             PlannerAccessFamily::TEXT_BITMAP_SCAN ||
                         lowered_candidate.family ==
                             PlannerAccessFamily::TEXT_SCORE_SCAN ||
                         lowered_candidate.family ==
                             PlannerAccessFamily::TEXT_RECHECK_SCAN)
                {
                    const double avg_postings_per_term = std::max(
                        1.0,
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "avg_postings_per_term",
                                           static_cast<double>(
                                               std::max<uint64_t>(1,
                                                                  expected_rows))));
                    const double score_rows_est = std::max(
                        avg_postings_per_term,
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "score_rows_est",
                                           avg_postings_per_term));
                    const double phrase_hit_rate = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "phrase_hit_rate",
                                           0.5),
                        0.0,
                        1.0);
                    const double merge_debt = std::max(
                        0.0,
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "merge_debt",
                                           0.0));
                    runtime_candidate_budget = std::max<uint64_t>(
                        1,
                        lowering_request.candidate_budget > 0
                            ? lowering_request.candidate_budget
                            : static_cast<uint64_t>(std::ceil(
                                  lowered_candidate.family ==
                                          PlannerAccessFamily::TEXT_SCORE_SCAN
                                      ? score_rows_est
                                      : avg_postings_per_term)));
                    const double candidate_fraction = std::clamp(
                        static_cast<double>(runtime_candidate_budget) /
                            static_cast<double>(
                                std::max<uint64_t>(1, base_rows)),
                        0.001,
                        1.0);
                    cost_index_tuples =
                        std::max<uint64_t>(1, runtime_candidate_budget);
                    cost_heap_tuples =
                        std::max<uint64_t>(1, runtime_candidate_budget);
                    cost_heap_pages = std::max<uint64_t>(
                        1,
                        static_cast<uint64_t>(std::ceil(
                            static_cast<double>(base_pages) *
                            candidate_fraction)));
                    cost_qual_cost *=
                        1.0 +
                        (lowered_candidate.family ==
                                 PlannerAccessFamily::TEXT_SCORE_SCAN
                             ? 0.50
                             : 0.15) +
                        std::min(1.0,
                                 merge_debt /
                                     std::max(1.0, score_rows_est)) +
                        ((1.0 - phrase_hit_rate) * 0.25);
                    candidate_cost = candidate_cost_model.costIndexScan(
                        cost_index_height,
                        cost_index_pages,
                        cost_index_tuples,
                        cost_heap_pages,
                        cost_heap_tuples,
                        cost_qual_cost,
                        cost_correlation,
                        ctx);
                    scan_kind = lowered_candidate.family_name;
                    runtime_coverage_fraction =
                        have_costing_index_metrics &&
                                costing_index_metrics_packet.coverage_fraction >
                                    0.0
                            ? costing_index_metrics_packet.coverage_fraction
                            : std::clamp(1.0 - candidate_fraction, 0.0, 1.0);
                }
                else if (lowered_candidate.family ==
                             PlannerAccessFamily::VECTOR_FLAT_SCAN ||
                         lowered_candidate.family ==
                             PlannerAccessFamily::HNSW_SCAN ||
                         lowered_candidate.family ==
                             PlannerAccessFamily::IVF_SCAN ||
                         lowered_candidate.family ==
                             PlannerAccessFamily::ANN_RERANK_SCAN ||
                         lowered_candidate.family ==
                             PlannerAccessFamily::ANN_HYBRID_FALLBACK_SCAN)
                {
                    const double avg_candidates_scanned = std::max(
                        1.0,
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "avg_candidates_scanned",
                                           static_cast<double>(
                                               std::max<uint64_t>(
                                                   1,
                                                   lowering_request
                                                       .candidate_budget))));
                    const double rerank_fraction = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "rerank_fraction",
                                           0.0),
                        0.0,
                        1.0);
                    const double recall_estimate = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "recall_estimate_at_k",
                                           lowered_candidate.family ==
                                                   PlannerAccessFamily::
                                                       VECTOR_FLAT_SCAN ||
                                               lowered_candidate.family ==
                                                   PlannerAccessFamily::
                                                       ANN_HYBRID_FALLBACK_SCAN
                                               ? 1.0
                                               : 0.85),
                        0.0,
                        1.0);
                    const double deleted_node_fraction = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "deleted_node_fraction",
                                           0.0),
                        0.0,
                        1.0);
                    const double segment_coverage_fraction = std::clamp(
                        familyMetricDouble(index,
                                           have_costing_index_metrics
                                               ? &costing_index_metrics_packet
                                               : nullptr,
                                           "segment_coverage_fraction",
                                           have_costing_index_metrics
                                               ? costing_index_metrics_packet
                                                     .coverage_fraction
                                               : recall_estimate),
                        0.0,
                        1.0);
                    runtime_candidate_budget = std::max<uint64_t>(
                        1,
                        lowering_request.candidate_budget > 0
                            ? lowering_request.candidate_budget
                            : static_cast<uint64_t>(
                                  std::ceil(avg_candidates_scanned)));
                    const double candidate_fraction = std::clamp(
                        static_cast<double>(runtime_candidate_budget) /
                            static_cast<double>(
                                std::max<uint64_t>(1, base_rows)),
                        0.001,
                        1.0);
                    cost_index_tuples =
                        std::max<uint64_t>(1, runtime_candidate_budget);
                    cost_heap_tuples =
                        std::max<uint64_t>(1, runtime_candidate_budget);
                    cost_heap_pages = std::max<uint64_t>(
                        1,
                        static_cast<uint64_t>(std::ceil(
                            static_cast<double>(base_pages) *
                            candidate_fraction)));
                    cost_qual_cost *=
                        1.0 + rerank_fraction + (deleted_node_fraction * 0.25);
                    candidate_cost = candidate_cost_model.costIndexScan(
                        cost_index_height,
                        cost_index_pages,
                        cost_index_tuples,
                        cost_heap_pages,
                        cost_heap_tuples,
                        cost_qual_cost,
                        cost_correlation,
                        ctx);
                    scan_kind = lowered_candidate.family_name;
                    runtime_coverage_fraction =
                        lowered_candidate.family ==
                                PlannerAccessFamily::VECTOR_FLAT_SCAN ||
                                lowered_candidate.family ==
                                    PlannerAccessFamily::
                                        ANN_HYBRID_FALLBACK_SCAN
                            ? 1.0
                            : std::clamp(
                                  std::max(recall_estimate,
                                           segment_coverage_fraction),
                                  0.0,
                                  1.0);
                }
                else if (covering_index && lowered_candidate.supports_covering)
                {
                    candidate_cost = candidate_cost_model.costIndexOnlyScan(
                        cost_index_height,
                        cost_index_pages,
                        cost_index_tuples,
                        cost_qual_cost,
                        cost_correlation,
                        ctx);
                    scan_kind = "INDEX_ONLY_SCAN";
                    runtime_coverage_fraction = 1.0;
                }
                else
                {
                    candidate_cost = candidate_cost_model.costIndexScan(
                        cost_index_height,
                        cost_index_pages,
                        cost_index_tuples,
                        cost_heap_pages,
                        cost_heap_tuples,
                        cost_qual_cost,
                        cost_correlation,
                        ctx);
                }
                if (relation_stats_penalty > 1.0)
                {
                    candidate_cost.startup_cost *= relation_stats_penalty;
                    candidate_cost.total_cost *= relation_stats_penalty;
                    appendRuntimeTrace(considered_paths,
                                       "STATS_FRESHNESS",
                                       relation_subject,
                                       accessTraceCandidateLabel(scan_kind,
                                                                index.index_name,
                                                                std::string()),
                                       "ADJUSTED",
                                       relation_stats_penalty_reason,
                                       candidate_cost.startup_cost,
                                                               candidate_cost.total_cost,
                                                               candidate_cost.rows);
                }
                std::vector<std::string> candidate_scan_families;
                appendUniqueText(candidate_scan_families, scan_kind);
                if (multicolumn_prefix_match)
                {
                    appendUniqueText(candidate_scan_families,
                                     "MULTICOLUMN_PREFIX_INDEX_SCAN");
                }
                appendUniqueText(candidate_scan_families,
                                 lowered_candidate.family_name);
                if (partial_index_match)
                {
                    appendUniqueText(candidate_scan_families,
                                     "PARTIAL_INDEX_SCAN");
                }
                if (expression_index_match)
                {
                    appendUniqueText(candidate_scan_families,
                                     "EXPRESSION_INDEX_SCAN");
                }
                std::string candidate_scan_family =
                    lowered_candidate.family_name.empty()
                        ? scan_kind
                        : lowered_candidate.family_name;
                for (const auto &family : candidate_scan_families)
                {
                    appendUniqueText(bundle.candidate_scan_families, family);
                }
                const double candidate_mga_penalty = applyMgaCostPenalty(
                    candidate_cost,
                    relation_mga_pressure,
                    scan_kind);
                if (candidate_mga_penalty > 0.0)
                {
                    appendRuntimeTrace(considered_paths,
                                       "MGA_COSTING",
                                       relation_subject,
                                       accessTraceCandidateLabel(scan_kind,
                                                                index.index_name,
                                                                std::string()),
                                       "ADJUSTED",
                                       "canonical MGA telemetry penalty=" +
                                           std::to_string(candidate_mga_penalty) + " " +
                                           describeMgaCostPressure(
                                               relation_mga_pressure),
                                       candidate_cost.startup_cost,
                                       candidate_cost.total_cost,
                                       candidate_cost.rows);
                }
                const MgaAccessGovernanceDecision candidate_mga_governance =
                    evaluateMgaAccessGovernance(
                        relation_mga_pressure,
                        lowered_candidate,
                        scan_kind,
                        exact_key_lookup,
                        candidate_order_complete || ordered_prefix_length > 0,
                        candidate_cost.rows,
                        base_rows);
                if (candidate_mga_governance.reject_candidate)
                {
                    appendRuntimeTrace(rejected_paths,
                                       "MGA_SWITCHOVER",
                                       relation_subject,
                                       accessTraceCandidateLabel(scan_kind,
                                                                index.index_name,
                                                                std::string()),
                                       "REJECTED",
                                       candidate_mga_governance.reason,
                                       candidate_cost.startup_cost,
                                       candidate_cost.total_cost,
                                       candidate_cost.rows);
                    continue;
                }

                appendRuntimeTrace(considered_paths,
                                   "ACCESS_PATH",
                                   relation_subject,
                                   accessTraceCandidateLabel(scan_kind,
                                                            index.index_name,
                                                            std::string()),
                                   "CONSIDERED",
                                   predicate.predicate_text,
                                   candidate_cost.startup_cost,
                                   candidate_cost.total_cost,
                                   candidate_cost.rows);
                const RuntimePlanIndexPredicate candidate_runtime_predicate =
                    makeRuntimePredicate(predicate);
                const std::vector<RuntimePlanIndexPredicate>
                    candidate_runtime_predicates = {candidate_runtime_predicate};
                std::shared_ptr<Path> candidate_path;
                std::shared_ptr<PlanNode> candidate_plan;
                if (scan_kind == "INDEX_ONLY_SCAN")
                {
                    candidate_path = std::make_shared<IndexOnlyScanPath>(
                        relation.table_info.table_id,
                        relation_name,
                        index.index_id,
                        index.index_name,
                        cost_index_height,
                        cost_index_pages,
                        cost_index_tuples,
                        cost_qual_cost,
                        cost_correlation,
                        candidate_cost);
                    auto index_plan =
                        std::make_shared<IndexOnlyScanNode>(
                            relation.table_info.table_id,
                            relation_name,
                            index.index_id,
                            index.index_name);
                    index_plan->setIndexCond(predicate.predicate_text);
                    index_plan->setFilter(buildPredicateText(predicate_index));
                    index_plan->setCost(candidate_cost);
                    candidate_plan = index_plan;
                }
                else
                {
                    candidate_path = std::make_shared<IndexScanPath>(
                        relation.table_info.table_id,
                        relation_name,
                        index.index_id,
                        index.index_name,
                        cost_index_height,
                        cost_index_pages,
                        cost_index_tuples,
                        cost_heap_pages,
                        cost_heap_tuples,
                        cost_qual_cost,
                        cost_correlation,
                        candidate_cost);
                    auto index_plan = std::make_shared<IndexScanNode>(
                        relation.table_info.table_id,
                        relation_name,
                        index.index_id,
                        index.index_name);
                    index_plan->setIndexQualCost(cost_qual_cost);
                    index_plan->setHeapQualCost(cost_qual_cost);
                    index_plan->setCorrelation(cost_correlation);
                    index_plan->setIndexCond(predicate.predicate_text);
                    index_plan->setFilter(buildPredicateText(predicate_index));
                    index_plan->setCost(candidate_cost);
                    candidate_plan = index_plan;
                }
                registerBundleCandidate(makeBaseAccessChoice(
                    candidate_path,
                    candidate_plan,
                    candidate_cost,
                    scan_kind,
                    candidate_scan_family,
                    lowered_candidate,
                    candidate_scan_families,
                    candidate_runtime_predicates,
                    std::string(),
                    covering_index,
                    exact_key_lookup,
                    candidate_order_complete || ordered_prefix_length > 0,
                    ordered_prefix_length,
                    candidate_ordering_keys,
                    candidate_order_complete,
                    candidate_interesting_order_score,
                    {},
                    {},
                    runtime_coverage_fraction,
                    runtime_candidate_budget,
                    index,
                    candidate_scan_families));

                if (candidate_cost.total_cost > best_cost.total_cost)
                {
                    appendRuntimeTrace(rejected_paths,
                                       "ACCESS_PATH",
                                       relation_subject,
                                       accessTraceCandidateLabel(scan_kind,
                                                                index.index_name,
                                                                std::string()),
                                       "REJECTED",
                                       "higher total cost than current best",
                                       candidate_cost.startup_cost,
                                       candidate_cost.total_cost,
                                       candidate_cost.rows);
                    continue;
                }

                matched_index = index;
                best_cost = candidate_cost;
                best_scan_kind = scan_kind;
                best_scan_family = candidate_scan_family;
                best_lowering = lowered_candidate;
                best_scan_family_tags = candidate_scan_families;
                best_scan_family_tags.erase(
                    std::remove(best_scan_family_tags.begin(),
                                best_scan_family_tags.end(),
                                best_scan_family),
                    best_scan_family_tags.end());
                best_runtime_predicates = candidate_runtime_predicates;
                best_bitmap_op.clear();
                best_covering_index = covering_index;
                best_exact_key_lookup = exact_key_lookup;
                best_coverage_fraction = runtime_coverage_fraction;
                best_candidate_budget = runtime_candidate_budget;
                best_ordered_output =
                    candidate_order_complete || ordered_prefix_length > 0;
                best_ordered_prefix_length = ordered_prefix_length;
                best_ordering_keys = candidate_ordering_keys;
                best_order_complete = candidate_order_complete;
                best_interesting_order_score = candidate_interesting_order_score;
                best_required_outer_relation_indexes.clear();
                best_required_outer_relation_aliases.clear();

                best_path = candidate_path;
                best_plan = candidate_plan;
            }

            for (const auto &index : relation.indexes)
            {
                if (index.column_ids.size() < 2)
                {
                    continue;
                }

                const auto skip_predicate_it =
                    std::find_if(predicates.begin(),
                                 predicates.end(),
                                 [&](const ResolvedScanPredicate &predicate) {
                                     return std::find(index.column_ids.begin() + 1,
                                                      index.column_ids.end(),
                                                      predicate.column_id) !=
                                            index.column_ids.end();
                                 });
                if (skip_predicate_it == predicates.end())
                {
                    continue;
                }

                const double predicate_selectivity =
                    std::min(selectivity,
                             estimatePredicateSelectivity(*skip_predicate_it));
                const uint64_t expected_rows =
                    relationOutputRows(base_rows, predicate_selectivity);
                const uint64_t index_pages =
                    std::max<uint64_t>(1, base_pages / 8);
                const uint64_t heap_pages =
                    std::max<uint64_t>(1,
                                       static_cast<uint64_t>(
                                           static_cast<double>(base_pages) *
                                           predicate_selectivity));
                PlannerFamilyLoweringRequest skip_lowering_request;
                skip_lowering_request =
                    loweringRequestForPredicate(index,
                                               *skip_predicate_it,
                                               false,
                                               true);
                const PlannerFamilyLoweringResult skip_lowered =
                    lowerPlannerFamily(skip_lowering_request);
                if (skip_lowered.queryability_state ==
                        AccessPathQueryabilityState::INVALID ||
                    skip_lowered.family != PlannerAccessFamily::BTREE_SKIP_SCAN)
                {
                    continue;
                }
                IndexFamilyMetricsPacket skip_metrics_packet;
                const bool have_skip_metrics =
                    loadIndexFamilyMetricsPacket(index, skip_metrics_packet);
                const IndexFamilyCostCalibrationInput skip_cost_input =
                    buildIndexFamilyCostInput(
                        skip_lowered,
                        index,
                        have_skip_metrics ? &skip_metrics_packet : nullptr,
                        relation_row_width,
                        relation_heap_rows_per_page,
                        false,
                        false);
                CostModel skip_cost_model(
                    deriveIndexFamilyFormulaProfile(planner_params,
                                                    skip_cost_input));
                CostEstimate skip_cost =
                    skip_cost_model.costIndexScan(3,
                                                  index_pages,
                                                  expected_rows,
                                                  heap_pages,
                                                  expected_rows,
                                                  qual_cost,
                                                  0.15,
                                                  ctx);
                skip_cost.startup_cost *= 1.20;
                skip_cost.total_cost *= 1.20;
                if (relation_stats_penalty > 1.0)
                {
                    skip_cost.startup_cost *= relation_stats_penalty;
                    skip_cost.total_cost *= relation_stats_penalty;
                    appendRuntimeTrace(rejected_paths,
                                       "STATS_FRESHNESS",
                                       relation_subject,
                                       accessTraceCandidateLabel("SKIP_SCAN",
                                                                index.index_name,
                                                                std::string()),
                                       "ADJUSTED",
                                       relation_stats_penalty_reason,
                                       skip_cost.startup_cost,
                                       skip_cost.total_cost,
                                       skip_cost.rows);
                }
                appendUniqueText(bundle.candidate_scan_families,
                                 skip_lowered.family_name);
                appendRuntimeTrace(considered_paths,
                                   "ACCESS_PATH",
                                   relation_subject,
                                   accessTraceCandidateLabel("SKIP_SCAN",
                                                            index.index_name,
                                                            std::string()),
                                   "CONSIDERED",
                                   skip_predicate_it->predicate_text,
                                   skip_cost.startup_cost,
                                   skip_cost.total_cost,
                                   skip_cost.rows);
                const MgaAccessGovernanceDecision skip_mga_governance =
                    evaluateMgaAccessGovernance(relation_mga_pressure,
                                                skip_lowered,
                                                "INDEX_SCAN",
                                                false,
                                                false,
                                                skip_cost.rows,
                                                base_rows);
                if (skip_mga_governance.reject_candidate)
                {
                    appendRuntimeTrace(rejected_paths,
                                       "MGA_SWITCHOVER",
                                       relation_subject,
                                       accessTraceCandidateLabel("SKIP_SCAN",
                                                                index.index_name,
                                                                std::string()),
                                       "REJECTED",
                                       skip_mga_governance.reason,
                                       skip_cost.startup_cost,
                                       skip_cost.total_cost,
                                       skip_cost.rows);
                    continue;
                }
                auto skip_path = std::make_shared<IndexScanPath>(
                    relation.table_info.table_id,
                    relation_name,
                    index.index_id,
                    index.index_name,
                    3,
                    index_pages,
                    expected_rows,
                    heap_pages,
                    expected_rows,
                    qual_cost,
                    0.15,
                    skip_cost);
                auto skip_plan = std::make_shared<IndexScanNode>(
                    relation.table_info.table_id,
                    relation_name,
                    index.index_id,
                    index.index_name);
                skip_plan->setIndexQualCost(qual_cost);
                skip_plan->setHeapQualCost(qual_cost);
                skip_plan->setCorrelation(0.15);
                skip_plan->setIndexCond(skip_predicate_it->predicate_text);
                skip_plan->setFilter(buildPredicateText());
                skip_plan->setCost(skip_cost);
                registerBundleCandidate(makeBaseAccessChoice(
                    skip_path,
                    skip_plan,
                    skip_cost,
                    "INDEX_SCAN",
                    skip_lowered.family_name,
                    skip_lowered,
                    {"SKIP_SCAN"},
                    {makeRuntimePredicate(*skip_predicate_it)},
                    std::string(),
                    false,
                    false,
                    false,
                    0,
                    {},
                    false,
                    0.0,
                    {},
                    {},
                    0.0,
                    0,
                    index,
                    {"INDEX_SCAN", skip_lowered.family_name}));
                if (skip_cost.total_cost > best_cost.total_cost)
                {
                    appendRuntimeTrace(rejected_paths,
                                       "ACCESS_PATH",
                                       relation_subject,
                                       accessTraceCandidateLabel("SKIP_SCAN",
                                                                index.index_name,
                                                                std::string()),
                                       "REJECTED",
                                       "higher total cost than current best",
                                       skip_cost.startup_cost,
                                       skip_cost.total_cost,
                                       skip_cost.rows);
                    continue;
                }

                matched_index = index;
                best_cost = skip_cost;
                best_scan_kind = "INDEX_SCAN";
                best_scan_family = skip_lowered.family_name;
                best_lowering = skip_lowered;
                best_scan_family_tags = {"SKIP_SCAN"};
                best_runtime_predicates = {
                    makeRuntimePredicate(*skip_predicate_it)};
                best_bitmap_op.clear();
                best_covering_index = false;
                best_exact_key_lookup = false;
                best_coverage_fraction = 0.0;
                best_candidate_budget = 0;
                best_ordered_output = false;
                best_ordered_prefix_length = 0;
                best_ordering_keys.clear();
                best_order_complete = false;
                best_interesting_order_score = 0.0;
                best_required_outer_relation_indexes.clear();
                best_required_outer_relation_aliases.clear();

                best_path = skip_path;
                best_plan = skip_plan;
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
                if (!predicate.has_index_match)
                {
                    bitmap_exact_lookup = false;
                    continue;
                }
                const PlannerFamilyLoweringResult predicate_lowering =
                    lowerPlannerFamily(loweringRequestForPredicate(
                        predicate.matched_index,
                        predicate,
                        false,
                        false));
                if (!supportsExactLookup(predicate_lowering, predicate))
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
                double bitmap_false_positive_ratio = 0.0;
                double bitmap_dead_fraction = 0.0;
                size_t bitmap_metric_count = 0;
                for (const auto &predicate : predicates)
                {
                    if (!predicate.has_index_match)
                    {
                        continue;
                    }
                    IndexFamilyMetricsPacket bitmap_metrics_packet;
                    if (!loadIndexFamilyMetricsPacket(predicate.matched_index,
                                                      bitmap_metrics_packet))
                    {
                        continue;
                    }
                    bitmap_false_positive_ratio += familyMetricDouble(
                        predicate.matched_index,
                        &bitmap_metrics_packet,
                        "bitmap_false_positive_ratio",
                        bitmap_metrics_packet.recheck_ratio_est);
                    bitmap_dead_fraction += bitmap_metrics_packet.dead_fraction;
                    ++bitmap_metric_count;
                }
                CostParameters bitmap_cost_params = planner_params;
                bitmap_cost_params.default_row_width_bytes = relation_row_width;
                bitmap_cost_params.calibrated_heap_rows_per_page =
                    relation_heap_rows_per_page;
                if (bitmap_metric_count > 0)
                {
                    bitmap_cost_params.calibrated_false_positive_ratio =
                        bitmap_false_positive_ratio /
                        static_cast<double>(bitmap_metric_count);
                    bitmap_cost_params.calibrated_dead_fraction =
                        bitmap_dead_fraction /
                        static_cast<double>(bitmap_metric_count);
                    bitmap_cost_params.calibrated_visibility_tuple_cost =
                        planner_params.cpu_operator_cost *
                        (0.10 + (bitmap_cost_params.calibrated_dead_fraction * 0.40) +
                         (bitmap_cost_params.calibrated_false_positive_ratio * 0.30));
                }
                CostModel bitmap_cost_model(bitmap_cost_params);
                CostEstimate bitmap_cost =
                    bitmap_cost_model.costBitmapScan(bitmap_index_ids.size(),
                                                     std::max<uint64_t>(1,
                                                                        bitmap_total_index_pages),
                                                     bitmap_heap_pages,
                                                     bitmap_rows,
                                                     qual_cost,
                                                     predicate_or ? "OR" : "AND",
                                                     ctx);
                if (relation_stats_penalty > 1.0)
                {
                    bitmap_cost.startup_cost *= relation_stats_penalty;
                    bitmap_cost.total_cost *= relation_stats_penalty;
                    appendRuntimeTrace(considered_paths,
                                       "STATS_FRESHNESS",
                                       relation_subject,
                                       accessTraceCandidateLabel("BITMAP_INDEX_SCAN",
                                                                bitmap_index_names.empty()
                                                                    ? std::string()
                                                                    : bitmap_index_names.front(),
                                                                predicate_or ? "OR" : "AND"),
                                       "ADJUSTED",
                                       relation_stats_penalty_reason,
                                       bitmap_cost.startup_cost,
                                       bitmap_cost.total_cost,
                                       bitmap_cost.rows);
                }
                const double bitmap_mga_penalty = applyMgaCostPenalty(
                    bitmap_cost,
                    relation_mga_pressure,
                    "BITMAP_INDEX_SCAN");
                if (bitmap_mga_penalty > 0.0)
                {
                    appendRuntimeTrace(considered_paths,
                                       "MGA_COSTING",
                                       relation_subject,
                                       accessTraceCandidateLabel("BITMAP_INDEX_SCAN",
                                                                bitmap_index_names.empty()
                                                                    ? std::string()
                                                                    : bitmap_index_names.front(),
                                                                predicate_or ? "OR" : "AND"),
                                       "ADJUSTED",
                                       "canonical MGA telemetry penalty=" +
                                           std::to_string(bitmap_mga_penalty) + " " +
                                           describeMgaCostPressure(
                                               relation_mga_pressure),
                                       bitmap_cost.startup_cost,
                                       bitmap_cost.total_cost,
                                       bitmap_cost.rows);
                }
                PlannerFamilyLoweringRequest bitmap_lowering_request;
                bitmap_lowering_request.bitmap_combine = true;
                const PlannerFamilyLoweringResult bitmap_lowered =
                    lowerPlannerFamily(bitmap_lowering_request);
                const MgaAccessGovernanceDecision bitmap_mga_governance =
                    evaluateMgaAccessGovernance(
                        relation_mga_pressure,
                        bitmap_lowered,
                        "BITMAP_INDEX_SCAN",
                        bitmap_exact_lookup,
                        false,
                        bitmap_cost.rows,
                        base_rows);
                if (bitmap_mga_governance.reject_candidate)
                {
                    appendRuntimeTrace(rejected_paths,
                                       "MGA_SWITCHOVER",
                                       relation_subject,
                                       accessTraceCandidateLabel("BITMAP_INDEX_SCAN",
                                                                bitmap_index_names.empty()
                                                                    ? std::string()
                                                                    : bitmap_index_names.front(),
                                                                predicate_or ? "OR" : "AND"),
                                       "REJECTED",
                                       bitmap_mga_governance.reason,
                                       bitmap_cost.startup_cost,
                                       bitmap_cost.total_cost,
                                       bitmap_cost.rows);
                    continue;
                }
                appendRuntimeTrace(considered_paths,
                                   "ACCESS_PATH",
                                   relation_subject,
                                   accessTraceCandidateLabel("BITMAP_INDEX_SCAN",
                                                            bitmap_index_names.empty()
                                                                ? std::string()
                                                                : bitmap_index_names.front(),
                                                            predicate_or ? "OR" : "AND"),
                                   "CONSIDERED",
                                   "bitmap over " +
                                       std::to_string(bitmap_index_ids.size()) + " indexes",
                                   bitmap_cost.startup_cost,
                                   bitmap_cost.total_cost,
                                   bitmap_cost.rows);
                const bool prefer_exact_bitmap =
                    !predicate_or &&
                    bitmap_exact_lookup &&
                    bitmap_exact_predicate_count == bitmap_predicates.size();
                if (bitmap_cost.total_cost <= best_cost.total_cost || prefer_exact_bitmap)
                {
                    best_cost = bitmap_cost;
                    best_scan_kind = "BITMAP_INDEX_SCAN";
                    best_scan_family = bitmap_lowered.family_name;
                    best_lowering = bitmap_lowered;
                    best_scan_family_tags.clear();
                    best_runtime_predicates = bitmap_predicates;
                    best_bitmap_op = predicate_or ? "OR" : "AND";
                    best_covering_index = false;
                    best_exact_key_lookup = bitmap_exact_lookup;
                    best_coverage_fraction = 0.0;
                    best_candidate_budget =
                        static_cast<uint64_t>(bitmap_predicates.size());
                    best_ordered_output = false;
                    best_ordered_prefix_length = 0;
                    best_ordering_keys.clear();
                    best_order_complete = false;
                    best_interesting_order_score = 0.0;
                    best_required_outer_relation_indexes.clear();
                    best_required_outer_relation_aliases.clear();
                    matched_index = {};
                    appendUniqueText(bundle.candidate_scan_families,
                                     bitmap_lowered.family_name);
                    auto bitmap_path = std::make_shared<BitmapIndexScanPath>(
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
                    bitmap_plan->setCost(bitmap_cost);
                    registerBundleCandidate(makeBaseAccessChoice(
                        bitmap_path,
                        bitmap_plan,
                        bitmap_cost,
                        "BITMAP_INDEX_SCAN",
                        bitmap_lowered.family_name,
                        bitmap_lowered,
                        {},
                        bitmap_predicates,
                        best_bitmap_op,
                        false,
                        bitmap_exact_lookup,
                        false,
                        0,
                        {},
                        false,
                        0.0,
                        {},
                        {},
                        0.0,
                        static_cast<uint64_t>(bitmap_predicates.size()),
                        core::CatalogManager::IndexInfo{},
                        {"BITMAP_INDEX_SCAN", bitmap_lowered.family_name}));
                    best_path = bitmap_path;
                    best_plan = bitmap_plan;
                }
                else
                {
                    appendRuntimeTrace(rejected_paths,
                                       "ACCESS_PATH",
                                       relation_subject,
                                       accessTraceCandidateLabel("BITMAP_INDEX_SCAN",
                                                                bitmap_index_names.empty()
                                                                    ? std::string()
                                                                    : bitmap_index_names.front(),
                                                                predicate_or ? "OR" : "AND"),
                                       "REJECTED",
                                       "higher total cost than current best",
                                       bitmap_cost.startup_cost,
                                       bitmap_cost.total_cost,
                                       bitmap_cost.rows);
                }
            }

            bundle.selectivity = selectivity;
            auto bundleSemanticPriority =
                [](const BaseAccessChoice &candidate) -> int {
                    const auto &relation = candidate.runtime_relation;
                    if (relation.scan_kind == "BITMAP_INDEX_SCAN" &&
                        relation.bitmap_op == "AND" &&
                        relation.exact_key_lookup &&
                        relation.index_predicates.size() > 1)
                    {
                        return 2;
                    }
                    if (relation.exact_key_lookup &&
                        relation.index_predicates.size() > 1)
                    {
                        return 1;
                    }
                    return 0;
                };
            auto chosen_it =
                bundle.candidates.empty()
                    ? bundle.candidates.end()
                    : std::min_element(
                          bundle.candidates.begin(),
                          bundle.candidates.end(),
                          [&](const BaseAccessChoice &left,
                              const BaseAccessChoice &right) {
                              const int left_priority =
                                  bundleSemanticPriority(left);
                              const int right_priority =
                                  bundleSemanticPriority(right);
                              if (left_priority != right_priority)
                              {
                                  return left_priority > right_priority;
                              }
                              const size_t left_predicate_count =
                                  left.runtime_relation.index_predicates.size();
                              const size_t right_predicate_count =
                                  right.runtime_relation.index_predicates.size();
                              if (left.runtime_relation.exact_key_lookup &&
                                  right.runtime_relation.exact_key_lookup &&
                                  left_predicate_count != right_predicate_count)
                              {
                                  return left_predicate_count >
                                         right_predicate_count;
                              }
                              const double left_cost =
                                  left.path ? left.path->totalCost()
                                            : left.runtime_relation.total_cost;
                              const double right_cost =
                                  right.path ? right.path->totalCost()
                                             : right.runtime_relation.total_cost;
                              if (left_cost != right_cost)
                              {
                                  return left_cost < right_cost;
                              }
                              const uint64_t left_rows =
                                  left.path ? left.path->rows()
                                            : left.runtime_relation.estimated_rows;
                              const uint64_t right_rows =
                                  right.path ? right.path->rows()
                                             : right.runtime_relation.estimated_rows;
                              if (left_rows != right_rows)
                              {
                                  return left_rows < right_rows;
                              }
                              if (left.access_descriptor.parallel_enabled !=
                                  right.access_descriptor.parallel_enabled)
                              {
                                  return left.access_descriptor.parallel_enabled;
                              }
                              return left.access_descriptor.path_name <
                                     right.access_descriptor.path_name;
                          });
            if (chosen_it == bundle.candidates.end())
            {
                std::vector<std::string> chosen_candidate_scan_families = {
                    best_scan_kind,
                    best_scan_family};
                chosen_it = bundle.candidates.insert(
                    bundle.candidates.end(),
                    makeBaseAccessChoice(best_path,
                                         best_plan,
                                         best_cost,
                                         best_scan_kind,
                                         best_scan_family,
                                         best_lowering,
                                         best_scan_family_tags,
                                         best_runtime_predicates,
                                         best_bitmap_op,
                                         best_covering_index,
                                         best_exact_key_lookup,
                                         best_ordered_output,
                                         best_ordered_prefix_length,
                                         best_ordering_keys,
                                         best_order_complete,
                                         best_interesting_order_score,
                                         best_required_outer_relation_indexes,
                                         best_required_outer_relation_aliases,
                                         best_coverage_fraction,
                                         best_candidate_budget,
                                         matched_index,
                                         chosen_candidate_scan_families));
            }
            BaseAccessChoice chosen_choice = *chosen_it;
            const auto parallel_scan_it = std::find_if(
                bundle.candidates.begin(),
                bundle.candidates.end(),
                [](const BaseAccessChoice &candidate) {
                    return candidate.access_descriptor.parallel_enabled &&
                           candidate.access_descriptor.parallel_stage == "SCAN";
                });
            if (parallel_scan_it != bundle.candidates.end())
            {
                if (chosen_choice.access_descriptor.parallel_enabled)
                {
                    appendRuntimeTrace(
                        considered_paths,
                        "PARALLEL",
                        relation_subject,
                        "PARALLEL_SEQ_SCAN",
                        "CHOSEN",
                        "search-visible parallel scan candidate selected",
                        chosen_choice.runtime_relation.startup_cost,
                        chosen_choice.runtime_relation.total_cost,
                        chosen_choice.runtime_relation.estimated_rows);
                }
                else
                {
                    chosen_choice.runtime_relation.parallel_eligible = true;
                    chosen_choice.runtime_relation.parallel_enabled = false;
                    chosen_choice.runtime_relation.parallel_workers_planned = 0;
                    chosen_choice.runtime_relation.parallel_stage = "SCAN";
                    chosen_choice.runtime_relation.parallel_rejection_reason =
                        "serial base access path cheaper than parallel scan candidate";
                    appendRuntimeTrace(
                        rejected_paths,
                        "PARALLEL",
                        relation_subject,
                        "PARALLEL_SEQ_SCAN",
                        "REJECTED",
                        chosen_choice.runtime_relation.parallel_rejection_reason,
                        parallel_scan_it->runtime_relation.startup_cost,
                        parallel_scan_it->runtime_relation.total_cost,
                        parallel_scan_it->runtime_relation.estimated_rows);
                }
            }
            if (bundle.candidate_scan_families.empty())
            {
                bundle.candidate_scan_families =
                    chosen_choice.candidate_scan_families;
            }
            if (bundle.candidate_family_identity_signatures.empty())
            {
                bundle.candidate_family_identity_signatures =
                    chosen_choice.runtime_relation
                        .candidate_family_identity_signatures;
            }
            if (bundle.candidate_family_statistics_signatures.empty())
            {
                bundle.candidate_family_statistics_signatures =
                    chosen_choice.runtime_relation
                        .candidate_family_statistics_signatures;
            }
            sortAndUniqueText(bundle.candidate_family_identity_signatures);
            sortAndUniqueText(bundle.candidate_family_statistics_signatures);
            if (relation.lateral)
            {
                appendStatsProvenance(statistics_provenance,
                                      relation_subject,
                                      "PARAMETERIZED_LATERAL",
                                      "evaluated per outer row through nested loop parameterization");
                const bool has_parameterizable_index =
                    std::any_of(relation.indexes.begin(),
                                relation.indexes.end(),
                                [](const core::CatalogManager::IndexInfo &index) {
                                    PlannerFamilyLoweringRequest request;
                                    request.index_type = index.index_type;
                                    request.predicate_shape =
                                        PredicateMatchShape::EQUALITY;
                                    const auto lowered =
                                        lowerPlannerFamily(request);
                                    return lowered.queryability_state !=
                                               AccessPathQueryabilityState::INVALID &&
                                           lowered.supports_parameterization;
                                });
                if (has_parameterizable_index)
                {
                    appendUniqueText(bundle.candidate_scan_families,
                                     "PARAMETERIZED_INDEX_SCAN");
                }
            }
            chosen_choice.candidate_scan_families = bundle.candidate_scan_families;
            chosen_choice.runtime_relation.candidate_scan_families =
                bundle.candidate_scan_families;
            chosen_choice.runtime_relation.candidate_family_identity_signatures =
                bundle.candidate_family_identity_signatures;
            chosen_choice.runtime_relation
                .candidate_family_statistics_signatures =
                bundle.candidate_family_statistics_signatures;
            chosen_choice.runtime_relation.candidate_budget =
                std::max<uint64_t>(chosen_choice.runtime_relation.candidate_budget,
                                   bundle.candidates.size());
            chosen_choice.access_descriptor.candidate_budget =
                std::max<uint64_t>(chosen_choice.access_descriptor.candidate_budget,
                                   bundle.candidates.size());
            appendRuntimeTrace(considered_paths,
                               "ACCESS_PATH",
                               relation_subject,
                               accessTraceCandidateLabel(
                                   chosen_choice.runtime_relation.scan_kind,
                                   chosen_choice.runtime_relation.index_name,
                                   chosen_choice.runtime_relation.bitmap_op),
                               "CHOSEN",
                               "selected access path from bundle of " +
                                   std::to_string(bundle.candidates.size()) +
                                   " candidate(s)",
                               chosen_choice.runtime_relation.startup_cost,
                               chosen_choice.runtime_relation.total_cost,
                               chosen_choice.runtime_relation.estimated_rows);
            if (chosen_it != bundle.candidates.end())
            {
                *chosen_it = chosen_choice;
                std::iter_swap(bundle.candidates.begin(), chosen_it);
            }
            else
            {
                bundle.candidates.insert(bundle.candidates.begin(),
                                         chosen_choice);
            }
            access_bundles[relation_index] = std::move(bundle);
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

        struct MaterializedJoinTree
        {
            std::shared_ptr<Path> path;
            std::shared_ptr<PlanNode> plan;
            uint64_t rows = 0;
            std::vector<size_t> relation_order;
            std::vector<RuntimePlanRelation> runtime_relations;
            std::vector<RuntimePlanJoinStep> runtime_joins;
        };

        auto buildSubtreeJoinDecision =
            [&](const MaterializedJoinTree &left_tree,
                const MaterializedJoinTree &right_tree,
                const ResolvedJoin &join,
                bool join_right_relation_in_right_tree,
                bool disconnected_component_cross_join = false) -> JoinDecision {
                JoinDecision decision;
                decision.valid = true;
                decision.join_index = join.source_join_index;
                decision.candidate_relation_index =
                    join_right_relation_in_right_tree ? join.right_relation_index
                                                      : join.left_relation_index;
                decision.selectivity =
                    join.join_type == parser::JoinType::CROSS ? 1.0
                                                              : estimateJoinSelectivityFor(join);
                if (decision.selectivity <= 0.0)
                {
                    decision.selectivity = 0.01;
                }

                const auto &candidate_relation =
                    planned_out.resolved_query
                        .relations[decision.candidate_relation_index];
                const auto &left_relation =
                    planned_out.resolved_query.relations[join.left_relation_index];
                const auto &right_relation =
                    planned_out.resolved_query.relations[join.right_relation_index];
                const auto &left_endpoint_relation =
                    planned_out.resolved_query.relations[
                        join_right_relation_in_right_tree ? join.left_relation_index
                                                          : join.right_relation_index];
                const auto &right_endpoint_relation =
                    planned_out.resolved_query.relations[
                        join_right_relation_in_right_tree ? join.right_relation_index
                                                          : join.left_relation_index];
                const std::string join_subject =
                    "join:" + joinTraceSubject(planned_out.resolved_query, join);
                const bool parameterized_inner =
                    std::any_of(right_tree.relation_order.begin(),
                                right_tree.relation_order.end(),
                                [&](size_t relation_index) {
                                    return relation_index < access_bundles.size() &&
                                           (!access_bundles[relation_index]
                                                 .candidates.empty() &&
                                            access_bundles[relation_index]
                                                .candidates.front()
                                                .runtime_relation.parameterized ||
                                            planned_out.resolved_query
                                                .relations[relation_index]
                                                .lateral);
                                });
                appendStatsProvenance(statistics_provenance,
                                      join_subject,
                                      joinStatsSource(planned_out.resolved_query,
                                                      join,
                                                      stats_manager_),
                                      join.condition_text.empty()
                                          ? planJoinTypeToString(join.join_type)
                                          : join.condition_text);
                const auto join_type = join.join_type;
                const bool has_resolved_join_key_metadata =
                    join.equi_join && !join.left_hash_qualifier.empty() &&
                    !join.left_hash_column.empty() &&
                    !join.right_hash_qualifier.empty() &&
                    !join.right_hash_column.empty();
                JoinLegalityDescriptor method_legality;
                method_legality.legality_class = join.legality_class;
                method_legality.reorderable = join.reorderable;
                method_legality.preserves_left_rows = join.preserves_left_rows;
                method_legality.preserves_right_rows = join.preserves_right_rows;
                method_legality.null_introduces_left = join.null_introduces_left;
                method_legality.null_introduces_right = join.null_introduces_right;
                method_legality.requires_original_order =
                    join.requires_original_order;
                method_legality.natural_barrier =
                    join.natural || isNaturalJoinType(join.join_type);
                method_legality.using_barrier = !join.using_columns.empty();
                method_legality.semi_duplicate_semantics =
                    join.legality_class == JoinLegalityClass::SEMI_BARRIER;
                method_legality.anti_duplicate_semantics =
                    join.legality_class == JoinLegalityClass::ANTI_BARRIER;
                method_legality.lateral_dependency = parameterized_inner;
                auto rejectForcedJoinMethod =
                    [&](const char *requested_method,
                        const std::string &reason) -> JoinDecision {
                        SET_ERROR_CONTEXT(ctx,
                                          core::Status::INVALID_ARGUMENT,
                                          (std::string("Optimizer control requires ") +
                                           requested_method + " for " + join_subject +
                                           ", but " + reason)
                                              .c_str());
                        decision.valid = false;
                        return decision;
                    };
                const char *nested_loop_candidate_name =
                    parameterized_inner ? "PARAMETERIZED_NESTED_LOOP"
                                        : "NESTED_LOOP";
                const auto nested_legality =
                    evaluateNestedLoopLegality(method_legality,
                                               parameterized_inner);
                const bool outer_presorted =
                    pathLikelyProvidesJoinOrder(left_tree.path);
                const bool inner_presorted =
                    pathLikelyProvidesJoinOrder(right_tree.path);
                const auto hash_legality =
                    evaluateHashJoinLegality(method_legality,
                                             join_type,
                                             join.equi_join,
                                             has_resolved_join_key_metadata,
                                             parameterized_inner);
                const auto merge_legality =
                    evaluateMergeJoinLegality(method_legality,
                                              join_type,
                                              join.equi_join,
                                              has_resolved_join_key_metadata,
                                              parameterized_inner,
                                              outer_presorted,
                                              inner_presorted);

                const bool allow_nested_loop =
                    nested_legality.legal &&
                    planner_controls.join_method !=
                        PlannerJoinMethodControl::HASH_ONLY &&
                    planner_controls.join_method !=
                        PlannerJoinMethodControl::MERGE_ONLY;
                CostEstimate nl_cost{};
                if (allow_nested_loop)
                {
                    nl_cost = active_cost_model.costNestedLoopJoin(
                        left_tree.path ? left_tree.path->cost() : CostEstimate{},
                        right_tree.path ? right_tree.path->cost() : CostEstimate{},
                        left_tree.rows,
                        right_tree.rows,
                        decision.selectivity,
                        join_type,
                        ctx);
                    appendRuntimeTrace(considered_paths,
                                       "JOIN_METHOD",
                                       join_subject,
                                       nested_loop_candidate_name,
                                       "CONSIDERED",
                                       parameterized_inner
                                           ? "parameterized nested-loop candidate"
                                           : "nested-loop candidate",
                                       nl_cost.startup_cost,
                                       nl_cost.total_cost,
                                       nl_cost.rows);
                }
                else
                {
                    nl_cost.total_cost = std::numeric_limits<double>::max();
                    appendRuntimeTrace(rejected_paths,
                                       "JOIN_METHOD",
                                       join_subject,
                                       nested_loop_candidate_name,
                                       "REJECTED",
                                       std::string("optimizer control forces ") +
                                           plannerJoinMethodName(
                                               planner_controls.join_method),
                                       0.0,
                                       0.0,
                                       0);
                }

                bool allow_hash =
                    hash_legality.legal &&
                    planner_controls.join_method !=
                        PlannerJoinMethodControl::NESTED_LOOP_ONLY &&
                    planner_controls.join_method !=
                        PlannerJoinMethodControl::MERGE_ONLY;
                CostEstimate hash_cost{};
                if (allow_hash)
                {
                    hash_cost = active_cost_model.costHashJoin(
                        left_tree.path ? left_tree.path->cost() : CostEstimate{},
                        right_tree.path ? right_tree.path->cost() : CostEstimate{},
                        left_tree.rows,
                        right_tree.rows,
                        decision.selectivity,
                        join_type,
                        ctx);
                    appendRuntimeTrace(considered_paths,
                                       "JOIN_METHOD",
                                       join_subject,
                                       "HASH_JOIN",
                                       "CONSIDERED",
                                       "hash-join candidate",
                                       hash_cost.startup_cost,
                                       hash_cost.total_cost,
                                       hash_cost.rows);
                    if (spill_policy == PlannerSpillPolicy::DISALLOW &&
                        hash_cost.spill_expected)
                    {
                        allow_hash = false;
                        hash_cost.total_cost = std::numeric_limits<double>::max();
                        appendRuntimeTrace(rejected_paths,
                                           "JOIN_METHOD",
                                           join_subject,
                                           "HASH_JOIN",
                                           "REJECTED",
                                           "spill policy disallows hash-join temp spill",
                                           0.0,
                                           0.0,
                                           0);
                        if (planner_controls.join_method ==
                            PlannerJoinMethodControl::HASH_ONLY)
                        {
                            return rejectForcedJoinMethod(
                                "HASH_JOIN",
                                "spill policy disallows hash-join temp spill");
                        }
                    }
                }
                else
                {
                    hash_cost.total_cost = std::numeric_limits<double>::max();
                    std::string reject_reason;
                    if (planner_controls.join_method ==
                            PlannerJoinMethodControl::NESTED_LOOP_ONLY ||
                        planner_controls.join_method ==
                            PlannerJoinMethodControl::MERGE_ONLY)
                    {
                        reject_reason = std::string("optimizer control forces ") +
                            plannerJoinMethodName(planner_controls.join_method);
                    }
                    else
                    {
                        reject_reason = joinMethodRejectReason(
                            JoinMethodFamily::HASH_JOIN,
                            hash_legality.reject_code);
                    }
                    appendRuntimeTrace(rejected_paths,
                                       "JOIN_METHOD",
                                       join_subject,
                                       "HASH_JOIN",
                                       "REJECTED",
                                       reject_reason,
                                       0.0,
                                       0.0,
                                       0);
                    if (planner_controls.join_method ==
                        PlannerJoinMethodControl::HASH_ONLY)
                    {
                        return rejectForcedJoinMethod("HASH_JOIN", reject_reason);
                    }
                }

                bool allow_merge =
                    merge_legality.legal &&
                    planner_controls.join_method !=
                        PlannerJoinMethodControl::NESTED_LOOP_ONLY &&
                    planner_controls.join_method !=
                        PlannerJoinMethodControl::HASH_ONLY;
                CostEstimate merge_cost{};
                if (allow_merge)
                {
                    merge_cost = active_cost_model.costMergeJoin(
                        left_tree.path ? left_tree.path->cost() : CostEstimate{},
                        right_tree.path ? right_tree.path->cost() : CostEstimate{},
                        left_tree.rows,
                        right_tree.rows,
                        decision.selectivity,
                        outer_presorted,
                        inner_presorted,
                        join_type,
                        ctx);
                    appendRuntimeTrace(considered_paths,
                                       "JOIN_METHOD",
                                       join_subject,
                                       merge_legality.requires_sort_outer ||
                                               merge_legality.requires_sort_inner
                                           ? "MERGE_JOIN[SORT_TO_MERGE]"
                                           : "MERGE_JOIN",
                                       "CONSIDERED",
                                       merge_legality.requires_sort_outer ||
                                               merge_legality.requires_sort_inner
                                           ? "explicit sort-to-merge candidate"
                                           : "ordered equi-join candidate",
                                       merge_cost.startup_cost,
                                       merge_cost.total_cost,
                                       merge_cost.rows);
                    if (spill_policy == PlannerSpillPolicy::DISALLOW &&
                        merge_cost.spill_expected)
                    {
                        allow_merge = false;
                        merge_cost.total_cost = std::numeric_limits<double>::max();
                        appendRuntimeTrace(rejected_paths,
                                           "JOIN_METHOD",
                                           join_subject,
                                           "MERGE_JOIN",
                                           "REJECTED",
                                           "spill policy disallows merge-join temp spill",
                                           0.0,
                                           0.0,
                                           0);
                        if (planner_controls.join_method ==
                            PlannerJoinMethodControl::MERGE_ONLY)
                        {
                            return rejectForcedJoinMethod(
                                "MERGE_JOIN",
                                "spill policy disallows merge-join temp spill");
                        }
                    }
                }
                else
                {
                    merge_cost.total_cost = std::numeric_limits<double>::max();
                    std::string reject_reason;
                    if (planner_controls.join_method ==
                            PlannerJoinMethodControl::NESTED_LOOP_ONLY ||
                        planner_controls.join_method ==
                            PlannerJoinMethodControl::HASH_ONLY)
                    {
                        reject_reason = std::string("optimizer control forces ") +
                            plannerJoinMethodName(planner_controls.join_method);
                    }
                    else
                    {
                        reject_reason = joinMethodRejectReason(
                            JoinMethodFamily::MERGE_JOIN,
                            merge_legality.reject_code);
                    }
                    appendRuntimeTrace(rejected_paths,
                                       "JOIN_METHOD",
                                       join_subject,
                                       "MERGE_JOIN",
                                       "REJECTED",
                                       reject_reason,
                                       0.0,
                                       0.0,
                                       0);
                    if (planner_controls.join_method ==
                        PlannerJoinMethodControl::MERGE_ONLY)
                    {
                        return rejectForcedJoinMethod("MERGE_JOIN", reject_reason);
                    }
                }

                enum class ChosenJoinMethod
                {
                    NESTED_LOOP,
                    HASH_JOIN,
                    MERGE_JOIN
                };
                const bool merge_requires_explicit_sort =
                    merge_legality.requires_sort_outer ||
                    merge_legality.requires_sort_inner;
                bool merge_deferred_for_ordering = false;
                ChosenJoinMethod chosen_method = ChosenJoinMethod::NESTED_LOOP;
                decision.total_cost = nl_cost.total_cost;
                decision.rows = nl_cost.rows;
                if (!allow_nested_loop)
                {
                    chosen_method = allow_hash ? ChosenJoinMethod::HASH_JOIN
                                               : ChosenJoinMethod::MERGE_JOIN;
                    decision.total_cost =
                        allow_hash ? hash_cost.total_cost : merge_cost.total_cost;
                    decision.rows = allow_hash ? hash_cost.rows : merge_cost.rows;
                }
                if (allow_hash && hash_cost.total_cost < decision.total_cost)
                {
                    chosen_method = ChosenJoinMethod::HASH_JOIN;
                    decision.total_cost = hash_cost.total_cost;
                    decision.rows = hash_cost.rows;
                }
                if (allow_merge &&
                    merge_cost.total_cost < decision.total_cost &&
                    (!merge_requires_explicit_sort ||
                     !allow_hash ||
                     planner_controls.join_method ==
                         PlannerJoinMethodControl::MERGE_ONLY))
                {
                    chosen_method = ChosenJoinMethod::MERGE_JOIN;
                    decision.total_cost = merge_cost.total_cost;
                    decision.rows = merge_cost.rows;
                }
                else if (allow_merge && allow_hash && merge_requires_explicit_sort &&
                         planner_controls.join_method !=
                             PlannerJoinMethodControl::MERGE_ONLY &&
                         merge_cost.total_cost < hash_cost.total_cost)
                {
                    merge_deferred_for_ordering = true;
                }
                const char *chosen_method_name =
                    chosen_method == ChosenJoinMethod::HASH_JOIN
                        ? "HASH_JOIN"
                        : chosen_method == ChosenJoinMethod::MERGE_JOIN
                              ? "MERGE_JOIN"
                              : nested_loop_candidate_name;
                if (allow_nested_loop &&
                    chosen_method != ChosenJoinMethod::NESTED_LOOP)
                {
                    appendRuntimeTrace(rejected_paths,
                                       "JOIN_METHOD",
                                       join_subject,
                                       nested_loop_candidate_name,
                                       "REJECTED",
                                       std::string("higher total cost than chosen ") +
                                           chosen_method_name,
                                       nl_cost.startup_cost,
                                       nl_cost.total_cost,
                                       nl_cost.rows);
                }
                if (allow_hash && chosen_method != ChosenJoinMethod::HASH_JOIN)
                {
                    appendRuntimeTrace(rejected_paths,
                                       "JOIN_METHOD",
                                       join_subject,
                                       "HASH_JOIN",
                                       "REJECTED",
                                       std::string("higher total cost than chosen ") +
                                           chosen_method_name,
                                       hash_cost.startup_cost,
                                       hash_cost.total_cost,
                                       hash_cost.rows);
                }
                if (allow_merge && chosen_method != ChosenJoinMethod::MERGE_JOIN)
                {
                    appendRuntimeTrace(rejected_paths,
                                       "JOIN_METHOD",
                                       join_subject,
                                       "MERGE_JOIN",
                                       "REJECTED",
                                       merge_deferred_for_ordering
                                           ? "explicit sort-to-merge candidate deferred without ordering reuse"
                                           : std::string("higher total cost than chosen ") +
                                                 chosen_method_name,
                                       merge_cost.startup_cost,
                                       merge_cost.total_cost,
                                       merge_cost.rows);
                }
                decision.runtime_join.source_join_index = join.source_join_index;
                decision.runtime_join.right_relation_index =
                    decision.candidate_relation_index;
                decision.runtime_join.join_edge_left_relation_index =
                    join.left_relation_index;
                decision.runtime_join.join_edge_right_relation_index =
                    join.right_relation_index;
                decision.runtime_join.join_edge_left_alias =
                    relationLookupName(left_relation);
                decision.runtime_join.join_edge_right_alias =
                    relationLookupName(right_relation);
                decision.runtime_join.join_edge_left_id_text =
                    left_relation.resolved
                        ? left_relation.table_info.table_id.toString()
                        : std::string();
                decision.runtime_join.join_edge_right_id_text =
                    right_relation.resolved
                        ? right_relation.table_info.table_id.toString()
                        : std::string();
                decision.runtime_join.join_type = planJoinTypeToString(join_type);
                decision.runtime_join.disconnected_component =
                    disconnected_component_cross_join ||
                    (join_type == parser::JoinType::CROSS &&
                     join.condition == nullptr &&
                     join.condition_text.empty());
                decision.runtime_join.legality_class =
                    std::string(joinLegalityClassName(join.legality_class));
                decision.runtime_join.legal_method_families.clear();
                if (nested_legality.legal)
                {
                    decision.runtime_join.legal_method_families.push_back(
                        nested_loop_candidate_name);
                }
                if (hash_legality.legal)
                {
                    decision.runtime_join.legal_method_families.push_back("HASH_JOIN");
                }
                if (merge_legality.legal)
                {
                    decision.runtime_join.legal_method_families.push_back("MERGE_JOIN");
                }
                decision.runtime_join.method_enablers.clear();
                decision.runtime_join.reorderable = join.reorderable;
                decision.runtime_join.natural = join.natural;
                decision.runtime_join.using_columns = join.using_columns;
                decision.runtime_join.condition_text = join.condition_text;
                decision.runtime_join.preserves_left_rows = join.preserves_left_rows;
                decision.runtime_join.preserves_right_rows = join.preserves_right_rows;
                decision.runtime_join.null_introduces_left =
                    join.null_introduces_left;
                decision.runtime_join.null_introduces_right =
                    join.null_introduces_right;
                decision.runtime_join.requires_original_order =
                    join.requires_original_order;
                decision.runtime_join.outer_reorder_barrier =
                    join.legality_class == JoinLegalityClass::LEFT_OUTER_BARRIER ||
                    join.legality_class == JoinLegalityClass::RIGHT_OUTER_BARRIER ||
                    join.legality_class == JoinLegalityClass::FULL_OUTER_BARRIER;
                decision.runtime_join.semi_reorder_barrier =
                    join.legality_class == JoinLegalityClass::SEMI_BARRIER;
                decision.runtime_join.anti_reorder_barrier =
                    join.legality_class == JoinLegalityClass::ANTI_BARRIER;
                decision.runtime_join.using_reorder_barrier =
                    !join.using_columns.empty();
                decision.runtime_join.natural_reorder_barrier =
                    join.natural || isNaturalJoinType(join.join_type);
                decision.runtime_join.lateral_reorder_barrier =
                    parameterized_inner;
                decision.runtime_join.parameterized_dependency =
                    parameterized_inner;
                decision.runtime_join.parameter_dependency_relation_indexes.clear();
                decision.runtime_join.parameter_dependency_relation_aliases.clear();
                if (parameterized_inner)
                {
                    decision.runtime_join.parameter_dependency_relation_indexes =
                        left_tree.relation_order;
                    for (size_t relation_index : left_tree.relation_order)
                    {
                        if (relation_index <
                            planned_out.resolved_query.relations.size())
                        {
                            decision.runtime_join
                                .parameter_dependency_relation_aliases.push_back(
                                    relationLookupName(
                                        planned_out.resolved_query
                                            .relations[relation_index]));
                        }
                    }
                }
                decision.runtime_join.equijoin_keys.clear();
                if (join.equi_join)
                {
                    RuntimePlanJoinKeyPair key_pair;
                    key_pair.left_qualifier = join.left_hash_qualifier;
                    key_pair.left_column_name = join.left_hash_column;
                    key_pair.right_qualifier = join.right_hash_qualifier;
                    key_pair.right_column_name = join.right_hash_column;
                    decision.runtime_join.equijoin_keys.push_back(
                        std::move(key_pair));
                }
                decision.runtime_join.residual_predicates.clear();
                if (!join.condition_text.empty() && !join.equi_join)
                {
                    decision.runtime_join.residual_predicates.push_back(
                        join.condition_text);
                }
                if (decision.runtime_join.disconnected_component &&
                    decision.runtime_join.condition_text.empty())
                {
                    decision.runtime_join.condition_text =
                        "[disconnected component cross join]";
                }
                decision.runtime_join.startup_cost =
                    chosen_method == ChosenJoinMethod::HASH_JOIN
                        ? hash_cost.startup_cost
                        : chosen_method == ChosenJoinMethod::MERGE_JOIN
                              ? merge_cost.startup_cost
                              : nl_cost.startup_cost;
                decision.runtime_join.total_cost =
                    chosen_method == ChosenJoinMethod::HASH_JOIN
                        ? hash_cost.total_cost
                        : chosen_method == ChosenJoinMethod::MERGE_JOIN
                              ? merge_cost.total_cost
                              : nl_cost.total_cost;
                decision.runtime_join.estimated_rows =
                    chosen_method == ChosenJoinMethod::HASH_JOIN
                        ? hash_cost.rows
                        : chosen_method == ChosenJoinMethod::MERGE_JOIN
                              ? merge_cost.rows
                              : nl_cost.rows;
                const CostEstimate &chosen_cost =
                    chosen_method == ChosenJoinMethod::HASH_JOIN
                        ? hash_cost
                        : chosen_method == ChosenJoinMethod::MERGE_JOIN
                              ? merge_cost
                              : nl_cost;
                decision.runtime_join.estimated_memory_bytes =
                    chosen_cost.memory_bytes;
                decision.runtime_join.memory_budget_bytes =
                    chosen_cost.memory_budget_bytes;
                decision.runtime_join.spill_expected =
                    chosen_cost.spill_expected;
                decision.runtime_join.spill_passes =
                    chosen_cost.spill_passes;
                decision.runtime_join.spill_bytes =
                    chosen_cost.spill_bytes;
                decision.runtime_join.spill_policy = spill_policy_text;
                decision.runtime_join.selectivity = decision.selectivity;
                decision.runtime_join.method =
                    chosen_method == ChosenJoinMethod::HASH_JOIN
                        ? "HASH_JOIN"
                        : chosen_method == ChosenJoinMethod::MERGE_JOIN
                              ? "MERGE_JOIN"
                              : nested_loop_candidate_name;
                if (chosen_method == ChosenJoinMethod::MERGE_JOIN)
                {
                    if (merge_legality.requires_sort_outer)
                    {
                        decision.runtime_join.method_enablers.push_back(
                            "SORT_OUTER");
                    }
                    if (merge_legality.requires_sort_inner)
                    {
                        decision.runtime_join.method_enablers.push_back(
                            "SORT_INNER");
                    }
                }
                if (parameterized_inner)
                {
                    decision.runtime_join.method_enablers.push_back(
                        "OUTER_PARAMETER_BINDING");
                }
                appendRuntimeTrace(considered_paths,
                                   "JOIN_METHOD",
                                   join_subject,
                                   decision.runtime_join.method,
                                   "CHOSEN",
                                   disconnected_component_cross_join
                                       ? "selected disconnected-component bridge"
                                       : "selected join method",
                                   decision.runtime_join.startup_cost,
                                   decision.runtime_join.total_cost,
                                   decision.runtime_join.estimated_rows);

                if (has_resolved_join_key_metadata &&
                    (hash_legality.legal || merge_legality.legal))
                {
                    decision.runtime_join.has_hash_keys = true;
                    decision.runtime_join.has_merge_keys = true;
                    decision.runtime_join.merge_outer_presorted = outer_presorted;
                    decision.runtime_join.merge_inner_presorted = inner_presorted;
                    if (join_right_relation_in_right_tree)
                    {
                        decision.runtime_join.left_hash_key.qualifier =
                            join.left_hash_qualifier;
                        decision.runtime_join.left_hash_key.column_name =
                            join.left_hash_column;
                        decision.runtime_join.right_hash_key.qualifier =
                            join.right_hash_qualifier;
                        decision.runtime_join.right_hash_key.column_name =
                            join.right_hash_column;
                        decision.runtime_join.left_merge_key.qualifier =
                            join.left_hash_qualifier;
                        decision.runtime_join.left_merge_key.column_name =
                            join.left_hash_column;
                        decision.runtime_join.right_merge_key.qualifier =
                            join.right_hash_qualifier;
                        decision.runtime_join.right_merge_key.column_name =
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
                        decision.runtime_join.left_merge_key.qualifier =
                            join.right_hash_qualifier;
                        decision.runtime_join.left_merge_key.column_name =
                            join.right_hash_column;
                        decision.runtime_join.right_merge_key.qualifier =
                            join.left_hash_qualifier;
                        decision.runtime_join.right_merge_key.column_name =
                            join.left_hash_column;
                    }
                }

                auto wrapMergeInputSort =
                    [&](const std::shared_ptr<PlanNode> &input_plan,
                        const std::shared_ptr<Path> &input_path,
                        const RuntimePlanMergeKey &merge_key,
                        uint64_t input_rows,
                        bool requires_sort)
                        -> std::pair<std::shared_ptr<PlanNode>, std::shared_ptr<Path>> {
                    if (!requires_sort)
                    {
                        return {input_plan, input_path};
                    }

                    const auto ordering_keys =
                        buildSyntheticSortOrderingKeys(merge_key);
                    const auto sort_key_texts =
                        buildSyntheticSortKeyTexts(merge_key);
                    const CostEstimate child_cost =
                        input_path
                            ? input_path->cost()
                            : (input_plan && input_plan->costEvidence().has_value()
                                   ? *input_plan->costEvidence()
                                   : CostEstimate{});
                    const uint64_t child_row_width =
                        child_cost.row_width_bytes > 0
                            ? child_cost.row_width_bytes
                            : std::max<uint64_t>(32,
                                                 std::max<uint64_t>(
                                                     1,
                                                     merge_cost.row_width_bytes / 2));
                    const CostEstimate sort_cost = active_cost_model.costSort(
                        input_rows,
                        child_row_width,
                        std::max<uint64_t>(1, ordering_keys.size()),
                        ctx);

                    auto sort_plan =
                        std::make_shared<SortNode>(input_plan, sort_key_texts);
                    sort_plan->setCost(sort_cost);

                    auto sort_path = std::make_shared<SortPath>(
                        input_path,
                        ordering_keys,
                        child_row_width,
                        sort_cost);
                    AccessPathDescriptor sort_descriptor;
                    sort_descriptor.family = "SORT";
                    sort_descriptor.parameterized =
                        input_path &&
                        input_path->accessDescriptor().parameterized;
                    if (input_path)
                    {
                        sort_descriptor.required_outer_relation_indexes =
                            input_path->accessDescriptor()
                                .required_outer_relation_indexes;
                    }
                    sort_descriptor.ordered_output = true;
                    sort_descriptor.order_complete = true;
                    sort_descriptor.ordered_prefix_length =
                        std::max<uint64_t>(1, ordering_keys.size());
                    sort_descriptor.interesting_order_score =
                        static_cast<double>(sort_descriptor.ordered_prefix_length);
                    sort_descriptor.ordering_keys = ordering_keys;
                    sort_path->setAccessDescriptor(std::move(sort_descriptor));
                    return {std::move(sort_plan), std::move(sort_path)};
                };

                if (chosen_method == ChosenJoinMethod::HASH_JOIN)
                {
                    std::vector<parser::v3::Expression *> hash_keys_outer;
                    std::vector<parser::v3::Expression *> hash_keys_inner;
                    auto hash_plan = std::make_shared<HashJoinNode>(
                        join_type,
                        left_tree.plan,
                        right_tree.plan,
                        const_cast<parser::v3::Expression *>(join.condition),
                        hash_keys_outer,
                        hash_keys_inner);
                    hash_plan->setJoinCondString(decision.runtime_join.condition_text);
                    hash_plan->setCost(hash_cost);
                    decision.plan = hash_plan;
                    decision.path = std::make_shared<HashJoinPath>(
                        join_type,
                        left_tree.path,
                        right_tree.path,
                        const_cast<parser::v3::Expression *>(join.condition),
                        hash_keys_outer,
                        hash_keys_inner,
                        decision.selectivity,
                        hash_cost);
                }
                else if (chosen_method == ChosenJoinMethod::MERGE_JOIN)
                {
                    std::vector<parser::v3::Expression *> merge_keys_outer;
                    std::vector<parser::v3::Expression *> merge_keys_inner;
                    auto [merge_outer_plan, merge_outer_path] = wrapMergeInputSort(
                        left_tree.plan,
                        left_tree.path,
                        decision.runtime_join.left_merge_key,
                        left_tree.rows,
                        merge_legality.requires_sort_outer);
                    auto [merge_inner_plan, merge_inner_path] = wrapMergeInputSort(
                        right_tree.plan,
                        right_tree.path,
                        decision.runtime_join.right_merge_key,
                        right_tree.rows,
                        merge_legality.requires_sort_inner);
                    auto merge_plan = std::make_shared<MergeJoinNode>(
                        join_type,
                        merge_outer_plan,
                        merge_inner_plan,
                        const_cast<parser::v3::Expression *>(join.condition),
                        merge_keys_outer,
                        merge_keys_inner,
                        outer_presorted,
                        inner_presorted);
                    merge_plan->setJoinCondString(decision.runtime_join.condition_text);
                    merge_plan->setCost(merge_cost);
                    decision.plan = merge_plan;
                    decision.path = std::make_shared<MergeJoinPath>(
                        join_type,
                        merge_outer_path,
                        merge_inner_path,
                        const_cast<parser::v3::Expression *>(join.condition),
                        merge_keys_outer,
                        merge_keys_inner,
                        decision.selectivity,
                        outer_presorted,
                        inner_presorted,
                        merge_cost);
                }
                else
                {
                    auto nested_plan = std::make_shared<NestedLoopJoinNode>(
                        join_type,
                        left_tree.plan,
                        right_tree.plan,
                        const_cast<parser::v3::Expression *>(join.condition));
                    nested_plan->setJoinCondString(decision.runtime_join.condition_text);
                    nested_plan->setCost(nl_cost);
                    decision.plan = nested_plan;
                    decision.path = std::make_shared<NestedLoopJoinPath>(
                        join_type,
                        left_tree.path,
                        right_tree.path,
                        const_cast<parser::v3::Expression *>(join.condition),
                        decision.selectivity,
                        nl_cost);
                }

                if (decision.path)
                {
                    AccessPathDescriptor join_descriptor;
                    join_descriptor.family = decision.runtime_join.method;
                    join_descriptor.parameterized = parameterized_inner;
                    if (parameterized_inner)
                    {
                        join_descriptor.required_outer_relation_indexes =
                            left_tree.relation_order;
                    }

                    if (chosen_method == ChosenJoinMethod::MERGE_JOIN &&
                        decision.runtime_join.has_merge_keys)
                    {
                        join_descriptor.ordered_output = true;
                        join_descriptor.order_complete = true;
                        join_descriptor.ordered_prefix_length = 1;
                        join_descriptor.ordering_keys.push_back(makeOrderingKey(
                            decision.runtime_join.left_merge_key.column_name));
                        join_descriptor.interesting_order_score = 1.0;
                    }
                    else if (chosen_method != ChosenJoinMethod::HASH_JOIN &&
                             left_tree.path)
                    {
                        const auto &outer_descriptor =
                            left_tree.path->accessDescriptor();
                        join_descriptor.ordered_output =
                            outer_descriptor.ordered_output;
                        join_descriptor.ordered_prefix_length =
                            outer_descriptor.ordered_prefix_length;
                        join_descriptor.ordering_keys =
                            outer_descriptor.ordering_keys;
                        join_descriptor.order_complete =
                            outer_descriptor.order_complete;
                        join_descriptor.interesting_order_score =
                            outer_descriptor.interesting_order_score;
                    }

                    decision.path->setAccessDescriptor(std::move(join_descriptor));
                }

                if (!decision.runtime_join.has_hash_keys)
                {
                    decision.runtime_join.left_hash_key.qualifier =
                        relationLookupName(left_endpoint_relation);
                    decision.runtime_join.right_hash_key.qualifier =
                        relationLookupName(right_endpoint_relation);
                }

                if (join_right_relation_in_right_tree &&
                    right_tree.relation_order.size() == 1 &&
                    right_tree.relation_order.front() == join.right_relation_index &&
                    join.equi_join &&
                    !parameterized_inner &&
                    !isZeroId(join.right_hash_column_id))
                {
                    const auto runtime_filter_index =
                        std::find_if(candidate_relation.indexes.begin(),
                                     candidate_relation.indexes.end(),
                                     [&](const core::CatalogManager::IndexInfo &index) {
                                         if (index.column_ids.empty() ||
                                             index.column_ids.front() !=
                                                 join.right_hash_column_id)
                                         {
                                             return false;
                                         }
                                         PlannerFamilyLoweringRequest request;
                                         request.index_type = index.index_type;
                                         request.predicate_shape =
                                             PredicateMatchShape::EQUALITY;
                                         const auto lowered =
                                             lowerPlannerFamily(request);
                                         return lowered.queryability_state !=
                                                    AccessPathQueryabilityState::INVALID &&
                                                lowered.exactness_class ==
                                                    AccessPathExactnessClass::EXACT_KEY;
                                     });
                    if (runtime_filter_index != candidate_relation.indexes.end())
                    {
                        decision.runtime_filter_enabled = true;
                        decision.runtime_filter_column = join.right_hash_column;
                        decision.runtime_filter_index_name =
                            runtime_filter_index->index_name;
                        decision.runtime_filter_index_id_text =
                            runtime_filter_index->index_id.toString();
                        appendRuntimeTrace(considered_paths,
                                           "RUNTIME_FILTER",
                                           join_subject,
                                           "INDEX_RUNTIME_FILTER[" +
                                               runtime_filter_index->index_name + "]",
                                           "CHOSEN",
                                           "join-key filter on right relation",
                                           0.0,
                                           0.0,
                                           right_tree.path
                                               ? right_tree.path->rows()
                                               : 0);
                    }
                    else
                    {
                        appendRuntimeTrace(rejected_paths,
                                           "RUNTIME_FILTER",
                                           join_subject,
                                           "INDEX_RUNTIME_FILTER",
                                           "REJECTED",
                                           "no leading btree index on join key",
                                           0.0,
                                           0.0,
                                           right_tree.path
                                               ? right_tree.path->rows()
                                               : 0);
                    }
                }

                return decision;
            };

        RuntimePlanSearchSummary search_summary;
        search_summary.requested_strategy =
            plannerJoinSearchName(planner_controls.join_search);
        search_summary.search_budget = planner_controls.search_depth;
        auto joinSearchStrategyName =
            [](JoinSearchStrategy strategy) -> const char * {
                switch (strategy)
                {
                    case JoinSearchStrategy::EXHAUSTIVE_DP:
                        return "EXHAUSTIVE_DP";
                    case JoinSearchStrategy::BOUNDED_DP:
                        return "BOUNDED_DP";
                    case JoinSearchStrategy::HYPERGRAPH_GREEDY:
                        return "HYPERGRAPH_GREEDY";
                    case JoinSearchStrategy::HEURISTIC_GREEDY:
                        return "HEURISTIC_GREEDY";
                    case JoinSearchStrategy::INPUT_ORDER:
                        return "INPUT_ORDER_ONLY";
                    case JoinSearchStrategy::AUTO:
                    default:
                        return "AUTO";
                }
            };

        auto leafCandidateForPath =
            [&](const std::shared_ptr<Path> &path)
                -> std::optional<std::pair<size_t, size_t>> {
                for (size_t relation_index = 0;
                     relation_index < access_bundles.size();
                     ++relation_index)
                {
                    for (size_t candidate_index = 0;
                         candidate_index <
                             access_bundles[relation_index].candidates.size();
                         ++candidate_index)
                    {
                        if (access_bundles[relation_index]
                                .candidates[candidate_index]
                                .path == path)
                        {
                            return std::make_pair(relation_index,
                                                  candidate_index);
                        }
                    }
                }
                return std::nullopt;
            };

        auto relationOrderContains =
            [](const std::vector<size_t> &relation_order,
               size_t relation_index) -> bool {
                return std::find(relation_order.begin(),
                                 relation_order.end(),
                                 relation_index) != relation_order.end();
            };

        std::function<core::Status(const std::shared_ptr<Path> &,
                                   MaterializedJoinTree &)>
            materializeBackendPlan;
        materializeBackendPlan =
            [&](const std::shared_ptr<Path> &backend_path,
                MaterializedJoinTree &materialized_out) -> core::Status {
                if (!backend_path)
                {
                    SET_ERROR_CONTEXT(ctx,
                                      core::Status::INTERNAL_ERROR,
                                      "Join backend returned null path");
                    return core::Status::INTERNAL_ERROR;
                }

                if (const auto leaf_candidate = leafCandidateForPath(backend_path);
                    leaf_candidate.has_value())
                {
                    const auto [relation_index, candidate_index] = *leaf_candidate;
                    const auto &candidate =
                        access_bundles[relation_index].candidates[candidate_index];
                    materialized_out.path = backend_path;
                    materialized_out.plan = candidate.plan;
                    materialized_out.rows =
                        backend_path ? backend_path->rows() : 0;
                    materialized_out.relation_order = {relation_index};
                    materialized_out.runtime_relations = {
                        candidate.runtime_relation};
                    return core::Status::OK;
                }

                std::shared_ptr<Path> left_backend_path;
                std::shared_ptr<Path> right_backend_path;
                parser::JoinType backend_join_type = parser::JoinType::INNER;
                parser::v3::Expression *backend_join_condition = nullptr;
                if (auto nested =
                        std::dynamic_pointer_cast<NestedLoopJoinPath>(backend_path))
                {
                    left_backend_path = nested->outerPath();
                    right_backend_path = nested->innerPath();
                    backend_join_type = nested->joinType();
                    backend_join_condition = nested->joinCondition();
                }
                else if (auto hash =
                             std::dynamic_pointer_cast<HashJoinPath>(backend_path))
                {
                    left_backend_path = hash->outerPath();
                    right_backend_path = hash->innerPath();
                    backend_join_type = hash->joinType();
                    backend_join_condition = hash->joinCondition();
                }
                else if (auto merge =
                             std::dynamic_pointer_cast<MergeJoinPath>(backend_path))
                {
                    left_backend_path = merge->outerPath();
                    right_backend_path = merge->innerPath();
                    backend_join_type = merge->joinType();
                    backend_join_condition = merge->joinCondition();
                }
                else
                {
                    SET_ERROR_CONTEXT(ctx,
                                      core::Status::INTERNAL_ERROR,
                                      "Join backend returned unrecognized composite path");
                    return core::Status::INTERNAL_ERROR;
                }

                MaterializedJoinTree left_tree;
                MaterializedJoinTree right_tree;
                auto status = materializeBackendPlan(left_backend_path, left_tree);
                if (status != core::Status::OK)
                {
                    return status;
                }
                status = materializeBackendPlan(right_backend_path, right_tree);
                if (status != core::Status::OK)
                {
                    return status;
                }

                JoinDecision best_join;
                size_t matching_candidates = 0;
                for (const auto &join : planned_out.resolved_query.joins)
                {
                    const bool left_relation_in_left_tree =
                        relationOrderContains(left_tree.relation_order,
                                              join.left_relation_index);
                    const bool left_relation_in_right_tree =
                        relationOrderContains(right_tree.relation_order,
                                              join.left_relation_index);
                    const bool right_relation_in_left_tree =
                        relationOrderContains(left_tree.relation_order,
                                              join.right_relation_index);
                    const bool right_relation_in_right_tree =
                        relationOrderContains(right_tree.relation_order,
                                              join.right_relation_index);
                    const bool split_across_subtrees =
                        (left_relation_in_left_tree &&
                         right_relation_in_right_tree) ||
                        (left_relation_in_right_tree &&
                         right_relation_in_left_tree);
                    if (!split_across_subtrees)
                    {
                        continue;
                    }
                    if (join.join_type != backend_join_type)
                    {
                        continue;
                    }
                    if (const_cast<parser::v3::Expression *>(join.condition) !=
                        backend_join_condition)
                    {
                        continue;
                    }

                    const bool join_right_relation_in_right_tree =
                        right_relation_in_right_tree;
                    auto candidate = buildSubtreeJoinDecision(
                        left_tree,
                        right_tree,
                        join,
                        join_right_relation_in_right_tree);
                    if (!candidate.valid)
                    {
                        if (ctx != nullptr && ctx->code != core::Status::OK)
                        {
                            return ctx->code;
                        }
                        continue;
                    }
                    ++matching_candidates;
                    if (!best_join.valid ||
                        candidate.total_cost < best_join.total_cost)
                    {
                        best_join = std::move(candidate);
                    }
                }

                if (!best_join.valid)
                {
                    if (backend_join_type != parser::JoinType::CROSS ||
                        backend_join_condition != nullptr)
                    {
                        SET_ERROR_CONTEXT(
                            ctx,
                            core::Status::INTERNAL_ERROR,
                            "Canonical join backend produced a composite join with no matching frozen join edge");
                        return core::Status::INTERNAL_ERROR;
                    }

                    ResolvedJoin cross_join;
                    cross_join.source_join_index =
                        std::numeric_limits<size_t>::max();
                    cross_join.left_relation_index =
                        left_tree.relation_order.empty()
                            ? 0
                            : left_tree.relation_order.front();
                    cross_join.right_relation_index =
                        right_tree.relation_order.empty()
                            ? 0
                            : right_tree.relation_order.front();
                    cross_join.join_type = parser::JoinType::CROSS;
                    cross_join.condition = nullptr;
                    cross_join.condition_text.clear();
                    const auto legality =
                        classifyJoinLegality(cross_join.join_type, false, false);
                    cross_join.legality_class = legality.legality_class;
                    cross_join.reorderable = legality.reorderable;
                    cross_join.preserves_left_rows =
                        legality.preserves_left_rows;
                    cross_join.preserves_right_rows =
                        legality.preserves_right_rows;
                    cross_join.null_introduces_left =
                        legality.null_introduces_left;
                    cross_join.null_introduces_right =
                        legality.null_introduces_right;
                    cross_join.requires_original_order =
                        legality.requires_original_order;
                    best_join = buildSubtreeJoinDecision(left_tree,
                                                         right_tree,
                                                         cross_join,
                                                         true,
                                                         true);
                    if (!best_join.valid)
                    {
                        if (ctx != nullptr && ctx->code != core::Status::OK)
                        {
                            return ctx->code;
                        }
                        return core::Status::INTERNAL_ERROR;
                    }
                }

                materialized_out.path = best_join.path;
                materialized_out.plan = best_join.plan;
                materialized_out.rows = best_join.rows;
                materialized_out.relation_order = left_tree.relation_order;
                materialized_out.relation_order.insert(
                    materialized_out.relation_order.end(),
                    right_tree.relation_order.begin(),
                    right_tree.relation_order.end());
                materialized_out.runtime_relations = left_tree.runtime_relations;
                materialized_out.runtime_relations.insert(
                    materialized_out.runtime_relations.end(),
                    right_tree.runtime_relations.begin(),
                    right_tree.runtime_relations.end());
                if (best_join.runtime_filter_enabled)
                {
                    auto runtime_relation_it = std::find_if(
                        materialized_out.runtime_relations.begin(),
                        materialized_out.runtime_relations.end(),
                        [&](RuntimePlanRelation &relation) {
                            return relation.source_relation_index ==
                                   best_join.candidate_relation_index;
                        });
                    if (runtime_relation_it !=
                        materialized_out.runtime_relations.end())
                    {
                        runtime_relation_it->runtime_filter_enabled = true;
                        runtime_relation_it->runtime_filter_column =
                            best_join.runtime_filter_column;
                        runtime_relation_it->runtime_filter_index_name =
                            best_join.runtime_filter_index_name;
                        runtime_relation_it->runtime_filter_index_id_text =
                            best_join.runtime_filter_index_id_text;
                        const auto partition_key_it =
                            std::find_if(runtime_relation_it->partition_key_columns.begin(),
                                         runtime_relation_it->partition_key_columns.end(),
                                         [&](const std::string &column_name) {
                                             return core::IdentifierUtils::namesMatch(
                                                 column_name,
                                                 false,
                                                 best_join.runtime_filter_column,
                                                 false);
                                         });
                        if (partition_key_it !=
                            runtime_relation_it->partition_key_columns.end())
                        {
                            runtime_relation_it->runtime_partition_pruning_eligible =
                                true;
                            if (std::find(
                                    runtime_relation_it
                                        ->runtime_partition_pruning_sources.begin(),
                                    runtime_relation_it
                                        ->runtime_partition_pruning_sources.end(),
                                    "RUNTIME_FILTER") ==
                                runtime_relation_it
                                    ->runtime_partition_pruning_sources.end())
                            {
                                runtime_relation_it
                                    ->runtime_partition_pruning_sources.push_back(
                                        "RUNTIME_FILTER");
                            }
                        }
                    }
                }
                if (best_join.runtime_join.parameterized_dependency)
                {
                    auto runtime_relation_it = std::find_if(
                        materialized_out.runtime_relations.begin(),
                        materialized_out.runtime_relations.end(),
                        [&](RuntimePlanRelation &relation) {
                            return relation.source_relation_index ==
                                   best_join.candidate_relation_index;
                        });
                    if (runtime_relation_it !=
                        materialized_out.runtime_relations.end())
                    {
                        runtime_relation_it->parameterized = true;
                        runtime_relation_it->required_outer_relation_indexes =
                            best_join.runtime_join
                                .parameter_dependency_relation_indexes;
                        runtime_relation_it->required_outer_relation_aliases =
                            best_join.runtime_join
                                .parameter_dependency_relation_aliases;
                        if (std::find(runtime_relation_it->candidate_scan_families.begin(),
                                      runtime_relation_it->candidate_scan_families.end(),
                                      "PARAMETERIZED_INDEX_SCAN") ==
                            runtime_relation_it->candidate_scan_families.end())
                        {
                            runtime_relation_it->candidate_scan_families.push_back(
                                "PARAMETERIZED_INDEX_SCAN");
                        }
                        if ((runtime_relation_it->scan_kind == "INDEX_SCAN" ||
                             runtime_relation_it->scan_kind ==
                                 "INDEX_ONLY_SCAN") &&
                            (runtime_relation_it->scan_family == "INDEX_SCAN" ||
                             runtime_relation_it->scan_family ==
                                 "INDEX_ONLY_SCAN"))
                        {
                            runtime_relation_it->scan_family =
                                "PARAMETERIZED_INDEX_SCAN";
                            if (std::find(runtime_relation_it->scan_family_tags.begin(),
                                          runtime_relation_it->scan_family_tags.end(),
                                          runtime_relation_it->scan_kind) ==
                                runtime_relation_it->scan_family_tags.end())
                            {
                                runtime_relation_it->scan_family_tags.push_back(
                                    runtime_relation_it->scan_kind);
                            }
                        }
                    }
                }
                materialized_out.runtime_joins = left_tree.runtime_joins;
                materialized_out.runtime_joins.insert(
                    materialized_out.runtime_joins.end(),
                    right_tree.runtime_joins.begin(),
                    right_tree.runtime_joins.end());
                materialized_out.runtime_joins.push_back(best_join.runtime_join);
                return core::Status::OK;
            };

        std::vector<size_t> relation_order;
        std::vector<RuntimePlanRelation> runtime_relations;
        std::vector<RuntimePlanJoinStep> runtime_joins;
        std::shared_ptr<Path> current_path;
        std::shared_ptr<PlanNode> current_plan;
        uint64_t current_rows = 0;

        if (planned_out.resolved_query.relations.size() <= 1)
        {
            search_summary.selected_strategy = "SINGLE_RELATION";
            if (!access_bundles.empty() &&
                !access_bundles[0].candidates.empty())
            {
                relation_order.push_back(0);
                runtime_relations.push_back(
                    access_bundles[0].candidates.front().runtime_relation);
                current_path = access_bundles[0].candidates.front().path;
                current_plan = access_bundles[0].candidates.front().plan;
                current_rows = current_path ? current_path->rows() : 0;
            }
        }
        else
        {
            JoinOrderingOptimizer join_optimizer(active_cost_model,
                                                 selectivity_estimator_);
            JoinPlanningControls join_controls;
            switch (planner_controls.join_search)
            {
                case PlannerJoinSearchMode::EXHAUSTIVE_DP:
                    join_controls.strategy = JoinSearchStrategy::EXHAUSTIVE_DP;
                    break;
                case PlannerJoinSearchMode::BOUNDED_DP:
                    join_controls.strategy = JoinSearchStrategy::BOUNDED_DP;
                    break;
                case PlannerJoinSearchMode::HYPERGRAPH_GREEDY:
                    join_controls.strategy = JoinSearchStrategy::HYPERGRAPH_GREEDY;
                    break;
                case PlannerJoinSearchMode::HEURISTIC_GREEDY:
                    join_controls.strategy = JoinSearchStrategy::HEURISTIC_GREEDY;
                    break;
                case PlannerJoinSearchMode::INPUT_ORDER:
                    join_controls.strategy = JoinSearchStrategy::INPUT_ORDER;
                    break;
                case PlannerJoinSearchMode::AUTO:
                default:
                    join_controls.strategy = JoinSearchStrategy::AUTO;
                    break;
            }
            join_controls.max_exhaustive_relations =
                planner_controls.exhaustive_join_limit;
            join_controls.max_bounded_dp_relations =
                planner_controls.bounded_dp_join_limit;
            join_controls.max_states_considered =
                planner_controls.max_states_considered;
            join_controls.fallback_prune_level =
                planner_controls.fallback_prune_level;
            if (planner_controls.search_depth > 0)
            {
                join_controls.max_pair_evaluations =
                    planner_controls.search_depth;
            }
            join_optimizer.setPlanningControls(join_controls);

            for (const auto &bundle : access_bundles)
            {
                const auto &relation =
                    planned_out.resolved_query.relations[bundle.relation_index];
                std::vector<std::shared_ptr<Path>> candidate_paths;
                candidate_paths.reserve(bundle.candidates.size());
                for (const auto &candidate : bundle.candidates)
                {
                    if (candidate.path)
                    {
                        candidate_paths.push_back(candidate.path);
                    }
                }
                join_optimizer.addRelation(
                    relation.resolved ? relation.table_info.table_id : core::ID{},
                    displayRelationName(relation),
                    relationLookupName(relation),
                    candidate_paths);
            }

            for (const auto &join : planned_out.resolved_query.joins)
            {
                const size_t edge_index = join_optimizer.addJoinEdge(
                    join.left_relation_index,
                    join.right_relation_index,
                    join.join_type,
                    const_cast<parser::v3::Expression *>(join.condition),
                    join.source_join_index,
                    join.legality_class,
                    join.reorderable,
                    join.requires_original_order);
                join_optimizer.setJoinSelectivity(
                    edge_index,
                    join.join_type == parser::JoinType::CROSS
                        ? 1.0
                        : estimateJoinSelectivityFor(join));
            }

            auto backend_path = join_optimizer.optimize(ctx);
            if (!backend_path)
            {
                if (ctx != nullptr && ctx->code != core::Status::OK)
                {
                    return ctx->code;
                }
                SET_ERROR_CONTEXT(ctx,
                                  core::Status::INTERNAL_ERROR,
                                  "Canonical join backend failed to produce a multi-relation plan");
                return core::Status::INTERNAL_ERROR;
            }

            const auto &join_telemetry = join_optimizer.lastTelemetry();
            search_summary.requested_strategy =
                joinSearchStrategyName(join_telemetry.requested_strategy);
            search_summary.selected_strategy =
                joinSearchStrategyName(join_telemetry.selected_strategy);
            search_summary.search_budget =
                join_telemetry.max_pair_evaluations;
            search_summary.considered_state_count =
                join_telemetry.considered_state_count;
            search_summary.pruned_state_count =
                join_telemetry.pruned_state_count;
            search_summary.pair_evaluation_count =
                join_telemetry.pair_evaluation_count;
            search_summary.max_pair_evaluations =
                join_telemetry.max_pair_evaluations;
            search_summary.max_states_considered =
                join_telemetry.max_states_considered;
            search_summary.exhaustive_join_limit =
                join_telemetry.exhaustive_join_limit;
            search_summary.bounded_dp_join_limit =
                join_telemetry.bounded_dp_join_limit;
            search_summary.fallback_prune_level =
                join_telemetry.fallback_prune_level;
            search_summary.fallback_reason = join_telemetry.fallback_reason;
            search_summary.fallback_threshold_name =
                join_telemetry.fallback_threshold_name;
            search_summary.fallback_threshold_value =
                join_telemetry.fallback_threshold_value;

            MaterializedJoinTree materialized_root;
            const auto materialize_status =
                materializeBackendPlan(backend_path, materialized_root);
            if (materialize_status != core::Status::OK)
            {
                return materialize_status;
            }

            relation_order = std::move(materialized_root.relation_order);
            runtime_relations = std::move(materialized_root.runtime_relations);
            runtime_joins = std::move(materialized_root.runtime_joins);
            current_path = materialized_root.path;
            current_plan = materialized_root.plan;
            current_rows = materialized_root.rows;
            planned_out.disconnected_join_graph =
                std::any_of(runtime_joins.begin(),
                            runtime_joins.end(),
                            [](const RuntimePlanJoinStep &step) {
                                return step.disconnected_component;
                            });
            planned_out.reordered_relations =
                join_optimizer.lastStrategyUsed() !=
                    JoinSearchStrategy::INPUT_ORDER &&
                !relation_order.empty() &&
                std::any_of(
                    relation_order.begin(),
                    relation_order.end(),
                    [index = size_t{0}](size_t relation_index) mutable {
                        return relation_index != index++;
                    });
        }

        auto estimateRowWidth = [&](const std::vector<size_t> &relations) -> uint64_t {
            uint64_t width = 0;
            if (relations.empty())
            {
                return planner_params.default_row_width_bytes;
            }
            for (size_t relation_index : relations)
            {
                if (relation_index >= planned_out.resolved_query.relations.size())
                {
                    continue;
                }
                if (relation_index < relation_row_width_bytes.size() &&
                    relation_row_width_bytes[relation_index] > 0)
                {
                    width += relation_row_width_bytes[relation_index];
                    continue;
                }
                width += static_cast<uint64_t>(
                    std::max<size_t>(
                        1,
                        planned_out.resolved_query.relations[relation_index]
                            .columns.size()) *
                    16);
            }
            return std::max<uint64_t>(planner_params.default_row_width_bytes,
                                      width);
        };

        const uint64_t row_width = estimateRowWidth(relation_order);

        if (!query_aggregates.empty() ||
            !select_stmt->group_by.empty() ||
            select_stmt->having != nullptr)
        {
            const AccessPathDescriptor input_descriptor =
                current_path ? current_path->accessDescriptor()
                             : AccessPathDescriptor{};
            const bool aggregate_reuses_group_order =
                !select_stmt->group_by.empty() && current_path &&
                orderingSatisfiesGrouping(current_path->accessDescriptor(),
                                         select_stmt->group_by,
                                         pool);
            const bool aggregate_has_distinct_inputs =
                std::any_of(query_aggregates.begin(),
                            query_aggregates.end(),
                            [](const parser::v3::FunctionCallExpr *func) {
                                return func != nullptr && func->distinct;
                            });
            const std::string hash_strategy =
                aggregate_has_distinct_inputs
                    ? "HASH_DISTINCT_AGGREGATE"
                    : "HASH_AGGREGATE";
            const std::string group_strategy =
                aggregate_has_distinct_inputs
                    ? "GROUP_DISTINCT_AGGREGATE_REUSE_ORDER"
                    : "GROUP_AGGREGATE_REUSE_ORDER";
            uint64_t estimated_groups = select_stmt->group_by.empty()
                ? 1
                : std::max<uint64_t>(1, current_rows / 10);
            const CostEstimate hash_aggregate_cost =
                active_cost_model.costAggregate(current_rows,
                                                estimated_groups,
                                                query_aggregates.size(),
                                                ctx);
            const CostEstimate group_aggregate_cost =
                active_cost_model.costGroupAggregate(current_rows,
                                                     estimated_groups,
                                                     query_aggregates.size(),
                                                     ctx);
            if (spill_policy == PlannerSpillPolicy::DISALLOW &&
                hash_aggregate_cost.spill_expected)
            {
                SET_ERROR_CONTEXT(ctx,
                                  core::Status::CONFIGURATION_LIMIT_EXCEEDED,
                                  "Aggregate operator exceeds work_mem under spill-disallow policy");
                return core::Status::CONFIGURATION_LIMIT_EXCEEDED;
            }
            const bool choose_group_aggregate =
                aggregate_reuses_group_order &&
                group_aggregate_cost.total_cost <= hash_aggregate_cost.total_cost;
            appendRuntimeTrace(considered_paths,
                               "UPPER_STAGE_STRATEGY",
                               "query:aggregate",
                               choose_group_aggregate ? group_strategy : hash_strategy,
                               "CHOSEN",
                               choose_group_aggregate
                                   ? "ordered input makes streaming/group aggregate cheaper than hash aggregate"
                                   : "hash aggregate remains the selected aggregate strategy",
                               choose_group_aggregate
                                   ? group_aggregate_cost.startup_cost
                                   : hash_aggregate_cost.startup_cost,
                               choose_group_aggregate
                                   ? group_aggregate_cost.total_cost
                                   : hash_aggregate_cost.total_cost,
                               choose_group_aggregate
                                   ? group_aggregate_cost.rows
                                   : hash_aggregate_cost.rows);
            if (aggregate_reuses_group_order)
            {
                appendRuntimeTrace(rejected_paths,
                                   "UPPER_STAGE_STRATEGY",
                                   "query:aggregate",
                                   choose_group_aggregate ? hash_strategy : group_strategy,
                                   "REJECTED",
                                   choose_group_aggregate
                                       ? "ordered/group aggregate is cheaper on existing input order"
                                       : "hash aggregate remains cheaper even though group order is available",
                                   choose_group_aggregate
                                       ? hash_aggregate_cost.startup_cost
                                       : group_aggregate_cost.startup_cost,
                                   choose_group_aggregate
                                       ? hash_aggregate_cost.total_cost
                                       : group_aggregate_cost.total_cost,
                                   choose_group_aggregate
                                       ? hash_aggregate_cost.rows
                                       : group_aggregate_cost.rows);
            }
            else if (!select_stmt->group_by.empty())
            {
                appendRuntimeTrace(rejected_paths,
                                   "UPPER_STAGE_STRATEGY",
                                   "query:aggregate",
                                   group_strategy,
                                   "REJECTED",
                                   "input path does not satisfy GROUP BY key order",
                                   group_aggregate_cost.startup_cost,
                                   group_aggregate_cost.total_cost,
                                   group_aggregate_cost.rows);
            }
            const CostEstimate aggregate_cost =
                choose_group_aggregate ? group_aggregate_cost : hash_aggregate_cost;
            current_path = std::make_shared<AggregatePath>(current_path,
                                                           select_stmt->group_by,
                                                           query_aggregates,
                                                           select_stmt->having,
                                                           estimated_groups,
                                                           aggregate_cost,
                                                           select_stmt->grouping_type,
                                                           select_stmt->grouping_sets);
            if (current_path)
            {
                AccessPathDescriptor aggregate_descriptor;
                aggregate_descriptor.family = "AGGREGATE";
                aggregate_descriptor.parameterized =
                    input_descriptor.parameterized;
                aggregate_descriptor.required_outer_relation_indexes =
                    input_descriptor.required_outer_relation_indexes;
                if (choose_group_aggregate)
                {
                    aggregate_descriptor.ordered_output = true;
                    aggregate_descriptor.order_complete = true;
                    aggregate_descriptor.ordered_prefix_length =
                        select_stmt->group_by.size();
                    aggregate_descriptor.interesting_order_score =
                        static_cast<double>(select_stmt->group_by.size());
                    for (auto *group_expr : select_stmt->group_by)
                    {
                        aggregate_descriptor.ordering_keys.push_back(
                            makeOrderingKey(expressionToString(group_expr, pool)));
                    }
                    appendRuntimeTrace(considered_paths,
                                       "SORT_AVOIDANCE",
                                       "query:group_by",
                                       "GROUP_ORDER_REUSE",
                                       "CHOSEN",
                                       "aggregate input already satisfies GROUP BY key order",
                                       aggregate_cost.startup_cost,
                                       aggregate_cost.total_cost,
                                       aggregate_cost.rows);
                }
                current_path->setAccessDescriptor(std::move(aggregate_descriptor));
            }
            auto aggregate_plan = std::make_shared<AggregateNode>(current_plan,
                                                                  select_stmt->group_by,
                                                                  query_aggregates,
                                                                  select_stmt->having,
                                                                  select_stmt->grouping_type,
                                                                  select_stmt->grouping_sets);
            aggregate_plan->setCost(aggregate_cost);
            current_plan = aggregate_plan;
            current_rows = aggregate_cost.rows;
        }

        if (!query_windows.empty())
        {
            const AccessPathDescriptor input_descriptor =
                current_path ? current_path->accessDescriptor()
                             : AccessPathDescriptor{};
            const bool window_reuses_input_order =
                current_path &&
                orderingSatisfiesWindow(current_path->accessDescriptor(),
                                        query_windows,
                                        pool);
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

            CostEstimate window_cost = active_cost_model.costWindow(current_rows,
                                                              row_width,
                                                              partition_keys,
                                                              order_keys,
                                                              window_functions.size(),
                                                              window_reuses_input_order,
                                                              ctx);
            if (spill_policy == PlannerSpillPolicy::DISALLOW &&
                window_cost.spill_expected)
            {
                SET_ERROR_CONTEXT(ctx,
                                  core::Status::CONFIGURATION_LIMIT_EXCEEDED,
                                  "Window operator exceeds work_mem under spill-disallow policy");
                return core::Status::CONFIGURATION_LIMIT_EXCEEDED;
            }
            appendRuntimeTrace(considered_paths,
                               "UPPER_STAGE_STRATEGY",
                               "query:window",
                               window_reuses_input_order
                                   ? "WINDOW_REUSE_ORDER"
                                   : "WINDOW_LOCAL_SORT",
                               "CHOSEN",
                               window_reuses_input_order
                                   ? "window stage reuses existing partition/order path"
                                   : "window stage requires internal local sort",
                               window_cost.startup_cost,
                               window_cost.total_cost,
                               window_cost.rows);
            if (!window_reuses_input_order)
            {
                appendRuntimeTrace(rejected_paths,
                                   "UPPER_STAGE_STRATEGY",
                                   "query:window",
                                   "WINDOW_REUSE_ORDER",
                                   "REJECTED",
                                   "input path does not satisfy window partition/order keys",
                                   window_cost.startup_cost,
                                   window_cost.total_cost,
                                   window_cost.rows);
            }
            current_path = std::make_shared<WindowPath>(current_path,
                                                        query_windows,
                                                        window_cost);
            if (current_path)
            {
                AccessPathDescriptor window_descriptor = input_descriptor;
                window_descriptor.family = "WINDOW";
                if (!window_reuses_input_order)
                {
                    window_descriptor.ordered_output = false;
                    window_descriptor.order_complete = false;
                    window_descriptor.ordered_prefix_length = 0;
                    window_descriptor.ordering_keys.clear();
                    window_descriptor.interesting_order_score = 0.0;
                }
                current_path->setAccessDescriptor(std::move(window_descriptor));
            }
            auto window_plan = std::make_shared<WindowNode>(current_plan, window_functions);
            window_plan->setCost(window_cost);
            current_plan = window_plan;
            current_rows = window_cost.rows;
        }

        if (select_stmt->distinct && select_stmt->distinct_on.empty())
        {
            const auto distinct_exprs = collectDistinctExpressions(select_stmt);
            if (!distinct_exprs.empty())
            {
                const AccessPathDescriptor input_descriptor =
                    current_path ? current_path->accessDescriptor()
                                 : AccessPathDescriptor{};
                const bool ordered_distinct =
                    current_path &&
                    orderingSatisfiesExpressions(current_path->accessDescriptor(),
                                                 distinct_exprs,
                                                 pool);
                const uint64_t estimated_distinct_rows =
                    std::max<uint64_t>(1, current_rows / 10);
                const CostEstimate hash_distinct_cost =
                    active_cost_model.costDistinct(current_rows,
                                                   estimated_distinct_rows,
                                                   distinct_exprs.size(),
                                                   false,
                                                   ctx);
                const CostEstimate ordered_distinct_cost =
                    active_cost_model.costDistinct(current_rows,
                                                   estimated_distinct_rows,
                                                   distinct_exprs.size(),
                                                   true,
                                                   ctx);
                const bool choose_ordered_distinct =
                    ordered_distinct &&
                    ordered_distinct_cost.total_cost <=
                        hash_distinct_cost.total_cost;

                appendRuntimeTrace(considered_paths,
                                   "UPPER_STAGE_STRATEGY",
                                   "query:distinct",
                                   choose_ordered_distinct
                                       ? "ORDERED_DISTINCT"
                                       : "HASH_DISTINCT",
                                   "CHOSEN",
                                   choose_ordered_distinct
                                       ? "existing order is sufficient for streaming DISTINCT"
                                       : "hash distinct remains the selected strategy",
                                   choose_ordered_distinct
                                       ? ordered_distinct_cost.startup_cost
                                       : hash_distinct_cost.startup_cost,
                                   choose_ordered_distinct
                                       ? ordered_distinct_cost.total_cost
                                       : hash_distinct_cost.total_cost,
                                   choose_ordered_distinct
                                       ? ordered_distinct_cost.rows
                                       : hash_distinct_cost.rows);
                if (ordered_distinct)
                {
                    appendRuntimeTrace(rejected_paths,
                                       "UPPER_STAGE_STRATEGY",
                                       "query:distinct",
                                       choose_ordered_distinct
                                           ? "HASH_DISTINCT"
                                           : "ORDERED_DISTINCT",
                                       "REJECTED",
                                       choose_ordered_distinct
                                           ? "ordered distinct is cheaper on the current input order"
                                           : "hash distinct remains cheaper despite available order",
                                       choose_ordered_distinct
                                           ? hash_distinct_cost.startup_cost
                                           : ordered_distinct_cost.startup_cost,
                                       choose_ordered_distinct
                                           ? hash_distinct_cost.total_cost
                                           : ordered_distinct_cost.total_cost,
                                       choose_ordered_distinct
                                           ? hash_distinct_cost.rows
                                           : ordered_distinct_cost.rows);
                }
                else
                {
                    appendRuntimeTrace(rejected_paths,
                                       "UPPER_STAGE_STRATEGY",
                                       "query:distinct",
                                       "ORDERED_DISTINCT",
                                       "REJECTED",
                                       "input path does not satisfy DISTINCT key order",
                                       ordered_distinct_cost.startup_cost,
                                       ordered_distinct_cost.total_cost,
                                       ordered_distinct_cost.rows);
                }

                const CostEstimate distinct_cost =
                    choose_ordered_distinct ? ordered_distinct_cost
                                            : hash_distinct_cost;
                std::vector<parser::v3::FunctionCallExpr *> no_aggregates;
                current_path = std::make_shared<AggregatePath>(
                    current_path,
                    distinct_exprs,
                    no_aggregates,
                    nullptr,
                    estimated_distinct_rows,
                    distinct_cost,
                    parser::GroupingType::STANDARD,
                    std::vector<std::vector<parser::v3::Expression *>>{});
                if (current_path)
                {
                    AccessPathDescriptor distinct_descriptor;
                    distinct_descriptor.family = "AGGREGATE";
                    distinct_descriptor.parameterized =
                        input_descriptor.parameterized;
                    distinct_descriptor.required_outer_relation_indexes =
                        input_descriptor.required_outer_relation_indexes;
                    if (choose_ordered_distinct)
                    {
                        distinct_descriptor.ordered_output = true;
                        distinct_descriptor.order_complete = true;
                        distinct_descriptor.ordered_prefix_length =
                            distinct_exprs.size();
                        distinct_descriptor.interesting_order_score =
                            static_cast<double>(distinct_exprs.size());
                        for (auto *expr : distinct_exprs)
                        {
                            distinct_descriptor.ordering_keys.push_back(
                                makeOrderingKey(
                                    expressionToString(expr, pool)));
                        }
                    }
                    current_path->setAccessDescriptor(
                        std::move(distinct_descriptor));
                }
                auto distinct_plan = std::make_shared<AggregateNode>(
                    current_plan,
                    distinct_exprs,
                    no_aggregates,
                    nullptr,
                    parser::GroupingType::STANDARD,
                    std::vector<std::vector<parser::v3::Expression *>>{});
                distinct_plan->setCost(distinct_cost);
                current_plan = distinct_plan;
                current_rows = distinct_cost.rows;
            }
            else
            {
                appendRuntimeTrace(considered_paths,
                                   "UPPER_STAGE_STRATEGY",
                                   "query:distinct",
                                   "DISTINCT_EXECUTOR_FALLBACK",
                                   "CHOSEN",
                                   "planner could not derive explicit DISTINCT key expressions from the select list",
                                   current_path ? current_path->startupCost() : 0.0,
                                   current_path ? current_path->totalCost() : 0.0,
                                   current_path ? current_path->rows() : current_rows);
            }
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

        if (!select_stmt->order_by.empty())
        {
            std::string sort_avoidance_reason;
            const bool order_already_satisfied =
                current_path &&
                orderingSatisfiesOrderBy(current_path->accessDescriptor(),
                                         select_stmt->order_by,
                                         pool,
                                         &sort_avoidance_reason);
            const uint64_t top_n_rows =
                has_limit
                    ? static_cast<uint64_t>(std::max<int64_t>(
                          1,
                          limit_count + std::max<int64_t>(offset_count, 0)))
                    : 0;
            if (order_already_satisfied)
            {
                appendRuntimeTrace(considered_paths,
                                   "SORT_AVOIDANCE",
                                   "query:order_by",
                                   "EXISTING_ORDER",
                                   "CHOSEN",
                                   "input path already satisfies ORDER BY",
                                   current_path->startupCost(),
                                   current_path->totalCost(),
                                   current_path->rows());
                if (has_limit)
                {
                    appendRuntimeTrace(considered_paths,
                                       "UPPER_STAGE_STRATEGY",
                                       "query:top_n",
                                       "ORDERED_LIMIT",
                                       "CHOSEN",
                                       "existing order allows LIMIT/OFFSET without an explicit sort",
                                       current_path->startupCost(),
                                       current_path->totalCost(),
                                       current_path->rows());
                }
            }
            else
            {
                CostEstimate sort_cost =
                    has_limit
                        ? active_cost_model.costSort(current_rows,
                                               row_width,
                                               select_stmt->order_by.size(),
                                               top_n_rows,
                                               ctx)
                        : active_cost_model.costSort(current_rows,
                                               row_width,
                                               select_stmt->order_by.size(),
                                               ctx);
                if (spill_policy == PlannerSpillPolicy::DISALLOW &&
                    sort_cost.spill_expected)
                {
                    SET_ERROR_CONTEXT(ctx,
                                      core::Status::CONFIGURATION_LIMIT_EXCEEDED,
                                      "Sort operator exceeds work_mem under spill-disallow policy");
                    return core::Status::CONFIGURATION_LIMIT_EXCEEDED;
                }
                appendRuntimeTrace(rejected_paths,
                                   "SORT_AVOIDANCE",
                                   "query:order_by",
                                   "EXISTING_ORDER",
                                   "REJECTED",
                                   sort_avoidance_reason.empty()
                                       ? "input path does not satisfy ORDER BY"
                                       : sort_avoidance_reason,
                                   current_path ? current_path->startupCost() : 0.0,
                                   current_path ? current_path->totalCost() : 0.0,
                                   current_path ? current_path->rows() : 0);
                if (has_limit)
                {
                    appendRuntimeTrace(rejected_paths,
                                       "UPPER_STAGE_STRATEGY",
                                       "query:top_n",
                                       "ORDERED_LIMIT",
                                       "REJECTED",
                                       sort_avoidance_reason.empty()
                                           ? "input path is not ordered for LIMIT/OFFSET reuse"
                                           : sort_avoidance_reason,
                                       current_path ? current_path->startupCost() : 0.0,
                                       current_path ? current_path->totalCost() : 0.0,
                                       current_path ? current_path->rows() : 0);
                    appendRuntimeTrace(considered_paths,
                                       "UPPER_STAGE_STRATEGY",
                                       "query:top_n",
                                       "TOP_N_SORT",
                                       "CHOSEN",
                                       "bounded top-N sort selected for ORDER BY with LIMIT/OFFSET",
                                       sort_cost.startup_cost,
                                       sort_cost.total_cost,
                                       sort_cost.rows);
                    appendRuntimeTrace(considered_paths,
                                       "SORT_STRATEGY",
                                       "query:order_by",
                                       "TOP_N_SORT",
                                       "CHOSEN",
                                       "top-N sort for ordered LIMIT/OFFSET",
                                       sort_cost.startup_cost,
                                       sort_cost.total_cost,
                                       sort_cost.rows);
                }
                else
                {
                    appendRuntimeTrace(considered_paths,
                                       "SORT_STRATEGY",
                                       "query:order_by",
                                       "FULL_SORT",
                                       "CHOSEN",
                                       "explicit sort required for ORDER BY",
                                       sort_cost.startup_cost,
                                       sort_cost.total_cost,
                                       sort_cost.rows);
                }
                current_path = std::make_shared<SortPath>(current_path,
                                                          select_stmt->order_by,
                                                          row_width,
                                                          sort_cost);
                if (current_path)
                {
                    AccessPathDescriptor sort_descriptor;
                    sort_descriptor.family = "SORT";
                    sort_descriptor.ordered_output = true;
                    sort_descriptor.order_complete = true;
                    sort_descriptor.ordered_prefix_length =
                        select_stmt->order_by.size();
                    sort_descriptor.interesting_order_score =
                        static_cast<double>(select_stmt->order_by.size());
                    for (auto *order_item : select_stmt->order_by)
                    {
                        if (order_item == nullptr || order_item->expr == nullptr)
                        {
                            continue;
                        }
                        sort_descriptor.ordering_keys.push_back(
                            makeOrderingKey(expressionToString(order_item->expr, pool),
                                            !order_item->ascending,
                                            orderItemNullsFirst(order_item)));
                    }
                    current_path->setAccessDescriptor(std::move(sort_descriptor));
                }
                auto sort_plan =
                    std::make_shared<SortNode>(current_plan, select_stmt->order_by);
                sort_plan->setCost(sort_cost);
                current_plan = sort_plan;
                current_rows = sort_cost.rows;
            }
        }

        auto estimateParallelPages = [&](uint64_t rows_estimate) -> uint64_t {
            const uint64_t bytes =
                std::max<uint64_t>(1, rows_estimate) *
                std::max<uint64_t>(1, row_width);
            return std::max<uint64_t>(
                1,
                (bytes + planner_params.planner_page_size_bytes - 1) /
                    planner_params.planner_page_size_bytes);
        };

        auto applyRelationParallelDecision =
            [&](size_t relation_index,
                const executor::ParallelPlanDecision &decision,
                const std::string &stage_name,
                bool enabled,
                const std::string &reason) -> void {
                auto it = std::find_if(runtime_relations.begin(),
                                       runtime_relations.end(),
                                       [&](RuntimePlanRelation &relation) {
                                           return relation.source_relation_index ==
                                                  relation_index;
                                       });
                if (it == runtime_relations.end())
                {
                    return;
                }
                it->parallel_eligible = decision.eligible;
                it->parallel_enabled = enabled;
                it->parallel_workers_planned =
                    enabled ? decision.workers_planned : 0U;
                it->parallel_stage = stage_name;
                it->parallel_rejection_reason = enabled ? std::string() : reason;
            };

        auto applyJoinParallelDecision =
            [&](const executor::ParallelPlanDecision &decision,
                const std::string &stage_name,
                bool enabled,
                const std::string &reason) -> void {
                if (runtime_joins.empty())
                {
                    return;
                }
                auto &join = runtime_joins.back();
                join.parallel_eligible = decision.eligible;
                join.parallel_enabled = enabled;
                join.parallel_workers_planned =
                    enabled ? decision.workers_planned : 0U;
                join.parallel_stage = stage_name;
                join.parallel_rejection_reason = enabled ? std::string() : reason;
            };

        auto stageCandidateName =
            [](executor::ParallelStageKind stage_kind,
               bool ordered_required) -> std::string {
                if (ordered_required)
                {
                    return "GATHER_MERGE";
                }
                switch (stage_kind)
                {
                    case executor::ParallelStageKind::SCAN:
                        return "PARALLEL_SEQ_SCAN";
                    case executor::ParallelStageKind::HASH_JOIN:
                        return "PARALLEL_HASH_JOIN";
                    case executor::ParallelStageKind::AGGREGATE:
                        return "PARALLEL_AGGREGATE";
                    case executor::ParallelStageKind::GATHER_MERGE:
                        return "GATHER_MERGE";
                }
                return "PARALLEL_STAGE";
            };

        std::shared_ptr<PlanNode> parallel_subject_plan = current_plan;
        executor::ParallelStageKind parallel_stage_kind =
            executor::ParallelStageKind::GATHER_MERGE;
        std::string parallel_stage_name = "UNSUPPORTED";
        bool parallel_ordered_required = false;
        uint64_t parallel_order_key_count = 1;
        bool parallel_candidate_supported = false;
        size_t parallel_scan_relation_index = std::numeric_limits<size_t>::max();
        const bool current_path_already_parallel =
            current_path != nullptr &&
            current_path->accessDescriptor().parallel_enabled;

        if (current_plan)
        {
            auto classifyParallelStage =
                [&](const std::shared_ptr<PlanNode> &candidate) -> bool {
                    if (!candidate)
                    {
                        return false;
                    }
                    switch (candidate->type())
                    {
                        case PlanNodeType::SEQ_SCAN:
                            parallel_stage_kind = executor::ParallelStageKind::SCAN;
                            parallel_stage_name = "SCAN";
                            parallel_candidate_supported = true;
                            if (!runtime_relations.empty())
                            {
                                parallel_scan_relation_index =
                                    runtime_relations.front().source_relation_index;
                            }
                            return true;
                        case PlanNodeType::HASH_JOIN:
                            parallel_stage_kind =
                                executor::ParallelStageKind::HASH_JOIN;
                            parallel_stage_name = "HASH_JOIN";
                            parallel_candidate_supported = true;
                            return true;
                        case PlanNodeType::AGGREGATE:
                            parallel_stage_kind =
                                executor::ParallelStageKind::AGGREGATE;
                            parallel_stage_name = "AGGREGATE";
                            parallel_candidate_supported = true;
                            return true;
                        default:
                            return false;
                    }
                };

            if (current_plan->type() == PlanNodeType::SORT)
            {
                parallel_ordered_required = true;
                parallel_order_key_count = std::max<uint64_t>(
                    1,
                    select_stmt->order_by.empty()
                        ? 1
                        : select_stmt->order_by.size());
                auto sort_plan = std::dynamic_pointer_cast<SortNode>(current_plan);
                if (sort_plan)
                {
                    parallel_subject_plan = sort_plan->childPlan();
                }
                if (!classifyParallelStage(parallel_subject_plan))
                {
                    parallel_stage_kind =
                        executor::ParallelStageKind::GATHER_MERGE;
                    parallel_stage_name = "GATHER_MERGE";
                    parallel_candidate_supported = true;
                }
            }
            else
            {
                classifyParallelStage(current_plan);
            }
        }

        const bool skip_late_parallel_scan_wrapper =
            parallel_scan_modeled_in_search &&
            parallel_stage_kind == executor::ParallelStageKind::SCAN;

        if (current_plan &&
            parallel_candidate_supported &&
            !current_path_already_parallel &&
            !skip_late_parallel_scan_wrapper)
        {
            const CostEstimate serial_cost = planCostEstimate(current_plan);
            uint64_t parallel_decision_rows = current_rows;
            if (parallel_subject_plan)
            {
                parallel_decision_rows = std::max<uint64_t>(
                    parallel_decision_rows,
                    planCostEstimate(parallel_subject_plan).rows);

                if (auto aggregate =
                        std::dynamic_pointer_cast<AggregateNode>(
                            parallel_subject_plan))
                {
                    parallel_decision_rows = std::max<uint64_t>(
                        parallel_decision_rows,
                        planCostEstimate(aggregate->childPlan()).rows);
                }
            }
            if (parallel_scan_relation_index !=
                std::numeric_limits<size_t>::max())
            {
                const auto relation_it =
                    std::find_if(runtime_relations.begin(),
                                 runtime_relations.end(),
                                 [&](const RuntimePlanRelation &relation) {
                                     return relation.source_relation_index ==
                                            parallel_scan_relation_index;
                                 });
                if (relation_it != runtime_relations.end())
                {
                    parallel_decision_rows = std::max<uint64_t>(
                        parallel_decision_rows,
                        std::max(relation_it->estimated_rows,
                                 relation_it->base_rows));
                }
            }
            else if (runtime_relations.size() == 1)
            {
                parallel_decision_rows = std::max<uint64_t>(
                    parallel_decision_rows,
                    std::max(runtime_relations.front().estimated_rows,
                             runtime_relations.front().base_rows));
            }
            const uint64_t parallel_pages =
                estimateParallelPages(std::max<uint64_t>(1, parallel_decision_rows));
            const auto decision = executor::evaluateParallelPlan(
                parallel_config,
                parallel_ordered_required
                    ? executor::ParallelStageKind::GATHER_MERGE
                    : parallel_stage_kind,
                parallel_decision_rows,
                parallel_pages,
                parallel_ordered_required,
                serial_cost.spill_expected,
                true);

            const std::string candidate_name =
                stageCandidateName(parallel_stage_kind, parallel_ordered_required);
            const std::string parallel_subject = "query:parallel";

            if (parallel_subject_plan)
            {
                parallel_subject_plan->setParallelMetadata(
                    true,
                    false,
                    0,
                    false,
                    parallel_stage_name,
                    decision.eligible
                        ? "parallel wrapper pending cost comparison"
                        : decision.rejection_reason);
            }

            if (!decision.eligible)
            {
                appendRuntimeTrace(rejected_paths,
                                   "PARALLEL",
                                   parallel_subject,
                                   candidate_name,
                                   "REJECTED",
                                   decision.rejection_reason,
                                   serial_cost.startup_cost,
                                   serial_cost.total_cost,
                                   current_rows);
                if (parallel_stage_kind == executor::ParallelStageKind::SCAN &&
                    parallel_scan_relation_index !=
                        std::numeric_limits<size_t>::max())
                {
                    applyRelationParallelDecision(parallel_scan_relation_index,
                                                  decision,
                                                  parallel_stage_name,
                                                  false,
                                                  decision.rejection_reason);
                }
                else if (parallel_stage_kind ==
                         executor::ParallelStageKind::HASH_JOIN)
                {
                    applyJoinParallelDecision(decision,
                                              parallel_stage_name,
                                              false,
                                              decision.rejection_reason);
                }
            }
            else
            {
                CostEstimate gather_cost;
                if (parallel_ordered_required)
                {
                    CostEstimate parallel_merge_input = serial_cost;
                    if (parallel_subject_plan)
                    {
                        const CostEstimate subject_cost =
                            planCostEstimate(parallel_subject_plan);
                        const uint64_t participants =
                            std::max<uint64_t>(
                                1,
                                static_cast<uint64_t>(decision.workers_planned) +
                                    (planner_controls.parallel_leader_participation
                                         ? 1ULL
                                         : 0ULL));
                        const uint64_t parallel_input_rows =
                            std::max<uint64_t>(current_rows, subject_cost.rows);
                        const uint64_t worker_rows =
                            std::max<uint64_t>(
                                1,
                                (parallel_input_rows + participants - 1) /
                                    participants);
                        const CostEstimate local_sort_cost =
                            active_cost_model.costSort(worker_rows,
                                                       row_width,
                                                       parallel_order_key_count,
                                                       ctx);

                        parallel_merge_input.startup_cost =
                            subject_cost.startup_cost +
                            (local_sort_cost.startup_cost *
                             static_cast<double>(participants));
                        parallel_merge_input.run_cost =
                            subject_cost.run_cost +
                            (local_sort_cost.run_cost *
                             static_cast<double>(participants));
                        parallel_merge_input.total_cost =
                            parallel_merge_input.startup_cost +
                            parallel_merge_input.run_cost;
                        parallel_merge_input.rows = parallel_input_rows;
                        parallel_merge_input.spill_expected =
                            subject_cost.spill_expected ||
                            local_sort_cost.spill_expected;
                        parallel_merge_input.spill_passes =
                            std::max(subject_cost.spill_passes,
                                     local_sort_cost.spill_passes);
                        parallel_merge_input.spill_bytes =
                            subject_cost.spill_bytes +
                            (local_sort_cost.spill_bytes * participants);
                    }

                    gather_cost = active_cost_model.costGatherMerge(
                        parallel_merge_input,
                        current_rows,
                        parallel_order_key_count,
                        decision.workers_planned,
                        planner_controls.parallel_leader_participation,
                        ctx);
                }
                else
                {
                    gather_cost = active_cost_model.costGather(
                        serial_cost,
                        current_rows,
                        decision.workers_planned,
                        planner_controls.parallel_leader_participation,
                        ctx);
                }
                const double skew_penalty_cost =
                    decision.skew_penalty *
                    std::max(1.0, gather_cost.total_cost * 0.05);
                if (skew_penalty_cost > 0.0)
                {
                    gather_cost.run_cost += skew_penalty_cost;
                    gather_cost.total_cost =
                        gather_cost.startup_cost + gather_cost.run_cost;
                    gather_cost.expanded_terms.push_back(CostFormulaTerm{
                        "run.parallel_skew_penalty",
                        1.0,
                        skew_penalty_cost,
                        skew_penalty_cost,
                        "cost"});
                }

                if (gather_cost.total_cost <= serial_cost.total_cost)
                {
                    appendRuntimeTrace(considered_paths,
                                       "PARALLEL",
                                       parallel_subject,
                                       candidate_name,
                                       "CHOSEN",
                                       "workers=" +
                                           std::to_string(
                                               decision.workers_planned) +
                                           " wrapper=" +
                                           std::string(parallel_ordered_required
                                                           ? "GATHER_MERGE"
                                                           : "GATHER"),
                                       gather_cost.startup_cost,
                                       gather_cost.total_cost,
                                       gather_cost.rows);

                    auto gather_path = std::make_shared<GatherPath>(
                        current_path,
                        decision.workers_planned,
                        planner_controls.parallel_leader_participation,
                        parallel_ordered_required,
                        gather_cost);
                    if (gather_path)
                    {
                        AccessPathDescriptor gather_descriptor =
                            current_path ? current_path->accessDescriptor()
                                         : AccessPathDescriptor{};
                        gather_descriptor.family =
                            parallel_ordered_required ? "GATHER_MERGE"
                                                      : "GATHER";
                        if (!parallel_ordered_required)
                        {
                            gather_descriptor.ordered_output = false;
                            gather_descriptor.order_complete = false;
                            gather_descriptor.ordered_prefix_length = 0;
                            gather_descriptor.ordering_keys.clear();
                            gather_descriptor.interesting_order_score = 0.0;
                        }
                        gather_descriptor.parallel_aware = true;
                        gather_descriptor.parallel_enabled = true;
                        gather_descriptor.parallel_workers_planned =
                            decision.workers_planned;
                        gather_descriptor.gather_merge =
                            parallel_ordered_required;
                        gather_descriptor.parallel_stage =
                            parallel_ordered_required ? "GATHER_MERGE"
                                                     : parallel_stage_name;
                        gather_descriptor.path_name =
                            stageCandidateName(parallel_stage_kind,
                                               parallel_ordered_required);
                        gather_path->setAccessDescriptor(
                            std::move(gather_descriptor));
                    }
                    current_path = gather_path;

                    auto gather_plan = std::make_shared<GatherNode>(
                        current_plan,
                        decision.workers_planned,
                        planner_controls.parallel_leader_participation,
                        parallel_ordered_required);
                    gather_plan->setCost(gather_cost);
                    gather_plan->setParallelMetadata(
                        true,
                        true,
                        decision.workers_planned,
                        parallel_ordered_required,
                        parallel_ordered_required ? "GATHER_MERGE" : "GATHER",
                        "parallel wrapper selected");
                    if (parallel_subject_plan)
                    {
                        parallel_subject_plan->setParallelMetadata(
                            true,
                            true,
                            decision.workers_planned,
                            false,
                            parallel_stage_name,
                            "parallel stage selected");
                    }
                    if (parallel_stage_kind ==
                            executor::ParallelStageKind::SCAN &&
                        parallel_scan_relation_index !=
                            std::numeric_limits<size_t>::max())
                    {
                        applyRelationParallelDecision(parallel_scan_relation_index,
                                                      decision,
                                                      parallel_stage_name,
                                                      true,
                                                      std::string());
                    }
                    else if (parallel_stage_kind ==
                             executor::ParallelStageKind::HASH_JOIN)
                    {
                        applyJoinParallelDecision(decision,
                                                  parallel_stage_name,
                                                  true,
                                                  std::string());
                    }
                    current_plan = gather_plan;
                    current_rows = gather_cost.rows;
                }
                else
                {
                    const std::string rejection_reason =
                        "parallel wrapper cost exceeds serial path";
                    appendRuntimeTrace(rejected_paths,
                                       "PARALLEL",
                                       parallel_subject,
                                       candidate_name,
                                       "REJECTED",
                                       rejection_reason,
                                       gather_cost.startup_cost,
                                       gather_cost.total_cost,
                                       gather_cost.rows);
                    if (parallel_subject_plan)
                    {
                        parallel_subject_plan->setParallelMetadata(
                            true,
                            false,
                            decision.workers_planned,
                            false,
                            parallel_stage_name,
                            rejection_reason);
                    }
                    if (parallel_stage_kind ==
                            executor::ParallelStageKind::SCAN &&
                        parallel_scan_relation_index !=
                            std::numeric_limits<size_t>::max())
                    {
                        applyRelationParallelDecision(parallel_scan_relation_index,
                                                      decision,
                                                      parallel_stage_name,
                                                      false,
                                                      rejection_reason);
                    }
                    else if (parallel_stage_kind ==
                             executor::ParallelStageKind::HASH_JOIN)
                    {
                        applyJoinParallelDecision(decision,
                                                  parallel_stage_name,
                                                  false,
                                                  rejection_reason);
                    }
                }
            }
        }
        else if (current_plan &&
                 planner_controls.enable_parallel &&
                 !current_path_already_parallel &&
                 !skip_late_parallel_scan_wrapper)
        {
            appendRuntimeTrace(rejected_paths,
                               "PARALLEL",
                               "query:parallel",
                               "PARALLEL_STAGE",
                               "REJECTED",
                               "current root stage is not in the guarded parallel family set",
                               current_plan->startupCost(),
                               current_plan->totalCost(),
                               current_rows);
        }

        if (has_limit || has_offset)
        {
            const AccessPathDescriptor input_descriptor =
                current_path ? current_path->accessDescriptor()
                             : AccessPathDescriptor{};
            CostEstimate limit_cost = active_cost_model.costLimit(current_rows,
                                                            has_limit ? limit_count : -1,
                                                            has_offset ? offset_count : -1,
                                                            ctx);
            current_path = std::make_shared<LimitPath>(current_path,
                                                       has_limit ? limit_count : -1,
                                                       has_offset ? offset_count : -1,
                                                       limit_cost);
            if (current_path)
            {
                AccessPathDescriptor limit_descriptor = input_descriptor;
                limit_descriptor.family = "LIMIT";
                current_path->setAccessDescriptor(limit_descriptor);
            }
            auto limit_plan = std::make_shared<LimitNode>(current_plan,
                                                          has_limit ? limit_count : -1,
                                                          has_offset ? offset_count : -1);
            limit_plan->setCost(limit_cost);
            current_plan = limit_plan;
            current_rows = limit_cost.rows;
        }

        planned_out.root_plan = current_plan;
        search_summary.rejected_candidate_count = rejected_paths.size();
        planned_out.runtime_plan.version = kRuntimePlanPayloadVersion;
        planned_out.runtime_plan.contract_id = kRuntimePlanContractId;
        planned_out.runtime_plan.join_graph_contract_id = kJoinGraphContractId;
        planned_out.runtime_plan.diagnostics_contract_id =
            kOptimizerDiagnosticsContractId;
        planned_out.runtime_plan.search_summary = std::move(search_summary);
        planned_out.runtime_plan.relations = std::move(runtime_relations);
        planned_out.runtime_plan.join_steps = std::move(runtime_joins);
        planned_out.runtime_plan.considered_paths = std::move(considered_paths);
        planned_out.runtime_plan.rejected_paths = std::move(rejected_paths);
        planned_out.runtime_plan.statistics_provenance =
            std::move(statistics_provenance);
        planned_out.runtime_plan.optimizer_controls =
            planner_controls.runtime_controls;
        planned_out.runtime_plan.root = toRuntimePlanNode(current_plan);
        if (!select_stmt->order_by.empty() && has_limit)
        {
            annotateTopNSort(planned_out.runtime_plan.root,
                             limit_count,
                             has_offset ? offset_count : 0);
        }
        annotateRuntimePlanResources(planned_out.runtime_plan.root,
                                     current_plan,
                                     active_cost_model,
                                     row_width,
                                     spill_policy_text);
        planned_out.runtime_plan.explain_text =
            current_plan ? current_plan->toString() : std::string("Result");

        std::ostringstream hash_seed;
        hash_seed << planned_out.runtime_plan.contract_id << '|'
                  << planned_out.runtime_plan.join_graph_contract_id << '|'
                  << planned_out.runtime_plan.diagnostics_contract_id << '|'
                  << planned_out.runtime_plan.search_summary.requested_strategy << '|'
                  << planned_out.runtime_plan.search_summary.selected_strategy << '|'
                  << planned_out.runtime_plan.search_summary.search_budget << '|'
                  << planned_out.runtime_plan.search_summary.considered_state_count << '|'
                  << planned_out.runtime_plan.search_summary.pruned_state_count << '|'
                  << planned_out.runtime_plan.search_summary.pair_evaluation_count << '|'
                  << planned_out.runtime_plan.search_summary.rejected_candidate_count << '|'
                  << planned_out.runtime_plan.search_summary.max_pair_evaluations << '|'
                  << planned_out.runtime_plan.search_summary.max_states_considered << '|'
                  << planned_out.runtime_plan.search_summary.exhaustive_join_limit << '|'
                  << planned_out.runtime_plan.search_summary.bounded_dp_join_limit << '|'
                  << planned_out.runtime_plan.search_summary.fallback_prune_level << '|'
                  << planned_out.runtime_plan.search_summary.fallback_reason << '|'
                  << planned_out.runtime_plan.search_summary.fallback_threshold_name
                  << '|'
                  << planned_out.runtime_plan.search_summary.fallback_threshold_value
                  << '|'
                  << planned_out.runtime_plan.explain_text << '|'
                  << planner_params.work_mem_bytes << '|'
                  << spill_policy_text << '|';
        for (const auto &control : planned_out.runtime_plan.optimizer_controls)
        {
            hash_seed << control.name << '=' << control.value << '@'
                      << control.source << '|';
        }
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
                      << join.join_edge_left_relation_index << ':'
                      << join.join_edge_right_relation_index << ':'
                      << join.join_edge_left_id_text << ':'
                      << join.join_edge_right_id_text << ':'
                      << (join.disconnected_component ? 1 : 0) << ':'
                      << join.legality_class << ':'
                      << (join.reorderable ? 1 : 0) << ':'
                      << (join.outer_reorder_barrier ? 1 : 0) << ':'
                      << (join.using_reorder_barrier ? 1 : 0) << ':'
                      << (join.natural_reorder_barrier ? 1 : 0) << ':'
                      << (join.lateral_reorder_barrier ? 1 : 0) << ':'
                      << (join.parameterized_dependency ? 1 : 0) << ':'
                      << join.method << ':'
                      << (join.has_merge_keys ? 1 : 0) << ':'
                      << (join.merge_outer_presorted ? 1 : 0) << ':'
                      << (join.merge_inner_presorted ? 1 : 0) << ':'
                      << join.memory_budget_bytes << ':'
                      << join.estimated_memory_bytes << ':'
                      << (join.spill_expected ? 1 : 0) << ':'
                      << join.spill_passes << ':'
                      << join.total_cost << ':';
            for (const auto &key_pair : join.equijoin_keys)
            {
                hash_seed << key_pair.left_qualifier << '.'
                          << key_pair.left_column_name << '='
                          << key_pair.right_qualifier << '.'
                          << key_pair.right_column_name << ';';
            }
            hash_seed << ':';
            for (const auto &predicate : join.residual_predicates)
            {
                hash_seed << predicate << ';';
            }
            hash_seed << ':';
            for (const auto &family : join.legal_method_families)
            {
                hash_seed << family << ';';
            }
            hash_seed << ':';
            for (const auto &enabler : join.method_enablers)
            {
                hash_seed << enabler << ';';
            }
            hash_seed << '|';
        }
        planned_out.runtime_plan.plan_hash = stablePlanHash(hash_seed.str());
        return core::Status::OK;
    }

} // namespace scratchbird::optimizer
