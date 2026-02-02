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
 * Load and Stress Testing Infrastructure
 * Copyright (c) 2025 ScratchBird Project
 */

#ifndef SCRATCHBIRD_TESTING_LOADTESTER_H
#define SCRATCHBIRD_TESTING_LOADTESTER_H

#include "TestTypes.h"
#include <memory>
#include <vector>
#include <functional>
#include <atomic>

namespace scratchbird {
namespace testing {

//=============================================================================
// Load Generator - Base Class
//=============================================================================

class LoadGenerator {
public:
    virtual ~LoadGenerator() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;

    virtual LoadTestResult getResults() const = 0;
    virtual ThroughputStats getCurrentThroughput() const = 0;
    virtual LatencyStats getCurrentLatency() const = 0;
};

//=============================================================================
// Connection Load Generator
//=============================================================================

class ConnectionLoadGenerator : public LoadGenerator {
public:
    ConnectionLoadGenerator();
    ~ConnectionLoadGenerator() override;

    // Configuration
    void setHost(const std::string& host);
    void setPort(int port);
    void setProtocol(Protocol protocol);
    void setDatabase(const std::string& database);
    void setUsername(const std::string& username);
    void setPassword(const std::string& password);
    void setSSL(bool enabled);

    // Load parameters
    void setConcurrentConnections(int count);
    void setTotalConnections(int count);
    void setConnectionRate(int per_second);
    void setConnectionDuration(int seconds);
    void setIdleTime(int seconds);

    // Execution
    void start() override;
    void stop() override;
    bool isRunning() const override;

    // Results
    LoadTestResult getResults() const override;
    ThroughputStats getCurrentThroughput() const override;
    LatencyStats getCurrentLatency() const override;

    // Connection-specific metrics
    int getActiveConnections() const;
    int getTotalConnections() const;
    int getFailedConnections() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//=============================================================================
// Query Load Generator
//=============================================================================

class QueryLoadGenerator : public LoadGenerator {
public:
    QueryLoadGenerator();
    ~QueryLoadGenerator() override;

    // Configuration
    void setHost(const std::string& host);
    void setPort(int port);
    void setProtocol(Protocol protocol);
    void setDatabase(const std::string& database);
    void setUsername(const std::string& username);
    void setPassword(const std::string& password);

    // Connection pool
    void setPoolSize(int size);
    void setPoolMinIdle(int min_idle);

    // Query parameters
    void setQueryRate(int per_second);
    void setDuration(int seconds);
    void setWarmup(int seconds);
    void setCooldown(int seconds);

    // Workload
    void addQuery(const std::string& sql, int weight = 1);
    void addParameterizedQuery(const std::string& sql,
                                std::function<std::vector<std::string>()> param_generator,
                                int weight = 1);
    void setReadWriteRatio(int read_percent, int write_percent);
    void clearQueries();

    // Execution
    void start() override;
    void stop() override;
    bool isRunning() const override;

    // Results
    LoadTestResult getResults() const override;
    ThroughputStats getCurrentThroughput() const override;
    LatencyStats getCurrentLatency() const override;

    // Query-specific metrics
    int64_t getTotalQueries() const;
    int64_t getSuccessfulQueries() const;
    int64_t getFailedQueries() const;
    double getQueriesPerSecond() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//=============================================================================
// Transaction Load Generator
//=============================================================================

class TransactionLoadGenerator : public LoadGenerator {
public:
    TransactionLoadGenerator();
    ~TransactionLoadGenerator() override;

    // Configuration (similar to QueryLoadGenerator)
    void setHost(const std::string& host);
    void setPort(int port);
    void setProtocol(Protocol protocol);
    void setDatabase(const std::string& database);
    void setUsername(const std::string& username);
    void setPassword(const std::string& password);
    void setPoolSize(int size);

    // Transaction parameters
    void setTransactionRate(int per_second);
    void setDuration(int seconds);

    // Transaction definition
    struct TransactionStep {
        std::string sql;
        std::function<std::vector<std::string>()> param_generator;
        bool expect_result = false;
    };

    void addTransaction(const std::string& name,
                        const std::vector<TransactionStep>& steps,
                        int weight = 1);
    void clearTransactions();

    // Isolation level
    enum class IsolationLevel {
        READ_UNCOMMITTED,
        READ_COMMITTED,
        REPEATABLE_READ,
        SERIALIZABLE
    };
    void setIsolationLevel(IsolationLevel level);

    // Execution
    void start() override;
    void stop() override;
    bool isRunning() const override;

    // Results
    LoadTestResult getResults() const override;
    ThroughputStats getCurrentThroughput() const override;
    LatencyStats getCurrentLatency() const override;

    // Transaction-specific metrics
    int64_t getTotalTransactions() const;
    int64_t getCommittedTransactions() const;
    int64_t getRolledBackTransactions() const;
    double getTransactionsPerSecond() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//=============================================================================
// Load Tester - Orchestrator
//=============================================================================

class LoadTester {
public:
    LoadTester();
    ~LoadTester();

    // Non-copyable
    LoadTester(const LoadTester&) = delete;
    LoadTester& operator=(const LoadTester&) = delete;

    //=========================================================================
    // Configuration
    //=========================================================================

    void setHost(const std::string& host);
    void setPort(int port);
    void setProtocol(Protocol protocol);
    void setDatabase(const std::string& database);
    void setUsername(const std::string& username);
    void setPassword(const std::string& password);

    //=========================================================================
    // Connection Load Tests
    //=========================================================================

    LoadTestResult runConnectionLoadTest(const LoadTestConfig& config);

    // Standard scenarios
    LoadTestResult runConcurrentConnectionTest(int connections);
    LoadTestResult runConnectionStorm(int connections, int duration_seconds);
    LoadTestResult runConnectionPoolCycle(int cycles, int pool_size);
    LoadTestResult runMaxConnectionsTest();
    LoadTestResult runLongLivedConnectionTest(int hours);
    LoadTestResult runIdleConnectionTimeoutTest(int idle_minutes);
    LoadTestResult runReconnectionStorm(int reconnects_per_second);

    //=========================================================================
    // Query Load Tests
    //=========================================================================

    LoadTestResult runQueryLoadTest(const LoadTestConfig& config);

    // Standard scenarios
    LoadTestResult runSimpleSelectThroughput(int target_qps);
    LoadTestResult runComplexJoinThroughput(int target_qps);
    LoadTestResult runInsertThroughput(int target_rps);
    LoadTestResult runUpdateThroughput(int target_rps);
    LoadTestResult runMixedOLTPWorkload(int read_percent, int write_percent);
    LoadTestResult runPreparedStatementReuse(int executions);
    LoadTestResult runLargeResultSetTest(int row_count);
    LoadTestResult runConcurrentTransactionTest(int transactions);

    //=========================================================================
    // Protocol-Specific Load Tests
    //=========================================================================

    LoadTestResult runPgBenchTPC_B(int scale_factor, int clients);
    LoadTestResult runPgBenchCopy(int row_count);
    LoadTestResult runSysBenchOLTP(int warehouses, int clients);
    LoadTestResult runMySQLBulkInsert(int row_count);
    LoadTestResult runTDSBulkCopy(int row_count);
    LoadTestResult runFirebirdGbakRestore(const std::string& backup_path);
    LoadTestResult runNativeSBLRThroughput(int target_tps);
    LoadTestResult runNativeFederatedQueries(int target_tps);

    //=========================================================================
    // Stress Tests
    //=========================================================================

    StressTestResult runStressTest(const StressTestConfig& config);

    // Standard stress scenarios
    StressTestResult runCPUStress(int target_percent, int duration_seconds);
    StressTestResult runMemoryPressure(int target_percent, int duration_seconds);
    StressTestResult runDiskIOSaturation(int target_mbps, int duration_seconds);
    StressTestResult runNetworkCongestion(int duration_seconds);
    StressTestResult runMixedProtocolStorm(int duration_seconds);
    StressTestResult runCheckpointUnderLoad(int duration_seconds);
    StressTestResult runGarbageCollectionStress(int duration_seconds);

    //=========================================================================
    // Endurance Tests
    //=========================================================================

    StressTestResult runEnduranceTest(const StressTestConfig& config);

    // Standard endurance scenarios
    StressTestResult runContinuousOLTP(int hours);
    StressTestResult runConnectionCycling(int hours);
    StressTestResult runLogRotation(int days);
    StressTestResult runStatisticsCollection(int days);
    StressTestResult runHotConfigReload(int reload_count);

    //=========================================================================
    // Custom Scenarios
    //=========================================================================

    void addGenerator(const std::string& name, std::unique_ptr<LoadGenerator> generator);
    void removeGenerator(const std::string& name);
    void clearGenerators();

    void startAll();
    void stopAll();
    bool isAnyRunning() const;

    //=========================================================================
    // Results and Reporting
    //=========================================================================

    std::vector<LoadTestResult> getAllLoadResults() const;
    std::vector<StressTestResult> getAllStressResults() const;

    std::string generateTextReport() const;
    std::string generateCSVReport() const;
    std::string generateJSON() const;

    // Export time-series data
    void exportLatencyTimeSeries(const std::string& path) const;
    void exportThroughputTimeSeries(const std::string& path) const;
    void exportResourceTimeSeries(const std::string& path) const;

    //=========================================================================
    // Callbacks
    //=========================================================================

    void setLogCallback(TestLogCallback callback);
    void setProgressCallback(TestProgressCallback callback);

    // Real-time metrics callback
    using MetricsCallback = std::function<void(double tps, double latency_ms, const ResourceUsage&)>;
    void setMetricsCallback(MetricsCallback callback, int interval_ms = 1000);

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
// Workload Generators
//=============================================================================

// TPC-B workload
class TPCBWorkload {
public:
    TPCBWorkload(int scale_factor);

    void initialize(const std::string& host, int port,
                    const std::string& database,
                    const std::string& username,
                    const std::string& password);

    void createSchema();
    void loadData();
    void cleanup();

    LoadTestResult run(int clients, int duration_seconds);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// TPC-C workload
class TPCCWorkload {
public:
    TPCCWorkload(int warehouses);

    void initialize(const std::string& host, int port,
                    const std::string& database,
                    const std::string& username,
                    const std::string& password);

    void createSchema();
    void loadData();
    void cleanup();

    LoadTestResult run(int terminals, int duration_seconds);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Custom OLTP workload
class OLTPWorkload {
public:
    OLTPWorkload();

    void addReadQuery(const std::string& sql, int weight = 1);
    void addWriteQuery(const std::string& sql, int weight = 1);
    void setReadWriteRatio(int read_percent);

    LoadTestResult run(const std::string& host, int port,
                       const std::string& database,
                       const std::string& username,
                       const std::string& password,
                       int clients, int duration_seconds);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace testing
} // namespace scratchbird

#endif // SCRATCHBIRD_TESTING_LOADTESTER_H
