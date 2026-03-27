/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * ThreadSanitizer Test: Buffer Pool Frame Metadata Race
 *
 * Tests CRITICAL-1 fix: BufferPool Frame Metadata Race Condition
 * - Issue: Multiple threads accessing frame->pin_count and frame->usage_count without synchronization
 * - Fix: Changed pin_count and usage_count to std::atomic<uint32_t>
 * - Validation: TSAN should detect NO data races
 *
 * Compilation:
 *   g++ -fsanitize=thread -g -O1 -std=c++17 \
 *       -I../../include -L../../build/src/core \
 *       test_buffer_pool_race.cpp -o test_buffer_pool_race \
 *       -lscratchbird_core -pthread
 *
 * Execution:
 *   ./test_buffer_pool_race
 *   Expected: "ThreadSanitizer: no issues found" + test passes
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <array>
#include <chrono>
#include <atomic>
#include <fstream>
#include "test_helpers.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;

namespace {

void writeTextFile(const std::filesystem::path& path, const std::string& contents)
{
    std::ofstream out(path);
    ASSERT_TRUE(out.is_open()) << path;
    out << contents;
}

} // namespace

class TSANBufferPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = uniqueTestDbPath("test_tsan_buffer_pool");
        std::remove(test_db_path_.c_str());

        ErrorContext ctx;
        Status status = Database::create(test_db_path_.c_str(), 8192, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<Database>();
        status = db_->open(test_db_path_.c_str(), &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;

        pool_ = db_->buffer_pool();
        ASSERT_NE(pool_, nullptr);

        // Create 1200 pages for testing (50 threads × 20 pages + buffer)
        // Test 2 needs pages 3-1003, so allocate 1200 to be safe
        for (int i = 0; i < 1200; ++i) {
            uint32_t page_id = 0;
            status = db_->page_manager()->allocatePage(page_id, &ctx);
            ASSERT_EQ(status, Status::OK);
        }
    }

    void TearDown() override {
        if (db_) {
            db_->close();
        }
        std::remove(test_db_path_.c_str());
    }

    std::string test_db_path_;
    std::unique_ptr<Database> db_;
    BufferPool* pool_;
};

class TSANSegmentedOwnershipTest : public ::testing::Test {
protected:
    static constexpr uint16_t kPartitionCount = 64;
    static constexpr uint32_t kPageSize = 16384;
    static constexpr uint16_t kHomePartitionCount = 32;

    void SetUp() override {
        Config::getInstance().clear();

        root_path_ = std::filesystem::path(uniqueTestDbPath("test_tsan_segmented_ownership", ""));
        std::filesystem::create_directories(root_path_);
        config_path_ = root_path_ / "sb_config.ini";
        test_db_path_ = root_path_ / "ownership_race.sbdb";

        writeTextFile(config_path_,
                      "[memory]\n"
                      "buffer_pool_size = 32\n"
                      "buffer_pool_layout = segmented\n"
                      "buffer_pool_bgwriter_enabled = false\n");

        ASSERT_EQ(Config::getInstance().initialize(config_path_.string(), &ctx_), Status::OK)
            << ctx_.message;
        ASSERT_EQ(Database::create(test_db_path_.string(), kPageSize, &ctx_), Status::OK)
            << ctx_.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_.string(), &ctx_), Status::OK) << ctx_.message;

        pool_ = db_->buffer_pool();
        ASSERT_NE(pool_, nullptr);

        const auto config = pool_->getConfigSnapshot();
        ASSERT_EQ(config.layout, BufferPool::PoolLayout::Segmented);
        ASSERT_EQ(config.pool_size, static_cast<uint32_t>(kHomePartitionCount));
    }

    void TearDown() override {
        if (db_) {
            db_->close();
        }
        Config::getInstance().clear();

        std::error_code ec;
        std::filesystem::remove_all(root_path_, ec);
    }

    static auto ownershipPartition(uint32_t page_id) -> uint16_t {
        return static_cast<uint16_t>(convertPageIDtoGPID(page_id) % kPartitionCount);
    }

    auto allocatePageForOwnershipPartition(uint16_t partition) -> uint32_t {
        for (int attempt = 0; attempt < 4096; ++attempt) {
            uint32_t page_id = 0;
            const Status status = db_->page_manager()->allocatePage(page_id, &ctx_);
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

    auto frameSnapshot(uint32_t page_id) -> BufferPool::MgaFrameSnapshot {
        BufferPool::MgaFrameSnapshot snapshot{};
        const Status status =
            pool_->getMgaFrameSnapshotGlobal(convertPageIDtoGPID(page_id), &snapshot, &ctx_);
        EXPECT_EQ(status, Status::OK) << ctx_.message;
        return snapshot;
    }

    std::filesystem::path root_path_;
    std::filesystem::path config_path_;
    std::filesystem::path test_db_path_;
    std::unique_ptr<Database> db_;
    BufferPool* pool_ = nullptr;
    ErrorContext ctx_;
};

/**
 * Test 1: Concurrent Pin/Unpin of Same Page
 *
 * This test stresses the atomic pin_count operations.
 * Multiple threads pin and unpin the same page concurrently.
 *
 * CRITICAL-1 Issue: Before fix, pin_count was uint32_t (non-atomic)
 * - Thread A: pin_count++ (read 0, write 1)
 * - Thread B: pin_count++ (read 0, write 1) - RACE!
 * - Result: pin_count = 1 instead of 2 → page evicted while in use
 *
 * After Fix: pin_count is std::atomic<uint32_t>
 * - Thread A: pin_count.fetch_add(1) (atomic)
 * - Thread B: pin_count.fetch_add(1) (atomic)
 * - Result: pin_count = 2 (correct)
 */
TEST_F(TSANBufferPoolTest, ConcurrentPinUnpinSamePage) {
    const int NUM_THREADS = 50;
    const int ITERATIONS = 1000;
    const uint32_t TEST_PAGE = 10;

    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext ctx;
            for (int i = 0; i < ITERATIONS; ++i) {
                void* buffer = nullptr;
                Status s = pool_->pinPage(TEST_PAGE, &buffer, &ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                    continue;
                }

                // Simulate some work
                std::this_thread::yield();

                s = pool_->unpinPage(TEST_PAGE, false, &ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Errors occurred during concurrent pin/unpin";

    // Verify buffer pool integrity
    auto stats = pool_->getStats();
    std::cout << "Buffer pool stats after test:\n";
    std::cout << "  Hits: " << stats.hits << "\n";
    std::cout << "  Misses: " << stats.misses << "\n";
    std::cout << "  Evictions: " << stats.evictions << "\n";
}

/**
 * Test 2: Concurrent Pin/Unpin of Different Pages
 *
 * This test stresses the frame allocation and page table race.
 * Multiple threads pin different pages, causing frequent cache misses.
 *
 * Validates:
 * - CRITICAL-1: Atomic pin_count/usage_count
 * - HIGH-1: Page table race (page_table_ updated before frame metadata)
 */
TEST_F(TSANBufferPoolTest, ConcurrentPinUnpinDifferentPages) {
    const int NUM_THREADS = 50;
    const int PAGES_PER_THREAD = 20;

    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext ctx;
            // Each thread works on its own set of pages
            uint32_t start_page = 3 + (t * PAGES_PER_THREAD);
            for (int i = 0; i < PAGES_PER_THREAD; ++i) {
                uint32_t page_id = start_page + i;
                void* buffer = nullptr;
                Status s = pool_->pinPage(page_id, &buffer, &ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                    continue;
                }

                // Simulate work
                std::this_thread::yield();

                s = pool_->unpinPage(page_id, false, &ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Errors occurred during concurrent pin/unpin";
}

/**
 * Test 3: Concurrent Usage Count Updates (Clock Sweep)
 *
 * This test stresses the atomic usage_count operations.
 * Multiple threads access pages, incrementing usage_count.
 * Background eviction decrements usage_count during clock sweep.
 *
 * CRITICAL-1 Issue: Before fix, usage_count was uint32_t (non-atomic)
 * - Increment and decrement operations could race
 * - Clock sweep could see inconsistent usage_count values
 *
 * After Fix: usage_count is std::atomic<uint32_t>
 * - All updates atomic
 * - Clock sweep sees consistent values
 */
TEST_F(TSANBufferPoolTest, ConcurrentUsageCountUpdates) {
    const int NUM_THREADS = 50;
    const int ITERATIONS = 500;
    const int UNIQUE_PAGES = 200; // Access 200 unique pages to force evictions

    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    // Create access pattern that triggers clock sweep
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext ctx;
            for (int i = 0; i < ITERATIONS; ++i) {
                // Access 200 unique pages in round-robin to trigger eviction
                // With buffer pool size ~32-128, this will definitely cause evictions
                uint32_t page_id = 3 + (i % UNIQUE_PAGES);
                void* buffer = nullptr;
                Status s = pool_->pinPage(page_id, &buffer, &ctx);
                if (s == Status::OK) {
                    pool_->unpinPage(page_id, false, &ctx);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);

    // Check that clock sweep happened
    auto stats = pool_->getStats();
    std::cout << "Clock sweep stats:\n";
    std::cout << "  Clock sweeps: " << stats.clock_sweeps << "\n";
    std::cout << "  Evictions: " << stats.evictions << "\n";
    std::cout << "  Buffer pool accessed: " << UNIQUE_PAGES << " unique pages\n";

    // With 200 unique pages and buffer pool size ~32-128, we should see evictions
    EXPECT_GT(stats.evictions, 0u) << "Expected evictions when accessing " << UNIQUE_PAGES << " pages";
}

TEST_F(TSANSegmentedOwnershipTest, ConcurrentSegmentedMissesTransferOwnershipToTargetPartitions) {
    const std::array<uint16_t, 4> partitions{33, 34, 35, 36};
    std::array<std::array<uint32_t, 2>, partitions.size()> pages{};
    for (size_t i = 0; i < partitions.size(); ++i) {
        pages[i][0] = allocatePageForOwnershipPartition(partitions[i]);
        pages[i][1] = allocatePageForOwnershipPartition(partitions[i]);
    }

    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    for (size_t i = 0; i < pages.size(); ++i) {
        threads.emplace_back([&, i]() {
            ErrorContext local_ctx;
            for (int iter = 0; iter < 200; ++iter) {
                void* buffer = nullptr;
                const uint32_t page_id = pages[i][static_cast<size_t>(iter % 2)];
                Status s = pool_->pinPage(page_id, &buffer, &local_ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }

                std::this_thread::yield();

                s = pool_->unpinPage(page_id, false, &local_ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(errors.load(), 0);

    for (size_t i = 0; i < pages.size(); ++i) {
        void* buffer = nullptr;
        ASSERT_EQ(pool_->pinPage(pages[i][0], &buffer, &ctx_), Status::OK) << ctx_.message;
        ASSERT_NE(buffer, nullptr);
        ASSERT_EQ(pool_->unpinPage(pages[i][0], false, &ctx_), Status::OK) << ctx_.message;

        const auto snapshot = frameSnapshot(pages[i][0]);
        EXPECT_TRUE(snapshot.resident);
        EXPECT_EQ(snapshot.owner_partition, partitions[i]);
        EXPECT_NE(snapshot.home_partition, snapshot.owner_partition);
    }
}

/**
 * Test 4: Concurrent Pin with Background Writer
 *
 * This test combines concurrent pin/unpin with background writer flush.
 * Validates that dirty flag and pin_count operations don't race.
 *
 * Validates:
 * - CRITICAL-1: Atomic pin_count
 * - Issue 1.21 (HIGH-20): Dirty bit race (verified false positive - no setDirty())
 */
TEST_F(TSANBufferPoolTest, ConcurrentPinWithBackgroundWriter) {
    const int NUM_THREADS = 30;
    const int ITERATIONS = 300;

    std::atomic<int> errors{0};
    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    // Background writer thread
    threads.emplace_back([&]() {
        ErrorContext ctx;
        while (!stop.load()) {
            pool_->flushAll(&ctx);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Pin/unpin threads with dirty writes
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext ctx;
            for (int i = 0; i < ITERATIONS; ++i) {
                uint32_t page_id = 3 + (i % 20);
                void* buffer = nullptr;
                Status s = pool_->pinPage(page_id, &buffer, &ctx);
                if (s == Status::OK) {
                    // Mark as dirty
                    bool dirty = (i % 2 == 0);
                    pool_->unpinPage(page_id, dirty, &ctx);
                }
            }
        });
    }

    // Wait for workers
    for (size_t i = 1; i < threads.size(); ++i) {
        threads[i].join();
    }

    // Stop background writer
    stop.store(true);
    threads[0].join();

    EXPECT_EQ(errors.load(), 0);
}

// Main function for standalone execution
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "ThreadSanitizer: Buffer Pool Race Test\n";
    std::cout << "========================================\n";
    std::cout << "Testing CRITICAL-1 fix:\n";
    std::cout << "  - Atomic pin_count operations\n";
    std::cout << "  - Atomic usage_count operations\n";
    std::cout << "  - Concurrent frame metadata access\n";
    std::cout << "\n";
    std::cout << "Expected: ThreadSanitizer reports NO data races\n";
    std::cout << "========================================\n\n";

    return RUN_ALL_TESTS();
}
