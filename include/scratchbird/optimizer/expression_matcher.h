#pragma once

#include "scratchbird/core/expression.h"
#include <string>
#include <string_view>
#include <vector>

/**
 * Expression Matcher for Task 17 Phase 8: Query Planner Integration
 *
 * Matches query expressions against index expressions to determine if an
 * expression index can be used to satisfy a query predicate.
 *
 * Key Features:
 * - Structural AST matching (exact matches)
 * - Commutative operator handling (a + b == b + a)
 * - Range scan compatibility (LIKE 'prefix%' can use exact match index)
 * - Type-aware matching
 *
 * Example Use Cases:
 *
 * 1. Exact Match:
 *    Query: WHERE LOWER(email) = 'test@example.com'
 *    Index: ON (LOWER(email))
 *    Result: EXACT_MATCH - can use index for point lookup
 *
 * 2. Range Scan:
 *    Query: WHERE LOWER(email) LIKE 'test%'
 *    Index: ON (LOWER(email))
 *    Result: RANGE_SCAN - can use index for prefix scan
 *
 * 3. No Match:
 *    Query: WHERE UPPER(email) = 'TEST'
 *    Index: ON (LOWER(email))
 *    Result: NO_MATCH - different functions
 *
 * PostgreSQL Compatibility:
 * - Matches PostgreSQL's expression index matching behavior
 * - Handles function calls, binary operators, casts
 * - Supports commutative operators (=, +, *, AND, OR)
 */

namespace scratchbird::optimizer
{
    using namespace scratchbird::core;

    /**
     * Match result indicating how well query expression matches index expression
     */
    enum class ExpressionMatchType
    {
        NO_MATCH,      // Cannot use index
        EXACT_MATCH,   // Can use index for point lookup (=, IN)
        RANGE_SCAN,    // Can use index for range scan (<, >, <=, >=, LIKE 'prefix%')
        PARTIAL_MATCH  // Can use index but with additional filtering
    };

    /**
     * ExpressionMatcher - Matches query expressions to index expressions
     *
     * Task 17 Phase 8
     */
    class ExpressionMatcher
    {
    public:
        /**
         * Check if query expression exactly matches index expression
         *
         * Returns true if the expressions are structurally identical,
         * accounting for commutative operators.
         *
         * @param query_expr Expression from WHERE clause
         * @param index_expr Expression from index definition
         * @param query_pool String pool for query expression (for resolving StringIds)
         * @param index_pool String pool for index expression (optional, defaults to query_pool)
         * @return true if exact structural match
         */
        static bool matches(const Expression *query_expr,
                           const Expression *index_expr);

        /**
         * Check if query expression can use index expression
         *
         * Returns match type indicating how the index can be used.
         *
         * @param query_expr Expression from WHERE clause
         * @param index_expr Expression from index definition
         * @param query_pool String pool for query expression
         * @param index_pool String pool for index expression (optional, defaults to query_pool)
         * @return Match type (EXACT_MATCH, RANGE_SCAN, or NO_MATCH)
         */
        static ExpressionMatchType canUse(const Expression *query_expr,
                                           const Expression *index_expr);

        /**
         * Check if operator is compatible with indexed expression
         *
         * Determines if a comparison operator can use an index.
         *
         * Examples:
         * - LOWER(email) = 'test' → EXACT_MATCH (index on LOWER(email))
         * - LOWER(email) < 'test' → RANGE_SCAN (index on LOWER(email))
         * - LOWER(email) LIKE 'test%' → RANGE_SCAN (index on LOWER(email))
         * - LOWER(email) LIKE '%test' → NO_MATCH (can't use index for suffix scan)
         *
         * @param op Binary operator type
         * @param left_expr Left operand
         * @param right_expr Right operand
         * @param index_expr Index expression
         * @param query_pool String pool for query expression
         * @param index_pool String pool for index expression
         * @return Match type
         */
        static ExpressionMatchType isOperatorCompatible(BinaryOp op,
                                                        const Expression *left_expr,
                                                        const Expression *right_expr,
                                                        const Expression *index_expr);

    private:
        // Type-specific matchers
        static bool matchLiteral(const LiteralExpr *q, const LiteralExpr *i);
        static bool matchIdentifier(const IdentifierExpr *q, const IdentifierExpr *i);
        static bool matchBinaryOp(const BinaryOpExpr *q, const BinaryOpExpr *i);
        static bool matchFunctionCall(const FunctionCallExpr *q, const FunctionCallExpr *i);
        static bool matchCast(const CastExpr *q, const CastExpr *i);
        static bool matchCase(const CaseExpr *q, const CaseExpr *i);
        static bool matchAggregate(const AggregateExpr *q, const AggregateExpr *i);
        static bool matchCoalesce(const CoalesceExpr *q, const CoalesceExpr *i);
        static bool matchNullIf(const NullIfExpr *q, const NullIfExpr *i);
        static bool matchExtract(const ExtractExpr *q, const ExtractExpr *i);

        // Helper methods
        static bool isCommutativeOperator(BinaryOp op);
        static bool isComparisonOperator(BinaryOp op);
        static bool isLikePrefixScan(const LiteralExpr *pattern);
        static std::string normalizeIdentifier(std::string_view name);
        static std::string normalizeFunctionName(std::string_view name);
        static ExpressionMatchType getMatchTypeForOperator(BinaryOp op);
    };

} // namespace scratchbird::optimizer
