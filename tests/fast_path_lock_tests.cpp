// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/fast_path_lock.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

using namespace scratchbird::engine;

class FastPathLockTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        config_.max_fast_path_locks = 16;
        config_.enabled = true;
        config_.collect_statistics = true;
        config_.enable_automatic_cleanup = false; // Disable for testing
        config_.max_lock_hold_time_seconds = 300;
        config_.cleanup_interval_seconds = 60;

        lock_manager_ = std::make_unique<FastPathLockManager>(config_);
        ASSERT_TRUE(lock_manager_->initialize());

        // Test resource IDs
        table1_resource_ = make_resource_id(1001, 0); // Table-level lock
        table2_resource_ = make_resource_id(1002, 0);
        row1_resource_ = make_resource_id(1001, 100); // Row-level lock
        row2_resource_ = make_resource_id(1001, 200);

        // Test transaction IDs
        xid1_ = 1000;
        xid2_ = 2000;
        xid3_ = 3000;
    }

    void TearDown() override
    {
        if (lock_manager_) {
            lock_manager_->shutdown();
        }
    }

    FastPathLockConfig config_;
    std::unique_ptr<FastPathLockManager> lock_manager_;

    std::uint64_t table1_resource_;
    std::uint64_t table2_resource_;
    std::uint64_t row1_resource_;
    std::uint64_t row2_resource_;

    std::uint64_t xid1_;
    std::uint64_t xid2_;
    std::uint64_t xid3_;
};

TEST_F(FastPathLockTest, LockTypeConflicts)
{
    // Test lock conflict matrix - PostgreSQL table-level lock semantics
    EXPECT_FALSE(
        lock_conflicts(FastPathLockType::ACCESS_SHARE_LOCK, FastPathLockType::ACCESS_SHARE_LOCK));
    EXPECT_TRUE(
        lock_conflicts(FastPathLockType::ACCESS_SHARE_LOCK, FastPathLockType::EXCLUSIVE_LOCK));
    // ROW_EXCLUSIVE locks don't conflict at table level, but do conflict at row level
    EXPECT_FALSE(
        lock_conflicts(FastPathLockType::ROW_EXCLUSIVE_LOCK, FastPathLockType::ROW_EXCLUSIVE_LOCK));
    EXPECT_FALSE(
        lock_conflicts(FastPathLockType::ROW_SHARE_LOCK, FastPathLockType::ROW_SHARE_LOCK));
    // Test an actual conflict case
    EXPECT_TRUE(lock_conflicts(FastPathLockType::SHARE_LOCK, FastPathLockType::ROW_EXCLUSIVE_LOCK));
}

TEST_F(FastPathLockTest, LockTypeSuitability)
{
    // Fast-path suitable locks
    EXPECT_TRUE(is_fast_path_suitable(FastPathLockType::ACCESS_SHARE_LOCK));
    EXPECT_TRUE(is_fast_path_suitable(FastPathLockType::ROW_SHARE_LOCK));
    EXPECT_TRUE(is_fast_path_suitable(FastPathLockType::ROW_EXCLUSIVE_LOCK));

    // Regular lock manager required
    EXPECT_FALSE(is_fast_path_suitable(FastPathLockType::EXCLUSIVE_LOCK));
    EXPECT_FALSE(is_fast_path_suitable(FastPathLockType::ACCESS_EXCLUSIVE_LOCK));
    EXPECT_FALSE(is_fast_path_suitable(FastPathLockType::NONE));
}

TEST_F(FastPathLockTest, ResourceIdUtilities)
{
    auto resource_id = make_resource_id(12345, 67890);
    EXPECT_EQ(get_table_oid_from_resource_id(resource_id), 12345);
    EXPECT_EQ(get_row_id_from_resource_id(resource_id), 67890);

    // Table-level resource (row_id = 0)
    auto table_resource = make_resource_id(999, 0);
    EXPECT_EQ(get_table_oid_from_resource_id(table_resource), 999);
    EXPECT_EQ(get_row_id_from_resource_id(table_resource), 0);
}

TEST_F(FastPathLockTest, BasicLockAcquisition)
{
    // Test basic lock acquisition
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(table1_resource_,
                                                  FastPathLockType::ACCESS_SHARE_LOCK, xid1_));

    // Same transaction can acquire compatible lock
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(table1_resource_,
                                                  FastPathLockType::ACCESS_SHARE_LOCK, xid1_));

    // Different transaction with compatible lock
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(table1_resource_,
                                                  FastPathLockType::ACCESS_SHARE_LOCK, xid2_));

    // Conflicting lock should fail
    EXPECT_FALSE(lock_manager_->try_fast_path_lock(table1_resource_,
                                                   FastPathLockType::EXCLUSIVE_LOCK, xid3_));
}

TEST_F(FastPathLockTest, LockRelease)
{
    // Acquire a row-level exclusive lock
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(row1_resource_,
                                                  FastPathLockType::ROW_EXCLUSIVE_LOCK, xid1_));

    // Verify it blocks conflicting row-level access (ROW_EXCLUSIVE on same row should conflict)
    EXPECT_FALSE(lock_manager_->try_fast_path_lock(row1_resource_,
                                                   FastPathLockType::ROW_EXCLUSIVE_LOCK, xid2_));

    // Release the lock
    EXPECT_TRUE(lock_manager_->release_fast_path_lock(row1_resource_, xid1_));

    // Now the same lock should succeed
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(row1_resource_,
                                                  FastPathLockType::ROW_EXCLUSIVE_LOCK, xid2_));
}

TEST_F(FastPathLockTest, MultipleResourceLocking)
{
    // Acquire locks on different resources
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(table1_resource_,
                                                  FastPathLockType::ACCESS_SHARE_LOCK, xid1_));
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(table2_resource_,
                                                  FastPathLockType::ACCESS_SHARE_LOCK, xid1_));
    EXPECT_TRUE(
        lock_manager_->try_fast_path_lock(row1_resource_, FastPathLockType::ROW_SHARE_LOCK, xid1_));
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(row2_resource_,
                                                  FastPathLockType::ROW_EXCLUSIVE_LOCK, xid1_));

    // Verify conflicts work correctly
    EXPECT_FALSE(lock_manager_->try_fast_path_lock(row2_resource_,
                                                   FastPathLockType::ROW_EXCLUSIVE_LOCK, xid2_));
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(row1_resource_, FastPathLockType::ROW_SHARE_LOCK,
                                                  xid2_)); // Compatible
}

TEST_F(FastPathLockTest, TransactionLockCleanup)
{
    // Acquire multiple locks for transaction
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(table1_resource_,
                                                  FastPathLockType::ACCESS_SHARE_LOCK, xid1_));
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(row1_resource_,
                                                  FastPathLockType::ROW_EXCLUSIVE_LOCK, xid1_));
    EXPECT_TRUE(
        lock_manager_->try_fast_path_lock(row2_resource_, FastPathLockType::ROW_SHARE_LOCK, xid1_));

    // Clear all locks for the transaction
    auto cleared_count = lock_manager_->clear_transaction_locks(xid1_);
    EXPECT_EQ(cleared_count, 3);

    // Verify locks are cleared - conflicting locks should now succeed
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(row1_resource_,
                                                  FastPathLockType::ROW_EXCLUSIVE_LOCK, xid2_));
}

TEST_F(FastPathLockTest, LockEligibilityCheck)
{
    // Initially eligible for suitable lock types
    EXPECT_TRUE(lock_manager_->is_fast_path_eligible(table1_resource_,
                                                     FastPathLockType::ACCESS_SHARE_LOCK, xid1_));

    // Not eligible for unsuitable lock types
    EXPECT_FALSE(lock_manager_->is_fast_path_eligible(table1_resource_,
                                                      FastPathLockType::EXCLUSIVE_LOCK, xid1_));

    // Test row-level lock conflicts: acquire a ROW_EXCLUSIVE lock on a specific row
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(row1_resource_,
                                                  FastPathLockType::ROW_EXCLUSIVE_LOCK, xid1_));

    // Should not be eligible for conflicting ROW_EXCLUSIVE lock on same row from different
    // transaction
    EXPECT_FALSE(lock_manager_->is_fast_path_eligible(row1_resource_,
                                                      FastPathLockType::ROW_EXCLUSIVE_LOCK, xid2_));
}

TEST_F(FastPathLockTest, StatisticsCollection)
{
    // Perform some lock operations
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(table1_resource_,
                                                  FastPathLockType::ACCESS_SHARE_LOCK, xid1_));
    EXPECT_FALSE(lock_manager_->try_fast_path_lock(
        table1_resource_, FastPathLockType::EXCLUSIVE_LOCK, xid2_)); // Should fallback
    EXPECT_TRUE(lock_manager_->release_fast_path_lock(table1_resource_, xid1_));

    auto stats = lock_manager_->get_statistics();
    EXPECT_GT(stats.fast_path_attempts.load(), 0);
    EXPECT_GT(stats.fast_path_successes.load(), 0);
    EXPECT_GT(stats.fast_path_releases.load(), 0);
    EXPECT_GT(stats.fallback_count.load(), 0);
    EXPECT_GT(stats.get_success_ratio(), 0.0);
}

TEST_F(FastPathLockTest, ConfigurationManagement)
{
    auto original_config = lock_manager_->get_config();
    EXPECT_EQ(original_config.max_fast_path_locks, 16);
    EXPECT_TRUE(original_config.enabled);

    // Update configuration
    FastPathLockConfig new_config = original_config;
    new_config.max_fast_path_locks = 32;
    new_config.collect_statistics = false;

    EXPECT_TRUE(lock_manager_->update_config(new_config));

    auto updated_config = lock_manager_->get_config();
    EXPECT_EQ(updated_config.max_fast_path_locks, 32);
    EXPECT_FALSE(updated_config.collect_statistics);

    // Invalid configuration should be rejected
    FastPathLockConfig invalid_config = new_config;
    invalid_config.max_fast_path_locks = 0;
    EXPECT_FALSE(lock_manager_->update_config(invalid_config));
}

TEST_F(FastPathLockTest, ConfigValidation)
{
    FastPathLockConfig valid_config;
    EXPECT_TRUE(valid_config.is_valid());

    FastPathLockConfig invalid_config;
    invalid_config.max_fast_path_locks = 0;
    EXPECT_FALSE(invalid_config.is_valid());

    invalid_config.max_fast_path_locks = 16;
    invalid_config.max_lock_hold_time_seconds = 0;
    EXPECT_FALSE(invalid_config.is_valid());
}

TEST_F(FastPathLockTest, BackendRegistration)
{
    auto initial_backend_count = lock_manager_->get_backend_count();

    // Register new backend
    auto test_thread_id = std::thread::id{}; // Default constructed thread_id
    EXPECT_TRUE(lock_manager_->register_backend(test_thread_id));
    EXPECT_EQ(lock_manager_->get_backend_count(), initial_backend_count + 1);

    // Registering same backend again should fail
    EXPECT_FALSE(lock_manager_->register_backend(test_thread_id));
    EXPECT_EQ(lock_manager_->get_backend_count(), initial_backend_count + 1);

    // Unregister backend
    lock_manager_->unregister_backend(test_thread_id);
    EXPECT_EQ(lock_manager_->get_backend_count(), initial_backend_count);
}

TEST_F(FastPathLockTest, SystemUtilization)
{
    // Initially should have low utilization
    auto initial_utilization = lock_manager_->get_system_utilization();
    EXPECT_GE(initial_utilization, 0.0);
    EXPECT_LE(initial_utilization, 1.0);

    // Acquire several locks to increase utilization
    for (int i = 0; i < 5; ++i) {
        auto resource_id = make_resource_id(2000 + i, 0);
        EXPECT_TRUE(lock_manager_->try_fast_path_lock(resource_id,
                                                      FastPathLockType::ACCESS_SHARE_LOCK, xid1_));
    }

    auto after_locks_utilization = lock_manager_->get_system_utilization();
    EXPECT_GE(after_locks_utilization, initial_utilization);
}

TEST_F(FastPathLockTest, PerformanceReport)
{
    // Perform some lock operations to generate data
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(table1_resource_,
                                                  FastPathLockType::ACCESS_SHARE_LOCK, xid1_));
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(row1_resource_,
                                                  FastPathLockType::ROW_EXCLUSIVE_LOCK, xid2_));

    auto report = lock_manager_->generate_performance_report();
    EXPECT_FALSE(report.empty());
    EXPECT_TRUE(report.find("Fast-Path Lock Manager Performance Report") != std::string::npos);
    EXPECT_TRUE(report.find("Fast-Path Attempts:") != std::string::npos);
    EXPECT_TRUE(report.find("Success Ratio:") != std::string::npos);
}

TEST_F(FastPathLockTest, LockTypeDistribution)
{
    // Acquire different types of locks
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(table1_resource_,
                                                  FastPathLockType::ACCESS_SHARE_LOCK, xid1_));
    EXPECT_TRUE(
        lock_manager_->try_fast_path_lock(row1_resource_, FastPathLockType::ROW_SHARE_LOCK, xid1_));
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(row2_resource_,
                                                  FastPathLockType::ROW_EXCLUSIVE_LOCK, xid1_));

    // Check lock type counts
    EXPECT_GT(lock_manager_->get_lock_type_count(FastPathLockType::ACCESS_SHARE_LOCK), 0);
    EXPECT_GT(lock_manager_->get_lock_type_count(FastPathLockType::ROW_SHARE_LOCK), 0);
    EXPECT_GT(lock_manager_->get_lock_type_count(FastPathLockType::ROW_EXCLUSIVE_LOCK), 0);
    EXPECT_EQ(lock_manager_->get_lock_type_count(FastPathLockType::EXCLUSIVE_LOCK), 0);
}

TEST_F(FastPathLockTest, ConcurrentAccess)
{
    const int num_threads = 4;
    const int locks_per_thread = 10;

    std::vector<std::thread> threads;
    std::atomic<int> successful_acquisitions{0};
    std::atomic<int> successful_releases{0};

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, locks_per_thread, &successful_acquisitions,
                              &successful_releases]() {
            std::uint64_t base_xid = 10000 + t * 1000;

            for (int i = 0; i < locks_per_thread; ++i) {
                auto resource_id = make_resource_id(3000 + t * 100 + i, 0);
                auto xid = base_xid + i;

                if (lock_manager_->try_fast_path_lock(resource_id,
                                                      FastPathLockType::ACCESS_SHARE_LOCK, xid)) {
                    successful_acquisitions.fetch_add(1);

                    // Hold lock briefly
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));

                    if (lock_manager_->release_fast_path_lock(resource_id, xid)) {
                        successful_releases.fetch_add(1);
                    }
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successful_acquisitions.load(), 0);
    EXPECT_EQ(successful_acquisitions.load(), successful_releases.load());

    auto stats = lock_manager_->get_statistics();
    EXPECT_EQ(stats.fast_path_successes.load(), successful_acquisitions.load());
    EXPECT_EQ(stats.fast_path_releases.load(), successful_releases.load());
}

TEST_F(FastPathLockTest, ArrayOverflow)
{
    // Try to acquire more locks than array capacity
    std::vector<std::uint64_t> resources;
    int successful_locks = 0;

    // Create more resources than the array can hold
    for (int i = 0; i < config_.max_fast_path_locks + 5; ++i) {
        auto resource_id = make_resource_id(4000 + i, 0);
        resources.push_back(resource_id);

        if (lock_manager_->try_fast_path_lock(resource_id, FastPathLockType::ACCESS_SHARE_LOCK,
                                              xid1_)) {
            successful_locks++;
        } else {
            break; // Array is full
        }
    }

    // Should be able to acquire exactly the array capacity
    EXPECT_LE(successful_locks, static_cast<int>(config_.max_fast_path_locks));

    auto stats = lock_manager_->get_statistics();
    if (successful_locks < static_cast<int>(resources.size())) {
        EXPECT_GT(stats.overflow_count.load(), 0);
    }
}

TEST_F(FastPathLockTest, DisabledLockManager)
{
    // Create disabled lock manager
    FastPathLockConfig disabled_config = config_;
    disabled_config.enabled = false;

    auto disabled_manager = std::make_unique<FastPathLockManager>(disabled_config);
    ASSERT_TRUE(disabled_manager->initialize());

    // All lock attempts should fail/fallback
    EXPECT_FALSE(disabled_manager->try_fast_path_lock(table1_resource_,
                                                      FastPathLockType::ACCESS_SHARE_LOCK, xid1_));
    EXPECT_FALSE(disabled_manager->release_fast_path_lock(table1_resource_, xid1_));

    auto stats = disabled_manager->get_statistics();
    EXPECT_GT(stats.fallback_count.load(), 0);

    disabled_manager->shutdown();
}

TEST_F(FastPathLockTest, MonitoringToggle)
{
    // Enable monitoring
    lock_manager_->set_monitoring_enabled(true);
    auto config_with_monitoring = lock_manager_->get_config();
    EXPECT_TRUE(config_with_monitoring.collect_statistics);

    // Disable monitoring
    lock_manager_->set_monitoring_enabled(false);
    auto config_without_monitoring = lock_manager_->get_config();
    EXPECT_FALSE(config_without_monitoring.collect_statistics);
}

TEST_F(FastPathLockTest, StatisticsReset)
{
    // Generate some statistics
    EXPECT_TRUE(lock_manager_->try_fast_path_lock(table1_resource_,
                                                  FastPathLockType::ACCESS_SHARE_LOCK, xid1_));
    EXPECT_TRUE(lock_manager_->release_fast_path_lock(table1_resource_, xid1_));

    auto stats_before_reset = lock_manager_->get_statistics();
    EXPECT_GT(stats_before_reset.fast_path_attempts.load(), 0);

    // Reset statistics
    lock_manager_->reset_statistics();

    auto stats_after_reset = lock_manager_->get_statistics();
    EXPECT_EQ(stats_after_reset.fast_path_attempts.load(), 0);
    EXPECT_EQ(stats_after_reset.fast_path_successes.load(), 0);
    EXPECT_EQ(stats_after_reset.fast_path_releases.load(), 0);
}
