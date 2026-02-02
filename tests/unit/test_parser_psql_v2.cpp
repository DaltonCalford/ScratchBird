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

#include "scratchbird/parser/parser_v2.h"

using namespace scratchbird::parser::v2;

class ParserPSQLV2Test : public ::testing::Test {
protected:
    Parser& createParser(const std::string& sql) {
        input_ = sql;
        parser_ = std::make_unique<Parser>(input_);
        return *parser_;
    }

    ParseResult parse(const std::string& sql) {
        return createParser(sql).parseStatement();
    }

    template <typename T>
    T* parseAs(const std::string& sql) {
        auto result = parse(sql);
        EXPECT_TRUE(result.success()) << "Parse failed for: " << sql;
        if (!result.success()) {
            for (const auto& err : result.errors()) {
                std::cerr << "Error: " << err.message << "\n";
            }
            return nullptr;
        }
        auto* stmt = result.statement();
        if (!stmt) {
            return nullptr;
        }
        return dynamic_cast<T*>(stmt);
    }

    std::string_view getString(StringPool::StringId id) const {
        return parser_ ? parser_->stringPool().get(id) : std::string_view{};
    }

private:
    std::string input_;
    std::unique_ptr<Parser> parser_;
};

TEST_F(ParserPSQLV2Test, CreateFunctionBasic) {
    auto* stmt = parseAs<CreateFunctionStmt>(
        "CREATE FUNCTION f(a INT) RETURNS INT AS BEGIN RETURN a; END");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->params.size(), 1u);
    EXPECT_NE(stmt->body, StringPool::INVALID_ID);
    EXPECT_FALSE(getString(stmt->body).empty());
}

TEST_F(ParserPSQLV2Test, CreateProcedureWithReturns) {
    auto* stmt = parseAs<CreateProcedureStmt>(
        "CREATE PROCEDURE p(a INT) RETURNS (b INT) AS BEGIN b := a; END");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->params.empty());
    EXPECT_NE(stmt->body, StringPool::INVALID_ID);
}

TEST_F(ParserPSQLV2Test, CreateTriggerExecuteProcedure) {
    auto* stmt = parseAs<CreateTriggerStmt>(
        "CREATE TRIGGER t AFTER INSERT ON users FOR EACH ROW EXECUTE PROCEDURE p() ");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->event_mask & (1u << static_cast<uint8_t>(TriggerEvent::INSERT)));
    EXPECT_NE(stmt->body, StringPool::INVALID_ID);
}

TEST_F(ParserPSQLV2Test, ExecuteBlockWithControlFlow) {
    auto* stmt = parseAs<ExecuteBlockStmt>(
        "EXECUTE BLOCK (a = 1) RETURNS (b INT) AS "
        "DECLARE VARIABLE c INT; "
        "BEGIN "
        "IF (a = 1) THEN b := a; "
        "WHILE (a < 3) DO a := a + 1; "
        "END");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->input_params.size(), 1u);
    EXPECT_EQ(stmt->output_params.size(), 1u);
    EXPECT_FALSE(stmt->variables.empty());
    auto* body = dynamic_cast<CompoundStmt*>(stmt->body);
    ASSERT_NE(body, nullptr);
    EXPECT_FALSE(body->statements.empty());
}

TEST_F(ParserPSQLV2Test, ExecuteProcedureStatement) {
    auto* stmt = parseAs<ExecuteProcedureStmt>(
        "EXECUTE PROCEDURE proc1(1, 2) RETURNING VALUES out1, out2");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->arguments.size(), 2u);
    EXPECT_EQ(stmt->returning_variables.size(), 2u);
}

TEST_F(ParserPSQLV2Test, ExecuteDynamicStatement) {
    auto* stmt = parseAs<ExecuteStatementStmt>(
        "EXECUTE STATEMENT 'SELECT 1' INTO v1, v2");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->into_variables.size(), 2u);
}
