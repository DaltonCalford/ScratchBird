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
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <thread>

using namespace scratchbird::core;
using namespace scratchbird::sblr;

namespace
{
    std::string generateUniqueDbPath()
    {
        std::ostringstream oss;
        oss << "/tmp/test_domain_encryption_"
            << std::this_thread::get_id() << "_"
            << std::chrono::steady_clock::now().time_since_epoch().count()
            << ".sbdb";
        return oss.str();
    }
}

class DomainEncryptionIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = generateUniqueDbPath();
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        status = db_.open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        domain_mgr_ = db_.domain_manager();
        ASSERT_NE(domain_mgr_, nullptr);

        CatalogManager::SchemaInfo schema_info;
        status = catalog_->getSchema("PUBLIC", schema_info, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;
        schema_id_ = schema_info.schema_id;

        status = domain_mgr_->createBasicDomain(schema_id_, "secure_text", DataType::TEXT,
                                                0, 0, false, "", {}, domain_id_, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        DomainSecurity security;
        security.encryption_enabled = true;
        security.encryption_algorithm = EncryptionAlgorithm::AES256_GCM;
        status = domain_mgr_->setSecurityOptions(domain_id_, security, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        CatalogManager::ColumnInfo col;
        col.column_id = generateUuidV7();
        col.column_name = "secret";
        col.data_type = static_cast<uint16_t>(DataType::TEXT);
        col.nullable = false;
        col.ordinal = 0;
        col.domain_id = domain_id_;
        columns_.push_back(col);

        status = catalog_->createTable(schema_id_, "secure_data", columns_, table_id_, 0, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        compiler_ = std::make_unique<QueryCompilerV3>(&db_);
        compiler_->setCurrentSchema(schema_id_);
        executor_ = std::make_unique<Executor>(&db_);
        Status conn_status = db_.connect(connection_ctx_, &ctx);
        ASSERT_EQ(conn_status, Status::OK) << ctx.message;
        connection_ctx_->setCurrentUser(catalog_->getSystemUserId(&ctx), true);
        ConnectionContext::setCurrent(connection_ctx_.get());
        executor_->setConnectionContext(connection_ctx_.get());
    }

    void TearDown() override
    {
        compiler_.reset();
        executor_.reset();
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.close();
        std::filesystem::remove(test_db_path_);
        std::filesystem::remove(test_db_path_ + "-lock");
    }

    ExecutionResult compileAndExecute(const std::string& sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            std::string errors;
            for (const auto& err : compile_result.errors())
            {
                errors += err + "\n";
            }
            return ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(compile_result.bytecode());
    }

    std::string test_db_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    DomainManager* domain_mgr_ = nullptr;
    ID schema_id_;
    ID domain_id_;
    ID table_id_;
    std::vector<CatalogManager::ColumnInfo> columns_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
};

TEST_F(DomainEncryptionIntegrationTest, EncryptsOnInsertAndDecryptsOnSelect)
{
    auto insert_result = compileAndExecute(
        "INSERT INTO secure_data (secret) VALUES ('topsecret')");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();

    auto select_result = compileAndExecute("SELECT secret FROM secure_data");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());

    auto* result_set = select_result.resultSet();
    ASSERT_NE(result_set, nullptr);
    ASSERT_EQ(result_set->rowCount(), 1u);
    EXPECT_EQ(result_set->getValue(0, 0).toString(), "topsecret");

    auto scan_iter = db_.storage_engine()->createScan(table_id_, nullptr);
    ASSERT_NE(scan_iter, nullptr);

    Tuple tuple;
    ErrorContext ctx;
    Status status = scan_iter->next(&tuple, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    const auto* header = reinterpret_cast<const TupleHeader*>(tuple.data);
    size_t data_offset = sizeof(TupleHeader);
    if (header->hasNulls() && header->null_bitmap_offset > 0 &&
        header->null_bitmap_offset < tuple.data_size)
    {
        size_t bitmap_bytes = (columns_.size() + 7) / 8;
        data_offset = header->null_bitmap_offset + bitmap_bytes;
    }

    ASSERT_LE(data_offset + sizeof(uint32_t), static_cast<size_t>(tuple.data_size));
    uint32_t len = 0;
    std::memcpy(&len, tuple.data + data_offset, sizeof(uint32_t));
    data_offset += sizeof(uint32_t);
    ASSERT_LE(data_offset + len, static_cast<size_t>(tuple.data_size));

    std::vector<uint8_t> encrypted_record(tuple.data + data_offset,
                                          tuple.data + data_offset + len);

    TypedValue stored_value(DataType::TEXT);
    status = stored_value.setEncryptedData(encrypted_record, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_TRUE(stored_value.isEncrypted());

    status = domain_mgr_->decryptValue(domain_id_, stored_value, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(stored_value.toString(), "topsecret");
}
