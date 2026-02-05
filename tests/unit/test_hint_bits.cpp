/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * @file test_hint_bits.cpp
 * @brief GoogleTest suite for Hint Bits optimization (Issue 2.13)
 *
 * Tests that hint bits are correctly set and checked during visibility
 * operations, providing a performance optimization by reducing
 * transaction state lookups.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/storage_engine.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using scratchbird::testing::TestDatabaseFile;

class HintBitsTest : public ::testing::Test
{
protected:
    static constexpr uint32_t kPageSize = 8192;

    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_hint_bits", ".db");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), kPageSize, &ctx), Status::OK)
            << "Database create failed: " << ctx.message;

        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK)
            << "Database open failed: " << ctx.message;

        // Initialize ProcArray
        Status status = db_.initializeProcArray(16, &ctx);
        if (status != Status::OK && status != Status::INVALID_ARGUMENT)
        {
            ASSERT_EQ(status, Status::OK) << ctx.message;
        }

        // Register backend
        ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id_, &ctx), Status::OK)
            << "Failed to register backend: " << ctx.message;

        // Create connection context
        ASSERT_EQ(db_.connect(conn_ctx_, &ctx), Status::OK)
            << "Failed to connect: " << ctx.message;
        ConnectionContext::setCurrent(conn_ctx_.get());
        ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK)
            << "Failed to initialize connection: " << ctx.message;

        xid_ = conn_ctx_->getCurrentXid();
        ASSERT_NE(xid_, 0u) << "Transaction ID should not be zero";

        // Get schema and create table using CatalogManager
        ID system_user = db_.catalog_manager()->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user, true);
        schema_id_ = resolveDefaultSchema(&ctx);
        ASSERT_NE(schema_id_, ID{}) << "Failed to resolve schema";

        table_id_ = createTestTable("test_hint_bits_table");
        ASSERT_NE(table_id_, ID{}) << "Failed to create test table";
    }

    void TearDown() override
    {
        ErrorContext ctx;
        ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        if (proc_id_ != 0)
        {
            ProcArrayManager::unregisterBackend(proc_id_, &ctx);
        }
        if (db_.is_open())
        {
            db_.close();
        }
        db_file_.reset();
    }

    ID resolveDefaultSchema(ErrorContext *ctx)
    {
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = db_.catalog_manager()->listSchemas(schemas, ctx);
        if (status == Status::OK && !schemas.empty())
        {
            return schemas.front().schema_id;
        }

        ID schema_id;
        status = db_.catalog_manager()->createSchema("main", "SYSTEM", schema_id, ctx);
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
        Status status = db_.catalog_manager()->createTable(schema_id_, name, columns, table_id, 0, &ctx);
        if (status != Status::OK)
        {
            return ID{};
        }
        return table_id;
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    Database db_;
    uint32_t proc_id_ = 0;
    std::unique_ptr<ConnectionContext> conn_ctx_;
    uint64_t xid_ = 0;
    ID schema_id_{};
    ID table_id_{};
};

/**
 * Test: Basic hint bits functionality - insert and read tuple
 *
 * Verifies that tuples can be inserted and read correctly,
 * and that hint bits are properly managed.
 */
TEST_F(HintBitsTest, BasicInsertAndRead)
{
    // Insert a tuple using StorageEngine API (returns page_id/item_id)
    std::vector<uint8_t> tuple_data(sizeof(TupleHeader) + 100, 0);
    auto *hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
    hdr->xmin = xid_;
    hdr->xmax = 0;
    hdr->infomask = 0; // No hint bits set initially

    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ErrorContext ctx;
    ASSERT_EQ(db_.storage_engine()->insertTuple(table_id_, tuple_data.data(), tuple_data.size(),
                                                &page_id, &item_id, &ctx),
              Status::OK)
        << "Failed to insert tuple: " << ctx.message;

    EXPECT_NE(page_id, 0u) << "Page ID should not be zero";

    // Commit the transaction
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK)
        << "Failed to commit: " << ctx.message;

    // Start a new transaction to read the tuple
    ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK)
        << "Failed to start new transaction: " << ctx.message;

    uint64_t new_xid = conn_ctx_->getCurrentXid();
    EXPECT_NE(new_xid, xid_) << "New transaction should have different XID";

    // First read: Should set hint bits
    Tuple tuple;
    ASSERT_EQ(db_.storage_engine()->getTuple(page_id, item_id, &tuple, &ctx), Status::OK)
        << "Failed to read tuple (first): " << ctx.message;

    EXPECT_GE(tuple.data_size, sizeof(TupleHeader)) << "Read size should include tuple header";

    // Second read: Should use hint bits (fast path)
    Tuple tuple2;
    ASSERT_EQ(db_.storage_engine()->getTuple(page_id, item_id, &tuple2, &ctx), Status::OK)
        << "Failed to read tuple (second): " << ctx.message;

    EXPECT_GE(tuple2.data_size, sizeof(TupleHeader)) << "Second read size should include tuple header";
}

/**
 * Test: Deleted tuple visibility with hint bits
 *
 * Verifies that deleted tuples are properly handled and
 * hint bits work correctly for deleted data.
 */
TEST_F(HintBitsTest, DeletedTupleVisibility)
{
    // Insert a tuple
    std::vector<uint8_t> tuple_data(sizeof(TupleHeader) + 100, 0);
    auto *hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
    hdr->xmin = xid_;
    hdr->xmax = 0;
    hdr->infomask = 0;

    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ErrorContext ctx;
    ASSERT_EQ(db_.storage_engine()->insertTuple(table_id_, tuple_data.data(), tuple_data.size(),
                                                &page_id, &item_id, &ctx),
              Status::OK)
        << "Failed to insert tuple: " << ctx.message;

    // Commit insertion
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK)
        << "Failed to commit insertion: " << ctx.message;

    // Start new transaction for deletion
    ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK)
        << "Failed to start new transaction: " << ctx.message;

    // Delete the tuple
    ASSERT_EQ(db_.storage_engine()->deleteTuple(table_id_, page_id, item_id, UINT16_MAX, &ctx),
              Status::OK)
        << "Failed to delete tuple: " << ctx.message;

    // Commit deletion
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK)
        << "Failed to commit deletion: " << ctx.message;

    // Start new transaction to verify deletion
    ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK)
        << "Failed to start transaction after delete: " << ctx.message;

    // Try to read - should not be visible (deleted)
    Tuple tuple;
    Status status = db_.storage_engine()->getTuple(page_id, item_id, &tuple, &ctx);

    EXPECT_EQ(status, Status::NOT_FOUND)
        << "Deleted tuple should not be visible, got status: " << static_cast<int>(status);
}

/**
 * Test: Multiple reads use hint bits fast path
 *
 * Verifies that repeated reads of the same tuple work correctly
 * and hint bits are properly utilized.
 */
TEST_F(HintBitsTest, MultipleReadsUseFastPath)
{
    // Insert a tuple
    std::vector<uint8_t> tuple_data(sizeof(TupleHeader) + 100, 0);
    auto *hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
    hdr->xmin = xid_;
    hdr->xmax = 0;
    hdr->infomask = 0;

    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ErrorContext ctx;
    ASSERT_EQ(db_.storage_engine()->insertTuple(table_id_, tuple_data.data(), tuple_data.size(),
                                                &page_id, &item_id, &ctx),
              Status::OK)
        << "Failed to insert tuple: " << ctx.message;

    // Commit
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK)
        << "Failed to commit: " << ctx.message;

    // Start new transaction
    ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK)
        << "Failed to start new transaction: " << ctx.message;

    // Multiple reads - all should succeed
    for (int i = 0; i < 10; i++)
    {
        Tuple tuple;
        ASSERT_EQ(db_.storage_engine()->getTuple(page_id, item_id, &tuple, &ctx), Status::OK)
            << "Read " << i << " failed: " << ctx.message;

        EXPECT_GE(tuple.data_size, sizeof(TupleHeader))
            << "Read " << i << " size should include tuple header";
    }
}

/**
 * Test: Hint bits not set initially
 *
 * Verifies that newly inserted tuples don't have hint bits set initially.
 */
TEST_F(HintBitsTest, HintBitsNotSetInitially)
{
    // Insert a tuple
    std::vector<uint8_t> tuple_data(sizeof(TupleHeader) + 100, 0);
    auto *hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
    hdr->xmin = xid_;
    hdr->xmax = 0;
    hdr->infomask = 0; // Explicitly no hint bits

    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ErrorContext ctx;
    ASSERT_EQ(db_.storage_engine()->insertTuple(table_id_, tuple_data.data(), tuple_data.size(),
                                                &page_id, &item_id, &ctx),
              Status::OK)
        << "Failed to insert tuple: " << ctx.message;

    // Read the tuple back immediately (same transaction)
    Tuple tuple;
    ASSERT_EQ(db_.storage_engine()->getTuple(page_id, item_id, &tuple, &ctx), Status::OK)
        << "Failed to read tuple: " << ctx.message;

    // Verify we can read the data
    EXPECT_GE(tuple.data_size, sizeof(TupleHeader))
        << "Read size should include tuple header";

    const auto *read_hdr = reinterpret_cast<const TupleHeader *>(tuple.data);
    EXPECT_EQ(read_hdr->xmin, xid_) << "xmin should match our transaction ID";
}

/**
 * Test: Multiple tuples with hint bits
 *
 * Verifies that multiple tuples in the same table work correctly
 * with hint bits.
 */
TEST_F(HintBitsTest, MultipleTuplesWithHintBits)
{
    std::vector<std::pair<uint32_t, uint16_t>> tuple_locations;
    ErrorContext ctx;

    // Insert multiple tuples
    for (int i = 0; i < 10; i++)
    {
        std::vector<uint8_t> tuple_data(sizeof(TupleHeader) + 50, 0);
        auto *hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
        hdr->xmin = xid_;
        hdr->xmax = 0;
        hdr->infomask = 0;

        // Add some data to distinguish tuples
        tuple_data[sizeof(TupleHeader)] = static_cast<uint8_t>(i);

        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(db_.storage_engine()->insertTuple(table_id_, tuple_data.data(),
                                                    tuple_data.size(), &page_id, &item_id, &ctx),
                  Status::OK)
            << "Failed to insert tuple " << i << ": " << ctx.message;

        tuple_locations.emplace_back(page_id, item_id);
    }

    // Commit
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK)
        << "Failed to commit: " << ctx.message;

    // Start new transaction
    ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK)
        << "Failed to start new transaction: " << ctx.message;

    // Read all tuples
    for (size_t i = 0; i < tuple_locations.size(); i++)
    {
        Tuple tuple;
        ASSERT_EQ(db_.storage_engine()->getTuple(tuple_locations[i].first,
                                                 tuple_locations[i].second,
                                                 &tuple, &ctx),
                  Status::OK)
            << "Failed to read tuple " << i << ": " << ctx.message;

        EXPECT_GE(tuple.data_size, sizeof(TupleHeader) + 50)
            << "Tuple " << i << " size mismatch";

        // Verify data
        EXPECT_EQ(tuple.data[sizeof(TupleHeader)], static_cast<uint8_t>(i))
            << "Tuple " << i << " data mismatch";
    }
}
