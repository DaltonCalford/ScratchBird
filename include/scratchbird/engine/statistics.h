#ifndef SCRATCHBIRD_ENGINE_STATISTICS_H
#define SCRATCHBIRD_ENGINE_STATISTICS_H

#include "scratchbird/engine/heap.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    // Column statistics for cost-based optimization
    struct ColumnStatistics {
        // Basic statistics
        std::uint64_t n_distinct{0}; // Number of distinct values
        std::uint64_t n_null{0};     // Number of NULL values
        std::uint64_t n_total{0};    // Total number of rows
        std::string min_value;       // Minimum value (as string)
        std::string max_value;       // Maximum value (as string)
        double avg_width{0.0};       // Average width in bytes

        // Most Common Values (MCVs)
        struct MCV {
            std::string value;
            std::uint64_t frequency;
            double fraction;
        };
        std::vector<MCV> most_common_values;

        // Histogram for range queries
        struct HistogramBucket {
            std::string lower_bound;
            std::string upper_bound;
            std::uint64_t frequency;
            double fraction;
        };
        std::vector<HistogramBucket> histogram;

        // Metadata
        std::time_t last_analyzed{0};
        std::uint64_t sample_size{0};

        // Serialization
        std::string to_json() const;
        static ColumnStatistics from_json(const std::string& json);

        // Estimation methods
        double estimate_selectivity(const std::string& op, const std::string& value) const;
        double estimate_distinct_rows() const;
        double estimate_null_fraction() const;
    };

    // Table statistics
    struct TableStatistics {
        std::uint64_t n_rows{0};
        std::uint64_t n_pages{0};
        double avg_row_size{0.0};
        std::time_t last_analyzed{0};

        std::unordered_map<std::string, ColumnStatistics> column_stats;

        // Serialization
        std::string to_json() const;
        static TableStatistics from_json(const std::string& json);
    };

    // Statistics collection and management
    class StatisticsCollector
    {
      public:
        StatisticsCollector(const std::string& db_path);

        // Main ANALYZE operations
        bool analyze_table(const std::string& schema, const std::string& table);
        bool analyze_column(const std::string& schema, const std::string& table,
                            const std::string& column);

        // Statistics retrieval
        TableStatistics get_table_statistics(const std::string& schema, const std::string& table);
        ColumnStatistics get_column_statistics(const std::string& schema, const std::string& table,
                                               const std::string& column);

        // Statistics storage/persistence
        bool store_table_statistics(const std::string& schema, const std::string& table,
                                    const TableStatistics& stats);
        bool store_column_statistics(const std::string& schema, const std::string& table,
                                     const std::string& column, const ColumnStatistics& stats);

        // Utility methods
        bool has_statistics(const std::string& schema, const std::string& table);
        std::vector<std::string> get_analyzed_tables(const std::string& schema = "");

        // Internal collection methods (public for testing)
        ColumnStatistics collect_column_statistics(const std::vector<Value>& values);
        std::vector<ColumnStatistics::MCV>
        compute_most_common_values(const std::vector<Value>& values, std::size_t max_mcvs = 10);
        std::vector<ColumnStatistics::HistogramBucket>
        compute_histogram(const std::vector<Value>& values, std::size_t num_buckets = 20);

      private:
        std::string db_path_;

        // Sampling for large tables
        std::vector<std::vector<Value>> sample_table_data(const std::string& schema,
                                                          const std::string& table,
                                                          std::size_t max_samples = 10000);

        // Statistics key generation for catalog storage
        std::string make_stats_key(const std::string& schema, const std::string& table,
                                   const std::string& column = "");
    };

    // Cost estimation using statistics
    class CostEstimator
    {
      public:
        CostEstimator(const std::string& db_path);

        // Selectivity estimation for WHERE clauses
        double estimate_predicate_selectivity(const std::string& schema, const std::string& table,
                                              const std::string& predicate);

        // Cardinality estimation
        std::uint64_t estimate_table_cardinality(const std::string& schema,
                                                 const std::string& table);
        std::uint64_t estimate_join_cardinality(const std::string& left_schema,
                                                const std::string& left_table,
                                                const std::string& right_schema,
                                                const std::string& right_table,
                                                const std::string& join_condition);

        // Cost estimation for operations
        double estimate_seq_scan_cost(const std::string& schema, const std::string& table);
        double estimate_index_scan_cost(const std::string& schema, const std::string& table,
                                        const std::string& index_name);
        double estimate_hash_join_cost(std::uint64_t left_cardinality,
                                       std::uint64_t right_cardinality);
        double estimate_nested_loop_cost(std::uint64_t left_cardinality,
                                         std::uint64_t right_cardinality);

      private:
        std::string db_path_;
        StatisticsCollector stats_collector_;

        // Helper methods for cost calculation
        double parse_and_estimate_predicate(const std::string& predicate,
                                            const TableStatistics& table_stats);
        double estimate_comparison_selectivity(const std::string& column, const std::string& op,
                                               const std::string& value,
                                               const ColumnStatistics& col_stats);
    };

    // High-level interface functions
    bool execute_analyze_command(const std::string& sql);
    TableStatistics get_table_stats(const std::string& db_path, const std::string& schema,
                                    const std::string& table);
    ColumnStatistics get_column_stats(const std::string& db_path, const std::string& schema,
                                      const std::string& table, const std::string& column);

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_STATISTICS_H
