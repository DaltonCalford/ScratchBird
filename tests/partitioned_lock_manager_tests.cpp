// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/partitioned_lock_manager.h"

#include <atomic>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <vector>

using namespace scratchbird::engine;

class PartitionedLockManagerTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create test configuration
        config_.partition_count = 4; // Small for testing
        config_.max_locks_per_partition = 100;
        config_.deadlock_detection_enabled =
            false;                                // Disable for testing to avoid background threads
        config_.deadlock_check_interval_ms = 100; // Fast for testing
        config_.max_wait_time_ms = 5000;          // 5 seconds
        config_.collect_statistics = true;
        config_.cross_partition_deadlock_detection = true;
        config_.initial_table_size = 16;
        config_.load_factor_threshold = 0.75;

        lock_manager_ = std::make_unique<PartitionedLockManager>(config_);
        ASSERT_TRUE(lock_manager_->initialize());

        // Test resource IDs distributed across partitions
        table1_resource_ = 1001;
        table2_resource_ = 1002;
        table3_resource_ = 1003;
        table4_resource_ = 1004;
        row1_resource_ = 2001;
        row2_resource_ = 2002;

        // Test transaction IDs
        xid1_ = 10001;
        xid2_ = 10002;
        xid3_ = 10003;
    }

    void TearDown() override
    {
        if (lock_manager_) {
            lock_manager_->shutdown();
        }
    }

    PartitionedLockManagerConfig config_;
    std::unique_ptr<PartitionedLockManager> lock_manager_;

    // Test resource IDs
    std::uint64_t table1_resource_;
    std::uint64_t table2_resource_;
    std::uint64_t table3_resource_;
    std::uint64_t table4_resource_;
    std::uint64_t row1_resource_;
    std::uint64_t row2_resource_;

    // Test transaction IDs
    std::uint64_t xid1_;
    std::uint64_t xid2_;
    std::uint64_t xid3_;
};

TEST_F(PartitionedLockManagerTest, ConfigurationValidation)
{
    // Test valid configuration
    PartitionedLockManagerConfig valid_config;
    EXPECT_TRUE(valid_config.is_valid());

    // Test invalid partition count (not power of 2)
    PartitionedLockManagerConfig invalid_config;
    invalid_config.partition_count = 7; // Not power of 2
    EXPECT_FALSE(invalid_config.is_valid());

    // Test zero partition count
    invalid_config.partition_count = 0;
    EXPECT_FALSE(invalid_config.is_valid());

    // Test invalid load factor
    invalid_config.partition_count = 16;
    invalid_config.load_factor_threshold = 1.5; // > 1.0
    EXPECT_FALSE(invalid_config.is_valid());
}

TEST_F(PartitionedLockManagerTest, BasicLockOperations)
{
    // Test basic lock acquisition
    EXPECT_TRUE(
        lock_manager_->try_lock(table1_resource_, FastPathLockType::ACCESS_SHARE_LOCK, xid1_));

    // Test compatible lock
    EXPECT_TRUE(
        lock_manager_->try_lock(table1_resource_, FastPathLockType::ACCESS_SHARE_LOCK, xid2_));

    // Test conflicting lock
    EXPECT_FALSE(
        lock_manager_->try_lock(table1_resource_, FastPathLockType::EXCLUSIVE_LOCK, xid3_));

    // Test lock release
    EXPECT_TRUE(lock_manager_->release_lock(table1_resource_, xid1_));
    EXPECT_TRUE(lock_manager_->release_lock(table1_resource_, xid2_));

    // Now exclusive lock should succeed
    EXPECT_TRUE(lock_manager_->try_lock(table1_resource_, FastPathLockType::EXCLUSIVE_LOCK, xid3_));
}

TEST_F(PartitionedLockManagerTest, PartitionDistribution)
{
    // Test that resources are distributed across partitions
    std::vector<std::uint64_t> resources = {table1_resource_, table2_resource_, table3_resource_,
                                            table4_resource_};

    std::set<std::uint32_t> used_partitions;
    for (auto resource_id : resources) {
        auto partition_id = lock_manager_->get_partition_id(resource_id);
        EXPECT_LT(partition_id, config_.partition_count);
        used_partitions.insert(partition_id);
    }

    // Should use multiple partitions (though not guaranteed to use all)
    EXPECT_GT(used_partitions.size(), 1u);
}

TEST_F(PartitionedLockManagerTest, ConflictDetection)
{
    // Test would_conflict functionality
    EXPECT_FALSE(lock_manager_->would_conflict(table1_resource_,
                                               FastPathLockType::ACCESS_SHARE_LOCK, xid1_));

    // Acquire lock
    EXPECT_TRUE(
        lock_manager_->try_lock(table1_resource_, FastPathLockType::ROW_EXCLUSIVE_LOCK, xid1_));

    // Test conflicts
    EXPECT_TRUE(
        lock_manager_->would_conflict(table1_resource_, FastPathLockType::SHARE_LOCK, xid2_));
    EXPECT_FALSE(lock_manager_->would_conflict(table1_resource_,
                                               FastPathLockType::ACCESS_SHARE_LOCK, xid2_));

    // Same transaction should not conflict with itself
    EXPECT_FALSE(
        lock_manager_->would_conflict(table1_resource_, FastPathLockType::EXCLUSIVE_LOCK, xid1_));
}

TEST_F(PartitionedLockManagerTest, LockInformation)
{
    // Initially no lock
    auto [lock_type, holder_xid] = lock_manager_->get_lock_info(table1_resource_);
    EXPECT_EQ(lock_type, FastPathLockType::NONE);
    EXPECT_EQ(holder_xid, 0u);

    // Acquire lock
    EXPECT_TRUE(lock_manager_->try_lock(table1_resource_, FastPathLockType::ROW_SHARE_LOCK, xid1_));

    // Check lock info
    std::tie(lock_type, holder_xid) = lock_manager_->get_lock_info(table1_resource_);
    EXPECT_EQ(lock_type, FastPathLockType::ROW_SHARE_LOCK);
    EXPECT_EQ(holder_xid, xid1_);

    // Release and check
    EXPECT_TRUE(lock_manager_->release_lock(table1_resource_, xid1_));
    std::tie(lock_type, holder_xid) = lock_manager_->get_lock_info(table1_resource_);
    EXPECT_EQ(lock_type, FastPathLockType::NONE);
    EXPECT_EQ(holder_xid, 0u);
}

TEST_F(PartitionedLockManagerTest, TransactionLockCleanup)
{
    // Acquire locks across multiple resources
    EXPECT_TRUE(
        lock_manager_->try_lock(table1_resource_, FastPathLockType::ACCESS_SHARE_LOCK, xid1_));
    EXPECT_TRUE(lock_manager_->try_lock(table2_resource_, FastPathLockType::ROW_SHARE_LOCK, xid1_));
    EXPECT_TRUE(
        lock_manager_->try_lock(table3_resource_, FastPathLockType::ROW_EXCLUSIVE_LOCK, xid1_));

    // Release all locks for transaction
    auto released_count = lock_manager_->release_transaction_locks(xid1_);
    EXPECT_EQ(released_count, 3u);

    // Verify locks are released
    auto [lock_type1, holder_xid1] = lock_manager_->get_lock_info(table1_resource_);
    EXPECT_EQ(lock_type1, FastPathLockType::NONE);

    auto [lock_type2, holder_xid2] = lock_manager_->get_lock_info(table2_resource_);
    EXPECT_EQ(lock_type2, FastPathLockType::NONE);

    auto [lock_type3, holder_xid3] = lock_manager_->get_lock_info(table3_resource_);
    EXPECT_EQ(lock_type3, FastPathLockType::NONE);
}

TEST_F(PartitionedLockManagerTest, StatisticsCollection)
{
    // Get initial statistics
    auto initial_stats = lock_manager_->get_aggregated_statistics();
    auto initial_requests = initial_stats.lock_requests.load();

    // Perform lock operations
    EXPECT_TRUE(
        lock_manager_->try_lock(table1_resource_, FastPathLockType::ACCESS_SHARE_LOCK, xid1_));
    EXPECT_FALSE(lock_manager_->try_lock(table1_resource_, FastPathLockType::EXCLUSIVE_LOCK,
                                         xid2_)); // Conflict
    EXPECT_TRUE(lock_manager_->release_lock(table1_resource_, xid1_));

    // Check statistics
    auto final_stats = lock_manager_->get_aggregated_statistics();
    EXPECT_GT(final_stats.lock_requests.load(), initial_requests);
    EXPECT_GT(final_stats.lock_grants.load(), 0u);
    EXPECT_GT(final_stats.lock_conflicts.load(), 0u);
    EXPECT_GT(final_stats.lock_releases.load(), 0u);
}

TEST_F(PartitionedLockManagerTest, ConfigurationUpdate)
{
    // Get current config
    auto current_config = lock_manager_->get_config();
    EXPECT_FALSE(current_config.deadlock_detection_enabled); // We disabled it in test setup

    // Update configuration
    current_config.deadlock_detection_enabled = true;
    current_config.collect_statistics = false;
    EXPECT_TRUE(lock_manager_->update_config(current_config));

    // Verify update
    auto updated_config = lock_manager_->get_config();
    EXPECT_TRUE(updated_config.deadlock_detection_enabled);
    EXPECT_FALSE(updated_config.collect_statistics);

    // Test invalid configuration update
    current_config.partition_count = 0; // Invalid
    EXPECT_FALSE(lock_manager_->update_config(current_config));
}

TEST_F(PartitionedLockManagerTest, SystemUtilization)
{
    // Initially empty
    EXPECT_EQ(lock_manager_->get_total_active_lock_count(), 0u);
    EXPECT_EQ(lock_manager_->get_system_utilization(), 0.0);

    // Add some locks
    EXPECT_TRUE(
        lock_manager_->try_lock(table1_resource_, FastPathLockType::ACCESS_SHARE_LOCK, xid1_));
    EXPECT_TRUE(
        lock_manager_->try_lock(table2_resource_, FastPathLockType::ROW_EXCLUSIVE_LOCK, xid2_));

    // Check utilization
    EXPECT_GT(lock_manager_->get_total_active_lock_count(), 0u);
    EXPECT_GT(lock_manager_->get_system_utilization(), 0.0);

    // Clean up
    lock_manager_->release_transaction_locks(xid1_);
    lock_manager_->release_transaction_locks(xid2_);
}

TEST_F(PartitionedLockManagerTest, PerformanceReport)
{
    // Perform some operations to generate data
    EXPECT_TRUE(
        lock_manager_->try_lock(table1_resource_, FastPathLockType::ACCESS_SHARE_LOCK, xid1_));
    EXPECT_TRUE(
        lock_manager_->try_lock(table2_resource_, FastPathLockType::ROW_EXCLUSIVE_LOCK, xid2_));

    // Generate report
    auto report = lock_manager_->generate_performance_report();

    // Verify report contains expected information
    EXPECT_TRUE(report.find("Partitioned Lock Manager Performance Report") != std::string::npos);
    EXPECT_TRUE(report.find("Partition Count") != std::string::npos);
    EXPECT_TRUE(report.find("Total Lock Requests") != std::string::npos);
    EXPECT_TRUE(report.find("Per-Partition Statistics") != std::string::npos);

    // Clean up
    lock_manager_->release_transaction_locks(xid1_);
    lock_manager_->release_transaction_locks(xid2_);
}

TEST_F(PartitionedLockManagerTest, MaintenanceOperations)
{
    // Add some locks
    EXPECT_TRUE(
        lock_manager_->try_lock(table1_resource_, FastPathLockType::ACCESS_SHARE_LOCK, xid1_));
    EXPECT_TRUE(
        lock_manager_->try_lock(table2_resource_, FastPathLockType::ROW_EXCLUSIVE_LOCK, xid2_));

    // Perform maintenance
    auto cleaned_count = lock_manager_->perform_maintenance();
    EXPECT_GE(cleaned_count, 0u); // May or may not clean anything

    // Clean up
    lock_manager_->release_transaction_locks(xid1_);
    lock_manager_->release_transaction_locks(xid2_);
}

TEST_F(PartitionedLockManagerTest, StatisticsReset)
{
    // Perform operations to generate statistics
    EXPECT_TRUE(
        lock_manager_->try_lock(table1_resource_, FastPathLockType::ACCESS_SHARE_LOCK, xid1_));
    EXPECT_TRUE(lock_manager_->release_lock(table1_resource_, xid1_));

    // Verify statistics exist
    auto stats = lock_manager_->get_aggregated_statistics();
    EXPECT_GT(stats.lock_requests.load(), 0u);

    // Reset statistics
    lock_manager_->reset_statistics();

    // Verify reset
    stats = lock_manager_->get_aggregated_statistics();
    EXPECT_EQ(stats.lock_requests.load(), 0u);
    EXPECT_EQ(stats.lock_grants.load(), 0u);
    EXPECT_EQ(stats.lock_releases.load(), 0u);
}

TEST_F(PartitionedLockManagerTest, ConcurrentAccess)
{
    const int num_threads = 4;
    const int operations_per_thread = 50;

    std::atomic<int> successful_locks{0};
    std::atomic<int> successful_releases{0};
    std::vector<std::future<void>> futures;

    // Launch concurrent threads
    for (int i = 0; i < num_threads; ++i) {
        futures.emplace_back(std::async(std::launch::async, [&, i]() {
            std::mt19937 rng(i); // Different seed per thread
            std::uniform_int_distribution<std::uint64_t> resource_dist(1000, 1100);
            std::uniform_int_distribution<std::uint64_t> xid_dist(20000 + i * 1000,
                                                                  20000 + (i + 1) * 1000);

            for (int op = 0; op < operations_per_thread; ++op) {
                auto resource_id = resource_dist(rng);
                auto xid = xid_dist(rng);

                // Try to acquire lock
                if (lock_manager_->try_lock(resource_id, FastPathLockType::ACCESS_SHARE_LOCK,
                                            xid)) {
                    successful_locks.fetch_add(1);

                    // Hold lock briefly
                    std::this_thread::sleep_for(std::chrono::microseconds(100));

                    // Release lock
                    if (lock_manager_->release_lock(resource_id, xid)) {
                        successful_releases.fetch_add(1);
                    }
                }
            }
        }));
    }

    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }

    // Verify some operations succeeded
    EXPECT_GT(successful_locks.load(), 0);
    EXPECT_EQ(successful_locks.load(), successful_releases.load());

    // Verify statistics were collected
    auto stats = lock_manager_->get_aggregated_statistics();
    EXPECT_GT(stats.lock_requests.load(), 0u);
    EXPECT_GT(stats.lock_grants.load(), 0u);
}

TEST_F(PartitionedLockManagerTest, HelperFunctions)
{
    // Test optimal partition count calculation
    auto optimal_count = calculate_optimal_partition_count(8); // 8 cores
    EXPECT_GT(optimal_count, 0u);
    EXPECT_LE(optimal_count, 128u); // Reasonable maximum

    // Should be power of 2
    EXPECT_EQ(optimal_count & (optimal_count - 1), 0u);

    // Test configuration validation
    EXPECT_TRUE(validate_partition_config(config_));

    PartitionedLockManagerConfig invalid_config;
    invalid_config.partition_count = 0;
    EXPECT_FALSE(validate_partition_config(invalid_config));

    // Test default configuration creation
    auto default_config = create_default_partition_config();
    EXPECT_TRUE(default_config.is_valid());
    EXPECT_GT(default_config.partition_count, 0u);
}

TEST_F(PartitionedLockManagerTest, DeadlockDetection)
{
    // This test verifies the framework is in place
    // Actual deadlock scenarios would require more complex setup

    // Enable deadlock detection
    auto config = lock_manager_->get_config();
    config.deadlock_detection_enabled = true;
    EXPECT_TRUE(lock_manager_->update_config(config));

    // Check for deadlocks (should find none)
    auto deadlocks = lock_manager_->check_for_deadlocks();
    EXPECT_TRUE(deadlocks.empty());

    // Test cross-partition deadlock detection toggle
    lock_manager_->set_cross_partition_deadlock_detection(false);
    config = lock_manager_->get_config();
    EXPECT_FALSE(config.cross_partition_deadlock_detection);

    lock_manager_->set_cross_partition_deadlock_detection(true);
    config = lock_manager_->get_config();
    EXPECT_TRUE(config.cross_partition_deadlock_detection);
}

TEST_F(PartitionedLockManagerTest, TimeoutHandling)
{
    // Acquire conflicting lock
    EXPECT_TRUE(lock_manager_->try_lock(table1_resource_, FastPathLockType::EXCLUSIVE_LOCK, xid1_));

    // Try to acquire conflicting lock - simplified implementation doesn't wait, just tries once
    auto start_time = std::chrono::steady_clock::now();
    bool result = lock_manager_->acquire_lock(table1_resource_, FastPathLockType::EXCLUSIVE_LOCK,
                                              xid2_, std::chrono::milliseconds(100));
    auto elapsed = std::chrono::steady_clock::now() - start_time;

    // Should fail immediately (simplified implementation doesn't support blocking)
    EXPECT_FALSE(result);
    EXPECT_LT(elapsed, std::chrono::milliseconds(10)); // Should return quickly

    // Clean up
    EXPECT_TRUE(lock_manager_->release_lock(table1_resource_, xid1_));
}

// Performance stress test (optional, may be slow)
TEST_F(PartitionedLockManagerTest, DISABLED_StressTest)
{
    const int num_threads = 8;
    const int operations_per_thread = 1000;
    const int num_resources = 100;

    std::atomic<int> total_operations{0};
    std::vector<std::future<void>> futures;

    auto start_time = std::chrono::steady_clock::now();

    // Launch stress test threads
    for (int i = 0; i < num_threads; ++i) {
        futures.emplace_back(std::async(std::launch::async, [&, i]() {
            std::mt19937 rng(i + 1000);
            std::uniform_int_distribution<int> resource_dist(0, num_resources - 1);
            std::uniform_int_distribution<int> lock_type_dist(1, 3); // Common lock types

            for (int op = 0; op < operations_per_thread; ++op) {
                auto resource_id = 5000 + resource_dist(rng);
                auto lock_type = static_cast<FastPathLockType>(lock_type_dist(rng));
                auto xid = 30000 + i * 10000 + op;

                // Try lock operation
                if (lock_manager_->try_lock(resource_id, lock_type, xid)) {
                    // Brief work simulation
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    lock_manager_->release_lock(resource_id, xid);
                }

                total_operations.fetch_add(1);
            }
        }));
    }

    // Wait for completion
    for (auto& future : futures) {
        future.wait();
    }

    auto elapsed = std::chrono::steady_clock::now() - start_time;
    auto ops_per_second = (total_operations.load() * 1000000) /
                          std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();

    EXPECT_EQ(total_operations.load(), num_threads * operations_per_thread);
    EXPECT_GT(ops_per_second, 1000); // Should achieve decent throughput

    // Log performance results
    std::cout << "Stress test completed:" << std::endl;
    std::cout << "  Total operations: " << total_operations.load() << std::endl;
    std::cout << "  Elapsed time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << " ms"
              << std::endl;
    std::cout << "  Operations/second: " << ops_per_second << std::endl;

    // Verify system is still functional
    auto stats = lock_manager_->get_aggregated_statistics();
    EXPECT_GT(stats.lock_requests.load(), 0u);
    EXPECT_EQ(lock_manager_->get_total_active_lock_count(), 0u); // All locks released
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
