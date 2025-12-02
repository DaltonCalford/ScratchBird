#include <gtest/gtest.h>
#include "scratchbird/parser/lexer.h"
#include <limits>
#include <thread>
#include <vector>
#include <atomic>

using namespace scratchbird::parser;

class LexerSecurityTest : public ::testing::Test
{
protected:
    bool hasError(const std::string &input)
    {
        SimpleErrorReporter reporter;
        Lexer lexer(input);
        lexer.setErrorReporter(&reporter);

        Token tok;
        do
        {
            tok = lexer.nextToken();
        } while (tok.type != TokenType::END_OF_FILE && tok.type != TokenType::ERROR);

        return reporter.hasErrors() || tok.type == TokenType::ERROR;
    }

    bool canTokenizeWithinTime(const std::string &input, int maxMillis)
    {
        auto start = std::chrono::high_resolution_clock::now();

        Lexer lexer(input);
        Token tok;
        do
        {
            tok = lexer.nextToken();

            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            if (elapsed.count() > maxMillis)
            {
                return false; // Took too long
            }
        } while (tok.type != TokenType::END_OF_FILE && tok.type != TokenType::ERROR);

        return true;
    }
};

// ===== Buffer Overflow Prevention =====

TEST_F(LexerSecurityTest, PreventBufferOverflow)
{
    // Test that we handle inputs larger than any reasonable buffer
    // The lexer should reject overlength identifiers (max 128 chars per SQL standard)
    // but must not crash or have buffer overflows
    size_t sizes[] = {
        1024,            // 1 KB
        1024 * 1024,     // 1 MB
        10 * 1024 * 1024 // 10 MB
    };

    for (size_t size : sizes)
    {
        std::string largeInput(size, 'a');

        // Should handle without crashing - lexer correctly rejects overlength identifiers
        Lexer lexer(largeInput);
        Token tok = lexer.nextToken();
        // Lexer returns ERROR for identifiers > 128 chars (SQL standard limit)
        EXPECT_EQ(tok.type, TokenType::ERROR);
    }
}

TEST_F(LexerSecurityTest, StringWithNullBytes)
{
    // Test string containing null bytes
    std::string nullStr = "'hello";
    nullStr.push_back('\0');
    nullStr += "world'";

    Lexer lexer(nullStr);
    Token tok = lexer.nextToken();

    // Should handle null bytes safely
    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
    std::string_view str = lexer.stringPool().get(tok.value.string_id);
    EXPECT_EQ(str.length(), 11); // "hello\0world"
}

// ===== Integer Overflow Prevention =====

TEST_F(LexerSecurityTest, IntegerOverflowPrevention)
{
    // Numbers that would overflow int64_t (lexer uses 64-bit integers)
    // Note: 2147483648 fits in int64_t so it's valid
    const char *overflowNumbers[] = {"9223372036854775808",  // int64_t max + 1
                                     "18446744073709551616", // uint64_t max + 1
                                     "99999999999999999999999999999999999999999999999999"};

    for (const char *num : overflowNumbers)
    {
        EXPECT_TRUE(hasError(num)) << "Should reject overflow: " << num;
    }

    // Verify that values within int64_t range are accepted
    Lexer lexer("2147483648");  // int32_t max + 1, but fits in int64_t
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(tok.value.int_value, 2147483648LL);
}

TEST_F(LexerSecurityTest, FloatOverflowPrevention)
{
    // Floats that would overflow
    EXPECT_TRUE(hasError("1e400"));  // Exceeds double max
    EXPECT_TRUE(hasError("1e1000")); // Way beyond double max

    // Should accept max double
    Lexer lexer("1.7976931348623157e308");
    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::FLOAT_LITERAL);
}

// ===== Denial of Service Prevention =====

TEST_F(LexerSecurityTest, ExponentialBacktrackingPrevention)
{
    // Patterns that could cause exponential behavior in regex-based lexers

    // Repeated lookahead patterns
    std::string badPattern;
    for (int i = 0; i < 100; i++)
    {
        badPattern += "123e+"; // Invalid scientific notation
    }

    // Should complete quickly despite many errors
    EXPECT_TRUE(canTokenizeWithinTime(badPattern, 100));
}

TEST_F(LexerSecurityTest, DeepNestingDoS)
{
    // Deeply nested comments (even though SQL doesn't support nesting)
    std::string deepNest;
    for (int i = 0; i < 10000; i++)
    {
        deepNest += "/*";
    }
    deepNest += "comment";
    for (int i = 0; i < 10000; i++)
    {
        deepNest += "*/";
    }

    // Should handle without stack overflow or hanging
    EXPECT_TRUE(canTokenizeWithinTime(deepNest, 100));
}

TEST_F(LexerSecurityTest, RepeatedErrorRecovery)
{
    // Many consecutive errors shouldn't cause performance issues
    std::string manyErrors;
    for (int i = 0; i < 10000; i++)
    {
        manyErrors += "@#$%^& "; // Invalid characters
    }

    // Should handle errors efficiently
    EXPECT_TRUE(canTokenizeWithinTime(manyErrors, 1000));
}

// ===== Memory Exhaustion Prevention =====

TEST_F(LexerSecurityTest, StringPoolExhaustion)
{
    // Try to exhaust memory with unique strings
    std::stringstream ss;

    // Generate 100k unique strings (but not too large)
    for (int i = 0; i < 100000; i++)
    {
        ss << "'unique_string_number_" << i << "' ";
    }

    std::string input = ss.str();

    // Should handle without exhausting memory
    // Monitor that this completes in reasonable time
    EXPECT_TRUE(canTokenizeWithinTime(input, 5000));
}

TEST_F(LexerSecurityTest, PreventQuadraticBehavior)
{
    // Input that might cause O(n²) behavior in naive implementations
    // Note: consecutive '<' chars are parsed as << (SHIFT_LEFT) operators
    std::string quadratic(10000, '<');

    // Many < characters - they get parsed as << operators
    // Should complete in linear time
    auto start = std::chrono::high_resolution_clock::now();

    Lexer lexer(quadratic);
    size_t tokenCount = 0;
    Token tok;
    do
    {
        tok = lexer.nextToken();
        // << is parsed as SHIFT_LEFT, so 10000 '<' chars = 5000 tokens
        if (tok.type == TokenType::SHIFT_LEFT || tok.type == TokenType::LESS_THAN)
        {
            tokenCount++;
        }
    } while (tok.type != TokenType::END_OF_FILE);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 10000 '<' chars become 5000 '<<' tokens
    EXPECT_EQ(tokenCount, 5000);
    EXPECT_LT(duration.count(), 100) << "Should tokenize in linear time";
}

// ===== Input Validation =====

TEST_F(LexerSecurityTest, RejectInvalidUTF8)
{
    // Note: The lexer validates UTF-8 in identifiers but allows arbitrary bytes in strings.
    // This is intentional - string literals can contain any data.

    // Test invalid UTF-8 in identifiers (should be rejected)
    std::string invalid_id1;
    invalid_id1 += '\xFF';
    invalid_id1 += '\xFE';
    invalid_id1 += "hello";

    // Invalid UTF-8 bytes that look like identifier starts
    Lexer lexer1(invalid_id1);
    Token tok1 = lexer1.nextToken();
    // Lexer should reject or handle invalid UTF-8 in identifiers
    // The lexer treats these as invalid characters
    EXPECT_TRUE(tok1.type == TokenType::ERROR || tok1.type != TokenType::IDENTIFIER)
        << "Invalid UTF-8 bytes should not produce a valid identifier";

    // String literals accept arbitrary bytes (binary data)
    std::string valid_str = "'hello\xFF\xFEworld'";
    Lexer lexer2(valid_str);
    Token tok2 = lexer2.nextToken();
    EXPECT_EQ(tok2.type, TokenType::STRING_LITERAL);
}

TEST_F(LexerSecurityTest, ControlCharacterHandling)
{
    // Test various control characters
    for (char c = 0; c < 32; c++)
    {
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ')
        {
            continue; // These are valid whitespace
        }

        std::string input = "SELECT";
        input.push_back(c);
        input += "FROM";

        // Most control characters should be rejected
        SimpleErrorReporter reporter;
        Lexer lexer(input);
        lexer.setErrorReporter(&reporter);

        Token tok;
        size_t validTokens = 0;
        do
        {
            tok = lexer.nextToken();
            if (tok.type != TokenType::ERROR && tok.type != TokenType::END_OF_FILE)
            {
                validTokens++;
            }
        } while (tok.type != TokenType::END_OF_FILE);

        // Should either error or skip the control character
        EXPECT_TRUE(reporter.hasErrors() || validTokens == 2)
            << "Control char " << (int)c << " not handled properly";
    }
}

// ===== SQL Injection Prevention Helpers =====

TEST_F(LexerSecurityTest, QuoteEscaping)
{
    // Ensure quotes are properly handled using backslash escaping
    // Note: lexer uses C-style backslash escaping, not SQL-style doubled quotes
    Lexer lexer("'O\\'Brien' 'can\\'t' 'it\\'s'");

    Token tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "O'Brien");

    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "can't");

    tok = lexer.nextToken();
    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), "it's");
}

TEST_F(LexerSecurityTest, CommentInjection)
{
    // Ensure comments can't be injected to change SQL meaning
    Lexer lexer("SELECT * FROM users WHERE id = 1 -- AND admin = true");

    std::vector<TokenType> tokens;
    Token tok;
    do
    {
        tok = lexer.nextToken();
        if (tok.type != TokenType::END_OF_FILE)
        {
            tokens.push_back(tok.type);
        }
    } while (tok.type != TokenType::END_OF_FILE);

    // Should end after the 1, comment consumes the rest
    EXPECT_EQ(tokens.size(), 8); // SELECT * FROM users WHERE id = 1
}

// ===== Resource Limit Tests =====

TEST_F(LexerSecurityTest, MaxIdentifierLength)
{
    // Test that extremely long identifiers are rejected (max 128 chars per SQL standard)
    std::string veryLongId(1'000'000, 'a');

    auto start = std::chrono::high_resolution_clock::now();
    Lexer lexer(veryLongId);
    Token tok = lexer.nextToken();
    auto end = std::chrono::high_resolution_clock::now();

    // Lexer correctly rejects identifiers > 128 characters
    EXPECT_EQ(tok.type, TokenType::ERROR);

    // Should complete quickly even for 1MB input
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 200);  // Allow some margin for CI variability

    // Verify valid-length identifier is accepted
    std::string validId(128, 'a');  // Exactly 128 chars
    Lexer lexer2(validId);
    Token tok2 = lexer2.nextToken();
    EXPECT_EQ(tok2.type, TokenType::IDENTIFIER);
}

TEST_F(LexerSecurityTest, MaxStringLength)
{
    // Test maximum string length handling
    std::string veryLongStr = "'";
    veryLongStr.append(1'000'000, 'x');
    veryLongStr += "'";

    auto start = std::chrono::high_resolution_clock::now();
    Lexer lexer(veryLongStr);
    Token tok = lexer.nextToken();
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);

    // Should complete quickly
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 50);
}

// ===== Concurrency Safety =====

TEST_F(LexerSecurityTest, MultipleLexxersIndependent)
{
    // Multiple lexers should not interfere with each other
    const int NUM_THREADS = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    auto lexTask = [&successCount](int id)
    {
        std::string input = "SELECT * FROM table" + std::to_string(id);
        Lexer lexer(input);

        Token tok = lexer.nextToken();
        if (tok.type == TokenType::KW_SELECT)
        {
            successCount++;
        }
    };

    // Launch threads
    for (int i = 0; i < NUM_THREADS; i++)
    {
        threads.emplace_back(lexTask, i);
    }

    // Wait for completion
    for (auto &t : threads)
    {
        t.join();
    }

    EXPECT_EQ(successCount, NUM_THREADS);
}

// ===== Path Traversal Prevention =====

TEST_F(LexerSecurityTest, NoFileSystemAccess)
{
    // Ensure lexer doesn't try to access file system
    // These should all be treated as regular tokens, not file paths
    // Note: The lexer uses C-style backslash escaping, so Windows paths need extra escapes
    struct PathTest {
        const char* sql_input;   // What goes in the SQL string literal
        const char* expected;    // What the lexer produces after escape processing
    };
    PathTest tests[] = {
        {"../../../etc/passwd", "../../../etc/passwd"},
        {"C:/Windows/System32/config", "C:/Windows/System32/config"},  // Use forward slashes
        {"~/.ssh/id_rsa", "~/.ssh/id_rsa"},
        {"/dev/null", "/dev/null"},
        {"//server/share/file", "//server/share/file"}  // UNC with forward slashes
    };

    for (const auto& test : tests)
    {
        std::string input = "SELECT '" + std::string(test.sql_input) + "'";
        Lexer lexer(input);

        Token tok = lexer.nextToken();
        EXPECT_EQ(tok.type, TokenType::KW_SELECT);

        tok = lexer.nextToken();
        EXPECT_EQ(tok.type, TokenType::STRING_LITERAL);
        // Should treat as literal string, not try to access file
        EXPECT_EQ(lexer.stringPool().get(tok.value.string_id), test.expected);
    }
}

// ===== Special Character Bombs =====

TEST_F(LexerSecurityTest, UnicodeBombs)
{
    // Unicode characters that have caused issues in other systems

    // Right-to-left override
    std::string rtl = "SELECT * FROM users WHERE name = '\u202E' --";
    EXPECT_TRUE(hasError(rtl) || canTokenizeWithinTime(rtl, 10));

    // Zero-width characters
    std::string zeroWidth = "SEL\u200BECT FR\u200COM";
    EXPECT_TRUE(hasError(zeroWidth));

    // Combining characters explosion
    std::string combining = "'A";
    for (int i = 0; i < 100; i++)
    {
        combining += "\u0301"; // Combining acute accent
    }
    combining += "'";
    EXPECT_TRUE(canTokenizeWithinTime(combining, 50));
}

// ===== Error Message Information Leakage =====

TEST_F(LexerSecurityTest, ErrorMessagesSafe)
{
    // Ensure error messages don't leak sensitive information
    SimpleErrorReporter reporter;

    // Test various error conditions
    std::vector<std::string> errorInputs = {
        "'unterminated string with password=secret123",
        "/* unterminated comment with api_key=abc123",
        "123456789012345678901234567890", // Very long number
        "@#$%^&*()_+"                     // Invalid characters
    };

    for (const auto &input : errorInputs)
    {
        reporter.clear();
        Lexer lexer(input);
        lexer.setErrorReporter(&reporter);

        Token tok;
        do
        {
            tok = lexer.nextToken();
        } while (tok.type != TokenType::END_OF_FILE);

        // Check that error messages don't contain the full input
        for (const auto &error : reporter.errors())
        {
            // Error message should be generic, not include sensitive data
            EXPECT_EQ(error.message.find("password="), std::string::npos);
            EXPECT_EQ(error.message.find("api_key="), std::string::npos);
        }
    }
}