#pragma once

#include "scratchbird/parser/ast.h"
#include "scratchbird/parser/string_pool.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/catalog_manager.h"
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
         * @param columns Table columns (with names and positions)
         * @param pool String pool for string ID resolution
         */
        ExpressionEvaluator(const std::vector<ColumnInfo> &columns, StringPool *pool);

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

    private:
        StringPool *pool_;
        std::map<StringPool::StringId, size_t> column_positions_; // column name -> row index

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
