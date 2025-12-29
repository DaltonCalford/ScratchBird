/**
 * Plan 01 Task D.4: R-Tree GC Tests
 *
 * Tests for removeDeadEntries() per Plan 01 index GC specification
 *
 * Implementation: Leaf page traversal using rtree_right_sibling chain.
 * Physically removes entries where:
 * - entry_row_id is in dead_tids (heap confirmed dead)
 * - entry_xmax < OIT (no transaction can see them)
 *
 * Test coverage:
 * 1. Unit test: Basic GC removes dead entries
 * 2. Restart test: GC effects persist across restart
 * 3. Negative test: Empty dead_tids is no-op
 * 4. Unaffected entries test: Entries not in dead_tids remain
 */

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/rtree_index.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/types.h"
#include <filesystem>
#include <memory>

using namespace scratchbird::core;

namespace {

class RTreeGCTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create temporary database
        test_db_path_ = "/tmp/test_rtree_gc.db";
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }

        // Create and open database
        db_ = std::make_unique<Database>();
        Status status = db_->create(test_db_path_, 16384, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to create database";

        status = db_->open(test_db_path_, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to open database";
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
            db_.reset();
        }

        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }
    }

    // Helper to create a bounding box
    std::vector<uint8_t> makeBBox(double min_x, double min_y, double max_x, double max_y)
    {
        std::vector<uint8_t> bbox(32); // 4 doubles
        double* coords = reinterpret_cast<double*>(bbox.data());
        coords[0] = min_x;
        coords[1] = min_y;
        coords[2] = max_x;
        coords[3] = max_y;
        return bbox;
    }

    std::unique_ptr<Database> db_;
    std::string test_db_path_;
};

/**
 * Test 1: Unit test - Basic GC removes dead entries
 *
 * This test verifies that R-Tree GC physically removes entries
 * where TID is in dead_tids AND xmax < OIT.
 */
TEST_F(RTreeGCTest, BasicGCRemovesDeadEntries)
{
    // Generate unique UUIDs
    UuidV7Bytes index_uuid = generateUuidV7();

    // Create R-Tree index
    uint32_t root_page = 0;
    Status status = RTreeIndex::create(db_.get(), index_uuid, &root_page, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to create R-Tree index";
    ASSERT_GT(root_page, 0);

    // Open index
    auto rtree = RTreeIndex::open(db_.get(), index_uuid, root_page, nullptr);
    ASSERT_NE(rtree, nullptr) << "Failed to open R-Tree index";

    // Get current XID for inserts
    uint64_t xmin = db_->transaction_manager()->getCurrentXid();

    // Insert 3 spatial entries
    TID tid1(0, 100, 0);
    TID tid2(0, 100, 1);
    TID tid3(0, 100, 2);

    std::vector<uint8_t> bbox1 = makeBBox(0.0, 0.0, 10.0, 10.0);
    std::vector<uint8_t> bbox2 = makeBBox(20.0, 20.0, 30.0, 30.0);
    std::vector<uint8_t> bbox3 = makeBBox(40.0, 40.0, 50.0, 50.0);

    status = rtree->insert(bbox1, tid1, xmin, nullptr);
    ASSERT_EQ(status, Status::OK);

    status = rtree->insert(bbox2, tid2, xmin, nullptr);
    ASSERT_EQ(status, Status::OK);

    status = rtree->insert(bbox3, tid3, xmin, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Flush to ensure persistence
    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK);

    // Logically delete tid1 and tid2 (set xmax)
    uint64_t xmax = db_->transaction_manager()->getCurrentXid();

    status = rtree->remove(bbox1, tid1, xmax, nullptr);
    ASSERT_EQ(status, Status::OK);

    status = rtree->remove(bbox2, tid2, xmax, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Flush deletions
    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK);

    // Run GC with tid1 and tid2 as dead
    std::vector<TID> dead_tids = {tid1, tid2};
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = rtree->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, nullptr);
    ASSERT_EQ(status, Status::OK) << "GC failed";

    // Verify that 2 entries were removed
    EXPECT_EQ(entries_removed, 2) << "Should have removed 2 entries";
    EXPECT_GT(pages_modified, 0) << "Should have modified at least one page";

    // Flush to ensure persistence
    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK);
}

/**
 * Test 2: Restart test - GC effects persist across restart
 *
 * This test verifies that physical removals persist after database restart.
 */
TEST_F(RTreeGCTest, GCEffectsPersistAcrossRestart)
{
    // Generate unique UUIDs
    UuidV7Bytes index_uuid = generateUuidV7();

    uint32_t root_page = 0;

    // Phase 1: Create index, insert, and run GC
    {
        Status status = RTreeIndex::create(db_.get(), index_uuid, &root_page, nullptr);
        ASSERT_EQ(status, Status::OK);

        auto rtree = RTreeIndex::open(db_.get(), index_uuid, root_page, nullptr);
        ASSERT_NE(rtree, nullptr);

        // Get current XID
        uint64_t xmin = db_->transaction_manager()->getCurrentXid();

        // Insert 2 entries
        TID tid1(0, 200, 0);
        TID tid2(0, 200, 1);

        std::vector<uint8_t> bbox1 = makeBBox(5.0, 5.0, 15.0, 15.0);
        std::vector<uint8_t> bbox2 = makeBBox(25.0, 25.0, 35.0, 35.0);

        status = rtree->insert(bbox1, tid1, xmin, nullptr);
        ASSERT_EQ(status, Status::OK);

        status = rtree->insert(bbox2, tid2, xmin, nullptr);
        ASSERT_EQ(status, Status::OK);

        // Logically delete tid1
        uint64_t xmax = db_->transaction_manager()->getCurrentXid();
        status = rtree->remove(bbox1, tid1, xmax, nullptr);
        ASSERT_EQ(status, Status::OK);

        // Run GC
        std::vector<TID> dead_tids = {tid1};
        uint64_t entries_removed = 0;

        status = rtree->removeDeadEntries(dead_tids, &entries_removed, nullptr, nullptr);
        ASSERT_EQ(status, Status::OK);
        EXPECT_EQ(entries_removed, 1) << "Should have removed 1 entry";

        // Flush to disk
        status = db_->buffer_pool()->flushAll(nullptr);
        ASSERT_EQ(status, Status::OK);
    }

    // Phase 2: Close and reopen database
    db_->close();
    Status status = db_->open(test_db_path_, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to reopen database";

    // Phase 3: Reopen index and verify GC persisted
    {
        auto rtree = RTreeIndex::open(db_.get(), index_uuid, root_page, nullptr);
        ASSERT_NE(rtree, nullptr) << "Failed to reopen R-Tree index after restart";

        // Running GC again with same dead TID should find nothing to remove
        std::vector<TID> dead_tids = {TID(0, 200, 0)};
        uint64_t entries_removed = 0;

        status = rtree->removeDeadEntries(dead_tids, &entries_removed, nullptr, nullptr);
        ASSERT_EQ(status, Status::OK);

        // Should be 0 because entry was already physically removed in Phase 1
        EXPECT_EQ(entries_removed, 0) << "Entry should already be removed from Phase 1";
    }
}

/**
 * Test 3: Negative test - Empty dead_tids is no-op
 *
 * This test verifies that passing empty dead_tids returns OK
 * without modifying anything.
 */
TEST_F(RTreeGCTest, EmptyDeadTidsIsNoOp)
{
    // Generate unique UUIDs
    UuidV7Bytes index_uuid = generateUuidV7();

    // Create R-Tree index
    uint32_t root_page = 0;
    Status status = RTreeIndex::create(db_.get(), index_uuid, &root_page, nullptr);
    ASSERT_EQ(status, Status::OK);

    auto rtree = RTreeIndex::open(db_.get(), index_uuid, root_page, nullptr);
    ASSERT_NE(rtree, nullptr);

    // Insert one entry
    uint64_t xmin = db_->transaction_manager()->getCurrentXid();
    TID tid(0, 300, 0);
    std::vector<uint8_t> bbox = makeBBox(10.0, 10.0, 20.0, 20.0);

    status = rtree->insert(bbox, tid, xmin, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Run GC with empty dead_tids
    std::vector<TID> empty_dead_tids;
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = rtree->removeDeadEntries(empty_dead_tids, &entries_removed, &pages_modified, nullptr);
    ASSERT_EQ(status, Status::OK) << "Empty dead_tids should return OK";

    // Verify counters are zero
    EXPECT_EQ(entries_removed, 0) << "No entries should be removed";
    EXPECT_EQ(pages_modified, 0) << "No pages should be modified";
}

/**
 * Test 4: Unaffected entries test - Entries not in dead_tids remain
 *
 * This test verifies that only entries with TIDs in dead_tids are removed,
 * and other entries remain untouched.
 */
TEST_F(RTreeGCTest, UnaffectedEntriesNotRemoved)
{
    // Generate unique UUIDs
    UuidV7Bytes index_uuid = generateUuidV7();

    // Create R-Tree index
    uint32_t root_page = 0;
    Status status = RTreeIndex::create(db_.get(), index_uuid, &root_page, nullptr);
    ASSERT_EQ(status, Status::OK);

    auto rtree = RTreeIndex::open(db_.get(), index_uuid, root_page, nullptr);
    ASSERT_NE(rtree, nullptr);

    // Get current XID
    uint64_t xmin = db_->transaction_manager()->getCurrentXid();

    // Insert 3 entries
    TID tid1(0, 400, 0);
    TID tid2(0, 400, 1);
    TID tid3(0, 400, 2);

    std::vector<uint8_t> bbox1 = makeBBox(0.0, 0.0, 5.0, 5.0);
    std::vector<uint8_t> bbox2 = makeBBox(10.0, 10.0, 15.0, 15.0);
    std::vector<uint8_t> bbox3 = makeBBox(20.0, 20.0, 25.0, 25.0);

    status = rtree->insert(bbox1, tid1, xmin, nullptr);
    ASSERT_EQ(status, Status::OK);

    status = rtree->insert(bbox2, tid2, xmin, nullptr);
    ASSERT_EQ(status, Status::OK);

    status = rtree->insert(bbox3, tid3, xmin, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Logically delete only tid2
    uint64_t xmax = db_->transaction_manager()->getCurrentXid();
    status = rtree->remove(bbox2, tid2, xmax, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Flush
    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK);

    // Run GC with only tid2 as dead
    std::vector<TID> dead_tids = {tid2};
    uint64_t entries_removed = 0;

    status = rtree->removeDeadEntries(dead_tids, &entries_removed, nullptr, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Only 1 entry should be removed
    EXPECT_EQ(entries_removed, 1) << "Only 1 entry should be removed";

    // Now GC with tid1 and tid3 should find nothing (they're not deleted)
    std::vector<TID> more_dead_tids = {tid1, tid3};
    entries_removed = 0;

    status = rtree->removeDeadEntries(more_dead_tids, &entries_removed, nullptr, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Should be 0 because tid1 and tid3 have xmax=0 (not deleted)
    EXPECT_EQ(entries_removed, 0) << "Entries without xmax should not be removed";
}

} // anonymous namespace
