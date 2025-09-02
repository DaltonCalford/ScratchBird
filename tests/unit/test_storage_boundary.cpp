#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include <cstring>
#include <filesystem>
#include <vector>

using namespace scratchbird::core;

class StorageBoundaryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any existing test files
        cleanup_test_files();
    }
    
    void TearDown() override {
        cleanup_test_files();
    }
    
    void cleanup_test_files() {
        std::filesystem::remove("test_boundary.db");
    }
};

// Test maximum tuple size that fits in a page
TEST_F(StorageBoundaryTest, MaxTupleSize) {
    // Create database
    ASSERT_EQ(Database::create("test_boundary.db", 8192), Status::Ok);
    
    Database db;
    ASSERT_EQ(db.open("test_boundary.db"), Status::Ok);
    
    StorageEngine engine(&db);
    
    // Calculate maximum tuple size
    // Page size - PageHeader - HeapPageSpecial - ItemPointer - TupleHeader
    uint32_t max_tuple_size = 8192 - sizeof(PageHeader) - sizeof(HeapPageSpecial) 
                             - sizeof(ItemPointer) - sizeof(TupleHeader);
    
    // Create a tuple of maximum size
    std::vector<uint8_t> large_tuple(max_tuple_size - sizeof(TupleHeader), 0xFF);
    
    uint32_t page_id;
    uint16_t item_id;
    ErrorContext ctx;
    
    // This should succeed
    Status status = engine.insert_tuple(1, large_tuple.data(), large_tuple.size(),
                                       &page_id, &item_id, &ctx);
    EXPECT_EQ(status, Status::Ok) << "Should insert maximum size tuple";
    
    // Try one byte larger - should fail
    large_tuple.push_back(0xFF);
    status = engine.insert_tuple(1, large_tuple.data(), large_tuple.size(),
                                &page_id, &item_id, &ctx);
    EXPECT_NE(status, Status::Ok) << "Should fail with tuple too large";
    
    db.close();
}

// Test zero-size tuple
TEST_F(StorageBoundaryTest, ZeroSizeTuple) {
    ASSERT_EQ(Database::create("test_boundary.db", 8192), Status::Ok);
    
    Database db;
    ASSERT_EQ(db.open("test_boundary.db"), Status::Ok);
    
    StorageEngine engine(&db);
    
    uint32_t page_id;
    uint16_t item_id;
    ErrorContext ctx;
    
    // Try to insert empty tuple (should fail or handle gracefully)
    Status status = engine.insert_tuple(1, nullptr, 0, &page_id, &item_id, &ctx);
    EXPECT_NE(status, Status::Ok) << "Should reject zero-size tuple";
    
    db.close();
}

// Test exact page boundary conditions
TEST_F(StorageBoundaryTest, ExactPageBoundary) {
    ASSERT_EQ(Database::create("test_boundary.db", 8192), Status::Ok);
    
    Database db;
    ASSERT_EQ(db.open("test_boundary.db"), Status::Ok);
    
    StorageEngine engine(&db);
    
    // Calculate space after headers
    uint32_t usable_space = 8192 - sizeof(PageHeader) - sizeof(HeapPageSpecial);
    uint32_t tuple_size = 100; // Small tuple size
    uint32_t max_tuples = (usable_space - sizeof(ItemPointer)) / 
                         (tuple_size + sizeof(TupleHeader) + sizeof(ItemPointer));
    
    std::vector<uint8_t> tuple_data(tuple_size, 0xAA);
    uint32_t page_id;
    uint16_t item_id;
    ErrorContext ctx;
    
    // Fill page to near capacity
    for (uint32_t i = 0; i < max_tuples - 1; i++) {
        Status status = engine.insert_tuple(1, tuple_data.data(), tuple_data.size(),
                                           &page_id, &item_id, &ctx);
        ASSERT_EQ(status, Status::Ok) << "Failed at tuple " << i;
    }
    
    // Insert one more - should succeed
    Status status = engine.insert_tuple(1, tuple_data.data(), tuple_data.size(),
                                       &page_id, &item_id, &ctx);
    EXPECT_EQ(status, Status::Ok) << "Should fit last tuple";
    
    // One more should require new page
    uint32_t first_page = page_id;
    status = engine.insert_tuple(1, tuple_data.data(), tuple_data.size(),
                                &page_id, &item_id, &ctx);
    EXPECT_EQ(status, Status::Ok) << "Should allocate new page";
    EXPECT_NE(page_id, first_page) << "Should be on different page";
    
    db.close();
}

// Test maximum item count (uint16_t limit)
TEST_F(StorageBoundaryTest, MaxItemCount) {
    // This test would require a very large page size to fit 65535 items
    // For now, test that item count doesn't overflow
    ASSERT_EQ(Database::create("test_boundary.db", 32768), Status::Ok);
    
    Database db;
    ASSERT_EQ(db.open("test_boundary.db"), Status::Ok);
    
    // Direct page manipulation to test boundary
    uint8_t* page_buffer = new(std::nothrow) uint8_t[32768];
    ASSERT_NE(page_buffer, nullptr);
    memset(page_buffer, 0, 32768);
    
    HeapPage heap_page(page_buffer, 32768);
    ASSERT_EQ(heap_page.initialize(7, nullptr), Status::Ok);
    
    // Manually set item count to near overflow
    heap_page.header()->item_count = 65530;
    
    // Verify validation catches this
    ErrorContext ctx;
    Status status = heap_page.validate(&ctx);
    EXPECT_NE(status, Status::Ok) << "Should detect invalid item count";
    
    delete[] page_buffer;
    db.close();
}

// Test tuple at exact offset boundaries
TEST_F(StorageBoundaryTest, TupleOffsetBoundaries) {
    ASSERT_EQ(Database::create("test_boundary.db", 8192), Status::Ok);
    
    Database db;
    ASSERT_EQ(db.open("test_boundary.db"), Status::Ok);
    
    uint8_t* page_buffer = new(std::nothrow) uint8_t[8192];
    ASSERT_NE(page_buffer, nullptr);
    memset(page_buffer, 0, 8192);
    
    HeapPage heap_page(page_buffer, 8192);
    ASSERT_EQ(heap_page.initialize(7, nullptr), Status::Ok);
    
    // Test tuple at start of data area (after headers)
    uint8_t tuple_data[50];
    memset(tuple_data, 0xBB, sizeof(tuple_data));
    
    uint16_t item_id;
    Status status = heap_page.insert_tuple(tuple_data, sizeof(tuple_data), 
                                          100, &item_id, nullptr);
    EXPECT_EQ(status, Status::Ok) << "Should insert at valid offset";
    
    // Verify tuple is readable
    const uint8_t* read_data;
    uint32_t read_size;
    status = heap_page.get_tuple(item_id, &read_data, &read_size, nullptr);
    EXPECT_EQ(status, Status::Ok) << "Should read tuple";
    EXPECT_EQ(read_size, sizeof(tuple_data) + sizeof(TupleHeader));
    
    delete[] page_buffer;
    db.close();
}

// Test page size boundaries (8KB, 16KB, 32KB)
TEST_F(StorageBoundaryTest, AllValidPageSizes) {
    const uint32_t page_sizes[] = {8192, 16384, 32768};
    
    for (uint32_t page_size : page_sizes) {
        std::string db_name = "test_boundary_" + std::to_string(page_size) + ".db";
        
        ASSERT_EQ(Database::create(db_name, page_size), Status::Ok);
        
        Database db;
        ASSERT_EQ(db.open(db_name), Status::Ok);
        EXPECT_EQ(db.page_size(), page_size) << "Page size mismatch";
        
        StorageEngine engine(&db);
        
        // Insert tuple in each page size
        uint8_t tuple_data[100];
        memset(tuple_data, 0xCC, sizeof(tuple_data));
        
        uint32_t page_id;
        uint16_t item_id;
        Status status = engine.insert_tuple(1, tuple_data, sizeof(tuple_data),
                                           &page_id, &item_id, nullptr);
        EXPECT_EQ(status, Status::Ok) << "Should insert in " << page_size << " page";
        
        db.close();
        std::filesystem::remove(db_name);
    }
}

// Test invalid page sizes
TEST_F(StorageBoundaryTest, InvalidPageSizes) {
    // Test page sizes that are not supported
    const uint32_t invalid_sizes[] = {4096, 7000, 9000, 65536, 0, UINT32_MAX};
    
    for (uint32_t page_size : invalid_sizes) {
        Status status = Database::create("test_boundary.db", page_size);
        EXPECT_NE(status, Status::Ok) << "Should reject page size " << page_size;
        cleanup_test_files();
    }
}

// Test tuple spanning special area
TEST_F(StorageBoundaryTest, TupleInSpecialArea) {
    ASSERT_EQ(Database::create("test_boundary.db", 8192), Status::Ok);
    
    Database db;
    ASSERT_EQ(db.open("test_boundary.db"), Status::Ok);
    
    uint8_t* page_buffer = new(std::nothrow) uint8_t[8192];
    ASSERT_NE(page_buffer, nullptr);
    memset(page_buffer, 0, 8192);
    
    HeapPage heap_page(page_buffer, 8192);
    ASSERT_EQ(heap_page.initialize(7, nullptr), Status::Ok);
    
    // Manually corrupt page to place tuple in special area
    HeapPageSpecial* special = reinterpret_cast<HeapPageSpecial*>(
        page_buffer + 8192 - sizeof(HeapPageSpecial));
    special->pd_upper = 8192 - 10; // Invalid - overlaps special area
    
    ErrorContext ctx;
    Status status = heap_page.validate(&ctx);
    EXPECT_NE(status, Status::Ok) << "Should detect tuple in special area";
    
    delete[] page_buffer;
    db.close();
}