/**
 * Unit tests for Firebird SQL Lexer
 *
 * Tests the Firebird 5.0 SQL lexer with all reserved keywords,
 * Firebird-specific operators, and literal types.
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/firebird/firebird_lexer.h"

using namespace scratchbird::parser::firebird;

class FirebirdLexerTest : public ::testing::Test {
protected:
    void SetUp() override {
        error_reporter_ = std::make_unique<SimpleErrorReporter>();
    }

    std::unique_ptr<Lexer> makeLexer(std::string_view input, SQLDialect dialect = SQLDialect::DIALECT_3) {
        auto lexer = std::make_unique<Lexer>(input, dialect);
        lexer->setErrorReporter(error_reporter_.get());
        return lexer;
    }

    std::vector<Token> tokenize(std::string_view input, SQLDialect dialect = SQLDialect::DIALECT_3) {
        auto lexer = makeLexer(input, dialect);
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

TEST_F(FirebirdLexerTest, EmptyInput) {
    auto tokens = tokenize("");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::END_OF_FILE);
}

TEST_F(FirebirdLexerTest, WhitespaceOnly) {
    auto tokens = tokenize("   \t\n  ");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::END_OF_FILE);
}

TEST_F(FirebirdLexerTest, SingleLineComment) {
    auto tokens = tokenize("-- this is a comment\nSELECT");
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::KW_SELECT);
    EXPECT_EQ(tokens[1].type, TokenType::END_OF_FILE);
}

TEST_F(FirebirdLexerTest, BlockComment) {
    auto tokens = tokenize("/* comment */ SELECT /* another */");
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::KW_SELECT);
}

// =============================================================================
// Reserved Keyword Tests - Statement Initiators
// =============================================================================

TEST_F(FirebirdLexerTest, ReservedKeywords_DDL) {
    auto test = [this](const char* text, TokenType expected) {
        auto tokens = tokenize(text);
        ASSERT_GE(tokens.size(), 1) << "Testing: " << text;
        EXPECT_EQ(tokens[0].type, expected) << "Testing: " << text;
    };

    test("CREATE", TokenType::KW_CREATE);
    test("ALTER", TokenType::KW_ALTER);
    test("DROP", TokenType::KW_DROP);
    test("RECREATE", TokenType::KW_RECREATE);
    test("TABLE", TokenType::KW_TABLE);
    test("INDEX", TokenType::KW_INDEX);
    test("VIEW", TokenType::KW_VIEW);
    test("PROCEDURE", TokenType::KW_PROCEDURE);
    test("FUNCTION", TokenType::KW_FUNCTION);
    test("TRIGGER", TokenType::KW_TRIGGER);
}

TEST_F(FirebirdLexerTest, ReservedKeywords_DML) {
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
    test("FROM", TokenType::KW_FROM);
    test("WHERE", TokenType::KW_WHERE);
    test("ORDER", TokenType::KW_ORDER);
    test("GROUP", TokenType::KW_GROUP);
    test("HAVING", TokenType::KW_HAVING);
}

TEST_F(FirebirdLexerTest, ReservedKeywords_Transaction) {
    auto test = [this](const char* text, TokenType expected) {
        auto tokens = tokenize(text);
        ASSERT_GE(tokens.size(), 1) << "Testing: " << text;
        EXPECT_EQ(tokens[0].type, expected) << "Testing: " << text;
    };

    test("COMMIT", TokenType::KW_COMMIT);
    test("ROLLBACK", TokenType::KW_ROLLBACK);
    test("SAVEPOINT", TokenType::KW_SAVEPOINT);
    test("RELEASE", TokenType::KW_RELEASE);
    test("START", TokenType::KW_START);
}

TEST_F(FirebirdLexerTest, ReservedKeywords_Types) {
    auto test = [this](const char* text, TokenType expected) {
        auto tokens = tokenize(text);
        ASSERT_GE(tokens.size(), 1) << "Testing: " << text;
        EXPECT_EQ(tokens[0].type, expected) << "Testing: " << text;
    };

    test("INTEGER", TokenType::KW_INTEGER);
    test("INT", TokenType::KW_INT);
    test("BIGINT", TokenType::KW_BIGINT);
    test("INT128", TokenType::KW_INT128);
    test("SMALLINT", TokenType::KW_SMALLINT);
    test("CHAR", TokenType::KW_CHAR);
    test("VARCHAR", TokenType::KW_VARCHAR);
    test("BLOB", TokenType::KW_BLOB);
    test("BOOLEAN", TokenType::KW_BOOLEAN);
    test("DATE", TokenType::KW_DATE);
    test("TIME", TokenType::KW_TIME);
    test("TIMESTAMP", TokenType::KW_TIMESTAMP);
    test("DECIMAL", TokenType::KW_DECIMAL);
    test("NUMERIC", TokenType::KW_NUMERIC);
    test("FLOAT", TokenType::KW_FLOAT);
    test("DOUBLE", TokenType::KW_DOUBLE);
    test("REAL", TokenType::KW_REAL);
    test("DECFLOAT", TokenType::KW_DECFLOAT);
}

TEST_F(FirebirdLexerTest, ReservedKeywords_PSQL) {
    auto test = [this](const char* text, TokenType expected) {
        auto tokens = tokenize(text);
        ASSERT_GE(tokens.size(), 1) << "Testing: " << text;
        EXPECT_EQ(tokens[0].type, expected) << "Testing: " << text;
    };

    test("BEGIN", TokenType::KW_BEGIN);
    test("END", TokenType::KW_END);
    test("DECLARE", TokenType::KW_DECLARE);
    test("VARIABLE", TokenType::KW_VARIABLE);
    test("CURSOR", TokenType::KW_CURSOR);
    test("RETURN", TokenType::KW_RETURN);
    test("RETURNS", TokenType::KW_RETURNS);
    test("WHILE", TokenType::KW_WHILE);
    test("FOR", TokenType::KW_FOR);
    test("EXECUTE", TokenType::KW_EXECUTE);
}

TEST_F(FirebirdLexerTest, ReservedKeywords_CaseInsensitive) {
    auto tokens1 = tokenize("select");
    auto tokens2 = tokenize("SELECT");
    auto tokens3 = tokenize("SeLeCt");

    EXPECT_EQ(tokens1[0].type, TokenType::KW_SELECT);
    EXPECT_EQ(tokens2[0].type, TokenType::KW_SELECT);
    EXPECT_EQ(tokens3[0].type, TokenType::KW_SELECT);
}

// =============================================================================
// Literal Tests
// =============================================================================

TEST_F(FirebirdLexerTest, IntegerLiterals) {
    auto lexer = makeLexer("123 0 999999");

    Token t1 = lexer->nextToken();
    EXPECT_EQ(t1.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(t1.value.int_value, 123);

    Token t2 = lexer->nextToken();
    EXPECT_EQ(t2.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(t2.value.int_value, 0);

    Token t3 = lexer->nextToken();
    EXPECT_EQ(t3.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(t3.value.int_value, 999999);
}

TEST_F(FirebirdLexerTest, FloatLiterals) {
    auto lexer = makeLexer("3.14 0.5 1.23e10 2.5E-3");

    Token t1 = lexer->nextToken();
    EXPECT_EQ(t1.type, TokenType::FLOAT_LITERAL);
    EXPECT_DOUBLE_EQ(t1.value.float_value, 3.14);

    Token t2 = lexer->nextToken();
    EXPECT_EQ(t2.type, TokenType::FLOAT_LITERAL);
    EXPECT_DOUBLE_EQ(t2.value.float_value, 0.5);

    Token t3 = lexer->nextToken();
    EXPECT_EQ(t3.type, TokenType::FLOAT_LITERAL);
    EXPECT_NEAR(t3.value.float_value, 1.23e10, 1e5);

    Token t4 = lexer->nextToken();
    EXPECT_EQ(t4.type, TokenType::FLOAT_LITERAL);
    EXPECT_NEAR(t4.value.float_value, 2.5e-3, 1e-6);
}

TEST_F(FirebirdLexerTest, StringLiterals) {
    auto lexer = makeLexer("'hello' 'world''s' ''");

    Token t1 = lexer->nextToken();
    EXPECT_EQ(t1.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer->stringPool().get(t1.value.string_id), "hello");

    Token t2 = lexer->nextToken();
    EXPECT_EQ(t2.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer->stringPool().get(t2.value.string_id), "world's");

    Token t3 = lexer->nextToken();
    EXPECT_EQ(t3.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer->stringPool().get(t3.value.string_id), "");
}

TEST_F(FirebirdLexerTest, QStringLiterals) {
    auto lexer = makeLexer("Q'{hello world}'");

    Token t = lexer->nextToken();
    EXPECT_EQ(t.type, TokenType::Q_STRING_LITERAL);
    EXPECT_EQ(lexer->stringPool().get(t.value.string_id), "hello world");
}

TEST_F(FirebirdLexerTest, QStringLiterals_DifferentDelimiters) {
    auto testQ = [this](const char* input, const char* expected) {
        auto lexer = makeLexer(input);
        Token t = lexer->nextToken();
        EXPECT_EQ(t.type, TokenType::Q_STRING_LITERAL) << "Input: " << input;
        EXPECT_EQ(lexer->stringPool().get(t.value.string_id), expected) << "Input: " << input;
    };

    testQ("Q'{text}'", "text");
    testQ("Q'[text]'", "text");
    testQ("Q'(text)'", "text");
    testQ("Q'<text>'", "text");
    testQ("Q'!text!'", "text");
}

TEST_F(FirebirdLexerTest, BlobLiterals) {
    auto lexer = makeLexer("X'DEADBEEF' x'1234'");

    Token t1 = lexer->nextToken();
    EXPECT_EQ(t1.type, TokenType::BLOB_LITERAL);
    EXPECT_EQ(lexer->stringPool().get(t1.value.string_id), "DEADBEEF");

    Token t2 = lexer->nextToken();
    EXPECT_EQ(t2.type, TokenType::BLOB_LITERAL);
    EXPECT_EQ(lexer->stringPool().get(t2.value.string_id), "1234");
}

TEST_F(FirebirdLexerTest, QuotedIdentifiers) {
    auto lexer = makeLexer("\"MyTable\" \"Column Name\" \"SELECT\"");

    Token t1 = lexer->nextToken();
    EXPECT_EQ(t1.type, TokenType::QUOTED_IDENTIFIER);
    EXPECT_TRUE(t1.is_delimited);
    EXPECT_EQ(lexer->stringPool().get(t1.value.string_id), "MyTable");

    Token t2 = lexer->nextToken();
    EXPECT_EQ(t2.type, TokenType::QUOTED_IDENTIFIER);
    EXPECT_EQ(lexer->stringPool().get(t2.value.string_id), "Column Name");

    Token t3 = lexer->nextToken();
    EXPECT_EQ(t3.type, TokenType::QUOTED_IDENTIFIER);
    EXPECT_EQ(lexer->stringPool().get(t3.value.string_id), "SELECT");
}

// =============================================================================
// Operator Tests
// =============================================================================

TEST_F(FirebirdLexerTest, ArithmeticOperators) {
    auto tokens = tokenize("+ - * /");

    EXPECT_EQ(tokens[0].type, TokenType::PLUS);
    EXPECT_EQ(tokens[1].type, TokenType::MINUS);
    EXPECT_EQ(tokens[2].type, TokenType::STAR);
    EXPECT_EQ(tokens[3].type, TokenType::SLASH);
}

TEST_F(FirebirdLexerTest, ComparisonOperators) {
    auto tokens = tokenize("= <> != < > <= >=");

    EXPECT_EQ(tokens[0].type, TokenType::EQUAL);
    EXPECT_EQ(tokens[1].type, TokenType::NOT_EQUAL);
    EXPECT_EQ(tokens[2].type, TokenType::NOT_EQUAL_BANG);
    EXPECT_EQ(tokens[3].type, TokenType::LESS_THAN);
    EXPECT_EQ(tokens[4].type, TokenType::GREATER_THAN);
    EXPECT_EQ(tokens[5].type, TokenType::LESS_EQUAL);
    EXPECT_EQ(tokens[6].type, TokenType::GREATER_EQUAL);
}

TEST_F(FirebirdLexerTest, FirebirdSpecificOperators) {
    auto tokens = tokenize("!< !> ~= ^=");

    EXPECT_EQ(tokens[0].type, TokenType::NOT_LESS);
    EXPECT_EQ(tokens[1].type, TokenType::NOT_GREATER);
    EXPECT_EQ(tokens[2].type, TokenType::NOT_EQUAL_TILDE);
    EXPECT_EQ(tokens[3].type, TokenType::NOT_EQUAL_CARET);
}

TEST_F(FirebirdLexerTest, ConcatenationOperator) {
    auto tokens = tokenize("||");
    EXPECT_EQ(tokens[0].type, TokenType::DOUBLE_PIPE);
}

TEST_F(FirebirdLexerTest, AssignmentOperator) {
    auto tokens = tokenize(":=");
    EXPECT_EQ(tokens[0].type, TokenType::COLON_EQUALS);
}

// =============================================================================
// Parameter Tests
// =============================================================================

TEST_F(FirebirdLexerTest, NamedParameters) {
    auto lexer = makeLexer(":param1 :my_param");

    Token t1 = lexer->nextToken();
    EXPECT_EQ(t1.type, TokenType::PARAMETER);
    EXPECT_EQ(lexer->stringPool().get(t1.value.string_id), "param1");

    Token t2 = lexer->nextToken();
    EXPECT_EQ(t2.type, TokenType::PARAMETER);
    EXPECT_EQ(lexer->stringPool().get(t2.value.string_id), "my_param");
}

TEST_F(FirebirdLexerTest, PositionalParameter) {
    auto lexer = makeLexer("?");

    Token t = lexer->nextToken();
    EXPECT_EQ(t.type, TokenType::PARAMETER);
    EXPECT_EQ(lexer->stringPool().get(t.value.string_id), "?");
}

// =============================================================================
// Punctuation Tests
// =============================================================================

TEST_F(FirebirdLexerTest, Punctuation) {
    auto tokens = tokenize("( ) [ ] , ; .");

    EXPECT_EQ(tokens[0].type, TokenType::LEFT_PAREN);
    EXPECT_EQ(tokens[1].type, TokenType::RIGHT_PAREN);
    EXPECT_EQ(tokens[2].type, TokenType::LEFT_BRACKET);
    EXPECT_EQ(tokens[3].type, TokenType::RIGHT_BRACKET);
    EXPECT_EQ(tokens[4].type, TokenType::COMMA);
    EXPECT_EQ(tokens[5].type, TokenType::SEMICOLON);
    EXPECT_EQ(tokens[6].type, TokenType::DOT);
}

// =============================================================================
// Complete Statement Tests
// =============================================================================

TEST_F(FirebirdLexerTest, SimpleSelect) {
    auto tokens = tokenize("SELECT id, name FROM employees WHERE active = TRUE;");

    EXPECT_EQ(tokens[0].type, TokenType::KW_SELECT);
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);    // id
    EXPECT_EQ(tokens[2].type, TokenType::COMMA);
    EXPECT_EQ(tokens[3].type, TokenType::KW_NAME);       // name is a non-reserved keyword
    EXPECT_EQ(tokens[4].type, TokenType::KW_FROM);
    EXPECT_EQ(tokens[5].type, TokenType::IDENTIFIER);    // employees
    EXPECT_EQ(tokens[6].type, TokenType::KW_WHERE);
    EXPECT_EQ(tokens[7].type, TokenType::KW_ACTIVE);     // active is a non-reserved keyword
    EXPECT_EQ(tokens[8].type, TokenType::EQUAL);
    EXPECT_EQ(tokens[9].type, TokenType::KW_TRUE);
    EXPECT_EQ(tokens[10].type, TokenType::SEMICOLON);
}

TEST_F(FirebirdLexerTest, SelectWithFirstSkip) {
    auto lexer = makeLexer("SELECT FIRST 10 SKIP 5 * FROM orders");

    EXPECT_EQ(lexer->nextToken().type, TokenType::KW_SELECT);
    EXPECT_EQ(lexer->nextToken().type, TokenType::KW_FIRST);   // FIRST is a keyword (parser handles context)
    EXPECT_EQ(lexer->nextToken().type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(lexer->nextToken().type, TokenType::KW_SKIP);    // SKIP is a keyword (parser handles context)
    EXPECT_EQ(lexer->nextToken().type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(lexer->nextToken().type, TokenType::STAR);
    EXPECT_EQ(lexer->nextToken().type, TokenType::KW_FROM);
}

TEST_F(FirebirdLexerTest, CreateProcedure) {
    auto tokens = tokenize(
        "CREATE PROCEDURE get_employee(emp_id INTEGER)\n"
        "RETURNS (emp_name VARCHAR(100))\n"
        "AS\n"
        "BEGIN\n"
        "  SELECT name FROM employees WHERE id = :emp_id INTO :emp_name;\n"
        "END"
    );

    EXPECT_EQ(tokens[0].type, TokenType::KW_CREATE);
    EXPECT_EQ(tokens[1].type, TokenType::KW_PROCEDURE);
    EXPECT_EQ(tokens[2].type, TokenType::IDENTIFIER);  // get_employee
}

TEST_F(FirebirdLexerTest, RDBSystemTable) {
    auto tokens = tokenize("SELECT * FROM RDB$RELATIONS");

    EXPECT_EQ(tokens[0].type, TokenType::KW_SELECT);
    EXPECT_EQ(tokens[1].type, TokenType::STAR);
    EXPECT_EQ(tokens[2].type, TokenType::KW_FROM);
    EXPECT_EQ(tokens[3].type, TokenType::IDENTIFIER);  // RDB$RELATIONS
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(FirebirdLexerTest, UnterminatedString) {
    auto tokens = tokenize("'unterminated");

    // Should still produce a token
    EXPECT_GE(tokens.size(), 1);
    EXPECT_TRUE(error_reporter_->hasErrors() || tokens.back().type != TokenType::END_OF_FILE);
}

TEST_F(FirebirdLexerTest, InvalidCharacter) {
    auto tokens = tokenize("SELECT @");

    EXPECT_EQ(tokens[0].type, TokenType::KW_SELECT);
    EXPECT_EQ(tokens[1].type, TokenType::ERROR);
}
