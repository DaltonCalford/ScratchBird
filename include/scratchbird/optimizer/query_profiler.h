// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-18: Query Profiler
//
// EXPLAIN ANALYZE implementation with per-operator timing, row counts,
// memory usage, and I/O operations.
//
// November 25, 2025

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <functional>

namespace scratchbird::optimizer {

// Forward declarations
class PlanNode;

// Operator execution statistics
struct OperatorStats {
    // Timing
    uint64_t startup_time_us = 0;       // Time to first tuple
    uint64_t execution_time_us = 0;     // Total execution time
    uint64_t wait_time_us = 0;          // Time waiting on I/O or locks

    // Row counts
    uint64_t rows_planned = 0;          // Estimated rows (from planner)
    uint64_t rows_actual = 0;           // Actual rows returned
    uint64_t rows_examined = 0;         // Rows examined (before filtering)
    uint64_t rows_filtered = 0;         // Rows filtered out

    // Memory
    size_t memory_used_bytes = 0;       // Peak memory usage
    size_t memory_allocated_bytes = 0;  // Total memory allocated
    size_t sort_space_bytes = 0;        // Sort/hash space used
    bool sort_spilled_to_disk = false;  // True if sort spilled to disk

    // I/O
    uint64_t pages_read = 0;            // Pages read from disk
    uint64_t pages_written = 0;         // Pages written to disk
    uint64_t buffer_hits = 0;           // Buffer pool hits
    uint64_t buffer_misses = 0;         // Buffer pool misses

    // Loop stats (for nested loop joins)
    uint64_t loops = 0;                 // Number of outer loop iterations
    double avg_rows_per_loop = 0.0;     // Average rows per loop

    // Index stats
    std::string index_name;             // Index used (if any)
    std::string index_condition;        // Index condition
    uint64_t index_seeks = 0;           // Number of index seeks
    uint64_t index_scans = 0;           // Number of index scans

    // Calculate metrics
    double selectivity() const {
        return rows_examined > 0 ? static_cast<double>(rows_actual) / rows_examined : 1.0;
    }

    double estimation_accuracy() const {
        if (rows_planned == 0) return rows_actual == 0 ? 1.0 : 0.0;
        return static_cast<double>(rows_actual) / rows_planned;
    }

    double buffer_hit_ratio() const {
        uint64_t total = buffer_hits + buffer_misses;
        return total > 0 ? static_cast<double>(buffer_hits) / total : 1.0;
    }
};

// Profile node for operator tree
class ProfileNode {
public:
    ProfileNode(const std::string& operator_name, const std::string& description = "");

    // Add child node
    void addChild(std::shared_ptr<ProfileNode> child);

    // Start timing
    void startTiming();
    void endTiming();

    // Record startup (first row)
    void recordFirstRow();

    // Update stats
    void addRowsActual(uint64_t rows);
    void addRowsExamined(uint64_t rows);
    void addRowsFiltered(uint64_t rows);
    void addMemoryUsed(size_t bytes);
    void addPageRead();
    void addPageWritten();
    void addBufferHit();
    void addBufferMiss();
    void addLoop();

    // Set planned stats
    void setRowsPlanned(uint64_t rows);
    void setIndexInfo(const std::string& name, const std::string& condition);

    // Get stats
    const OperatorStats& stats() const { return stats_; }
    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }
    const std::vector<std::shared_ptr<ProfileNode>>& children() const { return children_; }

    // Format output
    std::string toString(int indent = 0, bool verbose = false) const;
    std::string toJson() const;

private:
    std::string name_;
    std::string description_;
    OperatorStats stats_;
    std::vector<std::shared_ptr<ProfileNode>> children_;

    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point first_row_time_;
    bool first_row_recorded_ = false;
    bool timing_active_ = false;
};

// Query profile containing full execution trace
class QueryProfile {
public:
    QueryProfile();

    // Set query info
    void setQuery(const std::string& sql);
    void setPlanningTime(uint64_t us);
    void setExecutionTime(uint64_t us);

    // Set root profile node
    void setRoot(std::shared_ptr<ProfileNode> root);

    // Add trigger execution stats
    void addTriggerExecution(const std::string& trigger_name, uint64_t time_us, uint64_t rows);

    // Add subquery stats
    void addSubquery(const std::string& subquery_alias, std::shared_ptr<QueryProfile> subprofile);

    // Get stats
    const std::string& query() const { return query_; }
    uint64_t planningTime() const { return planning_time_us_; }
    uint64_t executionTime() const { return execution_time_us_; }
    std::shared_ptr<ProfileNode> root() const { return root_; }

    // Calculate totals
    uint64_t totalRows() const;
    uint64_t totalPagesRead() const;
    uint64_t totalMemoryUsed() const;

    // Format output
    std::string toString(bool verbose = false) const;
    std::string toJson() const;

    // EXPLAIN ANALYZE output format
    std::string toExplainAnalyze() const;

private:
    std::string query_;
    uint64_t planning_time_us_ = 0;
    uint64_t execution_time_us_ = 0;
    std::shared_ptr<ProfileNode> root_;

    struct TriggerStats {
        std::string name;
        uint64_t time_us;
        uint64_t rows;
    };
    std::vector<TriggerStats> trigger_stats_;

    std::unordered_map<std::string, std::shared_ptr<QueryProfile>> subqueries_;

    uint64_t sumStats(std::shared_ptr<ProfileNode> node,
                      std::function<uint64_t(const OperatorStats&)> getter) const;
};

// Query profiler manager
class QueryProfiler {
public:
    static QueryProfiler& getInstance();

    // Enable/disable profiling
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

    // Set verbosity level (0-3)
    void setVerbosity(int level) { verbosity_ = level; }
    int verbosity() const { return verbosity_; }

    // Create new profile for query
    std::shared_ptr<QueryProfile> beginProfile(const std::string& sql);

    // End profiling and store result
    void endProfile(std::shared_ptr<QueryProfile> profile);

    // Get recent profiles
    std::vector<std::shared_ptr<QueryProfile>> getRecentProfiles(size_t count = 10) const;

    // Get profile by query text (for debugging)
    std::shared_ptr<QueryProfile> findProfile(const std::string& sql_substring) const;

    // Clear stored profiles
    void clearProfiles();

    // Statistics
    uint64_t totalQueriesProfiled() const { return total_queries_; }
    uint64_t avgExecutionTimeUs() const;

private:
    QueryProfiler() = default;

    bool enabled_ = false;
    int verbosity_ = 1;  // 0=minimal, 1=normal, 2=verbose, 3=debug

    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<QueryProfile>> recent_profiles_;
    static constexpr size_t MAX_STORED_PROFILES = 100;

    std::atomic<uint64_t> total_queries_{0};
    std::atomic<uint64_t> total_execution_time_us_{0};
};

// RAII timer for profiling nodes
class ProfileTimer {
public:
    ProfileTimer(ProfileNode* node);
    ~ProfileTimer();

    void recordFirstRow();

private:
    ProfileNode* node_;
};

// Helper macros for profiling
#define PROFILE_BEGIN(node) ProfileTimer _profile_timer_##__LINE__(node)
#define PROFILE_FIRST_ROW(node) node->recordFirstRow()

} // namespace scratchbird::optimizer
