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

#include <memory>
#include <unordered_map>
#include <mutex>
#include "scratchbird/optimizer/statistics.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/catalog_manager.h"

namespace scratchbird::core
{
    class Database;
}

namespace scratchbird::optimizer
{

    using namespace scratchbird::core;

    /**
     * Statistics Manager - Manages query optimizer statistics
     *
     * Responsibilities:
     * - Collecting statistics via ANALYZE command
     * - Storing statistics in pg_statistic catalog
     * - Retrieving statistics for query optimization
     * - Maintaining statistics cache
     *
     * Phase 1, Task 1.1: Statistics Collection
     */
    class StatisticsManager
    {
    public:
        /**
         * Constructor
         *
         * @param db Database instance
         */
        explicit StatisticsManager(Database *db);

        ~StatisticsManager();

        /**
         * analyzeTable - Collect statistics for a table
         *
         * Implements the ANALYZE command. Samples table data using Vitter's
         * Algorithm S and computes column-level statistics.
         *
         * Algorithm:
         * 1. Determine sample size (default: min(30000, table_rows))
         * 2. Sample rows using Vitter's Algorithm S (reservoir sampling)
         * 3. For each column:
         *    a. Compute basic statistics (null_fraction, n_distinct, avg_width)
         *    b. Identify Most Common Values (MCVs) - top 100 by frequency
         *    c. Generate histogram (equal-height or equal-width)
         * 4. Store statistics in pg_statistic catalog
         * 5. Update table statistics (num_rows, num_pages)
         * 6. Refresh statistics cache
         *
         * @param table_id Table to analyze
         * @param sample_rate Fraction of table to sample (0.0-1.0, 0 = auto)
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Phase 1, Task 1.1
         */
        auto analyzeTable(const ID &table_id, float sample_rate = 0.0f,
                          ErrorContext *ctx = nullptr) -> Status;

        /**
         * analyzeColumn - Collect statistics for a single column
         *
         * Useful for targeted statistics updates without analyzing entire table.
         *
         * @param table_id Table ID
         * @param column_id Column ID
         * @param sample_rate Fraction of table to sample (0.0-1.0, 0 = auto)
         * @param ctx Error context
         * @return Status::OK on success
         *
         * Phase 1, Task 1.1
         */
        auto analyzeColumn(const ID &table_id, const ID &column_id, float sample_rate = 0.0f,
                           ErrorContext *ctx = nullptr) -> Status;

        /**
         * getColumnStatistics - Retrieve statistics for a column
         *
         * Loads from cache if available, otherwise reads from pg_statistic catalog.
         *
         * @param table_id Table ID
         * @param column_id Column ID
         * @param stats Output: column statistics
         * @param ctx Error context
         * @return Status::OK on success, Status::NOT_FOUND if no statistics
         *
         * Phase 1, Task 1.4: Selectivity Estimation
         */
        auto getColumnStatistics(const ID &table_id, const ID &column_id,
                                 ColumnStatistics &stats, ErrorContext *ctx = nullptr) -> Status;

        /**
         * getTableStatistics - Retrieve statistics for a table
         *
         * @param table_id Table ID
         * @param stats Output: table statistics
         * @param ctx Error context
         * @return Status::OK on success, Status::NOT_FOUND if no statistics
         *
         * Phase 1, Task 1.2: Cost Model
         */
        auto getTableStatistics(const ID &table_id, TableStatistics &stats,
                                ErrorContext *ctx = nullptr) -> Status;

        auto getColumnCorrelation(const ID &table_id,
                                  const ID &left_column_id,
                                  const ID &right_column_id,
                                  ColumnCorrelationStatistics &stats_out,
                                  ErrorContext *ctx = nullptr) -> Status;

        auto getExpressionStatistics(const ID &table_id,
                                     const std::string &expression_key,
                                     ExpressionStatistics &stats_out,
                                     ErrorContext *ctx = nullptr) -> Status;

        /**
         * dropStatistics - Remove statistics for a table
         *
         * Called when a table is dropped or truncated.
         *
         * @param table_id Table ID
         * @param ctx Error context
         * @return Status::OK on success
         *
         * Phase 1, Task 1.1
         */
        auto dropStatistics(const ID &table_id, ErrorContext *ctx = nullptr) -> Status;

        /**
         * invalidateCache - Clear statistics cache
         *
         * Forces next access to reload from catalog.
         * Called after ANALYZE or schema changes.
         *
         * @param table_id Optional: specific table to invalidate (0 = all)
         */
        auto invalidateCache(const ID &table_id = ID{}) -> void;

        /**
         * getStatisticsPageId - Get the page ID for pg_statistic catalog
         *
         * Used internally to read/write statistics records.
         */
        auto getStatisticsPageId() const -> uint32_t
        {
            return statistics_page_id_;
        }

    private:
        Database *db_;
        CatalogManager *catalog_;
        uint32_t statistics_page_id_;

        // Statistics cache (table_id + column_id -> ColumnStatistics)
        std::unordered_map<uint64_t, ColumnStatistics> column_stats_cache_;
        std::unordered_map<uint64_t, TableStatistics> table_stats_cache_;
        std::unordered_map<std::string, ColumnCorrelationStatistics> correlation_stats_cache_;
        std::unordered_map<std::string, ExpressionStatistics> expression_stats_cache_;
        mutable std::mutex cache_mutex_;

        /**
         * sampleTable - Sample rows from a table using Vitter's Algorithm S
         *
         * Implements reservoir sampling with Algorithm S for efficient random sampling
         * of large tables without requiring full table scan.
         *
         * Reference: J. S. Vitter, "Random Sampling with a Reservoir",
         * ACM Transactions on Mathematical Software, 1985
         *
         * @param table_id Table to sample
         * @param sample_size Number of rows to sample
         * @param sample_rows Output: sampled row data
         * @param ctx Error context
         * @return Status::OK on success
         *
         * Phase 1, Task 1.1
         */
        auto sampleTable(const ID &table_id, uint64_t sample_size,
                         std::vector<std::vector<uint8_t>> &sample_rows,
                         ErrorContext *ctx = nullptr) -> Status;

        auto analyzeTableInternal(const ID &table_id,
                                  float sample_rate,
                                  bool automatic,
                                  ErrorContext *ctx = nullptr) -> Status;

        /**
         * computeColumnStats - Compute statistics for a column from sample
         *
         * Analyzes sampled rows to compute:
         * - null_fraction
         * - n_distinct (using HyperLogLog for large cardinality)
         * - avg_width
         * - Most Common Values (MCVs)
         * - Histogram buckets
         *
         * @param table_id Table ID
         * @param column_id Column ID
         * @param sample_rows Sampled row data
         * @param stats Output: computed statistics
         * @param ctx Error context
         * @return Status::OK on success
         *
         * Phase 1, Task 1.1
         */
        auto computeColumnStats(const ID &table_id, const ID &column_id,
                                const std::vector<std::vector<uint8_t>> &sample_rows,
                                ColumnStatistics &stats, ErrorContext *ctx = nullptr) -> Status;

        /**
         * generateHistogram - Generate histogram buckets
         *
         * Creates equal-height or equal-width histogram from sorted values.
         *
         * @param values Sorted column values (non-NULL)
         * @param bucket_count Number of buckets (default: 100)
         * @param histogram_type Equal-height or equal-width
         * @param buckets Output: histogram buckets
         * @param ctx Error context
         * @return Status::OK on success
         *
         * Phase 1, Task 1.1
         */
        auto generateHistogram(const std::vector<std::vector<uint8_t>> &values, uint32_t bucket_count,
                               HistogramType histogram_type,
                               std::vector<HistogramBucket> &buckets,
                               ErrorContext *ctx = nullptr) -> Status;

        /**
         * identifyMCVs - Identify Most Common Values
         *
         * Finds the top-k most frequent values in the sample.
         *
         * @param values Column values
         * @param max_mcv_count Maximum MCVs to store (default: 100)
         * @param mcv_list Output: MCV entries sorted by frequency (descending)
         * @param ctx Error context
         * @return Status::OK on success
         *
         * Phase 1, Task 1.1
         */
        auto identifyMCVs(const std::vector<std::vector<uint8_t>> &values, uint32_t max_mcv_count,
                          std::vector<MCVEntry> &mcv_list, ErrorContext *ctx = nullptr) -> Status;

        /**
         * estimateNDistinct - Estimate number of distinct values
         *
         * Uses HyperLogLog algorithm for large cardinality estimation.
         * For small samples, uses exact count.
         *
         * @param values Column values
         * @param total_rows Total rows in table
         * @param sample_size Number of sampled rows
         * @return Estimated number of distinct values
         *
         * Phase 1, Task 1.1
         */
        auto estimateNDistinct(const std::vector<std::vector<uint8_t>> &values, uint64_t total_rows,
                               uint64_t sample_size) -> uint64_t;

        /**
         * storeColumnStatistics - Store column statistics to pg_statistic catalog
         *
         * Serializes ColumnStatistics and writes to catalog.
         * Uses TOAST for large MCVs and histograms.
         *
         * @param stats Column statistics to store
         * @param ctx Error context
         * @return Status::OK on success
         *
         * Phase 1, Task 1.1
         */
        auto storeColumnStatistics(const ColumnStatistics &stats, ErrorContext *ctx = nullptr)
            -> Status;

        /**
         * loadColumnStatistics - Load column statistics from pg_statistic catalog
         *
         * Reads from catalog and deserializes into ColumnStatistics.
         *
         * @param table_id Table ID
         * @param column_id Column ID
         * @param stats Output: loaded statistics
         * @param ctx Error context
         * @return Status::OK on success, Status::NOT_FOUND if no statistics
         *
         * Phase 1, Task 1.4
         */
        auto loadColumnStatistics(const ID &table_id, const ID &column_id,
                                  ColumnStatistics &stats, ErrorContext *ctx = nullptr) -> Status;

        /**
         * getCacheKey - Compute cache key for column statistics
         *
         * Combines table_id and column_id into a single uint64_t key.
         */
        static auto getCacheKey(const ID &table_id, const ID &column_id) -> uint64_t;

        auto getCorrelationCacheKey(const ID &table_id,
                                    const ID &left_column_id,
                                    const ID &right_column_id) const -> std::string;

        auto getExpressionCacheKey(const ID &table_id,
                                   const std::string &expression_key) const -> std::string;

        static auto makeSyntheticStatisticId(const ID &table_id,
                                             const std::string &kind,
                                             const std::string &key) -> ID;

        static auto makeCorrelationStatisticKey(const ID &left_column_id,
                                                const ID &right_column_id) -> std::string;

        auto storeCorrelationStatistic(const ColumnCorrelationStatistics &stats,
                                       ErrorContext *ctx = nullptr) -> Status;

        auto loadCorrelationStatistic(const ID &table_id,
                                      const ID &left_column_id,
                                      const ID &right_column_id,
                                      ColumnCorrelationStatistics &stats_out,
                                      ErrorContext *ctx = nullptr) -> Status;

        auto maybeAutoAnalyze(const ID &table_id, ErrorContext *ctx = nullptr) -> Status;

        /**
         * extractColumnValues - Extract values for a specific column from sample rows
         *
         * Helper function to extract column values from raw tuple data.
         */
        auto extractColumnValues(const ID &table_id, const ID &column_id,
                                const std::vector<std::vector<uint8_t>> &sample_rows,
                                const std::vector<core::CatalogManager::ColumnInfo> &columns,
                                ErrorContext *ctx = nullptr) -> std::vector<std::vector<uint8_t>>;

        auto computeCorrelationStatistics(
            const ID &table_id,
            const std::vector<core::CatalogManager::ColumnInfo> &columns,
            const std::vector<std::vector<uint8_t>> &sample_rows,
            uint64_t analyzed_time,
            ErrorContext *ctx = nullptr) -> void;

        auto computeExpressionStatistics(
            const ID &table_id,
            const std::vector<core::CatalogManager::ColumnInfo> &columns,
            const std::vector<std::vector<uint8_t>> &sample_rows,
            uint64_t analyzed_time,
            ErrorContext *ctx = nullptr) -> void;
    };

} // namespace scratchbird::optimizer
