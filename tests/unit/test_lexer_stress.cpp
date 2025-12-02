#include <gtest/gtest.h>
#include "scratchbird/parser/lexer.h"
#include <chrono>
#include <sstream>
#include <random>

using namespace scratchbird::parser;
using namespace std::chrono;

class LexerStressTest : public ::testing::Test
{
protected:
    void measureTokenizationTime(const std::string &description, const std::string &input,
                                 size_t expectedTokens)
    {
        auto start = high_resolution_clock::now();

        Lexer lexer(input);
        size_t tokenCount = 0;
        Token tok;
        do
        {
            tok = lexer.nextToken();
            tokenCount++;
        } while (tok.type != TokenType::END_OF_FILE && tok.type != TokenType::ERROR);

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);

        std::cout << description << ": " << tokenCount << " tokens in " << duration.count()
                  << " μs (" << (tokenCount * 1000000.0 / duration.count()) << " tokens/sec)"
                  << std::endl;

        EXPECT_GE(tokenCount, expectedTokens);
    }
};

// ===== Large Input Tests =====

TEST_F(LexerStressTest, VeryLargeIdentifier)
{
    // Test 1MB identifier - lexer should reject it quickly (max 128 chars per SQL standard)
    std::string largeId(1024 * 1024, 'a');

    auto start = high_resolution_clock::now();
    Lexer lexer(largeId);
    Token tok = lexer.nextToken();
    auto end = high_resolution_clock::now();

    // Lexer correctly rejects overlength identifiers
    EXPECT_EQ(tok.type, TokenType::ERROR);

    auto duration = duration_cast<milliseconds>(end - start);
    EXPECT_LT(duration.count(), 200) << "Should handle 1MB input in < 200ms";
}

TEST_F(LexerStressTest, VeryLargeString)
{
    // Test 1MB string literal
    std::string largeStr = "'";
    largeStr.append(1024 * 1024, 'x');
    largeStr += "'";

    auto start = high_resolution_clock::now();
    Lexer lexer(largeStr);
    Token tok = lexer.nextToken();
    auto end = high_resolution_clock::now();

    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);

    auto duration = duration_cast<milliseconds>(end - start);
    EXPECT_LT(duration.count(), 100) << "Should tokenize 1MB string in < 100ms";
}

TEST_F(LexerStressTest, ManySmallTokens)
{
    // Generate 100k small tokens
    std::stringstream ss;
    for (int i = 0; i < 100000; i++)
    {
        ss << "id" << i << " ";
    }
    std::string input = ss.str();

    measureTokenizationTime("100k identifiers", input, 100000);
}

TEST_F(LexerStressTest, ManyKeywords)
{
    // Generate 10k keywords
    std::stringstream ss;
    const char *keywords[] = {"SELECT", "FROM", "WHERE", "INSERT", "CREATE", "TABLE"};
    for (int i = 0; i < 10000; i++)
    {
        ss << keywords[i % 6] << " ";
    }
    std::string input = ss.str();

    measureTokenizationTime("10k keywords", input, 10000);
}

TEST_F(LexerStressTest, ManyNumbers)
{
    // Generate mixed integers and floats
    std::stringstream ss;
    for (int i = 0; i < 50000; i++)
    {
        if (i % 2 == 0)
        {
            ss << i << " ";
        }
        else
        {
            ss << (i * 0.1) << " ";
        }
    }
    std::string input = ss.str();

    measureTokenizationTime("50k numbers", input, 50000);
}

TEST_F(LexerStressTest, ComplexSQL)
{
    // Generate complex SQL with many different token types
    std::stringstream ss;
    for (int i = 0; i < 1000; i++)
    {
        ss << "INSERT INTO table" << i << " (id, name, value) VALUES (" << i << ", 'name" << i
           << "', " << (i * 0.5) << "); ";
    }
    std::string input = ss.str();

    measureTokenizationTime("1k INSERT statements", input, 17000); // ~17 tokens per INSERT
}

// ===== Memory Stress Tests =====

TEST_F(LexerStressTest, StringPoolStress)
{
    // Test string pool with many unique strings
    std::stringstream ss;
    for (int i = 0; i < 10000; i++)
    {
        ss << "'unique_string_" << i << "' ";
    }
    std::string input = ss.str();

    Lexer lexer(input);
    size_t stringCount = 0;
    Token tok;
    do
    {
        tok = lexer.nextToken();
        if (tok.type == TokenType::STRING_LITERAL)
        {
            stringCount++;
            // Verify we can still access the string
            std::string_view str = lexer.stringPool().get(tok.value.string_id);
            EXPECT_FALSE(str.empty());
        }
    } while (tok.type != TokenType::END_OF_FILE);

    EXPECT_EQ(stringCount, 10000);
}

TEST_F(LexerStressTest, IdentifierPoolStress)
{
    // Test with many repeated identifiers (should be interned)
    // Note: avoid SQL keywords like TABLE, INDEX, VALUE, COLUMN which are not identifiers
    std::stringstream ss;
    const char *identifiers[] = {"foo", "bar", "baz", "qux", "quux"};
    for (int i = 0; i < 50000; i++)
    {
        ss << identifiers[i % 5] << " ";
    }
    std::string input = ss.str();

    Lexer lexer(input);
    Token tok;
    size_t tokenCount = 0;
    do
    {
        tok = lexer.nextToken();
        if (tok.type == TokenType::IDENTIFIER)
        {
            tokenCount++;
        }
    } while (tok.type != TokenType::END_OF_FILE);

    EXPECT_EQ(tokenCount, 50000);
    // String pool should only have 5 unique strings despite 50k tokens
}

// ===== Edge Case Stress Tests =====

TEST_F(LexerStressTest, DeepCommentNesting)
{
    // Generate deeply nested comments (though SQL doesn't support this)
    std::stringstream ss;
    ss << "SELECT ";
    for (int i = 0; i < 100; i++)
    {
        ss << "/* comment " << i << " ";
    }
    ss << "nested";
    for (int i = 0; i < 100; i++)
    {
        ss << " */";
    }
    ss << " FROM table";

    std::string input = ss.str();
    Lexer lexer(input);

    // Should handle this gracefully even if not supporting nested comments
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::KW_SELECT);
}

TEST_F(LexerStressTest, VeryLongLine)
{
    // Test a single line with 100k characters
    std::stringstream ss;
    ss << "SELECT ";
    for (int i = 0; i < 10000; i++)
    {
        ss << "col" << i << ", ";
    }
    ss << "colLast FROM table";
    std::string input = ss.str();

    // Expected tokens: 1 SELECT + 10001 identifiers + 10000 commas + 1 FROM + 1 TABLE = 20004
    measureTokenizationTime("10k column SELECT", input, 20000);
}

TEST_F(LexerStressTest, ManyEscapedStrings)
{
    // Test many strings with escape sequences
    std::stringstream ss;
    for (int i = 0; i < 5000; i++)
    {
        ss << "'line\\n" << i << "\\ttab\\\\slash\\'' ";
    }
    std::string input = ss.str();

    auto start = high_resolution_clock::now();

    Lexer lexer(input);
    size_t stringCount = 0;
    Token tok;
    do
    {
        tok = lexer.nextToken();
        if (tok.type == TokenType::STRING_LITERAL)
        {
            stringCount++;
            // Verify escape sequences were processed
            std::string_view str = lexer.stringPool().get(tok.value.string_id);
            EXPECT_NE(str.find('\n'), std::string::npos);
            EXPECT_NE(str.find('\t'), std::string::npos);
        }
    } while (tok.type != TokenType::END_OF_FILE);

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);

    EXPECT_EQ(stringCount, 5000);
    std::cout << "Tokenized 5k escaped strings in " << duration.count() << " ms" << std::endl;
}

// ===== Random Input Tests =====

TEST_F(LexerStressTest, RandomValidSQL)
{
    // Generate random but valid SQL tokens
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> tokenType(0, 5);
    std::uniform_int_distribution<> number(0, 1000000);

    std::stringstream ss;
    for (int i = 0; i < 10000; i++)
    {
        switch (tokenType(gen))
        {
            case 0: // Keyword
                ss << "SELECT ";
                break;
            case 1: // Identifier
                ss << "id" << number(gen) << " ";
                break;
            case 2: // Integer
                ss << number(gen) << " ";
                break;
            case 3: // Float
                ss << (number(gen) * 0.01) << " ";
                break;
            case 4: // String
                ss << "'str" << number(gen) << "' ";
                break;
            case 5: // Operator
                ss << "= ";
                break;
        }
    }

    std::string input = ss.str();
    measureTokenizationTime("10k random tokens", input, 10000);
}

// ===== Worst Case Performance =====

TEST_F(LexerStressTest, WorstCaseBacktracking)
{
    // Test cases that might cause backtracking in poorly implemented lexers

    // Many incomplete tokens
    std::stringstream ss;
    for (int i = 0; i < 1000; i++)
    {
        ss << "123e 456. .789 <> <= >= "; // Tokens that require lookahead
    }
    std::string input = ss.str();

    auto start = high_resolution_clock::now();
    Lexer lexer(input);
    size_t tokenCount = 0;
    Token tok;
    do
    {
        tok = lexer.nextToken();
        tokenCount++;
    } while (tok.type != TokenType::END_OF_FILE);
    auto end = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(end - start);
    EXPECT_LT(duration.count(), 50) << "Should handle lookahead cases efficiently";
}

// ===== Concurrent String Pool Access =====

TEST_F(LexerStressTest, StringPoolConsistency)
{
    // Test that string pool maintains consistency under stress
    std::vector<std::pair<StringPool::StringId, std::string>> strings;

    // Create many strings
    Lexer lexer("");
    for (int i = 0; i < 10000; i++)
    {
        std::string str = "test_string_" + std::to_string(i);
        auto id = lexer.stringPool().intern(str);
        strings.push_back({id, str});
    }

    // Verify all strings are still accessible
    for (const auto &[id, expected] : strings)
    {
        std::string_view actual = lexer.stringPool().get(id);
        EXPECT_EQ(actual, expected);
    }
}

// ===== Input Boundary Tests =====

TEST_F(LexerStressTest, EmptyInputPerformance)
{
    // Ensure empty input is handled quickly
    auto start = high_resolution_clock::now();
    for (int i = 0; i < 100000; i++)
    {
        Lexer lexer("");
        Token tok = lexer.nextToken();
        EXPECT_EQ(tok.type, TokenType::END_OF_FILE);
    }
    auto end = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(end - start);
    EXPECT_LT(duration.count(), 100) << "100k empty lexers should initialize quickly";
}

TEST_F(LexerStressTest, SingleCharacterInputs)
{
    // Test many single-character inputs
    const char *singles = "+-*/=<>(),;.";

    auto start = high_resolution_clock::now();
    for (int i = 0; i < 10000; i++)
    {
        for (size_t j = 0; j < strlen(singles); j++)
        {
            std::string input(1, singles[j]);
            Lexer lexer(input);
            Token tok = lexer.nextToken();
            EXPECT_NE(tok.type, TokenType::ERROR);
        }
    }
    auto end = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(end - start);
    std::cout << "120k single-char tokenizations in " << duration.count() << " ms" << std::endl;
}

// ===== Error Recovery Stress =====

TEST_F(LexerStressTest, ManyErrors)
{
    // Generate input with many errors
    std::stringstream ss;
    for (int i = 0; i < 1000; i++)
    {
        ss << "'unterminated" << i << " ";
        ss << "valid" << i << " ";
        ss << "@invalid" << i << " ";
    }
    std::string input = ss.str();

    SimpleErrorReporter reporter;
    Lexer lexer(input);
    lexer.setErrorReporter(&reporter);

    auto start = high_resolution_clock::now();
    Token tok;
    size_t errorCount = 0;
    do
    {
        tok = lexer.nextToken();
        if (tok.type == TokenType::ERROR)
        {
            errorCount++;
        }
    } while (tok.type != TokenType::END_OF_FILE);
    auto end = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(end - start);
    EXPECT_GT(errorCount, 0);
    EXPECT_GT(reporter.errorCount(), 0);
    std::cout << "Handled " << errorCount << " errors in " << duration.count() << " ms"
              << std::endl;
}