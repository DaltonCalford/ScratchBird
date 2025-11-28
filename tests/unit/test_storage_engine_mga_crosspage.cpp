#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/catalog_manager.h"
#include <cstring>
#include <filesystem>
#include <vector>

using namespace scratchbird::core;

// Helper to create a test UUID
static inline UuidV7Bytes makeTestUUID(uint8_t value = 0xAB) {
    UuidV7Bytes uuid;
    memset(uuid.bytes.data(), value, 16);
    return uuid;
}

class StorageEngineMGATest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        cleanup_test_files();
    }

    void TearDown() override
    {
        cleanup_test_files();
    }

    void cleanup_test_files()
    {
        std::filesystem::remove("test_mga_crosspage.db");
    }

    // Helper: Create a test table and return its ID
    ID createTestTable(Database& db, const std::string& table_name, ErrorContext* ctx = nullptr) {
        CatalogManager* catalog = db.catalog_manager();
        EXPECT_NE(catalog, nullptr);

        // Create a test schema first
        ID schema_id;
        Status status = catalog->createSchema("test_schema", "test_user", schema_id, ctx);
        // Schema may already exist from previous test, that's OK
        if (status != Status::OK && status != Status::DUPLICATE_OBJECT) {
            EXPECT_EQ(status, Status::OK) << "Failed to create schema";
        }
        if (status == Status::DUPLICATE_OBJECT) {
            // Get existing schema
            CatalogManager::SchemaInfo schema_info;
            status = catalog->getSchema("test_schema", schema_info, ctx);
            EXPECT_EQ(status, Status::OK) << "Failed to get existing schema";
            schema_id = schema_info.schema_id;
        }

        // Create table with a simple column
        std::vector<CatalogManager::ColumnInfo> columns;
        CatalogManager::ColumnInfo col;
        col.column_name = "data";
        col.data_type = 25; // TEXT type
        col.max_length = 0;
        col.nullable = true;
        columns.push_back(col);

        ID table_id;
        status = catalog->createTable(schema_id, table_name, columns, table_id, 0, ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to create table: " << table_name;

        return table_id;
    }
};

// SPRINT 0: Test cross-page UPDATE preserves TID (Firebird MGA principle)
// This test verifies the critical bug fix where cross-page UPDATEs now correctly
// use Firebird MGA (in-place modification with back versions) instead of
// PostgreSQL MVCC (append-only with forward pointers)
TEST_F(StorageEngineMGATest, CrossPageUpdatePreservesTID)
{
    // Setup: Create database with small page size to force cross-page updates
    // Using 8KB pages - a 7KB tuple will require cross-page storage
    ASSERT_EQ(Database::create("test_mga_crosspage.db", 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open("test_mga_crosspage.db"), Status::OK);

    // Create a test table in the catalog (required for StorageEngine methods)
    ErrorContext ctx;
    ID table_id = createTestTable(db, "test_crosspage", &ctx);

    StorageEngine* engine = db.storage_engine();
    ASSERT_NE(engine, nullptr);

    // Step 1: Insert tuple with SMALL data
    std::vector<uint8_t> small_data(50, 0xAA);
    uint32_t original_page_id;
    uint16_t original_item_id;

    Status status = engine->insertTuple(
        table_id,
        small_data.data(),
        small_data.size(),
        &original_page_id,
        &original_item_id,
        &ctx
    );
    ASSERT_EQ(status, Status::OK) << "Failed to insert initial small tuple: " << ctx.message;

    // Record original TID
    GPID original_gpid = makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(original_page_id));
    TID original_tid(original_gpid, original_item_id);

    // Step 2: Update with LARGE data (7KB - will trigger cross-page update)
    std::vector<uint8_t> large_data(7000, 0xBB);
    uint32_t new_page_id;
    uint16_t new_item_id;

    status = engine->updateTuple(
        table_id,
        original_page_id,
        original_item_id,
        large_data.data(),
        large_data.size(),
        &new_page_id,
        &new_item_id,
        &ctx
    );
    ASSERT_EQ(status, Status::OK) << "Failed to update with large data (cross-page): " << ctx.message;

    TID new_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(new_page_id)), new_item_id);

    // CRITICAL ASSERTION 1: TID must be UNCHANGED (MGA principle)
    EXPECT_EQ(original_page_id, new_page_id)
        << "Cross-page UPDATE changed page_id - violates MGA TID stability!";
    EXPECT_EQ(original_item_id, new_item_id)
        << "Cross-page UPDATE changed item_id - violates MGA TID stability!";
    EXPECT_EQ(original_tid, new_tid)
        << "Cross-page UPDATE changed TID - violates Firebird MGA principles!";

    // CRITICAL ASSERTION 2: Data must be correct at ORIGINAL location
    Tuple read_tuple;
    status = engine->getTuple(original_page_id, original_item_id, &read_tuple, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to read tuple at original TID: " << ctx.message;
    EXPECT_EQ(read_tuple.data_size, large_data.size())
        << "Tuple data size mismatch after cross-page UPDATE";

    // Verify data content
    EXPECT_EQ(memcmp(read_tuple.data, large_data.data(), large_data.size()), 0)
        << "Tuple data corrupted after cross-page UPDATE";

    db.close();
}

// SPRINT 0: Test that same-page UPDATE still works correctly
TEST_F(StorageEngineMGATest, SamePageUpdatePreservesTID)
{
    ASSERT_EQ(Database::create("test_mga_crosspage.db", 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open("test_mga_crosspage.db"), Status::OK);

    // Create a test table in the catalog
    ErrorContext ctx;
    ID table_id = createTestTable(db, "test_samepage", &ctx);

    StorageEngine* engine = db.storage_engine();
    ASSERT_NE(engine, nullptr);

    // Insert small tuple
    std::vector<uint8_t> small_data(50, 0xAA);
    uint32_t original_page_id;
    uint16_t original_item_id;

    Status status = engine->insertTuple(
        table_id,
        small_data.data(),
        small_data.size(),
        &original_page_id,
        &original_item_id,
        &ctx
    );
    ASSERT_EQ(status, Status::OK) << "Failed to insert: " << ctx.message;

    // Update with another small tuple (same-page update)
    std::vector<uint8_t> small_data2(60, 0xBB);
    uint32_t new_page_id;
    uint16_t new_item_id;

    status = engine->updateTuple(
        table_id,
        original_page_id,
        original_item_id,
        small_data2.data(),
        small_data2.size(),
        &new_page_id,
        &new_item_id,
        &ctx
    );
    ASSERT_EQ(status, Status::OK) << "Failed to update: " << ctx.message;

    // Same-page UPDATE should also preserve TID
    EXPECT_EQ(original_page_id, new_page_id)
        << "Same-page UPDATE changed page_id";
    EXPECT_EQ(original_item_id, new_item_id)
        << "Same-page UPDATE changed item_id";

    // Verify data is correct
    Tuple read_tuple;
    status = engine->getTuple(original_page_id, original_item_id, &read_tuple, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to read: " << ctx.message;
    EXPECT_EQ(read_tuple.data_size, small_data2.size());

    db.close();
}

// SPRINT 0: Test multiple cross-page updates create proper version chain
TEST_F(StorageEngineMGATest, MultipleUpdatesCreateBackwardChain)
{
    ASSERT_EQ(Database::create("test_mga_crosspage.db", 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open("test_mga_crosspage.db"), Status::OK);

    // Create a test table in the catalog
    ErrorContext ctx;
    ID table_id = createTestTable(db, "test_multiupdate", &ctx);

    StorageEngine* engine = db.storage_engine();
    ASSERT_NE(engine, nullptr);

    // Insert initial small tuple
    std::vector<uint8_t> data1(50, 0x11);
    uint32_t page_id;
    uint16_t item_id;

    Status status = engine->insertTuple(
        table_id, data1.data(), data1.size(), &page_id, &item_id, &ctx
    );
    ASSERT_EQ(status, Status::OK) << "Failed to insert: " << ctx.message;

    TID original_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);

    // Update 1: Large data (cross-page)
    std::vector<uint8_t> data2(7000, 0x22);
    uint32_t page_id2;
    uint16_t item_id2;

    status = engine->updateTuple(
        table_id, page_id, item_id, data2.data(), data2.size(), &page_id2, &item_id2, &ctx
    );
    ASSERT_EQ(status, Status::OK) << "Failed first update: " << ctx.message;

    // TID should be stable
    EXPECT_EQ(page_id, page_id2);
    EXPECT_EQ(item_id, item_id2);

    // Update 2: Another large data (cross-page)
    std::vector<uint8_t> data3(6500, 0x33);
    uint32_t page_id3;
    uint16_t item_id3;

    status = engine->updateTuple(
        table_id, page_id2, item_id2, data3.data(), data3.size(), &page_id3, &item_id3, &ctx
    );
    ASSERT_EQ(status, Status::OK) << "Failed second update: " << ctx.message;

    // TID should still be stable
    EXPECT_EQ(page_id, page_id3);
    EXPECT_EQ(item_id, item_id3);

    // Verify final data is correct at ORIGINAL TID
    Tuple read_tuple;
    status = engine->getTuple(page_id, item_id, &read_tuple, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to read: " << ctx.message;
    EXPECT_EQ(read_tuple.data_size, data3.size());
    EXPECT_EQ(memcmp(read_tuple.data, data3.data(), data3.size()), 0);

    db.close();
}

// SPRINT 0: Test that overwriteTuple() properly handles size changes
TEST_F(StorageEngineMGATest, OverwriteTupleHandlesSizeChanges)
{
    ASSERT_EQ(Database::create("test_mga_crosspage.db", 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open("test_mga_crosspage.db"), Status::OK);

    // Use manual page buffer to test HeapPage::overwriteTuple() directly
    uint8_t *primary_buffer = new (std::nothrow) uint8_t[8192];
    ASSERT_NE(primary_buffer, nullptr);
    memset(primary_buffer, 0, 8192);

    uint8_t *back_buffer = new (std::nothrow) uint8_t[8192];
    ASSERT_NE(back_buffer, nullptr);
    memset(back_buffer, 0, 8192);

    // Initialize both pages
    HeapPage primary_page(primary_buffer, 8192);
    Status status = primary_page.initialize(100, nullptr);
    ASSERT_EQ(status, Status::OK);

    HeapPage back_page(back_buffer, 8192);
    status = back_page.initialize(200, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Insert initial tuple on primary page
    std::vector<uint8_t> initial_data(100, 0xAA);
    uint16_t primary_item_id;
    status = primary_page.insertTuple(initial_data.data(), initial_data.size(), 100, &primary_item_id, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Test Case 1: Overwrite with SMALLER data (should fit in same space)
    {
        std::vector<uint8_t> smaller_data(50, 0xBB);
        GPID back_gpid = makeGPID(PRIMARY_TABLESPACE_ID, 200);

        status = primary_page.overwriteTuple(
            primary_item_id,
            smaller_data.data(),
            smaller_data.size(),
            100, // xmax
            101, // new_xmin
            back_gpid,
            0,   // back_version_slot
            nullptr
        );
        ASSERT_EQ(status, Status::OK) << "Failed to overwrite with smaller data";

        // Verify data
        const uint8_t *read_data;
        uint32_t read_size;
        status = primary_page.getTuple(primary_item_id, &read_data, &read_size, nullptr);
        ASSERT_EQ(status, Status::OK);

        auto *hdr = reinterpret_cast<const TupleHeader *>(read_data);
        EXPECT_EQ(hdr->xmin, 101);
        EXPECT_TRUE(hdr->hasBackVersion());
        EXPECT_EQ(hdr->back_version_gpid, back_gpid);
    }

    // Test Case 2: Overwrite with LARGER data (requires new space allocation)
    {
        std::vector<uint8_t> larger_data(200, 0xCC);
        GPID back_gpid2 = makeGPID(PRIMARY_TABLESPACE_ID, 200);

        status = primary_page.overwriteTuple(
            primary_item_id,
            larger_data.data(),
            larger_data.size(),
            101, // xmax
            102, // new_xmin
            back_gpid2,
            1,   // back_version_slot
            nullptr
        );
        ASSERT_EQ(status, Status::OK) << "Failed to overwrite with larger data";

        // Verify data
        const uint8_t *read_data;
        uint32_t read_size;
        status = primary_page.getTuple(primary_item_id, &read_data, &read_size, nullptr);
        ASSERT_EQ(status, Status::OK);

        auto *hdr = reinterpret_cast<const TupleHeader *>(read_data);
        EXPECT_EQ(hdr->xmin, 102);
        EXPECT_TRUE(hdr->hasBackVersion());
        EXPECT_EQ(hdr->back_version_slot, 1);
    }

    delete[] primary_buffer;
    delete[] back_buffer;
    db.close();
}
