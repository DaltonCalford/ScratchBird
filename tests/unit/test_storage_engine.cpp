#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <fstream>

using namespace scratchbird::core;
namespace fs = std::filesystem;

class StorageEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any existing test database
        test_db_ = "test_storage.db";
        if (fs::exists(test_db_)) {
            fs::remove(test_db_);
        }
    }
    
    void TearDown() override {
        if (db_) {
            db_->close();
            db_.reset();
        }
        if (fs::exists(test_db_)) {
            fs::remove(test_db_);
        }
    }
    
    Status create_test_table(uint32_t* table_id_out) {
        ErrorContext ctx;
        
        // Create default schema if needed
        uint32_t schema_id;
        Status status = db_->catalog_manager()->create_schema("test", "root", schema_id, &ctx);
        if (status != Status::Ok && status != Status::FileExists) {
            return status;
        }
        
        // Create test table
        std::vector<CatalogManager::ColumnInfo> columns;
        // ColumnInfo: table_id, column_id, column_name, data_type, max_length, nullable, has_default, default_value
        columns.push_back({0, 0, "id", 1, 4, false, false, ""});        // int
        columns.push_back({0, 1, "name", 2, 64, true, false, ""});     // varchar
        columns.push_back({0, 2, "value", 3, 4, true, false, ""});     // float
        
        return db_->catalog_manager()->create_table(
            schema_id, "test_table", columns, *table_id_out, &ctx);
    }
    
    std::string test_db_;
    std::unique_ptr<Database> db_;
};

TEST_F(StorageEngineTest, HeapPageBasics) {
    ErrorContext ctx;
    
    // Create database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    
    // Open database
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    // Create a page buffer
    std::vector<uint8_t> page_buffer(8192, 0);
    
    // Initialize heap page
    HeapPage page(page_buffer.data(), 8192);
    ASSERT_EQ(Status::Ok, page.initialize(100, &ctx));
    
    // Verify page header
    EXPECT_EQ(PAGE_TYPE_HEAP, page.header()->page_type);
    EXPECT_EQ(100u, page.header()->page_id);
    EXPECT_EQ(0u, page.get_item_count());
    
    // Check free space (should be most of the page minus headers)
    uint32_t expected_free = 8192 - sizeof(PageHeader) - sizeof(HeapPageSpecial);
    EXPECT_NEAR(expected_free, page.get_free_space(), 8);
}

TEST_F(StorageEngineTest, TupleInsertion) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    // Create test table
    uint32_t table_id;
    ASSERT_EQ(Status::Ok, create_test_table(&table_id));
    
    // Get storage engine
    StorageEngine* engine = db_->storage_engine();
    ASSERT_NE(nullptr, engine);
    
    // Create test tuple data
    struct TestTuple {
        int32_t id;
        char name[64];
        float value;
    } test_data = {42, "Test Record", 3.14f};
    
    // Insert tuple
    uint32_t page_id;
    uint16_t item_id;
    ASSERT_EQ(Status::Ok, engine->insert_tuple(
        table_id, reinterpret_cast<uint8_t*>(&test_data),
        sizeof(TestTuple) + sizeof(TupleHeader), &page_id, &item_id, &ctx));
    
    // Verify insertion
    EXPECT_GE(page_id, 7u);  // Should be after catalog pages
    EXPECT_EQ(0u, item_id);  // First item in page
}

TEST_F(StorageEngineTest, TupleRetrieval) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    // Create test table
    uint32_t table_id;
    ASSERT_EQ(Status::Ok, create_test_table(&table_id));
    
    StorageEngine* engine = db_->storage_engine();
    
    // Insert a tuple
    struct TestTuple {
        int32_t id;
        char name[64];
        float value;
    } test_data = {123, "Retrieval Test", 2.718f};
    
    uint32_t page_id;
    uint16_t item_id;
    ASSERT_EQ(Status::Ok, engine->insert_tuple(
        table_id, reinterpret_cast<uint8_t*>(&test_data),
        sizeof(TestTuple) + sizeof(TupleHeader), &page_id, &item_id, &ctx));
    
    // Start a new transaction to ensure visibility
    // Transaction management is now handled by TransactionManager
    
    // Retrieve the tuple
    Tuple retrieved;
    ASSERT_EQ(Status::Ok, engine->get_tuple(page_id, item_id, &retrieved, &ctx));
    
    // Verify data
    ASSERT_EQ(sizeof(TestTuple), retrieved.size);
    TestTuple* retrieved_data = reinterpret_cast<TestTuple*>(retrieved.data.data());
    EXPECT_EQ(123, retrieved_data->id);
    EXPECT_STREQ("Retrieval Test", retrieved_data->name);
    EXPECT_FLOAT_EQ(2.718f, retrieved_data->value);
}

TEST_F(StorageEngineTest, TupleDeletion) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    // Create test table
    uint32_t table_id;
    ASSERT_EQ(Status::Ok, create_test_table(&table_id));
    
    StorageEngine* engine = db_->storage_engine();
    
    // Insert a tuple
    struct TestTuple {
        int32_t id;
        char name[64];
        float value;
    } test_data = {456, "Delete Me", 1.0f};
    
    uint32_t page_id;
    uint16_t item_id;
    ASSERT_EQ(Status::Ok, engine->insert_tuple(
        table_id, reinterpret_cast<uint8_t*>(&test_data),
        sizeof(TestTuple) + sizeof(TupleHeader), &page_id, &item_id, &ctx));
    
    // Start new transaction
    // Transaction management is now handled by TransactionManager
    
    // Delete the tuple
    ASSERT_EQ(Status::Ok, engine->delete_tuple(page_id, item_id, &ctx));
    
    // Start another transaction
    // Transaction management is now handled by TransactionManager
    
    // Try to retrieve - should not be visible
    Tuple retrieved;
    EXPECT_EQ(Status::NotFound, engine->get_tuple(page_id, item_id, &retrieved, &ctx));
}

TEST_F(StorageEngineTest, SequentialScan) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    // Create test table
    uint32_t table_id;
    ASSERT_EQ(Status::Ok, create_test_table(&table_id));
    
    StorageEngine* engine = db_->storage_engine();
    
    // Insert multiple tuples
    struct TestTuple {
        int32_t id;
        char name[64];
        float value;
    };
    
    std::vector<TestTuple> test_records = {
        {1, "First", 1.1f},
        {2, "Second", 2.2f},
        {3, "Third", 3.3f},
        {4, "Fourth", 4.4f},
        {5, "Fifth", 5.5f}
    };
    
    for (const auto& record : test_records) {
        uint32_t page_id;
        uint16_t item_id;
        ASSERT_EQ(Status::Ok, engine->insert_tuple(
            table_id, reinterpret_cast<const uint8_t*>(&record),
            sizeof(TestTuple) + sizeof(TupleHeader), &page_id, &item_id, &ctx));
    }
    
    // Start new transaction for visibility
    // Transaction management is now handled by TransactionManager
    
    // Create scan iterator
    auto scanner = engine->create_scan(table_id, &ctx);
    ASSERT_NE(nullptr, scanner);
    
    // Scan and count tuples
    int count = 0;
    std::vector<int32_t> found_ids;
    
    Tuple tuple;
    while (scanner->next(&tuple, &ctx) == Status::Ok) {
        ASSERT_EQ(sizeof(TestTuple), tuple.size);
        TestTuple* data = reinterpret_cast<TestTuple*>(tuple.data.data());
        found_ids.push_back(data->id);
        count++;
    }
    
    // Verify we found all tuples
    EXPECT_EQ(5, count);
    
    // Verify we found the right IDs (order may vary)
    std::sort(found_ids.begin(), found_ids.end());
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(i + 1, found_ids[i]);
    }
}

TEST_F(StorageEngineTest, Visibility) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    StorageEngine* engine = db_->storage_engine();
    
    // Test visibility rules
    uint64_t current_xid = engine->get_current_xid();
    
    // For testing, we'll use specific XIDs that avoid underflow
    // Since FROZEN_XID = 2, initial XID is 3, we'll test with larger values
    
    // Test 1: Tuple created by XID 1 (frozen), should be visible
    EXPECT_TRUE(engine->is_visible(1, 0, current_xid));
    
    // Test 2: Tuple created by XID 2 (frozen), should be visible  
    EXPECT_TRUE(engine->is_visible(2, 0, current_xid));
    
    // Test 3: Tuple created in future (current_xid + 10), should NOT be visible
    EXPECT_FALSE(engine->is_visible(current_xid + 10, 0, current_xid));
    
    // Test 4: Tuple created by XID 1, deleted by XID 2, should NOT be visible
    EXPECT_FALSE(engine->is_visible(1, 2, current_xid));
    
    // Test 5: Tuple created by XID 1, deleted in future, should be visible
    EXPECT_TRUE(engine->is_visible(1, current_xid + 10, current_xid));
}

TEST_F(StorageEngineTest, PageFull) {
    ErrorContext ctx;
    
    // Create and open database with small pages
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    // Create test table
    uint32_t table_id;
    ASSERT_EQ(Status::Ok, create_test_table(&table_id));
    
    StorageEngine* engine = db_->storage_engine();
    
    // Create a large tuple (about 1KB)
    struct LargeTuple {
        int32_t id;
        char padding[1020];  // ~1KB total with header
    };
    
    // Insert tuples until we span multiple pages
    std::vector<std::pair<uint32_t, uint16_t>> tuple_locations;
    uint32_t last_page_id = 0;
    
    for (int i = 0; i < 20; i++) {  // Should require at least 2-3 pages
        LargeTuple data;
        data.id = i;
        memset(data.padding, 'X', sizeof(data.padding));
        
        uint32_t page_id;
        uint16_t item_id;
        ASSERT_EQ(Status::Ok, engine->insert_tuple(
            table_id, reinterpret_cast<uint8_t*>(&data),
            sizeof(LargeTuple) + sizeof(TupleHeader), &page_id, &item_id, &ctx));
        
        tuple_locations.push_back({page_id, item_id});
        
        if (page_id > last_page_id) {
            last_page_id = page_id;
        }
    }
    
    // Verify we used multiple pages
    EXPECT_GT(last_page_id, tuple_locations[0].first);
    
    // Verify all tuples are retrievable
    // Transaction management is now handled by TransactionManager
    
    for (size_t i = 0; i < tuple_locations.size(); i++) {
        Tuple retrieved;
        ASSERT_EQ(Status::Ok, engine->get_tuple(
            tuple_locations[i].first, tuple_locations[i].second, &retrieved, &ctx));
        
        ASSERT_EQ(sizeof(LargeTuple), retrieved.size);
        LargeTuple* data = reinterpret_cast<LargeTuple*>(retrieved.data.data());
        EXPECT_EQ(static_cast<int32_t>(i), data->id);
    }
}

TEST_F(StorageEngineTest, ReuseDeletedSlots) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    // Create test table
    uint32_t table_id;
    ASSERT_EQ(Status::Ok, create_test_table(&table_id));
    
    StorageEngine* engine = db_->storage_engine();
    
    // Insert a tuple
    struct TestTuple {
        int32_t id;
        char name[64];
        float value;
    } test_data = {999, "Original", 9.99f};
    
    uint32_t page_id;
    uint16_t item_id;
    ASSERT_EQ(Status::Ok, engine->insert_tuple(
        table_id, reinterpret_cast<uint8_t*>(&test_data),
        sizeof(TestTuple) + sizeof(TupleHeader), &page_id, &item_id, &ctx));
    
    // Delete it
    // Transaction management is now handled by TransactionManager
    ASSERT_EQ(Status::Ok, engine->delete_tuple(page_id, item_id, &ctx));
    
    // Insert a new tuple - should reuse the slot
    test_data.id = 1000;
    strcpy(test_data.name, "Replacement");
    test_data.value = 10.0f;
    
    uint32_t new_page_id;
    uint16_t new_item_id;
    ASSERT_EQ(Status::Ok, engine->insert_tuple(
        table_id, reinterpret_cast<uint8_t*>(&test_data),
        sizeof(TestTuple) + sizeof(TupleHeader), &new_page_id, &new_item_id, &ctx));
    
    // Should reuse the same page and slot
    EXPECT_EQ(page_id, new_page_id);
    EXPECT_EQ(item_id, new_item_id);
    
    // Verify the new data
    // Transaction management is now handled by TransactionManager
    Tuple retrieved;
    ASSERT_EQ(Status::Ok, engine->get_tuple(new_page_id, new_item_id, &retrieved, &ctx));
    
    TestTuple* data = reinterpret_cast<TestTuple*>(retrieved.data.data());
    EXPECT_EQ(1000, data->id);
    EXPECT_STREQ("Replacement", data->name);
}

// Test: Corrupt Page Header - As requested by Agent A
TEST_F(StorageEngineTest, CorruptPageHeader) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    StorageEngine engine(db_.get());
    
    // Insert some data first
    std::vector<uint8_t> tuple_data(100, 0xAA);
    uint32_t page_id;
    uint16_t item_id;
    ASSERT_EQ(Status::Ok, engine.insert_tuple(1, 
                                             tuple_data.data(),
                                             tuple_data.size() + sizeof(TupleHeader),
                                             &page_id, &item_id, &ctx));
    
    // Sync to ensure it's written
    db_->sync(&ctx);
    
    // Close database to manipulate file
    db_.reset();
    
    // Corrupt the page header
    std::fstream file(test_db_, std::ios::binary | std::ios::in | std::ios::out);
    ASSERT_TRUE(file.is_open());
    
    // Seek to the heap page (assuming it's page 7)
    file.seekp(7 * 8192);
    
    // Write bad magic number
    uint32_t bad_magic = 0xDEADBEEF;
    file.write(reinterpret_cast<char*>(&bad_magic), sizeof(bad_magic));
    file.close();
    
    // Try to reopen - corruption might be detected at different times
    db_ = std::make_unique<Database>();
    Status open_status = db_->open(test_db_, &ctx);
    
    if (open_status == Status::Ok) {
        // If open succeeds, corruption should be detected during scan
        StorageEngine engine2(db_.get());
        
        // Try to scan - should detect corruption
        auto iterator = engine2.create_scan(1, &ctx);
        ASSERT_NE(nullptr, iterator);
        
        Tuple corrupted_tuple;
        Status status = iterator->next(&corrupted_tuple, &ctx);
        
        // Should detect the corruption
        EXPECT_NE(Status::Ok, status) << "Should detect corrupted page header during scan";
    } else {
        // Corruption detected during open is also acceptable
        EXPECT_EQ(Status::PageCorrupt, open_status) << "Expected PageCorrupt error on open";
    }
}

// Test: Invalid Item Pointer - As requested by Agent A  
TEST_F(StorageEngineTest, InvalidItemPointer) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    StorageEngine engine(db_.get());
    
    // Insert data
    std::vector<uint8_t> tuple_data(100, 0xBB);
    uint32_t page_id;
    uint16_t item_id;
    ASSERT_EQ(Status::Ok, engine.insert_tuple(1,
                                             tuple_data.data(),
                                             tuple_data.size() + sizeof(TupleHeader),
                                             &page_id, &item_id, &ctx));
    
    db_->sync(&ctx);
    
    // Close database
    db_.reset();
    
    // Corrupt an item pointer
    std::fstream file(test_db_, std::ios::binary | std::ios::in | std::ios::out);
    ASSERT_TRUE(file.is_open());
    
    // Seek to item pointer location (page 7 + PageHeader + item_id * ItemPointer)
    size_t offset = page_id * 8192 + sizeof(PageHeader) + item_id * sizeof(ItemPointer);
    file.seekp(offset);
    
    // Write invalid item pointer
    ItemPointer bad_item;
    bad_item.offset = 9000; // Beyond page size
    bad_item.length = 100;
    bad_item.flags = 0;
    
    file.write(reinterpret_cast<char*>(&bad_item), sizeof(ItemPointer));
    file.close();
    
    // Reopen - might detect corruption at open or during tuple access
    db_ = std::make_unique<Database>();
    Status open_status = db_->open(test_db_, &ctx);
    
    if (open_status == Status::Ok) {
        // If open succeeds, try to read the tuple
        StorageEngine engine2(db_.get());
        
        Tuple retrieved;
        Status status = engine2.get_tuple(page_id, item_id, &retrieved, &ctx);
        
        // Should detect invalid pointer
        EXPECT_NE(Status::Ok, status) << "Should detect invalid item pointer";
    } else {
        // Corruption detected during open is also acceptable
        EXPECT_EQ(Status::PageCorrupt, open_status) << "Expected PageCorrupt error on open";
    }
}

// Test: Checksum Mismatch - As requested by Agent A
TEST_F(StorageEngineTest, ChecksumMismatch) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    StorageEngine engine(db_.get());
    
    // Insert data
    std::vector<uint8_t> tuple_data(100, 0xCC);
    uint32_t page_id;
    uint16_t item_id;
    ASSERT_EQ(Status::Ok, engine.insert_tuple(1,
                                             tuple_data.data(),
                                             tuple_data.size() + sizeof(TupleHeader),
                                             &page_id, &item_id, &ctx));
    
    db_->sync(&ctx);
    
    // Close database
    db_.reset();
    
    // Corrupt data but not checksum
    std::fstream file(test_db_, std::ios::binary | std::ios::in | std::ios::out);
    ASSERT_TRUE(file.is_open());
    
    // Corrupt some data in the middle of the page
    file.seekp(page_id * 8192 + 1000);
    char corruption[] = "CORRUPTED_DATA";
    file.write(corruption, sizeof(corruption));
    file.close();
    
    // Try to reopen and read
    db_ = std::make_unique<Database>();
    Status open_status = db_->open(test_db_, &ctx);
    
    // Database might detect on open or on page read
    if (open_status == Status::Ok) {
        // Try to read the page
        uint8_t* buffer = new uint8_t[8192];
        Status read_status = db_->read_page(page_id, buffer, &ctx);
        
        // Should detect checksum mismatch
        EXPECT_EQ(Status::PageCorrupt, read_status) 
            << "Should detect checksum mismatch when reading corrupted page";
        
        delete[] buffer;
    } else {
        EXPECT_EQ(Status::PageCorrupt, open_status)
            << "Should detect corruption during database open";
    }
}