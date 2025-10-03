#include <gtest/gtest.h>
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/compressed_page_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include <cstring>
#include <vector>
#include <memory>

using namespace scratchbird::core;

class HeapToastIntegrationTest : public ::testing::Test
{
protected:
    static constexpr const char *TEST_DB = "/tmp/test_heap_toast_integration.db";
    static constexpr uint32_t PAGE_SIZE = 8192;
    static constexpr uint64_t TEST_XMIN = 100;
    static constexpr uint64_t TEST_XMAX = 200;

    std::unique_ptr<Database> db;
    std::unique_ptr<BufferPool> buffer_pool;
    std::unique_ptr<PageManager> page_manager;
    std::unique_ptr<StorageEngine> storage_engine;
    std::unique_ptr<CatalogManager> catalog_manager;
    std::unique_ptr<ToastManager> toast_manager;
    std::vector<uint8_t> page_buffer;
    ID test_table_id_;

    void SetUp() override
    {
        // Remove test database if it exists
        std::remove(TEST_DB);

        // Create database
        ErrorContext error_ctx;
        ASSERT_EQ(Database::create(TEST_DB, PAGE_SIZE, &error_ctx), Status::OK);

        // Open database
        db = std::make_unique<Database>();
        ASSERT_EQ(db->open(TEST_DB, &error_ctx), Status::OK);

        // Create buffer pool
        BufferPool::Config bp_config;
        bp_config.pool_size = 10;
        bp_config.page_size = PAGE_SIZE;
        buffer_pool = std::make_unique<BufferPool>(db.get(), bp_config);

        // Create page manager
        page_manager = std::make_unique<PageManager>(db.get(), PAGE_SIZE);
        ASSERT_EQ(page_manager->initialize(&error_ctx), Status::OK);

        // Create storage engine
        storage_engine = std::make_unique<StorageEngine>(db.get());

        // Create catalog manager
        catalog_manager = std::make_unique<CatalogManager>(db.get());
        ASSERT_EQ(catalog_manager->initialize(&error_ctx), Status::OK);

        // Create default schema and table for TOAST manager
        ID schema_id;
        ASSERT_EQ(catalog_manager->createSchema("public", "test", schema_id, &error_ctx),
                  Status::OK);

        std::vector<CatalogManager::ColumnInfo> columns;
        CatalogManager::ColumnInfo col1;
        col1.column_name = "id";
        col1.data_type = static_cast<uint16_t>(DataType::INT);
        col1.max_length = 4;
        col1.nullable = false;
        col1.has_default = false;
        columns.push_back(col1);

        CatalogManager::ColumnInfo col2;
        col2.column_name = "data";
        col2.data_type = static_cast<uint16_t>(DataType::BYTEA);
        col2.max_length = 0;
        col2.nullable = true;
        col2.has_default = false;
        columns.push_back(col2);
        ASSERT_EQ(catalog_manager->createTable(schema_id, "test_table", columns, test_table_id_,
                                                &error_ctx),
                  Status::OK);

        // Create TOAST manager
        toast_manager = std::make_unique<ToastManager>(db.get(), test_table_id_);

        // Allocate page buffer
        page_buffer.resize(PAGE_SIZE);
    }

    void TearDown() override
    {
        toast_manager.reset();
        catalog_manager.reset();
        storage_engine.reset();
        page_manager.reset();
        buffer_pool.reset();
        db.reset();
        std::remove(TEST_DB);
    }

    // Helper to create test data
    std::vector<uint8_t> create_test_data(uint32_t size)
    {
        std::vector<uint8_t> data(size);
        for (uint32_t i = 0; i < size; i++)
        {
            data[i] = static_cast<uint8_t>((i * 7 + 13) % 256);
        }
        return data;
    }
};

TEST_F(HeapToastIntegrationTest, BasicToastInsertAndRetrieve)
{
    ErrorContext error_ctx;

    // Create a heap page with TOAST support
    HeapPage heap_page(page_buffer.data(), PAGE_SIZE, toast_manager.get(), db.get(),
                       test_table_id_);
    ASSERT_EQ(heap_page.initialize(1, &error_ctx), Status::OK);

    // Create a large tuple that should trigger TOAST
    uint32_t data_size = 3000; // > TOAST_TUPLE_THRESHOLD
    std::vector<uint8_t> large_data = create_test_data(data_size);

    // Insert the tuple (should automatically TOAST)
    uint16_t item_id;
    ASSERT_EQ(heap_page.insertTuple(large_data.data(), data_size + sizeof(TupleHeader), TEST_XMIN,
                                     &item_id, &error_ctx),
              Status::OK);

    // Get the raw tuple (should contain TOAST pointer)
    const uint8_t *raw_data;
    uint32_t raw_size;
    ASSERT_EQ(heap_page.getTuple(item_id, &raw_data, &raw_size, &error_ctx), Status::OK);

    // Verify the raw tuple contains a TOAST pointer
    EXPECT_EQ(raw_size, sizeof(TupleHeader) + sizeof(ToastPointer));
    EXPECT_TRUE(isToastPointer(raw_data + sizeof(TupleHeader)));

    // Get the detoasted tuple
    std::vector<uint8_t> detoasted_buffer;
    ASSERT_EQ(heap_page.getTupleDetoasted(item_id, &detoasted_buffer, TEST_XMIN, &error_ctx),
              Status::OK);

    // Verify the detoasted data matches original
    EXPECT_EQ(detoasted_buffer.size(), data_size + sizeof(TupleHeader));
    EXPECT_EQ(memcmp(detoasted_buffer.data() + sizeof(TupleHeader), large_data.data(), data_size),
              0);
}

TEST_F(HeapToastIntegrationTest, SmallTupleNoToast)
{
    ErrorContext error_ctx;

    // Create a heap page with TOAST support
    HeapPage heap_page(page_buffer.data(), PAGE_SIZE, toast_manager.get(), db.get(),
                       test_table_id_);
    ASSERT_EQ(heap_page.initialize(1, &error_ctx), Status::OK);

    // Create a small tuple that should NOT trigger TOAST
    uint32_t data_size = 100; // < TOAST_TUPLE_THRESHOLD
    std::vector<uint8_t> small_data = create_test_data(data_size);

    // Insert the tuple (should NOT TOAST)
    uint16_t item_id;
    ASSERT_EQ(heap_page.insertTuple(small_data.data(), data_size + sizeof(TupleHeader), TEST_XMIN,
                                     &item_id, &error_ctx),
              Status::OK);

    // Get the raw tuple (should contain actual data, not TOAST pointer)
    const uint8_t *raw_data;
    uint32_t raw_size;
    ASSERT_EQ(heap_page.getTuple(item_id, &raw_data, &raw_size, &error_ctx), Status::OK);

    // Verify the raw tuple contains actual data
    EXPECT_EQ(raw_size, data_size + sizeof(TupleHeader));
    EXPECT_FALSE(isToastPointer(raw_data + sizeof(TupleHeader)));

    // Get the detoasted tuple (should be same as raw)
    std::vector<uint8_t> detoasted_buffer;
    ASSERT_EQ(heap_page.getTupleDetoasted(item_id, &detoasted_buffer, TEST_XMIN, &error_ctx),
              Status::OK);

    // Verify the data matches original
    EXPECT_EQ(detoasted_buffer.size(), data_size + sizeof(TupleHeader));
    EXPECT_EQ(memcmp(detoasted_buffer.data() + sizeof(TupleHeader), small_data.data(), data_size),
              0);
}

TEST_F(HeapToastIntegrationTest, ToastDeleteCleansUp)
{
    ErrorContext error_ctx;

    // Create a heap page with TOAST support
    HeapPage heap_page(page_buffer.data(), PAGE_SIZE, toast_manager.get(), db.get(),
                       test_table_id_);
    ASSERT_EQ(heap_page.initialize(1, &error_ctx), Status::OK);

    // Create and insert a large tuple
    uint32_t data_size = 3000;
    std::vector<uint8_t> large_data = create_test_data(data_size);
    uint16_t item_id;
    ASSERT_EQ(heap_page.insertTuple(large_data.data(), data_size + sizeof(TupleHeader), TEST_XMIN,
                                     &item_id, &error_ctx),
              Status::OK);

    // Get the TOAST pointer for verification
    const uint8_t *raw_data;
    uint32_t raw_size;
    ASSERT_EQ(heap_page.getTuple(item_id, &raw_data, &raw_size, &error_ctx), Status::OK);
    ASSERT_TRUE(isToastPointer(raw_data + sizeof(TupleHeader)));

    const ToastPointer *toast_ptr =
        reinterpret_cast<const ToastPointer *>(raw_data + sizeof(TupleHeader));
    uint32_t value_id = toast_ptr->va_valueid;

    // Delete the tuple (should also delete TOAST data)
    ASSERT_EQ(heap_page.deleteTuple(item_id, TEST_XMAX, &error_ctx), Status::OK);

    // Verify tuple is deleted
    EXPECT_EQ(heap_page.getTuple(item_id, &raw_data, &raw_size, &error_ctx), Status::NOT_FOUND);

    // Try to detoast the deleted value (should fail)
    std::vector<uint8_t> detoasted_data;
    EXPECT_NE(toast_manager->detoastValue(toast_ptr, &detoasted_data, TEST_XMIN, &error_ctx),
              Status::OK);
}

TEST_F(HeapToastIntegrationTest, MultipleToastedTuples)
{
    ErrorContext error_ctx;

    // Create a heap page with TOAST support
    HeapPage heap_page(page_buffer.data(), PAGE_SIZE, toast_manager.get(), db.get(),
                       test_table_id_);
    ASSERT_EQ(heap_page.initialize(1, &error_ctx), Status::OK);

    // Insert multiple large tuples
    std::vector<uint16_t> item_ids;
    std::vector<std::vector<uint8_t>> test_data;

    for (int i = 0; i < 5; i++)
    {
        uint32_t data_size = 2500 + i * 100; // Varying sizes, all > TOAST threshold
        test_data.push_back(create_test_data(data_size));

        uint16_t item_id;
        ASSERT_EQ(heap_page.insertTuple(test_data[i].data(), data_size + sizeof(TupleHeader),
                                         TEST_XMIN + i, &item_id, &error_ctx),
                  Status::OK);
        item_ids.push_back(item_id);
    }

    // Verify all tuples can be detoasted correctly
    for (size_t i = 0; i < item_ids.size(); i++)
    {
        std::vector<uint8_t> detoasted_buffer;
        ASSERT_EQ(heap_page.getTupleDetoasted(item_ids[i], &detoasted_buffer, TEST_XMIN + i,
                                                &error_ctx),
                  Status::OK);

        uint32_t expected_size = test_data[i].size() + sizeof(TupleHeader);
        EXPECT_EQ(detoasted_buffer.size(), expected_size);
        EXPECT_EQ(memcmp(detoasted_buffer.data() + sizeof(TupleHeader), test_data[i].data(),
                         test_data[i].size()),
                  0);
    }
}

TEST_F(HeapToastIntegrationTest, HeapPageWithoutToastManager)
{
    ErrorContext error_ctx;

    // Create a heap page WITHOUT TOAST support
    HeapPage heap_page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(heap_page.initialize(1, &error_ctx), Status::OK);

    // Try to insert a large tuple (should fail due to page size limit)
    uint32_t data_size = PAGE_SIZE - sizeof(PageHeader) - sizeof(HeapPageSpecial) - 100;
    std::vector<uint8_t> large_data = create_test_data(data_size);

    uint16_t item_id;
    Status s = heap_page.insertTuple(large_data.data(), data_size + sizeof(TupleHeader), TEST_XMIN,
                                      &item_id, &error_ctx);

    // Without TOAST manager, large tuples that exceed page capacity should fail
    if (data_size + sizeof(TupleHeader) + sizeof(ItemPointer) >
        PAGE_SIZE - sizeof(PageHeader) - sizeof(HeapPageSpecial))
    {
        EXPECT_EQ(s, Status::PAGE_FULL);
    }
}

TEST_F(HeapToastIntegrationTest, CompressedToastIntegration)
{
    ErrorContext error_ctx;

    // Create a heap page with TOAST support
    HeapPage heap_page(page_buffer.data(), PAGE_SIZE, toast_manager.get(), db.get(),
                       test_table_id_);
    ASSERT_EQ(heap_page.initialize(1, &error_ctx), Status::OK);

    // Create highly compressible data
    uint32_t data_size = 4000;
    std::vector<uint8_t> compressible_data(data_size, 'A'); // All same character

    // Insert the tuple (should TOAST with compression)
    uint16_t item_id;
    ASSERT_EQ(heap_page.insertTuple(compressible_data.data(), data_size + sizeof(TupleHeader),
                                     TEST_XMIN, &item_id, &error_ctx),
              Status::OK);

    // Get the raw tuple
    const uint8_t *raw_data;
    uint32_t raw_size;
    ASSERT_EQ(heap_page.getTuple(item_id, &raw_data, &raw_size, &error_ctx), Status::OK);

    // Verify it's a TOAST pointer
    ASSERT_TRUE(isToastPointer(raw_data + sizeof(TupleHeader)));
    const ToastPointer *toast_ptr =
        reinterpret_cast<const ToastPointer *>(raw_data + sizeof(TupleHeader));

    // Check that compression was applied (external size should be less than raw size)
    EXPECT_LT(toast_ptr->va_extsize, toast_ptr->va_rawsize);

    // Detoast and verify
    std::vector<uint8_t> detoasted_buffer;
    ASSERT_EQ(heap_page.getTupleDetoasted(item_id, &detoasted_buffer, TEST_XMIN, &error_ctx),
              Status::OK);

    EXPECT_EQ(detoasted_buffer.size(), data_size + sizeof(TupleHeader));
    EXPECT_EQ(
        memcmp(detoasted_buffer.data() + sizeof(TupleHeader), compressible_data.data(), data_size),
        0);
}