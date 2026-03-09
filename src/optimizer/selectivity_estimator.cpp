/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/optimizer/selectivity_estimator.h"
#include "scratchbird/parser/ast_v3.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/debug.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>

namespace scratchbird::optimizer
{
    namespace
    {
        struct ResolvedColumnRef
        {
            core::ID column_id{};
            std::string column_name;
        };

        auto isZeroId(const core::ID &id) -> bool
        {
            return id == core::ID{};
        }

        auto unwrapCasts(const parser::v3::Expression *expr) -> const parser::v3::Expression *
        {
            const auto *current = expr;
            while (current != nullptr &&
                   current->kind() == parser::v3::ASTKind::CastExpr)
            {
                current = static_cast<const parser::v3::CastExpr *>(current)->expr;
            }
            return current;
        }

        auto encodeScalarValueToBytes(const void *value,
                                      size_t value_size,
                                      std::vector<uint8_t> &out) -> void
        {
            out.resize(value_size);
            std::memcpy(out.data(), value, value_size);
        }

        auto literalExprToBytes(const parser::v3::Expression *expr,
                                const parser::v3::StringPool *pool,
                                std::vector<uint8_t> &value_out) -> bool
        {
            const auto *current = unwrapCasts(expr);
            if (current == nullptr)
            {
                return false;
            }

            switch (current->kind())
            {
                case parser::v3::ASTKind::LiteralExpr: {
                    const auto *literal = static_cast<const parser::v3::LiteralExpr *>(current);
                    switch (literal->literal_type)
                    {
                        case parser::v3::LiteralType::INTEGER:
                            encodeScalarValueToBytes(&literal->int_value,
                                                     sizeof(literal->int_value),
                                                     value_out);
                            return true;
                        case parser::v3::LiteralType::FLOAT:
                            encodeScalarValueToBytes(&literal->float_value,
                                                     sizeof(literal->float_value),
                                                     value_out);
                            return true;
                        case parser::v3::LiteralType::BOOLEAN:
                            value_out = {static_cast<uint8_t>(literal->bool_value ? 1 : 0)};
                            return true;
                        case parser::v3::LiteralType::STRING:
                            if (pool == nullptr ||
                                literal->string_value == parser::v3::StringPool::INVALID_ID)
                            {
                                return false;
                            }
                            {
                                const std::string text(pool->get(literal->string_value));
                                value_out.assign(text.begin(), text.end());
                            }
                            return true;
                        case parser::v3::LiteralType::NULL_VALUE:
                            value_out.clear();
                            return true;
                        default:
                            return false;
                    }
                }
                case parser::v3::ASTKind::LiteralInt8Expr: {
                    const auto value =
                        static_cast<const parser::v3::LiteralInt8Expr *>(current)->value;
                    encodeScalarValueToBytes(&value, sizeof(value), value_out);
                    return true;
                }
                case parser::v3::ASTKind::LiteralInt16Expr: {
                    const auto value =
                        static_cast<const parser::v3::LiteralInt16Expr *>(current)->value;
                    encodeScalarValueToBytes(&value, sizeof(value), value_out);
                    return true;
                }
                case parser::v3::ASTKind::LiteralUInt8Expr: {
                    const auto value =
                        static_cast<const parser::v3::LiteralUInt8Expr *>(current)->value;
                    encodeScalarValueToBytes(&value, sizeof(value), value_out);
                    return true;
                }
                case parser::v3::ASTKind::LiteralUInt16Expr: {
                    const auto value =
                        static_cast<const parser::v3::LiteralUInt16Expr *>(current)->value;
                    encodeScalarValueToBytes(&value, sizeof(value), value_out);
                    return true;
                }
                case parser::v3::ASTKind::LiteralUInt32Expr: {
                    const auto value =
                        static_cast<const parser::v3::LiteralUInt32Expr *>(current)->value;
                    encodeScalarValueToBytes(&value, sizeof(value), value_out);
                    return true;
                }
                case parser::v3::ASTKind::LiteralUInt64Expr: {
                    const auto value =
                        static_cast<const parser::v3::LiteralUInt64Expr *>(current)->value;
                    encodeScalarValueToBytes(&value, sizeof(value), value_out);
                    return true;
                }
                case parser::v3::ASTKind::LiteralFloat32Expr: {
                    const auto value =
                        static_cast<const parser::v3::LiteralFloat32Expr *>(current)->value;
                    encodeScalarValueToBytes(&value, sizeof(value), value_out);
                    return true;
                }
                default:
                    return false;
            }
        }

        auto literalExprToString(const parser::v3::Expression *expr,
                                 const parser::v3::StringPool *pool,
                                 std::string &value_out) -> bool
        {
            const auto *current = unwrapCasts(expr);
            if (current == nullptr)
            {
                return false;
            }
            if (current->kind() != parser::v3::ASTKind::LiteralExpr || pool == nullptr)
            {
                return false;
            }
            const auto *literal = static_cast<const parser::v3::LiteralExpr *>(current);
            if (literal->literal_type != parser::v3::LiteralType::STRING ||
                literal->string_value == parser::v3::StringPool::INVALID_ID)
            {
                return false;
            }
            value_out = pool->get(literal->string_value);
            return true;
        }

        auto expressionStatsKey(const parser::v3::Expression *expr,
                                const parser::v3::StringPool *pool) -> std::optional<std::string>
        {
            const auto *current = unwrapCasts(expr);
            if (current == nullptr || pool == nullptr ||
                current->kind() != parser::v3::ASTKind::FunctionCallExpr)
            {
                return std::nullopt;
            }

            const auto *func = static_cast<const parser::v3::FunctionCallExpr *>(current);
            if (func->arguments.size() != 1 || func->function_path.components.empty())
            {
                return std::nullopt;
            }

            const auto *arg = unwrapCasts(func->arguments.front());
            if (arg == nullptr || arg->kind() != parser::v3::ASTKind::ColumnRefExpr)
            {
                return std::nullopt;
            }

            const auto *column_ref = static_cast<const parser::v3::ColumnRefExpr *>(arg);
            if (column_ref->column.column_name == parser::v3::StringPool::INVALID_ID)
            {
                return std::nullopt;
            }

            const std::string func_name =
                core::IdentifierUtils::toUpper(
                    std::string(pool->get(func->function_path.components.back())));
            if (func_name != "LOWER" && func_name != "UPPER")
            {
                return std::nullopt;
            }

            const std::string column_name =
                core::IdentifierUtils::toUpper(
                    std::string(pool->get(column_ref->column.column_name)));
            return func_name + "(" + column_name + ")";
        }

        auto resolveColumnRef(core::Database *db,
                              const core::ID &table_id,
                              const parser::v3::Expression *expr,
                              const parser::v3::StringPool *pool,
                              core::ErrorContext *ctx) -> std::optional<ResolvedColumnRef>
        {
            if (db == nullptr || db->catalog_manager() == nullptr ||
                pool == nullptr || isZeroId(table_id))
            {
                return std::nullopt;
            }

            const auto *current = unwrapCasts(expr);
            if (current == nullptr ||
                current->kind() != parser::v3::ASTKind::ColumnRefExpr)
            {
                return std::nullopt;
            }

            const auto *column_ref = static_cast<const parser::v3::ColumnRefExpr *>(current);
            if (column_ref->column.column_name == parser::v3::StringPool::INVALID_ID)
            {
                return std::nullopt;
            }

            const std::string wanted(pool->get(column_ref->column.column_name));
            std::vector<core::CatalogManager::ColumnInfo> columns;
            if (db->catalog_manager()->getColumns(table_id, columns, ctx) != core::Status::OK)
            {
                return std::nullopt;
            }

            for (const auto &column : columns)
            {
                if (core::IdentifierUtils::namesMatch(column.column_name,
                                                      false,
                                                      wanted,
                                                      false))
                {
                    return ResolvedColumnRef{column.column_id, column.column_name};
                }
            }

            return std::nullopt;
        }

        auto resolveColumnPairForJoin(core::Database *db,
                                      const core::ID &left_table_id,
                                      const core::ID &right_table_id,
                                      const parser::v3::Expression *left_expr,
                                      const parser::v3::Expression *right_expr,
                                      const parser::v3::StringPool *pool,
                                      core::ID &left_column_id_out,
                                      core::ID &right_column_id_out,
                                      core::ErrorContext *ctx) -> bool
        {
            auto left_column = resolveColumnRef(db, left_table_id, left_expr, pool, ctx);
            auto right_column = resolveColumnRef(db, right_table_id, right_expr, pool, ctx);
            if (left_column.has_value() && right_column.has_value())
            {
                left_column_id_out = left_column->column_id;
                right_column_id_out = right_column->column_id;
                return true;
            }

            left_column = resolveColumnRef(db, left_table_id, right_expr, pool, ctx);
            right_column = resolveColumnRef(db, right_table_id, left_expr, pool, ctx);
            if (left_column.has_value() && right_column.has_value())
            {
                left_column_id_out = left_column->column_id;
                right_column_id_out = right_column->column_id;
                return true;
            }

            return false;
        }

        auto canonicalizeLiteralForExpression(std::string literal, const std::string &expression_key)
            -> std::string
        {
            if (expression_key.rfind("LOWER(", 0) == 0)
            {
                std::transform(literal.begin(),
                               literal.end(),
                               literal.begin(),
                               [](unsigned char ch) {
                                   return static_cast<char>(std::tolower(ch));
                               });
                return literal;
            }
            if (expression_key.rfind("UPPER(", 0) == 0)
            {
                return core::IdentifierUtils::toUpper(literal);
            }
            return literal;
        }
    } // namespace

    auto SelectivityEstimator::estimateWhereClause(
        const parser::v3::Expression *where_clause,
        const core::ID &table_id,
        const parser::v3::StringPool *pool,
        core::ErrorContext *ctx)
        -> double
    {
        if (!where_clause)
        {
            // No WHERE clause → all rows match
            return 1.0;
        }

        const auto *expr = unwrapCasts(where_clause);
        if (expr == nullptr)
        {
            return DEFAULT_RANGE_SEL;
        }

        if (expr->kind() == parser::v3::ASTKind::BinaryExpr)
        {
            const auto *binary = static_cast<const parser::v3::BinaryExpr *>(expr);
            if (binary->op == parser::v3::BinaryOp::AND)
            {
                double left_sel = estimateWhereClause(binary->left, table_id, pool, ctx);
                double right_sel = estimateWhereClause(binary->right, table_id, pool, ctx);
                double combined = estimateAnd(left_sel, right_sel);

                if (stats_manager_ != nullptr)
                {
                    auto left_column = resolveColumnRef(db_, table_id, binary->left, pool, ctx);
                    auto right_column = resolveColumnRef(db_, table_id, binary->right, pool, ctx);
                    if (left_column.has_value() && right_column.has_value())
                    {
                        ColumnCorrelationStatistics corr;
                        if (stats_manager_->getColumnCorrelation(table_id,
                                                                 left_column->column_id,
                                                                 right_column->column_id,
                                                                 corr,
                                                                 nullptr) == core::Status::OK)
                        {
                            const double magnitude = std::min(1.0, std::abs(corr.coefficient));
                            if (corr.coefficient >= 0.0)
                            {
                                combined += magnitude *
                                            (std::min(left_sel, right_sel) - combined);
                            }
                            else
                            {
                                combined *= std::max(0.1, 1.0 - magnitude * 0.5);
                            }
                            combined = std::max(0.0, std::min(1.0, combined));
                        }
                    }
                }
                return combined;
            }
            if (binary->op == parser::v3::BinaryOp::OR)
            {
                return estimateOr(estimateWhereClause(binary->left, table_id, pool, ctx),
                                  estimateWhereClause(binary->right, table_id, pool, ctx));
            }

            auto estimate_binary_predicate =
                [&](const parser::v3::Expression *column_expr,
                    const parser::v3::Expression *literal_expr,
                    bool reversed_range) -> std::optional<double> {
                    auto column = resolveColumnRef(db_, table_id, column_expr, pool, ctx);
                    if (!column.has_value())
                    {
                        auto expr_key = expressionStatsKey(column_expr, pool);
                        if (!expr_key.has_value() || binary->op != parser::v3::BinaryOp::EQ ||
                            stats_manager_ == nullptr)
                        {
                            return std::nullopt;
                        }

                        std::string literal_text;
                        if (!literalExprToString(literal_expr, pool, literal_text))
                        {
                            return std::nullopt;
                        }

                        ExpressionStatistics expr_stats;
                        if (stats_manager_->getExpressionStatistics(table_id,
                                                                    *expr_key,
                                                                    expr_stats,
                                                                    ctx) != core::Status::OK)
                        {
                            return std::nullopt;
                        }

                        const std::string normalized =
                            canonicalizeLiteralForExpression(literal_text, *expr_key);
                        std::vector<uint8_t> expr_value(sizeof(uint32_t) + normalized.size());
                        uint32_t len = static_cast<uint32_t>(normalized.size());
                        std::memcpy(expr_value.data(), &len, sizeof(len));
                        if (!normalized.empty())
                        {
                            std::memcpy(expr_value.data() + sizeof(uint32_t),
                                        normalized.data(),
                                        normalized.size());
                        }

                        const auto &stats = expr_stats.stats;
                        if (expr_value.empty())
                        {
                            return static_cast<double>(stats.null_fraction);
                        }
                        for (const auto &mcv : stats.mcv_list)
                        {
                            if (valueEquals(mcv.value_data, expr_value))
                            {
                                return static_cast<double>(mcv.frequency);
                            }
                        }
                        if (stats.num_distinct == 0)
                        {
                            return DEFAULT_EQUALITY_SEL;
                        }
                        double mcv_total = 0.0;
                        for (const auto &mcv : stats.mcv_list)
                        {
                            mcv_total += mcv.frequency;
                        }
                        const uint64_t remaining_distinct =
                            stats.num_distinct > stats.mcv_list.size()
                                ? stats.num_distinct - stats.mcv_list.size()
                                : 0;
                        if (remaining_distinct == 0)
                        {
                            return 0.0;
                        }
                        const double remaining =
                            std::max(0.0, 1.0 - mcv_total - stats.null_fraction);
                        return remaining / static_cast<double>(remaining_distinct);
                    }

                    std::vector<uint8_t> literal_bytes;
                    if (!literalExprToBytes(literal_expr, pool, literal_bytes))
                    {
                        return std::nullopt;
                    }

                    switch (binary->op)
                    {
                        case parser::v3::BinaryOp::EQ:
                            return estimateEquality(table_id, column->column_id, literal_bytes, ctx);
                        case parser::v3::BinaryOp::LT:
                            return estimateRange(table_id,
                                                 column->column_id,
                                                 reversed_range ? ">" : "<",
                                                 literal_bytes,
                                                 ctx);
                        case parser::v3::BinaryOp::LE:
                            return estimateRange(table_id,
                                                 column->column_id,
                                                 reversed_range ? ">=" : "<=",
                                                 literal_bytes,
                                                 ctx);
                        case parser::v3::BinaryOp::GT:
                            return estimateRange(table_id,
                                                 column->column_id,
                                                 reversed_range ? "<" : ">",
                                                 literal_bytes,
                                                 ctx);
                        case parser::v3::BinaryOp::GE:
                            return estimateRange(table_id,
                                                 column->column_id,
                                                 reversed_range ? "<=" : ">=",
                                                 literal_bytes,
                                                 ctx);
                        default:
                            return std::nullopt;
                    }
                };

            if (auto sel = estimate_binary_predicate(binary->left, binary->right, false))
            {
                return *sel;
            }
            if (auto sel = estimate_binary_predicate(binary->right, binary->left, true))
            {
                return *sel;
            }
        }

        if (expr->kind() == parser::v3::ASTKind::UnaryExpr)
        {
            const auto *unary = static_cast<const parser::v3::UnaryExpr *>(expr);
            if (unary->op == parser::v3::UnaryOp::NOT)
            {
                return estimateNot(estimateWhereClause(unary->operand, table_id, pool, ctx));
            }
            if (unary->op == parser::v3::UnaryOp::IS_NULL ||
                unary->op == parser::v3::UnaryOp::IS_NOT_NULL)
            {
                auto column = resolveColumnRef(db_, table_id, unary->operand, pool, ctx);
                if (!column.has_value() || stats_manager_ == nullptr)
                {
                    return unary->op == parser::v3::UnaryOp::IS_NOT_NULL
                               ? (1.0 - DEFAULT_EQUALITY_SEL)
                               : DEFAULT_EQUALITY_SEL;
                }

                ColumnStatistics col_stats;
                if (stats_manager_->getColumnStatistics(table_id, column->column_id, col_stats, ctx) !=
                    core::Status::OK)
                {
                    return unary->op == parser::v3::UnaryOp::IS_NOT_NULL
                               ? (1.0 - DEFAULT_EQUALITY_SEL)
                               : DEFAULT_EQUALITY_SEL;
                }
                return unary->op == parser::v3::UnaryOp::IS_NOT_NULL
                           ? (1.0 - col_stats.null_fraction)
                           : col_stats.null_fraction;
            }
        }

        if (expr->kind() == parser::v3::ASTKind::BetweenExpr)
        {
            const auto *between = static_cast<const parser::v3::BetweenExpr *>(expr);
            auto column = resolveColumnRef(db_, table_id, between->expr, pool, ctx);
            std::vector<uint8_t> lower_value;
            std::vector<uint8_t> upper_value;
            if (!column.has_value() ||
                !literalExprToBytes(between->low, pool, lower_value) ||
                !literalExprToBytes(between->high, pool, upper_value))
            {
                return DEFAULT_RANGE_SEL;
            }
            double sel = estimateBetween(table_id, column->column_id, lower_value, upper_value, ctx);
            return between->negated ? estimateNot(sel) : sel;
        }

        if (expr->kind() == parser::v3::ASTKind::LikeExpr)
        {
            const auto *like = static_cast<const parser::v3::LikeExpr *>(expr);
            auto column = resolveColumnRef(db_, table_id, like->expr, pool, ctx);
            std::string pattern;
            if (!column.has_value() ||
                !literalExprToString(like->pattern, pool, pattern))
            {
                return DEFAULT_LIKE_CONTAINS_SEL;
            }

            double sel = estimateLike(table_id, column->column_id, pattern, ctx);
            return like->negated ? estimateNot(sel) : sel;
        }

        if (expr->kind() == parser::v3::ASTKind::InExpr)
        {
            const auto *in_expr = static_cast<const parser::v3::InExpr *>(expr);
            auto column = resolveColumnRef(db_, table_id, in_expr->expr, pool, ctx);
            if (!column.has_value() || in_expr->has_subquery)
            {
                return DEFAULT_EQUALITY_SEL;
            }

            std::vector<std::vector<uint8_t>> values;
            for (const auto *entry : in_expr->values)
            {
                std::vector<uint8_t> bytes;
                if (literalExprToBytes(entry, pool, bytes))
                {
                    values.push_back(std::move(bytes));
                }
            }
            if (values.empty())
            {
                return DEFAULT_EQUALITY_SEL;
            }

            double sel = estimateIn(table_id, column->column_id, values, ctx);
            return in_expr->negated ? estimateNot(sel) : sel;
        }

        DEBUG_LOG_DB("Using default WHERE clause selectivity: " +
                     std::to_string(DEFAULT_RANGE_SEL));
        return DEFAULT_RANGE_SEL;
    }

    auto SelectivityEstimator::estimateEquality(
        const core::ID &table_id,
        const core::ID &column_id,
        const std::vector<uint8_t> &value,
        core::ErrorContext *ctx)
        -> double
    {
        DEBUG_LOG_DB("Estimating equality selectivity for column " +
                     column_id.toString());

        // Get column statistics
        ColumnStatistics col_stats;
        core::Status status = stats_manager_->getColumnStatistics(
            table_id, column_id, col_stats, ctx);

        if (status != core::Status::OK)
        {
            DEBUG_LOG_DB("No statistics available, using default: " +
                         std::to_string(DEFAULT_EQUALITY_SEL));
            return DEFAULT_EQUALITY_SEL;
        }

        // Check if value is NULL
        if (value.empty())
        {
            DEBUG_LOG_DB("NULL value, selectivity = null_fraction: " +
                         std::to_string(col_stats.null_fraction));
            return col_stats.null_fraction;
        }

        // Check MCVs first (most common values have exact frequencies)
        for (const auto &mcv : col_stats.mcv_list)
        {
            if (valueEquals(mcv.value_data, value))
            {
                DEBUG_LOG_DB("Value found in MCV list, frequency: " +
                             std::to_string(mcv.frequency));
                return mcv.frequency;
            }
        }

        // Value not in MCV list
        // Distribute remaining probability uniformly among remaining values
        if (col_stats.num_distinct == 0)
        {
            DEBUG_LOG_DB("num_distinct is 0, using default");
            return DEFAULT_EQUALITY_SEL;
        }

        if (col_stats.num_distinct <= col_stats.mcv_list.size())
        {
            // All distinct values are in MCV list, value not found
            DEBUG_LOG_DB("All distinct values in MCV list, value not found");
            return 0.0;
        }

        // Calculate total MCV frequency
        double mcv_total_freq = 0.0;
        for (const auto &mcv : col_stats.mcv_list)
        {
            mcv_total_freq += mcv.frequency;
        }

        // Remaining probability for non-MCV values
        double remaining_freq = 1.0 - mcv_total_freq - col_stats.null_fraction;
        if (remaining_freq < 0.0)
        {
            remaining_freq = 0.0;
        }

        // Number of non-MCV distinct values
        uint64_t remaining_distinct = col_stats.num_distinct - col_stats.mcv_list.size();
        if (remaining_distinct == 0)
        {
            DEBUG_LOG_DB("No remaining distinct values");
            return 0.0;
        }

        // Uniform distribution among remaining values
        double selectivity = remaining_freq / static_cast<double>(remaining_distinct);

        DEBUG_LOG_DB("Non-MCV value, selectivity: " + std::to_string(selectivity) +
                     " (remaining_freq=" + std::to_string(remaining_freq) +
                     ", remaining_distinct=" + std::to_string(remaining_distinct) + ")");

        return std::max(0.0, std::min(1.0, selectivity));
    }

    auto SelectivityEstimator::estimateRange(
        const core::ID &table_id,
        const core::ID &column_id,
        const std::string &op,
        const std::vector<uint8_t> &value,
        core::ErrorContext *ctx)
        -> double
    {
        DEBUG_LOG_DB("Estimating range selectivity for column " +
                     column_id.toString() + " with operator " + op);

        // Get column statistics
        ColumnStatistics col_stats;
        core::Status status = stats_manager_->getColumnStatistics(
            table_id, column_id, col_stats, ctx);

        if (status != core::Status::OK || col_stats.histogram_buckets.empty())
        {
            DEBUG_LOG_DB("No histogram available, using default: " +
                         std::to_string(DEFAULT_RANGE_SEL));
            return DEFAULT_RANGE_SEL;
        }

        double selectivity = 0.0;

        // Iterate through histogram buckets
        for (const auto &bucket : col_stats.histogram_buckets)
        {
            int cmp_lower = compareValues(value, std::vector<uint8_t>(
                bucket.lower_bound, bucket.lower_bound + 256));
            int cmp_upper = compareValues(value, std::vector<uint8_t>(
                bucket.upper_bound, bucket.upper_bound + 256));

            if (op == ">")
            {
                // Query: col > value
                // Include bucket if any values in bucket are > value

                if (cmp_lower >= 0)
                {
                    // value >= bucket.lower_bound
                    if (cmp_upper >= 0)
                    {
                        // value >= bucket.upper_bound → no values in bucket are > value
                        continue;
                    }
                    else
                    {
                        // bucket.lower_bound <= value < bucket.upper_bound
                        // Interpolate: fraction of bucket that is > value
                        double fraction = interpolateBucket(
                            value, bucket.lower_bound, bucket.upper_bound);
                        selectivity += bucket.frequency * fraction;
                    }
                }
                else
                {
                    // value < bucket.lower_bound → entire bucket is > value
                    selectivity += bucket.frequency;
                }
            }
            else if (op == ">=")
            {
                // Query: col >= value
                if (cmp_lower > 0)
                {
                    // value > bucket.lower_bound
                    if (cmp_upper >= 0)
                    {
                        // value >= bucket.upper_bound → no values in bucket are >= value
                        continue;
                    }
                    else
                    {
                        // bucket.lower_bound < value < bucket.upper_bound
                        // Interpolate: fraction of bucket that is >= value
                        double fraction = interpolateBucket(
                            value, bucket.lower_bound, bucket.upper_bound);
                        selectivity += bucket.frequency * fraction;
                    }
                }
                else
                {
                    // value <= bucket.lower_bound → entire bucket is >= value
                    selectivity += bucket.frequency;
                }
            }
            else if (op == "<")
            {
                // Query: col < value
                if (cmp_upper <= 0)
                {
                    // value <= bucket.upper_bound
                    if (cmp_lower <= 0)
                    {
                        // value <= bucket.lower_bound → no values in bucket are < value
                        continue;
                    }
                    else
                    {
                        // bucket.lower_bound < value <= bucket.upper_bound
                        // Interpolate: fraction of bucket that is < value
                        double fraction = 1.0 - interpolateBucket(
                            value, bucket.lower_bound, bucket.upper_bound);
                        selectivity += bucket.frequency * fraction;
                    }
                }
                else
                {
                    // value > bucket.upper_bound → entire bucket is < value
                    selectivity += bucket.frequency;
                }
            }
            else if (op == "<=")
            {
                // Query: col <= value
                if (cmp_upper < 0)
                {
                    // value < bucket.upper_bound
                    if (cmp_lower <= 0)
                    {
                        // value <= bucket.lower_bound → no values in bucket are <= value
                        continue;
                    }
                    else
                    {
                        // bucket.lower_bound < value < bucket.upper_bound
                        // Interpolate: fraction of bucket that is <= value
                        double fraction = 1.0 - interpolateBucket(
                            value, bucket.lower_bound, bucket.upper_bound);
                        selectivity += bucket.frequency * fraction;
                    }
                }
                else
                {
                    // value >= bucket.upper_bound → entire bucket is <= value
                    selectivity += bucket.frequency;
                }
            }
        }

        selectivity = std::max(0.0, std::min(1.0, selectivity));

        DEBUG_LOG_DB("Range selectivity: " + std::to_string(selectivity));

        return selectivity;
    }

    auto SelectivityEstimator::estimateBetween(
        const core::ID &table_id,
        const core::ID &column_id,
        const std::vector<uint8_t> &lower_value,
        const std::vector<uint8_t> &upper_value,
        core::ErrorContext *ctx)
        -> double
    {
        DEBUG_LOG_DB("Estimating BETWEEN selectivity");

        // BETWEEN a AND b = (col >= a) - (col > b)
        double sel_ge_lower = estimateRange(table_id, column_id, ">=", lower_value, ctx);
        double sel_gt_upper = estimateRange(table_id, column_id, ">", upper_value, ctx);

        double selectivity = sel_ge_lower - sel_gt_upper;
        selectivity = std::max(0.0, std::min(1.0, selectivity));

        DEBUG_LOG_DB("BETWEEN selectivity: " + std::to_string(selectivity) +
                     " (>= lower: " + std::to_string(sel_ge_lower) +
                     ", > upper: " + std::to_string(sel_gt_upper) + ")");

        return selectivity;
    }

    auto SelectivityEstimator::estimateLike(
        const core::ID &table_id,
        const core::ID &column_id,
        const std::string &pattern,
        core::ErrorContext *ctx)
        -> double
    {
        DEBUG_LOG_DB("Estimating LIKE selectivity for pattern: " + pattern);

        // Match all
        if (pattern == "%")
        {
            return 1.0;
        }

        bool starts_with_wildcard = (pattern.empty() ? false : pattern[0] == '%');
        bool ends_with_wildcard = (pattern.empty() ? false : pattern.back() == '%');

        if (!starts_with_wildcard && ends_with_wildcard)
        {
            // Prefix match: 'John%'
            // Could use histogram range estimation for string prefix
            // For now, use heuristic
            DEBUG_LOG_DB("Prefix LIKE, using default: " +
                         std::to_string(DEFAULT_LIKE_PREFIX_SEL));
            return DEFAULT_LIKE_PREFIX_SEL;
        }
        else if (starts_with_wildcard && !ends_with_wildcard)
        {
            // Suffix match: '%Smith'
            // Cannot use index
            DEBUG_LOG_DB("Suffix LIKE, using default: " +
                         std::to_string(DEFAULT_LIKE_SUFFIX_SEL));
            return DEFAULT_LIKE_SUFFIX_SEL;
        }
        else if (starts_with_wildcard && ends_with_wildcard)
        {
            // Contains match: '%John%'
            // Cannot use index
            DEBUG_LOG_DB("Contains LIKE, using default: " +
                         std::to_string(DEFAULT_LIKE_CONTAINS_SEL));
            return DEFAULT_LIKE_CONTAINS_SEL;
        }
        else
        {
            // Exact match: 'John' (no wildcards)
            // Treat as equality
            DEBUG_LOG_DB("Exact LIKE, treating as equality");
            std::vector<uint8_t> value(pattern.begin(), pattern.end());
            return estimateEquality(table_id, column_id, value, ctx);
        }
    }

    auto SelectivityEstimator::estimateIn(
        const core::ID &table_id,
        const core::ID &column_id,
        const std::vector<std::vector<uint8_t>> &values,
        core::ErrorContext *ctx)
        -> double
    {
        DEBUG_LOG_DB("Estimating IN selectivity for " +
                     std::to_string(values.size()) + " values");

        if (values.empty())
        {
            return 0.0;
        }

        double total_sel = 0.0;

        for (const auto &value : values)
        {
            double sel = estimateEquality(table_id, column_id, value, ctx);
            total_sel += sel;
        }

        // Cap at 1.0
        total_sel = std::min(1.0, total_sel);

        DEBUG_LOG_DB("IN selectivity: " + std::to_string(total_sel));

        return total_sel;
    }

    auto SelectivityEstimator::estimateAnd(double sel1, double sel2) const -> double
    {
        // Independence assumption: P(A AND B) = P(A) * P(B)
        double selectivity = sel1 * sel2;

        DEBUG_LOG_DB("AND selectivity: " + std::to_string(sel1) +
                     " * " + std::to_string(sel2) +
                     " = " + std::to_string(selectivity));

        return selectivity;
    }

    auto SelectivityEstimator::estimateOr(double sel1, double sel2) const -> double
    {
        // P(A OR B) = P(A) + P(B) - P(A AND B)
        //           = P(A) + P(B) - P(A) * P(B)
        double selectivity = sel1 + sel2 - (sel1 * sel2);

        DEBUG_LOG_DB("OR selectivity: " + std::to_string(sel1) +
                     " + " + std::to_string(sel2) +
                     " - (" + std::to_string(sel1) + " * " + std::to_string(sel2) + ")" +
                     " = " + std::to_string(selectivity));

        return selectivity;
    }

    auto SelectivityEstimator::estimateNot(double sel) const -> double
    {
        // P(NOT A) = 1 - P(A)
        double selectivity = 1.0 - sel;

        DEBUG_LOG_DB("NOT selectivity: 1.0 - " + std::to_string(sel) +
                     " = " + std::to_string(selectivity));

        return selectivity;
    }

    // Private helper methods

    auto SelectivityEstimator::compareValues(
        const std::vector<uint8_t> &v1,
        const std::vector<uint8_t> &value_two) const
        -> int
    {
        // Lexicographic comparison
        size_t min_len = std::min(v1.size(), value_two.size());

        for (size_t i = 0; i < min_len; i++)
        {
            if (v1[i] < value_two[i])
                return -1;
            if (v1[i] > value_two[i])
                return 1;
        }

        // Prefixes are equal, compare lengths
        if (v1.size() < value_two.size())
            return -1;
        if (v1.size() > value_two.size())
            return 1;

        return 0;
    }

    auto SelectivityEstimator::valueEquals(
        const uint8_t *v1,
        const std::vector<uint8_t> &value_two) const
        -> bool
    {
        // Compare up to value_two size (v1 is assumed to be at least as long)
        // For MCVs, we stored values in fixed 256-byte arrays
        size_t len = value_two.size();
        if (len > 256)
            len = 256;

        return std::memcmp(v1, value_two.data(), len) == 0;
    }

    auto SelectivityEstimator::interpolateBucket(
        const std::vector<uint8_t> &value,
        const uint8_t *bucket_min,
        const uint8_t *bucket_max) const
        -> double
    {
        // Linear interpolation within bucket
        // Returns fraction of bucket that is ABOVE value (for > queries)

        // For simplicity, assume numeric values
        // In a real implementation, this would need type-aware interpolation

        // Convert first 8 bytes to double for interpolation
        // (This is a simplification - real implementation needs type awareness)

        if (value.size() < sizeof(double))
        {
            // Default to middle of bucket
            return 0.5;
        }

        double val, b_min, b_max;
        std::memcpy(&val, value.data(), sizeof(double));
        std::memcpy(&b_min, bucket_min, sizeof(double));
        std::memcpy(&b_max, bucket_max, sizeof(double));

        if (b_max <= b_min)
        {
            // Invalid bucket
            return 0.5;
        }

        // Fraction of bucket above value
        double fraction = (b_max - val) / (b_max - b_min);

        // Clamp to [0, 1]
        return std::max(0.0, std::min(1.0, fraction));
    }

    auto SelectivityEstimator::estimateJoinSelectivity(
        const parser::v3::Expression* join_condition,
        const core::ID& left_table_id,
        const core::ID& right_table_id,
        const parser::v3::StringPool *pool,
        core::ErrorContext* ctx)
        -> double
    {
        if (!join_condition)
        {
            // CROSS JOIN or NATURAL JOIN without condition
            // Return 1.0 for now (Cartesian product)
            DEBUG_LOG_DB("No join condition, using selectivity 1.0");
            return 1.0;
        }

        const auto *expr = unwrapCasts(join_condition);
        if (auto* binary_expr = dynamic_cast<const parser::v3::BinaryExpr*>(expr))
        {
            // Check for equality (equi-join)
            if (binary_expr->op == parser::v3::BinaryOp::EQ)
            {
                core::ID left_column_id{};
                core::ID right_column_id{};
                if (resolveColumnPairForJoin(db_,
                                             left_table_id,
                                             right_table_id,
                                             binary_expr->left,
                                             binary_expr->right,
                                             pool,
                                             left_column_id,
                                             right_column_id,
                                             ctx))
                {
                    return estimateEquiJoinSelectivity(left_column_id,
                                                       right_column_id,
                                                       left_table_id,
                                                       right_table_id,
                                                       ctx);
                }
                DEBUG_LOG_DB("Equi-join detected without resolved column IDs, using default selectivity: " +
                             std::to_string(DEFAULT_EQUALITY_SEL));
                return DEFAULT_EQUALITY_SEL;
            }
            // Range join (>, <, >=, <=)
            else if (binary_expr->op == parser::v3::BinaryOp::GT ||
                    binary_expr->op == parser::v3::BinaryOp::LT ||
                    binary_expr->op == parser::v3::BinaryOp::GE ||
                    binary_expr->op == parser::v3::BinaryOp::LE)
            {
                DEBUG_LOG_DB("Range join detected, using default selectivity: " +
                           std::to_string(DEFAULT_RANGE_SEL));
                return DEFAULT_RANGE_SEL;
            }
            // AND - multiply selectivities (independence assumption)
            else if (binary_expr->op == parser::v3::BinaryOp::AND)
            {
                double left_sel = estimateJoinSelectivity(
                    binary_expr->left, left_table_id, right_table_id, pool, ctx);
                double right_sel = estimateJoinSelectivity(
                    binary_expr->right, left_table_id, right_table_id, pool, ctx);

                double combined = left_sel * right_sel;
                DEBUG_LOG_DB("AND join condition: " + std::to_string(left_sel) +
                           " * " + std::to_string(right_sel) +
                           " = " + std::to_string(combined));
                return combined;
            }
            // OR - add selectivities and subtract overlap
            else if (binary_expr->op == parser::v3::BinaryOp::OR)
            {
                double left_sel = estimateJoinSelectivity(
                    binary_expr->left, left_table_id, right_table_id, pool, ctx);
                double right_sel = estimateJoinSelectivity(
                    binary_expr->right, left_table_id, right_table_id, pool, ctx);

                // sel(A OR B) = sel(A) + sel(B) - sel(A) * sel(B)
                double combined = left_sel + right_sel - (left_sel * right_sel);
                DEBUG_LOG_DB("OR join condition: " + std::to_string(left_sel) +
                           " + " + std::to_string(right_sel) +
                           " - overlap = " + std::to_string(combined));
                return combined;
            }
        }

        // Default for unknown join conditions
        DEBUG_LOG_DB("Unknown join condition type, using default: " +
                   std::to_string(DEFAULT_RANGE_SEL));
        return DEFAULT_RANGE_SEL;
    }

    auto SelectivityEstimator::estimateEquiJoinSelectivity(
        const core::ID& left_col_id,
        const core::ID& right_col_id,
        const core::ID& left_table_id,
        const core::ID& right_table_id,
        core::ErrorContext* ctx)
        -> double
    {
        DEBUG_LOG_DB("Estimating equi-join selectivity");

        // Get statistics for both columns
        ColumnStatistics left_stats, right_stats;

        core::Status left_status = stats_manager_->getColumnStatistics(
            left_table_id, left_col_id, left_stats, ctx);
        core::Status right_status = stats_manager_->getColumnStatistics(
            right_table_id, right_col_id, right_stats, ctx);

        if (left_status != core::Status::OK || right_status != core::Status::OK)
        {
            // No statistics available, use default
            DEBUG_LOG_DB("No statistics for join columns, using default: " +
                       std::to_string(DEFAULT_EQUALITY_SEL));
            return DEFAULT_EQUALITY_SEL;
        }

        // Equi-join selectivity formula:
        // selectivity = 1 / MAX(n_distinct(left_col), n_distinct(right_col))
        //
        // Rationale: Each value in the smaller domain matches approximately
        // 1/n_distinct of the larger domain

        uint64_t max_distinct = std::max(left_stats.num_distinct, right_stats.num_distinct);

        if (max_distinct == 0)
        {
            // Avoid division by zero
            DEBUG_LOG_DB("n_distinct is 0, using default: " +
                       std::to_string(DEFAULT_EQUALITY_SEL));
            return DEFAULT_EQUALITY_SEL;
        }

        double selectivity = 1.0 / static_cast<double>(max_distinct);

        DEBUG_LOG_DB("Equi-join selectivity: 1 / MAX(" +
                   std::to_string(left_stats.n_distinct) + ", " +
                   std::to_string(right_stats.n_distinct) + ") = " +
                   std::to_string(selectivity));

        return selectivity;
    }

} // namespace scratchbird::optimizer
