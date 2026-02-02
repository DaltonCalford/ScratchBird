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
// P3-18: Query Profiler Implementation
//
// November 25, 2025

#include "scratchbird/optimizer/query_profiler.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace scratchbird::optimizer {

// ============================================================================
// ProfileNode Implementation
// ============================================================================

ProfileNode::ProfileNode(const std::string& operator_name, const std::string& description)
    : name_(operator_name), description_(description) {
}

void ProfileNode::addChild(std::shared_ptr<ProfileNode> child) {
    children_.push_back(std::move(child));
}

void ProfileNode::startTiming() {
    start_time_ = std::chrono::high_resolution_clock::now();
    timing_active_ = true;
}

void ProfileNode::endTiming() {
    if (timing_active_) {
        auto end = std::chrono::high_resolution_clock::now();
        stats_.execution_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start_time_).count();
        timing_active_ = false;
    }
}

void ProfileNode::recordFirstRow() {
    if (!first_row_recorded_) {
        first_row_time_ = std::chrono::high_resolution_clock::now();
        stats_.startup_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
            first_row_time_ - start_time_).count();
        first_row_recorded_ = true;
    }
}

void ProfileNode::addRowsActual(uint64_t rows) {
    stats_.rows_actual += rows;
}

void ProfileNode::addRowsExamined(uint64_t rows) {
    stats_.rows_examined += rows;
}

void ProfileNode::addRowsFiltered(uint64_t rows) {
    stats_.rows_filtered += rows;
}

void ProfileNode::addMemoryUsed(size_t bytes) {
    stats_.memory_allocated_bytes += bytes;
    if (bytes > stats_.memory_used_bytes) {
        stats_.memory_used_bytes = bytes;
    }
}

void ProfileNode::addPageRead() {
    stats_.pages_read++;
}

void ProfileNode::addPageWritten() {
    stats_.pages_written++;
}

void ProfileNode::addBufferHit() {
    stats_.buffer_hits++;
}

void ProfileNode::addBufferMiss() {
    stats_.buffer_misses++;
}

void ProfileNode::addLoop() {
    stats_.loops++;
    stats_.avg_rows_per_loop = static_cast<double>(stats_.rows_actual) / stats_.loops;
}

void ProfileNode::setRowsPlanned(uint64_t rows) {
    stats_.rows_planned = rows;
}

void ProfileNode::setIndexInfo(const std::string& name, const std::string& condition) {
    stats_.index_name = name;
    stats_.index_condition = condition;
}

std::string ProfileNode::toString(int indent, bool verbose) const {
    std::ostringstream ss;
    std::string pad(indent * 2, ' ');

    // Operator name and description
    ss << pad << "-> " << name_;
    if (!description_.empty()) {
        ss << " (" << description_ << ")";
    }

    // Timing
    ss << " (actual time=" << std::fixed << std::setprecision(3)
       << (stats_.startup_time_us / 1000.0) << ".."
       << (stats_.execution_time_us / 1000.0) << " ms";

    // Rows
    ss << " rows=" << stats_.rows_actual;

    // Loops
    if (stats_.loops > 1) {
        ss << " loops=" << stats_.loops;
    }

    ss << ")";

    // Row estimation accuracy
    if (stats_.rows_planned > 0) {
        double accuracy = stats_.estimation_accuracy();
        if (accuracy < 0.5 || accuracy > 2.0) {
            ss << " [estimate " << stats_.rows_planned << ", off by "
               << std::fixed << std::setprecision(1) << (accuracy * 100) << "%]";
        }
    }

    ss << "\n";

    // Verbose output
    if (verbose) {
        if (stats_.rows_examined > stats_.rows_actual) {
            ss << pad << "     Rows Removed by Filter: "
               << (stats_.rows_examined - stats_.rows_actual) << "\n";
        }

        if (stats_.memory_used_bytes > 0) {
            ss << pad << "     Memory: " << (stats_.memory_used_bytes / 1024) << " KB";
            if (stats_.sort_spilled_to_disk) {
                ss << " (spilled to disk)";
            }
            ss << "\n";
        }

        if (stats_.pages_read > 0 || stats_.pages_written > 0) {
            ss << pad << "     I/O: " << stats_.pages_read << " pages read, "
               << stats_.pages_written << " pages written";
            if (stats_.buffer_hits + stats_.buffer_misses > 0) {
                ss << " (hit ratio: " << std::fixed << std::setprecision(1)
                   << (stats_.buffer_hit_ratio() * 100) << "%)";
            }
            ss << "\n";
        }

        if (!stats_.index_name.empty()) {
            ss << pad << "     Index: " << stats_.index_name;
            if (!stats_.index_condition.empty()) {
                ss << " Cond: " << stats_.index_condition;
            }
            ss << "\n";
        }
    }

    // Children
    for (const auto& child : children_) {
        ss << child->toString(indent + 1, verbose);
    }

    return ss.str();
}

std::string ProfileNode::toJson() const {
    std::ostringstream ss;
    ss << "{";
    ss << "\"operator\":\"" << name_ << "\"";
    if (!description_.empty()) {
        ss << ",\"description\":\"" << description_ << "\"";
    }

    // Stats
    ss << ",\"timing\":{";
    ss << "\"startup_ms\":" << std::fixed << std::setprecision(3) << (stats_.startup_time_us / 1000.0);
    ss << ",\"execution_ms\":" << (stats_.execution_time_us / 1000.0);
    ss << "}";

    ss << ",\"rows\":{";
    ss << "\"planned\":" << stats_.rows_planned;
    ss << ",\"actual\":" << stats_.rows_actual;
    ss << ",\"examined\":" << stats_.rows_examined;
    ss << "}";

    if (stats_.memory_used_bytes > 0) {
        ss << ",\"memory\":{";
        ss << "\"used_bytes\":" << stats_.memory_used_bytes;
        ss << ",\"spilled\":" << (stats_.sort_spilled_to_disk ? "true" : "false");
        ss << "}";
    }

    if (stats_.pages_read > 0 || stats_.buffer_hits > 0) {
        ss << ",\"io\":{";
        ss << "\"pages_read\":" << stats_.pages_read;
        ss << ",\"pages_written\":" << stats_.pages_written;
        ss << ",\"buffer_hits\":" << stats_.buffer_hits;
        ss << ",\"buffer_misses\":" << stats_.buffer_misses;
        ss << "}";
    }

    if (stats_.loops > 1) {
        ss << ",\"loops\":" << stats_.loops;
    }

    if (!stats_.index_name.empty()) {
        ss << ",\"index\":{";
        ss << "\"name\":\"" << stats_.index_name << "\"";
        if (!stats_.index_condition.empty()) {
            ss << ",\"condition\":\"" << stats_.index_condition << "\"";
        }
        ss << "}";
    }

    // Children
    if (!children_.empty()) {
        ss << ",\"children\":[";
        for (size_t i = 0; i < children_.size(); ++i) {
            if (i > 0) ss << ",";
            ss << children_[i]->toJson();
        }
        ss << "]";
    }

    ss << "}";
    return ss.str();
}

// ============================================================================
// QueryProfile Implementation
// ============================================================================

QueryProfile::QueryProfile() {
}

void QueryProfile::setQuery(const std::string& sql) {
    query_ = sql;
}

void QueryProfile::setPlanningTime(uint64_t us) {
    planning_time_us_ = us;
}

void QueryProfile::setExecutionTime(uint64_t us) {
    execution_time_us_ = us;
}

void QueryProfile::setRoot(std::shared_ptr<ProfileNode> root) {
    root_ = std::move(root);
}

void QueryProfile::addTriggerExecution(const std::string& trigger_name, uint64_t time_us, uint64_t rows) {
    trigger_stats_.push_back({trigger_name, time_us, rows});
}

void QueryProfile::addSubquery(const std::string& subquery_alias, std::shared_ptr<QueryProfile> subprofile) {
    subqueries_[subquery_alias] = std::move(subprofile);
}

uint64_t QueryProfile::sumStats(std::shared_ptr<ProfileNode> node,
                                 std::function<uint64_t(const OperatorStats&)> getter) const {
    if (!node) return 0;

    uint64_t total = getter(node->stats());
    for (const auto& child : node->children()) {
        total += sumStats(child, getter);
    }
    return total;
}

uint64_t QueryProfile::totalRows() const {
    return root_ ? root_->stats().rows_actual : 0;
}

uint64_t QueryProfile::totalPagesRead() const {
    return sumStats(root_, [](const OperatorStats& s) { return s.pages_read; });
}

uint64_t QueryProfile::totalMemoryUsed() const {
    return sumStats(root_, [](const OperatorStats& s) { return s.memory_used_bytes; });
}

std::string QueryProfile::toString(bool verbose) const {
    std::ostringstream ss;

    ss << "Query Profile\n";
    ss << "=============\n";

    // Query text (truncated if too long)
    if (!query_.empty()) {
        std::string sql = query_;
        if (sql.length() > 100) {
            sql = sql.substr(0, 97) + "...";
        }
        ss << "Query: " << sql << "\n";
    }

    // Timing summary
    ss << "Planning Time: " << std::fixed << std::setprecision(3)
       << (planning_time_us_ / 1000.0) << " ms\n";
    ss << "Execution Time: " << (execution_time_us_ / 1000.0) << " ms\n";

    // Plan tree
    if (root_) {
        ss << "\nPlan:\n";
        ss << root_->toString(0, verbose);
    }

    // Trigger stats
    if (!trigger_stats_.empty()) {
        ss << "\nTrigger Executions:\n";
        for (const auto& ts : trigger_stats_) {
            ss << "  " << ts.name << ": " << (ts.time_us / 1000.0) << " ms, "
               << ts.rows << " rows\n";
        }
    }

    // Summary stats
    if (verbose) {
        ss << "\nSummary:\n";
        ss << "  Total Rows: " << totalRows() << "\n";
        ss << "  Total Pages Read: " << totalPagesRead() << "\n";
        ss << "  Peak Memory: " << (totalMemoryUsed() / 1024) << " KB\n";
    }

    return ss.str();
}

std::string QueryProfile::toJson() const {
    std::ostringstream ss;
    ss << "{";

    // Query
    if (!query_.empty()) {
        ss << "\"query\":\"";
        for (char c : query_) {
            if (c == '"') ss << "\\\"";
            else if (c == '\\') ss << "\\\\";
            else if (c == '\n') ss << "\\n";
            else ss << c;
        }
        ss << "\"";
    }

    // Timing
    ss << ",\"planning_time_ms\":" << std::fixed << std::setprecision(3)
       << (planning_time_us_ / 1000.0);
    ss << ",\"execution_time_ms\":" << (execution_time_us_ / 1000.0);

    // Plan tree
    if (root_) {
        ss << ",\"plan\":" << root_->toJson();
    }

    // Triggers
    if (!trigger_stats_.empty()) {
        ss << ",\"triggers\":[";
        for (size_t i = 0; i < trigger_stats_.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "{\"name\":\"" << trigger_stats_[i].name << "\"";
            ss << ",\"time_ms\":" << (trigger_stats_[i].time_us / 1000.0);
            ss << ",\"rows\":" << trigger_stats_[i].rows << "}";
        }
        ss << "]";
    }

    // Summary
    ss << ",\"summary\":{";
    ss << "\"total_rows\":" << totalRows();
    ss << ",\"total_pages_read\":" << totalPagesRead();
    ss << ",\"peak_memory_bytes\":" << totalMemoryUsed();
    ss << "}";

    ss << "}";
    return ss.str();
}

std::string QueryProfile::toExplainAnalyze() const {
    std::ostringstream ss;

    // Header
    ss << "QUERY PLAN\n";
    ss << std::string(70, '-') << "\n";

    // Plan tree
    if (root_) {
        ss << root_->toString(0, true);
    }

    ss << std::string(70, '-') << "\n";

    // Footer
    ss << "Planning time: " << std::fixed << std::setprecision(3)
       << (planning_time_us_ / 1000.0) << " ms\n";
    ss << "Execution time: " << (execution_time_us_ / 1000.0) << " ms\n";

    return ss.str();
}

// ============================================================================
// QueryProfiler Implementation
// ============================================================================

QueryProfiler& QueryProfiler::getInstance() {
    static QueryProfiler instance;
    return instance;
}

std::shared_ptr<QueryProfile> QueryProfiler::beginProfile(const std::string& sql) {
    if (!enabled_) return nullptr;

    auto profile = std::make_shared<QueryProfile>();
    profile->setQuery(sql);
    return profile;
}

void QueryProfiler::endProfile(std::shared_ptr<QueryProfile> profile) {
    if (!profile) return;

    total_queries_++;
    total_execution_time_us_ += profile->executionTime();

    std::lock_guard<std::mutex> lock(mutex_);
    recent_profiles_.push_back(std::move(profile));

    // Trim if too many
    while (recent_profiles_.size() > MAX_STORED_PROFILES) {
        recent_profiles_.erase(recent_profiles_.begin());
    }
}

std::vector<std::shared_ptr<QueryProfile>> QueryProfiler::getRecentProfiles(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t n = std::min(count, recent_profiles_.size());
    std::vector<std::shared_ptr<QueryProfile>> result(
        recent_profiles_.end() - n, recent_profiles_.end());

    return result;
}

std::shared_ptr<QueryProfile> QueryProfiler::findProfile(const std::string& sql_substring) const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto it = recent_profiles_.rbegin(); it != recent_profiles_.rend(); ++it) {
        if ((*it)->query().find(sql_substring) != std::string::npos) {
            return *it;
        }
    }

    return nullptr;
}

void QueryProfiler::clearProfiles() {
    std::lock_guard<std::mutex> lock(mutex_);
    recent_profiles_.clear();
}

uint64_t QueryProfiler::avgExecutionTimeUs() const {
    uint64_t total = total_queries_.load();
    if (total == 0) return 0;
    return total_execution_time_us_.load() / total;
}

// ============================================================================
// ProfileTimer Implementation
// ============================================================================

ProfileTimer::ProfileTimer(ProfileNode* node) : node_(node) {
    if (node_) {
        node_->startTiming();
    }
}

ProfileTimer::~ProfileTimer() {
    if (node_) {
        node_->endTiming();
    }
}

void ProfileTimer::recordFirstRow() {
    if (node_) {
        node_->recordFirstRow();
    }
}

} // namespace scratchbird::optimizer
