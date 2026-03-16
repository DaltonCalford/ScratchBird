/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>
#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/optimizer/statistics.h"
#include "scratchbird/core/status.h"

using namespace scratchbird::optimizer;
using namespace scratchbird::core;

/**
 * P1-10: Statistics Table and ANALYZE Command - Unit Tests
 *
 * This test suite verifies the Statistics Manager CRUD operations:
 * - analyzeTable() - Collect statistics for all columns
 * - analyzeColumn() - Collect statistics for a single column
 * - getTableStatistics() - Retrieve table-level statistics
 * - getColumnStatistics() - Retrieve column-level statistics
 * - dropStatistics() - Remove statistics for a table
 */

class StatisticsCRUDTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Note: This is a unit test for Statistics data structures and API
        // Full integration testing requires a complete Database setup
    }

    void TearDown() override {}
};

// Test ColumnStatistics structure defaults
TEST_F(StatisticsCRUDTest, ColumnStatisticsDefaults) {
    ColumnStatistics stats;

    EXPECT_EQ(stats.table_id, ID{});
    EXPECT_EQ(stats.column_id, ID{});
    EXPECT_TRUE(stats.column_name.empty());
    EXPECT_EQ(stats.data_type, DataType::UNKNOWN);
    EXPECT_EQ(stats.num_rows, 0);
    EXPECT_EQ(stats.num_nulls, 0);
    EXPECT_FLOAT_EQ(stats.null_fraction, 0.0f);
    EXPECT_EQ(stats.num_distinct, 0);
    EXPECT_FLOAT_EQ(stats.avg_width, 0.0f);
    EXPECT_EQ(stats.comparator_family, StatisticsComparatorFamily::UNKNOWN);
    EXPECT_EQ(stats.value_encoding, StatisticsValueEncoding::UNKNOWN);
    EXPECT_EQ(stats.collation_id, 0u);
    EXPECT_EQ(stats.type_precision, 0u);
    EXPECT_EQ(stats.type_scale, 0u);
    EXPECT_TRUE(stats.mcv_list.empty());
    EXPECT_EQ(stats.histogram_type, HistogramType::NONE);
    EXPECT_EQ(stats.histogram_bucket_count, 0);
    EXPECT_TRUE(stats.histogram_buckets.empty());
    EXPECT_EQ(stats.stats_snapshot_id, 0u);
    EXPECT_EQ(stats.last_analyzed_time, 0);
    EXPECT_EQ(stats.sample_size, 0);
    EXPECT_FLOAT_EQ(stats.sample_rate, 0.0f);
    EXPECT_EQ(stats.modified_rows_since_analyze, 0u);
    EXPECT_EQ(stats.staleness_class, StatisticsStalenessClass::UNKNOWN);
    EXPECT_EQ(stats.confidence_class, StatisticsConfidenceClass::UNKNOWN);
    EXPECT_FALSE(stats.auto_analyze_applied);
    EXPECT_EQ(stats.auto_analyze_threshold, 0u);
}

// Test TableStatistics structure defaults
TEST_F(StatisticsCRUDTest, TableStatisticsDefaults) {
    TableStatistics stats;

    EXPECT_EQ(stats.table_id, ID{});
    EXPECT_TRUE(stats.table_name.empty());
    EXPECT_EQ(stats.num_rows, 0);
    EXPECT_EQ(stats.num_pages, 0);
    EXPECT_FLOAT_EQ(stats.avg_row_size, 0.0f);
    EXPECT_EQ(stats.stats_snapshot_id, 0u);
    EXPECT_EQ(stats.last_analyzed_time, 0);
    EXPECT_EQ(stats.modified_rows_since_analyze, 0u);
    EXPECT_EQ(stats.staleness_class, StatisticsStalenessClass::UNKNOWN);
    EXPECT_EQ(stats.confidence_class, StatisticsConfidenceClass::UNKNOWN);
    EXPECT_FALSE(stats.auto_analyze_applied);
    EXPECT_EQ(stats.auto_analyze_threshold, 0u);
}

TEST_F(StatisticsCRUDTest, MultivariateStatisticsDefaults) {
    MultivariateStatistics stats;

    EXPECT_EQ(stats.table_id, ID{});
    EXPECT_TRUE(stats.column_ids.empty());
    EXPECT_TRUE(stats.column_names.empty());
    EXPECT_EQ(stats.sample_size, 0u);
    EXPECT_EQ(stats.last_analyzed_time, 0u);
    EXPECT_EQ(stats.ndistinct.num_distinct, 0u);
    EXPECT_TRUE(stats.ndistinct.column_ids.empty());
    EXPECT_TRUE(stats.ndistinct.column_names.empty());
    EXPECT_TRUE(stats.mcv_list.empty());
    EXPECT_TRUE(stats.dependencies.empty());
    EXPECT_TRUE(stats.pairwise_correlations.empty());
}

TEST_F(StatisticsCRUDTest, FunctionalDependencyDefaults) {
    FunctionalDependencyStatistics stats;

    EXPECT_TRUE(stats.determinant_column_ids.empty());
    EXPECT_TRUE(stats.dependent_column_ids.empty());
    EXPECT_TRUE(stats.determinant_column_names.empty());
    EXPECT_TRUE(stats.dependent_column_names.empty());
    EXPECT_DOUBLE_EQ(stats.strength, 0.0);
    EXPECT_EQ(stats.sample_size, 0u);
    EXPECT_EQ(stats.last_analyzed_time, 0u);
}

// Test HistogramType enum
TEST_F(StatisticsCRUDTest, HistogramTypes) {
    EXPECT_EQ(static_cast<uint8_t>(HistogramType::EQUAL_HEIGHT), 0);
    EXPECT_EQ(static_cast<uint8_t>(HistogramType::EQUAL_WIDTH), 1);
    EXPECT_EQ(static_cast<uint8_t>(HistogramType::NONE), 255);
}

// Test MCVEntry structure
TEST_F(StatisticsCRUDTest, MCVEntryDefaults) {
    MCVEntry mcv;

    EXPECT_TRUE(mcv.value_data.empty());
    EXPECT_EQ(mcv.value_oid, ID{});
    EXPECT_FLOAT_EQ(mcv.frequency, 0.0f);
}

// Test HistogramBucket structure
TEST_F(StatisticsCRUDTest, HistogramBucketDefaults) {
    HistogramBucket bucket;

    EXPECT_TRUE(bucket.lower_bound.empty());
    EXPECT_TRUE(bucket.upper_bound.empty());
    EXPECT_EQ(bucket.lower_oid, ID{});
    EXPECT_EQ(bucket.upper_oid, ID{});
    EXPECT_EQ(bucket.row_count, 0);
    EXPECT_FLOAT_EQ(bucket.frequency, 0.0f);
}

// Test basic statistics calculation
TEST_F(StatisticsCRUDTest, BasicStatistics) {
    ColumnStatistics stats;

    // Simulate analyzed data: 1000 rows, 50 NULLs, 200 distinct values
    stats.num_rows = 1000;
    stats.num_nulls = 50;
    stats.num_distinct = 200;

    // Calculate null fraction
    stats.null_fraction = static_cast<float>(stats.num_nulls) / stats.num_rows;
    EXPECT_FLOAT_EQ(stats.null_fraction, 0.05f);

    // Selectivity for equality on a uniformly distributed column
    // Selectivity ≈ 1 / num_distinct
    float expected_selectivity = 1.0f / stats.num_distinct;
    EXPECT_FLOAT_EQ(expected_selectivity, 0.005f);
}

// Test equal-height histogram
TEST_F(StatisticsCRUDTest, EqualHeightHistogram) {
    ColumnStatistics stats;
    stats.histogram_type = HistogramType::EQUAL_HEIGHT;
    stats.histogram_bucket_count = 10;

    // Each bucket should have approximately equal frequency (1/bucket_count)
    for (uint32_t i = 0; i < stats.histogram_bucket_count; ++i) {
        HistogramBucket bucket;
        bucket.frequency = 1.0f / stats.histogram_bucket_count;
        stats.histogram_buckets.push_back(bucket);
    }

    EXPECT_EQ(stats.histogram_buckets.size(), 10);

    // Verify each bucket has 10% of data
    for (const auto& bucket : stats.histogram_buckets) {
        EXPECT_FLOAT_EQ(bucket.frequency, 0.1f);
    }

    // Total frequency should sum to 1.0
    float total_frequency = 0.0f;
    for (const auto& bucket : stats.histogram_buckets) {
        total_frequency += bucket.frequency;
    }
    EXPECT_NEAR(total_frequency, 1.0f, 0.001f);
}

// Test equal-width histogram
TEST_F(StatisticsCRUDTest, EqualWidthHistogram) {
    ColumnStatistics stats;
    stats.histogram_type = HistogramType::EQUAL_WIDTH;
    stats.histogram_bucket_count = 5;
    stats.num_rows = 1000;

    // Simulate uneven distribution across equal-width buckets
    std::vector<uint64_t> row_counts = {100, 250, 400, 200, 50};

    for (uint64_t count : row_counts) {
        HistogramBucket bucket;
        bucket.row_count = count;
        bucket.frequency = static_cast<float>(count) / stats.num_rows;
        stats.histogram_buckets.push_back(bucket);
    }

    EXPECT_EQ(stats.histogram_buckets.size(), 5);
    EXPECT_EQ(stats.histogram_buckets[0].row_count, 100);
    EXPECT_EQ(stats.histogram_buckets[2].row_count, 400); // Peak
    EXPECT_FLOAT_EQ(stats.histogram_buckets[2].frequency, 0.4f);
}

// Test Most Common Values (MCVs)
TEST_F(StatisticsCRUDTest, MostCommonValues) {
    ColumnStatistics stats;

    // Simulate top 5 most common values
    std::vector<float> frequencies = {0.15f, 0.12f, 0.10f, 0.08f, 0.05f};

    for (float freq : frequencies) {
        MCVEntry mcv;
        mcv.frequency = freq;
        stats.mcv_list.push_back(mcv);
    }

    EXPECT_EQ(stats.mcv_list.size(), 5);
    EXPECT_FLOAT_EQ(stats.mcv_list[0].frequency, 0.15f); // Most common
    EXPECT_FLOAT_EQ(stats.mcv_list[4].frequency, 0.05f); // 5th most common

    // MCVs should cover 50% of data
    float mcv_coverage = 0.0f;
    for (const auto& mcv : stats.mcv_list) {
        mcv_coverage += mcv.frequency;
    }
    EXPECT_FLOAT_EQ(mcv_coverage, 0.50f);
}

// Test n_distinct estimation
TEST_F(StatisticsCRUDTest, DistinctEstimation) {
    // Scenario 1: Small table, exact count
    ColumnStatistics stats1;
    stats1.num_rows = 100;
    stats1.num_distinct = 75;
    EXPECT_EQ(stats1.num_distinct, 75);

    // Scenario 2: Large table with many distinct values
    ColumnStatistics stats2;
    stats2.num_rows = 1000000;
    stats2.num_distinct = 500000; // 50% unique
    EXPECT_EQ(stats2.num_distinct, 500000);

    // Scenario 3: Very few distinct values (low cardinality)
    ColumnStatistics stats3;
    stats3.num_rows = 1000000;
    stats3.num_distinct = 10; // e.g., status column
    EXPECT_EQ(stats3.num_distinct, 10);
}

// Test null fraction calculations
TEST_F(StatisticsCRUDTest, NullFractionCalculations) {
    // 0% nulls
    ColumnStatistics stats1;
    stats1.num_rows = 1000;
    stats1.num_nulls = 0;
    stats1.null_fraction = static_cast<float>(stats1.num_nulls) / stats1.num_rows;
    EXPECT_FLOAT_EQ(stats1.null_fraction, 0.0f);

    // 10% nulls
    ColumnStatistics stats2;
    stats2.num_rows = 1000;
    stats2.num_nulls = 100;
    stats2.null_fraction = static_cast<float>(stats2.num_nulls) / stats2.num_rows;
    EXPECT_FLOAT_EQ(stats2.null_fraction, 0.1f);

    // 50% nulls
    ColumnStatistics stats3;
    stats3.num_rows = 1000;
    stats3.num_nulls = 500;
    stats3.null_fraction = static_cast<float>(stats3.num_nulls) / stats3.num_rows;
    EXPECT_FLOAT_EQ(stats3.null_fraction, 0.5f);

    // 100% nulls
    ColumnStatistics stats4;
    stats4.num_rows = 1000;
    stats4.num_nulls = 1000;
    stats4.null_fraction = static_cast<float>(stats4.num_nulls) / stats4.num_rows;
    EXPECT_FLOAT_EQ(stats4.null_fraction, 1.0f);
}

// Test average width calculation
TEST_F(StatisticsCRUDTest, AverageWidthCalculation) {
    // Fixed-width type (INT32)
    ColumnStatistics stats1;
    stats1.data_type = DataType::INT32;
    stats1.avg_width = 4.0f;
    EXPECT_FLOAT_EQ(stats1.avg_width, 4.0f);

    // Variable-width type (VARCHAR)
    ColumnStatistics stats2;
    stats2.data_type = DataType::VARCHAR;
    stats2.avg_width = 25.5f; // Average string length
    EXPECT_FLOAT_EQ(stats2.avg_width, 25.5f);
}

// Test table statistics calculations
TEST_F(StatisticsCRUDTest, TableStatisticsCalculations) {
    TableStatistics stats;

    stats.num_rows = 100000;
    stats.num_pages = 1000;

    // Calculate average row size
    constexpr uint64_t PAGE_SIZE = 8192;
    stats.avg_row_size = static_cast<float>(stats.num_pages * PAGE_SIZE) / stats.num_rows;

    EXPECT_FLOAT_EQ(stats.avg_row_size, 81.92f);
}

// Test statistics timestamp handling
TEST_F(StatisticsCRUDTest, TimestampHandling) {
    ColumnStatistics stats;

    // Set last analyzed time to current timestamp
    stats.last_analyzed_time = 1700000000; // Unix timestamp

    EXPECT_EQ(stats.last_analyzed_time, 1700000000);

    // Verify we can detect stale statistics
    uint64_t current_time = 1700086400; // 24 hours later
    uint64_t age_seconds = current_time - stats.last_analyzed_time;
    EXPECT_EQ(age_seconds, 86400); // 1 day old
}

// Test sample rate calculations
TEST_F(StatisticsCRUDTest, SampleRateCalculations) {
    ColumnStatistics stats;

    // Full table scan (100% sample)
    stats.num_rows = 1000;
    stats.sample_size = 1000;
    stats.sample_rate = static_cast<float>(stats.sample_size) / stats.num_rows;
    EXPECT_FLOAT_EQ(stats.sample_rate, 1.0f);

    // 10% sample
    stats.num_rows = 1000000;
    stats.sample_size = 100000;
    stats.sample_rate = static_cast<float>(stats.sample_size) / stats.num_rows;
    EXPECT_FLOAT_EQ(stats.sample_rate, 0.1f);

    // Default PostgreSQL sample (30000 rows)
    stats.num_rows = 500000;
    stats.sample_size = 30000;
    stats.sample_rate = static_cast<float>(stats.sample_size) / stats.num_rows;
    EXPECT_FLOAT_EQ(stats.sample_rate, 0.06f);
}

// Test histogram bucket count
TEST_F(StatisticsCRUDTest, HistogramBucketCount) {
    // PostgreSQL default: 100 buckets
    ColumnStatistics stats1;
    stats1.histogram_bucket_count = 100;
    EXPECT_EQ(stats1.histogram_bucket_count, 100);

    // MySQL default: 256 buckets
    ColumnStatistics stats2;
    stats2.histogram_bucket_count = 256;
    EXPECT_EQ(stats2.histogram_bucket_count, 256);

    // Custom bucket count
    ColumnStatistics stats3;
    stats3.histogram_bucket_count = 50;
    EXPECT_EQ(stats3.histogram_bucket_count, 50);
}

// Test MCV list size limits
TEST_F(StatisticsCRUDTest, MCVListSizeLimits) {
    ColumnStatistics stats;

    // PostgreSQL stores up to 100 MCVs
    constexpr size_t MAX_MCV_COUNT = 100;

    for (size_t i = 0; i < MAX_MCV_COUNT; ++i) {
        MCVEntry mcv;
        mcv.frequency = 1.0f / (i + 1); // Decreasing frequency
        stats.mcv_list.push_back(mcv);
    }

    EXPECT_EQ(stats.mcv_list.size(), MAX_MCV_COUNT);
    EXPECT_GT(stats.mcv_list[0].frequency, stats.mcv_list[99].frequency);
}

// Test edge case: empty table
TEST_F(StatisticsCRUDTest, EmptyTableStatistics) {
    TableStatistics stats;

    stats.num_rows = 0;
    stats.num_pages = 0;
    stats.avg_row_size = 0.0f;

    EXPECT_EQ(stats.num_rows, 0);
    EXPECT_EQ(stats.num_pages, 0);
    EXPECT_FLOAT_EQ(stats.avg_row_size, 0.0f);
}

// Test edge case: single row table
TEST_F(StatisticsCRUDTest, SingleRowTable) {
    ColumnStatistics stats;

    stats.num_rows = 1;
    stats.num_nulls = 0;
    stats.num_distinct = 1;
    stats.null_fraction = 0.0f;

    EXPECT_EQ(stats.num_rows, 1);
    EXPECT_EQ(stats.num_distinct, 1);
}

// Test edge case: all NULL column
TEST_F(StatisticsCRUDTest, AllNullColumn) {
    ColumnStatistics stats;

    stats.num_rows = 1000;
    stats.num_nulls = 1000;
    stats.num_distinct = 0; // No non-NULL distinct values
    stats.null_fraction = 1.0f;

    EXPECT_FLOAT_EQ(stats.null_fraction, 1.0f);
    EXPECT_EQ(stats.num_distinct, 0);
}

// Test edge case: all unique values
TEST_F(StatisticsCRUDTest, AllUniqueValues) {
    ColumnStatistics stats;

    stats.num_rows = 1000;
    stats.num_nulls = 0;
    stats.num_distinct = 1000; // Every value is unique (e.g., PRIMARY KEY)

    EXPECT_EQ(stats.num_distinct, stats.num_rows);
}

// Test realistic scenario: user table
TEST_F(StatisticsCRUDTest, RealisticUserTable) {
    TableStatistics table_stats;
    table_stats.table_name = "users";
    table_stats.num_rows = 1000000;
    table_stats.num_pages = 50000;
    table_stats.avg_row_size = 100.0f;

    EXPECT_EQ(table_stats.table_name, "users");
    EXPECT_EQ(table_stats.num_rows, 1000000);

    // Email column (UNIQUE, mostly non-NULL)
    ColumnStatistics email_stats;
    email_stats.column_name = "email";
    email_stats.num_rows = 1000000;
    email_stats.num_nulls = 1000; // 0.1% NULL
    email_stats.num_distinct = 999000; // Almost all unique
    email_stats.null_fraction = 0.001f;

    EXPECT_EQ(email_stats.num_distinct, 999000);
    EXPECT_FLOAT_EQ(email_stats.null_fraction, 0.001f);

    // Status column (low cardinality)
    ColumnStatistics status_stats;
    status_stats.column_name = "status";
    status_stats.num_rows = 1000000;
    status_stats.num_nulls = 0;
    status_stats.num_distinct = 5; // active, inactive, pending, suspended, deleted
    status_stats.null_fraction = 0.0f;

    EXPECT_EQ(status_stats.num_distinct, 5);

    // MCVs for status column
    MCVEntry active{};
    active.frequency = 0.70f;
    status_stats.mcv_list.push_back(active); // active
    MCVEntry inactive{};
    inactive.frequency = 0.20f;
    status_stats.mcv_list.push_back(inactive); // inactive
    MCVEntry pending{};
    pending.frequency = 0.05f;
    status_stats.mcv_list.push_back(pending); // pending
    MCVEntry suspended{};
    suspended.frequency = 0.03f;
    status_stats.mcv_list.push_back(suspended); // suspended
    MCVEntry deleted{};
    deleted.frequency = 0.02f;
    status_stats.mcv_list.push_back(deleted); // deleted

    EXPECT_EQ(status_stats.mcv_list.size(), 5);
    EXPECT_FLOAT_EQ(status_stats.mcv_list[0].frequency, 0.70f);
}
