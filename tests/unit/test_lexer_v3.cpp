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
 * Unit tests for Lexer v3.0 - Gatekeeper Keyword Model
 *
 * Tests the "Smart Parser, Dumb Lexer" architecture where only ~35 keywords
 * are reserved (Gatekeepers) and all others are emitted as IDENTIFIER.
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/lexer_v3.h"

using namespace scratchbird::parser::v3;

class LexerV3Test : public ::testing::Test {
protected:
    void SetUp() override {
        error_reporter_ = std::make_unique<SimpleErrorReporter>();
    }

    std::unique_ptr<Lexer> makeLexer(std::string_view input) {
        auto lexer = std::make_unique<Lexer>(input);
        lexer->setErrorReporter(error_reporter_.get());
        return lexer;
    }

    std::vector<Token> tokenize(std::string_view input) {
        auto lexer = makeLexer(input);
        std::vector<Token> tokens;
        while (true) {
            Token t = lexer->nextToken();
            tokens.push_back(t);
            if (t.type == TokenType::END_OF_FILE || t.type == TokenType::ERROR) {
                break;
            }
        }
        return tokens;
    }

    std::unique_ptr<SimpleErrorReporter> error_reporter_;
};

// =============================================================================
// Basic Token Tests
// =============================================================================

TEST_F(LexerV3Test, EmptyInput) {
    auto tokens = tokenize("");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::END_OF_FILE);
}

TEST_F(LexerV3Test, WhitespaceOnly) {
    auto tokens = tokenize("   \t\n  ");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::END_OF_FILE);
}

TEST_F(LexerV3Test, SingleLineComment) {
    auto tokens = tokenize("-- this is a comment\nSELECT");
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::KW_SELECT);
    EXPECT_EQ(tokens[1].type, TokenType::END_OF_FILE);
}

TEST_F(LexerV3Test, BlockComment) {
    auto tokens = tokenize("/* comment */ SELECT /* another */");
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::KW_SELECT);
}

TEST_F(LexerV3Test, NestedBlockComment) {
    auto tokens = tokenize("/* outer /* nested */ end */ SELECT");
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::KW_SELECT);
}

// =============================================================================
// Gatekeeper Keyword Tests
// =============================================================================

TEST_F(LexerV3Test, GatekeeperKeywords_StatementInitiators) {
    // All statement initiators should be recognized as keywords
    auto test = [this](const char* text, TokenType expected) {
        auto tokens = tokenize(text);
        ASSERT_GE(tokens.size(), 1) << "Testing: " << text;
        EXPECT_EQ(tokens[0].type, expected) << "Testing: " << text;
    };

    test("SELECT", TokenType::KW_SELECT);
    test("INSERT", TokenType::KW_INSERT);
    test("UPDATE", TokenType::KW_UPDATE);
    test("DELETE", TokenType::KW_DELETE);
    test("MERGE", TokenType::KW_MERGE);
    test("CREATE", TokenType::KW_CREATE);
    test("ALTER", TokenType::KW_ALTER);
    test("DROP", TokenType::KW_DROP);
    test("TRUNCATE", TokenType::KW_TRUNCATE);
    test("GRANT", TokenType::KW_GRANT);
    test("REVOKE", TokenType::KW_REVOKE);
    test("COMMIT", TokenType::KW_COMMIT);
    test("ROLLBACK", TokenType::KW_ROLLBACK);
    test("BEGIN", TokenType::KW_BEGIN);
    test("END", TokenType::KW_END);
    test("DECLARE", TokenType::KW_DECLARE);
    test("SET", TokenType::KW_SET);
    test("SHOW", TokenType::KW_SHOW);
    test("EXPLAIN", TokenType::KW_EXPLAIN);
    test("ANALYZE", TokenType::KW_ANALYZE);
    test("CALL", TokenType::KW_CALL);
    test("EXECUTE", TokenType::KW_EXECUTE);
    test("PREPARE", TokenType::KW_PREPARE);
}

TEST_F(LexerV3Test, GatekeeperKeywords_ClauseInitiators) {
    auto test = [this](const char* text, TokenType expected) {
        auto tokens = tokenize(text);
        ASSERT_GE(tokens.size(), 1) << "Testing: " << text;
        EXPECT_EQ(tokens[0].type, expected) << "Testing: " << text;
    };

    test("FROM", TokenType::KW_FROM);
    test("WHERE", TokenType::KW_WHERE);
    test("GROUP", TokenType::KW_GROUP);
    test("HAVING", TokenType::KW_HAVING);
    test("ORDER", TokenType::KW_ORDER);
    test("LIMIT", TokenType::KW_LIMIT);
    test("OFFSET", TokenType::KW_OFFSET);
    test("UNION", TokenType::KW_UNION);
    test("INTERSECT", TokenType::KW_INTERSECT);
    test("EXCEPT", TokenType::KW_EXCEPT);
    test("WITH", TokenType::KW_WITH);
}

TEST_F(LexerV3Test, GatekeeperKeywords_ExpressionKeywords) {
    auto test = [this](const char* text, TokenType expected) {
        auto tokens = tokenize(text);
        ASSERT_GE(tokens.size(), 1) << "Testing: " << text;
        EXPECT_EQ(tokens[0].type, expected) << "Testing: " << text;
    };

    test("AND", TokenType::KW_AND);
    test("OR", TokenType::KW_OR);
    test("NOT", TokenType::KW_NOT);
    test("IS", TokenType::KW_IS);
    test("IN", TokenType::KW_IN);
    test("BETWEEN", TokenType::KW_BETWEEN);
    test("LIKE", TokenType::KW_LIKE);
    test("CASE", TokenType::KW_CASE);
    test("WHEN", TokenType::KW_WHEN);
    test("THEN", TokenType::KW_THEN);
    test("ELSE", TokenType::KW_ELSE);
    test("NULL", TokenType::KW_NULL);
    test("TRUE", TokenType::KW_TRUE);
    test("FALSE", TokenType::KW_FALSE);
    test("EXISTS", TokenType::KW_EXISTS);
    test("CAST", TokenType::KW_CAST);
    test("AS", TokenType::KW_AS);
}

TEST_F(LexerV3Test, GatekeeperKeywords_CaseInsensitive) {
    auto tokens1 = tokenize("select");
    EXPECT_EQ(tokens1[0].type, TokenType::KW_SELECT);

    auto tokens2 = tokenize("SELECT");
    EXPECT_EQ(tokens2[0].type, TokenType::KW_SELECT);

    auto tokens3 = tokenize("SeLeCt");
    EXPECT_EQ(tokens3[0].type, TokenType::KW_SELECT);
}

// =============================================================================
// Contextual Keywords (Now Identifiers) Tests
// =============================================================================

TEST_F(LexerV3Test, ContextualKeywords_TypeNames_AreIdentifiers) {
    // Type names should be emitted as IDENTIFIER, not keywords
    auto test = [this](const char* text) {
        auto tokens = tokenize(text);
        ASSERT_GE(tokens.size(), 1) << "Testing: " << text;
        EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER) << "Testing: " << text;
    };

    test("INTEGER");
    test("VARCHAR");
    test("TEXT");
    test("BOOLEAN");
    test("DATE");
    test("TIME");
    test("TIMESTAMP");
    test("UUID");
    test("JSON");
    test("JSONB");
}

TEST_F(LexerV3Test, ContextualKeywords_DDLObjects_AreIdentifiers) {
    auto test = [this](const char* text) {
        auto tokens = tokenize(text);
        ASSERT_GE(tokens.size(), 1) << "Testing: " << text;
        EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER) << "Testing: " << text;
    };

    test("TABLE");
    test("VIEW");
    test("INDEX");
    test("SEQUENCE");
    test("FUNCTION");
    test("PROCEDURE");
    test("TRIGGER");
    test("SCHEMA");
    test("DATABASE");
}

TEST_F(LexerV3Test, ContextualKeywords_CommonNames_AreIdentifiers) {
    // These common names should be usable as column names without quoting
    auto test = [this](const char* text) {
        auto tokens = tokenize(text);
        ASSERT_GE(tokens.size(), 1) << "Testing: " << text;
        EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER) << "Testing: " << text;
    };

    test("name");
    test("type");
    test("value");
    test("data");
    test("status");
    test("user");
    test("role");
    test("count");
    test("sum");
    test("avg");
    test("min");
    test("max");
    test("first");
    test("last");
    test("key");
    test("primary");
    test("foreign");
}

// =============================================================================
// Real SQL Statement Tests
// =============================================================================

TEST_F(LexerV3Test, SelectStatement_Basic) {
    auto lexer = makeLexer("SELECT id, name FROM users WHERE active = TRUE");

    EXPECT_EQ(lexer->nextToken().type, TokenType::KW_SELECT);

    auto id_token = lexer->nextToken();
    EXPECT_EQ(id_token.type, TokenType::IDENTIFIER);

    EXPECT_EQ(lexer->nextToken().type, TokenType::COMMA);

    auto name_token = lexer->nextToken();
    EXPECT_EQ(name_token.type, TokenType::IDENTIFIER);

    EXPECT_EQ(lexer->nextToken().type, TokenType::KW_FROM);

    auto users_token = lexer->nextToken();
    EXPECT_EQ(users_token.type, TokenType::IDENTIFIER);

    EXPECT_EQ(lexer->nextToken().type, TokenType::KW_WHERE);

    auto active_token = lexer->nextToken();
    EXPECT_EQ(active_token.type, TokenType::IDENTIFIER);

    EXPECT_EQ(lexer->nextToken().type, TokenType::EQUAL);
    EXPECT_EQ(lexer->nextToken().type, TokenType::KW_TRUE);
    EXPECT_EQ(lexer->nextToken().type, TokenType::END_OF_FILE);
}

TEST_F(LexerV3Test, CreateTable_WithReservedWordColumns) {
    // This is the key test case from the spec:
    // "CREATE TABLE procedure (value INT)"
    // - CREATE is a Gatekeeper → KW_CREATE
    // - TABLE is contextual → IDENTIFIER (parser resolves in CREATE context)
    // - procedure is identifier → IDENTIFIER
    // - value is identifier → IDENTIFIER
    // - INT is contextual → IDENTIFIER (parser resolves in type context)

    auto tokens = tokenize("CREATE TABLE procedure (value INT)");

    ASSERT_GE(tokens.size(), 8);
    EXPECT_EQ(tokens[0].type, TokenType::KW_CREATE);
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER); // TABLE - contextual
    EXPECT_EQ(tokens[2].type, TokenType::IDENTIFIER); // procedure
    EXPECT_EQ(tokens[3].type, TokenType::LEFT_PAREN);
    EXPECT_EQ(tokens[4].type, TokenType::IDENTIFIER); // value
    EXPECT_EQ(tokens[5].type, TokenType::IDENTIFIER); // INT - contextual type
    EXPECT_EQ(tokens[6].type, TokenType::RIGHT_PAREN);
}

TEST_F(LexerV3Test, Join_Statement) {
    // LEFT is now contextual (IDENTIFIER), not a Gatekeeper keyword
    auto tokens = tokenize("SELECT a.x FROM a LEFT JOIN b ON a.id = b.id");

    size_t i = 0;
    EXPECT_EQ(tokens[i++].type, TokenType::KW_SELECT);
    EXPECT_EQ(tokens[i++].type, TokenType::IDENTIFIER); // a
    EXPECT_EQ(tokens[i++].type, TokenType::DOT);
    EXPECT_EQ(tokens[i++].type, TokenType::IDENTIFIER); // x
    EXPECT_EQ(tokens[i++].type, TokenType::KW_FROM);
    EXPECT_EQ(tokens[i++].type, TokenType::IDENTIFIER); // a
    EXPECT_EQ(tokens[i++].type, TokenType::IDENTIFIER); // LEFT (now contextual)
    EXPECT_EQ(tokens[i++].type, TokenType::KW_JOIN);
    EXPECT_EQ(tokens[i++].type, TokenType::IDENTIFIER); // b
    EXPECT_EQ(tokens[i++].type, TokenType::KW_ON);
}

// =============================================================================
// Literal Tests
// =============================================================================

TEST_F(LexerV3Test, IntegerLiterals) {
    auto test = [this](const char* text, int64_t expected) {
        auto tokens = tokenize(text);
        ASSERT_GE(tokens.size(), 1) << "Testing: " << text;
        EXPECT_EQ(tokens[0].type, TokenType::INTEGER_LITERAL) << "Testing: " << text;
        EXPECT_EQ(tokens[0].value.int_value, expected) << "Testing: " << text;
    };

    test("0", 0);
    test("123", 123);
    test("999999999", 999999999);
}

TEST_F(LexerV3Test, HexLiterals) {
    auto tokens = tokenize("0x1A");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(tokens[0].value.int_value, 0x1A);
}

TEST_F(LexerV3Test, BinaryLiterals) {
    auto tokens = tokenize("0b1010");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(tokens[0].value.int_value, 10); // 0b1010 = 10
}

TEST_F(LexerV3Test, FloatLiterals) {
    auto test = [this](const char* text, double expected) {
        auto tokens = tokenize(text);
        ASSERT_GE(tokens.size(), 1) << "Testing: " << text;
        EXPECT_EQ(tokens[0].type, TokenType::FLOAT_LITERAL) << "Testing: " << text;
        EXPECT_DOUBLE_EQ(tokens[0].value.float_value, expected) << "Testing: " << text;
    };

    test("3.14", 3.14);
    test("1.0", 1.0);
    test("0.5", 0.5);
    test("1e10", 1e10);
    test("1.5e-3", 1.5e-3);
}

TEST_F(LexerV3Test, StringLiterals) {
    auto lexer = makeLexer("'hello world'");
    auto token = lexer->nextToken();
    EXPECT_EQ(token.type, TokenType::STRING_LITERAL);

    std::string_view text = lexer->stringPool().get(token.value.string_id);
    EXPECT_EQ(text, "hello world");
}

TEST_F(LexerV3Test, StringLiterals_EscapedQuotes) {
    auto lexer = makeLexer("'it''s a test'");
    auto token = lexer->nextToken();
    EXPECT_EQ(token.type, TokenType::STRING_LITERAL);

    std::string_view text = lexer->stringPool().get(token.value.string_id);
    EXPECT_EQ(text, "it's a test");
}

TEST_F(LexerV3Test, EscapeStringLiterals) {
    auto lexer = makeLexer("E'hello\\nworld'");
    auto token = lexer->nextToken();
    EXPECT_EQ(token.type, TokenType::STRING_LITERAL);

    std::string_view text = lexer->stringPool().get(token.value.string_id);
    EXPECT_EQ(text, "hello\nworld");
}

TEST_F(LexerV3Test, BlobLiterals) {
    auto lexer = makeLexer("X'DEADBEEF'");
    auto token = lexer->nextToken();
    EXPECT_EQ(token.type, TokenType::BLOB_LITERAL);

    std::string_view text = lexer->stringPool().get(token.value.string_id);
    EXPECT_EQ(text, "DEADBEEF");
}

// =============================================================================
// Identifier Tests
// =============================================================================

TEST_F(LexerV3Test, SimpleIdentifier) {
    auto lexer = makeLexer("my_column");
    auto token = lexer->nextToken();
    EXPECT_EQ(token.type, TokenType::IDENTIFIER);
    EXPECT_FALSE(token.is_delimited);

    std::string_view text = lexer->stringPool().get(token.value.string_id);
    EXPECT_EQ(text, "my_column");
}

TEST_F(LexerV3Test, QuotedIdentifier) {
    auto lexer = makeLexer("\"My Column\"");
    auto token = lexer->nextToken();
    EXPECT_EQ(token.type, TokenType::IDENTIFIER);
    EXPECT_TRUE(token.is_delimited);

    std::string_view text = lexer->stringPool().get(token.value.string_id);
    EXPECT_EQ(text, "My Column");
}

TEST_F(LexerV3Test, QuotedIdentifier_EscapedQuotes) {
    auto lexer = makeLexer("\"He said \"\"Hello\"\"\"");
    auto token = lexer->nextToken();
    EXPECT_EQ(token.type, TokenType::IDENTIFIER);
    EXPECT_TRUE(token.is_delimited);

    std::string_view text = lexer->stringPool().get(token.value.string_id);
    EXPECT_EQ(text, "He said \"Hello\"");
}

// =============================================================================
// Parameter Tests
// =============================================================================

TEST_F(LexerV3Test, PositionalParameters) {
    auto test = [this](const char* text, uint32_t expected) {
        auto tokens = tokenize(text);
        ASSERT_GE(tokens.size(), 1) << "Testing: " << text;
        EXPECT_EQ(tokens[0].type, TokenType::PARAMETER) << "Testing: " << text;
        EXPECT_EQ(tokens[0].value.param_index, expected) << "Testing: " << text;
    };

    test("$1", 1);
    test("$2", 2);
    test("$123", 123);
}

TEST_F(LexerV3Test, NamedParameters) {
    auto lexer = makeLexer(":user_id");
    auto token = lexer->nextToken();
    EXPECT_EQ(token.type, TokenType::PARAMETER);

    std::string_view text = lexer->stringPool().get(token.value.string_id);
    EXPECT_EQ(text, "user_id");
}

// =============================================================================
// Operator Tests
// =============================================================================

TEST_F(LexerV3Test, ArithmeticOperators) {
    auto tokens = tokenize("+ - * / % ^");
    EXPECT_EQ(tokens[0].type, TokenType::PLUS);
    EXPECT_EQ(tokens[1].type, TokenType::MINUS);
    EXPECT_EQ(tokens[2].type, TokenType::STAR);
    EXPECT_EQ(tokens[3].type, TokenType::SLASH);
    EXPECT_EQ(tokens[4].type, TokenType::PERCENT);
    EXPECT_EQ(tokens[5].type, TokenType::CARET);
}

TEST_F(LexerV3Test, ComparisonOperators) {
    auto tokens = tokenize("= <> != < > <= >=");
    EXPECT_EQ(tokens[0].type, TokenType::EQUAL);
    EXPECT_EQ(tokens[1].type, TokenType::NOT_EQUAL);
    EXPECT_EQ(tokens[2].type, TokenType::NOT_EQUAL);
    EXPECT_EQ(tokens[3].type, TokenType::LESS_THAN);
    EXPECT_EQ(tokens[4].type, TokenType::GREATER_THAN);
    EXPECT_EQ(tokens[5].type, TokenType::LESS_EQUAL);
    EXPECT_EQ(tokens[6].type, TokenType::GREATER_EQUAL);
}

TEST_F(LexerV3Test, StringOperators) {
    auto tokens = tokenize("||");
    EXPECT_EQ(tokens[0].type, TokenType::DOUBLE_PIPE);
}

TEST_F(LexerV3Test, JSONOperators) {
    auto tokens = tokenize("-> ->> #> #>>");
    EXPECT_EQ(tokens[0].type, TokenType::ARROW);
    EXPECT_EQ(tokens[1].type, TokenType::DOUBLE_ARROW);
    EXPECT_EQ(tokens[2].type, TokenType::HASH_ARROW);
    EXPECT_EQ(tokens[3].type, TokenType::HASH_DOUBLE_ARROW);
}

TEST_F(LexerV3Test, ArrayOperators) {
    auto tokens = tokenize("@> <@ && -|-");
    EXPECT_EQ(tokens[0].type, TokenType::AT_GREATER);
    EXPECT_EQ(tokens[1].type, TokenType::LESS_AT);
    EXPECT_EQ(tokens[2].type, TokenType::DOUBLE_AMPERSAND);
    EXPECT_EQ(tokens[3].type, TokenType::MINUS_PIPE_MINUS);
}

TEST_F(LexerV3Test, TypeCastOperator) {
    auto tokens = tokenize("::");
    EXPECT_EQ(tokens[0].type, TokenType::DOUBLE_COLON);
}

TEST_F(LexerV3Test, AssignmentOperators) {
    auto tokens = tokenize(":= =>");
    EXPECT_EQ(tokens[0].type, TokenType::COLON_EQUALS);
    EXPECT_EQ(tokens[1].type, TokenType::EQUALS_GREATER);
}

TEST_F(LexerV3Test, RegexOperators) {
    auto tokens = tokenize("~ ~* !~ !~*");
    EXPECT_EQ(tokens[0].type, TokenType::TILDE);
    EXPECT_EQ(tokens[1].type, TokenType::TILDE_STAR);
    EXPECT_EQ(tokens[2].type, TokenType::EXCLAIM_TILDE);
    EXPECT_EQ(tokens[3].type, TokenType::EXCLAIM_TILDE_STAR);
}

TEST_F(LexerV3Test, BitOperators) {
    auto tokens = tokenize("& | << >>");
    EXPECT_EQ(tokens[0].type, TokenType::AMPERSAND);
    EXPECT_EQ(tokens[1].type, TokenType::PIPE);
    EXPECT_EQ(tokens[2].type, TokenType::SHIFT_LEFT);
    EXPECT_EQ(tokens[3].type, TokenType::SHIFT_RIGHT);
}

// =============================================================================
// Punctuation Tests
// =============================================================================

TEST_F(LexerV3Test, Punctuation) {
    auto tokens = tokenize("( ) [ ] { } , ; . :");
    EXPECT_EQ(tokens[0].type, TokenType::LEFT_PAREN);
    EXPECT_EQ(tokens[1].type, TokenType::RIGHT_PAREN);
    EXPECT_EQ(tokens[2].type, TokenType::LEFT_BRACKET);
    EXPECT_EQ(tokens[3].type, TokenType::RIGHT_BRACKET);
    EXPECT_EQ(tokens[4].type, TokenType::LEFT_BRACE);
    EXPECT_EQ(tokens[5].type, TokenType::RIGHT_BRACE);
    EXPECT_EQ(tokens[6].type, TokenType::COMMA);
    EXPECT_EQ(tokens[7].type, TokenType::SEMICOLON);
    EXPECT_EQ(tokens[8].type, TokenType::DOT);
    EXPECT_EQ(tokens[9].type, TokenType::COLON);
}

// =============================================================================
// Source Location Tests
// =============================================================================

TEST_F(LexerV3Test, SourceLocation_Simple) {
    auto lexer = makeLexer("SELECT id");

    auto select_token = lexer->nextToken();
    EXPECT_EQ(select_token.span.start.line, 1);
    EXPECT_EQ(select_token.span.start.column, 1);
    EXPECT_EQ(select_token.span.length, 6);

    auto id_token = lexer->nextToken();
    EXPECT_EQ(id_token.span.start.line, 1);
    EXPECT_EQ(id_token.span.start.column, 8);
    EXPECT_EQ(id_token.span.length, 2);
}

TEST_F(LexerV3Test, SourceLocation_MultiLine) {
    auto lexer = makeLexer("SELECT\nid");

    auto select_token = lexer->nextToken();
    EXPECT_EQ(select_token.span.start.line, 1);
    EXPECT_EQ(select_token.span.start.column, 1);

    auto id_token = lexer->nextToken();
    EXPECT_EQ(id_token.span.start.line, 2);
    EXPECT_EQ(id_token.span.start.column, 1);
}

// =============================================================================
// Peek Tests
// =============================================================================

TEST_F(LexerV3Test, PeekToken) {
    auto lexer = makeLexer("SELECT FROM");

    // Peek should not consume
    Token peeked = lexer->peekToken();
    EXPECT_EQ(peeked.type, TokenType::KW_SELECT);

    // Peek again should return same token
    Token peeked2 = lexer->peekToken();
    EXPECT_EQ(peeked2.type, TokenType::KW_SELECT);

    // Next should return the peeked token
    Token next = lexer->nextToken();
    EXPECT_EQ(next.type, TokenType::KW_SELECT);

    // Now peek should return FROM
    Token peeked3 = lexer->peekToken();
    EXPECT_EQ(peeked3.type, TokenType::KW_FROM);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(LexerV3Test, UnterminatedString) {
    auto lexer = makeLexer("'unterminated");
    auto token = lexer->nextToken();
    // Should still return a string token (lexer consumes to end)
    EXPECT_EQ(token.type, TokenType::STRING_LITERAL);
}

TEST_F(LexerV3Test, UnterminatedBlockComment) {
    auto lexer = makeLexer("/* unterminated");
    auto token = lexer->nextToken();
    EXPECT_EQ(token.type, TokenType::END_OF_FILE);
    EXPECT_TRUE(error_reporter_->hasErrors());
}

TEST_F(LexerV3Test, InvalidCharacter) {
    auto lexer = makeLexer("SELECT ` FROM");
    lexer->nextToken(); // SELECT
    auto token = lexer->nextToken();
    EXPECT_EQ(token.type, TokenType::ERROR);
    EXPECT_TRUE(error_reporter_->hasErrors());
}

// =============================================================================
// Complex Statement Tests
// =============================================================================

TEST_F(LexerV3Test, ComplexQuery) {
    const char* sql = R"(
        SELECT u.id, u.name, COUNT(*) as total
        FROM users u
        LEFT JOIN orders o ON u.id = o.user_id
        WHERE u.status = 'active'
          AND o.created_at >= '2024-01-01'
        GROUP BY u.id, u.name
        HAVING COUNT(*) > 10
        ORDER BY total DESC
        LIMIT 100
    )";

    auto tokens = tokenize(sql);

    // Just verify we get a reasonable number of tokens without errors
    EXPECT_GT(tokens.size(), 30);
    EXPECT_FALSE(error_reporter_->hasErrors());

    // Verify it ends with EOF
    EXPECT_EQ(tokens.back().type, TokenType::END_OF_FILE);
}

TEST_F(LexerV3Test, InsertWithTypeCast) {
    const char* sql = "INSERT INTO t (col) VALUES ($1::INTEGER)";
    auto tokens = tokenize(sql);

    // Find the :: token
    bool found_double_colon = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::DOUBLE_COLON) {
            found_double_colon = true;
            break;
        }
    }
    EXPECT_TRUE(found_double_colon);

    // INTEGER should be an identifier (contextual keyword)
    bool found_integer_as_identifier = false;
    auto lexer = makeLexer(sql);
    Token t;
    while ((t = lexer->nextToken()).type != TokenType::END_OF_FILE) {
        if (t.type == TokenType::IDENTIFIER) {
            std::string_view text = lexer->stringPool().get(t.value.string_id);
            if (text == "INTEGER") {
                found_integer_as_identifier = true;
                break;
            }
        }
    }
    EXPECT_TRUE(found_integer_as_identifier);
}

// =============================================================================
// String Pool Tests
// =============================================================================

TEST_F(LexerV3Test, StringPool_Interning) {
    StringPool pool;

    auto id1 = pool.intern("hello");
    auto id2 = pool.intern("world");
    auto id3 = pool.intern("hello");  // Same as id1

    EXPECT_NE(id1, StringPool::INVALID_ID);
    EXPECT_NE(id2, StringPool::INVALID_ID);
    EXPECT_EQ(id1, id3);  // Same string, same ID
    EXPECT_NE(id1, id2);  // Different strings, different IDs

    EXPECT_EQ(pool.get(id1), "hello");
    EXPECT_EQ(pool.get(id2), "world");
}

TEST_F(LexerV3Test, StringPool_Clear) {
    StringPool pool;

    auto id1 = pool.intern("test");
    EXPECT_NE(id1, StringPool::INVALID_ID);

    pool.clear();

    // After clear, old IDs are invalid
    EXPECT_EQ(pool.get(id1), "");
}
