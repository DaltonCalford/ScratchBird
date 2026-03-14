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
using scratchbird::executor::ParallelPlanDecision;
using scratchbird::executor::ParallelStageKind;

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
