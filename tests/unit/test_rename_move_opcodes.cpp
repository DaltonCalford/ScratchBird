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
