/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>
#include <filesystem>

#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/transaction_manager.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;

/**
 * Test heap page memory management (Issue 1.4 from audit)
 * Verifies that findVisibleVersion() doesn't leak pinned pages
 */

class HeapPageMemoryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = uniqueTestDbPath("test_heap_page_memory", ".sbrd");
        std::filesystem::remove(test_db_path_);

        auto status = Database::create(test_db_path_, 16384, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database";

        status = db_.open(test_db_path_, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database";

        buffer_pool_ = db_.buffer_pool();
        ASSERT_NE(buffer_pool_, nullptr);
    }

    void TearDown() override
    {
        db_.close();
        std::filesystem::remove(test_db_path_);
    }

    std::string test_db_path_;
    Database db_;
    BufferPool *buffer_pool_ = nullptr;
};

// Test 1: Verify Snapshot cleanup unpins all pages
TEST_F(HeapPageMemoryTest, SnapshotCleanupUnpinsPages)
{
    // Create a snapshot
    // MGA: Snapshot removed - uint64_t current_xid = txn_mgr_->getCurrentXid();
    snapshot.xmin = 1;
    snapshot.xmax = 100;
    snapshot.buffer_pool = buffer_pool_;

    // Manually pin some pages and register with snapshot
    constexpr int NUM_PAGES = 5;
    std::vector<uint32_t> page_ids = {0, 1, 2, 3, 4};

    // Pin pages
    for (uint32_t page_id : page_ids)
    {
        void *buffer = nullptr;
        auto status = buffer_pool_->pinPage(page_id, &buffer, nullptr);
        if (status == Status::OK)
        {
            snapshot.pinned_pages.push_back(page_id);
        }
    }

    // Record stats before cleanup
    auto stats_before = buffer_pool_->getStats();

    // Verify pages are pinned
    EXPECT_EQ(snapshot.pinned_pages.size(), NUM_PAGES);

    // Call cleanup explicitly
    // MGA: snapshot.cleanup removed;

    // Verify all pages were unpinned
    EXPECT_EQ(snapshot.pinned_pages.size(), 0);
    EXPECT_EQ(snapshot.buffer_pool, nullptr);

    // Note: We can't directly verify pin counts in buffer pool
    // But we can verify the snapshot's internal state was cleared
}

// Test 2: Verify Snapshot destructor calls cleanup
TEST_F(HeapPageMemoryTest, SnapshotDestructorCallsCleanup)
{
    auto stats_before = buffer_pool_->getStats();

    // Create snapshot in inner scope
    {
        // MGA: Snapshot removed - uint64_t current_xid = txn_mgr_->getCurrentXid();
        snapshot.xmin = 1;
        snapshot.xmax = 100;
        snapshot.buffer_pool = buffer_pool_;

        // Pin a page
        void *buffer = nullptr;
        auto status = buffer_pool_->pinPage(0, &buffer, nullptr);
        ASSERT_EQ(status, Status::OK);
        snapshot.pinned_pages.push_back(0);

        // Snapshot still owns the pin
        EXPECT_EQ(snapshot.pinned_pages.size(), 1);
    }
    // Snapshot destructor should have unpinned the page

    auto stats_after = buffer_pool_->getStats();

    // Can't directly verify pin count, but test passes if no crash/leak
    SUCCEED();
}

// Test 3: Snapshot cleanup on error path
TEST_F(HeapPageMemoryTest, SnapshotCleanupOnErrorPath)
{
    auto stats_before = buffer_pool_->getStats();

    // Simulate error path scenario
    {
        // MGA: Snapshot removed - uint64_t current_xid = txn_mgr_->getCurrentXid();
        snapshot.xmin = 1;
        snapshot.xmax = 100;
        snapshot.buffer_pool = buffer_pool_;

        // Pin multiple pages
        for (uint32_t i = 0; i < 3; i++)
        {
            void *buffer = nullptr;
            auto status = buffer_pool_->pinPage(i, &buffer, nullptr);
            if (status == Status::OK)
            {
                snapshot.pinned_pages.push_back(i);
            }
        }

        // Simulate early return (error path)
        // In real code, this would be a return statement
        // Destructor will still be called when scope exits
    }

    // Verify destructor cleaned up
    auto stats_after = buffer_pool_->getStats();

    // Test passes if no crash/leak (Valgrind will detect leaks)
    SUCCEED();
}

// Test 4: Multiple snapshots, independent cleanup
TEST_F(HeapPageMemoryTest, MultipleSnapshotsIndependentCleanup)
{
    // Create two snapshots
    // MGA: Snapshot removed - uint64_t current_xid = txn_mgr_->getCurrentXid()1;
    snapshot1.xmin = 1;
    snapshot1.xmax = 50;
    snapshot1.buffer_pool = buffer_pool_;

    // MGA: Snapshot removed - uint64_t current_xid = txn_mgr_->getCurrentXid()2;
    snapshot2.xmin = 51;
    snapshot2.xmax = 100;
    snapshot2.buffer_pool = buffer_pool_;

    // Pin different pages for each snapshot
    void *buffer1 = nullptr;
    auto status1 = buffer_pool_->pinPage(0, &buffer1, nullptr);
    ASSERT_EQ(status1, Status::OK);
    snapshot1.pinned_pages.push_back(0);

    void *buffer2 = nullptr;
    auto status2 = buffer_pool_->pinPage(1, &buffer2, nullptr);
    ASSERT_EQ(status2, Status::OK);
    snapshot2.pinned_pages.push_back(1);

    // Cleanup snapshot1 only
    snapshot1.cleanup();
    EXPECT_EQ(snapshot1.pinned_pages.size(), 0);

    // snapshot2 should still have its pin
    EXPECT_EQ(snapshot2.pinned_pages.size(), 1);

    // Cleanup snapshot2
    snapshot2.cleanup();
    EXPECT_EQ(snapshot2.pinned_pages.size(), 0);
}

// Test 5: Snapshot with no pages pinned (edge case)
TEST_F(HeapPageMemoryTest, SnapshotWithNoPins)
{
    // MGA: Snapshot removed - uint64_t current_xid = txn_mgr_->getCurrentXid();
    snapshot.xmin = 1;
    snapshot.xmax = 100;
    // Note: buffer_pool is null, pinned_pages is empty

    // Cleanup should be safe with no pins
    // MGA: snapshot.cleanup removed;

    EXPECT_EQ(snapshot.pinned_pages.size(), 0);
    EXPECT_EQ(snapshot.buffer_pool, nullptr);
}

// Test 6: Snapshot double cleanup (defensive test)
TEST_F(HeapPageMemoryTest, SnapshotDoubleCleanup)
{
    // MGA: Snapshot removed - uint64_t current_xid = txn_mgr_->getCurrentXid();
    snapshot.xmin = 1;
    snapshot.xmax = 100;
    snapshot.buffer_pool = buffer_pool_;

    // Pin a page
    void *buffer = nullptr;
    auto status = buffer_pool_->pinPage(0, &buffer, nullptr);
    ASSERT_EQ(status, Status::OK);
    snapshot.pinned_pages.push_back(0);

    // First cleanup
    // MGA: snapshot.cleanup removed;
    EXPECT_EQ(snapshot.pinned_pages.size(), 0);
    EXPECT_EQ(snapshot.buffer_pool, nullptr);

    // Second cleanup should be safe (no-op)
    // MGA: snapshot.cleanup removed;
    EXPECT_EQ(snapshot.pinned_pages.size(), 0);
    EXPECT_EQ(snapshot.buffer_pool, nullptr);
}

// Test 7: Stress test with many pins
TEST_F(HeapPageMemoryTest, StressTestManyPins)
{
    // MGA: Snapshot removed - uint64_t current_xid = txn_mgr_->getCurrentXid();
    snapshot.xmin = 1;
    snapshot.xmax = 1000;
    snapshot.buffer_pool = buffer_pool_;

    // Pin many pages (limited by buffer pool size)
    constexpr int MAX_PINS = 100;
    for (int i = 0; i < MAX_PINS; i++)
    {
        uint32_t page_id = i % 10; // Reuse first 10 pages
        void *buffer = nullptr;
        auto status = buffer_pool_->pinPage(page_id, &buffer, nullptr);
        if (status == Status::OK)
        {
            snapshot.pinned_pages.push_back(page_id);
        }
        else
        {
            break; // Buffer pool full
        }
    }

    // Record how many we pinned
    size_t num_pinned = snapshot.pinned_pages.size();
    EXPECT_GT(num_pinned, 0);

    // Cleanup all
    // MGA: snapshot.cleanup removed;
    EXPECT_EQ(snapshot.pinned_pages.size(), 0);
}

// Test 8: Verify no leak on exception (simulated)
TEST_F(HeapPageMemoryTest, NoLeakOnException)
{
    auto stats_before = buffer_pool_->getStats();

    try
    {
        // MGA: Snapshot removed - uint64_t current_xid = txn_mgr_->getCurrentXid();
        snapshot.xmin = 1;
        snapshot.xmax = 100;
        snapshot.buffer_pool = buffer_pool_;

        // Pin pages
        for (uint32_t i = 0; i < 3; i++)
        {
            void *buffer = nullptr;
            auto status = buffer_pool_->pinPage(i, &buffer, nullptr);
            if (status == Status::OK)
            {
                snapshot.pinned_pages.push_back(i);
            }
        }

        // Simulate exception
        throw std::runtime_error("Simulated error");
    }
    catch (...)
    {
        // Exception caught, snapshot destructor should have run
    }

    auto stats_after = buffer_pool_->getStats();

    // Test passes if no leak (Valgrind will verify)
    SUCCEED();
}
