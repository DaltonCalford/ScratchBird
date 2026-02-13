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
#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

namespace
{
    void appendByte(std::vector<uint8_t>& out, uint8_t value)
    {
        out.push_back(value);
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
        out.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
        size_t offset = out.size();
        out.resize(offset + sizeof(uint16_t));
        scratchbird::sblr::writeInt16(&out[offset], static_cast<uint16_t>(opcode));
    }

    void appendLiteralString(std::vector<uint8_t>& out, const std::string& value)
    {
        appendByte(out, static_cast<uint8_t>(Opcode::LITERAL_STRING));
        appendString(out, value);
    }

    std::vector<uint8_t> buildBooleanReturnFunction(const std::string& param_name,
                                                    bool return_true)
    {
        std::vector<uint8_t> expr;
        appendExtendedOpcode(expr, ExtendedOpcode::EXT_VAR_LOAD);
        appendString(expr, param_name);
        appendExtendedOpcode(expr, ExtendedOpcode::EXT_VAR_LOAD);
        appendString(expr, param_name);
        appendByte(expr, static_cast<uint8_t>(return_true ? Opcode::EXPR_EQ : Opcode::EXPR_NE));

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

    std::vector<uint8_t> buildCompareReturnFunction(const std::string& param_name,
                                                    const std::string& literal,
                                                    Opcode compare_op)
    {
        std::vector<uint8_t> expr;
        appendExtendedOpcode(expr, ExtendedOpcode::EXT_VAR_LOAD);
        appendString(expr, param_name);
        appendLiteralString(expr, literal);
        appendByte(expr, static_cast<uint8_t>(compare_op));

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
                          DataType return_type,
                          const std::vector<CatalogManager::ParameterInfo>& params,
                          const std::vector<uint8_t>& bytecode)
    {
        CatalogManager::FunctionInfo info;
        info.function_id = generateUuidV7();
        info.schema_id = schema_id;
        info.name = name;
        ErrorContext ctx;
        info.owner_id = catalog->getSystemUserId(&ctx);
        info.parameters = params;
        info.return_type = return_type;
        info.bytecode = bytecode;
        auto now = static_cast<uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
        info.created_time = now;
        info.modified_time = now;

        Status status = catalog->registerFunction(info, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;
    }
}

class DomainValidationIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("domain_validation", ".sbdb");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);
        domain_mgr_ = db_.domain_manager();
        ASSERT_NE(domain_mgr_, nullptr);

        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(catalog_->getSchema("PUBLIC", schema_info, &ctx), Status::OK) << ctx.message;
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

        CatalogManager::ParameterInfo param;
        param.name = "value";
        param.type = DataType::TEXT;
        param.mode = CatalogManager::ParameterMode::IN;

        registerFunction(catalog_, schema_id_, "validate_ok", DataType::BOOLEAN,
                         {param}, buildBooleanReturnFunction("value", true));
        registerFunction(catalog_, schema_id_, "validate_fail", DataType::BOOLEAN,
                         {param}, buildBooleanReturnFunction("value", false));
        registerFunction(catalog_, schema_id_, "validate_not_bad", DataType::BOOLEAN,
                         {param}, buildCompareReturnFunction("value", "bad", Opcode::EXPR_NE));

        auto create_ok = compileAndExecute(
            "CREATE DOMAIN validated_text AS TEXT WITH VALIDATION ("
            "FUNCTION = validate_ok, ERROR_MESSAGE = 'invalid value')");
        ASSERT_TRUE(create_ok.success()) << create_ok.error();

        auto create_fail = compileAndExecute(
            "CREATE DOMAIN rejected_text AS TEXT WITH VALIDATION ("
            "FUNCTION = validate_fail, ERROR_MESSAGE = 'invalid value')");
        ASSERT_TRUE(create_fail.success()) << create_fail.error();

        auto create_conditional = compileAndExecute(
            "CREATE DOMAIN conditional_text AS TEXT WITH VALIDATION ("
            "FUNCTION = validate_not_bad, ERROR_MESSAGE = 'invalid value')");
        ASSERT_TRUE(create_conditional.success()) << create_conditional.error();

        DomainInfo ok_info;
        ASSERT_EQ(domain_mgr_->getDomain(schema_id_, "validated_text", ok_info, &ctx),
                  Status::OK) << ctx.message;
        ok_domain_id_ = ok_info.domain_id;

        DomainInfo bad_info;
        ASSERT_EQ(domain_mgr_->getDomain(schema_id_, "rejected_text", bad_info, &ctx),
                  Status::OK) << ctx.message;
        bad_domain_id_ = bad_info.domain_id;

        DomainInfo conditional_info;
        ASSERT_EQ(domain_mgr_->getDomain(schema_id_, "conditional_text", conditional_info, &ctx),
                  Status::OK) << ctx.message;
        conditional_domain_id_ = conditional_info.domain_id;

        auto ok_columns = buildColumns(ok_domain_id_);
        ASSERT_EQ(catalog_->createTable(schema_id_, "validated_table", ok_columns,
                                        ok_table_id_, 0, &ctx),
                  Status::OK) << ctx.message;

        auto bad_columns = buildColumns(bad_domain_id_);
        ASSERT_EQ(catalog_->createTable(schema_id_, "rejected_table", bad_columns,
                                        bad_table_id_, 0, &ctx),
                  Status::OK) << ctx.message;

        auto conditional_columns = buildColumns(conditional_domain_id_);
        ASSERT_EQ(catalog_->createTable(schema_id_, "conditional_table", conditional_columns,
                                        conditional_table_id_, 0, &ctx),
                  Status::OK) << ctx.message;
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.close();
        db_file_.reset();
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

    Database db_{};
    std::unique_ptr<TestDatabaseFile> db_file_;
    CatalogManager* catalog_ = nullptr;
    DomainManager* domain_mgr_ = nullptr;
    ID schema_id_{};
    ID ok_domain_id_{};
    ID bad_domain_id_{};
    ID conditional_domain_id_{};
    ID ok_table_id_{};
    ID bad_table_id_{};
    ID conditional_table_id_{};
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
};

TEST_F(DomainValidationIntegrationTest, AllowsValidDomainValue)
{
    auto insert_result = compileAndExecute(
        "INSERT INTO validated_table (id, val) VALUES (1, 'ok')");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();

    auto select_result = compileAndExecute("SELECT val FROM validated_table");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());
    EXPECT_EQ(select_result.resultSet()->getValue(0, 0).toString(), "ok");
}

TEST_F(DomainValidationIntegrationTest, RejectsInvalidDomainValue)
{
    auto insert_result = compileAndExecute(
        "INSERT INTO rejected_table (id, val) VALUES (1, 'bad')");
    ASSERT_FALSE(insert_result.success());
    EXPECT_NE(insert_result.error().find("invalid value"), std::string::npos);
}

TEST_F(DomainValidationIntegrationTest, RejectsInvalidUpdate)
{
    auto insert_result = compileAndExecute(
        "INSERT INTO conditional_table (id, val) VALUES (1, 'good')");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();

    auto update_result = compileAndExecute(
        "UPDATE conditional_table SET val = 'bad' WHERE id = 1");
    ASSERT_FALSE(update_result.success());
    EXPECT_NE(update_result.error().find("invalid value"), std::string::npos);
}
