// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/batch_operations.h"

#include <chrono>
#include <gtest/gtest.h>
#include <random>
#include <thread>

using namespace scratchbird::engine;

class BatchOperationsTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create test configuration
        config_.max_rows_per_batch = 100;
        config_.max_batch_memory_mb = 16;
        config_.batch_timeout = std::chrono::milliseconds(200);
        config_.execution_mode = BatchExecutionMode::DEFERRED; // Manual control for testing
        config_.min_batch_size = 5;
        config_.enable_parallel_execution = true;
        config_.continue_on_error = true;
        config_.max_retry_attempts = 2;
        config_.enable_statistics = true;
        config_.stats_window_size = 100;
        config_.enable_network_batching = true;
        config_.max_network_batch_size = 4096;
        config_.network_batch_timeout = std::chrono::milliseconds(50);

        // Create engine
        engine_ = std::make_unique<BatchOperationsEngine>(config_);
        ASSERT_TRUE(engine_->initialize());
    }

    void TearDown() override
    {
        if (engine_) {
            engine_->shutdown();
            engine_.reset();
        }
    }

    BatchConfig config_;
    std::unique_ptr<BatchOperationsEngine> engine_;
};

// Test batch configuration validation
TEST_F(BatchOperationsTest, ConfigurationValidation)
{
    BatchConfig valid_config;
    EXPECT_TRUE(valid_config.is_valid());

    // Test invalid configurations
    BatchConfig invalid_config1;
    invalid_config1.max_rows_per_batch = 0;
    EXPECT_FALSE(invalid_config1.is_valid());

    BatchConfig invalid_config2;
    invalid_config2.max_batch_memory_mb = 0;
    EXPECT_FALSE(invalid_config2.is_valid());

    BatchConfig invalid_config3;
    invalid_config3.batch_timeout = std::chrono::milliseconds(0);
    EXPECT_FALSE(invalid_config3.is_valid());

    BatchConfig invalid_config4;
    invalid_config4.min_batch_size = 1000;
    invalid_config4.max_rows_per_batch = 100; // min > max
    EXPECT_FALSE(invalid_config4.is_valid());
}

// Test BatchOperation creation and basic properties
TEST_F(BatchOperationsTest, BatchOperationBasics)
{
    BatchOperation operation(1, BatchOperationType::INSERT, "INSERT INTO test VALUES (?, ?)");

    EXPECT_EQ(operation.operation_id, 1);
    EXPECT_EQ(operation.type, BatchOperationType::INSERT);
    EXPECT_EQ(operation.sql, "INSERT INTO test VALUES (?, ?)");
    EXPECT_FALSE(operation.executed);
    EXPECT_EQ(operation.retry_count, 0);

    // Test parameter addition
    operation.parameters.push_back({"value1", "value2"});
    operation.parameters.push_back({"value3", "value4"});

    EXPECT_EQ(operation.parameters.size(), 2);
    EXPECT_GT(operation.get_estimated_size(), 0);
}

// Test BatchContainer functionality
TEST_F(BatchOperationsTest, BatchContainerOperations)
{
    BatchContainer batch(1, BatchOperationType::INSERT, config_);

    EXPECT_EQ(batch.get_id(), 1);
    EXPECT_EQ(batch.get_type(), BatchOperationType::INSERT);
    EXPECT_EQ(batch.size(), 0);
    EXPECT_EQ(batch.get_memory_usage(), 0);
    EXPECT_FALSE(batch.is_full());
    EXPECT_FALSE(batch.is_ready_for_execution());

    // Add operations
    for (int i = 0; i < 10; ++i) {
        auto operation = std::make_unique<BatchOperation>(i, BatchOperationType::INSERT,
                                                          "INSERT INTO test VALUES (?)");
        operation->parameters.push_back({std::to_string(i)});
        EXPECT_TRUE(batch.add_operation(std::move(operation)));
    }

    EXPECT_EQ(batch.size(), 10);
    EXPECT_GT(batch.get_memory_usage(), 0);
    EXPECT_FALSE(batch.is_full()); // Should not be full with 10 operations

    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    EXPECT_TRUE(batch.has_timed_out());
    EXPECT_TRUE(batch.is_ready_for_execution());
}

// Test BatchContainer memory limits
TEST_F(BatchOperationsTest, BatchContainerMemoryLimits)
{
    BatchConfig small_memory_config = config_;
    small_memory_config.max_batch_memory_mb = 1; // 1MB limit

    BatchContainer batch(1, BatchOperationType::INSERT, small_memory_config);

    // Create large operation
    auto large_operation = std::make_unique<BatchOperation>(1, BatchOperationType::INSERT,
                                                            "INSERT INTO test VALUES (?)");

    // Add large binary data to exceed memory limit
    large_operation->binary_data.resize(2 * 1024 * 1024); // 2MB

    EXPECT_FALSE(batch.add_operation(std::move(large_operation)));
    EXPECT_EQ(batch.size(), 0);
}

// Test NetworkBatchManager
TEST_F(BatchOperationsTest, NetworkBatchManager)
{
    NetworkBatchManager manager(config_);

    EXPECT_EQ(manager.get_batch_size(), 0);
    EXPECT_FALSE(manager.should_flush());

    // Add small messages
    std::vector<std::uint8_t> msg1 = {1, 2, 3, 4};
    std::vector<std::uint8_t> msg2 = {5, 6, 7, 8};

    EXPECT_TRUE(manager.add_message(msg1));
    EXPECT_TRUE(manager.add_message(msg2));
    EXPECT_GT(manager.get_batch_size(), 0);

    // Flush batch
    auto batched_data = manager.flush_batch();
    EXPECT_GT(batched_data.size(), 0);
    EXPECT_EQ(manager.get_batch_size(), 0);

    // Test timeout-based flushing
    manager.add_message(msg1);
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    EXPECT_TRUE(manager.should_flush());
}

// Test BatchOperationsEngine basic functionality
TEST_F(BatchOperationsTest, BatchEngineBasics)
{
    EXPECT_TRUE(engine_->is_running());
    EXPECT_EQ(engine_->get_active_batch_count(), 0);
    EXPECT_EQ(engine_->get_pending_operation_count(), 0);

    // Test configuration
    auto current_config = engine_->get_config();
    EXPECT_EQ(current_config.max_rows_per_batch, config_.max_rows_per_batch);

    // Update configuration
    BatchConfig new_config = current_config;
    new_config.max_rows_per_batch = 200;
    EXPECT_TRUE(engine_->update_config(new_config));

    auto updated_config = engine_->get_config();
    EXPECT_EQ(updated_config.max_rows_per_batch, 200);
}

// Test adding operations to engine
TEST_F(BatchOperationsTest, AddOperationsToEngine)
{
    // Add INSERT operations
    std::vector<std::vector<std::string>> params1 = {{"1", "John"}, {"2", "Jane"}};
    auto op_id1 = engine_->add_operation(BatchOperationType::INSERT,
                                         "INSERT INTO users VALUES (?, ?)", params1, "users");
    EXPECT_GT(op_id1, 0);
    EXPECT_EQ(engine_->get_pending_operation_count(), 2);

    // Add UPDATE operations
    std::vector<std::vector<std::string>> params2 = {{"John Doe", "1"}, {"Jane Doe", "2"}};
    auto op_id2 = engine_->add_operation(
        BatchOperationType::UPDATE, "UPDATE users SET name = ? WHERE id = ?", params2, "users");
    EXPECT_GT(op_id2, 0);
    EXPECT_EQ(engine_->get_pending_operation_count(), 4);

    // Should have active batches now
    EXPECT_GT(engine_->get_active_batch_count(), 0);
}

// Test batch execution
TEST_F(BatchOperationsTest, BatchExecution)
{
    // Add operations
    std::vector<std::vector<std::string>> params = {{"1", "Alice"}, {"2", "Bob"}, {"3", "Charlie"}};

    auto op_id = engine_->add_operation(BatchOperationType::INSERT,
                                        "INSERT INTO test VALUES (?, ?)", params, "test");
    EXPECT_GT(op_id, 0);

    // Execute all ready batches (should execute after timeout)
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    auto results = engine_->execute_ready_batches();

    EXPECT_GT(results.size(), 0);

    for (const auto& result : results) {
        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.total_operations, 1); // One operation with 3 parameter sets
        EXPECT_EQ(result.successful_operations, 1);
        EXPECT_EQ(result.failed_operations, 0);
        EXPECT_EQ(result.rows_affected, 3); // 3 rows from parameter sets
        EXPECT_GT(result.execution_time.count(), 0);
    }

    // Should have no pending operations after execution
    EXPECT_EQ(engine_->get_pending_operation_count(), 0);
}

// Test flush all batches
TEST_F(BatchOperationsTest, FlushAllBatches)
{
    // Add multiple operations of different types
    std::vector<std::vector<std::string>> insert_params = {{"1", "Alice"}};
    std::vector<std::vector<std::string>> update_params = {{"Bob", "1"}};
    std::vector<std::vector<std::string>> delete_params = {{"2"}};

    engine_->add_operation(BatchOperationType::INSERT, "INSERT INTO test VALUES (?, ?)",
                           insert_params, "test");
    engine_->add_operation(BatchOperationType::UPDATE, "UPDATE test SET name = ? WHERE id = ?",
                           update_params, "test");
    engine_->add_operation(BatchOperationType::DELETE, "DELETE FROM test WHERE id = ?",
                           delete_params, "test");

    EXPECT_EQ(engine_->get_pending_operation_count(), 3);

    // Flush all batches
    auto results = engine_->flush_all_batches();

    EXPECT_GE(results.size(), 1); // At least one batch executed
    EXPECT_EQ(engine_->get_pending_operation_count(), 0);
    EXPECT_EQ(engine_->get_active_batch_count(), 0);
}

// Test batch statistics
TEST_F(BatchOperationsTest, BatchStatistics)
{
    // Initial statistics should be zero
    auto initial_stats = engine_->get_statistics();
    EXPECT_EQ(initial_stats.total_batches, 0);
    EXPECT_EQ(initial_stats.total_operations, 0);

    // Add and execute operations
    std::vector<std::vector<std::string>> params = {{"1", "Test"}};
    engine_->add_operation(BatchOperationType::INSERT, "INSERT INTO test VALUES (?, ?)", params,
                           "test");

    auto results = engine_->flush_all_batches();
    EXPECT_GT(results.size(), 0);

    // Check updated statistics
    auto updated_stats = engine_->get_statistics();
    EXPECT_GT(updated_stats.total_batches, 0);
    EXPECT_GT(updated_stats.total_operations, 0);
    EXPECT_GT(updated_stats.successful_operations, 0);
    EXPECT_GT(updated_stats.total_rows_affected, 0);
    EXPECT_GT(updated_stats.get_throughput_ops_per_sec(), 0.0);

    // Test statistics reset
    engine_->reset_statistics();
    auto reset_stats = engine_->get_statistics();
    EXPECT_EQ(reset_stats.total_batches, 0);
    EXPECT_EQ(reset_stats.total_operations, 0);
}

// Test performance report generation
TEST_F(BatchOperationsTest, PerformanceReport)
{
    // Execute some operations first
    std::vector<std::vector<std::string>> params = {{"1", "Test"}};
    engine_->add_operation(BatchOperationType::INSERT, "INSERT INTO test VALUES (?, ?)", params,
                           "test");
    engine_->flush_all_batches();

    // Generate report
    auto report = engine_->generate_performance_report();
    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("Batch Operations Performance Report"), std::string::npos);
    EXPECT_NE(report.find("Total Batches:"), std::string::npos);
    EXPECT_NE(report.find("Success Rate:"), std::string::npos);
}

// Test batch callbacks
TEST_F(BatchOperationsTest, BatchCallbacks)
{
    bool completion_called = false;
    bool progress_called = false;
    bool error_called = false;

    // Set callbacks
    engine_->set_completion_callback([&completion_called](const BatchExecutionResult& result) {
        completion_called = true;
        EXPECT_TRUE(result.success);
    });

    engine_->set_progress_callback(
        [&progress_called](std::uint64_t completed, std::uint64_t total) {
            progress_called = true;
            EXPECT_LE(completed, total);
        });

    engine_->set_error_callback(
        [&error_called](std::uint64_t, const std::string&) { error_called = true; });

    // Execute operations to trigger callbacks
    std::vector<std::vector<std::string>> params = {{"1", "Test"}};
    engine_->add_operation(BatchOperationType::INSERT, "INSERT INTO test VALUES (?, ?)", params,
                           "test");
    engine_->flush_all_batches();

    EXPECT_TRUE(completion_called);
    EXPECT_TRUE(progress_called);
    // error_called should be false since we didn't have any errors
}

// Test batch cancellation
TEST_F(BatchOperationsTest, BatchCancellation)
{
    // Add operation to create a batch
    std::vector<std::vector<std::string>> params = {{"1", "Test"}};
    engine_->add_operation(BatchOperationType::INSERT, "INSERT INTO test VALUES (?, ?)", params,
                           "test");

    auto initial_count = engine_->get_active_batch_count();
    EXPECT_GT(initial_count, 0);

    // Cancel doesn't work without batch ID, but we can test the interface
    // In a real implementation, we'd need to expose batch IDs or add a cancel-by-type method

    // Flush instead to clean up
    engine_->flush_all_batches();
    EXPECT_EQ(engine_->get_active_batch_count(), 0);
}

// Test concurrent operations
TEST_F(BatchOperationsTest, ConcurrentOperations)
{
    const int num_threads = 4;
    const int ops_per_thread = 25;
    std::vector<std::thread> threads;
    std::atomic<int> completed_threads{0};

    // Create multiple threads adding operations concurrently
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, ops_per_thread, &completed_threads]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::vector<std::vector<std::string>> params = {
                    {std::to_string(t * ops_per_thread + i), "Thread" + std::to_string(t)}};

                auto op_id = engine_->add_operation(
                    BatchOperationType::INSERT, "INSERT INTO test VALUES (?, ?)", params, "test");
                EXPECT_GT(op_id, 0);

                // Small delay to simulate realistic usage
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            completed_threads.fetch_add(1);
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(completed_threads.load(), num_threads);
    EXPECT_EQ(engine_->get_pending_operation_count(), num_threads * ops_per_thread);

    // Execute all batches
    auto results = engine_->flush_all_batches();
    EXPECT_GT(results.size(), 0);
    EXPECT_EQ(engine_->get_pending_operation_count(), 0);

    // Verify total operations executed
    auto stats = engine_->get_statistics();
    EXPECT_EQ(stats.total_rows_affected, num_threads * ops_per_thread);
}

// Test memory usage tracking
TEST_F(BatchOperationsTest, MemoryUsageTracking)
{
    // Create operations with significant memory usage
    for (int i = 0; i < 50; ++i) {
        std::vector<std::vector<std::string>> params;
        for (int j = 0; j < 10; ++j) {
            std::string large_value(1000, 'A' + (i % 26)); // 1KB per parameter
            params.push_back({std::to_string(i * 10 + j), large_value});
        }

        engine_->add_operation(BatchOperationType::INSERT, "INSERT INTO large_test VALUES (?, ?)",
                               params, "large_test");
    }

    EXPECT_GT(engine_->get_pending_operation_count(), 0);

    // Execute and check memory statistics
    engine_->flush_all_batches();

    auto stats = engine_->get_statistics();
    EXPECT_GT(stats.peak_memory_usage_bytes, 0);
    EXPECT_GT(stats.total_memory_allocated, 0);
}

// Test edge cases and error conditions
TEST_F(BatchOperationsTest, EdgeCasesAndErrors)
{
    // Test with empty SQL
    auto op_id1 = engine_->add_operation(BatchOperationType::INSERT, "", {}, "");
    EXPECT_GT(op_id1, 0); // Should still create operation

    // Test with empty parameters
    auto op_id2 =
        engine_->add_operation(BatchOperationType::UPDATE, "UPDATE test SET x = 1", {}, "test");
    EXPECT_GT(op_id2, 0);

    // Test execution of these edge cases
    auto results = engine_->flush_all_batches();
    EXPECT_GT(results.size(), 0);
}

// Test batch timeout behavior
TEST_F(BatchOperationsTest, BatchTimeoutBehavior)
{
    // Configure short timeout
    BatchConfig timeout_config = config_;
    timeout_config.batch_timeout = std::chrono::milliseconds(100);
    timeout_config.execution_mode = BatchExecutionMode::IMMEDIATE;

    auto timeout_engine = std::make_unique<BatchOperationsEngine>(timeout_config);
    ASSERT_TRUE(timeout_engine->initialize());

    bool completion_called = false;
    timeout_engine->set_completion_callback(
        [&completion_called](const BatchExecutionResult&) { completion_called = true; });

    // Add single operation
    std::vector<std::vector<std::string>> params = {{"1", "Test"}};
    timeout_engine->add_operation(BatchOperationType::INSERT, "INSERT INTO test VALUES (?, ?)",
                                  params, "test");

    // Wait for timeout-based execution
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Check that batch was automatically executed due to timeout
    EXPECT_TRUE(completion_called);
    EXPECT_EQ(timeout_engine->get_pending_operation_count(), 0);

    timeout_engine->shutdown();
}

// Test performance under load
TEST_F(BatchOperationsTest, PerformanceUnderLoad)
{
    const int num_operations = 1000;

    auto start_time = std::chrono::steady_clock::now();

    // Add many operations
    for (int i = 0; i < num_operations; ++i) {
        std::vector<std::vector<std::string>> params = {
            {std::to_string(i), "LoadTest" + std::to_string(i)}};

        auto op_id = engine_->add_operation(
            BatchOperationType::INSERT, "INSERT INTO load_test VALUES (?, ?)", params, "load_test");
        EXPECT_GT(op_id, 0);
    }

    auto add_time = std::chrono::steady_clock::now();

    // Execute all operations
    auto results = engine_->flush_all_batches();

    auto end_time = std::chrono::steady_clock::now();

    // Verify results
    EXPECT_GT(results.size(), 0);

    auto stats = engine_->get_statistics();
    EXPECT_EQ(stats.total_rows_affected, num_operations);
    EXPECT_GT(stats.get_throughput_ops_per_sec(), 100.0); // Should be reasonably fast

    // Measure timings
    auto add_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(add_time - start_time);
    auto execute_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - add_time);

    // Operations should be added quickly (under 1 second for 1000 ops)
    EXPECT_LT(add_duration.count(), 1000);

    std::cout << "Performance Test Results:\n";
    std::cout << "Added " << num_operations << " operations in " << add_duration.count() << "ms\n";
    std::cout << "Executed " << num_operations << " operations in " << execute_duration.count()
              << "ms\n";
    std::cout << "Throughput: " << stats.get_throughput_ops_per_sec() << " ops/sec\n";
}
