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
#include "scratchbird/core/toast.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/garbage_collector.h"
#include "test_helpers.h"
#include <memory>
#include <vector>

using namespace scratchbird::core;

class ToastGCContractTest : public ::testing::Test
{
protected:
    class ScopedCurrentConnection
    {
    public:
        explicit ScopedCurrentConnection(ConnectionContext* ctx)
            : prev_(ConnectionContext::getCurrent())
        {
            ConnectionContext::setCurrent(ctx);
        }
        ~ScopedCurrentConnection()
        {
            ConnectionContext::setCurrent(prev_);
        }
    private:
        ConnectionContext* prev_;
    };

    void SetUp() override
    {
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_toast_gc_contract", ".db");
        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, 8192, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK) << ctx.message;

        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        std::vector<CatalogManager::SchemaInfo> schemas;
        ASSERT_EQ(catalog_->listSchemas(schemas, &ctx), Status::OK);
        ID schema_id;
        if (schemas.empty())
        {
            ASSERT_EQ(catalog_->createSchema("public", "test", schema_id, &ctx), Status::OK);
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

        CatalogManager::ColumnInfo data_col;
        data_col.column_name = "data";
        data_col.data_type = static_cast<uint16_t>(DataType::BYTEA);
        data_col.max_length = 0;
        data_col.nullable = true;
        data_col.has_default = false;
        columns.push_back(data_col);

        ASSERT_EQ(catalog_->createTable(schema_id, "toast_gc_contract", columns, table_id_, 0, &ctx),
                  Status::OK);

        ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_ctx_.get());
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

    auto createConnection(ErrorContext* ctx) -> std::unique_ptr<ConnectionContext>
    {
        std::unique_ptr<ConnectionContext> conn;
        Status status = db_->connect(conn, ctx);
        EXPECT_EQ(status, Status::OK) << ctx->message;
        return conn;
    }

    auto beginTxn(ConnectionContext* conn) -> uint64_t
    {
        ScopedCurrentConnection scope(conn);
        return conn->getCurrentXid();
    }

    void commitTxn(ConnectionContext* conn, ErrorContext* ctx)
    {
        ScopedCurrentConnection scope(conn);
        ASSERT_EQ(conn->commit(ctx), Status::OK) << ctx->message;
    }

    void rollbackTxn(ConnectionContext* conn, ErrorContext* ctx)
    {
        ScopedCurrentConnection scope(conn);
        ASSERT_EQ(conn->rollback(ctx), Status::OK) << ctx->message;
    }

    auto readAnyToastChunkXmax(const ID& toast_table_id, uint64_t* xmax_out, ErrorContext* ctx)
        -> Status
    {
        auto* storage = db_->storage_engine();
        auto scan = storage->createScanAll(toast_table_id, ctx);
        if (!scan)
        {
            return Status::INVALID_ARGUMENT;
        }

        Tuple t{};
        Status s = scan->next(&t, ctx);
        if (s != Status::OK)
        {
            return s;
        }
        if (t.data_size < sizeof(TupleHeader))
        {
            return Status::PAGE_CORRUPT;
        }

        const auto* hdr = reinterpret_cast<const TupleHeader*>(t.data);
        *xmax_out = hdr->xmax;
        return Status::OK;
    }

    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    ID table_id_{};
    std::string test_db_path_;
    std::unique_ptr<ConnectionContext> conn_ctx_;
};

TEST_F(ToastGCContractTest, TipGcDeletesChunksWithCommittedDeleteTransaction)
{
    ErrorContext ctx;
    ToastManager toast_mgr(db_.get(), table_id_);
    ASSERT_EQ(toast_mgr.createToastTable(&ctx), Status::OK) << ctx.message;

    uint64_t creator_xid = beginTxn(conn_ctx_.get());
    ASSERT_NE(creator_xid, 0u);

    std::string payload = "toast_gc_contract_committed_delete";
    std::vector<uint8_t> bytes(payload.begin(), payload.end());
    ToastPointer ptr{};
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        ASSERT_EQ(toast_mgr.toastValue(bytes.data(), static_cast<uint32_t>(bytes.size()),
                                       ToastStrategy::EXTERNAL, creator_xid, &ptr, &ctx),
                  Status::OK) << ctx.message;
    }
    commitTxn(conn_ctx_.get(), &ctx);

    auto deleter = createConnection(&ctx);
    uint64_t deleter_xid = beginTxn(deleter.get());
    ASSERT_NE(deleter_xid, 0u);
    {
        ScopedCurrentConnection scope(deleter.get());
        ASSERT_EQ(toast_mgr.deleteToastValue(ptr.lob_uuid, deleter_xid, &ctx), Status::OK)
            << ctx.message;
    }
    commitTxn(deleter.get(), &ctx);

    uint64_t chunks_deleted = 0;
    ASSERT_EQ(db_->garbage_collector()->cleanToastChunksByTIP(
                  toast_mgr.toastTableId(), &chunks_deleted, &ctx),
              Status::OK) << ctx.message;
    EXPECT_GT(chunks_deleted, 0u);
}

TEST_F(ToastGCContractTest, TipGcClearsXmaxForAbortedDeleteTransaction)
{
    ErrorContext ctx;
    ToastManager toast_mgr(db_.get(), table_id_);
    ASSERT_EQ(toast_mgr.createToastTable(&ctx), Status::OK) << ctx.message;

    uint64_t creator_xid = beginTxn(conn_ctx_.get());
    ASSERT_NE(creator_xid, 0u);

    std::string payload = "toast_gc_contract_aborted_delete";
    std::vector<uint8_t> bytes(payload.begin(), payload.end());
    ToastPointer ptr{};
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        ASSERT_EQ(toast_mgr.toastValue(bytes.data(), static_cast<uint32_t>(bytes.size()),
                                       ToastStrategy::EXTERNAL, creator_xid, &ptr, &ctx),
                  Status::OK) << ctx.message;
    }
    commitTxn(conn_ctx_.get(), &ctx);

    auto deleter = createConnection(&ctx);
    uint64_t deleter_xid = beginTxn(deleter.get());
    ASSERT_NE(deleter_xid, 0u);
    {
        ScopedCurrentConnection scope(deleter.get());
        ASSERT_EQ(toast_mgr.deleteToastValue(ptr.lob_uuid, deleter_xid, &ctx), Status::OK)
            << ctx.message;
    }
    rollbackTxn(deleter.get(), &ctx);

    uint64_t chunks_deleted = 0;
    ASSERT_EQ(db_->garbage_collector()->cleanToastChunksByTIP(
                  toast_mgr.toastTableId(), &chunks_deleted, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(chunks_deleted, 0u);

    uint64_t chunk_xmax = UINT64_MAX;
    ASSERT_EQ(readAnyToastChunkXmax(toast_mgr.toastTableId(), &chunk_xmax, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(chunk_xmax, 0u);

    auto reader = createConnection(&ctx);
    uint64_t reader_xid = beginTxn(reader.get());
    ASSERT_NE(reader_xid, 0u);
    std::vector<uint8_t> detoasted;
    {
        ScopedCurrentConnection scope(reader.get());
        ASSERT_EQ(toast_mgr.detoastValue(&ptr, &detoasted, reader_xid, &ctx), Status::OK)
            << ctx.message;
    }
    rollbackTxn(reader.get(), &ctx);
}

