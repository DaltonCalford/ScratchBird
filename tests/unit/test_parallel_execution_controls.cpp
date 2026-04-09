#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/type_serialization.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/executor/parallel_executor.h"
#include "test_helpers.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <thread>
#include <utility>
#include <vector>

using scratchbird::core::CatalogManager;
using scratchbird::core::ConnectionContext;
using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::ID;
using scratchbird::core::Status;
using scratchbird::core::StorageEngine;
using scratchbird::core::TupleHeader;
using scratchbird::executor::ParallelAggregate;
using scratchbird::executor::ParallelConfig;
using scratchbird::executor::ParallelExecutionManager;
using scratchbird::executor::ParallelHashJoin;
using scratchbird::executor::ParallelScan;
using scratchbird::executor::ParallelSort;
using scratchbird::executor::ParallelWindow;
using scratchbird::executor::ParallelPlanDecision;
using scratchbird::executor::ParallelStageKind;
using scratchbird::executor::WorkUnit;
using scratchbird::executor::WorkerResult;
using scratchbird::executor::WorkerPool;

namespace
{

auto makeTestId(uint8_t marker) -> ID
{
    ID id{};
    id.bytes[15] = marker;
    return id;
}

auto encodeParallelSortInt(int32_t value) -> std::vector<uint8_t>
{
    std::vector<uint8_t> row(sizeof(value));
    std::memcpy(row.data(), &value, sizeof(value));
    return row;
}

auto decodeParallelSortInt(const std::vector<uint8_t>& row) -> int32_t
{
    int32_t value = 0;
    if (row.size() >= sizeof(value))
    {
        std::memcpy(&value, row.data(), sizeof(value));
    }
    return value;
}

auto encodeParallelWindowRow(int32_t partition_key, int32_t order_key)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> row(sizeof(partition_key) + sizeof(order_key));
    std::memcpy(row.data(), &partition_key, sizeof(partition_key));
    std::memcpy(row.data() + sizeof(partition_key), &order_key, sizeof(order_key));
    return row;
}

auto decodeParallelWindowPartitionKey(const std::vector<uint8_t>& row) -> int32_t
{
    int32_t value = 0;
    if (row.size() >= sizeof(value))
    {
        std::memcpy(&value, row.data(), sizeof(value));
    }
    return value;
}

auto decodeParallelGroupKeyInt32(const std::vector<uint8_t>& bytes,
                                 bool* is_null_out = nullptr) -> int32_t
{
    if (is_null_out != nullptr)
    {
        *is_null_out = bytes.empty() || bytes.front() == '\x00';
    }
    if (bytes.size() <= 1 || bytes.front() == '\x00')
    {
        return 0;
    }

    ErrorContext ctx;
    auto decoded = scratchbird::core::TypeSerializer::deserialize(
        scratchbird::core::DataType::INT32,
        bytes.data() + 1,
        bytes.size() - 1,
        &ctx);
    if (!decoded.has_value())
    {
        return 0;
    }
    return decoded->toInt32();
}

} // namespace

class ParallelScanExecutionTest : public ::testing::Test
{
protected:
    static constexpr uint32_t kPageSize = 8192;
    void SetUp() override
    {
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_parallel_scan", ".db");
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, kPageSize, &ctx), Status::OK)
            << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK) << ctx.message;

        Status status = db_->initializeProcArray(16, &ctx);
        if (status != Status::OK && status != Status::INVALID_ARGUMENT)
        {
            ASSERT_EQ(status, Status::OK) << ctx.message;
        }

        ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_ctx_.get());
        ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK) << ctx.message;

        const ID system_user = db_->catalog_manager()->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user, true);

        schema_id_ = resolveDefaultSchema(&ctx);
        ASSERT_NE(schema_id_, ID{});

        engine_ = db_->storage_engine();
        ASSERT_NE(engine_, nullptr);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        db_.reset();
        std::filesystem::remove(test_db_path_);
    }

    auto resolveDefaultSchema(ErrorContext* ctx) -> ID
    {
        CatalogManager::SchemaInfo schema{};
        if (db_->catalog_manager()->getSchema("main", schema, ctx) == Status::OK)
        {
            return schema.schema_id;
        }
        if (db_->catalog_manager()->getSchema("users.public", schema, ctx) == Status::OK)
        {
            return schema.schema_id;
        }
        if (db_->catalog_manager()->getSchema("public", schema, ctx) == Status::OK)
        {
            return schema.schema_id;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        if (db_->catalog_manager()->listSchemas(schemas, ctx) == Status::OK && !schemas.empty())
        {
            return schemas.front().schema_id;
        }

        ID schema_id{};
        if (db_->catalog_manager()->createSchema("main", "SYSTEM", schema_id, ctx) == Status::OK)
        {
            return schema_id;
        }
        return ID{};
    }

    auto createSingleIntTable(const std::string& name) -> ID
    {
        ErrorContext ctx;
        std::vector<CatalogManager::ColumnInfo> columns;

        CatalogManager::ColumnInfo id_col;
        id_col.column_name = "metric";
        id_col.ordinal = 1;
        id_col.data_type = static_cast<uint16_t>(scratchbird::core::DataType::INT32);
        id_col.type_precision = 4;
        id_col.nullable = false;
        columns.push_back(id_col);

        ID table_id{};
        EXPECT_EQ(db_->catalog_manager()->createTable(schema_id_,
                                                      name,
                                                      columns,
                                                      table_id,
                                                      0,
                                                      &ctx),
                  Status::OK)
            << ctx.message;
        return table_id;
    }

    auto resolveOnlyColumnId(const ID& table_id) -> ID
    {
        ErrorContext ctx;
        std::vector<CatalogManager::ColumnInfo> columns;
        EXPECT_EQ(db_->catalog_manager()->getColumns(table_id, columns, &ctx), Status::OK)
            << ctx.message;
        EXPECT_EQ(columns.size(), 1u);
        if (columns.size() != 1u)
        {
            return ID{};
        }
        return columns.front().column_id;
    }

    auto buildWideIntTuple(int32_t value, size_t payload_size = 1024) -> std::vector<uint8_t>
    {
        TupleHeader header{};
        header.xmin = conn_ctx_ ? conn_ctx_->getCurrentXid() : 0;
        header.xmax = 0;
        header.back_version_gpid = scratchbird::core::INVALID_GPID;
        header.back_version_slot = 0;
        header.ctid_gpid = scratchbird::core::INVALID_GPID;
        header.ctid_slot = 0;
        header.infomask = 0;
        header.null_bitmap_offset = 0;
        header.padding = 0;
        header.session_id = ID{};

        std::vector<uint8_t> tuple(sizeof(TupleHeader) + sizeof(value) + payload_size, 0);
        std::memcpy(tuple.data(), &header, sizeof(TupleHeader));
        std::memcpy(tuple.data() + sizeof(TupleHeader), &value, sizeof(value));
        std::fill(tuple.begin() + static_cast<std::ptrdiff_t>(sizeof(TupleHeader) + sizeof(value)),
                  tuple.end(),
                  static_cast<uint8_t>('x'));
        return tuple;
    }

    auto buildIntTuple(int32_t value) -> std::vector<uint8_t>
    {
        return buildWideIntTuple(value, 0);
    }

    void commitCurrentTransaction()
    {
        ErrorContext ctx;
        ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;
    }

    std::string test_db_path_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<ConnectionContext> conn_ctx_;
    StorageEngine* engine_ = nullptr;
    ID schema_id_{};
};

TEST(ParallelExecutionControlsTest, ReinitializesWorkerPoolWhenWorkerCountChanges)
{
    auto& manager = ParallelExecutionManager::getInstance();
    manager.shutdown();

    ParallelConfig config;
    config.max_workers = 2;
    config.enable_parallel_scan = true;
    manager.initialize(config);
    ASSERT_NE(manager.getPool(), nullptr);
    EXPECT_EQ(manager.getPool()->numWorkers(), 2u);

    config.max_workers = 5;
    manager.initialize(config);
    ASSERT_NE(manager.getPool(), nullptr);
    EXPECT_EQ(manager.getPool()->numWorkers(), 5u);

    manager.shutdown();
}

TEST(ParallelExecutionControlsTest, OptimalWorkerCountHonorsConfiguredThresholds)
{
    auto& manager = ParallelExecutionManager::getInstance();
    manager.shutdown();

    ParallelConfig config;
    config.max_workers = 4;
    config.enable_parallel_scan = true;
    config.min_rows_per_worker = 10;
    config.min_pages_per_worker = 2;
    manager.initialize(config);

    EXPECT_EQ(manager.optimalWorkerCount(5, 5), 1u);
    EXPECT_EQ(manager.optimalWorkerCount(100, 8), 4u);

    manager.shutdown();
}

TEST(ParallelExecutionControlsTest, ZeroWorkerBudgetDisablesParallelization)
{
    auto& manager = ParallelExecutionManager::getInstance();
    manager.shutdown();

    ParallelConfig config;
    config.max_workers = 0;
    config.enable_parallel_scan = true;
    config.min_rows_per_worker = 1;
    config.min_pages_per_worker = 1;
    manager.initialize(config);

    EXPECT_FALSE(manager.shouldParallelize(100, 100));
    EXPECT_EQ(manager.optimalWorkerCount(100, 100), 1u);

    manager.shutdown();
}

TEST(ParallelExecutionControlsTest, EvaluateParallelPlanHonorsStageFlagsAndThresholds)
{
    ParallelConfig config;
    config.enable_parallel = true;
    config.enable_parallel_scan = true;
    config.max_workers = 4;
    config.max_workers_per_gather = 4;
    config.min_rows_per_worker = 100;
    config.min_pages_per_worker = 2;
    config.min_parallel_table_scan_size = 1;

    const ParallelPlanDecision chosen =
        scratchbird::executor::evaluateParallelPlan(config,
                                                    ParallelStageKind::SCAN,
                                                    1200,
                                                    32,
                                                    false,
                                                    false,
                                                    true);
    EXPECT_TRUE(chosen.eligible);
    EXPECT_GE(chosen.workers_planned, 2u);

    config.enable_parallel_scan = false;
    const ParallelPlanDecision rejected =
        scratchbird::executor::evaluateParallelPlan(config,
                                                    ParallelStageKind::SCAN,
                                                    1200,
                                                    32,
                                                    false,
                                                    false,
                                                    true);
    EXPECT_FALSE(rejected.eligible);
    EXPECT_EQ(rejected.rejection_reason, "parallel_scan_disabled");
}

TEST(ParallelExecutionControlsTest, EvaluateParallelPlanRejectsSpilledHashJoinAndUsesGatherMerge)
{
    ParallelConfig config;
    config.enable_parallel = true;
    config.enable_parallel_scan = true;
    config.enable_parallel_hash = true;
    config.enable_parallel_join = true;
    config.max_workers = 4;
    config.max_workers_per_gather = 4;
    config.min_rows_per_worker = 100;
    config.min_pages_per_worker = 2;
    config.min_parallel_table_scan_size = 1;

    const ParallelPlanDecision ordered =
        scratchbird::executor::evaluateParallelPlan(config,
                                                    ParallelStageKind::GATHER_MERGE,
                                                    1200,
                                                    32,
                                                    true,
                                                    false,
                                                    true);
    EXPECT_TRUE(ordered.eligible);
    EXPECT_TRUE(ordered.use_gather_merge);

    const ParallelPlanDecision spilled =
        scratchbird::executor::evaluateParallelPlan(config,
                                                    ParallelStageKind::HASH_JOIN,
                                                    1200,
                                                    32,
                                                    false,
                                                    true,
                                                    true);
    EXPECT_FALSE(spilled.eligible);
    EXPECT_EQ(spilled.rejection_reason, "spill_risk_blocks_parallel_stage");
}

TEST(ParallelExecutionControlsTest, ParallelAggregateRejectsUninitializedInputs)
{
    ParallelAggregate aggregate(nullptr, nullptr, ParallelConfig{});
    double result = 42.0;
    ErrorContext ctx;

    EXPECT_EQ(aggregate.execute(makeTestId(1),
                                makeTestId(2),
                                ParallelAggregate::AggType::COUNT,
                                &result,
                                &ctx),
              Status::INVALID_ARGUMENT);
    EXPECT_EQ(result, 0.0);
    EXPECT_EQ(ctx.message, "Database or pool not initialized");
}

TEST(ParallelExecutionControlsTest, WorkerPoolPrefersLocalQueueAndStealsWhenImbalanced)
{
    WorkerPool pool(2);
    std::vector<WorkUnit> units;
    units.reserve(16);
    for (uint32_t i = 0; i < 16; ++i)
    {
        WorkUnit unit;
        unit.worker_id = 0;
        unit.start_page = i;
        unit.end_page = i + 1;
        units.push_back(unit);
    }

    auto futures = pool.submitBatch(
        units,
        [](const WorkUnit& unit) -> WorkerResult
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            WorkerResult result;
            result.worker_id = unit.worker_id;
            result.executed_by_worker_id = unit.executed_by_worker_id;
            result.start_page = unit.start_page;
            result.end_page = unit.end_page;
            result.work_stolen = unit.work_stolen;
            result.rows_processed = 1;
            result.status = Status::OK;
            return result;
        });

    size_t local_count = 0;
    size_t stolen_count = 0;
    for (auto& future : futures)
    {
        const WorkerResult result = future.get();
        ASSERT_EQ(result.status, Status::OK);
        EXPECT_EQ(result.worker_id, 0u);
        if (result.work_stolen)
        {
            ++stolen_count;
            EXPECT_NE(result.executed_by_worker_id, result.worker_id);
        }
        else
        {
            ++local_count;
            EXPECT_EQ(result.executed_by_worker_id, result.worker_id);
        }
    }

    EXPECT_GT(local_count, 0u);
    EXPECT_GT(stolen_count, 0u);
    EXPECT_EQ(pool.totalSteals(), stolen_count);
}

TEST(ParallelExecutionControlsTest, ParallelAggregateGroupByRejectsUninitializedInputs)
{
    ParallelAggregate aggregate(nullptr, nullptr, ParallelConfig{});
    std::vector<std::pair<std::vector<uint8_t>, double>> results;
    ErrorContext ctx;

    EXPECT_EQ(aggregate.executeGroupBy(makeTestId(1),
                                       makeTestId(2),
                                       makeTestId(3),
                                       ParallelAggregate::AggType::SUM,
                                       &results,
                                       &ctx),
              Status::INVALID_ARGUMENT);
    EXPECT_TRUE(results.empty());
    EXPECT_EQ(ctx.message, "Database or pool not initialized");
}

TEST(ParallelExecutionControlsTest, ParallelHashJoinRejectsUninitializedInputs)
{
    ParallelHashJoin join(nullptr, nullptr, ParallelConfig{});
    ErrorContext ctx;

    EXPECT_EQ(join.execute(makeTestId(1),
                           makeTestId(2),
                           makeTestId(3),
                           makeTestId(4),
                           [](const uint8_t*, uint32_t, const uint8_t*, uint32_t) {},
                           &ctx),
              Status::INVALID_ARGUMENT);
    EXPECT_EQ(ctx.message, "Database or pool not initialized");
}

TEST(ParallelExecutionControlsTest, ParallelSortPublishesWorkerLocalMergeEvidence)
{
    ParallelConfig config;
    config.enable_parallel = true;
    config.max_workers = 4;
    WorkerPool pool(4);
    ParallelSort sort(nullptr, &pool, config);

    constexpr int32_t kRowCount = 40001;
    std::vector<std::vector<uint8_t>> rows;
    rows.reserve(kRowCount);
    for (int32_t value = kRowCount - 1; value >= 0; --value)
    {
        rows.push_back(encodeParallelSortInt(value));
    }

    ErrorContext ctx;
    ASSERT_EQ(sort.execute(rows,
                           [](const std::vector<uint8_t>& left,
                              const std::vector<uint8_t>& right) -> int
                           {
                               const int32_t left_value = decodeParallelSortInt(left);
                               const int32_t right_value = decodeParallelSortInt(right);
                               if (left_value < right_value)
                               {
                                   return -1;
                               }
                               if (left_value > right_value)
                               {
                                   return 1;
                               }
                               return 0;
                           },
                           &ctx),
              Status::OK)
        << ctx.message;

    ASSERT_EQ(rows.size(), static_cast<size_t>(kRowCount));
    for (int32_t i = 0; i < kRowCount; ++i)
    {
        EXPECT_EQ(decodeParallelSortInt(rows[static_cast<size_t>(i)]), i);
    }

    EXPECT_EQ(sort.rowsProcessed(), static_cast<uint64_t>(kRowCount));
    EXPECT_EQ(sort.workersUsed(), 4u);
    EXPECT_EQ(sort.morselCount(), 4u);
    EXPECT_TRUE(sort.localityPreferred());
    EXPECT_EQ(sort.workStealCount(), 0u);
    EXPECT_EQ(sort.exchangeMode(), "GATHER_MERGE");
    EXPECT_EQ(sort.crossPartitionTransferBytes(),
              static_cast<uint64_t>(kRowCount) * sizeof(int32_t));
    ASSERT_EQ(sort.executionInfos().size(), 4u);

    uint64_t rows_processed = 0;
    for (const auto& info : sort.executionInfos())
    {
        EXPECT_LT(info.start_page, info.end_page);
        EXPECT_EQ(info.preferred_worker_id, info.executed_by_worker_id);
        EXPECT_FALSE(info.work_stolen);
        rows_processed += info.rows_processed;
    }
    EXPECT_EQ(rows_processed, static_cast<uint64_t>(kRowCount));
}

TEST(ParallelExecutionControlsTest, ParallelWindowPublishesWorkerLocalPartitionEvidence)
{
    ParallelConfig config;
    config.enable_parallel = true;
    config.max_workers = 4;
    WorkerPool pool(4);
    ParallelWindow window(nullptr, &pool, config);

    constexpr int32_t kPartitions = 4;
    constexpr int32_t kRowsPerPartition = 10000;
    std::vector<std::vector<uint8_t>> rows;
    rows.reserve(static_cast<size_t>(kPartitions) * static_cast<size_t>(kRowsPerPartition));
    for (int32_t partition = 0; partition < kPartitions; ++partition)
    {
        for (int32_t order_key = 0; order_key < kRowsPerPartition; ++order_key)
        {
            rows.push_back(encodeParallelWindowRow(partition, order_key));
        }
    }

    std::vector<uint64_t> row_numbers;
    ErrorContext ctx;
    ASSERT_EQ(window.executeRowNumber(
                  rows,
                  [](const std::vector<uint8_t>& left,
                     const std::vector<uint8_t>& right)
                  {
                      return decodeParallelWindowPartitionKey(left) ==
                             decodeParallelWindowPartitionKey(right);
                  },
                  &row_numbers,
                  &ctx),
              Status::OK)
        << ctx.message;

    ASSERT_EQ(row_numbers.size(), rows.size());
    for (int32_t partition = 0; partition < kPartitions; ++partition)
    {
        const size_t base_index =
            static_cast<size_t>(partition) * static_cast<size_t>(kRowsPerPartition);
        for (int32_t order_key = 0; order_key < kRowsPerPartition; ++order_key)
        {
            EXPECT_EQ(row_numbers[base_index + static_cast<size_t>(order_key)],
                      static_cast<uint64_t>(order_key + 1));
        }
    }

    EXPECT_EQ(window.rowsProcessed(),
              static_cast<uint64_t>(kPartitions) *
                  static_cast<uint64_t>(kRowsPerPartition));
    EXPECT_EQ(window.workersUsed(), 4u);
    EXPECT_EQ(window.morselCount(), 4u);
    EXPECT_TRUE(window.localityPreferred());
    EXPECT_LE(window.workStealCount(), window.morselCount());
    EXPECT_EQ(window.crossPartitionTransferBytes(), 0u);
    EXPECT_EQ(window.exchangeMode(), "GATHER");
    ASSERT_EQ(window.executionInfos().size(), 4u);

    uint64_t rows_processed = 0;
    size_t local_partitions = 0;
    size_t stolen_partitions = 0;
    for (const auto& info : window.executionInfos())
    {
        EXPECT_LT(info.start_page, info.end_page);
        if (info.work_stolen)
        {
            ++stolen_partitions;
            EXPECT_NE(info.preferred_worker_id, info.executed_by_worker_id);
        }
        else
        {
            ++local_partitions;
            EXPECT_EQ(info.preferred_worker_id, info.executed_by_worker_id);
        }
        rows_processed += info.rows_processed;
    }
    EXPECT_GT(local_partitions, 0u);
    EXPECT_EQ(window.workStealCount(), stolen_partitions);
    EXPECT_EQ(rows_processed,
              static_cast<uint64_t>(kPartitions) *
                  static_cast<uint64_t>(kRowsPerPartition));
}

TEST(ParallelExecutionControlsTest, ParallelSortRejectsWorkerReservationOverflow)
{
    ParallelConfig config;
    config.enable_parallel = true;
    config.max_workers = 4;
    config.work_mem_per_worker = 1024;
    WorkerPool pool(4);
    ParallelSort sort(nullptr, &pool, config);

    std::vector<std::vector<uint8_t>> rows(12000, std::vector<uint8_t>(256, 0x2A));
    ErrorContext ctx;
    EXPECT_EQ(sort.execute(rows,
                           [](const std::vector<uint8_t>& left,
                              const std::vector<uint8_t>& right) -> int
                           {
                               return left < right ? -1 : (left > right ? 1 : 0);
                           },
                           &ctx),
              Status::OOM);
    EXPECT_NE(ctx.message.find("memory reservation"), std::string::npos);
    EXPECT_NE(ctx.message.find("work_mem_per_worker"), std::string::npos);
}

TEST(ParallelExecutionControlsTest, ParallelWindowRejectsWorkerReservationOverflow)
{
    ParallelConfig config;
    config.enable_parallel = true;
    config.max_workers = 4;
    config.work_mem_per_worker = 1024;
    WorkerPool pool(4);
    ParallelWindow window(nullptr, &pool, config);

    std::vector<std::vector<uint8_t>> rows;
    rows.reserve(4096);
    for (int32_t order_key = 0; order_key < 4096; ++order_key)
    {
        rows.push_back(encodeParallelWindowRow(1, order_key));
    }

    std::vector<uint64_t> row_numbers;
    ErrorContext ctx;
    EXPECT_EQ(window.executeRowNumber(
                  rows,
                  [](const std::vector<uint8_t>& left,
                     const std::vector<uint8_t>& right)
                  {
                      return decodeParallelWindowPartitionKey(left) ==
                             decodeParallelWindowPartitionKey(right);
                  },
                  &row_numbers,
                  &ctx),
              Status::OOM);
    EXPECT_NE(ctx.message.find("memory reservation"), std::string::npos);
    EXPECT_NE(ctx.message.find("work_mem_per_worker"), std::string::npos);
}

TEST_F(ParallelScanExecutionTest, ParallelScanExecutesRealWorkerRangesAndReturnsAllRows)
{
    ErrorContext ctx;
    const ID table_id = createSingleIntTable("parallel_scan_rows");

    constexpr int32_t kRowCount = 128;
    std::set<uint32_t> page_ids;
    for (int32_t value = 0; value < kRowCount; ++value)
    {
        const auto tuple = buildWideIntTuple(value);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTuple(table_id,
                                       tuple.data(),
                                       static_cast<uint32_t>(tuple.size()),
                                       &page_id,
                                       &item_id,
                                       &ctx),
                  Status::OK)
            << ctx.message;
        page_ids.insert(page_id);
    }
    ASSERT_GE(page_ids.size(), 2u);

    commitCurrentTransaction();

    ParallelConfig config;
    config.enable_parallel = true;
    config.enable_parallel_scan = true;
    config.max_workers = 4;
    config.max_workers_per_gather = 4;
    config.min_rows_per_worker = 1;
    config.min_pages_per_worker = 1;

    WorkerPool pool(4);
    ParallelScan scan(db_.get(), &pool, config);

    std::vector<uint32_t> seen_sizes;
    ASSERT_EQ(scan.execute(
                  table_id,
                  [&](const uint8_t* tuple_data, uint32_t tuple_size)
                  {
                      (void)tuple_data;
                      seen_sizes.push_back(tuple_size);
                  },
                  &ctx),
              Status::OK)
        << ctx.message;

    EXPECT_GE(scan.workersUsed(), 2u);
    EXPECT_EQ(scan.morselCount(), scan.workersUsed());
    EXPECT_TRUE(scan.localityPreferred());
    EXPECT_EQ(scan.workStealCount(), 0u);
    EXPECT_EQ(scan.crossPartitionTransferBytes(), 0u);
    EXPECT_EQ(scan.exchangeMode(), "GATHER");
    ASSERT_EQ(scan.executionInfos().size(), static_cast<size_t>(scan.workersUsed()));
    uint64_t worker_rows_processed = 0;
    size_t non_empty_worker_ranges = 0;
    for (const auto& info : scan.executionInfos())
    {
        EXPECT_LT(info.start_page, info.end_page);
        worker_rows_processed += info.rows_processed;
        if (info.rows_processed > 0)
        {
            ++non_empty_worker_ranges;
        }
        EXPECT_EQ(info.preferred_worker_id, info.executed_by_worker_id);
        EXPECT_FALSE(info.work_stolen);
    }
    EXPECT_EQ(worker_rows_processed, static_cast<uint64_t>(kRowCount));
    EXPECT_GE(non_empty_worker_ranges, 1u);
    EXPECT_EQ(scan.rowsProcessed(), static_cast<uint64_t>(kRowCount));
    ASSERT_EQ(seen_sizes.size(), static_cast<size_t>(kRowCount));
    for (uint32_t tuple_size : seen_sizes)
    {
        EXPECT_GT(tuple_size, sizeof(TupleHeader));
    }
}

TEST_F(ParallelScanExecutionTest,
       ParallelAggregateExecutesRealWorkerPartitionsForNumericColumn)
{
    ErrorContext ctx;
    const ID table_id = createSingleIntTable("parallel_aggregate_rows");
    const ID column_id = resolveOnlyColumnId(table_id);
    ASSERT_NE(column_id, ID{});

    constexpr int32_t kRowCount = 1024;
    for (int32_t value = 0; value < kRowCount; ++value)
    {
        const auto tuple = buildIntTuple(value);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTuple(table_id,
                                       tuple.data(),
                                       static_cast<uint32_t>(tuple.size()),
                                       &page_id,
                                       &item_id,
                                       &ctx),
                  Status::OK)
            << ctx.message;
    }

    commitCurrentTransaction();

    std::vector<CatalogManager::ColumnInfo> columns;
    ASSERT_EQ(db_->catalog_manager()->getColumns(table_id, columns, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(columns.size(), 1u);
    EXPECT_EQ(columns.front().column_name, "metric");
    EXPECT_EQ(columns.front().data_type,
              static_cast<uint16_t>(scratchbird::core::DataType::INT32));

    auto tuple_scan = engine_->createScan(table_id, &ctx);
    ASSERT_NE(tuple_scan, nullptr);
    scratchbird::core::Tuple first_tuple{};
    ASSERT_EQ(tuple_scan->next(&first_tuple, &ctx), Status::OK) << ctx.message;
    ASSERT_GE(first_tuple.data_size, sizeof(TupleHeader) + sizeof(int32_t));

    int32_t raw_metric = 0;
    std::memcpy(&raw_metric,
                first_tuple.data + sizeof(TupleHeader),
                sizeof(raw_metric));
    EXPECT_EQ(raw_metric, 0);

    const auto decoded_metric = scratchbird::core::TypeSerializer::deserialize(
        scratchbird::core::DataType::INT32,
        first_tuple.data + sizeof(TupleHeader),
        sizeof(raw_metric),
        &ctx);
    ASSERT_TRUE(decoded_metric.has_value()) << ctx.message;
    EXPECT_EQ(decoded_metric->toInt32(), 0);

    ParallelConfig config;
    config.enable_parallel = true;
    config.enable_parallel_aggregate = true;
    config.max_workers = 4;
    config.max_workers_per_gather = 4;
    config.min_rows_per_worker = 1;
    config.min_pages_per_worker = 1;

    WorkerPool pool(4);
    ParallelAggregate aggregate(db_.get(), &pool, config);
    double result = 0.0;

    ASSERT_EQ(aggregate.execute(table_id,
                                column_id,
                                ParallelAggregate::AggType::COUNT,
                                &result,
                                &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_GE(aggregate.workersUsed(), 2u);
    EXPECT_EQ(aggregate.morselCount(), aggregate.workersUsed());
    EXPECT_TRUE(aggregate.localityPreferred());
    EXPECT_EQ(aggregate.workStealCount(), 0u);
    EXPECT_EQ(aggregate.exchangeMode(), "GATHER");
    EXPECT_EQ(aggregate.crossPartitionTransferBytes(),
              static_cast<uint64_t>(aggregate.workersUsed()) *
                  sizeof(scratchbird::executor::PartialAggregateState));
    ASSERT_EQ(aggregate.executionInfos().size(),
              static_cast<size_t>(aggregate.workersUsed()));
    uint64_t aggregate_rows_processed = 0;
    size_t aggregate_non_empty_workers = 0;
    for (const auto& info : aggregate.executionInfos())
    {
        EXPECT_LT(info.start_page, info.end_page);
        aggregate_rows_processed += info.rows_processed;
        if (info.rows_processed > 0)
        {
            ++aggregate_non_empty_workers;
        }
        EXPECT_EQ(info.preferred_worker_id, info.executed_by_worker_id);
        EXPECT_FALSE(info.work_stolen);
    }
    EXPECT_EQ(aggregate_rows_processed, static_cast<uint64_t>(kRowCount));
    EXPECT_GE(aggregate_non_empty_workers, 1u);
    EXPECT_DOUBLE_EQ(result, static_cast<double>(kRowCount));

    ASSERT_EQ(aggregate.execute(table_id,
                                column_id,
                                ParallelAggregate::AggType::SUM,
                                &result,
                                &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_DOUBLE_EQ(result, 523776.0);

    ASSERT_EQ(aggregate.execute(table_id,
                                column_id,
                                ParallelAggregate::AggType::AVG,
                                &result,
                                &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_DOUBLE_EQ(result, 511.5);

    ASSERT_EQ(aggregate.execute(table_id,
                                column_id,
                                ParallelAggregate::AggType::MIN,
                                &result,
                                &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_DOUBLE_EQ(result, 0.0);

    ASSERT_EQ(aggregate.execute(table_id,
                                column_id,
                                ParallelAggregate::AggType::MAX,
                                &result,
                                &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_DOUBLE_EQ(result, 1023.0);
}

TEST_F(ParallelScanExecutionTest,
       ParallelAggregateExecutesRealWorkerPartitionsForSimpleGroupByCount)
{
    ErrorContext ctx;
    const ID table_id = createSingleIntTable("parallel_group_count_rows");
    const ID column_id = resolveOnlyColumnId(table_id);
    ASSERT_NE(column_id, ID{});

    constexpr int32_t kRowCount = 1024;
    constexpr int32_t kGroupCount = 8;
    for (int32_t value = 0; value < kRowCount; ++value)
    {
        const auto tuple = buildIntTuple(value % kGroupCount);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTuple(table_id,
                                       tuple.data(),
                                       static_cast<uint32_t>(tuple.size()),
                                       &page_id,
                                       &item_id,
                                       &ctx),
                  Status::OK)
            << ctx.message;
    }

    commitCurrentTransaction();

    ParallelConfig config;
    config.enable_parallel = true;
    config.enable_parallel_aggregate = true;
    config.max_workers = 4;
    config.max_workers_per_gather = 4;
    config.min_rows_per_worker = 1;
    config.min_pages_per_worker = 1;

    WorkerPool pool(4);
    ParallelAggregate aggregate(db_.get(), &pool, config);
    std::vector<std::pair<std::vector<uint8_t>, double>> results;

    ASSERT_EQ(aggregate.executeGroupBy(table_id,
                                       column_id,
                                       ID{},
                                       ParallelAggregate::AggType::COUNT,
                                       &results,
                                       &ctx),
              Status::OK)
        << ctx.message;

    EXPECT_GE(aggregate.workersUsed(), 2u);
    EXPECT_EQ(aggregate.morselCount(), 4u);
    EXPECT_TRUE(aggregate.localityPreferred());
    EXPECT_EQ(aggregate.exchangeMode(), "GATHER");
    EXPECT_FALSE(results.empty());
    ASSERT_EQ(results.size(), static_cast<size_t>(kGroupCount));

    std::map<int32_t, int64_t> counts_by_group;
    for (const auto& [encoded_key, count_value] : results)
    {
        bool is_null = false;
        const int32_t group_key =
            decodeParallelGroupKeyInt32(encoded_key, &is_null);
        EXPECT_FALSE(is_null);
        counts_by_group[group_key] = static_cast<int64_t>(count_value);
    }

    ASSERT_EQ(counts_by_group.size(), static_cast<size_t>(kGroupCount));
    for (int32_t group = 0; group < kGroupCount; ++group)
    {
        auto it = counts_by_group.find(group);
        ASSERT_NE(it, counts_by_group.end());
        EXPECT_EQ(it->second, kRowCount / kGroupCount);
    }

    uint64_t aggregate_rows_processed = 0;
    for (const auto& info : aggregate.executionInfos())
    {
        EXPECT_LT(info.start_page, info.end_page);
        aggregate_rows_processed += info.rows_processed;
    }
    EXPECT_EQ(aggregate_rows_processed, static_cast<uint64_t>(kRowCount));
    EXPECT_GT(aggregate.crossPartitionTransferBytes(), 0u);
}

TEST_F(ParallelScanExecutionTest, ParallelHashJoinExecutesRealWorkerBuildAndProbeSlices)
{
    ErrorContext ctx;
    const ID outer_table_id = createSingleIntTable("parallel_hash_outer");
    const ID inner_table_id = createSingleIntTable("parallel_hash_inner");
    const ID outer_column_id = resolveOnlyColumnId(outer_table_id);
    const ID inner_column_id = resolveOnlyColumnId(inner_table_id);
    ASSERT_NE(outer_column_id, ID{});
    ASSERT_NE(inner_column_id, ID{});

    constexpr int32_t kOuterRowCount = 2048;
    constexpr int32_t kInnerRowCount = 2048;
    for (int32_t value = 0; value < kOuterRowCount; ++value)
    {
        const auto tuple = buildIntTuple(value);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTuple(outer_table_id,
                                       tuple.data(),
                                       static_cast<uint32_t>(tuple.size()),
                                       &page_id,
                                       &item_id,
                                       &ctx),
                  Status::OK)
            << ctx.message;
    }

    for (int32_t value = 0; value < kInnerRowCount; ++value)
    {
        const auto tuple = buildIntTuple(value * 2);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTuple(inner_table_id,
                                       tuple.data(),
                                       static_cast<uint32_t>(tuple.size()),
                                       &page_id,
                                       &item_id,
                                       &ctx),
                  Status::OK)
            << ctx.message;
    }

    commitCurrentTransaction();

    ParallelConfig config;
    config.enable_parallel = true;
    config.enable_parallel_hash = true;
    config.enable_parallel_join = true;
    config.max_workers = 4;
    config.max_workers_per_gather = 4;
    config.min_rows_per_worker = 1;
    config.min_pages_per_worker = 1;

    WorkerPool pool(4);
    ParallelHashJoin join(db_.get(), &pool, config);
    uint64_t match_count = 0;

    ASSERT_EQ(join.execute(outer_table_id,
                           outer_column_id,
                           inner_table_id,
                           inner_column_id,
                           [&](const uint8_t*, uint32_t, const uint8_t*, uint32_t)
                           {
                               ++match_count;
                           },
                           &ctx),
              Status::OK)
        << ctx.message;

    EXPECT_EQ(match_count, 1024u);
    EXPECT_EQ(join.matchCount(), 1024u);
    EXPECT_EQ(join.rowsProcessed(),
              static_cast<uint64_t>(kOuterRowCount + kInnerRowCount));
    EXPECT_GE(join.workersUsed(), 2u);
    EXPECT_EQ(join.morselCount(), join.executionInfos().size());
    EXPECT_TRUE(join.localityPreferred());
    EXPECT_EQ(join.workStealCount(), 0u);
    EXPECT_EQ(join.exchangeMode(), "REPARTITION_PROBE");
    EXPECT_GT(join.crossPartitionTransferBytes(), 0u);
    ASSERT_EQ(join.executionInfos().size(), static_cast<size_t>(join.morselCount()));

    uint64_t worker_rows_processed = 0;
    size_t non_empty_worker_ranges = 0;
    for (const auto& info : join.executionInfos())
    {
        EXPECT_LT(info.start_page, info.end_page);
        worker_rows_processed += info.rows_processed;
        if (info.rows_processed > 0)
        {
            ++non_empty_worker_ranges;
        }
        EXPECT_EQ(info.preferred_worker_id, info.executed_by_worker_id);
        EXPECT_FALSE(info.work_stolen);
    }
    EXPECT_EQ(worker_rows_processed,
              static_cast<uint64_t>(kOuterRowCount + kInnerRowCount));
    EXPECT_GE(non_empty_worker_ranges, 1u);
}

TEST_F(ParallelScanExecutionTest, ParallelHashJoinRejectsBuildReservationOverflow)
{
    ErrorContext ctx;
    const ID outer_table_id = createSingleIntTable("parallel_hash_outer_budget");
    const ID inner_table_id = createSingleIntTable("parallel_hash_inner_budget");
    const ID outer_column_id = resolveOnlyColumnId(outer_table_id);
    const ID inner_column_id = resolveOnlyColumnId(inner_table_id);
    ASSERT_NE(outer_column_id, ID{});
    ASSERT_NE(inner_column_id, ID{});

    constexpr int32_t kOuterRowCount = 32;
    constexpr int32_t kInnerRowCount = 8;
    for (int32_t value = 0; value < kOuterRowCount; ++value)
    {
        const auto tuple = buildWideIntTuple(value, 4096);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTuple(outer_table_id,
                                       tuple.data(),
                                       static_cast<uint32_t>(tuple.size()),
                                       &page_id,
                                       &item_id,
                                       &ctx),
                  Status::OK)
            << ctx.message;
    }

    for (int32_t value = 0; value < kInnerRowCount; ++value)
    {
        const auto tuple = buildIntTuple(value);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTuple(inner_table_id,
                                       tuple.data(),
                                       static_cast<uint32_t>(tuple.size()),
                                       &page_id,
                                       &item_id,
                                       &ctx),
                  Status::OK)
            << ctx.message;
    }

    commitCurrentTransaction();

    ParallelConfig config;
    config.enable_parallel = true;
    config.enable_parallel_hash = true;
    config.enable_parallel_join = true;
    config.max_workers = 4;
    config.max_workers_per_gather = 4;
    config.min_rows_per_worker = 1;
    config.min_pages_per_worker = 1;
    config.work_mem_per_worker = 16;

    WorkerPool pool(4);
    ParallelHashJoin join(db_.get(), &pool, config);
    EXPECT_EQ(join.execute(outer_table_id,
                           outer_column_id,
                           inner_table_id,
                           inner_column_id,
                           [](const uint8_t*, uint32_t, const uint8_t*, uint32_t) {},
                           &ctx),
              Status::OOM);
    EXPECT_NE(ctx.message.find("memory reservation"), std::string::npos);
    EXPECT_NE(ctx.message.find("hash join build"), std::string::npos);
}
