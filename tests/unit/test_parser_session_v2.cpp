/**
 * Unit Tests for ScratchBird Parser v2.0 - Session & Transaction Statements
 *
 * Tests parsing of SET, SHOW, RESET, BEGIN, START TRANSACTION, COMMIT,
 * ROLLBACK, SAVEPOINT, RELEASE SAVEPOINT statements.
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/parser_v2.h"

using namespace scratchbird::parser::v2;

class ParserSessionV2Test : public ::testing::Test {
protected:
    void SetUp() override {
        parser_.reset();
    }

    Parser& createParser(const std::string& sql) {
        input_ = sql;
        parser_ = std::make_unique<Parser>(input_);
        return *parser_;
    }

    ParseResult parse(const std::string& sql) {
        return createParser(sql).parseStatement();
    }

    template<typename T>
    T* parseAs(const std::string& sql) {
        auto result = parse(sql);
        EXPECT_TRUE(result.success()) << "Parse failed for: " << sql;
        if (!result.success()) {
            for (const auto& err : result.errors()) {
                std::cerr << "Error: " << err.message << "\n";
            }
            return nullptr;
        }
        Statement* stmt = result.statement();
        if (!stmt) {
            std::cerr << "Null statement for: " << sql << "\n";
            return nullptr;
        }
        T* typedStmt = dynamic_cast<T*>(stmt);
        if (!typedStmt) {
            std::cerr << "Wrong statement type for: " << sql << ", got kind=" << static_cast<int>(stmt->kind()) << "\n";
        }
        return typedStmt;
    }

private:
    std::string input_;
    std::unique_ptr<Parser> parser_;
};

// =============================================================================
// START TRANSACTION / BEGIN Tests
// =============================================================================

TEST_F(ParserSessionV2Test, BeginTransaction) {
    auto* stmt = parseAs<StartTransactionStmt>("BEGIN");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->has_isolation_level);
    EXPECT_FALSE(stmt->has_access_mode);
}

TEST_F(ParserSessionV2Test, BeginTransactionExplicit) {
    auto* stmt = parseAs<StartTransactionStmt>("BEGIN TRANSACTION");
    ASSERT_NE(stmt, nullptr);
}

TEST_F(ParserSessionV2Test, BeginWork) {
    auto* stmt = parseAs<StartTransactionStmt>("BEGIN WORK");
    ASSERT_NE(stmt, nullptr);
}

TEST_F(ParserSessionV2Test, StartTransaction) {
    auto* stmt = parseAs<StartTransactionStmt>("START TRANSACTION");
    ASSERT_NE(stmt, nullptr);
}

TEST_F(ParserSessionV2Test, StartTransactionReadCommitted) {
    auto* stmt = parseAs<StartTransactionStmt>("START TRANSACTION ISOLATION LEVEL READ COMMITTED");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->has_isolation_level);
    EXPECT_EQ(stmt->isolation_level, IsolationLevel::READ_COMMITTED);
}

TEST_F(ParserSessionV2Test, StartTransactionSerializable) {
    auto* stmt = parseAs<StartTransactionStmt>("START TRANSACTION ISOLATION LEVEL SERIALIZABLE");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->has_isolation_level);
    EXPECT_EQ(stmt->isolation_level, IsolationLevel::SERIALIZABLE);
}

TEST_F(ParserSessionV2Test, StartTransactionRepeatableRead) {
    auto* stmt = parseAs<StartTransactionStmt>("START TRANSACTION ISOLATION LEVEL REPEATABLE READ");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->has_isolation_level);
    EXPECT_EQ(stmt->isolation_level, IsolationLevel::REPEATABLE_READ);
}

TEST_F(ParserSessionV2Test, StartTransactionReadUncommitted) {
    auto* stmt = parseAs<StartTransactionStmt>("START TRANSACTION ISOLATION LEVEL READ UNCOMMITTED");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->has_isolation_level);
    EXPECT_EQ(stmt->isolation_level, IsolationLevel::READ_UNCOMMITTED);
}

TEST_F(ParserSessionV2Test, StartTransactionReadOnly) {
    auto* stmt = parseAs<StartTransactionStmt>("START TRANSACTION READ ONLY");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->has_access_mode);
    EXPECT_EQ(stmt->access_mode, TransactionAccess::READ_ONLY);
}

TEST_F(ParserSessionV2Test, StartTransactionReadWrite) {
    auto* stmt = parseAs<StartTransactionStmt>("START TRANSACTION READ WRITE");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->has_access_mode);
    EXPECT_EQ(stmt->access_mode, TransactionAccess::READ_WRITE);
}

TEST_F(ParserSessionV2Test, StartTransactionDeferrable) {
    auto* stmt = parseAs<StartTransactionStmt>("START TRANSACTION DEFERRABLE");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->deferrable);
}

TEST_F(ParserSessionV2Test, StartTransactionNotDeferrable) {
    auto* stmt = parseAs<StartTransactionStmt>("START TRANSACTION NOT DEFERRABLE");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->not_deferrable);
}

TEST_F(ParserSessionV2Test, StartTransactionMultipleOptions) {
    auto* stmt = parseAs<StartTransactionStmt>("START TRANSACTION ISOLATION LEVEL SERIALIZABLE, READ ONLY, DEFERRABLE");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->has_isolation_level);
    EXPECT_EQ(stmt->isolation_level, IsolationLevel::SERIALIZABLE);
    EXPECT_TRUE(stmt->has_access_mode);
    EXPECT_EQ(stmt->access_mode, TransactionAccess::READ_ONLY);
    EXPECT_TRUE(stmt->deferrable);
}

// =============================================================================
// COMMIT Tests
// =============================================================================

TEST_F(ParserSessionV2Test, CommitBasic) {
    auto* stmt = parseAs<CommitStmt>("COMMIT");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->and_chain);
    EXPECT_FALSE(stmt->and_no_chain);
}

TEST_F(ParserSessionV2Test, CommitWork) {
    auto* stmt = parseAs<CommitStmt>("COMMIT WORK");
    ASSERT_NE(stmt, nullptr);
}

TEST_F(ParserSessionV2Test, CommitTransaction) {
    auto* stmt = parseAs<CommitStmt>("COMMIT TRANSACTION");
    ASSERT_NE(stmt, nullptr);
}

TEST_F(ParserSessionV2Test, CommitAndChain) {
    auto* stmt = parseAs<CommitStmt>("COMMIT AND CHAIN");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->and_chain);
}

TEST_F(ParserSessionV2Test, CommitAndNoChain) {
    auto* stmt = parseAs<CommitStmt>("COMMIT AND NO CHAIN");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->and_no_chain);
}

// =============================================================================
// ROLLBACK Tests
// =============================================================================

TEST_F(ParserSessionV2Test, RollbackBasic) {
    auto* stmt = parseAs<RollbackStmt>("ROLLBACK");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->to_savepoint);
    EXPECT_FALSE(stmt->and_chain);
}

TEST_F(ParserSessionV2Test, RollbackWork) {
    auto* stmt = parseAs<RollbackStmt>("ROLLBACK WORK");
    ASSERT_NE(stmt, nullptr);
}

TEST_F(ParserSessionV2Test, RollbackTransaction) {
    auto* stmt = parseAs<RollbackStmt>("ROLLBACK TRANSACTION");
    ASSERT_NE(stmt, nullptr);
}

TEST_F(ParserSessionV2Test, RollbackAndChain) {
    auto* stmt = parseAs<RollbackStmt>("ROLLBACK AND CHAIN");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->and_chain);
}

TEST_F(ParserSessionV2Test, RollbackAndNoChain) {
    auto* stmt = parseAs<RollbackStmt>("ROLLBACK AND NO CHAIN");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->and_no_chain);
}

TEST_F(ParserSessionV2Test, RollbackToSavepoint) {
    auto* stmt = parseAs<RollbackStmt>("ROLLBACK TO sp1");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->to_savepoint);
    EXPECT_NE(stmt->savepoint_name, StringPool::INVALID_ID);
}

TEST_F(ParserSessionV2Test, RollbackToSavepointExplicit) {
    auto* stmt = parseAs<RollbackStmt>("ROLLBACK TO SAVEPOINT sp1");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->to_savepoint);
}

// =============================================================================
// SAVEPOINT Tests
// =============================================================================

TEST_F(ParserSessionV2Test, SavepointBasic) {
    auto* stmt = parseAs<SavepointStmt>("SAVEPOINT my_savepoint");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->name, StringPool::INVALID_ID);
}

// =============================================================================
// RELEASE SAVEPOINT Tests
// =============================================================================

TEST_F(ParserSessionV2Test, ReleaseSavepointBasic) {
    auto* stmt = parseAs<ReleaseSavepointStmt>("RELEASE my_savepoint");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->name, StringPool::INVALID_ID);
}

TEST_F(ParserSessionV2Test, ReleaseSavepointExplicit) {
    auto* stmt = parseAs<ReleaseSavepointStmt>("RELEASE SAVEPOINT my_savepoint");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->name, StringPool::INVALID_ID);
}

// =============================================================================
// SET Statement Tests
// =============================================================================

TEST_F(ParserSessionV2Test, SetVariableEquals) {
    auto* stmt = parseAs<SetStmt>("SET search_path = public");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->set_type, SetStmt::SetType::VARIABLE);
    EXPECT_NE(stmt->name, StringPool::INVALID_ID);
    EXPECT_NE(stmt->value, nullptr);
}

TEST_F(ParserSessionV2Test, SetVariableTo) {
    auto* stmt = parseAs<SetStmt>("SET search_path TO public");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->set_type, SetStmt::SetType::VARIABLE);
}

TEST_F(ParserSessionV2Test, SetVariableToDefault) {
    auto* stmt = parseAs<SetStmt>("SET search_path TO DEFAULT");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->is_default);
}

TEST_F(ParserSessionV2Test, SetSession) {
    auto* stmt = parseAs<SetStmt>("SET SESSION search_path = public");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->scope, SetStmt::Scope::SESSION);
}

TEST_F(ParserSessionV2Test, SetLocal) {
    auto* stmt = parseAs<SetStmt>("SET LOCAL search_path = public");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->scope, SetStmt::Scope::LOCAL);
}

TEST_F(ParserSessionV2Test, SetTimeZone) {
    auto* stmt = parseAs<SetStmt>("SET TIME ZONE 'America/New_York'");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->set_type, SetStmt::SetType::TIME_ZONE);
    EXPECT_NE(stmt->value, nullptr);
}

TEST_F(ParserSessionV2Test, SetTimeZoneLocal) {
    auto* stmt = parseAs<SetStmt>("SET TIME ZONE LOCAL");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->set_type, SetStmt::SetType::TIME_ZONE);
    EXPECT_TRUE(stmt->is_default);
}

TEST_F(ParserSessionV2Test, SetTransaction) {
    auto* stmt = parseAs<SetStmt>("SET TRANSACTION ISOLATION LEVEL SERIALIZABLE");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->set_type, SetStmt::SetType::TRANSACTION);
    EXPECT_TRUE(stmt->has_isolation_level);
    EXPECT_EQ(stmt->isolation_level, IsolationLevel::SERIALIZABLE);
}

TEST_F(ParserSessionV2Test, SetTransactionReadOnly) {
    auto* stmt = parseAs<SetStmt>("SET TRANSACTION READ ONLY");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->set_type, SetStmt::SetType::TRANSACTION);
    EXPECT_TRUE(stmt->has_access_mode);
    EXPECT_EQ(stmt->access_mode, TransactionAccess::READ_ONLY);
}

TEST_F(ParserSessionV2Test, SetRole) {
    auto* stmt = parseAs<SetStmt>("SET ROLE admin");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->set_type, SetStmt::SetType::ROLE);
    EXPECT_NE(stmt->value, nullptr);
}

TEST_F(ParserSessionV2Test, SetRoleNone) {
    auto* stmt = parseAs<SetStmt>("SET ROLE NONE");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->set_type, SetStmt::SetType::ROLE);
    EXPECT_TRUE(stmt->is_default);
}

TEST_F(ParserSessionV2Test, SetSessionAuthorization) {
    auto* stmt = parseAs<SetStmt>("SET SESSION AUTHORIZATION admin");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->set_type, SetStmt::SetType::SESSION_AUTHORIZATION);
    EXPECT_NE(stmt->value, nullptr);
}

TEST_F(ParserSessionV2Test, SetVariableString) {
    auto* stmt = parseAs<SetStmt>("SET client_encoding = 'UTF8'");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->set_type, SetStmt::SetType::VARIABLE);
}

TEST_F(ParserSessionV2Test, SetVariableNumber) {
    auto* stmt = parseAs<SetStmt>("SET statement_timeout = 5000");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->set_type, SetStmt::SetType::VARIABLE);
}

// =============================================================================
// RESET Statement Tests
// =============================================================================

TEST_F(ParserSessionV2Test, ResetVariable) {
    auto* stmt = parseAs<ResetStmt>("RESET search_path");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->reset_all);
    EXPECT_NE(stmt->name, StringPool::INVALID_ID);
}

TEST_F(ParserSessionV2Test, ResetAll) {
    auto* stmt = parseAs<ResetStmt>("RESET ALL");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->reset_all);
}

// =============================================================================
// SHOW Statement Tests
// =============================================================================

TEST_F(ParserSessionV2Test, ShowVariable) {
    auto* stmt = parseAs<ShowStmt>("SHOW search_path");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->show_type, ShowStmt::ShowType::VARIABLE);
    EXPECT_NE(stmt->name, StringPool::INVALID_ID);
}

TEST_F(ParserSessionV2Test, ShowAll) {
    auto* stmt = parseAs<ShowStmt>("SHOW ALL");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->show_type, ShowStmt::ShowType::ALL);
}

TEST_F(ParserSessionV2Test, ShowTransactionIsolationLevel) {
    auto* stmt = parseAs<ShowStmt>("SHOW TRANSACTION ISOLATION LEVEL");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->show_type, ShowStmt::ShowType::TRANSACTION_ISOLATION_LEVEL);
}
