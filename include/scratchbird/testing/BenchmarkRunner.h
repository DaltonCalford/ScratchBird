/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/*
 * ScratchBird Database Engine
 * Performance Benchmark Framework
 * Copyright (c) 2025 ScratchBird Project
 */

#ifndef SCRATCHBIRD_TESTING_BENCHMARKRUNNER_H
#define SCRATCHBIRD_TESTING_BENCHMARKRUNNER_H

#include "TestTypes.h"
#include <memory>
#include <vector>
#include <functional>

namespace scratchbird {
namespace testing {

//=============================================================================
// Benchmark Definition
//=============================================================================

struct Benchmark {
    std::string name;
    std::string description;
    TestCategory category = TestCategory::PERFORMANCE;

    // Benchmark function
    std::function<void(BenchmarkResult&)> run_function;

    // Configuration
    BenchmarkConfig config;

    // Targets
    double target_tps = 0;              // Target transactions per second
    double target_latency_p99_ms = 0;   // Target p99 latency
    double target_throughput_mbps = 0;  // Target throughput

    // Metadata
    std::vector<std::string> tags;
    std::map<std::string, std::string> metadata;
};

//=============================================================================
// Latency Histogram
//=============================================================================

class LatencyHistogram {
public:
    LatencyHistogram();
    ~LatencyHistogram();

    // Record a latency sample
    void record(double latency_us);
    void recordMs(double latency_ms);

    // Statistics
    double getMin() const;
    double getMax() const;
    double getMean() const;
    double getMedian() const;
    double getPercentile(double p) const;  // 0-100
    double getP50() const { return getPercentile(50); }
    double getP90() const { return getPercentile(90); }
    double getP95() const { return getPercentile(95); }
    double getP99() const { return getPercentile(99); }
    double getP999() const { return getPercentile(99.9); }
    double getStdDev() const;
    int64_t getCount() const;

    // Convert to LatencyStats
    LatencyStats toStats() const;

    // Merge with another histogram
    void merge(const LatencyHistogram& other);

    // Reset
    void reset();

    // Export
    std::string toCSV() const;
    std::string toJSON() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//=============================================================================
// Throughput Tracker
//=============================================================================

class ThroughputTracker {
public:
    ThroughputTracker();
    ~ThroughputTracker();

    // Record operations
    void recordOperation();
    void recordOperations(int64_t count);
    void recordBytes(int64_t bytes);
    void recordRows(int64_t rows);
    void recordTransaction();

    // Statistics
    double getOperationsPerSecond() const;
    double getBytesPerSecond() const;
    double getRowsPerSecond() const;
    double getTransactionsPerSecond() const;
    int64_t getTotalOperations() const;
    int64_t getTotalBytes() const;

    // Time window statistics
    double getRecentOpsPerSecond(int window_seconds = 10) const;

    // Convert to ThroughputStats
    ThroughputStats toStats() const;

    // Time series data
    std::vector<std::pair<double, double>> getTimeSeries() const;  // timestamp, ops/s

    // Reset
    void reset();

    // Export
    std::string toCSV() const;
    std::string toJSON() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//=============================================================================
// Resource Monitor
//=============================================================================

class ResourceMonitor {
public:
    ResourceMonitor();
    ~ResourceMonitor();

    // Start/stop monitoring
    void start(int sample_interval_ms = 100);
    void stop();
    bool isRunning() const;

    // Current values
    ResourceUsage getCurrentUsage() const;

    // Statistics over monitoring period
    ResourceUsage getPeakUsage() const;
    ResourceUsage getAverageUsage() const;

    // Time series data
    std::vector<std::pair<double, ResourceUsage>> getTimeSeries() const;

    // Export
    std::string toCSV() const;
    std::string toJSON() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//=============================================================================
// Benchmark Timer
//=============================================================================

class BenchmarkTimer {
public:
    BenchmarkTimer();

    void start();
    void stop();
    void reset();

    bool isRunning() const;

    // Elapsed time
    double elapsedMicroseconds() const;
    double elapsedMilliseconds() const;
    double elapsedSeconds() const;

    // Lap timing
    double lap();  // Returns lap time, resets internal lap timer
    std::vector<double> getLaps() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//=============================================================================
// Benchmark Runner
//=============================================================================

class BenchmarkRunner {
public:
    BenchmarkRunner();
    ~BenchmarkRunner();

    // Non-copyable
    BenchmarkRunner(const BenchmarkRunner&) = delete;
    BenchmarkRunner& operator=(const BenchmarkRunner&) = delete;

    //=========================================================================
    // Configuration
    //=========================================================================

    void setHost(const std::string& host);
    void setPort(int port);
    void setProtocol(Protocol protocol);
    void setDatabase(const std::string& database);
    void setUsername(const std::string& username);
    void setPassword(const std::string& password);

    // Benchmark parameters
    void setWarmupDuration(int seconds);
    void setCooldownDuration(int seconds);
    void setSampleInterval(int ms);
    void setIterations(int count);

    //=========================================================================
    // Benchmark Registration
    //=========================================================================

    void registerBenchmark(const Benchmark& benchmark);
    void registerBenchmarks(const std::vector<Benchmark>& benchmarks);
    void clearBenchmarks();

    std::vector<Benchmark> getRegisteredBenchmarks() const;

    //=========================================================================
    // Execution
    //=========================================================================

    // Run all registered benchmarks
    std::vector<BenchmarkResult> runAllBenchmarks();

    // Run specific benchmark
    BenchmarkResult runBenchmark(const std::string& name);
    BenchmarkResult runBenchmark(const Benchmark& benchmark);

    // Run benchmarks by tag
    std::vector<BenchmarkResult> runBenchmarksByTag(const std::string& tag);

    //=========================================================================
    // Standard Benchmarks - Connection
    //=========================================================================

    BenchmarkResult benchmarkConnectionLatency();
    BenchmarkResult benchmarkSSLConnectionLatency();
    BenchmarkResult benchmarkAuthenticationLatency();
    BenchmarkResult benchmarkPoolAcquireLatency();
    BenchmarkResult benchmarkMaxConnectionsPerSecond();

    //=========================================================================
    // Standard Benchmarks - Query
    //=========================================================================

    BenchmarkResult benchmarkPointSelect();
    BenchmarkResult benchmarkRangeSelect();
    BenchmarkResult benchmarkComplexJoin();
    BenchmarkResult benchmarkAggregate();
    BenchmarkResult benchmarkInsertSingle();
    BenchmarkResult benchmarkInsertBatch();
    BenchmarkResult benchmarkUpdateSingle();
    BenchmarkResult benchmarkDeleteSingle();

    //=========================================================================
    // Standard Benchmarks - Throughput
    //=========================================================================

    BenchmarkResult benchmarkTPCB(int scale_factor, int clients);
    BenchmarkResult benchmarkTPCC(int warehouses, int terminals);
    BenchmarkResult benchmarkBulkInsert();
    BenchmarkResult benchmarkBulkSelect();
    BenchmarkResult benchmarkMixedOLTP(int read_percent);

    //=========================================================================
    // Standard Benchmarks - Scalability
    //=========================================================================

    std::vector<BenchmarkResult> benchmarkConnectionScaling(int max_connections);
    std::vector<BenchmarkResult> benchmarkCPUScaling(int max_cores);
    std::vector<BenchmarkResult> benchmarkDataScaling(int64_t max_rows);
    std::vector<BenchmarkResult> benchmarkConcurrencyScaling(int max_concurrent);
    std::vector<BenchmarkResult> benchmarkBufferPoolScaling(int max_mb);

    //=========================================================================
    // Standard Benchmarks - Protocol Comparison
    //=========================================================================

    std::vector<BenchmarkResult> compareProtocolPointSelect();
    std::vector<BenchmarkResult> compareProtocolInsert();
    std::vector<BenchmarkResult> compareProtocolBulkCopy();

    //=========================================================================
    // Utilities
    //=========================================================================

    // Metric collection helpers
    LatencyHistogram& getLatencyHistogram();
    ThroughputTracker& getThroughputTracker();
    ResourceMonitor& getResourceMonitor();

    // Baseline comparison
    void setBaseline(const std::string& name, const BenchmarkResult& result);
    BenchmarkResult getBaseline(const std::string& name) const;
    double compareToBaseline(const std::string& name, const BenchmarkResult& result) const;

    //=========================================================================
    // Results and Reporting
    //=========================================================================

    std::vector<BenchmarkResult> getAllResults() const;

    // Summary
    std::string generateSummary() const;
    std::string generateTextReport() const;
    std::string generateJSON() const;
    std::string generateCSV() const;
    std::string generateMarkdown() const;

    // Export
    void exportResults(const std::string& path, const std::string& format = "json");
    void exportLatencyData(const std::string& path);
    void exportThroughputData(const std::string& path);
    void exportResourceData(const std::string& path);

    //=========================================================================
    // Callbacks
    //=========================================================================

    void setLogCallback(TestLogCallback callback);
    void setProgressCallback(TestProgressCallback callback);

    // Real-time metrics
    using MetricsCallback = std::function<void(const std::string& benchmark,
                                                double progress,
                                                double current_tps,
                                                double current_latency_ms)>;
    void setMetricsCallback(MetricsCallback callback);

    //=========================================================================
    // Error Handling
    //=========================================================================

    std::string getLastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void log(const std::string& level, const std::string& message);
    void setError(const std::string& error);
};

//=============================================================================
// Standard Benchmark Definitions
//=============================================================================

std::vector<Benchmark> getConnectionBenchmarks();
std::vector<Benchmark> getQueryBenchmarks();
std::vector<Benchmark> getThroughputBenchmarks();
std::vector<Benchmark> getScalabilityBenchmarks();

// Get all standard benchmarks
std::vector<Benchmark> getAllStandardBenchmarks();

//=============================================================================
// Benchmark Targets (from Alpha 3 spec)
//=============================================================================

struct BenchmarkTargets {
    // Connection performance
    static constexpr double CONNECTION_LATENCY_LOCAL_MS = 5.0;
    static constexpr double SSL_CONNECTION_LATENCY_MS = 20.0;
    static constexpr double AUTH_LATENCY_MS = 2.0;
    static constexpr double POOL_ACQUIRE_LATENCY_MS = 1.0;
    static constexpr double MAX_CONNECTIONS_PER_SECOND = 1000.0;

    // Query performance
    static constexpr double POINT_SELECT_LATENCY_MS = 0.5;
    static constexpr double RANGE_SELECT_LATENCY_MS = 2.0;
    static constexpr double COMPLEX_JOIN_LATENCY_MS = 50.0;
    static constexpr double AGGREGATE_LATENCY_MS = 100.0;
    static constexpr double INSERT_SINGLE_LATENCY_MS = 1.0;
    static constexpr double INSERT_BATCH_1000_LATENCY_MS = 50.0;
    static constexpr double UPDATE_SINGLE_LATENCY_MS = 1.0;
    static constexpr double DELETE_SINGLE_LATENCY_MS = 1.0;

    // Throughput
    static constexpr double TPC_B_TPS = 10000.0;
    static constexpr double TPC_C_TPS = 5000.0;
    static constexpr double BULK_INSERT_ROWS_PER_SEC = 100000.0;
    static constexpr double BULK_SELECT_ROWS_PER_SEC = 500000.0;
    static constexpr double MIXED_OLTP_TPS = 15000.0;

    // Latency percentiles
    static constexpr double POINT_SELECT_P50_MS = 0.3;
    static constexpr double POINT_SELECT_P90_MS = 0.5;
    static constexpr double POINT_SELECT_P99_MS = 1.0;
    static constexpr double POINT_SELECT_P999_MS = 5.0;

    static constexpr double RANGE_SELECT_P50_MS = 1.0;
    static constexpr double RANGE_SELECT_P90_MS = 2.0;
    static constexpr double RANGE_SELECT_P99_MS = 5.0;
    static constexpr double RANGE_SELECT_P999_MS = 20.0;

    static constexpr double INSERT_P50_MS = 0.5;
    static constexpr double INSERT_P90_MS = 1.0;
    static constexpr double INSERT_P99_MS = 2.0;
    static constexpr double INSERT_P999_MS = 10.0;

    static constexpr double TRANSACTION_3OP_P50_MS = 2.0;
    static constexpr double TRANSACTION_3OP_P90_MS = 5.0;
    static constexpr double TRANSACTION_3OP_P99_MS = 10.0;
    static constexpr double TRANSACTION_3OP_P999_MS = 50.0;
};

} // namespace testing
} // namespace scratchbird

#endif // SCRATCHBIRD_TESTING_BENCHMARKRUNNER_H
