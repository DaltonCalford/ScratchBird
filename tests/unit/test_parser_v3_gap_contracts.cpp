/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <string>

#include "scratchbird/parser/ast_v3.h"
#include "scratchbird/parser/parser_v3.h"

namespace {

using namespace scratchbird::parser::v3;

TEST(ParserV3GapContractsTest, ParsesAdvancedOperatorsWithRuntimeClosure) {
    {
        Parser parser("SELECT 'alpha' LIKE 'a%'");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "LIKE should parse";
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
    }

    {
        Parser parser("SELECT 1 IN (1, 2, 3)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "IN list should parse";
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
    }

    {
        Parser parser("SELECT 'alpha' ~ 'a.*'");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "regex operator should parse";
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
    }
}

TEST(ParserV3GapContractsTest, CountDistinctParsesAndBuildsSelectSurface) {
    Parser parser("SELECT COUNT(DISTINCT id) FROM t");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());
    ASSERT_NE(nullptr, result.statement());
    EXPECT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
}

TEST(ParserV3GapContractsTest, ParsesExplicitCastForms) {
    {
        Parser parser("SELECT CAST('48656c6c6f' AS BLOB USING HEX)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
    }

    {
        Parser parser("SELECT '123'::INT");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
    }
}

}  // namespace
