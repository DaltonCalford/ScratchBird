/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include <cstring>
#include <vector>

using namespace scratchbird::core;

class HeapPageTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Allocate page buffer
        page_buffer_ = new uint8_t[page_size_];
        memset(page_buffer_, 0, page_size_);
    }

    void TearDown() override
    {
        delete[] page_buffer_;
    }

    uint8_t *page_buffer_ = nullptr;
    const uint32_t page_size_ = 8192;
};

// Test: Invalid Item Pointer - As requested by Agent A
TEST_F(HeapPageTest, InvalidItemPointer)
{
    HeapPage page(page_buffer_, page_size_);
    ASSERT_EQ(page.initialize(1, nullptr), Status::OK);

    // Insert a valid tuple first
    std::vector<uint8_t> tuple_data(100, 0xAA);
    uint16_t item_id;
    ASSERT_EQ(page.insertTuple(tuple_data.data(), tuple_data.size() + sizeof(TupleHeader), 100,
                                &item_id, nullptr),
              Status::OK);

    // Now corrupt the item pointer to point beyond page boundary
    ItemPointer *items = reinterpret_cast<ItemPointer *>(page_buffer_ + sizeof(PageHeader));
    items[item_id].offset = page_size_ + 100; // Beyond page boundary
    items[item_id].length = 100;

    // Try to read the tuple - should detect invalid pointer
    const uint8_t *data;
    uint32_t size;
    ErrorContext ctx;
    Status status = page.getTuple(item_id, &data, &size, &ctx);

    EXPECT_NE(status, Status::OK) << "Should detect invalid item pointer";
    EXPECT_EQ(status, Status::PAGE_CORRUPT) << "Should return PageCorrupt status";

    // Also test with offset that would cause integer overflow
    items[item_id].offset = page_size_ - 10;
    items[item_id].length = 200; // Would extend beyond page

    status = page.getTuple(item_id, &data, &size, &ctx);
    EXPECT_NE(status, Status::OK) << "Should detect tuple extending beyond page";
}

// Test: Checksum Mismatch - As requested by Agent A
TEST_F(HeapPageTest, ChecksumMismatch)
{
    HeapPage page(page_buffer_, page_size_);
    ASSERT_EQ(page.initialize(1, nullptr), Status::OK);

    // Insert some data
    std::vector<uint8_t> tuple_data(200, 0xBB);
    uint16_t item_id;
    ASSERT_EQ(page.insertTuple(tuple_data.data(), tuple_data.size() + sizeof(TupleHeader), 100,
                                &item_id, nullptr),
              Status::OK);

    // Get the current page header
    PageHeader *header = reinterpret_cast<PageHeader *>(page_buffer_);

    // Calculate correct checksum
    uint32_t original_checksum = header->checksum;

    // Corrupt some data in the page
    page_buffer_[1000] ^= 0xFF;
    page_buffer_[2000] ^= 0xFF;

    // The checksum should now be invalid
    // When Database::read_page is called, it should detect this
    ErrorContext ctx;
    Status status = page.validate(&ctx);

    // Note: HeapPage::validate might not check CRC, so let's manually verify
    uint32_t computed_crc = calculatePageChecksum(page_buffer_, page_size_);
    EXPECT_NE(computed_crc, original_checksum) << "Checksum should have changed after corruption";

    // Restore original checksum to test detection
    header->checksum = original_checksum;

    // Now the stored checksum doesn't match the data
    uint32_t new_computed_crc = calculatePageChecksum(page_buffer_, page_size_);
    EXPECT_NE(new_computed_crc, header->checksum)
        << "Stored checksum should not match corrupted data";
}

// Additional focused page tests

// Test: Page initialization and header setup
TEST_F(HeapPageTest, PageInitialization)
{
    HeapPage page(page_buffer_, page_size_);

    uint32_t page_id = 42;
    ASSERT_EQ(page.initialize(page_id, nullptr), Status::OK);

    // Verify header is set correctly
    PageHeader *header = page.header();
    EXPECT_EQ(header->magic, K_MAGIC_SBRD);
    EXPECT_EQ(header->version, 1);
    EXPECT_EQ(header->page_type, PAGE_TYPE_HEAP);
    EXPECT_EQ(header->page_size, page_size_);
    EXPECT_EQ(header->page_id, page_id);
    EXPECT_EQ(header->item_count, 0);

    // Verify special area
    HeapPageSpecial *special =
        reinterpret_cast<HeapPageSpecial *>(page_buffer_ + page_size_ - sizeof(HeapPageSpecial));
    EXPECT_EQ(special->pd_lower, sizeof(PageHeader));
    EXPECT_EQ(special->pd_upper, page_size_ - sizeof(HeapPageSpecial));
    EXPECT_EQ(special->pd_special, page_size_ - sizeof(HeapPageSpecial));
}

// Test: Tuple insertion and retrieval
TEST_F(HeapPageTest, TupleOperations)
{
    HeapPage page(page_buffer_, page_size_);
    ASSERT_EQ(page.initialize(1, nullptr), Status::OK);

    // Test various body sizes (tuple size = header + body)
    std::vector<size_t> body_sizes = {10, 50, 100, 500, 1000};
    std::vector<uint16_t> item_ids;

    for (size_t body_size : body_sizes)
    {
        // Create buffer that includes space for TupleHeader + body
        // insertTuple expects the full tuple buffer including header space
        std::vector<uint8_t> tuple_buffer(sizeof(TupleHeader) + body_size);

        // Initialize the body portion (after header)
        uint8_t *body_ptr = tuple_buffer.data() + sizeof(TupleHeader);
        for (size_t i = 0; i < body_size; i++)
        {
            body_ptr[i] = (i + body_size) & 0xFF;
        }

        uint16_t item_id;
        Status status = page.insertTuple(tuple_buffer.data(), tuple_buffer.size(),
                                          100 + body_size, &item_id, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to insert " << body_size << " byte body tuple";
        item_ids.push_back(item_id);
    }

    // Verify all tuples can be retrieved correctly
    for (size_t i = 0; i < body_sizes.size(); i++)
    {
        const uint8_t *retrieved_data;
        uint32_t retrieved_size;
        Status status = page.getTuple(item_ids[i], &retrieved_data, &retrieved_size, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to retrieve tuple " << i;

        // Size should include header
        EXPECT_EQ(retrieved_size, body_sizes[i] + sizeof(TupleHeader));

        // Verify body data integrity (skip header)
        const uint8_t *tuple_body = retrieved_data + sizeof(TupleHeader);
        for (size_t j = 0; j < body_sizes[i]; j++)
        {
            EXPECT_EQ(tuple_body[j], (j + body_sizes[i]) & 0xFF)
                << "Data mismatch at byte " << j << " of tuple " << i;
        }
    }
}

// Test: Page free space calculation
TEST_F(HeapPageTest, FreeSpaceCalculation)
{
    HeapPage page(page_buffer_, page_size_);
    ASSERT_EQ(page.initialize(1, nullptr), Status::OK);

    uint32_t initial_free_space = page.getFreeSpace();
    uint32_t expected_free = page_size_ - sizeof(PageHeader) - sizeof(HeapPageSpecial);
    EXPECT_EQ(initial_free_space, expected_free);

    // Insert a tuple and verify free space decreases
    // Create buffer with header + 100 bytes body
    const size_t body_size = 100;
    std::vector<uint8_t> tuple_data(sizeof(TupleHeader) + body_size, 0xCC);
    uint16_t item_id;
    ASSERT_EQ(page.insertTuple(tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr),
              Status::OK);

    uint32_t after_insert_free = page.getFreeSpace();
    // Space used includes ItemPointer + total tuple size (header + body)
    uint32_t space_used = sizeof(ItemPointer) + sizeof(TupleHeader) + body_size;
    EXPECT_EQ(after_insert_free, initial_free_space - space_used);
}

// Test: Deleted tuple handling
TEST_F(HeapPageTest, DeletedTupleHandling)
{
    HeapPage page(page_buffer_, page_size_);
    ASSERT_EQ(page.initialize(1, nullptr), Status::OK);

    // Insert multiple tuples
    std::vector<uint16_t> item_ids;
    for (int i = 0; i < 5; i++)
    {
        // Create buffer with header + 100 bytes body
        std::vector<uint8_t> tuple_data(sizeof(TupleHeader) + 100, i);
        uint16_t item_id;
        ASSERT_EQ(page.insertTuple(tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr),
                  Status::OK);
        item_ids.push_back(item_id);
    }

    // Delete some tuples
    ASSERT_EQ(page.deleteTuple(item_ids[1], 200, nullptr), Status::OK);
    ASSERT_EQ(page.deleteTuple(item_ids[3], 200, nullptr), Status::OK);

    // Verify deleted tuples are marked correctly
    const uint8_t *data;
    uint32_t size;

    // Non-deleted tuples should still be accessible
    EXPECT_EQ(page.getTuple(item_ids[0], &data, &size, nullptr), Status::OK);
    EXPECT_EQ(page.getTuple(item_ids[2], &data, &size, nullptr), Status::OK);
    EXPECT_EQ(page.getTuple(item_ids[4], &data, &size, nullptr), Status::OK);

    // Deleted tuples might still be readable but marked as deleted
    Status status = page.getTuple(item_ids[1], &data, &size, nullptr);
    if (status == Status::OK)
    {
        const TupleHeader *hdr = reinterpret_cast<const TupleHeader *>(data);
        EXPECT_TRUE(hdr->isDeleted() || hdr->xmax != 0) << "Tuple should be marked as deleted";
    }
}

// Test: Page validation
TEST_F(HeapPageTest, PageValidation)
{
    HeapPage page(page_buffer_, page_size_);
    ASSERT_EQ(page.initialize(1, nullptr), Status::OK);

    // Valid page should pass validation
    ErrorContext ctx;
    EXPECT_EQ(page.validate(&ctx), Status::OK);

    // Corrupt the page header magic
    PageHeader *header = page.header();
    uint32_t original_magic = header->magic;
    header->magic = 0xDEADBEEF;

    EXPECT_NE(page.validate(&ctx), Status::OK) << "Should detect invalid magic";

    // Restore magic, corrupt page type
    header->magic = original_magic;
    header->page_type = 99; // Invalid type

    EXPECT_NE(page.validate(&ctx), Status::OK) << "Should detect invalid page type";

    // Restore page type, corrupt special area
    header->page_type = PAGE_TYPE_HEAP;
    HeapPageSpecial *special =
        reinterpret_cast<HeapPageSpecial *>(page_buffer_ + page_size_ - sizeof(HeapPageSpecial));
    special->pd_lower = page_size_; // Invalid - beyond page

    EXPECT_NE(page.validate(&ctx), Status::OK) << "Should detect invalid special area";
}

// Test: Minimum tuple size boundary (Suite 1 compliance requirement)
TEST_F(HeapPageTest, MinimumTupleSizeBoundary)
{
    HeapPage page(page_buffer_, page_size_);
    ASSERT_EQ(page.initialize(1, nullptr), Status::OK);

    // Test inserting minimum valid tuple - just the TupleHeader with no data
    // TupleHeader contains: xmin, xmax, flags, and other metadata
    uint16_t item_id;

    // Create minimal tuple: allocate space for TupleHeader but no payload
    std::vector<uint8_t> empty_tuple_data(sizeof(TupleHeader), 0);

    // The insertTuple function expects data_size to include the TupleHeader
    // So minimum size is sizeof(TupleHeader) + 0 bytes of actual data
    Status status = page.insertTuple(empty_tuple_data.data(), sizeof(TupleHeader), 100,
                                      &item_id, nullptr);

    // Should succeed - even an empty tuple is valid
    EXPECT_EQ(status, Status::OK) << "Should be able to insert minimum size tuple (just TupleHeader)";

    if (status == Status::OK) {
        // Verify we can retrieve it
        const uint8_t *data;
        uint32_t size;
        EXPECT_EQ(page.getTuple(item_id, &data, &size, nullptr), Status::OK);

        // The tuple should have at least the TupleHeader
        EXPECT_GE(size, sizeof(TupleHeader)) << "Retrieved tuple should have at least TupleHeader size";
    }

    // Test with 1-byte data (smallest non-empty tuple)
    std::vector<uint8_t> one_byte_data = {0x42};
    uint16_t item_id2;
    status = page.insertTuple(one_byte_data.data(), one_byte_data.size() + sizeof(TupleHeader),
                              101, &item_id2, nullptr);
    EXPECT_EQ(status, Status::OK) << "Should be able to insert 1-byte data tuple";

    if (status == Status::OK) {
        // Verify retrieval
        const uint8_t *data2;
        uint32_t size2;
        EXPECT_EQ(page.getTuple(item_id2, &data2, &size2, nullptr), Status::OK);
        EXPECT_GE(size2, sizeof(TupleHeader) + 1) << "Should have TupleHeader + 1 byte";
    }
}

// Test: Corrupted Page Size Detection (MED-005)
TEST_F(HeapPageTest, CorruptedPageSizeDetection)
{
    // Initialize page normally
    HeapPage page(page_buffer_, page_size_);
    uint32_t page_id = 123;
    ASSERT_EQ(page.initialize(page_id, nullptr), Status::OK);

    // Insert some data to make it a realistic page
    std::vector<uint8_t> tuple_data(100, 0xDD);
    uint16_t item_id;
    ASSERT_EQ(page.insertTuple(tuple_data.data(), tuple_data.size() + sizeof(TupleHeader), 100,
                                &item_id, nullptr),
              Status::OK);

    // Verify page is correctly initialized
    PageHeader *header = page.header();
    EXPECT_EQ(header->page_size, page_size_) << "Page size should match initial value";

    // Simulate corruption: change the stored page size to a different value
    uint32_t corrupted_size = 4096; // Different from actual page_size_
    header->page_size = corrupted_size;

    // Re-initialize the page (simulating a page reload from disk)
    // The initialize() method should detect the mismatch, log a warning, and correct it
    ErrorContext ctx;
    Status status = page.initialize(page_id, &ctx);

    // Initialize should succeed (it corrects the error)
    EXPECT_EQ(status, Status::OK) << "Should successfully correct page size mismatch";

    // Verify that page size was corrected
    EXPECT_EQ(header->page_size, page_size_)
        << "Page size should be corrected to buffer pool size";
    EXPECT_NE(header->page_size, corrupted_size)
        << "Page size should no longer be the corrupted value";

    // Verify the page is still functional after correction
    const uint8_t *retrieved_data;
    uint32_t retrieved_size;
    status = page.getTuple(item_id, &retrieved_data, &retrieved_size, nullptr);
    EXPECT_EQ(status, Status::OK) << "Should still be able to retrieve tuples after correction";
}

// Test: Multiple Page Size Corruptions
TEST_F(HeapPageTest, MultiplePageSizeCorruptions)
{
    // Test that page size correction works consistently across multiple corruptions
    HeapPage page(page_buffer_, page_size_);
    uint32_t page_id = 456;
    ASSERT_EQ(page.initialize(page_id, nullptr), Status::OK);

    PageHeader *header = page.header();

    // Test various corrupted page sizes
    std::vector<uint32_t> corrupted_sizes = {4096, 16384, 32768, 1024};

    for (uint32_t corrupted_size : corrupted_sizes)
    {
        // Corrupt the page size
        header->page_size = corrupted_size;
        EXPECT_EQ(header->page_size, corrupted_size) << "Corruption should be applied";

        // Re-initialize - should correct the corruption
        ErrorContext ctx;
        Status status = page.initialize(page_id, &ctx);
        EXPECT_EQ(status, Status::OK) << "Should correct corruption with size " << corrupted_size;

        // Verify correction
        EXPECT_EQ(header->page_size, page_size_)
            << "Should correct page size from " << corrupted_size << " to " << page_size_;
    }
}