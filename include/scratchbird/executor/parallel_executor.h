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

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/gpid.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <deque>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <future>

// Forward declarations
namespace scratchbird::core {
    class Database;
    class ConnectionContext;
}

namespace scratchbird::executor {

using scratchbird::core::ID;
using scratchbird::core::GPID;

// Parallel execution configuration
struct ParallelConfig {
    bool enable_parallel = true;
    uint32_t max_workers = 4;               // Maximum worker threads
    uint32_t max_workers_per_gather = 0;    // 0 => use max_workers
    uint32_t min_rows_per_worker = 10000;   // Minimum rows to parallelize
    uint32_t min_pages_per_worker = 100;    // Minimum pages per worker
    uint64_t min_parallel_table_scan_size = 0; // Additional planner threshold
    size_t work_mem_per_worker = 64 * 1024 * 1024;  // 64MB per worker
    bool enable_parallel_scan = true;
    bool enable_parallel_hash = true;
    bool enable_parallel_aggregate = true;
    bool enable_parallel_join = true;
    bool parallel_leader_participation = true;
    double parallel_setup_cost = 1000.0;    // Cost penalty for going parallel
    double parallel_tuple_cost = 0.1;       // Extra cost per tuple for parallelism
};

enum class ParallelStageKind : uint8_t {
    SCAN = 0,
    HASH_JOIN = 1,
    AGGREGATE = 2,
    GATHER_MERGE = 3
};

struct ParallelPlanDecision {
    bool eligible = false;
    uint32_t workers_planned = 0;
    bool use_gather_merge = false;
    double skew_penalty = 0.0;
    std::string rejection_reason;
};

inline auto parallelStageName(ParallelStageKind stage) -> const char *
{
    switch (stage)
    {
        case ParallelStageKind::SCAN: return "SCAN";
        case ParallelStageKind::HASH_JOIN: return "HASH_JOIN";
        case ParallelStageKind::AGGREGATE: return "AGGREGATE";
        case ParallelStageKind::GATHER_MERGE: return "GATHER_MERGE";
    }
    return "UNKNOWN";
}

inline auto evaluateParallelPlan(const ParallelConfig& config,
                                 ParallelStageKind stage,
                                 uint64_t num_rows,
                                 uint64_t num_pages,
                                 bool ordered_required,
                                 bool spill_expected,
                                 bool safety_ok = true)
    -> ParallelPlanDecision
{
    ParallelPlanDecision decision;
    decision.use_gather_merge = ordered_required;

    if (!safety_ok)
    {
        decision.rejection_reason = "snapshot_or_policy_safety_rejects_parallel";
        return decision;
    }
    if (!config.enable_parallel)
    {
        decision.rejection_reason = "parallel_execution_disabled";
        return decision;
    }

    uint32_t worker_cap = config.max_workers;
    if (config.max_workers_per_gather > 0)
    {
        worker_cap = std::min(worker_cap, config.max_workers_per_gather);
    }
    if (worker_cap <= 1)
    {
        decision.rejection_reason = "worker_budget_exhausted";
        return decision;
    }

    switch (stage)
    {
        case ParallelStageKind::SCAN:
            if (!config.enable_parallel_scan)
            {
                decision.rejection_reason = "parallel_scan_disabled";
                return decision;
            }
            break;
        case ParallelStageKind::HASH_JOIN:
            if (!config.enable_parallel_join)
            {
                decision.rejection_reason = "parallel_join_disabled";
                return decision;
            }
            if (!config.enable_parallel_hash)
            {
                decision.rejection_reason = "parallel_hash_disabled";
                return decision;
            }
            break;
        case ParallelStageKind::AGGREGATE:
            if (!config.enable_parallel_aggregate)
            {
                decision.rejection_reason = "parallel_aggregate_disabled";
                return decision;
            }
            break;
        case ParallelStageKind::GATHER_MERGE:
            if (ordered_required && !config.enable_parallel_scan &&
                !config.enable_parallel_join &&
                !config.enable_parallel_aggregate)
            {
                decision.rejection_reason = "no_parallel_stage_family_enabled";
                return decision;
            }
            break;
    }

    if (num_rows < config.min_rows_per_worker)
    {
        decision.rejection_reason = "row_estimate_below_parallel_threshold";
        return decision;
    }

    const uint64_t min_pages = std::max<uint64_t>(config.min_pages_per_worker,
                                                  config.min_parallel_table_scan_size);
    if (num_pages < min_pages)
    {
        decision.rejection_reason = "page_estimate_below_parallel_threshold";
        return decision;
    }

    if (spill_expected &&
        (stage == ParallelStageKind::HASH_JOIN ||
         stage == ParallelStageKind::AGGREGATE))
    {
        decision.rejection_reason = "spill_risk_blocks_parallel_stage";
        return decision;
    }

    const uint32_t workers_by_rows = static_cast<uint32_t>(
        std::max<uint64_t>(1, (num_rows + config.min_rows_per_worker - 1) /
                                  std::max<uint32_t>(1, config.min_rows_per_worker)));
    const uint32_t workers_by_pages = static_cast<uint32_t>(
        std::max<uint64_t>(1, (num_pages + std::max<uint64_t>(1, min_pages) - 1) /
                                  std::max<uint64_t>(1, min_pages)));
    const uint32_t workers = std::min(worker_cap,
                                      std::max(1u, std::min(workers_by_rows, workers_by_pages)));
    if (workers <= 1)
    {
        decision.rejection_reason = "parallel_degree_not_beneficial";
        return decision;
    }

    const double row_remainder_ratio =
        num_rows == 0 ? 0.0
                      : static_cast<double>(num_rows % workers) /
                            static_cast<double>(num_rows);
    const double page_remainder_ratio =
        num_pages == 0 ? 0.0
                       : static_cast<double>(num_pages % workers) /
                             static_cast<double>(num_pages);
    decision.skew_penalty = std::max(row_remainder_ratio, page_remainder_ratio);
    decision.eligible = true;
    decision.workers_planned = workers;
    return decision;
}

// Work unit for parallel execution
struct WorkUnit {
    ID table_id;
    uint32_t start_page = 0;
    uint32_t end_page = 0;
    uint32_t worker_id = 0;
    uint32_t executed_by_worker_id = 0;
    bool work_stolen = false;
    void* context = nullptr;                // Worker-specific context
};

// Result from parallel worker
struct WorkerResult {
    uint32_t worker_id = 0;
    uint32_t executed_by_worker_id = 0;
    uint32_t start_page = 0;
    uint32_t end_page = 0;
    bool work_stolen = false;
    core::Status status = core::Status::OK;
    uint64_t rows_processed = 0;
    uint64_t rows_returned = 0;
    uint64_t transfer_bytes = 0;
    double execution_time_ms = 0.0;
    std::vector<uint8_t> partial_result;    // Serialized partial result
    std::string error_message;
};

struct ParallelWorkerExecutionInfo {
    uint32_t preferred_worker_id = 0;
    uint32_t executed_by_worker_id = 0;
    uint32_t start_page = 0;
    uint32_t end_page = 0;
    uint64_t rows_processed = 0;
    bool work_stolen = false;
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

    // Get total work-steal count across the pool lifetime.
    uint64_t totalSteals() const { return work_steal_count_.load(std::memory_order_acquire); }

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
    std::vector<std::deque<std::unique_ptr<Task>>> task_queues_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> shutdown_{false};
    std::atomic<uint32_t> active_tasks_{0};
    std::atomic<uint64_t> work_steal_count_{0};

    auto hasPendingTasksLocked() const -> bool;
    auto popTaskLocked(uint32_t worker_index, bool* stole) -> std::unique_ptr<Task>;
    void workerMain(uint32_t worker_index);
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
    uint32_t morselCount() const { return morsel_count_; }
    bool localityPreferred() const { return locality_preferred_; }
    uint64_t workStealCount() const { return work_steal_count_; }
    uint64_t crossPartitionTransferBytes() const { return cross_partition_transfer_bytes_; }
    const std::string& exchangeMode() const { return exchange_mode_; }
    const std::vector<ParallelWorkerExecutionInfo>& executionInfos() const
    {
        return execution_infos_;
    }

private:
    core::Database* db_;
    WorkerPool* pool_;
    ParallelConfig config_;

    std::atomic<uint64_t> rows_processed_{0};
    uint32_t workers_used_ = 0;
    double execution_time_ms_ = 0.0;
    uint32_t morsel_count_ = 0;
    bool locality_preferred_ = false;
    uint64_t work_steal_count_ = 0;
    uint64_t cross_partition_transfer_bytes_ = 0;
    std::string exchange_mode_ = "SERIAL";
    std::vector<ParallelWorkerExecutionInfo> execution_infos_;

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
    uint32_t workers_used_ = 0;
    uint32_t morsel_count_ = 0;
    bool locality_preferred_ = false;
    uint64_t work_steal_count_ = 0;
    uint64_t cross_partition_transfer_bytes_ = 0;
    std::string exchange_mode_ = "SERIAL";
    std::vector<ParallelWorkerExecutionInfo> execution_infos_;

    WorkerResult aggregateWorker(const WorkUnit& unit);
    double mergePartialResults(const std::vector<PartialAggregateState>& partials, AggType agg_type);

public:
    uint32_t workersUsed() const { return workers_used_; }
    uint32_t morselCount() const { return morsel_count_; }
    bool localityPreferred() const { return locality_preferred_; }
    uint64_t workStealCount() const { return work_steal_count_; }
    uint64_t crossPartitionTransferBytes() const { return cross_partition_transfer_bytes_; }
    const std::string& exchangeMode() const { return exchange_mode_; }
    const std::vector<ParallelWorkerExecutionInfo>& executionInfos() const
    {
        return execution_infos_;
    }
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

    uint64_t rowsProcessed() const { return rows_processed_; }
    uint64_t matchCount() const { return match_count_; }
    uint32_t workersUsed() const { return workers_used_; }
    uint32_t morselCount() const { return morsel_count_; }
    bool localityPreferred() const { return locality_preferred_; }
    uint64_t workStealCount() const { return work_steal_count_; }
    uint64_t crossPartitionTransferBytes() const { return cross_partition_transfer_bytes_; }
    const std::string& exchangeMode() const { return exchange_mode_; }
    const std::vector<ParallelWorkerExecutionInfo>& executionInfos() const
    {
        return execution_infos_;
    }

private:
    core::Database* db_;
    WorkerPool* pool_;
    ParallelConfig config_;

    // Partitioned hash table (one per partition)
    struct HashEntry {
        std::vector<uint8_t> key;
        core::TypedValue key_value;
        std::vector<uint8_t> tuple;
    };

    struct HashPartition {
        std::unordered_multimap<uint64_t, HashEntry> entries;
        std::mutex mutex;
    };

    std::vector<HashPartition> partitions_;
    static constexpr uint32_t NUM_PARTITIONS = 64;

    std::atomic<uint64_t> rows_processed_{0};
    std::atomic<uint64_t> match_count_{0};
    uint32_t workers_used_ = 0;
    uint32_t morsel_count_ = 0;
    bool locality_preferred_ = false;
    uint64_t work_steal_count_ = 0;
    uint64_t cross_partition_transfer_bytes_ = 0;
    std::string exchange_mode_ = "SERIAL";
    std::vector<ParallelWorkerExecutionInfo> execution_infos_;

    WorkerResult buildWorker(const WorkUnit& unit,
                             const std::vector<core::CatalogManager::ColumnInfo>& columns,
                             size_t join_column_index);
    WorkerResult probeWorker(
        const WorkUnit& unit,
        const std::vector<core::CatalogManager::ColumnInfo>& columns,
        size_t join_column_index,
        const std::function<void(const uint8_t*, uint32_t, const uint8_t*, uint32_t)>& match_callback,
        std::mutex* callback_mutex);
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

    uint64_t rowsProcessed() const { return rows_processed_; }
    uint32_t workersUsed() const { return workers_used_; }
    uint32_t morselCount() const { return morsel_count_; }
    bool localityPreferred() const { return locality_preferred_; }
    uint64_t workStealCount() const { return work_steal_count_; }
    uint64_t crossPartitionTransferBytes() const { return cross_partition_transfer_bytes_; }
    const std::string& exchangeMode() const { return exchange_mode_; }
    const std::vector<ParallelWorkerExecutionInfo>& executionInfos() const
    {
        return execution_infos_;
    }

private:
    core::Database* db_;
    WorkerPool* pool_;
    ParallelConfig config_;

    uint64_t rows_processed_ = 0;
    uint32_t workers_used_ = 0;
    uint32_t morsel_count_ = 0;
    bool locality_preferred_ = false;
    uint64_t work_steal_count_ = 0;
    uint64_t cross_partition_transfer_bytes_ = 0;
    std::string exchange_mode_ = "SERIAL";
    std::vector<ParallelWorkerExecutionInfo> execution_infos_;

    void sortPartition(std::vector<std::vector<uint8_t>>& partition,
                      const std::function<int(const std::vector<uint8_t>&, const std::vector<uint8_t>&)>& comparator);
    void mergePartitions(std::vector<std::vector<std::vector<uint8_t>>>& partitions,
                        std::vector<std::vector<uint8_t>>& result,
                        const std::function<int(const std::vector<uint8_t>&, const std::vector<uint8_t>&)>& comparator);
};

// Parallel window executor
class ParallelWindow {
public:
    ParallelWindow(core::Database* db, WorkerPool* pool, const ParallelConfig& config);
    ~ParallelWindow();

    // Execute parallel ROW_NUMBER over already ordered window input.
    core::Status executeRowNumber(
        const std::vector<std::vector<uint8_t>>& data,
        const std::function<bool(const std::vector<uint8_t>&, const std::vector<uint8_t>&)>& same_partition,
        std::vector<uint64_t>* row_numbers_out,
        core::ErrorContext* ctx = nullptr);

    uint64_t rowsProcessed() const { return rows_processed_; }
    uint32_t workersUsed() const { return workers_used_; }
    uint32_t morselCount() const { return morsel_count_; }
    bool localityPreferred() const { return locality_preferred_; }
    uint64_t workStealCount() const { return work_steal_count_; }
    uint64_t crossPartitionTransferBytes() const { return cross_partition_transfer_bytes_; }
    const std::string& exchangeMode() const { return exchange_mode_; }
    const std::vector<ParallelWorkerExecutionInfo>& executionInfos() const
    {
        return execution_infos_;
    }

private:
    core::Database* db_;
    WorkerPool* pool_;
    ParallelConfig config_;

    uint64_t rows_processed_ = 0;
    uint32_t workers_used_ = 0;
    uint32_t morsel_count_ = 0;
    bool locality_preferred_ = false;
    uint64_t work_steal_count_ = 0;
    uint64_t cross_partition_transfer_bytes_ = 0;
    std::string exchange_mode_ = "SERIAL";
    std::vector<ParallelWorkerExecutionInfo> execution_infos_;
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
