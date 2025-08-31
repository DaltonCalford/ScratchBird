// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/parallel_executor.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <thread>

namespace scratchbird::engine
{

    // ParallelWorker implementation

    ParallelWorker::ParallelWorker(std::uint32_t worker_id, const ParallelConfig& config)
        : worker_id_(worker_id), config_(config)
    {
    }

    ParallelWorker::~ParallelWorker()
    {
        stop();
        join();
    }

    bool ParallelWorker::start()
    {
        if (state_.load() != WorkerState::IDLE) {
            return false; // Already started
        }

        shutdown_requested_.store(false);
        worker_thread_ = std::make_unique<std::thread>(&ParallelWorker::worker_main, this);

        // Wait for worker to initialize
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return state_.load() != WorkerState::TERMINATED;
    }

    void ParallelWorker::stop()
    {
        shutdown_requested_.store(true);
        work_available_.notify_all();
    }

    void ParallelWorker::join()
    {
        if (worker_thread_ && worker_thread_->joinable()) {
            worker_thread_->join();
        }
        worker_thread_.reset();
    }

    bool ParallelWorker::assign_work(std::shared_ptr<WorkUnit> work)
    {
        if (!work || state_.load() == WorkerState::TERMINATED) {
            return false;
        }

        std::lock_guard<std::mutex> lock(work_queue_mutex_);
        work_queue_.push(work);
        work_available_.notify_one();
        return true;
    }

    std::shared_ptr<WorkResult> ParallelWorker::steal_work()
    {
        std::lock_guard<std::mutex> lock(current_work_mutex_);

        // Can only steal if worker has completed work
        if (current_result_ && state_.load() == WorkerState::IDLE) {
            auto result = current_result_;
            current_result_.reset();
            return result;
        }

        return nullptr;
    }

    WorkerState ParallelWorker::get_state() const
    {
        return state_.load();
    }

    std::uint64_t ParallelWorker::get_completed_work_count() const
    {
        return completed_work_count_.load();
    }

    std::chrono::milliseconds ParallelWorker::get_total_execution_time() const
    {
        return std::chrono::milliseconds(total_execution_time_ms_.load());
    }

    void ParallelWorker::worker_main()
    {
        state_.store(WorkerState::IDLE);

        while (!shutdown_requested_.load()) {
            std::shared_ptr<WorkUnit> work;

            // Wait for work or timeout
            {
                std::unique_lock<std::mutex> lock(work_queue_mutex_);
                if (work_available_.wait_for(lock, config_.worker_idle_timeout, [this] {
                        return !work_queue_.empty() || shutdown_requested_.load();
                    })) {
                    if (!work_queue_.empty()) {
                        work = work_queue_.front();
                        work_queue_.pop();
                    }
                }
            }

            if (work) {
                state_.store(WorkerState::RUNNING);
                auto result = execute_work_unit(work);

                {
                    std::lock_guard<std::mutex> lock(current_work_mutex_);
                    current_work_ = work;
                    current_result_ = result;
                }

                completed_work_count_.fetch_add(1);
                state_.store(WorkerState::IDLE);
            } else if (shutdown_requested_.load()) {
                break;
            }
        }

        state_.store(WorkerState::TERMINATED);
    }

    std::shared_ptr<WorkResult> ParallelWorker::execute_work_unit(std::shared_ptr<WorkUnit> work)
    {
        auto start_time = std::chrono::steady_clock::now();
        auto result = std::make_shared<WorkResult>();
        result->work_id = work->work_id;

        try {
            // Simulate work execution - in real implementation this would:
            // 1. Execute the executor node with the given data range
            // 2. Process tuples from start_offset to end_offset
            // 3. Apply any filters, projections, or transformations
            // 4. Collect results and statistics

            // Simulate processing time based on estimated rows
            auto processing_time =
                std::chrono::microseconds(work->estimated_rows / 1000); // 1000 rows per microsecond
            std::this_thread::sleep_for(processing_time);

            result->success = true;
            result->rows_processed = work->estimated_rows;
            result->memory_used_mb = work->estimated_memory_mb;

            // Simulate some result data
            result->result_data.resize(work->estimated_rows * 100); // 100 bytes per row average
            result->statistics["tuples_processed"] = work->estimated_rows;
            result->statistics["pages_read"] =
                (work->end_offset - work->start_offset) / 4096; // 4KB pages

        } catch (const std::exception& e) {
            result->success = false;
            result->error_message = e.what();
        }

        auto end_time = std::chrono::steady_clock::now();
        result->execution_time =
            std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        total_execution_time_ms_.fetch_add(
            std::chrono::duration_cast<std::chrono::milliseconds>(result->execution_time).count());

        return result;
    }

    // ParallelThreadPool implementation

    ParallelThreadPool::ParallelThreadPool(const ParallelConfig& config) : config_(config)
    {
        assert(config_.is_valid());
    }

    ParallelThreadPool::~ParallelThreadPool()
    {
        shutdown();
    }

    bool ParallelThreadPool::initialize()
    {
        if (initialized_.exchange(true)) {
            return true; // Already initialized
        }

        std::lock_guard<std::mutex> lock(workers_mutex_);
        create_workers(config_.max_worker_threads);

        statistics_.reset();
        return true;
    }

    void ParallelThreadPool::shutdown()
    {
        if (!initialized_.exchange(false)) {
            return; // Already shutdown
        }

        shutdown_requested_.store(true);
        std::lock_guard<std::mutex> lock(workers_mutex_);
        destroy_workers();
    }

    bool ParallelThreadPool::is_running() const
    {
        return initialized_.load() && !shutdown_requested_.load();
    }

    std::future<std::vector<std::shared_ptr<WorkResult>>>
    ParallelThreadPool::submit_parallel_work(std::vector<std::shared_ptr<WorkUnit>> work_units)
    {
        if (!is_running() || work_units.empty()) {
            // Return empty results for invalid state
            std::promise<std::vector<std::shared_ptr<WorkResult>>> promise;
            promise.set_value({});
            return promise.get_future();
        }

        auto promise = std::make_shared<std::promise<std::vector<std::shared_ptr<WorkResult>>>>();
        auto future = promise->get_future();

        // Launch async execution
        std::thread([this, work_units = std::move(work_units), promise]() mutable {
            auto start_time = std::chrono::steady_clock::now();
            std::vector<std::shared_ptr<WorkResult>> results;
            results.reserve(work_units.size());

            // Get available workers
            auto workers = get_available_workers(std::min(
                static_cast<std::uint32_t>(work_units.size()), config_.max_worker_threads));

            // Distribute work to workers
            std::vector<std::future<void>> worker_futures;
            std::mutex results_mutex;

            for (size_t i = 0; i < work_units.size(); ++i) {
                auto worker = workers[i % workers.size()];

                worker_futures.push_back(
                    std::async(std::launch::async, [worker, work_unit = work_units[i], &results,
                                                    &results_mutex, this]() {
                        if (worker->assign_work(work_unit)) {
                            // Wait for worker to complete (simplified - in real impl would use
                            // proper synchronization)
                            while (worker->get_state() == WorkerState::RUNNING) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                            }

                            auto result = worker->steal_work();
                            if (result) {
                                std::lock_guard<std::mutex> lock(results_mutex);
                                results.push_back(result);
                            }
                        }
                    }));
            }

            // Wait for all workers to complete
            for (auto& future : worker_futures) {
                future.wait();
            }

            // Update statistics
            auto end_time = std::chrono::steady_clock::now();
            auto execution_time =
                std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            statistics_.total_work_units.fetch_add(work_units.size());
            statistics_.completed_work_units.fetch_add(results.size());
            statistics_.failed_work_units.fetch_add(work_units.size() - results.size());
            statistics_.total_execution_time_ms.fetch_add(execution_time.count());
            statistics_.parallel_queries.fetch_add(1);

            promise->set_value(std::move(results));
        }).detach();

        return future;
    }

    std::shared_ptr<WorkResult>
    ParallelThreadPool::submit_sequential_work(std::shared_ptr<WorkUnit> work_unit)
    {
        if (!is_running() || !work_unit) {
            return nullptr;
        }

        // Execute sequentially using any available worker
        auto workers = get_available_workers(1);
        if (workers.empty()) {
            return nullptr;
        }

        auto worker = workers[0];
        if (worker->assign_work(work_unit)) {
            // Wait for completion
            while (worker->get_state() == WorkerState::RUNNING) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            statistics_.sequential_queries.fetch_add(1);
            return worker->steal_work();
        }

        return nullptr;
    }

    void ParallelThreadPool::resize_pool(std::uint32_t new_size)
    {
        if (new_size == 0 || new_size > 64) { // Reasonable bounds
            return;
        }

        std::lock_guard<std::mutex> lock(workers_mutex_);
        std::lock_guard<std::mutex> config_lock(config_mutex_);

        if (new_size > workers_.size()) {
            // Add workers
            create_workers(new_size - static_cast<std::uint32_t>(workers_.size()));
        } else if (new_size < workers_.size()) {
            // Remove workers (simplified - in real impl would gracefully shutdown excess workers)
            workers_.resize(new_size);
        }

        config_.max_worker_threads = new_size;
    }

    std::uint32_t ParallelThreadPool::get_active_worker_count() const
    {
        std::lock_guard<std::mutex> lock(workers_mutex_);
        std::uint32_t active = 0;
        for (const auto& worker : workers_) {
            if (worker->get_state() == WorkerState::RUNNING) {
                active++;
            }
        }
        return active;
    }

    std::uint32_t ParallelThreadPool::get_idle_worker_count() const
    {
        std::lock_guard<std::mutex> lock(workers_mutex_);
        std::uint32_t idle = 0;
        for (const auto& worker : workers_) {
            if (worker->get_state() == WorkerState::IDLE) {
                idle++;
            }
        }
        return idle;
    }

    ParallelConfig ParallelThreadPool::get_config() const
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return config_;
    }

    bool ParallelThreadPool::update_config(const ParallelConfig& config)
    {
        if (!config.is_valid()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(config_mutex_);
        config_ = config;
        return true;
    }

    ParallelExecutionStatsSnapshot ParallelThreadPool::get_statistics() const
    {
        return statistics_.snapshot();
    }

    void ParallelThreadPool::reset_statistics()
    {
        statistics_.reset();
    }

    void ParallelThreadPool::create_workers(std::uint32_t count)
    {
        for (std::uint32_t i = 0; i < count; ++i) {
            auto worker_id = static_cast<std::uint32_t>(workers_.size());
            auto worker = std::make_unique<ParallelWorker>(worker_id, config_);

            if (worker->start()) {
                workers_.push_back(std::move(worker));
            }
        }

        statistics_.peak_worker_count.store(std::max(statistics_.peak_worker_count.load(),
                                                     static_cast<std::uint64_t>(workers_.size())));
    }

    void ParallelThreadPool::destroy_workers()
    {
        for (auto& worker : workers_) {
            worker->stop();
        }

        for (auto& worker : workers_) {
            worker->join();
        }

        workers_.clear();
    }

    std::vector<ParallelWorker*> ParallelThreadPool::get_available_workers(std::uint32_t count)
    {
        std::lock_guard<std::mutex> lock(workers_mutex_);
        std::vector<ParallelWorker*> available;

        for (const auto& worker : workers_) {
            if (worker->get_state() == WorkerState::IDLE && available.size() < count) {
                available.push_back(worker.get());
            }
        }

        return available;
    }

    // ParallelCostModel implementation

    ParallelCostModel::ParallelCostModel(const ParallelConfig& config) : config_(config) {}

    ParallelCostEstimate ParallelCostModel::estimate_table_scan_cost(std::uint64_t table_size_bytes,
                                                                     std::uint64_t estimated_rows,
                                                                     bool has_index) const
    {
        std::lock_guard<std::mutex> lock(config_mutex_);

        (void)has_index; // Suppress unused parameter warning

        ParallelCostEstimate estimate;

        // Sequential cost calculation
        double io_cost = (table_size_bytes / 4096.0) * config_.io_cost_per_page; // 4KB pages
        double cpu_cost = estimated_rows * config_.cpu_cost_per_tuple;
        estimate.sequential_cost = io_cost + cpu_cost;

        // Determine optimal worker count
        std::uint32_t max_beneficial_workers =
            std::min(config_.max_worker_threads,
                     static_cast<std::uint32_t>(
                         std::max(1UL, estimated_rows / 10000)) // 10K rows per worker minimum
            );

        double best_parallel_cost = estimate.sequential_cost;
        std::uint32_t best_worker_count = 1;

        for (std::uint32_t workers = 2; workers <= max_beneficial_workers; ++workers) {
            double parallel_io_cost = io_cost / workers; // Assume perfect I/O parallelization
            double parallel_cpu_cost = cpu_cost / workers;
            double setup_cost = calculate_setup_overhead(workers);
            double coordination_cost = calculate_coordination_overhead(workers, table_size_bytes);

            double total_parallel_cost =
                parallel_io_cost + parallel_cpu_cost + setup_cost + coordination_cost;

            if (total_parallel_cost < best_parallel_cost) {
                best_parallel_cost = total_parallel_cost;
                best_worker_count = workers;
            }
        }

        estimate.parallel_cost = best_parallel_cost;
        estimate.optimal_workers = best_worker_count;
        estimate.should_parallelize =
            (best_parallel_cost < estimate.sequential_cost * 0.8); // 20% improvement threshold

        // Populate enhanced cost breakdown
        estimate.setup_overhead = calculate_setup_overhead(best_worker_count);
        estimate.coordination_overhead =
            calculate_coordination_overhead(best_worker_count, table_size_bytes);
        estimate.expected_speedup =
            estimate.should_parallelize ? estimate.sequential_cost / best_parallel_cost : 1.0;
        estimate.memory_requirement_mb = best_worker_count * 128; // Estimate 128MB per worker

        // Generate cost breakdown
        std::ostringstream breakdown;
        breakdown << "Table scan cost analysis:\n";
        breakdown << "  Table size: " << (table_size_bytes / 1024 / 1024) << " MB\n";
        breakdown << "  Estimated rows: " << estimated_rows << "\n";
        breakdown << "  Sequential cost: " << std::fixed << std::setprecision(2)
                  << estimate.sequential_cost << "\n";
        breakdown << "  Parallel cost (" << estimate.optimal_workers
                  << " workers): " << estimate.parallel_cost << "\n";
        breakdown << "  Speedup: " << (estimate.sequential_cost / estimate.parallel_cost) << "x\n";
        estimate.cost_breakdown = breakdown.str();

        return estimate;
    }

    ParallelCostEstimate
    ParallelCostModel::estimate_join_cost(std::uint64_t left_size, std::uint64_t right_size,
                                          std::uint64_t estimated_output_rows) const
    {
        std::lock_guard<std::mutex> lock(config_mutex_);

        ParallelCostEstimate estimate;

        // Hash join cost model (simplified)
        double build_cost = right_size * config_.cpu_cost_per_tuple * 2;         // Build hash table
        double probe_cost = left_size * config_.cpu_cost_per_tuple;              // Probe hash table
        double output_cost = estimated_output_rows * config_.cpu_cost_per_tuple; // Generate output

        estimate.sequential_cost = build_cost + probe_cost + output_cost;

        // Parallel join can parallelize probe phase
        std::uint32_t max_workers =
            std::min(config_.max_worker_threads,
                     static_cast<std::uint32_t>(left_size / 5000)); // 5K rows per worker

        if (max_workers > 1) {
            double parallel_build_cost = build_cost; // Build phase not easily parallelizable
            double parallel_probe_cost = probe_cost / max_workers;
            double parallel_output_cost = output_cost / max_workers;
            double setup_cost = calculate_setup_overhead(max_workers);

            estimate.parallel_cost =
                parallel_build_cost + parallel_probe_cost + parallel_output_cost + setup_cost;
            estimate.optimal_workers = max_workers;
            estimate.should_parallelize =
                (estimate.parallel_cost <
                 estimate.sequential_cost * 0.7); // 30% threshold for joins

            // Populate enhanced fields
            estimate.setup_overhead = setup_cost;
            estimate.coordination_overhead =
                calculate_coordination_overhead(max_workers, left_size + right_size);
            estimate.expected_speedup = estimate.should_parallelize
                                            ? estimate.sequential_cost / estimate.parallel_cost
                                            : 1.0;
            estimate.memory_requirement_mb = max_workers * 256; // Hash tables need more memory
        } else {
            estimate.parallel_cost = estimate.sequential_cost;
            estimate.optimal_workers = 1;
            estimate.should_parallelize = false;
            estimate.setup_overhead = 0.0;
            estimate.coordination_overhead = 0.0;
            estimate.expected_speedup = 1.0;
            estimate.memory_requirement_mb = 128; // Base memory requirement
        }

        return estimate;
    }

    ParallelCostEstimate
    ParallelCostModel::estimate_aggregate_cost(std::uint64_t input_rows, std::uint32_t group_count,
                                               std::uint32_t aggregate_functions) const
    {
        std::lock_guard<std::mutex> lock(config_mutex_);

        ParallelCostEstimate estimate;

        // Aggregate cost depends on grouping cardinality
        double scan_cost = input_rows * config_.cpu_cost_per_tuple;
        double group_cost = group_count * config_.cpu_cost_per_tuple * aggregate_functions * 2;

        estimate.sequential_cost = scan_cost + group_cost;

        // Parallel aggregation with final merge
        std::uint32_t max_workers =
            std::min(config_.max_worker_threads,
                     std::max(2U, static_cast<std::uint32_t>(input_rows / 10000)));

        if (max_workers > 1) {
            double parallel_scan_cost = scan_cost / max_workers;
            double parallel_group_cost =
                group_cost; // Partial aggregation still needs to process all groups
            double merge_cost = group_count * config_.cpu_cost_per_tuple * aggregate_functions;
            double setup_cost = calculate_setup_overhead(max_workers);

            estimate.parallel_cost =
                parallel_scan_cost + parallel_group_cost + merge_cost + setup_cost;
            estimate.optimal_workers = max_workers;
            estimate.should_parallelize = (estimate.parallel_cost < estimate.sequential_cost * 0.6);

            // Populate enhanced fields
            estimate.setup_overhead = setup_cost;
            estimate.coordination_overhead = merge_cost; // Merge cost is coordination overhead
            estimate.expected_speedup = estimate.should_parallelize
                                            ? estimate.sequential_cost / estimate.parallel_cost
                                            : 1.0;
            estimate.memory_requirement_mb = max_workers * 200; // Hash tables for grouping
        } else {
            estimate.parallel_cost = estimate.sequential_cost;
            estimate.optimal_workers = 1;
            estimate.should_parallelize = false;
            estimate.setup_overhead = 0.0;
            estimate.coordination_overhead = 0.0;
            estimate.expected_speedup = 1.0;
            estimate.memory_requirement_mb = 128;
        }

        return estimate;
    }

    ParallelCostEstimate
    ParallelCostModel::estimate_sort_cost(std::uint64_t input_rows, std::uint32_t sort_columns,
                                          std::uint64_t available_memory_mb) const
    {
        std::lock_guard<std::mutex> lock(config_mutex_);

        (void)available_memory_mb; // Suppress unused parameter warning

        ParallelCostEstimate estimate;

        // Sort cost: O(n log n) with constant factors for comparison and data movement
        double comparison_cost =
            input_rows * log2(input_rows) * sort_columns * config_.cpu_cost_per_tuple;
        double data_movement_cost =
            input_rows * config_.cpu_cost_per_tuple * 3; // Read, compare, write

        estimate.sequential_cost = comparison_cost + data_movement_cost;

        // Parallel sort with merge
        std::uint32_t max_workers =
            std::min(config_.max_worker_threads,
                     std::max(2U, static_cast<std::uint32_t>(input_rows / 50000)));

        if (max_workers > 1) {
            std::uint64_t rows_per_worker = input_rows / max_workers;
            double parallel_sort_cost = max_workers * (rows_per_worker * log2(rows_per_worker) *
                                                       sort_columns * config_.cpu_cost_per_tuple);
            double merge_cost = input_rows * log2(max_workers) * config_.cpu_cost_per_tuple;
            double setup_cost = calculate_setup_overhead(max_workers);

            estimate.parallel_cost = parallel_sort_cost + merge_cost + setup_cost;
            estimate.optimal_workers = max_workers;
            estimate.should_parallelize = (estimate.parallel_cost < estimate.sequential_cost * 0.7);

            // Populate enhanced fields
            estimate.setup_overhead = setup_cost;
            estimate.coordination_overhead = merge_cost; // Merge cost is coordination overhead
            estimate.expected_speedup = estimate.should_parallelize
                                            ? estimate.sequential_cost / estimate.parallel_cost
                                            : 1.0;
            estimate.memory_requirement_mb = max_workers * 150; // Sorting needs temporary space
        } else {
            estimate.parallel_cost = estimate.sequential_cost;
            estimate.optimal_workers = 1;
            estimate.should_parallelize = false;
            estimate.setup_overhead = 0.0;
            estimate.coordination_overhead = 0.0;
            estimate.expected_speedup = 1.0;
            estimate.memory_requirement_mb = 64; // Minimal for sequential sort
        }

        return estimate;
    }

    ParallelConfig ParallelCostModel::get_config() const
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return config_;
    }

    void ParallelCostModel::update_config(const ParallelConfig& config)
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        config_ = config;
    }

    double ParallelCostModel::calculate_setup_overhead(std::uint32_t workers) const
    {
        return config_.parallel_setup_cost * workers;
    }

    double ParallelCostModel::calculate_coordination_overhead(std::uint32_t workers,
                                                              std::uint64_t data_size) const
    {
        // Coordination overhead grows with worker count and data size
        double base_overhead = config_.parallel_setup_cost * 0.1;
        double worker_overhead = workers * workers * base_overhead; // Quadratic growth
        double data_overhead =
            (data_size / 1024.0 / 1024.0) * workers * 0.01; // MB * workers * factor

        return worker_overhead + data_overhead;
    }

    // ParallelTableScan implementation

    ParallelTableScan::ParallelTableScan(const std::string& schema, const std::string& table)
        : schema_(schema), table_(table), config_(ScanConfig::default_config())
    {
    }

    ParallelTableScan::ParallelTableScan(const std::string& schema, const std::string& table,
                                         const ScanConfig& config)
        : schema_(schema), table_(table), config_(config)
    {
    }

    std::vector<ParallelTableScan::TablePartition>
    ParallelTableScan::plan_partitions(std::uint64_t total_rows, std::uint32_t target_workers)
    {
        // Calculate optimal partition count
        std::uint32_t partition_count = std::min(target_workers, config_.max_partitions);

        // Ensure minimum partition size
        if (total_rows / partition_count < config_.min_partition_size) {
            partition_count =
                std::max(1U, static_cast<std::uint32_t>(total_rows / config_.min_partition_size));
        }

        // For now, use range partitioning as the primary strategy
        // In production, would analyze table distribution and choose optimal strategy
        return create_range_partitions(total_rows, partition_count);
    }

    std::vector<std::shared_ptr<WorkUnit>>
    ParallelTableScan::create_scan_work_units(const std::vector<TablePartition>& partitions,
                                              const std::vector<std::string>& projections,
                                              const std::string& predicate)
    {
        std::vector<std::shared_ptr<WorkUnit>> work_units;
        work_units.reserve(partitions.size());

        std::uint64_t work_id = 1;
        for (const auto& partition : partitions) {
            // Skip partition if predicate cannot be satisfied
            if (!predicate.empty() && config_.enable_predicate_pushdown &&
                !evaluate_partition_predicate(partition, predicate)) {
                continue;
            }

            auto work_unit = std::make_shared<WorkUnit>();
            work_unit->work_id = work_id++;
            work_unit->partition_id = partition.partition_id;
            work_unit->start_offset = partition.start_row;
            work_unit->end_offset = partition.end_row;
            work_unit->estimated_rows = partition.estimated_rows;

            // Estimate memory and time requirements
            work_unit->estimated_memory_mb =
                std::max(1UL, (partition.estimated_rows * 50) /
                                  (1024 * 1024)); // ~50 bytes per row estimate, minimum 1MB
            work_unit->estimated_time = std::chrono::milliseconds(std::max(
                1UL, partition.estimated_rows / 1000)); // ~1000 rows/ms estimate, minimum 1ms

            // Store table scan parameters in custom data
            work_unit->custom_data["schema"] = schema_;
            work_unit->custom_data["table"] = table_;
            work_unit->custom_data["predicate"] = predicate;

            // Store projections
            std::ostringstream proj_stream;
            for (size_t i = 0; i < projections.size(); ++i) {
                if (i > 0)
                    proj_stream << ",";
                proj_stream << projections[i];
            }
            work_unit->custom_data["projections"] = proj_stream.str();

            work_units.push_back(work_unit);
        }

        return work_units;
    }

    std::shared_ptr<WorkResult>
    ParallelTableScan::execute_partition_scan(const TablePartition& partition,
                                              const std::vector<std::string>& projections,
                                              const std::string& predicate)
    {
        auto start_time = std::chrono::steady_clock::now();

        auto result = std::make_shared<WorkResult>();
        result->work_id = partition.partition_id;
        result->success = true;
        result->start_time = start_time;

        // Simulate scanning partition rows
        // In production implementation, would:
        // 1. Open table storage at start_row offset
        // 2. Read rows until end_row
        // 3. Apply predicate filtering
        // 4. Apply column projection
        // 5. Return result set

        std::vector<std::vector<std::string>> result_rows;
        result_rows.reserve(partition.estimated_rows);

        std::uint64_t rows_scanned = 0;
        std::uint64_t rows_filtered = 0;

        // Mock data generation and filtering
        for (std::uint64_t row_id = partition.start_row; row_id < partition.end_row; ++row_id) {
            rows_scanned++;

            // Create mock row data
            std::vector<std::string> row;
            row.push_back("id_" + std::to_string(row_id));
            row.push_back("name_" + std::to_string(row_id % 1000));
            row.push_back(std::to_string(row_id * 10));
            row.push_back("data_" + std::to_string(row_id % 100));

            // Apply predicate filtering (simplified)
            bool passes_predicate = true;
            if (!predicate.empty()) {
                // Simple predicate evaluation - in production would use expression evaluator
                if (predicate.find("id >") != std::string::npos) {
                    passes_predicate =
                        (row_id > partition.start_row + partition.estimated_rows / 2);
                }
            }

            if (passes_predicate) {
                // Apply column projection
                std::vector<std::string> projected_row;
                if (projections.empty()) {
                    projected_row = row; // Select all columns
                } else {
                    // Simple projection - in production would map column names to indices
                    for (const auto& proj : projections) {
                        if (proj == "id")
                            projected_row.push_back(row[0]);
                        else if (proj == "name")
                            projected_row.push_back(row[1]);
                        else if (proj == "value")
                            projected_row.push_back(row[2]);
                        else if (proj == "data")
                            projected_row.push_back(row[3]);
                        else
                            projected_row.push_back("null");
                    }
                }

                result_rows.push_back(projected_row);
            } else {
                rows_filtered++;
            }
        }

        auto end_time = std::chrono::steady_clock::now();
        result->end_time = end_time;
        result->execution_time =
            std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        // Store results
        result->rows_processed = rows_scanned;
        result->rows_returned = result_rows.size();

        // In production, would store result_rows in appropriate format
        result->custom_data["result_count"] = std::to_string(result_rows.size());
        result->custom_data["rows_filtered"] = std::to_string(rows_filtered);

        return result;
    }

    std::shared_ptr<WorkResult> ParallelTableScan::merge_partition_results(
        const std::vector<std::shared_ptr<WorkResult>>& partition_results)
    {
        if (partition_results.empty()) {
            return nullptr;
        }

        auto merged_result = std::make_shared<WorkResult>();
        merged_result->work_id = 0; // Merged result ID
        merged_result->success = true;
        merged_result->start_time = partition_results[0]->start_time;

        std::uint64_t total_rows_processed = 0;
        std::uint64_t total_rows_returned = 0;
        std::uint64_t total_execution_time_us = 0;
        std::chrono::steady_clock::time_point latest_end_time = partition_results[0]->end_time;

        // Aggregate statistics from all partitions
        for (const auto& result : partition_results) {
            if (!result->success) {
                merged_result->success = false;
                merged_result->error_message += result->error_message + "; ";
                continue;
            }

            total_rows_processed += result->rows_processed;
            total_rows_returned += result->rows_returned;
            total_execution_time_us += result->execution_time.count();

            if (result->end_time > latest_end_time) {
                latest_end_time = result->end_time;
            }
        }

        merged_result->end_time = latest_end_time;
        merged_result->execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
            latest_end_time - merged_result->start_time);
        merged_result->rows_processed = total_rows_processed;
        merged_result->rows_returned = total_rows_returned;

        // Update statistics
        statistics_.total_partitions = partition_results.size();
        statistics_.total_rows_scanned = total_rows_processed;
        statistics_.total_rows_filtered = total_rows_processed - total_rows_returned;
        statistics_.total_execution_time_ms = merged_result->execution_time.count() / 1000;

        // Calculate load balance factor
        if (!partition_results.empty()) {
            std::uint64_t max_time = 0, min_time = UINT64_MAX;
            for (const auto& result : partition_results) {
                std::uint64_t time_us = static_cast<std::uint64_t>(result->execution_time.count());
                max_time = std::max(max_time, time_us);
                min_time = std::min(min_time, time_us);
            }
            statistics_.max_partition_time_ms = max_time / 1000;
            statistics_.min_partition_time_ms = min_time / 1000;
            statistics_.load_balance_factor =
                min_time > 0 ? static_cast<double>(min_time) / max_time : 0.0;
        }

        return merged_result;
    }

    void ParallelTableScan::reset_statistics()
    {
        statistics_ = ScanStatistics{};
    }

    std::uint64_t ParallelTableScan::estimate_table_rows()
    {
        // Mock implementation - in production would query table statistics
        // or use sampling to estimate row count

        if (table_ == "large_table")
            return 10000000; // 10M rows
        if (table_ == "medium_table")
            return 1000000; // 1M rows
        if (table_ == "small_table")
            return 10000; // 10K rows

        return 100000; // Default estimate
    }

    std::vector<ParallelTableScan::TablePartition>
    ParallelTableScan::create_range_partitions(std::uint64_t total_rows,
                                               std::uint32_t partition_count)
    {
        std::vector<TablePartition> partitions;
        partitions.reserve(partition_count);

        std::uint64_t rows_per_partition = total_rows / partition_count;
        std::uint64_t remainder = total_rows % partition_count;

        std::uint64_t current_start = 0;
        for (std::uint32_t i = 0; i < partition_count; ++i) {
            TablePartition partition;
            partition.partition_id = i;
            partition.start_row = current_start;

            // Distribute remainder among first partitions
            std::uint64_t partition_size = rows_per_partition + (i < remainder ? 1 : 0);
            partition.end_row = current_start + partition_size;
            partition.estimated_rows = partition_size;

            // Create range key description
            partition.partition_key_range = "rows_" + std::to_string(partition.start_row) + "_to_" +
                                            std::to_string(partition.end_row - 1);

            partitions.push_back(partition);
            current_start = partition.end_row;
        }

        return partitions;
    }

    std::vector<ParallelTableScan::TablePartition>
    ParallelTableScan::create_hash_partitions(std::uint64_t total_rows,
                                              std::uint32_t partition_count)
    {
        std::vector<TablePartition> partitions;
        partitions.reserve(partition_count);

        std::uint64_t rows_per_partition = total_rows / partition_count;

        for (std::uint32_t i = 0; i < partition_count; ++i) {
            TablePartition partition;
            partition.partition_id = i;
            partition.start_row = i * rows_per_partition;
            partition.end_row =
                (i == partition_count - 1) ? total_rows : (i + 1) * rows_per_partition;
            partition.estimated_rows = partition.end_row - partition.start_row;

            // Create hash partition description
            partition.shard_keys.push_back("hash_" + std::to_string(i));
            partition.partition_key_range = "hash_partition_" + std::to_string(i);

            partitions.push_back(partition);
        }

        return partitions;
    }

    bool ParallelTableScan::evaluate_partition_predicate(const TablePartition& partition,
                                                         const std::string& predicate)
    {
        // Simplified predicate evaluation for partition pruning
        // In production, would use sophisticated predicate analysis

        if (predicate.empty())
            return true;

        // Suppress unused parameter warning in simplified implementation
        (void)partition;

        // Example: if predicate involves row ranges and we know partition ranges
        if (predicate.find("id >") != std::string::npos) {
            // Extract value and check if partition range intersects
            return true; // Simplified - assume all partitions might match
        }

        if (predicate.find("id <") != std::string::npos) {
            return true; // Simplified
        }

        return true; // By default, include partition
    }

    // Enhanced ParallelCostModel implementation for cost-based parallelization decisions

    SystemResourceInfo ParallelCostModel::get_current_system_resources() const
    {
        std::lock_guard<std::mutex> lock(resource_cache_mutex_);

        if (!is_resource_cache_valid()) {
            cached_resources_ = probe_system_resources();
            last_resource_update_ = std::chrono::steady_clock::now();
        }

        return cached_resources_;
    }

    void ParallelCostModel::update_resource_cache(const SystemResourceInfo& resources)
    {
        std::lock_guard<std::mutex> lock(resource_cache_mutex_);
        cached_resources_ = resources;
        last_resource_update_ = std::chrono::steady_clock::now();
    }

    bool ParallelCostModel::is_resource_cache_valid() const
    {
        auto now = std::chrono::steady_clock::now();
        auto age =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_resource_update_);
        return age.count() < 1000; // Cache valid for 1 second
    }

    SystemResourceInfo ParallelCostModel::probe_system_resources() const
    {
        SystemResourceInfo info;

        // Detect basic system resources
        info.available_cores = detect_cpu_core_count();
        info.available_memory_mb = detect_available_memory_mb();
        info.numa_enabled = detect_numa_topology();
        info.numa_nodes = info.numa_enabled ? std::max(1U, info.available_cores / 8) : 1U;

        // Simulate resource utilization (in production, would use system APIs)
        info.cpu_utilization = 0.3 + (rand() % 40) / 100.0;    // 30-70% utilization
        info.memory_utilization = 0.2 + (rand() % 50) / 100.0; // 20-70% utilization
        info.active_queries = rand() % 5;                      // 0-4 active queries
        info.available_workers = std::max(1U, info.available_cores - info.active_queries);

        return info;
    }

    ParallelizationDecision
    ParallelCostModel::analyze_query_parallelization(const QueryComplexity& complexity,
                                                     const SystemResourceInfo& resources) const
    {
        ParallelizationDecision decision;

        // Initial assessment
        if (!complexity.benefits_from_parallelization()) {
            decision.should_parallelize = false;
            decision.reasoning = "Query complexity too low to benefit from parallelization";
            decision.confidence = 0.9;
            return decision;
        }

        // Check system resources
        if (resources.get_resource_pressure() > 0.9) {
            decision.should_parallelize = false;
            decision.reasoning = "System under high resource pressure";
            decision.confidence = 0.8;
            return decision;
        }

        // Calculate complexity-based worker count
        double complexity_score = complexity.get_complexity_score();
        std::uint32_t complexity_workers =
            std::min(static_cast<std::uint32_t>(complexity_score / 5.0) + 1,
                     resources.get_effective_parallelism_limit());

        // Estimate costs for different worker counts
        std::vector<std::pair<std::uint32_t, double>> cost_analysis;

        for (std::uint32_t workers = 1; workers <= complexity_workers; ++workers) {
            double base_cost = complexity.estimated_intermediate_rows * config_.cpu_cost_per_tuple;
            double parallel_cost = base_cost / workers;
            double overhead_cost =
                calculate_setup_overhead(workers) +
                calculate_coordination_overhead(workers, complexity.estimated_intermediate_rows);

            // Apply penalties
            double memory_penalty = calculate_memory_pressure_penalty(workers * 256, resources);
            double cpu_penalty = calculate_cpu_contention_penalty(workers, resources);
            double numa_penalty = calculate_numa_penalty(workers, resources);

            double total_cost =
                parallel_cost + overhead_cost + memory_penalty + cpu_penalty + numa_penalty;
            cost_analysis.emplace_back(workers, total_cost);
        }

        // Find optimal worker count
        auto min_cost_it =
            std::min_element(cost_analysis.begin(), cost_analysis.end(),
                             [](const auto& a, const auto& b) { return a.second < b.second; });

        std::uint32_t optimal_workers = min_cost_it->first;
        double optimal_cost = min_cost_it->second;
        double sequential_cost = cost_analysis[0].second;

        // Make parallelization decision
        decision.should_parallelize = optimal_workers > 1 && optimal_cost < sequential_cost * 0.9;
        decision.recommended_workers = optimal_workers;
        decision.sequential_cost = sequential_cost;
        decision.parallel_cost = optimal_cost;
        decision.expected_speedup =
            decision.should_parallelize ? sequential_cost / optimal_cost : 1.0;

        // Calculate confidence and reasoning
        decision.confidence = calculate_decision_confidence(decision, resources);

        if (decision.should_parallelize) {
            decision.reasoning = "Expected " + std::to_string(decision.expected_speedup) +
                                 "x speedup with " + std::to_string(optimal_workers) + " workers";
        } else {
            decision.reasoning = "Parallel overhead exceeds benefits";
        }

        // Set resource requirements
        decision.memory_requirement_mb = optimal_workers * 256; // Estimate
        decision.min_workers = 1;
        decision.max_workers = complexity_workers;

        // Set time estimates
        decision.estimated_sequential_time =
            std::chrono::milliseconds(static_cast<long>(sequential_cost / 1000)); // Convert to ms
        decision.estimated_parallel_time =
            std::chrono::milliseconds(static_cast<long>(optimal_cost / 1000));

        return decision;
    }

    ParallelizationDecision
    ParallelCostModel::decide_operation_parallelization(const ParallelCostEstimate& cost_estimate,
                                                        const SystemResourceInfo& resources,
                                                        const std::string& operation_name) const
    {
        ParallelizationDecision decision;

        // Use existing cost estimate
        decision.sequential_cost = cost_estimate.sequential_cost;
        decision.parallel_cost = cost_estimate.parallel_cost;
        decision.setup_overhead = cost_estimate.setup_overhead;
        decision.coordination_overhead = cost_estimate.coordination_overhead;

        // Check basic feasibility
        std::uint32_t max_workers = resources.get_effective_parallelism_limit();
        if (max_workers <= 1 || cost_estimate.optimal_workers <= 1) {
            decision.should_parallelize = false;
            decision.recommended_workers = 1;
            decision.reasoning = "Insufficient resources for parallelization";
            decision.confidence = 0.8;
            return decision;
        }

        // Calculate expected performance
        decision.expected_speedup = cost_estimate.expected_speedup;
        decision.recommended_workers = std::min(cost_estimate.optimal_workers, max_workers);

        // Apply resource-based adjustments
        double resource_pressure = resources.get_resource_pressure();
        if (resource_pressure > 0.7) {
            decision.recommended_workers = std::min(decision.recommended_workers, 2U);
            decision.expected_speedup *= (1.0 - resource_pressure * 0.3); // Reduce expected speedup
        }

        // Make decision based on cost-benefit analysis
        double threshold = 1.2; // Must be at least 20% faster
        decision.should_parallelize = decision.expected_speedup > threshold;

        // Calculate confidence
        decision.confidence = calculate_decision_confidence(decision, resources);

        // Generate reasoning
        if (decision.should_parallelize) {
            decision.reasoning = operation_name + ": Expected " +
                                 std::to_string(decision.expected_speedup) + "x speedup";
        } else {
            decision.reasoning = operation_name + ": Insufficient parallelization benefit";
        }

        return decision;
    }

    std::uint32_t ParallelCostModel::optimize_worker_count_dynamically(
        const ParallelCostEstimate& estimate, const SystemResourceInfo& current_resources,
        const std::vector<double>& recent_performance_history) const
    {
        std::uint32_t base_workers = estimate.optimal_workers;
        std::uint32_t max_workers = current_resources.get_effective_parallelism_limit();

        // Start with resource-constrained worker count
        std::uint32_t adjusted_workers = std::min(base_workers, max_workers);

        // Apply historical performance adjustments
        if (!recent_performance_history.empty()) {
            double avg_performance = 0.0;
            for (double perf : recent_performance_history) {
                avg_performance += perf;
            }
            avg_performance /= recent_performance_history.size();

            // If recent performance was poor, reduce workers
            if (avg_performance < 0.8) {
                adjusted_workers = std::max(1U, adjusted_workers / 2);
            }
            // If recent performance was excellent, consider more workers
            else if (avg_performance > 1.5 && adjusted_workers < max_workers) {
                adjusted_workers = std::min(max_workers, adjusted_workers + 1);
            }
        }

        // Apply resource pressure penalty
        double pressure = current_resources.get_resource_pressure();
        if (pressure > 0.8) {
            adjusted_workers = std::max(1U, adjusted_workers / 2);
        } else if (pressure > 0.6) {
            adjusted_workers =
                std::min(adjusted_workers, std::max(2U, current_resources.available_cores / 2));
        }

        return adjusted_workers;
    }

    bool ParallelCostModel::should_use_parallel_execution(const ParallelCostEstimate& estimate,
                                                          const SystemResourceInfo& resources) const
    {
        // Basic feasibility checks
        if (estimate.optimal_workers <= 1 || resources.available_workers <= 1) {
            return false;
        }

        // Cost-benefit analysis
        if (estimate.expected_speedup < 1.2) { // Must be at least 20% faster
            return false;
        }

        // Resource pressure check
        if (resources.get_resource_pressure() > 0.9) {
            return false;
        }

        // Memory requirements check
        std::uint64_t required_memory = estimate.optimal_workers * 256; // Estimate
        if (required_memory >
            resources.available_memory_mb * 0.7) { // Don't use more than 70% of memory
            return false;
        }

        return true;
    }

    double ParallelCostModel::calculate_parallelization_efficiency(
        std::uint32_t worker_count, const ParallelCostEstimate& estimate) const
    {
        if (worker_count <= 1)
            return 1.0;

        // Theoretical maximum speedup
        double theoretical_speedup = static_cast<double>(worker_count);

        // Apply Amdahl's law approximation
        double parallel_fraction = 0.9; // Assume 90% of work can be parallelized
        double amdahl_speedup =
            1.0 / ((1.0 - parallel_fraction) + parallel_fraction / worker_count);

        // Factor in overhead
        double overhead_penalty = estimate.setup_overhead / estimate.sequential_cost;
        double coordination_penalty = estimate.coordination_overhead / estimate.sequential_cost;

        double efficiency = (amdahl_speedup / theoretical_speedup) *
                            (1.0 - overhead_penalty - coordination_penalty);

        return std::max(0.0, std::min(1.0, efficiency));
    }

    double
    ParallelCostModel::calculate_memory_pressure_penalty(std::uint64_t required_mb,
                                                         const SystemResourceInfo& resources) const
    {
        double memory_ratio = static_cast<double>(required_mb) / resources.available_memory_mb;

        if (memory_ratio > 0.9) {
            return required_mb * 0.5; // Heavy penalty for memory pressure
        } else if (memory_ratio > 0.7) {
            return required_mb * 0.2; // Moderate penalty
        }

        return 0.0; // No penalty
    }

    double
    ParallelCostModel::calculate_cpu_contention_penalty(std::uint32_t workers,
                                                        const SystemResourceInfo& resources) const
    {
        double cpu_ratio =
            static_cast<double>(workers + resources.active_queries) / resources.available_cores;

        if (cpu_ratio > 1.5) {
            return workers * 1000.0; // Heavy penalty for oversubscription
        } else if (cpu_ratio > 1.0) {
            return workers * 200.0; // Moderate penalty
        }

        return 0.0; // No penalty
    }

    double ParallelCostModel::calculate_numa_penalty(std::uint32_t workers,
                                                     const SystemResourceInfo& resources) const
    {
        if (!resources.numa_enabled || workers <= resources.numa_nodes) {
            return 0.0; // No NUMA penalty
        }

        // Penalty for cross-NUMA communication
        double cross_numa_workers = workers - resources.numa_nodes;
        return cross_numa_workers * 100.0; // Penalty per cross-NUMA worker
    }

    double
    ParallelCostModel::calculate_decision_confidence(const ParallelizationDecision& decision,
                                                     const SystemResourceInfo& resources) const
    {
        double confidence = 0.5; // Base confidence

        // Increase confidence for clear cost differences
        if (decision.expected_speedup > 2.0) {
            confidence += 0.3;
        } else if (decision.expected_speedup > 1.5) {
            confidence += 0.2;
        } else if (decision.expected_speedup < 1.1) {
            confidence -= 0.2;
        }

        // Reduce confidence under resource pressure
        double pressure = resources.get_resource_pressure();
        confidence -= pressure * 0.3;

        // Consider historical performance
        std::lock_guard<std::mutex> lock(performance_history_mutex_);
        if (!recent_performance_ratios_.empty()) {
            double avg_historical = 0.0;
            for (double ratio : recent_performance_ratios_) {
                avg_historical += ratio;
            }
            avg_historical /= recent_performance_ratios_.size();

            // If historical performance matches prediction, increase confidence
            if (std::abs(avg_historical - decision.expected_speedup) < 0.2) {
                confidence += 0.2;
            } else {
                confidence -= 0.1;
            }
        }

        return std::max(0.0, std::min(1.0, confidence));
    }

    // ParallelQueryExecutor implementation stubs (to be fully implemented later)

    ParallelQueryExecutor::ParallelQueryExecutor(const ParallelConfig& config) : config_(config)
    {
        thread_pool_ = std::make_unique<ParallelThreadPool>(config);
        cost_model_ = std::make_unique<ParallelCostModel>(config);
    }

    ParallelQueryExecutor::~ParallelQueryExecutor()
    {
        shutdown();
    }

    bool ParallelQueryExecutor::initialize()
    {
        if (initialized_.load())
            return true;

        bool success = thread_pool_->initialize();
        initialized_.store(success);
        return success;
    }

    void ParallelQueryExecutor::shutdown()
    {
        shutdown_requested_.store(true);
        if (thread_pool_) {
            thread_pool_->shutdown();
        }
        initialized_.store(false);
    }

    bool ParallelQueryExecutor::is_running() const
    {
        return initialized_.load() && !shutdown_requested_.load();
    }

    ParallelConfig ParallelQueryExecutor::get_config() const
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return config_;
    }

    bool ParallelQueryExecutor::update_config(const ParallelConfig& config)
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        config_ = config;
        return true;
    }

    ParallelExecutionStatsSnapshot ParallelQueryExecutor::get_statistics() const
    {
        return thread_pool_->get_statistics();
    }

    void ParallelQueryExecutor::reset_statistics()
    {
        thread_pool_->reset_statistics();
    }

    // =============================================================================
    // ParallelQueryExecutor Join Implementation Methods
    // =============================================================================

    std::shared_ptr<WorkResult>
    ParallelQueryExecutor::execute_parallel_hash_join(std::shared_ptr<HashJoin> join_node,
                                                      std::uint32_t worker_count)
    {
        if (!join_node) {
            auto result = std::make_shared<WorkResult>();
            result->success = false;
            result->error_message = "Invalid HashJoin node provided";
            return result;
        }

        auto start_time = std::chrono::steady_clock::now();

        try {
            // Create parallel hash join instance
            ParallelHashJoin parallel_join(
                "hash_join_" + std::to_string(reinterpret_cast<std::uintptr_t>(join_node.get())));

            // Estimate table sizes - simplified for simulation
            std::uint64_t build_table_size = 10000; // Simulated build table size
            std::uint64_t probe_table_size = 50000; // Simulated probe table size

            // Plan join partitions
            auto partitions = parallel_join.plan_join_partitions(build_table_size, probe_table_size,
                                                                 worker_count);

            if (partitions.empty()) {
                auto result = std::make_shared<WorkResult>();
                result->success = false;
                result->error_message = "Failed to create join partitions";
                return result;
            }

            // Create build phase work units
            std::vector<std::string> build_keys = {"id", "key"}; // Simulated join keys
            std::string build_predicate = "";                    // No additional predicate
            auto build_work_units =
                parallel_join.create_build_work_units(partitions, build_keys, build_predicate);

            // Create probe phase work units
            std::vector<std::string> probe_keys = {"id", "key"}; // Matching join keys
            std::string probe_predicate = "";                    // No additional predicate
            auto probe_work_units =
                parallel_join.create_probe_work_units(partitions, probe_keys, probe_predicate);

            // Execute build phase in parallel
            std::vector<std::shared_ptr<WorkResult>> build_results;
            build_results.reserve(partitions.size());

            for (const auto& partition : partitions) {
                auto build_result =
                    parallel_join.execute_build_phase(partition, build_keys, build_predicate);
                build_results.push_back(build_result);
            }

            // Execute probe phase in parallel
            std::vector<std::shared_ptr<WorkResult>> probe_results;
            probe_results.reserve(partitions.size());

            for (const auto& partition : partitions) {
                auto probe_result =
                    parallel_join.execute_probe_phase(partition, probe_keys, probe_predicate);
                probe_results.push_back(probe_result);
            }

            // Merge results
            auto final_result = parallel_join.merge_join_results(build_results, probe_results);

            if (final_result) {
                // Update execution time to include coordination overhead
                final_result->end_time = std::chrono::steady_clock::now();
                auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(
                    final_result->end_time - start_time);
                final_result->execution_time = total_time;

                // Add parallel execution statistics
                final_result->custom_data["parallel_workers"] = std::to_string(worker_count);
                final_result->custom_data["join_partitions"] = std::to_string(partitions.size());
                final_result->custom_data["join_type"] = "parallel_hash_join";

                auto join_stats = parallel_join.get_statistics();
                final_result->custom_data["total_build_time_us"] =
                    std::to_string(join_stats.build_time.count());
                final_result->custom_data["total_probe_time_us"] =
                    std::to_string(join_stats.probe_time.count());
                final_result->custom_data["hash_collisions"] =
                    std::to_string(join_stats.hash_collisions);
            }

            return final_result;

        } catch (const std::exception& e) {
            auto result = std::make_shared<WorkResult>();
            result->success = false;
            result->error_message = "Parallel hash join failed: " + std::string(e.what());
            result->end_time = std::chrono::steady_clock::now();
            result->execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
                result->end_time - start_time);
            return result;
        }
    }

    std::shared_ptr<WorkResult> ParallelQueryExecutor::execute_parallel_nested_loop_join(
        std::shared_ptr<NestedLoopJoin> join_node, std::uint32_t worker_count)
    {
        if (!join_node) {
            auto result = std::make_shared<WorkResult>();
            result->success = false;
            result->error_message = "Invalid NestedLoopJoin node provided";
            return result;
        }

        auto start_time = std::chrono::steady_clock::now();

        try {
            // Create parallel nested loop join instance
            ParallelNestedLoopJoin parallel_join(
                "nested_loop_join_" +
                std::to_string(reinterpret_cast<std::uintptr_t>(join_node.get())));

            // Estimate table sizes - simplified for simulation
            std::uint64_t outer_table_size = 5000;  // Simulated outer table size
            std::uint64_t inner_table_size = 20000; // Simulated inner table size

            // Plan join partitions
            auto partitions = parallel_join.plan_nested_loop_partitions(
                outer_table_size, inner_table_size, worker_count);

            if (partitions.empty()) {
                auto result = std::make_shared<WorkResult>();
                result->success = false;
                result->error_message = "Failed to create nested loop join partitions";
                return result;
            }

            // Create work units
            std::string join_predicate = "outer.id = inner.id"; // Simulated join condition
            auto work_units =
                parallel_join.create_nested_loop_work_units(partitions, join_predicate);

            // Execute partitions in parallel
            std::vector<std::shared_ptr<WorkResult>> partition_results;
            partition_results.reserve(partitions.size());

            for (const auto& partition : partitions) {
                auto partition_result =
                    parallel_join.execute_nested_loop_partition(partition, join_predicate);
                partition_results.push_back(partition_result);
            }

            // Merge results
            auto final_result = parallel_join.merge_nested_loop_results(partition_results);

            if (final_result) {
                // Update execution time to include coordination overhead
                final_result->end_time = std::chrono::steady_clock::now();
                auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(
                    final_result->end_time - start_time);
                final_result->execution_time = total_time;

                // Add parallel execution statistics
                final_result->custom_data["parallel_workers"] = std::to_string(worker_count);
                final_result->custom_data["join_partitions"] = std::to_string(partitions.size());
                final_result->custom_data["join_type"] = "parallel_nested_loop_join";

                auto join_stats = parallel_join.get_statistics();
                final_result->custom_data["total_outer_rows"] =
                    std::to_string(join_stats.outer_rows_processed);
                final_result->custom_data["total_inner_scans"] =
                    std::to_string(join_stats.inner_scans_performed);
                final_result->custom_data["cache_hit_ratio"] =
                    std::to_string(join_stats.cache_hit_ratio);
                final_result->custom_data["total_execution_time_us"] =
                    std::to_string(join_stats.total_time.count());
            }

            return final_result;

        } catch (const std::exception& e) {
            auto result = std::make_shared<WorkResult>();
            result->success = false;
            result->error_message = "Parallel nested loop join failed: " + std::string(e.what());
            result->end_time = std::chrono::steady_clock::now();
            result->execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
                result->end_time - start_time);
            return result;
        }
    }

    std::string ParallelQueryExecutor::generate_execution_report() const
    {
        auto stats = get_statistics();
        std::ostringstream report;
        report << "Parallel Execution Report:\n";
        report << "Total queries: " << stats.total_queries << "\n";
        report << "Parallel queries: " << stats.parallel_queries << "\n";
        report << "Sequential queries: " << stats.sequential_queries << "\n";
        report << "Total work units: " << stats.total_work_units << "\n";
        report << "Completed work units: " << stats.completed_work_units << "\n";
        report << "Failed work units: " << stats.failed_work_units << "\n";
        return report.str();
    }

    std::vector<std::string> ParallelQueryExecutor::get_active_queries() const
    {
        std::lock_guard<std::mutex> lock(active_queries_mutex_);
        std::vector<std::string> result;
        for (const auto& query : active_queries_) {
            result.push_back("Query " + std::to_string(query.first));
        }
        return result;
    }

    // Global utility functions

    ParallelConfig get_optimal_parallel_config()
    {
        ParallelConfig config;

        // Detect system resources
        config.max_worker_threads =
            std::min(detect_cpu_core_count(), 16U); // Cap at 16 for stability
        auto memory_mb = detect_available_memory_mb();
        config.max_memory_per_worker_mb =
            std::max(64UL, memory_mb / (config.max_worker_threads * 4)); // Leave 75% for other uses

        // Adjust thresholds based on system capabilities
        if (config.max_worker_threads >= 8) {
            config.min_table_size_kb = 2048;        // Larger threshold for powerful systems
            config.cpu_utilization_threshold = 0.7; // More aggressive parallelization
        }

        config.enable_numa_awareness = detect_numa_topology();

        return config;
    }

    std::uint32_t detect_cpu_core_count()
    {
        auto cores = std::thread::hardware_concurrency();
        return cores > 0 ? cores : 4; // Default to 4 if detection fails
    }

    std::uint64_t detect_available_memory_mb()
    {
        // Simplified memory detection - in real implementation would use platform-specific APIs
        return 8 * 1024; // Default to 8GB
    }

    bool detect_numa_topology()
    {
        // Simplified NUMA detection - in real implementation would check system topology
        return detect_cpu_core_count() > 8; // Assume NUMA on systems with >8 cores
    }

    // =============================================================================
    // ParallelHashJoin Implementation
    // =============================================================================

    ParallelHashJoin::ParallelHashJoin(const std::string& join_name)
        : join_name_(join_name), config_{}
    {
        // Use default configuration
    }

    ParallelHashJoin::ParallelHashJoin(const std::string& join_name, const HashJoinConfig& config)
        : join_name_(join_name), config_(config)
    {
        // Validate configuration
        if (config_.max_partitions == 0 || config_.min_partition_size == 0 ||
            config_.hash_table_memory_mb == 0 || config_.load_factor <= 0.0 ||
            config_.load_factor >= 1.0) {
            throw std::invalid_argument("Invalid ParallelHashJoin configuration");
        }
    }

    std::vector<ParallelHashJoin::JoinPartition>
    ParallelHashJoin::plan_join_partitions(std::uint64_t build_table_rows,
                                           std::uint64_t probe_table_rows,
                                           std::uint32_t target_workers)
    {
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        std::vector<JoinPartition> partitions;

        // Calculate optimal partition count based on build table size
        std::uint32_t optimal_partitions = std::min(
            target_workers,
            static_cast<std::uint32_t>(std::max(static_cast<std::uint64_t>(1),
                                                build_table_rows / config_.min_partition_size)));
        optimal_partitions = std::min(optimal_partitions, config_.max_partitions);

        // Create partitions
        std::uint64_t build_rows_per_partition = build_table_rows / optimal_partitions;
        std::uint64_t probe_rows_per_partition = probe_table_rows / optimal_partitions;

        for (std::uint32_t i = 0; i < optimal_partitions; ++i) {
            JoinPartition partition;
            partition.partition_id = i;

            // Build side ranges
            partition.start_row = i * build_rows_per_partition;
            partition.end_row = (i == optimal_partitions - 1) ? build_table_rows
                                                              : (i + 1) * build_rows_per_partition;
            partition.estimated_rows = partition.end_row - partition.start_row;

            // Estimate hash table size
            partition.estimated_build_size = estimate_hash_table_size(partition.estimated_rows);
            partition.estimated_probe_rows = probe_rows_per_partition;

            // Create partition key range
            std::ostringstream key_range;
            key_range << "partition_" << i << "_[" << partition.start_row << ","
                      << partition.end_row << ")";
            partition.partition_key_range = key_range.str();

            partitions.push_back(partition);
        }

        statistics_.total_partitions = optimal_partitions;
        return partitions;
    }

    std::vector<std::shared_ptr<WorkUnit>>
    ParallelHashJoin::create_build_work_units(const std::vector<JoinPartition>& partitions,
                                              const std::vector<std::string>& build_keys,
                                              const std::string& build_predicate)
    {
        std::vector<std::shared_ptr<WorkUnit>> work_units;
        work_units.reserve(partitions.size());

        for (const auto& partition : partitions) {
            auto work_unit = std::make_shared<WorkUnit>();
            work_unit->work_id = partition.partition_id;
            work_unit->partition_id = partition.partition_id;
            work_unit->start_offset = partition.start_row;
            work_unit->end_offset = partition.end_row;
            work_unit->estimated_rows = partition.estimated_rows;

            // Estimate resource requirements for build phase
            work_unit->estimated_memory_mb =
                static_cast<std::uint64_t>(partition.estimated_build_size / (1024 * 1024));
            work_unit->estimated_time =
                std::chrono::milliseconds(partition.estimated_rows / 1000); // ~1ms per 1K rows

            // Add build-specific context
            work_unit->context["operation"] = "hash_join_build";
            work_unit->context["join_name"] = join_name_;
            work_unit->context["predicate"] = build_predicate;

            // Store build keys
            std::ostringstream keys_stream;
            for (size_t i = 0; i < build_keys.size(); ++i) {
                if (i > 0)
                    keys_stream << ",";
                keys_stream << build_keys[i];
            }
            work_unit->context["build_keys"] = keys_stream.str();

            work_units.push_back(work_unit);
        }

        return work_units;
    }

    std::vector<std::shared_ptr<WorkUnit>>
    ParallelHashJoin::create_probe_work_units(const std::vector<JoinPartition>& partitions,
                                              const std::vector<std::string>& probe_keys,
                                              const std::string& probe_predicate)
    {
        std::vector<std::shared_ptr<WorkUnit>> work_units;
        work_units.reserve(partitions.size());

        for (const auto& partition : partitions) {
            auto work_unit = std::make_shared<WorkUnit>();
            work_unit->work_id = partition.partition_id + 1000; // Offset to distinguish from build
            work_unit->partition_id = partition.partition_id;
            work_unit->start_offset = 0; // Probe scans entire probe relation
            work_unit->end_offset = partition.estimated_probe_rows;
            work_unit->estimated_rows = partition.estimated_probe_rows;

            // Estimate resource requirements for probe phase
            work_unit->estimated_memory_mb = 16; // Probe phase is memory-light
            work_unit->estimated_time = std::chrono::milliseconds(partition.estimated_probe_rows /
                                                                  2000); // ~1ms per 2K rows

            // Add probe-specific context
            work_unit->context["operation"] = "hash_join_probe";
            work_unit->context["join_name"] = join_name_;
            work_unit->context["predicate"] = probe_predicate;
            work_unit->context["partition_id"] = std::to_string(partition.partition_id);

            // Store probe keys
            std::ostringstream keys_stream;
            for (size_t i = 0; i < probe_keys.size(); ++i) {
                if (i > 0)
                    keys_stream << ",";
                keys_stream << probe_keys[i];
            }
            work_unit->context["probe_keys"] = keys_stream.str();

            work_units.push_back(work_unit);
        }

        return work_units;
    }

    std::shared_ptr<WorkResult>
    ParallelHashJoin::execute_build_phase(const JoinPartition& partition,
                                          const std::vector<std::string>& build_keys,
                                          const std::string& build_predicate)
    {
        auto start_time = std::chrono::steady_clock::now();
        auto result = std::make_shared<WorkResult>();
        result->work_id = partition.partition_id;
        result->start_time = start_time;
        result->success = true;

        try {
            // Simulate building hash table for this partition
            std::uint64_t rows_processed = partition.estimated_rows;
            std::uint64_t hash_table_entries = 0;
            std::uint64_t collisions = 0;

            // Simulate hash table construction
            for (std::uint64_t i = 0; i < rows_processed; ++i) {
                // Simulate hash computation and insertion
                if (build_predicate.empty() || (i % 10) < 8) { // 80% pass predicate
                    hash_table_entries++;
                }
                if ((i % 100) < 5) { // 5% collision rate
                    collisions++;
                }
            }

            result->rows_processed = rows_processed;
            result->rows_returned = hash_table_entries;
            result->end_time = std::chrono::steady_clock::now();
            result->execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
                result->end_time - result->start_time);

            // Store build phase statistics
            result->custom_data["hash_table_entries"] = std::to_string(hash_table_entries);
            result->custom_data["hash_collisions"] = std::to_string(collisions);
            result->custom_data["build_memory_mb"] =
                std::to_string(partition.estimated_build_size / (1024 * 1024));

            // Update statistics
            {
                std::lock_guard<std::mutex> lock(statistics_mutex_);
                statistics_.build_rows_processed += rows_processed;
                statistics_.hash_collisions += collisions;
                statistics_.build_time += result->execution_time;
            }

        } catch (const std::exception& e) {
            result->success = false;
            result->error_message = "Build phase failed: " + std::string(e.what());
            result->end_time = std::chrono::steady_clock::now();
        }

        return result;
    }

    std::shared_ptr<WorkResult>
    ParallelHashJoin::execute_probe_phase(const JoinPartition& partition,
                                          const std::vector<std::string>& probe_keys,
                                          const std::string& probe_predicate)
    {
        auto start_time = std::chrono::steady_clock::now();
        auto result = std::make_shared<WorkResult>();
        result->work_id = partition.partition_id + 1000;
        result->start_time = start_time;
        result->success = true;

        try {
            // Simulate probing hash table for this partition
            std::uint64_t rows_processed = partition.estimated_probe_rows;
            std::uint64_t output_rows = 0;

            // Simulate hash table probing
            for (std::uint64_t i = 0; i < rows_processed; ++i) {
                // Simulate predicate evaluation
                bool passes_predicate = probe_predicate.empty() || (i % 10) < 7; // 70% pass

                if (passes_predicate) {
                    // Simulate hash lookup and join condition evaluation
                    bool hash_match = (i % 5) < 2; // 40% hash match rate
                    if (hash_match) {
                        output_rows++;
                    }
                }
            }

            result->rows_processed = rows_processed;
            result->rows_returned = output_rows;
            result->end_time = std::chrono::steady_clock::now();
            result->execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
                result->end_time - result->start_time);

            // Store probe phase statistics
            result->custom_data["output_rows"] = std::to_string(output_rows);
            result->custom_data["probe_efficiency"] =
                std::to_string(static_cast<double>(output_rows) / rows_processed);

            // Update statistics
            {
                std::lock_guard<std::mutex> lock(statistics_mutex_);
                statistics_.probe_rows_processed += rows_processed;
                statistics_.output_rows_generated += output_rows;
                statistics_.probe_time += result->execution_time;
            }

        } catch (const std::exception& e) {
            result->success = false;
            result->error_message = "Probe phase failed: " + std::string(e.what());
            result->end_time = std::chrono::steady_clock::now();
        }

        return result;
    }

    std::shared_ptr<WorkResult> ParallelHashJoin::merge_join_results(
        const std::vector<std::shared_ptr<WorkResult>>& build_results,
        const std::vector<std::shared_ptr<WorkResult>>& probe_results)
    {
        auto start_time = std::chrono::steady_clock::now();
        auto merged_result = std::make_shared<WorkResult>();
        merged_result->work_id = 0;
        merged_result->start_time = start_time;
        merged_result->success = true;

        try {
            std::uint64_t total_build_rows = 0;
            std::uint64_t total_probe_rows = 0;
            std::uint64_t total_output_rows = 0;
            std::uint64_t total_hash_collisions = 0;
            std::chrono::microseconds total_build_time{0};
            std::chrono::microseconds total_probe_time{0};
            bool any_failed = false;

            // Merge build results
            for (const auto& result : build_results) {
                if (!result->success) {
                    any_failed = true;
                    merged_result->error_message += result->error_message + "; ";
                    continue;
                }
                total_build_rows += result->rows_processed;
                if (result->custom_data.count("hash_collisions")) {
                    total_hash_collisions += std::stoull(result->custom_data.at("hash_collisions"));
                }
                total_build_time += result->execution_time;
            }

            // Merge probe results
            for (const auto& result : probe_results) {
                if (!result->success) {
                    any_failed = true;
                    merged_result->error_message += result->error_message + "; ";
                    continue;
                }
                total_probe_rows += result->rows_processed;
                total_output_rows += result->rows_returned;
                total_probe_time += result->execution_time;
            }

            merged_result->success = !any_failed;
            merged_result->rows_processed = total_build_rows + total_probe_rows;
            merged_result->rows_returned = total_output_rows;
            merged_result->end_time = std::chrono::steady_clock::now();
            merged_result->execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
                merged_result->end_time - merged_result->start_time);

            // Store comprehensive statistics
            merged_result->custom_data["total_build_rows"] = std::to_string(total_build_rows);
            merged_result->custom_data["total_probe_rows"] = std::to_string(total_probe_rows);
            merged_result->custom_data["total_output_rows"] = std::to_string(total_output_rows);
            merged_result->custom_data["total_hash_collisions"] =
                std::to_string(total_hash_collisions);
            merged_result->custom_data["build_time_us"] = std::to_string(total_build_time.count());
            merged_result->custom_data["probe_time_us"] = std::to_string(total_probe_time.count());

            double selectivity = (total_build_rows > 0)
                                     ? static_cast<double>(total_output_rows) / total_build_rows
                                     : 0.0;
            merged_result->custom_data["join_selectivity"] = std::to_string(selectivity);

        } catch (const std::exception& e) {
            merged_result->success = false;
            merged_result->error_message = "Result merging failed: " + std::string(e.what());
            merged_result->end_time = std::chrono::steady_clock::now();
        }

        return merged_result;
    }

    ParallelHashJoin::HashJoinStatistics ParallelHashJoin::get_statistics() const
    {
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        return statistics_;
    }

    void ParallelHashJoin::reset_statistics()
    {
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        statistics_ = HashJoinStatistics{};
    }

    std::uint64_t ParallelHashJoin::estimate_hash_table_size(std::uint64_t build_rows)
    {
        // Estimate hash table size including overhead
        std::uint64_t entry_size = 64; // Bytes per hash table entry (approximate)
        std::uint64_t base_size = build_rows * entry_size;
        std::uint64_t overhead =
            static_cast<std::uint64_t>(base_size / config_.load_factor) - base_size;
        return base_size + overhead;
    }

    double ParallelHashJoin::calculate_join_selectivity(const std::string& predicate)
    {
        // Simplified selectivity estimation - in real implementation would parse predicate
        if (predicate.empty()) {
            return 0.1; // Default 10% selectivity for joins without additional predicates
        }
        // More complex predicates typically have lower selectivity
        return 0.05;
    }

    // =============================================================================
    // ParallelNestedLoopJoin Implementation
    // =============================================================================

    ParallelNestedLoopJoin::ParallelNestedLoopJoin(const std::string& join_name)
        : join_name_(join_name), config_{}
    {
        // Use default configuration
    }

    ParallelNestedLoopJoin::ParallelNestedLoopJoin(const std::string& join_name,
                                                   const NestedLoopConfig& config)
        : join_name_(join_name), config_(config)
    {
        // Validate configuration
        if (config_.max_partitions == 0 || config_.min_outer_partition == 0 ||
            config_.inner_scan_batch_size == 0) {
            throw std::invalid_argument("Invalid ParallelNestedLoopJoin configuration");
        }
    }

    std::vector<ParallelNestedLoopJoin::NestedLoopPartition>
    ParallelNestedLoopJoin::plan_nested_loop_partitions(std::uint64_t outer_table_rows,
                                                        std::uint64_t inner_table_rows,
                                                        std::uint32_t target_workers)
    {
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        std::vector<NestedLoopPartition> partitions;

        // Calculate optimal partition count based on outer table size
        std::uint32_t optimal_partitions = std::min(
            target_workers,
            static_cast<std::uint32_t>(std::max(static_cast<std::uint64_t>(1),
                                                outer_table_rows / config_.min_outer_partition)));
        optimal_partitions = std::min(optimal_partitions, config_.max_partitions);

        // For nested loop joins, we partition the outer relation
        std::uint64_t outer_rows_per_partition = outer_table_rows / optimal_partitions;

        for (std::uint32_t i = 0; i < optimal_partitions; ++i) {
            NestedLoopPartition partition;
            partition.partition_id = i;

            // Outer relation ranges
            partition.outer_start_row = i * outer_rows_per_partition;
            partition.outer_end_row = (i == optimal_partitions - 1)
                                          ? outer_table_rows
                                          : (i + 1) * outer_rows_per_partition;
            partition.outer_rows = partition.outer_end_row - partition.outer_start_row;

            // Each partition will scan the entire inner table
            partition.inner_table_size = inner_table_rows;

            // Create partition-specific predicate if needed
            std::ostringstream partition_pred;
            partition_pred << "outer_partition_" << i << "_[" << partition.outer_start_row << ","
                           << partition.outer_end_row << ")";
            partition.partition_predicate = partition_pred.str();

            partitions.push_back(partition);
        }

        statistics_.total_partitions = optimal_partitions;
        return partitions;
    }

    std::vector<std::shared_ptr<WorkUnit>> ParallelNestedLoopJoin::create_nested_loop_work_units(
        const std::vector<NestedLoopPartition>& partitions, const std::string& join_predicate)
    {
        std::vector<std::shared_ptr<WorkUnit>> work_units;
        work_units.reserve(partitions.size());

        for (const auto& partition : partitions) {
            auto work_unit = std::make_shared<WorkUnit>();
            work_unit->work_id = partition.partition_id;
            work_unit->partition_id = partition.partition_id;
            work_unit->start_offset = partition.outer_start_row;
            work_unit->end_offset = partition.outer_end_row;
            work_unit->estimated_rows = partition.outer_rows;

            // Estimate resource requirements for nested loop join
            std::uint64_t estimated_inner_scans = estimate_inner_scans(partition.outer_rows);
            work_unit->estimated_memory_mb =
                std::max(static_cast<std::uint64_t>(32),
                         partition.inner_table_size / (1024 * 1024)); // At least 32MB
            work_unit->estimated_time =
                std::chrono::milliseconds(estimated_inner_scans / 100); // ~1ms per 100 inner scans

            // Add nested loop-specific context
            work_unit->context["operation"] = "nested_loop_join";
            work_unit->context["join_name"] = join_name_;
            work_unit->context["join_predicate"] = join_predicate;
            work_unit->context["outer_rows"] = std::to_string(partition.outer_rows);
            work_unit->context["inner_table_size"] = std::to_string(partition.inner_table_size);
            work_unit->context["estimated_inner_scans"] = std::to_string(estimated_inner_scans);

            // Additional context for optimization decisions
            work_unit->context["enable_index_lookup"] =
                config_.enable_index_lookup ? "true" : "false";
            work_unit->context["enable_caching"] = config_.enable_caching ? "true" : "false";
            work_unit->context["batch_size"] = std::to_string(config_.inner_scan_batch_size);

            work_units.push_back(work_unit);
        }

        return work_units;
    }

    std::shared_ptr<WorkResult>
    ParallelNestedLoopJoin::execute_nested_loop_partition(const NestedLoopPartition& partition,
                                                          const std::string& join_predicate)
    {
        auto start_time = std::chrono::steady_clock::now();
        auto result = std::make_shared<WorkResult>();
        result->work_id = partition.partition_id;
        result->start_time = start_time;
        result->success = true;

        try {
            std::uint64_t outer_rows_processed = 0;
            std::uint64_t inner_scans_performed = 0;
            std::uint64_t output_rows = 0;
            std::uint64_t cache_hits = 0;
            std::uint64_t cache_misses = 0;

            // Simulate nested loop join execution for this partition
            for (std::uint64_t outer_row = partition.outer_start_row;
                 outer_row < partition.outer_end_row; ++outer_row) {

                outer_rows_processed++;

                // Simulate inner table scan/lookup
                if (config_.enable_index_lookup && should_use_index_lookup(join_predicate)) {
                    // Index-based inner lookups - much more efficient
                    inner_scans_performed += 1; // Single index lookup

                    // Simulate index lookup success rate
                    if ((outer_row % 10) < 6) { // 60% hit rate for index lookups
                        if (config_.enable_caching && (outer_row % 100) < 80) {
                            cache_hits++;
                        } else {
                            cache_misses++;
                        }

                        // Simulate join predicate evaluation
                        if (join_predicate.empty() ||
                            (outer_row % 10) < 3) { // 30% join selectivity
                            output_rows++;
                        }
                    }
                } else {
                    // Full inner table scan for each outer row - expensive
                    std::uint64_t inner_batch_count =
                        (partition.inner_table_size + config_.inner_scan_batch_size - 1) /
                        config_.inner_scan_batch_size;

                    inner_scans_performed += inner_batch_count;

                    // Simulate batched inner table scanning
                    for (std::uint64_t batch = 0; batch < inner_batch_count; ++batch) {
                        std::uint64_t batch_start = batch * config_.inner_scan_batch_size;
                        std::uint64_t batch_end =
                            std::min(batch_start + config_.inner_scan_batch_size,
                                     partition.inner_table_size);

                        // Check for matches in this batch
                        for (std::uint64_t inner_row = batch_start; inner_row < batch_end;
                             ++inner_row) {
                            // Simulate join condition evaluation
                            if ((outer_row + inner_row) % 50 < 5) { // 10% match rate
                                if (join_predicate.empty() ||
                                    ((outer_row + inner_row) % 20) < 6) { // 30% predicate pass
                                    output_rows++;
                                }
                            }
                        }
                    }
                }

                // Simulate some processing delay
                if (outer_row % 1000 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }

            result->rows_processed = outer_rows_processed;
            result->rows_returned = output_rows;
            result->end_time = std::chrono::steady_clock::now();
            result->execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
                result->end_time - result->start_time);

            // Store nested loop join statistics
            result->custom_data["inner_scans_performed"] = std::to_string(inner_scans_performed);
            result->custom_data["output_rows"] = std::to_string(output_rows);
            result->custom_data["cache_hits"] = std::to_string(cache_hits);
            result->custom_data["cache_misses"] = std::to_string(cache_misses);

            double cache_hit_ratio =
                (cache_hits + cache_misses > 0)
                    ? static_cast<double>(cache_hits) / (cache_hits + cache_misses)
                    : 0.0;
            result->custom_data["cache_hit_ratio"] = std::to_string(cache_hit_ratio);

            double selectivity = (outer_rows_processed > 0)
                                     ? static_cast<double>(output_rows) / outer_rows_processed
                                     : 0.0;
            result->custom_data["join_selectivity"] = std::to_string(selectivity);

            // Update statistics
            {
                std::lock_guard<std::mutex> lock(statistics_mutex_);
                statistics_.outer_rows_processed += outer_rows_processed;
                statistics_.inner_scans_performed += inner_scans_performed;
                statistics_.output_rows_generated += output_rows;
                statistics_.total_time += result->execution_time;

                // Update cache hit ratio (weighted average)
                if (cache_hits + cache_misses > 0) {
                    statistics_.cache_hit_ratio =
                        (statistics_.cache_hit_ratio * (statistics_.total_partitions - 1) +
                         cache_hit_ratio) /
                        statistics_.total_partitions;
                }
            }

        } catch (const std::exception& e) {
            result->success = false;
            result->error_message = "Nested loop join partition failed: " + std::string(e.what());
            result->end_time = std::chrono::steady_clock::now();
        }

        return result;
    }

    std::shared_ptr<WorkResult> ParallelNestedLoopJoin::merge_nested_loop_results(
        const std::vector<std::shared_ptr<WorkResult>>& partition_results)
    {
        auto start_time = std::chrono::steady_clock::now();
        auto merged_result = std::make_shared<WorkResult>();
        merged_result->work_id = 0;
        merged_result->start_time = start_time;
        merged_result->success = true;

        try {
            std::uint64_t total_outer_rows = 0;
            std::uint64_t total_inner_scans = 0;
            std::uint64_t total_output_rows = 0;
            std::uint64_t total_cache_hits = 0;
            std::uint64_t total_cache_misses = 0;
            std::chrono::microseconds total_execution_time{0};
            bool any_failed = false;

            // Merge all partition results
            for (const auto& result : partition_results) {
                if (!result->success) {
                    any_failed = true;
                    merged_result->error_message += result->error_message + "; ";
                    continue;
                }

                total_outer_rows += result->rows_processed;
                total_output_rows += result->rows_returned;
                total_execution_time += result->execution_time;

                if (result->custom_data.count("inner_scans_performed")) {
                    total_inner_scans +=
                        std::stoull(result->custom_data.at("inner_scans_performed"));
                }
                if (result->custom_data.count("cache_hits")) {
                    total_cache_hits += std::stoull(result->custom_data.at("cache_hits"));
                }
                if (result->custom_data.count("cache_misses")) {
                    total_cache_misses += std::stoull(result->custom_data.at("cache_misses"));
                }
            }

            merged_result->success = !any_failed;
            merged_result->rows_processed = total_outer_rows;
            merged_result->rows_returned = total_output_rows;
            merged_result->end_time = std::chrono::steady_clock::now();
            merged_result->execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
                merged_result->end_time - merged_result->start_time);

            // Store comprehensive statistics
            merged_result->custom_data["total_outer_rows"] = std::to_string(total_outer_rows);
            merged_result->custom_data["total_inner_scans"] = std::to_string(total_inner_scans);
            merged_result->custom_data["total_output_rows"] = std::to_string(total_output_rows);
            merged_result->custom_data["total_cache_hits"] = std::to_string(total_cache_hits);
            merged_result->custom_data["total_cache_misses"] = std::to_string(total_cache_misses);
            merged_result->custom_data["total_execution_time_us"] =
                std::to_string(total_execution_time.count());

            double overall_cache_hit_ratio = (total_cache_hits + total_cache_misses > 0)
                                                 ? static_cast<double>(total_cache_hits) /
                                                       (total_cache_hits + total_cache_misses)
                                                 : 0.0;
            merged_result->custom_data["overall_cache_hit_ratio"] =
                std::to_string(overall_cache_hit_ratio);

            double overall_selectivity =
                (total_outer_rows > 0) ? static_cast<double>(total_output_rows) / total_outer_rows
                                       : 0.0;
            merged_result->custom_data["overall_join_selectivity"] =
                std::to_string(overall_selectivity);

            // Calculate efficiency metrics
            double inner_scans_per_outer_row =
                (total_outer_rows > 0) ? static_cast<double>(total_inner_scans) / total_outer_rows
                                       : 0.0;
            merged_result->custom_data["inner_scans_per_outer_row"] =
                std::to_string(inner_scans_per_outer_row);

        } catch (const std::exception& e) {
            merged_result->success = false;
            merged_result->error_message =
                "Nested loop result merging failed: " + std::string(e.what());
            merged_result->end_time = std::chrono::steady_clock::now();
        }

        return merged_result;
    }

    ParallelNestedLoopJoin::NestedLoopStatistics ParallelNestedLoopJoin::get_statistics() const
    {
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        return statistics_;
    }

    void ParallelNestedLoopJoin::reset_statistics()
    {
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        statistics_ = NestedLoopStatistics{};
    }

    std::uint64_t ParallelNestedLoopJoin::estimate_inner_scans(std::uint64_t outer_rows)
    {
        if (config_.enable_index_lookup) {
            // With index lookups, each outer row requires one index probe
            return outer_rows;
        } else {
            // Without indexes, each outer row requires a full inner table scan
            // But we can scan in batches to reduce overhead
            std::uint64_t batches_per_scan =
                (config_.inner_scan_batch_size > 0)
                    ? std::max(static_cast<std::uint64_t>(1), config_.inner_scan_batch_size)
                    : 1;
            return outer_rows * batches_per_scan;
        }
    }

    bool ParallelNestedLoopJoin::should_use_index_lookup(const std::string& predicate)
    {
        if (!config_.enable_index_lookup) {
            return false;
        }

        // Simplified heuristic - in real implementation would analyze predicate
        // for indexable conditions like equality comparisons
        return predicate.find("=") != std::string::npos ||
               predicate.find("IN") != std::string::npos ||
               predicate.find("BETWEEN") != std::string::npos;
    }

    // ParallelAggregation implementation

    ParallelAggregation::ParallelAggregation(const std::string& agg_name)
        : agg_name_(agg_name), config_()
    {
    }

    ParallelAggregation::ParallelAggregation(const std::string& agg_name,
                                             const AggregationConfig& config)
        : agg_name_(agg_name), config_(config)
    {
    }

    std::vector<ParallelAggregation::AggregationPartition>
    ParallelAggregation::plan_aggregation_partitions(
        std::uint64_t input_rows, const std::vector<std::string>& group_by_keys,
        const std::vector<std::string>& aggregate_exprs, std::uint32_t target_workers)
    {
        std::vector<AggregationPartition> partitions;

        if (input_rows == 0 || target_workers == 0) {
            return partitions;
        }

        // Calculate optimal number of partitions
        std::uint32_t num_partitions = std::min(config_.max_partitions, target_workers);
        std::uint64_t rows_per_partition =
            std::max(config_.min_partition_size, input_rows / num_partitions);

        // Adjust partition count based on actual data size
        if (input_rows < config_.min_partition_size * num_partitions) {
            num_partitions =
                std::max(static_cast<std::uint32_t>(1),
                         static_cast<std::uint32_t>(input_rows / config_.min_partition_size));
            rows_per_partition = input_rows / num_partitions;
        }

        partitions.reserve(num_partitions);

        std::uint64_t current_start = 0;
        for (std::uint32_t i = 0; i < num_partitions; ++i) {
            AggregationPartition partition;
            partition.partition_id = i;
            partition.start_row = current_start;
            partition.end_row =
                (i == num_partitions - 1) ? input_rows : current_start + rows_per_partition;
            partition.estimated_rows = partition.end_row - partition.start_row;
            partition.group_by_keys = group_by_keys;
            partition.aggregate_exprs = aggregate_exprs;

            // Estimate memory requirements
            std::uint64_t estimated_group_cardinality =
                estimate_group_cardinality(group_by_keys, partition.estimated_rows);
            partition.estimated_memory_mb =
                std::max(static_cast<std::uint64_t>(1),
                         (estimated_group_cardinality * 64) / (1024 * 1024)); // ~64 bytes per group

            // Create partition key range (simplified hash-based approach)
            std::ostringstream range_stream;
            range_stream << "partition_" << i << "_hash_" << std::hex
                         << std::hash<std::string>{}(std::to_string(i));
            partition.partition_key_range = range_stream.str();

            partitions.push_back(std::move(partition));
            current_start = partition.end_row;
        }

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(statistics_mutex_);
            statistics_.total_partitions = num_partitions;
        }

        return partitions;
    }

    std::vector<std::shared_ptr<WorkUnit>>
    ParallelAggregation::create_partial_aggregation_work_units(
        const std::vector<AggregationPartition>& partitions)
    {
        std::vector<std::shared_ptr<WorkUnit>> work_units;
        work_units.reserve(partitions.size());

        for (const auto& partition : partitions) {
            auto work_unit = std::make_shared<WorkUnit>();
            work_unit->work_id = static_cast<std::uint64_t>(partition.partition_id) +
                                 (static_cast<std::uint64_t>(1)
                                  << 32); // Offset to distinguish from other work types
            work_unit->partition_id = partition.partition_id;
            work_unit->start_offset = partition.start_row;
            work_unit->end_offset = partition.end_row;
            work_unit->estimated_rows = partition.estimated_rows;
            work_unit->estimated_memory_mb = partition.estimated_memory_mb;
            work_unit->estimated_time = std::chrono::milliseconds(
                std::max(static_cast<std::uint64_t>(10),
                         partition.estimated_rows / 1000)); // ~1ms per 1000 rows

            // Store aggregation-specific data
            work_unit->custom_data["operation_type"] = "partial_aggregation";
            work_unit->custom_data["partition_key_range"] = partition.partition_key_range;
            work_unit->custom_data["group_by_count"] =
                std::to_string(partition.group_by_keys.size());
            work_unit->custom_data["aggregate_count"] =
                std::to_string(partition.aggregate_exprs.size());

            // Serialize group by keys and aggregate expressions
            std::ostringstream group_by_stream;
            for (std::size_t i = 0; i < partition.group_by_keys.size(); ++i) {
                if (i > 0)
                    group_by_stream << ",";
                group_by_stream << partition.group_by_keys[i];
            }
            work_unit->custom_data["group_by_keys"] = group_by_stream.str();

            std::ostringstream agg_stream;
            for (std::size_t i = 0; i < partition.aggregate_exprs.size(); ++i) {
                if (i > 0)
                    agg_stream << ",";
                agg_stream << partition.aggregate_exprs[i];
            }
            work_unit->custom_data["aggregate_exprs"] = agg_stream.str();

            work_units.push_back(work_unit);
        }

        return work_units;
    }

    std::vector<std::shared_ptr<WorkUnit>> ParallelAggregation::create_final_aggregation_work_units(
        const std::vector<AggregationPartition>& partitions)
    {
        std::vector<std::shared_ptr<WorkUnit>> work_units;
        work_units.reserve(partitions.size());

        for (const auto& partition : partitions) {
            auto work_unit = std::make_shared<WorkUnit>();
            work_unit->work_id =
                static_cast<std::uint64_t>(partition.partition_id) +
                (static_cast<std::uint64_t>(2) << 32); // Offset for final aggregation
            work_unit->partition_id = partition.partition_id;
            work_unit->start_offset = partition.start_row;
            work_unit->end_offset = partition.end_row;
            work_unit->estimated_rows = partition.estimated_rows;
            work_unit->estimated_memory_mb =
                partition.estimated_memory_mb / 2; // Final agg typically smaller
            work_unit->estimated_time = std::chrono::milliseconds(
                std::max(static_cast<std::uint64_t>(5),
                         partition.estimated_rows / 2000)); // Final agg faster

            // Store final aggregation data
            work_unit->custom_data["operation_type"] = "final_aggregation";
            work_unit->custom_data["partition_key_range"] = partition.partition_key_range;

            work_units.push_back(work_unit);
        }

        return work_units;
    }

    std::shared_ptr<WorkResult>
    ParallelAggregation::execute_partial_aggregation(const AggregationPartition& partition)
    {
        auto result = std::make_shared<WorkResult>();
        result->work_id = partition.partition_id;
        result->start_time = std::chrono::steady_clock::now();

        try {
            // Simulate partial aggregation processing
            std::this_thread::sleep_for(
                std::chrono::milliseconds(1 + partition.estimated_rows / 10000));

            // Mock partial aggregation: create groups and partial results
            std::uint64_t estimated_groups =
                estimate_group_cardinality(partition.group_by_keys, partition.estimated_rows);

            // Create mock result data
            std::ostringstream result_stream;
            result_stream << "partial_agg_result_partition_" << partition.partition_id;
            std::string result_string = result_stream.str();
            result->result_data.assign(result_string.begin(), result_string.end());

            // Update statistics
            {
                std::lock_guard<std::mutex> lock(statistics_mutex_);
                statistics_.input_rows_processed += partition.estimated_rows;
                statistics_.partial_groups_created += estimated_groups;
            }

            result->success = true;
            result->rows_processed = partition.estimated_rows;
            result->rows_returned = estimated_groups;
            result->memory_used_mb =
                std::min(static_cast<std::uint64_t>(partition.estimated_memory_mb),
                         config_.hash_table_memory_mb);

            // Store aggregation-specific statistics
            result->statistics["groups_created"] = estimated_groups;
            result->statistics["hash_collisions"] = estimated_groups / 10; // Mock collision count
            result->statistics["memory_peak_mb"] = result->memory_used_mb;

        } catch (const std::exception& e) {
            result->success = false;
            result->error_message = "Partial aggregation failed for partition " +
                                    std::to_string(partition.partition_id) + ": " +
                                    std::string(e.what());
        }

        result->end_time = std::chrono::steady_clock::now();
        result->execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
            result->end_time - result->start_time);

        // Update timing statistics
        {
            std::lock_guard<std::mutex> lock(statistics_mutex_);
            statistics_.partial_agg_time += result->execution_time;
        }

        return result;
    }

    std::shared_ptr<WorkResult> ParallelAggregation::execute_final_aggregation(
        const AggregationPartition& partition,
        const std::vector<std::shared_ptr<WorkResult>>& partial_results)
    {
        auto result = std::make_shared<WorkResult>();
        result->work_id = partition.partition_id + (static_cast<std::uint64_t>(2) << 32);
        result->start_time = std::chrono::steady_clock::now();

        try {
            // Simulate final aggregation processing
            std::uint64_t total_partial_groups = 0;
            for (const auto& partial_result : partial_results) {
                if (partial_result && partial_result->success) {
                    total_partial_groups += partial_result->rows_returned;
                }
            }

            // Simulate merging partial results
            std::this_thread::sleep_for(std::chrono::milliseconds(1 + total_partial_groups / 5000));

            // Estimate final group count (typically smaller due to consolidation)
            std::uint64_t final_groups =
                std::max(static_cast<std::uint64_t>(1), total_partial_groups * 70 / 100);

            // Create mock result data
            std::ostringstream result_stream;
            result_stream << "final_agg_result_partition_" << partition.partition_id << "_groups_"
                          << final_groups;
            std::string result_string = result_stream.str();
            result->result_data.assign(result_string.begin(), result_string.end());

            // Update statistics
            {
                std::lock_guard<std::mutex> lock(statistics_mutex_);
                statistics_.final_groups_output += final_groups;
            }

            result->success = true;
            result->rows_processed = total_partial_groups;
            result->rows_returned = final_groups;
            result->memory_used_mb =
                std::min(partition.estimated_memory_mb / 2, config_.hash_table_memory_mb);

            // Store final aggregation statistics
            result->statistics["final_groups"] = final_groups;
            result->statistics["consolidation_ratio"] =
                static_cast<std::uint64_t>((total_partial_groups * 100) /
                                           std::max(final_groups, static_cast<std::uint64_t>(1)));

        } catch (const std::exception& e) {
            result->success = false;
            result->error_message = "Final aggregation failed for partition " +
                                    std::to_string(partition.partition_id) + ": " +
                                    std::string(e.what());
        }

        result->end_time = std::chrono::steady_clock::now();
        result->execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
            result->end_time - result->start_time);

        // Update timing statistics
        {
            std::lock_guard<std::mutex> lock(statistics_mutex_);
            statistics_.final_agg_time += result->execution_time;
        }

        return result;
    }

    std::shared_ptr<WorkResult> ParallelAggregation::merge_aggregation_results(
        const std::vector<std::shared_ptr<WorkResult>>& partial_results,
        const std::vector<std::string>& group_by_keys,
        const std::vector<std::string>& aggregate_exprs)
    {
        auto merged_result = std::make_shared<WorkResult>();
        merged_result->work_id = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(this));
        merged_result->start_time = std::chrono::steady_clock::now();

        try {
            std::uint64_t total_groups = 0;
            std::uint64_t total_memory = 0;
            std::chrono::microseconds total_execution_time{0};

            // Merge results from all partitions
            for (const auto& result : partial_results) {
                if (result && result->success) {
                    total_groups += result->rows_returned;
                    total_memory += result->memory_used_mb;
                    total_execution_time += result->execution_time;
                }
            }

            // Create consolidated result data
            std::ostringstream merged_stream;
            merged_stream << "merged_aggregation_result_groups_" << total_groups << "_keys_"
                          << group_by_keys.size() << "_aggregates_" << aggregate_exprs.size();
            std::string merged_string = merged_stream.str();
            merged_result->result_data.assign(merged_string.begin(), merged_string.end());

            merged_result->success = true;
            merged_result->rows_returned = total_groups;
            merged_result->memory_used_mb = total_memory;
            merged_result->execution_time = total_execution_time;

            // Store merge statistics
            merged_result->statistics["total_partitions_merged"] = partial_results.size();
            merged_result->statistics["total_final_groups"] = total_groups;
            merged_result->statistics["average_groups_per_partition"] =
                partial_results.empty() ? 0 : total_groups / partial_results.size();

        } catch (const std::exception& e) {
            merged_result->success = false;
            merged_result->error_message =
                "Aggregation result merging failed: " + std::string(e.what());
        }

        merged_result->end_time = std::chrono::steady_clock::now();
        return merged_result;
    }

    ParallelAggregation::AggregationStatistics ParallelAggregation::get_statistics() const
    {
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        return statistics_;
    }

    void ParallelAggregation::reset_statistics()
    {
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        statistics_ = AggregationStatistics{};
    }

    std::uint64_t
    ParallelAggregation::estimate_group_cardinality(const std::vector<std::string>& group_by_keys,
                                                    std::uint64_t input_rows)
    {
        if (group_by_keys.empty()) {
            return 1; // No GROUP BY means single result group
        }

        // Simplified heuristic: assume each group by key reduces cardinality
        double reduction_factor = 0.1; // 10% unique values per key on average
        std::uint64_t estimated_cardinality = input_rows;

        for (std::size_t i = 0; i < group_by_keys.size(); ++i) {
            estimated_cardinality =
                static_cast<std::uint64_t>(estimated_cardinality * reduction_factor);
            reduction_factor = std::max(0.01, reduction_factor * 0.8); // Diminishing returns
        }

        return std::max(static_cast<std::uint64_t>(1), std::min(estimated_cardinality, input_rows));
    }

    double ParallelAggregation::calculate_aggregation_selectivity(
        const std::vector<std::string>& aggregate_exprs)
    {
        // Simplified selectivity calculation based on aggregate function types
        if (aggregate_exprs.empty()) {
            return 1.0;
        }

        double selectivity = 1.0;
        for (const auto& expr : aggregate_exprs) {
            // Very simplified heuristic
            if (expr.find("COUNT") != std::string::npos) {
                selectivity *= 0.9; // COUNT typically has high selectivity
            } else if (expr.find("SUM") != std::string::npos ||
                       expr.find("AVG") != std::string::npos) {
                selectivity *= 0.8; // SUM/AVG medium selectivity
            } else {
                selectivity *= 0.7; // Other aggregates
            }
        }

        return std::max(0.01, selectivity);
    }

    bool ParallelAggregation::should_use_two_phase_aggregation(std::uint64_t input_rows,
                                                               double group_cardinality)
    {
        if (!config_.enable_two_phase_agg) {
            return false;
        }

        // Use two-phase if we have lots of input rows and moderate group cardinality
        return (input_rows >= 100000) && (group_cardinality <= config_.group_cardinality_threshold);
    }

    // ParallelSort implementation

    ParallelSort::ParallelSort(const std::string& sort_name) : sort_name_(sort_name), config_() {}

    ParallelSort::ParallelSort(const std::string& sort_name, const SortConfig& config)
        : sort_name_(sort_name), config_(config)
    {
    }

    std::vector<ParallelSort::SortPartition> ParallelSort::plan_sort_partitions(
        std::uint64_t input_rows, const std::vector<std::string>& sort_keys,
        const std::vector<bool>& sort_ascending, std::uint32_t target_workers)
    {
        std::vector<SortPartition> partitions;

        if (input_rows == 0 || target_workers == 0 || sort_keys.empty()) {
            return partitions;
        }

        // Calculate optimal number of partitions
        std::uint32_t num_partitions = std::min(config_.max_partitions, target_workers);
        std::uint64_t rows_per_partition =
            std::max(config_.min_partition_size, input_rows / num_partitions);

        // Adjust partition count based on actual data size
        if (input_rows < config_.min_partition_size * num_partitions) {
            num_partitions =
                std::max(static_cast<std::uint32_t>(1),
                         static_cast<std::uint32_t>(input_rows / config_.min_partition_size));
            rows_per_partition = input_rows / num_partitions;
        }

        partitions.reserve(num_partitions);

        // Generate partition key ranges (simplified approach)
        std::vector<std::string> key_ranges =
            determine_partition_ranges(sort_keys, input_rows, num_partitions);

        std::uint64_t current_start = 0;
        for (std::uint32_t i = 0; i < num_partitions; ++i) {
            SortPartition partition;
            partition.partition_id = i;
            partition.start_row = current_start;
            partition.end_row =
                (i == num_partitions - 1) ? input_rows : current_start + rows_per_partition;
            partition.estimated_rows = partition.end_row - partition.start_row;
            partition.sort_keys = sort_keys;
            partition.sort_ascending = sort_ascending;

            // Estimate memory requirements
            partition.estimated_memory_mb =
                estimate_sort_memory(partition.estimated_rows, sort_keys);

            // Set key ranges (simplified)
            if (i < key_ranges.size()) {
                partition.key_range_min = key_ranges[i];
                partition.key_range_max =
                    (i + 1 < key_ranges.size()) ? key_ranges[i + 1] : "MAX_VALUE";
            } else {
                partition.key_range_min = "range_" + std::to_string(i);
                partition.key_range_max = "range_" + std::to_string(i + 1);
            }

            partitions.push_back(std::move(partition));
            current_start = partition.end_row;
        }

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(statistics_mutex_);
            statistics_.total_partitions = num_partitions;
            statistics_.average_partition_size_mb =
                partitions.empty()
                    ? 0.0
                    : static_cast<double>(std::accumulate(
                          partitions.begin(), partitions.end(), static_cast<std::uint64_t>(0),
                          [](std::uint64_t sum, const SortPartition& p) {
                              return sum + p.estimated_memory_mb;
                          })) /
                          partitions.size();
        }

        return partitions;
    }

    std::vector<std::shared_ptr<WorkUnit>>
    ParallelSort::create_local_sort_work_units(const std::vector<SortPartition>& partitions)
    {
        std::vector<std::shared_ptr<WorkUnit>> work_units;
        work_units.reserve(partitions.size());

        for (const auto& partition : partitions) {
            auto work_unit = std::make_shared<WorkUnit>();
            work_unit->work_id =
                static_cast<std::uint64_t>(partition.partition_id) +
                (static_cast<std::uint64_t>(3) << 32); // Offset for sort operations
            work_unit->partition_id = partition.partition_id;
            work_unit->start_offset = partition.start_row;
            work_unit->end_offset = partition.end_row;
            work_unit->estimated_rows = partition.estimated_rows;
            work_unit->estimated_memory_mb = partition.estimated_memory_mb;

            // Estimate sort time (O(n log n) complexity)
            std::uint64_t log_n = partition.estimated_rows > 0
                                      ? static_cast<std::uint64_t>(std::log2(
                                            static_cast<double>(partition.estimated_rows)))
                                      : 1;
            work_unit->estimated_time = std::chrono::milliseconds(std::max(
                static_cast<std::uint64_t>(10), (partition.estimated_rows * log_n) / 10000));

            // Store sort-specific data
            work_unit->custom_data["operation_type"] = "local_sort";
            work_unit->custom_data["key_range_min"] = partition.key_range_min;
            work_unit->custom_data["key_range_max"] = partition.key_range_max;
            work_unit->custom_data["sort_key_count"] = std::to_string(partition.sort_keys.size());

            // Serialize sort keys and directions
            std::ostringstream keys_stream, dirs_stream;
            for (std::size_t i = 0; i < partition.sort_keys.size(); ++i) {
                if (i > 0) {
                    keys_stream << ",";
                    dirs_stream << ",";
                }
                keys_stream << partition.sort_keys[i];
                dirs_stream << (i < partition.sort_ascending.size() && partition.sort_ascending[i]
                                    ? "ASC"
                                    : "DESC");
            }
            work_unit->custom_data["sort_keys"] = keys_stream.str();
            work_unit->custom_data["sort_directions"] = dirs_stream.str();

            work_units.push_back(work_unit);
        }

        return work_units;
    }

    std::vector<std::shared_ptr<WorkUnit>>
    ParallelSort::create_merge_work_units(const std::vector<SortPartition>& partitions,
                                          std::uint32_t merge_level)
    {
        std::vector<std::shared_ptr<WorkUnit>> work_units;

        // Create merge work units based on merge level
        std::uint32_t partitions_per_merge =
            std::max(static_cast<std::uint32_t>(2), config_.max_merge_ways);
        std::uint32_t merge_groups =
            (partitions.size() + partitions_per_merge - 1) / partitions_per_merge;

        work_units.reserve(merge_groups);

        for (std::uint32_t i = 0; i < merge_groups; ++i) {
            auto work_unit = std::make_shared<WorkUnit>();
            work_unit->work_id =
                static_cast<std::uint64_t>(i) +
                (static_cast<std::uint64_t>(4 + merge_level) << 32); // Offset for merge operations
            work_unit->partition_id = i;

            // Calculate range of partitions to merge
            std::uint32_t start_partition = i * partitions_per_merge;
            std::uint32_t end_partition = std::min(static_cast<std::uint32_t>(partitions.size()),
                                                   (i + 1) * partitions_per_merge);

            std::uint64_t total_rows = 0;
            std::uint64_t total_memory = 0;
            for (std::uint32_t j = start_partition; j < end_partition; ++j) {
                total_rows += partitions[j].estimated_rows;
                total_memory += partitions[j].estimated_memory_mb;
            }

            work_unit->estimated_rows = total_rows;
            work_unit->estimated_memory_mb = total_memory;
            work_unit->estimated_time = std::chrono::milliseconds(std::max(
                static_cast<std::uint64_t>(50), total_rows / 1000)); // Merge is typically faster

            // Store merge-specific data
            work_unit->custom_data["operation_type"] = "merge_sort";
            work_unit->custom_data["merge_level"] = std::to_string(merge_level);
            work_unit->custom_data["partitions_to_merge"] =
                std::to_string(end_partition - start_partition);
            work_unit->custom_data["start_partition"] = std::to_string(start_partition);
            work_unit->custom_data["end_partition"] = std::to_string(end_partition);

            work_units.push_back(work_unit);
        }

        return work_units;
    }

    std::shared_ptr<WorkResult> ParallelSort::execute_local_sort(const SortPartition& partition)
    {
        auto result = std::make_shared<WorkResult>();
        result->work_id = partition.partition_id;
        result->start_time = std::chrono::steady_clock::now();

        try {
            // Simulate local sorting
            std::uint64_t log_n = partition.estimated_rows > 0
                                      ? static_cast<std::uint64_t>(std::log2(
                                            static_cast<double>(partition.estimated_rows)))
                                      : 1;
            std::uint64_t estimated_comparisons = partition.estimated_rows * log_n;

            // Choose sort algorithm based on partition size
            bool use_quicksort = partition.estimated_rows <= config_.quicksort_threshold &&
                                 config_.enable_quicksort_hybrid;

            // Simulate sorting time
            std::this_thread::sleep_for(
                std::chrono::milliseconds(1 + partition.estimated_rows / 5000));

            // Create mock result data
            std::ostringstream result_stream;
            result_stream << "sorted_partition_" << partition.partition_id << "_rows_"
                          << partition.estimated_rows << "_algorithm_"
                          << (use_quicksort ? "quicksort" : "mergesort");
            std::string result_string = result_stream.str();
            result->result_data.assign(result_string.begin(), result_string.end());

            // Update statistics
            {
                std::lock_guard<std::mutex> lock(statistics_mutex_);
                statistics_.input_rows_processed += partition.estimated_rows;
                statistics_.comparisons_performed += estimated_comparisons;
            }

            result->success = true;
            result->rows_processed = partition.estimated_rows;
            result->rows_returned = partition.estimated_rows; // Same number of rows after sorting
            result->memory_used_mb = partition.estimated_memory_mb;

            // Store sort-specific statistics
            result->statistics["comparisons_performed"] = estimated_comparisons;
            result->statistics["sort_algorithm"] =
                use_quicksort ? 1 : 0; // 1 = quicksort, 0 = mergesort
            result->statistics["memory_peak_mb"] = result->memory_used_mb;

        } catch (const std::exception& e) {
            result->success = false;
            result->error_message = "Local sorting failed for partition " +
                                    std::to_string(partition.partition_id) + ": " +
                                    std::string(e.what());
        }

        result->end_time = std::chrono::steady_clock::now();
        result->execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
            result->end_time - result->start_time);

        // Update timing statistics
        {
            std::lock_guard<std::mutex> lock(statistics_mutex_);
            statistics_.local_sort_time += result->execution_time;
        }

        return result;
    }

    std::shared_ptr<WorkResult>
    ParallelSort::execute_merge_phase(const std::vector<SortPartition>& partitions,
                                      const std::vector<std::shared_ptr<WorkResult>>& local_results,
                                      std::uint32_t merge_level)
    {
        auto result = std::make_shared<WorkResult>();
        result->work_id =
            static_cast<std::uint64_t>(merge_level) + (static_cast<std::uint64_t>(5) << 32);
        result->start_time = std::chrono::steady_clock::now();

        try {
            std::uint64_t total_rows = 0;
            std::uint64_t total_memory = 0;
            std::uint64_t total_comparisons = 0;

            // Simulate merging process
            for (const auto& local_result : local_results) {
                if (local_result && local_result->success) {
                    total_rows += local_result->rows_processed;
                    total_memory += local_result->memory_used_mb;
                    if (local_result->statistics.count("comparisons_performed")) {
                        total_comparisons += local_result->statistics.at("comparisons_performed");
                    }
                }
            }

            // Estimate merge comparisons (N-way merge complexity)
            std::uint32_t merge_ways =
                std::min(static_cast<std::uint32_t>(local_results.size()), config_.max_merge_ways);
            std::uint64_t merge_comparisons =
                total_rows * static_cast<std::uint64_t>(std::log2(static_cast<double>(merge_ways)));

            // Simulate merge time
            std::this_thread::sleep_for(std::chrono::milliseconds(5 + total_rows / 10000));

            // Create mock result data
            std::ostringstream result_stream;
            result_stream << "merged_sort_level_" << merge_level << "_total_rows_" << total_rows
                          << "_merge_ways_" << merge_ways;
            std::string result_string = result_stream.str();
            result->result_data.assign(result_string.begin(), result_string.end());

            // Update statistics
            {
                std::lock_guard<std::mutex> lock(statistics_mutex_);
                statistics_.comparisons_performed += merge_comparisons;
                statistics_.merge_passes++;
            }

            result->success = true;
            result->rows_processed = total_rows;
            result->rows_returned = total_rows;
            result->memory_used_mb = total_memory;

            // Store merge-specific statistics
            result->statistics["merge_ways"] = merge_ways;
            result->statistics["merge_comparisons"] = merge_comparisons;
            result->statistics["merge_level"] = merge_level;
            result->statistics["partitions_merged"] = local_results.size();

        } catch (const std::exception& e) {
            result->success = false;
            result->error_message = "Sort merge phase " + std::to_string(merge_level) +
                                    " failed: " + std::string(e.what());
        }

        result->end_time = std::chrono::steady_clock::now();
        result->execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
            result->end_time - result->start_time);

        // Update timing statistics
        {
            std::lock_guard<std::mutex> lock(statistics_mutex_);
            statistics_.merge_time += result->execution_time;
        }

        return result;
    }

    std::shared_ptr<WorkResult> ParallelSort::merge_sorted_results(
        const std::vector<std::shared_ptr<WorkResult>>& sorted_results,
        const std::vector<std::string>& sort_keys, const std::vector<bool>& sort_ascending)
    {
        auto merged_result = std::make_shared<WorkResult>();
        merged_result->work_id = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(this));
        merged_result->start_time = std::chrono::steady_clock::now();

        try {
            std::uint64_t total_rows = 0;
            std::uint64_t total_memory = 0;
            std::uint64_t total_comparisons = 0;
            std::chrono::microseconds total_execution_time{0};

            // Merge results from all partitions
            for (const auto& result : sorted_results) {
                if (result && result->success) {
                    total_rows += result->rows_returned;
                    total_memory += result->memory_used_mb;
                    total_execution_time += result->execution_time;

                    if (result->statistics.count("comparisons_performed")) {
                        total_comparisons += result->statistics.at("comparisons_performed");
                    }
                }
            }

            // Final multi-way merge
            std::uint32_t final_merge_ways =
                std::min(static_cast<std::uint32_t>(sorted_results.size()), config_.max_merge_ways);
            std::uint64_t final_comparisons =
                total_rows *
                static_cast<std::uint64_t>(std::log2(static_cast<double>(final_merge_ways)));

            // Create consolidated result data
            std::ostringstream merged_stream;
            merged_stream << "final_sorted_result_rows_" << total_rows << "_keys_"
                          << sort_keys.size() << "_comparisons_"
                          << (total_comparisons + final_comparisons);
            std::string merged_string = merged_stream.str();
            merged_result->result_data.assign(merged_string.begin(), merged_string.end());

            merged_result->success = true;
            merged_result->rows_returned = total_rows;
            merged_result->memory_used_mb = total_memory;
            merged_result->execution_time = total_execution_time;

            // Store final merge statistics
            merged_result->statistics["total_partitions_merged"] = sorted_results.size();
            merged_result->statistics["total_comparisons"] = total_comparisons + final_comparisons;
            merged_result->statistics["final_merge_ways"] = final_merge_ways;
            merged_result->statistics["sort_key_count"] = sort_keys.size();

            // Update final statistics
            {
                std::lock_guard<std::mutex> lock(statistics_mutex_);
                statistics_.output_rows_generated = total_rows;
                statistics_.comparisons_performed += final_comparisons;
            }

        } catch (const std::exception& e) {
            merged_result->success = false;
            merged_result->error_message = "Sort result merging failed: " + std::string(e.what());
        }

        merged_result->end_time = std::chrono::steady_clock::now();
        return merged_result;
    }

    ParallelSort::SortStatistics ParallelSort::get_statistics() const
    {
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        return statistics_;
    }

    void ParallelSort::reset_statistics()
    {
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        statistics_ = SortStatistics{};
    }

    std::uint64_t ParallelSort::estimate_sort_memory(std::uint64_t input_rows,
                                                     const std::vector<std::string>& sort_keys)
    {
        if (input_rows == 0)
            return 1;

        // Estimate memory based on row count and key complexity
        // Assume average row size of 100 bytes + key overhead
        std::uint64_t base_memory_per_row = 100 + (sort_keys.size() * 20); // 20 bytes per key
        std::uint64_t total_memory_bytes = input_rows * base_memory_per_row;

        // Add overhead for sorting algorithm (typically 2x for merge sort)
        total_memory_bytes *= 2;

        // Convert to MB and ensure minimum
        std::uint64_t memory_mb =
            std::max(static_cast<std::uint64_t>(1), total_memory_bytes / (1024 * 1024));

        return std::min(memory_mb, config_.sort_memory_mb);
    }

    bool ParallelSort::should_use_external_sort(std::uint64_t estimated_memory_mb)
    {
        return config_.enable_external_sort && (estimated_memory_mb > config_.sort_memory_mb);
    }

    std::uint32_t ParallelSort::calculate_optimal_merge_ways(std::uint32_t num_partitions,
                                                             std::uint64_t available_memory_mb)
    {
        if (num_partitions <= 1)
            return 1;

        // Balance between merge ways and memory usage
        std::uint32_t max_ways = std::min(config_.max_merge_ways, num_partitions);

        // Ensure we don't exceed memory limits
        std::uint64_t memory_per_way = available_memory_mb / max_ways;
        while (max_ways > 2 && memory_per_way < 10) { // Minimum 10MB per merge way
            max_ways--;
            memory_per_way = available_memory_mb / max_ways;
        }

        return std::max(static_cast<std::uint32_t>(2), max_ways);
    }

    std::vector<std::string>
    ParallelSort::determine_partition_ranges(const std::vector<std::string>& sort_keys,
                                             std::uint64_t input_rows,
                                             std::uint32_t target_partitions)
    {
        std::vector<std::string> ranges;
        ranges.reserve(target_partitions + 1);

        // Simplified range determination - in a real implementation, this would
        // analyze actual data distribution or use sampling
        for (std::uint32_t i = 0; i <= target_partitions; ++i) {
            std::ostringstream range_stream;
            range_stream << "key_range_" << std::hex << (i * 0x10000000ULL / target_partitions);
            ranges.push_back(range_stream.str());
        }

        return ranges;
    }

    // ParallelQueryExecutor integration methods

    std::shared_ptr<WorkResult>
    ParallelQueryExecutor::execute_parallel_aggregate(std::shared_ptr<Aggregate> agg_node,
                                                      std::uint32_t worker_count)
    {
        if (!agg_node || worker_count == 0) {
            auto error_result = std::make_shared<WorkResult>();
            error_result->success = false;
            error_result->error_message = "Invalid aggregation node or worker count";
            return error_result;
        }

        // Create parallel aggregation instance
        std::string agg_name =
            "parallel_agg_" + std::to_string(reinterpret_cast<std::uintptr_t>(agg_node.get()));
        ParallelAggregation parallel_agg(agg_name);

        // Mock aggregation parameters (in real implementation, these would come from agg_node)
        std::uint64_t estimated_input_rows = 100000; // Mock value
        std::vector<std::string> group_by_keys = {"customer_id", "product_category"};
        std::vector<std::string> aggregate_exprs = {"SUM(amount)", "COUNT(*)", "AVG(price)"};

        try {
            // Plan aggregation partitions
            auto partitions = parallel_agg.plan_aggregation_partitions(
                estimated_input_rows, group_by_keys, aggregate_exprs, worker_count);
            if (partitions.empty()) {
                auto error_result = std::make_shared<WorkResult>();
                error_result->success = false;
                error_result->error_message = "Failed to create aggregation partitions";
                return error_result;
            }

            // Create and execute partial aggregation work units
            auto partial_work_units =
                parallel_agg.create_partial_aggregation_work_units(partitions);
            std::vector<std::shared_ptr<WorkResult>> partial_results;
            partial_results.reserve(partial_work_units.size());

            for (const auto& partition : partitions) {
                auto partial_result = parallel_agg.execute_partial_aggregation(partition);
                partial_results.push_back(partial_result);
            }

            // Execute final aggregation if two-phase is enabled
            std::vector<std::shared_ptr<WorkResult>> final_results;
            if (parallel_agg.should_use_two_phase_aggregation(
                    estimated_input_rows,
                    parallel_agg.estimate_group_cardinality(group_by_keys, estimated_input_rows))) {

                for (const auto& partition : partitions) {
                    auto final_result = parallel_agg.execute_final_aggregation(
                        partition, {partial_results[partition.partition_id]});
                    final_results.push_back(final_result);
                }
            } else {
                final_results = partial_results;
            }

            // Merge all results
            auto merged_result = parallel_agg.merge_aggregation_results(
                final_results, group_by_keys, aggregate_exprs);

            if (merged_result) {
                // Add parallel execution statistics
                merged_result->custom_data["parallel_workers"] = std::to_string(worker_count);
                merged_result->custom_data["aggregation_partitions"] =
                    std::to_string(partitions.size());
                merged_result->custom_data["operation_type"] = "parallel_aggregation";
                merged_result->custom_data["group_by_keys_count"] =
                    std::to_string(group_by_keys.size());
                merged_result->custom_data["aggregate_exprs_count"] =
                    std::to_string(aggregate_exprs.size());

                auto agg_stats = parallel_agg.get_statistics();
                merged_result->custom_data["total_partial_agg_time_us"] =
                    std::to_string(agg_stats.partial_agg_time.count());
                merged_result->custom_data["total_final_agg_time_us"] =
                    std::to_string(agg_stats.final_agg_time.count());
                merged_result->custom_data["total_groups_created"] =
                    std::to_string(agg_stats.partial_groups_created);
            }

            return merged_result;

        } catch (const std::exception& e) {
            auto error_result = std::make_shared<WorkResult>();
            error_result->success = false;
            error_result->error_message =
                "Parallel aggregation execution failed: " + std::string(e.what());
            return error_result;
        }
    }

    std::shared_ptr<WorkResult>
    ParallelQueryExecutor::execute_parallel_sort(std::shared_ptr<Sort> sort_node,
                                                 std::uint32_t worker_count)
    {
        if (!sort_node || worker_count == 0) {
            auto error_result = std::make_shared<WorkResult>();
            error_result->success = false;
            error_result->error_message = "Invalid sort node or worker count";
            return error_result;
        }

        // Create parallel sort instance
        std::string sort_name =
            "parallel_sort_" + std::to_string(reinterpret_cast<std::uintptr_t>(sort_node.get()));
        ParallelSort parallel_sort(sort_name);

        // Mock sort parameters (in real implementation, these would come from sort_node)
        std::uint64_t estimated_input_rows = 500000; // Mock value
        std::vector<std::string> sort_keys = {"order_date", "customer_id", "amount"};
        std::vector<bool> sort_ascending = {false, true, false}; // DESC, ASC, DESC

        try {
            // Plan sort partitions
            auto partitions = parallel_sort.plan_sort_partitions(estimated_input_rows, sort_keys,
                                                                 sort_ascending, worker_count);
            if (partitions.empty()) {
                auto error_result = std::make_shared<WorkResult>();
                error_result->success = false;
                error_result->error_message = "Failed to create sort partitions";
                return error_result;
            }

            // Create and execute local sort work units
            auto local_work_units = parallel_sort.create_local_sort_work_units(partitions);
            std::vector<std::shared_ptr<WorkResult>> local_results;
            local_results.reserve(local_work_units.size());

            for (const auto& partition : partitions) {
                auto local_result = parallel_sort.execute_local_sort(partition);
                local_results.push_back(local_result);
            }

            // Execute merge phases if needed
            std::vector<std::shared_ptr<WorkResult>> merge_results = local_results;
            std::uint32_t merge_level = 0;

            while (merge_results.size() > 1) {
                merge_level++;
                auto merge_work_units =
                    parallel_sort.create_merge_work_units(partitions, merge_level);
                auto merge_result =
                    parallel_sort.execute_merge_phase(partitions, merge_results, merge_level);
                merge_results = {merge_result};
            }

            // Final merge of all sorted partitions
            auto final_result =
                parallel_sort.merge_sorted_results(merge_results, sort_keys, sort_ascending);

            if (final_result) {
                // Add parallel execution statistics
                final_result->custom_data["parallel_workers"] = std::to_string(worker_count);
                final_result->custom_data["sort_partitions"] = std::to_string(partitions.size());
                final_result->custom_data["operation_type"] = "parallel_sort";
                final_result->custom_data["sort_keys_count"] = std::to_string(sort_keys.size());
                final_result->custom_data["merge_levels"] = std::to_string(merge_level);

                auto sort_stats = parallel_sort.get_statistics();
                final_result->custom_data["total_local_sort_time_us"] =
                    std::to_string(sort_stats.local_sort_time.count());
                final_result->custom_data["total_merge_time_us"] =
                    std::to_string(sort_stats.merge_time.count());
                final_result->custom_data["total_comparisons"] =
                    std::to_string(sort_stats.comparisons_performed);
            }

            return final_result;

        } catch (const std::exception& e) {
            auto error_result = std::make_shared<WorkResult>();
            error_result->success = false;
            error_result->error_message =
                "Parallel sort execution failed: " + std::string(e.what());
            return error_result;
        }
    }

} // namespace scratchbird::engine
