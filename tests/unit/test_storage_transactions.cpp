#include <gtest/gtest.h>
#include <cstring>
#include <memory>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/storage_engine.h"
#include "test_helpers.h"

using namespace scratchbird::core;

class StorageTransactionTest : public ::testing::Test
{
protected:
    class ScopedCurrentConnection
    {
    public:
        explicit ScopedCurrentConnection(ConnectionContext *ctx)
            : prev_(ConnectionContext::getCurrent())
        {
            ConnectionContext::setCurrent(ctx);
        }
        ~ScopedCurrentConnection()
        {
            ConnectionContext::setCurrent(prev_);
        }

    private:
        ConnectionContext *prev_;
    };

    void SetUp() override
    {
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_storage_tx", ".db");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, 8192, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK) << ctx.message;

        catalog_ = db_->catalog_manager();
        storage_ = db_->storage_engine();
        ASSERT_NE(catalog_, nullptr);
        ASSERT_NE(storage_, nullptr);

        ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_ctx_.get());

        createTestTable(&ctx);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        if (db_)
        {
            db_->close();
        }
    }

    void createTestTable(ErrorContext *ctx)
    {
        std::vector<CatalogManager::SchemaInfo> schemas;
        ASSERT_EQ(catalog_->listSchemas(schemas, ctx), Status::OK);
        ID schema_id;
        if (schemas.empty())
        {
            ASSERT_EQ(catalog_->createSchema("public", "test", schema_id, ctx), Status::OK)
                << ctx->message;
        }
        else
        {
            schema_id = schemas[0].schema_id;
        }

        std::vector<CatalogManager::ColumnInfo> columns;
        CatalogManager::ColumnInfo id_col;
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT32);
        id_col.max_length = 4;
        id_col.nullable = false;
        id_col.has_default = false;
        columns.push_back(id_col);

        CatalogManager::ColumnInfo value_col;
        value_col.column_name = "val";
        value_col.data_type = static_cast<uint16_t>(DataType::INT32);
        value_col.max_length = 4;
        value_col.nullable = true;
        value_col.has_default = false;
        columns.push_back(value_col);

        ASSERT_EQ(catalog_->createTable(schema_id, "storage_tx_test", columns, table_id_, 0, ctx),
                  Status::OK)
            << ctx->message;
    }

    std::vector<uint8_t> makeTuple(int32_t id, int32_t val)
    {
        std::vector<uint8_t> buffer(sizeof(TupleHeader) + sizeof(int32_t) * 2);
        std::memset(buffer.data(), 0, buffer.size());
        std::memcpy(buffer.data() + sizeof(TupleHeader), &id, sizeof(int32_t));
        std::memcpy(buffer.data() + sizeof(TupleHeader) + sizeof(int32_t), &val, sizeof(int32_t));
        return buffer;
    }

    std::unique_ptr<Database> db_;
    CatalogManager *catalog_ = nullptr;
    StorageEngine *storage_ = nullptr;
    std::string test_db_path_;
    ID table_id_{};
    std::unique_ptr<ConnectionContext> conn_ctx_;
};

TEST_F(StorageTransactionTest, InsertUsesCurrentXid)
{
    ErrorContext ctx;
    ScopedCurrentConnection scope(conn_ctx_.get());

    auto tuple = makeTuple(1, 42);
    uint32_t page_id = 0;
    uint16_t item_id = 0;

    uint64_t xid = conn_ctx_->getCurrentXid();
    ASSERT_NE(xid, 0u);

    ASSERT_EQ(storage_->insertTuple(table_id_, tuple.data(), tuple.size(), &page_id, &item_id, &ctx),
              Status::OK) << ctx.message;

    void *page_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPage(page_id, &page_buffer, &ctx), Status::OK) << ctx.message;

    auto *page_data = static_cast<uint8_t *>(page_buffer);
    HeapPage heap(page_data, db_->page_size());

    const auto *items = reinterpret_cast<const ItemPointer *>(page_data + sizeof(PageHeader));
    ASSERT_FALSE(items[item_id].isDeleted());
    const auto *hdr = reinterpret_cast<const TupleHeader *>(page_data + items[item_id].offset);
    EXPECT_EQ(hdr->xmin, xid);
    EXPECT_EQ(hdr->xmax, 0u);

    db_->buffer_pool()->unpinPage(page_id, false, &ctx);
}

TEST_F(StorageTransactionTest, UpdateCreatesBackVersion)
{
    ErrorContext ctx;
    ScopedCurrentConnection scope(conn_ctx_.get());

    auto tuple_v1 = makeTuple(1, 10);
    uint32_t page_id = 0;
    uint16_t item_id = 0;

    ASSERT_EQ(storage_->insertTuple(table_id_, tuple_v1.data(), tuple_v1.size(),
                                    &page_id, &item_id, &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    uint64_t xid_update = conn_ctx_->getCurrentXid();
    auto tuple_second = makeTuple(1, 20);

    uint32_t updated_page_id = 0;
    uint16_t updated_item_id = 0;
    ASSERT_EQ(storage_->updateTuple(table_id_, page_id, item_id,
                                    tuple_second.data(), tuple_second.size(),
                                    &updated_page_id, &updated_item_id, &ctx),
              Status::OK) << ctx.message;

    void *page_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPage(updated_page_id, &page_buffer, &ctx), Status::OK)
        << ctx.message;

    auto *page_data = static_cast<uint8_t *>(page_buffer);
    HeapPage heap(page_data, db_->page_size());

    const uint8_t *tuple_data = nullptr;
    uint32_t tuple_size = 0;
    ASSERT_EQ(heap.getTuple(updated_item_id, &tuple_data, &tuple_size, &ctx), Status::OK)
        << ctx.message;

    const auto *hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
    ASSERT_TRUE(hdr->hasBackVersion());

    GPID back_gpid = hdr->back_version_gpid;
    uint16_t back_offset = hdr->back_version_slot;
    uint32_t back_page_id = static_cast<uint32_t>(getPageNumber(back_gpid));

    db_->buffer_pool()->unpinPage(updated_page_id, false, &ctx);

    void *back_page_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPage(back_page_id, &back_page_buffer, &ctx), Status::OK)
        << ctx.message;

    auto *back_page_data = static_cast<uint8_t *>(back_page_buffer);
    const auto *back_hdr = reinterpret_cast<const TupleHeader *>(back_page_data + back_offset);

    EXPECT_TRUE(back_hdr->infomask & TupleHeader::HEAP_CHAIN);
    EXPECT_TRUE(back_hdr->infomask & TupleHeader::HEAP_UPDATED);
    EXPECT_EQ(back_hdr->xmax, xid_update);

    db_->buffer_pool()->unpinPage(back_page_id, false, &ctx);
}

TEST_F(StorageTransactionTest, DeleteMarksTupleDeleted)
{
    ErrorContext ctx;
    ScopedCurrentConnection scope(conn_ctx_.get());

    auto tuple = makeTuple(1, 11);
    uint32_t page_id = 0;
    uint16_t item_id = 0;

    ASSERT_EQ(storage_->insertTuple(table_id_, tuple.data(), tuple.size(), &page_id, &item_id, &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    uint64_t xid_delete = conn_ctx_->getCurrentXid();
    ASSERT_EQ(storage_->deleteTuple(table_id_, page_id, item_id, UINT16_MAX, &ctx), Status::OK)
        << ctx.message;

    void *page_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPage(page_id, &page_buffer, &ctx), Status::OK) << ctx.message;

    auto *page_data = static_cast<uint8_t *>(page_buffer);
    HeapPage heap(page_data, db_->page_size());

    const auto *items = reinterpret_cast<const ItemPointer *>(page_data + sizeof(PageHeader));
    ASSERT_TRUE(items[item_id].isDeleted());
    const auto *hdr = reinterpret_cast<const TupleHeader *>(page_data + items[item_id].offset);
    EXPECT_EQ(hdr->xmax, xid_delete);
    EXPECT_TRUE(hdr->infomask & TupleHeader::FLAG_DELETED);

    db_->buffer_pool()->unpinPage(page_id, false, &ctx);
}
