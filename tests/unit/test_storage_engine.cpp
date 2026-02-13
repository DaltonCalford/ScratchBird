#include <gtest/gtest.h>
#include <cstring>
#include <filesystem>
#include <memory>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/types.h"
#include "test_helpers.h"

using namespace scratchbird::core;

class StorageEngineTest : public ::testing::Test
{
protected:
    static constexpr uint32_t kPageSize = 8192;

    void SetUp() override
    {
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_storage_engine", ".db");
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, kPageSize, &ctx), Status::OK)
            << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK)
            << ctx.message;

        Status status = db_->initializeProcArray(16, &ctx);
        if (status != Status::OK && status != Status::INVALID_ARGUMENT)
        {
            ASSERT_EQ(status, Status::OK) << ctx.message;
        }

        ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK)
            << ctx.message;
        ConnectionContext::setCurrent(conn_ctx_.get());
        ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK)
            << ctx.message;

        ID system_user = db_->catalog_manager()->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user, true);

        schema_id_ = resolveDefaultSchema(&ctx);
        ASSERT_NE(schema_id_, ID{});

        engine_ = db_->storage_engine();
        ASSERT_NE(engine_, nullptr);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        db_.reset();
        std::filesystem::remove(test_db_path_);
    }

    ID resolveDefaultSchema(ErrorContext *ctx)
    {
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = db_->catalog_manager()->listSchemas(schemas, ctx);
        if (status == Status::OK && !schemas.empty())
        {
            return schemas.front().schema_id;
        }

        ID schema_id;
        status = db_->catalog_manager()->createSchema("main", "SYSTEM", schema_id, ctx);
        if (status == Status::OK)
        {
            return schema_id;
        }
        return ID{};
    }

    ID createTestTable(const std::string &name)
    {
        ErrorContext ctx;
        std::vector<CatalogManager::ColumnInfo> columns;

        CatalogManager::ColumnInfo id_col;
        id_col.column_name = "id";
        id_col.ordinal = 1;
        id_col.data_type = static_cast<uint16_t>(DataType::INT32);
        id_col.type_precision = 4;
        id_col.nullable = false;
        columns.push_back(id_col);

        CatalogManager::ColumnInfo value_col;
        value_col.column_name = "value";
        value_col.ordinal = 2;
        value_col.data_type = static_cast<uint16_t>(DataType::VARCHAR);
        value_col.type_precision = 100;
        value_col.nullable = true;
        columns.push_back(value_col);

        ID table_id;
        Status status =
            db_->catalog_manager()->createTable(schema_id_, name, columns, table_id, 0, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return table_id;
    }

    std::vector<uint8_t> buildTuple(const void *payload, size_t payload_size,
                                    uint64_t xmin = config::DEFAULT_INITIAL_XID)
    {
        TupleHeader header{};
        header.xmin = xmin;
        header.xmax = 0;
        header.back_version_gpid = INVALID_GPID;
        header.back_version_slot = 0;
        header.ctid_gpid = INVALID_GPID;
        header.ctid_slot = 0;
        header.infomask = 0;
        header.null_bitmap_offset = 0;
        header.padding = 0;
        header.session_id = ID{};

        std::vector<uint8_t> tuple(sizeof(TupleHeader) + payload_size, 0);
        std::memcpy(tuple.data(), &header, sizeof(TupleHeader));
        if (payload_size > 0)
        {
            std::memcpy(tuple.data() + sizeof(TupleHeader), payload, payload_size);
        }
        return tuple;
    }

    std::string test_db_path_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<ConnectionContext> conn_ctx_;
    StorageEngine *engine_ = nullptr;
    ID schema_id_{};
};

TEST_F(StorageEngineTest, HeapPageBasics)
{
    ErrorContext ctx;
    std::vector<uint8_t> page_buffer(kPageSize, 0);

    HeapPage page(page_buffer.data(), kPageSize);
    ASSERT_EQ(page.initialize(100, &ctx), Status::OK);

    EXPECT_EQ(PAGE_TYPE_HEAP, page.header()->page_type);
    EXPECT_EQ(100u, page.header()->page_id);
    EXPECT_EQ(0u, page.getItemCount());

    uint32_t expected_free = kPageSize - sizeof(PageHeader) - sizeof(HeapPageSpecial);
    EXPECT_NEAR(expected_free, page.getFreeSpace(), 8);
}

TEST_F(StorageEngineTest, InsertAndGetTuple)
{
    ID table_id = createTestTable("test_table");

    struct TestData
    {
        int32_t id;
        char value[100];
    } test_data = {1, "Hello World"};

    auto tuple = buildTuple(&test_data, sizeof(TestData));

    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ErrorContext ctx;
    ASSERT_EQ(engine_->insertTuple(table_id, tuple.data(),
                                   static_cast<uint32_t>(tuple.size()),
                                   &page_id, &item_id, &ctx),
              Status::OK) << ctx.message;

    Tuple retrieved{};
    ASSERT_EQ(engine_->getTuple(page_id, item_id, &retrieved, &ctx), Status::OK)
        << ctx.message;

    ASSERT_GE(retrieved.data_size, sizeof(TupleHeader) + sizeof(TestData));
    const uint8_t *payload = retrieved.data + sizeof(TupleHeader);
    const auto *retrieved_data = reinterpret_cast<const TestData *>(payload);
    EXPECT_EQ(retrieved_data->id, test_data.id);
    EXPECT_STREQ(retrieved_data->value, test_data.value);
}

TEST_F(StorageEngineTest, DeleteMarksTupleHeader)
{
    ID table_id = createTestTable("delete_test");

    struct TestData
    {
        int32_t id;
        char value[100];
    } test_data = {1, "To be deleted"};

    auto tuple = buildTuple(&test_data, sizeof(TestData));

    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ErrorContext ctx;
    ASSERT_EQ(engine_->insertTuple(table_id, tuple.data(),
                                   static_cast<uint32_t>(tuple.size()),
                                   &page_id, &item_id, &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(engine_->deleteTuple(table_id, page_id, item_id, UINT16_MAX, &ctx), Status::OK)
        << ctx.message;

    Tuple retrieved{};
    EXPECT_EQ(engine_->getTuple(page_id, item_id, &retrieved, &ctx), Status::NOT_FOUND)
        << "Deleted tuple should not be visible";
}

TEST_F(StorageEngineTest, SequentialScan)
{
    ID table_id = createTestTable("scan_test");

    struct TestData
    {
        int32_t id;
        char value[32];
    };
    std::vector<TestData> rows = {
        {1, "Tuple 1"},
        {2, "Tuple 2"},
        {3, "Tuple 3"},
    };

    ErrorContext ctx;
    for (const auto &row : rows)
    {
        auto tuple = buildTuple(&row, sizeof(TestData));
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTuple(table_id, tuple.data(),
                                       static_cast<uint32_t>(tuple.size()),
                                       &page_id, &item_id, &ctx),
                  Status::OK) << ctx.message;
    }

    auto scanner = engine_->createScan(table_id, &ctx);
    ASSERT_NE(scanner, nullptr);

    Tuple tuple{};
    size_t count = 0;
    while (scanner->next(&tuple, &ctx) == Status::OK)
    {
        const uint8_t *payload = tuple.data + sizeof(TupleHeader);
        const auto *data = reinterpret_cast<const TestData *>(payload);
        EXPECT_EQ(data->id, rows[count].id);
        EXPECT_STREQ(data->value, rows[count].value);
        count++;
    }
    EXPECT_EQ(count, rows.size());
}

TEST_F(StorageEngineTest, PageFullAllocatesNewPage)
{
    ID table_id = createTestTable("page_full_test");

    struct LargeData
    {
        int32_t id;
        char payload[200];
    };

    bool saw_new_page = false;
    uint32_t first_page_id = 0;

    ErrorContext ctx;
    for (int i = 0; i < 500; i++)
    {
        LargeData data{};
        data.id = i;
        memset(data.payload, 'X', sizeof(data.payload));

        auto tuple = buildTuple(&data, sizeof(LargeData));
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTuple(table_id, tuple.data(),
                                       static_cast<uint32_t>(tuple.size()),
                                       &page_id, &item_id, &ctx),
                  Status::OK) << ctx.message;

        if (i == 0)
        {
            first_page_id = page_id;
        }
        else if (page_id != first_page_id)
        {
            saw_new_page = true;
            break;
        }
    }

    EXPECT_TRUE(saw_new_page)
        << "Expected inserts to span multiple pages";
}
