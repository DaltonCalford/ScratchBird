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
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/error_context.h"
#include "test_helpers.h"
#include <filesystem>
#include <vector>
#include <cstring>

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;

class EncryptedStorageTest : public ::testing::Test
{
protected:
    static constexpr uint32_t PAGE_SIZE = 8192;

    void SetUp() override
    {
        test_db_path_ = uniqueTestDbPath("test_encrypted_storage");
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_.string(), PAGE_SIZE, &ctx), Status::OK)
            << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_.string(), &ctx), Status::OK)
            << ctx.message;

        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        createTestTable();
    }

    void TearDown() override
    {
        db_.reset();
        std::filesystem::remove(test_db_path_);
    }

    void createTestTable()
    {
        ErrorContext ctx;

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
        data_col.column_name = "payload";
        data_col.data_type = static_cast<uint16_t>(DataType::TEXT);
        data_col.max_length = 0;
        data_col.nullable = true;
        data_col.has_default = false;
        columns.push_back(data_col);

        ASSERT_EQ(catalog_->createTable(schema_id, "encrypted_storage", columns, table_id_, 0, &ctx),
                  Status::OK);
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager *catalog_ = nullptr;
    ID table_id_;
};

TEST_F(EncryptedStorageTest, ToastedEncryptedValueRoundTrip)
{
    ErrorContext ctx;

    ToastManager toast_mgr(db_.get(), table_id_);
    ASSERT_EQ(toast_mgr.createToastTable(&ctx), Status::OK) << ctx.message;

    std::vector<uint8_t> page_buffer(PAGE_SIZE);
    HeapPage page(page_buffer.data(), PAGE_SIZE, &toast_mgr, db_.get(), table_id_);
    ASSERT_EQ(page.initialize(1, &ctx), Status::OK) << ctx.message;

    std::string plaintext(4096, 'x');
    TypedValue value = TypedValue::makeText(plaintext);
    std::vector<uint8_t> key(32, 0x2A);
    ASSERT_EQ(value.encrypt(key, EncryptionAlgorithm::AES256_GCM, 1, &ctx), Status::OK) << ctx.message;

    const auto& record = value.encryptedData();
    ASSERT_FALSE(record.empty());

    uint32_t record_len = static_cast<uint32_t>(record.size());
    std::vector<uint8_t> tuple_data(sizeof(TupleHeader) + sizeof(uint32_t) + record_len);
    std::memset(tuple_data.data(), 0, sizeof(TupleHeader));
    std::memcpy(tuple_data.data() + sizeof(TupleHeader), &record_len, sizeof(uint32_t));
    std::memcpy(tuple_data.data() + sizeof(TupleHeader) + sizeof(uint32_t),
                record.data(), record_len);

    ASSERT_TRUE(ToastManager::shouldToast(tuple_data.size(), PAGE_SIZE));

    uint16_t item_id = 0;
    ASSERT_EQ(page.insertTuple(tuple_data.data(), static_cast<uint32_t>(tuple_data.size()),
                               100, &item_id, &ctx),
              Status::OK) << ctx.message;

    std::vector<uint8_t> detoasted;
    ASSERT_EQ(page.getTupleDetoasted(item_id, &detoasted, 100, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(detoasted.size(), tuple_data.size());
    ASSERT_EQ(0, std::memcmp(detoasted.data() + sizeof(TupleHeader),
                             tuple_data.data() + sizeof(TupleHeader),
                             tuple_data.size() - sizeof(TupleHeader)));

    uint32_t stored_len = 0;
    std::memcpy(&stored_len, detoasted.data() + sizeof(TupleHeader), sizeof(uint32_t));
    ASSERT_EQ(stored_len, record_len);

    std::vector<uint8_t> stored_record(detoasted.begin() + sizeof(TupleHeader) + sizeof(uint32_t),
                                       detoasted.begin() + sizeof(TupleHeader) + sizeof(uint32_t) + stored_len);

    TypedValue decoded(DataType::TEXT);
    ASSERT_EQ(decoded.setEncryptedData(stored_record, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(decoded.decrypt(key, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(decoded.getText(), plaintext);
}
