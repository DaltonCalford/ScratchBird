/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include <gtest/gtest.h>

#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/gc_manager.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/storage_engine.h"
#include "test_helpers.h"

#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace scratchbird::core;

class VersionChainGcIntegrationTest : public ::testing::Test
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
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_version_chain_gc", ".db");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, 8192, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK) << ctx.message;

        storage_ = db_->storage_engine();
        catalog_ = db_->catalog_manager();
        gc_manager_ = db_->gc_manager();

        ASSERT_NE(storage_, nullptr);
        ASSERT_NE(catalog_, nullptr);
        ASSERT_NE(gc_manager_, nullptr);

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

        ASSERT_EQ(catalog_->createTable(schema_id, "version_chain_gc", columns, table_id_, 0, ctx),
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

    std::vector<uint8_t> makeLargeTuple(size_t payload_bytes)
    {
        std::vector<uint8_t> buffer(sizeof(TupleHeader) + payload_bytes, 0);
        for (size_t i = 0; i < payload_bytes; i++)
        {
            buffer[sizeof(TupleHeader) + i] = static_cast<uint8_t>(i & 0xFF);
        }
        return buffer;
    }

    std::unique_ptr<Database> db_;
    StorageEngine *storage_ = nullptr;
    CatalogManager *catalog_ = nullptr;
    GcManager *gc_manager_ = nullptr;
    std::string test_db_path_;
    ID table_id_{};
    std::unique_ptr<ConnectionContext> conn_ctx_;
};

TEST_F(VersionChainGcIntegrationTest, PrunesBackVersionWhenHorizonAdvances)
{
    ErrorContext ctx;
    ScopedCurrentConnection scope(conn_ctx_.get());

    auto tuple_v1 = makeLargeTuple(64);
    uint32_t page_id = 0;
    uint16_t item_id = 0;

    uint64_t xid_insert = conn_ctx_->getCurrentXid();
    ASSERT_NE(xid_insert, 0u);

    ASSERT_EQ(storage_->insertTuple(table_id_, tuple_v1.data(), tuple_v1.size(),
                                    &page_id, &item_id, &ctx),
              Status::OK) << ctx.message;

    const uint32_t primary_page_id = page_id;
    const uint16_t primary_item_id = item_id;

    // Fill the first heap page so updates require a cross-page back version.
    for (int i = 0; i < 512; ++i)
    {
        auto filler = makeLargeTuple(64);
        uint32_t filler_page = 0;
        uint16_t filler_item = 0;
        Status s = storage_->insertTuple(table_id_, filler.data(), filler.size(),
                                         &filler_page, &filler_item, &ctx);
        if (s != Status::OK)
        {
            break;
        }
        if (filler_page != primary_page_id)
        {
            break;
        }
    }

    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    uint64_t xid_update1 = conn_ctx_->getCurrentXid();
    ASSERT_GT(xid_update1, xid_insert);

    auto tuple_second = makeLargeTuple(64);
    uint32_t updated_page_id = 0;
    uint16_t updated_item_id = 0;
    ASSERT_EQ(storage_->updateTuple(table_id_, primary_page_id, primary_item_id,
                                    tuple_second.data(), tuple_second.size(),
                                    &updated_page_id, &updated_item_id, &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    uint64_t xid_update2 = conn_ctx_->getCurrentXid();
    ASSERT_GT(xid_update2, xid_update1);

    auto tuple_v3 = makeLargeTuple(64);
    uint32_t updated_page_id2 = 0;
    uint16_t updated_item_id2 = 0;
    ASSERT_EQ(storage_->updateTuple(table_id_, updated_page_id, updated_item_id,
                                    tuple_v3.data(), tuple_v3.size(),
                                    &updated_page_id2, &updated_item_id2, &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    uint64_t xid_after_update = conn_ctx_->getCurrentXid();
    ASSERT_GT(xid_after_update, xid_update2);

    void *page_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPage(updated_page_id2, &page_buffer, &ctx), Status::OK)
        << ctx.message;

    auto *page_data = static_cast<uint8_t *>(page_buffer);
    HeapPage heap(page_data, db_->page_size());

    const uint8_t *tuple_data = nullptr;
    uint32_t tuple_size = 0;
    ASSERT_EQ(heap.getTuple(updated_item_id2, &tuple_data, &tuple_size, &ctx), Status::OK)
        << ctx.message;

    const auto *hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
    ASSERT_TRUE(hdr->hasBackVersion());

    GPID back_gpid = hdr->back_version_gpid;
    uint32_t back_page_id = static_cast<uint32_t>(getPageNumber(back_gpid));
    if (back_page_id == updated_page_id2)
    {
        db_->buffer_pool()->unpinPage(updated_page_id2, false, &ctx);
        GTEST_SKIP() << "Unable to force cross-page back version; skipping prune validation";
    }

    db_->buffer_pool()->unpinPage(updated_page_id2, false, &ctx);

    GcStats stats;
    ASSERT_EQ(gc_manager_->gcPage(table_id_, back_page_id, &stats, &ctx), Status::OK)
        << ctx.message;
    EXPECT_GT(stats.version_chains_pruned, 0u);

    void *back_page_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPage(back_page_id, &back_page_buffer, &ctx), Status::OK)
        << ctx.message;

    auto *back_page_data = static_cast<uint8_t *>(back_page_buffer);
    HeapPage back_heap(back_page_data, db_->page_size());

    bool found_pruned = false;
    uint16_t item_count = back_heap.getItemCount();
    for (uint16_t item_id = 0; item_id < item_count; ++item_id)
    {
        const uint8_t *back_tuple = nullptr;
        uint32_t back_tuple_size = 0;
        if (back_heap.getTuple(item_id, &back_tuple, &back_tuple_size, &ctx) != Status::OK)
        {
            continue;
        }

        const auto *back_hdr = reinterpret_cast<const TupleHeader *>(back_tuple);
        if ((back_hdr->infomask & TupleHeader::HEAP_CHAIN) != 0 &&
            (back_hdr->infomask & TupleHeader::HEAP_UPDATED) != 0)
        {
            found_pruned = (back_hdr->infomask & TupleHeader::HEAP_XMAX_COMMITTED) != 0;
            if (found_pruned)
            {
                break;
            }
        }
    }

    EXPECT_TRUE(found_pruned) << "GC should mark prunable back versions as committed";

    db_->buffer_pool()->unpinPage(back_page_id, false, &ctx);
}
