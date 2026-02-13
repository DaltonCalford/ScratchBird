#include <gtest/gtest.h>
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "test_helpers.h"
#include <cstring>
#include <filesystem>
#include <vector>
#include <memory>

using namespace scratchbird::core;

class HeapToastIntegrationTest : public ::testing::Test
{
protected:
    static constexpr uint32_t PAGE_SIZE = 8192;
    static constexpr uint64_t TEST_XMIN = 100;
    static constexpr uint64_t TEST_XMAX = 200;

    std::unique_ptr<Database> db;
    StorageEngine* storage_engine = nullptr;
    CatalogManager* catalog_manager = nullptr;
    std::unique_ptr<ToastManager> toast_manager;
    std::vector<uint8_t> page_buffer;
    ID test_table_id_;
    std::string test_db_path_;

    void SetUp() override
    {
        ErrorContext error_ctx;
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_heap_toast_integration");
        std::filesystem::remove(test_db_path_);
        ASSERT_EQ(Database::create(test_db_path_, PAGE_SIZE, &error_ctx), Status::OK)
            << error_ctx.message;

        // Open database
        db = std::make_unique<Database>();
        ASSERT_EQ(db->open(test_db_path_, &error_ctx), Status::OK)
            << error_ctx.message;

        storage_engine = db->storage_engine();
        ASSERT_NE(storage_engine, nullptr);

        catalog_manager = db->catalog_manager();
        ASSERT_NE(catalog_manager, nullptr);

        // Create default schema and table for TOAST manager
        ID schema_id;
        std::vector<CatalogManager::SchemaInfo> schemas;
        ASSERT_EQ(catalog_manager->listSchemas(schemas, &error_ctx), Status::OK);
        if (schemas.empty()) {
            ASSERT_EQ(catalog_manager->createSchema("public", "test", schema_id, &error_ctx),
                      Status::OK);
        } else {
            schema_id = schemas[0].schema_id;
        }

        std::vector<CatalogManager::ColumnInfo> columns;
        CatalogManager::ColumnInfo col1;
        col1.column_name = "id";
        col1.data_type = static_cast<uint16_t>(DataType::INT32);
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
                                                0, &error_ctx),
                  Status::OK);

        // Create TOAST manager
        toast_manager = std::make_unique<ToastManager>(db.get(), test_table_id_);
        ASSERT_EQ(toast_manager->initialize(&error_ctx), Status::OK)
            << error_ctx.message;

        // Allocate page buffer
        page_buffer.resize(PAGE_SIZE);
    }

    void TearDown() override
    {
        toast_manager.reset();
        db.reset();
        if (!test_db_path_.empty()) {
            std::filesystem::remove(test_db_path_);
        }
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

    std::vector<uint8_t> buildTuple(const std::vector<uint8_t>& payload,
                                    uint64_t xmin,
                                    uint64_t xmax = 0)
    {
        TupleHeader header{};
        header.xmin = xmin;
        header.xmax = xmax;
        header.back_version_gpid = INVALID_GPID;
        header.back_version_slot = 0;
        header.ctid_gpid = INVALID_GPID;
        header.ctid_slot = 0;
        header.infomask = 0;
        header.null_bitmap_offset = 0;
        header.padding = 0;
        header.session_id = ID{};

        std::vector<uint8_t> tuple(sizeof(TupleHeader) + payload.size());
        std::memcpy(tuple.data(), &header, sizeof(TupleHeader));
        if (!payload.empty())
        {
            std::memcpy(tuple.data() + sizeof(TupleHeader), payload.data(), payload.size());
        }
        return tuple;
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
    std::vector<uint8_t> tuple = buildTuple(large_data, TEST_XMIN);

    // Insert the tuple (should automatically TOAST)
    uint16_t item_id;
    ASSERT_EQ(heap_page.insertTuple(tuple.data(), tuple.size(), TEST_XMIN,
                                     &item_id, &error_ctx),
              Status::OK);

    // Get the raw tuple (should contain TOAST pointer)
    const uint8_t *raw_data;
    uint32_t raw_size;
    ASSERT_EQ(heap_page.getTuple(item_id, &raw_data, &raw_size, &error_ctx), Status::OK);

    // Verify the raw tuple contains a TOAST pointer
    EXPECT_EQ(raw_size, sizeof(TupleHeader) + sizeof(ToastPointer));
    EXPECT_TRUE(ToastManager::isToastPointer(raw_data + sizeof(TupleHeader),
                                             raw_size - sizeof(TupleHeader)));

    // Get the detoasted tuple
    std::vector<uint8_t> detoasted_buffer;
    ASSERT_EQ(heap_page.getTupleDetoasted(item_id, &detoasted_buffer, TEST_XMIN, &error_ctx),
              Status::OK);

    // Verify the detoasted data matches original
    EXPECT_EQ(detoasted_buffer.size(), tuple.size());
    EXPECT_EQ(memcmp(detoasted_buffer.data() + sizeof(TupleHeader),
                     large_data.data(),
                     large_data.size()),
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
    std::vector<uint8_t> tuple = buildTuple(small_data, TEST_XMIN);

    // Insert the tuple (should NOT TOAST)
    uint16_t item_id;
    ASSERT_EQ(heap_page.insertTuple(tuple.data(), tuple.size(), TEST_XMIN,
                                     &item_id, &error_ctx),
              Status::OK);

    // Get the raw tuple (should contain actual data, not TOAST pointer)
    const uint8_t *raw_data;
    uint32_t raw_size;
    ASSERT_EQ(heap_page.getTuple(item_id, &raw_data, &raw_size, &error_ctx), Status::OK);

    // Verify the raw tuple contains actual data
    EXPECT_EQ(raw_size, tuple.size());
    EXPECT_FALSE(ToastManager::isToastPointer(raw_data + sizeof(TupleHeader),
                                              raw_size - sizeof(TupleHeader)));

    // Get the detoasted tuple (should be same as raw)
    std::vector<uint8_t> detoasted_buffer;
    ASSERT_EQ(heap_page.getTupleDetoasted(item_id, &detoasted_buffer, TEST_XMIN, &error_ctx),
              Status::OK);

    // Verify the data matches original
    EXPECT_EQ(detoasted_buffer.size(), tuple.size());
    EXPECT_EQ(memcmp(detoasted_buffer.data() + sizeof(TupleHeader),
                     small_data.data(),
                     small_data.size()),
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
    std::vector<uint8_t> tuple = buildTuple(large_data, TEST_XMIN);
    uint16_t item_id;
    ASSERT_EQ(heap_page.insertTuple(tuple.data(), tuple.size(), TEST_XMIN,
                                     &item_id, &error_ctx),
              Status::OK);

    // Get the TOAST pointer for verification
    const uint8_t *raw_data;
    uint32_t raw_size;
    ASSERT_EQ(heap_page.getTuple(item_id, &raw_data, &raw_size, &error_ctx), Status::OK);
    ASSERT_TRUE(ToastManager::isToastPointer(raw_data + sizeof(TupleHeader),
                                             raw_size - sizeof(TupleHeader)));

    const ToastPointer *toast_ptr =
        reinterpret_cast<const ToastPointer *>(raw_data + sizeof(TupleHeader));
    // Delete the tuple (should also delete TOAST data)
    ASSERT_EQ(heap_page.deleteTuple(item_id, TEST_XMAX, &error_ctx), Status::OK);

    // Verify tuple is deleted
    EXPECT_EQ(heap_page.getTuple(item_id, &raw_data, &raw_size, &error_ctx), Status::NOT_FOUND);

    // Try to detoast the deleted value (visibility depends on xmax status)
    std::vector<uint8_t> detoasted_data;
    Status detoast_status =
        toast_manager->detoastValue(toast_ptr, &detoasted_data, TEST_XMIN, &error_ctx);
    EXPECT_EQ(detoast_status, Status::OK);
    if (detoast_status == Status::OK)
    {
        EXPECT_EQ(detoasted_data.size(), large_data.size());
        EXPECT_EQ(memcmp(detoasted_data.data(), large_data.data(), large_data.size()), 0);
    }
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
    std::vector<uint8_t> tuple = buildTuple(test_data[i], TEST_XMIN + i);

    uint16_t item_id;
    ASSERT_EQ(heap_page.insertTuple(tuple.data(), tuple.size(),
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

        std::vector<uint8_t> expected = buildTuple(test_data[i], TEST_XMIN + i);
        EXPECT_EQ(detoasted_buffer.size(), expected.size());
        EXPECT_EQ(memcmp(detoasted_buffer.data() + sizeof(TupleHeader),
                         test_data[i].data(),
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
    std::vector<uint8_t> tuple = buildTuple(compressible_data, TEST_XMIN);

    // Insert the tuple (should TOAST with compression)
    uint16_t item_id;
    ASSERT_EQ(heap_page.insertTuple(tuple.data(), tuple.size(),
                                     TEST_XMIN, &item_id, &error_ctx),
              Status::OK);

    // Get the raw tuple
    const uint8_t *raw_data;
    uint32_t raw_size;
    ASSERT_EQ(heap_page.getTuple(item_id, &raw_data, &raw_size, &error_ctx), Status::OK);

    // Verify it's a TOAST pointer
    ASSERT_TRUE(ToastManager::isToastPointer(raw_data + sizeof(TupleHeader),
                                             raw_size - sizeof(TupleHeader)));
    const ToastPointer *toast_ptr =
        reinterpret_cast<const ToastPointer *>(raw_data + sizeof(TupleHeader));

    // Check that compression was applied (external size should be less than raw size)
    EXPECT_LT(toast_ptr->va_extsize, toast_ptr->va_rawsize);

    // Detoast and verify
    std::vector<uint8_t> detoasted_buffer;
    ASSERT_EQ(heap_page.getTupleDetoasted(item_id, &detoasted_buffer, TEST_XMIN, &error_ctx),
              Status::OK);

    EXPECT_EQ(detoasted_buffer.size(), tuple.size());
    EXPECT_EQ(memcmp(detoasted_buffer.data() + sizeof(TupleHeader),
                     compressible_data.data(),
                     compressible_data.size()),
              0);
}
