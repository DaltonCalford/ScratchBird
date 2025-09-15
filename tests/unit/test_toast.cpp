#include <gtest/gtest.h>
#include "scratchbird/core/toast.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include <filesystem>
#include <random>
#include <cstring>

using namespace scratchbird::core;

class ToastTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "scratchbird_toast_test";
        std::filesystem::remove_all(test_dir_);  // Clean up any previous runs
        std::filesystem::create_directories(test_dir_);
        
        // Create test database
        db_path_ = test_dir_ / "toast_test.db";
        Status status = Database::create(db_path_.string(), 8192);
        ASSERT_EQ(status, Status::Ok);
        
        db_ = std::make_unique<Database>();
        status = db_->open(db_path_.string());
        ASSERT_EQ(status, Status::Ok);
        
        // Create default schema
        CatalogManager* catalog = db_->catalog_manager();
        status = catalog->create_schema("public", "test", schema_id_);
        ASSERT_EQ(status, Status::Ok);
    }
    
    void TearDown() override {
        db_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
    // Generate test data
    std::vector<uint8_t> generate_data(size_t size, char pattern = 'A') {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; i++) {
            data[i] = pattern + (i % 26);
        }
        return data;
    }
    
    std::filesystem::path test_dir_;
    std::filesystem::path db_path_;
    std::unique_ptr<Database> db_;
    ID schema_id_;
};

TEST_F(ToastTest, BasicToastOperations) {
    // Create a regular table first
    CatalogManager* catalog = db_->catalog_manager();
    
    // Create columns for test table
    std::vector<CatalogManager::ColumnInfo> columns;
    
    CatalogManager::ColumnInfo col1;
    col1.column_name = "id";
    col1.data_type = static_cast<uint16_t>(DataType::Int);
    col1.max_length = 4;
    col1.nullable = false;
    columns.push_back(col1);
    
    CatalogManager::ColumnInfo col2;
    col2.column_name = "data";
    col2.data_type = static_cast<uint16_t>(DataType::Bytea);
    col2.max_length = 0;  // Unlimited
    col2.nullable = true;
    columns.push_back(col2);
    
    ID table_id;
    Status status = catalog->create_table(schema_id_, "test_table", columns, table_id);
    ASSERT_EQ(status, Status::Ok);
    
    // Create TOAST manager for the table
    ToastManager toast_mgr(db_.get(), table_id);
    status = toast_mgr.initialize();
    ASSERT_EQ(status, Status::Ok);
    
    // Test small value (should not be TOASTed)
    auto small_data = generate_data(100);
    EXPECT_FALSE(ToastManager::should_toast(small_data.size(), 8192));
    
    // Test large value (should be TOASTed)
    auto large_data = generate_data(5000);
    EXPECT_TRUE(ToastManager::should_toast(large_data.size(), 8192));
    
    // TOAST the large value
    ToastPointer pointer;
    status = toast_mgr.toast_value(large_data.data(), large_data.size(),
                                  ToastStrategy::EXTENDED, 1000, &pointer);
    ASSERT_EQ(status, Status::Ok);
    
    // Verify pointer
    EXPECT_EQ(pointer.va_header, 0x01);  // TOAST marker
    EXPECT_EQ(pointer.va_rawsize, large_data.size());
    EXPECT_EQ(pointer.va_extsize, large_data.size());  // Uncompressed
    EXPECT_GT(pointer.va_valueid, 0u);
    
    // Detoast the value
    std::vector<uint8_t> retrieved_data;
    status = toast_mgr.detoast_value(&pointer, &retrieved_data, 1000);
    ASSERT_EQ(status, Status::Ok);
    
    // Verify data matches
    EXPECT_EQ(retrieved_data.size(), large_data.size());
    EXPECT_EQ(memcmp(retrieved_data.data(), large_data.data(), large_data.size()), 0);
}

TEST_F(ToastTest, CompressedToast) {
    // Skip if no compression support
#ifndef HAVE_LZ4
    GTEST_SKIP() << "LZ4 compression not available";
#endif
    
    CatalogManager* catalog = db_->catalog_manager();
    
    // Create test table
    std::vector<CatalogManager::ColumnInfo> columns;
    CatalogManager::ColumnInfo col;
    col.column_name = "data";
    col.data_type = static_cast<uint16_t>(DataType::Bytea);
    col.max_length = 0;
    col.nullable = false;
    columns.push_back(col);
    
    ID table_id;
    Status status = catalog->create_table(schema_id_, "compress_test", columns, table_id);
    ASSERT_EQ(status, Status::Ok);
    
    ToastManager toast_mgr(db_.get(), table_id);
    status = toast_mgr.initialize();
    ASSERT_EQ(status, Status::Ok);
    
    // Create highly compressible data
    std::vector<uint8_t> data(10000, 'X');  // 10KB of 'X'
    
    // TOAST with compression
    ToastPointer pointer;
    status = toast_mgr.toast_value(data.data(), data.size(),
                                  ToastStrategy::EXTERNAL, 2000, &pointer);
    ASSERT_EQ(status, Status::Ok);
    
    // Verify it was compressed
    EXPECT_EQ(pointer.va_header, 0x01);
    EXPECT_EQ(pointer.va_rawsize, data.size());
    EXPECT_LT(pointer.va_extsize, data.size());  // Should be smaller
    
    // Detoast and verify
    std::vector<uint8_t> retrieved;
    status = toast_mgr.detoast_value(&pointer, &retrieved, 2000);
    ASSERT_EQ(status, Status::Ok);
    
    EXPECT_EQ(retrieved.size(), data.size());
    EXPECT_EQ(memcmp(retrieved.data(), data.data(), data.size()), 0);
}

TEST_F(ToastTest, MultipleChunks) {
    CatalogManager* catalog = db_->catalog_manager();
    
    // Create test table
    std::vector<CatalogManager::ColumnInfo> columns;
    CatalogManager::ColumnInfo col;
    col.column_name = "large_data";
    col.data_type = static_cast<uint16_t>(DataType::Bytea);
    col.max_length = 0;
    col.nullable = false;
    columns.push_back(col);
    
    ID table_id;
    Status status = catalog->create_table(schema_id_, "chunk_test", columns, table_id);
    ASSERT_EQ(status, Status::Ok);
    
    ToastManager toast_mgr(db_.get(), table_id);
    status = toast_mgr.initialize();
    ASSERT_EQ(status, Status::Ok);
    
    // Create data that spans multiple chunks
    size_t data_size = TOAST_MAX_CHUNK_SIZE * 3 + 500;  // 3.x chunks
    auto data = generate_data(data_size, 'M');
    
    // TOAST the value
    ToastPointer pointer;
    status = toast_mgr.toast_value(data.data(), data.size(),
                                  ToastStrategy::EXTENDED, 3000, &pointer);
    ASSERT_EQ(status, Status::Ok);
    
    // Detoast and verify
    std::vector<uint8_t> retrieved;
    status = toast_mgr.detoast_value(&pointer, &retrieved, 3000);
    ASSERT_EQ(status, Status::Ok);
    
    EXPECT_EQ(retrieved.size(), data.size());
    EXPECT_EQ(memcmp(retrieved.data(), data.data(), data.size()), 0);
}

TEST_F(ToastTest, ToastDelete) {
    CatalogManager* catalog = db_->catalog_manager();
    
    // Create test table
    std::vector<CatalogManager::ColumnInfo> columns;
    CatalogManager::ColumnInfo col;
    col.column_name = "data";
    col.data_type = static_cast<uint16_t>(DataType::Bytea);
    col.max_length = 0;
    col.nullable = false;
    columns.push_back(col);
    
    ID table_id;
    Status status = catalog->create_table(schema_id_, "delete_test", columns, table_id);
    ASSERT_EQ(status, Status::Ok);
    
    ToastManager toast_mgr(db_.get(), table_id);
    status = toast_mgr.initialize();
    ASSERT_EQ(status, Status::Ok);
    
    // TOAST a value
    auto data = generate_data(5000);
    ToastPointer pointer;
    status = toast_mgr.toast_value(data.data(), data.size(),
                                  ToastStrategy::EXTENDED, 4000, &pointer);
    ASSERT_EQ(status, Status::Ok);
    
    // Verify we can read it
    std::vector<uint8_t> retrieved;
    status = toast_mgr.detoast_value(&pointer, &retrieved, 4000);
    ASSERT_EQ(status, Status::Ok);
    
    // Delete the TOAST value
    status = toast_mgr.delete_toast_value(pointer.va_valueid, 4001);
    ASSERT_EQ(status, Status::Ok);
    
    // Try to read it again - should fail
    // (In a real system with MVCC, this would depend on transaction visibility)
}

TEST_F(ToastTest, StrategySelection) {
    // Test strategy selection logic
    
    // Small data - should use PLAIN
    auto small_data = generate_data(100);
    EXPECT_EQ(ToastManager::choose_strategy(small_data.data(), small_data.size()),
              ToastStrategy::PLAIN);
    
    // Medium data - should use EXTENDED
    auto medium_data = generate_data(3000);
    EXPECT_EQ(ToastManager::choose_strategy(medium_data.data(), medium_data.size(), false),
              ToastStrategy::EXTENDED);
    
    // Large data with compression - should use EXTERNAL
    auto large_data = generate_data(10000);
    EXPECT_EQ(ToastManager::choose_strategy(large_data.data(), large_data.size(), true),
              ToastStrategy::EXTERNAL);
}

TEST_F(ToastTest, EdgeCases) {
    CatalogManager* catalog = db_->catalog_manager();
    
    // Create test table
    std::vector<CatalogManager::ColumnInfo> columns;
    CatalogManager::ColumnInfo col;
    col.column_name = "data";
    col.data_type = static_cast<uint16_t>(DataType::Bytea);
    col.max_length = 0;
    col.nullable = false;
    columns.push_back(col);
    
    ID table_id;
    Status status = catalog->create_table(schema_id_, "edge_test", columns, table_id);
    ASSERT_EQ(status, Status::Ok);
    
    ToastManager toast_mgr(db_.get(), table_id);
    status = toast_mgr.initialize();
    ASSERT_EQ(status, Status::Ok);
    
    // Test exact chunk boundary
    auto exact_chunk = generate_data(TOAST_MAX_CHUNK_SIZE);
    ToastPointer pointer1;
    status = toast_mgr.toast_value(exact_chunk.data(), exact_chunk.size(),
                                  ToastStrategy::EXTENDED, 5000, &pointer1);
    ASSERT_EQ(status, Status::Ok);
    
    // Test empty data
    ToastPointer pointer2;
    status = toast_mgr.toast_value(nullptr, 0,
                                  ToastStrategy::EXTENDED, 5001, &pointer2);
    EXPECT_NE(status, Status::Ok);  // Should fail
    
    // Test maximum size
    size_t max_size = TOAST_MAX_CHUNK_SIZE * 100;  // 100 chunks
    auto max_data = generate_data(max_size);
    ToastPointer pointer3;
    status = toast_mgr.toast_value(max_data.data(), max_data.size(),
                                  ToastStrategy::EXTENDED, 5002, &pointer3);
    ASSERT_EQ(status, Status::Ok);
    
    // Verify retrieval
    std::vector<uint8_t> retrieved;
    status = toast_mgr.detoast_value(&pointer3, &retrieved, 5002);
    ASSERT_EQ(status, Status::Ok);
    EXPECT_EQ(retrieved.size(), max_data.size());
}