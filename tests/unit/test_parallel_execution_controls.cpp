#include <gtest/gtest.h>

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/executor/parallel_executor.h"

#include <utility>
#include <vector>

using scratchbird::core::ErrorContext;
using scratchbird::core::ID;
using scratchbird::core::Status;
using scratchbird::executor::ParallelAggregate;
using scratchbird::executor::ParallelConfig;
using scratchbird::executor::ParallelExecutionManager;
using scratchbird::executor::ParallelHashJoin;

namespace
{

auto makeTestId(uint8_t marker) -> ID
{
    ID id{};
    id.bytes[15] = marker;
    return id;
}

} // namespace

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

TEST(ParallelExecutionControlsTest, ParallelAggregateFailsClosed)
{
    ParallelAggregate aggregate(nullptr, nullptr, ParallelConfig{});
    double result = 42.0;
    ErrorContext ctx;

    EXPECT_EQ(aggregate.execute(makeTestId(1),
                                makeTestId(2),
                                ParallelAggregate::AggType::COUNT,
                                &result,
                                &ctx),
              Status::NOT_IMPLEMENTED);
    EXPECT_EQ(result, 0.0);
    EXPECT_EQ(ctx.message, "Parallel aggregate execution is not implemented");
}

TEST(ParallelExecutionControlsTest, ParallelAggregateGroupByFailsClosed)
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
              Status::NOT_IMPLEMENTED);
    EXPECT_TRUE(results.empty());
    EXPECT_EQ(ctx.message, "Parallel GROUP BY execution is not implemented");
}

TEST(ParallelExecutionControlsTest, ParallelHashJoinFailsClosed)
{
    ParallelHashJoin join(nullptr, nullptr, ParallelConfig{});
    ErrorContext ctx;

    EXPECT_EQ(join.execute(makeTestId(1),
                           makeTestId(2),
                           makeTestId(3),
                           makeTestId(4),
                           [](const uint8_t*, uint32_t, const uint8_t*, uint32_t) {},
                           &ctx),
              Status::NOT_IMPLEMENTED);
    EXPECT_EQ(ctx.message, "Parallel hash join execution is not implemented");
}
