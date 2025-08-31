#include "scratchbird/engine/parallel_executor.h"

#include <algorithm>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace scratchbird::engine;

// Mock classes for testing (minimal implementation)
namespace scratchbird::engine
{
    class Aggregate
    {
      public:
        Aggregate() = default;
        virtual ~Aggregate() = default;
    };

    class Sort
    {
      public:
        Sort() = default;
        virtual ~Sort() = default;
    };
} // namespace scratchbird::engine

class ParallelAggregationSortingTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        config_ = ParallelConfig{};
        config_.max_worker_threads = 4;
        config_.min_table_size_kb = 1024;
        config_.cpu_utilization_threshold = 0.8;

        executor_ = std::make_unique<ParallelQueryExecutor>(config_);
    }

    void TearDown() override
    {
        executor_.reset();
    }

    ParallelConfig config_;
    std::unique_ptr<ParallelQueryExecutor> executor_;

    // Test data constants
    static constexpr std::uint64_t SMALL_TABLE_ROWS = 1000;
    static constexpr std::uint64_t MEDIUM_TABLE_ROWS = 50000;
    static constexpr std::uint64_t LARGE_TABLE_ROWS = 500000;
    static constexpr std::uint32_t TEST_WORKER_COUNT = 4;
};

// ==================== Parallel Aggregation Tests ====================

TEST_F(ParallelAggregationSortingTest, ParallelAggregationBasicFunctionality)
{
    // Create a parallel aggregation instance
    ParallelAggregation parallel_agg("test_basic_aggregation");

    std::vector<std::string> group_by_keys = {"customer_id", "product_category"};
    std::vector<std::string> aggregate_exprs = {"SUM(amount)", "COUNT(*)", "AVG(price)"};

    // Plan aggregation partitions
    auto partitions = parallel_agg.plan_aggregation_partitions(MEDIUM_TABLE_ROWS, group_by_keys,
                                                               aggregate_exprs, TEST_WORKER_COUNT);

    EXPECT_GT(partitions.size(), 0);
    EXPECT_LE(partitions.size(), TEST_WORKER_COUNT);

    // Verify partition properties
    std::uint64_t total_rows = 0;
    for (const auto& partition : partitions) {
        EXPECT_GT(partition.estimated_rows, 0);
        EXPECT_GT(partition.estimated_memory_mb, 0);
        EXPECT_EQ(partition.group_by_keys, group_by_keys);
        EXPECT_EQ(partition.aggregate_exprs, aggregate_exprs);
        EXPECT_FALSE(partition.partition_key_range.empty());
        total_rows += partition.estimated_rows;
    }

    EXPECT_EQ(total_rows, MEDIUM_TABLE_ROWS);
}

TEST_F(ParallelAggregationSortingTest, ParallelAggregationPartitionPlanning)
{
    ParallelAggregation::AggregationConfig agg_config;
    agg_config.max_partitions = 8;
    agg_config.min_partition_size = 5000;
    agg_config.hash_table_memory_mb = 128;

    ParallelAggregation parallel_agg("test_partition_planning", agg_config);

    std::vector<std::string> group_by_keys = {"region", "category", "subcategory"};
    std::vector<std::string> aggregate_exprs = {"SUM(sales)", "COUNT(DISTINCT customer_id)",
                                                "MAX(order_date)"};

    // Test with different input sizes
    auto small_partitions = parallel_agg.plan_aggregation_partitions(
        SMALL_TABLE_ROWS, group_by_keys, aggregate_exprs, 2);
    auto large_partitions = parallel_agg.plan_aggregation_partitions(
        LARGE_TABLE_ROWS, group_by_keys, aggregate_exprs, 8);

    // Small table should create fewer partitions
    EXPECT_LE(small_partitions.size(), 2);

    // Large table should create more partitions
    EXPECT_GT(large_partitions.size(), small_partitions.size());
    EXPECT_LE(large_partitions.size(), agg_config.max_partitions);

    // Verify memory estimation scales with partition size
    for (const auto& partition : large_partitions) {
        EXPECT_GE(partition.estimated_memory_mb, 1);
        EXPECT_LE(partition.estimated_memory_mb, agg_config.hash_table_memory_mb);
    }
}

TEST_F(ParallelAggregationSortingTest, ParallelAggregationWorkUnitCreation)
{
    ParallelAggregation parallel_agg("test_work_units");

    std::vector<std::string> group_by_keys = {"store_id"};
    std::vector<std::string> aggregate_exprs = {"SUM(revenue)", "COUNT(*)"};

    auto partitions = parallel_agg.plan_aggregation_partitions(MEDIUM_TABLE_ROWS, group_by_keys,
                                                               aggregate_exprs, 3);
    ASSERT_GT(partitions.size(), 0);

    // Test partial aggregation work units
    auto partial_work_units = parallel_agg.create_partial_aggregation_work_units(partitions);
    EXPECT_EQ(partial_work_units.size(), partitions.size());

    for (std::size_t i = 0; i < partial_work_units.size(); ++i) {
        const auto& work_unit = partial_work_units[i];
        const auto& partition = partitions[i];

        EXPECT_EQ(work_unit->partition_id, partition.partition_id);
        EXPECT_EQ(work_unit->start_offset, partition.start_row);
        EXPECT_EQ(work_unit->end_offset, partition.end_row);
        EXPECT_EQ(work_unit->estimated_rows, partition.estimated_rows);
        EXPECT_GT(work_unit->estimated_memory_mb, 0);

        // Verify custom data
        EXPECT_EQ(work_unit->custom_data.at("operation_type"), "partial_aggregation");
        EXPECT_EQ(work_unit->custom_data.at("group_by_count"), "1");
        EXPECT_EQ(work_unit->custom_data.at("aggregate_count"), "2");
    }

    // Test final aggregation work units
    auto final_work_units = parallel_agg.create_final_aggregation_work_units(partitions);
    EXPECT_EQ(final_work_units.size(), partitions.size());

    for (const auto& work_unit : final_work_units) {
        EXPECT_EQ(work_unit->custom_data.at("operation_type"), "final_aggregation");
    }
}

TEST_F(ParallelAggregationSortingTest, ParallelAggregationExecution)
{
    ParallelAggregation parallel_agg("test_execution");

    std::vector<std::string> group_by_keys = {"department"};
    std::vector<std::string> aggregate_exprs = {"SUM(salary)", "AVG(experience)"};

    auto partitions =
        parallel_agg.plan_aggregation_partitions(10000, group_by_keys, aggregate_exprs, 2);
    ASSERT_EQ(partitions.size(), 2);

    // Execute partial aggregation on both partitions
    std::vector<std::shared_ptr<WorkResult>> partial_results;
    for (const auto& partition : partitions) {
        auto result = parallel_agg.execute_partial_aggregation(partition);
        ASSERT_NE(result, nullptr);
        EXPECT_TRUE(result->success);
        EXPECT_GT(result->rows_processed, 0);
        EXPECT_GT(result->rows_returned, 0);
        EXPECT_GT(result->memory_used_mb, 0);
        EXPECT_FALSE(result->result_data.empty());

        // Verify aggregation-specific statistics
        EXPECT_GT(result->statistics.at("groups_created"), 0);
        EXPECT_GE(result->statistics.at("hash_collisions"), 0);

        partial_results.push_back(result);
    }

    // Execute final aggregation
    auto final_result = parallel_agg.execute_final_aggregation(partitions[0], {partial_results[0]});
    ASSERT_NE(final_result, nullptr);
    EXPECT_TRUE(final_result->success);

    // Merge all results
    auto merged_result =
        parallel_agg.merge_aggregation_results({final_result}, group_by_keys, aggregate_exprs);
    ASSERT_NE(merged_result, nullptr);
    EXPECT_TRUE(merged_result->success);
    EXPECT_GT(merged_result->rows_returned, 0);

    // Verify merge statistics
    EXPECT_EQ(merged_result->statistics.at("total_partitions_merged"), 1);
    EXPECT_GT(merged_result->statistics.at("total_final_groups"), 0);
}

TEST_F(ParallelAggregationSortingTest, ParallelAggregationStatistics)
{
    ParallelAggregation parallel_agg("test_statistics");

    // Initial statistics should be zero
    auto initial_stats = parallel_agg.get_statistics();
    EXPECT_EQ(initial_stats.total_partitions, 0);
    EXPECT_EQ(initial_stats.input_rows_processed, 0);
    EXPECT_EQ(initial_stats.partial_groups_created, 0);

    // Execute some aggregation operations
    std::vector<std::string> group_by_keys = {"category"};
    std::vector<std::string> aggregate_exprs = {"COUNT(*)"};

    auto partitions =
        parallel_agg.plan_aggregation_partitions(5000, group_by_keys, aggregate_exprs, 2);

    for (const auto& partition : partitions) {
        auto result = parallel_agg.execute_partial_aggregation(partition);
        EXPECT_TRUE(result->success);
    }

    // Verify statistics were updated
    auto final_stats = parallel_agg.get_statistics();
    EXPECT_GT(final_stats.total_partitions, 0);
    EXPECT_GT(final_stats.input_rows_processed, 0);
    EXPECT_GT(final_stats.partial_groups_created, 0);
    EXPECT_GT(final_stats.partial_agg_time.count(), 0);

    // Test statistics reset
    parallel_agg.reset_statistics();
    auto reset_stats = parallel_agg.get_statistics();
    EXPECT_EQ(reset_stats.input_rows_processed, 0);
    EXPECT_EQ(reset_stats.partial_groups_created, 0);
}

// ==================== Parallel Sort Tests ====================

TEST_F(ParallelAggregationSortingTest, ParallelSortBasicFunctionality)
{
    ParallelSort parallel_sort("test_basic_sort");

    std::vector<std::string> sort_keys = {"timestamp", "customer_id", "amount"};
    std::vector<bool> sort_ascending = {false, true, false}; // DESC, ASC, DESC

    // Plan sort partitions
    auto partitions = parallel_sort.plan_sort_partitions(MEDIUM_TABLE_ROWS, sort_keys,
                                                         sort_ascending, TEST_WORKER_COUNT);

    EXPECT_GT(partitions.size(), 0);
    EXPECT_LE(partitions.size(), TEST_WORKER_COUNT);

    // Verify partition properties
    std::uint64_t total_rows = 0;
    for (const auto& partition : partitions) {
        EXPECT_GT(partition.estimated_rows, 0);
        EXPECT_GT(partition.estimated_memory_mb, 0);
        EXPECT_EQ(partition.sort_keys, sort_keys);
        EXPECT_EQ(partition.sort_ascending, sort_ascending);
        EXPECT_FALSE(partition.key_range_min.empty());
        EXPECT_FALSE(partition.key_range_max.empty());
        total_rows += partition.estimated_rows;
    }

    EXPECT_EQ(total_rows, MEDIUM_TABLE_ROWS);
}

TEST_F(ParallelAggregationSortingTest, ParallelSortPartitionPlanning)
{
    ParallelSort::SortConfig sort_config;
    sort_config.max_partitions = 6;
    sort_config.min_partition_size = 10000;
    sort_config.sort_memory_mb = 256;
    sort_config.enable_external_sort = true;

    ParallelSort parallel_sort("test_sort_planning", sort_config);

    std::vector<std::string> sort_keys = {"order_date", "priority", "region"};
    std::vector<bool> sort_ascending = {false, true, true};

    // Test with different input sizes
    auto small_partitions =
        parallel_sort.plan_sort_partitions(SMALL_TABLE_ROWS, sort_keys, sort_ascending, 2);
    auto large_partitions =
        parallel_sort.plan_sort_partitions(LARGE_TABLE_ROWS, sort_keys, sort_ascending, 6);

    // Small table might create fewer partitions due to min size
    EXPECT_LE(small_partitions.size(), 2);

    // Large table should create more partitions
    EXPECT_GT(large_partitions.size(), small_partitions.size());
    EXPECT_LE(large_partitions.size(), sort_config.max_partitions);

    // Verify memory estimation
    for (const auto& partition : large_partitions) {
        EXPECT_GE(partition.estimated_memory_mb, 1);
        // May exceed sort_memory_mb for very large partitions (would trigger external sort)
    }

    // Verify statistics were updated
    auto stats = parallel_sort.get_statistics();
    EXPECT_GT(stats.total_partitions, 0);
    EXPECT_GT(stats.average_partition_size_mb, 0.0);
}

TEST_F(ParallelAggregationSortingTest, ParallelSortWorkUnitCreation)
{
    ParallelSort parallel_sort("test_sort_work_units");

    std::vector<std::string> sort_keys = {"product_name", "price"};
    std::vector<bool> sort_ascending = {true, false};

    auto partitions =
        parallel_sort.plan_sort_partitions(MEDIUM_TABLE_ROWS, sort_keys, sort_ascending, 3);
    ASSERT_GT(partitions.size(), 0);

    // Test local sort work units
    auto local_work_units = parallel_sort.create_local_sort_work_units(partitions);
    EXPECT_EQ(local_work_units.size(), partitions.size());

    for (std::size_t i = 0; i < local_work_units.size(); ++i) {
        const auto& work_unit = local_work_units[i];
        const auto& partition = partitions[i];

        EXPECT_EQ(work_unit->partition_id, partition.partition_id);
        EXPECT_EQ(work_unit->start_offset, partition.start_row);
        EXPECT_EQ(work_unit->end_offset, partition.end_row);
        EXPECT_EQ(work_unit->estimated_rows, partition.estimated_rows);
        EXPECT_GT(work_unit->estimated_memory_mb, 0);

        // Verify custom data
        EXPECT_EQ(work_unit->custom_data.at("operation_type"), "local_sort");
        EXPECT_EQ(work_unit->custom_data.at("sort_key_count"), "2");
        EXPECT_FALSE(work_unit->custom_data.at("sort_keys").empty());
        EXPECT_FALSE(work_unit->custom_data.at("sort_directions").empty());
    }

    // Test merge work units
    auto merge_work_units = parallel_sort.create_merge_work_units(partitions, 1);
    EXPECT_GT(merge_work_units.size(), 0);

    for (const auto& work_unit : merge_work_units) {
        EXPECT_EQ(work_unit->custom_data.at("operation_type"), "merge_sort");
        EXPECT_EQ(work_unit->custom_data.at("merge_level"), "1");
    }
}

TEST_F(ParallelAggregationSortingTest, ParallelSortExecution)
{
    ParallelSort parallel_sort("test_sort_execution");

    std::vector<std::string> sort_keys = {"last_name", "first_name"};
    std::vector<bool> sort_ascending = {true, true};

    auto partitions = parallel_sort.plan_sort_partitions(20000, sort_keys, sort_ascending, 3);
    ASSERT_GT(partitions.size(), 0);

    // Execute local sort on all partitions
    std::vector<std::shared_ptr<WorkResult>> local_results;
    for (const auto& partition : partitions) {
        auto result = parallel_sort.execute_local_sort(partition);
        ASSERT_NE(result, nullptr);
        EXPECT_TRUE(result->success);
        EXPECT_GT(result->rows_processed, 0);
        EXPECT_EQ(result->rows_returned, result->rows_processed); // Sort doesn't change row count
        EXPECT_GT(result->memory_used_mb, 0);
        EXPECT_FALSE(result->result_data.empty());

        // Verify sort-specific statistics
        EXPECT_GT(result->statistics.at("comparisons_performed"), 0);
        EXPECT_GE(result->statistics.at("sort_algorithm"), 0);
        EXPECT_LE(result->statistics.at("sort_algorithm"), 1);

        local_results.push_back(result);
    }

    // Execute merge phase
    auto merge_result = parallel_sort.execute_merge_phase(partitions, local_results, 1);
    ASSERT_NE(merge_result, nullptr);
    EXPECT_TRUE(merge_result->success);

    // Final merge
    auto final_result =
        parallel_sort.merge_sorted_results({merge_result}, sort_keys, sort_ascending);
    ASSERT_NE(final_result, nullptr);
    EXPECT_TRUE(final_result->success);
    EXPECT_GT(final_result->rows_returned, 0);

    // Verify final merge statistics
    EXPECT_EQ(final_result->statistics.at("total_partitions_merged"), 1);
    EXPECT_GE(final_result->statistics.at("total_comparisons"),
              0); // Allow zero for small data sets
    EXPECT_EQ(final_result->statistics.at("sort_key_count"), sort_keys.size());
}

TEST_F(ParallelAggregationSortingTest, ParallelSortMemoryEstimation)
{
    ParallelSort parallel_sort("test_memory_estimation");

    std::vector<std::string> simple_keys = {"id"};
    std::vector<std::string> complex_keys = {"field1", "field2", "field3", "field4", "field5"};

    // Test memory estimation with different key complexities
    std::uint64_t simple_memory = parallel_sort.estimate_sort_memory(10000, simple_keys);
    std::uint64_t complex_memory = parallel_sort.estimate_sort_memory(10000, complex_keys);

    EXPECT_GT(simple_memory, 0);
    EXPECT_GT(complex_memory, simple_memory); // More keys should require more memory

    // Test external sort decision
    EXPECT_FALSE(parallel_sort.should_use_external_sort(100)); // Small memory usage

    // Test with configuration that would trigger external sort
    ParallelSort::SortConfig config;
    config.sort_memory_mb = 50;
    config.enable_external_sort = true;
    ParallelSort external_sort("test_external", config);
    EXPECT_TRUE(external_sort.should_use_external_sort(100)); // Exceeds limit
}

TEST_F(ParallelAggregationSortingTest, ParallelSortStatistics)
{
    ParallelSort parallel_sort("test_sort_statistics");

    // Initial statistics should be zero
    auto initial_stats = parallel_sort.get_statistics();
    EXPECT_EQ(initial_stats.total_partitions, 0);
    EXPECT_EQ(initial_stats.input_rows_processed, 0);
    EXPECT_EQ(initial_stats.comparisons_performed, 0);

    // Execute some sort operations
    std::vector<std::string> sort_keys = {"name"};
    std::vector<bool> sort_ascending = {true};

    auto partitions = parallel_sort.plan_sort_partitions(5000, sort_keys, sort_ascending, 2);

    for (const auto& partition : partitions) {
        auto result = parallel_sort.execute_local_sort(partition);
        EXPECT_TRUE(result->success);
    }

    // Verify statistics were updated
    auto final_stats = parallel_sort.get_statistics();
    EXPECT_GT(final_stats.total_partitions, 0);
    EXPECT_GT(final_stats.input_rows_processed, 0);
    EXPECT_GT(final_stats.comparisons_performed, 0);
    EXPECT_GT(final_stats.local_sort_time.count(), 0);

    // Test statistics reset
    parallel_sort.reset_statistics();
    auto reset_stats = parallel_sort.get_statistics();
    EXPECT_EQ(reset_stats.input_rows_processed, 0);
    EXPECT_EQ(reset_stats.comparisons_performed, 0);
}

// ==================== Parallel Query Executor Integration Tests ====================

TEST_F(ParallelAggregationSortingTest, ExecutorParallelAggregateIntegration)
{
    // Create mock aggregate node (using minimal mock class)
    auto mock_agg_node = std::make_shared<Aggregate>();

    // Execute parallel aggregation through executor
    auto result = executor_->execute_parallel_aggregate(mock_agg_node, TEST_WORKER_COUNT);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->success);
    EXPECT_GT(result->rows_returned, 0);
    EXPECT_FALSE(result->result_data.empty());

    // Verify custom data is set correctly
    EXPECT_EQ(result->custom_data["operation_type"], "parallel_aggregation");
    EXPECT_GT(std::stoull(result->custom_data["aggregation_partitions"]), 0);
}

TEST_F(ParallelAggregationSortingTest, ExecutorParallelSortIntegration)
{
    // Create mock sort node (using minimal mock class)
    auto mock_sort_node = std::make_shared<Sort>();

    // Execute parallel sort through executor
    auto result = executor_->execute_parallel_sort(mock_sort_node, TEST_WORKER_COUNT);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->success);
    EXPECT_GT(result->rows_returned, 0);
    EXPECT_FALSE(result->result_data.empty());

    // Verify custom data is set correctly
    EXPECT_EQ(result->custom_data["operation_type"], "parallel_sort");
    EXPECT_GT(std::stoull(result->custom_data["sort_partitions"]), 0);
}

TEST_F(ParallelAggregationSortingTest, ExecutorErrorHandling)
{
    // Test with null aggregate node
    auto result1 = executor_->execute_parallel_aggregate(nullptr, TEST_WORKER_COUNT);
    EXPECT_FALSE(result1->success);
    EXPECT_FALSE(result1->error_message.empty());

    // Test with zero worker count
    auto mock_agg_node = std::make_shared<Aggregate>();
    auto result2 = executor_->execute_parallel_aggregate(mock_agg_node, 0);
    EXPECT_FALSE(result2->success);
    EXPECT_FALSE(result2->error_message.empty());

    // Test with null sort node
    auto result3 = executor_->execute_parallel_sort(nullptr, TEST_WORKER_COUNT);
    EXPECT_FALSE(result3->success);
    EXPECT_FALSE(result3->error_message.empty());

    // Test sort with zero workers
    auto mock_sort_node = std::make_shared<Sort>();
    auto result4 = executor_->execute_parallel_sort(mock_sort_node, 0);
    EXPECT_FALSE(result4->success);
    EXPECT_FALSE(result4->error_message.empty());
}

// ==================== Configuration and Edge Cases Tests ====================

TEST_F(ParallelAggregationSortingTest, AggregationConfigurationValidation)
{
    // Test custom aggregation configuration
    ParallelAggregation::AggregationConfig config;
    config.max_partitions = 16;
    config.min_partition_size = 2000;
    config.hash_table_memory_mb = 512;
    config.enable_two_phase_agg = true;
    config.enable_preaggregation = true;
    config.group_cardinality_threshold = 0.05;

    ParallelAggregation parallel_agg("test_config", config);

    std::vector<std::string> group_by_keys = {"region", "product"};
    std::vector<std::string> aggregate_exprs = {"SUM(sales)", "COUNT(*)"};

    // Test configuration is applied
    auto partitions =
        parallel_agg.plan_aggregation_partitions(100000, group_by_keys, aggregate_exprs, 10);

    EXPECT_LE(partitions.size(), config.max_partitions);

    // All partitions should meet minimum size requirement or be the remainder
    std::uint64_t total_partitions = partitions.size();
    for (std::size_t i = 0; i < partitions.size(); ++i) {
        if (i < total_partitions - 1) {
            EXPECT_GE(partitions[i].estimated_rows, config.min_partition_size);
        }
    }
}

TEST_F(ParallelAggregationSortingTest, SortConfigurationValidation)
{
    // Test custom sort configuration
    ParallelSort::SortConfig config;
    config.max_partitions = 12;
    config.min_partition_size = 15000;
    config.sort_memory_mb = 1024;
    config.enable_external_sort = true;
    config.enable_quicksort_hybrid = true;
    config.quicksort_threshold = 25000;
    config.max_merge_ways = 8;

    ParallelSort parallel_sort("test_sort_config", config);

    std::vector<std::string> sort_keys = {"timestamp", "user_id"};
    std::vector<bool> sort_ascending = {false, true};

    // Test configuration is applied
    auto partitions = parallel_sort.plan_sort_partitions(200000, sort_keys, sort_ascending, 10);

    EXPECT_LE(partitions.size(), config.max_partitions);

    // Test merge ways calculation
    std::uint32_t optimal_ways = parallel_sort.calculate_optimal_merge_ways(8, 500);
    EXPECT_LE(optimal_ways, config.max_merge_ways);
    EXPECT_GE(optimal_ways, 2);
}

TEST_F(ParallelAggregationSortingTest, EdgeCaseHandling)
{
    ParallelAggregation parallel_agg("test_edge_cases");
    ParallelSort parallel_sort("test_edge_cases");

    std::vector<std::string> group_by_keys = {"id"};
    std::vector<std::string> aggregate_exprs = {"COUNT(*)"};
    std::vector<std::string> sort_keys = {"value"};
    std::vector<bool> sort_ascending = {true};

    // Test with zero input rows
    auto empty_agg_partitions =
        parallel_agg.plan_aggregation_partitions(0, group_by_keys, aggregate_exprs, 4);
    EXPECT_TRUE(empty_agg_partitions.empty());

    auto empty_sort_partitions =
        parallel_sort.plan_sort_partitions(0, sort_keys, sort_ascending, 4);
    EXPECT_TRUE(empty_sort_partitions.empty());

    // Test with zero workers
    auto zero_worker_agg =
        parallel_agg.plan_aggregation_partitions(1000, group_by_keys, aggregate_exprs, 0);
    EXPECT_TRUE(zero_worker_agg.empty());

    auto zero_worker_sort = parallel_sort.plan_sort_partitions(1000, sort_keys, sort_ascending, 0);
    EXPECT_TRUE(zero_worker_sort.empty());

    // Test with empty keys
    std::vector<std::string> empty_keys;
    auto empty_sort_keys = parallel_sort.plan_sort_partitions(1000, empty_keys, sort_ascending, 4);
    EXPECT_TRUE(empty_sort_keys.empty());

    // Test with single row
    auto single_row_partitions =
        parallel_agg.plan_aggregation_partitions(1, group_by_keys, aggregate_exprs, 4);
    EXPECT_EQ(single_row_partitions.size(), 1);
    EXPECT_EQ(single_row_partitions[0].estimated_rows, 1);
}

TEST_F(ParallelAggregationSortingTest, PerformanceAndScalability)
{
    // Test with large datasets to verify scalability
    ParallelAggregation large_agg("performance_test_agg");
    ParallelSort large_sort("performance_test_sort");

    std::vector<std::string> group_by_keys = {"category", "region", "quarter"};
    std::vector<std::string> aggregate_exprs = {"SUM(revenue)", "COUNT(DISTINCT customer_id)",
                                                "AVG(satisfaction_score)"};
    std::vector<std::string> sort_keys = {"timestamp", "priority_score", "customer_tier"};
    std::vector<bool> sort_ascending = {false, false, true};

    const std::uint64_t VERY_LARGE_ROWS = 10000000; // 10M rows
    const std::uint32_t MAX_WORKERS = 8;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Plan partitions for large dataset
    auto agg_partitions = large_agg.plan_aggregation_partitions(VERY_LARGE_ROWS, group_by_keys,
                                                                aggregate_exprs, MAX_WORKERS);
    auto sort_partitions =
        large_sort.plan_sort_partitions(VERY_LARGE_ROWS, sort_keys, sort_ascending, MAX_WORKERS);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto planning_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Planning should be fast even for large datasets
    EXPECT_LT(planning_time.count(), 1000); // Less than 1 second

    // Verify partition sizes are reasonable
    EXPECT_GT(agg_partitions.size(), 0);
    EXPECT_LE(agg_partitions.size(), MAX_WORKERS);
    EXPECT_GT(sort_partitions.size(), 0);
    EXPECT_LE(sort_partitions.size(), MAX_WORKERS);

    // Each partition should handle a reasonable number of rows
    for (const auto& partition : agg_partitions) {
        EXPECT_GT(partition.estimated_rows, 100000); // At least 100K rows per partition
        EXPECT_LE(partition.estimated_rows, VERY_LARGE_ROWS);
    }

    for (const auto& partition : sort_partitions) {
        EXPECT_GT(partition.estimated_rows, 100000);
        EXPECT_LE(partition.estimated_rows, VERY_LARGE_ROWS);
    }
}

// ==================== Helper Functions and Utilities Tests ====================

TEST_F(ParallelAggregationSortingTest, GroupCardinalityEstimation)
{
    ParallelAggregation parallel_agg("test_cardinality");

    // Test empty group by (should return 1)
    std::vector<std::string> no_group_by;
    std::uint64_t no_group_cardinality =
        parallel_agg.estimate_group_cardinality(no_group_by, 10000);
    EXPECT_EQ(no_group_cardinality, 1);

    // Test single group by key
    std::vector<std::string> single_key = {"category"};
    std::uint64_t single_cardinality = parallel_agg.estimate_group_cardinality(single_key, 10000);
    EXPECT_GT(single_cardinality, 1);
    EXPECT_LE(single_cardinality, 10000);

    // Test multiple group by keys (should have lower cardinality than single key)
    std::vector<std::string> multi_keys = {"category", "region", "quarter"};
    std::uint64_t multi_cardinality = parallel_agg.estimate_group_cardinality(multi_keys, 10000);
    EXPECT_GT(multi_cardinality, 1);
    EXPECT_LT(multi_cardinality, single_cardinality); // More selective
}

TEST_F(ParallelAggregationSortingTest, AggregationSelectivityCalculation)
{
    ParallelAggregation parallel_agg("test_selectivity");

    // Test empty aggregate expressions
    std::vector<std::string> no_aggs;
    double no_agg_selectivity = parallel_agg.calculate_aggregation_selectivity(no_aggs);
    EXPECT_EQ(no_agg_selectivity, 1.0);

    // Test COUNT expressions (high selectivity)
    std::vector<std::string> count_aggs = {"COUNT(*)", "COUNT(DISTINCT customer_id)"};
    double count_selectivity = parallel_agg.calculate_aggregation_selectivity(count_aggs);
    EXPECT_GT(count_selectivity, 0.0);
    EXPECT_LE(count_selectivity, 1.0);

    // Test mixed expressions
    std::vector<std::string> mixed_aggs = {"SUM(amount)", "AVG(price)", "MAX(date)"};
    double mixed_selectivity = parallel_agg.calculate_aggregation_selectivity(mixed_aggs);
    EXPECT_GT(mixed_selectivity, 0.0);
    EXPECT_LE(mixed_selectivity, 1.0);
}

TEST_F(ParallelAggregationSortingTest, TwoPhaseAggregationDecision)
{
    ParallelAggregation::AggregationConfig config;
    config.enable_two_phase_agg = true;
    config.group_cardinality_threshold = 0.1;

    ParallelAggregation parallel_agg("test_two_phase", config);

    // Large dataset with low cardinality should use two-phase
    EXPECT_TRUE(parallel_agg.should_use_two_phase_aggregation(1000000, 0.05)); // 5% cardinality

    // Small dataset should not use two-phase
    EXPECT_FALSE(parallel_agg.should_use_two_phase_aggregation(10000, 0.05));

    // High cardinality should not use two-phase
    EXPECT_FALSE(parallel_agg.should_use_two_phase_aggregation(1000000, 0.5)); // 50% cardinality

    // Disabled configuration should not use two-phase
    config.enable_two_phase_agg = false;
    ParallelAggregation no_two_phase("test_no_two_phase", config);
    EXPECT_FALSE(no_two_phase.should_use_two_phase_aggregation(1000000, 0.05));
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
