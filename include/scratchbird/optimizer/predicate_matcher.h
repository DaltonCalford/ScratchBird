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
#include <string>
#include <string_view>
#include <vector>

/**
 * Predicate Matcher for Task 17 Phase 9: Filtered Index Support
 *
 * Determines if a query's WHERE clause implies a filtered index's predicate,
 * enabling the query planner to use partial/filtered indexes automatically.
 *
 * Key Concept: Implication
 * - If query_pred IMPLIES index_pred, then all rows matching query_pred
 *   are guaranteed to match index_pred, so the filtered index can be used.
 *
 * Example Use Cases:
 *
 * 1. Simple Implication:
 *    Query: WHERE age > 30
 *    Index: ON table (col) WHERE age > 18
 *    Result: true (age > 30 implies age > 18)
 *
 * 2. Equality Implication:
 *    Query: WHERE status = 'active'
 *    Index: ON table (col) WHERE status = 'active'
 *    Result: true (exact match)
 *
 * 3. AND Conjunction:
 *    Query: WHERE active = true AND verified = true
 *    Index: ON table (col) WHERE active = true
 *    Result: true (conjunct implies index predicate)
 *
 * 4. No Implication:
 *    Query: WHERE age > 18
 *    Index: ON table (col) WHERE age > 30
 *    Result: false (age > 18 does NOT imply age > 30)
 *
 * PostgreSQL Compatibility:
 * - Matches PostgreSQL's filtered index matching behavior
 * - Conservative approach: only matches when implication is certain
 * - Handles NULL semantics correctly
 */

namespace scratchbird::optimizer
{
    using namespace scratchbird::core;

    /**
     * PredicateMatcher - Determines if query predicate implies index predicate
     *
     * Task 17 Phase 9
     */
    class PredicateMatcher
    {
    public:
        /**
         * Check if query predicate implies index predicate
         *
         * Returns true if all rows matching query_pred are guaranteed to
         * match index_pred, enabling the use of the filtered index.
         *
         * This is a conservative implementation that handles common cases:
         * - Exact equality matches
         * - Range implication (stricter range implies weaker range)
         * - Conjunctive predicates (AND clauses)
         * - NOT NULL implications
         *
         * @param query_pred Predicate from query WHERE clause
         * @param index_pred Predicate from filtered index WHERE clause
         * @param query_pool String pool for query predicate
         * @param index_pool String pool for index predicate (optional, defaults to query_pool)
         * @return true if query_pred implies index_pred
         */
        static bool implies(const Expression *query_pred,
                           const Expression *index_pred);

        /**
         * Check if query predicate contains (implies) index predicate as a conjunct
         *
         * Searches through AND clauses in query_pred to find a match.
         *
         * Example:
         *   Query: WHERE a = 1 AND b = 2 AND c = 3
         *   Index: WHERE b = 2
         *   Result: true (b = 2 is a conjunct in query)
         *
         * @param query_pred Query predicate (may contain AND clauses)
         * @param index_pred Index predicate
         * @param query_pool String pool for query predicate
         * @param index_pool String pool for index predicate (optional, defaults to query_pool)
         * @return true if index_pred appears as conjunct in query_pred
         */
        static bool containsConjunct(const Expression *query_pred,
                                     const Expression *index_pred);

    private:
        /**
         * Check if two predicates are structurally identical
         *
         * Uses ExpressionMatcher::matches() internally.
         */
        static bool predicatesEqual(const Expression *pred1,
                                    const Expression *pred2);

        /**
         * Check if query range implies index range
         *
         * Handles comparisons like:
         * - age > 30 implies age > 18 (true)
         * - age < 10 implies age < 20 (true)
         * - age > 18 implies age > 30 (false)
         *
         * @param query_binop Query comparison (e.g., age > 30)
         * @param index_binop Index comparison (e.g., age > 18)
         * @param query_pool String pool for query predicate
         * @param index_pool String pool for index predicate
         * @return true if query range implies index range
         */
        static bool rangeImplies(const BinaryOpExpr *query_binop,
                                const BinaryOpExpr *index_binop);

        /**
         * Check if NOT NULL predicate is implied
         *
         * Examples:
         * - "col = 5" implies "col IS NOT NULL"
         * - "col > 10" implies "col IS NOT NULL"
         *
         * @param query_pred Query predicate
         * @param index_not_null Index predicate (col IS NOT NULL)
         * @param query_pool String pool for query predicate
         * @param index_pool String pool for index predicate
         * @return true if query implies NOT NULL
         */
        static bool impliesNotNull(const Expression *query_pred,
                                   const Expression *index_not_null);

        /**
         * Extract column identifier from expression if possible
         *
         * Returns nullptr if expression is not a simple column reference.
         */
        static const IdentifierExpr *extractColumn(const Expression *expr);

        /**
         * Compare two literal values
         *
         * Returns:
         * -1 if lit1 < lit2
         *  0 if lit1 == lit2
         *  1 if lit1 > lit2
         *  INT_MIN if incomparable (different types, NULL, etc.)
         */
        static int compareLiterals(const LiteralExpr *lit1, const LiteralExpr *lit2);

        /**
         * Check if operator implies NOT NULL
         *
         * Operators like =, <, >, <=, >= all imply their operands are NOT NULL
         * (due to NULL propagation rules).
         */
        static bool operatorImpliesNotNull(BinaryOp op);

        /**
         * Recursively collect all AND conjuncts from expression
         *
         * Example: (a AND b AND c) → [a, b, c]
         */
        static void collectConjuncts(const Expression *expr,
                                     std::vector<const Expression *> &conjuncts);

        static std::string normalizeIdentifier(std::string_view name);
    };

} // namespace scratchbird::optimizer
