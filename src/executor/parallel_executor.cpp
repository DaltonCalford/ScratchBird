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
// P2-20: Parallel Query Execution Implementation
//
// November 25, 2025

#include "scratchbird/executor/parallel_executor.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/logger.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace scratchbird::executor {

using namespace scratchbird::core;

namespace
{

auto effectiveWorkerCap(const ParallelConfig& config) -> uint32_t
{
    uint32_t worker_cap = config.max_workers;
    if (config.max_workers_per_gather > 0)
    {
        worker_cap = std::min(worker_cap, config.max_workers_per_gather);
    }
    return worker_cap;
}

auto shouldParallelizeWithConfig(const ParallelConfig& config,
                                 bool initialized,
                                 uint64_t num_rows,
                                 uint64_t num_pages) -> bool
{
    if (!initialized) {
        return false;
    }
    if (!config.enable_parallel) {
        return false;
    }
    if (effectiveWorkerCap(config) <= 1) {
        return false;
    }
    if (!config.enable_parallel_scan) {
        return false;
    }
    if (num_rows < config.min_rows_per_worker) {
        return false;
    }
    if (num_pages < config.min_pages_per_worker) {
        return false;
    }
    return true;
}

} // namespace

// =================================================================================================
// WorkerPool Implementation
// =================================================================================================

WorkerPool::WorkerPool(uint32_t num_workers)
    : num_workers_(num_workers)
{
    workers_.reserve(num_workers);
    for (uint32_t i = 0; i < num_workers; ++i) {
        workers_.emplace_back(&WorkerPool::workerMain, this);
    }

    LOG_INFO(GENERAL, "WorkerPool created with %u workers", num_workers);
}

WorkerPool::~WorkerPool()
{
    shutdown();
}

std::future<WorkerResult> WorkerPool::submit(WorkUnit unit, WorkerFunction func)
{
    auto task = std::make_unique<Task>();
    task->unit = std::move(unit);
    task->func = std::move(func);
    auto future = task->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (shutdown_) {
            task->promise.set_value(WorkerResult{0, Status::CANCELLED, 0, 0, 0, {}, "Pool shutdown"});
            return future;
        }
        task_queue_.push(std::move(task));
        ++active_tasks_;
    }

    queue_cv_.notify_one();
    return future;
}

std::vector<std::future<WorkerResult>> WorkerPool::submitBatch(
    const std::vector<WorkUnit>& units, WorkerFunction func)
{
    std::vector<std::future<WorkerResult>> futures;
    futures.reserve(units.size());

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (shutdown_) {
            return futures;
        }

        for (const auto& unit : units) {
            auto task = std::make_unique<Task>();
            task->unit = unit;
            task->func = func;
            futures.push_back(task->promise.get_future());
            task_queue_.push(std::move(task));
            ++active_tasks_;
        }
    }

    queue_cv_.notify_all();
    return futures;
}

void WorkerPool::waitAll()
{
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_cv_.wait(lock, [this] {
        return task_queue_.empty() && active_tasks_ == 0;
    });
}

size_t WorkerPool::pendingTasks() const
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return task_queue_.size();
}

void WorkerPool::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (shutdown_) return;
        shutdown_ = true;
    }

    queue_cv_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    workers_.clear();
    LOG_INFO(GENERAL, "WorkerPool shutdown complete");
}

void WorkerPool::workerMain()
{
    while (true) {
        std::unique_ptr<Task> task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return shutdown_ || !task_queue_.empty();
            });

            if (shutdown_ && task_queue_.empty()) {
                return;
            }

            if (!task_queue_.empty()) {
                task = std::move(task_queue_.front());
                task_queue_.pop();
            }
        }

        if (task) {
            auto start_time = std::chrono::steady_clock::now();

            try {
                WorkerResult result = task->func(task->unit);
                auto end_time = std::chrono::steady_clock::now();
                result.execution_time_ms = std::chrono::duration<double, std::milli>(
                    end_time - start_time).count();
                task->promise.set_value(std::move(result));
            } catch (const std::exception& e) {
                WorkerResult error_result;
                error_result.status = Status::INTERNAL_ERROR;
                error_result.error_message = e.what();
                task->promise.set_value(std::move(error_result));
            }

            --active_tasks_;
            queue_cv_.notify_all();
        }
    }
}

// =================================================================================================
// ParallelScan Implementation
// =================================================================================================

ParallelScan::ParallelScan(Database* db, WorkerPool* pool, const ParallelConfig& config)
    : db_(db), pool_(pool), config_(config)
{
}

ParallelScan::~ParallelScan() = default;

Status ParallelScan::execute(const ID& table_id,
                             const std::function<void(const uint8_t*, uint32_t)>& row_callback,
                             ErrorContext* ctx)
{
    if (!db_ || !pool_) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database or pool not initialized");
        return Status::INVALID_ARGUMENT;
    }

    auto start_time = std::chrono::steady_clock::now();
    rows_processed_ = 0;
    uint32_t num_workers = pool_->numWorkers();

    if (num_workers > 1) {
        LOG_WARNING(GENERAL,
                    "ParallelScan worker execution is not finalized; falling back to sequential scan");
    }
    workers_used_ = 1;
    auto scan = db_->storage_engine()->createScan(table_id, ctx);
    if (!scan) {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Failed to create scan");
        return Status::INTERNAL_ERROR;
    }

    Tuple tuple;
    while (scan->next(&tuple, ctx) == Status::OK) {
        row_callback(tuple.data, tuple.data_size);
        ++rows_processed_;
    }

    auto end_time = std::chrono::steady_clock::now();
    execution_time_ms_ = std::chrono::duration<double, std::milli>(
        end_time - start_time).count();

    LOG_INFO(GENERAL, "ParallelScan completed: %lu rows, %u workers, %.2f ms",
             rows_processed_.load(), workers_used_, execution_time_ms_);

    return Status::OK;
}

std::vector<WorkUnit> ParallelScan::partitionTable(const ID& table_id, uint32_t num_partitions)
{
    std::vector<WorkUnit> units;
    for (uint32_t i = 0; i < num_partitions; ++i) {
        WorkUnit unit;
        unit.table_id = table_id;
        unit.worker_id = i;
        unit.start_page = i;
        unit.end_page = i + 1;
        units.push_back(unit);
    }

    return units;
}

WorkerResult ParallelScan::scanWorker(const WorkUnit& unit)
{
    WorkerResult result;
    result.worker_id = unit.worker_id;

    // In a full implementation, we would scan pages [start_page, end_page)
    // and collect/process rows. For now, return success.
    result.status = Status::OK;
    result.rows_processed = 0; // Would be actual count

    return result;
}

// =================================================================================================
// ParallelAggregate Implementation
// =================================================================================================

ParallelAggregate::ParallelAggregate(Database* db, WorkerPool* pool, const ParallelConfig& config)
    : db_(db), pool_(pool), config_(config)
{
}

ParallelAggregate::~ParallelAggregate() = default;

Status ParallelAggregate::execute(const ID& table_id,
                                  const ID& column_id,
                                  AggType agg_type,
                                  double* result_out,
                                  ErrorContext* ctx)
{
    if (!result_out) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null result output");
        return Status::INVALID_ARGUMENT;
    }
    (void)table_id;
    (void)column_id;
    (void)agg_type;
    *result_out = 0.0;
    SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                      "Parallel aggregate execution is not implemented");
    return Status::NOT_IMPLEMENTED;
}

Status ParallelAggregate::executeGroupBy(const ID& table_id,
                                         const ID& group_column_id,
                                         const ID& agg_column_id,
                                         AggType agg_type,
                                         std::vector<std::pair<std::vector<uint8_t>, double>>* results_out,
                                         ErrorContext* ctx)
{
    if (!results_out) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null results output");
        return Status::INVALID_ARGUMENT;
    }
    (void)table_id;
    (void)group_column_id;
    (void)agg_column_id;
    (void)agg_type;
    results_out->clear();
    SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                      "Parallel GROUP BY execution is not implemented");
    return Status::NOT_IMPLEMENTED;
}

PartialAggregateState ParallelAggregate::aggregateWorker(const WorkUnit& unit,
                                                          const ID& column_id,
                                                          AggType agg_type)
{
    PartialAggregateState state;

    // In a full implementation, would scan pages and compute partial aggregate
    // For now, return empty state
    return state;
}

double ParallelAggregate::mergePartialResults(const std::vector<PartialAggregateState>& partials,
                                               AggType agg_type)
{
    if (partials.empty()) return 0.0;

    switch (agg_type) {
        case AggType::COUNT: {
            uint64_t total = 0;
            for (const auto& p : partials) {
                total += p.count;
            }
            return static_cast<double>(total);
        }

        case AggType::SUM: {
            double total = 0.0;
            for (const auto& p : partials) {
                total += p.sum;
            }
            return total;
        }

        case AggType::AVG: {
            double total_sum = 0.0;
            uint64_t total_count = 0;
            for (const auto& p : partials) {
                total_sum += p.sum;
                total_count += p.count;
            }
            return total_count > 0 ? total_sum / total_count : 0.0;
        }

        case AggType::MIN: {
            double result = 0.0;
            bool first = true;
            for (const auto& p : partials) {
                if (p.has_min) {
                    if (first || p.min_val < result) {
                        result = p.min_val;
                        first = false;
                    }
                }
            }
            return result;
        }

        case AggType::MAX: {
            double result = 0.0;
            bool first = true;
            for (const auto& p : partials) {
                if (p.has_max) {
                    if (first || p.max_val > result) {
                        result = p.max_val;
                        first = false;
                    }
                }
            }
            return result;
        }

        case AggType::VARIANCE:
        case AggType::STDDEV: {
            // Welford's parallel algorithm
            double total_count = 0;
            double total_sum = 0;
            double total_sum_sq = 0;

            for (const auto& p : partials) {
                total_count += p.count;
                total_sum += p.sum;
                total_sum_sq += p.sum_sq;
            }

            if (total_count <= 1) return 0.0;

            double mean = total_sum / total_count;
            double variance = (total_sum_sq - total_count * mean * mean) / (total_count - 1);

            if (agg_type == AggType::STDDEV) {
                return std::sqrt(variance);
            }
            return variance;
        }

        default:
            return 0.0;
    }
}

// =================================================================================================
// ParallelHashJoin Implementation
// =================================================================================================

ParallelHashJoin::ParallelHashJoin(Database* db, WorkerPool* pool, const ParallelConfig& config)
    : db_(db), pool_(pool), config_(config)
{
}

ParallelHashJoin::~ParallelHashJoin() = default;

Status ParallelHashJoin::execute(const ID& outer_table_id,
                                  const ID& outer_join_column_id,
                                  const ID& inner_table_id,
                                  const ID& inner_join_column_id,
                                  const std::function<void(const uint8_t*, uint32_t, const uint8_t*, uint32_t)>& match_callback,
                                  ErrorContext* ctx)
{
    (void)outer_table_id;
    (void)outer_join_column_id;
    (void)inner_table_id;
    (void)inner_join_column_id;
    (void)match_callback;
    SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                      "Parallel hash join execution is not implemented");
    return Status::NOT_IMPLEMENTED;
}

void ParallelHashJoin::buildWorker(const WorkUnit& unit, const ID& join_column_id)
{
    // In a full implementation, would:
    // 1. Scan pages [start_page, end_page)
    // 2. For each row, extract join column value
    // 3. Hash the value and add to appropriate partition
}

void ParallelHashJoin::probeWorker(const WorkUnit& unit, const ID& join_column_id,
                                    const std::function<void(const uint8_t*, uint32_t, const uint8_t*, uint32_t)>& match_callback)
{
    // In a full implementation, would:
    // 1. Scan pages [start_page, end_page)
    // 2. For each row, extract join column value
    // 3. Hash the value and look up in partition
    // 4. For each match, call match_callback
}

uint64_t ParallelHashJoin::hashJoinKey(const uint8_t* key_data, uint32_t key_size)
{
    // Simple hash function (FNV-1a)
    uint64_t hash = 14695981039346656037ULL;
    for (uint32_t i = 0; i < key_size; ++i) {
        hash ^= key_data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

// =================================================================================================
// ParallelSort Implementation
// =================================================================================================

ParallelSort::ParallelSort(Database* db, WorkerPool* pool, const ParallelConfig& config)
    : db_(db), pool_(pool), config_(config)
{
}

ParallelSort::~ParallelSort() = default;

Status ParallelSort::execute(std::vector<std::vector<uint8_t>>& data,
                             const std::function<int(const std::vector<uint8_t>&, const std::vector<uint8_t>&)>& comparator,
                             ErrorContext* ctx)
{
    if (data.size() <= 1) return Status::OK;

    uint32_t num_workers = std::min(static_cast<uint32_t>(pool_->numWorkers()),
                                    static_cast<uint32_t>((data.size() + 10000) / 10000));
    if (num_workers <= 1) {
        // Sequential sort
        std::sort(data.begin(), data.end(),
                 [&comparator](const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
                     return comparator(a, b) < 0;
                 });
        return Status::OK;
    }

    // Partition data
    size_t partition_size = (data.size() + num_workers - 1) / num_workers;
    std::vector<std::vector<std::vector<uint8_t>>> partitions(num_workers);

    for (size_t i = 0; i < data.size(); ++i) {
        uint32_t part_idx = static_cast<uint32_t>(i / partition_size);
        if (part_idx >= num_workers) part_idx = num_workers - 1;
        partitions[part_idx].push_back(std::move(data[i]));
    }
    data.clear();

    // Sort partitions in parallel
    std::vector<std::future<WorkerResult>> futures;
    for (uint32_t i = 0; i < num_workers; ++i) {
        WorkUnit unit;
        unit.worker_id = i;
        unit.context = &partitions[i];

        auto future = pool_->submit(unit,
            [this, &comparator](const WorkUnit& u) {
                auto* partition = static_cast<std::vector<std::vector<uint8_t>>*>(u.context);
                sortPartition(*partition, comparator);
                WorkerResult result;
                result.worker_id = u.worker_id;
                result.status = Status::OK;
                return result;
            });
        futures.push_back(std::move(future));
    }

    // Wait for sorts to complete
    for (auto& future : futures) {
        future.get();
    }

    // Merge sorted partitions
    mergePartitions(partitions, data, comparator);

    return Status::OK;
}

void ParallelSort::sortPartition(std::vector<std::vector<uint8_t>>& partition,
                                 const std::function<int(const std::vector<uint8_t>&, const std::vector<uint8_t>&)>& comparator)
{
    std::sort(partition.begin(), partition.end(),
             [&comparator](const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
                 return comparator(a, b) < 0;
             });
}

void ParallelSort::mergePartitions(std::vector<std::vector<std::vector<uint8_t>>>& partitions,
                                   std::vector<std::vector<uint8_t>>& result,
                                   const std::function<int(const std::vector<uint8_t>&, const std::vector<uint8_t>&)>& comparator)
{
    // K-way merge using min-heap
    using Entry = std::pair<size_t, size_t>; // (partition_idx, element_idx)

    auto entry_compare = [&partitions, &comparator](const Entry& a, const Entry& b) {
        return comparator(partitions[a.first][a.second], partitions[b.first][b.second]) > 0;
    };

    std::priority_queue<Entry, std::vector<Entry>, decltype(entry_compare)> heap(entry_compare);

    // Initialize heap with first element from each non-empty partition
    for (size_t i = 0; i < partitions.size(); ++i) {
        if (!partitions[i].empty()) {
            heap.push({i, 0});
        }
    }

    // Merge
    result.reserve(partitions.size() * (partitions.empty() ? 0 : partitions[0].size()));
    while (!heap.empty()) {
        auto [part_idx, elem_idx] = heap.top();
        heap.pop();

        result.push_back(std::move(partitions[part_idx][elem_idx]));

        if (elem_idx + 1 < partitions[part_idx].size()) {
            heap.push({part_idx, elem_idx + 1});
        }
    }
}

// =================================================================================================
// ParallelExecutionManager Implementation
// =================================================================================================

ParallelExecutionManager& ParallelExecutionManager::getInstance()
{
    static ParallelExecutionManager instance;
    return instance;
}

ParallelExecutionManager::~ParallelExecutionManager()
{
    shutdown();
}

void ParallelExecutionManager::initialize(const ParallelConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);

    const uint32_t requested_workers = std::max(1u, config.max_workers);
    const bool rebuild_pool =
        !initialized_ || !pool_ || pool_->numWorkers() != requested_workers;

    config_ = config;
    if (rebuild_pool) {
        if (pool_) {
            pool_->shutdown();
            pool_.reset();
        }
        pool_ = std::make_unique<WorkerPool>(requested_workers);
    }
    initialized_ = true;

    LOG_INFO(GENERAL, "ParallelExecutionManager initialized with %u workers",
             requested_workers);
}

WorkerPool* ParallelExecutionManager::getPool()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        config_ = ParallelConfig{};
        pool_ = std::make_unique<WorkerPool>(std::max(1u, config_.max_workers));
        initialized_ = true;
    }
    return pool_.get();
}

bool ParallelExecutionManager::shouldParallelize(uint64_t num_rows, uint64_t num_pages) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return shouldParallelizeWithConfig(config_, initialized_, num_rows, num_pages);
}

uint32_t ParallelExecutionManager::optimalWorkerCount(uint64_t num_rows, uint64_t num_pages) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) return 1;
    if (!shouldParallelizeWithConfig(config_, initialized_, num_rows, num_pages)) return 1;

    // Calculate based on data size
    uint32_t workers_by_rows = static_cast<uint32_t>(
        (num_rows + config_.min_rows_per_worker - 1) / config_.min_rows_per_worker);
    uint32_t workers_by_pages = static_cast<uint32_t>(
        (num_pages + config_.min_pages_per_worker - 1) / config_.min_pages_per_worker);

    uint32_t optimal = std::min(workers_by_rows, workers_by_pages);
    return std::min(optimal, effectiveWorkerCap(config_));
}

void ParallelExecutionManager::shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (pool_) {
        pool_->shutdown();
        pool_.reset();
    }
    initialized_ = false;
}

} // namespace scratchbird::executor
