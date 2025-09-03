#include <gtest/gtest.h>
#include "scratchbird/parser/lexer.h"
#include <limits>
#include <string>
#include <cmath>

using namespace scratchbird::parser;

class LexerEdgeCaseTest : public ::testing::Test {
protected:
    void expectError(const std::string& input, const std::string& expectedError = "") {
        SimpleErrorReporter reporter;
        Lexer lexer(input);
        lexer.setErrorReporter(&reporter);
        
        Token tok = lexer.nextToken();
        while (tok.type != TokenType::END_OF_FILE && tok.type != TokenType::ERROR) {
            tok = lexer.nextToken();
        }
        
        EXPECT_TRUE(reporter.hasErrors()) << "Expected error for input: " << input;
        if (!expectedError.empty() && reporter.hasErrors()) {
            EXPECT_NE(reporter.errors()[0].message.find(expectedError), std::string::npos)
                << "Error message doesn't contain: " << expectedError;
        }
    }
};

// ===== Integer Overflow Tests =====

TEST_F(LexerEdgeCaseTest, IntegerOverflow) {
    // Test maximum int64_t
    Lexer lexer1("9223372036854775807");  // 2^63 - 1
    Token tok = lexer1.nextToken();
    EXPECT_EQ(tok.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(tok.value.int_value, std::numeric_limits<int64_t>::max());
    
    // Test minimum int64_t (as negative number)
    Lexer lexer2("-9223372036854775808");  // -2^63
    tok = lexer2.nextToken();
    EXPECT_EQ(tok.type, TokenType::MINUS);
    tok = lexer2.nextToken();
    // This number is too large as positive (INT64_MAX is 9223372036854775807)
    EXPECT_EQ(tok.type, TokenType::ERROR);
    
    // Test overflow
    expectError("9223372036854775808", "Invalid integer");
    expectError("99999999999999999999999999999999", "Invalid integer");
}

// ===== String Literal Edge Cases =====

TEST_F(LexerEdgeCaseTest, EmptyString) {
    Lexer lexer("''");
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "");
}

TEST_F(LexerEdgeCaseTest, UnterminatedString) {
    expectError("'hello", "Unterminated string literal");
    expectError("'hello\n", "Unterminated string literal");
}

TEST_F(LexerEdgeCaseTest, StringWithAllEscapes) {
    Lexer lexer("'\\n\\t\\r\\\\\\'\\\"'");
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
    std::string_view str = lexer.stringPool().get(tok.value.string_id);
    EXPECT_EQ(str, "\n\t\r\\'\"");
}

TEST_F(LexerEdgeCaseTest, InvalidEscapeSequence) {
    // The lexer accepts any character after backslash, no validation
    Lexer lexer1("'\\x'");
    Token tok = lexer1.nextToken();
    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer1.stringPool().get(tok.value.string_id), "x");
    
    Lexer lexer2("'\\u'");
    tok = lexer2.nextToken();
    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer2.stringPool().get(tok.value.string_id), "u");
    
    Lexer lexer3("'\\7'");
    tok = lexer3.nextToken();
    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer3.stringPool().get(tok.value.string_id), "7");
}

TEST_F(LexerEdgeCaseTest, VeryLongString) {
    std::string longStr = "'";
    longStr.append(10000, 'a');
    longStr += "'";
    
    Lexer lexer(longStr);
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id).length(), 10000);
}

// ===== Identifier Edge Cases =====

TEST_F(LexerEdgeCaseTest, IdentifierStartingWithUnderscore) {
    Lexer lexer("_id __private _123");
    
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "_id");
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "__private");
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "_123");
}

TEST_F(LexerEdgeCaseTest, IdentifierWithNumbers) {
    Lexer lexer("id123 test_456 column1");
    
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "id123");
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "test_456");
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "column1");
}

TEST_F(LexerEdgeCaseTest, VeryLongIdentifier) {
    std::string longId(1000, 'a');
    Lexer lexer(longId);
    
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id).length(), 1000);
}

TEST_F(LexerEdgeCaseTest, KeywordLikeIdentifiers) {
    // Test identifiers that start with keywords
    Lexer lexer("selecting created from_table");
    
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);  // Not KW_SELECT
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);  // Not KW_CREATE
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);  // Not KW_FROM
}

// ===== Number Edge Cases =====

TEST_F(LexerEdgeCaseTest, NumbersWithTrailingDot) {
    Lexer lexer("123.");
    
    // The lexer requires a digit after the decimal point for floats
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(tok.value.int_value, 123);
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::DOT);
}

TEST_F(LexerEdgeCaseTest, NumbersWithLeadingDot) {
    Lexer lexer(".123");
    
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::DOT);
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(tok.value.int_value, 123);
}

TEST_F(LexerEdgeCaseTest, ScientificNotationEdgeCases) {
    // Valid scientific notation
    Lexer lexer1("1e10 1E10 1e+10 1e-10 1.5e10");
    
    Token tok = lexer1.nextToken();
    EXPECT_EQ(tok.type, TokenType::FLOAT_LITERAL);
    EXPECT_DOUBLE_EQ(tok.value.float_value, 1e10);
    
    tok = lexer1.nextToken();
    EXPECT_EQ(tok.type, TokenType::FLOAT_LITERAL);
    EXPECT_DOUBLE_EQ(tok.value.float_value, 1E10);
    
    tok = lexer1.nextToken();
    EXPECT_EQ(tok.type, TokenType::FLOAT_LITERAL);
    EXPECT_DOUBLE_EQ(tok.value.float_value, 1e+10);
    
    tok = lexer1.nextToken();
    EXPECT_EQ(tok.type, TokenType::FLOAT_LITERAL);
    EXPECT_DOUBLE_EQ(tok.value.float_value, 1e-10);
    
    tok = lexer1.nextToken();
    EXPECT_EQ(tok.type, TokenType::FLOAT_LITERAL);
    EXPECT_DOUBLE_EQ(tok.value.float_value, 1.5e10);
}

TEST_F(LexerEdgeCaseTest, IncompleteScientificNotation) {
    // "123e" should be parsed as integer 123 followed by identifier 'e'
    Lexer lexer1("123e");
    Token tok = lexer1.nextToken();
    EXPECT_EQ(tok.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(tok.value.int_value, 123);
    tok = lexer1.nextToken();
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer1.stringPool().get(tok.value.string_id), "e");
    
    // "123e+" should be parsed as integer followed by identifier and plus
    Lexer lexer2("123e+");
    tok = lexer2.nextToken();
    EXPECT_EQ(tok.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(tok.value.int_value, 123);
    tok = lexer2.nextToken();
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer2.stringPool().get(tok.value.string_id), "e");
    tok = lexer2.nextToken();
    EXPECT_EQ(tok.type, TokenType::PLUS);
}

TEST_F(LexerEdgeCaseTest, FloatOverflowAndUnderflow) {
    // Test very large float
    Lexer lexer1("1e308");
    Token tok = lexer1.nextToken();
    EXPECT_EQ(tok.type, TokenType::FLOAT_LITERAL);
    EXPECT_FALSE(std::isinf(tok.value.float_value));
    
    // Test float overflow
    expectError("1e309", "Invalid floating-point number");
    
    // Test very small float
    Lexer lexer2("1e-308");
    tok = lexer2.nextToken();
    EXPECT_EQ(tok.type, TokenType::FLOAT_LITERAL);
    EXPECT_NE(tok.value.float_value, 0.0);
}

// ===== Operator Edge Cases =====

TEST_F(LexerEdgeCaseTest, AdjacentOperators) {
    Lexer lexer("<><=>==");
    
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::NOT_EQUAL);  // <>
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::LESS_EQUAL);  // <=
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::GREATER_EQUAL);  // >=
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::EQUAL);  // =
}

TEST_F(LexerEdgeCaseTest, OperatorLikeSequences) {
    // Test that we don't create invalid multi-char operators
    Lexer lexer("<<>>===");
    
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::LESS_THAN);  // <
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::NOT_EQUAL);  // <>
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::GREATER_EQUAL);  // >=
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::EQUAL);  // =
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::EQUAL);  // =
}

// ===== Comment Edge Cases =====

TEST_F(LexerEdgeCaseTest, CommentAtEndOfFile) {
    Lexer lexer1("SELECT -- comment");
    Token tok = lexer1.nextToken();
    EXPECT_EQ(tok.type, TokenType::KW_SELECT);
    tok = lexer1.nextToken();
    EXPECT_EQ(tok.type, TokenType::END_OF_FILE);
    
    Lexer lexer2("SELECT /* comment");
    expectError("SELECT /* comment", "Unterminated comment");
}

TEST_F(LexerEdgeCaseTest, NestedBlockComments) {
    // SQL standard doesn't support nested comments
    Lexer lexer("/* outer /* inner */ still in comment */ end");
    
    Token tok = lexer.nextToken();
    // The first */ should end the comment
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "still");
}

TEST_F(LexerEdgeCaseTest, CommentWithSpecialChars) {
    Lexer lexer("-- Comment with 特殊字符 and émojis 🎉\nSELECT");
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::KW_SELECT);
}

// ===== Unicode and Special Character Tests =====

TEST_F(LexerEdgeCaseTest, UnicodeInStrings) {
    Lexer lexer("'Hello 世界' 'Émoji 🎉'");
    
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "Hello 世界");
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "Émoji 🎉");
}

TEST_F(LexerEdgeCaseTest, UnicodeInIdentifiers) {
    // The lexer might not validate UTF-8 in identifiers
    // Let's test what actually happens
    Lexer lexer1("创建表");
    Token tok = lexer1.nextToken();
    // Likely treats as invalid since not ASCII alpha
    EXPECT_EQ(tok.type, TokenType::ERROR);
    
    Lexer lexer2("café");
    tok = lexer2.nextToken();
    // 'caf' might be parsed as identifier, then 'é' as error
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer2.stringPool().get(tok.value.string_id), "caf");
}

// ===== Whitespace Edge Cases =====

TEST_F(LexerEdgeCaseTest, MixedWhitespace) {
    Lexer lexer("SELECT\t\n\r\n  \t  FROM");
    
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::KW_SELECT);
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::KW_FROM);
}

TEST_F(LexerEdgeCaseTest, ZeroWidthSpaces) {
    // Zero-width space (U+200B) and other invisible unicode
    std::string input = "SELECT\u200BFROM";  // Zero-width space between SELECT and FROM
    
    Lexer lexer(input);
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::KW_SELECT);
    
    tok = lexer.nextToken();
    // Zero-width space likely causes error
    EXPECT_EQ(tok.type, TokenType::ERROR);
}

// ===== Mixed Edge Cases =====

TEST_F(LexerEdgeCaseTest, NumberFollowedByIdentifier) {
    Lexer lexer("123abc 456.789def");
    
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(tok.value.int_value, 123);
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "abc");
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::FLOAT_LITERAL);
    EXPECT_DOUBLE_EQ(tok.value.float_value, 456.789);
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "def");
}

TEST_F(LexerEdgeCaseTest, SpecialSQLCharacters) {
    // Test handling of SQL special characters
    // These should parse SELECT then error on special char
    Lexer lexer1("SELECT $1");
    Token tok = lexer1.nextToken();
    EXPECT_EQ(tok.type, TokenType::KW_SELECT);
    tok = lexer1.nextToken();
    EXPECT_EQ(tok.type, TokenType::ERROR); // $ is invalid
    
    Lexer lexer2("SELECT @var");
    tok = lexer2.nextToken();
    EXPECT_EQ(tok.type, TokenType::KW_SELECT);
    tok = lexer2.nextToken();
    EXPECT_EQ(tok.type, TokenType::ERROR); // @ is invalid
}

// ===== Location Tracking Edge Cases =====

TEST_F(LexerEdgeCaseTest, LocationAfterLongToken) {
    std::string longId(1000, 'a');
    longId += " next";
    Lexer lexer(longId);
    
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.location.offset, 0);
    EXPECT_EQ(tok.length, 1000);
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.location.offset, 1001);
}

TEST_F(LexerEdgeCaseTest, LocationWithMixedNewlines) {
    Lexer lexer("line1\r\nline2\rline3\nline4");
    
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.location.line, 1);
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.location.line, 2);
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.location.line, 3);
    
    tok = lexer.nextToken();
    EXPECT_EQ(tok.location.line, 4);
}

// ===== Lookahead Edge Cases =====

TEST_F(LexerEdgeCaseTest, PeekDoesntAdvance) {
    Lexer lexer("A B C");
    
    Token peek1 = lexer.peekToken();
    EXPECT_EQ(peek1.type, TokenType::IDENTIFIER);
    
    Token peek2 = lexer.peekToken();
    EXPECT_EQ(peek2.type, TokenType::IDENTIFIER);
    EXPECT_EQ(peek1.location.offset, peek2.location.offset);
    
    Token next = lexer.nextToken();
    EXPECT_EQ(next.type, TokenType::IDENTIFIER);
    EXPECT_EQ(next.location.offset, peek1.location.offset);
}

TEST_F(LexerEdgeCaseTest, PeekAtEOF) {
    Lexer lexer("");
    
    Token peek = lexer.peekToken();
    EXPECT_EQ(peek.type, TokenType::END_OF_FILE);
    
    Token next = lexer.nextToken();
    EXPECT_EQ(next.type, TokenType::END_OF_FILE);
}