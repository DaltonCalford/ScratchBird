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
#include "scratchbird/core/debug.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace scratchbird::optimizer
{

    auto SelectivityEstimator::estimateWhereClause(
        const parser::v3::Expression *where_clause,
        const core::ID &table_id,
        core::ErrorContext *ctx)
        -> double
    {
        if (!where_clause)
        {
            // No WHERE clause → all rows match
            return 1.0;
        }

        // For Phase 1, use simple heuristic
        // Phase 2 will traverse expression tree to estimate each predicate
        // For now, return conservative default
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
        const std::vector<uint8_t> &v2) const
        -> int
    {
        // Lexicographic comparison
        size_t min_len = std::min(v1.size(), v2.size());

        for (size_t i = 0; i < min_len; i++)
        {
            if (v1[i] < v2[i])
                return -1;
            if (v1[i] > v2[i])
                return 1;
        }

        // Prefixes are equal, compare lengths
        if (v1.size() < v2.size())
            return -1;
        if (v1.size() > v2.size())
            return 1;

        return 0;
    }

    auto SelectivityEstimator::valueEquals(
        const uint8_t *v1,
        const std::vector<uint8_t> &v2) const
        -> bool
    {
        // Compare up to v2 size (v1 is assumed to be at least as long)
        // For MCVs, we stored values in fixed 256-byte arrays
        size_t len = v2.size();
        if (len > 256)
            len = 256;

        return std::memcmp(v1, v2.data(), len) == 0;
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

        // For Phase 1, use simplified heuristic based on expression type
        // Full implementation would recursively traverse the expression tree

        // Check if this is a binary operation (V3 BinaryExpr)
        if (auto* binary_expr = dynamic_cast<const parser::v3::BinaryExpr*>(join_condition))
        {
            // Check for equality (equi-join)
            if (binary_expr->op == parser::v3::BinaryOp::EQ)
            {
                // Try to extract column references from both sides
                // For Phase 1, use default equi-join selectivity
                // Full implementation would call estimateEquiJoinSelectivity

                DEBUG_LOG_DB("Equi-join detected, using default selectivity: " +
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
                    binary_expr->left, left_table_id, right_table_id, ctx);
                double right_sel = estimateJoinSelectivity(
                    binary_expr->right, left_table_id, right_table_id, ctx);

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
                    binary_expr->left, left_table_id, right_table_id, ctx);
                double right_sel = estimateJoinSelectivity(
                    binary_expr->right, left_table_id, right_table_id, ctx);

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
