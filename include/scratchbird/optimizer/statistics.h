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

    /**
     * Most Common Values (MCV) Entry
     *
     * Stores the most frequent values in a column and their frequencies.
     * Used for selectivity estimation of equality predicates.
     */
    struct MCVEntry
    {
        uint8_t value_data[256];  // Serialized value (TOAST ref if larger)
        ID value_oid;             // TOAST reference for large values (UUID v7, zero if inline)
        float frequency;          // Fraction of rows with this value (0.0-1.0)

        MCVEntry() : value_oid(), frequency(0.0f)
        {
            std::memset(value_data, 0, sizeof(value_data));
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
        uint8_t lower_bound[256];  // Lower bound value (serialized)
        uint8_t upper_bound[256];  // Upper bound value (serialized)
        ID lower_oid;              // TOAST reference for large lower bound (UUID v7, zero if inline)
        ID upper_oid;              // TOAST reference for large upper bound (UUID v7, zero if inline)
        uint64_t row_count;        // Number of rows in this bucket (equal-width)
        float frequency;           // Fraction of rows in this bucket (equal-height)

        HistogramBucket() : lower_oid(), upper_oid(), row_count(0), frequency(0.0f)
        {
            std::memset(lower_bound, 0, sizeof(lower_bound));
            std::memset(upper_bound, 0, sizeof(upper_bound));
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

        // Most Common Values (MCV)
        std::vector<MCVEntry> mcv_list;  // Most common values (up to 100)

        // Histogram
        HistogramType histogram_type;
        uint32_t histogram_bucket_count;  // Number of histogram buckets
        std::vector<HistogramBucket> histogram_buckets;

        // Metadata
        uint64_t last_analyzed_time;  // Timestamp of last ANALYZE
        uint64_t sample_size;         // Number of rows sampled for statistics
        float sample_rate;            // Fraction of table sampled

        ColumnStatistics()
            : data_type(core::DataType::UNKNOWN),
              num_rows(0),
              num_nulls(0),
              null_fraction(0.0f),
              num_distinct(0),
              avg_width(0.0f),
              histogram_type(HistogramType::NONE),
              histogram_bucket_count(0),
              last_analyzed_time(0),
              sample_size(0),
              sample_rate(0.0f)
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
        uint64_t last_analyzed_time; // Timestamp of last ANALYZE

        TableStatistics()
            : num_rows(0),
              num_pages(0),
              avg_row_size(0.0f),
              last_analyzed_time(0)
        {
        }
    };

} // namespace scratchbird::optimizer
