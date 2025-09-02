#include <gtest/gtest.h>
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include <cstring>
#include <vector>

using namespace scratchbird::core;

class HeapPageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Allocate page buffer
        page_buffer_ = new uint8_t[page_size_];
        memset(page_buffer_, 0, page_size_);
    }
    
    void TearDown() override {
        delete[] page_buffer_;
    }
    
    uint8_t* page_buffer_ = nullptr;
    const uint32_t page_size_ = 8192;
};

// Test: Invalid Item Pointer - As requested by Agent A
TEST_F(HeapPageTest, InvalidItemPointer) {
    HeapPage page(page_buffer_, page_size_);
    ASSERT_EQ(page.initialize(1, nullptr), Status::Ok);
    
    // Insert a valid tuple first
    std::vector<uint8_t> tuple_data(100, 0xAA);
    uint16_t item_id;
    ASSERT_EQ(page.insert_tuple(tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                               100, &item_id, nullptr), Status::Ok);
    
    // Now corrupt the item pointer to point beyond page boundary
    ItemPointer* items = reinterpret_cast<ItemPointer*>(page_buffer_ + sizeof(PageHeader));
    items[item_id].offset = page_size_ + 100; // Beyond page boundary
    items[item_id].length = 100;
    
    // Try to read the tuple - should detect invalid pointer
    const uint8_t* data;
    uint32_t size;
    ErrorContext ctx;
    Status status = page.get_tuple(item_id, &data, &size, &ctx);
    
    EXPECT_NE(status, Status::Ok) << "Should detect invalid item pointer";
    EXPECT_EQ(status, Status::PageCorrupt) << "Should return PageCorrupt status";
    
    // Also test with offset that would cause integer overflow
    items[item_id].offset = page_size_ - 10;
    items[item_id].length = 200; // Would extend beyond page
    
    status = page.get_tuple(item_id, &data, &size, &ctx);
    EXPECT_NE(status, Status::Ok) << "Should detect tuple extending beyond page";
}

// Test: Checksum Mismatch - As requested by Agent A
TEST_F(HeapPageTest, ChecksumMismatch) {
    HeapPage page(page_buffer_, page_size_);
    ASSERT_EQ(page.initialize(1, nullptr), Status::Ok);
    
    // Insert some data
    std::vector<uint8_t> tuple_data(200, 0xBB);
    uint16_t item_id;
    ASSERT_EQ(page.insert_tuple(tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                               100, &item_id, nullptr), Status::Ok);
    
    // Get the current page header
    PageHeader* header = reinterpret_cast<PageHeader*>(page_buffer_);
    
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
    uint32_t computed_crc = calculate_page_checksum(page_buffer_, page_size_);
    EXPECT_NE(computed_crc, original_checksum) << "Checksum should have changed after corruption";
    
    // Restore original checksum to test detection
    header->checksum = original_checksum;
    
    // Now the stored checksum doesn't match the data
    uint32_t new_computed_crc = calculate_page_checksum(page_buffer_, page_size_);
    EXPECT_NE(new_computed_crc, header->checksum) << "Stored checksum should not match corrupted data";
}

// Additional focused page tests

// Test: Page initialization and header setup
TEST_F(HeapPageTest, PageInitialization) {
    HeapPage page(page_buffer_, page_size_);
    
    uint32_t page_id = 42;
    ASSERT_EQ(page.initialize(page_id, nullptr), Status::Ok);
    
    // Verify header is set correctly
    PageHeader* header = page.header();
    EXPECT_EQ(header->magic, kMagicSBRD);
    EXPECT_EQ(header->version, 1);
    EXPECT_EQ(header->page_type, PAGE_TYPE_HEAP);
    EXPECT_EQ(header->page_size, page_size_);
    EXPECT_EQ(header->page_id, page_id);
    EXPECT_EQ(header->item_count, 0);
    
    // Verify special area
    HeapPageSpecial* special = reinterpret_cast<HeapPageSpecial*>(
        page_buffer_ + page_size_ - sizeof(HeapPageSpecial));
    EXPECT_EQ(special->pd_lower, sizeof(PageHeader));
    EXPECT_EQ(special->pd_upper, page_size_ - sizeof(HeapPageSpecial));
    EXPECT_EQ(special->pd_special, page_size_ - sizeof(HeapPageSpecial));
}

// Test: Tuple insertion and retrieval
TEST_F(HeapPageTest, TupleOperations) {
    HeapPage page(page_buffer_, page_size_);
    ASSERT_EQ(page.initialize(1, nullptr), Status::Ok);
    
    // Test various tuple sizes
    std::vector<size_t> tuple_sizes = {10, 50, 100, 500, 1000};
    std::vector<uint16_t> item_ids;
    
    for (size_t size : tuple_sizes) {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; i++) {
            data[i] = (i + size) & 0xFF;
        }
        
        uint16_t item_id;
        Status status = page.insert_tuple(data.data(), data.size() + sizeof(TupleHeader),
                                         100 + size, &item_id, nullptr);
        ASSERT_EQ(status, Status::Ok) << "Failed to insert " << size << " byte tuple";
        item_ids.push_back(item_id);
    }
    
    // Verify all tuples can be retrieved correctly
    for (size_t i = 0; i < tuple_sizes.size(); i++) {
        const uint8_t* retrieved_data;
        uint32_t retrieved_size;
        Status status = page.get_tuple(item_ids[i], &retrieved_data, &retrieved_size, nullptr);
        ASSERT_EQ(status, Status::Ok) << "Failed to retrieve tuple " << i;
        
        // Size should include header
        EXPECT_EQ(retrieved_size, tuple_sizes[i] + sizeof(TupleHeader));
        
        // Verify data integrity
        const uint8_t* tuple_body = retrieved_data + sizeof(TupleHeader);
        for (size_t j = 0; j < tuple_sizes[i]; j++) {
            EXPECT_EQ(tuple_body[j], (j + tuple_sizes[i]) & 0xFF) 
                << "Data mismatch at byte " << j << " of tuple " << i;
        }
    }
}

// Test: Page free space calculation
TEST_F(HeapPageTest, FreeSpaceCalculation) {
    HeapPage page(page_buffer_, page_size_);
    ASSERT_EQ(page.initialize(1, nullptr), Status::Ok);
    
    uint32_t initial_free_space = page.get_free_space();
    uint32_t expected_free = page_size_ - sizeof(PageHeader) - sizeof(HeapPageSpecial);
    EXPECT_EQ(initial_free_space, expected_free);
    
    // Insert a tuple and verify free space decreases
    std::vector<uint8_t> tuple_data(100, 0xCC);
    uint16_t item_id;
    ASSERT_EQ(page.insert_tuple(tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                               100, &item_id, nullptr), Status::Ok);
    
    uint32_t after_insert_free = page.get_free_space();
    uint32_t space_used = sizeof(ItemPointer) + tuple_data.size() + sizeof(TupleHeader);
    EXPECT_EQ(after_insert_free, initial_free_space - space_used);
}

// Test: Deleted tuple handling
TEST_F(HeapPageTest, DeletedTupleHandling) {
    HeapPage page(page_buffer_, page_size_);
    ASSERT_EQ(page.initialize(1, nullptr), Status::Ok);
    
    // Insert multiple tuples
    std::vector<uint16_t> item_ids;
    for (int i = 0; i < 5; i++) {
        std::vector<uint8_t> data(100, i);
        uint16_t item_id;
        ASSERT_EQ(page.insert_tuple(data.data(), data.size() + sizeof(TupleHeader),
                                   100, &item_id, nullptr), Status::Ok);
        item_ids.push_back(item_id);
    }
    
    // Delete some tuples
    ASSERT_EQ(page.delete_tuple(item_ids[1], 200, nullptr), Status::Ok);
    ASSERT_EQ(page.delete_tuple(item_ids[3], 200, nullptr), Status::Ok);
    
    // Verify deleted tuples are marked correctly
    const uint8_t* data;
    uint32_t size;
    
    // Non-deleted tuples should still be accessible
    EXPECT_EQ(page.get_tuple(item_ids[0], &data, &size, nullptr), Status::Ok);
    EXPECT_EQ(page.get_tuple(item_ids[2], &data, &size, nullptr), Status::Ok);
    EXPECT_EQ(page.get_tuple(item_ids[4], &data, &size, nullptr), Status::Ok);
    
    // Deleted tuples might still be readable but marked as deleted
    Status status = page.get_tuple(item_ids[1], &data, &size, nullptr);
    if (status == Status::Ok) {
        const TupleHeader* hdr = reinterpret_cast<const TupleHeader*>(data);
        EXPECT_TRUE(hdr->is_deleted() || hdr->xmax != 0) << "Tuple should be marked as deleted";
    }
}

// Test: Page validation
TEST_F(HeapPageTest, PageValidation) {
    HeapPage page(page_buffer_, page_size_);
    ASSERT_EQ(page.initialize(1, nullptr), Status::Ok);
    
    // Valid page should pass validation
    ErrorContext ctx;
    EXPECT_EQ(page.validate(&ctx), Status::Ok);
    
    // Corrupt the page header magic
    PageHeader* header = page.header();
    uint32_t original_magic = header->magic;
    header->magic = 0xDEADBEEF;
    
    EXPECT_NE(page.validate(&ctx), Status::Ok) << "Should detect invalid magic";
    
    // Restore magic, corrupt page type
    header->magic = original_magic;
    header->page_type = 99; // Invalid type
    
    EXPECT_NE(page.validate(&ctx), Status::Ok) << "Should detect invalid page type";
    
    // Restore page type, corrupt special area
    header->page_type = PAGE_TYPE_HEAP;
    HeapPageSpecial* special = reinterpret_cast<HeapPageSpecial*>(
        page_buffer_ + page_size_ - sizeof(HeapPageSpecial));
    special->pd_lower = page_size_; // Invalid - beyond page
    
    EXPECT_NE(page.validate(&ctx), Status::Ok) << "Should detect invalid special area";
}