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

#include <chrono>
#include <cstdint>

namespace scratchbird::sblr
{
    /**
     * Query execution limits to prevent DoS attacks
     *
     * SECURITY ENHANCEMENT (MEDIUM-3): Protects against:
     * - Infinite recursion (recursive CTEs)
     * - Long-running queries (query timeout)
     * - Memory exhaustion (result set limits)
     * - Cartesian products (row count limits)
     */
    struct QueryLimits
    {
        // Execution time limit (default: 30 seconds)
        uint64_t max_execution_time_ms = 30000;

        // Maximum recursion depth for CTEs (default: 100)
        uint32_t max_cte_recursion_depth = 100;

        // Maximum result rows (default: 10 million)
        uint64_t max_result_rows = 10000000;

        // Maximum intermediate result rows (default: 100 million)
        uint64_t max_intermediate_rows = 100000000;

        /**
         * Get default limits
         */
        static QueryLimits defaults()
        {
            return QueryLimits{};
        }

        /**
         * Get strict limits for security-critical environments
         */
        static QueryLimits strict()
        {
            QueryLimits limits;
            limits.max_execution_time_ms = 10000;    // 10 seconds
            limits.max_cte_recursion_depth = 50;      // 50 levels
            limits.max_result_rows = 1000000;         // 1 million
            limits.max_intermediate_rows = 10000000;  // 10 million
            return limits;
        }

        /**
         * Get relaxed limits for batch processing
         */
        static QueryLimits relaxed()
        {
            QueryLimits limits;
            limits.max_execution_time_ms = 300000;      // 5 minutes
            limits.max_cte_recursion_depth = 200;       // 200 levels
            limits.max_result_rows = 100000000;         // 100 million
            limits.max_intermediate_rows = 1000000000;  // 1 billion
            return limits;
        }
    };

} // namespace scratchbird::sblr
