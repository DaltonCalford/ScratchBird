/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/types.h"

namespace scratchbird::optimizer
{

    using ID = core::UuidV7Bytes;

    /**
     * Statistics Catalog - Stores column-level statistics for query optimization
     *
     * Similar to PostgreSQL's pg_statistic catalog table.
     *
     * Phase 1, Task 1.1: Statistics Collection
     */

    /**
     * Histogram Type - Determines how histogram buckets are created
     */
    enum class HistogramType : uint8_t
    {
        EQUAL_HEIGHT = 0,  // Equal number of rows per bucket (PostgreSQL default)
        EQUAL_WIDTH = 1,   // Equal value range per bucket (MySQL default)
        NONE = 255         // No histogram (column has too few distinct values)
    };

    enum class StatisticsComparatorFamily : uint32_t
    {
        UNKNOWN = 0,
        SIGNED_INTEGER = 1,
        UNSIGNED_INTEGER = 2,
        NUMERIC = 3,
        STRING = 4,
        TEMPORAL = 5,
        UUID = 6,
        BOOLEAN = 7,
        BINARY = 8
    };

    enum class StatisticsValueEncoding : uint32_t
    {
        UNKNOWN = 0,
        PLAIN_FIXED = 1,
        PLAIN_LENGTH_PREFIXED = 2,
        UUID_BYTES = 3
    };

    enum class StatisticsStalenessClass : uint32_t
    {
        UNKNOWN = 0,
        FRESH = 1,
        WARM = 2,
        STALE = 3,
        EXPIRED = 4
    };

    enum class StatisticsConfidenceClass : uint32_t
    {
        UNKNOWN = 0,
        LOW = 1,
        MEDIUM = 2,
        HIGH = 3
    };

    /**
     * Most Common Values (MCV) Entry
     *
     * Stores the most frequent values in a column and their frequencies.
     * Used for selectivity estimation of equality predicates.
     */
    struct MCVEntry
    {
        std::vector<uint8_t> value_data;  // Serialized plain value bytes
        ID value_oid;             // TOAST reference for large values (UUID v7, zero if inline)
        float frequency;          // Fraction of rows with this value (0.0-1.0)

        MCVEntry() : value_oid(), frequency(0.0f)
        {
        }
    };

    /**
     * Histogram Bucket
     *
     * Represents a bucket in an equal-height or equal-width histogram.
     * For equal-height: each bucket has ~same number of rows
     * For equal-width: each bucket has ~same value range
     */
    struct HistogramBucket
    {
        std::vector<uint8_t> lower_bound;  // Lower bound value (serialized)
        std::vector<uint8_t> upper_bound;  // Upper bound value (serialized)
        ID lower_oid;              // TOAST reference for large lower bound (UUID v7, zero if inline)
        ID upper_oid;              // TOAST reference for large upper bound (UUID v7, zero if inline)
        uint64_t row_count;        // Number of rows in this bucket (equal-width)
        float frequency;           // Fraction of rows in this bucket (equal-height)

        HistogramBucket() : lower_oid(), upper_oid(), row_count(0), frequency(0.0f)
        {
        }
    };

    /**
     * Column Statistics Info (in-memory structure)
     *
     * This is the in-memory representation loaded from pg_statistic catalog.
     */
    struct ColumnStatistics
    {
        ID table_id;              // Table this column belongs to
        ID column_id;             // Column ID
        std::string column_name;  // Column name (for convenience)
        core::DataType data_type; // Column data type

        // Basic statistics
        uint64_t num_rows;          // Total rows in table at ANALYZE time
        uint64_t num_nulls;         // Number of NULL values
        float null_fraction;        // Fraction of NULLs (num_nulls / num_rows)
        uint64_t num_distinct;      // Number of distinct non-NULL values (or estimate)
        float avg_width;            // Average width in bytes (for variable-length types)

        // Typed statistics metadata
        StatisticsComparatorFamily comparator_family;
        StatisticsValueEncoding value_encoding;
        uint32_t collation_id;
        uint32_t type_precision;
        uint32_t type_scale;

        // Most Common Values (MCV)
        std::vector<MCVEntry> mcv_list;  // Most common values (up to 100)

        // Histogram
        HistogramType histogram_type;
        uint32_t histogram_bucket_count;  // Number of histogram buckets
        std::vector<HistogramBucket> histogram_buckets;

        // Metadata
        uint64_t stats_snapshot_id = 0;
        uint64_t last_analyzed_time;  // Timestamp of last ANALYZE
        uint64_t sample_size;         // Number of rows sampled for statistics
        float sample_rate;            // Fraction of table sampled
        uint64_t modified_rows_since_analyze;
        StatisticsStalenessClass staleness_class;
        StatisticsConfidenceClass confidence_class;
        bool auto_analyze_applied;
        uint64_t auto_analyze_threshold;

        ColumnStatistics()
            : data_type(core::DataType::UNKNOWN),
              num_rows(0),
              num_nulls(0),
              null_fraction(0.0f),
              num_distinct(0),
              avg_width(0.0f),
              comparator_family(StatisticsComparatorFamily::UNKNOWN),
              value_encoding(StatisticsValueEncoding::UNKNOWN),
              collation_id(0),
              type_precision(0),
              type_scale(0),
              histogram_type(HistogramType::NONE),
              histogram_bucket_count(0),
              last_analyzed_time(0),
              sample_size(0),
              sample_rate(0.0f),
              modified_rows_since_analyze(0),
              staleness_class(StatisticsStalenessClass::UNKNOWN),
              confidence_class(StatisticsConfidenceClass::UNKNOWN),
              auto_analyze_applied(false),
              auto_analyze_threshold(0)
        {
        }
    };

    /**
     * Table Statistics Info (in-memory structure)
     *
     * Aggregated statistics at table level.
     */
    struct TableStatistics
    {
        ID table_id;
        std::string table_name;

        uint64_t num_rows;           // Total number of rows
        uint64_t num_pages;          // Number of heap pages
        float avg_row_size;          // Average row size in bytes
        uint64_t stats_snapshot_id = 0;
        uint64_t last_analyzed_time; // Timestamp of last ANALYZE
        uint64_t modified_rows_since_analyze = 0;
        StatisticsStalenessClass staleness_class = StatisticsStalenessClass::UNKNOWN;
        StatisticsConfidenceClass confidence_class = StatisticsConfidenceClass::UNKNOWN;
        bool auto_analyze_applied = false;
        uint64_t auto_analyze_threshold = 0;

        TableStatistics()
            : num_rows(0),
              num_pages(0),
              avg_row_size(0.0f),
              last_analyzed_time(0)
        {
        }
    };

    struct ColumnCorrelationStatistics
    {
        ID table_id;
        ID left_column_id;
        ID right_column_id;
        std::string left_column_name;
        std::string right_column_name;
        double coefficient = 0.0;
        uint64_t sample_size = 0;
        uint64_t last_analyzed_time = 0;
    };

    struct ExpressionStatistics
    {
        ID table_id;
        std::string expression_key;
        ColumnStatistics stats;
    };

    inline auto statisticsStalenessClassName(StatisticsStalenessClass value)
        -> const char *
    {
        switch (value)
        {
            case StatisticsStalenessClass::FRESH:
                return "FRESH";
            case StatisticsStalenessClass::WARM:
                return "WARM";
            case StatisticsStalenessClass::STALE:
                return "STALE";
            case StatisticsStalenessClass::EXPIRED:
                return "EXPIRED";
            case StatisticsStalenessClass::UNKNOWN:
            default:
                return "UNKNOWN";
        }
    }

    inline auto statisticsConfidenceClassName(StatisticsConfidenceClass value)
        -> const char *
    {
        switch (value)
        {
            case StatisticsConfidenceClass::LOW:
                return "LOW";
            case StatisticsConfidenceClass::MEDIUM:
                return "MEDIUM";
            case StatisticsConfidenceClass::HIGH:
                return "HIGH";
            case StatisticsConfidenceClass::UNKNOWN:
            default:
                return "UNKNOWN";
        }
    }

} // namespace scratchbird::optimizer
