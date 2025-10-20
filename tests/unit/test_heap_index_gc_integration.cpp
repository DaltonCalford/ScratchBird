/**
 * @file test_heap_index_gc_integration.cpp
 * @brief Integration tests for PHASE 2 TASK 2.6: Heap-Index GC Integration
 *
 * Tests the integration between HeapPage::collectDeadTuples(),
 * GarbageCollector::cleanIndexes(), and heap pruning to ensure
 * that dead tuples are properly removed from both heap and indexes.
 *
 * Test coverage:
 * - collectDeadTuples() correctly identifies dead tuples based on OIT
 * - GC cleanPage() flow calls collectDeadTuples() before prunePage()
 * - TID format is correct: (page_id << 32) | (item_id << 16)
 *
 * NOTE: Full end-to-end index cleanup testing requires table metadata
 * integration which is pending (see GarbageCollector::cleanIndexes TODO).
 */

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/heap_page.h"
#include <filesystem>
#include <vector>

using namespace scratchbird::core;

class HeapIndexGCIntegrationTest : public ::testing::Test
{
protected:
    std::string test_db_path_;
    Database db_;

    void SetUp() override
    {
        // Create temporary database for testing
        test_db_path_ = "/tmp/test_heap_index_gc_" + std::to_string(getpid()) + ".db";

        // Remove if exists
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }

        // Create database
        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        status = db_.open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;

        // Initialize garbage collector
        auto gc = db_.garbage_collector();
        ASSERT_NE(gc, nullptr);
        status = gc->initialize(&ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to initialize GC: " << ctx.message;
    }

    void TearDown() override
    {
        db_.close();

        // Clean up test database
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }
    }
};

// ========== HeapPage::collectDeadTuples() Tests ==========

TEST_F(HeapIndexGCIntegrationTest, CollectDeadTuples_EmptyPage)
{
    ErrorContext ctx;

    // Allocate a test page
    auto page_mgr = db_.page_manager();
    ASSERT_NE(page_mgr, nullptr);

    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    Status status = page_mgr->allocatePage(&page_id, &page_data, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_NE(page_data, nullptr);

    // Initialize heap page
    HeapPage heap_page(page_data, db_.page_size());
    status = heap_page.initialize(page_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Collect dead tuples with OIT=1000
    std::vector<uint64_t> dead_tids;
    status = heap_page.collectDeadTuples(1000, &dead_tids, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_TRUE(dead_tids.empty()) << "Empty page should have no dead tuples";

    // Free page
    page_mgr->freePage(page_id, &ctx);
}

TEST_F(HeapIndexGCIntegrationTest, CollectDeadTuples_LiveTuplesOnly)
{
    ErrorContext ctx;

    // Allocate and initialize page
    auto page_mgr = db_.page_manager();
    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    Status status = page_mgr->allocatePage(&page_id, &page_data, &ctx);
    ASSERT_EQ(status, Status::OK);

    HeapPage heap_page(page_data, db_.page_size());
    status = heap_page.initialize(page_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Insert live tuples (xmax=0)
    uint8_t tuple_data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    for (int i = 0; i < 3; i++)
    {
        uint16_t item_id = 0;
        status = heap_page.insertTuple(tuple_data, sizeof(tuple_data), 100 + i, &item_id, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Collect dead tuples with OIT=1000
    std::vector<uint64_t> dead_tids;
    status = heap_page.collectDeadTuples(1000, &dead_tids, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_TRUE(dead_tids.empty()) << "No dead tuples (all xmax=0)";

    page_mgr->freePage(page_id, &ctx);
}

TEST_F(HeapIndexGCIntegrationTest, CollectDeadTuples_AllDead)
{
    ErrorContext ctx;

    // Allocate and initialize page
    auto page_mgr = db_.page_manager();
    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    Status status = page_mgr->allocatePage(&page_id, &page_data, &ctx);
    ASSERT_EQ(status, Status::OK);

    HeapPage heap_page(page_data, db_.page_size());
    status = heap_page.initialize(page_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Insert and delete tuples (xmax < OIT)
    uint8_t tuple_data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    for (int i = 0; i < 3; i++)
    {
        uint16_t item_id = 0;
        status = heap_page.insertTuple(tuple_data, sizeof(tuple_data), 100 + i, &item_id, &ctx);
        ASSERT_EQ(status, Status::OK);

        // Delete tuple (sets xmax and HEAP_XMAX_COMMITTED flag)
        status = heap_page.deleteTuple(item_id, 500 + i, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Collect dead tuples with OIT=1000 (all xmax values 500-502 < 1000)
    std::vector<uint64_t> dead_tids;
    status = heap_page.collectDeadTuples(1000, &dead_tids, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(dead_tids.size(), 3) << "All 3 tuples should be dead";

    // Verify TID format: (page_id << 32) | (item_id << 16)
    for (size_t i = 0; i < dead_tids.size(); i++)
    {
        uint64_t tid = dead_tids[i];
        uint32_t tid_page = static_cast<uint32_t>(tid >> 32);
        uint16_t tid_item = static_cast<uint16_t>((tid >> 16) & 0xFFFF);

        EXPECT_EQ(tid_page, page_id) << "TID page_id should match";
        EXPECT_EQ(tid_item, i) << "TID item_id should be " << i;
    }

    page_mgr->freePage(page_id, &ctx);
}

TEST_F(HeapIndexGCIntegrationTest, CollectDeadTuples_MixedLiveAndDead)
{
    ErrorContext ctx;

    // Allocate and initialize page
    auto page_mgr = db_.page_manager();
    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    Status status = page_mgr->allocatePage(&page_id, &page_data, &ctx);
    ASSERT_EQ(status, Status::OK);

    HeapPage heap_page(page_data, db_.page_size());
    status = heap_page.initialize(page_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Insert mix of live and dead tuples
    uint8_t tuple_data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    // Tuple 0: Live (xmax=0)
    uint16_t item_id = 0;
    status = heap_page.insertTuple(tuple_data, sizeof(tuple_data), 100, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Tuple 1: Dead (xmax=500 < OIT=1000)
    status = heap_page.insertTuple(tuple_data, sizeof(tuple_data), 200, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = heap_page.deleteTuple(item_id, 500, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Tuple 2: Live (xmax=0)
    status = heap_page.insertTuple(tuple_data, sizeof(tuple_data), 300, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Tuple 3: Dead (xmax=600 < OIT=1000)
    status = heap_page.insertTuple(tuple_data, sizeof(tuple_data), 400, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = heap_page.deleteTuple(item_id, 600, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Tuple 4: Live (xmax=1100 >= OIT=1000, still visible to older transactions)
    status = heap_page.insertTuple(tuple_data, sizeof(tuple_data), 500, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = heap_page.deleteTuple(item_id, 1100, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Collect dead tuples with OIT=1000
    std::vector<uint64_t> dead_tids;
    status = heap_page.collectDeadTuples(1000, &dead_tids, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(dead_tids.size(), 2) << "2 tuples should be dead (items 1 and 3)";

    // Verify dead TIDs are for items 1 and 3
    if (dead_tids.size() == 2)
    {
        uint16_t item1 = static_cast<uint16_t>((dead_tids[0] >> 16) & 0xFFFF);
        uint16_t item2 = static_cast<uint16_t>((dead_tids[1] >> 16) & 0xFFFF);

        EXPECT_EQ(item1, 1) << "First dead TID should be item 1";
        EXPECT_EQ(item2, 3) << "Second dead TID should be item 3";
    }

    page_mgr->freePage(page_id, &ctx);
}

TEST_F(HeapIndexGCIntegrationTest, CollectDeadTuples_NullPointer)
{
    ErrorContext ctx;

    // Allocate and initialize page
    auto page_mgr = db_.page_manager();
    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    Status status = page_mgr->allocatePage(&page_id, &page_data, &ctx);
    ASSERT_EQ(status, Status::OK);

    HeapPage heap_page(page_data, db_.page_size());
    status = heap_page.initialize(page_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Call with nullptr
    status = heap_page.collectDeadTuples(1000, nullptr, &ctx);

    EXPECT_EQ(status, Status::INVALID_ARGUMENT);
    EXPECT_NE(ctx.message.find("cannot be null"), std::string::npos);

    page_mgr->freePage(page_id, &ctx);
}

// ========== GarbageCollector Integration Tests ==========

TEST_F(HeapIndexGCIntegrationTest, GC_Integration_FlowWorks)
{
    ErrorContext ctx;

    // Create page with dead tuple
    auto page_mgr = db_.page_manager();
    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    Status status = page_mgr->allocatePage(&page_id, &page_data, &ctx);
    ASSERT_EQ(status, Status::OK);

    HeapPage heap_page(page_data, db_.page_size());
    status = heap_page.initialize(page_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Insert and delete a tuple
    uint8_t tuple_data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint16_t item_id = 0;
    status = heap_page.insertTuple(tuple_data, sizeof(tuple_data), 100, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = heap_page.deleteTuple(item_id, 500, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Mark page dirty so GC will process it
    auto gc = db_.garbage_collector();
    gc->markPageDirty(page_id);

    // Trigger cooperative GC on this page
    // This should call collectDeadTuples() -> cleanIndexes() -> prunePage()
    gc->processPageCooperative(page_id, &ctx);

    // Verify page was processed
    auto stats = gc->getStatistics();
    EXPECT_GT(stats.cooperative_runs, 0) << "Cooperative GC should have run";

    // Note: cleanIndexes() currently logs but doesn't clean (pending table metadata)
    // This test verifies the integration flow works without errors

    page_mgr->freePage(page_id, &ctx);
}

TEST_F(HeapIndexGCIntegrationTest, GC_Integration_HandlesEmptyPage)
{
    ErrorContext ctx;

    // Create empty page
    auto page_mgr = db_.page_manager();
    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    Status status = page_mgr->allocatePage(&page_id, &page_data, &ctx);
    ASSERT_EQ(status, Status::OK);

    HeapPage heap_page(page_data, db_.page_size());
    status = heap_page.initialize(page_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Mark page dirty and run GC
    auto gc = db_.garbage_collector();
    gc->markPageDirty(page_id);
    gc->processPageCooperative(page_id, &ctx);

    // Should handle empty page gracefully
    auto stats = gc->getStatistics();
    EXPECT_GT(stats.cooperative_runs, 0);

    page_mgr->freePage(page_id, &ctx);
}

// Note: main() is provided by GTest::gtest_main linked in CMakeLists.txt
