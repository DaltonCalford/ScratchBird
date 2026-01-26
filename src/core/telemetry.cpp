// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-16: Telemetry Export Implementation
//
// November 25, 2025

#include "scratchbird/core/telemetry.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace scratchbird::core {

// Default latency buckets (in seconds): 1ms, 5ms, 10ms, 25ms, 50ms, 100ms, 250ms, 500ms, 1s, 2.5s, 5s, 10s
const std::vector<double> Histogram::DEFAULT_LATENCY_BUCKETS = {
    0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0
};

// ============================================================================
// Metric Base Class
// ============================================================================

Metric::Metric(const std::string& name, const std::string& help, MetricType type)
    : name_(name), help_(help), type_(type) {
}

std::string Metric::escapeLabel(const std::string& str) {
    std::ostringstream ss;
    for (char c : str) {
        switch (c) {
            case '\\': ss << "\\\\"; break;
            case '"': ss << "\\\""; break;
            case '\n': ss << "\\n"; break;
            default: ss << c; break;
        }
    }
    return ss.str();
}

std::string Metric::formatLabels(const std::vector<MetricLabel>& labels) {
    if (labels.empty()) return "";

    std::ostringstream ss;
    ss << "{";
    for (size_t i = 0; i < labels.size(); ++i) {
        if (i > 0) ss << ",";
        ss << labels[i].name << "=\"" << escapeLabel(labels[i].value) << "\"";
    }
    ss << "}";
    return ss.str();
}

// ============================================================================
// Counter Implementation
// ============================================================================

Counter::Counter(const std::string& name, const std::string& help,
                 const std::vector<std::string>& label_names)
    : Metric(name, help, MetricType::COUNTER), label_names_(label_names) {
}

std::string Counter::makeKey(const std::vector<std::string>& label_values) const {
    if (label_values.empty()) return "";
    std::ostringstream ss;
    for (size_t i = 0; i < label_values.size(); ++i) {
        if (i > 0) ss << ",";
        ss << label_values[i];
    }
    return ss.str();
}

void Counter::inc(double value, const std::vector<std::string>& label_values) {
    std::string key = makeKey(label_values);
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = values_.find(key);
    if (it == values_.end()) {
        values_[key] = value;
    } else {
        double current = it->second.load();
        while (!it->second.compare_exchange_weak(current, current + value));
    }
}

double Counter::get(const std::vector<std::string>& label_values) const {
    std::string key = makeKey(label_values);
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = values_.find(key);
    return it != values_.end() ? it->second.load() : 0.0;
}

std::string Counter::toPrometheus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream ss;

    ss << "# HELP " << name_ << " " << help_ << "\n";
    ss << "# TYPE " << name_ << " counter\n";

    if (values_.empty()) {
        ss << name_ << " 0\n";
    } else {
        for (const auto& [key, value] : values_) {
            ss << name_;
            if (!key.empty() && !label_names_.empty()) {
                ss << "{";
                std::vector<std::string> parts;
                std::istringstream iss(key);
                std::string part;
                while (std::getline(iss, part, ',')) {
                    parts.push_back(part);
                }
                for (size_t i = 0; i < label_names_.size() && i < parts.size(); ++i) {
                    if (i > 0) ss << ",";
                    ss << label_names_[i] << "=\"" << escapeLabel(parts[i]) << "\"";
                }
                ss << "}";
            }
            ss << " " << std::fixed << std::setprecision(6) << value.load() << "\n";
        }
    }

    return ss.str();
}

std::string Counter::toOpenMetrics() const {
    std::ostringstream ss;
    ss << toPrometheus();
    return ss.str();
}

// ============================================================================
// Gauge Implementation
// ============================================================================

Gauge::Gauge(const std::string& name, const std::string& help,
             const std::vector<std::string>& label_names)
    : Metric(name, help, MetricType::GAUGE), label_names_(label_names) {
}

std::string Gauge::makeKey(const std::vector<std::string>& label_values) const {
    if (label_values.empty()) return "";
    std::ostringstream ss;
    for (size_t i = 0; i < label_values.size(); ++i) {
        if (i > 0) ss << ",";
        ss << label_values[i];
    }
    return ss.str();
}

void Gauge::set(double value, const std::vector<std::string>& label_values) {
    std::string key = makeKey(label_values);
    std::lock_guard<std::mutex> lock(mutex_);
    values_[key] = value;
}

void Gauge::inc(double value, const std::vector<std::string>& label_values) {
    std::string key = makeKey(label_values);
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = values_.find(key);
    if (it == values_.end()) {
        values_[key] = value;
    } else {
        double current = it->second.load();
        while (!it->second.compare_exchange_weak(current, current + value));
    }
}

void Gauge::dec(double value, const std::vector<std::string>& label_values) {
    inc(-value, label_values);
}

double Gauge::get(const std::vector<std::string>& label_values) const {
    std::string key = makeKey(label_values);
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = values_.find(key);
    return it != values_.end() ? it->second.load() : 0.0;
}

std::string Gauge::toPrometheus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream ss;

    ss << "# HELP " << name_ << " " << help_ << "\n";
    ss << "# TYPE " << name_ << " gauge\n";

    if (values_.empty()) {
        ss << name_ << " 0\n";
    } else {
        for (const auto& [key, value] : values_) {
            ss << name_;
            if (!key.empty() && !label_names_.empty()) {
                ss << "{";
                std::vector<std::string> parts;
                std::istringstream iss(key);
                std::string part;
                while (std::getline(iss, part, ',')) {
                    parts.push_back(part);
                }
                for (size_t i = 0; i < label_names_.size() && i < parts.size(); ++i) {
                    if (i > 0) ss << ",";
                    ss << label_names_[i] << "=\"" << escapeLabel(parts[i]) << "\"";
                }
                ss << "}";
            }
            ss << " " << std::fixed << std::setprecision(6) << value.load() << "\n";
        }
    }

    return ss.str();
}

std::string Gauge::toOpenMetrics() const {
    return toPrometheus();
}

// ============================================================================
// Histogram Implementation
// ============================================================================

Histogram::Histogram(const std::string& name, const std::string& help,
                     const std::vector<double>& buckets,
                     const std::vector<std::string>& label_names)
    : Metric(name, help, MetricType::HISTOGRAM),
      buckets_(buckets), label_names_(label_names) {
    // Ensure buckets are sorted
    std::sort(buckets_.begin(), buckets_.end());
}

std::string Histogram::makeKey(const std::vector<std::string>& label_values) const {
    if (label_values.empty()) return "";
    std::ostringstream ss;
    for (size_t i = 0; i < label_values.size(); ++i) {
        if (i > 0) ss << ",";
        ss << label_values[i];
    }
    return ss.str();
}

Histogram::HistogramData* Histogram::getOrCreate(const std::string& key) {
    auto it = data_.find(key);
    if (it == data_.end()) {
        data_[key] = std::make_unique<HistogramData>(buckets_.size());
        return data_[key].get();
    }
    return it->second.get();
}

void Histogram::observe(double value, const std::vector<std::string>& label_values) {
    std::string key = makeKey(label_values);
    std::lock_guard<std::mutex> lock(mutex_);

    HistogramData* data = getOrCreate(key);

    // Update buckets
    for (size_t i = 0; i < buckets_.size(); ++i) {
        if (value <= buckets_[i]) {
            data->bucket_counts[i]++;
        }
    }

    // Update sum and count
    double current_sum = data->sum.load();
    while (!data->sum.compare_exchange_weak(current_sum, current_sum + value));
    data->count++;
}

double Histogram::sum(const std::vector<std::string>& label_values) const {
    std::string key = makeKey(label_values);
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = data_.find(key);
    return it != data_.end() ? it->second->sum.load() : 0.0;
}

uint64_t Histogram::count(const std::vector<std::string>& label_values) const {
    std::string key = makeKey(label_values);
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = data_.find(key);
    return it != data_.end() ? it->second->count.load() : 0;
}

std::string Histogram::toPrometheus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream ss;

    ss << "# HELP " << name_ << " " << help_ << "\n";
    ss << "# TYPE " << name_ << " histogram\n";

    for (const auto& [key, data] : data_) {
        std::string base_labels;
        if (!key.empty() && !label_names_.empty()) {
            std::vector<std::string> parts;
            std::istringstream iss(key);
            std::string part;
            while (std::getline(iss, part, ',')) {
                parts.push_back(part);
            }
            std::ostringstream lss;
            for (size_t i = 0; i < label_names_.size() && i < parts.size(); ++i) {
                if (i > 0) lss << ",";
                lss << label_names_[i] << "=\"" << escapeLabel(parts[i]) << "\"";
            }
            base_labels = lss.str();
        }

        // Output bucket counts
        uint64_t cumulative = 0;
        for (size_t i = 0; i < buckets_.size(); ++i) {
            cumulative += data->bucket_counts[i].load();
            ss << name_ << "_bucket{";
            if (!base_labels.empty()) ss << base_labels << ",";
            ss << "le=\"" << std::fixed << std::setprecision(6) << buckets_[i] << "\"} "
               << cumulative << "\n";
        }

        // +Inf bucket
        ss << name_ << "_bucket{";
        if (!base_labels.empty()) ss << base_labels << ",";
        ss << "le=\"+Inf\"} " << data->count.load() << "\n";

        // Sum and count
        ss << name_ << "_sum";
        if (!base_labels.empty()) ss << "{" << base_labels << "}";
        ss << " " << std::fixed << std::setprecision(6) << data->sum.load() << "\n";

        ss << name_ << "_count";
        if (!base_labels.empty()) ss << "{" << base_labels << "}";
        ss << " " << data->count.load() << "\n";
    }

    return ss.str();
}

std::string Histogram::toOpenMetrics() const {
    return toPrometheus();
}

// ============================================================================
// HistogramTimer Implementation
// ============================================================================

HistogramTimer::HistogramTimer(Histogram* histogram, const std::vector<std::string>& label_values)
    : histogram_(histogram), label_values_(label_values),
      start_(std::chrono::high_resolution_clock::now()) {
}

HistogramTimer::~HistogramTimer() {
    if (!cancelled_ && histogram_) {
        histogram_->observe(elapsed(), label_values_);
    }
}

double HistogramTimer::elapsed() const {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(now - start_).count();
}

void HistogramTimer::cancel() {
    cancelled_ = true;
}

// ============================================================================
// MetricsRegistry Implementation
// ============================================================================

MetricsRegistry& MetricsRegistry::getInstance() {
    static MetricsRegistry* instance = new MetricsRegistry();
    return *instance;
}

Counter* MetricsRegistry::registerCounter(const std::string& name, const std::string& help,
                                          const std::vector<std::string>& label_names) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto counter = std::make_unique<Counter>(name, help, label_names);
    Counter* ptr = counter.get();
    metrics_[name] = std::move(counter);
    return ptr;
}

Gauge* MetricsRegistry::registerGauge(const std::string& name, const std::string& help,
                                      const std::vector<std::string>& label_names) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto gauge = std::make_unique<Gauge>(name, help, label_names);
    Gauge* ptr = gauge.get();
    metrics_[name] = std::move(gauge);
    return ptr;
}

Histogram* MetricsRegistry::registerHistogram(const std::string& name, const std::string& help,
                                               const std::vector<double>& buckets,
                                               const std::vector<std::string>& label_names) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto histogram = std::make_unique<Histogram>(name, help, buckets, label_names);
    Histogram* ptr = histogram.get();
    metrics_[name] = std::move(histogram);
    return ptr;
}

Metric* MetricsRegistry::get(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = metrics_.find(name);
    return it != metrics_.end() ? it->second.get() : nullptr;
}

std::string MetricsRegistry::exportPrometheus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream ss;

    for (const auto& [name, metric] : metrics_) {
        ss << metric->toPrometheus();
    }

    return ss.str();
}

std::string MetricsRegistry::exportOpenMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream ss;

    for (const auto& [name, metric] : metrics_) {
        ss << metric->toOpenMetrics();
    }

    // OpenMetrics requires EOF marker
    ss << "# EOF\n";

    return ss.str();
}

void MetricsRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.clear();
}

// ============================================================================
// ScratchBirdMetrics Implementation
// ============================================================================

ScratchBirdMetrics& ScratchBirdMetrics::getInstance() {
    static ScratchBirdMetrics instance;
    return instance;
}

void ScratchBirdMetrics::initialize() {
    if (initialized_) return;

    MetricsRegistry& reg = MetricsRegistry::getInstance();

    // Query metrics
    query_duration_seconds = reg.registerHistogram(
        "scratchbird_query_duration_seconds",
        "Query execution duration in seconds",
        Histogram::DEFAULT_LATENCY_BUCKETS,
        {"query_type", "database"});

    queries_total = reg.registerCounter(
        "scratchbird_queries_total",
        "Total number of queries executed",
        {"query_type", "database"});

    query_errors_total = reg.registerCounter(
        "scratchbird_query_errors_total",
        "Total number of query errors",
        {"error_type", "database"});

    query_rows_returned_total = reg.registerCounter(
        "scratchbird_query_rows_returned_total",
        "Total rows returned by SELECT queries",
        {"type", "database"});

    query_rows_affected_total = reg.registerCounter(
        "scratchbird_query_rows_affected_total",
        "Total rows affected by DML queries",
        {"type", "database"});

    query_currently_running = reg.registerGauge(
        "scratchbird_query_currently_running",
        "Number of currently running statements",
        {"database"});

    // Transaction metrics
    transactions_total = reg.registerCounter(
        "scratchbird_transactions_total",
        "Total number of transactions",
        {"database"});

    transactions_committed = reg.registerCounter(
        "scratchbird_transactions_committed_total",
        "Total number of committed transactions",
        {"database"});

    transactions_rolled_back = reg.registerCounter(
        "scratchbird_transactions_rolled_back_total",
        "Total number of rolled back transactions",
        {"database"});

    transactions_active = reg.registerGauge(
        "scratchbird_transactions_active",
        "Number of currently active transactions",
        {"database"});

    // Buffer pool metrics
    buffer_pool_size_bytes = reg.registerGauge(
        "scratchbird_buffer_pool_size_bytes",
        "Buffer pool size in bytes");

    buffer_pool_pages_total = reg.registerGauge(
        "scratchbird_buffer_pool_pages_total",
        "Total pages in buffer pool");

    buffer_pool_pages_dirty = reg.registerGauge(
        "scratchbird_buffer_pool_pages_dirty",
        "Number of dirty pages in buffer pool");

    buffer_pool_reads_total = reg.registerCounter(
        "scratchbird_buffer_pool_reads_total",
        "Total page reads from disk");

    buffer_pool_writes_total = reg.registerCounter(
        "scratchbird_buffer_pool_writes_total",
        "Total page writes to disk");

    buffer_pool_hits_total = reg.registerCounter(
        "scratchbird_buffer_pool_hits_total",
        "Total buffer pool cache hits");

    buffer_pool_misses_total = reg.registerCounter(
        "scratchbird_buffer_pool_misses_total",
        "Total buffer pool cache misses");

    // Lock metrics
    lock_wait_seconds = reg.registerHistogram(
        "scratchbird_lock_wait_seconds",
        "Lock wait time in seconds",
        Histogram::DEFAULT_LATENCY_BUCKETS,
        {"lock_type"});

    lock_deadlocks_total = reg.registerCounter(
        "scratchbird_lock_deadlocks_total",
        "Total number of deadlocks detected");

    locks_held = reg.registerGauge(
        "scratchbird_locks_held",
        "Number of currently held locks",
        {"lock_type"});

    // Index metrics
    index_scans_total = reg.registerCounter(
        "scratchbird_index_scans_total",
        "Total index scans",
        {"index_type", "table"});

    seq_scans_total = reg.registerCounter(
        "scratchbird_seq_scans_total",
        "Total sequential scans",
        {"table"});

    index_scan_duration_seconds = reg.registerHistogram(
        "scratchbird_index_scan_duration_seconds",
        "Index scan duration in seconds",
        Histogram::DEFAULT_LATENCY_BUCKETS,
        {"index_type"});

    // Disk I/O metrics
    disk_read_bytes_total = reg.registerCounter(
        "scratchbird_disk_read_bytes_total",
        "Total bytes read from disk");

    disk_write_bytes_total = reg.registerCounter(
        "scratchbird_disk_write_bytes_total",
        "Total bytes written to disk");

    disk_read_latency_seconds = reg.registerHistogram(
        "scratchbird_disk_read_latency_seconds",
        "Disk read latency in seconds",
        Histogram::DEFAULT_LATENCY_BUCKETS);

    disk_write_latency_seconds = reg.registerHistogram(
        "scratchbird_disk_write_latency_seconds",
        "Disk write latency in seconds",
        Histogram::DEFAULT_LATENCY_BUCKETS);

    // Connection metrics
    connections_active = reg.registerGauge(
        "scratchbird_connections_active",
        "Number of active connections");

    connections_idle = reg.registerGauge(
        "scratchbird_connections_idle",
        "Number of idle connections");

    connections_total = reg.registerCounter(
        "scratchbird_connections_total",
        "Total connections established");

    // Catalog metrics
    tables_count = reg.registerGauge(
        "scratchbird_tables_count",
        "Number of tables in the database",
        {"database"});

    indexes_count = reg.registerGauge(
        "scratchbird_indexes_count",
        "Number of indexes in the database",
        {"database"});

    // TOAST metrics
    toast_reads_total = reg.registerCounter(
        "scratchbird_toast_reads_total",
        "Total TOAST value reads");

    toast_writes_total = reg.registerCounter(
        "scratchbird_toast_writes_total",
        "Total TOAST value writes");

    // COPY metrics
    copy_rows_total = reg.registerCounter(
        "scratchbird_copy_rows_total",
        "Rows processed by COPY",
        {"direction"});

    copy_bytes_total = reg.registerCounter(
        "scratchbird_copy_bytes_total",
        "Bytes processed by COPY",
        {"direction"});

    copy_errors_total = reg.registerCounter(
        "scratchbird_copy_errors_total",
        "Total COPY errors");

    copy_duration_seconds = reg.registerHistogram(
        "scratchbird_copy_duration_seconds",
        "COPY duration in seconds",
        Histogram::DEFAULT_LATENCY_BUCKETS,
        {"direction"});

    // Scheduler metrics
    scheduler_queue_depth = reg.registerGauge(
        "scratchbird_scheduler_queue_depth",
        "Number of due jobs waiting to run");

    scheduler_jobs_running = reg.registerGauge(
        "scratchbird_scheduler_jobs_running",
        "Number of active job runs");

    scheduler_jobs_failed_total = reg.registerCounter(
        "scratchbird_scheduler_jobs_failed_total",
        "Total number of failed job runs");

    scheduler_job_run_latency_seconds = reg.registerHistogram(
        "scratchbird_scheduler_job_run_latency_seconds",
        "Job run duration in seconds",
        Histogram::DEFAULT_LATENCY_BUCKETS);

    initialized_ = true;
}

// ============================================================================
// MetricsEndpoint Implementation
// ============================================================================

std::string MetricsEndpoint::handleRequest(const std::string& path,
                                            const std::string& accept_header) {
    // Check if OpenMetrics format is requested
    bool openmetrics = accept_header.find("application/openmetrics-text") != std::string::npos;

    if (openmetrics) {
        return MetricsRegistry::getInstance().exportOpenMetrics();
    } else {
        return MetricsRegistry::getInstance().exportPrometheus();
    }
}

std::string MetricsEndpoint::getContentType(bool openmetrics) {
    if (openmetrics) {
        return "application/openmetrics-text; version=1.0.0; charset=utf-8";
    } else {
        return "text/plain; version=0.0.4; charset=utf-8";
    }
}

} // namespace scratchbird::core
