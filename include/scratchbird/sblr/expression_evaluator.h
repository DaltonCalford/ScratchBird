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

#include "scratchbird/core/expression.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/tid.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/**
 * Expression Evaluator for Task 17: Expression and Filtered Indexes
 *
 * Evaluates expression AST trees against table rows to compute index keys
 * and evaluate predicates for filtered indexes.
 *
 * Features:
 * - Evaluate any expression type against a row
 * - Type coercion and null handling
 * - Predicate evaluation (returns bool)
 * - Column position mapping for fast lookups
 * - Transaction-aware evaluation (Task 17 MGA Phase 1.4)
 * - Visibility checking for tuple versions
 */

namespace scratchbird::sblr
{
    using namespace scratchbird::core;

    class ExpressionEvaluator
    {
    public:
        /**
         * Create evaluator with column information
         * Task 17 MGA Phase 1.4: Added db and xid parameters for transaction context
         *
         * @param columns Table columns (with names and positions)
         * @param pool String pool for string ID resolution
         * @param db Database instance (for tuple fetching, can be nullptr if not needed)
         * @param xid Transaction ID (for visibility checks, 0 if not needed)
         */
        ExpressionEvaluator(const std::vector<CatalogManager::ColumnInfo> &columns,
                           Database *db = nullptr,
                           uint64_t xid = 0);

        /**
         * Evaluate expression against a row
         * @param expr Expression to evaluate
         * @param row Row values (in column order)
         * @return Computed value
         */
        TypedValue evaluate(const Expression *expr, const std::vector<TypedValue> &row);

        /**
         * Evaluate predicate (must return boolean)
         * @param predicate Predicate expression
         * @param row Row values
         * @return true if predicate is satisfied, false otherwise
         */
        bool evaluatePredicate(const Expression *predicate, const std::vector<TypedValue> &row);

        /**
         * Task 17 MGA Phase 1.4: Evaluate expression for a specific tuple (by TID)
         * Fetches the tuple, checks visibility, and evaluates the expression.
         *
         * @param expr Expression to evaluate
         * @param tid Tuple identifier
         * @param result_out Output parameter for computed value
         * @return true if tuple is visible and evaluation succeeded, false otherwise
         */
        bool evaluateForTuple(const Expression *expr,
                             const TID &tid,
                             TypedValue &result_out);

        /**
         * Task 17 MGA Phase 1.4: Evaluate predicate for a specific tuple (by TID)
         * Fetches the tuple, checks visibility, and evaluates the predicate.
         *
         * @param predicate Predicate expression
         * @param tid Tuple identifier
         * @param result_out Output parameter for boolean result
         * @return true if tuple is visible and evaluation succeeded, false otherwise
         */
        bool evaluatePredicateForTuple(const Expression *predicate,
                                       const TID &tid,
                                       bool &result_out);

        /**
         * WP-6 PARSE-4: Evaluate expression with column names and values directly
         * Preferred interface when tuple data is already available.
         *
         * @param expr Expression to evaluate
         * @param column_names Names of columns in the row
         * @param column_values Values for each column
         * @return Evaluated result
         */
        TypedValue evaluateForRow(const Expression *expr,
                                  const std::vector<std::string>& column_names,
                                  const std::vector<TypedValue>& column_values);

        /**
         * WP-6 PARSE-5: Evaluate predicate with column names and values directly
         * Preferred interface when tuple data is already available.
         *
         * @param predicate Predicate expression
         * @param column_names Names of columns in the row
         * @param column_values Values for each column
         * @return Boolean result of predicate evaluation
         */
        bool evaluatePredicateForRow(const Expression *predicate,
                                     const std::vector<std::string>& column_names,
                                     const std::vector<TypedValue>& column_values);

    private:
        std::unordered_map<std::string, size_t> column_positions_; // normalized column name -> row index

        // Task 17 MGA Phase 1.4: Transaction context for visibility checks
        Database *db_;   // Database instance for tuple fetching (nullable)
        uint64_t xid_;         // Current transaction ID for visibility checks

        // Expression type handlers
        TypedValue evaluateLiteral(const LiteralExpr *expr, const std::vector<TypedValue> &row);
        TypedValue evaluateIdentifier(const IdentifierExpr *expr,
                                       const std::vector<TypedValue> &row);
        TypedValue evaluateBinaryOp(const BinaryOpExpr *expr, const std::vector<TypedValue> &row);
        TypedValue evaluateFunctionCall(const FunctionCallExpr *expr,
                                        const std::vector<TypedValue> &row);
        TypedValue evaluateCast(const CastExpr *expr, const std::vector<TypedValue> &row);
        TypedValue evaluateCase(const CaseExpr *expr, const std::vector<TypedValue> &row);
        TypedValue evaluateAggregate(const AggregateExpr *expr,
                                      const std::vector<TypedValue> &row);
        TypedValue evaluateCoalesce(const CoalesceExpr *expr, const std::vector<TypedValue> &row);
        TypedValue evaluateNullIf(const NullIfExpr *expr, const std::vector<TypedValue> &row);
        TypedValue evaluateExtract(const ExtractExpr *expr, const std::vector<TypedValue> &row);
        TypedValue evaluateAlterElement(const AlterElementExpr *expr,
                                        const std::vector<TypedValue> &row);

        // Helper methods
        TypedValue castValue(const TypedValue &value, const TypeInfo& target_type,
                             CastFormat format);
        bool isTruthy(const TypedValue &value);
        static std::string normalizeIdentifier(std::string_view name);
        int compareValues(const TypedValue &left, const TypedValue &right);
    };

} // namespace scratchbird::sblr
