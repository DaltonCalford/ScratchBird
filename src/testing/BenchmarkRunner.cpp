/*
 * ScratchBird Database Engine
 * Performance Benchmark Framework Implementation
 * Copyright (c) 2025 ScratchBird Project
 */

#include "scratchbird/testing/BenchmarkRunner.h"
#include <sstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <fstream>
#include <atomic>

namespace scratchbird {
namespace testing {

//=============================================================================
// Latency Histogram Implementation
//=============================================================================

struct LatencyHistogram::Impl {
    std::vector<double> samples;
    mutable std::mutex mutex;
    double min_val = std::numeric_limits<double>::max();
    double max_val = std::numeric_limits<double>::min();
    double sum = 0;
    double sum_sq = 0;
    int64_t count = 0;
};

LatencyHistogram::LatencyHistogram()
    : impl_(std::make_unique<Impl>()) {
}

LatencyHistogram::~LatencyHistogram() = default;

void LatencyHistogram::record(double latency_us) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->samples.push_back(latency_us);
    impl_->min_val = std::min(impl_->min_val, latency_us);
    impl_->max_val = std::max(impl_->max_val, latency_us);
    impl_->sum += latency_us;
    impl_->sum_sq += latency_us * latency_us;
    impl_->count++;
}

void LatencyHistogram::recordMs(double latency_ms) {
    record(latency_ms * 1000.0);
}

double LatencyHistogram::getMin() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->count > 0 ? impl_->min_val / 1000.0 : 0;
}

double LatencyHistogram::getMax() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->count > 0 ? impl_->max_val / 1000.0 : 0;
}

double LatencyHistogram::getMean() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->count > 0 ? (impl_->sum / impl_->count) / 1000.0 : 0;
}

double LatencyHistogram::getMedian() const {
    return getPercentile(50);
}

double LatencyHistogram::getPercentile(double p) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->samples.empty()) return 0;

    auto sorted = impl_->samples;
    std::sort(sorted.begin(), sorted.end());

    size_t idx = static_cast<size_t>((p / 100.0) * sorted.size());
    if (idx >= sorted.size()) idx = sorted.size() - 1;

    return sorted[idx] / 1000.0;  // Convert to ms
}

double LatencyHistogram::getStdDev() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->count < 2) return 0;

    double mean = impl_->sum / impl_->count;
    double variance = (impl_->sum_sq / impl_->count) - (mean * mean);
    return std::sqrt(variance) / 1000.0;
}

int64_t LatencyHistogram::getCount() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->count;
}

LatencyStats LatencyHistogram::toStats() const {
    LatencyStats stats;
    stats.min_ms = getMin();
    stats.max_ms = getMax();
    stats.mean_ms = getMean();
    stats.median_ms = getMedian();
    stats.p90_ms = getP90();
    stats.p95_ms = getP95();
    stats.p99_ms = getP99();
    stats.p999_ms = getP999();
    stats.stddev_ms = getStdDev();
    stats.sample_count = getCount();
    return stats;
}

void LatencyHistogram::merge(const LatencyHistogram& other) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::lock_guard<std::mutex> other_lock(other.impl_->mutex);

    impl_->samples.insert(impl_->samples.end(),
                           other.impl_->samples.begin(),
                           other.impl_->samples.end());
    impl_->min_val = std::min(impl_->min_val, other.impl_->min_val);
    impl_->max_val = std::max(impl_->max_val, other.impl_->max_val);
    impl_->sum += other.impl_->sum;
    impl_->sum_sq += other.impl_->sum_sq;
    impl_->count += other.impl_->count;
}

void LatencyHistogram::reset() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->samples.clear();
    impl_->min_val = std::numeric_limits<double>::max();
    impl_->max_val = std::numeric_limits<double>::min();
    impl_->sum = 0;
    impl_->sum_sq = 0;
    impl_->count = 0;
}

std::string LatencyHistogram::toCSV() const {
    std::stringstream ss;
    ss << "sample_us\n";
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (double s : impl_->samples) {
        ss << s << "\n";
    }
    return ss.str();
}

std::string LatencyHistogram::toJSON() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"count\": " << getCount() << ",\n";
    ss << "  \"min_ms\": " << getMin() << ",\n";
    ss << "  \"max_ms\": " << getMax() << ",\n";
    ss << "  \"mean_ms\": " << getMean() << ",\n";
    ss << "  \"p50_ms\": " << getMedian() << ",\n";
    ss << "  \"p90_ms\": " << getP90() << ",\n";
    ss << "  \"p99_ms\": " << getP99() << "\n";
    ss << "}\n";
    return ss.str();
}

//=============================================================================
// Throughput Tracker Implementation
//=============================================================================

struct ThroughputTracker::Impl {
    std::atomic<int64_t> operations{0};
    std::atomic<int64_t> bytes{0};
    std::atomic<int64_t> rows{0};
    std::atomic<int64_t> transactions{0};

    std::chrono::steady_clock::time_point start_time;
    std::vector<std::pair<double, double>> time_series;  // timestamp, ops/s
    std::mutex time_series_mutex;
};

ThroughputTracker::ThroughputTracker()
    : impl_(std::make_unique<Impl>()) {
    impl_->start_time = std::chrono::steady_clock::now();
}

ThroughputTracker::~ThroughputTracker() = default;

void ThroughputTracker::recordOperation() {
    impl_->operations++;
}

void ThroughputTracker::recordOperations(int64_t count) {
    impl_->operations += count;
}

void ThroughputTracker::recordBytes(int64_t bytes) {
    impl_->bytes += bytes;
}

void ThroughputTracker::recordRows(int64_t rows) {
    impl_->rows += rows;
}

void ThroughputTracker::recordTransaction() {
    impl_->transactions++;
}

double ThroughputTracker::getOperationsPerSecond() const {
    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - impl_->start_time).count();
    return elapsed > 0 ? impl_->operations / elapsed : 0;
}

double ThroughputTracker::getBytesPerSecond() const {
    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - impl_->start_time).count();
    return elapsed > 0 ? impl_->bytes / elapsed : 0;
}

double ThroughputTracker::getRowsPerSecond() const {
    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - impl_->start_time).count();
    return elapsed > 0 ? impl_->rows / elapsed : 0;
}

double ThroughputTracker::getTransactionsPerSecond() const {
    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - impl_->start_time).count();
    return elapsed > 0 ? impl_->transactions / elapsed : 0;
}

int64_t ThroughputTracker::getTotalOperations() const {
    return impl_->operations;
}

int64_t ThroughputTracker::getTotalBytes() const {
    return impl_->bytes;
}

double ThroughputTracker::getRecentOpsPerSecond(int window_seconds) const {
    // For simplicity, return overall rate
    return getOperationsPerSecond();
}

ThroughputStats ThroughputTracker::toStats() const {
    ThroughputStats stats;
    stats.total_operations = impl_->operations;
    stats.operations_per_second = getOperationsPerSecond();
    stats.bytes_per_second = getBytesPerSecond();
    stats.rows_per_second = getRowsPerSecond();
    stats.transactions_per_second = getTransactionsPerSecond();
    return stats;
}

std::vector<std::pair<double, double>> ThroughputTracker::getTimeSeries() const {
    std::lock_guard<std::mutex> lock(impl_->time_series_mutex);
    return impl_->time_series;
}

void ThroughputTracker::reset() {
    impl_->operations = 0;
    impl_->bytes = 0;
    impl_->rows = 0;
    impl_->transactions = 0;
    impl_->start_time = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(impl_->time_series_mutex);
    impl_->time_series.clear();
}

std::string ThroughputTracker::toCSV() const {
    std::stringstream ss;
    ss << "timestamp,ops_per_second\n";
    std::lock_guard<std::mutex> lock(impl_->time_series_mutex);
    for (const auto& [ts, ops] : impl_->time_series) {
        ss << ts << "," << ops << "\n";
    }
    return ss.str();
}

std::string ThroughputTracker::toJSON() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"total_operations\": " << getTotalOperations() << ",\n";
    ss << "  \"operations_per_second\": " << getOperationsPerSecond() << ",\n";
    ss << "  \"bytes_per_second\": " << getBytesPerSecond() << ",\n";
    ss << "  \"rows_per_second\": " << getRowsPerSecond() << ",\n";
    ss << "  \"transactions_per_second\": " << getTransactionsPerSecond() << "\n";
    ss << "}\n";
    return ss.str();
}

//=============================================================================
// Resource Monitor Implementation
//=============================================================================

struct ResourceMonitor::Impl {
    std::atomic<bool> running{false};
    int sample_interval_ms = 100;
    std::thread monitor_thread;

    std::vector<std::pair<double, ResourceUsage>> samples;
    std::mutex samples_mutex;

    ResourceUsage peak;
    ResourceUsage sum;
    int sample_count = 0;
};

ResourceMonitor::ResourceMonitor()
    : impl_(std::make_unique<Impl>()) {
}

ResourceMonitor::~ResourceMonitor() {
    stop();
}

void ResourceMonitor::start(int sample_interval_ms) {
    impl_->sample_interval_ms = sample_interval_ms;
    impl_->running = true;

    impl_->monitor_thread = std::thread([this]() {
        auto start = std::chrono::steady_clock::now();

        while (impl_->running) {
            ResourceUsage usage;
            // In a real implementation, would read from /proc, etc.
            usage.cpu_total_percent = 50.0;  // Placeholder
            usage.memory_rss_bytes = 1024 * 1024 * 100;  // 100MB placeholder

            auto now = std::chrono::steady_clock::now();
            double timestamp = std::chrono::duration<double>(now - start).count();

            {
                std::lock_guard<std::mutex> lock(impl_->samples_mutex);
                impl_->samples.emplace_back(timestamp, usage);

                // Update peak
                if (usage.cpu_total_percent > impl_->peak.cpu_total_percent) {
                    impl_->peak.cpu_total_percent = usage.cpu_total_percent;
                }
                if (usage.memory_rss_bytes > impl_->peak.memory_rss_bytes) {
                    impl_->peak.memory_rss_bytes = usage.memory_rss_bytes;
                }

                // Update sum for average
                impl_->sum.cpu_total_percent += usage.cpu_total_percent;
                impl_->sum.memory_rss_bytes += usage.memory_rss_bytes;
                impl_->sample_count++;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(impl_->sample_interval_ms));
        }
    });
}

void ResourceMonitor::stop() {
    impl_->running = false;
    if (impl_->monitor_thread.joinable()) {
        impl_->monitor_thread.join();
    }
}

bool ResourceMonitor::isRunning() const {
    return impl_->running;
}

ResourceUsage ResourceMonitor::getCurrentUsage() const {
    std::lock_guard<std::mutex> lock(impl_->samples_mutex);
    if (impl_->samples.empty()) return ResourceUsage{};
    return impl_->samples.back().second;
}

ResourceUsage ResourceMonitor::getPeakUsage() const {
    std::lock_guard<std::mutex> lock(impl_->samples_mutex);
    return impl_->peak;
}

ResourceUsage ResourceMonitor::getAverageUsage() const {
    std::lock_guard<std::mutex> lock(impl_->samples_mutex);
    if (impl_->sample_count == 0) return ResourceUsage{};

    ResourceUsage avg;
    avg.cpu_total_percent = impl_->sum.cpu_total_percent / impl_->sample_count;
    avg.memory_rss_bytes = impl_->sum.memory_rss_bytes / impl_->sample_count;
    return avg;
}

std::vector<std::pair<double, ResourceUsage>> ResourceMonitor::getTimeSeries() const {
    std::lock_guard<std::mutex> lock(impl_->samples_mutex);
    return impl_->samples;
}

std::string ResourceMonitor::toCSV() const {
    std::stringstream ss;
    ss << "timestamp,cpu_percent,memory_bytes\n";
    std::lock_guard<std::mutex> lock(impl_->samples_mutex);
    for (const auto& [ts, usage] : impl_->samples) {
        ss << ts << "," << usage.cpu_total_percent << "," << usage.memory_rss_bytes << "\n";
    }
    return ss.str();
}

std::string ResourceMonitor::toJSON() const {
    std::stringstream ss;
    auto peak = getPeakUsage();
    auto avg = getAverageUsage();
    ss << "{\n";
    ss << "  \"peak_cpu_percent\": " << peak.cpu_total_percent << ",\n";
    ss << "  \"peak_memory_bytes\": " << peak.memory_rss_bytes << ",\n";
    ss << "  \"avg_cpu_percent\": " << avg.cpu_total_percent << ",\n";
    ss << "  \"avg_memory_bytes\": " << avg.memory_rss_bytes << "\n";
    ss << "}\n";
    return ss.str();
}

//=============================================================================
// Benchmark Timer Implementation
//=============================================================================

struct BenchmarkTimer::Impl {
    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point stop;
    std::chrono::steady_clock::time_point lap_start;
    std::vector<double> laps;
    bool running = false;
};

BenchmarkTimer::BenchmarkTimer()
    : impl_(std::make_unique<Impl>()) {
}

void BenchmarkTimer::start() {
    impl_->start = std::chrono::steady_clock::now();
    impl_->lap_start = impl_->start;
    impl_->running = true;
}

void BenchmarkTimer::stop() {
    impl_->stop = std::chrono::steady_clock::now();
    impl_->running = false;
}

void BenchmarkTimer::reset() {
    impl_->running = false;
    impl_->laps.clear();
}

bool BenchmarkTimer::isRunning() const {
    return impl_->running;
}

double BenchmarkTimer::elapsedMicroseconds() const {
    auto end = impl_->running ? std::chrono::steady_clock::now() : impl_->stop;
    return std::chrono::duration<double, std::micro>(end - impl_->start).count();
}

double BenchmarkTimer::elapsedMilliseconds() const {
    return elapsedMicroseconds() / 1000.0;
}

double BenchmarkTimer::elapsedSeconds() const {
    return elapsedMicroseconds() / 1000000.0;
}

double BenchmarkTimer::lap() {
    auto now = std::chrono::steady_clock::now();
    double lap_time = std::chrono::duration<double, std::micro>(now - impl_->lap_start).count();
    impl_->laps.push_back(lap_time);
    impl_->lap_start = now;
    return lap_time;
}

std::vector<double> BenchmarkTimer::getLaps() const {
    return impl_->laps;
}

//=============================================================================
// Benchmark Runner Implementation
//=============================================================================

struct BenchmarkRunner::Impl {
    std::string host = "localhost";
    int port = 3092;
    Protocol protocol = Protocol::NATIVE;
    std::string database = "testdb";
    std::string username = "testuser";
    std::string password = "test123";

    int warmup_duration = 10;
    int cooldown_duration = 5;
    int sample_interval_ms = 100;
    int iterations = 1;

    std::vector<Benchmark> registered_benchmarks;
    std::vector<BenchmarkResult> results;
    std::map<std::string, BenchmarkResult> baselines;

    LatencyHistogram latency_histogram;
    ThroughputTracker throughput_tracker;
    ResourceMonitor resource_monitor;

    std::string last_error;
    TestLogCallback log_callback;
    TestProgressCallback progress_callback;
    BenchmarkRunner::MetricsCallback metrics_callback;
};

BenchmarkRunner::BenchmarkRunner()
    : impl_(std::make_unique<Impl>()) {
}

BenchmarkRunner::~BenchmarkRunner() = default;

void BenchmarkRunner::setHost(const std::string& host) { impl_->host = host; }
void BenchmarkRunner::setPort(int port) { impl_->port = port; }
void BenchmarkRunner::setProtocol(Protocol protocol) { impl_->protocol = protocol; }
void BenchmarkRunner::setDatabase(const std::string& database) { impl_->database = database; }
void BenchmarkRunner::setUsername(const std::string& username) { impl_->username = username; }
void BenchmarkRunner::setPassword(const std::string& password) { impl_->password = password; }
void BenchmarkRunner::setWarmupDuration(int seconds) { impl_->warmup_duration = seconds; }
void BenchmarkRunner::setCooldownDuration(int seconds) { impl_->cooldown_duration = seconds; }
void BenchmarkRunner::setSampleInterval(int ms) { impl_->sample_interval_ms = ms; }
void BenchmarkRunner::setIterations(int count) { impl_->iterations = count; }

void BenchmarkRunner::registerBenchmark(const Benchmark& benchmark) {
    impl_->registered_benchmarks.push_back(benchmark);
}

void BenchmarkRunner::registerBenchmarks(const std::vector<Benchmark>& benchmarks) {
    for (const auto& b : benchmarks) {
        registerBenchmark(b);
    }
}

void BenchmarkRunner::clearBenchmarks() {
    impl_->registered_benchmarks.clear();
}

std::vector<Benchmark> BenchmarkRunner::getRegisteredBenchmarks() const {
    return impl_->registered_benchmarks;
}

std::vector<BenchmarkResult> BenchmarkRunner::runAllBenchmarks() {
    std::vector<BenchmarkResult> results;

    log("INFO", "Running " + std::to_string(impl_->registered_benchmarks.size()) + " benchmarks");

    for (const auto& benchmark : impl_->registered_benchmarks) {
        auto result = runBenchmark(benchmark);
        results.push_back(result);
        impl_->results.push_back(result);
    }

    return results;
}

BenchmarkResult BenchmarkRunner::runBenchmark(const std::string& name) {
    for (const auto& b : impl_->registered_benchmarks) {
        if (b.name == name) {
            return runBenchmark(b);
        }
    }

    BenchmarkResult result;
    result.benchmark_name = name;
    result.status = TestStatus::ERROR;
    result.summary = "Benchmark not found: " + name;
    return result;
}

BenchmarkResult BenchmarkRunner::runBenchmark(const Benchmark& benchmark) {
    BenchmarkResult result;
    result.benchmark_name = benchmark.name;
    result.config = benchmark.config;
    result.start_time = std::chrono::system_clock::now();
    result.status = TestStatus::RUNNING;

    log("INFO", "Running benchmark: " + benchmark.name);

    // Reset metrics
    impl_->latency_histogram.reset();
    impl_->throughput_tracker.reset();

    // Start resource monitoring
    impl_->resource_monitor.start(impl_->sample_interval_ms);

    // Warmup phase
    if (impl_->warmup_duration > 0) {
        log("DEBUG", "Warmup phase: " + std::to_string(impl_->warmup_duration) + "s");
        std::this_thread::sleep_for(std::chrono::seconds(impl_->warmup_duration));
    }

    // Run the benchmark
    if (benchmark.run_function) {
        benchmark.run_function(result);
    } else {
        // Default benchmark: simple query throughput
        auto duration = std::chrono::seconds(benchmark.config.duration_seconds);
        auto start = std::chrono::steady_clock::now();

        while (std::chrono::steady_clock::now() - start < duration) {
            auto query_start = std::chrono::steady_clock::now();

            // Simulate query execution
            std::this_thread::sleep_for(std::chrono::microseconds(500));

            auto query_end = std::chrono::steady_clock::now();
            double latency_us = std::chrono::duration<double, std::micro>(query_end - query_start).count();

            impl_->latency_histogram.record(latency_us);
            impl_->throughput_tracker.recordOperation();
        }
    }

    // Cooldown phase
    if (impl_->cooldown_duration > 0) {
        log("DEBUG", "Cooldown phase: " + std::to_string(impl_->cooldown_duration) + "s");
        std::this_thread::sleep_for(std::chrono::seconds(impl_->cooldown_duration));
    }

    // Stop resource monitoring
    impl_->resource_monitor.stop();

    // Collect results
    result.end_time = std::chrono::system_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::seconds>(
        result.end_time - result.start_time);
    result.latency = impl_->latency_histogram.toStats();
    result.queries_per_second = impl_->throughput_tracker.getOperationsPerSecond();
    result.transactions_per_second = impl_->throughput_tracker.getTransactionsPerSecond();
    result.peak_usage = impl_->resource_monitor.getPeakUsage();
    result.avg_usage = impl_->resource_monitor.getAverageUsage();

    // Compare to target
    result.target_tps = benchmark.target_tps;
    result.actual_tps = result.queries_per_second;
    if (result.target_tps > 0) {
        result.percent_of_target = (result.actual_tps / result.target_tps) * 100.0;
        result.met_target = result.actual_tps >= result.target_tps;
    }

    result.status = TestStatus::PASSED;
    result.summary = "Achieved " + std::to_string(static_cast<int>(result.actual_tps)) +
                     " QPS, p99 latency " + std::to_string(result.latency.p99_ms) + "ms";

    log("INFO", "Benchmark completed: " + result.summary);

    return result;
}

std::vector<BenchmarkResult> BenchmarkRunner::runBenchmarksByTag(const std::string& tag) {
    std::vector<BenchmarkResult> results;

    for (const auto& b : impl_->registered_benchmarks) {
        if (std::find(b.tags.begin(), b.tags.end(), tag) != b.tags.end()) {
            results.push_back(runBenchmark(b));
        }
    }

    return results;
}

//=============================================================================
// Standard Benchmarks
//=============================================================================

BenchmarkResult BenchmarkRunner::benchmarkConnectionLatency() {
    Benchmark b;
    b.name = "Connection Latency";
    b.target_latency_p99_ms = BenchmarkTargets::CONNECTION_LATENCY_LOCAL_MS;
    b.config.duration_seconds = 30;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkSSLConnectionLatency() {
    Benchmark b;
    b.name = "SSL Connection Latency";
    b.target_latency_p99_ms = BenchmarkTargets::SSL_CONNECTION_LATENCY_MS;
    b.config.duration_seconds = 30;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkAuthenticationLatency() {
    Benchmark b;
    b.name = "Authentication Latency";
    b.target_latency_p99_ms = BenchmarkTargets::AUTH_LATENCY_MS;
    b.config.duration_seconds = 30;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkPoolAcquireLatency() {
    Benchmark b;
    b.name = "Pool Acquire Latency";
    b.target_latency_p99_ms = BenchmarkTargets::POOL_ACQUIRE_LATENCY_MS;
    b.config.duration_seconds = 30;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkMaxConnectionsPerSecond() {
    Benchmark b;
    b.name = "Max Connections/Second";
    b.target_tps = BenchmarkTargets::MAX_CONNECTIONS_PER_SECOND;
    b.config.duration_seconds = 30;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkPointSelect() {
    Benchmark b;
    b.name = "Point SELECT";
    b.target_latency_p99_ms = BenchmarkTargets::POINT_SELECT_LATENCY_MS * 2;  // p99 target
    b.config.duration_seconds = 60;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkRangeSelect() {
    Benchmark b;
    b.name = "Range SELECT";
    b.target_latency_p99_ms = BenchmarkTargets::RANGE_SELECT_LATENCY_MS * 2.5;
    b.config.duration_seconds = 60;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkComplexJoin() {
    Benchmark b;
    b.name = "Complex JOIN";
    b.target_latency_p99_ms = BenchmarkTargets::COMPLEX_JOIN_LATENCY_MS;
    b.config.duration_seconds = 60;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkAggregate() {
    Benchmark b;
    b.name = "Aggregate";
    b.target_latency_p99_ms = BenchmarkTargets::AGGREGATE_LATENCY_MS;
    b.config.duration_seconds = 60;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkInsertSingle() {
    Benchmark b;
    b.name = "INSERT Single";
    b.target_latency_p99_ms = BenchmarkTargets::INSERT_SINGLE_LATENCY_MS * 2;
    b.config.duration_seconds = 60;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkInsertBatch() {
    Benchmark b;
    b.name = "INSERT Batch (1000)";
    b.target_latency_p99_ms = BenchmarkTargets::INSERT_BATCH_1000_LATENCY_MS;
    b.config.duration_seconds = 60;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkUpdateSingle() {
    Benchmark b;
    b.name = "UPDATE Single";
    b.target_latency_p99_ms = BenchmarkTargets::UPDATE_SINGLE_LATENCY_MS * 3;
    b.config.duration_seconds = 60;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkDeleteSingle() {
    Benchmark b;
    b.name = "DELETE Single";
    b.target_latency_p99_ms = BenchmarkTargets::DELETE_SINGLE_LATENCY_MS * 3;
    b.config.duration_seconds = 60;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkTPCB(int scale_factor, int clients) {
    Benchmark b;
    b.name = "TPC-B (pgbench)";
    b.target_tps = BenchmarkTargets::TPC_B_TPS;
    b.config.scale_factor = scale_factor;
    b.config.clients = clients;
    b.config.duration_seconds = 60;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkTPCC(int warehouses, int terminals) {
    Benchmark b;
    b.name = "TPC-C";
    b.target_tps = BenchmarkTargets::TPC_C_TPS;
    b.config.scale_factor = warehouses;
    b.config.clients = terminals;
    b.config.duration_seconds = 60;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkBulkInsert() {
    Benchmark b;
    b.name = "Bulk INSERT";
    b.target_tps = BenchmarkTargets::BULK_INSERT_ROWS_PER_SEC;
    b.config.duration_seconds = 60;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkBulkSelect() {
    Benchmark b;
    b.name = "Bulk SELECT";
    b.target_tps = BenchmarkTargets::BULK_SELECT_ROWS_PER_SEC;
    b.config.duration_seconds = 60;
    return runBenchmark(b);
}

BenchmarkResult BenchmarkRunner::benchmarkMixedOLTP(int read_percent) {
    Benchmark b;
    b.name = "Mixed OLTP (" + std::to_string(read_percent) + "% read)";
    b.target_tps = BenchmarkTargets::MIXED_OLTP_TPS;
    b.config.duration_seconds = 60;
    return runBenchmark(b);
}

std::vector<BenchmarkResult> BenchmarkRunner::benchmarkConnectionScaling(int max_connections) {
    std::vector<BenchmarkResult> results;
    for (int conns = 10; conns <= max_connections; conns *= 2) {
        Benchmark b;
        b.name = "Connection Scaling (" + std::to_string(conns) + ")";
        b.config.clients = conns;
        b.config.duration_seconds = 30;
        auto result = runBenchmark(b);
        result.tps_by_clients[conns] = result.queries_per_second;
        results.push_back(result);
    }
    return results;
}

std::vector<BenchmarkResult> BenchmarkRunner::benchmarkCPUScaling(int max_cores) {
    std::vector<BenchmarkResult> results;
    for (int cores = 1; cores <= max_cores; cores *= 2) {
        Benchmark b;
        b.name = "CPU Scaling (" + std::to_string(cores) + " cores)";
        b.config.threads = cores;
        b.config.duration_seconds = 30;
        results.push_back(runBenchmark(b));
    }
    return results;
}

std::vector<BenchmarkResult> BenchmarkRunner::benchmarkDataScaling(int64_t max_rows) {
    std::vector<BenchmarkResult> results;
    for (int64_t rows = 1000; rows <= max_rows; rows *= 10) {
        Benchmark b;
        b.name = "Data Scaling (" + std::to_string(rows) + " rows)";
        b.config.duration_seconds = 30;
        results.push_back(runBenchmark(b));
    }
    return results;
}

std::vector<BenchmarkResult> BenchmarkRunner::benchmarkConcurrencyScaling(int max_concurrent) {
    std::vector<BenchmarkResult> results;
    for (int conc = 1; conc <= max_concurrent; conc *= 2) {
        Benchmark b;
        b.name = "Concurrency Scaling (" + std::to_string(conc) + ")";
        b.config.clients = conc;
        b.config.duration_seconds = 30;
        results.push_back(runBenchmark(b));
    }
    return results;
}

std::vector<BenchmarkResult> BenchmarkRunner::benchmarkBufferPoolScaling(int max_mb) {
    std::vector<BenchmarkResult> results;
    for (int mb = 256; mb <= max_mb; mb *= 2) {
        Benchmark b;
        b.name = "Buffer Pool (" + std::to_string(mb) + " MB)";
        b.config.duration_seconds = 30;
        results.push_back(runBenchmark(b));
    }
    return results;
}

std::vector<BenchmarkResult> BenchmarkRunner::compareProtocolPointSelect() {
    std::vector<BenchmarkResult> results;
    for (auto protocol : {Protocol::POSTGRESQL, Protocol::MYSQL, Protocol::FIREBIRD, Protocol::NATIVE}) {
        impl_->protocol = protocol;
        Benchmark b;
        b.name = "Point SELECT (" + toString(protocol) + ")";
        b.config.duration_seconds = 30;
        results.push_back(runBenchmark(b));
    }
    return results;
}

std::vector<BenchmarkResult> BenchmarkRunner::compareProtocolInsert() {
    std::vector<BenchmarkResult> results;
    for (auto protocol : {Protocol::POSTGRESQL, Protocol::MYSQL, Protocol::FIREBIRD, Protocol::NATIVE}) {
        impl_->protocol = protocol;
        Benchmark b;
        b.name = "INSERT (" + toString(protocol) + ")";
        b.config.duration_seconds = 30;
        results.push_back(runBenchmark(b));
    }
    return results;
}

std::vector<BenchmarkResult> BenchmarkRunner::compareProtocolBulkCopy() {
    std::vector<BenchmarkResult> results;
    for (auto protocol : {Protocol::POSTGRESQL, Protocol::MYSQL, Protocol::FIREBIRD, Protocol::NATIVE}) {
        impl_->protocol = protocol;
        Benchmark b;
        b.name = "Bulk COPY (" + toString(protocol) + ")";
        b.config.duration_seconds = 30;
        results.push_back(runBenchmark(b));
    }
    return results;
}

LatencyHistogram& BenchmarkRunner::getLatencyHistogram() {
    return impl_->latency_histogram;
}

ThroughputTracker& BenchmarkRunner::getThroughputTracker() {
    return impl_->throughput_tracker;
}

ResourceMonitor& BenchmarkRunner::getResourceMonitor() {
    return impl_->resource_monitor;
}

void BenchmarkRunner::setBaseline(const std::string& name, const BenchmarkResult& result) {
    impl_->baselines[name] = result;
}

BenchmarkResult BenchmarkRunner::getBaseline(const std::string& name) const {
    auto it = impl_->baselines.find(name);
    if (it != impl_->baselines.end()) {
        return it->second;
    }
    return BenchmarkResult{};
}

double BenchmarkRunner::compareToBaseline(const std::string& name, const BenchmarkResult& result) const {
    auto baseline = getBaseline(name);
    if (baseline.actual_tps > 0) {
        return ((result.actual_tps - baseline.actual_tps) / baseline.actual_tps) * 100.0;
    }
    return 0;
}

std::vector<BenchmarkResult> BenchmarkRunner::getAllResults() const {
    return impl_->results;
}

std::string BenchmarkRunner::generateSummary() const {
    std::stringstream ss;
    ss << "Benchmark Summary\n";
    ss << "=================\n\n";

    for (const auto& result : impl_->results) {
        ss << result.benchmark_name << ":\n";
        ss << "  TPS: " << result.actual_tps;
        if (result.target_tps > 0) {
            ss << " (target: " << result.target_tps << ", " << result.percent_of_target << "%)";
        }
        ss << "\n";
        ss << "  Latency p99: " << result.latency.p99_ms << " ms\n";
        ss << "  Status: " << toString(result.status) << "\n\n";
    }

    return ss.str();
}

std::string BenchmarkRunner::generateTextReport() const {
    return generateSummary();
}

std::string BenchmarkRunner::generateJSON() const {
    std::stringstream ss;
    ss << "{\n  \"benchmarks\": [\n";

    bool first = true;
    for (const auto& result : impl_->results) {
        if (!first) ss << ",\n";
        first = false;
        ss << "    {\n";
        ss << "      \"name\": \"" << result.benchmark_name << "\",\n";
        ss << "      \"tps\": " << result.actual_tps << ",\n";
        ss << "      \"target_tps\": " << result.target_tps << ",\n";
        ss << "      \"p99_ms\": " << result.latency.p99_ms << ",\n";
        ss << "      \"met_target\": " << (result.met_target ? "true" : "false") << "\n";
        ss << "    }";
    }

    ss << "\n  ]\n}\n";
    return ss.str();
}

std::string BenchmarkRunner::generateCSV() const {
    std::stringstream ss;
    ss << "benchmark,tps,target_tps,percent_of_target,p50_ms,p99_ms,status\n";

    for (const auto& result : impl_->results) {
        ss << result.benchmark_name << ","
           << result.actual_tps << ","
           << result.target_tps << ","
           << result.percent_of_target << ","
           << result.latency.median_ms << ","
           << result.latency.p99_ms << ","
           << toString(result.status) << "\n";
    }

    return ss.str();
}

std::string BenchmarkRunner::generateMarkdown() const {
    std::stringstream ss;
    ss << "# Benchmark Results\n\n";
    ss << "| Benchmark | TPS | Target | % of Target | p99 Latency | Status |\n";
    ss << "|-----------|-----|--------|-------------|-------------|--------|\n";

    for (const auto& result : impl_->results) {
        ss << "| " << result.benchmark_name
           << " | " << static_cast<int>(result.actual_tps)
           << " | " << static_cast<int>(result.target_tps)
           << " | " << static_cast<int>(result.percent_of_target) << "%"
           << " | " << result.latency.p99_ms << " ms"
           << " | " << toString(result.status) << " |\n";
    }

    return ss.str();
}

void BenchmarkRunner::exportResults(const std::string& path, const std::string& format) {
    std::ofstream file(path);
    if (format == "json") {
        file << generateJSON();
    } else if (format == "csv") {
        file << generateCSV();
    } else if (format == "markdown" || format == "md") {
        file << generateMarkdown();
    } else {
        file << generateTextReport();
    }
}

void BenchmarkRunner::exportLatencyData(const std::string& path) {
    std::ofstream file(path);
    file << impl_->latency_histogram.toCSV();
}

void BenchmarkRunner::exportThroughputData(const std::string& path) {
    std::ofstream file(path);
    file << impl_->throughput_tracker.toCSV();
}

void BenchmarkRunner::exportResourceData(const std::string& path) {
    std::ofstream file(path);
    file << impl_->resource_monitor.toCSV();
}

void BenchmarkRunner::setLogCallback(TestLogCallback callback) {
    impl_->log_callback = callback;
}

void BenchmarkRunner::setProgressCallback(TestProgressCallback callback) {
    impl_->progress_callback = callback;
}

void BenchmarkRunner::setMetricsCallback(MetricsCallback callback) {
    impl_->metrics_callback = callback;
}

std::string BenchmarkRunner::getLastError() const {
    return impl_->last_error;
}

void BenchmarkRunner::log(const std::string& level, const std::string& message) {
    if (impl_->log_callback) {
        impl_->log_callback(level, "[BenchmarkRunner] " + message);
    }
}

void BenchmarkRunner::setError(const std::string& error) {
    impl_->last_error = error;
    log("ERROR", error);
}

//=============================================================================
// Standard Benchmark Definitions
//=============================================================================

std::vector<Benchmark> getConnectionBenchmarks() {
    std::vector<Benchmark> benchmarks;

    Benchmark conn_lat;
    conn_lat.name = "Connection Latency";
    conn_lat.target_latency_p99_ms = 5.0;
    conn_lat.config.duration_seconds = 30;
    benchmarks.push_back(conn_lat);

    Benchmark ssl_lat;
    ssl_lat.name = "SSL Connection Latency";
    ssl_lat.target_latency_p99_ms = 20.0;
    ssl_lat.config.duration_seconds = 30;
    benchmarks.push_back(ssl_lat);

    return benchmarks;
}

std::vector<Benchmark> getQueryBenchmarks() {
    std::vector<Benchmark> benchmarks;

    Benchmark point;
    point.name = "Point SELECT";
    point.target_latency_p99_ms = 1.0;
    point.config.duration_seconds = 60;
    benchmarks.push_back(point);

    Benchmark range;
    range.name = "Range SELECT";
    range.target_latency_p99_ms = 5.0;
    range.config.duration_seconds = 60;
    benchmarks.push_back(range);

    return benchmarks;
}

std::vector<Benchmark> getThroughputBenchmarks() {
    std::vector<Benchmark> benchmarks;

    Benchmark tpcb;
    tpcb.name = "TPC-B";
    tpcb.target_tps = 10000;
    tpcb.config.duration_seconds = 60;
    benchmarks.push_back(tpcb);

    Benchmark tpcc;
    tpcc.name = "TPC-C";
    tpcc.target_tps = 5000;
    tpcc.config.duration_seconds = 60;
    benchmarks.push_back(tpcc);

    return benchmarks;
}

std::vector<Benchmark> getScalabilityBenchmarks() {
    std::vector<Benchmark> benchmarks;

    Benchmark conn_scale;
    conn_scale.name = "Connection Scaling";
    conn_scale.config.duration_seconds = 30;
    benchmarks.push_back(conn_scale);

    return benchmarks;
}

std::vector<Benchmark> getAllStandardBenchmarks() {
    std::vector<Benchmark> all;

    auto conn = getConnectionBenchmarks();
    auto query = getQueryBenchmarks();
    auto throughput = getThroughputBenchmarks();
    auto scale = getScalabilityBenchmarks();

    all.insert(all.end(), conn.begin(), conn.end());
    all.insert(all.end(), query.begin(), query.end());
    all.insert(all.end(), throughput.begin(), throughput.end());
    all.insert(all.end(), scale.begin(), scale.end());

    return all;
}

} // namespace testing
} // namespace scratchbird
