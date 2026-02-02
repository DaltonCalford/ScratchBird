/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// test_transaction_advanced.cpp - Tests for Phase 3 Task 3.6 Advanced Transaction Features
// Tests RESERVING clause, LOCK TIMEOUT, and SET TRANSACTION syntax

#include <gtest/gtest.h>

#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/parser/parser_v2.h"
#include "scratchbird/sblr/bytecode_generator_v2.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "test_helpers.h"
#include <memory>
#include <sstream>
#include <string>

using namespace scratchbird;
using namespace scratchbird::parser::v2;
using namespace scratchbird::sblr;
using namespace scratchbird::testing;

// Test fixture with helper methods
class TransactionAdvancedTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("txn_advanced");

        core::ErrorContext ctx;
        auto status = core::Database::create(db_file_->path(), 16384, &ctx);
        ASSERT_EQ(status, core::Status::OK) << ctx.message;

        db_ = std::make_unique<core::Database>();
        status = db_->open(db_file_->path(), &ctx);
        ASSERT_EQ(status, core::Status::OK) << ctx.message;

        compiler_ = std::make_unique<QueryCompilerV2>(db_.get());
    }

    void TearDown() override
    {
        compiler_.reset();
        db_.reset();
        db_file_.reset();
    }

    std::string generateAndDisassemble(const std::string& sql)
    {
        auto result = compiler_->compile(sql);
        if (!result.success())
        {
            std::stringstream ss;
            ss << "Compile errors:\n";
            for (const auto& err : result.errors())
            {
                ss << "  " << err << "\n";
            }
            return ss.str();
        }

        return BytecodeDisassemblerV2::disassemble(result.bytecode());
    }

    struct ParsedSQL {
        Statement* stmt = nullptr;
        std::unique_ptr<Parser> parser;
    };

    ParsedSQL parseSQL(const std::string& sql)
    {
        auto parser = std::make_unique<Parser>(sql);
        auto result = parser->parseStatement();
        EXPECT_TRUE(result.success());
        if (!result.success() && !result.errors().empty())
        {
            ADD_FAILURE() << "Parse failed: " << result.errors()[0].message;
        }
        return {result.statement(), std::move(parser)};
    }

private:
    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<core::Database> db_;
    std::unique_ptr<QueryCompilerV2> compiler_;
};

// Test basic START TRANSACTION with LOCK TIMEOUT
TEST_F(TransactionAdvancedTest, StartTransactionWithLockTimeout)
{
    auto parsed = parseSQL("START TRANSACTION LOCK TIMEOUT 30;");

    ASSERT_NE(parsed.stmt, nullptr);
    EXPECT_EQ(parsed.stmt->kind(), ASTKind::StartTransactionStmt);

    auto* start_txn = static_cast<StartTransactionStmt*>(parsed.stmt);
    EXPECT_TRUE(start_txn->has_lock_timeout);
    EXPECT_EQ(start_txn->lock_timeout_seconds, 30u);
    EXPECT_TRUE(start_txn->table_reservations.empty());
}

// Test START TRANSACTION with RESERVING clause - single table
TEST_F(TransactionAdvancedTest, StartTransactionWithReservingSingleTable)
{
    auto parsed = parseSQL("START TRANSACTION RESERVING employees FOR SHARED READ;");

    ASSERT_NE(parsed.stmt, nullptr);
    EXPECT_EQ(parsed.stmt->kind(), ASTKind::StartTransactionStmt);

    auto* start_txn = static_cast<StartTransactionStmt*>(parsed.stmt);
    const auto& reservations = start_txn->table_reservations;

    ASSERT_EQ(reservations.size(), 1u);
    EXPECT_EQ(parsed.parser->stringPool().get(reservations[0].table_name), "employees");
    EXPECT_EQ(reservations[0].lock_mode, TableLockMode::SHARED);
    EXPECT_FALSE(reservations[0].for_write);
}

// Test START TRANSACTION with RESERVING clause - multiple tables
TEST_F(TransactionAdvancedTest, StartTransactionWithReservingMultipleTables)
{
    auto parsed = parseSQL(
        "START TRANSACTION RESERVING "
        "employees FOR SHARED READ, "
        "departments FOR PROTECTED WRITE, "
        "salaries FOR SHARED WRITE;"
    );

    ASSERT_NE(parsed.stmt, nullptr);
    auto* start_txn = static_cast<StartTransactionStmt*>(parsed.stmt);
    const auto& reservations = start_txn->table_reservations;

    ASSERT_EQ(reservations.size(), 3u);

    // First reservation: employees FOR SHARED READ
    EXPECT_EQ(parsed.parser->stringPool().get(reservations[0].table_name), "employees");
    EXPECT_EQ(reservations[0].lock_mode, TableLockMode::SHARED);
    EXPECT_FALSE(reservations[0].for_write);

    // Second reservation: departments FOR PROTECTED WRITE
    EXPECT_EQ(parsed.parser->stringPool().get(reservations[1].table_name), "departments");
    EXPECT_EQ(reservations[1].lock_mode, TableLockMode::PROTECTED);
    EXPECT_TRUE(reservations[1].for_write);

    // Third reservation: salaries FOR SHARED WRITE
    EXPECT_EQ(parsed.parser->stringPool().get(reservations[2].table_name), "salaries");
    EXPECT_EQ(reservations[2].lock_mode, TableLockMode::SHARED);
    EXPECT_TRUE(reservations[2].for_write);
}

// Test START TRANSACTION with both LOCK TIMEOUT and RESERVING
TEST_F(TransactionAdvancedTest, StartTransactionWithLockTimeoutAndReserving)
{
    auto parsed = parseSQL(
        "START TRANSACTION LOCK TIMEOUT 60 "
        "RESERVING employees FOR SHARED READ;"
    );

    ASSERT_NE(parsed.stmt, nullptr);
    auto* start_txn = static_cast<StartTransactionStmt*>(parsed.stmt);

    EXPECT_TRUE(start_txn->has_lock_timeout);
    EXPECT_EQ(start_txn->lock_timeout_seconds, 60u);
    ASSERT_EQ(start_txn->table_reservations.size(), 1u);
    EXPECT_EQ(parsed.parser->stringPool().get(start_txn->table_reservations[0].table_name), "employees");
}

// Test START TRANSACTION with all parameters
// Parser expects clauses in order: READ ONLY, NOT WAIT, ISOLATION LEVEL, LOCK TIMEOUT, RESERVING
TEST_F(TransactionAdvancedTest, StartTransactionWithAllParameters)
{
    auto parsed = parseSQL(
        "START TRANSACTION READ ONLY "
        "NOT WAIT "
        "ISOLATION LEVEL SNAPSHOT TABLE STABILITY "
        "LOCK TIMEOUT 120 "
        "RESERVING accounts FOR PROTECTED READ;"
    );

    ASSERT_NE(parsed.stmt, nullptr);
    auto* start_txn = static_cast<StartTransactionStmt*>(parsed.stmt);

    EXPECT_TRUE(start_txn->has_access_mode);
    EXPECT_EQ(start_txn->access_mode, TransactionAccess::READ_ONLY);
    EXPECT_TRUE(start_txn->has_isolation_level);
    EXPECT_EQ(start_txn->isolation_level, IsolationLevel::SERIALIZABLE);
    EXPECT_TRUE(start_txn->has_wait_mode);
    EXPECT_EQ(start_txn->wait_mode, TransactionWaitMode::NO_WAIT);
    EXPECT_TRUE(start_txn->has_lock_timeout);
    EXPECT_EQ(start_txn->lock_timeout_seconds, 120u);
    ASSERT_EQ(start_txn->table_reservations.size(), 1u);
}

// Test SET TRANSACTION basic syntax
TEST_F(TransactionAdvancedTest, SetTransactionBasic)
{
    auto parsed = parseSQL("SET TRANSACTION READ ONLY;");

    ASSERT_NE(parsed.stmt, nullptr);
    EXPECT_EQ(parsed.stmt->kind(), ASTKind::SetStmt);

    auto* set_txn = static_cast<SetStmt*>(parsed.stmt);
    EXPECT_EQ(set_txn->set_type, SetStmt::SetType::TRANSACTION);
    EXPECT_TRUE(set_txn->has_access_mode);
    EXPECT_EQ(set_txn->access_mode, TransactionAccess::READ_ONLY);
}

// Test SET TRANSACTION with LOCK TIMEOUT
TEST_F(TransactionAdvancedTest, SetTransactionWithLockTimeout)
{
    auto parsed = parseSQL("SET TRANSACTION LOCK TIMEOUT 45;");

    ASSERT_NE(parsed.stmt, nullptr);
    auto* set_txn = static_cast<SetStmt*>(parsed.stmt);
    EXPECT_TRUE(set_txn->has_lock_timeout);
    EXPECT_EQ(set_txn->lock_timeout_seconds, 45u);
}

// Test SET TRANSACTION with RESERVING clause
TEST_F(TransactionAdvancedTest, SetTransactionWithReserving)
{
    auto parsed = parseSQL(
        "SET TRANSACTION RESERVING orders FOR PROTECTED WRITE;"
    );

    ASSERT_NE(parsed.stmt, nullptr);
    auto* set_txn = static_cast<SetStmt*>(parsed.stmt);

    const auto& reservations = set_txn->table_reservations;
    ASSERT_EQ(reservations.size(), 1u);
    EXPECT_EQ(parsed.parser->stringPool().get(reservations[0].table_name), "orders");
    EXPECT_EQ(reservations[0].lock_mode, TableLockMode::PROTECTED);
    EXPECT_TRUE(reservations[0].for_write);
}

// Test SET TRANSACTION with all parameters
TEST_F(TransactionAdvancedTest, SetTransactionWithAllParameters)
{
    auto parsed = parseSQL(
        "SET TRANSACTION READ WRITE "
        "ISOLATION LEVEL SNAPSHOT "
        "LOCK TIMEOUT 90 "
        "RESERVING products FOR SHARED READ, inventory FOR PROTECTED WRITE;"
    );

    ASSERT_NE(parsed.stmt, nullptr);
    auto* set_txn = static_cast<SetStmt*>(parsed.stmt);

    EXPECT_TRUE(set_txn->has_access_mode);
    EXPECT_EQ(set_txn->access_mode, TransactionAccess::READ_WRITE);
    EXPECT_TRUE(set_txn->has_isolation_level);
    EXPECT_EQ(set_txn->isolation_level, IsolationLevel::REPEATABLE_READ);
    EXPECT_TRUE(set_txn->has_lock_timeout);
    EXPECT_EQ(set_txn->lock_timeout_seconds, 90u);

    const auto& reservations = set_txn->table_reservations;
    ASSERT_EQ(reservations.size(), 2u);
}

// Test bytecode generation for START TRANSACTION with LOCK TIMEOUT
TEST_F(TransactionAdvancedTest, BytecodeStartTransactionWithLockTimeout)
{
    std::string disasm = generateAndDisassemble("START TRANSACTION LOCK TIMEOUT 30;");

    EXPECT_NE(disasm.find("VERSION 2"), std::string::npos);
    EXPECT_NE(disasm.find("START_TRANSACTION"), std::string::npos);
}

// Test bytecode generation for START TRANSACTION with RESERVING
TEST_F(TransactionAdvancedTest, BytecodeStartTransactionWithReserving)
{
    std::string disasm = generateAndDisassemble(
        "START TRANSACTION RESERVING employees FOR SHARED READ;"
    );

    EXPECT_NE(disasm.find("START_TRANSACTION"), std::string::npos);
    EXPECT_NE(disasm.find("TABLE_REF"), std::string::npos)
        << "Should have TABLE_REF opcode for reserved table";
    EXPECT_NE(disasm.find("employees"), std::string::npos)
        << "Should reference employees table. Disasm: " << disasm;
}

// Test bytecode generation for SET TRANSACTION
TEST_F(TransactionAdvancedTest, BytecodeSetTransaction)
{
    std::string disasm = generateAndDisassemble(
        "SET TRANSACTION READ ONLY LOCK TIMEOUT 60;"
    );

    EXPECT_NE(disasm.find("SET_TRANSACTION"), std::string::npos);
}

// Test bytecode generation with multiple table reservations
TEST_F(TransactionAdvancedTest, BytecodeMultipleReservations)
{
    std::string disasm = generateAndDisassemble(
        "START TRANSACTION RESERVING "
        "t1 FOR SHARED READ, "
        "t2 FOR PROTECTED WRITE;"
    );

    EXPECT_NE(disasm.find("START_TRANSACTION"), std::string::npos)
        << "Disasm: " << disasm;
    EXPECT_NE(disasm.find("TABLE_REF \"t1\""), std::string::npos);
    EXPECT_NE(disasm.find("TABLE_REF \"t2\""), std::string::npos);
}

// Test error handling - invalid lock timeout syntax
TEST_F(TransactionAdvancedTest, ErrorInvalidLockTimeout)
{
    Parser parser("START TRANSACTION LOCK TIMEOUT abc;");
    auto result = parser.parseStatement();

    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.errors().empty());
}

// Test error handling - invalid RESERVING syntax
TEST_F(TransactionAdvancedTest, ErrorInvalidReservingSyntax)
{
    Parser parser("START TRANSACTION RESERVING employees;");
    auto result = parser.parseStatement();

    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.errors().empty());
}

// Test error handling - invalid lock mode
TEST_F(TransactionAdvancedTest, ErrorInvalidLockMode)
{
    Parser parser("START TRANSACTION RESERVING employees FOR EXCLUSIVE READ;");
    auto result = parser.parseStatement();

    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.errors().empty());
}

// Test COMMIT and ROLLBACK bytecode
TEST_F(TransactionAdvancedTest, BytecodeCommitAndRollback)
{
    {
        std::string disasm = generateAndDisassemble("COMMIT;");
        EXPECT_NE(disasm.find("VERSION 2"), std::string::npos);
        EXPECT_NE(disasm.find("COMMIT"), std::string::npos);
    }

    {
        std::string disasm = generateAndDisassemble("ROLLBACK;");
        EXPECT_NE(disasm.find("VERSION 2"), std::string::npos);
        EXPECT_NE(disasm.find("ROLLBACK"), std::string::npos);
    }
}

// Test PROTECTED READ mode
TEST_F(TransactionAdvancedTest, ProtectedReadMode)
{
    auto parsed = parseSQL(
        "START TRANSACTION RESERVING data FOR PROTECTED READ;"
    );

    ASSERT_NE(parsed.stmt, nullptr);
    auto* start_txn = static_cast<StartTransactionStmt*>(parsed.stmt);
    const auto& reservations = start_txn->table_reservations;

    ASSERT_EQ(reservations.size(), 1u);
    EXPECT_EQ(reservations[0].lock_mode, TableLockMode::PROTECTED);
    EXPECT_FALSE(reservations[0].for_write);
}
