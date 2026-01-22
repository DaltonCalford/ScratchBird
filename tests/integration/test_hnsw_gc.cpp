/**
 * Plan 01 Task D.1: HNSW GC Tests
 *
 * Tests for removeDeadEntries() per plan_01_task_d_index_gc_checklist.md
 *
 * Test coverage:
 * 1. Unit test: Create index, insert, delete, run GC, verify
 * 2. Restart test: GC works after database restart
 * 3. Negative test: Empty dead_tids is no-op
 */

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/hnsw_index.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/vector.h"
#include <filesystem>
#include <memory>

using namespace scratchbird::core;

namespace {

class HnswGCTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create temporary database
        test_db_path_ = "/tmp/test_hnsw_gc.db";
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

    std::unique_ptr<Database> db_;
    std::string test_db_path_;
};

/**
 * Test 1: Unit test - Insert vectors, delete some, run GC, verify deleted nodes marked
 *
 * This test verifies the basic GC functionality:
 * - Create HNSW index
 * - Insert vectors
 * - Delete vectors (creating dead TIDs)
 * - Run GC
 * - Verify nodes are marked as deleted (xmax set, DELETED flag set)
 */
TEST_F(HnswGCTest, BasicGCMarksDeletedNodes)
{
    // Generate unique UUIDs
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    // Create HNSW index (3 dimensions)
    GPID root_gpid = 0;
    Status status = db_->page_manager()->allocatePageInTablespace(0, &root_gpid, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to allocate HNSW root page";
    uint32_t root_page = static_cast<uint32_t>(getPageNumber(root_gpid));
    status = HnswIndex::create(db_.get(), index_uuid, table_uuid,
                                     {column_uuid}, 3,
                                     DistanceMetric::EUCLIDEAN,
                                     16, 200, 100, root_gpid, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to create HNSW index";
    ASSERT_GT(root_page, 0);

    // Open index
    auto hnsw = HnswIndex::open(db_.get(), index_uuid, root_gpid, nullptr);
    ASSERT_NE(hnsw, nullptr) << "Failed to open HNSW index";

    // Insert 5 vectors
    std::vector<TID> inserted_tids;
    for (uint32_t i = 0; i < 5; i++)
    {
        // Create vector (3D)
        std::vector<float> vec_data = {
            static_cast<float>(i),
            static_cast<float>(i * 2),
            static_cast<float>(i * 3)
        };
        VectorValue vec(vec_data);

        // Create TID (tablespace 0, page 100, slot i)
        TID tid(0, 100, i);
        inserted_tids.push_back(tid);

        status = hnsw->insert(vec, tid, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to insert vector " << i;
    }

    // Flush to ensure persistence
    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK);

    // Delete TIDs 1, 2, 4 (3 out of 5)
    std::vector<TID> dead_tids = {
        inserted_tids[1],
        inserted_tids[2],
        inserted_tids[4]
    };

    // Run GC
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;
    status = hnsw->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, nullptr);
    ASSERT_EQ(status, Status::OK) << "GC failed";

    // Verify counters
    EXPECT_EQ(entries_removed, 3) << "Should have marked 3 nodes as deleted";
    EXPECT_GT(pages_modified, 0) << "Should have modified at least one page";

    // Flush to ensure persistence
    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK);
}

/**
 * Test 2: Restart test - GC survives database restart
 *
 * This test verifies that deleted nodes remain marked after restart.
 */
TEST_F(HnswGCTest, GCSurvivesRestart)
{
    // Generate unique UUIDs
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    GPID root_gpid = 0;
    uint32_t root_page = 0;

    // Phase 1: Create index, insert, GC
    {
        Status status = db_->page_manager()->allocatePageInTablespace(0, &root_gpid, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to allocate HNSW root page";
        root_page = static_cast<uint32_t>(getPageNumber(root_gpid));
        status = HnswIndex::create(db_.get(), index_uuid, table_uuid,
                                         {column_uuid}, 2,
                                         DistanceMetric::EUCLIDEAN,
                                         16, 200, 100, root_gpid, nullptr);
        ASSERT_EQ(status, Status::OK);

        auto hnsw = HnswIndex::open(db_.get(), index_uuid, root_gpid, nullptr);
        ASSERT_NE(hnsw, nullptr);

        // Insert 3 vectors
        std::vector<TID> inserted_tids;
        for (uint32_t i = 0; i < 3; i++)
        {
            std::vector<float> vec_data = {static_cast<float>(i), static_cast<float>(i * 2)};
            VectorValue vec(vec_data);
            TID tid(0, 200, i);
            inserted_tids.push_back(tid);

            status = hnsw->insert(vec, tid, nullptr);
            ASSERT_EQ(status, Status::OK);
        }

        // Delete vector 1
        std::vector<TID> dead_tids = {inserted_tids[1]};

        uint64_t entries_removed = 0;
        uint64_t pages_modified = 0;
        status = hnsw->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, nullptr);
        ASSERT_EQ(status, Status::OK);
        EXPECT_EQ(entries_removed, 1);

        // Flush to disk
        status = db_->buffer_pool()->flushAll(nullptr);
        ASSERT_EQ(status, Status::OK);
    }

    // Phase 2: Close and reopen database
    db_->close();
    Status status = db_->open(test_db_path_, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to reopen database";

    // Phase 3: Open index and verify GC persisted
    {
        auto hnsw = HnswIndex::open(db_.get(), index_uuid, root_page, nullptr);
        ASSERT_NE(hnsw, nullptr) << "Failed to reopen HNSW index after restart";

        // Running GC again with same dead TID should find it already deleted
        std::vector<TID> dead_tids = {TID(0, 200, 1)};

        uint64_t entries_removed = 0;
        uint64_t pages_modified = 0;
        status = hnsw->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, nullptr);
        ASSERT_EQ(status, Status::OK);

        // Should be 0 because node was already marked deleted in Phase 1
        EXPECT_EQ(entries_removed, 0) << "Node should already be deleted from Phase 1";
    }
}

/**
 * Test 3: Negative test - Empty dead_tids is no-op
 *
 * This test verifies that passing empty dead_tids returns OK
 * without modifying anything.
 */
TEST_F(HnswGCTest, EmptyDeadTidsIsNoOp)
{
    // Generate unique UUIDs
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    // Create HNSW index
    GPID root_gpid = 0;
    Status status = db_->page_manager()->allocatePageInTablespace(0, &root_gpid, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to allocate HNSW root page";
    uint32_t root_page = static_cast<uint32_t>(getPageNumber(root_gpid));
    status = HnswIndex::create(db_.get(), index_uuid, table_uuid,
                                     {column_uuid}, 2,
                                     DistanceMetric::EUCLIDEAN,
                                     16, 200, 100, root_gpid, nullptr);
    ASSERT_EQ(status, Status::OK);

    auto hnsw = HnswIndex::open(db_.get(), index_uuid, root_gpid, nullptr);
    ASSERT_NE(hnsw, nullptr);

    // Insert one vector
    std::vector<float> vec_data = {1.0f, 2.0f};
    VectorValue vec(vec_data);
    TID tid(0, 300, 0);
    status = hnsw->insert(vec, tid, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Run GC with empty dead_tids
    std::vector<TID> empty_dead_tids;
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = hnsw->removeDeadEntries(empty_dead_tids, &entries_removed, &pages_modified, nullptr);
    ASSERT_EQ(status, Status::OK) << "Empty dead_tids should return OK";

    // Verify counters are zero
    EXPECT_EQ(entries_removed, 0) << "No entries should be removed";
    EXPECT_EQ(pages_modified, 0) << "No pages should be modified";
}

/**
 * Test 4: Multiple GC passes work correctly
 *
 * This test verifies that multiple GC passes with different dead_tids
 * all work correctly.
 */
TEST_F(HnswGCTest, MultipleGCPasses)
{
    // Generate unique UUIDs
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    // Create HNSW index
    GPID root_gpid = 0;
    Status status = db_->page_manager()->allocatePageInTablespace(0, &root_gpid, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to allocate HNSW root page";
    uint32_t root_page = static_cast<uint32_t>(getPageNumber(root_gpid));
    status = HnswIndex::create(db_.get(), index_uuid, table_uuid,
                                     {column_uuid}, 2,
                                     DistanceMetric::EUCLIDEAN,
                                     16, 200, 100, root_gpid, nullptr);
    ASSERT_EQ(status, Status::OK);

    auto hnsw = HnswIndex::open(db_.get(), index_uuid, root_gpid, nullptr);
    ASSERT_NE(hnsw, nullptr);

    // Insert 6 vectors
    std::vector<TID> inserted_tids;
    for (uint32_t i = 0; i < 6; i++)
    {
        std::vector<float> vec_data = {static_cast<float>(i), static_cast<float>(i * 10)};
        VectorValue vec(vec_data);
        TID tid(0, 400, i);
        inserted_tids.push_back(tid);

        status = hnsw->insert(vec, tid, nullptr);
        ASSERT_EQ(status, Status::OK);
    }

    // First GC pass: Delete TIDs 0, 1
    {
        std::vector<TID> dead_tids = {inserted_tids[0], inserted_tids[1]};
        uint64_t entries_removed = 0;
        status = hnsw->removeDeadEntries(dead_tids, &entries_removed, nullptr, nullptr);
        ASSERT_EQ(status, Status::OK);
        EXPECT_EQ(entries_removed, 2);
    }

    // Second GC pass: Delete TIDs 3, 4, 5
    {
        std::vector<TID> dead_tids = {inserted_tids[3], inserted_tids[4], inserted_tids[5]};
        uint64_t entries_removed = 0;
        status = hnsw->removeDeadEntries(dead_tids, &entries_removed, nullptr, nullptr);
        ASSERT_EQ(status, Status::OK);
        EXPECT_EQ(entries_removed, 3);
    }

    // Third GC pass: Try to delete already deleted TID 1 (should be 0)
    {
        std::vector<TID> dead_tids = {inserted_tids[1]};
        uint64_t entries_removed = 0;
        status = hnsw->removeDeadEntries(dead_tids, &entries_removed, nullptr, nullptr);
        ASSERT_EQ(status, Status::OK);
        EXPECT_EQ(entries_removed, 0) << "Already deleted node should not be counted again";
    }
}

} // anonymous namespace
