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
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"
#include <filesystem>
#include <vector>
#include <thread>
#include <atomic>
#include <unistd.h>

using namespace scratchbird::core;

class TablespaceAutoextendTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Generate unique file prefix per test to avoid conflicts during parallel execution
        test_id_ = std::to_string(getpid()) + "_" + std::to_string(test_counter_++);
        db_path_ = "/tmp/test_autoextend_" + test_id_ + ".db";
        ts_autoextend_ = "test_ts_autoextend_" + test_id_ + ".sbts";
        ts_maxsize_ = "test_ts_maxsize_" + test_id_ + ".sbts";
        ts_concurrent_ = "test_ts_concurrent_" + test_id_ + ".sbts";
        ts_disabled_ = "test_ts_disabled_" + test_id_ + ".sbts";
        cleanup_test_files();
    }

    void TearDown() override
    {
        cleanup_test_files();
    }

    void cleanup_test_files()
    {
        std::filesystem::remove(db_path_);
        std::filesystem::remove(ts_autoextend_);
        std::filesystem::remove(ts_maxsize_);
        std::filesystem::remove(ts_concurrent_);
        std::filesystem::remove(ts_disabled_);
    }

    std::string test_id_;
    std::string db_path_;
    std::string ts_autoextend_;
    std::string ts_maxsize_;
    std::string ts_concurrent_;
    std::string ts_disabled_;
    static inline std::atomic<int> test_counter_{0};
};

// Test 1: Basic autoextend on page allocation
TEST_F(TablespaceAutoextendTest, AutoextendOnAllocation)
{
    // Create database
    ASSERT_EQ(Database::create(db_path_, 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_), Status::OK);

    PageManager *page_mgr = db.page_manager();
    ASSERT_NE(page_mgr, nullptr);

    // Create tablespace with very small size and autoextend enabled
    TablespaceConfig config;
    config.autoextend_enabled = true;
    config.autoextend_size_mb = 1;  // Extend by 1MB at a time
    config.max_size_mb = 10;        // Max 10MB
    config.prealloc_pages = 2;      // Start with only 2 pages (16KB)

    Status status = page_mgr->createTablespace(
        1, "test_ts_autoextend", ts_autoextend_.c_str(), config, nullptr
    );
    ASSERT_EQ(status, Status::OK) << "Failed to create tablespace";

    // Allocate pages until we trigger autoextend
    // With 2 preallocated pages (page 0 = header, page 1 = FSM), we have 0 data pages
    // First allocation should trigger extension

    std::vector<GPID> allocated_pages;

    // Allocate pages - first few should trigger autoextend
    for (int i = 0; i < 150; i++)  // 150 pages = ~1.2MB (should trigger 2 extensions)
    {
        GPID gpid;
        status = page_mgr->allocatePageInTablespace(1, &gpid, nullptr);
        ASSERT_EQ(status, Status::OK)
            << "Failed to allocate page " << i << " (status=" << static_cast<int>(status) << ")";

        allocated_pages.push_back(gpid);

        // Verify GPID is in correct tablespace
        EXPECT_EQ(getTablespaceID(gpid), 1) << "Page " << i << " allocated in wrong tablespace";
    }

    EXPECT_EQ(allocated_pages.size(), 150) << "Should have allocated 150 pages";

    // Verify all GPIDs are unique
    std::set<uint64_t> unique_page_numbers;
    for (const auto& gpid : allocated_pages)
    {
        unique_page_numbers.insert(getPageNumber(gpid));
    }
    EXPECT_EQ(unique_page_numbers.size(), 150) << "Some page numbers were duplicated";

    // Check metrics
    PageManager::TablespaceMetrics metrics;
    bool got_metrics = page_mgr->getTablespaceMetrics(1, &metrics);
    EXPECT_TRUE(got_metrics);
    if (got_metrics)
    {
        EXPECT_GT(metrics.extension_count, 0) << "Should have triggered at least one extension";
        EXPECT_GT(metrics.total_pages_added, 0) << "Should have added pages via extension";
        EXPECT_EQ(metrics.failed_extension_count, 0) << "Should have no failed extensions";
    }

    db.close();
}

// Test 2: MAXSIZE enforcement
TEST_F(TablespaceAutoextendTest, MaxsizeEnforcement)
{
    ASSERT_EQ(Database::create(db_path_, 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_), Status::OK);

    PageManager *page_mgr = db.page_manager();
    ASSERT_NE(page_mgr, nullptr);

    // Create tablespace with very small maxsize
    TablespaceConfig config;
    config.autoextend_enabled = true;
    config.autoextend_size_mb = 1;  // Extend by 1MB
    config.max_size_mb = 2;         // Max 2MB (~256 pages at 8KB)
    config.prealloc_pages = 2;

    Status status = page_mgr->createTablespace(
        1, "test_ts_maxsize", ts_maxsize_.c_str(), config, nullptr
    );
    ASSERT_EQ(status, Status::OK);

    // Allocate pages until we hit MAXSIZE
    std::vector<GPID> allocated_pages;
    bool hit_maxsize = false;

    for (int i = 0; i < 300; i++)  // Try to allocate more than maxsize allows
    {
        GPID gpid;
        status = page_mgr->allocatePageInTablespace(1, &gpid, nullptr);

        if (status == Status::PAGE_FULL)
        {
            hit_maxsize = true;
            break;
        }

        ASSERT_EQ(status, Status::OK) << "Unexpected error at page " << i;
        allocated_pages.push_back(gpid);
    }

    EXPECT_TRUE(hit_maxsize) << "Should have hit MAXSIZE limit";
    EXPECT_LT(allocated_pages.size(), 300) << "Should have stopped before allocating 300 pages";

    // Verify we're close to the maxsize (2MB = 256 pages, minus 2 for header+FSM = 254 data pages)
    // Allow some variance due to rounding
    EXPECT_GE(allocated_pages.size(), 200) << "Should have allocated close to maxsize";
    EXPECT_LE(allocated_pages.size(), 260) << "Should not exceed maxsize";

    // Check metrics
    PageManager::TablespaceMetrics metrics;
    bool got_metrics = page_mgr->getTablespaceMetrics(1, &metrics);
    EXPECT_TRUE(got_metrics);
    if (got_metrics)
    {
        EXPECT_GT(metrics.failed_extension_count, 0)
            << "Should have at least one failed extension (hitting MAXSIZE)";
    }

    db.close();
}

// Test 3: Concurrent allocations triggering extension (thread safety)
TEST_F(TablespaceAutoextendTest, ConcurrentExtension)
{
    ASSERT_EQ(Database::create(db_path_, 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_), Status::OK);

    PageManager *page_mgr = db.page_manager();
    ASSERT_NE(page_mgr, nullptr);

    // Create tablespace with small size
    TablespaceConfig config;
    config.autoextend_enabled = true;
    config.autoextend_size_mb = 1;
    config.max_size_mb = 20;  // Large enough for concurrent test
    config.prealloc_pages = 2;

    Status status = page_mgr->createTablespace(
        1, "test_ts_concurrent", ts_concurrent_.c_str(), config, nullptr
    );
    ASSERT_EQ(status, Status::OK);

    // Launch multiple threads that allocate pages concurrently
    const int num_threads = 4;
    const int pages_per_thread = 50;
    std::vector<std::thread> threads;
    std::vector<std::vector<GPID>> thread_pages(num_threads);
    std::vector<bool> thread_success(num_threads, true);

    for (int t = 0; t < num_threads; t++)
    {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < pages_per_thread; i++)
            {
                GPID gpid;
                Status alloc_status = page_mgr->allocatePageInTablespace(1, &gpid, nullptr);

                if (alloc_status != Status::OK)
                {
                    thread_success[t] = false;
                    return;
                }

                thread_pages[t].push_back(gpid);
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads)
    {
        thread.join();
    }

    // Verify all threads succeeded
    for (int t = 0; t < num_threads; t++)
    {
        EXPECT_TRUE(thread_success[t]) << "Thread " << t << " failed to allocate pages";
        EXPECT_EQ(thread_pages[t].size(), pages_per_thread)
            << "Thread " << t << " allocated wrong number of pages";
    }

    // Verify all allocated pages are unique (no duplicates across threads)
    std::set<uint64_t> all_page_numbers;
    for (int t = 0; t < num_threads; t++)
    {
        for (const auto& gpid : thread_pages[t])
        {
            uint64_t page_num = getPageNumber(gpid);
            EXPECT_EQ(all_page_numbers.count(page_num), 0)
                << "Duplicate page number " << page_num << " allocated";
            all_page_numbers.insert(page_num);
        }
    }

    EXPECT_EQ(all_page_numbers.size(), num_threads * pages_per_thread)
        << "Total unique pages should equal pages allocated";

    // Check that extension happened multiple times
    PageManager::TablespaceMetrics metrics;
    bool got_metrics = page_mgr->getTablespaceMetrics(1, &metrics);
    EXPECT_TRUE(got_metrics);
    if (got_metrics)
    {
        EXPECT_GT(metrics.extension_count, 1)
            << "Should have triggered multiple extensions with concurrent allocations";
    }

    db.close();
}

// Test 4: Extension failure handling (autoextend disabled)
TEST_F(TablespaceAutoextendTest, AutoextendDisabled)
{
    ASSERT_EQ(Database::create(db_path_, 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_), Status::OK);

    PageManager *page_mgr = db.page_manager();
    ASSERT_NE(page_mgr, nullptr);

    // Create tablespace with autoextend DISABLED
    TablespaceConfig config;
    config.autoextend_enabled = false;  // Disabled!
    config.autoextend_size_mb = 1;
    config.max_size_mb = 0;  // No limit
    config.prealloc_pages = 5;  // Only 5 pages (page 0,1 = header/FSM, 3 data pages)

    Status status = page_mgr->createTablespace(
        1, "test_ts_disabled", ts_disabled_.c_str(), config, nullptr
    );
    ASSERT_EQ(status, Status::OK);

    // Allocate the preallocated pages (should succeed)
    std::vector<GPID> allocated_pages;

    for (int i = 0; i < 10; i++)
    {
        GPID gpid;
        status = page_mgr->allocatePageInTablespace(1, &gpid, nullptr);

        if (status != Status::OK)
        {
            break;  // Ran out of space
        }

        allocated_pages.push_back(gpid);
    }

    // Should have allocated some pages but then failed (autoextend disabled)
    EXPECT_GT(allocated_pages.size(), 0) << "Should allocate at least some preallocated pages";
    EXPECT_LT(allocated_pages.size(), 10) << "Should run out of space before 10 pages";

    // Try one more allocation - should fail with INVALID_ARGUMENT (autoextend disabled)
    GPID gpid;
    status = page_mgr->allocatePageInTablespace(1, &gpid, nullptr);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT)
        << "Should fail with INVALID_ARGUMENT when autoextend is disabled";

    db.close();
}

// Test 5: Verify statistics tracking
TEST_F(TablespaceAutoextendTest, StatisticsTracking)
{
    ASSERT_EQ(Database::create(db_path_, 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_), Status::OK);

    PageManager *page_mgr = db.page_manager();
    CatalogManager *catalog = db.catalog_manager();
    ASSERT_NE(page_mgr, nullptr);
    ASSERT_NE(catalog, nullptr);

    // Create tablespace
    TablespaceConfig config;
    config.autoextend_enabled = true;
    config.autoextend_size_mb = 1;
    config.max_size_mb = 5;
    config.prealloc_pages = 2;

    Status status = page_mgr->createTablespace(
        1, "test_ts_autoextend", ts_autoextend_.c_str(), config, nullptr
    );
    ASSERT_EQ(status, Status::OK);

    // Get initial metrics
    PageManager::TablespaceMetrics metrics_before;
    bool got_metrics = page_mgr->getTablespaceMetrics(1, &metrics_before);
    EXPECT_TRUE(got_metrics);

    uint64_t initial_extension_count = got_metrics ? metrics_before.extension_count : 0;

    // Allocate enough pages to trigger at least 2 extensions
    for (int i = 0; i < 250; i++)
    {
        GPID gpid;
        status = page_mgr->allocatePageInTablespace(1, &gpid, nullptr);
        if (status != Status::OK)
        {
            break;  // Hit maxsize or error
        }
    }

    // Get final metrics
    PageManager::TablespaceMetrics metrics_after;
    got_metrics = page_mgr->getTablespaceMetrics(1, &metrics_after);
    EXPECT_TRUE(got_metrics);

    if (got_metrics)
    {
        // Verify extension count increased
        EXPECT_GT(metrics_after.extension_count, initial_extension_count)
            << "Extension count should have increased";

        // Verify pages were added
        EXPECT_GT(metrics_after.total_pages_added, 0)
            << "Should have tracked pages added";

        // Verify timestamps are set
        if (metrics_after.extension_count > 0)
        {
            EXPECT_GT(metrics_after.first_extension_time, 0)
                << "First extension time should be set";
            EXPECT_GT(metrics_after.last_extension_time, 0)
                << "Last extension time should be set";
            EXPECT_GE(metrics_after.last_extension_time, metrics_after.first_extension_time)
                << "Last extension should be >= first extension";
        }
    }

    db.close();
}
