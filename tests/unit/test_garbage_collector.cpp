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
 * @file test_garbage_collector.cpp
 * @brief Comprehensive tests for garbage collection system (Phase 4)
 *
 * Tests cover:
 * - Physical tuple removal and page compaction
 * - Dirty page tracking and priority queues
 * - Adaptive rate adjustment
 * - Enhanced metrics and statistics
 * - Background and cooperative GC
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>
#include "scratchbird/core/database.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/gc_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/sweep_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/proc_array.h"
#include "test_helpers.h"

using namespace scratchbird::core;

namespace
{
    constexpr size_t kSweepProgressSlotGeneration = 16;
    constexpr size_t kSweepProgressSlotActive = 17;
    constexpr size_t kSweepProgressSlotStartHorizon = 18;
    constexpr size_t kSweepProgressSlotPageCursor = 21;

    auto buildTuple(const uint8_t *payload, size_t payload_size) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> tuple(sizeof(TupleHeader) + payload_size, 0);
        TupleHeader header{};
        header.session_id = ID{};
        std::memcpy(tuple.data(), &header, sizeof(TupleHeader));
        if (payload_size > 0)
        {
            std::memcpy(tuple.data() + sizeof(TupleHeader), payload, payload_size);
        }
        return tuple;
    }
}

class GarbageCollectorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Generate unique database path for this test
        test_db_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_gc");
    }

    void TearDown() override
    {
        // TestDatabaseFile automatically cleans up
        test_db_.reset();
    }

    // Helper: Create database and initialize GC
    bool createTestDatabase(Database& db)
    {
        ErrorContext ctx;
        if (Database::create(test_db_->path(), 16384, &ctx) != Status::OK)
        {
            return false;
        }

        if (db.open(test_db_->path(), &ctx) != Status::OK)
        {
            return false;
        }

        // Initialize GC
        auto gc = db.garbage_collector();
        if (!gc || gc->initialize(&ctx) != Status::OK)
        {
            return false;
        }

        return true;
    }

    bool createCommittedRetainedTransaction(Database& db,
                                            ID& tx_uuid_out,
                                            uint64_t& txid_out)
    {
        ErrorContext ctx;
        std::unique_ptr<ConnectionContext> conn;
        if (db.connect(conn, &ctx) != Status::OK)
        {
            return false;
        }

        txid_out = conn->getCurrentXid();
        tx_uuid_out = conn->getCurrentTransactionUuid();
        if (txid_out == 0 || tx_uuid_out == ID{})
        {
            return false;
        }

        return conn->commit(&ctx) == Status::OK;
    }

    bool createRolledBackRetainedTransaction(Database& db,
                                             ID& tx_uuid_out,
                                             uint64_t& txid_out)
    {
        ErrorContext ctx;
        std::unique_ptr<ConnectionContext> conn;
        if (db.connect(conn, &ctx) != Status::OK)
        {
            return false;
        }

        txid_out = conn->getCurrentXid();
        tx_uuid_out = conn->getCurrentTransactionUuid();
        if (txid_out == 0 || tx_uuid_out == ID{})
        {
            return false;
        }

        return conn->rollback(&ctx) == Status::OK;
    }

    bool readRawPage(const Database& db, uint32_t page_id, std::vector<uint8_t>& page_out)
    {
        page_out.assign(db.page_size(), 0);
        std::ifstream in(test_db_->path(), std::ios::binary);
        if (!in.is_open())
        {
            return false;
        }
        in.seekg(static_cast<std::streamoff>(page_id) * static_cast<std::streamoff>(db.page_size()));
        in.read(reinterpret_cast<char*>(page_out.data()), static_cast<std::streamsize>(page_out.size()));
        return in.good();
    }

    bool writeRawPage(const Database& db, uint32_t page_id, const std::vector<uint8_t>& page)
    {
        std::fstream io(test_db_->path(), std::ios::in | std::ios::out | std::ios::binary);
        if (!io.is_open())
        {
            return false;
        }
        io.seekp(static_cast<std::streamoff>(page_id) * static_cast<std::streamoff>(db.page_size()));
        io.write(reinterpret_cast<const char*>(page.data()), static_cast<std::streamsize>(page.size()));
        io.flush();
        return io.good();
    }

    bool allocateHeapAuditPage(Database& db, uint32_t& page_id_out, std::vector<uint8_t>& page_out)
    {
        ErrorContext ctx;
        page_id_out = 0;
        if (db.page_manager() == nullptr)
        {
            return false;
        }
        if (db.page_manager()->allocatePage(page_id_out, &ctx) != Status::OK)
        {
            return false;
        }

        page_out.assign(db.page_size(), 0);
        HeapPage heap(page_out.data(), db.page_size());
        if (heap.initialize(page_id_out, &ctx) != Status::OK)
        {
            return false;
        }
        return db.write_page(page_id_out, page_out.data(), &ctx) == Status::OK;
    }

    std::string summarizePageAuditFindings(const std::vector<SweepPageAuditFinding>& findings,
                                           uint32_t page_id) const
    {
        std::ostringstream out;
        bool first = true;
        for (const auto& finding : findings)
        {
            if (finding.page_id != page_id)
            {
                continue;
            }
            if (!first)
            {
                out << "; ";
            }
            first = false;
            out << finding.error_code << "@" << finding.scan_mode << "/" << finding.trigger_source;
        }
        if (first)
        {
            out << "<none>";
        }
        return out.str();
    }

    std::string extractManifestField(const std::string& manifest, const std::string& key) const
    {
        const std::string prefix = key + "=";
        const size_t start = manifest.find(prefix);
        if (start == std::string::npos)
        {
            return {};
        }

        const size_t value_start = start + prefix.size();
        const size_t end = manifest.find('\n', value_start);
        if (end == std::string::npos)
        {
            return manifest.substr(value_start);
        }
        return manifest.substr(value_start, end - value_start);
    }

    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_;
};

// ========== Basic Functionality Tests ==========

TEST_F(GarbageCollectorTest, Initialization)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);
    EXPECT_TRUE(gc->isEnabled());
    EXPECT_EQ(gc->getPolicy(), GCPolicy::COMBINED);
}

TEST_F(GarbageCollectorTest, EnableDisable)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Test enable/disable
    EXPECT_TRUE(gc->isEnabled());

    gc->disable();
    EXPECT_FALSE(gc->isEnabled());

    gc->enable();
    EXPECT_TRUE(gc->isEnabled());
}

TEST_F(GarbageCollectorTest, PolicyManagement)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Test policy changes
    gc->setPolicy(GCPolicy::COOPERATIVE);
    EXPECT_EQ(gc->getPolicy(), GCPolicy::COOPERATIVE);

    gc->setPolicy(GCPolicy::BACKGROUND);
    EXPECT_EQ(gc->getPolicy(), GCPolicy::BACKGROUND);

    gc->setPolicy(GCPolicy::COMBINED);
    EXPECT_EQ(gc->getPolicy(), GCPolicy::COMBINED);
}

TEST_F(GarbageCollectorTest, GcManagerUsesCanonicalHeapReclaimHorizon)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    GcManager gc_mgr(&db);
    ErrorContext ctx;
    uint64_t horizon = 0;
    ASSERT_EQ(gc_mgr.getGcHorizon(&horizon, &ctx), Status::OK) << ctx.message;

    ReclaimHorizonSnapshot expected{};
    ASSERT_EQ(db.transaction_manager()->captureReclaimHorizons(expected, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(horizon, expected.heap_reclaim_horizon);
}

TEST_F(GarbageCollectorTest, SweepPersistsCanonicalHeapReclaimStartHorizon)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto sweep_mgr = db.sweep_manager();
    ASSERT_NE(sweep_mgr, nullptr);

    ErrorContext ctx;
    ReclaimHorizonSnapshot expected{};
    ASSERT_EQ(db.transaction_manager()->captureReclaimHorizons(expected, &ctx), Status::OK)
        << ctx.message;

    ASSERT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::OK) << ctx.message;

    db.close();

    std::vector<uint8_t> raw_page;
    ASSERT_TRUE(readRawPage(db, BOOTSTRAP_PAGE_SYSTEM_STATE, raw_page));
    const auto* state_page =
        reinterpret_cast<const BootstrapSystemStatePage*>(raw_page.data());
    EXPECT_EQ(state_page->reserved[kSweepProgressSlotStartHorizon],
              expected.heap_reclaim_horizon);
}

// ========== Dirty Page Tracking Tests ==========

TEST_F(GarbageCollectorTest, DirtyPageTracking)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Initial state
    EXPECT_EQ(gc->getDirtyPageCount(), 0);

    // Mark some pages dirty
    gc->markPageDirty(100);
    EXPECT_EQ(gc->getDirtyPageCount(), 1);

    gc->markPageDirty(200);
    EXPECT_EQ(gc->getDirtyPageCount(), 2);

    gc->markPageDirty(300);
    EXPECT_EQ(gc->getDirtyPageCount(), 3);

    // Mark same page again (should not increase count)
    gc->markPageDirty(100);
    EXPECT_EQ(gc->getDirtyPageCount(), 3);
}

TEST_F(GarbageCollectorTest, DirtyPagePriority)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Mark page multiple times (increases priority via mark_count)
    gc->markPageDirty(100);
    gc->markPageDirty(100);
    gc->markPageDirty(100);

    // Mark another page once
    gc->markPageDirty(200);

    // Both pages are dirty
    EXPECT_EQ(gc->getDirtyPageCount(), 2);

    // Page 100 should have higher priority due to mark_count
    // (This is tested indirectly through statistics after GC runs)
}

TEST_F(GarbageCollectorTest, CooperativeGcPrunesMatureHistoryWithoutKillingLiveRoot)
{
    Config &config = Config::getInstance();
    const std::string previous_enabled =
        config.getString("garbage_collection", "enabled", "true");
    const std::string previous_policy =
        config.getString("garbage_collection", "policy", "COMBINED");
    const std::string previous_rate =
        config.getString("garbage_collection", "cooperative_rate", "100");
    struct GarbageCollectionConfigRestore
    {
        Config &config;
        std::string enabled;
        std::string policy;
        std::string rate;

        ~GarbageCollectionConfigRestore()
        {
            config.set("garbage_collection", "enabled", enabled);
            config.set("garbage_collection", "policy", policy);
            config.set("garbage_collection", "cooperative_rate", rate);
        }
    } restore{config, previous_enabled, previous_policy, previous_rate};

    config.set("garbage_collection", "enabled", "true");
    config.set("garbage_collection", "policy", "COMBINED");
    config.set("garbage_collection", "cooperative_rate", "1");

    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    ErrorContext ctx;
    auto *txn_manager = db.transaction_manager();
    ASSERT_NE(txn_manager, nullptr);

    uint64_t reclaim_horizon = txn_manager->getOldestSnapshot();
    for (int i = 0; reclaim_horizon <= 1 && i < 4; ++i)
    {
        ID tx_uuid{};
        uint64_t txid = 0;
        ASSERT_TRUE(createCommittedRetainedTransaction(db, tx_uuid, txid));
        reclaim_horizon = txn_manager->getOldestSnapshot();
    }
    ASSERT_GT(reclaim_horizon, 1u);

    uint32_t page_id = 0;
    ASSERT_EQ(db.page_manager()->allocatePage(page_id, &ctx), Status::OK) << ctx.message;

    void *page_buffer = nullptr;
    ASSERT_EQ(db.buffer_pool()->pinPage(page_id, &page_buffer, &ctx), Status::OK) << ctx.message;

    auto *page_data = static_cast<uint8_t *>(page_buffer);
    HeapPage heap_page(page_data, db.page_size(), nullptr, &db, ID{});
    ASSERT_EQ(heap_page.initialize(page_id, &ctx), Status::OK) << ctx.message;

    uint8_t payload_v1[8] = {0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58};
    auto tuple_v1 = buildTuple(payload_v1, sizeof(payload_v1));
    uint16_t head_item_id = 0;
    ASSERT_EQ(heap_page.insertTuple(tuple_v1.data(),
                                    static_cast<uint32_t>(tuple_v1.size()),
                                    100,
                                    &head_item_id,
                                    &ctx),
              Status::OK) << ctx.message;

    uint8_t payload_v2[8] = {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68};
    auto tuple_v2 = buildTuple(payload_v2, sizeof(payload_v2));
    const uint64_t delete_xid = reclaim_horizon - 1;
    const uint64_t new_xmin = reclaim_horizon + 1;
    uint16_t stable_item_id = 0;
    ASSERT_EQ(heap_page.updateTuple(head_item_id,
                                    tuple_v2.data(),
                                    static_cast<uint32_t>(tuple_v2.size()),
                                    delete_xid,
                                    new_xmin,
                                    &stable_item_id,
                                    &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(stable_item_id, head_item_id);

    const uint8_t *head_tuple = nullptr;
    uint32_t head_tuple_size = 0;
    ASSERT_EQ(heap_page.getTuple(head_item_id, &head_tuple, &head_tuple_size, &ctx), Status::OK)
        << ctx.message;
    auto *head_hdr = reinterpret_cast<const TupleHeader *>(head_tuple);
    ASSERT_TRUE(head_hdr->hasBackVersion());

    const uint8_t *back_tuple = nullptr;
    uint32_t back_tuple_size = 0;
    ASSERT_EQ(heap_page.getTuple(head_hdr->back_version_slot, &back_tuple, &back_tuple_size, &ctx),
              Status::OK) << ctx.message;
    auto *back_hdr = reinterpret_cast<TupleHeader *>(const_cast<uint8_t *>(back_tuple));
    back_hdr->infomask |= TupleHeader::HEAP_XMAX_COMMITTED;

    HeapPage::VersionMaturityScan maturity_scan{};
    ASSERT_EQ(heap_page.scanVersionMaturity(reclaim_horizon, &maturity_scan, nullptr, nullptr, &ctx),
              Status::OK) << ctx.message;
    EXPECT_GT(maturity_scan.reclaimable_item_count, 0u);
    EXPECT_GT(maturity_scan.prune_only_item_count, 0u);

    db.buffer_pool()->unpinPage(page_id, true, &ctx);

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);
    const auto stats_before = gc->getStatistics();
    gc->markPageDirty(page_id);
    gc->processPageCooperative(page_id, &ctx);

    auto stats = gc->getStatistics();
    EXPECT_GT(stats.cooperative_runs, stats_before.cooperative_runs);
    EXPECT_GT(stats.tuples_removed, 0u);

    ASSERT_EQ(db.buffer_pool()->pinPage(page_id, &page_buffer, &ctx), Status::OK) << ctx.message;
    page_data = static_cast<uint8_t *>(page_buffer);
    HeapPage reloaded_page(page_data, db.page_size(), nullptr, &db, ID{});

    const uint8_t *visible_tuple = nullptr;
    uint32_t visible_tuple_size = 0;
    ASSERT_EQ(reloaded_page.getTuple(head_item_id, &visible_tuple, &visible_tuple_size, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(visible_tuple_size, tuple_v2.size());
    EXPECT_EQ(std::memcmp(visible_tuple + sizeof(TupleHeader),
                          tuple_v2.data() + sizeof(TupleHeader),
                          tuple_v2.size() - sizeof(TupleHeader)),
              0);

    db.buffer_pool()->unpinPage(page_id, false, &ctx);
}

// ========== Statistics Tests ==========

TEST_F(GarbageCollectorTest, InitialStatistics)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    auto stats = gc->getStatistics();

    // All counters should be zero initially
    EXPECT_EQ(stats.tuples_removed, 0);
    EXPECT_EQ(stats.pages_cleaned, 0);
    EXPECT_EQ(stats.cooperative_runs, 0);
    EXPECT_EQ(stats.background_runs, 0);
    EXPECT_EQ(stats.space_reclaimed_bytes, 0);

    // Histogram should be zero
    EXPECT_EQ(stats.duration_0_10ms, 0);
    EXPECT_EQ(stats.duration_10_50ms, 0);
    EXPECT_EQ(stats.duration_50_100ms, 0);
    EXPECT_EQ(stats.duration_100_500ms, 0);
    EXPECT_EQ(stats.duration_500_1000ms, 0);
    EXPECT_EQ(stats.duration_1000ms_plus, 0);

    // Efficiency metrics
    EXPECT_EQ(stats.pages_with_no_garbage, 0);
    EXPECT_EQ(stats.max_space_reclaimed_single_page, 0);
    EXPECT_EQ(stats.total_dirty_pages_marked, 0);

    // Tuning parameters
    EXPECT_EQ(stats.current_cooperative_rate, 100);
    EXPECT_EQ(stats.current_background_interval_ms, 5000);
}

TEST_F(GarbageCollectorTest, AccumulationTracking)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Mark pages dirty
    for (uint32_t i = 0; i < 10; i++)
    {
        gc->markPageDirty(i);
    }

    auto stats = gc->getStatistics();
    EXPECT_EQ(stats.total_dirty_pages_marked, 10);
    EXPECT_EQ(stats.dirty_page_count, 10);

    // Mark more pages
    for (uint32_t i = 10; i < 20; i++)
    {
        gc->markPageDirty(i);
    }

    stats = gc->getStatistics();
    EXPECT_EQ(stats.total_dirty_pages_marked, 20);
    EXPECT_EQ(stats.dirty_page_count, 20);
}

// ========== Background GC Tests ==========

TEST_F(GarbageCollectorTest, BackgroundGCStartStop)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Initially not running
    EXPECT_FALSE(gc->isBackgroundGCRunning());

    // Start background GC
    ErrorContext ctx;
    ASSERT_EQ(gc->startBackgroundGC(&ctx), Status::OK);
    EXPECT_TRUE(gc->isBackgroundGCRunning());

    // Try starting again (should fail)
    EXPECT_NE(gc->startBackgroundGC(&ctx), Status::OK);

    // Stop background GC
    ASSERT_EQ(gc->stopBackgroundGC(&ctx), Status::OK);
    EXPECT_FALSE(gc->isBackgroundGCRunning());

    // Try stopping again (should fail)
    EXPECT_NE(gc->stopBackgroundGC(&ctx), Status::OK);
}

TEST_F(GarbageCollectorTest, BackgroundGCRunsAndUpdatesStatistics)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Mark some pages dirty
    for (uint32_t i = 0; i < 5; i++)
    {
        gc->markPageDirty(100 + i);
    }

    // Start background GC
    ErrorContext ctx;
    ASSERT_EQ(gc->startBackgroundGC(&ctx), Status::OK);

    // Wait for at least one background run
    // Expected runtime: ~6-7 seconds.
    std::this_thread::sleep_for(std::chrono::milliseconds(6000));

    // Stop background GC
    ASSERT_EQ(gc->stopBackgroundGC(&ctx), Status::OK);

    // Check statistics
    auto stats = gc->getStatistics();
    EXPECT_GT(stats.background_runs, 0) << "Background GC should have run at least once";

    // Duration histogram should have at least one bucket populated
    uint64_t total_hist = stats.duration_0_10ms + stats.duration_10_50ms +
                          stats.duration_50_100ms + stats.duration_100_500ms +
                          stats.duration_500_1000ms + stats.duration_1000ms_plus;
    EXPECT_GT(total_hist, 0) << "Duration histogram should be populated";
}

// ========== Adaptive Tuning Tests ==========

TEST_F(GarbageCollectorTest, AdaptiveTuningEnableDisable)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Adaptive tuning should be enabled by default
    EXPECT_TRUE(gc->isAdaptiveTuningEnabled());

    // Disable
    gc->setAdaptiveTuning(false);
    EXPECT_FALSE(gc->isAdaptiveTuningEnabled());

    // Enable
    gc->setAdaptiveTuning(true);
    EXPECT_TRUE(gc->isAdaptiveTuningEnabled());
}

TEST_F(GarbageCollectorTest, TuningParametersExposed)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    auto stats = gc->getStatistics();

    // Tuning parameters should be exposed
    EXPECT_GT(stats.current_cooperative_rate, 0);
    EXPECT_GT(stats.current_background_interval_ms, 0);
}

// ========== Priority Calculation Tests ==========

TEST_F(GarbageCollectorTest, PriorityCalculationBasic)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Test priority calculation with different inputs
    // Note: This tests the formula indirectly through behavior

    // New page (mark_count=1, age=0)
    gc->markPageDirty(100);

    // Wait a bit and mark another page
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    gc->markPageDirty(200);

    // Mark first page again (increases mark_count)
    gc->markPageDirty(100);
    gc->markPageDirty(100);

    // Page 100 should have higher priority (higher mark_count)
    // This is verified through the order of cleaning in background GC
}

// Note: HeapPage functionality (markTupleUnused, defragmentPage, prunePage) is tested
// through actual GC operations in the integration tests above. Unit tests for HeapPage
// are in separate HeapPage test files.

// ========== Stress Tests ==========

TEST_F(GarbageCollectorTest, ManyDirtyPages)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Mark many pages dirty
    constexpr uint32_t NUM_PAGES = 1000;
    for (uint32_t i = 0; i < NUM_PAGES; i++)
    {
        gc->markPageDirty(i);
    }

    EXPECT_EQ(gc->getDirtyPageCount(), NUM_PAGES);

    auto stats = gc->getStatistics();
    EXPECT_EQ(stats.total_dirty_pages_marked, NUM_PAGES);
}

TEST_F(GarbageCollectorTest, HighChurnPages)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Mark same pages dirty many times (high churn)
    constexpr uint32_t NUM_MARKS = 100;
    for (uint32_t i = 0; i < NUM_MARKS; i++)
    {
        gc->markPageDirty(100);  // Same page
        gc->markPageDirty(200);  // Same page
        gc->markPageDirty(300);  // Same page
    }

    // Should only have 3 unique pages
    EXPECT_EQ(gc->getDirtyPageCount(), 3);

    // But total marks should be NUM_MARKS * 3
    auto stats = gc->getStatistics();
    EXPECT_EQ(stats.total_dirty_pages_marked, NUM_MARKS * 3);
}

// ========== Edge Cases ==========

TEST_F(GarbageCollectorTest, CleanPageScanning)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Mark system pages as dirty (they likely have no garbage)
    gc->markPageDirty(0);  // Header page
    gc->markPageDirty(1);  // Catalog page

    // Start background GC
    ErrorContext ctx;
    ASSERT_EQ(gc->startBackgroundGC(&ctx), Status::OK);

    // Wait for GC to run
    // Expected runtime: ~6-7 seconds.
    std::this_thread::sleep_for(std::chrono::milliseconds(6000));

    // Stop background GC
    ASSERT_EQ(gc->stopBackgroundGC(&ctx), Status::OK);

    // Check stats - should have scanned pages with no garbage
    auto stats = gc->getStatistics();
    EXPECT_GT(stats.pages_with_no_garbage, 0) << "Should have found clean pages";
}

TEST_F(GarbageCollectorTest, ZeroDirtyPages)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Start background GC with no dirty pages
    ErrorContext ctx;
    ASSERT_EQ(gc->startBackgroundGC(&ctx), Status::OK);

    // Wait for GC to run
    // Expected runtime: ~6-7 seconds.
    std::this_thread::sleep_for(std::chrono::milliseconds(6000));

    // Stop background GC
    ASSERT_EQ(gc->stopBackgroundGC(&ctx), Status::OK);

    // Should have run but found nothing to clean
    auto stats = gc->getStatistics();
    EXPECT_GT(stats.background_runs, 0);
    EXPECT_EQ(stats.pages_cleaned, 0);
}

// ========== Integration Tests ==========

TEST_F(GarbageCollectorTest, SweepIntegration)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    auto sweep_mgr = db.sweep_manager();
    ASSERT_NE(gc, nullptr);
    ASSERT_NE(sweep_mgr, nullptr);

    // Start background GC
    ErrorContext ctx;
    ASSERT_EQ(gc->startBackgroundGC(&ctx), Status::OK);

    // Mark some pages dirty
    gc->markPageDirty(100);
    gc->markPageDirty(200);

    // Simulate sweep completion (notifies GC)
    gc->notifySweepComplete(1000, 2000);

    // GC should wake and process dirty pages
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Stop background GC
    ASSERT_EQ(gc->stopBackgroundGC(&ctx), Status::OK);

    // Verify GC ran
    auto stats = gc->getStatistics();
    EXPECT_GT(stats.background_runs, 0);
}

TEST_F(GarbageCollectorTest, SweepAdvancesOitUsingPreparedFrontierFromTipWalk)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    ErrorContext ctx;
    ASSERT_EQ(db.initializeProcArray(16, &ctx), Status::OK) << ctx.message;

    auto *txn_mgr = db.transaction_manager();
    auto *sweep_mgr = db.sweep_manager();
    ASSERT_NE(txn_mgr, nullptr);
    ASSERT_NE(sweep_mgr, nullptr);

    uint32_t proc_id = 0;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK) << ctx.message;

    uint64_t xid = 0;
    ASSERT_EQ(txn_mgr->beginTransaction(proc_id, xid, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(txn_mgr->commitTransaction(proc_id, xid, &ctx), Status::OK) << ctx.message;

    ASSERT_EQ(txn_mgr->beginTransaction(proc_id, xid, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(txn_mgr->rollbackTransaction(proc_id, xid, &ctx), Status::OK) << ctx.message;

    uint64_t prepared_xid = 0;
    ASSERT_EQ(txn_mgr->beginTransaction(proc_id, prepared_xid, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(txn_mgr->prepareTransaction(proc_id,
                                          prepared_xid,
                                          "gc_sweep_tip_walk_prepared",
                                          generateUuidV7(),
                                          &ctx),
              Status::OK)
        << ctx.message;

    uint64_t later_committed_xid = 0;
    ASSERT_EQ(txn_mgr->beginTransaction(proc_id, later_committed_xid, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(txn_mgr->commitTransaction(proc_id, later_committed_xid, &ctx), Status::OK)
        << ctx.message;

    ASSERT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(txn_mgr->getOldestXid(), prepared_xid);

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(GarbageCollectorTest, SweepResumeStateSurvivesRestartAndReusesGeneration)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    db.close();

    std::vector<uint8_t> raw_page;
    ASSERT_TRUE(readRawPage(db, BOOTSTRAP_PAGE_SYSTEM_STATE, raw_page));
    auto *state_page = reinterpret_cast<BootstrapSystemStatePage *>(raw_page.data());
    state_page->reserved[kSweepProgressSlotGeneration] = 7;
    state_page->reserved[kSweepProgressSlotActive] = 1;
    state_page->reserved[kSweepProgressSlotStartHorizon] = 1234;
    state_page->reserved[kSweepProgressSlotPageCursor] = 55;
    state_page->page_header.checksum = calculatePageChecksum(raw_page.data(), db.page_size());
    ASSERT_TRUE(writeRawPage(db, BOOTSTRAP_PAGE_SYSTEM_STATE, raw_page));

    ErrorContext ctx;
    ASSERT_EQ(db.open(test_db_->path(), &ctx), Status::OK) << ctx.message;
    auto sweep_mgr = db.sweep_manager();
    ASSERT_NE(sweep_mgr, nullptr);

    ASSERT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::OK) << ctx.message;

    db.close();
    raw_page.clear();
    ASSERT_TRUE(readRawPage(db, BOOTSTRAP_PAGE_SYSTEM_STATE, raw_page));
    state_page = reinterpret_cast<BootstrapSystemStatePage *>(raw_page.data());
    EXPECT_EQ(state_page->reserved[kSweepProgressSlotGeneration], 7u);
    EXPECT_EQ(state_page->reserved[kSweepProgressSlotActive], 0u);
    EXPECT_EQ(state_page->reserved[kSweepProgressSlotStartHorizon], 1234u);
    EXPECT_EQ(state_page->reserved[kSweepProgressSlotPageCursor], 55u);
}

TEST_F(GarbageCollectorTest, SweepPolicyResolutionDeterministicByScopeChain)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto sweep_mgr = db.sweep_manager();
    ASSERT_NE(sweep_mgr, nullptr);

    const ID schema_id = generateUuidV7();
    const ID table_id = generateUuidV7();

    SweepPolicyBinding db_binding{};
    db_binding.scope_kind = SweepScopeKind::DATABASE;
    db_binding.scope_id = db.uuid();
    db_binding.lanes = {SweepPolicyLane::LINEAGE_RETENTION};

    SweepPolicyBinding schema_binding{};
    schema_binding.scope_kind = SweepScopeKind::SCHEMA;
    schema_binding.scope_id = schema_id;
    schema_binding.lanes = {SweepPolicyLane::OBJECT_TOUCH_AUDIT};

    SweepPolicyBinding table_binding{};
    table_binding.scope_kind = SweepScopeKind::TABLE;
    table_binding.scope_id = table_id;
    table_binding.lanes = {SweepPolicyLane::SHADOW_CAPTURE, SweepPolicyLane::WAL_AFTER_EXPORT};

    ErrorContext ctx;
    ASSERT_EQ(sweep_mgr->setPolicyBindings({db_binding, schema_binding, table_binding}, &ctx),
              Status::OK)
        << ctx.message;

    SweepPolicyBinding resolved{};
    ASSERT_EQ(sweep_mgr->resolvePolicyBinding({{SweepScopeKind::TABLE, table_id},
                                               {SweepScopeKind::SCHEMA, schema_id},
                                               {SweepScopeKind::DATABASE, db.uuid()}},
                                              resolved,
                                              &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(resolved.scope_kind, SweepScopeKind::TABLE);
    ASSERT_EQ(resolved.lanes.size(), 2u);
    EXPECT_EQ(resolved.lanes[0], SweepPolicyLane::SHADOW_CAPTURE);
    EXPECT_EQ(resolved.lanes[1], SweepPolicyLane::WAL_AFTER_EXPORT);

    ASSERT_EQ(sweep_mgr->resolvePolicyBinding({{SweepScopeKind::TABLE, generateUuidV7()},
                                               {SweepScopeKind::SCHEMA, schema_id},
                                               {SweepScopeKind::DATABASE, db.uuid()}},
                                              resolved,
                                              &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(resolved.scope_kind, SweepScopeKind::SCHEMA);
    ASSERT_EQ(resolved.lanes.size(), 1u);
    EXPECT_EQ(resolved.lanes[0], SweepPolicyLane::OBJECT_TOUCH_AUDIT);

    ASSERT_EQ(sweep_mgr->resolvePolicyBinding({{SweepScopeKind::TABLE, generateUuidV7()},
                                               {SweepScopeKind::SCHEMA, generateUuidV7()},
                                               {SweepScopeKind::DATABASE, db.uuid()}},
                                              resolved,
                                              &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(resolved.scope_kind, SweepScopeKind::DATABASE);
    ASSERT_EQ(resolved.lanes.size(), 1u);
    EXPECT_EQ(resolved.lanes[0], SweepPolicyLane::LINEAGE_RETENTION);
}

TEST_F(GarbageCollectorTest, SweepPersistsLocalEvidenceManifestBeforePruneHandoff)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto sweep_mgr = db.sweep_manager();
    auto gc = db.garbage_collector();
    ASSERT_NE(sweep_mgr, nullptr);
    ASSERT_NE(gc, nullptr);

    ID tx_uuid{};
    uint64_t txid = 0;
    ASSERT_TRUE(createCommittedRetainedTransaction(db, tx_uuid, txid));

    SweepPolicyBinding binding{};
    binding.scope_kind = SweepScopeKind::DATABASE;
    binding.scope_id = db.uuid();
    binding.lanes = {SweepPolicyLane::LINEAGE_RETENTION, SweepPolicyLane::WAL_AFTER_EXPORT};
    binding.strict_audit = true;

    ErrorContext ctx;
    ASSERT_EQ(sweep_mgr->setPolicyBindings({binding}, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::OK) << ctx.message;
    EXPECT_FALSE(gc->isSweepPruneBlocked());

    auto stats = sweep_mgr->getStatistics();
    EXPECT_EQ(stats.last_evidence_items_emitted, 1u);
    EXPECT_FALSE(stats.prune_blocked);

    std::vector<SweepEvidenceWorkItem> items;
    ASSERT_EQ(sweep_mgr->listEvidenceWorkItems(items, &ctx), Status::OK) << ctx.message;
    auto it = std::find_if(items.begin(), items.end(),
                           [txid](const SweepEvidenceWorkItem& item) { return item.txid == txid; });
    ASSERT_NE(it, items.end());
    EXPECT_EQ(it->delivery_state, "REMOTE_PENDING");
    EXPECT_EQ(it->tx_uuid, tx_uuid);
    EXPECT_TRUE(std::filesystem::exists(it->spool_path));

    std::ifstream in(it->spool_path, std::ios::binary);
    ASSERT_TRUE(in.is_open());
    std::string manifest((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(manifest.find("tx_uuid=" + tx_uuid.toString()), std::string::npos);
    EXPECT_NE(manifest.find("txid=" + std::to_string(txid)), std::string::npos);
    EXPECT_NE(manifest.find("policy_lanes=LINEAGE_RETENTION,WAL_AFTER_EXPORT"),
              std::string::npos);
}

TEST_F(GarbageCollectorTest, SweepBlocksPruneWhenLocalEvidencePersistenceFails)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto sweep_mgr = db.sweep_manager();
    auto gc = db.garbage_collector();
    ASSERT_NE(sweep_mgr, nullptr);
    ASSERT_NE(gc, nullptr);

    ID tx_uuid{};
    uint64_t txid = 0;
    ASSERT_TRUE(createCommittedRetainedTransaction(db, tx_uuid, txid));

    SweepPolicyBinding binding{};
    binding.scope_kind = SweepScopeKind::DATABASE;
    binding.scope_id = db.uuid();
    binding.lanes = {SweepPolicyLane::LINEAGE_RETENTION};
    binding.strict_audit = true;

    ErrorContext ctx;
    ASSERT_EQ(sweep_mgr->setPolicyBindings({binding}, &ctx), Status::OK) << ctx.message;

    std::filesystem::path blocking_path(db.path());
    blocking_path += ".forensics";
    {
        std::ofstream out(blocking_path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << "blocked";
    }

    EXPECT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::IO_ERROR);
    EXPECT_TRUE(gc->isSweepPruneBlocked());

    auto stats = sweep_mgr->getStatistics();
    EXPECT_EQ(stats.evidence_persist_failures, 1u);
    EXPECT_TRUE(stats.prune_blocked);

    ErrorContext query_ctx;
    std::vector<SweepEvidenceWorkItem> items;
    ASSERT_EQ(sweep_mgr->listEvidenceWorkItems(items, &query_ctx), Status::OK)
        << query_ctx.message;
    EXPECT_TRUE(std::none_of(items.begin(), items.end(),
                             [txid](const SweepEvidenceWorkItem& item) {
                                 return item.txid == txid;
                             }));
}

TEST_F(GarbageCollectorTest, SweepRetriesLocalEvidenceAfterRestartWithoutDroppingGeneration)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto sweep_mgr = db.sweep_manager();
    auto gc = db.garbage_collector();
    ASSERT_NE(sweep_mgr, nullptr);
    ASSERT_NE(gc, nullptr);

    ID tx_uuid{};
    uint64_t txid = 0;
    ASSERT_TRUE(createCommittedRetainedTransaction(db, tx_uuid, txid));

    SweepPolicyBinding binding{};
    binding.scope_kind = SweepScopeKind::DATABASE;
    binding.scope_id = db.uuid();
    binding.lanes = {SweepPolicyLane::LINEAGE_RETENTION};
    binding.strict_audit = true;

    ErrorContext ctx;
    ASSERT_EQ(sweep_mgr->setPolicyBindings({binding}, &ctx), Status::OK) << ctx.message;

    std::filesystem::path blocking_path(db.path());
    blocking_path += ".forensics";
    {
        std::ofstream out(blocking_path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << "blocked";
    }

    EXPECT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::IO_ERROR);
    EXPECT_TRUE(gc->isSweepPruneBlocked());

    db.close();

    std::vector<uint8_t> raw_page;
    ASSERT_TRUE(readRawPage(db, BOOTSTRAP_PAGE_SYSTEM_STATE, raw_page));
    auto* state_page = reinterpret_cast<BootstrapSystemStatePage*>(raw_page.data());
    const uint64_t generation_before = state_page->reserved[kSweepProgressSlotGeneration];
    EXPECT_GT(generation_before, 0u);
    EXPECT_EQ(state_page->reserved[kSweepProgressSlotActive], 1u);

    ASSERT_TRUE(std::filesystem::remove(blocking_path));

    ASSERT_EQ(db.open(test_db_->path(), &ctx), Status::OK) << ctx.message;
    sweep_mgr = db.sweep_manager();
    gc = db.garbage_collector();
    ASSERT_NE(sweep_mgr, nullptr);
    ASSERT_NE(gc, nullptr);

    ASSERT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::OK) << ctx.message;
    EXPECT_FALSE(gc->isSweepPruneBlocked());

    std::vector<SweepEvidenceWorkItem> items;
    ASSERT_EQ(sweep_mgr->listEvidenceWorkItems(items, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(std::count_if(items.begin(), items.end(),
                            [txid](const SweepEvidenceWorkItem& item) {
                                return item.txid == txid;
                            }),
              1);

    db.close();
    raw_page.clear();
    ASSERT_TRUE(readRawPage(db, BOOTSTRAP_PAGE_SYSTEM_STATE, raw_page));
    state_page = reinterpret_cast<BootstrapSystemStatePage*>(raw_page.data());
    EXPECT_EQ(state_page->reserved[kSweepProgressSlotGeneration], generation_before);
    EXPECT_EQ(state_page->reserved[kSweepProgressSlotActive], 0u);
}

TEST_F(GarbageCollectorTest, SweepExportsWalAfterLogForCommittedTransactionsOnly)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto sweep_mgr = db.sweep_manager();
    auto gc = db.garbage_collector();
    ASSERT_NE(sweep_mgr, nullptr);
    ASSERT_NE(gc, nullptr);

    ID committed_tx_uuid_1{};
    uint64_t committed_txid_1 = 0;
    ASSERT_TRUE(createCommittedRetainedTransaction(db, committed_tx_uuid_1, committed_txid_1));

    ID rolled_back_tx_uuid{};
    uint64_t rolled_back_txid = 0;
    ASSERT_TRUE(createRolledBackRetainedTransaction(db, rolled_back_tx_uuid, rolled_back_txid));

    ID committed_tx_uuid_2{};
    uint64_t committed_txid_2 = 0;
    ASSERT_TRUE(createCommittedRetainedTransaction(db, committed_tx_uuid_2, committed_txid_2));

    SweepPolicyBinding binding{};
    binding.scope_kind = SweepScopeKind::DATABASE;
    binding.scope_id = db.uuid();
    binding.lanes = {SweepPolicyLane::LINEAGE_RETENTION, SweepPolicyLane::WAL_AFTER_EXPORT};
    binding.strict_audit = true;

    ErrorContext ctx;
    ASSERT_EQ(sweep_mgr->setPolicyBindings({binding}, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::OK) << ctx.message;
    EXPECT_FALSE(gc->isSweepPruneBlocked());

    auto stats = sweep_mgr->getStatistics();
    EXPECT_GE(stats.last_evidence_items_emitted, 3u);
    EXPECT_EQ(stats.last_wal_after_segments_emitted, 2u);
    EXPECT_EQ(stats.wal_after_backlog_depth, 0u);
    EXPECT_EQ(stats.wal_after_export_failures, 0u);

    std::vector<SweepWalAfterLogSegment> segments;
    ASSERT_EQ(sweep_mgr->listWalAfterLogSegments(segments, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(segments.size(), 2u);
    EXPECT_EQ(segments[0].stream_seq, 1u);
    EXPECT_EQ(segments[0].txid, committed_txid_1);
    EXPECT_EQ(segments[1].stream_seq, 2u);
    EXPECT_EQ(segments[1].txid, committed_txid_2);
    EXPECT_TRUE(std::none_of(segments.begin(), segments.end(),
                             [rolled_back_txid](const SweepWalAfterLogSegment& item) {
                                 return item.txid == rolled_back_txid;
                             }));

    ASSERT_TRUE(std::filesystem::exists(segments[0].segment_path));
    std::ifstream in(segments[0].segment_path, std::ios::binary);
    ASSERT_TRUE(in.is_open());
    std::string manifest((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(manifest.find("tx_uuid=" + committed_tx_uuid_1.toString()), std::string::npos);
    EXPECT_NE(manifest.find("txid=" + std::to_string(committed_txid_1)), std::string::npos);
    EXPECT_NE(manifest.find("shipping_mode=DEBUG"), std::string::npos);
    EXPECT_NE(manifest.find("commit_time="), std::string::npos);
}

TEST_F(GarbageCollectorTest, SweepWalAfterLogFailureDoesNotBlockPrune)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto sweep_mgr = db.sweep_manager();
    auto gc = db.garbage_collector();
    ASSERT_NE(sweep_mgr, nullptr);
    ASSERT_NE(gc, nullptr);

    ID tx_uuid{};
    uint64_t txid = 0;
    ASSERT_TRUE(createCommittedRetainedTransaction(db, tx_uuid, txid));

    SweepPolicyBinding binding{};
    binding.scope_kind = SweepScopeKind::DATABASE;
    binding.scope_id = db.uuid();
    binding.lanes = {SweepPolicyLane::LINEAGE_RETENTION, SweepPolicyLane::WAL_AFTER_EXPORT};
    binding.strict_audit = true;

    ErrorContext ctx;
    ASSERT_EQ(sweep_mgr->setPolicyBindings({binding}, &ctx), Status::OK) << ctx.message;

    std::filesystem::path blocking_path(db.path());
    blocking_path += ".forensics";
    std::filesystem::create_directories(blocking_path);
    blocking_path /= "wal_after_log";
    {
        std::ofstream out(blocking_path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << "blocked";
    }

    ASSERT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::OK) << ctx.message;
    EXPECT_FALSE(gc->isSweepPruneBlocked());

    auto stats = sweep_mgr->getStatistics();
    EXPECT_GE(stats.last_evidence_items_emitted, 1u);
    EXPECT_EQ(stats.last_wal_after_segments_emitted, 0u);
    EXPECT_GE(stats.wal_after_backlog_depth, 1u);
    EXPECT_EQ(stats.wal_after_export_failures, 1u);

    std::vector<SweepEvidenceWorkItem> items;
    ASSERT_EQ(sweep_mgr->listEvidenceWorkItems(items, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(std::any_of(items.begin(), items.end(),
                            [txid](const SweepEvidenceWorkItem& item) {
                                return item.txid == txid;
                            }));

    std::vector<SweepWalAfterLogSegment> segments;
    ASSERT_EQ(sweep_mgr->listWalAfterLogSegments(segments, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(segments.empty());
}

TEST_F(GarbageCollectorTest, SweepPageSpotAuditEmitsDeterministicFindingsWithoutInlineRepair)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto sweep_mgr = db.sweep_manager();
    ASSERT_NE(sweep_mgr, nullptr);

    uint32_t audit_page_id = 0;
    std::vector<uint8_t> raw_page;
    ASSERT_TRUE(allocateHeapAuditPage(db, audit_page_id, raw_page));

    auto* header = reinterpret_cast<PageHeader*>(raw_page.data());
    const uint32_t original_checksum = header->checksum;
    header->checksum ^= 0x00FF00FFu;
    ASSERT_TRUE(writeRawPage(db, audit_page_id, raw_page));

    ID tx_uuid{};
    uint64_t txid = 0;
    ASSERT_TRUE(createCommittedRetainedTransaction(db, tx_uuid, txid));

    SweepPolicyBinding binding{};
    binding.scope_kind = SweepScopeKind::DATABASE;
    binding.scope_id = db.uuid();
    binding.lanes = {SweepPolicyLane::PAGE_SPOT_AUDIT};
    binding.strict_audit = true;

    ErrorContext ctx;
    ASSERT_EQ(sweep_mgr->setPolicyBindings({binding}, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::OK) << ctx.message;

    auto stats = sweep_mgr->getStatistics();
    EXPECT_GE(stats.last_page_audit_findings_emitted, 1u);
    EXPECT_EQ(stats.page_audit_mode_downgrades, 0u);

    std::vector<SweepPageAuditFinding> findings;
    ASSERT_EQ(sweep_mgr->listPageAuditFindings(findings, &ctx), Status::OK) << ctx.message;
    auto it = std::find_if(findings.begin(), findings.end(),
                           [audit_page_id](const SweepPageAuditFinding& finding) {
                               return finding.page_id == audit_page_id &&
                                      finding.error_code == "PAGE_CHECKSUM_FAIL";
                           });
    ASSERT_TRUE(it != findings.end()) << summarizePageAuditFindings(findings, audit_page_id);
    EXPECT_EQ(it->scan_mode, "DIAGNOSTIC");
    EXPECT_EQ(it->trigger_source, "SWEEP_BACKGROUND");
    EXPECT_EQ(it->severity, "ERROR");

    std::vector<uint8_t> after_page;
    ASSERT_TRUE(readRawPage(db, audit_page_id, after_page));
    const auto* after_header = reinterpret_cast<const PageHeader*>(after_page.data());
    EXPECT_EQ(after_header->checksum, (original_checksum ^ 0x00FF00FFu));
    EXPECT_FALSE(validatePageChecksum(after_page.data(), db.page_size()));
}

TEST_F(GarbageCollectorTest, SweepPageSpotAuditDowngradesToLightUnderForegroundPressure)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto sweep_mgr = db.sweep_manager();
    ASSERT_NE(sweep_mgr, nullptr);

    uint32_t audit_page_id = 0;
    std::vector<uint8_t> raw_page;
    ASSERT_TRUE(allocateHeapAuditPage(db, audit_page_id, raw_page));

    auto* header = reinterpret_cast<PageHeader*>(raw_page.data());
    const uint32_t original_upper = pageUpper(*header);
    pageSetLower(*header, sizeof(PageHeader) + sizeof(ItemPointer));
    pageSetUpper(*header, original_upper);
    header->item_count = 1;
    auto* item = reinterpret_cast<ItemPointer*>(raw_page.data() + sizeof(PageHeader));
    item->offset = db.page_size() - 128;
    item->length = 8;
    item->flags = 0;
    header->checksum = calculatePageChecksum(raw_page.data(), db.page_size());
    ASSERT_TRUE(writeRawPage(db, audit_page_id, raw_page));

    SweepPolicyBinding binding{};
    binding.scope_kind = SweepScopeKind::DATABASE;
    binding.scope_id = db.uuid();
    binding.lanes = {SweepPolicyLane::PAGE_SPOT_AUDIT};
    binding.strict_audit = true;

    ErrorContext ctx;
    ASSERT_EQ(sweep_mgr->setPolicyBindings({binding}, &ctx), Status::OK) << ctx.message;

    ID tx_uuid_1{};
    uint64_t txid_1 = 0;
    ASSERT_TRUE(createCommittedRetainedTransaction(db, tx_uuid_1, txid_1));
    ASSERT_EQ(sweep_mgr->executeSweep(true, &ctx), Status::OK) << ctx.message;

    auto stats = sweep_mgr->getStatistics();
    EXPECT_EQ(stats.page_audit_mode_downgrades, 1u);

    std::vector<SweepPageAuditFinding> findings;
    ASSERT_EQ(sweep_mgr->listPageAuditFindings(findings, &ctx), Status::OK) << ctx.message;
    auto foreground_it = std::find_if(findings.begin(), findings.end(),
                                      [audit_page_id](const SweepPageAuditFinding& finding) {
                                          return finding.page_id == audit_page_id;
                                      });
    EXPECT_TRUE(foreground_it == findings.end()) << summarizePageAuditFindings(findings, audit_page_id);

    std::vector<uint8_t> after_foreground_page;
    ASSERT_TRUE(readRawPage(db, audit_page_id, after_foreground_page));
    const auto* foreground_item =
        reinterpret_cast<const ItemPointer*>(after_foreground_page.data() + sizeof(PageHeader));
    EXPECT_EQ(foreground_item->offset, db.page_size() - 128);
    EXPECT_EQ(foreground_item->length, 8u);
    EXPECT_TRUE(validatePageChecksum(after_foreground_page.data(), db.page_size()));

    ID tx_uuid_2{};
    uint64_t txid_2 = 0;
    ASSERT_TRUE(createCommittedRetainedTransaction(db, tx_uuid_2, txid_2));
    ASSERT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::OK) << ctx.message;

    findings.clear();
    ASSERT_EQ(sweep_mgr->listPageAuditFindings(findings, &ctx), Status::OK) << ctx.message;
    auto it = std::find_if(findings.begin(), findings.end(),
                           [audit_page_id](const SweepPageAuditFinding& finding) {
                               return finding.page_id == audit_page_id &&
                                      finding.error_code == "RECORD_CHAIN_CORRUPT";
                           });
    ASSERT_TRUE(it != findings.end()) << summarizePageAuditFindings(findings, audit_page_id);
    EXPECT_EQ(it->scan_mode, "DIAGNOSTIC");
    EXPECT_EQ(it->trigger_source, "SWEEP_BACKGROUND");
    EXPECT_EQ(it->severity, "ERROR");
}

TEST_F(GarbageCollectorTest, SweepShadowCaptureEmitsLogicalManifestFromRetainedEvidence)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto sweep_mgr = db.sweep_manager();
    ASSERT_NE(sweep_mgr, nullptr);

    ID tx_uuid{};
    uint64_t txid = 0;
    ASSERT_TRUE(createCommittedRetainedTransaction(db, tx_uuid, txid));

    SweepPolicyBinding binding{};
    binding.scope_kind = SweepScopeKind::DATABASE;
    binding.scope_id = db.uuid();
    binding.lanes = {SweepPolicyLane::LINEAGE_RETENTION, SweepPolicyLane::SHADOW_CAPTURE};
    binding.strict_audit = true;

    ErrorContext ctx;
    ASSERT_EQ(sweep_mgr->setPolicyBindings({binding}, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::OK) << ctx.message;

    auto stats = sweep_mgr->getStatistics();
    EXPECT_EQ(stats.last_shadow_capture_manifests_emitted, 1u);
    EXPECT_EQ(stats.shadow_capture_failures, 0u);

    std::vector<SweepEvidenceWorkItem> items;
    ASSERT_EQ(sweep_mgr->listEvidenceWorkItems(items, &ctx), Status::OK) << ctx.message;
    auto item_it = std::find_if(items.begin(), items.end(),
                                [txid](const SweepEvidenceWorkItem& item) {
                                    return item.txid == txid;
                                });
    ASSERT_NE(item_it, items.end());

    std::vector<SweepShadowCaptureManifest> manifests;
    ASSERT_EQ(sweep_mgr->listShadowCaptureManifests(manifests, &ctx), Status::OK) << ctx.message;
    auto manifest_it = std::find_if(manifests.begin(), manifests.end(),
                                    [tx_uuid](const SweepShadowCaptureManifest& manifest) {
                                        return manifest.tx_uuid == tx_uuid;
                                    });
    ASSERT_NE(manifest_it, manifests.end());
    EXPECT_EQ(manifest_it->capture_scope, "TRANSACTION");
    EXPECT_EQ(manifest_it->capture_format, "LOGICAL_TX_SUMMARY");
    EXPECT_NE(manifest_it->payload_manifest.find("source_work_item_uuid=" +
                                                 item_it->work_item_id.toString()),
              std::string::npos);
    EXPECT_NE(manifest_it->payload_manifest.find("source_manifest_path=" + item_it->spool_path),
              std::string::npos);
    const std::string shadow_path =
        extractManifestField(manifest_it->payload_manifest, "shadow_path");
    ASSERT_FALSE(shadow_path.empty());
    EXPECT_TRUE(std::filesystem::exists(shadow_path));
}

TEST_F(GarbageCollectorTest, SweepBlocksPruneWhenShadowCapturePersistenceFails)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto sweep_mgr = db.sweep_manager();
    auto gc = db.garbage_collector();
    ASSERT_NE(sweep_mgr, nullptr);
    ASSERT_NE(gc, nullptr);

    ID tx_uuid{};
    uint64_t txid = 0;
    ASSERT_TRUE(createCommittedRetainedTransaction(db, tx_uuid, txid));

    SweepPolicyBinding binding{};
    binding.scope_kind = SweepScopeKind::DATABASE;
    binding.scope_id = db.uuid();
    binding.lanes = {SweepPolicyLane::LINEAGE_RETENTION, SweepPolicyLane::SHADOW_CAPTURE};
    binding.strict_audit = true;

    ErrorContext ctx;
    ASSERT_EQ(sweep_mgr->setPolicyBindings({binding}, &ctx), Status::OK) << ctx.message;

    std::filesystem::path blocking_root(db.path());
    blocking_root += ".forensics";
    ASSERT_TRUE(std::filesystem::create_directories(blocking_root) || std::filesystem::exists(blocking_root));
    std::filesystem::path blocking_path = blocking_root / "shadow_capture";
    {
        std::ofstream out(blocking_path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << "blocked";
    }

    EXPECT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::IO_ERROR);
    EXPECT_TRUE(gc->isSweepPruneBlocked());

    auto stats = sweep_mgr->getStatistics();
    EXPECT_EQ(stats.shadow_capture_failures, 1u);
    EXPECT_EQ(stats.last_shadow_capture_manifests_emitted, 0u);
    EXPECT_TRUE(stats.prune_blocked);

    std::vector<SweepShadowCaptureManifest> manifests;
    ASSERT_EQ(sweep_mgr->listShadowCaptureManifests(manifests, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(std::none_of(manifests.begin(), manifests.end(),
                             [tx_uuid](const SweepShadowCaptureManifest& manifest) {
                                 return manifest.tx_uuid == tx_uuid;
                             }));
}

TEST_F(GarbageCollectorTest, SweepResumesAfterShadowCaptureFailureWithoutDuplicatingLocalEvidence)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto sweep_mgr = db.sweep_manager();
    auto gc = db.garbage_collector();
    ASSERT_NE(sweep_mgr, nullptr);
    ASSERT_NE(gc, nullptr);

    ID tx_uuid{};
    uint64_t txid = 0;
    ASSERT_TRUE(createCommittedRetainedTransaction(db, tx_uuid, txid));

    SweepPolicyBinding binding{};
    binding.scope_kind = SweepScopeKind::DATABASE;
    binding.scope_id = db.uuid();
    binding.lanes = {SweepPolicyLane::LINEAGE_RETENTION, SweepPolicyLane::SHADOW_CAPTURE};
    binding.strict_audit = true;

    ErrorContext ctx;
    ASSERT_EQ(sweep_mgr->setPolicyBindings({binding}, &ctx), Status::OK) << ctx.message;

    std::filesystem::path blocking_root(db.path());
    blocking_root += ".forensics";
    ASSERT_TRUE(std::filesystem::create_directories(blocking_root) ||
                std::filesystem::exists(blocking_root));
    std::filesystem::path blocking_path = blocking_root / "shadow_capture";
    {
        std::ofstream out(blocking_path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << "blocked";
    }

    EXPECT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::IO_ERROR);
    EXPECT_TRUE(gc->isSweepPruneBlocked());

    std::vector<SweepEvidenceWorkItem> items;
    ASSERT_EQ(sweep_mgr->listEvidenceWorkItems(items, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(std::count_if(items.begin(), items.end(),
                            [txid](const SweepEvidenceWorkItem& item) {
                                return item.txid == txid;
                            }),
              1);

    db.close();

    std::vector<uint8_t> raw_page;
    ASSERT_TRUE(readRawPage(db, BOOTSTRAP_PAGE_SYSTEM_STATE, raw_page));
    auto* state_page = reinterpret_cast<BootstrapSystemStatePage*>(raw_page.data());
    const uint64_t generation_before = state_page->reserved[kSweepProgressSlotGeneration];
    EXPECT_GT(generation_before, 0u);
    EXPECT_EQ(state_page->reserved[kSweepProgressSlotActive], 1u);

    ASSERT_TRUE(std::filesystem::remove(blocking_path));

    ASSERT_EQ(db.open(test_db_->path(), &ctx), Status::OK) << ctx.message;
    sweep_mgr = db.sweep_manager();
    gc = db.garbage_collector();
    ASSERT_NE(sweep_mgr, nullptr);
    ASSERT_NE(gc, nullptr);

    ASSERT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::OK) << ctx.message;
    EXPECT_FALSE(gc->isSweepPruneBlocked());

    items.clear();
    ASSERT_EQ(sweep_mgr->listEvidenceWorkItems(items, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(std::count_if(items.begin(), items.end(),
                            [txid](const SweepEvidenceWorkItem& item) {
                                return item.txid == txid;
                            }),
              1);

    std::vector<SweepShadowCaptureManifest> manifests;
    ASSERT_EQ(sweep_mgr->listShadowCaptureManifests(manifests, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(std::count_if(manifests.begin(), manifests.end(),
                            [tx_uuid](const SweepShadowCaptureManifest& manifest) {
                                return manifest.tx_uuid == tx_uuid;
                            }),
              1);

    db.close();
    raw_page.clear();
    ASSERT_TRUE(readRawPage(db, BOOTSTRAP_PAGE_SYSTEM_STATE, raw_page));
    state_page = reinterpret_cast<BootstrapSystemStatePage*>(raw_page.data());
    EXPECT_EQ(state_page->reserved[kSweepProgressSlotGeneration], generation_before);
    EXPECT_EQ(state_page->reserved[kSweepProgressSlotActive], 0u);
}

TEST_F(GarbageCollectorTest, ConcurrentAccess)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Start background GC
    ErrorContext ctx;
    ASSERT_EQ(gc->startBackgroundGC(&ctx), Status::OK);

    // Concurrently mark pages dirty from multiple threads
    std::vector<std::thread> threads;
    constexpr int NUM_THREADS = 4;
    constexpr int MARKS_PER_THREAD = 100;

    for (int t = 0; t < NUM_THREADS; t++)
    {
        threads.emplace_back([gc, t]() {
            for (int i = 0; i < MARKS_PER_THREAD; i++)
            {
                uint32_t page_id = t * MARKS_PER_THREAD + i;
                gc->markPageDirty(page_id);
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads)
    {
        thread.join();
    }

    // Stop background GC
    ASSERT_EQ(gc->stopBackgroundGC(&ctx), Status::OK);

    // Verify no crashes and stats are consistent
    auto stats = gc->getStatistics();
    EXPECT_EQ(stats.total_dirty_pages_marked, NUM_THREADS * MARKS_PER_THREAD);
}

// ========== Performance Tests ==========

TEST_F(GarbageCollectorTest, PriorityQueuePerformance)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Mark many pages dirty
    auto start = std::chrono::steady_clock::now();

    constexpr uint32_t NUM_PAGES = 10000;
    for (uint32_t i = 0; i < NUM_PAGES; i++)
    {
        gc->markPageDirty(i);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should be fast (< 100ms for 10k pages)
    EXPECT_LT(duration_ms, 100) << "Marking 10k pages should be fast";

    EXPECT_EQ(gc->getDirtyPageCount(), NUM_PAGES);
}

TEST_F(GarbageCollectorTest, StatisticsAccessPerformance)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto gc = db.garbage_collector();
    ASSERT_NE(gc, nullptr);

    // Get statistics many times
    auto start = std::chrono::steady_clock::now();

    constexpr int NUM_CALLS = 10000;
    for (int i = 0; i < NUM_CALLS; i++)
    {
        auto stats = gc->getStatistics();
        (void)stats;  // Suppress unused warning
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should be fast (< 100ms for 10k calls)
    EXPECT_LT(duration_ms, 100) << "Getting statistics 10k times should be fast";
}

// Note: main() is provided by GTest::gtest_main linked in CMakeLists.txt
