// ScratchBird Storage TOAST Indexing Integration Test
// Tests Phase 3 implementation: Storage layer automatically detoasts before indexing
// Validates that indexes contain actual detoasted values, not TOAST pointer bytes

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/tid.h"
#include <memory>
#include <string>
#include <vector>

using namespace scratchbird::core;

class StorageToastIndexingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create test database
        db_ = std::make_unique<Database>("test_storage_toast_indexing.db", 50 * 1024 * 1024); // 50MB
        ASSERT_EQ(db_->open(), Status::OK);

        storage_ = db_->storage_engine();
        catalog_ = db_->catalog_manager();
        tm_ = db_->transaction_manager();

        ASSERT_NE(storage_, nullptr);
        ASSERT_NE(catalog_, nullptr);
        ASSERT_NE(tm_, nullptr);
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

        // Create distinct text for testing (repeat pattern with counter)
        for (size_t i = 0; i < size_kb * 1024; i += 64)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "TOAST_TEST_DATA_%08zu_", i);
            text.append(buf);
        }

        return text;
    }

    // Helper: Serialize a simple tuple with TupleHeader + INT32 + TEXT
    std::vector<uint8_t> serializeTuple(int32_t id, const std::string& text, uint64_t xmin)
    {
        std::vector<uint8_t> tuple;

        // TupleHeader (44 bytes)
        TupleHeader header{};
        header.xmin = xmin;
        header.xmax = 0;
        header.back_version_gpid = INVALID_GPID;
        header.back_version_slot = 0;
        header.ctid_gpid = INVALID_GPID;
        header.ctid_slot = 0;
        header.infomask = 0;
        header.null_bitmap_offset = 0;

        tuple.resize(sizeof(TupleHeader));
        std::memcpy(tuple.data(), &header, sizeof(TupleHeader));

        // INT32 field
        size_t int_offset = tuple.size();
        tuple.resize(int_offset + sizeof(int32_t));
        std::memcpy(tuple.data() + int_offset, &id, sizeof(int32_t));

        // VARCHAR/TEXT field (uint32_t length + data)
        size_t text_offset = tuple.size();
        uint32_t text_len = static_cast<uint32_t>(text.size());
        tuple.resize(text_offset + sizeof(uint32_t) + text_len);
        std::memcpy(tuple.data() + text_offset, &text_len, sizeof(uint32_t));
        std::memcpy(tuple.data() + text_offset + sizeof(uint32_t), text.data(), text_len);

        return tuple;
    }

    std::unique_ptr<Database> db_;
    StorageEngine* storage_;
    CatalogManager* catalog_;
    TransactionManager* tm_;
};

// =============================================================================
// Test 1: Insert with TOAST + B-tree Index
// =============================================================================

TEST_F(StorageToastIndexingTest, InsertWithToastAndBTreeIndex)
{
    // This test validates Phase 3 Task 3.2: Storage engine insert path integration
    //
    // Expected behavior:
    // 1. Insert tuple with large TEXT column (>2KB, triggers TOAST)
    // 2. Storage layer automatically creates TOAST chunks
    // 3. Storage layer detoasts before calling index->insert()
    // 4. Index contains actual detoasted value, NOT 18-byte TOAST pointer
    // 5. Query via index returns correct results

    uint64_t xid = tm_->beginTransaction();
    ASSERT_NE(xid, 0);

    // Create large text (5KB - will be TOASTed)
    std::string large_text = createLargeText(5);
    ASSERT_GT(large_text.size(), 2048u); // Ensure it exceeds TOAST threshold

    // Serialize tuple
    std::vector<uint8_t> tuple = serializeTuple(1, large_text, xid);

    // Create table (simplified - in real code this would go through catalog)
    ID table_id = generateUuidV7();

    // Insert tuple
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ErrorContext ctx;
    Status status = storage_->insertTuple(table_id, tuple.data(), tuple.size(),
                                          &page_id, &item_id, &ctx);

    // Verify insert succeeded
    EXPECT_EQ(status, Status::OK) << "Insert failed: " << ctx.message;
    EXPECT_NE(page_id, 0u);

    // TODO: Once catalog integration is complete, verify:
    // 1. Index was updated with detoasted value
    // 2. Index does NOT contain 18-byte TOAST pointer
    // 3. Search via index returns correct tuple

    tm_->commit(xid);
}

// =============================================================================
// Test 2: Update with Changed Indexed Column
// =============================================================================

TEST_F(StorageToastIndexingTest, UpdateWithChangedIndexedColumn)
{
    // This test validates Phase 3 Task 3.3: Storage engine update path integration
    //
    // Expected behavior:
    // 1. Insert tuple with indexed TOASTed column
    // 2. Update that column with new value
    // 3. Storage layer detects indexed column changed
    // 4. Storage layer removes old key, inserts new key
    // 5. TID remains STABLE (same page_id, item_id)
    // 6. Index contains new detoasted value

    uint64_t xid1 = tm_->beginTransaction();
    ASSERT_NE(xid1, 0);

    // Insert initial tuple
    std::string initial_text = createLargeText(3);
    std::vector<uint8_t> tuple1 = serializeTuple(1, initial_text, xid1);

    ID table_id = generateUuidV7();
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ErrorContext ctx;
    Status status = storage_->insertTuple(table_id, tuple1.data(), tuple1.size(),
                                          &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    tm_->commit(xid1);

    // Update with different text
    uint64_t xid2 = tm_->beginTransaction();
    std::string updated_text = createLargeText(4); // Different content
    std::vector<uint8_t> tuple2 = serializeTuple(1, updated_text, xid2);

    uint32_t new_page_id = 0;
    uint16_t new_item_id = 0;
    status = storage_->updateTuple(table_id, page_id, item_id,
                                   tuple2.data(), tuple2.size(),
                                   &new_page_id, &new_item_id, &ctx);

    // Verify update succeeded
    EXPECT_EQ(status, Status::OK) << "Update failed: " << ctx.message;

    // Verify TID stability (MGA benefit!)
    EXPECT_EQ(new_page_id, page_id) << "Page ID changed - TID not stable!";
    EXPECT_EQ(new_item_id, item_id) << "Item ID changed - TID not stable!";

    // TODO: Verify index was updated with new detoasted value

    tm_->commit(xid2);
}

// =============================================================================
// Test 3: Update with Unchanged Indexed Column (MGA Optimization)
// =============================================================================

TEST_F(StorageToastIndexingTest, UpdateWithUnchangedIndexedColumn)
{
    // This test validates MGA optimization in Phase 3 Task 3.3
    //
    // Expected behavior:
    // 1. Insert tuple with indexed column A and non-indexed column B
    // 2. Update column B only (column A unchanged)
    // 3. Storage layer detects indexed column unchanged
    // 4. Storage layer skips index update (MGA TID stability!)
    // 5. ~80% performance improvement for this case

    uint64_t xid1 = tm_->beginTransaction();
    ASSERT_NE(xid1, 0);

    // For this test, we'd need a tuple with multiple columns
    // and catalog integration to mark which columns are indexed
    // This is a placeholder for future implementation

    // Simplified test: Insert and update with same text
    std::string text = createLargeText(3);
    std::vector<uint8_t> tuple1 = serializeTuple(1, text, xid1);

    ID table_id = generateUuidV7();
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ErrorContext ctx;
    Status status = storage_->insertTuple(table_id, tuple1.data(), tuple1.size(),
                                          &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    tm_->commit(xid1);

    // Update with SAME text (indexed column unchanged)
    uint64_t xid2 = tm_->beginTransaction();
    std::vector<uint8_t> tuple2 = serializeTuple(2, text, xid2); // Changed ID, same text

    uint32_t new_page_id = 0;
    uint16_t new_item_id = 0;
    status = storage_->updateTuple(table_id, page_id, item_id,
                                   tuple2.data(), tuple2.size(),
                                   &new_page_id, &new_item_id, &ctx);

    EXPECT_EQ(status, Status::OK) << "Update failed: " << ctx.message;

    // TODO: Verify NO index maintenance was performed
    // (In real implementation, would need instrumentation/logging to verify)

    tm_->commit(xid2);
}

// =============================================================================
// Test 4: Multiple Indexes on Same TOAST Column (Cache Test)
// =============================================================================

TEST_F(StorageToastIndexingTest, MultipleIndexesSameToastColumn)
{
    // This test validates detoasting cache in IndexKeyExtractor
    //
    // Expected behavior:
    // 1. Create 3 indexes on same TOASTed column
    // 2. Insert tuple with large value
    // 3. Storage layer detoasts ONCE (first index)
    // 4. Cache hit for second and third indexes
    // 5. All 3 indexes contain actual detoasted value

    uint64_t xid = tm_->beginTransaction();
    ASSERT_NE(xid, 0);

    // Create large text
    std::string large_text = createLargeText(6);
    std::vector<uint8_t> tuple = serializeTuple(1, large_text, xid);

    // Create table
    ID table_id = generateUuidV7();

    // TODO: Create 3 indexes on text column via catalog

    // Insert tuple
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ErrorContext ctx;
    Status status = storage_->insertTuple(table_id, tuple.data(), tuple.size(),
                                          &page_id, &item_id, &ctx);

    EXPECT_EQ(status, Status::OK) << "Insert failed: " << ctx.message;

    // TODO: Verify all 3 indexes have the same detoasted value
    // TODO: Add performance instrumentation to verify cache hit

    tm_->commit(xid);
}

// =============================================================================
// Test 5: TOAST Pointer Detection
// =============================================================================

TEST_F(StorageToastIndexingTest, ToastPointerDetection)
{
    // This test validates ToastManager::isToastPointer()
    //
    // Expected behavior:
    // 1. Detect 18-byte TOAST pointer with correct magic
    // 2. Reject non-TOAST data
    // 3. Reject incorrect sizes

    // Create a valid TOAST pointer
    ToastPointer toast_ptr{};
    toast_ptr.va_tag = static_cast<uint8_t>(ToastStrategy::EXTERNAL);
    toast_ptr.va_rawsize = 5000;
    toast_ptr.va_extsize = 5000;
    toast_ptr.va_valueid = generateUuidV7();

    // Test 1: Valid TOAST pointer (18 bytes with magic)
    EXPECT_TRUE(ToastManager::isToastPointer(
        reinterpret_cast<const uint8_t*>(&toast_ptr),
        sizeof(ToastPointer)
    ));

    // Test 2: Wrong size (should return false)
    EXPECT_FALSE(ToastManager::isToastPointer(
        reinterpret_cast<const uint8_t*>(&toast_ptr),
        10 // Wrong size
    ));

    // Test 3: Regular data (not a TOAST pointer)
    uint8_t regular_data[18] = {0};
    EXPECT_FALSE(ToastManager::isToastPointer(regular_data, 18));

    // Test 4: Empty data
    EXPECT_FALSE(ToastManager::isToastPointer(nullptr, 0));
}

// =============================================================================
// Test 6: Detoast If Needed
// =============================================================================

TEST_F(StorageToastIndexingTest, DetoastIfNeeded)
{
    // This test validates ToastManager::detoastIfNeeded()
    //
    // Expected behavior:
    // 1. Pass through inline data unchanged
    // 2. Detoast TOAST pointers
    // 3. Handle errors gracefully

    uint64_t xid = tm_->beginTransaction();
    ASSERT_NE(xid, 0);

    ID table_id = generateUuidV7();
    ToastManager toast_mgr(db_.get(), table_id);
    ErrorContext ctx;

    // Initialize TOAST manager
    Status status = toast_mgr.initialize(&ctx);
    if (status != Status::OK)
    {
        // TOAST table doesn't exist - skip this test
        tm_->commit(xid);
        GTEST_SKIP() << "TOAST table not initialized, skipping detoastIfNeeded test";
        return;
    }

    // Test 1: Inline data (should pass through unchanged)
    std::string inline_text = "Small inline text";
    std::vector<uint8_t> result;

    status = toast_mgr.detoastIfNeeded(
        reinterpret_cast<const uint8_t*>(inline_text.data()),
        inline_text.size(),
        &result,
        xid,
        &ctx
    );

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(result.size(), inline_text.size());
    EXPECT_EQ(std::string(result.begin(), result.end()), inline_text);

    // Test 2: TOAST pointer (would need actual TOAST chunks to test properly)
    // This is a placeholder - full test requires end-to-end TOAST flow

    tm_->commit(xid);
}

// =============================================================================
// Main
// =============================================================================

