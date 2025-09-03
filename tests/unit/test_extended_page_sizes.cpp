#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include <filesystem>
#include <vector>
#include <chrono>
#include <iostream>
#include <iomanip>

using namespace scratchbird;
using namespace scratchbird::core;

class ExtendedPageSizesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any existing test files
        for (uint32_t ps : {8192u, 16384u, 32768u, 65536u, 131072u}) {
            std::string path = "test_extended_" + std::to_string(ps) + ".db";
            std::filesystem::remove(path);
        }
    }
    
    void TearDown() override {
        // Clean up test files
        for (uint32_t ps : {8192u, 16384u, 32768u, 65536u, 131072u}) {
            std::string path = "test_extended_" + std::to_string(ps) + ".db";
            std::filesystem::remove(path);
        }
    }
};

TEST_F(ExtendedPageSizesTest, CreateDatabaseAllPageSizes) {
    ErrorContext ctx;
    const std::vector<uint32_t> page_sizes = {8192u, 16384u, 32768u, 65536u, 131072u};
    
    for (uint32_t ps : page_sizes) {
        std::string path = "test_extended_" + std::to_string(ps) + ".db";
        
        // Create database with specific page size
        ASSERT_EQ(Database::create(path, ps, &ctx), Status::Ok) 
            << "Failed to create database with page size " << ps;
        
        // Open and verify
        Database db;
        ASSERT_EQ(db.open(path, &ctx), Status::Ok)
            << "Failed to open database with page size " << ps;
        
        ASSERT_EQ(db.page_size(), ps) 
            << "Page size mismatch for " << ps;
        
        // Verify file size is at least 2 pages (header + catalog)
        auto file_size = std::filesystem::file_size(path);
        ASSERT_GE(file_size, ps * 2)
            << "File size too small for page size " << ps;
        
        db.close();
    }
}

TEST_F(ExtendedPageSizesTest, BufferPoolWithLargePages) {
    ErrorContext ctx;
    const std::vector<uint32_t> page_sizes = {65536u, 131072u};
    
    for (uint32_t ps : page_sizes) {
        std::string path = "test_extended_bp_" + std::to_string(ps) + ".db";
        std::filesystem::remove(path);
        
        ASSERT_EQ(Database::create(path, ps, &ctx), Status::Ok);
        
        Database db;
        ASSERT_EQ(db.open(path, &ctx), Status::Ok);
        
        // Test buffer pool operations with large pages
        BufferPool::Config config;
        config.pool_size = 10;
        config.page_size = ps;
        
        BufferPool bp(&db, config);
        ASSERT_EQ(bp.initialize(&ctx), Status::Ok);
        
        // Pin and unpin pages
        void* buffer;
        ASSERT_EQ(bp.pin_page(0, &buffer, &ctx), Status::Ok);
        ASSERT_NE(buffer, nullptr);
        
        bp.unpin_page(0, false, &ctx);
        
        // Test multiple page allocations
        for (int i = 0; i < 5; i++) {
            ASSERT_EQ(bp.pin_page(i, &buffer, &ctx), Status::Ok);
            bp.unpin_page(i, true, &ctx);
        }
        
        bp.shutdown();
        db.close();
        std::filesystem::remove(path);
    }
}

TEST_F(ExtendedPageSizesTest, HeapPageOperationsLargePages) {
    ErrorContext ctx;
    const std::vector<uint32_t> page_sizes = {65536u, 131072u};
    
    for (uint32_t ps : page_sizes) {
        // Create a heap page buffer
        uint8_t* page_buffer = new(std::nothrow) uint8_t[ps];
        ASSERT_NE(page_buffer, nullptr);
        memset(page_buffer, 0, ps);
        
        HeapPage heap_page(page_buffer, ps);
        ASSERT_EQ(heap_page.initialize(1, &ctx), Status::Ok);
        
        // Calculate expected free space
        uint32_t expected_free = ps - sizeof(PageHeader) - sizeof(HeapPageSpecial);
        ASSERT_NEAR(heap_page.get_free_space(), expected_free, 100)
            << "Free space calculation incorrect for page size " << ps;
        
        // Test inserting tuples
        std::vector<uint8_t> tuple_data(100);
        memset(tuple_data.data(), 'A', tuple_data.size());
        
        // Add TupleHeader
        TupleHeader* hdr = reinterpret_cast<TupleHeader*>(tuple_data.data());
        hdr->xmin = 1;
        hdr->xmax = 0;
        hdr->flags = 0;
        hdr->null_bitmap_offset = 0;
        
        uint16_t item_id;
        ASSERT_EQ(heap_page.insert_tuple(tuple_data.data(), tuple_data.size(), 
                                        1, &item_id, &ctx), Status::Ok)
            << "Failed to insert tuple in page size " << ps;
        
        // Test retrieving tuple
        const uint8_t* retrieved_data;
        uint32_t retrieved_size;
        ASSERT_EQ(heap_page.get_tuple(item_id, &retrieved_data, &retrieved_size, &ctx), Status::Ok);
        ASSERT_EQ(retrieved_size, tuple_data.size());
        
        delete[] page_buffer;
    }
}

TEST_F(ExtendedPageSizesTest, StorageEngineWithLargePages) {
    ErrorContext ctx;
    const std::vector<uint32_t> page_sizes = {65536u, 131072u};
    
    for (uint32_t ps : page_sizes) {
        std::string path = "test_extended_storage_" + std::to_string(ps) + ".db";
        std::filesystem::remove(path);
        
        ASSERT_EQ(Database::create(path, ps, &ctx), Status::Ok);
        
        Database db;
        ASSERT_EQ(db.open(path, &ctx), Status::Ok);
        
        // Storage operations will be initialized internally by the database
        // Let's just verify basic operations work
        
        // Verify we can read the system catalog page
        void* page0;
        BufferPool::Config config;
        config.pool_size = 10;
        config.page_size = ps;
        BufferPool bp(&db, config);
        ASSERT_EQ(bp.initialize(&ctx), Status::Ok);
        
        ASSERT_EQ(bp.pin_page(0, &page0, &ctx), Status::Ok);
        
        // Verify header
        PageHeader* hdr = reinterpret_cast<PageHeader*>(page0);
        ASSERT_EQ(hdr->magic, kMagicSBRD);
        ASSERT_EQ(hdr->page_size, ps);
        
        bp.unpin_page(0, false, &ctx);
        bp.shutdown();
        db.close();
        std::filesystem::remove(path);
    }
}

TEST_F(ExtendedPageSizesTest, MaxTupleSizeForLargePages) {
    ErrorContext ctx;
    
    // Test maximum tuple size for 64K and 128K pages
    const std::vector<std::pair<uint32_t, uint32_t>> page_and_max_sizes = {
        {65536u, 65536u - sizeof(PageHeader) - sizeof(HeapPageSpecial) - sizeof(ItemPointer) - 100},
        {131072u, 131072u - sizeof(PageHeader) - sizeof(HeapPageSpecial) - sizeof(ItemPointer) - 100}
    };
    
    for (const auto& [ps, max_tuple_size] : page_and_max_sizes) {
        uint8_t* page_buffer = new(std::nothrow) uint8_t[ps];
        ASSERT_NE(page_buffer, nullptr);
        memset(page_buffer, 0, ps);
        
        HeapPage heap_page(page_buffer, ps);
        ASSERT_EQ(heap_page.initialize(1, &ctx), Status::Ok);
        
        // Create a large tuple
        std::vector<uint8_t> tuple_data(max_tuple_size);
        TupleHeader* hdr = reinterpret_cast<TupleHeader*>(tuple_data.data());
        hdr->xmin = 1;
        hdr->xmax = 0;
        hdr->flags = 0;
        hdr->null_bitmap_offset = 0;
        
        // Fill rest with data
        memset(tuple_data.data() + sizeof(TupleHeader), 'X', 
               tuple_data.size() - sizeof(TupleHeader));
        
        uint16_t item_id;
        ASSERT_EQ(heap_page.insert_tuple(tuple_data.data(), tuple_data.size(),
                                        1, &item_id, &ctx), Status::Ok)
            << "Failed to insert max-size tuple for page size " << ps;
        
        // Verify we can't insert another large tuple
        uint16_t item_id2;
        ASSERT_NE(heap_page.insert_tuple(tuple_data.data(), tuple_data.size(),
                                        1, &item_id2, &ctx), Status::Ok)
            << "Should not be able to insert second large tuple";
        
        delete[] page_buffer;
    }
}

TEST_F(ExtendedPageSizesTest, PerformanceComparison) {
    ErrorContext ctx;
    const std::vector<uint32_t> page_sizes = {8192u, 16384u, 32768u, 65536u, 131072u};
    
    std::cout << "\nPerformance comparison across page sizes:\n";
    std::cout << "Page Size | File Size (KB)\n";
    std::cout << "----------|---------------\n";
    
    for (uint32_t ps : page_sizes) {
        std::string path = "test_extended_perf_" + std::to_string(ps) + ".db";
        std::filesystem::remove(path);
        
        ASSERT_EQ(Database::create(path, ps, &ctx), Status::Ok);
        
        // Check file size
        auto file_size = std::filesystem::file_size(path);
        std::cout << std::setw(9) << ps << " | " 
                  << std::setw(14) << (file_size / 1024) << "\n";
        
        std::filesystem::remove(path);
    }
}

TEST_F(ExtendedPageSizesTest, InvalidPageSizes) {
    ErrorContext ctx;
    Database db;
    
    // Test invalid page sizes
    const std::vector<uint32_t> invalid_sizes = {
        0, 1024, 2048, 4096, 7000, 9000, 15000, 17000,
        40000, 50000, 60000, 70000, 100000, 150000, 262144
    };
    
    for (uint32_t ps : invalid_sizes) {
        std::string path = "test_invalid_" + std::to_string(ps) + ".db";
        ASSERT_NE(Database::create(path, ps, &ctx), Status::Ok)
            << "Should not accept page size " << ps;
        
        // Verify file was not created
        ASSERT_FALSE(std::filesystem::exists(path));
    }
}

TEST_F(ExtendedPageSizesTest, FSMWithLargePages) {
    ErrorContext ctx;
    const std::vector<uint32_t> page_sizes = {65536u, 131072u};
    
    for (uint32_t ps : page_sizes) {
        std::string path = "test_extended_fsm_" + std::to_string(ps) + ".db";
        std::filesystem::remove(path);
        
        ASSERT_EQ(Database::create(path, ps, &ctx), Status::Ok);
        
        Database db;
        ASSERT_EQ(db.open(path, &ctx), Status::Ok);
        
        // Initialize page manager to test FSM with large pages
        PageManager pm(&db, ps);
        ASSERT_EQ(pm.initialize(&ctx), Status::Ok);
        
        // Allocate some pages
        std::vector<uint32_t> allocated_pages;
        for (int i = 0; i < 10; i++) {
            uint32_t page_id;
            ASSERT_EQ(pm.allocate_page(page_id, &ctx), Status::Ok);
            allocated_pages.push_back(page_id);
        }
        
        // Free some pages
        for (int i = 0; i < 5; i++) {
            ASSERT_EQ(pm.free_page(allocated_pages[i], &ctx), Status::Ok);
        }
        
        // Verify FSM tracks free pages correctly
        uint32_t page_id;
        ASSERT_EQ(pm.allocate_page(page_id, &ctx), Status::Ok);
        
        // Should reuse one of the freed pages
        bool found = false;
        for (int i = 0; i < 5; i++) {
            if (page_id == allocated_pages[i]) {
                found = true;
                break;
            }
        }
        ASSERT_TRUE(found) << "FSM should reuse freed pages";
        
        db.close();
        std::filesystem::remove(path);
    }
}