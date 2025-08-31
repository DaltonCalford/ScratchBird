// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <scratchbird/engine/parallel_executor.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    /// Aggregation function types
    enum class AggregateFunction : std::uint8_t {
        COUNT = 0,
        SUM = 1,
        AVG = 2,
        MIN = 3,
        MAX = 4,
        STDDEV = 5,
        VARIANCE = 6,
        COUNT_DISTINCT = 7
    };

    /// Sort direction
    enum class SortDirection : std::uint8_t { ASCENDING = 0, DESCENDING = 1 };

    /// Data types for aggregation and sorting
    enum class DataType : std::uint8_t {
        INTEGER = 0,
        BIGINT = 1,
        DOUBLE = 2,
        VARCHAR = 3,
        TIMESTAMP = 4,
        DECIMAL = 5
    };

    /// Generic value holder for aggregation and sorting
    struct Value {
        DataType type;
        union {
            std::int32_t int_val;
            std::int64_t bigint_val;
            double double_val;
            std::int64_t timestamp_val;
        };
        std::string string_val;

        Value() : type(DataType::INTEGER), int_val(0) {}
        explicit Value(std::int32_t val) : type(DataType::INTEGER), int_val(val) {}
        explicit Value(std::int64_t val) : type(DataType::BIGINT), bigint_val(val) {}
        explicit Value(double val) : type(DataType::DOUBLE), double_val(val) {}
        explicit Value(const std::string& val)
            : type(DataType::VARCHAR), int_val(0), string_val(val)
        {
        }

        bool operator<(const Value& other) const;
        bool operator==(const Value& other) const;
        Value operator+(const Value& other) const;
        Value& operator+=(const Value& other);
    };

    /// Row data structure for parallel processing
    struct Row {
        std::vector<Value> columns;

        Row() = default;
        explicit Row(std::size_t column_count) : columns(column_count) {}

        Value& operator[](std::size_t index)
        {
            return columns[index];
        }
        const Value& operator[](std::size_t index) const
        {
            return columns[index];
        }

        std::size_t size() const
        {
            return columns.size();
        }
    };

    /// Aggregate column definition
    struct AggregateColumn {
        std::size_t source_column;  ///< Source column index
        AggregateFunction function; ///< Aggregation function
        DataType result_type;       ///< Result data type
        std::string alias;          ///< Output column alias

        AggregateColumn(std::size_t col, AggregateFunction func, DataType type,
                        const std::string& name = "")
            : source_column(col), function(func), result_type(type), alias(name)
        {
        }
    };

    /// Sort column definition
    struct SortColumn {
        std::size_t column_index; ///< Column to sort by
        SortDirection direction;  ///< Sort direction

        SortColumn(std::size_t col, SortDirection dir = SortDirection::ASCENDING)
            : column_index(col), direction(dir)
        {
        }
    };

    /// Partial aggregate state for a group
    struct PartialAggregate {
        std::vector<Value> group_keys;
        std::vector<Value> aggregate_values;
        std::vector<std::uint64_t> counts; ///< For COUNT and AVG functions

        PartialAggregate() = default;
        explicit PartialAggregate(std::size_t key_count, std::size_t agg_count);

        void combine(const PartialAggregate& other);
        Row finalize(const std::vector<AggregateColumn>& agg_columns) const;
    };

    /// Hash function for group keys
    struct GroupKeyHash {
        std::size_t operator()(const std::vector<Value>& keys) const;
    };

    /// Equality comparison for group keys
    struct GroupKeyEqual {
        bool operator()(const std::vector<Value>& lhs, const std::vector<Value>& rhs) const;
    };

    /// Parallel aggregation engine
    class ParallelAggregator
    {
      public:
        /// Configuration for parallel aggregation
        struct Config {
            std::size_t max_workers{4};        ///< Maximum number of worker threads
            std::size_t rows_per_batch{10000}; ///< Rows processed per batch
            std::size_t max_memory_per_worker{64 * 1024 * 1024}; ///< 64MB per worker
            bool enable_preaggregation{true}; ///< Enable worker-level preaggregation
            double memory_limit_factor{0.8};  ///< Use 80% of available memory
        };

        ParallelAggregator();
        explicit ParallelAggregator(const Config& config);
        ~ParallelAggregator();

        /// Non-copyable, non-moveable
        ParallelAggregator(const ParallelAggregator&) = delete;
        ParallelAggregator& operator=(const ParallelAggregator&) = delete;
        ParallelAggregator(ParallelAggregator&&) = delete;
        ParallelAggregator& operator=(ParallelAggregator&&) = delete;

        /// Execute parallel aggregation
        std::vector<Row> aggregate(const std::vector<Row>& input_rows,
                                   const std::vector<std::size_t>& group_columns,
                                   const std::vector<AggregateColumn>& agg_columns);

        /// Get aggregation statistics
        struct Statistics {
            std::size_t total_input_rows{0};
            std::size_t total_output_rows{0};
            std::size_t groups_processed{0};
            std::chrono::nanoseconds total_time{0};
            std::chrono::nanoseconds worker_time{0};
            std::chrono::nanoseconds merge_time{0};
            std::size_t memory_usage{0};
            double efficiency_ratio{0.0};
        };

        Statistics get_statistics() const;
        void reset_statistics();

      private:
        Config config_;
        mutable std::mutex stats_mutex_;
        Statistics stats_;

        /// Worker thread data
        struct WorkerData {
            std::thread thread;
            std::atomic<bool> active{false};
            std::vector<Row> input_batch;
            std::unordered_map<std::vector<Value>, PartialAggregate, GroupKeyHash, GroupKeyEqual>
                partial_results;
            std::condition_variable work_available;
            std::mutex work_mutex;
        };

        std::vector<std::unique_ptr<WorkerData>> workers_;
        std::atomic<bool> shutdown_{false};

        void initialize_workers();
        void shutdown_workers();

        void worker_thread(std::size_t worker_id);

        void process_batch(WorkerData& worker, const std::vector<std::size_t>& group_columns,
                           const std::vector<AggregateColumn>& agg_columns);

        std::vector<Row> merge_partial_results(const std::vector<AggregateColumn>& agg_columns);
    };

    /// Parallel sorting engine
    class ParallelSorter
    {
      public:
        /// Configuration for parallel sorting
        struct Config {
            std::size_t max_workers{4};        ///< Maximum number of worker threads
            std::size_t rows_per_chunk{50000}; ///< Rows per sorting chunk
            std::size_t max_memory_per_worker{128 * 1024 * 1024}; ///< 128MB per worker
            bool enable_external_sort{true};    ///< Enable disk-based external sorting
            std::size_t merge_fan_out{16};      ///< K-way merge fan-out
            std::string temp_directory{"/tmp"}; ///< Directory for temporary files
        };

        ParallelSorter();
        explicit ParallelSorter(const Config& config);
        ~ParallelSorter();

        /// Non-copyable, non-moveable
        ParallelSorter(const ParallelSorter&) = delete;
        ParallelSorter& operator=(const ParallelSorter&) = delete;
        ParallelSorter(ParallelSorter&&) = delete;
        ParallelSorter& operator=(ParallelSorter&&) = delete;

        /// Execute parallel sorting
        std::vector<Row> sort(const std::vector<Row>& input_rows,
                              const std::vector<SortColumn>& sort_columns);

        /// Get sorting statistics
        struct Statistics {
            std::size_t total_rows{0};
            std::size_t chunks_created{0};
            std::size_t merge_passes{0};
            std::chrono::nanoseconds total_time{0};
            std::chrono::nanoseconds sort_time{0};
            std::chrono::nanoseconds merge_time{0};
            std::size_t memory_usage{0};
            std::size_t temp_files_created{0};
            std::size_t temp_bytes_written{0};
            double parallelization_efficiency{0.0};
        };

        Statistics get_statistics() const;
        void reset_statistics();

      private:
        Config config_;
        mutable std::mutex stats_mutex_;
        Statistics stats_;

        /// Sorted chunk metadata
        struct SortedChunk {
            std::vector<Row> rows;      ///< In-memory rows (if small enough)
            std::string temp_file_path; ///< Path to temporary file (if external)
            bool is_external{false};    ///< Whether chunk is stored externally
            std::size_t row_count{0};   ///< Number of rows in chunk

            SortedChunk() = default;
            SortedChunk(std::vector<Row>&& chunk_rows);
            SortedChunk(const std::string& file_path, std::size_t rows);
        };

        /// Worker thread data
        struct WorkerData {
            std::thread thread;
            std::atomic<bool> active{false};
            std::vector<Row> input_chunk;
            std::unique_ptr<SortedChunk> result_chunk;
            std::condition_variable work_available;
            std::mutex work_mutex;
        };

        std::vector<std::unique_ptr<WorkerData>> workers_;
        std::atomic<bool> shutdown_{false};

        void initialize_workers();
        void shutdown_workers();

        void worker_thread(std::size_t worker_id);

        void sort_chunk(WorkerData& worker, const std::vector<SortColumn>& sort_columns);

        std::vector<Row> merge_sorted_chunks(std::vector<std::unique_ptr<SortedChunk>>& chunks,
                                             const std::vector<SortColumn>& sort_columns);

        void write_chunk_to_file(const std::vector<Row>& rows, const std::string& file_path);
        std::vector<Row> read_chunk_from_file(const std::string& file_path);

        /// Multi-way merge iterator for external sorting
        class MergeIterator
        {
          public:
            MergeIterator(std::vector<std::unique_ptr<SortedChunk>>& chunks,
                          const std::vector<SortColumn>& sort_columns);

            bool has_next() const;
            Row next();

          private:
            struct ChunkIterator {
                std::size_t chunk_index;
                std::size_t row_index;
                std::vector<Row> current_buffer;
                bool is_external;
                std::string file_path;

                ChunkIterator(std::size_t chunk_idx, bool external, const std::string& path = "");
                bool load_next_buffer();
            };

            std::vector<ChunkIterator> iterators_;
            const std::vector<SortColumn>& sort_columns_;

            struct IteratorComparator {
                const std::vector<SortColumn>& sort_columns;

                explicit IteratorComparator(const std::vector<SortColumn>& cols)
                    : sort_columns(cols)
                {
                }
                bool operator()(const ChunkIterator& lhs, const ChunkIterator& rhs) const;
            };

            std::priority_queue<ChunkIterator, std::vector<ChunkIterator>, IteratorComparator>
                heap_;
        };

        /// Row comparator for sorting
        struct RowComparator {
            const std::vector<SortColumn>& sort_columns;

            explicit RowComparator(const std::vector<SortColumn>& cols) : sort_columns(cols) {}
            bool operator()(const Row& lhs, const Row& rhs) const;
        };
    };

    /// Combined parallel aggregation and sorting pipeline
    class ParallelAggregationSortingPipeline
    {
      public:
        /// Pipeline configuration
        struct Config {
            ParallelAggregator::Config aggregation_config;
            ParallelSorter::Config sorting_config;
            bool enable_pipelined_execution{true};    ///< Pipeline aggregation and sorting
            std::size_t pipeline_buffer_size{100000}; ///< Buffer size between stages
        };

        ParallelAggregationSortingPipeline();
        explicit ParallelAggregationSortingPipeline(const Config& config);
        ~ParallelAggregationSortingPipeline();

        /// Execute aggregation followed by sorting
        std::vector<Row> aggregate_and_sort(const std::vector<Row>& input_rows,
                                            const std::vector<std::size_t>& group_columns,
                                            const std::vector<AggregateColumn>& agg_columns,
                                            const std::vector<SortColumn>& sort_columns);

        /// Get combined statistics
        struct Statistics {
            ParallelAggregator::Statistics aggregation_stats;
            ParallelSorter::Statistics sorting_stats;
            std::chrono::nanoseconds total_pipeline_time{0};
            std::size_t intermediate_rows{0};
            double pipeline_efficiency{0.0};
        };

        Statistics get_statistics() const;
        void reset_statistics();

      private:
        Config config_;
        ParallelAggregator aggregator_;
        ParallelSorter sorter_;
        mutable Statistics stats_;
    };

    /// Utility functions for parallel aggregation and sorting
    namespace parallel_agg_sort_utils
    {
        /// Optimal worker count based on data size and system capabilities
        std::size_t calculate_optimal_workers(std::size_t data_size, std::size_t available_memory);

        /// Memory usage estimation for aggregation
        std::size_t estimate_aggregation_memory(std::size_t input_rows,
                                                std::size_t estimated_groups,
                                                const std::vector<AggregateColumn>& agg_columns);

        /// Memory usage estimation for sorting
        std::size_t estimate_sorting_memory(std::size_t input_rows, std::size_t row_size);

        /// Generate temporary file path
        std::string generate_temp_file_path(const std::string& base_dir, const std::string& prefix);

        /// Clean up temporary files
        void cleanup_temp_files(const std::vector<std::string>& file_paths);

        /// System memory information
        struct SystemMemoryInfo {
            std::size_t total_memory;
            std::size_t available_memory;
            std::size_t cache_line_size;
            std::size_t cpu_count;
        };

        SystemMemoryInfo get_system_memory_info();
    } // namespace parallel_agg_sort_utils

} // namespace scratchbird::engine
