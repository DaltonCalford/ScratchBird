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
#include <array>
#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>
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

    enum class ExpressionStatsFunction : uint32_t
    {
        UNKNOWN = 0,
        LOWER = 1,
        UPPER = 2,
        TRIM = 3,
        LTRIM = 4,
        RTRIM = 5
    };

    inline constexpr const char *kExpressionStatsContractId =
        "sb_expression_stats/v1";
    inline constexpr const char *kExpressionStatsRegistryContractId =
        "sb_expression_stats_registry/v1";
    inline constexpr const char *kExpressionStatsCoverageContractId =
        "sb_expression_stats_coverage/v1";

    enum class IndexFamilyMetricsType : uint32_t
    {
        UNKNOWN = 0,
        ORDERED_EXACT = 1,
        SUMMARY_CANDIDATE = 2,
        GENERALIZED_SPATIAL = 3,
        TEXT_SEARCH = 4,
        ANN = 5
    };

    enum class IndexMetricsConfidenceClass : uint32_t
    {
        UNKNOWN = 0,
        LOW = 1,
        MEDIUM = 2,
        HIGH = 3,
        INVALID = 4
    };

    enum class IndexMetricsQueryabilityState : uint32_t
    {
        UNKNOWN = 0,
        QUERYABLE = 1,
        LIMITED = 2,
        INVALID = 3
    };

    enum class IndexMetricsFreshnessClass : uint32_t
    {
        UNKNOWN = 0,
        CURRENT = 1,
        AGED = 2,
        STALE_DEGRADED = 3,
        UNUSABLE = 4
    };

    enum class IndexMetricsInvalidationState : uint32_t
    {
        UNKNOWN = 0,
        VALID = 1,
        INVALIDATED_SOFT = 2,
        INVALIDATED_HARD = 3
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

    struct ExpressionStatsDescriptor
    {
        std::string contract_id = kExpressionStatsContractId;
        std::string registry_contract_id =
            kExpressionStatsRegistryContractId;
        std::string coverage_contract_id =
            kExpressionStatsCoverageContractId;
        ExpressionStatsFunction function = ExpressionStatsFunction::UNKNOWN;
        std::string function_name;
        std::string column_name;
        std::string coverage_class;
        core::DataType input_data_type = core::DataType::UNKNOWN;
        core::DataType result_data_type = core::DataType::UNKNOWN;
    };

    struct ExpressionStatistics
    {
        ID table_id;
        std::string expression_key;
        std::string expression_contract_id = kExpressionStatsContractId;
        std::string registry_contract_id =
            kExpressionStatsRegistryContractId;
        std::string coverage_contract_id =
            kExpressionStatsCoverageContractId;
        std::string function_name;
        std::string base_column_name;
        std::string coverage_class;
        core::DataType input_data_type = core::DataType::UNKNOWN;
        core::DataType result_data_type = core::DataType::UNKNOWN;
        ColumnStatistics stats;
    };

    struct ExpressionStatsRegistryEntry
    {
        ExpressionStatsFunction function = ExpressionStatsFunction::UNKNOWN;
        const char *canonical_name = "UNKNOWN";
        const char *alternate_name = "";
        const char *coverage_class = "UNSUPPORTED";
    };

    struct MultivariateMCVEntry
    {
        std::vector<std::vector<uint8_t>> values;
        float frequency = 0.0f;
    };

    struct MultiColumnNDistinctStatistics
    {
        std::vector<ID> column_ids;
        std::vector<std::string> column_names;
        uint64_t num_distinct = 0;
        double distinct_fraction = 0.0;
        uint64_t sample_size = 0;
        uint64_t last_analyzed_time = 0;
    };

    struct FunctionalDependencyStatistics
    {
        std::vector<ID> determinant_column_ids;
        std::vector<ID> dependent_column_ids;
        std::vector<std::string> determinant_column_names;
        std::vector<std::string> dependent_column_names;
        double strength = 0.0;
        uint64_t sample_size = 0;
        uint64_t last_analyzed_time = 0;
    };

    struct MultivariateStatistics
    {
        ID table_id;
        std::vector<ID> column_ids;
        std::vector<std::string> column_names;
        uint64_t sample_size = 0;
        uint64_t last_analyzed_time = 0;
        MultiColumnNDistinctStatistics ndistinct;
        std::vector<MultivariateMCVEntry> mcv_list;
        std::vector<FunctionalDependencyStatistics> dependencies;
        std::vector<ColumnCorrelationStatistics> pairwise_correlations;
    };

    struct IndexFamilyMetricsPacket
    {
        ID index_id;
        std::string physical_family;
        std::string planner_family;
        IndexMetricsQueryabilityState queryability_state =
            IndexMetricsQueryabilityState::UNKNOWN;
        uint64_t metrics_publication_epoch = 0;
        uint64_t metrics_last_refresh_xid = 0;
        IndexMetricsConfidenceClass metrics_confidence_class =
            IndexMetricsConfidenceClass::UNKNOWN;
        IndexMetricsFreshnessClass metrics_freshness_class =
            IndexMetricsFreshnessClass::UNKNOWN;
        IndexMetricsInvalidationState metrics_invalidation_state =
            IndexMetricsInvalidationState::UNKNOWN;
        std::string metrics_invalidation_reason;
        uint64_t leaf_pages = 0;
        uint16_t height = 0;
        uint64_t row_count_est = 0;
        uint64_t live_entry_count_est = 0;
        double dead_fraction = 0.0;
        double bloat_ratio = 0.0;
        double recheck_ratio_est = 0.0;
        double correlation = 0.0;
        double coverage_fraction = 0.0;
        uint64_t maintenance_backlog_ops = 0;
        uint64_t publish_lag_xids = 0;
        uint64_t reclaim_lag_xids = 0;
        uint32_t family_metrics_version = 0;
        IndexFamilyMetricsType family_metrics_type =
            IndexFamilyMetricsType::UNKNOWN;
        std::string family_metrics_payload;
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

    inline auto expressionStatsAsciiUpper(std::string text) -> std::string
    {
        std::transform(text.begin(),
                       text.end(),
                       text.begin(),
                       [](unsigned char ch) {
                           return static_cast<char>(std::toupper(ch));
                       });
        return text;
    }

    inline auto expressionStatsRegistryEntries()
        -> const std::array<ExpressionStatsRegistryEntry, 5> &
    {
        static constexpr std::array<ExpressionStatsRegistryEntry, 5>
            kExpressionStatsRegistry{{
                {ExpressionStatsFunction::LOWER,
                 "LOWER",
                 "LCASE",
                 "UNARY_TEXT_EQ_NORMALIZED"},
                {ExpressionStatsFunction::UPPER,
                 "UPPER",
                 "UCASE",
                 "UNARY_TEXT_EQ_NORMALIZED"},
                {ExpressionStatsFunction::TRIM,
                 "TRIM",
                 "BTRIM",
                 "UNARY_TEXT_EQ_NORMALIZED"},
                {ExpressionStatsFunction::LTRIM,
                 "LTRIM",
                 "",
                 "UNARY_TEXT_EQ_NORMALIZED"},
                {ExpressionStatsFunction::RTRIM,
                 "RTRIM",
                 "",
                 "UNARY_TEXT_EQ_NORMALIZED"},
            }};
        return kExpressionStatsRegistry;
    }

    inline auto findExpressionStatsRegistryEntry(ExpressionStatsFunction value)
        -> const ExpressionStatsRegistryEntry *
    {
        for (const auto &entry : expressionStatsRegistryEntries())
        {
            if (entry.function == value)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    inline auto findExpressionStatsRegistryEntry(std::string_view name)
        -> const ExpressionStatsRegistryEntry *
    {
        const std::string normalized =
            expressionStatsAsciiUpper(std::string(name));
        for (const auto &entry : expressionStatsRegistryEntries())
        {
            if (normalized == entry.canonical_name ||
                (!std::string_view(entry.alternate_name).empty() &&
                 normalized == entry.alternate_name))
            {
                return &entry;
            }
        }
        return nullptr;
    }

    inline auto expressionStatsFunctionName(ExpressionStatsFunction value)
        -> const char *
    {
        const auto *entry = findExpressionStatsRegistryEntry(value);
        return entry != nullptr ? entry->canonical_name : "UNKNOWN";
    }

    inline auto expressionStatsCoverageClassName(
        ExpressionStatsFunction value) -> const char *
    {
        const auto *entry = findExpressionStatsRegistryEntry(value);
        return entry != nullptr ? entry->coverage_class : "UNSUPPORTED";
    }

    inline auto expressionStatsFunctionFromName(std::string_view name)
        -> ExpressionStatsFunction
    {
        const auto *entry = findExpressionStatsRegistryEntry(name);
        return entry != nullptr ? entry->function
                                : ExpressionStatsFunction::UNKNOWN;
    }

    inline auto applyExpressionStatsTextTransform(std::string &text,
                                                  ExpressionStatsFunction function)
        -> void
    {
        switch (function)
        {
            case ExpressionStatsFunction::LOWER:
                std::transform(text.begin(),
                               text.end(),
                               text.begin(),
                               [](unsigned char ch) {
                                   return static_cast<char>(std::tolower(ch));
                               });
                return;
            case ExpressionStatsFunction::UPPER:
                std::transform(text.begin(),
                               text.end(),
                               text.begin(),
                               [](unsigned char ch) {
                                   return static_cast<char>(std::toupper(ch));
                               });
                return;
            case ExpressionStatsFunction::TRIM:
            {
                auto is_space = [](unsigned char ch) {
                    return std::isspace(ch) != 0;
                };
                const auto begin =
                    std::find_if_not(text.begin(), text.end(), is_space);
                const auto end =
                    std::find_if_not(text.rbegin(), text.rend(), is_space)
                        .base();
                text = begin < end ? std::string(begin, end) : std::string();
                return;
            }
            case ExpressionStatsFunction::LTRIM:
            {
                auto is_space = [](unsigned char ch) {
                    return std::isspace(ch) != 0;
                };
                const auto begin =
                    std::find_if_not(text.begin(), text.end(), is_space);
                text = std::string(begin, text.end());
                return;
            }
            case ExpressionStatsFunction::RTRIM:
            {
                auto is_space = [](unsigned char ch) {
                    return std::isspace(ch) != 0;
                };
                const auto end =
                    std::find_if_not(text.rbegin(), text.rend(), is_space)
                        .base();
                text = std::string(text.begin(), end);
                return;
            }
            case ExpressionStatsFunction::UNKNOWN:
            default:
                return;
        }
    }

    inline auto isTextLikeExpressionStatsInput(core::DataType type) -> bool
    {
        switch (type)
        {
            case core::DataType::CHAR:
            case core::DataType::VARCHAR:
            case core::DataType::TEXT:
            case core::DataType::JSON:
            case core::DataType::JSONB:
            case core::DataType::XML:
            case core::DataType::BLOB_SUB_TYPE_TEXT:
                return true;
            case core::DataType::UNKNOWN:
                return true;
            default:
                return false;
        }
    }

    inline auto expressionStatsResultType(ExpressionStatsFunction function,
                                          core::DataType input_type)
        -> core::DataType
    {
        switch (function)
        {
            case ExpressionStatsFunction::LOWER:
            case ExpressionStatsFunction::UPPER:
            case ExpressionStatsFunction::TRIM:
            case ExpressionStatsFunction::LTRIM:
            case ExpressionStatsFunction::RTRIM:
                return input_type == core::DataType::UNKNOWN
                           ? core::DataType::VARCHAR
                           : input_type;
            case ExpressionStatsFunction::UNKNOWN:
            default:
                return core::DataType::UNKNOWN;
        }
    }

    inline auto buildExpressionStatsDescriptor(std::string_view function_name,
                                               std::string_view column_name,
                                               core::DataType input_type =
                                                   core::DataType::UNKNOWN)
        -> std::optional<ExpressionStatsDescriptor>
    {
        const ExpressionStatsFunction function =
            expressionStatsFunctionFromName(function_name);
        if (function == ExpressionStatsFunction::UNKNOWN ||
            !isTextLikeExpressionStatsInput(input_type))
        {
            return std::nullopt;
        }

        ExpressionStatsDescriptor descriptor;
        descriptor.function = function;
        descriptor.function_name = expressionStatsFunctionName(function);
        descriptor.column_name = expressionStatsAsciiUpper(std::string(column_name));
        descriptor.coverage_class =
            expressionStatsCoverageClassName(function);
        descriptor.input_data_type = input_type;
        descriptor.result_data_type =
            expressionStatsResultType(function, input_type);
        return descriptor;
    }

    inline auto canonicalExpressionStatsKey(
        const ExpressionStatsDescriptor &descriptor) -> std::string
    {
        return std::string("EXPRv1|") + descriptor.function_name + "|" +
               expressionStatsAsciiUpper(descriptor.column_name);
    }

    inline auto legacyExpressionStatsKey(
        const ExpressionStatsDescriptor &descriptor) -> std::string
    {
        return descriptor.function_name + "(" +
               expressionStatsAsciiUpper(descriptor.column_name) + ")";
    }

    inline auto parseExpressionStatsKey(std::string_view key,
                                        ExpressionStatsDescriptor &descriptor_out)
        -> bool
    {
        const std::string normalized =
            expressionStatsAsciiUpper(std::string(key));
        if (normalized.rfind("EXPRV1|", 0) == 0)
        {
            const size_t first_sep = normalized.find('|', 7);
            if (first_sep == std::string::npos || first_sep + 1 >= normalized.size())
            {
                return false;
            }
            auto descriptor = buildExpressionStatsDescriptor(
                normalized.substr(7, first_sep - 7),
                normalized.substr(first_sep + 1));
            if (!descriptor.has_value())
            {
                return false;
            }
            descriptor_out = std::move(*descriptor);
            return true;
        }

        const size_t open_paren = normalized.find('(');
        const size_t close_paren = normalized.rfind(')');
        if (open_paren == std::string::npos || close_paren == std::string::npos ||
            close_paren <= open_paren + 1)
        {
            return false;
        }

        auto descriptor = buildExpressionStatsDescriptor(
            normalized.substr(0, open_paren),
            normalized.substr(open_paren + 1, close_paren - open_paren - 1));
        if (!descriptor.has_value())
        {
            return false;
        }
        descriptor_out = std::move(*descriptor);
        return true;
    }

    inline auto indexFamilyMetricsTypeName(IndexFamilyMetricsType value)
        -> const char *
    {
        switch (value)
        {
            case IndexFamilyMetricsType::ORDERED_EXACT:
                return "ORDERED_EXACT";
            case IndexFamilyMetricsType::SUMMARY_CANDIDATE:
                return "SUMMARY_CANDIDATE";
            case IndexFamilyMetricsType::GENERALIZED_SPATIAL:
                return "GENERALIZED_SPATIAL";
            case IndexFamilyMetricsType::TEXT_SEARCH:
                return "TEXT_SEARCH";
            case IndexFamilyMetricsType::ANN:
                return "ANN";
            case IndexFamilyMetricsType::UNKNOWN:
            default:
                return "UNKNOWN";
        }
    }

    inline auto indexMetricsConfidenceClassName(IndexMetricsConfidenceClass value)
        -> const char *
    {
        switch (value)
        {
            case IndexMetricsConfidenceClass::LOW:
                return "LOW";
            case IndexMetricsConfidenceClass::MEDIUM:
                return "MEDIUM";
            case IndexMetricsConfidenceClass::HIGH:
                return "HIGH";
            case IndexMetricsConfidenceClass::INVALID:
                return "INVALID";
            case IndexMetricsConfidenceClass::UNKNOWN:
            default:
                return "UNKNOWN";
        }
    }

    inline auto indexMetricsQueryabilityStateName(
        IndexMetricsQueryabilityState value) -> const char *
    {
        switch (value)
        {
            case IndexMetricsQueryabilityState::QUERYABLE:
                return "QUERYABLE";
            case IndexMetricsQueryabilityState::LIMITED:
                return "LIMITED";
            case IndexMetricsQueryabilityState::INVALID:
                return "INVALID";
            case IndexMetricsQueryabilityState::UNKNOWN:
            default:
                return "UNKNOWN";
        }
    }

    inline auto indexMetricsFreshnessClassName(
        IndexMetricsFreshnessClass value) -> const char *
    {
        switch (value)
        {
            case IndexMetricsFreshnessClass::CURRENT:
                return "CURRENT";
            case IndexMetricsFreshnessClass::AGED:
                return "AGED";
            case IndexMetricsFreshnessClass::STALE_DEGRADED:
                return "STALE_DEGRADED";
            case IndexMetricsFreshnessClass::UNUSABLE:
                return "UNUSABLE";
            case IndexMetricsFreshnessClass::UNKNOWN:
            default:
                return "UNKNOWN";
        }
    }

    inline auto indexMetricsInvalidationStateName(
        IndexMetricsInvalidationState value) -> const char *
    {
        switch (value)
        {
            case IndexMetricsInvalidationState::VALID:
                return "VALID";
            case IndexMetricsInvalidationState::INVALIDATED_SOFT:
                return "INVALIDATED_SOFT";
            case IndexMetricsInvalidationState::INVALIDATED_HARD:
                return "INVALIDATED_HARD";
            case IndexMetricsInvalidationState::UNKNOWN:
            default:
                return "UNKNOWN";
        }
    }

    inline auto indexMetricsFreshnessClassFromName(std::string_view value)
        -> IndexMetricsFreshnessClass
    {
        if (value == "CURRENT")
        {
            return IndexMetricsFreshnessClass::CURRENT;
        }
        if (value == "AGED" || value == "STALE_ACCEPTABLE")
        {
            return IndexMetricsFreshnessClass::AGED;
        }
        if (value == "STALE_DEGRADED")
        {
            return IndexMetricsFreshnessClass::STALE_DEGRADED;
        }
        if (value == "UNUSABLE")
        {
            return IndexMetricsFreshnessClass::UNUSABLE;
        }
        return IndexMetricsFreshnessClass::UNKNOWN;
    }

    inline auto indexMetricsInvalidationStateFromName(std::string_view value)
        -> IndexMetricsInvalidationState
    {
        if (value == "VALID")
        {
            return IndexMetricsInvalidationState::VALID;
        }
        if (value == "INVALIDATED_SOFT")
        {
            return IndexMetricsInvalidationState::INVALIDATED_SOFT;
        }
        if (value == "INVALIDATED_HARD")
        {
            return IndexMetricsInvalidationState::INVALIDATED_HARD;
        }
        return IndexMetricsInvalidationState::UNKNOWN;
    }

} // namespace scratchbird::optimizer
