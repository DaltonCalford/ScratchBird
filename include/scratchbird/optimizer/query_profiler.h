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
#include <string_view>
#include <vector>
#include <memory>
#include <chrono>
#include <deque>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <mutex>
#include <functional>

namespace scratchbird::optimizer {

// Forward declarations
class PlanNode;

enum class RegressionSeverity {
    NONE,
    MINOR,
    MAJOR
};

struct RegressionPolicy {
    bool enabled = false;
    double max_execution_time_ratio = 1.25;
    size_t min_baseline_samples = 3;
};

struct RegressionSignal {
    bool available = false;
    bool protected_workload = false;
    bool regression_detected = false;
    RegressionSeverity severity = RegressionSeverity::NONE;
    double execution_time_ratio = 1.0;
    double threshold_ratio = 1.0;
    uint64_t baseline_execution_time_us = 0;
    uint64_t current_execution_time_us = 0;
    size_t baseline_sample_count = 0;
};

struct CardinalityFeedbackPolicy {
    bool enabled = true;
    uint64_t min_observations = 1;
    double max_estimation_error_ratio = 4.0;
    uint64_t max_replan_actions = 2;
    uint64_t max_same_plan_replans = 1;
};

struct CardinalityFeedbackSignal {
    bool available = false;
    bool replan_required = false;
    bool replan_suppressed = false;
    bool stats_refresh_requested = false;
    bool stats_refresh_applied = false;
    uint64_t last_estimated_rows = 0;
    uint64_t last_actual_rows = 0;
    double estimation_error_ratio = 1.0;
    double correction_factor = 1.0;
    uint64_t observation_count = 0;
    uint64_t replan_action_count = 0;
    std::string last_plan_hash;
    std::string guardrail_reason;
};

struct QueryHistoryEntry {
    uint64_t sequence = 0;
    std::string query;
    std::string fingerprint;
    uint64_t planning_time_us = 0;
    uint64_t execution_time_us = 0;
    uint64_t total_rows = 0;
    uint64_t total_pages_read = 0;
    uint64_t peak_memory_bytes = 0;
    bool protected_workload = false;
    RegressionSignal regression;
};

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

    // Metadata
    void setFingerprint(std::string fingerprint);
    void setProtectedWorkload(bool value);
    void setRegressionSignal(const RegressionSignal& signal);

    // Get stats
    const std::string& query() const { return query_; }
    const std::string& fingerprint() const { return fingerprint_; }
    uint64_t planningTime() const { return planning_time_us_; }
    uint64_t executionTime() const { return execution_time_us_; }
    std::shared_ptr<ProfileNode> root() const { return root_; }
    bool protectedWorkload() const { return protected_workload_; }
    const RegressionSignal& regressionSignal() const { return regression_signal_; }

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
    std::string fingerprint_;
    bool protected_workload_ = false;
    RegressionSignal regression_signal_;

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

    // Fingerprinted query history and regression detection
    void setRegressionPolicy(const RegressionPolicy& policy);
    RegressionPolicy regressionPolicy() const;
    std::string fingerprintQuery(std::string_view sql) const;
    void markProtectedWorkload(std::string_view sql);
    void unmarkProtectedWorkload(std::string_view sql);
    void clearProtectedWorkloads();
    bool isProtectedWorkload(std::string_view sql) const;
    std::vector<QueryHistoryEntry> getHistoryForFingerprint(
        std::string_view fingerprint, size_t count = 10) const;
    std::vector<QueryHistoryEntry> getHistoryForQuery(
        std::string_view sql, size_t count = 10) const;
    std::optional<RegressionSignal> latestRegressionForFingerprint(
        std::string_view fingerprint) const;
    std::optional<RegressionSignal> latestRegressionForQuery(
        std::string_view sql) const;

    // Adaptive cardinality feedback and bounded replan signaling.
    void setCardinalityFeedbackPolicy(const CardinalityFeedbackPolicy& policy);
    CardinalityFeedbackPolicy cardinalityFeedbackPolicy() const;
    CardinalityFeedbackSignal recordCardinalityFeedback(
        std::string_view feedback_key,
        std::string_view plan_hash,
        uint64_t estimated_rows,
        uint64_t actual_rows);
    std::optional<CardinalityFeedbackSignal> latestCardinalityFeedback(
        std::string_view feedback_key) const;
    std::optional<CardinalityFeedbackSignal> acknowledgeCardinalityFeedback(
        std::string_view feedback_key,
        bool stats_refresh_applied);
    void clearCardinalityFeedback();

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
    std::unordered_map<std::string, std::deque<QueryHistoryEntry>> history_by_fingerprint_;
    std::unordered_set<std::string> protected_workloads_;
    RegressionPolicy regression_policy_;
    struct CardinalityFeedbackState
    {
        CardinalityFeedbackSignal signal;
        uint64_t last_consumed_observation = 0;
        std::unordered_map<std::string, uint64_t> replan_actions_by_plan_hash;
    };
    std::unordered_map<std::string, CardinalityFeedbackState> cardinality_feedback_;
    CardinalityFeedbackPolicy cardinality_feedback_policy_;
    uint64_t history_sequence_ = 0;
    static constexpr size_t MAX_STORED_PROFILES = 100;
    static constexpr size_t MAX_HISTORY_PER_FINGERPRINT = 32;

    std::atomic<uint64_t> total_queries_{0};
    std::atomic<uint64_t> total_execution_time_us_{0};

    RegressionSignal evaluateRegressionLocked(const std::string& fingerprint,
                                              uint64_t execution_time_us) const;
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
