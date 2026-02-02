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
 * Unit Tests for ScratchBird Parser State v2.0
 *
 * Tests the ParserState class which provides contextual keyword matching
 * for the "Smart Parser, Dumb Lexer" architecture.
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/parser_state_v2.h"

using namespace scratchbird::parser::v2;

class ParserStateV2Test : public ::testing::Test {
protected:
    std::unique_ptr<Lexer> makeLexer(const std::string& input) {
        // Store input in member to keep it alive
        input_ = input;
        return std::make_unique<Lexer>(input_);
    }

    std::unique_ptr<ParserState> makeState(const std::string& input) {
        lexer_ = makeLexer(input);
        return std::make_unique<ParserState>(*lexer_);
    }

private:
    std::string input_;
    std::unique_ptr<Lexer> lexer_;
};

// =============================================================================
// Basic State Tests
// =============================================================================

TEST_F(ParserStateV2Test, InitialState) {
    auto state = makeState("SELECT");

    // Initial mode should be STATEMENT
    EXPECT_EQ(state->currentMode(), ParseMode::STATEMENT);

    // Current token should be SELECT keyword
    EXPECT_EQ(state->current().type, TokenType::KW_SELECT);
}

TEST_F(ParserStateV2Test, ModeStackPushPop) {
    auto state = makeState("CREATE TABLE");

    EXPECT_EQ(state->currentMode(), ParseMode::STATEMENT);

    state->pushMode(ParseMode::DDL);
    EXPECT_EQ(state->currentMode(), ParseMode::DDL);

    state->pushMode(ParseMode::COLUMN_DEF);
    EXPECT_EQ(state->currentMode(), ParseMode::COLUMN_DEF);

    state->popMode();
    EXPECT_EQ(state->currentMode(), ParseMode::DDL);

    state->popMode();
    EXPECT_EQ(state->currentMode(), ParseMode::STATEMENT);
}

TEST_F(ParserStateV2Test, IsInMode) {
    auto state = makeState("SELECT");

    state->pushMode(ParseMode::DML_SELECT);
    state->pushMode(ParseMode::EXPRESSION);

    EXPECT_TRUE(state->isInMode(ParseMode::STATEMENT));
    EXPECT_TRUE(state->isInMode(ParseMode::DML_SELECT));
    EXPECT_TRUE(state->isInMode(ParseMode::EXPRESSION));
    EXPECT_FALSE(state->isInMode(ParseMode::DDL));
}

TEST_F(ParserStateV2Test, PopModeDoesNotRemoveBase) {
    auto state = makeState("SELECT");

    // Pop multiple times - should not crash or remove STATEMENT mode
    state->popMode();
    state->popMode();
    state->popMode();

    EXPECT_EQ(state->currentMode(), ParseMode::STATEMENT);
}

// =============================================================================
// Token Navigation Tests
// =============================================================================

TEST_F(ParserStateV2Test, AdvanceAndPrevious) {
    auto state = makeState("SELECT id FROM");

    // First token is SELECT
    EXPECT_EQ(state->current().type, TokenType::KW_SELECT);

    state->advance();
    // Now at identifier "id"
    EXPECT_EQ(state->current().type, TokenType::IDENTIFIER);
    EXPECT_EQ(state->previous().type, TokenType::KW_SELECT);

    state->advance();
    // Now at FROM
    EXPECT_EQ(state->current().type, TokenType::KW_FROM);
    EXPECT_EQ(state->previous().type, TokenType::IDENTIFIER);
}

TEST_F(ParserStateV2Test, MatchAdvancesOnSuccess) {
    auto state = makeState("SELECT FROM");

    EXPECT_TRUE(state->match(TokenType::KW_SELECT));
    // Should have advanced to FROM
    EXPECT_EQ(state->current().type, TokenType::KW_FROM);
}

TEST_F(ParserStateV2Test, MatchDoesNotAdvanceOnFailure) {
    auto state = makeState("SELECT FROM");

    EXPECT_FALSE(state->match(TokenType::KW_INSERT));
    // Should still be at SELECT
    EXPECT_EQ(state->current().type, TokenType::KW_SELECT);
}

TEST_F(ParserStateV2Test, CheckDoesNotAdvance) {
    auto state = makeState("SELECT FROM");

    EXPECT_TRUE(state->check(TokenType::KW_SELECT));
    // Should still be at SELECT
    EXPECT_EQ(state->current().type, TokenType::KW_SELECT);
}

TEST_F(ParserStateV2Test, IsAtEnd) {
    auto state = makeState("SELECT");

    EXPECT_FALSE(state->isAtEnd());
    state->advance();  // Move to EOF
    EXPECT_TRUE(state->isAtEnd());
}

// =============================================================================
// Contextual Keyword Matching Tests
// =============================================================================

TEST_F(ParserStateV2Test, MatchContextual_Success) {
    auto state = makeState("TABLE users");

    // TABLE is an identifier (not a Gatekeeper keyword)
    EXPECT_EQ(state->current().type, TokenType::IDENTIFIER);

    // Match it contextually
    EXPECT_TRUE(state->matchContextual("TABLE"));

    // Should have advanced to "users"
    EXPECT_EQ(state->current().type, TokenType::IDENTIFIER);
}

TEST_F(ParserStateV2Test, MatchContextual_CaseInsensitive) {
    auto state = makeState("table INDEX view");

    EXPECT_TRUE(state->matchContextual("TABLE"));
    EXPECT_TRUE(state->matchContextual("index"));
    EXPECT_TRUE(state->matchContextual("VIEW"));
}

TEST_F(ParserStateV2Test, MatchContextual_FailsOnMismatch) {
    auto state = makeState("TABLE users");

    EXPECT_FALSE(state->matchContextual("INDEX"));
    // Should not have advanced
    EXPECT_TRUE(state->checkContextual("TABLE"));
}

TEST_F(ParserStateV2Test, MatchContextual_FailsOnGatekeeperKeyword) {
    auto state = makeState("SELECT id");

    // SELECT is a Gatekeeper keyword, not an IDENTIFIER
    EXPECT_FALSE(state->matchContextual("SELECT"));
    // Should not have advanced
    EXPECT_EQ(state->current().type, TokenType::KW_SELECT);
}

TEST_F(ParserStateV2Test, CheckContextual_DoesNotAdvance) {
    auto state = makeState("TABLE users");

    EXPECT_TRUE(state->checkContextual("TABLE"));
    // Should still be at TABLE
    EXPECT_TRUE(state->checkContextual("TABLE"));
}

TEST_F(ParserStateV2Test, CheckContextual_CaseInsensitive) {
    auto state = makeState("Table");

    EXPECT_TRUE(state->checkContextual("TABLE"));
    EXPECT_TRUE(state->checkContextual("table"));
    EXPECT_TRUE(state->checkContextual("TaBlE"));
}

// =============================================================================
// Identifier Access Tests
// =============================================================================

TEST_F(ParserStateV2Test, IsIdentifier) {
    auto state = makeState("myTable SELECT 123");

    // myTable is an identifier
    EXPECT_TRUE(state->isIdentifier());

    state->advance();
    // SELECT is a keyword, not identifier
    EXPECT_FALSE(state->isIdentifier());

    state->advance();
    // 123 is a number literal
    EXPECT_FALSE(state->isIdentifier());
}

TEST_F(ParserStateV2Test, CurrentIdentifierText) {
    auto state = makeState("myTable SELECT");

    EXPECT_EQ(state->currentIdentifierText(), "myTable");

    state->advance();
    // SELECT is not an identifier, should return empty
    EXPECT_EQ(state->currentIdentifierText(), "");
}

TEST_F(ParserStateV2Test, CurrentStringId) {
    auto state = makeState("myTable SELECT");

    StringPool::StringId id = state->currentStringId();
    EXPECT_NE(id, StringPool::INVALID_ID);
    EXPECT_EQ(state->stringPool().get(id), "myTable");

    state->advance();
    // SELECT is not an identifier
    EXPECT_EQ(state->currentStringId(), StringPool::INVALID_ID);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(ParserStateV2Test, ExpectContextual_Success) {
    auto state = makeState("TABLE users");

    // Set up error tracking
    bool error_called = false;
    state->setErrorHandler([](const char*, SourceLocation, void* ctx) {
        *static_cast<bool*>(ctx) = true;
    }, &error_called);

    state->expectContextual("TABLE", "Expected TABLE");

    EXPECT_FALSE(error_called);
    // Should have advanced to "users"
    EXPECT_TRUE(state->checkContextual("users"));
}

TEST_F(ParserStateV2Test, ExpectContextual_Failure) {
    auto state = makeState("INDEX users");

    bool error_called = false;
    state->setErrorHandler([](const char*, SourceLocation, void* ctx) {
        *static_cast<bool*>(ctx) = true;
    }, &error_called);

    state->expectContextual("TABLE", "Expected TABLE");

    EXPECT_TRUE(error_called);
    // Should have advanced anyway (error recovery)
    EXPECT_TRUE(state->checkContextual("users"));
}

// =============================================================================
// ParseModeGuard Tests
// =============================================================================

TEST_F(ParserStateV2Test, ParseModeGuard_BasicScope) {
    auto state = makeState("SELECT");

    EXPECT_EQ(state->currentMode(), ParseMode::STATEMENT);

    {
        ParseModeGuard guard(*state, ParseMode::DDL);
        EXPECT_EQ(state->currentMode(), ParseMode::DDL);
    }

    EXPECT_EQ(state->currentMode(), ParseMode::STATEMENT);
}

TEST_F(ParserStateV2Test, ParseModeGuard_NestedScopes) {
    auto state = makeState("SELECT");

    EXPECT_EQ(state->currentMode(), ParseMode::STATEMENT);

    {
        ParseModeGuard guard1(*state, ParseMode::DDL);
        EXPECT_EQ(state->currentMode(), ParseMode::DDL);

        {
            ParseModeGuard guard2(*state, ParseMode::COLUMN_DEF);
            EXPECT_EQ(state->currentMode(), ParseMode::COLUMN_DEF);
        }

        EXPECT_EQ(state->currentMode(), ParseMode::DDL);
    }

    EXPECT_EQ(state->currentMode(), ParseMode::STATEMENT);
}

// =============================================================================
// Integration Tests - Parsing Patterns
// =============================================================================

TEST_F(ParserStateV2Test, CreateTablePattern) {
    // Simulates: CREATE TABLE users (id INT, name VARCHAR)
    auto state = makeState("CREATE TABLE users ( id INT , name VARCHAR )");

    // CREATE is a Gatekeeper keyword
    EXPECT_TRUE(state->match(TokenType::KW_CREATE));

    // TABLE is contextual
    {
        ParseModeGuard guard(*state, ParseMode::DDL);
        EXPECT_TRUE(state->matchContextual("TABLE"));

        // Table name "users"
        EXPECT_TRUE(state->isIdentifier());
        EXPECT_EQ(state->currentIdentifierText(), "users");
        state->advance();

        // (
        EXPECT_TRUE(state->match(TokenType::LEFT_PAREN));

        {
            ParseModeGuard colGuard(*state, ParseMode::COLUMN_DEF);

            // Column "id"
            EXPECT_EQ(state->currentIdentifierText(), "id");
            state->advance();

            // Type "INT" is contextual
            EXPECT_TRUE(state->matchContextual("INT"));

            // ,
            EXPECT_TRUE(state->match(TokenType::COMMA));

            // Column "name"
            EXPECT_EQ(state->currentIdentifierText(), "name");
            state->advance();

            // Type "VARCHAR" is contextual
            EXPECT_TRUE(state->matchContextual("VARCHAR"));
        }

        // )
        EXPECT_TRUE(state->match(TokenType::RIGHT_PAREN));
    }

    EXPECT_TRUE(state->isAtEnd());
}

TEST_F(ParserStateV2Test, SelectWithJoinPattern) {
    // Simulates: SELECT name FROM users LEFT JOIN orders ON users.id = orders.user_id
    auto state = makeState("SELECT name FROM users LEFT JOIN orders ON users . id = orders . user_id");

    // SELECT keyword
    EXPECT_TRUE(state->match(TokenType::KW_SELECT));

    {
        ParseModeGuard guard(*state, ParseMode::DML_SELECT);

        // "name" column
        EXPECT_EQ(state->currentIdentifierText(), "name");
        state->advance();

        // FROM keyword
        EXPECT_TRUE(state->match(TokenType::KW_FROM));

        {
            ParseModeGuard tableGuard(*state, ParseMode::TABLE_REF);

            // "users" table
            EXPECT_EQ(state->currentIdentifierText(), "users");
            state->advance();

            // LEFT is now contextual (not a Gatekeeper keyword)
            // Parser checks it via matchContextual in FROM clause context
            EXPECT_TRUE(state->matchContextual("LEFT"));

            // JOIN is still a Gatekeeper keyword
            EXPECT_TRUE(state->match(TokenType::KW_JOIN));

            // "orders" table
            EXPECT_EQ(state->currentIdentifierText(), "orders");
            state->advance();
        }

        // ON keyword
        EXPECT_TRUE(state->match(TokenType::KW_ON));

        {
            ParseModeGuard exprGuard(*state, ParseMode::EXPRESSION);

            // users.id = orders.user_id
            EXPECT_EQ(state->currentIdentifierText(), "users");
            state->advance();
            EXPECT_TRUE(state->match(TokenType::DOT));
            EXPECT_EQ(state->currentIdentifierText(), "id");
            state->advance();
            EXPECT_TRUE(state->match(TokenType::EQUAL));
            EXPECT_EQ(state->currentIdentifierText(), "orders");
            state->advance();
            EXPECT_TRUE(state->match(TokenType::DOT));
            EXPECT_EQ(state->currentIdentifierText(), "user_id");
            state->advance();
        }
    }

    EXPECT_TRUE(state->isAtEnd());
}

TEST_F(ParserStateV2Test, ContextualKeywordsAsIdentifiers) {
    // The key design test: common words should be valid identifiers
    // CREATE TABLE procedure (value INT, type VARCHAR, name TEXT)
    auto state = makeState("CREATE TABLE procedure ( value INT , type VARCHAR , name TEXT )");

    EXPECT_TRUE(state->match(TokenType::KW_CREATE));

    {
        ParseModeGuard guard(*state, ParseMode::DDL);

        // TABLE is contextual after CREATE
        EXPECT_TRUE(state->matchContextual("TABLE"));

        // "procedure" is table name (not reserved)
        EXPECT_EQ(state->currentIdentifierText(), "procedure");
        state->advance();

        EXPECT_TRUE(state->match(TokenType::LEFT_PAREN));

        // "value" is column name
        EXPECT_EQ(state->currentIdentifierText(), "value");
        state->advance();

        // "INT" is contextual type
        EXPECT_TRUE(state->matchContextual("INT"));

        EXPECT_TRUE(state->match(TokenType::COMMA));

        // "type" is column name
        EXPECT_EQ(state->currentIdentifierText(), "type");
        state->advance();

        // "VARCHAR" is contextual type
        EXPECT_TRUE(state->matchContextual("VARCHAR"));

        EXPECT_TRUE(state->match(TokenType::COMMA));

        // "name" is column name
        EXPECT_EQ(state->currentIdentifierText(), "name");
        state->advance();

        // "TEXT" is contextual type
        EXPECT_TRUE(state->matchContextual("TEXT"));

        EXPECT_TRUE(state->match(TokenType::RIGHT_PAREN));
    }

    EXPECT_TRUE(state->isAtEnd());
}

// =============================================================================
// Utility Function Tests
// =============================================================================

TEST_F(ParserStateV2Test, CaseInsensitiveEquals) {
    EXPECT_TRUE(caseInsensitiveEquals("TABLE", "TABLE"));
    EXPECT_TRUE(caseInsensitiveEquals("table", "TABLE"));
    EXPECT_TRUE(caseInsensitiveEquals("TaBlE", "tAbLe"));
    EXPECT_TRUE(caseInsensitiveEquals("", ""));

    EXPECT_FALSE(caseInsensitiveEquals("TABLE", "INDEX"));
    EXPECT_FALSE(caseInsensitiveEquals("TABLE", "TABLES"));
    EXPECT_FALSE(caseInsensitiveEquals("T", ""));
}

TEST_F(ParserStateV2Test, ParseModeToString) {
    EXPECT_STREQ(parseModeToString(ParseMode::STATEMENT), "STATEMENT");
    EXPECT_STREQ(parseModeToString(ParseMode::DDL), "DDL");
    EXPECT_STREQ(parseModeToString(ParseMode::DML_SELECT), "DML_SELECT");
    EXPECT_STREQ(parseModeToString(ParseMode::EXPRESSION), "EXPRESSION");
    EXPECT_STREQ(parseModeToString(ParseMode::COLUMN_DEF), "COLUMN_DEF");
}
