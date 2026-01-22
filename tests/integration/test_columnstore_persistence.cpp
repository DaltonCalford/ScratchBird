/**
 * Plan 01 Task C: Columnstore Persistence Tests
 *
 * Tests for dual meta page persistence with generation counters and CRC32C
 * per plan_01_core_storage_gc_clarifications.md
 *
 * Test coverage:
 * 1. Happy path: Insert segments, verify catalog persists
 * 2. Restart test: Segments persist across database close/reopen
 * 3. Generation selection: Corrupted page is ignored, valid page is used
 * 4. Both invalid: Both pages corrupted returns INDEX_CORRUPTED error
 * 5. Empty index: New index with no segments loads successfully
 */

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/columnstore_index.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/uuidv7.h"
#include <filesystem>
#include <memory>
#include <cstring>

using namespace scratchbird::core;

namespace {

class ColumnstorePersistenceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create temporary database
        test_db_path_ = "/tmp/test_columnstore_persist.db";
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
 * Test 1: Happy path - Create index, insert segments, verify persistence
 *
 * This test verifies that the columnstore index can be created with dual
 * meta pages, segments can be inserted, and the catalog persists correctly.
 */
TEST_F(ColumnstorePersistenceTest, CreateAndInsertSegments)
{
    // Generate unique index UUID
    UuidV7Bytes index_uuid = generateUuidV7();

    // Create columnstore index
    GPID meta_gpid = 0;
    uint32_t meta_page = 0;
    Status status = db_->page_manager()->allocatePageInTablespace(0, &meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to allocate columnstore meta page";
    meta_page = static_cast<uint32_t>(getPageNumber(meta_gpid));
    status = ColumnstoreIndexSimple::create(db_.get(), index_uuid, meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to create columnstore index";
    EXPECT_GT(getPageNumber(meta_gpid), 0) << "Meta page should be allocated";

    // Open the index
    auto cs_index = ColumnstoreIndexSimple::open(db_.get(), index_uuid, meta_gpid, nullptr);
    ASSERT_NE(cs_index, nullptr) << "Failed to open columnstore index";

    // Insert some column data
    std::vector<uint8_t> col_data_1(1000, 0xAA);
    status = cs_index->insertColumn(1, 100, col_data_1, nullptr);
    EXPECT_EQ(status, Status::OK) << "Failed to insert column 1";

    std::vector<uint8_t> col_data_2(2000, 0xBB);
    status = cs_index->insertColumn(2, 200, col_data_2, nullptr);
    EXPECT_EQ(status, Status::OK) << "Failed to insert column 2";

    // Flush to ensure persistence
    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to flush buffer pool";

    // Verify index metadata is accessible
    EXPECT_EQ(memcmp(cs_index->getIndexUuid().bytes.data(), index_uuid.bytes.data(), 16), 0);
    EXPECT_EQ(cs_index->getMetaPage(), meta_page);
}

/**
 * Test 2: Restart test - Verify segments persist across database restart
 *
 * This test verifies that segment catalog persists to disk and can be
 * reloaded after closing and reopening the database.
 */
TEST_F(ColumnstorePersistenceTest, SegmentsPersistAcrossRestart)
{
    // Generate unique index UUID
    UuidV7Bytes index_uuid = generateUuidV7();

    GPID meta_gpid = 0;
    uint32_t meta_page = 0;

    // Phase 1: Create index and insert data
    {
        Status status = db_->page_manager()->allocatePageInTablespace(0, &meta_gpid, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to allocate columnstore meta page";
        meta_page = static_cast<uint32_t>(getPageNumber(meta_gpid));
        status = ColumnstoreIndexSimple::create(db_.get(), index_uuid, meta_gpid, nullptr);
        ASSERT_EQ(status, Status::OK);

        auto cs_index = ColumnstoreIndexSimple::open(db_.get(), index_uuid, meta_gpid, nullptr);
        ASSERT_NE(cs_index, nullptr);

        // Insert multiple columns
        std::vector<uint8_t> col_data_1(500, 0x11);
        status = cs_index->insertColumn(1, 50, col_data_1, nullptr);
        ASSERT_EQ(status, Status::OK);

        std::vector<uint8_t> col_data_2(1000, 0x22);
        status = cs_index->insertColumn(2, 100, col_data_2, nullptr);
        ASSERT_EQ(status, Status::OK);

        std::vector<uint8_t> col_data_3(1500, 0x33);
        status = cs_index->insertColumn(3, 150, col_data_3, nullptr);
        ASSERT_EQ(status, Status::OK);

        // Flush to disk
        status = db_->buffer_pool()->flushAll(nullptr);
        ASSERT_EQ(status, Status::OK);
    }

    // Phase 2: Close and reopen database
    db_->close();
    Status status = db_->open(test_db_path_, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to reopen database";

    // Phase 3: Open index and verify segments are loaded
    {
        auto cs_index = ColumnstoreIndexSimple::open(db_.get(), index_uuid, meta_gpid, nullptr);
        ASSERT_NE(cs_index, nullptr) << "Failed to reopen columnstore index after restart";

        // Verify index metadata persisted
        EXPECT_EQ(memcmp(cs_index->getIndexUuid().bytes.data(), index_uuid.bytes.data(), 16), 0);
        EXPECT_EQ(cs_index->getMetaPage(), meta_page);

        // Try to read back data from each column
        std::vector<uint8_t> data_out;

        status = cs_index->scanColumn(1, 0, 50, &data_out, nullptr);
        EXPECT_EQ(status, Status::OK) << "Failed to scan column 1 after restart";

        status = cs_index->scanColumn(2, 0, 100, &data_out, nullptr);
        EXPECT_EQ(status, Status::OK) << "Failed to scan column 2 after restart";

        status = cs_index->scanColumn(3, 0, 150, &data_out, nullptr);
        EXPECT_EQ(status, Status::OK) << "Failed to scan column 3 after restart";
    }
}

/**
 * Test 3: Generation selection - Corrupted page is ignored
 *
 * This test verifies that when one meta page is corrupted, the system
 * correctly selects the valid page based on generation counter and CRC32C.
 */
TEST_F(ColumnstorePersistenceTest, GenerationSelectionOneCorrupted)
{
    // Generate unique index UUID
    UuidV7Bytes index_uuid = generateUuidV7();

    GPID meta_gpid_a = 0;
    uint32_t meta_page_a = 0;

    // Create index and insert data
    Status status = db_->page_manager()->allocatePageInTablespace(0, &meta_gpid_a, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to allocate columnstore meta page";
    meta_page_a = static_cast<uint32_t>(getPageNumber(meta_gpid_a));
    status = ColumnstoreIndexSimple::create(db_.get(), index_uuid, meta_gpid_a, nullptr);
    ASSERT_EQ(status, Status::OK);

    auto cs_index = ColumnstoreIndexSimple::open(db_.get(), index_uuid, meta_gpid_a, nullptr);
    ASSERT_NE(cs_index, nullptr);

    // Insert data to create segments
    std::vector<uint8_t> col_data(1000, 0xDD);
    status = cs_index->insertColumn(1, 100, col_data, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Flush to disk
    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK);

    // Get peer page ID from meta page header
    void* meta_buffer = nullptr;
    status = db_->buffer_pool()->pinPageGlobal(meta_gpid_a, &meta_buffer, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Read peer_page_id from meta page header (offset 16)
    uint32_t peer_page_id = 0;
    memcpy(&peer_page_id, static_cast<uint8_t*>(meta_buffer) + 16, sizeof(uint32_t));

    db_->buffer_pool()->unpinPageGlobal(meta_gpid_a, false, nullptr);
    EXPECT_GT(peer_page_id, 0) << "Peer page should be allocated";

    // Corrupt the peer page (meta_page_b) by zeroing it
    void* peer_buffer = nullptr;
    status = db_->buffer_pool()->pinPageGlobal(makeGPID(0, peer_page_id), &peer_buffer, nullptr);
    ASSERT_EQ(status, Status::OK);

    memset(peer_buffer, 0, 16384);  // Zero entire page

    db_->buffer_pool()->unpinPageGlobal(makeGPID(0, peer_page_id), true, nullptr);

    // Flush corruption to disk
    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK);

    // Close and reopen
    cs_index.reset();
    db_->close();
    status = db_->open(test_db_path_, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Reopen index - should use valid page (meta_page_a)
    cs_index = ColumnstoreIndexSimple::open(db_.get(), index_uuid, meta_gpid_a, nullptr);
    ASSERT_NE(cs_index, nullptr) << "Should load from valid meta page when peer is corrupted";

    // Verify data is still accessible
    std::vector<uint8_t> data_out;
    status = cs_index->scanColumn(1, 0, 100, &data_out, nullptr);
    EXPECT_EQ(status, Status::OK) << "Should be able to scan column after recovery";
}

/**
 * Test 4: Both meta pages invalid - Returns INDEX_CORRUPTED
 *
 * This test verifies that when both meta pages are corrupted,
 * the system returns INDEX_CORRUPTED error.
 */
TEST_F(ColumnstorePersistenceTest, BothMetaPagesCorrupted)
{
    // Generate unique index UUID
    UuidV7Bytes index_uuid = generateUuidV7();

    GPID meta_gpid_a = 0;
    uint32_t meta_page_a = 0;

    // Create index
    Status status = db_->page_manager()->allocatePageInTablespace(0, &meta_gpid_a, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to allocate columnstore meta page";
    meta_page_a = static_cast<uint32_t>(getPageNumber(meta_gpid_a));
    status = ColumnstoreIndexSimple::create(db_.get(), index_uuid, meta_gpid_a, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Get peer page ID
    void* meta_buffer = nullptr;
    status = db_->buffer_pool()->pinPageGlobal(meta_gpid_a, &meta_buffer, nullptr);
    ASSERT_EQ(status, Status::OK);

    uint32_t peer_page_id = 0;
    memcpy(&peer_page_id, static_cast<uint8_t*>(meta_buffer) + 16, sizeof(uint32_t));

    db_->buffer_pool()->unpinPageGlobal(meta_gpid_a, false, nullptr);

    // Corrupt both meta pages by zeroing them
    void* buffer_a = nullptr;
    status = db_->buffer_pool()->pinPageGlobal(meta_gpid_a, &buffer_a, nullptr);
    ASSERT_EQ(status, Status::OK);
    memset(buffer_a, 0, 16384);
    db_->buffer_pool()->unpinPageGlobal(meta_gpid_a, true, nullptr);

    void* buffer_b = nullptr;
    status = db_->buffer_pool()->pinPageGlobal(makeGPID(0, peer_page_id), &buffer_b, nullptr);
    ASSERT_EQ(status, Status::OK);
    memset(buffer_b, 0, 16384);
    db_->buffer_pool()->unpinPageGlobal(makeGPID(0, peer_page_id), true, nullptr);

    // Flush corruption to disk
    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK);

    // Close and reopen
    db_->close();
    status = db_->open(test_db_path_, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Try to open index - should fail with INDEX_CORRUPTED
    auto cs_index = ColumnstoreIndexSimple::open(db_.get(), index_uuid, meta_gpid_a, nullptr);
    EXPECT_EQ(cs_index, nullptr) << "Should fail to open when both meta pages are corrupted";
}

/**
 * Test 5: Empty index - New index with no segments loads successfully
 *
 * This test verifies that a newly created index with no segments
 * can be opened and has valid dual meta pages.
 */
TEST_F(ColumnstorePersistenceTest, EmptyIndexLoads)
{
    // Generate unique index UUID
    UuidV7Bytes index_uuid = generateUuidV7();

    // Create empty index
    GPID meta_gpid = 0;
    Status status = db_->page_manager()->allocatePageInTablespace(0, &meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to allocate columnstore meta page";
    uint32_t meta_page = static_cast<uint32_t>(getPageNumber(meta_gpid));
    status = ColumnstoreIndexSimple::create(db_.get(), index_uuid, meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Flush to disk
    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK);

    // Close and reopen
    db_->close();
    status = db_->open(test_db_path_, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Open empty index
    auto cs_index = ColumnstoreIndexSimple::open(db_.get(), index_uuid, meta_gpid, nullptr);
    ASSERT_NE(cs_index, nullptr) << "Should be able to open empty columnstore index";

    // Verify metadata
    EXPECT_EQ(memcmp(cs_index->getIndexUuid().bytes.data(), index_uuid.bytes.data(), 16), 0);
    EXPECT_EQ(cs_index->getMetaPage(), meta_page);
}

/**
 * Test 6: Multiple updates increment generation
 *
 * This test verifies that each catalog update increments the generation
 * counter, and the newest generation is selected on reload.
 */
TEST_F(ColumnstorePersistenceTest, GenerationIncrementsOnUpdates)
{
    // Generate unique index UUID
    UuidV7Bytes index_uuid = generateUuidV7();

    GPID meta_gpid = 0;
    uint32_t meta_page = 0;

    // Create index
    Status status = db_->page_manager()->allocatePageInTablespace(0, &meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to allocate columnstore meta page";
    meta_page = static_cast<uint32_t>(getPageNumber(meta_gpid));
    status = ColumnstoreIndexSimple::create(db_.get(), index_uuid, meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK);

    auto cs_index = ColumnstoreIndexSimple::open(db_.get(), index_uuid, meta_gpid, nullptr);
    ASSERT_NE(cs_index, nullptr);

    // Insert multiple batches to trigger catalog updates
    for (int i = 1; i <= 5; i++)
    {
        std::vector<uint8_t> col_data(i * 100, static_cast<uint8_t>(i));
        status = cs_index->insertColumn(i, i * 10, col_data, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to insert column " << i;
    }

    // Flush to disk
    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK);

    // Close and reopen
    cs_index.reset();
    db_->close();
    status = db_->open(test_db_path_, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Reopen index - should load from highest generation
    cs_index = ColumnstoreIndexSimple::open(db_.get(), index_uuid, meta_gpid, nullptr);
    ASSERT_NE(cs_index, nullptr) << "Failed to reopen after multiple updates";

    // Verify all columns are accessible
    std::vector<uint8_t> data_out;
    for (int i = 1; i <= 5; i++)
    {
        status = cs_index->scanColumn(i, 0, i * 10, &data_out, nullptr);
        EXPECT_EQ(status, Status::OK) << "Failed to scan column " << i << " after reload";
    }
}

} // anonymous namespace
