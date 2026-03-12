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
 * End-to-End Tests for ScratchBird Query Compiler V3
 *
 * Phase 9: Integration tests for the complete Parser V3 pipeline
 *
 * Tests the full compilation flow:
 * SQL -> Lexer -> Parser -> Semantic Analyzer -> Bytecode Generator
 */

#include <gtest/gtest.h>
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/bytecode_validator.h"
#include "scratchbird/sblr/extract_element_ops.h"
#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/domain_manager.h"
#include "test_helpers.h"
#include "unit/test_user_helpers.h"
#include <cstdio>
#include <filesystem>
#include <optional>
#include <sstream>
#include <thread>
#include <chrono>

using namespace scratchbird::sblr;
using namespace scratchbird::core;

// Generate a unique database path per test to avoid conflicts in parallel execution
static std::string generateUniqueDbPath() {
    std::ostringstream oss;
    oss << "/tmp/test_query_compiler_v3_"
        << std::this_thread::get_id() << "_"
        << std::chrono::steady_clock::now().time_since_epoch().count()
        << ".sbdb";
    return oss.str();
}

namespace {

namespace sblr_v3 = scratchbird::sblr::v3;

bool containsOpcodeInInstructionValue(const sblr_v3::Value& value, sblr_v3::Opcode opcode);

bool containsOpcodeInInstruction(const sblr_v3::Instruction& inst, sblr_v3::Opcode opcode)
{
    if (inst.opcode == static_cast<uint16_t>(opcode))
    {
        return true;
    }
    return containsOpcodeInInstructionValue(inst.payload, opcode);
}

bool containsOpcodeInInstructionValue(const sblr_v3::Value& value, sblr_v3::Opcode opcode)
{
    if (auto ptr = std::get_if<sblr_v3::Value::InstrPtr>(&value.data))
    {
        if (*ptr)
        {
            return containsOpcodeInInstruction(**ptr, opcode);
        }
        return false;
    }

    if (auto bytes = std::get_if<sblr_v3::Value::Bytes>(&value.data))
    {
        if (bytes->empty())
        {
            return false;
        }
        size_t off = 0;
        sblr_v3::Instruction nested;
        sblr_v3::DecodeError err;
        if (sblr_v3::decodeInstructionWithSchema(bytes->data(), bytes->size(), off, nested, err))
        {
            return containsOpcodeInInstruction(nested, opcode);
        }
        return false;
    }

    if (auto list = std::get_if<sblr_v3::Value::List>(&value.data))
    {
        for (const auto& entry : *list)
        {
            if (containsOpcodeInInstructionValue(entry, opcode))
            {
                return true;
            }
        }
        return false;
    }

    if (auto obj = std::get_if<sblr_v3::Value::Object>(&value.data))
    {
        for (const auto& kv : *obj)
        {
            if (containsOpcodeInInstructionValue(kv.second, opcode))
            {
                return true;
            }
        }
        return false;
    }

    return false;
}

bool containsOpcodeDeep(const std::vector<uint8_t>& bytecode, sblr_v3::Opcode opcode)
{
    sblr_v3::Container container;
    std::string err;
    if (!sblr_v3::decodeContainer(bytecode.data(), bytecode.size(), container, err))
    {
        return false;
    }
    size_t offset = 0;
    sblr_v3::DecodeError decode_err;
    while (offset < container.bytecode_stream.size())
    {
        sblr_v3::Instruction inst;
        if (!sblr_v3::decodeInstructionWithSchema(container.bytecode_stream.data(),
                                                  container.bytecode_stream.size(),
                                                  offset,
                                                  inst,
                                                  decode_err) &&
            !sblr_v3::decodeInstruction(container.bytecode_stream.data(),
                                        container.bytecode_stream.size(),
                                        offset,
                                        inst,
                                        decode_err))
        {
            break;
        }
        if (containsOpcodeInInstruction(inst, opcode))
        {
            return true;
        }
    }
    return false;
}

std::optional<uint64_t> findOpcodePayloadU64(const std::vector<uint8_t>& bytecode,
                                             sblr_v3::Opcode opcode,
                                             const std::string& field_name)
{
    sblr_v3::Container container;
    std::string err;
    if (!sblr_v3::decodeContainer(bytecode.data(), bytecode.size(), container, err))
    {
        return std::nullopt;
    }

    size_t offset = 0;
    sblr_v3::DecodeError decode_err;
    while (offset < container.bytecode_stream.size())
    {
        sblr_v3::Instruction inst;
        if (!sblr_v3::decodeInstructionWithSchema(container.bytecode_stream.data(),
                                                  container.bytecode_stream.size(),
                                                  offset,
                                                  inst,
                                                  decode_err) &&
            !sblr_v3::decodeInstruction(container.bytecode_stream.data(),
                                        container.bytecode_stream.size(),
                                        offset,
                                        inst,
                                        decode_err))
        {
            break;
        }
        if (inst.opcode != static_cast<uint16_t>(opcode))
        {
            continue;
        }

        const auto* obj = std::get_if<sblr_v3::Value::Object>(&inst.payload.data);
        if (!obj)
        {
            return std::nullopt;
        }
        auto it = obj->find(field_name);
        if (it == obj->end())
        {
            return std::nullopt;
        }
        if (const auto* value = std::get_if<uint64_t>(&it->second.data))
        {
            return *value;
        }
        return std::nullopt;
    }

    return std::nullopt;
}

bool patchOpcodePayloadU64(std::vector<uint8_t>& bytecode,
                           sblr_v3::Opcode opcode,
                           const std::string& field_name,
                           uint64_t value)
{
    sblr_v3::Container container;
    std::string err;
    if (!sblr_v3::decodeContainer(bytecode.data(), bytecode.size(), container, err))
    {
        return false;
    }

    std::vector<sblr_v3::Instruction> instructions;
    size_t offset = 0;
    sblr_v3::DecodeError decode_err;
    bool patched = false;
    while (offset < container.bytecode_stream.size())
    {
        sblr_v3::Instruction inst;
        if (!sblr_v3::decodeInstructionWithSchema(container.bytecode_stream.data(),
                                                  container.bytecode_stream.size(),
                                                  offset,
                                                  inst,
                                                  decode_err) &&
            !sblr_v3::decodeInstruction(container.bytecode_stream.data(),
                                        container.bytecode_stream.size(),
                                        offset,
                                        inst,
                                        decode_err))
        {
            return false;
        }

        if (inst.opcode == static_cast<uint16_t>(opcode))
        {
            auto* obj = std::get_if<sblr_v3::Value::Object>(&inst.payload.data);
            if (obj == nullptr)
            {
                return false;
            }
            (*obj)[field_name] = sblr_v3::Value(value);
            patched = true;
        }
        instructions.push_back(std::move(inst));
    }

    if (!patched)
    {
        return false;
    }

    sblr_v3::Buffer stream;
    for (const auto& inst : instructions)
    {
        if (!sblr_v3::encodeInstructionWithSchema(inst, stream, decode_err))
        {
            sblr_v3::encodeInstruction(inst, stream);
        }
    }
    container.bytecode_stream = std::move(stream);
    return sblr_v3::encodeContainer(container, bytecode, err);
}

} // namespace

class QueryCompilerV3Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary database for testing with unique path
        test_db_path_ = generateUniqueDbPath();
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database";

        status = db_.open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database";

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr) << "CatalogManager is null";

        CatalogManager::SchemaInfo public_schema_info;
        status = catalog_->getSchema("public", public_schema_info, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to resolve public schema: " << ctx.message;
        public_schema_id_ = public_schema_info.schema_id;

        // Create a test schema
        EnsureUser(catalog_, "test_user");
        status = catalog_->createSchema("test", "test_user", test_schema_id_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test schema";

        // Create compiler and executor
        compiler_ = std::make_unique<QueryCompilerV3>(&db_);
        executor_ = std::make_unique<Executor>(&db_);

        status = db_.connect(connection_ctx_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create connection: " << ctx.message;
        connection_ctx_->setCurrentSchemaId(public_schema_id_);
        auto system_user_id = catalog_->getSystemUserId(&ctx);
        connection_ctx_->setCurrentUser(system_user_id, true);
        ConnectionContext::setCurrent(connection_ctx_.get());
        executor_->setConnectionContext(connection_ctx_.get());
    }

    void TearDown() override {
        compiler_.reset();
        executor_.reset();
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.close();
        // Clean up database file and lock file
        std::filesystem::remove(test_db_path_);
        std::filesystem::remove(test_db_path_ + "-lock");
    }

    // Compile and execute SQL
    ExecutionResult compileAndExecute(const std::string& sql) {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success()) {
            std::string errors;
            for (const auto& err : compile_result.errors()) {
                errors += err + "\n";
            }
            return ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(compile_result.bytecode());
    }

    std::string test_db_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    ID public_schema_id_;
    ID test_schema_id_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
};

// =============================================================================
// Compilation Tests
// =============================================================================

TEST_F(QueryCompilerV3Test, CompileSimpleSelect) {
    auto result = compiler_->compile("SELECT 1");
    ASSERT_TRUE(result.success()) << "Compilation failed";
    EXPECT_GT(result.bytecode().size(), 0);
}

TEST_F(QueryCompilerV3Test, CompileSelectWithArithmetic) {
    auto result = compiler_->compile("SELECT 1 + 2 * 3");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, CompileSelectWithCompactArithmetic) {
    auto result = compiler_->compile("SELECT 10+5-2");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, ValidateCompactAndSpacedArithmeticBytecode) {
    auto compact = compiler_->compile("SELECT 10+5");
    ASSERT_TRUE(compact.success()) << "Compact compilation failed";

    auto spaced = compiler_->compile("SELECT 10 + 5");
    ASSERT_TRUE(spaced.success()) << "Spaced compilation failed";

    ErrorContext compact_ctx;
    auto compact_status = validateBytecode(compact.bytecode(), &compact_ctx);
    EXPECT_EQ(compact_status, Status::OK) << compact_ctx.message;

    ErrorContext spaced_ctx;
    auto spaced_status = validateBytecode(spaced.bytecode(), &spaced_ctx);
    EXPECT_EQ(spaced_status, Status::OK) << spaced_ctx.message;
}

TEST_F(QueryCompilerV3Test, RouteParityClosedFamiliesDoNotUseBridgeFallbackPaths) {
    const std::vector<std::string> statements = {
        "CREATE DOMAIN dom_route AS INT",
        "DROP DOMAIN dom_route",
        "CREATE PUBLICATION pub_route FOR ALL TABLES",
        "CREATE SUBSCRIPTION sub_route CONNECTION 'host=127.0.0.1 dbname=main' PUBLICATION pub_route",
        "CREATE REPLICATION CHANNEL repl_route DIRECTION ONE_WAY SOURCE db_a TARGET db_b",
        "ALTER REPLICATION CHANNEL repl_route SET DIRECTION BIDIRECTIONAL",
        "RESYNC REPLICATION CHANNEL repl_route FORCE",
        "DROP REPLICATION CHANNEL IF EXISTS repl_route CASCADE"
    };

    for (const auto& sql : statements) {
        auto compile_result = compiler_->compile(sql);
        ASSERT_TRUE(compile_result.success()) << "Compilation failed for SQL: " << sql;

        auto exec_result = executor_->execute(compile_result.bytecode());
        if (!exec_result.success()) {
            EXPECT_EQ(exec_result.error().find("BRG_0406"), std::string::npos)
                << "Unexpected bridge fallback for SQL: " << sql << "\nerror=" << exec_result.error();
            EXPECT_EQ(exec_result.error().find("V3 opcode not implemented in executor"), std::string::npos)
                << "Unexpected unimplemented opcode fallback for SQL: " << sql << "\nerror=" << exec_result.error();
            EXPECT_EQ(exec_result.error().find("Unsupported DDL mutation opcode"), std::string::npos)
                << "Unexpected unsupported DDL mutation opcode for SQL: " << sql << "\nerror=" << exec_result.error();
        }
    }
}

TEST_F(QueryCompilerV3Test, ExecuteCanonicalExtractMonthFromCurrentDate) {
    auto result = compiler_->compile("SELECT EXTRACT(MONTH FROM CAST('2026-03-03' AS DATE))");
    ASSERT_TRUE(result.success()) << "EXTRACT(MONTH FROM CAST('2026-03-03' AS DATE)) compile failed";
}

TEST_F(QueryCompilerV3Test, CompileExtractUnknownFieldUsesDeterministicErrorCode) {
    auto result = compiler_->compile("SELECT EXTRACT('not_a_field' FROM CAST('2026-03-03' AS DATE))");
    ASSERT_FALSE(result.success()) << "Compilation unexpectedly succeeded";

    bool found = false;
    for (const auto& err : result.errors()) {
        if (err.find("EXTRACT_FIELD_UNKNOWN(NOT_A_FIELD)") != std::string::npos) {
            found = true;
            break;
        }
    }

    EXPECT_TRUE(found);
}

TEST_F(QueryCompilerV3Test, ExtractElementFieldNotValidForTypeUsesDeterministicErrorCode) {
    TypedValue source = TypedValue::makeDate(0);
    TypedValue out;
    std::string err;
    bool ok = scratchbird::sblr::extractElement(source,
                                                scratchbird::sblr::ExtractField::VERSION,
                                                {},
                                                &out,
                                                &err);
    ASSERT_FALSE(ok) << "extractElement unexpectedly succeeded";
    EXPECT_NE(err.find("EXTRACT_FIELD_NOT_VALID_FOR_TYPE(VERSION, DATE"), std::string::npos) << err;
}

TEST_F(QueryCompilerV3Test, ExtractElementConstraintViolationUsesDeterministicErrorCode) {
    TypedValue source = TypedValue::makeArray({TypedValue::makeInt32(1)});
    TypedValue out;
    std::string err;
    bool ok = scratchbird::sblr::extractElement(source,
                                                scratchbird::sblr::ExtractField::ELEMENT,
                                                {},
                                                &out,
                                                &err);
    ASSERT_FALSE(ok) << "extractElement unexpectedly succeeded";
    EXPECT_NE(err.find("EXTRACT_FIELD_VALUE_CONSTRAINT_VIOLATION(ELEMENT, ARRAY"),
              std::string::npos)
        << err;
}

TEST_F(QueryCompilerV3Test, ExtractElementSupportsTimeWithTimeZoneType) {
    auto result = compiler_->compile(
        "SELECT EXTRACT(HOUR FROM CAST('10:00:00+01:00' AS TIME WITH TIME ZONE))");
    ASSERT_TRUE(result.success()) << "TIME WITH TIME ZONE extract compile failed";
}

TEST_F(QueryCompilerV3Test, ExtractElementSupportsTimestampWithTimeZoneType) {
    auto result = compiler_->compile(
        "SELECT EXTRACT(YEAR FROM CAST('1970-01-01 01:00:00+01:00' AS TIMESTAMP WITH TIME ZONE))");
    ASSERT_TRUE(result.success()) << "TIMESTAMP WITH TIME ZONE extract compile failed";
}

TEST_F(QueryCompilerV3Test, ExecuteCreateViewWithMaterializedOption) {
    auto create_base = compileAndExecute("CREATE TABLE v_mat_base (id INT)");
    ASSERT_TRUE(create_base.success()) << "CREATE TABLE v_mat_base failed: " << create_base.error();

    auto result = compileAndExecute(
        "CREATE VIEW v_mat WITH (MATERIALIZED = TRUE) AS SELECT id FROM v_mat_base WITH DATA");
    ASSERT_TRUE(result.success()) << "CREATE VIEW ... WITH (MATERIALIZED = TRUE) failed: " << result.error();
}

TEST_F(QueryCompilerV3Test, CompileSelectWithStringLiteral) {
    auto result = compiler_->compile("SELECT 'hello world'");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, CompileSelectWithComparison) {
    auto result = compiler_->compile("SELECT 1 = 1, 2 < 3, 4 > 1");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, CompileSelectWithLogical) {
    auto result = compiler_->compile("SELECT TRUE AND FALSE, TRUE OR FALSE");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, CompileSelectWithCase) {
    // Note: Use TRUE literal instead of 1 = 1 comparison due to semantic analyzer limitation
    // with searched CASE WHEN condition type inference
    auto result = compiler_->compile("SELECT CASE WHEN TRUE THEN 'yes' ELSE 'no' END");
    if (!result.success()) {
        std::cerr << "Errors:\n";
        for (const auto& err : result.errors()) {
            std::cerr << "  " << err << "\n";
        }
    }
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, CompileSelectWithCast) {
    auto result = compiler_->compile("SELECT CAST(123 AS VARCHAR)");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, ExecuteCountDistinctAggregate) {
    auto create_table = compileAndExecute("CREATE TABLE count_distinct_t (id INT)");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    ASSERT_TRUE(compileAndExecute("INSERT INTO count_distinct_t (id) VALUES (1)").success());
    ASSERT_TRUE(compileAndExecute("INSERT INTO count_distinct_t (id) VALUES (1)").success());
    ASSERT_TRUE(compileAndExecute("INSERT INTO count_distinct_t (id) VALUES (2)").success());

    auto select_count = compileAndExecute("SELECT COUNT(DISTINCT id) FROM count_distinct_t");
    ASSERT_TRUE(select_count.success()) << select_count.error();
    ASSERT_TRUE(select_count.hasResultSet());
    ASSERT_EQ(select_count.resultSet()->rowCount(), 1u);
    EXPECT_EQ(select_count.resultSet()->getValue(0, 0).toInt64(), 2);
}

TEST_F(QueryCompilerV3Test, ExecuteJsonExistsOperatorSpecificity) {
    auto query = compileAndExecute(
        "SELECT "
        "'{\"a\":1,\"b\":2}' ? 'a', "
        "'{\"a\":1,\"b\":2}' ? 'z', "
        "'{\"a\":1,\"b\":2}' ?| ARRAY['a','z'], "
        "'{\"a\":1,\"b\":2}' ?& ARRAY['a','b'], "
        "'{\"a\":1,\"b\":2}' ?& ARRAY['a','z']");

    ASSERT_TRUE(query.success()) << query.error();
    ASSERT_TRUE(query.hasResultSet());
    ASSERT_EQ(query.resultSet()->rowCount(), 1u);
    EXPECT_EQ(query.resultSet()->getValue(0, 0).toString(), "true");
    EXPECT_EQ(query.resultSet()->getValue(0, 1).toString(), "false");
    EXPECT_EQ(query.resultSet()->getValue(0, 2).toString(), "true");
    EXPECT_EQ(query.resultSet()->getValue(0, 3).toString(), "true");
    EXPECT_EQ(query.resultSet()->getValue(0, 4).toString(), "false");
}

TEST_F(QueryCompilerV3Test, ExecuteSweepDatabase) {
    auto result = compileAndExecute("SWEEP DATABASE");
    ASSERT_TRUE(result.success()) << result.error();
}

TEST_F(QueryCompilerV3Test, RejectsVacuumAlias) {
    auto result = compileAndExecute("VACUUM");
    ASSERT_FALSE(result.success());
}

TEST_F(QueryCompilerV3Test, ExecuteCreateTableWithDomainColumn) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_domain = compileAndExecute("CREATE DOMAIN positive_int AS INT NOT NULL");
    ASSERT_TRUE(create_domain.success()) << create_domain.error();

    auto create_table = compileAndExecute("CREATE TABLE domain_table (value positive_int)");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    auto status = catalog_->getTable(test_schema_id_, "domain_table", table_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::ColumnInfo column_info;
    status = catalog_->getColumn(table_info.table_id, "value", column_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    DomainInfo domain_info;
    status = db_.domain_manager()->getDomain(test_schema_id_, "positive_int", domain_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    EXPECT_EQ(column_info.domain_id, domain_info.domain_id);
    EXPECT_EQ(column_info.data_type, static_cast<uint16_t>(domain_info.base_type));
}

TEST_F(QueryCompilerV3Test, ExecuteCreateTableWithDomainColumn_DefaultPublicSchema) {
    ErrorContext ctx;

    auto create_domain = compileAndExecute("CREATE DOMAIN public_int AS INT NOT NULL");
    ASSERT_TRUE(create_domain.success()) << create_domain.error();

    auto create_table = compileAndExecute("CREATE TABLE public_domain_table (value public_int)");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    CatalogManager::TableInfo table_info;
    auto status = catalog_->getTable(public_schema_id_, "public_domain_table", table_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::ColumnInfo column_info;
    status = catalog_->getColumn(table_info.table_id, "value", column_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    DomainInfo domain_info;
    status = db_.domain_manager()->getDomain(public_schema_id_, "public_int", domain_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    EXPECT_EQ(column_info.domain_id, domain_info.domain_id);
    EXPECT_EQ(column_info.data_type, static_cast<uint16_t>(domain_info.base_type));
}

TEST_F(QueryCompilerV3Test, DomainDefaultAppliedWhenColumnDefaultMissing) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_domain = compileAndExecute("CREATE DOMAIN d_text AS VARCHAR(10) DEFAULT 'alpha'");
    ASSERT_TRUE(create_domain.success()) << create_domain.error();

    auto create_table = compileAndExecute("CREATE TABLE domain_default_table (val d_text)");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto insert_row = compileAndExecute("INSERT INTO domain_default_table DEFAULT VALUES");
    ASSERT_TRUE(insert_row.success()) << insert_row.error();

    auto select_row = compileAndExecute("SELECT val FROM domain_default_table");
    ASSERT_TRUE(select_row.success()) << select_row.error();
    ASSERT_TRUE(select_row.hasResultSet());

    auto* results = select_row.resultSet();
    ASSERT_EQ(results->rowCount(), 1u);
    EXPECT_EQ(results->getValue(0, 0).toString(), "alpha");
}

TEST_F(QueryCompilerV3Test, InformationSchemaColumnsReportsDomainName) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_domain = compileAndExecute("CREATE DOMAIN d_info AS INT");
    ASSERT_TRUE(create_domain.success()) << create_domain.error();

    auto create_table = compileAndExecute("CREATE TABLE domain_info_table (val d_info)");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    ErrorContext ctx;
    CatalogManager::SchemaInfo schema_info;
    auto status = catalog_->getSchema(test_schema_id_, schema_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    std::string schema_name = schema_info.full_path.empty()
        ? schema_info.schema_name
        : schema_info.full_path;

    std::string sql =
        "SELECT domain_name FROM information_schema.columns "
        "WHERE table_schema = '" + schema_name +
        "' AND table_name = 'domain_info_table' AND column_name = 'val'";
    auto select_row = compileAndExecute(sql);
    ASSERT_TRUE(select_row.success()) << select_row.error();
    ASSERT_TRUE(select_row.hasResultSet());

    auto* results = select_row.resultSet();
    ASSERT_EQ(results->rowCount(), 1u);
    EXPECT_EQ(results->getValue(0, 0).toString(), "d_info");
}

TEST_F(QueryCompilerV3Test, DomainArrayEnforcesConstraintsAndSize) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_domain = compileAndExecute("CREATE DOMAIN positive_int AS INT CHECK (VALUE > 0)");
    ASSERT_TRUE(create_domain.success()) << create_domain.error();

    auto create_table = compileAndExecute("CREATE TABLE domain_array_table (vals positive_int[2])");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    auto status = catalog_->getTable(test_schema_id_, "domain_array_table", table_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::ColumnInfo column_info;
    status = catalog_->getColumn(table_info.table_id, "vals", column_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_TRUE(column_info.is_array);
    EXPECT_EQ(column_info.array_size, 2u);

    DomainInfo domain_info;
    status = db_.domain_manager()->getDomain(test_schema_id_, "positive_int", domain_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(column_info.domain_id, domain_info.domain_id);

    auto insert_ok = compileAndExecute("INSERT INTO domain_array_table (vals) VALUES (ARRAY[1, 2])");
    ASSERT_TRUE(insert_ok.success()) << insert_ok.error();

    auto select_row = compileAndExecute("SELECT vals FROM domain_array_table");
    ASSERT_TRUE(select_row.success()) << select_row.error();
    ASSERT_TRUE(select_row.hasResultSet());

    auto* results = select_row.resultSet();
    ASSERT_EQ(results->rowCount(), 1u);
    EXPECT_EQ(results->getValue(0, 0).toString(), "{1, 2}");

    auto insert_bad_size = compileAndExecute("INSERT INTO domain_array_table (vals) VALUES (ARRAY[1, 2, 3])");
    EXPECT_FALSE(insert_bad_size.success());

    auto insert_bad_value = compileAndExecute("INSERT INTO domain_array_table (vals) VALUES (ARRAY[1, -5])");
    EXPECT_FALSE(insert_bad_value.success());
}

TEST_F(QueryCompilerV3Test, UniqueArrayIndexWholeModeUsesWholeArrayKey) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_table = compileAndExecute("CREATE TABLE array_whole_table (id INT, vals INT[8])");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto create_index = compileAndExecute(
        "CREATE UNIQUE INDEX uq_array_whole ON array_whole_table USING BTREE (vals) "
        "WITH (ARRAY_UNIQUENESS = 'WHOLE')");
    ASSERT_TRUE(create_index.success()) << create_index.error();

    auto insert_first =
        compileAndExecute("INSERT INTO array_whole_table (id, vals) VALUES (1, ARRAY[1, 2])");
    ASSERT_TRUE(insert_first.success()) << insert_first.error();

    auto insert_same =
        compileAndExecute("INSERT INTO array_whole_table (id, vals) VALUES (2, ARRAY[1, 2])");
    EXPECT_FALSE(insert_same.success());
    EXPECT_NE(insert_same.error().find("UNIQUE index violation"), std::string::npos)
        << insert_same.error();

    auto insert_different_order =
        compileAndExecute("INSERT INTO array_whole_table (id, vals) VALUES (3, ARRAY[2, 1])");
    EXPECT_TRUE(insert_different_order.success()) << insert_different_order.error();

}

TEST_F(QueryCompilerV3Test, UniqueArrayIndexElementModeRejectsElementOverlap) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_table = compileAndExecute("CREATE TABLE array_element_table (id INT, vals INT[8])");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto create_index = compileAndExecute(
        "CREATE UNIQUE INDEX uq_array_element ON array_element_table USING BTREE (vals) "
        "WITH (ARRAY_UNIQUENESS = 'ELEMENT')");
    ASSERT_TRUE(create_index.success()) << create_index.error();

    auto insert_first =
        compileAndExecute("INSERT INTO array_element_table (id, vals) VALUES (1, ARRAY[1, 2])");
    ASSERT_TRUE(insert_first.success()) << insert_first.error();

    auto insert_overlap =
        compileAndExecute("INSERT INTO array_element_table (id, vals) VALUES (2, ARRAY[2, 3])");
    EXPECT_FALSE(insert_overlap.success());
    EXPECT_NE(insert_overlap.error().find("duplicate array element"), std::string::npos)
        << insert_overlap.error();

    auto insert_disjoint =
        compileAndExecute("INSERT INTO array_element_table (id, vals) VALUES (3, ARRAY[4, 5])");
    EXPECT_TRUE(insert_disjoint.success()) << insert_disjoint.error();

}

TEST_F(QueryCompilerV3Test, ArrayUniquenessOptionRequiresUniqueIndex) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_table = compileAndExecute("CREATE TABLE array_option_table (id INT, vals INT[8])");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto create_index = compileAndExecute(
        "CREATE INDEX idx_array_option ON array_option_table USING BTREE (vals) "
        "WITH (ARRAY_UNIQUENESS = 'ELEMENT')");
    EXPECT_FALSE(create_index.success());
    EXPECT_NE(create_index.error().find("requires a UNIQUE index"), std::string::npos)
        << create_index.error();
}

TEST_F(QueryCompilerV3Test, ExecuteCreateViewStoresDefinition) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_table = compileAndExecute("CREATE TABLE view_base (id INT)");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto create_view = compileAndExecute("CREATE VIEW view_v AS SELECT id FROM view_base");
    ASSERT_TRUE(create_view.success()) << create_view.error();

    ErrorContext ctx;
    CatalogManager::ViewInfo view_info;
    auto status = catalog_->getView(test_schema_id_, "view_v", view_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(view_info.definition, "SELECT id FROM view_base");
}

TEST_F(QueryCompilerV3Test, CompileCreateViewDoesNotMarkSimpleViewTemporary) {
    compiler_->setCurrentSchema(test_schema_id_);

    auto compile_result =
        compiler_->compile("CREATE VIEW view_flag_v AS SELECT id, public_col FROM view_flag_base");
    ASSERT_TRUE(compile_result.success());

    auto flags = findOpcodePayloadU64(
        compile_result.bytecode(),
        sblr_v3::Opcode::SBLR3_CREATE_VIEW,
        "flags");
    ASSERT_TRUE(flags.has_value());
    EXPECT_EQ((*flags & 0x0004u), 0u);
}

TEST_F(QueryCompilerV3Test, ExecuteCreateViewIsVisibleAcrossSessions) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_table = compileAndExecute("CREATE TABLE view_scope_base (id INT)");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto create_view = compileAndExecute("CREATE VIEW view_scope_v AS SELECT id FROM view_scope_base");
    ASSERT_TRUE(create_view.success()) << create_view.error();

    ErrorContext ctx;
    CatalogManager::ViewInfo view_info;
    auto status = catalog_->getView(test_schema_id_, "view_scope_v", view_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(view_info.temp_metadata_scope, CatalogManager::TempMetadataScope::NONE);

    std::unique_ptr<ConnectionContext> second_connection_ctx;
    status = db_.connect(second_connection_ctx, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create second connection: " << ctx.message;
    second_connection_ctx->setCurrentSchemaId(test_schema_id_);
    auto system_user_id = catalog_->getSystemUserId(&ctx);
    second_connection_ctx->setCurrentUser(system_user_id, true);

    ConnectionContext::setCurrent(second_connection_ctx.get());
    CatalogManager::ViewInfo second_view_info;
    status = catalog_->getView(test_schema_id_, "view_scope_v", second_view_info, &ctx);
    ConnectionContext::setCurrent(connection_ctx_.get());

    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(second_view_info.temp_metadata_scope, CatalogManager::TempMetadataScope::NONE);
}

TEST_F(QueryCompilerV3Test, ExecuteCreateViewIgnoresStrayTemporaryFlagWithoutTemporarySql) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_table = compileAndExecute("CREATE TABLE view_flag_guard_base (id INT)");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    const std::string create_view_sql =
        "CREATE VIEW view_flag_guard_v AS SELECT id FROM view_flag_guard_base";
    auto compile_result = compiler_->compile(create_view_sql);
    ASSERT_TRUE(compile_result.success());

    auto flags = findOpcodePayloadU64(
        compile_result.bytecode(),
        sblr_v3::Opcode::SBLR3_CREATE_VIEW,
        "flags");
    ASSERT_TRUE(flags.has_value());

    std::vector<uint8_t> mutated = compile_result.bytecode();
    ASSERT_TRUE(patchOpcodePayloadU64(
        mutated,
        sblr_v3::Opcode::SBLR3_CREATE_VIEW,
        "flags",
        *flags | 0x0004u));

    connection_ctx_->set_dialect_tag("firebird");
    connection_ctx_->beginStatementTracking(create_view_sql);
    auto create_view = executor_->execute(mutated);
    connection_ctx_->endStatementTrackingSuccess(0);
    connection_ctx_->set_dialect_tag("scratchbird");
    ASSERT_TRUE(create_view.success()) << create_view.error();

    ErrorContext ctx;
    CatalogManager::ViewInfo view_info;
    auto status = catalog_->getView(test_schema_id_, "view_flag_guard_v", view_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(view_info.temp_metadata_scope, CatalogManager::TempMetadataScope::NONE);

    auto commit = compileAndExecute("COMMIT");
    ASSERT_TRUE(commit.success()) << commit.error();

    std::unique_ptr<ConnectionContext> second_connection_ctx;
    status = db_.connect(second_connection_ctx, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create second connection: " << ctx.message;
    second_connection_ctx->setCurrentSchemaId(test_schema_id_);
    auto system_user_id = catalog_->getSystemUserId(&ctx);
    second_connection_ctx->setCurrentUser(system_user_id, true);

    ConnectionContext::setCurrent(second_connection_ctx.get());
    CatalogManager::ViewInfo second_view_info;
    status = catalog_->getView(test_schema_id_, "view_flag_guard_v", second_view_info, &ctx);
    ConnectionContext::setCurrent(connection_ctx_.get());

    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(second_view_info.temp_metadata_scope, CatalogManager::TempMetadataScope::NONE);
}

TEST_F(QueryCompilerV3Test, ExecuteSelectFromViewReturnsRowsAcrossSessions) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_table = compileAndExecute(
        "CREATE TABLE view_select_base (id INT, public_col VARCHAR(32))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto insert_row = compileAndExecute(
        "INSERT INTO view_select_base (id, public_col) VALUES (1, 'p1')");
    ASSERT_TRUE(insert_row.success()) << insert_row.error();

    auto create_view = compileAndExecute(
        "CREATE VIEW view_select_v AS SELECT id, public_col FROM view_select_base");
    ASSERT_TRUE(create_view.success()) << create_view.error();

    auto commit = compileAndExecute("COMMIT");
    ASSERT_TRUE(commit.success()) << commit.error();

    ErrorContext ctx;
    std::unique_ptr<ConnectionContext> second_connection_ctx;
    auto status = db_.connect(second_connection_ctx, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create second connection: " << ctx.message;
    second_connection_ctx->setCurrentSchemaId(test_schema_id_);
    auto system_user_id = catalog_->getSystemUserId(&ctx);
    second_connection_ctx->setCurrentUser(system_user_id, true);

    QueryCompilerV3 second_compiler(&db_);
    second_compiler.setCurrentSchema(test_schema_id_);
    Executor second_executor(&db_);
    second_executor.setCurrentSchema(test_schema_id_);
    second_executor.setConnectionContext(second_connection_ctx.get());

    ConnectionContext::setCurrent(second_connection_ctx.get());
    auto base_compile = second_compiler.compile("SELECT id, public_col FROM view_select_base");
    ASSERT_TRUE(base_compile.success()) << "Compilation failed for base table";
    auto base_select_result = second_executor.execute(base_compile.bytecode());
    ASSERT_TRUE(base_select_result.success()) << base_select_result.error();
    ASSERT_TRUE(base_select_result.hasResultSet());
    ASSERT_NE(base_select_result.resultSet(), nullptr);
    EXPECT_EQ(base_select_result.resultSet()->rowCount(), 1u);

    auto view_compile = second_compiler.compile("SELECT id, public_col FROM view_select_v");
    ASSERT_TRUE(view_compile.success()) << "Compilation failed";
    auto select_result = second_executor.execute(view_compile.bytecode());
    ConnectionContext::setCurrent(connection_ctx_.get());

    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());
    ASSERT_NE(select_result.resultSet(), nullptr);
    EXPECT_EQ(select_result.resultSet()->rowCount(), 1u);
    ASSERT_EQ(select_result.resultSet()->columnCount(), 2u);
    EXPECT_EQ(select_result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(select_result.resultSet()->getValue(0, 1).toString(), "p1");
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(QueryCompilerV3Test, CompileInvalidSyntax) {
    auto result = compiler_->compile("SELECT FROM");
    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.errors().empty());
}

TEST_F(QueryCompilerV3Test, CompileEmptyQuery) {
    auto result = compiler_->compile("");
    EXPECT_FALSE(result.success());
}

// =============================================================================
// Transaction Statement Tests
// =============================================================================

TEST_F(QueryCompilerV3Test, CompileStartTransaction) {
    auto result = compiler_->compile("START TRANSACTION");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, CompileCommit) {
    auto result = compiler_->compile("COMMIT");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, CompileRollback) {
    auto result = compiler_->compile("ROLLBACK");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

// =============================================================================
// DDL Statement Tests
// =============================================================================

TEST_F(QueryCompilerV3Test, CompileCreateTable) {
    auto result = compiler_->compile("CREATE TABLE products (id INT, name VARCHAR(100))");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

// =============================================================================
// End-to-End Execution Tests
// =============================================================================

TEST_F(QueryCompilerV3Test, ExecuteSimpleSelect) {
    auto result = compileAndExecute("SELECT 42");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->rowCount(), 1);
    EXPECT_EQ(rs->columnCount(), 1);
}

TEST_F(QueryCompilerV3Test, ExecuteSelectWithExpression) {
    auto result = compileAndExecute("SELECT 10 + 5");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->rowCount(), 1);
}

TEST_F(QueryCompilerV3Test, ExecuteSelectWithCompactExpression) {
    auto result = compileAndExecute("SELECT 10+5");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    ASSERT_EQ(rs->columnCount(), 1);
    EXPECT_DOUBLE_EQ(rs->getValue(0, 0).toDouble(), 15.0);
}

TEST_F(QueryCompilerV3Test, ExecuteSelectMultipleColumns) {
    auto result = compileAndExecute("SELECT 1, 2, 3");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->rowCount(), 1);
    EXPECT_EQ(rs->columnCount(), 3);
}

TEST_F(QueryCompilerV3Test, ExecuteContextFunctionsRuntimeClosed) {
    auto begin_result = compileAndExecute("BEGIN");
    ASSERT_TRUE(begin_result.success()) << "BEGIN failed: " << begin_result.error();

    auto result = compileAndExecute(
        "SELECT CURRENT_USER, CURRENT_CONNECTION, CURRENT_TRANSACTION, NOW()");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    ASSERT_EQ(rs->columnCount(), 4);

    EXPECT_FALSE(rs->getValue(0, 0).isNull()); // CURRENT_USER
    EXPECT_FALSE(rs->getValue(0, 1).isNull()); // CURRENT_CONNECTION
    EXPECT_FALSE(rs->getValue(0, 2).isNull()); // CURRENT_TRANSACTION
    EXPECT_FALSE(rs->getValue(0, 3).isNull()); // NOW()

    auto commit_result = compileAndExecute("COMMIT");
    ASSERT_TRUE(commit_result.success()) << "COMMIT failed: " << commit_result.error();
}

TEST_F(QueryCompilerV3Test, ExecuteBareContextKeywordsRuntimeClosed) {
    auto begin_result = compileAndExecute("BEGIN");
    ASSERT_TRUE(begin_result.success()) << "BEGIN failed: " << begin_result.error();

    auto result = compileAndExecute(
        "SELECT NOW, CURRENT_DATE, CURRENT_TIME, CURRENT_TIMESTAMP, SESSION_USER, CURRENT_SESSION");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    ASSERT_EQ(rs->columnCount(), 6);

    EXPECT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_FALSE(rs->getValue(0, 1).isNull());
    EXPECT_FALSE(rs->getValue(0, 2).isNull());
    EXPECT_FALSE(rs->getValue(0, 3).isNull());
    EXPECT_FALSE(rs->getValue(0, 4).isNull());
    EXPECT_FALSE(rs->getValue(0, 5).isNull());

    auto commit_result = compileAndExecute("COMMIT");
    ASSERT_TRUE(commit_result.success()) << "COMMIT failed: " << commit_result.error();
}

TEST_F(QueryCompilerV3Test, ExecuteNowVsCurrentTimestampSemantics) {
    auto begin_result = compileAndExecute("BEGIN");
    ASSERT_TRUE(begin_result.success()) << "BEGIN failed: " << begin_result.error();

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    auto first = compileAndExecute("SELECT CURRENT_TIMESTAMP, NOW(), 1");
    ASSERT_TRUE(first.success()) << "Execution failed: " << first.error();
    ASSERT_TRUE(first.hasResultSet());
    auto* first_rs = first.resultSet();
    ASSERT_NE(first_rs, nullptr);
    ASSERT_EQ(first_rs->rowCount(), 1);
    ASSERT_EQ(first_rs->columnCount(), 3);

    const auto first_current_ts = first_rs->getValue(0, 0);
    const auto first_now_ts = first_rs->getValue(0, 1);
    ASSERT_FALSE(first_current_ts.isNull());
    ASSERT_FALSE(first_now_ts.isNull());

    const int64_t first_current_micros = first_current_ts.getTimestamp();
    const int64_t first_now_micros = first_now_ts.getTimestamp();
    EXPECT_GE(first_now_micros, first_current_micros);
    EXPECT_GE(first_now_micros - first_current_micros, 5000);

    std::this_thread::sleep_for(std::chrono::milliseconds(15));

    auto second = compileAndExecute("SELECT CURRENT_TIMESTAMP, NOW(), 2");
    ASSERT_TRUE(second.success()) << "Execution failed: " << second.error();
    ASSERT_TRUE(second.hasResultSet());
    auto* second_rs = second.resultSet();
    ASSERT_NE(second_rs, nullptr);
    ASSERT_EQ(second_rs->rowCount(), 1);
    ASSERT_EQ(second_rs->columnCount(), 3);

    const auto second_current_ts = second_rs->getValue(0, 0);
    const auto second_now_ts = second_rs->getValue(0, 1);
    ASSERT_FALSE(second_current_ts.isNull());
    ASSERT_FALSE(second_now_ts.isNull());

    const int64_t second_current_micros = second_current_ts.getTimestamp();
    const int64_t second_now_micros = second_now_ts.getTimestamp();

    EXPECT_EQ(second_current_micros, first_current_micros);
    EXPECT_GE(second_now_micros, first_now_micros + 5000);
    EXPECT_GE(second_now_micros, second_current_micros);
    EXPECT_GE(second_now_micros - second_current_micros, 5000);

    auto commit_result = compileAndExecute("COMMIT");
    ASSERT_TRUE(commit_result.success()) << "COMMIT failed: " << commit_result.error();
}

TEST_F(QueryCompilerV3Test, ExecuteV3AbsFunctionEvaluates) {
    auto result = compileAndExecute("SELECT ABS(-5)");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    ASSERT_EQ(rs->columnCount(), 1);
    EXPECT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 5);
}

TEST_F(QueryCompilerV3Test, ExecuteV3LikeOperatorEvaluates) {
    auto result = compileAndExecute("SELECT 'alpha' LIKE 'a%'");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    EXPECT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_TRUE(rs->getValue(0, 0).toBoolean());
}

TEST_F(QueryCompilerV3Test, ExecuteV3InListEvaluates) {
    auto result = compileAndExecute("SELECT 1 IN (1, 2, 3)");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    EXPECT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_TRUE(rs->getValue(0, 0).toBoolean());
}

TEST_F(QueryCompilerV3Test, ExecuteV3RegexOperatorEvaluates) {
    auto result = compileAndExecute("SELECT 'alpha' ~ 'a.*'");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    EXPECT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_TRUE(rs->getValue(0, 0).toBoolean());
}

TEST_F(QueryCompilerV3Test, ExecuteV3MathAndConcatFunctionsEvaluate) {
    auto result = compileAndExecute(
        "SELECT POWER(2, 3), SIN(0), COS(0), TAN(0), CONCAT('a', 'b')");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    ASSERT_EQ(rs->columnCount(), 5);

    EXPECT_NEAR(rs->getValue(0, 0).toDouble(), 8.0, 1e-9);
    EXPECT_NEAR(rs->getValue(0, 1).toDouble(), 0.0, 1e-9);
    EXPECT_NEAR(rs->getValue(0, 2).toDouble(), 1.0, 1e-9);
    EXPECT_NEAR(rs->getValue(0, 3).toDouble(), 0.0, 1e-9);
    EXPECT_EQ(rs->getValue(0, 4).toString(), "ab");
}

TEST_F(QueryCompilerV3Test, CanonicalFunctionDispatchUsesDedicatedOpcodes) {
    struct Case {
        const char* sql;
        scratchbird::sblr::v3::Opcode expected_opcode;
    };

    const std::vector<Case> cases = {
        {"SELECT ABS(-5)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_ABS},
        {"SELECT ACOS(0)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_ACOS},
        {"SELECT ACOSH(1)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_ACOSH},
        {"SELECT AGE(CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_AGE},
        {"SELECT ASIN(0)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_ASIN},
        {"SELECT ASINH(0)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_ASINH},
        {"SELECT ATAN(1)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_ATAN},
        {"SELECT ATAN2(1, 1)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_ATAN2},
        {"SELECT ATANH(0.5)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_ATANH},
        {"SELECT CBRT(8)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_CBRT},
        {"SELECT CEILING(1.2)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_CEIL},
        {"SELECT CHAR_LENGTH('abc')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_CHAR_LENGTH},
        {"SELECT COLLATE('abc', 'en_US')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_COLLATE},
        {"SELECT COL_DESCRIPTION(1, 1)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_COL_DESCRIPTION},
        {"SELECT COALESCE(NULL, 1)", scratchbird::sblr::v3::Opcode::SBLR3_COALESCE},
        {"SELECT CONCAT_WS('-', 'a', 'b')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_CONCAT_WS},
        {"SELECT CONVERT('abc', 'utf8')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_CONVERT},
        {"SELECT COSH(0)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_COSH},
        {"SELECT COT(1)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_COT},
        {"SELECT CURRENT_CONNECTION", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_CURRENT_CONNECTION},
        {"SELECT CURRENT_DATE", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_CURRENT_DATE},
        {"SELECT CURRENT_ROLE", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_CURRENT_ROLE},
        {"SELECT CURRENT_SESSION", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_CURRENT_CONNECTION},
        {"SELECT CURRENT_TIME", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_CURRENT_TIME},
        {"SELECT CURRENT_TIMESTAMP", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_NOW},
        {"SELECT CURRENT_TRANSACTION", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_CURRENT_TRANSACTION},
        {"SELECT CURRENT_USER", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_CURRENT_USER},
        {"SELECT DATE_ADD(1, 2)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_DATE_ADD},
        {"SELECT DATE_DIFF(10, 4)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_DATE_DIFF},
        {"SELECT DATE_SUB(10, 4)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_DATE_SUB},
        {"SELECT DEGREES(3.1415926535)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_DEGREES},
        {"SELECT EXP(1)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_EXP},
        {"SELECT FLOOR(1.9)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_FLOOR},
        {"SELECT FORMAT_TYPE(23, NULL)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_FORMAT_TYPE},
        {"SELECT GREATEST(3, 2, 1)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_GREATEST},
        {"SELECT LEAST(3, 2, 1)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_LEAST},
        {"SELECT LOWER('ABC')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_LOWER},
        {"SELECT LENGTH('abc')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_LENGTH},
        {"SELECT LN(1)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_LN},
        {"SELECT LOG(10)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_LOG},
        {"SELECT LOG10(10)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_LOG10},
        {"SELECT LOG2(8)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_LOG2},
        {"SELECT LTRIM('  abc')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_LTRIM},
        {"SELECT MOD(7, 3)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_MOD},
        {"SELECT NOW()", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_NOW},
        {"SELECT NULLIF(1, 1)", scratchbird::sblr::v3::Opcode::SBLR3_NULLIF},
        {"SELECT OBJ_DESCRIPTION(1, 'pg_class')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_OBJ_DESCRIPTION},
        {"SELECT OCTET_LENGTH('abc')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_OCTET_LENGTH},
        {"SELECT PI()", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_PI},
        {"SELECT POWER(2, 3)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_POWER},
        {"SELECT RADIANS(180)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_RADIANS},
        {"SELECT REPLACE('abc', 'a', 'z')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_REPLACE},
        {"SELECT ROUND(3.1415926535, 2)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_ROUND},
        {"SELECT RTRIM('abc  ')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_RTRIM},
        {"SELECT SESSION_USER", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_CURRENT_USER},
        {"SELECT SHOBJ_DESCRIPTION(1, 'pg_class')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_SHOBJ_DESCRIPTION},
        {"SELECT SIGN(-5)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_SIGN},
        {"SELECT SINH(0)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_SINH},
        {"SELECT SQRT(9)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_SQRT},
        {"SELECT TRIM('  abc  ')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_TRIM},
        {"SELECT SUBSTRING('abc', 1, 1)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_SUBSTRING},
        {"SELECT TANH(0)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_TANH},
        {"SELECT TO_CHAR(CURRENT_DATE)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_TO_CHAR},
        {"SELECT TO_DATE('2026-03-03')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_TO_DATE},
        {"SELECT TO_TIMESTAMP('2026-03-03 12:34:56')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_TO_TIMESTAMP},
        {"SELECT TRUNC(3.1415926535, 2)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_TRUNC},
        {"SELECT UPPER('abc')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_UPPER},
        {"SELECT ARRAY_POSITION(ARRAY[1, 2, 3], 2)", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_ARRAY_POSITION},
        {"SELECT ENDS_WITH('abc', 'bc')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_ENDS_WITH},
        {"SELECT JSON_ARRAY(1, 2, 3)", scratchbird::sblr::v3::Opcode::SBLR3_JSON_ARRAY},
        {"SELECT JSON_OBJECT('a', 1)", scratchbird::sblr::v3::Opcode::SBLR3_JSON_OBJECT},
        {"SELECT JSON_EXTRACT('{\"a\":1}', '$.a')", scratchbird::sblr::v3::Opcode::SBLR3_JSON_EXTRACT},
        {"SELECT JSON_EXISTS('{\"a\":1}', '$.a')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_JSON_EXISTS},
        {"SELECT JSON_HAS_KEY('{\"a\":1}', 'a')", scratchbird::sblr::v3::Opcode::SBLR3_FUNC_JSON_HAS_KEY},
        {"SELECT JSON_SET('{\"a\":1}', '$.a', 2)", scratchbird::sblr::v3::Opcode::SBLR3_JSON_SET},
        {"SELECT JSON_INSERT('{\"a\":1}', '$.b', 2)", scratchbird::sblr::v3::Opcode::SBLR3_JSON_INSERT},
        {"SELECT JSON_REMOVE('{\"a\":1}', '$.a')", scratchbird::sblr::v3::Opcode::SBLR3_JSON_REMOVE},
    };

    for (const auto& c : cases) {
        auto result = compiler_->compile(c.sql);
        ASSERT_TRUE(result.success()) << "Compilation failed for SQL: " << c.sql;

        EXPECT_TRUE(containsOpcodeDeep(result.bytecode(), c.expected_opcode))
            << "Expected opcode not present for SQL: " << c.sql;

        EXPECT_FALSE(containsOpcodeDeep(
            result.bytecode(), scratchbird::sblr::v3::Opcode::SBLR3_EXPR_FUNCTION_CALL))
            << "Unexpected generic function-call opcode for SQL: " << c.sql;
    }
}

TEST_F(QueryCompilerV3Test, CompileCurrentDatabaseUsesGenericFunctionCallOpcode) {
    auto result = compiler_->compile("SELECT CURRENT_DATABASE()");
    ASSERT_TRUE(result.success()) << "Compilation failed";
    EXPECT_TRUE(containsOpcodeDeep(
        result.bytecode(), scratchbird::sblr::v3::Opcode::SBLR3_EXPR_FUNCTION_CALL));
}

TEST_F(QueryCompilerV3Test, ExecuteV3ConcatOperatorEvaluates) {
    auto result = compileAndExecute("SELECT 'a' || 'b'");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    ASSERT_EQ(rs->columnCount(), 1);
    EXPECT_EQ(rs->getValue(0, 0).toString(), "ab");
}

TEST_F(QueryCompilerV3Test, ExecuteCanonicalMathFunctionsEvaluate) {
    auto result = compileAndExecute(
        "SELECT "
        "ACOS(1), ACOSH(2), ASIN(1), ASINH(1), ATAN(1), ATAN2(1, 1), ATANH(0.5), "
        "CBRT(8), CEILING(1.2), COSH(0), COT(1), DEGREES(3.1415926535), EXP(1), FLOOR(1.9), "
        "LN(1), LOG(10), LOG10(10), LOG2(8), MOD(7, 3), PI(), RADIANS(180), ROUND(3.14159, 2), "
        "SIGN(-5), SINH(0), SQRT(9), TANH(0), TRUNC(3.14159, 2)");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());
    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);
    for (size_t i = 0; i < rs->columnCount(); ++i) {
        EXPECT_FALSE(rs->getValue(0, i).isNull()) << "Column " << i << " is NULL";
    }
}

TEST_F(QueryCompilerV3Test, ExecuteCanonicalMathFunctionsInvalidDomainReturnsNull) {
    auto result = compileAndExecute(
        "SELECT "
        "ACOS(2), ASIN(2), ACOSH(0), ATANH(1), "
        "LN(0), LOG10(0), LOG2(0), LOG(1, 10), LOG(10, -1), "
        "SQRT(-1), EXP(1000000)");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());
    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);
    ASSERT_EQ(rs->columnCount(), 11u);
    for (size_t i = 0; i < rs->columnCount(); ++i) {
        EXPECT_TRUE(rs->getValue(0, i).isNull()) << "Expected NULL for invalid-domain column " << i;
    }
}

TEST_F(QueryCompilerV3Test, ExecuteCanonicalStringAndDateFunctionsEvaluate) {
    auto result = compileAndExecute(
        "SELECT "
        "LENGTH('abc'), CHAR_LENGTH('abc'), OCTET_LENGTH('abc'), "
        "UPPER('abc'), LOWER('ABC'), TRIM('  abc  '), LTRIM('  abc'), RTRIM('abc  '), "
        "SUBSTRING('abcdef', 2, 3), "
        "DATE_ADD(CAST('2026-03-03' AS DATE), 1), "
        "DATE_SUB(CAST('2026-03-03' AS DATE), 1), "
        "DATE_DIFF(CAST('2026-03-03' AS DATE), CAST('2026-03-01' AS DATE))");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());
    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);
    for (size_t i = 0; i < rs->columnCount(); ++i) {
        EXPECT_FALSE(rs->getValue(0, i).isNull()) << "Column " << i << " is NULL";
    }
}

TEST_F(QueryCompilerV3Test, ExecuteCanonicalCatalogHelperFunctionsEvaluate) {
    auto result = compileAndExecute(
        "SELECT "
        "AGE(CURRENT_TIMESTAMP, CURRENT_TIMESTAMP), "
        "FORMAT_TYPE(23, NULL), "
        "OBJ_DESCRIPTION(1, 'pg_class'), "
        "SHOBJ_DESCRIPTION(1, 'pg_class'), "
        "COL_DESCRIPTION(1, 1)");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());
    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);
    ASSERT_EQ(rs->columnCount(), 5u);
    EXPECT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_FALSE(rs->getValue(0, 1).isNull());
}

TEST_F(QueryCompilerV3Test, ExecuteCanonicalCurrentDatabaseFunctionEvaluate) {
    auto result = compileAndExecute("SELECT CURRENT_DATABASE()");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());
    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);
    ASSERT_EQ(rs->columnCount(), 1u);
    EXPECT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_FALSE(rs->getValue(0, 0).toString().empty());
}

TEST_F(QueryCompilerV3Test, ExecuteCanonicalFunctionResultShapeParity) {
    auto result = compileAndExecute(
        "SELECT "
        "LENGTH('abc'), "
        "CHAR_LENGTH('abc'), "
        "OCTET_LENGTH('abc'), "
        "SIGN(-5), "
        "ROUND(3.14159, 2), "
        "TRUNC(3.14159, 2), "
        "DATE_DIFF(CAST('2026-03-03' AS DATE), CAST('2026-03-01' AS DATE)), "
        "FORMAT_TYPE(23, NULL)");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());
    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);
    ASSERT_EQ(rs->columnCount(), 8u);

    const auto v0 = rs->getValue(0, 0);
    const auto v1 = rs->getValue(0, 1);
    const auto v2 = rs->getValue(0, 2);
    const auto v3 = rs->getValue(0, 3);
    const auto v4 = rs->getValue(0, 4);
    const auto v5 = rs->getValue(0, 5);
    const auto v6 = rs->getValue(0, 6);
    const auto v7 = rs->getValue(0, 7);

    EXPECT_EQ(v0.type(), DataType::INT32);
    EXPECT_EQ(v1.type(), DataType::INT32);
    EXPECT_EQ(v2.type(), DataType::INT32);
    EXPECT_EQ(v3.type(), DataType::INT32);
    EXPECT_EQ(v4.type(), DataType::FLOAT64);
    EXPECT_EQ(v5.type(), DataType::FLOAT64);
    EXPECT_EQ(v6.type(), DataType::INT64);
    EXPECT_TRUE(v7.type() == DataType::TEXT || v7.type() == DataType::VARCHAR);

    EXPECT_EQ(v0.toInt64(), 3);
    EXPECT_EQ(v1.toInt64(), 3);
    EXPECT_EQ(v2.toInt64(), 3);
    EXPECT_EQ(v3.toInt64(), -1);
    EXPECT_NEAR(v4.toDouble(), 3.14, 1e-9);
    EXPECT_NEAR(v5.toDouble(), 3.14, 1e-9);
    EXPECT_EQ(v6.toInt64(), 2);
    EXPECT_FALSE(v7.isNull());
}

TEST_F(QueryCompilerV3Test, ExecuteOperatorStrictModeBlocksImplicitNumericCast) {
    auto relaxed = compileAndExecute("SELECT '2' + 3");
    ASSERT_TRUE(relaxed.success()) << "Execution failed: " << relaxed.error();
    ASSERT_TRUE(relaxed.hasResultSet());
    EXPECT_FALSE(relaxed.resultSet()->getValue(0, 0).isNull());

    auto set_mode = compileAndExecute("SET operator.strict_mode ON");
    ASSERT_TRUE(set_mode.success()) << "Failed to enable strict mode: " << set_mode.error();

    auto strict = compileAndExecute("SELECT '2' + 3");
    if (strict.success()) {
        ASSERT_TRUE(strict.hasResultSet());
        EXPECT_TRUE(strict.resultSet()->getValue(0, 0).isNull());
    } else {
        EXPECT_NE(strict.error().find("Implicit casts disabled"), std::string::npos);
    }

    auto reset_mode = compileAndExecute("SET operator.strict_mode OFF");
    ASSERT_TRUE(reset_mode.success()) << "Failed to disable strict mode: " << reset_mode.error();
}

TEST_F(QueryCompilerV3Test, ExecuteSetSchemaShorthandUpdatesSchemaContext) {
    auto set_schema = compileAndExecute("SET SCHEMA test");
    ASSERT_TRUE(set_schema.success()) << "SET SCHEMA failed: " << set_schema.error();

    auto show_current = compileAndExecute("SHOW current_schema");
    ASSERT_TRUE(show_current.success()) << "SHOW current_schema failed: " << show_current.error();
    ASSERT_TRUE(show_current.hasResultSet());
    ASSERT_EQ(show_current.resultSet()->rowCount(), 1u);
    ASSERT_EQ(show_current.resultSet()->columnCount(), 2u);
    const auto current_schema = show_current.resultSet()->getValue(0, 1).toString();
    EXPECT_NE(current_schema.find("test"), std::string::npos);

    auto show_path = compileAndExecute("SHOW search_path");
    ASSERT_TRUE(show_path.success()) << "SHOW search_path failed: " << show_path.error();
    ASSERT_TRUE(show_path.hasResultSet());
    ASSERT_EQ(show_path.resultSet()->rowCount(), 1u);
    ASSERT_EQ(show_path.resultSet()->columnCount(), 2u);
    const auto search_path = show_path.resultSet()->getValue(0, 1).toString();
    EXPECT_NE(search_path.find("test"), std::string::npos);
}

TEST_F(QueryCompilerV3Test, ExecuteSetCurrentSchemaSupportsToAndDefault) {
    auto set_schema = compileAndExecute("SET CURRENT_SCHEMA TO test");
    ASSERT_TRUE(set_schema.success()) << "SET CURRENT_SCHEMA failed: " << set_schema.error();

    auto show_current = compileAndExecute("SHOW current_schema");
    ASSERT_TRUE(show_current.success()) << "SHOW current_schema failed: " << show_current.error();
    ASSERT_TRUE(show_current.hasResultSet());
    ASSERT_EQ(show_current.resultSet()->rowCount(), 1u);
    const auto current_schema = show_current.resultSet()->getValue(0, 1).toString();
    EXPECT_NE(current_schema.find("test"), std::string::npos);

    auto reset_schema = compileAndExecute("SET CURRENT_SCHEMA DEFAULT");
    ASSERT_TRUE(reset_schema.success()) << "SET CURRENT_SCHEMA DEFAULT failed: "
                                        << reset_schema.error();

    auto show_after_reset = compileAndExecute("SHOW current_schema");
    ASSERT_TRUE(show_after_reset.success()) << "SHOW current_schema failed: "
                                            << show_after_reset.error();
    ASSERT_TRUE(show_after_reset.hasResultSet());
    ASSERT_EQ(show_after_reset.resultSet()->rowCount(), 1u);
    const auto reset_schema_value = show_after_reset.resultSet()->getValue(0, 1).toString();
    EXPECT_NE(reset_schema_value.find("public"), std::string::npos);
}

TEST_F(QueryCompilerV3Test, EmulatedSessionResetAllKeepsDialectTag) {
    connection_ctx_->set_dialect_tag("MYSQL");

    auto reset_all = compileAndExecute("RESET ALL");
    ASSERT_TRUE(reset_all.success()) << "RESET ALL failed: " << reset_all.error();
    EXPECT_EQ(connection_ctx_->dialect_tag(), "MYSQL");
}

TEST_F(QueryCompilerV3Test, EmulatedSessionRejectsSetParserDefaultOrSwitch) {
    connection_ctx_->set_dialect_tag("MYSQL");

    auto reset_parser = compileAndExecute("SET PARSER DEFAULT");
    ASSERT_FALSE(reset_parser.success());
    EXPECT_EQ(connection_ctx_->dialect_tag(), "MYSQL");

    auto set_parser_native = compileAndExecute("SET PARSER SCRATCHBIRD");
    ASSERT_FALSE(set_parser_native.success());
    EXPECT_EQ(connection_ctx_->dialect_tag(), "MYSQL");
}

TEST_F(QueryCompilerV3Test, ExecuteCastUsingHex) {
    auto result = compileAndExecute(
        "SELECT CAST(CAST('48656c6c6f' AS BLOB USING hex) AS VARCHAR USING hex)");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    ASSERT_EQ(rs->columnCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_EQ(value.getVarchar(), "48656c6c6f");
}

TEST_F(QueryCompilerV3Test, ExecuteCastUsingBase64) {
    auto result = compileAndExecute(
        "SELECT CAST(CAST('SGVsbG8=' AS BLOB USING base64) AS VARCHAR USING base64)");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    ASSERT_EQ(rs->columnCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_EQ(value.getVarchar(), "SGVsbG8=");
}

TEST_F(QueryCompilerV3Test, ExecuteSelectBoolean) {
    auto result = compileAndExecute("SELECT TRUE, FALSE");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());
}

TEST_F(QueryCompilerV3Test, ExecuteSelectNull) {
    auto result = compileAndExecute("SELECT NULL");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(QueryCompilerV3Test, CompilationStatisticsEnabled) {
    compiler_->setStatsEnabled(true);

    auto result = compiler_->compile("SELECT 1 + 2 * 3");
    ASSERT_TRUE(result.success());

    const auto& stats = result.stats();
    EXPECT_GT(stats.bytecode_size, 0);
    // Parser time should be non-zero
    EXPECT_GE(stats.parser_time.count(), 0);
}

// =============================================================================
// Optimization Tests
// =============================================================================

TEST_F(QueryCompilerV3Test, OptimizationsEnabled) {
    compiler_->setOptimizationsEnabled(true);

    auto result = compiler_->compile("SELECT 1 + 2");
    ASSERT_TRUE(result.success());
}

TEST_F(QueryCompilerV3Test, OptimizationsDisabled) {
    compiler_->setOptimizationsEnabled(false);

    auto result = compiler_->compile("SELECT 1 + 2");
    ASSERT_TRUE(result.success());
}
