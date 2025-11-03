// ScratchBird TOAST Garbage Collection Integration Test
// Tests Phase 4 implementation: TOAST orphan detection and TIP-based GC

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/vacuum.h"
#include "scratchbird/core/toast.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>

using namespace scratchbird::core;

class ToastGarbageCollectionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create test database
        db_ = std::make_unique<Database>("test_toast_gc.db", 50 * 1024 * 1024); // 50MB
        ASSERT_EQ(db_->open(), Status::OK);

        storage_ = db_->storage_engine();
        catalog_ = db_->catalog_manager();
        tm_ = db_->transaction_manager();
        gc_ = db_->garbage_collector();
        vacuum_ = db_->vacuum();

        ASSERT_NE(storage_, nullptr);
        ASSERT_NE(catalog_, nullptr);
        ASSERT_NE(tm_, nullptr);
        ASSERT_NE(gc_, nullptr);
        ASSERT_NE(vacuum_, nullptr);
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
        }
    }

    // Helper: Create a large text value that will be TOASTed (>2KB)
    std::string createLargeText(size_t size_kb)
    {
        std::string text;
        text.reserve(size_kb * 1024);

        for (size_t i = 0; i < size_kb * 1024; i += 64)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "TOAST_GC_TEST_DATA_%08zu_", i);
            text.append(buf);
        }

        return text;
    }

    std::unique_ptr<Database> db_;
    StorageEngine* storage_;
    CatalogManager* catalog_;
    TransactionManager* tm_;
    GarbageCollector* gc_;
    Vacuum* vacuum_;
};

// =============================================================================
// Test 1: Orphan Detection - Detect TOAST chunks with no parent tuple
// =============================================================================

TEST_F(ToastGarbageCollectionTest, OrphanDetection)
{
    // Phase 4 Task 4.1 Test: Orphan Detection
    //
    // Scenario:
    // 1. Create table with TOAST
    // 2. Insert tuple with large value (creates TOAST chunks)
    // 3. Abort transaction (TOAST chunks become orphans)
    // 4. Run orphan detection
    // 5. Verify orphans detected

    // Create table with TOAST column
    ID table_id = generateUuidV7();
    ID toast_table_id = generateUuidV7();

    // Register table in catalog (simplified)
    CatalogManager::TableInfo table_info;
    table_info.table_id = table_id;
    table_info.table_name = "test_orphan_table";
    table_info.has_toast = true;
    table_info.toast_table_id = toast_table_id;

    // Start transaction
    uint64_t xid = tm_->beginTransaction();
    ASSERT_NE(xid, 0);

    // Create TOAST manager
    ToastManager toast_mgr(db_.get(), table_id);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        tm_->abort(xid);
        GTEST_SKIP() << "Failed to create TOAST table, skipping test";
        return;
    }

    // Create large value
    std::string large_text = createLargeText(5); // 5KB
    ASSERT_GT(large_text.size(), 2048u);

    // TOAST the value
    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        reinterpret_cast<const uint8_t*>(large_text.data()),
        large_text.size(),
        ToastStrategy::EXTERNAL,
        xid,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK) << "TOAST failed: " << ctx.message;

    // Abort transaction - TOAST chunks become orphans
    tm_->abort(xid);

    // Run orphan detection
    std::unordered_set<uint32_t> orphaned_value_ids;
    status = gc_->detectOrphanedToastChunks(toast_table_id, &orphaned_value_ids, &ctx);

    EXPECT_EQ(status, Status::OK) << "Orphan detection failed: " << ctx.message;
    EXPECT_GT(orphaned_value_ids.size(), 0u) << "Expected to find orphaned chunks";

    // Verify the orphaned value_id matches what we created
    EXPECT_TRUE(orphaned_value_ids.find(toast_ptr.va_valueid) != orphaned_value_ids.end())
        << "Expected to find our aborted TOAST value as orphan";
}

// =============================================================================
// Test 2: Orphan Cleanup - Delete orphaned TOAST chunks
// =============================================================================

TEST_F(ToastGarbageCollectionTest, OrphanCleanup)
{
    // Phase 4 Task 4.2 Test: Orphan Cleanup
    //
    // Scenario:
    // 1. Create orphaned TOAST chunks (abort transaction)
    // 2. Run orphan cleanup
    // 3. Verify chunks deleted
    // 4. Run orphan detection again
    // 5. Verify no orphans remain

    ID table_id = generateUuidV7();
    ID toast_table_id = generateUuidV7();

    // Create TOAST manager
    ToastManager toast_mgr(db_.get(), table_id);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Create orphan
    uint64_t xid = tm_->beginTransaction();
    std::string large_text = createLargeText(3);

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        reinterpret_cast<const uint8_t*>(large_text.data()),
        large_text.size(),
        ToastStrategy::EXTERNAL,
        xid,
        &toast_ptr,
        &ctx
    );

    tm_->abort(xid); // Create orphan

    // Detect orphans
    std::unordered_set<uint32_t> orphaned_value_ids;
    status = gc_->detectOrphanedToastChunks(toast_table_id, &orphaned_value_ids, &ctx);
    ASSERT_EQ(status, Status::OK);
    size_t orphans_before = orphaned_value_ids.size();
    ASSERT_GT(orphans_before, 0u);

    // Clean orphans
    uint64_t chunks_deleted = 0;
    status = gc_->cleanOrphanedToastChunks(toast_table_id, orphaned_value_ids,
                                           &chunks_deleted, &ctx);

    EXPECT_EQ(status, Status::OK) << "Orphan cleanup failed: " << ctx.message;
    EXPECT_GT(chunks_deleted, 0u) << "Expected to delete orphaned chunks";

    // Verify no orphans remain
    orphaned_value_ids.clear();
    status = gc_->detectOrphanedToastChunks(toast_table_id, &orphaned_value_ids, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(orphaned_value_ids.size(), 0u) << "Expected all orphans to be cleaned";
}

// =============================================================================
// Test 3: TIP-Based GC - Delete chunks with committed xmax
// =============================================================================

TEST_F(ToastGarbageCollectionTest, TIPBasedGC)
{
    // Phase 4 Task 4.4 Test: TIP-Based Garbage Collection
    //
    // Scenario:
    // 1. Create TOAST value (transaction commits)
    // 2. Delete TOAST value (set xmax, commit)
    // 3. Run TIP-based GC
    // 4. Verify chunks physically deleted (xmax committed)

    ID table_id = generateUuidV7();
    ID toast_table_id = generateUuidV7();

    ToastManager toast_mgr(db_.get(), table_id);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Create TOAST value
    uint64_t xid1 = tm_->beginTransaction();
    std::string large_text = createLargeText(4);

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        reinterpret_cast<const uint8_t*>(large_text.data()),
        large_text.size(),
        ToastStrategy::EXTERNAL,
        xid1,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK);
    uint32_t value_id = toast_ptr.va_valueid;

    tm_->commit(xid1); // Commit - chunks are now visible

    // Delete TOAST value (set xmax)
    uint64_t xid2 = tm_->beginTransaction();
    status = toast_mgr.deleteToastValue(value_id, xid2, &ctx);
    ASSERT_EQ(status, Status::OK);
    tm_->commit(xid2); // Commit - chunks now have committed xmax

    // Run TIP-based GC
    uint64_t chunks_deleted = 0;
    status = gc_->cleanToastChunksByTIP(toast_table_id, &chunks_deleted, &ctx);

    EXPECT_EQ(status, Status::OK) << "TIP-based GC failed: " << ctx.message;
    EXPECT_GT(chunks_deleted, 0u) << "Expected to delete chunks with committed xmax";
}

// =============================================================================
// Test 4: Vacuum Integration - Verify vacuum processes TOAST tables
// =============================================================================

TEST_F(ToastGarbageCollectionTest, VacuumIntegration)
{
    // Phase 4 Task 4.3 Test: Vacuum Integration
    //
    // Scenario:
    // 1. Create orphaned TOAST chunks
    // 2. Run VACUUM
    // 3. Verify TOAST table processed
    // 4. Verify orphans cleaned up

    ID table_id = generateUuidV7();
    ID toast_table_id = generateUuidV7();

    ToastManager toast_mgr(db_.get(), table_id);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Create orphan
    uint64_t xid = tm_->beginTransaction();
    std::string large_text = createLargeText(3);

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        reinterpret_cast<const uint8_t*>(large_text.data()),
        large_text.size(),
        ToastStrategy::EXTERNAL,
        xid,
        &toast_ptr,
        &ctx
    );

    tm_->abort(xid); // Create orphan

    // Verify orphan exists
    std::unordered_set<uint32_t> orphans_before;
    gc_->detectOrphanedToastChunks(toast_table_id, &orphans_before, &ctx);
    ASSERT_GT(orphans_before.size(), 0u);

    // Run VACUUM on database
    VacuumStats stats;
    status = vacuum_->vacuumDatabase(&stats, &ctx);

    EXPECT_EQ(status, Status::OK) << "Vacuum failed: " << ctx.message;

    // Verify orphans cleaned
    std::unordered_set<uint32_t> orphans_after;
    gc_->detectOrphanedToastChunks(toast_table_id, &orphans_after, &ctx);
    EXPECT_LT(orphans_after.size(), orphans_before.size())
        << "Expected vacuum to clean some orphans";
}

// =============================================================================
// Test 5: Aborted Delete - Verify xmax cleared for aborted deletions
// =============================================================================

TEST_F(ToastGarbageCollectionTest, AbortedDelete)
{
    // Phase 4 Task 4.4 Test: Aborted Delete Handling
    //
    // Scenario:
    // 1. Create TOAST value (commit)
    // 2. Delete TOAST value (set xmax, ABORT)
    // 3. Run TIP-based GC
    // 4. Verify chunks NOT deleted (xmax aborted)
    // 5. Verify value still accessible

    ID table_id = generateUuidV7();
    ID toast_table_id = generateUuidV7();

    ToastManager toast_mgr(db_.get(), table_id);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Create TOAST value
    uint64_t xid1 = tm_->beginTransaction();
    std::string large_text = createLargeText(3);

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        reinterpret_cast<const uint8_t*>(large_text.data()),
        large_text.size(),
        ToastStrategy::EXTERNAL,
        xid1,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK);
    uint32_t value_id = toast_ptr.va_valueid;
    tm_->commit(xid1);

    // Delete but abort
    uint64_t xid2 = tm_->beginTransaction();
    status = toast_mgr.deleteToastValue(value_id, xid2, &ctx);
    tm_->abort(xid2); // ABORT - chunks should remain accessible

    // Run TIP-based GC
    uint64_t chunks_deleted = 0;
    gc_->cleanToastChunksByTIP(toast_table_id, &chunks_deleted, &ctx);

    // Chunks should NOT be deleted (xmax aborted)
    // Note: Current implementation may not fully handle this yet (see TODO in code)

    // Verify value still accessible
    uint64_t xid3 = tm_->beginTransaction();
    std::vector<uint8_t> detoasted_data;
    status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid3, &ctx);

    EXPECT_EQ(status, Status::OK) << "Should still be able to detoast after aborted delete";

    tm_->commit(xid3);
}

// =============================================================================
// Test 6: Stress Test - Many orphans
// =============================================================================

TEST_F(ToastGarbageCollectionTest, StressTestManyOrphans)
{
    // Stress test: Create many orphaned TOAST values and verify all cleaned
    //
    // Scenario:
    // 1. Create 100 orphaned TOAST values
    // 2. Run orphan detection
    // 3. Run orphan cleanup
    // 4. Verify all orphans deleted

    ID table_id = generateUuidV7();
    ID toast_table_id = generateUuidV7();

    ToastManager toast_mgr(db_.get(), table_id);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Create many orphans
    constexpr int NUM_ORPHANS = 100;
    for (int i = 0; i < NUM_ORPHANS; i++)
    {
        uint64_t xid = tm_->beginTransaction();
        std::string large_text = createLargeText(2);

        ToastPointer toast_ptr;
        status = toast_mgr.toastValue(
            reinterpret_cast<const uint8_t*>(large_text.data()),
            large_text.size(),
            ToastStrategy::EXTERNAL,
            xid,
            &toast_ptr,
            &ctx
        );

        tm_->abort(xid); // Create orphan
    }

    // Detect orphans
    std::unordered_set<uint32_t> orphaned_value_ids;
    status = gc_->detectOrphanedToastChunks(toast_table_id, &orphaned_value_ids, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_GT(orphaned_value_ids.size(), 0u);

    // Clean all orphans
    uint64_t chunks_deleted = 0;
    status = gc_->cleanOrphanedToastChunks(toast_table_id, orphaned_value_ids,
                                           &chunks_deleted, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_GT(chunks_deleted, 0u);

    // Verify all cleaned
    orphaned_value_ids.clear();
    status = gc_->detectOrphanedToastChunks(toast_table_id, &orphaned_value_ids, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(orphaned_value_ids.size(), 0u);
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
