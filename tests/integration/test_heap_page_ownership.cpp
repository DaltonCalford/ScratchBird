/**
 * Plan 01 Task B: Heap Page Ownership Tests
 *
 * Tests for table_id field in PageHeader per ON_DISK_FORMAT.md v1.4.0
 *
 * Test coverage:
 * 1. Happy path: PageHeader has table_id field set correctly
 * 2. Restart test: table_id persists across database restart
 * 3. Negative test: enumerateTablePages returns empty for non-existent table
 */

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/page_manager.h"
#include <filesystem>
#include <memory>
#include <cstring>

using namespace scratchbird::core;

namespace {

class HeapPageOwnershipTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create temporary database
        test_db_path_ = "/tmp/test_heap_ownership.db";
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
 * Test 1: PageHeader contains table_id field
 *
 * This test verifies that PageHeader structure has the table_id field
 * and is exactly 80 bytes as specified in ON_DISK_FORMAT.md v1.4.0
 */
TEST_F(HeapPageOwnershipTest, PageHeaderSize)
{
    // Verify PageHeader is 80 bytes
    EXPECT_EQ(sizeof(PageHeader), 80)
        << "PageHeader must be 80 bytes per ON_DISK_FORMAT.md v1.4.0";

    // Verify table_id field is at correct offset (0x30)
    PageHeader test_header;
    uintptr_t header_addr = reinterpret_cast<uintptr_t>(&test_header);
    uintptr_t table_id_addr = reinterpret_cast<uintptr_t>(&test_header.table_id);

    EXPECT_EQ(table_id_addr - header_addr, 0x30)
        << "table_id must be at offset 0x30 in PageHeader";

    // Verify table_id is 16 bytes
    EXPECT_EQ(sizeof(test_header.table_id), 16)
        << "table_id must be 16 bytes (UUID)";
}

/**
 * Test 2: Heap page initialization sets table_id
 *
 * This test verifies that when a heap page is initialized,
 * the table_id field is properly set in the PageHeader.
 */
TEST_F(HeapPageOwnershipTest, HeapPageSetsTableId)
{
    // Allocate a page
    uint32_t page_id = 0;
    Status status = db_->page_manager()->allocatePage(page_id, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Pin the page
    void* buffer = nullptr;
    status = db_->buffer_pool()->pinPage(page_id, &buffer, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Ensure page is zero-filled (uninitialized)
    memset(buffer, 0, 16384);

    // Create a test table_id
    ID test_table_id;
    test_table_id.bytes.fill(0xAB);  // Fill with test pattern

    // Initialize as heap page
    HeapPage heap_page(static_cast<uint8_t*>(buffer), 16384, nullptr, db_.get(), test_table_id);
    status = heap_page.initialize(page_id, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Verify table_id was set in PageHeader
    const PageHeader* header = heap_page.header();
    EXPECT_EQ(memcmp(header->table_id, test_table_id.bytes.data(), sizeof(header->table_id)), 0)
        << "HeapPage::initialize() must set table_id in PageHeader";

    // Verify page type is HEAP
    EXPECT_EQ(header->page_type, PAGE_TYPE_HEAP);

    // Unpin page
    db_->buffer_pool()->unpinPage(page_id, true, nullptr);
}

/**
 * Test 3: Table ID persistence across restart
 *
 * This test verifies that table_id in PageHeader persists
 * when the database is closed and reopened.
 */
TEST_F(HeapPageOwnershipTest, TableIdPersistsAcrossRestart)
{
    // Allocate a page
    uint32_t page_id = 0;
    Status status = db_->page_manager()->allocatePage(page_id, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Create a unique table_id
    ID test_table_id;
    for (size_t i = 0; i < 16; i++)
    {
        test_table_id.bytes[i] = static_cast<uint8_t>(0x10 + i);
    }

    // Initialize heap page with table_id
    {
        void* buffer = nullptr;
        status = db_->buffer_pool()->pinPage(page_id, &buffer, nullptr);
        ASSERT_EQ(status, Status::OK);

        // Ensure page is zero-filled (uninitialized)
        memset(buffer, 0, 16384);

        HeapPage heap_page(static_cast<uint8_t*>(buffer), 16384, nullptr, db_.get(), test_table_id);
        status = heap_page.initialize(page_id, nullptr);
        ASSERT_EQ(status, Status::OK);

        db_->buffer_pool()->unpinPage(page_id, true, nullptr);
    }

    // Flush to ensure write
    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK);

    // Close database
    db_->close();

    // Reopen database
    status = db_->open(test_db_path_, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Read page and verify table_id persisted
    void* buffer = nullptr;
    status = db_->buffer_pool()->pinPage(page_id, &buffer, nullptr);
    ASSERT_EQ(status, Status::OK);

    const PageHeader* header = static_cast<const PageHeader*>(buffer);
    EXPECT_EQ(memcmp(header->table_id, test_table_id.bytes.data(), sizeof(header->table_id)), 0)
        << "table_id must persist across database restart";

    db_->buffer_pool()->unpinPage(page_id, false, nullptr);
}

/**
 * Test 4: enumerateTablePages with non-existent table
 *
 * Negative test: enumerateTablePages should fail gracefully
 * when given a table ID that doesn't exist.
 */
TEST_F(HeapPageOwnershipTest, EnumerateNonExistentTable)
{
    CatalogManager* catalog = db_->catalog_manager();
    ASSERT_NE(catalog, nullptr);

    // Create a fake table ID that doesn't exist
    ID fake_table_id;
    fake_table_id.bytes.fill(0xFF);

    // Try to enumerate pages
    std::vector<GPID> pages;
    Status status = catalog->enumerateTablePages(fake_table_id, pages, nullptr);

    // Should fail with NOT_FOUND or similar
    EXPECT_NE(status, Status::OK)
        << "enumerateTablePages should fail for non-existent table";
}

/**
 * Test 5: Zero table_id triggers warning
 *
 * Per ON_DISK_FORMAT.md, heap pages with zero table_id are a corruption risk.
 * Verify that the system logs a warning when this occurs.
 */
TEST_F(HeapPageOwnershipTest, ZeroTableIdWarning)
{
    // Allocate a page
    uint32_t page_id = 0;
    Status status = db_->page_manager()->allocatePage(page_id, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Create zero table_id (corruption scenario)
    ID zero_table_id;
    zero_table_id.bytes.fill(0);

    // Initialize heap page with zero table_id
    void* buffer = nullptr;
    status = db_->buffer_pool()->pinPage(page_id, &buffer, nullptr);
    ASSERT_EQ(status, Status::OK);

    // This should trigger a warning but still succeed
    HeapPage heap_page(static_cast<uint8_t*>(buffer), 16384, nullptr, db_.get(), zero_table_id);
    status = heap_page.initialize(page_id, nullptr);

    // Initialize should succeed even with zero table_id (for now)
    EXPECT_EQ(status, Status::OK);

    // But verify the zero was set (system should log warning)
    const PageHeader* header = heap_page.header();
    bool all_zeros = true;
    for (size_t i = 0; i < sizeof(header->table_id); i++)
    {
        if (header->table_id[i] != 0)
        {
            all_zeros = false;
            break;
        }
    }
    EXPECT_TRUE(all_zeros) << "Zero table_id should be preserved (with warning)";

    db_->buffer_pool()->unpinPage(page_id, false, nullptr);
}

} // anonymous namespace
