#pragma once

#include "scratchbird/parser/ast.h"
#include "scratchbird/parser/token.h"
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
        ExpressionEvaluator(const std::vector<core::CatalogManager::ColumnInfo> &columns, parser::StringPool *pool);

        /**
         * Evaluate expression against a row
         * @param expr Expression to evaluate
         * @param row Row values (in column order)
         * @return Computed value
         */
        core::TypedValue evaluate(const parser::Expression *expr, const std::vector<core::TypedValue> &row);

        /**
         * Evaluate predicate (must return boolean)
         * @param predicate Predicate expression
         * @param row Row values
         * @return true if predicate is satisfied, false otherwise
         */
        bool evaluatePredicate(const parser::Expression *predicate, const std::vector<core::TypedValue> &row);

    private:
        parser::StringPool *pool_;
        std::map<parser::StringPool::StringId, size_t> column_positions_; // column name -> row index

        // Expression type handlers
        core::TypedValue evaluateLiteral(const parser::LiteralExpr *expr, const std::vector<core::TypedValue> &row);
        core::TypedValue evaluateIdentifier(const parser::IdentifierExpr *expr,
                                       const std::vector<core::TypedValue> &row);
        core::TypedValue evaluateBinaryOp(const parser::BinaryOpExpr *expr, const std::vector<core::TypedValue> &row);
        core::TypedValue evaluateFunctionCall(const parser::FunctionCallExpr *expr,
                                        const std::vector<core::TypedValue> &row);
        core::TypedValue evaluateCast(const parser::CastExpr *expr, const std::vector<core::TypedValue> &row);
        core::TypedValue evaluateCase(const parser::CaseExpr *expr, const std::vector<core::TypedValue> &row);
        core::TypedValue evaluateAggregate(const parser::AggregateExpr *expr,
                                      const std::vector<core::TypedValue> &row);
        core::TypedValue evaluateCoalesce(const parser::CoalesceExpr *expr, const std::vector<core::TypedValue> &row);
        core::TypedValue evaluateNullIf(const parser::NullIfExpr *expr, const std::vector<core::TypedValue> &row);

        // Helper methods
        core::TypedValue castValue(const core::TypedValue &value, core::DataType target_type);
        bool isTruthy(const core::TypedValue &value);
        int compareValues(const core::TypedValue &left, const core::TypedValue &right);
    };

} // namespace scratchbird::sblr
