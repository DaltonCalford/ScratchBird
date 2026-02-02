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
 * Cache/buffer benchmarks for scan-resistant ring behavior.
 */

#include <gtest/gtest.h>

#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include "test_helpers.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using scratchbird::core::BufferPool;
using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::testing::TestDatabaseFile;

namespace {

struct HotAccessStats {
    uint64_t hits = 0;
    uint64_t misses = 0;
    double hit_ratio = 0.0;
    double scan_ms = 0.0;
    double hot_ms = 0.0;
};

}  // namespace

class CacheBufferBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("test_cache_buffer_benchmark");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 8192, &ctx), Status::OK)
            << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), Status::OK)
            << "Failed to open database: " << ctx.message;

        pool_ = db_->buffer_pool();
        ASSERT_NE(pool_, nullptr);

        constexpr uint32_t kTotalPages = 512;
        page_ids_.reserve(kTotalPages);
        for (uint32_t i = 0; i < kTotalPages; ++i) {
            uint32_t page_id = 0;
            ASSERT_EQ(db_->page_manager()->allocatePage(page_id, &ctx), Status::OK)
                << "Failed to allocate page: " << ctx.message;
            page_ids_.push_back(page_id);
        }
    }

    void TearDown() override {
        if (db_) {
            db_->close();
        }
        db_.reset();
        db_file_.reset();
    }

    void primeHotPages(const std::vector<uint32_t>& hot_pages) {
        ErrorContext ctx;
        for (uint32_t page_id : hot_pages) {
            void* buffer = nullptr;
            ASSERT_EQ(pool_->pinPage(page_id, &buffer, &ctx, BufferPool::AccessStrategy::Normal), Status::OK)
                << "Failed to pin page: " << ctx.message;
            ASSERT_EQ(pool_->unpinPage(page_id, false, &ctx), Status::OK)
                << "Failed to unpin page: " << ctx.message;
        }
    }

    HotAccessStats runScan(BufferPool::AccessStrategy scan_strategy,
                           const std::vector<uint32_t>& hot_pages,
                           const std::vector<uint32_t>& scan_pages) {
        ErrorContext ctx;
        primeHotPages(hot_pages);

        auto stats_before = pool_->getStats();

        auto scan_start = std::chrono::steady_clock::now();
        for (uint32_t page_id : scan_pages) {
            void* buffer = nullptr;
            Status status = pool_->pinPage(page_id, &buffer, &ctx, scan_strategy);
            if (status == Status::OK) {
                pool_->unpinPage(page_id, false, &ctx);
            }
        }
        auto scan_end = std::chrono::steady_clock::now();

        auto stats_after_scan = pool_->getStats();

        auto hot_start = std::chrono::steady_clock::now();
        for (uint32_t page_id : hot_pages) {
            void* buffer = nullptr;
            Status status = pool_->pinPage(page_id, &buffer, &ctx, BufferPool::AccessStrategy::Normal);
            if (status == Status::OK) {
                pool_->unpinPage(page_id, false, &ctx);
            }
        }
        auto hot_end = std::chrono::steady_clock::now();

        auto stats_after_hot = pool_->getStats();

        HotAccessStats stats;
        stats.hits = stats_after_hot.hits - stats_after_scan.hits;
        stats.misses = stats_after_hot.misses - stats_after_scan.misses;
        uint64_t total = stats.hits + stats.misses;
        stats.hit_ratio = total > 0 ? static_cast<double>(stats.hits) / total : 0.0;
        stats.scan_ms = std::chrono::duration<double, std::milli>(scan_end - scan_start).count();
        stats.hot_ms = std::chrono::duration<double, std::milli>(hot_end - hot_start).count();
        return stats;
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    BufferPool* pool_ = nullptr;
    std::vector<uint32_t> page_ids_;
};

TEST_F(CacheBufferBenchmarkTest, ScanResistanceBenchmark) {
    constexpr size_t kHotCount = 16;
    ASSERT_GE(page_ids_.size(), kHotCount + 1);

    std::vector<uint32_t> hot_pages(page_ids_.begin(), page_ids_.begin() + kHotCount);
    std::vector<uint32_t> scan_pages(page_ids_.begin() + kHotCount, page_ids_.end());

    auto normal = runScan(BufferPool::AccessStrategy::Normal, hot_pages, scan_pages);
    auto sequential = runScan(BufferPool::AccessStrategy::Sequential, hot_pages, scan_pages);

    std::cout << "[Benchmark] ScanResistance Normal hot_hit_ratio="
              << normal.hit_ratio << " scan_ms=" << normal.scan_ms
              << " hot_ms=" << normal.hot_ms << "\n";
    std::cout << "[Benchmark] ScanResistance Sequential hot_hit_ratio="
              << sequential.hit_ratio << " scan_ms=" << sequential.scan_ms
              << " hot_ms=" << sequential.hot_ms << "\n";

    EXPECT_GE(sequential.hits, normal.hits);
    EXPECT_LE(sequential.misses, normal.misses);
}
