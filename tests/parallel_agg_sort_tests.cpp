// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <chrono>
#include <gtest/gtest.h>
#include <random>
#include <scratchbird/engine/parallel_aggregation_sorting.h>
#include <thread>
#include <vector>

using namespace scratchbird::engine;
using namespace std::chrono_literals;

class ParallelAggSortTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Generate test data
        test_rows_ = generate_test_rows(10000);
        small_test_rows_ = generate_test_rows(100);
    }

    std::vector<Row> generate_test_rows(std::size_t count)
    {
        std::vector<Row> rows;
        rows.reserve(count);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> group_dist(1, 10);
        std::uniform_int_distribution<> value_dist(1, 1000);
        std::uniform_real_distribution<> double_dist(1.0, 1000.0);

        for (std::size_t i = 0; i < count; ++i) {
            Row row(4);
            row[0] = Value(group_dist(gen));                               // Group column
            row[1] = Value(value_dist(gen));                               // Integer value
            row[2] = Value(double_dist(gen));                              // Double value
            row[3] = Value("category_" + std::to_string(group_dist(gen))); // String value
            rows.push_back(std::move(row));
        }

        return rows;
    }

    std::vector<Row> test_rows_;
    std::vector<Row> small_test_rows_;
};

// Value tests
TEST_F(ParallelAggSortTest, ValueOperations)
{
    Value int_val(42);
    Value double_val(3.14);
    Value string_val("test");

    EXPECT_EQ(int_val.type, DataType::INTEGER);
    EXPECT_EQ(int_val.int_val, 42);

    EXPECT_EQ(double_val.type, DataType::DOUBLE);
    EXPECT_DOUBLE_EQ(double_val.double_val, 3.14);

    EXPECT_EQ(string_val.type, DataType::VARCHAR);
    EXPECT_EQ(string_val.string_val, "test");

    // Test comparison
    Value int1(10);
    Value int2(20);
    EXPECT_TRUE(int1 < int2);
    EXPECT_FALSE(int2 < int1);
    EXPECT_TRUE(int1 == int1);

    // Test addition
    Value sum = int1 + int2;
    EXPECT_EQ(sum.int_val, 30);

    int1 += int2;
    EXPECT_EQ(int1.int_val, 30);
}

// Row tests
TEST_F(ParallelAggSortTest, RowOperations)
{
    Row row(3);
    row[0] = Value(1);
    row[1] = Value(2.5);
    row[2] = Value("test");

    EXPECT_EQ(row.size(), 3);
    EXPECT_EQ(row[0].int_val, 1);
    EXPECT_DOUBLE_EQ(row[1].double_val, 2.5);
    EXPECT_EQ(row[2].string_val, "test");
}

// PartialAggregate tests
TEST_F(ParallelAggSortTest, PartialAggregateOperations)
{
    PartialAggregate agg1(2, 2);
    agg1.group_keys = {Value(1), Value("group1")};
    agg1.aggregate_values = {Value(10), Value(5)};
    agg1.counts = {1, 1};

    PartialAggregate agg2(2, 2);
    agg2.group_keys = {Value(1), Value("group1")};
    agg2.aggregate_values = {Value(20), Value(3)};
    agg2.counts = {2, 1};

    agg1.combine(agg2);

    EXPECT_EQ(agg1.aggregate_values[0].int_val, 30);
    EXPECT_EQ(agg1.aggregate_values[1].int_val, 8);
    EXPECT_EQ(agg1.counts[0], 3);
    EXPECT_EQ(agg1.counts[1], 2);
}

// ParallelAggregator basic functionality tests
TEST_F(ParallelAggSortTest, BasicAggregation)
{
    ParallelAggregator::Config config;
    config.max_workers = 2;
    config.rows_per_batch = 100;

    ParallelAggregator aggregator(config);

    std::vector<std::size_t> group_columns = {0}; // Group by first column
    std::vector<AggregateColumn> agg_columns = {
        AggregateColumn(1, AggregateFunction::SUM, DataType::INTEGER, "sum_value"),
        AggregateColumn(1, AggregateFunction::COUNT, DataType::BIGINT, "count_value"),
        AggregateColumn(2, AggregateFunction::AVG, DataType::DOUBLE, "avg_double")};

    auto result = aggregator.aggregate(small_test_rows_, group_columns, agg_columns);

    EXPECT_GT(result.size(), 0);
    EXPECT_LE(result.size(), 10); // At most 10 groups

    // Verify that each row has the expected structure
    for (const auto& row : result) {
        EXPECT_EQ(row.size(), 4); // 1 group column + 3 aggregate columns
    }

    auto stats = aggregator.get_statistics();
    EXPECT_EQ(stats.total_input_rows, small_test_rows_.size());
    EXPECT_EQ(stats.total_output_rows, result.size());
    EXPECT_GT(stats.groups_processed, 0);
}

// ParallelSorter basic functionality tests
TEST_F(ParallelAggSortTest, BasicSorting)
{
    ParallelSorter::Config config;
    config.max_workers = 2;
    config.rows_per_chunk = 50;
    config.enable_external_sort = false; // Use in-memory sorting for tests

    ParallelSorter sorter(config);

    std::vector<SortColumn> sort_columns = {
        SortColumn(0, SortDirection::ASCENDING), // Sort by first column ascending
        SortColumn(1, SortDirection::DESCENDING) // Then by second column descending
    };

    auto result = sorter.sort(small_test_rows_, sort_columns);

    EXPECT_EQ(result.size(), small_test_rows_.size());

    // Verify sorting order
    for (std::size_t i = 1; i < result.size(); ++i) {
        const auto& prev = result[i - 1];
        const auto& curr = result[i];

        // First column should be in ascending order
        if (prev[0].int_val == curr[0].int_val) {
            // If first column is equal, second column should be descending
            EXPECT_GE(prev[1].int_val, curr[1].int_val);
        } else {
            EXPECT_LE(prev[0].int_val, curr[0].int_val);
        }
    }

    auto stats = sorter.get_statistics();
    EXPECT_EQ(stats.total_rows, small_test_rows_.size());
    EXPECT_GT(stats.chunks_created, 0);
}

// ParallelAggregationSortingPipeline tests
TEST_F(ParallelAggSortTest, Pipeline)
{
    ParallelAggregationSortingPipeline::Config config;
    config.aggregation_config.max_workers = 2;
    config.sorting_config.max_workers = 2;
    config.enable_pipelined_execution = true;

    ParallelAggregationSortingPipeline pipeline(config);

    std::vector<std::size_t> group_columns = {0};
    std::vector<AggregateColumn> agg_columns = {
        AggregateColumn(1, AggregateFunction::SUM, DataType::INTEGER, "sum_value"),
        AggregateColumn(1, AggregateFunction::COUNT, DataType::BIGINT, "count_value")};
    std::vector<SortColumn> sort_columns = {
        SortColumn(1, SortDirection::DESCENDING) // Sort by sum descending
    };

    auto result =
        pipeline.aggregate_and_sort(small_test_rows_, group_columns, agg_columns, sort_columns);

    EXPECT_GT(result.size(), 0);
    EXPECT_LE(result.size(), 10); // At most 10 groups

    // Verify that results are sorted by sum in descending order
    for (std::size_t i = 1; i < result.size(); ++i) {
        EXPECT_GE(result[i - 1][1].int_val, result[i][1].int_val);
    }

    auto stats = pipeline.get_statistics();
    EXPECT_GT(stats.aggregation_stats.total_input_rows, 0);
    EXPECT_GT(stats.sorting_stats.total_rows, 0);
    EXPECT_EQ(stats.intermediate_rows, result.size());
}

// Utility function tests
TEST_F(ParallelAggSortTest, UtilityFunctions)
{
    using namespace parallel_agg_sort_utils;

    // Test optimal worker calculation
    auto workers = calculate_optimal_workers(100000, 1024 * 1024 * 1024); // 1GB
    EXPECT_GT(workers, 0);
    EXPECT_LE(workers, std::thread::hardware_concurrency());

    // Test memory estimation
    std::vector<AggregateColumn> agg_cols = {
        AggregateColumn(0, AggregateFunction::SUM, DataType::INTEGER),
        AggregateColumn(1, AggregateFunction::COUNT, DataType::BIGINT)};

    auto agg_memory = estimate_aggregation_memory(10000, 100, agg_cols);
    EXPECT_GT(agg_memory, 0);

    auto sort_memory = estimate_sorting_memory(10000, 64);
    EXPECT_GT(sort_memory, 0);

    // Test temp file generation
    auto temp_path = generate_temp_file_path("/tmp", "test");
    EXPECT_FALSE(temp_path.empty());
    EXPECT_NE(temp_path.find("/tmp"), std::string::npos);
    EXPECT_NE(temp_path.find("test"), std::string::npos);

    // Test system memory info
    auto mem_info = get_system_memory_info();
    EXPECT_GT(mem_info.total_memory, 0);
    EXPECT_GT(mem_info.available_memory, 0);
    EXPECT_GT(mem_info.cache_line_size, 0);
    EXPECT_GT(mem_info.cpu_count, 0);
}

// Error handling tests
TEST_F(ParallelAggSortTest, ErrorHandling)
{
    ParallelAggregator aggregator;

    // Test with empty input
    std::vector<Row> empty_rows;
    std::vector<std::size_t> group_columns = {0};
    std::vector<AggregateColumn> agg_columns = {
        AggregateColumn(1, AggregateFunction::SUM, DataType::INTEGER)};

    auto result = aggregator.aggregate(empty_rows, group_columns, agg_columns);
    EXPECT_TRUE(result.empty());
}

// Statistics tracking tests
TEST_F(ParallelAggSortTest, StatisticsTracking)
{
    ParallelAggregator aggregator;
    ParallelSorter sorter;

    // Reset statistics
    aggregator.reset_statistics();
    sorter.reset_statistics();

    auto agg_stats_before = aggregator.get_statistics();
    auto sort_stats_before = sorter.get_statistics();

    EXPECT_EQ(agg_stats_before.total_input_rows, 0);
    EXPECT_EQ(sort_stats_before.total_rows, 0);

    // Perform operations
    std::vector<std::size_t> group_columns = {0};
    std::vector<AggregateColumn> agg_columns = {
        AggregateColumn(1, AggregateFunction::SUM, DataType::INTEGER)};
    std::vector<SortColumn> sort_columns = {SortColumn(0, SortDirection::ASCENDING)};

    auto agg_result = aggregator.aggregate(small_test_rows_, group_columns, agg_columns);
    auto sort_result = sorter.sort(small_test_rows_, sort_columns);

    auto agg_stats_after = aggregator.get_statistics();
    auto sort_stats_after = sorter.get_statistics();

    EXPECT_EQ(agg_stats_after.total_input_rows, small_test_rows_.size());
    EXPECT_EQ(agg_stats_after.total_output_rows, agg_result.size());
    EXPECT_GT(agg_stats_after.total_time.count(), 0);

    EXPECT_EQ(sort_stats_after.total_rows, small_test_rows_.size());
    EXPECT_GT(sort_stats_after.total_time.count(), 0);
}
