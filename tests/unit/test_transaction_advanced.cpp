// test_transaction_advanced.cpp - Tests for Phase 3 Task 3.6 Advanced Transaction Features
// Tests RESERVING clause, LOCK TIMEOUT, and SET TRANSACTION syntax

#include <gtest/gtest.h>
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/ast.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/sblr/opcodes.h"
#include <vector>
#include <string>
#include <sstream>

using namespace scratchbird;
using namespace scratchbird::parser;
using namespace scratchbird::sblr;

// Test fixture with helper methods
class TransactionAdvancedTest : public ::testing::Test
{
protected:
    std::string generateAndDisassemble(const std::string& sql)
    {
        Lexer lexer(sql, pool_);
        Parser parser(lexer, pool_);
        auto stmt = parser.parse();

        if (stmt == nullptr)
        {
            std::stringstream ss;
            ss << "Parse errors:\n";
            for (const auto& err : parser.getErrors())
            {
                ss << "  " << err << "\n";
            }
            return ss.str();
        }

        BytecodeGenerator generator(pool_);
        auto result = generator.generate(stmt.get());

        if (!result.success())
        {
            std::stringstream ss;
            ss << "Bytecode generation errors:\n";
            for (const auto& err : result.errors())
            {
                ss << "  " << err << "\n";
            }
            return ss.str();
        }

        return BytecodeDisassembler::disassemble(result.bytecode());
    }

    std::unique_ptr<Statement> parseSQL(const std::string& sql)
    {
        Lexer lexer(sql, pool_);
        Parser parser(lexer, pool_);
        auto stmt = parser.parse();
        EXPECT_TRUE(stmt != nullptr);
        if (stmt == nullptr && !parser.getErrors().empty())
        {
            ADD_FAILURE() << "Parse failed: " << parser.getErrors()[0];
        }
        return stmt;
    }

    StringPool pool_;
};

// Test basic START TRANSACTION with LOCK TIMEOUT
TEST_F(TransactionAdvancedTest, StartTransactionWithLockTimeout)
{
    auto stmt = parseSQL("START TRANSACTION LOCK TIMEOUT 30;");

    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->kind(), ASTKind::START_TRANSACTION);

    auto* start_txn = static_cast<StartTransactionStmt*>(stmt.get());
    EXPECT_EQ(start_txn->lockTimeout(), 30u);
    EXPECT_TRUE(start_txn->tableReservations().empty());
}

// Test START TRANSACTION with RESERVING clause - single table
TEST_F(TransactionAdvancedTest, StartTransactionWithReservingSingleTable)
{
    auto stmt = parseSQL("START TRANSACTION RESERVING employees FOR SHARED READ;");

    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->kind(), ASTKind::START_TRANSACTION);

    auto* start_txn = static_cast<StartTransactionStmt*>(stmt.get());
    const auto& reservations = start_txn->tableReservations();

    ASSERT_EQ(reservations.size(), 1u);
    EXPECT_EQ(pool_.get(reservations[0].table_name), "employees");
    EXPECT_EQ(reservations[0].lock_mode, TableLockMode::SHARED);
    EXPECT_FALSE(reservations[0].for_write);
}

// Test START TRANSACTION with RESERVING clause - multiple tables
TEST_F(TransactionAdvancedTest, StartTransactionWithReservingMultipleTables)
{
    auto stmt = parseSQL(
        "START TRANSACTION RESERVING "
        "employees FOR SHARED READ, "
        "departments FOR PROTECTED WRITE, "
        "salaries FOR SHARED WRITE;"
    );

    ASSERT_NE(stmt, nullptr);
    auto* start_txn = static_cast<StartTransactionStmt*>(stmt.get());
    const auto& reservations = start_txn->tableReservations();

    ASSERT_EQ(reservations.size(), 3u);

    // First reservation: employees FOR SHARED READ
    EXPECT_EQ(pool_.get(reservations[0].table_name), "employees");
    EXPECT_EQ(reservations[0].lock_mode, TableLockMode::SHARED);
    EXPECT_FALSE(reservations[0].for_write);

    // Second reservation: departments FOR PROTECTED WRITE
    EXPECT_EQ(pool_.get(reservations[1].table_name), "departments");
    EXPECT_EQ(reservations[1].lock_mode, TableLockMode::PROTECTED);
    EXPECT_TRUE(reservations[1].for_write);

    // Third reservation: salaries FOR SHARED WRITE
    EXPECT_EQ(pool_.get(reservations[2].table_name), "salaries");
    EXPECT_EQ(reservations[2].lock_mode, TableLockMode::SHARED);
    EXPECT_TRUE(reservations[2].for_write);
}

// Test START TRANSACTION with both LOCK TIMEOUT and RESERVING
TEST_F(TransactionAdvancedTest, StartTransactionWithLockTimeoutAndReserving)
{
    auto stmt = parseSQL(
        "START TRANSACTION LOCK TIMEOUT 60 "
        "RESERVING employees FOR SHARED READ;"
    );

    ASSERT_NE(stmt, nullptr);
    auto* start_txn = static_cast<StartTransactionStmt*>(stmt.get());

    EXPECT_EQ(start_txn->lockTimeout(), 60u);
    ASSERT_EQ(start_txn->tableReservations().size(), 1u);
    EXPECT_EQ(pool_.get(start_txn->tableReservations()[0].table_name), "employees");
}

// Test START TRANSACTION with all parameters
TEST_F(TransactionAdvancedTest, StartTransactionWithAllParameters)
{
    auto stmt = parseSQL(
        "START TRANSACTION READ ONLY "
        "ISOLATION LEVEL SNAPSHOT TABLE STABILITY "
        "NO WAIT "
        "LOCK TIMEOUT 120 "
        "RESERVING accounts FOR PROTECTED READ;"
    );

    ASSERT_NE(stmt, nullptr);
    auto* start_txn = static_cast<StartTransactionStmt*>(stmt.get());

    EXPECT_EQ(start_txn->mode(), TransactionMode::READ_ONLY);
    EXPECT_EQ(start_txn->isolation(), IsolationLevel::SNAPSHOT_TABLE_STABILITY);
    EXPECT_FALSE(start_txn->wait());
    EXPECT_EQ(start_txn->lockTimeout(), 120u);
    ASSERT_EQ(start_txn->tableReservations().size(), 1u);
}

// Test SET TRANSACTION basic syntax
TEST_F(TransactionAdvancedTest, SetTransactionBasic)
{
    auto stmt = parseSQL("SET TRANSACTION READ ONLY;");

    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->kind(), ASTKind::SET_TRANSACTION);

    auto* set_txn = static_cast<SetTransactionStmt*>(stmt.get());
    EXPECT_EQ(set_txn->mode(), TransactionMode::READ_ONLY);
}

// Test SET TRANSACTION with LOCK TIMEOUT
TEST_F(TransactionAdvancedTest, SetTransactionWithLockTimeout)
{
    auto stmt = parseSQL("SET TRANSACTION LOCK TIMEOUT 45;");

    ASSERT_NE(stmt, nullptr);
    auto* set_txn = static_cast<SetTransactionStmt*>(stmt.get());
    EXPECT_EQ(set_txn->lockTimeout(), 45u);
}

// Test SET TRANSACTION with RESERVING clause
TEST_F(TransactionAdvancedTest, SetTransactionWithReserving)
{
    auto stmt = parseSQL(
        "SET TRANSACTION RESERVING orders FOR PROTECTED WRITE;"
    );

    ASSERT_NE(stmt, nullptr);
    auto* set_txn = static_cast<SetTransactionStmt*>(stmt.get());

    const auto& reservations = set_txn->tableReservations();
    ASSERT_EQ(reservations.size(), 1u);
    EXPECT_EQ(pool_.get(reservations[0].table_name), "orders");
    EXPECT_EQ(reservations[0].lock_mode, TableLockMode::PROTECTED);
    EXPECT_TRUE(reservations[0].for_write);
}

// Test SET TRANSACTION with all parameters
TEST_F(TransactionAdvancedTest, SetTransactionWithAllParameters)
{
    auto stmt = parseSQL(
        "SET TRANSACTION READ WRITE "
        "ISOLATION LEVEL SNAPSHOT "
        "LOCK TIMEOUT 90 "
        "RESERVING products FOR SHARED READ, inventory FOR PROTECTED WRITE;"
    );

    ASSERT_NE(stmt, nullptr);
    auto* set_txn = static_cast<SetTransactionStmt*>(stmt.get());

    EXPECT_EQ(set_txn->mode(), TransactionMode::READ_WRITE);
    EXPECT_EQ(set_txn->isolation(), IsolationLevel::SNAPSHOT);
    EXPECT_EQ(set_txn->lockTimeout(), 90u);

    const auto& reservations = set_txn->tableReservations();
    ASSERT_EQ(reservations.size(), 2u);
}

// Test bytecode generation for START TRANSACTION with LOCK TIMEOUT
TEST_F(TransactionAdvancedTest, BytecodeStartTransactionWithLockTimeout)
{
    std::string disasm = generateAndDisassemble("START TRANSACTION LOCK TIMEOUT 30;");

    EXPECT_NE(disasm.find("VERSION 1"), std::string::npos);
    EXPECT_NE(disasm.find("START_TRANSACTION"), std::string::npos);
    // Lock timeout will be in the bytecode output
}

// Test bytecode generation for START TRANSACTION with RESERVING
TEST_F(TransactionAdvancedTest, BytecodeStartTransactionWithReserving)
{
    std::string disasm = generateAndDisassemble(
        "START TRANSACTION RESERVING employees FOR SHARED READ;"
    );

    EXPECT_NE(disasm.find("START_TRANSACTION"), std::string::npos);
    EXPECT_NE(disasm.find("TABLE_REF \"employees\""), std::string::npos);
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

    EXPECT_NE(disasm.find("START_TRANSACTION"), std::string::npos);
    EXPECT_NE(disasm.find("TABLE_REF \"t1\""), std::string::npos);
    EXPECT_NE(disasm.find("TABLE_REF \"t2\""), std::string::npos);
}

// Test error handling - invalid lock timeout syntax
TEST_F(TransactionAdvancedTest, ErrorInvalidLockTimeout)
{
    Lexer lexer("START TRANSACTION LOCK TIMEOUT abc;", pool_);
    Parser parser(lexer, pool_);
    auto stmt = parser.parse();

    // Should fail - lock timeout requires an integer
    EXPECT_EQ(stmt, nullptr);
    EXPECT_FALSE(parser.getErrors().empty());
}

// Test error handling - invalid RESERVING syntax
TEST_F(TransactionAdvancedTest, ErrorInvalidReservingSyntax)
{
    Lexer lexer("START TRANSACTION RESERVING employees;", pool_);
    Parser parser(lexer, pool_);
    auto stmt = parser.parse();

    // Should fail - missing FOR keyword
    EXPECT_EQ(stmt, nullptr);
    EXPECT_FALSE(parser.getErrors().empty());
}

// Test error handling - invalid lock mode
TEST_F(TransactionAdvancedTest, ErrorInvalidLockMode)
{
    Lexer lexer("START TRANSACTION RESERVING employees FOR EXCLUSIVE READ;", pool_);
    Parser parser(lexer, pool_);
    auto stmt = parser.parse();

    // Should fail - EXCLUSIVE is not a valid lock mode
    EXPECT_EQ(stmt, nullptr);
    EXPECT_FALSE(parser.getErrors().empty());
}

// Test COMMIT and ROLLBACK bytecode
TEST_F(TransactionAdvancedTest, BytecodeCommitAndRollback)
{
    // Test COMMIT
    {
        std::string disasm = generateAndDisassemble("COMMIT;");
        EXPECT_NE(disasm.find("VERSION 1"), std::string::npos);
        EXPECT_NE(disasm.find("COMMIT"), std::string::npos);
    }

    // Test ROLLBACK
    {
        std::string disasm = generateAndDisassemble("ROLLBACK;");
        EXPECT_NE(disasm.find("VERSION 1"), std::string::npos);
        EXPECT_NE(disasm.find("ROLLBACK"), std::string::npos);
    }
}

// Test PROTECTED READ mode
TEST_F(TransactionAdvancedTest, ProtectedReadMode)
{
    auto stmt = parseSQL(
        "START TRANSACTION RESERVING data FOR PROTECTED READ;"
    );

    ASSERT_NE(stmt, nullptr);
    auto* start_txn = static_cast<StartTransactionStmt*>(stmt.get());
    const auto& reservations = start_txn->tableReservations();

    ASSERT_EQ(reservations.size(), 1u);
    EXPECT_EQ(reservations[0].lock_mode, TableLockMode::PROTECTED);
    EXPECT_FALSE(reservations[0].for_write);
}
