#pragma once

/**
 * SelectivityEstimator - Estimates predicate selectivity for query optimization
 *
 * V2 MIGRATION STATUS: COMPLETE
 *
 * Uses V2 ResolvedExpression types for WHERE clause and join condition estimation.
 */

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/types.h"
#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/sblr/resolved_ast_v2.h"
#include <string>
#include <vector>

namespace scratchbird::optimizer
{

    /**
     * SelectivityEstimator - Estimates predicate selectivity for query optimization
     *
     * Selectivity is the fraction of rows (0.0 to 1.0) that satisfy a predicate.
     * Accurate selectivity estimates are critical for cost-based optimization:
     * - Too high → Optimizer may choose wrong access path
     * - Too low → Optimizer may miss good index opportunities
     * - Goal: Estimate within 2-3x of actual selectivity
     *
     * Uses statistics from StatisticsManager:
     * - n_distinct: For equality selectivity
     * - MCVs: For exact frequencies of common values
     * - Histograms: For range selectivity
     * - null_fraction: For IS NULL predicates
     *
     * Phase 1, Task 1.4
     */
    class SelectivityEstimator
    {
    public:
        /**
         * Constructor
         *
         * @param stats_manager Statistics manager for accessing column statistics
         */
        explicit SelectivityEstimator(StatisticsManager *stats_manager)
            : stats_manager_(stats_manager)
        {
        }

        /**
         * estimateWhereClause - Estimate selectivity of entire WHERE clause
         *
         * Recursively traverses expression tree and estimates selectivity
         * for each predicate, combining them with AND/OR/NOT rules.
         *
         * @param where_clause WHERE clause expression (can be NULL)
         * @param table_id Table ID for column lookups
         * @param ctx Error context
         * @return Selectivity estimate (0.0 to 1.0)
         *
         * Phase 1, Task 1.4.1
         */
        auto estimateWhereClause(const parser::v2::ResolvedExpression *where_clause,
                                 const core::ID &table_id,
                                 core::ErrorContext *ctx = nullptr)
            -> double;

        /**
         * estimateEquality - Estimate selectivity for equality predicate (col = value)
         *
         * Algorithm:
         * 1. Check if value is in MCV list → use MCV frequency
         * 2. Otherwise, use uniform distribution: 1.0 / n_distinct
         * 3. Adjust for remaining probability after MCVs
         *
         * Example:
         *   n_distinct = 100, mcv_list = [('USA', 0.40), ('Canada', 0.20)]
         *   Query: col = 'USA' → selectivity = 0.40 (from MCV)
         *   Query: col = 'France' → selectivity = (1.0 - 0.60 - null_frac) / 98
         *
         * @param table_id Table ID
         * @param column_id Column ID
         * @param value Value to compare (empty vector = NULL)
         * @param ctx Error context
         * @return Selectivity estimate
         *
         * Phase 1, Task 1.4.2
         */
        auto estimateEquality(const core::ID &table_id,
                              const core::ID &column_id,
                              const std::vector<uint8_t> &value,
                              core::ErrorContext *ctx = nullptr)
            -> double;

        /**
         * estimateRange - Estimate selectivity for range predicate (col >, <, >=, <=, BETWEEN value)
         *
         * Uses histogram buckets to estimate fraction of values in range.
         *
         * Algorithm:
         * 1. For each histogram bucket:
         *    - If bucket is completely in range → add full bucket frequency
         *    - If bucket is partially in range → interpolate fraction
         *    - If bucket is completely out of range → skip
         * 2. Sum contributions from all buckets
         *
         * Example:
         *   Query: age > 30
         *   Histogram: [18-25: 25%], [26-35: 25%], [36-50: 25%], [51-80: 25%]
         *   Bucket 1: 0% (all ≤ 30)
         *   Bucket 2: ~55.6% (partial: 30 is 5/9 through [26,35])
         *   Bucket 3: 100% (all > 30)
         *   Bucket 4: 100% (all > 30)
         *   selectivity = 0.25*0 + 0.25*0.556 + 0.25*1.0 + 0.25*1.0 = 0.639
         *
         * @param table_id Table ID
         * @param column_id Column ID
         * @param op Operator (">", "<", ">=", "<=")
         * @param value Value to compare
         * @param ctx Error context
         * @return Selectivity estimate
         *
         * Phase 1, Task 1.4.3
         */
        auto estimateRange(const core::ID &table_id,
                           const core::ID &column_id,
                           const std::string &op,
                           const std::vector<uint8_t> &value,
                           core::ErrorContext *ctx = nullptr)
            -> double;

        /**
         * estimateBetween - Estimate selectivity for BETWEEN predicate (col BETWEEN a AND b)
         *
         * Formula: selectivity(col BETWEEN a AND b) = selectivity(col >= a) - selectivity(col > b)
         *
         * @param table_id Table ID
         * @param column_id Column ID
         * @param lower_value Lower bound (inclusive)
         * @param upper_value Upper bound (inclusive)
         * @param ctx Error context
         * @return Selectivity estimate
         *
         * Phase 1, Task 1.4.3
         */
        auto estimateBetween(const core::ID &table_id,
                             const core::ID &column_id,
                             const std::vector<uint8_t> &lower_value,
                             const std::vector<uint8_t> &upper_value,
                             core::ErrorContext *ctx = nullptr)
            -> double;

        /**
         * estimateLike - Estimate selectivity for LIKE predicate (col LIKE pattern)
         *
         * Pattern analysis:
         * - 'prefix%': Use histogram range estimation (prefix match)
         * - '%suffix': Cannot use index, use heuristic (5%)
         * - '%contains%': Cannot use index, use heuristic (1%)
         * - 'exact': Treat as equality
         *
         * @param table_id Table ID
         * @param column_id Column ID
         * @param pattern LIKE pattern with % wildcards
         * @param ctx Error context
         * @return Selectivity estimate
         *
         * Phase 1, Task 1.4.4
         */
        auto estimateLike(const core::ID &table_id,
                          const core::ID &column_id,
                          const std::string &pattern,
                          core::ErrorContext *ctx = nullptr)
            -> double;

        /**
         * estimateIn - Estimate selectivity for IN predicate (col IN (v1, v2, v3))
         *
         * Formula: sum of individual equality selectivities, capped at 1.0
         *
         * Example:
         *   Query: country IN ('USA', 'Canada', 'UK')
         *   selectivity = sel('USA') + sel('Canada') + sel('UK')
         *               = 0.40 + 0.20 + 0.10 = 0.70
         *
         * @param table_id Table ID
         * @param column_id Column ID
         * @param values List of values to match
         * @param ctx Error context
         * @return Selectivity estimate
         *
         * Phase 1, Task 1.4.5
         */
        auto estimateIn(const core::ID &table_id,
                        const core::ID &column_id,
                        const std::vector<std::vector<uint8_t>> &values,
                        core::ErrorContext *ctx = nullptr)
            -> double;

        /**
         * estimateAnd - Combine selectivities with AND
         *
         * Formula (independence assumption): sel1 * sel2
         *
         * Example:
         *   age > 25 (sel=0.75) AND country='USA' (sel=0.40)
         *   → selectivity = 0.75 * 0.40 = 0.30
         *
         * @param sel1 Selectivity of first predicate
         * @param sel2 Selectivity of second predicate
         * @return Combined selectivity
         *
         * Phase 1, Task 1.4.5
         */
        auto estimateAnd(double sel1, double sel2) const -> double;

        /**
         * estimateOr - Combine selectivities with OR
         *
         * Formula: sel1 + sel2 - (sel1 * sel2)
         *
         * Example:
         *   age > 60 (sel=0.10) OR country='USA' (sel=0.40)
         *   → selectivity = 0.10 + 0.40 - (0.10 * 0.40) = 0.46
         *
         * @param sel1 Selectivity of first predicate
         * @param sel2 Selectivity of second predicate
         * @return Combined selectivity
         *
         * Phase 1, Task 1.4.5
         */
        auto estimateOr(double sel1, double sel2) const -> double;

        /**
         * estimateNot - Negate selectivity with NOT
         *
         * Formula: 1.0 - sel
         *
         * Example:
         *   NOT (age > 60) where age > 60 has sel=0.10
         *   → selectivity = 1.0 - 0.10 = 0.90
         *
         * @param sel Selectivity of predicate
         * @return Negated selectivity
         *
         * Phase 1, Task 1.4.5
         */
        auto estimateNot(double sel) const -> double;

        /**
         * estimateJoinSelectivity - Estimate selectivity of join condition
         *
         * Selectivity determines how many rows the join produces:
         *   output_rows = left_rows × right_rows × selectivity
         *
         * Algorithm:
         * 1. For equi-joins (col1 = col2):
         *    - Use n_distinct statistics from both tables
         *    - Formula: 1 / MAX(n_distinct(col1), n_distinct(col2))
         *    - Rationale: Each value in smaller domain matches 1/n_distinct of larger
         *
         * 2. For foreign key joins (detected via catalog):
         *    - Formula: 1 / n_distinct(primary_key_col)
         *    - Rationale: Each FK value matches exactly one PK value
         *
         * 3. For range joins (col1 > col2, col1 < col2):
         *    - Use DEFAULT_RANGE_SEL (0.33)
         *    - Conservative estimate without cross-table histograms
         *
         * 4. For compound joins (col1 = col2 AND col3 = col4):
         *    - Multiply selectivities (independence assumption)
         *    - May underestimate for correlated columns
         *
         * Example:
         *   users.id = orders.user_id
         *   n_distinct(users.id) = 10,000 (primary key)
         *   n_distinct(orders.user_id) = 5,000 (foreign key)
         *   selectivity = 1 / MAX(10,000, 5,000) = 1 / 10,000 = 0.0001
         *   output_rows = 10,000 × 100,000 × 0.0001 = 100,000 (same as orders count)
         *
         * @param join_condition Join condition expression (ON clause)
         *   @param left_table_id Left table ID
         * @param right_table_id Right table ID
         * @param ctx Error context
         * @return Selectivity estimate (0.0 to 1.0)
         *
         * Phase 1, Task 3.2
         */
        auto estimateJoinSelectivity(const parser::v2::ResolvedExpression* join_condition,
                                     const core::ID& left_table_id,
                                     const core::ID& right_table_id,
                                     core::ErrorContext* ctx = nullptr)
            -> double;

    private:
        /**
         * estimateEquiJoinSelectivity - Estimate selectivity for equi-join (col1 = col2)
         *
         * Uses n_distinct statistics from both columns.
         *
         * @param left_col_id Left column ID
         * @param right_col_id Right column ID
         * @param left_table_id Left table ID
         * @param right_table_id Right table ID
         * @param ctx Error context
         * @return Selectivity estimate
         *
         * Phase 1, Task 3.2
         */
        auto estimateEquiJoinSelectivity(const core::ID& left_col_id,
                                        const core::ID& right_col_id,
                                        const core::ID& left_table_id,
                                        const core::ID& right_table_id,
                                        core::ErrorContext* ctx = nullptr)
            -> double;

    private:
        StatisticsManager *stats_manager_;

        /**
         * compareValues - Compare two byte vectors as column values
         *
         * @param v1 First value
         * @param v2 Second value
         * @return -1 if v1 < v2, 0 if equal, 1 if v1 > v2
         */
        auto compareValues(const std::vector<uint8_t> &v1,
                           const std::vector<uint8_t> &v2) const
            -> int;

        /**
         * valueEquals - Check if two byte vectors are equal
         */
        auto valueEquals(const uint8_t *v1, const std::vector<uint8_t> &v2) const
            -> bool;

        /**
         * interpolateBucket - Interpolate position within histogram bucket
         *
         * Returns fraction of bucket that is beyond 'value' (for > queries)
         * or before 'value' (for < queries).
         *
         * @param value Value to interpolate
         * @param bucket_min Bucket lower bound
         * @param bucket_max Bucket upper bound
         * @return Fraction (0.0 to 1.0)
         */
        auto interpolateBucket(const std::vector<uint8_t> &value,
                               const uint8_t *bucket_min,
                               const uint8_t *bucket_max) const
            -> double;

        // Default selectivity values when statistics unavailable
        static constexpr double DEFAULT_EQUALITY_SEL = 0.01;       // 1% (assume 100 distinct)
        static constexpr double DEFAULT_INEQUALITY_SEL = 0.99;     // 99%
        static constexpr double DEFAULT_RANGE_SEL = 0.33;          // 33%
        static constexpr double DEFAULT_BETWEEN_SEL = 0.10;        // 10%
        static constexpr double DEFAULT_LIKE_PREFIX_SEL = 0.10;    // 10%
        static constexpr double DEFAULT_LIKE_SUFFIX_SEL = 0.05;    // 5%
        static constexpr double DEFAULT_LIKE_CONTAINS_SEL = 0.01;  // 1%
        static constexpr double DEFAULT_NULL_SEL = 0.05;           // 5%
        static constexpr double DEFAULT_NOT_NULL_SEL = 0.95;       // 95%
    };

} // namespace scratchbird::optimizer
