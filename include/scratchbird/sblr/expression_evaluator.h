#pragma once

#include "scratchbird/parser/ast.h"
#include "scratchbird/parser/token.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/tid.h"
#include <vector>
#include <map>
#include <string>

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
    using namespace scratchbird::parser;
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
                           StringPool *pool,
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

    private:
        StringPool *pool_;
        std::map<StringPool::StringId, size_t> column_positions_; // column name -> row index

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

        // Helper methods
        TypedValue castValue(const TypedValue &value, DataType target_type);
        bool isTruthy(const TypedValue &value);
        int compareValues(const TypedValue &left, const TypedValue &right);
    };

} // namespace scratchbird::sblr
