#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include <cstring>
#include <filesystem>
#include <vector>
#include <thread>
#include <sys/resource.h>

using namespace scratchbird::core;

class StorageCriticalFixesTest : public ::testing::Test {
protected:
    void SetUp() override {
        cleanup_test_files();
    }
    
    void TearDown() override {
        cleanup_test_files();
    }
    
    void cleanup_test_files() {
        std::filesystem::remove("test_critical.db");
    }
    
    // Helper to get current memory usage
    size_t get_memory_usage() {
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        return usage.ru_maxrss * 1024; // Convert to bytes on Linux
    }
};

// Test 1: Verify memory leak fix in HeapScanIterator
// This test will FAIL until the memory leak is fixed
TEST_F(StorageCriticalFixesTest, HeapScanIterator_NoMemoryLeak) {
    // Create database with many tuples
    ASSERT_EQ(Database::create("test_critical.db", 8192), Status::Ok);
    
    Database db;
    ASSERT_EQ(db.open("test_critical.db"), Status::Ok);
    
    StorageEngine engine(&db);
    
    // Insert 1000 tuples
    std::vector<uint8_t> tuple_data(100, 0xAA);
    for (int i = 0; i < 1000; i++) {
        uint32_t page_id;
        uint16_t item_id;
        Status status = engine.insert_tuple(1, tuple_data.data(), tuple_data.size(),
                                           &page_id, &item_id, nullptr);
        ASSERT_EQ(status, Status::Ok) << "Failed to insert tuple " << i;
    }
    
    // Measure memory before scan
    size_t memory_before = get_memory_usage();
    
    // Perform multiple scans to amplify memory leak
    for (int scan = 0; scan < 10; scan++) {
        auto iterator = engine.create_scan(1, nullptr);
        ASSERT_NE(iterator, nullptr);
        
        int count = 0;
        Tuple tuple;
        while (!iterator->is_done()) {
            Status status = iterator->next(&tuple, nullptr);
            if (status == Status::Ok) {
                count++;
            }
        }
        EXPECT_GT(count, 0) << "Should have scanned some tuples";
    }
    
    // Force garbage collection if possible
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Measure memory after scans
    size_t memory_after = get_memory_usage();
    
    // Memory increase should be minimal (less than 10MB for metadata)
    size_t memory_increase = memory_after - memory_before;
    
    // CURRENT BEHAVIOR: This will FAIL due to memory leak
    // Each scan creates a new StorageEngine instance that's never freed
    // Expected: ~1000 * 10 * sizeof(StorageEngine) ≈ 10MB+ leak
    EXPECT_LT(memory_increase, 10 * 1024 * 1024) 
        << "Memory leak detected: " << memory_increase << " bytes increased";
    
    db.close();
}

// Test 2: Verify buffer overflow fix in tuple insertion
// This test will FAIL until the buffer overflow issue is fixed
TEST_F(StorageCriticalFixesTest, HeapPage_InsertTuple_BufferOverflowProtection) {
    ASSERT_EQ(Database::create("test_critical.db", 8192), Status::Ok);
    
    Database db;
    ASSERT_EQ(db.open("test_critical.db"), Status::Ok);
    
    StorageEngine engine(&db);
    
    // Test Case 1: Correct API usage - tuple_size includes TupleHeader
    {
        // Create raw data
        std::vector<uint8_t> raw_data(50, 0xBB);
        
        uint32_t page_id;
        uint16_t item_id;
        // Pass raw data but size includes header
        Status status = engine.insert_tuple(1, raw_data.data(), 
                                           raw_data.size() + sizeof(TupleHeader),
                                           &page_id, &item_id, nullptr);
        
        // Should succeed with fixed implementation
        EXPECT_EQ(status, Status::Ok) << "API expects tuple_size to include header";
        
        // Verify the data was stored correctly
        Tuple read_tuple;
        status = engine.get_tuple(page_id, item_id, &read_tuple, nullptr);
        EXPECT_EQ(status, Status::Ok);
        // The returned size should be the full size including header
        EXPECT_EQ(read_tuple.size, raw_data.size() + sizeof(TupleHeader));
    }
    
    // Test Case 2: Tuple data that does NOT include space for TupleHeader
    // This will cause buffer overflow in current implementation
    {
        // Create raw data without header space
        std::vector<uint8_t> raw_data(50, 0xCC);
        
        // CURRENT BEHAVIOR: This will cause buffer overflow
        // The code does: memcpy(dst, src, tuple_size - sizeof(TupleHeader))
        // If tuple_size = 50, it tries to copy 50 - 12 = 38 bytes
        // But we're passing 50 bytes of data, reading past the end
        
        // To detect this, we need to ensure the API contract is clear
        // Either:
        // 1. API should accept raw data and add header (safer)
        // 2. API should validate that tuple_size >= sizeof(TupleHeader) + data_size
    }
    
    // Test Case 3: Minimum size validation
    {
        // Try to insert tuple smaller than TupleHeader size
        std::vector<uint8_t> tiny_data(5, 0xDD);
        
        uint32_t page_id;
        uint16_t item_id;
        Status status = engine.insert_tuple(1, tiny_data.data(), tiny_data.size(),
                                           &page_id, &item_id, nullptr);
        
        // CURRENT BEHAVIOR: This will cause integer underflow
        // tuple_size(5) - sizeof(TupleHeader)(12) = -7 wrapped to huge number
        EXPECT_NE(status, Status::Ok) 
            << "Should reject tuple smaller than header size";
    }
    
    db.close();
}

// Test 3: Verify proper StorageEngine usage in HeapScanIterator
// This test demonstrates the correct fix
TEST_F(StorageCriticalFixesTest, HeapScanIterator_ProperEngineUsage) {
    ASSERT_EQ(Database::create("test_critical.db", 8192), Status::Ok);
    
    Database db;
    ASSERT_EQ(db.open("test_critical.db"), Status::Ok);
    
    StorageEngine engine(&db);
    
    // Insert test data
    std::vector<uint8_t> tuple_data(100, 0xEE);
    uint32_t page_id;
    uint16_t item_id;
    
    // Insert with different transaction IDs to test visibility
    // Note: begin_transaction is now handled by TransactionManager
    ASSERT_EQ(engine.insert_tuple(1, tuple_data.data(), tuple_data.size(),
                                  &page_id, &item_id, nullptr), Status::Ok);
    
    // Create iterator
    auto iterator = engine.create_scan(1, nullptr);
    ASSERT_NE(iterator, nullptr);
    
    // The iterator should use the parent engine's transaction context
    // NOT create its own engine instance
    Tuple tuple;
    Status status = iterator->next(&tuple, nullptr);
    EXPECT_EQ(status, Status::Ok) << "Should find visible tuple";
    
    // Verify no memory leak by checking iterator internals
    // (This would require friend class or public method to verify)
    
    db.close();
}

// Test 4: Stress test for memory leak detection
TEST_F(StorageCriticalFixesTest, HeapScanIterator_StressTestMemoryLeak) {
    ASSERT_EQ(Database::create("test_critical.db", 32768), Status::Ok);
    
    Database db;
    ASSERT_EQ(db.open("test_critical.db"), Status::Ok);
    
    StorageEngine engine(&db);
    
    // Insert many tuples across multiple pages
    std::vector<uint8_t> tuple_data(500, 0xFF);
    for (int i = 0; i < 500; i++) {
        uint32_t page_id;
        uint16_t item_id;
        Status status = engine.insert_tuple(1, tuple_data.data(), tuple_data.size(),
                                           &page_id, &item_id, nullptr);
        ASSERT_EQ(status, Status::Ok);
    }
    
    // Get baseline memory
    size_t initial_memory = get_memory_usage();
    
    // Perform many sequential scans
    for (int iteration = 0; iteration < 100; iteration++) {
        auto iterator = engine.create_scan(1, nullptr);
        
        int count = 0;
        Tuple tuple;
        while (!iterator->is_done()) {
            if (iterator->next(&tuple, nullptr) == Status::Ok) {
                count++;
            }
        }
        
        EXPECT_GT(count, 0) << "Should scan tuples in iteration " << iteration;
        
        // Check memory growth periodically
        if (iteration % 10 == 0) {
            size_t current_memory = get_memory_usage();
            size_t growth = current_memory - initial_memory;
            
            // CURRENT BEHAVIOR: Will show steady memory growth
            // Each scan leaks a StorageEngine instance
            // After 100 scans: ~100 * sizeof(StorageEngine) * 500 tuples
            if (iteration > 0) {
                EXPECT_LT(growth, 50 * 1024 * 1024) 
                    << "Excessive memory growth at iteration " << iteration 
                    << ": " << growth << " bytes";
            }
        }
    }
    
    db.close();
}

// Test 5: Buffer overflow with boundary conditions
TEST_F(StorageCriticalFixesTest, HeapPage_InsertTuple_BoundaryValidation) {
    ASSERT_EQ(Database::create("test_critical.db", 8192), Status::Ok);
    
    Database db;
    ASSERT_EQ(db.open("test_critical.db"), Status::Ok);
    
    // Direct page manipulation to test the fix
    uint8_t* page_buffer = new(std::nothrow) uint8_t[8192];
    ASSERT_NE(page_buffer, nullptr);
    memset(page_buffer, 0, 8192);
    
    HeapPage heap_page(page_buffer, 8192);
    ASSERT_EQ(heap_page.initialize(7, nullptr), Status::Ok);
    
    // Test exact TupleHeader size
    {
        uint8_t data[sizeof(TupleHeader)];
        memset(data, 0xAA, sizeof(data));
        
        uint16_t item_id;
        Status status = heap_page.insert_tuple(data, sizeof(data), 100, &item_id, nullptr);
        
        // CURRENT BEHAVIOR: This will try to copy negative bytes
        // sizeof(data) - sizeof(TupleHeader) = 0 bytes to copy
        // But the API is unclear about whether data includes header
        EXPECT_NE(status, Status::Ok) 
            << "Should handle edge case of tuple_size == sizeof(TupleHeader)";
    }
    
    // Test with proper data size
    {
        uint8_t data[100];
        memset(data, 0xBB, sizeof(data));
        
        uint16_t item_id;
        // If API expects raw data, pass raw data size
        // If API expects size including header, pass size + sizeof(TupleHeader)
        Status status = heap_page.insert_tuple(data, sizeof(data) + sizeof(TupleHeader), 
                                             100, &item_id, nullptr);
        
        if (status == Status::Ok) {
            // Verify data integrity
            const uint8_t* read_data;
            uint32_t read_size;
            status = heap_page.get_tuple(item_id, &read_data, &read_size, nullptr);
            EXPECT_EQ(status, Status::Ok);
            
            // Should be able to read back our data
            const uint8_t* tuple_body = read_data + sizeof(TupleHeader);
            EXPECT_EQ(memcmp(tuple_body, data, sizeof(data)), 0) 
                << "Data corruption detected";
        }
    }
    
    delete[] page_buffer;
    db.close();
}

// Test 6: Comprehensive test for both fixes together
TEST_F(StorageCriticalFixesTest, CombinedFixes_ScanAndInsert) {
    ASSERT_EQ(Database::create("test_critical.db", 16384), Status::Ok);
    
    Database db;
    ASSERT_EQ(db.open("test_critical.db"), Status::Ok);
    
    StorageEngine engine(&db);
    
    // Insert various sized tuples with clear API usage
    std::vector<std::vector<uint8_t>> test_data = {
        std::vector<uint8_t>(50, 0x11),
        std::vector<uint8_t>(100, 0x22),
        std::vector<uint8_t>(200, 0x33),
        std::vector<uint8_t>(1000, 0x44)
    };
    
    std::vector<std::pair<uint32_t, uint16_t>> inserted_ids;
    
    for (const auto& data : test_data) {
        uint32_t page_id;
        uint16_t item_id;
        
        // Clear API usage - if it expects size with header
        size_t total_size = data.size() + sizeof(TupleHeader);
        Status status = engine.insert_tuple(1, data.data(), total_size,
                                           &page_id, &item_id, nullptr);
        
        // Should validate the size properly
        if (status == Status::Ok) {
            inserted_ids.push_back({page_id, item_id});
        }
    }
    
    EXPECT_EQ(inserted_ids.size(), test_data.size()) 
        << "Some insertions failed";
    
    // Now scan without memory leak
    size_t memory_before = get_memory_usage();
    
    for (int i = 0; i < 50; i++) {
        auto iterator = engine.create_scan(1, nullptr);
        
        int count = 0;
        Tuple tuple;
        while (!iterator->is_done()) {
            if (iterator->next(&tuple, nullptr) == Status::Ok) {
                count++;
                // Verify tuple data is valid
                EXPECT_GT(tuple.size, sizeof(TupleHeader));
            }
        }
        
        EXPECT_EQ(count, inserted_ids.size()) 
            << "Scan found wrong number of tuples";
    }
    
    size_t memory_after = get_memory_usage();
    size_t memory_growth = memory_after - memory_before;
    
    // Should have minimal memory growth after 50 scans
    EXPECT_LT(memory_growth, 5 * 1024 * 1024) 
        << "Memory leak still present: " << memory_growth << " bytes";
    
    db.close();
}