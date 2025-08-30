// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    // Forward declarations
    class QueryPlan;
    class ExecutorNode;
    class TableScan;
    class IndexScan;
    class HashJoin;
    class NestedLoopJoin;
    class Aggregate;
    class Sort;

    /// Parallel execution modes
    enum class ParallelMode : std::uint8_t {
        DISABLED = 0, ///< No parallel execution
        ADAPTIVE = 1, ///< Adaptive parallelization based on cost
        FORCED = 2,   ///< Force parallelization regardless of cost
        LIMITED = 3   ///< Limited parallelization with resource constraints
    };

    /// Worker thread states
    enum class WorkerState : std::uint8_t {
        IDLE = 0,      ///< Worker is idle and waiting for work
        RUNNING = 1,   ///< Worker is executing a task
        FINISHING = 2, ///< Worker is finishing current task
        TERMINATED = 3 ///< Worker has been terminated
    };

    /// Parallel execution configuration
    struct ParallelConfig {
        /// Core parallelization settings
        std::uint32_t max_worker_threads{4};   ///< Maximum worker threads
        std::uint32_t min_table_size_kb{1024}; ///< Minimum table size for parallelization (KB)
        double cpu_utilization_threshold{0.8}; ///< CPU threshold for enabling parallelization

        /// Resource management
        std::uint64_t max_memory_per_worker_mb{256};  ///< Maximum memory per worker (MB)
        std::uint32_t max_parallel_queries{2};        ///< Maximum parallel queries simultaneously
        std::chrono::seconds worker_idle_timeout{30}; ///< Worker idle timeout

        /// Cost model parameters
        double parallel_setup_cost{1000.0}; ///< Fixed cost of setting up parallel execution
        double cpu_cost_per_tuple{0.01};    ///< CPU cost per tuple processed
        double io_cost_per_page{1.0};       ///< I/O cost per page read

        /// Performance tuning
        std::uint32_t work_steal_attempts{3}; ///< Number of work stealing attempts
        std::uint32_t batch_size{1000};       ///< Number of tuples per batch
        bool enable_numa_awareness{true};     ///< Enable NUMA-aware scheduling

        /// Validation
        bool is_valid() const
        {
            return max_worker_threads > 0 && max_worker_threads <= 64 && // Reasonable upper limit
                   min_table_size_kb > 0 && cpu_utilization_threshold > 0.0 &&
                   cpu_utilization_threshold <= 1.0 && max_memory_per_worker_mb > 0 &&
                   max_parallel_queries > 0 && worker_idle_timeout.count() > 0 &&
                   parallel_setup_cost >= 0.0 && cpu_cost_per_tuple > 0.0 &&
                   io_cost_per_page > 0.0 && work_steal_attempts > 0 && batch_size > 0;
        }
    };

    /// Work unit for parallel execution
    struct WorkUnit {
        std::uint64_t work_id;              ///< Unique work identifier
        std::uint32_t partition_id;         ///< Partition this work belongs to
        std::shared_ptr<ExecutorNode> node; ///< Executor node to process

        /// Data range for this work unit
        std::uint64_t start_offset{0};   ///< Start offset in data
        std::uint64_t end_offset{0};     ///< End offset in data
        std::uint64_t estimated_rows{0}; ///< Estimated number of rows

        /// Resource requirements
        std::uint64_t estimated_memory_mb{0};        ///< Estimated memory requirement
        std::chrono::milliseconds estimated_time{0}; ///< Estimated execution time

        /// Execution context
        std::unordered_map<std::string, std::string> context; ///< Execution context variables
        std::unordered_map<std::string, std::string>
            custom_data; ///< Custom data for specific operations

        WorkUnit() = default;
        WorkUnit(std::uint64_t id, std::uint32_t partition, std::shared_ptr<ExecutorNode> exec_node)
            : work_id(id), partition_id(partition), node(std::move(exec_node))
        {
        }
    };

    /// Result from parallel work execution
    struct WorkResult {
        std::uint64_t work_id;     ///< Work unit identifier
        bool success{false};       ///< Execution success status
        std::string error_message; ///< Error message if failed

        /// Performance metrics
        std::uint64_t rows_processed{0};             ///< Number of rows processed
        std::uint64_t memory_used_mb{0};             ///< Peak memory usage
        std::chrono::microseconds execution_time{0}; ///< Actual execution time

        /// Result data (could be materialized tuples, statistics, etc.)
        std::vector<std::uint8_t> result_data;                     ///< Serialized result data
        std::unordered_map<std::string, std::uint64_t> statistics; ///< Execution statistics
        std::unordered_map<std::string, std::string> custom_data;  ///< Custom result data

        /// Timing information
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;
        std::uint64_t rows_returned{0};
    };

    /// Parallel execution statistics snapshot (non-atomic for returning)
    struct ParallelExecutionStatsSnapshot {
        /// Execution counts
        std::uint64_t total_queries{0};
        std::uint64_t parallel_queries{0};
        std::uint64_t sequential_queries{0};

        /// Worker utilization
        std::uint64_t total_work_units{0};
        std::uint64_t completed_work_units{0};
        std::uint64_t failed_work_units{0};

        /// Performance metrics
        std::uint64_t total_execution_time_ms{0};
        std::uint64_t parallel_overhead_ms{0};
        std::uint64_t work_stealing_attempts{0};
        std::uint64_t successful_work_steals{0};

        /// Resource usage
        std::uint64_t peak_worker_count{0};
        std::uint64_t total_memory_allocated_mb{0};
        std::uint64_t peak_memory_usage_mb{0};
    };

    /// Internal parallel execution statistics with atomic members
    struct ParallelExecutionStats {
        /// Execution counts
        std::atomic<std::uint64_t> total_queries{0};
        std::atomic<std::uint64_t> parallel_queries{0};
        std::atomic<std::uint64_t> sequential_queries{0};

        /// Worker utilization
        std::atomic<std::uint64_t> total_work_units{0};
        std::atomic<std::uint64_t> completed_work_units{0};
        std::atomic<std::uint64_t> failed_work_units{0};

        /// Performance metrics
        std::atomic<std::uint64_t> total_execution_time_ms{0};
        std::atomic<std::uint64_t> parallel_overhead_ms{0};
        std::atomic<std::uint64_t> work_stealing_attempts{0};
        std::atomic<std::uint64_t> successful_work_steals{0};

        /// Resource usage
        std::atomic<std::uint64_t> peak_worker_count{0};
        std::atomic<std::uint64_t> total_memory_allocated_mb{0};
        std::atomic<std::uint64_t> peak_memory_usage_mb{0};

        void reset()
        {
            total_queries.store(0);
            parallel_queries.store(0);
            sequential_queries.store(0);
            total_work_units.store(0);
            completed_work_units.store(0);
            failed_work_units.store(0);
            total_execution_time_ms.store(0);
            parallel_overhead_ms.store(0);
            work_stealing_attempts.store(0);
            successful_work_steals.store(0);
            peak_worker_count.store(0);
            total_memory_allocated_mb.store(0);
            peak_memory_usage_mb.store(0);
        }

        ParallelExecutionStatsSnapshot snapshot() const
        {
            ParallelExecutionStatsSnapshot result;
            result.total_queries = total_queries.load();
            result.parallel_queries = parallel_queries.load();
            result.sequential_queries = sequential_queries.load();
            result.total_work_units = total_work_units.load();
            result.completed_work_units = completed_work_units.load();
            result.failed_work_units = failed_work_units.load();
            result.total_execution_time_ms = total_execution_time_ms.load();
            result.parallel_overhead_ms = parallel_overhead_ms.load();
            result.work_stealing_attempts = work_stealing_attempts.load();
            result.successful_work_steals = successful_work_steals.load();
            result.peak_worker_count = peak_worker_count.load();
            result.total_memory_allocated_mb = total_memory_allocated_mb.load();
            result.peak_memory_usage_mb = peak_memory_usage_mb.load();
            return result;
        }

        /// Calculate performance metrics
        double get_parallelization_ratio() const
        {
            auto total = total_queries.load();
            return total > 0 ? static_cast<double>(parallel_queries.load()) / total : 0.0;
        }

        double get_work_unit_success_rate() const
        {
            auto total = total_work_units.load();
            return total > 0 ? static_cast<double>(completed_work_units.load()) / total : 0.0;
        }

        double get_work_stealing_success_rate() const
        {
            auto attempts = work_stealing_attempts.load();
            return attempts > 0 ? static_cast<double>(successful_work_steals.load()) / attempts
                                : 0.0;
        }

        double get_average_worker_utilization() const
        {
            auto peak = peak_worker_count.load();
            return peak > 0 ? static_cast<double>(completed_work_units.load()) / peak : 0.0;
        }
    };

    /// Parallel cost estimation result
    struct ParallelCostEstimate {
        double sequential_cost{0.0};      ///< Estimated sequential execution cost
        double parallel_cost{0.0};        ///< Estimated parallel execution cost
        std::uint32_t optimal_workers{1}; ///< Optimal number of worker threads
        bool should_parallelize{false};   ///< Whether parallelization is beneficial
        std::string cost_breakdown;       ///< Detailed cost analysis

        /// Enhanced cost breakdown
        double setup_overhead{0.0};        ///< Cost of setting up parallel execution
        double coordination_overhead{0.0}; ///< Cost of coordinating between workers
        double expected_speedup{1.0};      ///< Expected speedup factor
        double memory_requirement_mb{0.0}; ///< Estimated memory requirement

        /// Calculate derived metrics
        double get_overhead_ratio() const
        {
            return sequential_cost > 0 ? (setup_overhead + coordination_overhead) / sequential_cost
                                       : 0.0;
        }

        double get_efficiency() const
        {
            return optimal_workers > 1 ? expected_speedup / optimal_workers : 1.0;
        }
    };

    /// Worker thread class for parallel execution
    class ParallelWorker
    {
      public:
        explicit ParallelWorker(std::uint32_t worker_id, const ParallelConfig& config);
        ~ParallelWorker();

        /// Non-copyable, non-moveable
        ParallelWorker(const ParallelWorker&) = delete;
        ParallelWorker& operator=(const ParallelWorker&) = delete;
        ParallelWorker(ParallelWorker&&) = delete;
        ParallelWorker& operator=(ParallelWorker&&) = delete;

        /// Worker lifecycle management
        bool start();
        void stop();
        void join();

        /// Work assignment
        bool assign_work(std::shared_ptr<WorkUnit> work);
        std::shared_ptr<WorkResult> steal_work();

        /// Status and monitoring
        WorkerState get_state() const;
        std::uint32_t get_worker_id() const
        {
            return worker_id_;
        }
        std::uint64_t get_completed_work_count() const;
        std::chrono::milliseconds get_total_execution_time() const;

      private:
        std::uint32_t worker_id_;
        ParallelConfig config_;

        /// Thread management
        std::unique_ptr<std::thread> worker_thread_;
        std::atomic<WorkerState> state_{WorkerState::IDLE};
        std::atomic<bool> shutdown_requested_{false};

        /// Work queue
        std::queue<std::shared_ptr<WorkUnit>> work_queue_;
        std::mutex work_queue_mutex_;
        std::condition_variable work_available_;

        /// Current work
        std::shared_ptr<WorkUnit> current_work_;
        std::shared_ptr<WorkResult> current_result_;
        std::mutex current_work_mutex_;

        /// Statistics
        std::atomic<std::uint64_t> completed_work_count_{0};
        std::atomic<std::uint64_t> total_execution_time_ms_{0};

        /// Worker thread main loop
        void worker_main();

        /// Execute a single work unit
        std::shared_ptr<WorkResult> execute_work_unit(std::shared_ptr<WorkUnit> work);
    };

    /// Thread pool for parallel query execution
    class ParallelThreadPool
    {
      public:
        explicit ParallelThreadPool(const ParallelConfig& config = ParallelConfig{});
        ~ParallelThreadPool();

        /// Non-copyable, non-moveable
        ParallelThreadPool(const ParallelThreadPool&) = delete;
        ParallelThreadPool& operator=(const ParallelThreadPool&) = delete;
        ParallelThreadPool(ParallelThreadPool&&) = delete;
        ParallelThreadPool& operator=(ParallelThreadPool&&) = delete;

        /// Pool lifecycle
        bool initialize();
        void shutdown();
        bool is_running() const;

        /// Work submission and execution
        std::future<std::vector<std::shared_ptr<WorkResult>>>
        submit_parallel_work(std::vector<std::shared_ptr<WorkUnit>> work_units);

        std::shared_ptr<WorkResult> submit_sequential_work(std::shared_ptr<WorkUnit> work_unit);

        /// Pool management
        void resize_pool(std::uint32_t new_size);
        std::uint32_t get_active_worker_count() const;
        std::uint32_t get_idle_worker_count() const;

        /// Configuration and monitoring
        ParallelConfig get_config() const;
        bool update_config(const ParallelConfig& config);
        ParallelExecutionStatsSnapshot get_statistics() const;
        void reset_statistics();

      private:
        ParallelConfig config_;
        mutable std::mutex config_mutex_;

        /// Worker management
        std::vector<std::unique_ptr<ParallelWorker>> workers_;
        std::atomic<bool> initialized_{false};
        std::atomic<bool> shutdown_requested_{false};
        mutable std::mutex workers_mutex_;

        /// Statistics
        ParallelExecutionStats statistics_;

        /// Work distribution
        std::atomic<std::uint64_t> next_work_id_{1};

        /// Helper methods
        void create_workers(std::uint32_t count);
        void destroy_workers();
        std::vector<ParallelWorker*> get_available_workers(std::uint32_t count);
    };

    /// System resource information for cost decisions
    struct SystemResourceInfo {
        std::uint32_t available_cores{0};
        std::uint64_t available_memory_mb{0};
        double cpu_utilization{0.0};        ///< Current CPU utilization (0.0-1.0)
        double memory_utilization{0.0};     ///< Current memory utilization (0.0-1.0)
        std::uint32_t active_queries{0};    ///< Number of active queries
        std::uint32_t available_workers{0}; ///< Available worker threads
        bool numa_enabled{false};           ///< NUMA topology available
        std::uint32_t numa_nodes{1};        ///< Number of NUMA nodes

        /// Calculate resource pressure (0.0 = no pressure, 1.0 = maximum pressure)
        double get_resource_pressure() const
        {
            return std::max(cpu_utilization, memory_utilization);
        }

        /// Get effective parallelism limit based on current system state
        std::uint32_t get_effective_parallelism_limit() const
        {
            double pressure = get_resource_pressure();
            if (pressure > 0.9)
                return 1; // Sequential only under high pressure
            if (pressure > 0.7)
                return std::min(2U, available_workers);
            if (pressure > 0.5)
                return std::min(available_cores / 2, available_workers);
            return available_workers;
        }
    };

    /// Query complexity analysis for cost-based decisions
    struct QueryComplexity {
        std::uint32_t table_count{0};
        std::uint32_t join_count{0};
        std::uint32_t aggregate_count{0};
        std::uint32_t sort_count{0};
        std::uint32_t window_function_count{0};
        std::uint64_t estimated_result_rows{0};
        std::uint64_t estimated_intermediate_rows{0};
        bool has_subqueries{false};
        bool has_recursive_cte{false};

        /// Calculate complexity score (higher = more complex)
        double get_complexity_score() const
        {
            double score = 0.0;
            score += table_count * 1.0;
            score += join_count * 5.0;
            score += aggregate_count * 3.0;
            score += sort_count * 4.0;
            score += window_function_count * 6.0;
            score += (estimated_intermediate_rows / 1000000.0) * 2.0; // Per million rows
            if (has_subqueries)
                score += 10.0;
            if (has_recursive_cte)
                score += 20.0;
            return score;
        }

        /// Determine if query benefits from parallelization
        bool benefits_from_parallelization() const
        {
            if (has_recursive_cte)
                return false; // Complex recursion usually doesn't benefit
            if (estimated_result_rows < 1000)
                return false;                    // Too small
            return get_complexity_score() > 5.0; // Complexity threshold
        }
    };

    /// Parallelization decision with detailed reasoning
    struct ParallelizationDecision {
        bool should_parallelize{false};
        std::uint32_t recommended_workers{1};
        double confidence{0.0}; ///< Confidence in the decision (0.0-1.0)
        std::string reasoning;  ///< Human-readable explanation

        /// Cost breakdown
        double sequential_cost{0.0};
        double parallel_cost{0.0};
        double setup_overhead{0.0};
        double coordination_overhead{0.0};
        double expected_speedup{1.0};

        /// Resource requirements
        std::uint64_t memory_requirement_mb{0};
        std::uint32_t min_workers{1};
        std::uint32_t max_workers{1};

        /// Performance prediction
        std::chrono::milliseconds estimated_sequential_time{0};
        std::chrono::milliseconds estimated_parallel_time{0};
    };

    /// Cost-based parallel execution planner with advanced decision making
    class ParallelCostModel
    {
      public:
        explicit ParallelCostModel(const ParallelConfig& config = ParallelConfig{});

        /// Cost estimation methods
        ParallelCostEstimate estimate_table_scan_cost(std::uint64_t table_size_bytes,
                                                      std::uint64_t estimated_rows,
                                                      bool has_index = false) const;

        ParallelCostEstimate estimate_join_cost(std::uint64_t left_size, std::uint64_t right_size,
                                                std::uint64_t estimated_output_rows) const;

        ParallelCostEstimate estimate_aggregate_cost(std::uint64_t input_rows,
                                                     std::uint32_t group_count,
                                                     std::uint32_t aggregate_functions) const;

        ParallelCostEstimate estimate_sort_cost(std::uint64_t input_rows,
                                                std::uint32_t sort_columns,
                                                std::uint64_t available_memory_mb) const;

        /// Advanced cost-based parallelization decisions
        ParallelizationDecision
        analyze_query_parallelization(const QueryComplexity& complexity,
                                      const SystemResourceInfo& resources) const;

        ParallelizationDecision
        decide_operation_parallelization(const ParallelCostEstimate& cost_estimate,
                                         const SystemResourceInfo& resources,
                                         const std::string& operation_name) const;

        /// Dynamic optimization
        std::uint32_t optimize_worker_count_dynamically(
            const ParallelCostEstimate& estimate, const SystemResourceInfo& current_resources,
            const std::vector<double>& recent_performance_history) const;

        /// Resource-aware decisions
        bool should_use_parallel_execution(const ParallelCostEstimate& estimate,
                                           const SystemResourceInfo& resources) const;

        double calculate_parallelization_efficiency(std::uint32_t worker_count,
                                                    const ParallelCostEstimate& estimate) const;

        /// Legacy methods for compatibility
        bool should_parallelize_query(const QueryPlan& plan) const;
        std::uint32_t get_optimal_worker_count(const ParallelCostEstimate& estimate) const;

        /// System resource monitoring
        SystemResourceInfo get_current_system_resources() const;
        void update_resource_cache(const SystemResourceInfo& resources);

        /// Configuration
        ParallelConfig get_config() const;
        void update_config(const ParallelConfig& config);

      private:
        ParallelConfig config_;
        mutable std::mutex config_mutex_;

        /// Cached system resource information
        mutable SystemResourceInfo cached_resources_;
        mutable std::chrono::steady_clock::time_point last_resource_update_;
        mutable std::mutex resource_cache_mutex_;

        /// Performance history for adaptive decisions
        mutable std::vector<double> recent_performance_ratios_;
        mutable std::mutex performance_history_mutex_;

        /// Helper methods for cost calculation
        double calculate_sequential_scan_cost(std::uint64_t rows) const;
        double calculate_parallel_scan_cost(std::uint64_t rows, std::uint32_t workers) const;
        double calculate_setup_overhead(std::uint32_t workers) const;
        double calculate_coordination_overhead(std::uint32_t workers,
                                               std::uint64_t data_size) const;

        /// Advanced cost modeling
        double calculate_memory_pressure_penalty(std::uint64_t required_mb,
                                                 const SystemResourceInfo& resources) const;
        double calculate_cpu_contention_penalty(std::uint32_t workers,
                                                const SystemResourceInfo& resources) const;
        double calculate_numa_penalty(std::uint32_t workers,
                                      const SystemResourceInfo& resources) const;

        /// Decision algorithms
        std::uint32_t
        find_optimal_worker_count_binary_search(const ParallelCostEstimate& base_estimate,
                                                const SystemResourceInfo& resources) const;

        double estimate_parallel_efficiency(std::uint32_t workers,
                                            const QueryComplexity& complexity) const;

        /// Resource monitoring
        SystemResourceInfo probe_system_resources() const;
        bool is_resource_cache_valid() const;
        void update_performance_history(double actual_speedup);

        /// Decision confidence calculation
        double calculate_decision_confidence(const ParallelizationDecision& decision,
                                             const SystemResourceInfo& resources) const;
    };

    /// Parallel table scan implementation
    class ParallelTableScan
    {
      public:
        /// Table partition for parallel scanning
        struct TablePartition {
            std::uint64_t partition_id;
            std::uint64_t start_row;
            std::uint64_t end_row;
            std::uint64_t estimated_rows;
            std::string partition_key_range;     // For range partitioning
            std::vector<std::string> shard_keys; // For hash partitioning
        };

        /// Parallel scan configuration
        struct ScanConfig {
            std::uint32_t max_partitions = 8;        ///< Maximum number of partitions
            std::uint64_t min_partition_size = 1000; ///< Minimum rows per partition
            std::uint32_t prefetch_batches = 2;      ///< Number of batches to prefetch
            bool enable_predicate_pushdown = true;   ///< Push predicates to partitions
            bool enable_projection_pushdown = true;  ///< Push projections to partitions

            static ScanConfig default_config()
            {
                return ScanConfig{};
            }
        };

        explicit ParallelTableScan(const std::string& schema, const std::string& table);
        ParallelTableScan(const std::string& schema, const std::string& table,
                          const ScanConfig& config);
        ~ParallelTableScan() = default;

        /// Scan planning
        std::vector<TablePartition> plan_partitions(std::uint64_t total_rows,
                                                    std::uint32_t target_workers);

        /// Create work units for parallel execution
        std::vector<std::shared_ptr<WorkUnit>>
        create_scan_work_units(const std::vector<TablePartition>& partitions,
                               const std::vector<std::string>& projections,
                               const std::string& predicate);

        /// Execute partition scan (called by workers)
        std::shared_ptr<WorkResult>
        execute_partition_scan(const TablePartition& partition,
                               const std::vector<std::string>& projections,
                               const std::string& predicate);

        /// Merge partition results
        std::shared_ptr<WorkResult>
        merge_partition_results(const std::vector<std::shared_ptr<WorkResult>>& partition_results);

        /// Statistics and diagnostics
        struct ScanStatistics {
            std::uint64_t total_partitions{0};
            std::uint64_t total_rows_scanned{0};
            std::uint64_t total_rows_filtered{0};
            std::uint64_t total_execution_time_ms{0};
            std::uint64_t max_partition_time_ms{0};
            std::uint64_t min_partition_time_ms{UINT64_MAX};
            double load_balance_factor{0.0}; // 1.0 = perfectly balanced
        };

        ScanStatistics get_statistics() const
        {
            return statistics_;
        }
        void reset_statistics();

      private:
        std::string schema_;
        std::string table_;
        ScanConfig config_;
        ScanStatistics statistics_;

        /// Helper methods
        std::uint64_t estimate_table_rows();
        std::vector<TablePartition> create_range_partitions(std::uint64_t total_rows,
                                                            std::uint32_t partition_count);
        std::vector<TablePartition> create_hash_partitions(std::uint64_t total_rows,
                                                           std::uint32_t partition_count);
        bool evaluate_partition_predicate(const TablePartition& partition,
                                          const std::string& predicate);
    };

    /// Main parallel query executor
    class ParallelQueryExecutor
    {
      public:
        explicit ParallelQueryExecutor(const ParallelConfig& config = ParallelConfig{});
        ~ParallelQueryExecutor();

        /// Non-copyable, non-moveable
        ParallelQueryExecutor(const ParallelQueryExecutor&) = delete;
        ParallelQueryExecutor& operator=(const ParallelQueryExecutor&) = delete;
        ParallelQueryExecutor(ParallelQueryExecutor&&) = delete;
        ParallelQueryExecutor& operator=(ParallelQueryExecutor&&) = delete;

        /// Lifecycle management
        bool initialize();
        void shutdown();
        bool is_running() const;

        /// Query execution
        std::shared_ptr<WorkResult> execute_query(std::shared_ptr<QueryPlan> plan);
        std::future<std::shared_ptr<WorkResult>>
        execute_query_async(std::shared_ptr<QueryPlan> plan);

        /// Parallel operation implementations
        std::vector<std::shared_ptr<WorkResult>>
        execute_parallel_table_scan(std::shared_ptr<TableScan> scan_node,
                                    std::uint32_t partition_count);

        std::shared_ptr<WorkResult> execute_parallel_hash_join(std::shared_ptr<HashJoin> join_node,
                                                               std::uint32_t worker_count);

        std::shared_ptr<WorkResult> execute_parallel_aggregate(std::shared_ptr<Aggregate> agg_node,
                                                               std::uint32_t worker_count);

        std::shared_ptr<WorkResult> execute_parallel_sort(std::shared_ptr<Sort> sort_node,
                                                          std::uint32_t worker_count);

        /// Configuration and monitoring
        ParallelConfig get_config() const;
        bool update_config(const ParallelConfig& config);
        ParallelExecutionStatsSnapshot get_statistics() const;
        void reset_statistics();

        /// Diagnostics
        std::string generate_execution_report() const;
        std::vector<std::string> get_active_queries() const;

      private:
        ParallelConfig config_;
        mutable std::mutex config_mutex_;

        /// Core components
        std::unique_ptr<ParallelThreadPool> thread_pool_;
        std::unique_ptr<ParallelCostModel> cost_model_;

        /// Execution state
        std::atomic<bool> initialized_{false};
        std::atomic<bool> shutdown_requested_{false};

        /// Active query tracking
        std::unordered_map<std::uint64_t, std::shared_ptr<QueryPlan>> active_queries_;
        std::atomic<std::uint64_t> next_query_id_{1};
        mutable std::mutex active_queries_mutex_;

        /// Helper methods
        std::vector<std::shared_ptr<WorkUnit>>
        partition_table_scan(std::shared_ptr<TableScan> scan_node, std::uint32_t partition_count);

        std::shared_ptr<WorkResult>
        merge_scan_results(const std::vector<std::shared_ptr<WorkResult>>& results);

        std::shared_ptr<WorkResult>
        merge_aggregate_results(const std::vector<std::shared_ptr<WorkResult>>& results);

        std::shared_ptr<WorkResult>
        merge_sort_results(const std::vector<std::shared_ptr<WorkResult>>& results);

        bool should_use_parallel_execution(std::shared_ptr<QueryPlan> plan);
    };

    /// Global utility functions

    /// Get optimal configuration based on system resources
    ParallelConfig get_optimal_parallel_config();

    /// System resource detection
    std::uint32_t detect_cpu_core_count();
    std::uint64_t detect_available_memory_mb();
    bool detect_numa_topology();

    /// Performance benchmarking
    struct ParallelBenchmarkResult {
        std::uint32_t worker_count;
        double execution_time_ms;
        double throughput_rows_per_sec;
        double speedup_factor;
        double efficiency;
    };

    std::vector<ParallelBenchmarkResult>
    benchmark_parallel_execution(const std::vector<std::uint32_t>& worker_counts,
                                 std::shared_ptr<QueryPlan> test_plan);

} // namespace scratchbird::engine
