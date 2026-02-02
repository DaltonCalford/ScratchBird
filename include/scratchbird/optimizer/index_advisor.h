/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P2-25: Index Advisor
//
// Analyzes query patterns and workloads to:
// - Suggest missing indexes that would improve performance
// - Identify unused indexes that could be dropped
// - Estimate cost/benefit of index changes
// - Track query patterns over time
//
// November 25, 2025

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/optimizer/statistics_manager.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <chrono>
#include <atomic>

// Forward declarations
namespace scratchbird::core {
    class Database;
    class CatalogManager;
}

namespace scratchbird::optimizer {

using core::ID;
using core::UuidV7Bytes;

// Index recommendation type
enum class IndexRecommendationType : uint8_t {
    CREATE_BTREE = 0,           // Create B-tree index
    CREATE_HASH = 1,            // Create hash index
    CREATE_LSM = 2,             // Create LSM-tree index
    CREATE_COMPOSITE = 3,       // Create composite (multi-column) index
    CREATE_PARTIAL = 4,         // Create partial index with WHERE clause
    DROP_UNUSED = 5,            // Drop unused index
    REINDEX = 6                 // Rebuild fragmented index
};

// Column usage pattern in queries
struct ColumnUsage {
    ID column_id;
    std::string column_name;
    uint64_t equality_count = 0;    // col = value
    uint64_t range_count = 0;       // col > value, col < value, BETWEEN
    uint64_t like_count = 0;        // col LIKE pattern
    uint64_t in_list_count = 0;     // col IN (values)
    uint64_t join_count = 0;        // Used in JOIN condition
    uint64_t order_by_count = 0;    // Used in ORDER BY
    uint64_t group_by_count = 0;    // Used in GROUP BY
    uint64_t select_count = 0;      // Used in SELECT list (covering index candidate)
    uint64_t total_queries = 0;     // Total queries using this column

    double getScore() const {
        // Weight different usages by their index benefit
        return equality_count * 10.0 +
               range_count * 8.0 +
               like_count * 5.0 +
               in_list_count * 7.0 +
               join_count * 12.0 +
               order_by_count * 6.0 +
               group_by_count * 4.0 +
               select_count * 1.0;
    }
};

// Table usage statistics
struct TableUsageStats {
    ID table_id;
    std::string table_name;
    uint64_t total_queries = 0;
    uint64_t seq_scans = 0;         // Full table scans
    uint64_t index_scans = 0;       // Index scans
    uint64_t rows_fetched = 0;      // Estimated rows fetched
    std::chrono::steady_clock::time_point last_query;
    std::unordered_map<ID, ColumnUsage, core::IDHash> columns;
};

// Index usage tracking
struct IndexUsageStats {
    ID index_id;
    ID table_id;
    std::string index_name;
    std::string table_name;
    uint64_t scan_count = 0;        // Times index was used
    uint64_t tuple_reads = 0;       // Tuples read via index
    uint64_t size_pages = 0;        // Index size in pages
    uint64_t maintenance_cost = 0;  // Write operations affecting index
    std::chrono::steady_clock::time_point last_used;
    std::chrono::steady_clock::time_point created_at;
    bool is_primary_key = false;
    bool is_unique = false;
    bool is_foreign_key = false;
};

// Index recommendation
struct IndexRecommendation {
    ID table_id;
    std::string table_name;
    std::vector<std::string> column_names;
    IndexRecommendationType type;
    std::string where_clause;       // For partial indexes
    std::string create_sql;         // Generated CREATE INDEX statement
    std::string drop_sql;           // Generated DROP INDEX statement (for DROP_UNUSED)
    std::string index_name;         // Existing or suggested index name

    // Cost-benefit analysis
    double benefit_score = 0.0;     // Estimated performance improvement
    double cost_score = 0.0;        // Estimated maintenance cost
    double net_benefit = 0.0;       // benefit - cost
    uint64_t affected_queries = 0;  // Number of queries that would benefit
    double estimated_size_mb = 0.0; // Estimated index size in MB
    double estimated_speedup = 0.0; // Estimated query speedup factor

    // Priority and confidence
    double priority = 0.0;          // Overall recommendation priority (0.0-100.0)
    double confidence = 0.0;        // Confidence in recommendation (0.0-1.0)
    std::string reason;             // Human-readable explanation
};

// Query pattern for analysis
struct QueryPattern {
    std::string sql_text;           // Original SQL (truncated/anonymized)
    uint64_t execution_count = 0;   // Times executed
    double total_time_ms = 0.0;     // Total execution time
    double avg_time_ms = 0.0;       // Average execution time
    uint64_t rows_examined = 0;     // Total rows examined
    uint64_t rows_returned = 0;     // Total rows returned
    bool used_index = false;        // Whether any index was used
    ID table_id;                    // Primary table
    std::vector<ID> column_ids;     // Columns in WHERE/JOIN
    std::chrono::steady_clock::time_point first_seen;
    std::chrono::steady_clock::time_point last_seen;
};

// Index advisor configuration
struct IndexAdvisorConfig {
    uint64_t max_recommendations = 20;      // Max recommendations to generate
    double min_benefit_score = 10.0;        // Minimum benefit score to recommend
    double min_confidence = 0.5;            // Minimum confidence (0.0-1.0)
    uint64_t unused_threshold_days = 30;    // Days before index considered unused
    uint64_t min_query_count = 5;           // Minimum queries before recommending
    double max_index_size_mb = 1024.0;      // Max recommended index size
    bool include_partial_indexes = true;    // Consider partial indexes
    bool include_covering_indexes = false;  // Consider covering indexes
    bool track_query_patterns = true;       // Track query patterns
    uint64_t pattern_history_size = 1000;   // Max query patterns to track
};

// Index advisor class
class IndexAdvisor {
public:
    explicit IndexAdvisor(core::Database* db, const IndexAdvisorConfig& config = {});
    ~IndexAdvisor();

    // Disable copy
    IndexAdvisor(const IndexAdvisor&) = delete;
    IndexAdvisor& operator=(const IndexAdvisor&) = delete;

    // === Query Pattern Recording ===

    // Record a query execution (called after query completes)
    void recordQuery(const std::string& sql_text, const ID& table_id,
                    const std::vector<ID>& column_ids, bool used_index,
                    double execution_time_ms, uint64_t rows_examined,
                    uint64_t rows_returned);

    // Record column usage in predicate
    void recordColumnUsage(const ID& table_id, const ID& column_id,
                          const std::string& usage_type);

    // Record index usage
    void recordIndexUsage(const ID& index_id, uint64_t tuples_read);

    // Record index maintenance (INSERT/UPDATE/DELETE)
    void recordIndexMaintenance(const ID& index_id);

    // === Analysis and Recommendations ===

    // Generate index recommendations
    core::Status analyze(std::vector<IndexRecommendation>* recommendations,
                        core::ErrorContext* ctx = nullptr);

    // Analyze specific table
    core::Status analyzeTable(const ID& table_id,
                             std::vector<IndexRecommendation>* recommendations,
                             core::ErrorContext* ctx = nullptr);

    // Find unused indexes
    core::Status findUnusedIndexes(std::vector<IndexRecommendation>* recommendations,
                                   core::ErrorContext* ctx = nullptr);

    // Suggest indexes for specific query
    core::Status suggestIndexesForQuery(const std::string& sql_text,
                                        std::vector<IndexRecommendation>* recommendations,
                                        core::ErrorContext* ctx = nullptr);

    // === Reporting ===

    // Get table usage statistics
    core::Status getTableStats(const ID& table_id, TableUsageStats* stats,
                              core::ErrorContext* ctx = nullptr);

    // Get all table statistics
    core::Status getAllTableStats(std::vector<TableUsageStats>* stats,
                                 core::ErrorContext* ctx = nullptr);

    // Get index usage statistics
    core::Status getIndexStats(const ID& index_id, IndexUsageStats* stats,
                              core::ErrorContext* ctx = nullptr);

    // Get all index statistics
    core::Status getAllIndexStats(std::vector<IndexUsageStats>* stats,
                                 core::ErrorContext* ctx = nullptr);

    // Get query patterns
    core::Status getQueryPatterns(std::vector<QueryPattern>* patterns,
                                 core::ErrorContext* ctx = nullptr);

    // Get top slow queries without indexes
    core::Status getTopSlowQueries(uint64_t limit, std::vector<QueryPattern>* patterns,
                                  core::ErrorContext* ctx = nullptr);

    // === Configuration ===

    // Update configuration
    void setConfig(const IndexAdvisorConfig& config);
    const IndexAdvisorConfig& getConfig() const { return config_; }

    // Reset statistics
    void resetStats();

    // === Workload Analysis ===

    // Start workload capture
    void startWorkloadCapture();

    // Stop workload capture
    void stopWorkloadCapture();

    // Is capturing workload?
    bool isCapturing() const { return capturing_.load(); }

    // Get capture duration
    std::chrono::seconds getCaptureDuration() const;

private:
    core::Database* db_;
    core::CatalogManager* catalog_;
    std::unique_ptr<CostModel> cost_model_;
    IndexAdvisorConfig config_;

    // Statistics storage
    std::unordered_map<ID, TableUsageStats, core::IDHash> table_stats_;
    std::unordered_map<ID, IndexUsageStats, core::IDHash> index_stats_;
    std::vector<QueryPattern> query_patterns_;
    mutable std::mutex stats_mutex_;

    // Workload capture state
    std::atomic<bool> capturing_{false};
    std::chrono::steady_clock::time_point capture_start_;

    // Internal helpers
    void initializeIndexStats();
    IndexRecommendation generateCreateRecommendation(
        const TableUsageStats& table_stats, const ColumnUsage& col_usage);
    IndexRecommendation generateCompositeRecommendation(
        const TableUsageStats& table_stats, const std::vector<const ColumnUsage*>& columns);
    IndexRecommendation generateDropRecommendation(const IndexUsageStats& index_stats);

    double estimateIndexBenefit(const TableUsageStats& table_stats,
                               const ColumnUsage& col_usage);
    double estimateIndexCost(const TableUsageStats& table_stats,
                            const ColumnUsage& col_usage);
    double estimateIndexSize(const ID& table_id, const std::vector<ID>& column_ids);

    std::string generateIndexName(const std::string& table_name,
                                 const std::vector<std::string>& column_names,
                                 IndexRecommendationType type);
    std::string generateCreateSQL(const IndexRecommendation& rec);
    std::string generateDropSQL(const std::string& index_name);

    bool isIndexExisting(const ID& table_id, const std::vector<ID>& column_ids);
    bool isIndexUsed(const ID& index_id);
    bool canBenefitFromIndex(const ColumnUsage& col_usage);

    void trimQueryPatterns();
};

} // namespace scratchbird::optimizer
