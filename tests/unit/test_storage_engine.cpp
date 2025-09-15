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
    
    Status create_test_table(ID& table_id_out) {
        ErrorContext ctx;
        
        // Create a schema first
        ID schema_id;
        Status status = db_->catalog_manager()->create_schema("test", "root", schema_id, &ctx);
        if (status != Status::Ok) {
            return status;
        }
        
        // Define columns
        std::vector<CatalogManager::ColumnInfo> columns;
        columns.push_back({{}, 0, "id", static_cast<uint16_t>(DataType::Int32), 4, false, false, ""});
        columns.push_back({{}, 1, "value", static_cast<uint16_t>(DataType::Varchar), 100, true, false, ""});
        
        // Create table
        return db_->catalog_manager()->create_table(schema_id, "test_table", columns, table_id_out, &ctx);
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

TEST_F(StorageEngineTest, InsertAndGetTuple) {
    CreateAndOpenDatabase();
    ErrorContext ctx;
    
    ID table_id;
    ASSERT_EQ(Status::Ok, create_test_table(table_id));
    
    StorageEngine* engine = db_->storage_engine();
    
    // Test data
    struct TestData {
        int32_t id;
        char value[100];
    };
    TestData test_data = {1, "Hello World"};
    
    uint32_t page_id;
    uint16_t item_id;
    Status status = engine->insert_tuple(table_id, reinterpret_cast<uint8_t*>(&test_data),
                                        sizeof(TestData), &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::Ok);
    
    // Get tuple back
    Tuple retrieved_tuple;
    status = engine->get_tuple(page_id, item_id, &retrieved_tuple, &ctx);
    ASSERT_EQ(status, Status::Ok);
    
    TestData* retrieved_data = reinterpret_cast<TestData*>(const_cast<uint8_t*>(retrieved_tuple.data));
    EXPECT_EQ(retrieved_data->id, test_data.id);
    EXPECT_STREQ(retrieved_data->value, test_data.value);
}

TEST_F(StorageEngineTest, DeleteTuple) {
    CreateAndOpenDatabase();
    ErrorContext ctx;
    
    ID table_id;
    ASSERT_EQ(Status::Ok, create_test_table(table_id));
    
    StorageEngine* engine = db_->storage_engine();
    
    // Insert a tuple
    struct TestData { int32_t id; char value[100]; };
    TestData test_data = {1, "To be deleted"};
    uint32_t page_id;
    uint16_t item_id;
    ASSERT_EQ(engine->insert_tuple(table_id, reinterpret_cast<uint8_t*>(&test_data),
                                  sizeof(TestData), &page_id, &item_id, &ctx), Status::Ok);
    
    // Delete the tuple
    ASSERT_EQ(engine->delete_tuple(page_id, item_id, &ctx), Status::Ok);
    
    // Try to get the deleted tuple (should fail)
    Tuple retrieved_tuple;
    EXPECT_EQ(engine->get_tuple(page_id, item_id, &retrieved_tuple, &ctx), Status::NotFound);
}

TEST_F(StorageEngineTest, SequentialScan) {
    CreateAndOpenDatabase();
    ErrorContext ctx;
    
    ID table_id;
    ASSERT_EQ(Status::Ok, create_test_table(table_id));
    
    StorageEngine* engine = db_->storage_engine();
    
    // Insert multiple tuples
    struct TestData { int32_t id; char value[100]; };
    std::vector<TestData> test_data = {
        {1, "Tuple 1"},
        {2, "Tuple 2"},
        {3, "Tuple 3"}
    };
    
    for (const auto& data : test_data) {
        uint32_t page_id;
        uint16_t item_id;
        ASSERT_EQ(engine->insert_tuple(table_id, reinterpret_cast<uint8_t*>(const_cast<TestData*>(&data)),
                                      sizeof(TestData), &page_id, &item_id, &ctx), Status::Ok);
    }
    
    // Create a scan
    auto scanner = engine->create_scan(table_id, &ctx);
    ASSERT_NE(scanner, nullptr);
    
    // Iterate through tuples
    Tuple tuple;
    int count = 0;
    while (scanner->next(&tuple, &ctx) == Status::Ok) {
        TestData* retrieved_data = reinterpret_cast<TestData*>(const_cast<uint8_t*>(tuple.data));
        EXPECT_EQ(retrieved_data->id, test_data[count].id);
        EXPECT_STREQ(retrieved_data->value, test_data[count].value);
        count++;
    }
    EXPECT_EQ(count, test_data.size());
}

TEST_F(StorageEngineTest, Visibility) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    ID table_id;
    ASSERT_EQ(Status::Ok, create_test_table(table_id));

    StorageEngine* engine = db_->storage_engine();
    TransactionManager* tm = db_->transaction_manager();
    
    // Insert a tuple with xmin = 100 (committed)
    struct TestData { int32_t id; };
    TestData data1 = {1};
    uint32_t page_id1;
    uint16_t item_id1;
    ASSERT_EQ(engine->insert_tuple(table_id, reinterpret_cast<uint8_t*>(&data1),
                                  sizeof(TestData), &page_id1, &item_id1, &ctx), Status::Ok);
    
    // Simulate transaction 100 committing
    tm->commit_transaction(100, &ctx);
    
    // Insert a tuple with xmin = 101 (active)
    TestData data2 = {2};
    uint32_t page_id2;
    uint16_t item_id2;
    ASSERT_EQ(engine->insert_tuple(table_id, reinterpret_cast<uint8_t*>(&data2),
                                  sizeof(TestData), &page_id2, &item_id2, &ctx), Status::Ok);
    
    // Get current XID (should be 102)
    uint64_t current_xid = tm->get_current_xid();
    EXPECT_EQ(current_xid, 102);
    
    // Tuple 1 (xmin=100, committed) should be visible
    Tuple tuple1;
    EXPECT_EQ(engine->get_tuple(page_id1, item_id1, &tuple1, &ctx), Status::Ok);
    
    // Tuple 2 (xmin=101, active) should NOT be visible
    Tuple tuple2;
    EXPECT_EQ(engine->get_tuple(page_id2, item_id2, &tuple2, &ctx), Status::NotFound);
}

TEST_F(StorageEngineTest, PageFull) {
    ErrorContext ctx;
    
    // Create and open database with small pages
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    // Create test table
    ID table_id;
    ASSERT_EQ(Status::Ok, create_test_table(table_id));
    
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
        
        ASSERT_EQ(sizeof(LargeTuple) + sizeof(TupleHeader), retrieved.data_size);
        LargeTuple* data = reinterpret_cast<LargeTuple*>(
            const_cast<uint8_t*>(retrieved.data) + sizeof(TupleHeader));
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
    ID table_id;
    ASSERT_EQ(Status::Ok, create_test_table(table_id));
    
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
    
    TestTuple* data = reinterpret_cast<TestTuple*>(
        const_cast<uint8_t*>(retrieved.data) + sizeof(TupleHeader));
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
    ID table_id = generate_uuid_v7();
    ASSERT_EQ(Status::Ok, engine.insert_tuple(table_id, 
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
        auto iterator = engine2.create_scan(table_id, &ctx);
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
    ID table_id = generate_uuid_v7();
    ASSERT_EQ(Status::Ok, engine.insert_tuple(table_id,
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
    ID table_id = generate_uuid_v7();
    ASSERT_EQ(Status::Ok, engine.insert_tuple(table_id,
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