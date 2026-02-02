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
// P3-16: Telemetry Export
//
// Prometheus/OpenMetrics compatible metrics export system.
// Provides query latency histograms, transaction throughput, cache hit rates,
// lock wait times, and disk I/O statistics.
//
// November 25, 2025

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <memory>
#include <chrono>
#include <functional>

namespace scratchbird::core {

// Forward declarations
class Database;

// Metric types following Prometheus conventions
enum class MetricType : uint8_t {
    COUNTER = 0,      // Monotonically increasing value
    GAUGE = 1,        // Value that can go up and down
    HISTOGRAM = 2,    // Distribution of values
    SUMMARY = 3       // Similar to histogram with quantiles
};

// Label pair for metric dimensions
struct MetricLabel {
    std::string name;
    std::string value;
};

// Base metric class
class Metric {
public:
    Metric(const std::string& name, const std::string& help, MetricType type);
    virtual ~Metric() = default;

    const std::string& name() const { return name_; }
    const std::string& help() const { return help_; }
    MetricType type() const { return type_; }

    // Export to Prometheus text format
    virtual std::string toPrometheus() const = 0;

    // Export to OpenMetrics format
    virtual std::string toOpenMetrics() const = 0;

protected:
    std::string name_;
    std::string help_;
    MetricType type_;
    mutable std::mutex mutex_;

    static std::string escapeLabel(const std::string& str);
    static std::string formatLabels(const std::vector<MetricLabel>& labels);
};

// Counter metric - monotonically increasing
class Counter : public Metric {
public:
    Counter(const std::string& name, const std::string& help,
            const std::vector<std::string>& label_names = {});

    // Increment counter
    void inc(double value = 1.0, const std::vector<std::string>& label_values = {});

    // Get current value
    double get(const std::vector<std::string>& label_values = {}) const;

    std::string toPrometheus() const override;
    std::string toOpenMetrics() const override;

private:
    std::vector<std::string> label_names_;
    std::unordered_map<std::string, std::atomic<double>> values_;

    std::string makeKey(const std::vector<std::string>& label_values) const;
};

// Gauge metric - can increase or decrease
class Gauge : public Metric {
public:
    Gauge(const std::string& name, const std::string& help,
          const std::vector<std::string>& label_names = {});

    void set(double value, const std::vector<std::string>& label_values = {});
    void inc(double value = 1.0, const std::vector<std::string>& label_values = {});
    void dec(double value = 1.0, const std::vector<std::string>& label_values = {});
    double get(const std::vector<std::string>& label_values = {}) const;

    std::string toPrometheus() const override;
    std::string toOpenMetrics() const override;

private:
    std::vector<std::string> label_names_;
    std::unordered_map<std::string, std::atomic<double>> values_;

    std::string makeKey(const std::vector<std::string>& label_values) const;
};

// Histogram metric - distribution of values
class Histogram : public Metric {
public:
    // Default buckets for latency measurements (in seconds)
    static const std::vector<double> DEFAULT_LATENCY_BUCKETS;

    Histogram(const std::string& name, const std::string& help,
              const std::vector<double>& buckets = DEFAULT_LATENCY_BUCKETS,
              const std::vector<std::string>& label_names = {});

    // Observe a value
    void observe(double value, const std::vector<std::string>& label_values = {});

    // Get sum of all observed values
    double sum(const std::vector<std::string>& label_values = {}) const;

    // Get count of observations
    uint64_t count(const std::vector<std::string>& label_values = {}) const;

    std::string toPrometheus() const override;
    std::string toOpenMetrics() const override;

private:
    struct HistogramData {
        std::vector<std::atomic<uint64_t>> bucket_counts;
        std::atomic<double> sum{0.0};
        std::atomic<uint64_t> count{0};

        HistogramData(size_t num_buckets) : bucket_counts(num_buckets) {
            for (auto& bc : bucket_counts) bc = 0;
        }
    };

    std::vector<double> buckets_;
    std::vector<std::string> label_names_;
    mutable std::unordered_map<std::string, std::unique_ptr<HistogramData>> data_;

    std::string makeKey(const std::vector<std::string>& label_values) const;
    HistogramData* getOrCreate(const std::string& key);
};

// Timer helper for automatic histogram observation
class HistogramTimer {
public:
    HistogramTimer(Histogram* histogram, const std::vector<std::string>& label_values = {});
    ~HistogramTimer();

    // Get elapsed time in seconds
    double elapsed() const;

    // Cancel observation (destructor won't record)
    void cancel();

private:
    Histogram* histogram_;
    std::vector<std::string> label_values_;
    std::chrono::high_resolution_clock::time_point start_;
    bool cancelled_ = false;
};

// Metrics registry
class MetricsRegistry {
public:
    static MetricsRegistry& getInstance();

    // Register metrics
    Counter* registerCounter(const std::string& name, const std::string& help,
                            const std::vector<std::string>& label_names = {});
    Gauge* registerGauge(const std::string& name, const std::string& help,
                        const std::vector<std::string>& label_names = {});
    Histogram* registerHistogram(const std::string& name, const std::string& help,
                                 const std::vector<double>& buckets = Histogram::DEFAULT_LATENCY_BUCKETS,
                                 const std::vector<std::string>& label_names = {});

    // Get metric by name
    Metric* get(const std::string& name);

    // Export all metrics
    std::string exportPrometheus() const;
    std::string exportOpenMetrics() const;

    // Clear all metrics (for testing)
    void clear();

private:
    MetricsRegistry() = default;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<Metric>> metrics_;
};

// Pre-defined ScratchBird metrics
class ScratchBirdMetrics {
public:
    static ScratchBirdMetrics& getInstance();

    // Initialize all metrics
    void initialize();

    // Query metrics
    Histogram* query_duration_seconds;      // Query execution time
    Counter* queries_total;                  // Total queries executed
    Counter* query_errors_total;             // Query errors by type
    Counter* query_rows_returned_total;      // Rows returned by SELECT
    Counter* query_rows_affected_total;      // Rows affected by DML
    Gauge* query_currently_running;         // In-flight statements
    Gauge* query_progress_rows;             // Rows processed for current query
    Gauge* query_progress_bytes;            // Bytes processed for current query
    Gauge* query_progress_last_update_micros; // Last progress update timestamp

    // Transaction metrics
    Counter* transactions_total;             // Total transactions
    Counter* transactions_committed;         // Committed transactions
    Counter* transactions_rolled_back;       // Rolled back transactions
    Gauge* transactions_active;              // Currently active transactions

    // Buffer pool metrics
    Gauge* buffer_pool_size_bytes;          // Buffer pool size
    Gauge* buffer_pool_pages_total;         // Total pages in buffer pool
    Gauge* buffer_pool_pages_dirty;         // Dirty pages count
    Counter* buffer_pool_reads_total;       // Page reads from disk
    Counter* buffer_pool_writes_total;      // Page writes to disk
    Counter* buffer_pool_hits_total;        // Buffer pool hits
    Counter* buffer_pool_misses_total;      // Buffer pool misses

    // Lock metrics
    Histogram* lock_wait_seconds;           // Lock wait time
    Counter* lock_deadlocks_total;          // Deadlock count
    Gauge* locks_held;                      // Currently held locks

    // Index metrics
    Counter* index_scans_total;             // Index scans by type
    Counter* seq_scans_total;               // Sequential scans
    Histogram* index_scan_duration_seconds; // Index scan time

    // Disk I/O metrics
    Counter* disk_read_bytes_total;         // Bytes read from disk
    Counter* disk_write_bytes_total;        // Bytes written to disk
    Histogram* disk_read_latency_seconds;   // Disk read latency
    Histogram* disk_write_latency_seconds;  // Disk write latency

    // Connection metrics
    Gauge* connections_active;              // Active connections
    Gauge* connections_idle;                // Idle connections
    Counter* connections_total;             // Total connections established

    // Catalog metrics
    Gauge* tables_count;                    // Number of tables
    Gauge* indexes_count;                   // Number of indexes

    // TOAST metrics
    Counter* toast_reads_total;             // TOAST value reads
    Counter* toast_writes_total;            // TOAST value writes

    // COPY metrics
    Counter* copy_rows_total;               // Rows processed by COPY
    Counter* copy_bytes_total;              // Bytes processed by COPY
    Counter* copy_errors_total;             // COPY errors
    Histogram* copy_duration_seconds;       // COPY duration

    // Scheduler metrics
    Gauge* scheduler_queue_depth;          // Due jobs waiting
    Gauge* scheduler_jobs_running;         // Active job runs
    Counter* scheduler_jobs_failed_total;  // Failed job runs
    Histogram* scheduler_job_run_latency_seconds; // Job run duration

    // Cache metrics
    Counter* statement_cache_hits_total;      // Statement cache hits
    Counter* statement_cache_misses_total;    // Statement cache misses
    Counter* statement_cache_evictions_total; // Statement cache evictions
    Counter* result_cache_hits_total;         // Result cache hits
    Counter* result_cache_misses_total;       // Result cache misses
    Counter* result_cache_evictions_total;    // Result cache evictions
    Counter* translation_cache_hits_total;    // Translation cache hits
    Counter* translation_cache_misses_total;  // Translation cache misses
    Counter* translation_cache_evictions_total; // Translation cache evictions

private:
    ScratchBirdMetrics() = default;
    bool initialized_ = false;
};

// HTTP metrics endpoint handler (simple implementation)
class MetricsEndpoint {
public:
    // Handle HTTP request for metrics
    // Returns HTTP response body
    static std::string handleRequest(const std::string& path,
                                     const std::string& accept_header);

    // Get content type for response
    static std::string getContentType(bool openmetrics);
};

} // namespace scratchbird::core
