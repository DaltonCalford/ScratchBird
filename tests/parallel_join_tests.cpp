// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/parallel_executor.h"

#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

using namespace scratchbird::engine;

class ParallelJoinTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Initialize parallel executor with test configuration
        ParallelConfig config;
        config.max_worker_threads = 4;
        config.min_table_size_kb = 100;
        config.cpu_utilization_threshold = 0.5;

        executor_ = std::make_unique<ParallelQueryExecutor>(config);
        ASSERT_TRUE(executor_->initialize());
    }

    void TearDown() override
    {
        if (executor_) {
            executor_->shutdown();
        }
    }

    std::unique_ptr<ParallelQueryExecutor> executor_;
};

/// Test parallel hash join basic functionality
TEST_F(ParallelJoinTest, BasicParallelHashJoin)
{
    // Create a mock HashJoin node (nullptr is acceptable for simulation)
    std::shared_ptr<HashJoin> hash_join_node = nullptr;

    auto result = executor_->execute_parallel_hash_join(hash_join_node, 4);

    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->success); // Should fail with null node
    EXPECT_FALSE(result->error_message.empty());
    EXPECT_EQ(result->error_message, "Invalid HashJoin node provided");
}

/// Test parallel nested loop join basic functionality
TEST_F(ParallelJoinTest, BasicParallelNestedLoopJoin)
{
    // Create a mock NestedLoopJoin node (nullptr is acceptable for simulation)
    std::shared_ptr<NestedLoopJoin> nested_loop_join_node = nullptr;

    auto result = executor_->execute_parallel_nested_loop_join(nested_loop_join_node, 4);

    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->success); // Should fail with null node
    EXPECT_FALSE(result->error_message.empty());
    EXPECT_EQ(result->error_message, "Invalid NestedLoopJoin node provided");
}

/// Test ParallelHashJoin partition planning
TEST_F(ParallelJoinTest, HashJoinPartitionPlanning)
{
    ParallelHashJoin hash_join("test_hash_join");

    std::uint64_t build_table_rows = 10000;
    std::uint64_t probe_table_rows = 50000;
    std::uint32_t target_workers = 4;

    auto partitions =
        hash_join.plan_join_partitions(build_table_rows, probe_table_rows, target_workers);

    EXPECT_GE(partitions.size(), 1);
    EXPECT_LE(partitions.size(), target_workers);

    // Check partition properties
    std::uint64_t total_estimated_rows = 0;
    for (const auto& partition : partitions) {
        EXPECT_LT(partition.partition_id, target_workers);
        EXPECT_GE(partition.estimated_rows, 1);
        EXPECT_GT(partition.estimated_build_size, 0);
        EXPECT_FALSE(partition.partition_key_range.empty());
        total_estimated_rows += partition.estimated_rows;
    }

    EXPECT_EQ(total_estimated_rows, build_table_rows);
}

/// Test ParallelHashJoin work unit creation
TEST_F(ParallelJoinTest, HashJoinWorkUnitCreation)
{
    ParallelHashJoin hash_join("test_hash_join");

    // Plan partitions
    auto partitions = hash_join.plan_join_partitions(8000, 20000, 4);
    ASSERT_FALSE(partitions.empty());

    // Create build work units
    std::vector<std::string> build_keys = {"id", "customer_id"};
    std::string build_predicate = "status = 'active'";

    auto build_work_units =
        hash_join.create_build_work_units(partitions, build_keys, build_predicate);

    EXPECT_EQ(build_work_units.size(), partitions.size());

    for (size_t i = 0; i < build_work_units.size(); ++i) {
        const auto& work_unit = build_work_units[i];
        const auto& partition = partitions[i];

        EXPECT_EQ(work_unit->partition_id, partition.partition_id);
        EXPECT_EQ(work_unit->start_offset, partition.start_row);
        EXPECT_EQ(work_unit->end_offset, partition.end_row);
        EXPECT_EQ(work_unit->estimated_rows, partition.estimated_rows);
        EXPECT_GE(work_unit->estimated_memory_mb, 0); // Can be 0 for small partitions

        // Check context data
        EXPECT_EQ(work_unit->context["operation"], "hash_join_build");
        EXPECT_EQ(work_unit->context["predicate"], build_predicate);
        EXPECT_EQ(work_unit->context["build_keys"], "id,customer_id");
    }

    // Create probe work units
    std::vector<std::string> probe_keys = {"id", "customer_id"};
    std::string probe_predicate = "amount > 100";

    auto probe_work_units =
        hash_join.create_probe_work_units(partitions, probe_keys, probe_predicate);

    EXPECT_EQ(probe_work_units.size(), partitions.size());

    for (const auto& work_unit : probe_work_units) {
        EXPECT_EQ(work_unit->context["operation"], "hash_join_probe");
        EXPECT_EQ(work_unit->context["predicate"], probe_predicate);
        EXPECT_EQ(work_unit->context["probe_keys"], "id,customer_id");
    }
}

/// Test ParallelHashJoin execution phases
TEST_F(ParallelJoinTest, HashJoinExecutionPhases)
{
    ParallelHashJoin hash_join("test_hash_join");

    // Create test partition
    ParallelHashJoin::JoinPartition partition;
    partition.partition_id = 0;
    partition.start_row = 0;
    partition.end_row = 1000;
    partition.estimated_rows = 1000;
    partition.estimated_build_size = 64000;

    std::vector<std::string> build_keys = {"id"};
    std::string build_predicate = "";

    // Test build phase execution
    auto build_result = hash_join.execute_build_phase(partition, build_keys, build_predicate);

    ASSERT_NE(build_result, nullptr);
    EXPECT_TRUE(build_result->success);
    EXPECT_EQ(build_result->rows_processed, partition.estimated_rows);
    EXPECT_GT(build_result->rows_returned, 0);
    EXPECT_GT(build_result->execution_time.count(), 0);
    EXPECT_FALSE(build_result->custom_data["hash_table_entries"].empty());
    EXPECT_FALSE(build_result->custom_data["hash_collisions"].empty());

    // Test probe phase execution
    std::vector<std::string> probe_keys = {"id"};
    std::string probe_predicate = "";

    auto probe_result = hash_join.execute_probe_phase(partition, probe_keys, probe_predicate);

    ASSERT_NE(probe_result, nullptr);
    EXPECT_TRUE(probe_result->success);
    EXPECT_GT(probe_result->rows_processed, 0);
    EXPECT_GT(probe_result->execution_time.count(), 0);
    EXPECT_FALSE(probe_result->custom_data["output_rows"].empty());
    EXPECT_FALSE(probe_result->custom_data["probe_efficiency"].empty());
}

/// Test ParallelHashJoin result merging
TEST_F(ParallelJoinTest, HashJoinResultMerging)
{
    ParallelHashJoin hash_join("test_hash_join");

    // Create mock build results
    std::vector<std::shared_ptr<WorkResult>> build_results;
    for (int i = 0; i < 3; ++i) {
        auto result = std::make_shared<WorkResult>();
        result->work_id = i;
        result->success = true;
        result->rows_processed = 1000;
        result->rows_returned = 800;
        result->execution_time = std::chrono::microseconds(1000 * (i + 1));
        result->custom_data["hash_collisions"] = std::to_string(i * 10);
        build_results.push_back(result);
    }

    // Create mock probe results
    std::vector<std::shared_ptr<WorkResult>> probe_results;
    for (int i = 0; i < 3; ++i) {
        auto result = std::make_shared<WorkResult>();
        result->work_id = i + 1000;
        result->success = true;
        result->rows_processed = 5000;
        result->rows_returned = 2000;
        result->execution_time = std::chrono::microseconds(2000 * (i + 1));
        probe_results.push_back(result);
    }

    auto merged_result = hash_join.merge_join_results(build_results, probe_results);

    ASSERT_NE(merged_result, nullptr);
    EXPECT_TRUE(merged_result->success);
    EXPECT_EQ(merged_result->rows_processed, 18000); // 3000 build + 15000 probe
    EXPECT_EQ(merged_result->rows_returned, 6000);   // Sum of probe results

    // Check merged statistics
    EXPECT_EQ(merged_result->custom_data["total_build_rows"], "3000");
    EXPECT_EQ(merged_result->custom_data["total_probe_rows"], "15000");
    EXPECT_EQ(merged_result->custom_data["total_output_rows"], "6000");
    EXPECT_EQ(merged_result->custom_data["total_hash_collisions"], "30"); // 0+10+20

    // Check selectivity calculation
    double selectivity = std::stod(merged_result->custom_data["join_selectivity"]);
    EXPECT_DOUBLE_EQ(selectivity, 2.0); // 6000 / 3000
}

/// Test ParallelNestedLoopJoin partition planning
TEST_F(ParallelJoinTest, NestedLoopJoinPartitionPlanning)
{
    ParallelNestedLoopJoin nested_join("test_nested_join");

    std::uint64_t outer_table_rows = 5000;
    std::uint64_t inner_table_rows = 20000;
    std::uint32_t target_workers = 4;

    auto partitions =
        nested_join.plan_nested_loop_partitions(outer_table_rows, inner_table_rows, target_workers);

    EXPECT_GE(partitions.size(), 1);
    EXPECT_LE(partitions.size(), target_workers);

    std::uint64_t total_outer_rows = 0;
    for (const auto& partition : partitions) {
        EXPECT_LT(partition.partition_id, target_workers);
        EXPECT_GE(partition.outer_rows, 1);
        EXPECT_EQ(partition.inner_table_size, inner_table_rows);
        EXPECT_FALSE(partition.partition_predicate.empty());
        total_outer_rows += partition.outer_rows;
    }

    EXPECT_EQ(total_outer_rows, outer_table_rows);
}

/// Test ParallelNestedLoopJoin execution
TEST_F(ParallelJoinTest, NestedLoopJoinExecution)
{
    ParallelNestedLoopJoin nested_join("test_nested_join");

    // Create test partition
    ParallelNestedLoopJoin::NestedLoopPartition partition;
    partition.partition_id = 0;
    partition.outer_start_row = 0;
    partition.outer_end_row = 1000;
    partition.outer_rows = 1000;
    partition.inner_table_size = 5000;

    std::string join_predicate = "outer.customer_id = inner.id";

    auto result = nested_join.execute_nested_loop_partition(partition, join_predicate);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->success);
    EXPECT_EQ(result->rows_processed, partition.outer_rows);
    EXPECT_GE(result->rows_returned, 0);
    EXPECT_GT(result->execution_time.count(), 0);

    // Check statistics
    EXPECT_FALSE(result->custom_data["inner_scans_performed"].empty());
    EXPECT_FALSE(result->custom_data["cache_hit_ratio"].empty());
    EXPECT_FALSE(result->custom_data["join_selectivity"].empty());

    // Verify inner scans count
    std::uint64_t inner_scans = std::stoull(result->custom_data["inner_scans_performed"]);
    EXPECT_GT(inner_scans, 0);
}

/// Test ParallelNestedLoopJoin with index optimization
TEST_F(ParallelJoinTest, NestedLoopJoinWithIndexOptimization)
{
    ParallelNestedLoopJoin::NestedLoopConfig config;
    config.enable_index_lookup = true;
    config.enable_caching = true;

    ParallelNestedLoopJoin nested_join("test_indexed_nested_join", config);

    ParallelNestedLoopJoin::NestedLoopPartition partition;
    partition.partition_id = 0;
    partition.outer_start_row = 0;
    partition.outer_end_row = 500;
    partition.outer_rows = 500;
    partition.inner_table_size = 10000;

    // Use an indexable predicate
    std::string join_predicate = "outer.id = inner.customer_id";

    auto result = nested_join.execute_nested_loop_partition(partition, join_predicate);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->success);

    // With index optimization, should have fewer inner scans
    std::uint64_t inner_scans = std::stoull(result->custom_data["inner_scans_performed"]);
    EXPECT_LE(inner_scans, partition.outer_rows); // Should be close to outer_rows for index lookups
}

/// Test parallel join statistics tracking
TEST_F(ParallelJoinTest, JoinStatisticsTracking)
{
    // Test hash join statistics
    {
        ParallelHashJoin hash_join("test_hash_join");

        auto initial_stats = hash_join.get_statistics();
        EXPECT_EQ(initial_stats.total_partitions, 0);
        EXPECT_EQ(initial_stats.build_rows_processed, 0);

        // Execute some operations to update statistics
        auto partitions = hash_join.plan_join_partitions(1000, 5000, 2);
        std::vector<std::string> keys = {"id"};

        for (const auto& partition : partitions) {
            hash_join.execute_build_phase(partition, keys, "");
            hash_join.execute_probe_phase(partition, keys, "");
        }

        auto updated_stats = hash_join.get_statistics();
        EXPECT_EQ(updated_stats.total_partitions, partitions.size());
        EXPECT_GT(updated_stats.build_rows_processed, 0);
        EXPECT_GT(updated_stats.probe_rows_processed, 0);

        // Test statistics reset
        hash_join.reset_statistics();
        auto reset_stats = hash_join.get_statistics();
        EXPECT_EQ(reset_stats.build_rows_processed, 0);
        EXPECT_EQ(reset_stats.probe_rows_processed, 0);
    }

    // Test nested loop join statistics
    {
        ParallelNestedLoopJoin nested_join("test_nested_join");

        auto initial_stats = nested_join.get_statistics();
        EXPECT_EQ(initial_stats.total_partitions, 0);
        EXPECT_EQ(initial_stats.outer_rows_processed, 0);

        // Execute some operations
        auto partitions = nested_join.plan_nested_loop_partitions(800, 4000, 2);

        for (const auto& partition : partitions) {
            nested_join.execute_nested_loop_partition(partition, "outer.id = inner.id");
        }

        auto updated_stats = nested_join.get_statistics();
        EXPECT_EQ(updated_stats.total_partitions, partitions.size());
        EXPECT_GT(updated_stats.outer_rows_processed, 0);
        EXPECT_GT(updated_stats.inner_scans_performed, 0);
    }
}

/// Test error handling in parallel joins
TEST_F(ParallelJoinTest, JoinErrorHandling)
{
    // Test invalid configuration for hash join
    ParallelHashJoin::HashJoinConfig invalid_config;
    invalid_config.max_partitions = 0; // Invalid

    EXPECT_THROW(ParallelHashJoin("test", invalid_config), std::invalid_argument);

    // Test invalid configuration for nested loop join
    ParallelNestedLoopJoin::NestedLoopConfig invalid_nl_config;
    invalid_nl_config.min_outer_partition = 0; // Invalid

    EXPECT_THROW(ParallelNestedLoopJoin("test", invalid_nl_config), std::invalid_argument);

    // Test empty result merging
    ParallelHashJoin hash_join("test_hash_join");
    std::vector<std::shared_ptr<WorkResult>> empty_build;
    std::vector<std::shared_ptr<WorkResult>> empty_probe;

    auto merged_result = hash_join.merge_join_results(empty_build, empty_probe);
    EXPECT_NE(merged_result, nullptr);
    EXPECT_TRUE(merged_result->success);
    EXPECT_EQ(merged_result->rows_processed, 0);
}

/// Integration test combining parallel hash join functionality
TEST_F(ParallelJoinTest, HashJoinIntegrationTest)
{
    ParallelHashJoin hash_join("integration_test_hash_join");

    std::uint64_t build_table_rows = 2000;
    std::uint64_t probe_table_rows = 8000;
    std::uint32_t worker_count = 3;

    // Plan partitions
    auto partitions =
        hash_join.plan_join_partitions(build_table_rows, probe_table_rows, worker_count);
    ASSERT_FALSE(partitions.empty());

    // Create work units
    std::vector<std::string> keys = {"customer_id", "order_id"};
    auto build_work_units = hash_join.create_build_work_units(partitions, keys, "");
    auto probe_work_units = hash_join.create_probe_work_units(partitions, keys, "");

    EXPECT_EQ(build_work_units.size(), partitions.size());
    EXPECT_EQ(probe_work_units.size(), partitions.size());

    // Execute all phases
    std::vector<std::shared_ptr<WorkResult>> build_results;
    std::vector<std::shared_ptr<WorkResult>> probe_results;

    for (const auto& partition : partitions) {
        auto build_result = hash_join.execute_build_phase(partition, keys, "");
        auto probe_result = hash_join.execute_probe_phase(partition, keys, "");

        EXPECT_TRUE(build_result->success);
        EXPECT_TRUE(probe_result->success);

        build_results.push_back(build_result);
        probe_results.push_back(probe_result);
    }

    // Merge results
    auto final_result = hash_join.merge_join_results(build_results, probe_results);

    ASSERT_NE(final_result, nullptr);
    EXPECT_TRUE(final_result->success);
    EXPECT_GT(final_result->rows_processed, 0);
    EXPECT_GT(final_result->rows_returned, 0);

    // Check final statistics
    auto final_stats = hash_join.get_statistics();
    EXPECT_EQ(final_stats.total_partitions, partitions.size());
    EXPECT_GT(final_stats.build_rows_processed, 0);
    EXPECT_GT(final_stats.probe_rows_processed, 0);
}

/// Run all tests
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
