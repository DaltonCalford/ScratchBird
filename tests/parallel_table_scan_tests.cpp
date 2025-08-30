// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/parallel_executor.h"

#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace scratchbird::engine;

class ParallelTableScanTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Default scan configuration for testing
        ParallelTableScan::ScanConfig config;
        config.max_partitions = 4;
        config.min_partition_size = 100;
        config.enable_predicate_pushdown = true;
        config.enable_projection_pushdown = true;

        scan_ = std::make_unique<ParallelTableScan>("test_schema", "test_table", config);
    }

    void TearDown() override
    {
        scan_.reset();
    }

    std::unique_ptr<ParallelTableScan> scan_;
};

/// Test basic partition planning
TEST_F(ParallelTableScanTest, BasicPartitionPlanning)
{
    const std::uint64_t total_rows = 10000;
    const std::uint32_t target_workers = 4;

    auto partitions = scan_->plan_partitions(total_rows, target_workers);

    EXPECT_EQ(partitions.size(), 4);

    std::uint64_t total_estimated_rows = 0;
    std::uint64_t last_end_row = 0;

    for (const auto& partition : partitions) {
        // Check partition IDs are sequential
        EXPECT_LT(partition.partition_id, target_workers);

        // Check row ranges don't overlap and are contiguous
        EXPECT_EQ(partition.start_row, last_end_row);
        EXPECT_GT(partition.end_row, partition.start_row);

        // Check estimated rows match the range
        EXPECT_EQ(partition.estimated_rows, partition.end_row - partition.start_row);

        total_estimated_rows += partition.estimated_rows;
        last_end_row = partition.end_row;

        // Check partition key range is set
        EXPECT_FALSE(partition.partition_key_range.empty());
    }

    // Total estimated rows should equal input
    EXPECT_EQ(total_estimated_rows, total_rows);
    EXPECT_EQ(last_end_row, total_rows);
}

/// Test partition planning with uneven distribution
TEST_F(ParallelTableScanTest, UnevenPartitionPlanning)
{
    const std::uint64_t total_rows = 10003; // Not evenly divisible
    const std::uint32_t target_workers = 4;

    auto partitions = scan_->plan_partitions(total_rows, target_workers);

    EXPECT_EQ(partitions.size(), 4);

    std::uint64_t total_estimated_rows = 0;
    for (const auto& partition : partitions) {
        total_estimated_rows += partition.estimated_rows;
    }

    EXPECT_EQ(total_estimated_rows, total_rows);

    // First partitions should get the remainder
    EXPECT_GE(partitions[0].estimated_rows, partitions[3].estimated_rows);
}

/// Test partition planning with minimum size constraint
TEST_F(ParallelTableScanTest, MinimumPartitionSizeConstraint)
{
    const std::uint64_t total_rows = 300; // Small table
    const std::uint32_t target_workers = 8;

    auto partitions = scan_->plan_partitions(total_rows, target_workers);

    // Should create fewer partitions due to minimum size constraint
    EXPECT_LE(partitions.size(), 3); // 300 / 100 = 3 max partitions
    EXPECT_GT(partitions.size(), 0);

    for (const auto& partition : partitions) {
        EXPECT_GE(partition.estimated_rows, 100); // Minimum partition size
    }
}

/// Test work unit creation
TEST_F(ParallelTableScanTest, WorkUnitCreation)
{
    const std::uint64_t total_rows = 1000;
    const std::uint32_t target_workers = 2;

    auto partitions = scan_->plan_partitions(total_rows, target_workers);

    std::vector<std::string> projections = {"id", "name", "value"};
    std::string predicate = "id > 500";

    auto work_units = scan_->create_scan_work_units(partitions, projections, predicate);

    EXPECT_EQ(work_units.size(), partitions.size());

    for (size_t i = 0; i < work_units.size(); ++i) {
        const auto& work_unit = work_units[i];
        const auto& partition = partitions[i];

        // Check work unit properties
        EXPECT_EQ(work_unit->partition_id, partition.partition_id);
        EXPECT_EQ(work_unit->start_offset, partition.start_row);
        EXPECT_EQ(work_unit->end_offset, partition.end_row);
        EXPECT_EQ(work_unit->estimated_rows, partition.estimated_rows);

        // Check custom data is populated
        EXPECT_EQ(work_unit->custom_data["schema"], "test_schema");
        EXPECT_EQ(work_unit->custom_data["table"], "test_table");
        EXPECT_EQ(work_unit->custom_data["predicate"], predicate);
        EXPECT_EQ(work_unit->custom_data["projections"], "id,name,value");

        // Check resource estimates are reasonable
        EXPECT_GT(work_unit->estimated_memory_mb, 0);
        EXPECT_GT(work_unit->estimated_time.count(), 0);
    }
}

/// Test partition scan execution
TEST_F(ParallelTableScanTest, PartitionScanExecution)
{
    // Create a test partition
    ParallelTableScan::TablePartition partition;
    partition.partition_id = 0;
    partition.start_row = 0;
    partition.end_row = 100;
    partition.estimated_rows = 100;

    std::vector<std::string> projections = {"id", "name"};
    std::string predicate = ""; // No predicate for this test

    auto result = scan_->execute_partition_scan(partition, projections, predicate);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->success);
    EXPECT_EQ(result->work_id, partition.partition_id);
    EXPECT_EQ(result->rows_processed, 100);
    EXPECT_EQ(result->rows_returned, 100); // No filtering
    EXPECT_GT(result->execution_time.count(), 0);

    // Check custom data
    EXPECT_EQ(result->custom_data["result_count"], "100");
    EXPECT_EQ(result->custom_data["rows_filtered"], "0");
}

/// Test partition scan with predicate filtering
TEST_F(ParallelTableScanTest, PartitionScanWithPredicate)
{
    ParallelTableScan::TablePartition partition;
    partition.partition_id = 1;
    partition.start_row = 0;
    partition.end_row = 200;
    partition.estimated_rows = 200;

    std::vector<std::string> projections = {"id", "value"};
    std::string predicate = "id > 100"; // Should filter out half the rows

    auto result = scan_->execute_partition_scan(partition, projections, predicate);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->success);
    EXPECT_EQ(result->rows_processed, 200);
    EXPECT_LT(result->rows_returned, result->rows_processed); // Some filtering occurred

    // Check that filtering statistics are recorded
    std::uint64_t filtered_count = std::stoull(result->custom_data["rows_filtered"]);
    EXPECT_GT(filtered_count, 0);
    EXPECT_EQ(result->rows_returned + filtered_count, result->rows_processed);
}

/// Test result merging
TEST_F(ParallelTableScanTest, ResultMerging)
{
    // Create multiple partition results
    std::vector<std::shared_ptr<WorkResult>> partition_results;

    for (int i = 0; i < 3; ++i) {
        auto result = std::make_shared<WorkResult>();
        result->work_id = i;
        result->success = true;
        result->rows_processed = 100;
        result->rows_returned = 90;                                         // 10% filtered
        result->execution_time = std::chrono::microseconds(1000 * (i + 1)); // Varying times
        result->start_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(100);
        result->end_time = std::chrono::steady_clock::now();

        partition_results.push_back(result);
    }

    auto merged_result = scan_->merge_partition_results(partition_results);

    ASSERT_NE(merged_result, nullptr);
    EXPECT_TRUE(merged_result->success);
    EXPECT_EQ(merged_result->rows_processed, 300); // Sum of all partitions
    EXPECT_EQ(merged_result->rows_returned, 270);  // Sum of all returned rows

    // Check statistics are updated
    auto stats = scan_->get_statistics();
    EXPECT_EQ(stats.total_partitions, 3);
    EXPECT_EQ(stats.total_rows_scanned, 300);
    EXPECT_EQ(stats.total_rows_filtered, 30);
    EXPECT_GT(stats.load_balance_factor, 0.0);
    EXPECT_LE(stats.load_balance_factor, 1.0);
}

/// Test empty result handling
TEST_F(ParallelTableScanTest, EmptyResultMerging)
{
    std::vector<std::shared_ptr<WorkResult>> empty_results;

    auto merged_result = scan_->merge_partition_results(empty_results);

    EXPECT_EQ(merged_result, nullptr);
}

/// Test failed partition handling
TEST_F(ParallelTableScanTest, FailedPartitionHandling)
{
    std::vector<std::shared_ptr<WorkResult>> mixed_results;

    // Add successful result
    auto success_result = std::make_shared<WorkResult>();
    success_result->work_id = 0;
    success_result->success = true;
    success_result->rows_processed = 100;
    success_result->rows_returned = 90;
    success_result->start_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(50);
    success_result->end_time = std::chrono::steady_clock::now();
    mixed_results.push_back(success_result);

    // Add failed result
    auto failed_result = std::make_shared<WorkResult>();
    failed_result->work_id = 1;
    failed_result->success = false;
    failed_result->error_message = "Partition scan failed";
    failed_result->start_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(50);
    failed_result->end_time = std::chrono::steady_clock::now();
    mixed_results.push_back(failed_result);

    auto merged_result = scan_->merge_partition_results(mixed_results);

    ASSERT_NE(merged_result, nullptr);
    EXPECT_FALSE(merged_result->success);
    EXPECT_FALSE(merged_result->error_message.empty());
    EXPECT_EQ(merged_result->rows_processed, 100); // Only successful partition counted
}

/// Test statistics tracking
TEST_F(ParallelTableScanTest, StatisticsTracking)
{
    // Initially, statistics should be zero
    auto initial_stats = scan_->get_statistics();
    EXPECT_EQ(initial_stats.total_partitions, 0);
    EXPECT_EQ(initial_stats.total_rows_scanned, 0);

    // Execute a scan and merge results to update statistics
    std::vector<std::shared_ptr<WorkResult>> results;

    auto result = std::make_shared<WorkResult>();
    result->work_id = 0;
    result->success = true;
    result->rows_processed = 1000;
    result->rows_returned = 800;
    result->execution_time = std::chrono::microseconds(5000);
    result->start_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(5);
    result->end_time = std::chrono::steady_clock::now();
    results.push_back(result);

    scan_->merge_partition_results(results);

    auto updated_stats = scan_->get_statistics();
    EXPECT_EQ(updated_stats.total_partitions, 1);
    EXPECT_EQ(updated_stats.total_rows_scanned, 1000);
    EXPECT_EQ(updated_stats.total_rows_filtered, 200);
    EXPECT_GT(updated_stats.total_execution_time_ms, 0);

    // Test statistics reset
    scan_->reset_statistics();
    auto reset_stats = scan_->get_statistics();
    EXPECT_EQ(reset_stats.total_partitions, 0);
    EXPECT_EQ(reset_stats.total_rows_scanned, 0);
}

/// Test configuration validation
TEST_F(ParallelTableScanTest, ConfigurationValidation)
{
    ParallelTableScan::ScanConfig valid_config;
    valid_config.max_partitions = 8;
    valid_config.min_partition_size = 1000;
    valid_config.prefetch_batches = 2;
    valid_config.enable_predicate_pushdown = true;
    valid_config.enable_projection_pushdown = false;

    // Should construct successfully with valid config
    EXPECT_NO_THROW({ ParallelTableScan valid_scan("schema", "table", valid_config); });
}

/// Performance test for partition planning
TEST_F(ParallelTableScanTest, PartitionPlanningPerformance)
{
    const std::uint64_t large_table_rows = 100000000; // 100M rows
    const std::uint32_t target_workers = 16;

    auto start_time = std::chrono::steady_clock::now();

    auto partitions = scan_->plan_partitions(large_table_rows, target_workers);

    auto end_time = std::chrono::steady_clock::now();
    auto planning_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Planning should be fast (< 10ms for 100M rows)
    EXPECT_LT(planning_time.count(), 10);

    // Should create reasonable number of partitions
    EXPECT_GT(partitions.size(), 0);
    EXPECT_LE(partitions.size(), target_workers);

    // Check total coverage
    std::uint64_t total_rows = 0;
    for (const auto& partition : partitions) {
        total_rows += partition.estimated_rows;
    }
    EXPECT_EQ(total_rows, large_table_rows);
}

/// Integration test combining all functionality
TEST_F(ParallelTableScanTest, IntegrationTest)
{
    const std::uint64_t total_rows = 5000;
    const std::uint32_t target_workers = 4;

    // Plan partitions
    auto partitions = scan_->plan_partitions(total_rows, target_workers);
    EXPECT_GT(partitions.size(), 0);

    // Create work units
    std::vector<std::string> projections = {"id", "name", "value"};
    std::string predicate = "id > 1000";
    auto work_units = scan_->create_scan_work_units(partitions, projections, predicate);
    EXPECT_EQ(work_units.size(), partitions.size());

    // Execute partition scans
    std::vector<std::shared_ptr<WorkResult>> results;
    for (const auto& partition : partitions) {
        auto result = scan_->execute_partition_scan(partition, projections, predicate);
        ASSERT_NE(result, nullptr);
        EXPECT_TRUE(result->success);
        results.push_back(result);
    }

    // Merge results
    auto final_result = scan_->merge_partition_results(results);
    ASSERT_NE(final_result, nullptr);
    EXPECT_TRUE(final_result->success);
    EXPECT_EQ(final_result->rows_processed, total_rows);
    EXPECT_LT(final_result->rows_returned, total_rows); // Some filtering

    // Check final statistics
    auto final_stats = scan_->get_statistics();
    EXPECT_EQ(final_stats.total_partitions, partitions.size());
    EXPECT_EQ(final_stats.total_rows_scanned, total_rows);
    EXPECT_GT(final_stats.total_rows_filtered, 0);
    EXPECT_GT(final_stats.load_balance_factor, 0.0);
}

/// Run all tests
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
