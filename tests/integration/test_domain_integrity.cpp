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
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include <chrono>
#include <filesystem>
#include <sstream>
#include <thread>
#include <vector>

using namespace scratchbird::core;
using namespace scratchbird::sblr;

namespace
{
    void appendByte(std::vector<uint8_t>& out, uint8_t value)
    {
        out.push_back(value);
    }

    void appendInt16(std::vector<uint8_t>& out, uint16_t value)
    {
        size_t offset = out.size();
        out.resize(offset + sizeof(uint16_t));
        scratchbird::sblr::writeInt16(&out[offset], value);
    }

    void appendInt32(std::vector<uint8_t>& out, uint32_t value)
    {
        size_t offset = out.size();
        out.resize(offset + sizeof(uint32_t));
        scratchbird::sblr::writeInt32(&out[offset], value);
    }

    void appendUVarint(std::vector<uint8_t>& out, uint64_t value)
    {
        size_t offset = out.size();
        out.resize(offset + 10);
        size_t count = scratchbird::sblr::writeUVarint(&out[offset], value);
        out.resize(offset + count);
    }

    void appendString(std::vector<uint8_t>& out, const std::string& value)
    {
        appendUVarint(out, static_cast<uint64_t>(value.size()));
        out.insert(out.end(), value.begin(), value.end());
    }

    void appendExtendedOpcode(std::vector<uint8_t>& out, ExtendedOpcode opcode)
    {
        appendByte(out, static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
        appendInt16(out, static_cast<uint16_t>(opcode));
    }

    void appendLiteralString(std::vector<uint8_t>& out, const std::string& value)
    {
        appendByte(out, static_cast<uint8_t>(Opcode::LITERAL_STRING));
        appendString(out, value);
    }

    std::vector<uint8_t> buildConstantReturnFunction(const std::string& value)
    {
        std::vector<uint8_t> expr;
        appendLiteralString(expr, value);

        std::vector<uint8_t> bytecode;
        appendByte(bytecode, static_cast<uint8_t>(Opcode::VERSION));
        appendByte(bytecode, SBLR_VERSION);
        appendExtendedOpcode(bytecode, ExtendedOpcode::EXT_RETURN);
        appendByte(bytecode, 1);
        appendInt32(bytecode, static_cast<uint32_t>(expr.size()));
        bytecode.insert(bytecode.end(), expr.begin(), expr.end());
        appendByte(bytecode, static_cast<uint8_t>(Opcode::END));
        return bytecode;
    }

    void registerFunction(CatalogManager* catalog,
                          const ID& schema_id,
                          const std::string& name,
                          const std::vector<uint8_t>& bytecode)
    {
        CatalogManager::ParameterInfo param;
        param.name = "value";
        param.type = DataType::TEXT;
        param.mode = CatalogManager::ParameterMode::IN;

        CatalogManager::FunctionInfo info;
        info.function_id = generateUuidV7();
        info.schema_id = schema_id;
        info.name = name;
        ErrorContext ctx;
        info.owner_id = catalog->getSystemUserId(&ctx);
        info.parameters = {param};
        info.return_type = DataType::TEXT;
        info.bytecode = bytecode;
        auto now = static_cast<uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
        info.created_time = now;
        info.modified_time = now;

        Status status = catalog->registerFunction(info, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;
    }

    std::string generateUniqueDbPath()
    {
        std::ostringstream oss;
        oss << "/tmp/test_domain_integrity_"
            << std::this_thread::get_id() << "_"
            << std::chrono::steady_clock::now().time_since_epoch().count()
            << ".sbdb";
        return oss.str();
    }
}

class DomainIntegrityIntegrationTest : public ::testing::Test
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

        compiler_ = std::make_unique<QueryCompilerV3>(&db_);
        compiler_->setCurrentSchema(schema_id_);
        executor_ = std::make_unique<Executor>(&db_);
        executor_->setCurrentSchema(schema_id_);
        Status conn_status = db_.connect(connection_ctx_, &ctx);
        ASSERT_EQ(conn_status, Status::OK) << ctx.message;
        connection_ctx_->setCurrentUser(catalog_->getSystemUserId(&ctx), true);
        ConnectionContext::setCurrent(connection_ctx_.get());
        executor_->setConnectionContext(connection_ctx_.get());

        auto create_domain = compileAndExecute(
            "CREATE DOMAIN global_unique_text AS TEXT WITH INTEGRITY (UNIQUENESS = TRUE)");
        ASSERT_TRUE(create_domain.success()) << create_domain.error();

        DomainInfo domain_info;
        status = domain_mgr_->getDomain(schema_id_, "global_unique_text", domain_info, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;
        domain_id_ = domain_info.domain_id;

        auto columns_one = buildColumns(domain_id_);
        status = catalog_->createTable(schema_id_, "domain_table_one", columns_one, table_one_id_, 0, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        auto columns_two = buildColumns(domain_id_);
        status = catalog_->createTable(schema_id_, "domain_table_two", columns_two, table_two_id_, 0, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        registerFunction(catalog_, schema_id_, "norm_const",
                         buildConstantReturnFunction("custom"));

        status = domain_mgr_->createBasicDomain(schema_id_, "normalized_text", DataType::TEXT,
                                                0, 0, true, "", {}, normalization_domain_id_, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        DomainIntegrity norm_integrity;
        norm_integrity.normalization_enabled = true;
        norm_integrity.normalization_function = "LOWERCASE";
        status = domain_mgr_->setIntegrityOptions(normalization_domain_id_, norm_integrity, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        status = domain_mgr_->createBasicDomain(schema_id_, "custom_norm_text", DataType::TEXT,
                                                0, 0, true, "", {}, custom_norm_domain_id_, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        DomainIntegrity custom_integrity;
        custom_integrity.normalization_enabled = true;
        custom_integrity.normalization_function = "norm_const";
        status = domain_mgr_->setIntegrityOptions(custom_norm_domain_id_, custom_integrity, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        auto norm_columns = buildColumns(normalization_domain_id_);
        status = catalog_->createTable(schema_id_, "normalized_table", norm_columns,
                                       normalization_table_id_, 0, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        auto custom_columns = buildColumns(custom_norm_domain_id_);
        status = catalog_->createTable(schema_id_, "custom_norm_table", custom_columns,
                                       custom_norm_table_id_, 0, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        compiler_->setCurrentSchema(schema_id_);
        executor_->setCurrentSchema(schema_id_);
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

    std::vector<CatalogManager::ColumnInfo> buildColumns(const ID& domain_id)
    {
        CatalogManager::ColumnInfo id_col;
        id_col.column_id = generateUuidV7();
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT32);
        id_col.nullable = false;
        id_col.ordinal = 0;

        CatalogManager::ColumnInfo val_col;
        val_col.column_id = generateUuidV7();
        val_col.column_name = "val";
        val_col.data_type = static_cast<uint16_t>(DataType::TEXT);
        val_col.nullable = false;
        val_col.ordinal = 1;
        val_col.domain_id = domain_id;

        return {id_col, val_col};
    }

    std::string test_db_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    DomainManager* domain_mgr_ = nullptr;
    ID schema_id_;
    ID domain_id_;
    ID table_one_id_;
    ID table_two_id_;
    ID normalization_domain_id_;
    ID custom_norm_domain_id_;
    ID normalization_table_id_;
    ID custom_norm_table_id_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
};

TEST_F(DomainIntegrityIntegrationTest, RejectsDuplicateDomainValuesAcrossTables)
{
    auto insert_one = compileAndExecute(
        "INSERT INTO domain_table_one (id, val) VALUES (1, 'alpha')");
    ASSERT_TRUE(insert_one.success()) << insert_one.error();

    auto duplicate = compileAndExecute(
        "INSERT INTO domain_table_two (id, val) VALUES (1, 'alpha')");
    ASSERT_FALSE(duplicate.success());
    EXPECT_NE(duplicate.error().find("Domain uniqueness"), std::string::npos);
}

TEST_F(DomainIntegrityIntegrationTest, AllowsReuseAfterDelete)
{
    auto insert_one = compileAndExecute(
        "INSERT INTO domain_table_one (id, val) VALUES (1, 'alpha')");
    ASSERT_TRUE(insert_one.success()) << insert_one.error();

    auto insert_two = compileAndExecute(
        "INSERT INTO domain_table_two (id, val) VALUES (1, 'beta')");
    ASSERT_TRUE(insert_two.success()) << insert_two.error();

    auto update_conflict = compileAndExecute(
        "UPDATE domain_table_two SET val = 'alpha' WHERE id = 1");
    ASSERT_FALSE(update_conflict.success());

    auto delete_one = compileAndExecute(
        "DELETE FROM domain_table_one WHERE id = 1");
    ASSERT_TRUE(delete_one.success()) << delete_one.error();

    auto update_ok = compileAndExecute(
        "UPDATE domain_table_two SET val = 'alpha' WHERE id = 1");
    ASSERT_TRUE(update_ok.success()) << update_ok.error();
}

TEST_F(DomainIntegrityIntegrationTest, AppliesBuiltinNormalizationOnInsert)
{
    auto insert_result = compileAndExecute(
        "INSERT INTO normalized_table (id, val) VALUES (1, 'MiXeD')");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();

    auto select_result = compileAndExecute("SELECT val FROM normalized_table");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());
    EXPECT_EQ(select_result.resultSet()->getValue(0, 0).toString(), "mixed");
}

TEST_F(DomainIntegrityIntegrationTest, AppliesCustomNormalizationOnInsert)
{
    auto insert_result = compileAndExecute(
        "INSERT INTO custom_norm_table (id, val) VALUES (1, 'anything')");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();

    auto select_result = compileAndExecute("SELECT val FROM custom_norm_table");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());
    EXPECT_EQ(select_result.resultSet()->getValue(0, 0).toString(), "custom");
}

TEST_F(DomainIntegrityIntegrationTest, AppliesNormalizationOnUpdate)
{
    auto insert_result = compileAndExecute(
        "INSERT INTO normalized_table (id, val) VALUES (1, 'initial')");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();

    auto update_result = compileAndExecute(
        "UPDATE normalized_table SET val = 'MiXeD' WHERE id = 1");
    ASSERT_TRUE(update_result.success()) << update_result.error();

    auto select_result = compileAndExecute(
        "SELECT val FROM normalized_table WHERE id = 1");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());
    EXPECT_EQ(select_result.resultSet()->getValue(0, 0).toString(), "mixed");
}
