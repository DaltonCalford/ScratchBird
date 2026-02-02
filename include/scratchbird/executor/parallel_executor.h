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
// P2-20: Parallel Query Execution
//
// Framework for parallel query execution including:
// - Worker pool management with configurable thread count
// - Parallel sequential scans (table partitioning)
// - Parallel aggregates (partial aggregation + merge)
// - Parallel hash joins (partitioned build + probe)
// - Result gathering and coordination
//
// November 25, 2025

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/gpid.h"
#include <memory>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <future>

namespace scratchbird::executor {

// Forward declarations
namespace scratchbird::core {
    class Database;
    class ConnectionContext;
}

using core::ID;
using core::GPID;

// Parallel execution configuration
struct ParallelConfig {
    uint32_t max_workers = 4;               // Maximum worker threads
    uint32_t min_rows_per_worker = 10000;   // Minimum rows to parallelize
    uint32_t min_pages_per_worker = 100;    // Minimum pages per worker
    size_t work_mem_per_worker = 64 * 1024 * 1024;  // 64MB per worker
    bool enable_parallel_scan = true;
    bool enable_parallel_aggregate = true;
    bool enable_parallel_join = true;
    double parallel_setup_cost = 1000.0;    // Cost penalty for going parallel
    double parallel_tuple_cost = 0.1;       // Extra cost per tuple for parallelism
};

// Work unit for parallel execution
struct WorkUnit {
    ID table_id;
    uint32_t start_page = 0;
    uint32_t end_page = 0;
    uint32_t worker_id = 0;
    void* context = nullptr;                // Worker-specific context
};

// Result from parallel worker
struct WorkerResult {
    uint32_t worker_id = 0;
    core::Status status = core::Status::OK;
    uint64_t rows_processed = 0;
    uint64_t rows_returned = 0;
    double execution_time_ms = 0.0;
    std::vector<uint8_t> partial_result;    // Serialized partial result
    std::string error_message;
};

// Partial aggregate state
struct PartialAggregateState {
    uint64_t count = 0;
    double sum = 0.0;
    double sum_sq = 0.0;                    // For variance/stddev
    double min_val = 0.0;
    double max_val = 0.0;
    bool has_min = false;
    bool has_max = false;
};

// Worker function type
using WorkerFunction = std::function<WorkerResult(const WorkUnit&)>;

// Worker thread state
enum class WorkerState : uint8_t {
    IDLE = 0,
    RUNNING = 1,
    FINISHED = 2,
    ERROR = 3
};

// Worker pool manager
class WorkerPool {
public:
    explicit WorkerPool(uint32_t num_workers);
    ~WorkerPool();

    // Disable copy
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    // Submit work to pool
    std::future<WorkerResult> submit(WorkUnit unit, WorkerFunction func);

    // Submit multiple work units
    std::vector<std::future<WorkerResult>> submitBatch(
        const std::vector<WorkUnit>& units, WorkerFunction func);

    // Wait for all submitted work to complete
    void waitAll();

    // Get number of workers
    uint32_t numWorkers() const { return num_workers_; }

    // Get number of pending tasks
    size_t pendingTasks() const;

    // Shutdown pool
    void shutdown();

private:
    struct Task {
        WorkUnit unit;
        WorkerFunction func;
        std::promise<WorkerResult> promise;
    };

    uint32_t num_workers_;
    std::vector<std::thread> workers_;
    std::queue<std::unique_ptr<Task>> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> shutdown_{false};
    std::atomic<uint32_t> active_tasks_{0};

    void workerMain();
};

// Parallel scan executor
class ParallelScan {
public:
    ParallelScan(core::Database* db, WorkerPool* pool, const ParallelConfig& config);
    ~ParallelScan();

    // Execute parallel sequential scan
    core::Status execute(const ID& table_id,
                        const std::function<void(const uint8_t*, uint32_t)>& row_callback,
                        core::ErrorContext* ctx = nullptr);

    // Get statistics
    uint64_t rowsProcessed() const { return rows_processed_; }
    uint64_t workersUsed() const { return workers_used_; }
    double executionTimeMs() const { return execution_time_ms_; }

private:
    core::Database* db_;
    WorkerPool* pool_;
    ParallelConfig config_;

    std::atomic<uint64_t> rows_processed_{0};
    uint32_t workers_used_ = 0;
    double execution_time_ms_ = 0.0;

    std::vector<WorkUnit> partitionTable(const ID& table_id, uint32_t num_partitions);
    WorkerResult scanWorker(const WorkUnit& unit);
};

// Parallel aggregate executor
class ParallelAggregate {
public:
    enum class AggType : uint8_t {
        COUNT = 0,
        SUM = 1,
        AVG = 2,
        MIN = 3,
        MAX = 4,
        STDDEV = 5,
        VARIANCE = 6
    };

    ParallelAggregate(core::Database* db, WorkerPool* pool, const ParallelConfig& config);
    ~ParallelAggregate();

    // Execute parallel aggregation
    core::Status execute(const ID& table_id,
                        const ID& column_id,
                        AggType agg_type,
                        double* result_out,
                        core::ErrorContext* ctx = nullptr);

    // Execute parallel GROUP BY aggregation
    core::Status executeGroupBy(const ID& table_id,
                               const ID& group_column_id,
                               const ID& agg_column_id,
                               AggType agg_type,
                               std::vector<std::pair<std::vector<uint8_t>, double>>* results_out,
                               core::ErrorContext* ctx = nullptr);

private:
    core::Database* db_;
    WorkerPool* pool_;
    ParallelConfig config_;

    PartialAggregateState aggregateWorker(const WorkUnit& unit, const ID& column_id, AggType agg_type);
    double mergePartialResults(const std::vector<PartialAggregateState>& partials, AggType agg_type);
};

// Parallel hash join executor
class ParallelHashJoin {
public:
    ParallelHashJoin(core::Database* db, WorkerPool* pool, const ParallelConfig& config);
    ~ParallelHashJoin();

    // Execute parallel hash join
    // Build side (outer) is hashed, probe side (inner) is scanned
    core::Status execute(const ID& outer_table_id,
                        const ID& outer_join_column_id,
                        const ID& inner_table_id,
                        const ID& inner_join_column_id,
                        const std::function<void(const uint8_t*, uint32_t, const uint8_t*, uint32_t)>& match_callback,
                        core::ErrorContext* ctx = nullptr);

private:
    core::Database* db_;
    WorkerPool* pool_;
    ParallelConfig config_;

    // Partitioned hash table (one per partition)
    struct HashPartition {
        std::unordered_multimap<uint64_t, std::vector<uint8_t>> entries;
        std::mutex mutex;
    };

    std::vector<HashPartition> partitions_;
    static constexpr uint32_t NUM_PARTITIONS = 64;

    void buildWorker(const WorkUnit& unit, const ID& join_column_id);
    void probeWorker(const WorkUnit& unit, const ID& join_column_id,
                    const std::function<void(const uint8_t*, uint32_t, const uint8_t*, uint32_t)>& match_callback);
    uint64_t hashJoinKey(const uint8_t* key_data, uint32_t key_size);
};

// Parallel sort executor
class ParallelSort {
public:
    ParallelSort(core::Database* db, WorkerPool* pool, const ParallelConfig& config);
    ~ParallelSort();

    // Execute parallel sort
    core::Status execute(std::vector<std::vector<uint8_t>>& data,
                        const std::function<int(const std::vector<uint8_t>&, const std::vector<uint8_t>&)>& comparator,
                        core::ErrorContext* ctx = nullptr);

private:
    core::Database* db_;
    WorkerPool* pool_;
    ParallelConfig config_;

    void sortPartition(std::vector<std::vector<uint8_t>>& partition,
                      const std::function<int(const std::vector<uint8_t>&, const std::vector<uint8_t>&)>& comparator);
    void mergePartitions(std::vector<std::vector<std::vector<uint8_t>>>& partitions,
                        std::vector<std::vector<uint8_t>>& result,
                        const std::function<int(const std::vector<uint8_t>&, const std::vector<uint8_t>&)>& comparator);
};

// Global parallel execution manager
class ParallelExecutionManager {
public:
    static ParallelExecutionManager& getInstance();

    // Initialize with configuration
    void initialize(const ParallelConfig& config);

    // Get worker pool
    WorkerPool* getPool();

    // Get configuration
    const ParallelConfig& getConfig() const { return config_; }

    // Check if parallelism is beneficial
    bool shouldParallelize(uint64_t num_rows, uint64_t num_pages) const;

    // Calculate optimal worker count
    uint32_t optimalWorkerCount(uint64_t num_rows, uint64_t num_pages) const;

    // Shutdown
    void shutdown();

private:
    ParallelExecutionManager() = default;
    ~ParallelExecutionManager();

    ParallelConfig config_;
    std::unique_ptr<WorkerPool> pool_;
    mutable std::mutex mutex_;
    bool initialized_ = false;
};

} // namespace scratchbird::executor
