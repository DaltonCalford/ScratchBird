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
#include "scratchbird/sblr/firebird_query_compiler.h"
#include "scratchbird/sblr/mysql_query_compiler.h"
#include "scratchbird/sblr/postgresql_query_compiler.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/bytecode_validator.h"
#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_opcode_registry.h"
#include "test_helpers.h"

#include <memory>
#include <string>
#include <vector>

using scratchbird::core::Database;
using scratchbird::sblr::FirebirdQueryCompiler;
using scratchbird::sblr::MySQLQueryCompiler;
using scratchbird::sblr::PostgreSQLQueryCompiler;
using scratchbird::sblr::QueryCompilerV3;
namespace sblr_v3 = scratchbird::sblr::v3;
using scratchbird::testing::TestDatabaseFile;

namespace {

bool containsOpcode(const std::vector<uint8_t>& bytecode, sblr_v3::Opcode opcode)
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
        if (!sblr_v3::decodeInstruction(container.bytecode_stream.data(),
                                         container.bytecode_stream.size(), offset,
                                         inst, decode_err))
        {
            break;
        }
        if (inst.opcode == static_cast<uint16_t>(opcode))
        {
            return true;
        }
    }
    return false;
}

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

bool containsLegacyOpcodeByte(const std::vector<uint8_t>& bytecode,
                              scratchbird::sblr::Opcode opcode)
{
    return std::find(bytecode.begin(),
                     bytecode.end(),
                     static_cast<uint8_t>(opcode)) != bytecode.end();
}

bool firstCreateTableColumnHasIdentity(const std::vector<uint8_t>& bytecode, bool* always_out = nullptr)
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
            return false;
        }

        if (inst.opcode != static_cast<uint16_t>(sblr_v3::Opcode::SBLR3_CREATE_TABLE))
        {
            continue;
        }

        const auto* payload = std::get_if<sblr_v3::Value::Object>(&inst.payload.data);
        if (!payload)
        {
            return false;
        }
        auto cols_it = payload->find("columns");
        if (cols_it == payload->end())
        {
            return false;
        }
        const auto* cols = std::get_if<sblr_v3::Value::List>(&cols_it->second.data);
        if (!cols || cols->empty())
        {
            return false;
        }
        const auto* first_col = std::get_if<sblr_v3::Value::Object>(&cols->front().data);
        if (!first_col)
        {
            return false;
        }
        auto identity_it = first_col->find("identity");
        if (identity_it == first_col->end() || identity_it->second.isNull())
        {
            return false;
        }
        const auto* identity = std::get_if<sblr_v3::Value::Object>(&identity_it->second.data);
        if (!identity)
        {
            return false;
        }
        if (always_out != nullptr)
        {
            auto always_it = identity->find("always");
            if (always_it != identity->end())
            {
                if (const auto* always = std::get_if<bool>(&always_it->second.data))
                {
                    *always_out = *always;
                }
            }
        }
        return true;
    }

    return false;
}

} // namespace

class RenameMoveOpcodeV3Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_rename_move_opcodes_v3");
        ASSERT_EQ(Database::create(db_file_->path(), 16384), scratchbird::core::Status::OK);
        db_ = std::make_unique<Database>();
        scratchbird::core::ErrorContext ctx;
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), scratchbird::core::Status::OK) << ctx.message;
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
};

TEST_F(RenameMoveOpcodeV3Test, PostgresRenameTableEmitsV3Opcode)
{
    PostgreSQLQueryCompiler compiler(db_.get());
    auto result = compiler.compile("ALTER TABLE IF EXISTS foo RENAME TO bar");
    ASSERT_TRUE(result.success()) << (result.errors().empty() ? "" : result.errors()[0]);
    EXPECT_TRUE(containsOpcode(result.bytecode(), sblr_v3::Opcode::SBLR3_RENAME_OBJECT));
}

TEST_F(RenameMoveOpcodeV3Test, PostgresMoveTableEmitsV3Opcode)
{
    PostgreSQLQueryCompiler compiler(db_.get());
    auto result = compiler.compile("ALTER TABLE IF EXISTS foo SET SCHEMA app");
    ASSERT_TRUE(result.success()) << (result.errors().empty() ? "" : result.errors()[0]);
    EXPECT_TRUE(containsOpcode(result.bytecode(), sblr_v3::Opcode::SBLR3_MOVE_OBJECT));
}

TEST_F(RenameMoveOpcodeV3Test, MySqlRenameTableEmitsV3Opcode)
{
    MySQLQueryCompiler compiler(db_.get());
    auto result = compiler.compile("RENAME TABLE foo TO bar");
    ASSERT_TRUE(result.success()) << (result.errors().empty() ? "" : result.errors()[0]);
    EXPECT_TRUE(containsOpcode(result.bytecode(), sblr_v3::Opcode::SBLR3_RENAME_OBJECT));
}

TEST_F(RenameMoveOpcodeV3Test, MySqlMoveTableEmitsV3Opcode)
{
    MySQLQueryCompiler compiler(db_.get());
    auto result = compiler.compile("RENAME TABLE foo TO app.bar");
    ASSERT_TRUE(result.success()) << (result.errors().empty() ? "" : result.errors()[0]);
    EXPECT_TRUE(containsOpcode(result.bytecode(), sblr_v3::Opcode::SBLR3_MOVE_OBJECT));
}

TEST_F(RenameMoveOpcodeV3Test, FirebirdRenameColumnEmitsV3Opcode)
{
    FirebirdQueryCompiler compiler(db_.get());
    auto result = compiler.compile("ALTER TABLE foo ALTER COLUMN bar TO baz");
    ASSERT_TRUE(result.success()) << (result.errors().empty() ? "" : result.errors()[0]);
    EXPECT_TRUE(containsOpcode(result.bytecode(), sblr_v3::Opcode::SBLR3_RENAME_COLUMN) ||
                containsOpcode(result.bytecode(), sblr_v3::Opcode::SBLR3_RENAME_OBJECT));
}

TEST_F(RenameMoveOpcodeV3Test, FirebirdRenameDomainEmitsV3Opcode)
{
    FirebirdQueryCompiler compiler(db_.get());
    auto result = compiler.compile("ALTER DOMAIN foo TO bar");
    ASSERT_TRUE(result.success()) << (result.errors().empty() ? "" : result.errors()[0]);
    EXPECT_TRUE(containsOpcode(result.bytecode(), sblr_v3::Opcode::SBLR3_RENAME_OBJECT));
}

TEST_F(RenameMoveOpcodeV3Test, NativeRenameTableEmitsV3Opcode)
{
    QueryCompilerV3 compiler(db_.get());
    auto result = compiler.compile("ALTER TABLE IF EXISTS foo RENAME TO bar");
    ASSERT_TRUE(result.success()) << (result.errors().empty() ? "" : result.errors()[0]);
    EXPECT_TRUE(containsOpcode(result.bytecode(), sblr_v3::Opcode::SBLR3_RENAME_OBJECT));
}

TEST_F(RenameMoveOpcodeV3Test, PostgresCompactArithmeticBytecodeValidWithNullDbContext)
{
    PostgreSQLQueryCompiler compiler(nullptr);

    auto compact = compiler.compile("SELECT 10+5;");
    ASSERT_TRUE(compact.success()) << (compact.errors().empty() ? "" : compact.errors()[0]);

    auto spaced = compiler.compile("SELECT 10 + 5;");
    ASSERT_TRUE(spaced.success()) << (spaced.errors().empty() ? "" : spaced.errors()[0]);

    scratchbird::core::ErrorContext compact_ctx;
    auto compact_status = scratchbird::sblr::validateBytecode(compact.bytecode(), &compact_ctx);
    EXPECT_EQ(compact_status, scratchbird::core::Status::OK) << compact_ctx.message;

    scratchbird::core::ErrorContext spaced_ctx;
    auto spaced_status = scratchbird::sblr::validateBytecode(spaced.bytecode(), &spaced_ctx);
    EXPECT_EQ(spaced_status, scratchbird::core::Status::OK) << spaced_ctx.message;
}

TEST_F(RenameMoveOpcodeV3Test, PostgresExistsCompilesToSubqueryExistsOpcode)
{
    PostgreSQLQueryCompiler compiler(db_.get());
    auto result = compiler.compile("SELECT EXISTS(SELECT 1)");
    ASSERT_TRUE(result.success()) << (result.errors().empty() ? "" : result.errors()[0]);
    EXPECT_TRUE(containsOpcodeDeep(result.bytecode(), sblr_v3::Opcode::SBLR3_SUBQUERY_EXISTS));
}

TEST_F(RenameMoveOpcodeV3Test, PostgresRepeatCompilesToDedicatedRepeatOpcode)
{
    PostgreSQLQueryCompiler compiler(db_.get());
    auto result = compiler.compile("SELECT repeat('ab', 3)");
    ASSERT_TRUE(result.success()) << (result.errors().empty() ? "" : result.errors()[0]);
    EXPECT_TRUE(containsOpcodeDeep(result.bytecode(), sblr_v3::Opcode::SBLR3_REPEAT));
    EXPECT_FALSE(containsOpcodeDeep(result.bytecode(), sblr_v3::Opcode::SBLR3_EXPR_FUNCTION_CALL));
}

TEST_F(RenameMoveOpcodeV3Test, PostgresSerialColumnsEmitIdentityOpcode)
{
    PostgreSQLQueryCompiler compiler(db_.get());
    auto result = compiler.compile(
        "CREATE TABLE serial_test (id SERIAL PRIMARY KEY, payload TEXT)");
    ASSERT_TRUE(result.success()) << (result.errors().empty() ? "" : result.errors()[0]);
    bool always = true;
    EXPECT_TRUE(firstCreateTableColumnHasIdentity(result.bytecode(), &always));
    EXPECT_FALSE(always);
}
