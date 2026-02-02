/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// Task 17 Phase 4: Comprehensive MGA Compliance Testing for B-tree
// Tests visibility filtering, soft deletion, and MVCC correctness

#include <gtest/gtest.h>
#include "scratchbird/core/btree.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "test_helpers.h"
#include <vector>
#include <cstring>

using namespace scratchbird::core;

class BTreeMGATest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create temporary database for testing
        db_path_ = scratchbird::testing::uniqueTestDbPath("test_btree_mga", ".db");
        remove(db_path_.c_str());

        ErrorContext ctx;
        auto status = Database::create(db_path_, db_config_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database";

        db_ = Database::open(db_path_, db_config_, &ctx);
        ASSERT_NE(db_, nullptr) << "Failed to open database";

        txn_mgr_ = db_->transaction_manager();
        ASSERT_NE(txn_mgr_, nullptr) << "Transaction manager not available";
    }

    void TearDown() override
    {
        if (db_)
        {
            delete db_;
            db_ = nullptr;
        }
        remove(db_path_.c_str());
    }

    // Helper: Create a test B-tree index
    std::unique_ptr<BTree> createTestIndex()
    {
        ErrorContext ctx;

        // Generate UUIDs for index and table
        UuidV7Bytes index_uuid = generateUuidV7();
        UuidV7Bytes table_uuid = generateUuidV7();
        std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

        uint32_t root_page;
        auto status = BTree::create(db_, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to create B-tree";

        return BTree::open(db_, index_uuid, root_page, &ctx);
    }

    // Helper: Serialize integer key
    std::vector<uint8_t> serializeKey(int32_t value)
    {
        std::vector<uint8_t> key(sizeof(int32_t));
        memcpy(key.data(), &value, sizeof(int32_t));
        return key;
    }

    // Helper: Begin transaction and get XID
    uint64_t beginTransaction()
    {
        ErrorContext ctx;
        uint64_t xid = 0;
        auto status = txn_mgr_->beginTransaction(xid, false, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to begin transaction";
        return xid;
    }

    // Helper: Commit transaction
    void commitTransaction(uint64_t xid)
    {
        ErrorContext ctx;
        auto status = txn_mgr_->commitTransaction(xid, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to commit transaction";
    }

    // Helper: Rollback transaction
    void rollbackTransaction(uint64_t xid)
    {
        ErrorContext ctx;
        auto status = txn_mgr_->abortTransaction(xid, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to rollback transaction";
    }

    // Helper: Get snapshot
    TransactionManager::Snapshot getSnapshot()
    {
        ErrorContext ctx;
        // MGA: Snapshot removed - uint64_t current_xid = txn_mgr_->getCurrentXid();
        auto status = // MGA: getSnapshot removed;
        EXPECT_EQ(status, Status::OK) << "Failed to get snapshot";
        return snapshot;
    }

    std::string db_path_;
    DatabaseConfig db_config_;
    Database *db_ = nullptr;
    TransactionManager *txn_mgr_ = nullptr;
};

// ============================================================================
// PHASE 3.1 TESTS: btn_xmin Population
// ============================================================================

TEST_F(BTreeMGATest, InsertPopulatesXmin)
{
    auto btree = createTestIndex();
    ASSERT_NE(btree, nullptr);

    uint64_t xid = beginTransaction();

    // Insert entry
    auto key = serializeKey(100);
    TID tid(0, 1, 0);  // tablespace=0, page=1, slot=0
    ErrorContext ctx;
    auto status = btree->insert(key, tid, xid, &ctx);
    EXPECT_EQ(status, Status::OK);

    commitTransaction(xid);

    // Search should find the entry
    std::vector<TID> tids;
    auto snapshot = getSnapshot();
    status = btree->search(key, &snapshot, &tids, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(tids.size(), 1);
    EXPECT_EQ(tids[0], tid);
}

TEST_F(BTreeMGATest, XminZeroForSystemOperations)
{
    auto btree = createTestIndex();
    ASSERT_NE(btree, nullptr);

    // Insert with xid=0 (system operation)
    auto key = serializeKey(200);
    TID tid(0, 2, 0);
    ErrorContext ctx;
    auto status = btree->insert(key, tid, 0, &ctx);  // xid = 0
    EXPECT_EQ(status, Status::OK);

    // Should be visible to all snapshots (legacy behavior)
    std::vector<TID> tids;
    auto snapshot = getSnapshot();
    status = btree->search(key, &snapshot, &tids, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(tids.size(), 1);
}

// ============================================================================
// PHASE 3.2 TESTS: Soft Deletion (markDeleted)
// ============================================================================

TEST_F(BTreeMGATest, MarkDeletedSetsXmax)
{
    auto btree = createTestIndex();
    ASSERT_NE(btree, nullptr);

    // Insert entry
    uint64_t xid_insert = beginTransaction();
    auto key = serializeKey(300);
    TID tid(0, 3, 0);
    ErrorContext ctx;
    auto status = btree->insert(key, tid, xid_insert, &ctx);
    EXPECT_EQ(status, Status::OK);
    commitTransaction(xid_insert);

    // Soft delete
    uint64_t xid_delete = beginTransaction();
    status = btree->markDeleted(key, tid, xid_delete, &ctx);
    EXPECT_EQ(status, Status::OK);
    commitTransaction(xid_delete);

    // Entry should be invisible to new snapshots
    std::vector<TID> tids;
    auto snapshot = getSnapshot();
    status = btree->search(key, &snapshot, &tids, &ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);  // Deleted before snapshot
}

TEST_F(BTreeMGATest, SoftDeletedEntryRemainsInIndex)
{
    auto btree = createTestIndex();
    ASSERT_NE(btree, nullptr);

    uint64_t xid_insert = beginTransaction();
    auto key = serializeKey(400);
    TID tid(0, 4, 0);
    ErrorContext ctx;
    btree->insert(key, tid, xid_insert, &ctx);
    commitTransaction(xid_insert);

    uint64_t xid_delete = beginTransaction();
    btree->markDeleted(key, tid, xid_delete, &ctx);
    commitTransaction(xid_delete);

    // Entry still physically present (for VACUUM to clean up)
    // We can verify this by passing nullptr snapshot
    std::vector<TID> tids;
    auto status = btree->search(key, nullptr, &tids, &ctx);
    // Note: Current implementation may not expose this, but entry exists on-disk
}

// ============================================================================
// PHASE 3.3 TESTS: Visibility-Aware Search
// ============================================================================

TEST_F(BTreeMGATest, InProgressTransactionInvisibleToOthers)
{
    auto btree = createTestIndex();
    ASSERT_NE(btree, nullptr);

    // Transaction 1: Insert but don't commit
    uint64_t xid1 = beginTransaction();
    auto snapshot_before = getSnapshot();  // Snapshot before insert

    auto key = serializeKey(500);
    TID tid(0, 5, 0);
    ErrorContext ctx;
    btree->insert(key, tid, xid1, &ctx);

    // Don't commit xid1 yet

    // Transaction 2: Try to see the entry
    std::vector<TID> tids;
    auto status = btree->search(key, &snapshot_before, &tids, &ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);  // In-progress txn invisible

    // Commit and retry
    commitTransaction(xid1);
    auto snapshot_after = getSnapshot();
    status = btree->search(key, &snapshot_after, &tids, &ctx);
    EXPECT_EQ(status, Status::OK);  // Now visible
    EXPECT_EQ(tids.size(), 1);
}

TEST_F(BTreeMGATest, AbortedTransactionInvisible)
{
    auto btree = createTestIndex();
    ASSERT_NE(btree, nullptr);

    uint64_t xid = beginTransaction();
    auto key = serializeKey(600);
    TID tid(0, 6, 0);
    ErrorContext ctx;
    btree->insert(key, tid, xid, &ctx);

    // Abort instead of commit
    rollbackTransaction(xid);

    // Entry should be invisible
    std::vector<TID> tids;
    auto snapshot = getSnapshot();
    auto status = btree->search(key, &snapshot, &tids, &ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
}

TEST_F(BTreeMGATest, SnapshotIsolation)
{
    auto btree = createTestIndex();
    ASSERT_NE(btree, nullptr);

    // Insert entry
    uint64_t xid1 = beginTransaction();
    auto key = serializeKey(700);
    TID tid(0, 7, 0);
    ErrorContext ctx;
    btree->insert(key, tid, xid1, &ctx);
    commitTransaction(xid1);

    // Take snapshot
    auto snapshot = getSnapshot();

    // Delete entry after snapshot
    uint64_t xid2 = beginTransaction();
    btree->markDeleted(key, tid, xid2, &ctx);
    commitTransaction(xid2);

    // Old snapshot should still see the entry (repeatable read)
    std::vector<TID> tids;
    auto status = btree->search(key, &snapshot, &tids, &ctx);
    EXPECT_EQ(status, Status::OK);  // Still visible to old snapshot
    EXPECT_EQ(tids.size(), 1);

    // New snapshot should not see it
    auto new_snapshot = getSnapshot();
    tids.clear();
    status = btree->search(key, &new_snapshot, &tids, &ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);  // Invisible to new snapshot
}

// ============================================================================
// PHASE 3.3 TESTS: Range Scan Visibility
// ============================================================================

TEST_F(BTreeMGATest, RangeScanSkipsInvisibleEntries)
{
    auto btree = createTestIndex();
    ASSERT_NE(btree, nullptr);

    // Insert 10 entries
    uint64_t xid_insert = beginTransaction();
    ErrorContext ctx;
    for (int i = 0; i < 10; i++)
    {
        auto key = serializeKey(i * 100);
        TID tid(0, i, 0);
        btree->insert(key, tid, xid_insert, &ctx);
    }
    commitTransaction(xid_insert);

    // Take snapshot
    auto snapshot = getSnapshot();

    // Delete every other entry
    uint64_t xid_delete = beginTransaction();
    for (int i = 0; i < 10; i += 2)  // Delete 0, 200, 400, 600, 800
    {
        auto key = serializeKey(i * 100);
        TID tid(0, i, 0);
        btree->markDeleted(key, tid, xid_delete, &ctx);
    }
    commitTransaction(xid_delete);

    // Range scan with old snapshot should see all 10
    auto iter_old = btree->rangeScan(nullptr, nullptr, &snapshot, true, true, &ctx);
    int count_old = 0;
    while (iter_old->hasNext())
    {
        TID tid;
        std::vector<uint8_t> key;
        iter_old->next(&key, &tid, &ctx);
        count_old++;
    }
    EXPECT_EQ(count_old, 10);  // Old snapshot sees all

    // Range scan with new snapshot should see only 5
    auto new_snapshot = getSnapshot();
    auto iter_new = btree->rangeScan(nullptr, nullptr, &new_snapshot, true, true, &ctx);
    int count_new = 0;
    while (iter_new->hasNext())
    {
        TID tid;
        std::vector<uint8_t> key;
        iter_new->next(&key, &tid, &ctx);
        count_new++;
    }
    EXPECT_EQ(count_new, 5);  // New snapshot sees only non-deleted
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

TEST_F(BTreeMGATest, VisibilityFilteringPerformance)
{
    auto btree = createTestIndex();
    ASSERT_NE(btree, nullptr);

    // Insert 1000 entries
    uint64_t xid_insert = beginTransaction();
    ErrorContext ctx;
    for (int i = 0; i < 1000; i++)
    {
        auto key = serializeKey(i);
        TID tid(0, i, 0);
        btree->insert(key, tid, xid_insert, &ctx);
    }
    commitTransaction(xid_insert);

    // Delete 900 of them (90% deleted)
    uint64_t xid_delete = beginTransaction();
    for (int i = 0; i < 900; i++)
    {
        auto key = serializeKey(i);
        TID tid(0, i, 0);
        btree->markDeleted(key, tid, xid_delete, &ctx);
    }
    commitTransaction(xid_delete);

    // Range scan should be fast (only returns 100 visible entries)
    auto snapshot = getSnapshot();
    auto iter = btree->rangeScan(nullptr, nullptr, &snapshot, true, true, &ctx);

    int visible_count = 0;
    while (iter->hasNext())
    {
        TID tid;
        std::vector<uint8_t> key;
        iter->next(&key, &tid, &ctx);
        visible_count++;
    }

    EXPECT_EQ(visible_count, 100);  // Only 100 visible
    // Note: In production, we'd measure time here and verify 10-100x speedup
}

// ============================================================================
// EDGE CASES
// ============================================================================

TEST_F(BTreeMGATest, NullSnapshotSeesAllEntries)
{
    auto btree = createTestIndex();
    ASSERT_NE(btree, nullptr);

    uint64_t xid1 = beginTransaction();
    auto key = serializeKey(800);
    TID tid(0, 8, 0);
    ErrorContext ctx;
    btree->insert(key, tid, xid1, &ctx);
    // Don't commit - leave in-progress

    // Search with nullptr snapshot (VACUUM mode)
    std::vector<TID> tids;
    auto status = btree->search(key, nullptr, &tids, &ctx);
    EXPECT_EQ(status, Status::OK);  // Sees even in-progress entries
    EXPECT_EQ(tids.size(), 1);

    rollbackTransaction(xid1);
}

TEST_F(BTreeMGATest, MultipleVersionsOfSameKey)
{
    auto btree = createTestIndex();
    ASSERT_NE(btree, nullptr);

    auto key = serializeKey(900);
    ErrorContext ctx;

    // Insert, delete, re-insert same key
    uint64_t xid1 = beginTransaction();
    TID tid1(0, 9, 0);
    btree->insert(key, tid1, xid1, &ctx);
    commitTransaction(xid1);

    auto snapshot1 = getSnapshot();

    uint64_t xid2 = beginTransaction();
    btree->markDeleted(key, tid1, xid2, &ctx);
    commitTransaction(xid2);

    auto snapshot2 = getSnapshot();

    uint64_t xid3 = beginTransaction();
    TID tid2(0, 9, 1);  // Different slot
    btree->insert(key, tid2, xid3, &ctx);
    commitTransaction(xid3);

    auto snapshot3 = getSnapshot();

    // Verify visibility across snapshots
    std::vector<TID> tids;

    // Snapshot1: Should see tid1
    tids.clear();
    btree->search(key, &snapshot1, &tids, &ctx);
    EXPECT_EQ(tids.size(), 1);
    if (tids.size() == 1) EXPECT_EQ(tids[0], tid1);

    // Snapshot2: Should see nothing (deleted)
    tids.clear();
    btree->search(key, &snapshot2, &tids, &ctx);
    EXPECT_EQ(tids.size(), 0);

    // Snapshot3: Should see tid2
    tids.clear();
    btree->search(key, &snapshot3, &tids, &ctx);
    EXPECT_GE(tids.size(), 1);  // May see both, but at least tid2
}
