// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/batch_operations.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace scratchbird::engine
{

    // BatchContainer implementation

    BatchContainer::BatchContainer(std::uint64_t batch_id, BatchOperationType type,
                                   const BatchConfig& config)
        : batch_id_(batch_id), type_(type), config_(config),
          created_time_(std::chrono::steady_clock::now())
    {
        operations_.reserve(config_.max_rows_per_batch);
    }

    bool BatchContainer::add_operation(std::unique_ptr<BatchOperation> operation)
    {
        if (!operation) {
            return false;
        }

        std::lock_guard<std::mutex> lock(operations_mutex_);

        // Check if batch is full
        if (operations_.size() >= config_.max_rows_per_batch) {
            return false;
        }

        // Check memory constraints
        auto operation_size = operation->get_estimated_size();
        if (estimated_memory_usage_ + operation_size >
            (config_.max_batch_memory_mb * 1024 * 1024)) {
            return false;
        }

        // Add operation
        estimated_memory_usage_ += operation_size;
        operations_.push_back(std::move(operation));

        return true;
    }

    bool BatchContainer::is_ready_for_execution() const
    {
        std::lock_guard<std::mutex> lock(operations_mutex_);
        return !operations_.empty() && (is_full() || has_timed_out());
    }

    bool BatchContainer::is_full() const
    {
        std::lock_guard<std::mutex> lock(operations_mutex_);
        return operations_.size() >= config_.max_rows_per_batch ||
               estimated_memory_usage_ >= (config_.max_batch_memory_mb * 1024 * 1024);
    }

    bool BatchContainer::has_timed_out() const
    {
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - created_time_);
        return age >= config_.batch_timeout;
    }

    std::uint32_t BatchContainer::size() const
    {
        std::lock_guard<std::mutex> lock(operations_mutex_);
        return static_cast<std::uint32_t>(operations_.size());
    }

    std::size_t BatchContainer::get_memory_usage() const
    {
        std::lock_guard<std::mutex> lock(operations_mutex_);
        return estimated_memory_usage_;
    }

    const std::vector<std::unique_ptr<BatchOperation>>& BatchContainer::get_operations() const
    {
        return operations_;
    }

    void BatchContainer::clear()
    {
        std::lock_guard<std::mutex> lock(operations_mutex_);
        operations_.clear();
        estimated_memory_usage_ = 0;
        created_time_ = std::chrono::steady_clock::now();
    }

    std::chrono::milliseconds BatchContainer::get_age() const
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - created_time_);
    }

    // NetworkBatchManager implementation

    NetworkBatchManager::NetworkBatchManager(const BatchConfig& config)
        : config_(config), last_flush_(std::chrono::steady_clock::now())
    {
        batched_messages_.reserve(config_.max_network_batch_size);
    }

    bool NetworkBatchManager::add_message(const std::vector<std::uint8_t>& message)
    {
        std::lock_guard<std::mutex> lock(batch_mutex_);

        // Check if adding this message would exceed batch size
        if (batched_messages_.size() + message.size() > config_.max_network_batch_size) {
            return false;
        }

        // Add message length prefix (4 bytes) followed by message data
        std::uint32_t msg_len = static_cast<std::uint32_t>(message.size());
        batched_messages_.insert(batched_messages_.end(),
                                 reinterpret_cast<const std::uint8_t*>(&msg_len),
                                 reinterpret_cast<const std::uint8_t*>(&msg_len) + sizeof(msg_len));
        batched_messages_.insert(batched_messages_.end(), message.begin(), message.end());

        return true;
    }

    std::vector<std::uint8_t> NetworkBatchManager::flush_batch()
    {
        std::lock_guard<std::mutex> lock(batch_mutex_);

        std::vector<std::uint8_t> result = std::move(batched_messages_);
        batched_messages_.clear();
        batched_messages_.reserve(config_.max_network_batch_size);
        last_flush_ = std::chrono::steady_clock::now();

        return result;
    }

    bool NetworkBatchManager::should_flush() const
    {
        std::lock_guard<std::mutex> lock(batch_mutex_);

        // Flush if batch is getting full
        if (batched_messages_.size() >= config_.max_network_batch_size * 0.8) {
            return true;
        }

        // Flush if timeout reached
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_flush_);
        return elapsed >= config_.network_batch_timeout;
    }

    std::size_t NetworkBatchManager::get_batch_size() const
    {
        std::lock_guard<std::mutex> lock(batch_mutex_);
        return batched_messages_.size();
    }

    void NetworkBatchManager::reset()
    {
        std::lock_guard<std::mutex> lock(batch_mutex_);
        batched_messages_.clear();
        last_flush_ = std::chrono::steady_clock::now();
    }

    // BatchOperationsEngine implementation

    BatchOperationsEngine::BatchOperationsEngine(const BatchConfig& config) : config_(config)
    {
        // Validate configuration
        if (!config_.is_valid()) {
            throw std::invalid_argument("Invalid batch configuration");
        }
    }

    BatchOperationsEngine::~BatchOperationsEngine()
    {
        shutdown();
    }

    bool BatchOperationsEngine::initialize()
    {
        if (initialized_.exchange(true)) {
            return true; // Already initialized
        }

        // Initialize statistics
        statistics_.reset();

        // Initialize batch type tracking
        std::lock_guard<std::mutex> lock(batches_mutex_);
        current_batch_ids_[BatchOperationType::INSERT] = 0;
        current_batch_ids_[BatchOperationType::UPDATE] = 0;
        current_batch_ids_[BatchOperationType::DELETE] = 0;
        current_batch_ids_[BatchOperationType::MIXED] = 0;

        return true;
    }

    void BatchOperationsEngine::shutdown()
    {
        if (!initialized_.exchange(false)) {
            return; // Already shutdown
        }

        shutdown_requested_.store(true);

        // Execute any remaining batches
        try {
            flush_all_batches();
        } catch (...) {
            // Ignore errors during shutdown
        }

        // Clear all batches
        std::lock_guard<std::mutex> lock(batches_mutex_);
        active_batches_.clear();
        current_batch_ids_.clear();
    }

    bool BatchOperationsEngine::is_running() const
    {
        return initialized_.load() && !shutdown_requested_.load();
    }

    bool BatchOperationsEngine::update_config(const BatchConfig& config)
    {
        if (!config.is_valid()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(config_mutex_);
        config_ = config;
        return true;
    }

    BatchConfig BatchOperationsEngine::get_config() const
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return config_;
    }

    std::uint64_t
    BatchOperationsEngine::add_operation(BatchOperationType type, const std::string& sql,
                                         const std::vector<std::vector<std::string>>& parameters,
                                         const std::string& table_name)
    {
        if (!is_running()) {
            return 0; // Engine not running
        }

        // Create operation
        std::uint64_t operation_id = next_operation_id_.fetch_add(1);
        auto operation = std::make_unique<BatchOperation>(operation_id, type, sql);
        operation->parameters = parameters;
        operation->table_name = table_name;

        // Get or create appropriate batch
        BatchContainer* batch = get_or_create_batch(type);
        if (!batch) {
            return 0; // Failed to get/create batch
        }

        // Try to add operation to batch
        if (!batch->add_operation(std::move(operation))) {
            // Batch is full, execute it and create a new one
            auto batch_id = batch->get_id();
            execute_batch(batch_id);

            // Try again with new batch
            batch = get_or_create_batch(type);
            if (!batch || !batch->add_operation(std::move(operation))) {
                return 0; // Still failed
            }
        }

        // Check if we should execute ready batches
        if (config_.execution_mode == BatchExecutionMode::IMMEDIATE) {
            check_and_execute_ready_batches();
        }

        return operation_id;
    }

    BatchExecutionResult BatchOperationsEngine::execute_batch(std::uint64_t batch_id)
    {
        std::unique_ptr<BatchContainer> batch;

        // Remove batch from active batches
        {
            std::lock_guard<std::mutex> lock(batches_mutex_);
            auto it = active_batches_.find(batch_id);
            if (it == active_batches_.end()) {
                // Batch not found - return empty result
                BatchExecutionResult result;
                result.batch_id = batch_id;
                return result;
            }
            batch = std::move(it->second);
            active_batches_.erase(it);
        }

        // Execute the batch
        auto result = execute_batch_internal(*batch);

        // Update statistics
        update_statistics(result);

        // Call completion callback if set
        {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            if (completion_callback_) {
                completion_callback_(result);
            }
        }

        return result;
    }

    std::vector<BatchExecutionResult> BatchOperationsEngine::execute_ready_batches()
    {
        std::vector<std::uint64_t> ready_batch_ids;

        // Find ready batches
        {
            std::lock_guard<std::mutex> lock(batches_mutex_);
            for (const auto& [batch_id, batch] : active_batches_) {
                if (batch->is_ready_for_execution()) {
                    ready_batch_ids.push_back(batch_id);
                }
            }
        }

        // Execute ready batches
        std::vector<BatchExecutionResult> results;
        results.reserve(ready_batch_ids.size());

        for (auto batch_id : ready_batch_ids) {
            results.push_back(execute_batch(batch_id));
        }

        return results;
    }

    std::vector<BatchExecutionResult> BatchOperationsEngine::flush_all_batches()
    {
        std::vector<std::uint64_t> all_batch_ids;

        // Get all active batch IDs
        {
            std::lock_guard<std::mutex> lock(batches_mutex_);
            all_batch_ids.reserve(active_batches_.size());
            for (const auto& [batch_id, _] : active_batches_) {
                all_batch_ids.push_back(batch_id);
            }
        }

        // Execute all batches
        std::vector<BatchExecutionResult> results;
        results.reserve(all_batch_ids.size());

        for (auto batch_id : all_batch_ids) {
            results.push_back(execute_batch(batch_id));
        }

        return results;
    }

    bool BatchOperationsEngine::cancel_batch(std::uint64_t batch_id)
    {
        std::lock_guard<std::mutex> lock(batches_mutex_);
        auto it = active_batches_.find(batch_id);
        if (it == active_batches_.end()) {
            return false; // Batch not found
        }

        active_batches_.erase(it);
        return true;
    }

    void BatchOperationsEngine::set_completion_callback(BatchCompletionCallback callback)
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        completion_callback_ = callback;
    }

    void BatchOperationsEngine::set_progress_callback(BatchProgressCallback callback)
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        progress_callback_ = callback;
    }

    void BatchOperationsEngine::set_error_callback(BatchErrorCallback callback)
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        error_callback_ = callback;
    }

    BatchStatisticsSnapshot BatchOperationsEngine::get_statistics() const
    {
        // Return snapshot of atomic statistics
        return statistics_.snapshot();
    }

    void BatchOperationsEngine::reset_statistics()
    {
        statistics_.reset();
    }

    std::uint32_t BatchOperationsEngine::get_active_batch_count() const
    {
        std::lock_guard<std::mutex> lock(batches_mutex_);
        return static_cast<std::uint32_t>(active_batches_.size());
    }

    std::uint64_t BatchOperationsEngine::get_pending_operation_count() const
    {
        std::uint64_t total = 0;
        std::lock_guard<std::mutex> lock(batches_mutex_);
        for (const auto& [_, batch] : active_batches_) {
            const auto& operations = batch->get_operations();
            for (const auto& operation : operations) {
                if (operation->parameters.empty()) {
                    total += 1; // Single operation with no parameters
                } else {
                    total += operation->parameters.size(); // Count parameter rows
                }
            }
        }
        return total;
    }

    std::string BatchOperationsEngine::generate_performance_report() const
    {
        auto stats = get_statistics();
        std::ostringstream report;

        report << "Batch Operations Performance Report\n";
        report << "===================================\n";
        report << std::fixed << std::setprecision(2);

        // Overall statistics
        report << "Total Batches: " << stats.total_batches << "\n";
        report << "Successful Batches: " << stats.successful_batches << "\n";
        report << "Failed Batches: " << stats.failed_batches << "\n";
        report << "Success Rate: " << (stats.get_success_rate() * 100.0) << "%\n\n";

        // Operation statistics
        report << "Total Operations: " << stats.total_operations << "\n";
        report << "Successful Operations: " << stats.successful_operations << "\n";
        report << "Failed Operations: " << stats.failed_operations << "\n";
        report << "Rows Affected: " << stats.total_rows_affected << "\n\n";

        // Performance metrics
        report << "Average Execution Time: " << stats.get_avg_execution_time_ms() << " ms\n";
        report << "Throughput: " << stats.get_throughput_ops_per_sec() << " ops/sec\n";
        report << "Average Batch Size: " << stats.avg_batch_size << "\n";
        report << "Max Batch Size: " << stats.max_batch_size << "\n\n";

        // Memory usage
        report << "Peak Memory Usage: " << (stats.peak_memory_usage_bytes / 1024 / 1024) << " MB\n";
        report << "Total Memory Allocated: " << (stats.total_memory_allocated / 1024 / 1024)
               << " MB\n\n";

        // Current state
        report << "Active Batches: " << get_active_batch_count() << "\n";
        report << "Pending Operations: " << get_pending_operation_count() << "\n";

        return report.str();
    }

    // Private methods implementation

    BatchContainer* BatchOperationsEngine::get_or_create_batch(BatchOperationType type)
    {
        std::lock_guard<std::mutex> lock(batches_mutex_);

        // Check if we have an active batch for this type
        auto current_id_it = current_batch_ids_.find(type);
        if (current_id_it != current_batch_ids_.end() && current_id_it->second != 0) {
            auto batch_it = active_batches_.find(current_id_it->second);
            if (batch_it != active_batches_.end() && !batch_it->second->is_full()) {
                return batch_it->second.get();
            }
        }

        // Create new batch
        std::uint64_t new_batch_id = next_batch_id_.fetch_add(1);
        auto new_batch = std::make_unique<BatchContainer>(new_batch_id, type, config_);

        BatchContainer* batch_ptr = new_batch.get();
        active_batches_[new_batch_id] = std::move(new_batch);
        current_batch_ids_[type] = new_batch_id;

        return batch_ptr;
    }

    BatchExecutionResult BatchOperationsEngine::execute_batch_internal(BatchContainer& batch)
    {
        BatchExecutionResult result;
        result.batch_id = batch.get_id();
        result.start_time = std::chrono::steady_clock::now();

        const auto& operations = batch.get_operations();
        result.total_operations = static_cast<std::uint32_t>(operations.size());

        if (operations.empty()) {
            result.end_time = std::chrono::steady_clock::now();
            return result;
        }

        try {
            // Execute operations
            execute_operations(operations, result);
            result.success = (result.failed_operations == 0) || config_.continue_on_error;
        } catch (const std::exception& e) {
            result.success = false;
            // Add error for all operations if batch-level failure
            for (const auto& op : operations) {
                result.operation_errors.emplace_back(op->operation_id, e.what());
            }
            result.failed_operations = result.total_operations;
        }

        result.end_time = std::chrono::steady_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
            result.end_time - result.start_time);

        return result;
    }

    void BatchOperationsEngine::execute_operations(
        const std::vector<std::unique_ptr<BatchOperation>>& operations,
        BatchExecutionResult& result)
    {
        // Group operations by table/statement for optimization
        std::unordered_map<std::string, std::vector<const BatchOperation*>> grouped_ops;

        for (const auto& op : operations) {
            std::string key = op->table_name + "|" + op->sql;
            grouped_ops[key].push_back(op.get());
        }

        // Execute grouped operations
        for (const auto& [key, ops] : grouped_ops) {
            for (const auto* op : ops) {
                try {
                    // Simulate operation execution
                    // In a real implementation, this would:
                    // 1. Prepare the SQL statement
                    // 2. Bind parameters for all rows
                    // 3. Execute as a batch
                    // 4. Process results

                    std::this_thread::sleep_for(
                        std::chrono::microseconds(100)); // Simulate execution time

                    // Simulate some operations affecting multiple rows
                    std::uint64_t rows_affected = op->parameters.size();
                    if (rows_affected == 0)
                        rows_affected = 1;

                    result.rows_affected += rows_affected;
                    result.successful_operations++;

                } catch (const std::exception& e) {
                    result.failed_operations++;
                    result.operation_errors.emplace_back(op->operation_id, e.what());

                    // Call error callback if set
                    {
                        std::lock_guard<std::mutex> lock(callbacks_mutex_);
                        if (error_callback_) {
                            error_callback_(op->operation_id, e.what());
                        }
                    }

                    if (!config_.continue_on_error) {
                        throw; // Re-throw to stop batch execution
                    }
                }

                // Update progress callback
                {
                    std::lock_guard<std::mutex> lock(callbacks_mutex_);
                    if (progress_callback_) {
                        progress_callback_(result.successful_operations + result.failed_operations,
                                           result.total_operations);
                    }
                }
            }
        }
    }

    void BatchOperationsEngine::update_statistics(const BatchExecutionResult& result)
    {
        // Update batch statistics
        statistics_.total_batches.fetch_add(1);
        if (result.success) {
            statistics_.successful_batches.fetch_add(1);
        } else {
            statistics_.failed_batches.fetch_add(1);
        }

        // Update operation statistics
        statistics_.total_operations.fetch_add(result.total_operations);
        statistics_.successful_operations.fetch_add(result.successful_operations);
        statistics_.failed_operations.fetch_add(result.failed_operations);
        statistics_.total_rows_affected.fetch_add(result.rows_affected);

        // Update timing statistics
        statistics_.total_execution_time_us.fetch_add(result.execution_time.count());
        statistics_.total_network_time_us.fetch_add(result.network_time.count());

        // Update batch size statistics
        auto current_max = statistics_.max_batch_size.load();
        while (result.total_operations > current_max &&
               !statistics_.max_batch_size.compare_exchange_weak(current_max,
                                                                 result.total_operations)) {
            // Loop until successful update or we find a larger value
        }

        auto current_min = statistics_.min_batch_size.load();
        while (result.total_operations < current_min &&
               !statistics_.min_batch_size.compare_exchange_weak(current_min,
                                                                 result.total_operations)) {
            // Loop until successful update or we find a smaller value
        }

        // Update average batch size (simplified - not perfectly accurate under concurrency)
        auto total_batches = statistics_.total_batches.load();
        auto total_ops = statistics_.total_operations.load();
        if (total_batches > 0) {
            double avg = static_cast<double>(total_ops) / total_batches;
            statistics_.avg_batch_size.store(avg);
        }
    }

    void BatchOperationsEngine::cleanup_completed_batches()
    {
        // This method would be called periodically to clean up completed batches
        // For now, batches are cleaned up immediately after execution
    }

    void BatchOperationsEngine::check_and_execute_ready_batches()
    {
        if (config_.execution_mode == BatchExecutionMode::IMMEDIATE) {
            execute_ready_batches();
        }
    }

} // namespace scratchbird::engine
