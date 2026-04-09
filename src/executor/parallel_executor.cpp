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

// Section 36 invariant: parallel_executor consumes bounded plan shapes chosen
// upstream. Execution support here does not by itself prove adaptive planning,
// broad cost-based optimization, or stable cross-version plan identity.

#include "scratchbird/executor/parallel_executor.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/plain_value_reader.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/logger.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <set>

namespace scratchbird::executor {

using namespace scratchbird::core;

namespace
{

struct ParallelScanContext
{
    const std::function<void(const uint8_t*, uint32_t)>* row_callback = nullptr;
    std::mutex* callback_mutex = nullptr;
};

struct ParallelAggregateContext
{
    std::vector<CatalogManager::ColumnInfo> table_columns;
    size_t target_column_index = 0;
    ParallelAggregate::AggType agg_type = ParallelAggregate::AggType::COUNT;
    std::vector<PartialAggregateState>* partial_states = nullptr;
};

struct ParallelGroupByState
{
    PartialAggregateState aggregate_state;
    TypedValue group_value = TypedValue::makeNull(DataType::UNKNOWN);
    bool has_group_value = false;
};

struct ParallelGroupByContext
{
    std::vector<CatalogManager::ColumnInfo> table_columns;
    size_t group_column_index = 0;
    size_t target_column_index = 0;
    bool count_star = false;
    ParallelAggregate::AggType agg_type = ParallelAggregate::AggType::COUNT;
    std::vector<std::unordered_map<std::string, ParallelGroupByState>>* partial_states = nullptr;
};

auto isZeroId(const ID& id) -> bool
{
    for (uint8_t byte : id.bytes)
    {
        if (byte != 0)
        {
            return false;
        }
    }
    return true;
}

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

auto workerReservationFits(const ParallelConfig& config, uint64_t required_bytes) -> bool
{
    return config.work_mem_per_worker == 0 || required_bytes <= config.work_mem_per_worker;
}

auto encodeParallelGroupKey(const TypedValue& value,
                            std::string* encoded_out,
                            ErrorContext* ctx) -> bool
{
    if (encoded_out == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Parallel group key output is null");
        return false;
    }

    encoded_out->clear();
    encoded_out->push_back(value.isNull() ? '\x00' : '\x01');
    if (value.isNull())
    {
        return true;
    }

    std::vector<uint8_t> payload;
    ErrorContext local_ctx;
    if (value.serializePlainValue(payload, &local_ctx) != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx,
                          local_ctx.code == Status::OK ? Status::INVALID_ARGUMENT
                                                       : local_ctx.code,
                          local_ctx.message.empty()
                              ? "Failed to encode parallel group key"
                              : local_ctx.message.c_str());
        return false;
    }

    encoded_out->append(reinterpret_cast<const char*>(payload.data()), payload.size());
    return true;
}

auto mergeParallelAggregateState(PartialAggregateState& target,
                                 const PartialAggregateState& source,
                                 ParallelAggregate::AggType agg_type) -> void
{
    switch (agg_type)
    {
        case ParallelAggregate::AggType::COUNT:
            target.count += source.count;
            break;
        case ParallelAggregate::AggType::SUM:
            target.sum += source.sum;
            target.count += source.count;
            break;
        case ParallelAggregate::AggType::AVG:
        case ParallelAggregate::AggType::STDDEV:
        case ParallelAggregate::AggType::VARIANCE:
            target.count += source.count;
            target.sum += source.sum;
            target.sum_sq += source.sum_sq;
            break;
        case ParallelAggregate::AggType::MIN:
            if (source.has_min &&
                (!target.has_min || source.min_val < target.min_val))
            {
                target.min_val = source.min_val;
                target.has_min = true;
            }
            break;
        case ParallelAggregate::AggType::MAX:
            if (source.has_max &&
                (!target.has_max || source.max_val > target.max_val))
            {
                target.max_val = source.max_val;
                target.has_max = true;
            }
            break;
    }
}

auto makeWorkerReservationMessage(const char* stage,
                                  uint64_t required_bytes,
                                  uint64_t budget_bytes) -> std::string
{
    std::string detail = "Parallel ";
    detail += stage;
    detail += " worker memory reservation (";
    detail += std::to_string(required_bytes);
    detail += " bytes) exceeds configured work_mem_per_worker (";
    detail += std::to_string(budget_bytes);
    detail += " bytes)";
    return detail;
}

auto buildParallelStorageTypeInfo(const CatalogManager::ColumnInfo& column) -> TypeInfo
{
    const uint16_t storage_type =
        (column.physical_data_type != 0) ? column.physical_data_type : column.data_type;
    TypeInfo info(static_cast<DataType>(storage_type));
    if (column.is_array)
    {
        info.type = DataType::ARRAY;
        info.element_type = static_cast<DataType>(storage_type);
    }
    info.precision = (column.type_precision != 0) ? column.type_precision : column.max_length;
    info.scale = column.type_scale;
    info.with_timezone = column.with_timezone;
    info.timezone_hint = column.timezone_hint;
    return info;
}

auto buildParallelLogicalTypeInfo(const CatalogManager::ColumnInfo& column) -> TypeInfo
{
    TypeInfo info(static_cast<DataType>(column.data_type));
    if (column.is_array)
    {
        info.type = DataType::ARRAY;
        info.element_type = static_cast<DataType>(column.data_type);
    }
    info.precision = (column.type_precision != 0) ? column.type_precision : column.max_length;
    info.scale = column.type_scale;
    info.with_timezone = column.with_timezone;
    info.timezone_hint = column.timezone_hint;
    return info;
}

auto decodeProjectedTupleValue(const uint8_t* tuple_data,
                               uint32_t tuple_size,
                               const std::vector<CatalogManager::ColumnInfo>& columns,
                               size_t target_column_index,
                               TypedValue* value_out,
                               ErrorContext* ctx) -> bool
{
    if (value_out == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Projected value output is null");
        return false;
    }
    if (tuple_data == nullptr || tuple_size < sizeof(TupleHeader))
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Tuple data is invalid");
        return false;
    }
    if (target_column_index >= columns.size())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Projected column index is out of range");
        return false;
    }

    const auto* header = reinterpret_cast<const TupleHeader*>(tuple_data);
    if (header->isDeleted())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Tuple is deleted");
        return false;
    }

    const uint8_t* null_bitmap = nullptr;
    if (header->hasNulls() && header->null_bitmap_offset > 0 &&
        header->null_bitmap_offset < tuple_size)
    {
        null_bitmap = tuple_data + header->null_bitmap_offset;
    }

    size_t data_offset = sizeof(TupleHeader);
    if (header->hasNulls() && null_bitmap != nullptr)
    {
        data_offset = header->null_bitmap_offset + ((columns.size() + 7) / 8);
    }

    for (size_t i = 0; i < columns.size(); ++i)
    {
        const bool selected = (i == target_column_index);
        if (null_bitmap != nullptr)
        {
            const size_t byte_offset = i / 8;
            const size_t bit_pos = i % 8;
            if ((null_bitmap[byte_offset] & (1u << bit_pos)) != 0)
            {
                if (selected)
                {
                    *value_out = TypedValue::makeNull(
                        static_cast<DataType>(columns[i].data_type));
                    return true;
                }
                continue;
            }
        }

        const auto& column = columns[i];
        const DataType logical_type = static_cast<DataType>(column.data_type);
        const uint16_t physical_type_code =
            (column.physical_data_type != 0) ? column.physical_data_type : column.data_type;
        const DataType physical_type = static_cast<DataType>(physical_type_code);
        const DataType storage_type = column.is_array ? DataType::ARRAY : physical_type;

        ErrorContext local_ctx;
        TypeInfo type_info = buildParallelStorageTypeInfo(column);
        size_t value_size = 0;
        if (computePlainValueSize(type_info.type,
                                  type_info,
                                  tuple_data + data_offset,
                                  tuple_size - data_offset,
                                  value_size,
                                  &local_ctx) != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx,
                              local_ctx.code == Status::OK ? Status::DATA_CORRUPTED
                                                           : local_ctx.code,
                              local_ctx.message.empty()
                                  ? "Failed to size projected tuple value"
                                  : local_ctx.message.c_str());
            return false;
        }
        if (data_offset + value_size > tuple_size)
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::DATA_CORRUPTED,
                              "Projected tuple value overruns tuple boundary");
            return false;
        }
        if (!selected)
        {
            data_offset += value_size;
            continue;
        }

        TypedValue value(storage_type);
        if (value.type() == DataType::DECIMAL ||
            value.type() == DataType::DECFLOAT16 ||
            value.type() == DataType::DECFLOAT34)
        {
            const uint32_t precision =
                (column.type_precision != 0) ? column.type_precision : column.max_length;
            value.setDecimalType(static_cast<uint8_t>(precision),
                                 static_cast<uint8_t>(column.type_scale));
        }

        std::vector<uint8_t> payload(tuple_data + data_offset,
                                     tuple_data + data_offset + value_size);
        data_offset += value_size;
        if (value.deserializePlainValue(payload, &local_ctx) != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx,
                              local_ctx.code == Status::OK ? Status::DATA_CORRUPTED
                                                           : local_ctx.code,
                              local_ctx.message.empty()
                                  ? "Failed to deserialize projected tuple value"
                                  : local_ctx.message.c_str());
            return false;
        }
        if (!column.is_array && physical_type != logical_type)
        {
            TypedValue logical_value;
            if (value.convertTo(buildParallelLogicalTypeInfo(column),
                                logical_value,
                                CastFormat::DEFAULT,
                                &local_ctx) != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx,
                                  local_ctx.code == Status::OK ? Status::INVALID_ARGUMENT
                                                               : local_ctx.code,
                                  local_ctx.message.empty()
                                      ? "Failed to cast projected tuple value"
                                      : local_ctx.message.c_str());
                return false;
            }
            value = std::move(logical_value);
        }

        *value_out = std::move(value);
        return true;
    }

    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Projected column not found in tuple");
    return false;
}

auto supportsParallelAggregateValue(const TypedValue& value) -> bool
{
    switch (value.type())
    {
        case DataType::BOOLEAN:
        case DataType::INT8:
        case DataType::INT16:
        case DataType::INT32:
        case DataType::INT64:
        case DataType::UINT8:
        case DataType::UINT16:
        case DataType::UINT32:
        case DataType::UINT64:
        case DataType::FLOAT32:
        case DataType::FLOAT64:
        case DataType::DECIMAL:
        case DataType::DECFLOAT16:
        case DataType::DECFLOAT34:
        case DataType::MONEY:
            return true;
        default:
            return false;
    }
}

auto valueToParallelAggregateDouble(const TypedValue& value,
                                    double* value_out,
                                    ErrorContext* ctx) -> bool
{
    if (value_out == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Aggregate double output is null");
        return false;
    }
    if (value.isNull())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Aggregate value is null");
        return false;
    }
    if (!supportsParallelAggregateValue(value))
    {
        SET_ERROR_CONTEXT(ctx,
                          Status::NOT_IMPLEMENTED,
                          "Parallel aggregate value type is not supported");
        return false;
    }
    *value_out = value.toDouble();
    return true;
}

auto resolveParallelColumnIndex(const std::vector<CatalogManager::ColumnInfo>& columns,
                                const ID& column_id,
                                size_t* index_out,
                                ErrorContext* ctx) -> bool
{
    if (index_out == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Column index output is null");
        return false;
    }

    for (size_t i = 0; i < columns.size(); ++i)
    {
        if (columns[i].column_id == column_id)
        {
            *index_out = i;
            return true;
        }
    }

    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Join column not found");
    return false;
}

auto extractProjectedTupleStorageBytes(
    const uint8_t* tuple_data,
    uint32_t tuple_size,
    const std::vector<CatalogManager::ColumnInfo>& columns,
    size_t target_column_index,
    std::vector<uint8_t>* value_bytes_out,
    bool* has_value_out,
    ErrorContext* ctx) -> bool
{
    if (value_bytes_out == nullptr || has_value_out == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Projected value output is null");
        return false;
    }

    value_bytes_out->clear();
    *has_value_out = false;

    const auto* header = reinterpret_cast<const TupleHeader*>(tuple_data);
    if (header->isDeleted())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Tuple is deleted");
        return false;
    }

    const uint8_t* null_bitmap = nullptr;
    if (header->hasNulls() && header->null_bitmap_offset > 0 &&
        header->null_bitmap_offset < tuple_size)
    {
        null_bitmap = tuple_data + header->null_bitmap_offset;
    }

    size_t data_offset = sizeof(TupleHeader);
    if (header->hasNulls() && null_bitmap != nullptr)
    {
        data_offset = header->null_bitmap_offset + ((columns.size() + 7) / 8);
    }

    for (size_t i = 0; i < columns.size(); ++i)
    {
        const bool selected = (i == target_column_index);
        if (null_bitmap != nullptr)
        {
            const size_t byte_offset = i / 8;
            const size_t bit_pos = i % 8;
            if ((null_bitmap[byte_offset] & (1u << bit_pos)) != 0)
            {
                if (selected)
                {
                    return true;
                }
                continue;
            }
        }

        const auto& column = columns[i];
        TypeInfo type_info = buildParallelStorageTypeInfo(column);
        size_t value_size = 0;
        ErrorContext local_ctx;
        if (computePlainValueSize(type_info.type,
                                  type_info,
                                  tuple_data + data_offset,
                                  tuple_size - data_offset,
                                  value_size,
                                  &local_ctx) != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx,
                              local_ctx.code == Status::OK ? Status::DATA_CORRUPTED
                                                           : local_ctx.code,
                              local_ctx.message.empty()
                                  ? "Failed to size projected tuple value"
                                  : local_ctx.message.c_str());
            return false;
        }
        if (data_offset + value_size > tuple_size)
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::DATA_CORRUPTED,
                              "Projected tuple value overruns tuple boundary");
            return false;
        }
        if (!selected)
        {
            data_offset += value_size;
            continue;
        }

        value_bytes_out->assign(tuple_data + data_offset,
                                tuple_data + data_offset + value_size);
        *has_value_out = true;
        return true;
    }

    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Projected column index is out of range");
    return false;
}

auto partitionTableWork(Database* db,
                        const ID& table_id,
                        uint32_t num_partitions) -> std::vector<WorkUnit>
{
    std::vector<WorkUnit> units;
    if (db == nullptr || db->page_manager() == nullptr || num_partitions == 0)
    {
        return units;
    }

    uint16_t tablespace_id = PRIMARY_TABLESPACE_ID;
    if (!isZeroId(table_id))
    {
        CatalogManager::TableInfo table_info{};
        ErrorContext table_ctx;
        if (db->catalog_manager()->getTable(table_id, table_info, &table_ctx) != Status::OK)
        {
            LOG_WARNING(GENERAL,
                        "Parallel work failed to resolve table metadata for partitioning: %s",
                        table_ctx.message.c_str());
            return units;
        }
        tablespace_id = table_info.tablespace_id;
    }

    uint32_t start_page = 0;
    uint32_t end_page_exclusive = 0;
    if (tablespace_id == PRIMARY_TABLESPACE_ID)
    {
        if (isZeroId(table_id))
        {
            start_page = Config::getInstance().getUInt("storage", "heap_scan_start_page", 7);
        }
        ErrorContext page_ctx;
        if (db->page_manager()->getTablespaceTotalPages(tablespace_id,
                                                        &end_page_exclusive,
                                                        &page_ctx) != Status::OK)
        {
            LOG_WARNING(GENERAL,
                        "Parallel work failed to resolve primary page count: %s",
                        page_ctx.message.c_str());
            return units;
        }
        start_page = std::min(start_page, end_page_exclusive);
    }
    else
    {
        std::vector<GPID> allocated_pages;
        ErrorContext page_ctx;
        if (db->page_manager()->getAllocatedPages(tablespace_id,
                                                  allocated_pages,
                                                  &page_ctx) != Status::OK)
        {
            LOG_WARNING(GENERAL,
                        "Parallel work failed to enumerate tablespace pages: %s",
                        page_ctx.message.c_str());
            return units;
        }
        end_page_exclusive = static_cast<uint32_t>(allocated_pages.size());
    }

    if (end_page_exclusive <= start_page)
    {
        return units;
    }

    const uint32_t total_pages = end_page_exclusive - start_page;
    const uint32_t worker_count = std::min(num_partitions, total_pages);
    if (worker_count == 0)
    {
        return units;
    }

    units.reserve(worker_count);
    const uint32_t base_span = total_pages / worker_count;
    const uint32_t remainder = total_pages % worker_count;
    uint32_t next_start = start_page;
    for (uint32_t i = 0; i < worker_count; ++i)
    {
        const uint32_t span = base_span + (i < remainder ? 1u : 0u);
        if (span == 0)
        {
            continue;
        }

        WorkUnit unit;
        unit.table_id = table_id;
        unit.worker_id = i;
        unit.start_page = next_start;
        unit.end_page = next_start + span;
        units.push_back(unit);
        next_start += span;
    }

    return units;
}

} // namespace

// =================================================================================================
// WorkerPool Implementation
// =================================================================================================

WorkerPool::WorkerPool(uint32_t num_workers)
    : num_workers_(num_workers)
    , task_queues_(num_workers)
{
    workers_.reserve(num_workers);
    for (uint32_t i = 0; i < num_workers; ++i) {
        workers_.emplace_back(&WorkerPool::workerMain, this, i);
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
            WorkerResult cancelled;
            cancelled.worker_id = task->unit.worker_id;
            cancelled.status = Status::CANCELLED;
            cancelled.error_message = "Pool shutdown";
            task->promise.set_value(std::move(cancelled));
            return future;
        }
        const size_t queue_index =
            task_queues_.empty() ? 0u
                                 : static_cast<size_t>(task->unit.worker_id % num_workers_);
        task_queues_[queue_index].push_back(std::move(task));
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
            const size_t queue_index =
                task_queues_.empty() ? 0u
                                     : static_cast<size_t>(task->unit.worker_id % num_workers_);
            task_queues_[queue_index].push_back(std::move(task));
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
        return !hasPendingTasksLocked() && active_tasks_ == 0;
    });
}

size_t WorkerPool::pendingTasks() const
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    size_t pending = 0;
    for (const auto& queue : task_queues_)
    {
        pending += queue.size();
    }
    return pending;
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

auto WorkerPool::hasPendingTasksLocked() const -> bool
{
    for (const auto& queue : task_queues_)
    {
        if (!queue.empty())
        {
            return true;
        }
    }
    return false;
}

auto WorkerPool::popTaskLocked(uint32_t worker_index, bool* stole) -> std::unique_ptr<Task>
{
    if (stole != nullptr)
    {
        *stole = false;
    }

    if (worker_index < task_queues_.size() && !task_queues_[worker_index].empty())
    {
        auto task = std::move(task_queues_[worker_index].front());
        task_queues_[worker_index].pop_front();
        return task;
    }

    for (size_t queue_index = 0; queue_index < task_queues_.size(); ++queue_index)
    {
        if (queue_index == worker_index || task_queues_[queue_index].empty())
        {
            continue;
        }

        auto task = std::move(task_queues_[queue_index].front());
        task_queues_[queue_index].pop_front();
        if (stole != nullptr)
        {
            *stole = true;
        }
        work_steal_count_.fetch_add(1, std::memory_order_relaxed);
        return task;
    }

    return nullptr;
}

void WorkerPool::workerMain(uint32_t worker_index)
{
    while (true) {
        std::unique_ptr<Task> task;
        bool stole = false;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return shutdown_ || hasPendingTasksLocked();
            });

            if (shutdown_ && !hasPendingTasksLocked()) {
                return;
            }

            task = popTaskLocked(worker_index, &stole);
        }

        if (task) {
            auto start_time = std::chrono::steady_clock::now();
            task->unit.executed_by_worker_id = worker_index;
            task->unit.work_stolen = stole;

            try {
                WorkerResult result = task->func(task->unit);
                auto end_time = std::chrono::steady_clock::now();
                result.worker_id = task->unit.worker_id;
                result.executed_by_worker_id = worker_index;
                result.start_page = task->unit.start_page;
                result.end_page = task->unit.end_page;
                result.work_stolen = stole;
                result.execution_time_ms = std::chrono::duration<double, std::milli>(
                    end_time - start_time).count();
                task->promise.set_value(std::move(result));
            } catch (const std::exception& e) {
                WorkerResult error_result;
                error_result.worker_id = task->unit.worker_id;
                error_result.executed_by_worker_id = worker_index;
                error_result.start_page = task->unit.start_page;
                error_result.end_page = task->unit.end_page;
                error_result.work_stolen = stole;
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
    workers_used_ = 0;
    morsel_count_ = 0;
    locality_preferred_ = false;
    work_steal_count_ = 0;
    cross_partition_transfer_bytes_ = 0;
    exchange_mode_ = "SERIAL";
    execution_infos_.clear();

    const auto runSequential = [&]() -> Status
    {
        auto scan = db_->storage_engine()->createScan(table_id, ctx);
        if (!scan)
        {
            SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Failed to create scan");
            return Status::INTERNAL_ERROR;
        }

        Tuple tuple;
        while (scan->next(&tuple, ctx) == Status::OK)
        {
            row_callback(tuple.data, tuple.data_size);
            ++rows_processed_;
        }

        workers_used_ = 1;
        morsel_count_ = 1;
        locality_preferred_ = true;
        return Status::OK;
    };

    const uint32_t requested_workers = std::max<uint32_t>(1, pool_->numWorkers());
    auto units = partitionTable(table_id, requested_workers);
    morsel_count_ = units.empty() ? 1u : static_cast<uint32_t>(units.size());
    if (requested_workers <= 1 || units.size() <= 1)
    {
        Status status = runSequential();
        if (status != Status::OK)
        {
            return status;
        }
    }
    else
    {
        std::mutex callback_mutex;
        ParallelScanContext scan_context{&row_callback, &callback_mutex};
        for (auto& unit : units)
        {
            unit.context = &scan_context;
        }

        auto futures = pool_->submitBatch(
            units,
            [this](const WorkUnit& unit)
            {
                return scanWorker(unit);
            });
        workers_used_ = static_cast<uint32_t>(units.size());
        locality_preferred_ = true;
        exchange_mode_ = "GATHER";
        const uint64_t steals_before = pool_->totalSteals();

        Status overall_status = Status::OK;
        std::string overall_error_message;
        for (auto& future : futures)
        {
            WorkerResult result = future.get();
            rows_processed_.fetch_add(result.rows_processed, std::memory_order_relaxed);
            ParallelWorkerExecutionInfo info;
            info.preferred_worker_id = result.worker_id;
            info.executed_by_worker_id = result.executed_by_worker_id;
            info.start_page = result.start_page;
            info.end_page = result.end_page;
            info.rows_processed = result.rows_processed;
            info.work_stolen = result.work_stolen;
            execution_infos_.push_back(info);
            if (result.status != Status::OK && overall_status == Status::OK)
            {
                overall_status = result.status;
                overall_error_message = result.error_message;
            }
        }
        work_steal_count_ = pool_->totalSteals() - steals_before;

        if (overall_status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx,
                              overall_status,
                              overall_error_message.empty()
                                  ? "Parallel scan worker failed"
                                  : overall_error_message.c_str());
            return overall_status;
        }
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
    return partitionTableWork(db_, table_id, num_partitions);
}

WorkerResult ParallelScan::scanWorker(const WorkUnit& unit)
{
    WorkerResult result;
    result.worker_id = unit.worker_id;
    result.executed_by_worker_id = unit.executed_by_worker_id;
    result.start_page = unit.start_page;
    result.end_page = unit.end_page;
    result.work_stolen = unit.work_stolen;
    ErrorContext ctx;
    auto scan = db_->storage_engine()->createScanRange(unit.table_id,
                                                       unit.start_page,
                                                       unit.end_page,
                                                       &ctx);
    if (!scan)
    {
        result.status = ctx.code == Status::OK ? Status::INTERNAL_ERROR : ctx.code;
        result.error_message =
            ctx.message.empty() ? "Failed to create parallel scan worker iterator" : ctx.message;
        return result;
    }

    auto* scan_context = static_cast<ParallelScanContext*>(unit.context);
    Tuple tuple{};
    Status scan_status = Status::OK;
    while ((scan_status = scan->next(&tuple, &ctx)) == Status::OK)
    {
        if (scan_context != nullptr && scan_context->row_callback != nullptr)
        {
            if (scan_context->callback_mutex != nullptr)
            {
                std::lock_guard<std::mutex> lock(*scan_context->callback_mutex);
                (*scan_context->row_callback)(tuple.data, tuple.data_size);
            }
            else
            {
                (*scan_context->row_callback)(tuple.data, tuple.data_size);
            }
        }
        ++result.rows_processed;
    }

    if (scan_status != Status::NOT_FOUND)
    {
        result.status = scan_status;
        result.error_message = ctx.message;
        return result;
    }

    result.status = Status::OK;

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
    *result_out = 0.0;
    if (db_ == nullptr || pool_ == nullptr)
    {
        SET_ERROR_CONTEXT(ctx,
                          Status::INVALID_ARGUMENT,
                          "Database or pool not initialized");
        return Status::INVALID_ARGUMENT;
    }

    std::vector<CatalogManager::ColumnInfo> columns;
    if (db_->catalog_manager()->getColumns(table_id, columns, ctx) != Status::OK)
    {
        return ctx != nullptr ? ctx->code : Status::NOT_FOUND;
    }

    size_t target_column_index = columns.size();
    for (size_t i = 0; i < columns.size(); ++i)
    {
        if (columns[i].column_id == column_id)
        {
            target_column_index = i;
            break;
        }
    }
    if (target_column_index >= columns.size())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Aggregate column not found");
        return Status::NOT_FOUND;
    }

    const uint32_t requested_workers = std::max<uint32_t>(1, pool_->numWorkers());
    auto units = partitionTableWork(db_, table_id, requested_workers);
    if (units.empty())
    {
        return Status::OK;
    }
    workers_used_ = 0;
    morsel_count_ = static_cast<uint32_t>(units.size());
    locality_preferred_ = false;
    work_steal_count_ = 0;
    cross_partition_transfer_bytes_ = 0;
    exchange_mode_ = units.size() > 1 ? "GATHER" : "SERIAL";
    execution_infos_.clear();

    ParallelAggregateContext aggregate_context;
    aggregate_context.table_columns = columns;
    aggregate_context.target_column_index = target_column_index;
    aggregate_context.agg_type = agg_type;
    for (auto& unit : units)
    {
        unit.context = &aggregate_context;
    }

    std::vector<PartialAggregateState> partials(units.size());
    aggregate_context.partial_states = &partials;

    auto collectResult = [&](const WorkerResult& worker_result) -> Status
    {
        if (worker_result.status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx,
                              worker_result.status,
                                  worker_result.error_message.empty()
                                      ? "Parallel aggregate worker failed"
                                      : worker_result.error_message.c_str());
            return worker_result.status;
        }
        return Status::OK;
    };

    if (units.size() <= 1)
    {
        WorkerResult result = aggregateWorker(units.front());
        execution_infos_.push_back(
            ParallelWorkerExecutionInfo{result.worker_id,
                                        result.executed_by_worker_id,
                                        result.start_page,
                                        result.end_page,
                                        result.rows_processed,
                                        result.work_stolen});
        workers_used_ = 1;
        morsel_count_ = 1;
        locality_preferred_ = true;
        Status status = collectResult(result);
        if (status != Status::OK)
        {
            return status;
        }
    }
    else
    {
        const uint64_t steals_before = pool_->totalSteals();
        auto futures = pool_->submitBatch(
            units,
            [this](const WorkUnit& unit)
            {
                return aggregateWorker(unit);
            });
        for (auto& future : futures)
        {
            WorkerResult result = future.get();
            execution_infos_.push_back(
                ParallelWorkerExecutionInfo{result.worker_id,
                                            result.executed_by_worker_id,
                                            result.start_page,
                                            result.end_page,
                                            result.rows_processed,
                                            result.work_stolen});
            Status status = collectResult(result);
            if (status != Status::OK)
            {
                return status;
            }
        }
        workers_used_ = static_cast<uint32_t>(execution_infos_.size());
        locality_preferred_ = true;
        work_steal_count_ = pool_->totalSteals() - steals_before;
        cross_partition_transfer_bytes_ =
            static_cast<uint64_t>(execution_infos_.size()) * sizeof(PartialAggregateState);
    }

    *result_out = mergePartialResults(partials, agg_type);
    return Status::OK;
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
    results_out->clear();
    if (db_ == nullptr || pool_ == nullptr)
    {
        SET_ERROR_CONTEXT(ctx,
                          Status::INVALID_ARGUMENT,
                          "Database or pool not initialized");
        return Status::INVALID_ARGUMENT;
    }

    std::vector<CatalogManager::ColumnInfo> columns;
    if (db_->catalog_manager()->getColumns(table_id, columns, ctx) != Status::OK)
    {
        return ctx != nullptr ? ctx->code : Status::NOT_FOUND;
    }

    size_t group_column_index = columns.size();
    if (!resolveParallelColumnIndex(columns,
                                    group_column_id,
                                    &group_column_index,
                                    ctx))
    {
        return ctx != nullptr ? ctx->code : Status::NOT_FOUND;
    }

    const bool count_star = isZeroId(agg_column_id) && agg_type == AggType::COUNT;
    size_t target_column_index = columns.size();
    if (!count_star)
    {
        if (!resolveParallelColumnIndex(columns,
                                        agg_column_id,
                                        &target_column_index,
                                        ctx))
        {
            return ctx != nullptr ? ctx->code : Status::NOT_FOUND;
        }
    }

    const uint32_t requested_workers = std::max<uint32_t>(1, pool_->numWorkers());
    auto units = partitionTableWork(db_, table_id, requested_workers);
    if (units.empty())
    {
        return Status::OK;
    }

    workers_used_ = 0;
    morsel_count_ = static_cast<uint32_t>(units.size());
    locality_preferred_ = false;
    work_steal_count_ = 0;
    cross_partition_transfer_bytes_ = 0;
    exchange_mode_ = units.size() > 1 ? "GATHER" : "SERIAL";
    execution_infos_.clear();

    std::vector<std::unordered_map<std::string, ParallelGroupByState>> partial_states(
        units.size());
    ParallelGroupByContext group_context;
    group_context.table_columns = columns;
    group_context.group_column_index = group_column_index;
    group_context.target_column_index = target_column_index;
    group_context.count_star = count_star;
    group_context.agg_type = agg_type;
    group_context.partial_states = &partial_states;

    for (auto& unit : units)
    {
        unit.context = &group_context;
    }

    auto worker = [&](const WorkUnit& unit) -> WorkerResult {
        WorkerResult result;
        result.worker_id = unit.worker_id;
        result.executed_by_worker_id = unit.executed_by_worker_id;
        result.start_page = unit.start_page;
        result.end_page = unit.end_page;
        result.work_stolen = unit.work_stolen;

        auto* grouped_context = static_cast<ParallelGroupByContext*>(unit.context);
        if (grouped_context == nullptr || grouped_context->partial_states == nullptr ||
            unit.worker_id >= grouped_context->partial_states->size())
        {
            result.status = Status::INVALID_ARGUMENT;
            result.error_message = "Parallel group aggregate worker context is missing";
            return result;
        }

        ErrorContext local_ctx;
        auto scan = db_->storage_engine()->createScanRange(unit.table_id,
                                                           unit.start_page,
                                                           unit.end_page,
                                                           &local_ctx);
        if (!scan)
        {
            result.status = local_ctx.code == Status::OK ? Status::INTERNAL_ERROR
                                                         : local_ctx.code;
            result.error_message =
                local_ctx.message.empty()
                    ? "Failed to create parallel aggregate group iterator"
                    : local_ctx.message;
            return result;
        }

        auto& worker_states = (*grouped_context->partial_states)[unit.worker_id];
        Tuple tuple{};
        Status scan_status = Status::OK;
        while ((scan_status = scan->next(&tuple, &local_ctx)) == Status::OK)
        {
            TypedValue group_value;
            if (!decodeProjectedTupleValue(tuple.data,
                                           tuple.data_size,
                                           grouped_context->table_columns,
                                           grouped_context->group_column_index,
                                           &group_value,
                                           &local_ctx))
            {
                result.status =
                    local_ctx.code == Status::OK ? Status::DATA_CORRUPTED : local_ctx.code;
                result.error_message =
                    local_ctx.message.empty()
                        ? "Parallel aggregate worker failed to decode group value"
                        : local_ctx.message;
                return result;
            }

            std::string encoded_group_key;
            if (!encodeParallelGroupKey(group_value, &encoded_group_key, &local_ctx))
            {
                result.status =
                    local_ctx.code == Status::OK ? Status::INVALID_ARGUMENT : local_ctx.code;
                result.error_message =
                    local_ctx.message.empty()
                        ? "Parallel aggregate worker failed to encode group key"
                        : local_ctx.message;
                return result;
            }

            auto [state_it, inserted] = worker_states.try_emplace(encoded_group_key);
            ParallelGroupByState& grouped_state = state_it->second;
            if (inserted)
            {
                grouped_state.group_value = group_value;
                grouped_state.has_group_value = true;
            }

            if (grouped_context->count_star)
            {
                ++grouped_state.aggregate_state.count;
            }
            else
            {
                TypedValue aggregate_value;
                if (!decodeProjectedTupleValue(tuple.data,
                                               tuple.data_size,
                                               grouped_context->table_columns,
                                               grouped_context->target_column_index,
                                               &aggregate_value,
                                               &local_ctx))
                {
                    result.status = local_ctx.code == Status::OK ? Status::DATA_CORRUPTED
                                                                 : local_ctx.code;
                    result.error_message =
                        local_ctx.message.empty()
                            ? "Parallel aggregate worker failed to decode aggregate value"
                            : local_ctx.message;
                    return result;
                }

                if (!aggregate_value.isNull())
                {
                    if (grouped_context->agg_type == AggType::COUNT)
                    {
                        ++grouped_state.aggregate_state.count;
                    }
                    else
                    {
                        double numeric_value = 0.0;
                        if (!valueToParallelAggregateDouble(aggregate_value,
                                                            &numeric_value,
                                                            &local_ctx))
                        {
                            result.status =
                                local_ctx.code == Status::OK ? Status::NOT_IMPLEMENTED
                                                             : local_ctx.code;
                            result.error_message =
                                local_ctx.message.empty()
                                    ? "Parallel aggregate worker encountered unsupported grouped value type"
                                    : local_ctx.message;
                            return result;
                        }

                        switch (grouped_context->agg_type)
                        {
                            case AggType::SUM:
                            case AggType::AVG:
                            case AggType::STDDEV:
                            case AggType::VARIANCE:
                                ++grouped_state.aggregate_state.count;
                                grouped_state.aggregate_state.sum += numeric_value;
                                if (grouped_context->agg_type == AggType::STDDEV ||
                                    grouped_context->agg_type == AggType::VARIANCE)
                                {
                                    grouped_state.aggregate_state.sum_sq +=
                                        numeric_value * numeric_value;
                                }
                                break;
                            case AggType::MIN:
                                if (!grouped_state.aggregate_state.has_min ||
                                    numeric_value <
                                        grouped_state.aggregate_state.min_val)
                                {
                                    grouped_state.aggregate_state.min_val = numeric_value;
                                    grouped_state.aggregate_state.has_min = true;
                                }
                                break;
                            case AggType::MAX:
                                if (!grouped_state.aggregate_state.has_max ||
                                    numeric_value >
                                        grouped_state.aggregate_state.max_val)
                                {
                                    grouped_state.aggregate_state.max_val = numeric_value;
                                    grouped_state.aggregate_state.has_max = true;
                                }
                                break;
                            case AggType::COUNT:
                                break;
                        }
                    }
                }
            }

            ++result.rows_processed;
        }

        if (scan_status != Status::NOT_FOUND)
        {
            result.status = scan_status;
            result.error_message = local_ctx.message;
            return result;
        }

        uint64_t transfer_bytes = 0;
        for (const auto& [encoded_key, state] : worker_states)
        {
            (void)state;
            transfer_bytes += static_cast<uint64_t>(encoded_key.size());
            transfer_bytes += sizeof(ParallelGroupByState);
        }
        result.transfer_bytes = transfer_bytes;
        if (!workerReservationFits(config_, result.transfer_bytes))
        {
            result.status = Status::OOM;
            result.error_message =
                makeWorkerReservationMessage("aggregate group",
                                             result.transfer_bytes,
                                             config_.work_mem_per_worker);
            return result;
        }

        result.status = Status::OK;
        return result;
    };

    auto collectResult = [&](const WorkerResult& worker_result) -> Status {
        execution_infos_.push_back(
            ParallelWorkerExecutionInfo{worker_result.worker_id,
                                        worker_result.executed_by_worker_id,
                                        worker_result.start_page,
                                        worker_result.end_page,
                                        worker_result.rows_processed,
                                        worker_result.work_stolen});
        cross_partition_transfer_bytes_ += worker_result.transfer_bytes;
        if (worker_result.status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx,
                              worker_result.status,
                              worker_result.error_message.empty()
                                  ? "Parallel grouped aggregate worker failed"
                                  : worker_result.error_message.c_str());
            return worker_result.status;
        }
        return Status::OK;
    };

    if (units.size() <= 1)
    {
        WorkerResult result = worker(units.front());
        Status status = collectResult(result);
        if (status != Status::OK)
        {
            return status;
        }
        workers_used_ = 1;
        morsel_count_ = 1;
        locality_preferred_ = true;
    }
    else
    {
        const uint64_t steals_before = pool_->totalSteals();
        auto futures = pool_->submitBatch(units, worker);
        for (auto& future : futures)
        {
            Status status = collectResult(future.get());
            if (status != Status::OK)
            {
                return status;
            }
        }

        std::set<uint32_t> unique_workers;
        for (const auto& info : execution_infos_)
        {
            unique_workers.insert(info.executed_by_worker_id);
        }
        workers_used_ =
            static_cast<uint32_t>(std::max<size_t>(1, unique_workers.size()));
        locality_preferred_ = true;
        work_steal_count_ = pool_->totalSteals() - steals_before;
    }

    std::unordered_map<std::string, ParallelGroupByState> merged_states;
    for (const auto& worker_states : partial_states)
    {
        for (const auto& [encoded_key, state] : worker_states)
        {
            auto [merged_it, inserted] = merged_states.try_emplace(encoded_key);
            if (inserted)
            {
                merged_it->second = state;
                continue;
            }

            if (!merged_it->second.has_group_value && state.has_group_value)
            {
                merged_it->second.group_value = state.group_value;
                merged_it->second.has_group_value = true;
            }
            mergeParallelAggregateState(merged_it->second.aggregate_state,
                                        state.aggregate_state,
                                        agg_type);
        }
    }

    results_out->reserve(merged_states.size());
    for (const auto& [encoded_key, state] : merged_states)
    {
        std::vector<PartialAggregateState> partial_vector{state.aggregate_state};
        results_out->emplace_back(std::vector<uint8_t>(encoded_key.begin(),
                                                       encoded_key.end()),
                                  mergePartialResults(partial_vector, agg_type));
    }

    std::sort(results_out->begin(),
              results_out->end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.first < rhs.first;
              });
    return Status::OK;
}

WorkerResult ParallelAggregate::aggregateWorker(const WorkUnit& unit)
{
    WorkerResult result;
    result.worker_id = unit.worker_id;
    result.executed_by_worker_id = unit.executed_by_worker_id;
    result.start_page = unit.start_page;
    result.end_page = unit.end_page;
    result.work_stolen = unit.work_stolen;

    auto* aggregate_context = static_cast<ParallelAggregateContext*>(unit.context);
    if (aggregate_context == nullptr)
    {
        result.status = Status::INVALID_ARGUMENT;
        result.error_message = "Parallel aggregate worker context is missing";
        return result;
    }
    if (aggregate_context->partial_states == nullptr ||
        unit.worker_id >= aggregate_context->partial_states->size())
    {
        result.status = Status::INVALID_ARGUMENT;
        result.error_message = "Parallel aggregate worker state slots are missing";
        return result;
    }

    ErrorContext ctx;
    auto scan = db_->storage_engine()->createScanRange(unit.table_id,
                                                       unit.start_page,
                                                       unit.end_page,
                                                       &ctx);
    if (!scan)
    {
        result.status = ctx.code == Status::OK ? Status::INTERNAL_ERROR : ctx.code;
        result.error_message =
            ctx.message.empty() ? "Failed to create parallel aggregate worker iterator"
                                : ctx.message;
        return result;
    }

    PartialAggregateState partial_state;
    Tuple tuple{};
    Status scan_status = Status::OK;
    while ((scan_status = scan->next(&tuple, &ctx)) == Status::OK)
    {
        TypedValue value;
        if (!decodeProjectedTupleValue(tuple.data,
                                       tuple.data_size,
                                       aggregate_context->table_columns,
                                       aggregate_context->target_column_index,
                                       &value,
                                       &ctx))
        {
            result.status = ctx.code == Status::OK ? Status::DATA_CORRUPTED : ctx.code;
            result.error_message =
                ctx.message.empty() ? "Parallel aggregate worker failed to decode tuple"
                                    : ctx.message;
            return result;
        }

        if (!value.isNull())
        {
            if (aggregate_context->agg_type == AggType::COUNT)
            {
                ++partial_state.count;
            }
            else
            {
                double numeric_value = 0.0;
                if (!valueToParallelAggregateDouble(value, &numeric_value, &ctx))
                {
                    result.status =
                        ctx.code == Status::OK ? Status::NOT_IMPLEMENTED : ctx.code;
                    result.error_message =
                        ctx.message.empty()
                            ? "Parallel aggregate encountered unsupported value type"
                            : ctx.message;
                    return result;
                }

                switch (aggregate_context->agg_type)
                {
                    case AggType::SUM:
                    case AggType::AVG:
                    case AggType::STDDEV:
                    case AggType::VARIANCE:
                        ++partial_state.count;
                        partial_state.sum += numeric_value;
                        if (aggregate_context->agg_type == AggType::STDDEV ||
                            aggregate_context->agg_type == AggType::VARIANCE)
                        {
                            partial_state.sum_sq += numeric_value * numeric_value;
                        }
                        break;
                    case AggType::MIN:
                        if (!partial_state.has_min || numeric_value < partial_state.min_val)
                        {
                            partial_state.min_val = numeric_value;
                            partial_state.has_min = true;
                        }
                        break;
                    case AggType::MAX:
                        if (!partial_state.has_max || numeric_value > partial_state.max_val)
                        {
                            partial_state.max_val = numeric_value;
                            partial_state.has_max = true;
                        }
                        break;
                    case AggType::COUNT:
                        break;
                }
            }
        }

        ++result.rows_processed;
    }

    if (scan_status != Status::NOT_FOUND)
    {
        result.status = scan_status;
        result.error_message = ctx.message;
        return result;
    }

    (*aggregate_context->partial_states)[unit.worker_id] = partial_state;
    result.status = Status::OK;
    return result;
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
    , partitions_(NUM_PARTITIONS)
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
    if (db_ == nullptr || pool_ == nullptr)
    {
        SET_ERROR_CONTEXT(ctx,
                          Status::INVALID_ARGUMENT,
                          "Database or pool not initialized");
        return Status::INVALID_ARGUMENT;
    }

    rows_processed_ = 0;
    match_count_ = 0;
    workers_used_ = 0;
    morsel_count_ = 0;
    locality_preferred_ = false;
    work_steal_count_ = 0;
    cross_partition_transfer_bytes_ = 0;
    exchange_mode_ = "SERIAL";
    execution_infos_.clear();

    for (auto& partition : partitions_)
    {
        std::lock_guard<std::mutex> lock(partition.mutex);
        partition.entries.clear();
    }

    std::vector<CatalogManager::ColumnInfo> outer_columns;
    if (db_->catalog_manager()->getColumns(outer_table_id, outer_columns, ctx) != Status::OK)
    {
        return ctx != nullptr ? ctx->code : Status::NOT_FOUND;
    }
    std::vector<CatalogManager::ColumnInfo> inner_columns;
    if (db_->catalog_manager()->getColumns(inner_table_id, inner_columns, ctx) != Status::OK)
    {
        return ctx != nullptr ? ctx->code : Status::NOT_FOUND;
    }

    size_t outer_join_column_index = 0;
    if (!resolveParallelColumnIndex(outer_columns, outer_join_column_id, &outer_join_column_index, ctx))
    {
        return ctx != nullptr ? ctx->code : Status::NOT_FOUND;
    }
    size_t inner_join_column_index = 0;
    if (!resolveParallelColumnIndex(inner_columns, inner_join_column_id, &inner_join_column_index, ctx))
    {
        return ctx != nullptr ? ctx->code : Status::NOT_FOUND;
    }

    const uint32_t requested_workers = std::max<uint32_t>(1, pool_->numWorkers());
    auto outer_units = partitionTableWork(db_, outer_table_id, requested_workers);
    auto inner_units = partitionTableWork(db_, inner_table_id, requested_workers);
    if (outer_units.empty() || inner_units.empty())
    {
        return Status::OK;
    }

    morsel_count_ = static_cast<uint32_t>(outer_units.size() + inner_units.size());
    locality_preferred_ = true;
    exchange_mode_ = morsel_count_ > 2 ? "REPARTITION_PROBE" : "SERIAL";

    auto collectResult = [&](const WorkerResult& worker_result) -> Status
    {
        rows_processed_.fetch_add(worker_result.rows_processed, std::memory_order_relaxed);
        cross_partition_transfer_bytes_ += worker_result.transfer_bytes;
        execution_infos_.push_back(
            ParallelWorkerExecutionInfo{worker_result.worker_id,
                                        worker_result.executed_by_worker_id,
                                        worker_result.start_page,
                                        worker_result.end_page,
                                        worker_result.rows_processed,
                                        worker_result.work_stolen});
        if (worker_result.status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx,
                              worker_result.status,
                              worker_result.error_message.empty()
                                  ? "Parallel hash join worker failed"
                                  : worker_result.error_message.c_str());
            return worker_result.status;
        }
        return Status::OK;
    };

    const bool run_parallel =
        requested_workers > 1 && (outer_units.size() > 1 || inner_units.size() > 1);
    std::mutex callback_mutex;

    if (!run_parallel)
    {
        WorkerResult build_result =
            buildWorker(outer_units.front(), outer_columns, outer_join_column_index);
        Status status = collectResult(build_result);
        if (status != Status::OK)
        {
            return status;
        }

        WorkerResult probe_result = probeWorker(inner_units.front(),
                                                inner_columns,
                                                inner_join_column_index,
                                                match_callback,
                                                &callback_mutex);
        status = collectResult(probe_result);
        if (status != Status::OK)
        {
            return status;
        }
    }
    else
    {
        const uint64_t steals_before = pool_->totalSteals();
        auto build_futures = pool_->submitBatch(
            outer_units,
            [this, &outer_columns, outer_join_column_index](const WorkUnit& unit)
            {
                return buildWorker(unit, outer_columns, outer_join_column_index);
            });

        for (auto& future : build_futures)
        {
            Status status = collectResult(future.get());
            if (status != Status::OK)
            {
                return status;
            }
        }

        auto probe_futures = pool_->submitBatch(
            inner_units,
            [this,
             &inner_columns,
             inner_join_column_index,
             &match_callback,
             &callback_mutex](const WorkUnit& unit)
            {
                return probeWorker(unit,
                                   inner_columns,
                                   inner_join_column_index,
                                   match_callback,
                                   &callback_mutex);
            });

        for (auto& future : probe_futures)
        {
            Status status = collectResult(future.get());
            if (status != Status::OK)
            {
                return status;
            }
        }

        work_steal_count_ = pool_->totalSteals() - steals_before;
    }

    std::set<uint32_t> unique_workers;
    for (const auto& info : execution_infos_)
    {
        unique_workers.insert(info.executed_by_worker_id);
    }
    workers_used_ = static_cast<uint32_t>(std::max<size_t>(1, unique_workers.size()));

    return Status::OK;
}

WorkerResult ParallelHashJoin::buildWorker(const WorkUnit& unit,
                                           const std::vector<CatalogManager::ColumnInfo>& columns,
                                           size_t join_column_index)
{
    WorkerResult result;
    result.worker_id = unit.worker_id;
    result.executed_by_worker_id = unit.executed_by_worker_id;
    result.start_page = unit.start_page;
    result.end_page = unit.end_page;
    result.work_stolen = unit.work_stolen;

    ErrorContext ctx;
    auto scan = db_->storage_engine()->createScanRange(unit.table_id,
                                                       unit.start_page,
                                                       unit.end_page,
                                                       &ctx);
    if (!scan)
    {
        result.status = ctx.code == Status::OK ? Status::INTERNAL_ERROR : ctx.code;
        result.error_message =
            ctx.message.empty() ? "Failed to create parallel hash build iterator" : ctx.message;
        return result;
    }

    Tuple tuple{};
    Status scan_status = Status::OK;
    while ((scan_status = scan->next(&tuple, &ctx)) == Status::OK)
    {
        TypedValue key_value;
        if (!decodeProjectedTupleValue(tuple.data,
                                       tuple.data_size,
                                       columns,
                                       join_column_index,
                                       &key_value,
                                       &ctx))
        {
            result.status = ctx.code == Status::OK ? Status::DATA_CORRUPTED : ctx.code;
            result.error_message =
                ctx.message.empty() ? "Parallel hash build failed to decode join value"
                                    : ctx.message;
            return result;
        }

        std::vector<uint8_t> key_bytes;
        bool has_value = false;
        if (!extractProjectedTupleStorageBytes(tuple.data,
                                               tuple.data_size,
                                               columns,
                                               join_column_index,
                                               &key_bytes,
                                               &has_value,
                                               &ctx))
        {
            result.status = ctx.code == Status::OK ? Status::DATA_CORRUPTED : ctx.code;
            result.error_message =
                ctx.message.empty() ? "Parallel hash build failed to decode join key"
                                    : ctx.message;
            return result;
        }

        ++result.rows_processed;
        if (!has_value)
        {
            continue;
        }

        const uint64_t key_hash =
            hashJoinKey(key_bytes.data(), static_cast<uint32_t>(key_bytes.size()));
        HashEntry entry;
        entry.key = std::move(key_bytes);
        entry.key_value = std::move(key_value);
        entry.tuple.assign(tuple.data, tuple.data + tuple.data_size);
        result.transfer_bytes += entry.tuple.size();
        if (!workerReservationFits(config_, result.transfer_bytes))
        {
            result.status = Status::OOM;
            result.error_message =
                makeWorkerReservationMessage("hash join build",
                                             result.transfer_bytes,
                                             config_.work_mem_per_worker);
            return result;
        }

        HashPartition& partition = partitions_[key_hash % partitions_.size()];
        {
            std::lock_guard<std::mutex> lock(partition.mutex);
            partition.entries.emplace(key_hash, std::move(entry));
        }
    }

    if (scan_status != Status::NOT_FOUND)
    {
        result.status = scan_status;
        result.error_message = ctx.message;
        return result;
    }

    result.status = Status::OK;
    return result;
}

WorkerResult ParallelHashJoin::probeWorker(
    const WorkUnit& unit,
    const std::vector<CatalogManager::ColumnInfo>& columns,
    size_t join_column_index,
    const std::function<void(const uint8_t*, uint32_t, const uint8_t*, uint32_t)>& match_callback,
    std::mutex* callback_mutex)
{
    WorkerResult result;
    result.worker_id = unit.worker_id;
    result.executed_by_worker_id = unit.executed_by_worker_id;
    result.start_page = unit.start_page;
    result.end_page = unit.end_page;
    result.work_stolen = unit.work_stolen;

    ErrorContext ctx;
    auto scan = db_->storage_engine()->createScanRange(unit.table_id,
                                                       unit.start_page,
                                                       unit.end_page,
                                                       &ctx);
    if (!scan)
    {
        result.status = ctx.code == Status::OK ? Status::INTERNAL_ERROR : ctx.code;
        result.error_message =
            ctx.message.empty() ? "Failed to create parallel hash probe iterator" : ctx.message;
        return result;
    }

    Tuple tuple{};
    Status scan_status = Status::OK;
    while ((scan_status = scan->next(&tuple, &ctx)) == Status::OK)
    {
        TypedValue probe_value;
        if (!decodeProjectedTupleValue(tuple.data,
                                       tuple.data_size,
                                       columns,
                                       join_column_index,
                                       &probe_value,
                                       &ctx))
        {
            result.status = ctx.code == Status::OK ? Status::DATA_CORRUPTED : ctx.code;
            result.error_message =
                ctx.message.empty() ? "Parallel hash probe failed to decode join value"
                                    : ctx.message;
            return result;
        }

        std::vector<uint8_t> key_bytes;
        bool has_value = false;
        if (!extractProjectedTupleStorageBytes(tuple.data,
                                               tuple.data_size,
                                               columns,
                                               join_column_index,
                                               &key_bytes,
                                               &has_value,
                                               &ctx))
        {
            result.status = ctx.code == Status::OK ? Status::DATA_CORRUPTED : ctx.code;
            result.error_message =
                ctx.message.empty() ? "Parallel hash probe failed to decode join key"
                                    : ctx.message;
            return result;
        }

        ++result.rows_processed;
        if (!has_value)
        {
            continue;
        }

        const uint64_t key_hash =
            hashJoinKey(key_bytes.data(), static_cast<uint32_t>(key_bytes.size()));
        HashPartition& partition = partitions_[key_hash % partitions_.size()];

        std::vector<HashEntry> matches;
        {
            std::lock_guard<std::mutex> lock(partition.mutex);
            auto range = partition.entries.equal_range(key_hash);
            for (auto it = range.first; it != range.second; ++it)
            {
                if (it->second.key == key_bytes && it->second.key_value == probe_value)
                {
                    matches.push_back(it->second);
                }
            }
        }

        for (const auto& match : matches)
        {
            if (callback_mutex != nullptr)
            {
                std::lock_guard<std::mutex> lock(*callback_mutex);
                match_callback(match.tuple.data(),
                               static_cast<uint32_t>(match.tuple.size()),
                               tuple.data,
                               tuple.data_size);
            }
            else
            {
                match_callback(match.tuple.data(),
                               static_cast<uint32_t>(match.tuple.size()),
                               tuple.data,
                               tuple.data_size);
            }
            ++result.rows_returned;
            match_count_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    if (scan_status != Status::NOT_FOUND)
    {
        result.status = scan_status;
        result.error_message = ctx.message;
        return result;
    }

    result.status = Status::OK;
    return result;
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
    if (pool_ == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Worker pool not initialized");
        return Status::INVALID_ARGUMENT;
    }

    rows_processed_ = 0;
    workers_used_ = 0;
    morsel_count_ = 0;
    locality_preferred_ = false;
    work_steal_count_ = 0;
    cross_partition_transfer_bytes_ = 0;
    exchange_mode_ = "SERIAL";
    execution_infos_.clear();

    if (data.size() <= 1)
    {
        rows_processed_ = static_cast<uint64_t>(data.size());
        workers_used_ = data.empty() ? 0u : 1u;
        morsel_count_ = data.empty() ? 0u : 1u;
        locality_preferred_ = !data.empty();
        return Status::OK;
    }

    const size_t rows_per_worker_target =
        std::max<size_t>(1, static_cast<size_t>(config_.min_rows_per_worker));
    uint32_t num_workers =
        std::min(static_cast<uint32_t>(pool_->numWorkers()),
                 static_cast<uint32_t>((data.size() + rows_per_worker_target - 1) /
                                       rows_per_worker_target));
    if (num_workers <= 1) {
        // Sequential sort
        std::sort(data.begin(), data.end(),
                 [&comparator](const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
                     return comparator(a, b) < 0;
                 });
        rows_processed_ = static_cast<uint64_t>(data.size());
        workers_used_ = 1;
        morsel_count_ = 1;
        locality_preferred_ = true;
        return Status::OK;
    }

    const size_t total_rows = data.size();

    // Partition data
    size_t partition_size = (data.size() + num_workers - 1) / num_workers;
    std::vector<std::vector<std::vector<uint8_t>>> partitions(num_workers);
    std::vector<size_t> partition_bytes(num_workers, 0);

    for (size_t i = 0; i < data.size(); ++i) {
        uint32_t part_idx = static_cast<uint32_t>(i / partition_size);
        if (part_idx >= num_workers) part_idx = num_workers - 1;
        partition_bytes[part_idx] += data[i].size();
        partitions[part_idx].push_back(std::move(data[i]));
    }
    data.clear();

    for (uint32_t i = 0; i < num_workers; ++i)
    {
        if (!workerReservationFits(config_, partition_bytes[i]))
        {
            const std::string detail =
                makeWorkerReservationMessage("sort",
                                             partition_bytes[i],
                                             config_.work_mem_per_worker);
            SET_ERROR_CONTEXT(ctx, Status::OOM, detail.c_str());
            return Status::OOM;
        }
    }

    workers_used_ = num_workers;
    morsel_count_ = num_workers;
    locality_preferred_ = true;
    exchange_mode_ = "GATHER_MERGE";
    const uint64_t steals_before = pool_->totalSteals();

    // Sort partitions in parallel
    std::vector<std::future<WorkerResult>> futures;
    for (uint32_t i = 0; i < num_workers; ++i) {
        WorkUnit unit;
        unit.worker_id = i;
        unit.start_page = static_cast<uint32_t>(i * partition_size);
        unit.end_page = static_cast<uint32_t>(
            std::min(total_rows, (static_cast<size_t>(i) + 1) * partition_size));
        unit.context = &partitions[i];

        auto future = pool_->submit(unit,
            [this, &comparator, &partition_bytes](const WorkUnit& u) {
                auto* partition = static_cast<std::vector<std::vector<uint8_t>>*>(u.context);
                sortPartition(*partition, comparator);
                WorkerResult result;
                result.worker_id = u.worker_id;
                result.executed_by_worker_id = u.executed_by_worker_id;
                result.start_page = u.start_page;
                result.end_page = u.end_page;
                result.work_stolen = u.work_stolen;
                result.rows_processed = partition == nullptr
                    ? 0u
                    : static_cast<uint64_t>(partition->size());
                result.transfer_bytes = partition_bytes[u.worker_id];
                result.status = Status::OK;
                return result;
            });
        futures.push_back(std::move(future));
    }

    // Wait for sorts to complete
    for (auto& future : futures) {
        WorkerResult result = future.get();
        rows_processed_ += result.rows_processed;
        cross_partition_transfer_bytes_ += result.transfer_bytes;
        execution_infos_.push_back(ParallelWorkerExecutionInfo{result.worker_id,
                                                               result.executed_by_worker_id,
                                                               result.start_page,
                                                               result.end_page,
                                                               result.rows_processed,
                                                               result.work_stolen});
        if (result.status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx,
                              result.status,
                              result.error_message.empty()
                                  ? "Parallel sort worker failed"
                                  : result.error_message.c_str());
            return result.status;
        }
    }
    work_steal_count_ = pool_->totalSteals() - steals_before;

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
// ParallelWindow Implementation
// =================================================================================================

ParallelWindow::ParallelWindow(Database* db, WorkerPool* pool, const ParallelConfig& config)
    : db_(db), pool_(pool), config_(config)
{
}

ParallelWindow::~ParallelWindow() = default;

Status ParallelWindow::executeRowNumber(
    const std::vector<std::vector<uint8_t>>& data,
    const std::function<bool(const std::vector<uint8_t>&, const std::vector<uint8_t>&)>& same_partition,
    std::vector<uint64_t>* row_numbers_out,
    ErrorContext* ctx)
{
    if (pool_ == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Worker pool not initialized");
        return Status::INVALID_ARGUMENT;
    }
    if (row_numbers_out == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Row number output not initialized");
        return Status::INVALID_ARGUMENT;
    }

    rows_processed_ = 0;
    workers_used_ = 0;
    morsel_count_ = 0;
    locality_preferred_ = false;
    work_steal_count_ = 0;
    cross_partition_transfer_bytes_ = 0;
    exchange_mode_ = "SERIAL";
    execution_infos_.clear();

    row_numbers_out->assign(data.size(), 0);
    if (data.empty())
    {
        return Status::OK;
    }

    std::vector<std::pair<uint32_t, uint32_t>> partition_ranges;
    uint32_t partition_start = 0;
    for (uint32_t i = 1; i < data.size(); ++i)
    {
        if (!same_partition(data[i - 1], data[i]))
        {
            partition_ranges.emplace_back(partition_start, i);
            partition_start = i;
        }
    }
    partition_ranges.emplace_back(partition_start, static_cast<uint32_t>(data.size()));

    uint64_t max_partition_output_bytes = 0;
    for (const auto& range : partition_ranges)
    {
        const uint64_t partition_rows =
            static_cast<uint64_t>(range.second - range.first);
        max_partition_output_bytes = std::max(
            max_partition_output_bytes,
            partition_rows * static_cast<uint64_t>(sizeof(uint64_t)));
    }
    if (!workerReservationFits(config_, max_partition_output_bytes))
    {
        const std::string detail =
            makeWorkerReservationMessage("window",
                                         max_partition_output_bytes,
                                         config_.work_mem_per_worker);
        SET_ERROR_CONTEXT(ctx, Status::OOM, detail.c_str());
        return Status::OOM;
    }

    const uint32_t requested_workers = std::max<uint32_t>(1, pool_->numWorkers());
    const uint32_t num_workers = std::min<uint32_t>(
        requested_workers,
        static_cast<uint32_t>(partition_ranges.size()));
    workers_used_ = num_workers;
    morsel_count_ = static_cast<uint32_t>(partition_ranges.size());
    locality_preferred_ = true;

    const auto runSequential = [&]() -> Status
    {
        for (const auto& range : partition_ranges)
        {
            for (uint32_t row_index = range.first; row_index < range.second; ++row_index)
            {
                (*row_numbers_out)[row_index] =
                    static_cast<uint64_t>(row_index - range.first + 1);
                ++rows_processed_;
            }
            execution_infos_.push_back(ParallelWorkerExecutionInfo{0,
                                                                   0,
                                                                   range.first,
                                                                   range.second,
                                                                   range.second - range.first,
                                                                   false});
        }
        workers_used_ = 1;
        morsel_count_ = static_cast<uint32_t>(partition_ranges.size());
        locality_preferred_ = true;
        return Status::OK;
    };

    if (num_workers <= 1 || partition_ranges.size() <= 1)
    {
        return runSequential();
    }

    struct WindowRowNumberContext {
        const std::vector<std::vector<uint8_t>>* rows = nullptr;
        std::vector<uint64_t>* row_numbers = nullptr;
    };

    WindowRowNumberContext window_context{&data, row_numbers_out};
    std::vector<WorkUnit> units;
    units.reserve(partition_ranges.size());
    for (size_t partition_index = 0; partition_index < partition_ranges.size(); ++partition_index)
    {
        WorkUnit unit;
        unit.worker_id = static_cast<uint32_t>(partition_index % num_workers);
        unit.start_page = partition_ranges[partition_index].first;
        unit.end_page = partition_ranges[partition_index].second;
        unit.context = &window_context;
        units.push_back(unit);
    }

    exchange_mode_ = "GATHER";
    const uint64_t steals_before = pool_->totalSteals();
    auto futures = pool_->submitBatch(
        units,
        [](const WorkUnit& unit) -> WorkerResult
        {
            WorkerResult result;
            result.worker_id = unit.worker_id;
            result.executed_by_worker_id = unit.executed_by_worker_id;
            result.start_page = unit.start_page;
            result.end_page = unit.end_page;
            result.work_stolen = unit.work_stolen;

            auto* window_context =
                static_cast<WindowRowNumberContext*>(unit.context);
            if (window_context == nullptr || window_context->row_numbers == nullptr)
            {
                result.status = Status::INVALID_ARGUMENT;
                result.error_message = "Parallel window worker missing output buffer";
                return result;
            }

            for (uint32_t row_index = unit.start_page; row_index < unit.end_page; ++row_index)
            {
                (*window_context->row_numbers)[row_index] =
                    static_cast<uint64_t>(row_index - unit.start_page + 1);
                ++result.rows_processed;
            }

            result.status = Status::OK;
            return result;
        });

    for (auto& future : futures)
    {
        WorkerResult result = future.get();
        rows_processed_ += result.rows_processed;
        execution_infos_.push_back(ParallelWorkerExecutionInfo{result.worker_id,
                                                               result.executed_by_worker_id,
                                                               result.start_page,
                                                               result.end_page,
                                                               result.rows_processed,
                                                               result.work_stolen});
        if (result.status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx,
                              result.status,
                              result.error_message.empty()
                                  ? "Parallel window worker failed"
                                  : result.error_message.c_str());
            return result.status;
        }
    }
    work_steal_count_ = pool_->totalSteals() - steals_before;

    return Status::OK;
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
