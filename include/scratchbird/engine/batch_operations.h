// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    // Forward declarations
    class Connection;
    class QueryExecutor;
    class PreparedStatement;

    /// Types of batch operations supported
    enum class BatchOperationType : std::uint8_t {
        INSERT = 1, ///< Batch INSERT operations
        UPDATE = 2, ///< Batch UPDATE operations
        DELETE = 3, ///< Batch DELETE operations
        MIXED = 4   ///< Mixed operation types (advanced batching)
    };

    /// Batch execution modes
    enum class BatchExecutionMode : std::uint8_t {
        IMMEDIATE = 1, ///< Execute immediately when thresholds are met
        DEFERRED = 2,  ///< Defer execution until explicit flush
        ADAPTIVE = 3   ///< Adaptively choose based on performance
    };

    /// Batch operation configuration
    struct BatchConfig {
        /// Batch size limits
        std::uint32_t max_rows_per_batch{1000};       ///< Maximum rows per batch
        std::uint32_t max_batch_memory_mb{64};        ///< Maximum memory per batch (MB)
        std::chrono::milliseconds batch_timeout{100}; ///< Maximum time to accumulate batch

        /// Performance tuning
        BatchExecutionMode execution_mode{BatchExecutionMode::ADAPTIVE};
        std::uint32_t min_batch_size{10};     ///< Minimum batch size for efficiency
        bool enable_parallel_execution{true}; ///< Enable parallel batch execution

        /// Error handling
        bool continue_on_error{false};       ///< Continue batch on individual statement errors
        std::uint32_t max_retry_attempts{3}; ///< Maximum retry attempts for failed operations

        /// Monitoring and statistics
        bool enable_statistics{true};          ///< Enable batch operation statistics
        std::uint32_t stats_window_size{1000}; ///< Statistics rolling window size

        /// Network batching
        bool enable_network_batching{true};                  ///< Enable network message batching
        std::uint32_t max_network_batch_size{8192};          ///< Maximum network batch size (bytes)
        std::chrono::milliseconds network_batch_timeout{50}; ///< Network batch timeout

        /// Validation
        bool is_valid() const
        {
            return max_rows_per_batch > 0 && max_batch_memory_mb > 0 && batch_timeout.count() > 0 &&
                   min_batch_size <= max_rows_per_batch && max_retry_attempts > 0 &&
                   stats_window_size > 0 && max_network_batch_size > 0 &&
                   network_batch_timeout.count() > 0;
        }
    };

    /// Individual batch operation
    struct BatchOperation {
        /// Operation identification
        std::uint64_t operation_id; ///< Unique operation ID
        BatchOperationType type;    ///< Type of operation
        std::string sql;            ///< SQL statement

        /// Parameters and data
        std::vector<std::vector<std::string>> parameters; ///< Parameter values for each row
        std::vector<std::uint8_t> binary_data; ///< Optional binary data (for BLOBs, etc.)

        /// Metadata
        std::string table_name;                             ///< Target table name
        std::vector<std::string> column_names;              ///< Column names involved
        std::chrono::steady_clock::time_point created_time; ///< When operation was created

        /// Execution state
        bool executed{false};         ///< Has this operation been executed
        std::string error_message;    ///< Error message if execution failed
        std::uint32_t retry_count{0}; ///< Number of retry attempts

        /// Constructor
        BatchOperation(std::uint64_t id, BatchOperationType op_type, const std::string& statement)
            : operation_id(id), type(op_type), sql(statement),
              created_time(std::chrono::steady_clock::now())
        {
        }

        /// Get operation size estimate (for memory management)
        std::size_t get_estimated_size() const
        {
            std::size_t size = sql.size() + table_name.size() + binary_data.size();
            for (const auto& row : parameters) {
                for (const auto& param : row) {
                    size += param.size();
                }
            }
            for (const auto& col : column_names) {
                size += col.size();
            }
            return size;
        }
    };

    /// Batch execution result
    struct BatchExecutionResult {
        /// Overall result
        bool success{false};                              ///< Overall batch success
        std::uint64_t batch_id;                           ///< Batch identifier
        std::chrono::steady_clock::time_point start_time; ///< Execution start time
        std::chrono::steady_clock::time_point end_time;   ///< Execution end time

        /// Operation results
        std::uint32_t total_operations{0};      ///< Total operations in batch
        std::uint32_t successful_operations{0}; ///< Successfully executed operations
        std::uint32_t failed_operations{0};     ///< Failed operations

        /// Performance metrics
        std::uint64_t rows_affected{0};              ///< Total rows affected
        std::chrono::microseconds execution_time{0}; ///< Total execution time
        std::chrono::microseconds network_time{0};   ///< Network communication time

        /// Error information
        std::vector<std::pair<std::uint64_t, std::string>>
            operation_errors; ///< Operation ID -> Error message

        /// Calculate execution statistics
        double get_execution_time_ms() const
        {
            return static_cast<double>(execution_time.count()) / 1000.0;
        }

        double get_throughput_ops_per_sec() const
        {
            if (execution_time.count() == 0)
                return 0.0;
            return (static_cast<double>(successful_operations) * 1000000.0) /
                   execution_time.count();
        }

        double get_success_rate() const
        {
            return total_operations > 0
                       ? static_cast<double>(successful_operations) / total_operations
                       : 0.0;
        }
    };

    /// Batch statistics snapshot (non-atomic for returning)
    struct BatchStatisticsSnapshot {
        /// Operation counts
        std::uint64_t total_batches{0};
        std::uint64_t successful_batches{0};
        std::uint64_t failed_batches{0};
        std::uint64_t total_operations{0};
        std::uint64_t successful_operations{0};
        std::uint64_t failed_operations{0};

        /// Performance metrics
        std::uint64_t total_execution_time_us{0};
        std::uint64_t total_network_time_us{0};
        std::uint64_t total_rows_affected{0};

        /// Batch size statistics
        std::uint32_t max_batch_size{0};
        std::uint32_t min_batch_size{UINT32_MAX};
        double avg_batch_size{0.0};

        /// Memory usage
        std::uint64_t peak_memory_usage_bytes{0};
        std::uint64_t total_memory_allocated{0};

        /// Get average execution time per batch (milliseconds)
        double get_avg_execution_time_ms() const
        {
            return total_batches > 0
                       ? (static_cast<double>(total_execution_time_us) / total_batches) / 1000.0
                       : 0.0;
        }

        /// Get overall throughput (operations per second)
        double get_throughput_ops_per_sec() const
        {
            return total_execution_time_us > 0
                       ? (static_cast<double>(successful_operations) * 1000000.0) /
                             total_execution_time_us
                       : 0.0;
        }

        /// Get success rate
        double get_success_rate() const
        {
            return total_operations > 0
                       ? static_cast<double>(successful_operations) / total_operations
                       : 0.0;
        }
    };

    /// Internal batch statistics with atomic members (for thread-safe updates)
    struct BatchStatistics {
        /// Operation counts
        std::atomic<std::uint64_t> total_batches{0};
        std::atomic<std::uint64_t> successful_batches{0};
        std::atomic<std::uint64_t> failed_batches{0};
        std::atomic<std::uint64_t> total_operations{0};
        std::atomic<std::uint64_t> successful_operations{0};
        std::atomic<std::uint64_t> failed_operations{0};

        /// Performance metrics
        std::atomic<std::uint64_t> total_execution_time_us{0};
        std::atomic<std::uint64_t> total_network_time_us{0};
        std::atomic<std::uint64_t> total_rows_affected{0};

        /// Batch size statistics
        std::atomic<std::uint32_t> max_batch_size{0};
        std::atomic<std::uint32_t> min_batch_size{UINT32_MAX};
        std::atomic<double> avg_batch_size{0.0};

        /// Memory usage
        std::atomic<std::uint64_t> peak_memory_usage_bytes{0};
        std::atomic<std::uint64_t> total_memory_allocated{0};

        void reset()
        {
            total_batches.store(0);
            successful_batches.store(0);
            failed_batches.store(0);
            total_operations.store(0);
            successful_operations.store(0);
            failed_operations.store(0);
            total_execution_time_us.store(0);
            total_network_time_us.store(0);
            total_rows_affected.store(0);
            max_batch_size.store(0);
            min_batch_size.store(UINT32_MAX);
            avg_batch_size.store(0.0);
            peak_memory_usage_bytes.store(0);
            total_memory_allocated.store(0);
        }

        /// Create a snapshot of the statistics
        BatchStatisticsSnapshot snapshot() const
        {
            BatchStatisticsSnapshot result;
            result.total_batches = total_batches.load();
            result.successful_batches = successful_batches.load();
            result.failed_batches = failed_batches.load();
            result.total_operations = total_operations.load();
            result.successful_operations = successful_operations.load();
            result.failed_operations = failed_operations.load();
            result.total_execution_time_us = total_execution_time_us.load();
            result.total_network_time_us = total_network_time_us.load();
            result.total_rows_affected = total_rows_affected.load();
            result.max_batch_size = max_batch_size.load();
            result.min_batch_size = min_batch_size.load();
            result.avg_batch_size = avg_batch_size.load();
            result.peak_memory_usage_bytes = peak_memory_usage_bytes.load();
            result.total_memory_allocated = total_memory_allocated.load();
            return result;
        }
    };

    /// Batch operation callback types
    using BatchCompletionCallback = std::function<void(const BatchExecutionResult&)>;
    using BatchProgressCallback = std::function<void(std::uint64_t completed, std::uint64_t total)>;
    using BatchErrorCallback =
        std::function<void(std::uint64_t operation_id, const std::string& error)>;

    /// Batch container - manages a collection of operations
    class BatchContainer
    {
      public:
        /// Constructor
        explicit BatchContainer(std::uint64_t batch_id, BatchOperationType type,
                                const BatchConfig& config = BatchConfig{});

        /// Destructor
        ~BatchContainer() = default;

        /// Non-copyable, moveable
        BatchContainer(const BatchContainer&) = delete;
        BatchContainer& operator=(const BatchContainer&) = delete;
        BatchContainer(BatchContainer&&) = default;
        BatchContainer& operator=(BatchContainer&&) = default;

        /// Batch management

        /// Add operation to batch
        bool add_operation(std::unique_ptr<BatchOperation> operation);

        /// Check if batch is ready for execution
        bool is_ready_for_execution() const;

        /// Check if batch is full (meets size/memory thresholds)
        bool is_full() const;

        /// Check if batch has timed out
        bool has_timed_out() const;

        /// Get batch size (number of operations)
        std::uint32_t size() const;

        /// Get estimated memory usage
        std::size_t get_memory_usage() const;

        /// Get batch type
        BatchOperationType get_type() const
        {
            return type_;
        }

        /// Get batch ID
        std::uint64_t get_id() const
        {
            return batch_id_;
        }

        /// Access operations
        const std::vector<std::unique_ptr<BatchOperation>>& get_operations() const;

        /// Clear all operations (for reuse)
        void clear();

        /// Get batch age
        std::chrono::milliseconds get_age() const;

      private:
        std::uint64_t batch_id_;
        BatchOperationType type_;
        BatchConfig config_;
        std::vector<std::unique_ptr<BatchOperation>> operations_;
        std::chrono::steady_clock::time_point created_time_;
        mutable std::mutex operations_mutex_;
        std::size_t estimated_memory_usage_{0};
    };

    /// Network message batching for protocol optimization
    class NetworkBatchManager
    {
      public:
        /// Constructor
        explicit NetworkBatchManager(const BatchConfig& config = BatchConfig{});

        /// Destructor
        ~NetworkBatchManager() = default;

        /// Add message to network batch
        bool add_message(const std::vector<std::uint8_t>& message);

        /// Flush network batch (send accumulated messages)
        std::vector<std::uint8_t> flush_batch();

        /// Check if batch should be flushed
        bool should_flush() const;

        /// Get current batch size
        std::size_t get_batch_size() const;

        /// Reset batch
        void reset();

      private:
        BatchConfig config_;
        std::vector<std::uint8_t> batched_messages_;
        std::chrono::steady_clock::time_point last_flush_;
        mutable std::mutex batch_mutex_;
    };

    /// Main batch operations engine
    class BatchOperationsEngine
    {
      public:
        /// Constructor
        explicit BatchOperationsEngine(const BatchConfig& config = BatchConfig{});

        /// Destructor
        ~BatchOperationsEngine();

        /// Non-copyable, non-moveable
        BatchOperationsEngine(const BatchOperationsEngine&) = delete;
        BatchOperationsEngine& operator=(const BatchOperationsEngine&) = delete;
        BatchOperationsEngine(BatchOperationsEngine&&) = delete;
        BatchOperationsEngine& operator=(BatchOperationsEngine&&) = delete;

        /// Lifecycle management

        /// Initialize the batch engine
        bool initialize();

        /// Shutdown the batch engine
        void shutdown();

        /// Check if engine is running
        bool is_running() const;

        /// Configuration management

        /// Update configuration
        bool update_config(const BatchConfig& config);

        /// Get current configuration
        BatchConfig get_config() const;

        /// Batch operation management

        /// Add operation to appropriate batch
        std::uint64_t add_operation(BatchOperationType type, const std::string& sql,
                                    const std::vector<std::vector<std::string>>& parameters,
                                    const std::string& table_name = "");

        /// Execute specific batch
        BatchExecutionResult execute_batch(std::uint64_t batch_id);

        /// Execute all ready batches
        std::vector<BatchExecutionResult> execute_ready_batches();

        /// Flush all batches (force execution)
        std::vector<BatchExecutionResult> flush_all_batches();

        /// Cancel pending batch
        bool cancel_batch(std::uint64_t batch_id);

        /// Callback management

        /// Set batch completion callback
        void set_completion_callback(BatchCompletionCallback callback);

        /// Set batch progress callback
        void set_progress_callback(BatchProgressCallback callback);

        /// Set batch error callback
        void set_error_callback(BatchErrorCallback callback);

        /// Statistics and monitoring

        /// Get batch statistics
        BatchStatisticsSnapshot get_statistics() const;

        /// Reset statistics
        void reset_statistics();

        /// Get active batch count
        std::uint32_t get_active_batch_count() const;

        /// Get pending operation count
        std::uint64_t get_pending_operation_count() const;

        /// Generate performance report
        std::string generate_performance_report() const;

      private:
        /// Configuration
        BatchConfig config_;
        mutable std::mutex config_mutex_;

        /// Batch management
        std::unordered_map<std::uint64_t, std::unique_ptr<BatchContainer>> active_batches_;
        std::unordered_map<BatchOperationType, std::uint64_t>
            current_batch_ids_; ///< Current batch ID per type
        std::atomic<std::uint64_t> next_batch_id_{1};
        std::atomic<std::uint64_t> next_operation_id_{1};
        mutable std::mutex batches_mutex_;

        /// Statistics
        BatchStatistics statistics_;

        /// Engine state
        std::atomic<bool> initialized_{false};
        std::atomic<bool> shutdown_requested_{false};

        /// Callbacks
        BatchCompletionCallback completion_callback_;
        BatchProgressCallback progress_callback_;
        BatchErrorCallback error_callback_;
        mutable std::mutex callbacks_mutex_;

        /// Private methods

        /// Get or create batch for operation type
        BatchContainer* get_or_create_batch(BatchOperationType type);

        /// Execute single batch container
        BatchExecutionResult execute_batch_internal(BatchContainer& batch);

        /// Execute operations within batch
        void execute_operations(const std::vector<std::unique_ptr<BatchOperation>>& operations,
                                BatchExecutionResult& result);

        /// Update statistics after batch execution
        void update_statistics(const BatchExecutionResult& result);

        /// Clean up completed batches
        void cleanup_completed_batches();

        /// Check for ready batches and auto-execute if needed
        void check_and_execute_ready_batches();
    };

} // namespace scratchbird::engine
