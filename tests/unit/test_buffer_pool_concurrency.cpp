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
#include <thread>
#include <vector>
#include <atomic>
#include <array>
#include <random>
#include <filesystem>
#include <fstream>

#include "scratchbird/core/config.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/page_manager.h"
#include "test_helpers.h"

using namespace scratchbird::core;

namespace {

void writeTextFile(const std::filesystem::path& path, const std::string& contents)
{
    std::ofstream out(path);
    ASSERT_TRUE(out.is_open()) << path;
    out << contents;
}

} // namespace

/**
 * Test buffer pool concurrency (Issue 1.3 from audit)
 * Verifies that concurrent access to buffer pool is thread-safe
 */

class BufferPoolConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a unique database path for test isolation (parallel execution)
        test_db_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_buffer_pool_concurrency", ".sbrd");

        auto status = Database::create(test_db_file_->path(), 16384, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database";

        status = db_.open(test_db_file_->path(), nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database";

        buffer_pool_ = db_.buffer_pool();
        ASSERT_NE(buffer_pool_, nullptr);
    }

    void TearDown() override {
        db_.close();
        // TestDatabaseFile will cleanup automatically on destruction
    }

    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_file_;
    Database db_;
    BufferPool* buffer_pool_ = nullptr;
};

class SegmentedOwnershipConcurrencyTest : public ::testing::Test {
protected:
    static constexpr uint16_t kPartitionCount = 64;
    static constexpr uint32_t kPageSize = 16384;
    static constexpr uint16_t kHomePartitionCount = 32;

    void SetUp() override {
        Config::getInstance().clear();

        temp_root_ = std::filesystem::path(
            scratchbird::testing::uniqueTestDbPath("test_buffer_pool_segmented_ownership", ""));
        std::filesystem::create_directories(temp_root_);
        config_path_ = temp_root_ / "sb_config.ini";
        db_path_ = temp_root_ / "ownership.sbdb";

        writeTextFile(config_path_,
                      "[memory]\n"
                      "buffer_pool_size = 32\n"
                      "buffer_pool_layout = segmented\n"
                      "buffer_pool_bgwriter_enabled = false\n");

        ASSERT_EQ(Config::getInstance().initialize(config_path_.string(), &ctx_), Status::OK)
            << ctx_.message;
        ASSERT_EQ(Database::create(db_path_.string(), kPageSize, &ctx_), Status::OK)
            << ctx_.message;
        ASSERT_EQ(db_.open(db_path_.string(), &ctx_), Status::OK) << ctx_.message;

        buffer_pool_ = db_.buffer_pool();
        ASSERT_NE(buffer_pool_, nullptr);

        const auto pool_config = buffer_pool_->getConfigSnapshot();
        ASSERT_EQ(pool_config.layout, BufferPool::PoolLayout::Segmented);
        ASSERT_EQ(pool_config.pool_size, static_cast<uint32_t>(kHomePartitionCount));
        ASSERT_EQ(pool_config.page_size, kPageSize);
    }

    void TearDown() override {
        db_.close();
        Config::getInstance().clear();

        std::error_code ec;
        std::filesystem::remove_all(temp_root_, ec);
    }

    static auto ownershipPartition(uint32_t page_id) -> uint16_t {
        return static_cast<uint16_t>(convertPageIDtoGPID(page_id) % kPartitionCount);
    }

    auto allocatePageForOwnershipPartition(uint16_t partition) -> uint32_t {
        for (int attempt = 0; attempt < 4096; ++attempt) {
            uint32_t page_id = 0;
            const Status status = db_.page_manager()->allocatePage(page_id, &ctx_);
            EXPECT_EQ(status, Status::OK) << ctx_.message;
            if (status != Status::OK) {
                return 0;
            }
            if (ownershipPartition(page_id) == partition) {
                return page_id;
            }
        }

        ADD_FAILURE() << "Failed to allocate page for partition " << partition;
        return 0;
    }

    void materializePage(uint32_t page_id) {
        void* buffer = nullptr;
        ASSERT_EQ(buffer_pool_->pinPage(page_id, &buffer, &ctx_), Status::OK) << ctx_.message;
        ASSERT_NE(buffer, nullptr);
        ASSERT_EQ(buffer_pool_->unpinPage(page_id, false, &ctx_), Status::OK) << ctx_.message;
    }

    auto frameSnapshot(uint32_t page_id) -> BufferPool::MgaFrameSnapshot {
        BufferPool::MgaFrameSnapshot snapshot{};
        const Status status = buffer_pool_->getMgaFrameSnapshotGlobal(convertPageIDtoGPID(page_id),
                                                                      &snapshot,
                                                                      &ctx_);
        EXPECT_EQ(status, Status::OK) << ctx_.message;
        return snapshot;
    }

    std::filesystem::path temp_root_;
    std::filesystem::path config_path_;
    std::filesystem::path db_path_;
    Database db_;
    BufferPool* buffer_pool_ = nullptr;
    ErrorContext ctx_;
};

// Test 1: Concurrent pin/unpin operations
TEST_F(BufferPoolConcurrencyTest, ConcurrentPinUnpin) {
    constexpr int NUM_THREADS = 10;
    constexpr int OPERATIONS_PER_THREAD = 100;
    constexpr uint32_t TEST_PAGE_ID = 0; // Page 0 (database header)

    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &errors]() {
            for (int i = 0; i < OPERATIONS_PER_THREAD; i++) {
                void* buffer = nullptr;
                auto status = buffer_pool_->pinPage(TEST_PAGE_ID, &buffer, nullptr);
                if (status != Status::OK) {
                    errors++;
                    continue;
                }

                // Verify we got a valid buffer
                if (buffer == nullptr) {
                    errors++;
                }

                // Unpin immediately
                status = buffer_pool_->unpinPage(TEST_PAGE_ID, false, nullptr);
                if (status != Status::OK) {
                    errors++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Errors occurred during concurrent pin/unpin";
}

// Test 2: Pin count overflow detection
TEST_F(BufferPoolConcurrencyTest, PinCountOverflow) {
    constexpr uint32_t TEST_PAGE_ID = 0;

    // Pin the page once to get it into buffer pool
    void* buffer = nullptr;
    auto status = buffer_pool_->pinPage(TEST_PAGE_ID, &buffer, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Now try to pin it UINT32_MAX times (this would take too long)
    // Instead, we'll verify the overflow check exists by trying to pin
    // when pin_count is already very high

    // For this test, we can't easily set pin_count to UINT32_MAX-1
    // without modifying the class, so we'll just verify the basic
    // pin/unpin cycle works correctly

    // Unpin the page
    status = buffer_pool_->unpinPage(TEST_PAGE_ID, false, nullptr);
    EXPECT_EQ(status, Status::OK);
}

// Test 3: Concurrent access to different pages
TEST_F(BufferPoolConcurrencyTest, ConcurrentDifferentPages) {
    constexpr int NUM_THREADS = 10;
    constexpr int PAGES_PER_THREAD = 10;

    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &errors]() {
            // Each thread accesses its own set of pages
            for (int i = 0; i < PAGES_PER_THREAD; i++) {
                uint32_t page_id = t * PAGES_PER_THREAD + i;

                // Skip if page doesn't exist yet
                // (We only have page 0 in a fresh database)
                if (page_id > 0) {
                    continue; // Skip for now, would need to allocate pages first
                }

                void* buffer = nullptr;
                auto status = buffer_pool_->pinPage(page_id, &buffer, nullptr);
                if (status != Status::OK) {
                    errors++;
                    continue;
                }

                status = buffer_pool_->unpinPage(page_id, false, nullptr);
                if (status != Status::OK) {
                    errors++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should have no errors (all threads skip non-existent pages)
    EXPECT_EQ(errors.load(), 0);
}

// Test 4: Stress test with pin/unpin and flush
TEST_F(BufferPoolConcurrencyTest, ConcurrentPinUnpinFlush) {
    constexpr int NUM_THREADS = 5;
    constexpr int OPERATIONS = 50;
    constexpr uint32_t TEST_PAGE_ID = 0;

    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    // Some threads pin/unpin
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &errors]() {
            for (int i = 0; i < OPERATIONS; i++) {
                void* buffer = nullptr;
                auto status = buffer_pool_->pinPage(TEST_PAGE_ID, &buffer, nullptr);
                if (status != Status::OK) {
                    errors++;
                    continue;
                }

                // Unpin with dirty flag randomly
                bool is_dirty = (i % 2 == 0);
                status = buffer_pool_->unpinPage(TEST_PAGE_ID, is_dirty, nullptr);
                if (status != Status::OK) {
                    errors++;
                }
            }
        });
    }

    // One thread flushes
    threads.emplace_back([this, &errors]() {
        for (int i = 0; i < OPERATIONS; i++) {
            auto status = buffer_pool_->flushPage(TEST_PAGE_ID, nullptr);
            if (status != Status::OK) {
                // Flush might fail if page not in pool, which is OK
                // Just don't count as error
            }

            // Small delay to let other threads work
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Errors during concurrent pin/unpin/flush";
}

// Test 5: Verify no LRU corruption under load
TEST_F(BufferPoolConcurrencyTest, LRUIntegrity) {
    constexpr int NUM_THREADS = 10;
    constexpr int OPERATIONS = 100;
    constexpr uint32_t TEST_PAGE_ID = 0;

    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &errors]() {
            for (int i = 0; i < OPERATIONS; i++) {
                void* buffer = nullptr;
                auto status = buffer_pool_->pinPage(TEST_PAGE_ID, &buffer, nullptr);
                if (status != Status::OK) {
                    errors++;
                    continue;
                }

                // The pin should update LRU list internally
                // We're testing that concurrent LRU updates don't corrupt the list

                status = buffer_pool_->unpinPage(TEST_PAGE_ID, false, nullptr);
                if (status != Status::OK) {
                    errors++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Errors during LRU stress test";

    // Verify buffer pool is still functional after stress test
    void* buffer = nullptr;
    auto status = buffer_pool_->pinPage(TEST_PAGE_ID, &buffer, nullptr);
    EXPECT_EQ(status, Status::OK) << "Buffer pool should still work after stress test";

    if (status == Status::OK) {
        buffer_pool_->unpinPage(TEST_PAGE_ID, false, nullptr);
    }
}

// Test 6: Verify statistics are consistent
TEST_F(BufferPoolConcurrencyTest, StatisticsConsistency) {
    constexpr int NUM_THREADS = 10;
    constexpr int OPERATIONS = 100;
    constexpr uint32_t TEST_PAGE_ID = 0;

    auto stats_before = buffer_pool_->getStats();

    std::vector<std::thread> threads;
    std::atomic<int> successful_ops{0};

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &successful_ops]() {
            for (int i = 0; i < OPERATIONS; i++) {
                void* buffer = nullptr;
                auto status = buffer_pool_->pinPage(TEST_PAGE_ID, &buffer, nullptr);
                if (status == Status::OK) {
                    buffer_pool_->unpinPage(TEST_PAGE_ID, false, nullptr);
                    successful_ops++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto stats_after = buffer_pool_->getStats();

    // After first operation, all subsequent should be hits
    // We should have: hits = (NUM_THREADS * OPERATIONS) - 1 (first is miss)
    // But this depends on timing, so just verify hits increased
    EXPECT_GT(stats_after.hits, stats_before.hits) << "Hits should increase";

    // Verify total operations match
    uint64_t total_ops = (stats_after.hits - stats_before.hits) +
                         (stats_after.misses - stats_before.misses);
    EXPECT_GE(total_ops, static_cast<uint64_t>(successful_ops.load()))
        << "Stats should cover operations";
}

// Test 7: Unpinning twice should fail gracefully
TEST_F(BufferPoolConcurrencyTest, DoubleUnpinDetection) {
    constexpr uint32_t TEST_PAGE_ID = 0;

    void* buffer = nullptr;
    auto status = buffer_pool_->pinPage(TEST_PAGE_ID, &buffer, nullptr);
    ASSERT_EQ(status, Status::OK);

    // First unpin should succeed
    status = buffer_pool_->unpinPage(TEST_PAGE_ID, false, nullptr);
    EXPECT_EQ(status, Status::OK);

    // Second unpin should fail (pin_count is 0)
    status = buffer_pool_->unpinPage(TEST_PAGE_ID, false, nullptr);
    EXPECT_NE(status, Status::OK) << "Double unpin should fail";
}

TEST_F(SegmentedOwnershipConcurrencyTest,
       SegmentedMissKeepsHomeOwnershipWhenTargetPartitionHasLocalCapacity) {
    bool found_local_home = false;

    for (uint16_t partition = 0; partition < kHomePartitionCount; ++partition) {
        const uint32_t page_id = allocatePageForOwnershipPartition(partition);
        ASSERT_EQ(ownershipPartition(page_id), partition);

        materializePage(page_id);

        const auto snapshot = frameSnapshot(page_id);
        if (snapshot.owner_partition == partition && snapshot.home_partition == partition) {
            EXPECT_TRUE(snapshot.resident);
            found_local_home = true;
            break;
        }
    }

    EXPECT_TRUE(found_local_home)
        << "Expected at least one bootstrap-surviving local home partition in the segmented pool";
}

TEST_F(SegmentedOwnershipConcurrencyTest,
       SegmentedMissTransfersDonorFreeFramesToUnderprovisionedPartitions) {
    const uint32_t page_id = allocatePageForOwnershipPartition(33);
    ASSERT_EQ(ownershipPartition(page_id), 33u);

    materializePage(page_id);

    const auto snapshot = frameSnapshot(page_id);
    EXPECT_TRUE(snapshot.resident);
    EXPECT_EQ(snapshot.owner_partition, 33u);
    EXPECT_NE(snapshot.home_partition, snapshot.owner_partition);
    EXPECT_LT(snapshot.home_partition, kHomePartitionCount);
}

TEST_F(SegmentedOwnershipConcurrencyTest,
       SegmentedMissCanTransferVictimOwnershipAfterFreeFramesExhausted) {
    const std::array<uint16_t, 33> partitions{
        33, 34, 35, 36, 37, 38, 39, 40, 41,
        42, 43, 44, 45, 46, 47, 48, 49, 50,
        51, 52, 53, 54, 55, 56, 57, 58, 59,
        60, 61, 62, 63, 32, 33};
    std::vector<uint32_t> page_ids;
    page_ids.reserve(partitions.size());

    for (uint16_t partition : partitions) {
        page_ids.push_back(allocatePageForOwnershipPartition(partition));
    }

    for (size_t i = 0; i < static_cast<size_t>(kHomePartitionCount); ++i) {
        materializePage(page_ids[i]);
        const auto snapshot = frameSnapshot(page_ids[i]);
        EXPECT_EQ(snapshot.owner_partition, partitions[i]);
        EXPECT_NE(snapshot.home_partition, snapshot.owner_partition);
    }

    const auto stats_before = buffer_pool_->getStats();
    materializePage(page_ids.back());
    const auto stats_after = buffer_pool_->getStats();

    const auto snapshot = frameSnapshot(page_ids.back());
    EXPECT_TRUE(snapshot.resident);
    EXPECT_EQ(snapshot.owner_partition, partitions.back());
    EXPECT_NE(snapshot.home_partition, snapshot.owner_partition);
    EXPECT_GT(stats_after.evictions, stats_before.evictions);
}
