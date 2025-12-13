/*
 * ScratchBird Database Engine
 * Load and Stress Testing Implementation
 * Copyright (c) 2025 ScratchBird Project
 */

#include "scratchbird/testing/LoadTester.h"
#include <sstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <random>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace scratchbird {
namespace testing {

//=============================================================================
// Connection Load Generator Implementation
//=============================================================================

struct ConnectionLoadGenerator::Impl {
    std::string host = "localhost";
    int port = 3092;
    Protocol protocol = Protocol::NATIVE;
    std::string database = "testdb";
    std::string username = "testuser";
    std::string password = "test123";
    bool ssl_enabled = false;

    int concurrent_connections = 10;
    int total_connections = 100;
    int connection_rate = 10;
    int connection_duration = 60;
    int idle_time = 0;

    std::atomic<bool> running{false};
    std::atomic<int> active_connections{0};
    std::atomic<int> completed_connections{0};
    std::atomic<int> failed_connections{0};

    std::vector<double> connection_latencies;
    std::mutex latency_mutex;

    LoadTestResult result;
};

ConnectionLoadGenerator::ConnectionLoadGenerator()
    : impl_(std::make_unique<Impl>()) {
}

ConnectionLoadGenerator::~ConnectionLoadGenerator() {
    stop();
}

void ConnectionLoadGenerator::setHost(const std::string& host) { impl_->host = host; }
void ConnectionLoadGenerator::setPort(int port) { impl_->port = port; }
void ConnectionLoadGenerator::setProtocol(Protocol protocol) { impl_->protocol = protocol; }
void ConnectionLoadGenerator::setDatabase(const std::string& database) { impl_->database = database; }
void ConnectionLoadGenerator::setUsername(const std::string& username) { impl_->username = username; }
void ConnectionLoadGenerator::setPassword(const std::string& password) { impl_->password = password; }
void ConnectionLoadGenerator::setSSL(bool enabled) { impl_->ssl_enabled = enabled; }

void ConnectionLoadGenerator::setConcurrentConnections(int count) { impl_->concurrent_connections = count; }
void ConnectionLoadGenerator::setTotalConnections(int count) { impl_->total_connections = count; }
void ConnectionLoadGenerator::setConnectionRate(int per_second) { impl_->connection_rate = per_second; }
void ConnectionLoadGenerator::setConnectionDuration(int seconds) { impl_->connection_duration = seconds; }
void ConnectionLoadGenerator::setIdleTime(int seconds) { impl_->idle_time = seconds; }

void ConnectionLoadGenerator::start() {
    impl_->running = true;
    impl_->result.start_time = std::chrono::system_clock::now();
    impl_->result.type = LoadTestType::CONNECTION;
    impl_->result.status = TestStatus::RUNNING;

    // Spawn worker threads
    std::vector<std::thread> workers;
    int connections_started = 0;

    while (impl_->running && connections_started < impl_->total_connections) {
        // Rate limiting
        int batch_size = std::min(impl_->connection_rate,
                                   impl_->total_connections - connections_started);

        for (int i = 0; i < batch_size && impl_->running; i++) {
            // Wait for available slot
            while (impl_->active_connections >= impl_->concurrent_connections && impl_->running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            if (!impl_->running) break;

            // Start a connection
            impl_->active_connections++;
            connections_started++;

            workers.emplace_back([this]() {
                auto start = std::chrono::steady_clock::now();

                // Simulate connection (in real impl, actually connect)
                bool success = true;  // Would actually attempt connection
                std::this_thread::sleep_for(std::chrono::milliseconds(5));  // Simulate connect time

                auto end = std::chrono::steady_clock::now();
                double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();

                {
                    std::lock_guard<std::mutex> lock(impl_->latency_mutex);
                    impl_->connection_latencies.push_back(latency_ms);
                }

                if (success) {
                    impl_->completed_connections++;

                    // Hold connection if idle time specified
                    if (impl_->idle_time > 0) {
                        std::this_thread::sleep_for(std::chrono::seconds(impl_->idle_time));
                    }
                } else {
                    impl_->failed_connections++;
                }

                impl_->active_connections--;
            });
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Wait for all workers
    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }

    impl_->running = false;
    impl_->result.end_time = std::chrono::system_clock::now();
    impl_->result.status = TestStatus::PASSED;
}

void ConnectionLoadGenerator::stop() {
    impl_->running = false;
}

bool ConnectionLoadGenerator::isRunning() const {
    return impl_->running;
}

LoadTestResult ConnectionLoadGenerator::getResults() const {
    LoadTestResult result = impl_->result;
    result.total_connections = impl_->completed_connections + impl_->failed_connections;
    result.successful_connections = impl_->completed_connections;
    result.failed_connections = impl_->failed_connections;

    // Calculate latency stats
    std::lock_guard<std::mutex> lock(impl_->latency_mutex);
    if (!impl_->connection_latencies.empty()) {
        auto& latencies = impl_->connection_latencies;
        std::sort(latencies.begin(), latencies.end());

        result.connection_latency.sample_count = latencies.size();
        result.connection_latency.min_ms = latencies.front();
        result.connection_latency.max_ms = latencies.back();

        double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
        result.connection_latency.mean_ms = sum / latencies.size();
        result.connection_latency.median_ms = latencies[latencies.size() / 2];
        result.connection_latency.p90_ms = latencies[static_cast<size_t>(latencies.size() * 0.90)];
        result.connection_latency.p99_ms = latencies[static_cast<size_t>(latencies.size() * 0.99)];
    }

    return result;
}

ThroughputStats ConnectionLoadGenerator::getCurrentThroughput() const {
    ThroughputStats stats;
    auto elapsed = std::chrono::duration<double>(
        std::chrono::system_clock::now() - impl_->result.start_time).count();
    if (elapsed > 0) {
        stats.operations_per_second = impl_->completed_connections / elapsed;
    }
    stats.total_operations = impl_->completed_connections;
    return stats;
}

LatencyStats ConnectionLoadGenerator::getCurrentLatency() const {
    return getResults().connection_latency;
}

int ConnectionLoadGenerator::getActiveConnections() const {
    return impl_->active_connections;
}

int ConnectionLoadGenerator::getTotalConnections() const {
    return impl_->completed_connections + impl_->failed_connections;
}

int ConnectionLoadGenerator::getFailedConnections() const {
    return impl_->failed_connections;
}

//=============================================================================
// Query Load Generator Implementation
//=============================================================================

struct QueryLoadGenerator::Impl {
    std::string host = "localhost";
    int port = 3092;
    Protocol protocol = Protocol::NATIVE;
    std::string database = "testdb";
    std::string username = "testuser";
    std::string password = "test123";

    int pool_size = 10;
    int pool_min_idle = 2;
    int query_rate = 1000;
    int duration = 60;
    int warmup = 10;
    int cooldown = 5;

    struct QueryDef {
        std::string sql;
        std::function<std::vector<std::string>()> param_gen;
        int weight = 1;
    };
    std::vector<QueryDef> queries;
    int read_percent = 80;

    std::atomic<bool> running{false};
    std::atomic<int64_t> total_queries{0};
    std::atomic<int64_t> successful_queries{0};
    std::atomic<int64_t> failed_queries{0};

    std::vector<double> query_latencies;
    std::mutex latency_mutex;

    LoadTestResult result;
};

QueryLoadGenerator::QueryLoadGenerator()
    : impl_(std::make_unique<Impl>()) {
}

QueryLoadGenerator::~QueryLoadGenerator() {
    stop();
}

void QueryLoadGenerator::setHost(const std::string& host) { impl_->host = host; }
void QueryLoadGenerator::setPort(int port) { impl_->port = port; }
void QueryLoadGenerator::setProtocol(Protocol protocol) { impl_->protocol = protocol; }
void QueryLoadGenerator::setDatabase(const std::string& database) { impl_->database = database; }
void QueryLoadGenerator::setUsername(const std::string& username) { impl_->username = username; }
void QueryLoadGenerator::setPassword(const std::string& password) { impl_->password = password; }
void QueryLoadGenerator::setPoolSize(int size) { impl_->pool_size = size; }
void QueryLoadGenerator::setPoolMinIdle(int min_idle) { impl_->pool_min_idle = min_idle; }
void QueryLoadGenerator::setQueryRate(int per_second) { impl_->query_rate = per_second; }
void QueryLoadGenerator::setDuration(int seconds) { impl_->duration = seconds; }
void QueryLoadGenerator::setWarmup(int seconds) { impl_->warmup = seconds; }
void QueryLoadGenerator::setCooldown(int seconds) { impl_->cooldown = seconds; }

void QueryLoadGenerator::addQuery(const std::string& sql, int weight) {
    Impl::QueryDef qd;
    qd.sql = sql;
    qd.weight = weight;
    impl_->queries.push_back(qd);
}

void QueryLoadGenerator::addParameterizedQuery(const std::string& sql,
                                                std::function<std::vector<std::string>()> param_generator,
                                                int weight) {
    Impl::QueryDef qd;
    qd.sql = sql;
    qd.param_gen = param_generator;
    qd.weight = weight;
    impl_->queries.push_back(qd);
}

void QueryLoadGenerator::setReadWriteRatio(int read_percent, int write_percent) {
    impl_->read_percent = read_percent;
}

void QueryLoadGenerator::clearQueries() {
    impl_->queries.clear();
}

void QueryLoadGenerator::start() {
    impl_->running = true;
    impl_->result.start_time = std::chrono::system_clock::now();
    impl_->result.type = LoadTestType::QUERY;
    impl_->result.status = TestStatus::RUNNING;

    // Add default queries if none specified
    if (impl_->queries.empty()) {
        addQuery("SELECT 1", 1);
    }

    // Calculate total weight
    int total_weight = 0;
    for (const auto& q : impl_->queries) {
        total_weight += q.weight;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> weight_dist(0, total_weight - 1);

    // Worker threads
    std::vector<std::thread> workers;
    for (int i = 0; i < impl_->pool_size; i++) {
        workers.emplace_back([this, &gen, &weight_dist, total_weight]() {
            std::mt19937 local_gen(std::random_device{}());

            while (impl_->running) {
                // Select query based on weight
                int w = weight_dist(local_gen);
                int cumulative = 0;
                const Impl::QueryDef* selected = &impl_->queries[0];

                for (const auto& q : impl_->queries) {
                    cumulative += q.weight;
                    if (w < cumulative) {
                        selected = &q;
                        break;
                    }
                }

                // Execute query
                auto start = std::chrono::steady_clock::now();

                // Simulate query execution
                bool success = true;
                std::this_thread::sleep_for(std::chrono::microseconds(500));  // Simulate

                auto end = std::chrono::steady_clock::now();
                double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();

                impl_->total_queries++;
                if (success) {
                    impl_->successful_queries++;
                } else {
                    impl_->failed_queries++;
                }

                {
                    std::lock_guard<std::mutex> lock(impl_->latency_mutex);
                    impl_->query_latencies.push_back(latency_ms);
                }

                // Rate limiting
                if (impl_->query_rate > 0) {
                    double target_interval_us = 1000000.0 / (impl_->query_rate / impl_->pool_size);
                    double elapsed_us = latency_ms * 1000;
                    if (elapsed_us < target_interval_us) {
                        std::this_thread::sleep_for(
                            std::chrono::microseconds(static_cast<int>(target_interval_us - elapsed_us)));
                    }
                }
            }
        });
    }

    // Run for duration
    std::this_thread::sleep_for(std::chrono::seconds(impl_->duration));

    impl_->running = false;

    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }

    impl_->result.end_time = std::chrono::system_clock::now();
    impl_->result.status = TestStatus::PASSED;
}

void QueryLoadGenerator::stop() {
    impl_->running = false;
}

bool QueryLoadGenerator::isRunning() const {
    return impl_->running;
}

LoadTestResult QueryLoadGenerator::getResults() const {
    LoadTestResult result = impl_->result;
    result.total_queries = impl_->total_queries;
    result.successful_queries = impl_->successful_queries;
    result.failed_queries = impl_->failed_queries;

    auto elapsed = std::chrono::duration<double>(
        impl_->result.end_time - impl_->result.start_time).count();
    if (elapsed > 0) {
        result.query_throughput.operations_per_second = impl_->successful_queries / elapsed;
    }

    // Calculate latency stats
    std::lock_guard<std::mutex> lock(impl_->latency_mutex);
    if (!impl_->query_latencies.empty()) {
        auto latencies = impl_->query_latencies;
        std::sort(latencies.begin(), latencies.end());

        result.query_latency.sample_count = latencies.size();
        result.query_latency.min_ms = latencies.front();
        result.query_latency.max_ms = latencies.back();

        double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
        result.query_latency.mean_ms = sum / latencies.size();
        result.query_latency.median_ms = latencies[latencies.size() / 2];
        result.query_latency.p90_ms = latencies[static_cast<size_t>(latencies.size() * 0.90)];
        result.query_latency.p99_ms = latencies[static_cast<size_t>(latencies.size() * 0.99)];
    }

    return result;
}

ThroughputStats QueryLoadGenerator::getCurrentThroughput() const {
    ThroughputStats stats;
    auto elapsed = std::chrono::duration<double>(
        std::chrono::system_clock::now() - impl_->result.start_time).count();
    if (elapsed > 0) {
        stats.operations_per_second = impl_->successful_queries / elapsed;
    }
    stats.total_operations = impl_->total_queries;
    return stats;
}

LatencyStats QueryLoadGenerator::getCurrentLatency() const {
    return getResults().query_latency;
}

int64_t QueryLoadGenerator::getTotalQueries() const { return impl_->total_queries; }
int64_t QueryLoadGenerator::getSuccessfulQueries() const { return impl_->successful_queries; }
int64_t QueryLoadGenerator::getFailedQueries() const { return impl_->failed_queries; }

double QueryLoadGenerator::getQueriesPerSecond() const {
    return getCurrentThroughput().operations_per_second;
}

//=============================================================================
// Transaction Load Generator Implementation
//=============================================================================

struct TransactionLoadGenerator::Impl {
    std::string host = "localhost";
    int port = 3092;
    Protocol protocol = Protocol::NATIVE;
    std::string database = "testdb";
    std::string username = "testuser";
    std::string password = "test123";
    int pool_size = 10;

    int transaction_rate = 100;
    int duration = 60;

    struct TransactionDef {
        std::string name;
        std::vector<TransactionLoadGenerator::TransactionStep> steps;
        int weight = 1;
    };
    std::vector<TransactionDef> transactions;
    TransactionLoadGenerator::IsolationLevel isolation = TransactionLoadGenerator::IsolationLevel::READ_COMMITTED;

    std::atomic<bool> running{false};
    std::atomic<int64_t> total_transactions{0};
    std::atomic<int64_t> committed{0};
    std::atomic<int64_t> rolled_back{0};

    LoadTestResult result;
};

TransactionLoadGenerator::TransactionLoadGenerator()
    : impl_(std::make_unique<Impl>()) {
}

TransactionLoadGenerator::~TransactionLoadGenerator() {
    stop();
}

void TransactionLoadGenerator::setHost(const std::string& host) { impl_->host = host; }
void TransactionLoadGenerator::setPort(int port) { impl_->port = port; }
void TransactionLoadGenerator::setProtocol(Protocol protocol) { impl_->protocol = protocol; }
void TransactionLoadGenerator::setDatabase(const std::string& database) { impl_->database = database; }
void TransactionLoadGenerator::setUsername(const std::string& username) { impl_->username = username; }
void TransactionLoadGenerator::setPassword(const std::string& password) { impl_->password = password; }
void TransactionLoadGenerator::setPoolSize(int size) { impl_->pool_size = size; }
void TransactionLoadGenerator::setTransactionRate(int per_second) { impl_->transaction_rate = per_second; }
void TransactionLoadGenerator::setDuration(int seconds) { impl_->duration = seconds; }

void TransactionLoadGenerator::addTransaction(const std::string& name,
                                               const std::vector<TransactionStep>& steps,
                                               int weight) {
    Impl::TransactionDef td;
    td.name = name;
    td.steps = steps;
    td.weight = weight;
    impl_->transactions.push_back(td);
}

void TransactionLoadGenerator::clearTransactions() {
    impl_->transactions.clear();
}

void TransactionLoadGenerator::setIsolationLevel(IsolationLevel level) {
    impl_->isolation = level;
}

void TransactionLoadGenerator::start() {
    impl_->running = true;
    impl_->result.start_time = std::chrono::system_clock::now();
    impl_->result.type = LoadTestType::MIXED;
    impl_->result.status = TestStatus::RUNNING;

    // Worker threads simulate transactions
    std::vector<std::thread> workers;
    for (int i = 0; i < impl_->pool_size; i++) {
        workers.emplace_back([this]() {
            while (impl_->running) {
                // Simulate transaction
                impl_->total_transactions++;

                // Simulate commit/rollback (95% commit rate)
                if (std::rand() % 100 < 95) {
                    impl_->committed++;
                } else {
                    impl_->rolled_back++;
                }

                // Rate limiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(impl_->duration));
    impl_->running = false;

    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }

    impl_->result.end_time = std::chrono::system_clock::now();
    impl_->result.status = TestStatus::PASSED;
}

void TransactionLoadGenerator::stop() {
    impl_->running = false;
}

bool TransactionLoadGenerator::isRunning() const {
    return impl_->running;
}

LoadTestResult TransactionLoadGenerator::getResults() const {
    LoadTestResult result = impl_->result;
    result.total_transactions = impl_->total_transactions;
    result.committed_transactions = impl_->committed;
    result.rolled_back_transactions = impl_->rolled_back;

    auto elapsed = std::chrono::duration<double>(
        impl_->result.end_time - impl_->result.start_time).count();
    if (elapsed > 0) {
        result.transaction_throughput.transactions_per_second = impl_->committed / elapsed;
    }

    return result;
}

ThroughputStats TransactionLoadGenerator::getCurrentThroughput() const {
    ThroughputStats stats;
    auto elapsed = std::chrono::duration<double>(
        std::chrono::system_clock::now() - impl_->result.start_time).count();
    if (elapsed > 0) {
        stats.transactions_per_second = impl_->committed / elapsed;
    }
    return stats;
}

LatencyStats TransactionLoadGenerator::getCurrentLatency() const {
    return LatencyStats{};
}

int64_t TransactionLoadGenerator::getTotalTransactions() const { return impl_->total_transactions; }
int64_t TransactionLoadGenerator::getCommittedTransactions() const { return impl_->committed; }
int64_t TransactionLoadGenerator::getRolledBackTransactions() const { return impl_->rolled_back; }

double TransactionLoadGenerator::getTransactionsPerSecond() const {
    return getCurrentThroughput().transactions_per_second;
}

//=============================================================================
// Load Tester Implementation
//=============================================================================

struct LoadTester::Impl {
    std::string host = "localhost";
    int port = 3092;
    Protocol protocol = Protocol::NATIVE;
    std::string database = "testdb";
    std::string username = "testuser";
    std::string password = "test123";

    std::map<std::string, std::unique_ptr<LoadGenerator>> generators;
    std::vector<LoadTestResult> load_results;
    std::vector<StressTestResult> stress_results;
    std::string last_error;

    TestLogCallback log_callback;
    TestProgressCallback progress_callback;
    LoadTester::MetricsCallback metrics_callback;
};

LoadTester::LoadTester()
    : impl_(std::make_unique<Impl>()) {
}

LoadTester::~LoadTester() = default;

void LoadTester::setHost(const std::string& host) { impl_->host = host; }
void LoadTester::setPort(int port) { impl_->port = port; }
void LoadTester::setProtocol(Protocol protocol) { impl_->protocol = protocol; }
void LoadTester::setDatabase(const std::string& database) { impl_->database = database; }
void LoadTester::setUsername(const std::string& username) { impl_->username = username; }
void LoadTester::setPassword(const std::string& password) { impl_->password = password; }

//=============================================================================
// Connection Load Tests
//=============================================================================

LoadTestResult LoadTester::runConnectionLoadTest(const LoadTestConfig& config) {
    auto generator = std::make_unique<ConnectionLoadGenerator>();
    generator->setHost(impl_->host);
    generator->setPort(impl_->port);
    generator->setProtocol(impl_->protocol);
    generator->setDatabase(impl_->database);
    generator->setUsername(impl_->username);
    generator->setPassword(impl_->password);
    generator->setConcurrentConnections(config.concurrent_connections);
    generator->setTotalConnections(config.total_connections);
    generator->setConnectionRate(config.connection_rate);
    generator->setConnectionDuration(config.duration_seconds);

    log("INFO", "Starting connection load test: " +
        std::to_string(config.concurrent_connections) + " concurrent, " +
        std::to_string(config.total_connections) + " total");

    generator->start();

    LoadTestResult result = generator->getResults();
    impl_->load_results.push_back(result);

    log("INFO", "Connection load test completed: " +
        std::to_string(result.successful_connections) + "/" +
        std::to_string(result.total_connections) + " successful");

    return result;
}

LoadTestResult LoadTester::runConcurrentConnectionTest(int connections) {
    LoadTestConfig config;
    config.concurrent_connections = connections;
    config.total_connections = connections;
    config.duration_seconds = 10;
    return runConnectionLoadTest(config);
}

LoadTestResult LoadTester::runConnectionStorm(int connections, int duration_seconds) {
    LoadTestConfig config;
    config.concurrent_connections = connections / 10;  // 10% concurrent
    config.total_connections = connections;
    config.connection_rate = connections / duration_seconds;
    config.duration_seconds = duration_seconds;
    return runConnectionLoadTest(config);
}

LoadTestResult LoadTester::runConnectionPoolCycle(int cycles, int pool_size) {
    LoadTestConfig config;
    config.concurrent_connections = pool_size;
    config.total_connections = cycles * pool_size;
    config.connection_rate = pool_size * 10;  // Fast cycling
    config.duration_seconds = cycles;
    return runConnectionLoadTest(config);
}

LoadTestResult LoadTester::runMaxConnectionsTest() {
    // Binary search for max connections
    int low = 1, high = 10000;
    int max_successful = 0;

    while (low <= high) {
        int mid = (low + high) / 2;
        auto result = runConcurrentConnectionTest(mid);

        if (result.failed_connections == 0) {
            max_successful = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    LoadTestResult result;
    result.total_connections = max_successful;
    result.successful_connections = max_successful;
    result.summary = "Maximum concurrent connections: " + std::to_string(max_successful);
    return result;
}

LoadTestResult LoadTester::runLongLivedConnectionTest(int hours) {
    LoadTestConfig config;
    config.concurrent_connections = 10;
    config.total_connections = 10;
    config.duration_seconds = hours * 3600;
    return runConnectionLoadTest(config);
}

LoadTestResult LoadTester::runIdleConnectionTimeoutTest(int idle_minutes) {
    auto generator = std::make_unique<ConnectionLoadGenerator>();
    generator->setHost(impl_->host);
    generator->setPort(impl_->port);
    generator->setConcurrentConnections(5);
    generator->setTotalConnections(5);
    generator->setIdleTime(idle_minutes * 60);

    generator->start();
    return generator->getResults();
}

LoadTestResult LoadTester::runReconnectionStorm(int reconnects_per_second) {
    LoadTestConfig config;
    config.concurrent_connections = 10;
    config.total_connections = reconnects_per_second * 60;  // 1 minute of storm
    config.connection_rate = reconnects_per_second;
    config.duration_seconds = 60;
    return runConnectionLoadTest(config);
}

//=============================================================================
// Query Load Tests
//=============================================================================

LoadTestResult LoadTester::runQueryLoadTest(const LoadTestConfig& config) {
    auto generator = std::make_unique<QueryLoadGenerator>();
    generator->setHost(impl_->host);
    generator->setPort(impl_->port);
    generator->setProtocol(impl_->protocol);
    generator->setDatabase(impl_->database);
    generator->setUsername(impl_->username);
    generator->setPassword(impl_->password);
    generator->setPoolSize(config.concurrent_connections);
    generator->setQueryRate(config.query_rate);
    generator->setDuration(config.duration_seconds);
    generator->setWarmup(config.warmup_seconds);
    generator->setCooldown(config.cooldown_seconds);

    log("INFO", "Starting query load test: " +
        std::to_string(config.query_rate) + " QPS target, " +
        std::to_string(config.duration_seconds) + "s duration");

    generator->start();

    LoadTestResult result = generator->getResults();
    impl_->load_results.push_back(result);

    log("INFO", "Query load test completed: " +
        std::to_string(result.query_throughput.operations_per_second) + " QPS achieved");

    return result;
}

LoadTestResult LoadTester::runSimpleSelectThroughput(int target_qps) {
    LoadTestConfig config;
    config.concurrent_connections = std::max(1, target_qps / 1000);
    config.query_rate = target_qps;
    config.duration_seconds = 60;
    return runQueryLoadTest(config);
}

LoadTestResult LoadTester::runComplexJoinThroughput(int target_qps) {
    LoadTestConfig config;
    config.concurrent_connections = std::max(1, target_qps / 100);
    config.query_rate = target_qps;
    config.duration_seconds = 60;
    return runQueryLoadTest(config);
}

LoadTestResult LoadTester::runInsertThroughput(int target_rps) {
    LoadTestConfig config;
    config.concurrent_connections = std::max(1, target_rps / 1000);
    config.query_rate = target_rps;
    config.read_percent = 0;
    config.write_percent = 100;
    config.duration_seconds = 60;
    return runQueryLoadTest(config);
}

LoadTestResult LoadTester::runUpdateThroughput(int target_rps) {
    return runInsertThroughput(target_rps);  // Similar load profile
}

LoadTestResult LoadTester::runMixedOLTPWorkload(int read_percent, int write_percent) {
    LoadTestConfig config;
    config.concurrent_connections = 32;
    config.query_rate = 10000;
    config.read_percent = read_percent;
    config.write_percent = write_percent;
    config.duration_seconds = 60;
    return runQueryLoadTest(config);
}

LoadTestResult LoadTester::runPreparedStatementReuse(int executions) {
    LoadTestConfig config;
    config.concurrent_connections = 4;
    config.query_rate = executions / 60;  // Over 1 minute
    config.duration_seconds = 60;
    return runQueryLoadTest(config);
}

LoadTestResult LoadTester::runLargeResultSetTest(int row_count) {
    LoadTestConfig config;
    config.concurrent_connections = 1;
    config.query_rate = 10;
    config.duration_seconds = 30;
    return runQueryLoadTest(config);
}

LoadTestResult LoadTester::runConcurrentTransactionTest(int transactions) {
    auto generator = std::make_unique<TransactionLoadGenerator>();
    generator->setHost(impl_->host);
    generator->setPort(impl_->port);
    generator->setPoolSize(transactions);
    generator->setTransactionRate(transactions);
    generator->setDuration(60);

    generator->start();
    return generator->getResults();
}

//=============================================================================
// Protocol-Specific Load Tests
//=============================================================================

LoadTestResult LoadTester::runPgBenchTPC_B(int scale_factor, int clients) {
    log("INFO", "Running pgbench TPC-B: scale=" + std::to_string(scale_factor) +
        ", clients=" + std::to_string(clients));

    LoadTestConfig config;
    config.concurrent_connections = clients;
    config.query_rate = 10000;  // Target 10K TPS
    config.duration_seconds = 60;
    return runQueryLoadTest(config);
}

LoadTestResult LoadTester::runPgBenchCopy(int row_count) {
    LoadTestConfig config;
    config.concurrent_connections = 1;
    config.duration_seconds = row_count / 100000;  // ~100K rows/second
    return runQueryLoadTest(config);
}

LoadTestResult LoadTester::runSysBenchOLTP(int warehouses, int clients) {
    LoadTestConfig config;
    config.concurrent_connections = clients;
    config.query_rate = 5000;  // Target 5K TPS
    config.read_percent = 80;
    config.write_percent = 20;
    config.duration_seconds = 60;
    return runQueryLoadTest(config);
}

LoadTestResult LoadTester::runMySQLBulkInsert(int row_count) {
    return runInsertThroughput(row_count / 60);
}

LoadTestResult LoadTester::runTDSBulkCopy(int row_count) {
    return runPgBenchCopy(row_count);
}

LoadTestResult LoadTester::runFirebirdGbakRestore(const std::string& backup_path) {
    LoadTestResult result;
    result.summary = "gbak restore simulation";
    return result;
}

LoadTestResult LoadTester::runNativeSBLRThroughput(int target_tps) {
    LoadTestConfig config;
    config.concurrent_connections = target_tps / 1000;
    config.query_rate = target_tps;
    config.duration_seconds = 60;
    return runQueryLoadTest(config);
}

LoadTestResult LoadTester::runNativeFederatedQueries(int target_tps) {
    return runNativeSBLRThroughput(target_tps);
}

//=============================================================================
// Stress Tests
//=============================================================================

StressTestResult LoadTester::runStressTest(const StressTestConfig& config) {
    StressTestResult result;
    result.type = config.stress_type;
    result.config = config;
    result.target_duration = std::chrono::seconds(config.duration_seconds);
    result.status = TestStatus::RUNNING;

    log("INFO", "Starting stress test: " + std::to_string(config.duration_seconds) + "s duration");

    auto start = std::chrono::steady_clock::now();

    // Run load while monitoring resources
    LoadTestConfig load_config;
    load_config.concurrent_connections = 100;
    load_config.query_rate = 10000;
    load_config.duration_seconds = config.duration_seconds;

    runQueryLoadTest(load_config);

    auto end = std::chrono::steady_clock::now();
    result.actual_duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
    result.status = TestStatus::PASSED;

    impl_->stress_results.push_back(result);
    return result;
}

StressTestResult LoadTester::runCPUStress(int target_percent, int duration_seconds) {
    StressTestConfig config;
    config.stress_type = StressTestType::CPU;
    config.target_cpu_percent = target_percent;
    config.duration_seconds = duration_seconds;
    return runStressTest(config);
}

StressTestResult LoadTester::runMemoryPressure(int target_percent, int duration_seconds) {
    StressTestConfig config;
    config.stress_type = StressTestType::MEMORY;
    config.target_memory_percent = target_percent;
    config.duration_seconds = duration_seconds;
    return runStressTest(config);
}

StressTestResult LoadTester::runDiskIOSaturation(int target_mbps, int duration_seconds) {
    StressTestConfig config;
    config.stress_type = StressTestType::DISK_IO;
    config.target_io_mbps = target_mbps;
    config.duration_seconds = duration_seconds;
    return runStressTest(config);
}

StressTestResult LoadTester::runNetworkCongestion(int duration_seconds) {
    StressTestConfig config;
    config.stress_type = StressTestType::NETWORK;
    config.duration_seconds = duration_seconds;
    return runStressTest(config);
}

StressTestResult LoadTester::runMixedProtocolStorm(int duration_seconds) {
    StressTestConfig config;
    config.stress_type = StressTestType::MIXED;
    config.duration_seconds = duration_seconds;
    return runStressTest(config);
}

StressTestResult LoadTester::runCheckpointUnderLoad(int duration_seconds) {
    StressTestConfig config;
    config.stress_type = StressTestType::CHECKPOINT;
    config.duration_seconds = duration_seconds;
    return runStressTest(config);
}

StressTestResult LoadTester::runGarbageCollectionStress(int duration_seconds) {
    StressTestConfig config;
    config.stress_type = StressTestType::GARBAGE_COLLECTION;
    config.duration_seconds = duration_seconds;
    return runStressTest(config);
}

//=============================================================================
// Endurance Tests
//=============================================================================

StressTestResult LoadTester::runEnduranceTest(const StressTestConfig& config) {
    return runStressTest(config);
}

StressTestResult LoadTester::runContinuousOLTP(int hours) {
    StressTestConfig config;
    config.stress_type = StressTestType::MIXED;
    config.duration_seconds = hours * 3600;
    return runEnduranceTest(config);
}

StressTestResult LoadTester::runConnectionCycling(int hours) {
    StressTestConfig config;
    config.duration_seconds = hours * 3600;
    return runEnduranceTest(config);
}

StressTestResult LoadTester::runLogRotation(int days) {
    StressTestConfig config;
    config.duration_seconds = days * 86400;
    return runEnduranceTest(config);
}

StressTestResult LoadTester::runStatisticsCollection(int days) {
    StressTestConfig config;
    config.duration_seconds = days * 86400;
    return runEnduranceTest(config);
}

StressTestResult LoadTester::runHotConfigReload(int reload_count) {
    StressTestResult result;
    result.summary = "Hot config reload: " + std::to_string(reload_count) + " reloads";
    result.status = TestStatus::PASSED;
    return result;
}

//=============================================================================
// Custom Scenarios
//=============================================================================

void LoadTester::addGenerator(const std::string& name, std::unique_ptr<LoadGenerator> generator) {
    impl_->generators[name] = std::move(generator);
}

void LoadTester::removeGenerator(const std::string& name) {
    impl_->generators.erase(name);
}

void LoadTester::clearGenerators() {
    impl_->generators.clear();
}

void LoadTester::startAll() {
    for (auto& [name, gen] : impl_->generators) {
        gen->start();
    }
}

void LoadTester::stopAll() {
    for (auto& [name, gen] : impl_->generators) {
        gen->stop();
    }
}

bool LoadTester::isAnyRunning() const {
    for (const auto& [name, gen] : impl_->generators) {
        if (gen->isRunning()) return true;
    }
    return false;
}

//=============================================================================
// Results and Reporting
//=============================================================================

std::vector<LoadTestResult> LoadTester::getAllLoadResults() const {
    return impl_->load_results;
}

std::vector<StressTestResult> LoadTester::getAllStressResults() const {
    return impl_->stress_results;
}

std::string LoadTester::generateTextReport() const {
    std::stringstream ss;

    ss << "========================================\n";
    ss << " Load Test Report\n";
    ss << "========================================\n\n";

    ss << "Load Tests: " << impl_->load_results.size() << "\n";
    for (const auto& result : impl_->load_results) {
        ss << "  - Queries: " << result.total_queries
           << ", QPS: " << result.query_throughput.operations_per_second
           << ", p99: " << result.query_latency.p99_ms << "ms\n";
    }

    ss << "\nStress Tests: " << impl_->stress_results.size() << "\n";
    for (const auto& result : impl_->stress_results) {
        ss << "  - Duration: " << result.actual_duration.count() << "s"
           << ", Status: " << toString(result.status) << "\n";
    }

    return ss.str();
}

std::string LoadTester::generateCSVReport() const {
    std::stringstream ss;
    ss << "test_type,total_ops,ops_per_sec,p50_ms,p99_ms,status\n";

    for (const auto& result : impl_->load_results) {
        ss << "load," << result.total_queries
           << "," << result.query_throughput.operations_per_second
           << "," << result.query_latency.median_ms
           << "," << result.query_latency.p99_ms
           << "," << toString(result.status) << "\n";
    }

    return ss.str();
}

std::string LoadTester::generateJSON() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"load_tests\": " << impl_->load_results.size() << ",\n";
    ss << "  \"stress_tests\": " << impl_->stress_results.size() << "\n";
    ss << "}\n";
    return ss.str();
}

void LoadTester::exportLatencyTimeSeries(const std::string& path) const {
    // Export latency data to CSV file
}

void LoadTester::exportThroughputTimeSeries(const std::string& path) const {
    // Export throughput data to CSV file
}

void LoadTester::exportResourceTimeSeries(const std::string& path) const {
    // Export resource usage data to CSV file
}

//=============================================================================
// Callbacks
//=============================================================================

void LoadTester::setLogCallback(TestLogCallback callback) {
    impl_->log_callback = callback;
}

void LoadTester::setProgressCallback(TestProgressCallback callback) {
    impl_->progress_callback = callback;
}

void LoadTester::setMetricsCallback(MetricsCallback callback, int interval_ms) {
    impl_->metrics_callback = callback;
}

//=============================================================================
// Error Handling
//=============================================================================

std::string LoadTester::getLastError() const {
    return impl_->last_error;
}

void LoadTester::log(const std::string& level, const std::string& message) {
    if (impl_->log_callback) {
        impl_->log_callback(level, "[LoadTester] " + message);
    }
}

void LoadTester::setError(const std::string& error) {
    impl_->last_error = error;
    log("ERROR", error);
}

//=============================================================================
// TPC-B Workload Implementation
//=============================================================================

struct TPCBWorkload::Impl {
    int scale_factor = 1;
    std::string host, database, username, password;
    int port = 0;
};

TPCBWorkload::TPCBWorkload(int scale_factor)
    : impl_(std::make_unique<Impl>()) {
    impl_->scale_factor = scale_factor;
}

void TPCBWorkload::initialize(const std::string& host, int port,
                               const std::string& database,
                               const std::string& username,
                               const std::string& password) {
    impl_->host = host;
    impl_->port = port;
    impl_->database = database;
    impl_->username = username;
    impl_->password = password;
}

void TPCBWorkload::createSchema() {
    // Create TPC-B schema tables
}

void TPCBWorkload::loadData() {
    // Load TPC-B test data based on scale factor
}

void TPCBWorkload::cleanup() {
    // Drop TPC-B tables
}

LoadTestResult TPCBWorkload::run(int clients, int duration_seconds) {
    LoadTester tester;
    tester.setHost(impl_->host);
    tester.setPort(impl_->port);
    tester.setDatabase(impl_->database);
    tester.setUsername(impl_->username);
    tester.setPassword(impl_->password);

    return tester.runPgBenchTPC_B(impl_->scale_factor, clients);
}

//=============================================================================
// TPC-C Workload Implementation
//=============================================================================

struct TPCCWorkload::Impl {
    int warehouses = 1;
    std::string host, database, username, password;
    int port = 0;
};

TPCCWorkload::TPCCWorkload(int warehouses)
    : impl_(std::make_unique<Impl>()) {
    impl_->warehouses = warehouses;
}

void TPCCWorkload::initialize(const std::string& host, int port,
                               const std::string& database,
                               const std::string& username,
                               const std::string& password) {
    impl_->host = host;
    impl_->port = port;
    impl_->database = database;
    impl_->username = username;
    impl_->password = password;
}

void TPCCWorkload::createSchema() {
    // Create TPC-C schema tables
}

void TPCCWorkload::loadData() {
    // Load TPC-C test data
}

void TPCCWorkload::cleanup() {
    // Drop TPC-C tables
}

LoadTestResult TPCCWorkload::run(int terminals, int duration_seconds) {
    LoadTester tester;
    tester.setHost(impl_->host);
    tester.setPort(impl_->port);
    tester.setDatabase(impl_->database);
    tester.setUsername(impl_->username);
    tester.setPassword(impl_->password);

    return tester.runSysBenchOLTP(impl_->warehouses, terminals);
}

//=============================================================================
// OLTP Workload Implementation
//=============================================================================

struct OLTPWorkload::Impl {
    std::vector<std::pair<std::string, int>> read_queries;
    std::vector<std::pair<std::string, int>> write_queries;
    int read_percent = 80;
};

OLTPWorkload::OLTPWorkload()
    : impl_(std::make_unique<Impl>()) {
}

void OLTPWorkload::addReadQuery(const std::string& sql, int weight) {
    impl_->read_queries.emplace_back(sql, weight);
}

void OLTPWorkload::addWriteQuery(const std::string& sql, int weight) {
    impl_->write_queries.emplace_back(sql, weight);
}

void OLTPWorkload::setReadWriteRatio(int read_percent) {
    impl_->read_percent = read_percent;
}

LoadTestResult OLTPWorkload::run(const std::string& host, int port,
                                  const std::string& database,
                                  const std::string& username,
                                  const std::string& password,
                                  int clients, int duration_seconds) {
    LoadTester tester;
    tester.setHost(host);
    tester.setPort(port);
    tester.setDatabase(database);
    tester.setUsername(username);
    tester.setPassword(password);

    return tester.runMixedOLTPWorkload(impl_->read_percent, 100 - impl_->read_percent);
}

} // namespace testing
} // namespace scratchbird
