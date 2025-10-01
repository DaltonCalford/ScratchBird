#include <gtest/gtest.h>
#include "scratchbird/parser/lexer.h"
#include <vector>
#include <string>

using namespace scratchbird::parser;

class LexerTest : public ::testing::Test
{
protected:
    std::vector<Token> tokenizeAll(const std::string &input)
    {
        Lexer lexer(input);
        std::vector<Token> tokens;

        Token tok;
        do
        {
            tok = lexer.nextToken();
            tokens.push_back(tok);
        } while (tok.type != TokenType::END_OF_FILE && tok.type != TokenType::ERROR);

        return tokens;
    }

    void expectTokens(const std::string &input, const std::vector<TokenType> &expected)
    {
        auto tokens = tokenizeAll(input);
        ASSERT_EQ(tokens.size(), expected.size() + 1); // +1 for EOF

        for (size_t i = 0; i < expected.size(); i++)
        {
            EXPECT_EQ(tokens[i].type, expected[i]) << "Token " << i << " mismatch in: " << input;
        }

        EXPECT_EQ(tokens.back().type, TokenType::END_OF_FILE);
    }
};

TEST_F(LexerTest, EmptyInput)
{
    Lexer lexer("");
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::END_OF_FILE);
}

TEST_F(LexerTest, Whitespace)
{
    expectTokens("   \t\n  ", {});
}

TEST_F(LexerTest, SimpleIdentifiers)
{
    expectTokens("foo bar baz",
                 {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER});
}

TEST_F(LexerTest, Keywords)
{
    expectTokens("CREATE TABLE SELECT FROM WHERE",
                 {TokenType::KW_CREATE, TokenType::KW_TABLE, TokenType::KW_SELECT,
                  TokenType::KW_FROM, TokenType::KW_WHERE});
}

TEST_F(LexerTest, CaseInsensitiveKeywords)
{
    expectTokens("create TaBLe SeLeCt",
                 {TokenType::KW_CREATE, TokenType::KW_TABLE, TokenType::KW_SELECT});
}

TEST_F(LexerTest, IntegerLiterals)
{
    Lexer lexer("42 0 12345");

    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(tok.value.int_value, 42);

    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(tok.value.int_value, 0);

    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(tok.value.int_value, 12345);
}

TEST_F(LexerTest, FloatLiterals)
{
    Lexer lexer("3.14 0.0 1e10 2.5e-3");

    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::FLOAT_LITERAL);
    EXPECT_DOUBLE_EQ(tok.value.float_value, 3.14);

    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::FLOAT_LITERAL);
    EXPECT_DOUBLE_EQ(tok.value.float_value, 0.0);

    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::FLOAT_LITERAL);
    EXPECT_DOUBLE_EQ(tok.value.float_value, 1e10);

    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::FLOAT_LITERAL);
    EXPECT_DOUBLE_EQ(tok.value.float_value, 2.5e-3);
}

TEST_F(LexerTest, StringLiterals)
{
    Lexer lexer("'hello' 'world' 'escape\\'test'");

    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "hello");

    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "world");

    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "escape'test");
}

TEST_F(LexerTest, Operators)
{
    expectTokens("+ - * / % = <> < > <= >= ( ) , ; .",
                 {TokenType::PLUS, TokenType::MINUS, TokenType::STAR, TokenType::SLASH,
                  TokenType::PERCENT, TokenType::EQUAL, TokenType::NOT_EQUAL, TokenType::LESS_THAN,
                  TokenType::GREATER_THAN, TokenType::LESS_EQUAL, TokenType::GREATER_EQUAL,
                  TokenType::LEFT_PAREN, TokenType::RIGHT_PAREN, TokenType::COMMA,
                  TokenType::SEMICOLON, TokenType::DOT});
}

TEST_F(LexerTest, LineComments)
{
    expectTokens("foo -- comment\nbar", {TokenType::IDENTIFIER, TokenType::IDENTIFIER});
}

TEST_F(LexerTest, BlockComments)
{
    expectTokens("foo /* block comment */ bar", {TokenType::IDENTIFIER, TokenType::IDENTIFIER});
}

TEST_F(LexerTest, CreateTableStatement)
{
    expectTokens("CREATE TABLE users (id INTEGER NOT NULL, name VARCHAR(50))",
                 {TokenType::KW_CREATE, TokenType::KW_TABLE, TokenType::IDENTIFIER,
                  TokenType::LEFT_PAREN, TokenType::IDENTIFIER, TokenType::KW_INTEGER,
                  TokenType::KW_NOT, TokenType::KW_NULL, TokenType::COMMA, TokenType::IDENTIFIER,
                  TokenType::KW_VARCHAR, TokenType::LEFT_PAREN, TokenType::INTEGER_LITERAL,
                  TokenType::RIGHT_PAREN, TokenType::RIGHT_PAREN});
}

TEST_F(LexerTest, InsertStatement)
{
    expectTokens("INSERT INTO users (id, name) VALUES (1, 'John')",
                 {TokenType::KW_INSERT, TokenType::KW_INTO, TokenType::IDENTIFIER,
                  TokenType::LEFT_PAREN, TokenType::IDENTIFIER, TokenType::COMMA,
                  TokenType::IDENTIFIER, TokenType::RIGHT_PAREN, TokenType::KW_VALUES,
                  TokenType::LEFT_PAREN, TokenType::INTEGER_LITERAL, TokenType::COMMA,
                  TokenType::STRING_LITERAL, TokenType::RIGHT_PAREN});
}

TEST_F(LexerTest, SelectStatement)
{
    expectTokens("SELECT * FROM users WHERE id = 1",
                 {TokenType::KW_SELECT, TokenType::STAR, TokenType::KW_FROM, TokenType::IDENTIFIER,
                  TokenType::KW_WHERE, TokenType::IDENTIFIER, TokenType::EQUAL,
                  TokenType::INTEGER_LITERAL});
}

TEST_F(LexerTest, LocationTracking)
{
    Lexer lexer("foo\nbar baz");

    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.location.line, 1);
    EXPECT_EQ(tok.location.column, 1);
    EXPECT_EQ(tok.location.offset, 0);

    tok = lexer.nextToken();
    EXPECT_EQ(tok.location.line, 2);
    EXPECT_EQ(tok.location.column, 1);
    EXPECT_EQ(tok.location.offset, 4);

    tok = lexer.nextToken();
    EXPECT_EQ(tok.location.line, 2);
    EXPECT_EQ(tok.location.column, 5);
    EXPECT_EQ(tok.location.offset, 8);
}

TEST_F(LexerTest, ErrorHandling)
{
    SimpleErrorReporter reporter;
    Lexer lexer("'unterminated string");
    lexer.setErrorReporter(&reporter);

    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::ERROR);
    EXPECT_TRUE(reporter.hasErrors());
    EXPECT_EQ(reporter.errorCount(), 1);
}