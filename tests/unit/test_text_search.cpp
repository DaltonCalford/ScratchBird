/**
 * Text Search Parser Tests
 *
 * Tests for text search and regex SQL constructs.
 * Tests marked DISABLED_ require parser features not yet implemented.
 * These will be enabled during Alpha 2 parser enhancement phase.
 *
 * Currently supported:
 * - ILIKE operator
 * - Regex operators (~, ~*, !~, !~*)
 * - REGEXP_MATCHES (2-3 args)
 * - REGEXP_REPLACE (3 args)
 * - REGEXP_SPLIT_TO_ARRAY
 * - SPLIT_PART
 * - STRPOS
 * - INITCAP, ASCII, CHR, REPEAT
 *
 * Alpha 2 parser features (disabled):
 * - REGEXP_REPLACE with 4 args (flags)
 * - POSITION ... IN syntax
 * - OVERLAY ... PLACING ... FROM syntax
 * - QUOTE_LITERAL, QUOTE_IDENT
 * - REVERSE
 * - Table-valued functions: STRING_TO_TABLE, REGEXP_SPLIT_TO_TABLE, UNNEST_TEXT
 */

#include <gtest/gtest.h>

#include <string>

using namespace scratchbird::parser;

class TextSearchTest : public ::testing::Test
{
protected:
    void testParse(const std::string &sql, bool should_succeed = true)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        if (should_succeed)
        {
            ASSERT_TRUE(result.success()) << "Parse failed for: " << sql;
            // Note: We only test parsing here, not semantic analysis.
            // Semantic analysis requires tables to exist in the symbol table,
            // but these parser tests just verify SQL syntax is accepted.
        }
        else
        {
            EXPECT_FALSE(result.success()) << "Parse should have failed for: " << sql;
        }
    }
};

// ===== ILIKE Tests (SUPPORTED) =====

TEST_F(TextSearchTest, ILIKE_Basic)
{
    testParse("SELECT * FROM users WHERE email ILIKE '%@gmail.com'", true);
}

TEST_F(TextSearchTest, ILIKE_StartsWith)
{
    testParse("SELECT * FROM products WHERE name ILIKE 'apple%'", true);
}

TEST_F(TextSearchTest, ILIKE_EndsWith)
{
    testParse("SELECT * FROM logs WHERE message ILIKE '%error'", true);
}

TEST_F(TextSearchTest, ILIKE_SingleChar)
{
    testParse("SELECT * FROM users WHERE code ILIKE 'A_C'", true);
}

// ===== Regex Operator Tests (SUPPORTED) =====

TEST_F(TextSearchTest, RegexMatch_CaseSensitive)
{
    testParse("SELECT * FROM logs WHERE message ~ 'ERROR: \\d+'", true);
}

TEST_F(TextSearchTest, RegexMatch_CaseInsensitive)
{
    testParse("SELECT * FROM emails WHERE address ~* '[a-z0-9._%+-]+@[a-z0-9.-]+\\.[a-z]{2,}'", true);
}

TEST_F(TextSearchTest, RegexNotMatch_CaseSensitive)
{
    testParse("SELECT * FROM users WHERE username !~ '[^a-zA-Z0-9_]'", true);
}

TEST_F(TextSearchTest, RegexNotMatch_CaseInsensitive)
{
    testParse("SELECT * FROM data WHERE content !~* 'forbidden'", true);
}

// ===== REGEXP_MATCHES Tests (SUPPORTED) =====

TEST_F(TextSearchTest, REGEXP_MATCHES_Basic)
{
    testParse("SELECT REGEXP_MATCHES('foo123bar456', '\\d+', 'g') FROM users", true);
}

TEST_F(TextSearchTest, REGEXP_MATCHES_TwoArgs)
{
    testParse("SELECT REGEXP_MATCHES(text_col, '[A-Z]+') FROM table1", true);
}

TEST_F(TextSearchTest, REGEXP_MATCHES_CaseInsensitive)
{
    testParse("SELECT REGEXP_MATCHES(data, 'hello', 'i') FROM logs", true);
}

// ===== REGEXP_REPLACE Tests =====

TEST_F(TextSearchTest, REGEXP_REPLACE_Basic)
{
    testParse("SELECT REGEXP_REPLACE('Hello World', 'World', 'Universe') FROM table1", true);
}

// ENABLED: Parser now supports 4-argument REGEXP_REPLACE
TEST_F(TextSearchTest, REGEXP_REPLACE_Global)
{
    testParse("SELECT REGEXP_REPLACE('abc123def456', '\\d+', 'X', 'g') FROM table1", true);
}

// ENABLED: Parser now supports 4-argument REGEXP_REPLACE with unreserved keyword column names
TEST_F(TextSearchTest, REGEXP_REPLACE_CaseInsensitive)
{
    testParse("SELECT REGEXP_REPLACE(text, 'ERROR', 'Warning', 'i') FROM logs", true);
}

// ===== REGEXP_SPLIT Tests =====

TEST_F(TextSearchTest, REGEXP_SPLIT_TO_ARRAY)
{
    testParse("SELECT REGEXP_SPLIT_TO_ARRAY('one,two,three', ',') FROM table1", true);
}

// ENABLED: Parser now supports table-valued functions in FROM clause
TEST_F(TextSearchTest, REGEXP_SPLIT_TO_TABLE)
{
    testParse("SELECT * FROM REGEXP_SPLIT_TO_TABLE('foo,bar,baz', ',')", true);
}

// ENABLED: Parser now supports table-valued functions in FROM clause
TEST_F(TextSearchTest, REGEXP_SPLIT_TO_TABLE_WithFlags)
{
    testParse("SELECT * FROM REGEXP_SPLIT_TO_TABLE('OneTwoThree', '[A-Z]', 'g')", true);
}

// ===== String Tokenization Tests =====

TEST_F(TextSearchTest, SPLIT_PART_Basic)
{
    testParse("SELECT SPLIT_PART('a,b,c,d', ',', 2) FROM table1", true);
}

TEST_F(TextSearchTest, SPLIT_PART_LastField)
{
    testParse("SELECT SPLIT_PART(path, '/', 4) FROM files", true);
}

// ENABLED: Parser now supports table-valued functions in FROM clause
TEST_F(TextSearchTest, STRING_TO_TABLE_Basic)
{
    testParse("SELECT * FROM STRING_TO_TABLE('foo,bar,baz', ',')", true);
}

// ENABLED: Parser now supports table-valued functions in FROM clause
TEST_F(TextSearchTest, STRING_TO_TABLE_InWHERE)
{
    testParse("SELECT word FROM STRING_TO_TABLE('one two three', ' ') WHERE LENGTH(word) > 2", true);
}

// ENABLED: Parser now supports table-valued functions in FROM clause
TEST_F(TextSearchTest, UNNEST_TEXT_Basic)
{
    testParse("SELECT * FROM UNNEST_TEXT(ARRAY['a', 'b', 'c'])", true);
}

// ===== Text Utility Tests =====

TEST_F(TextSearchTest, STRPOS_Basic)
{
    testParse("SELECT STRPOS('hello world', 'world') FROM table1", true);
}

TEST_F(TextSearchTest, STRPOS_NotFound)
{
    testParse("SELECT STRPOS(text_col, 'missing') FROM table1", true);
}

// ENABLED: Parser now supports POSITION...IN syntax
TEST_F(TextSearchTest, POSITION_Basic)
{
    testParse("SELECT POSITION('world' IN 'hello world') FROM table1", true);
}

// ENABLED: Parser now supports POSITION...IN syntax
TEST_F(TextSearchTest, POSITION_WithColumn)
{
    testParse("SELECT POSITION('needle' IN haystack) FROM searches", true);
}

// ENABLED: Parser now supports OVERLAY...PLACING...FROM syntax
TEST_F(TextSearchTest, OVERLAY_Basic)
{
    testParse("SELECT OVERLAY('Txxxxas' PLACING 'hom' FROM 2 FOR 4) FROM table1", true);
}

// ENABLED: Parser now supports OVERLAY...PLACING...FROM syntax
TEST_F(TextSearchTest, OVERLAY_WithoutLength)
{
    testParse("SELECT OVERLAY(text PLACING 'NEW' FROM 5) FROM table1", true);
}

// ENABLED: Parser now supports SQL-style escaped quotes ('')
TEST_F(TextSearchTest, QUOTE_LITERAL_Basic)
{
    testParse("SELECT QUOTE_LITERAL('O''Reilly') FROM table1", true);
}

// ENABLED: Parser now supports QUOTE_IDENT function
TEST_F(TextSearchTest, QUOTE_IDENT_Basic)
{
    testParse("SELECT QUOTE_IDENT('table name') FROM table1", true);
}

// ===== Case Conversion Tests =====

TEST_F(TextSearchTest, INITCAP_Basic)
{
    testParse("SELECT INITCAP('hello world') FROM table1", true);
}

TEST_F(TextSearchTest, INITCAP_MultiWord)
{
    testParse("SELECT INITCAP(name) FROM users", true);
}

TEST_F(TextSearchTest, ASCII_Basic)
{
    testParse("SELECT ASCII('A') FROM table1", true);
}

TEST_F(TextSearchTest, ASCII_Column)
{
    testParse("SELECT ASCII(first_char) FROM data", true);
}

TEST_F(TextSearchTest, CHR_Basic)
{
    testParse("SELECT CHR(65) FROM table1", true);
}

TEST_F(TextSearchTest, CHR_Expression)
{
    testParse("SELECT CHR(64 + position) FROM data", true);
}

TEST_F(TextSearchTest, REPEAT_Basic)
{
    testParse("SELECT REPEAT('*', 5) FROM table1", true);
}

TEST_F(TextSearchTest, REPEAT_Column)
{
    testParse("SELECT REPEAT(char_val, count_val) FROM patterns", true);
}

// ENABLED: Parser now supports REVERSE function
TEST_F(TextSearchTest, REVERSE_Basic)
{
    testParse("SELECT REVERSE('hello') FROM table1", true);
}

// ENABLED: Parser now supports unreserved keywords (like 'text') as identifiers
TEST_F(TextSearchTest, REVERSE_Column)
{
    testParse("SELECT REVERSE(text) FROM table1", true);
}

// ===== Complex Query Tests =====

// ENABLED: Parser now supports 4-arg REGEXP_REPLACE and unreserved keywords as identifiers
TEST_F(TextSearchTest, MultipleRegexOperations)
{
    testParse("SELECT REGEXP_REPLACE(INITCAP(text), '\\s+', ' ', 'g') FROM table1", true);
}

// ENABLED: Parser now supports regex operators in compound WHERE clauses
TEST_F(TextSearchTest, RegexInWHERE)
{
    testParse("SELECT * FROM logs WHERE message ~ 'ERROR' AND severity !~ 'DEBUG'", true);
}

// ENABLED: Parser now supports ILIKE in compound WHERE clauses
TEST_F(TextSearchTest, ILIKEWithOR)
{
    testParse("SELECT * FROM users WHERE email ILIKE '%@gmail.com' OR email ILIKE '%@yahoo.com'", true);
}

TEST_F(TextSearchTest, STRPOSinWHERE)
{
    testParse("SELECT * FROM documents WHERE STRPOS(content, 'keyword') > 0", true);
}

// ENABLED: Parser now supports chained functions with unreserved keywords as identifiers
TEST_F(TextSearchTest, ChainedStringFunctions)
{
    testParse("SELECT REVERSE(UPPER(TRIM(text))) FROM data", true);
}

TEST_F(TextSearchTest, REPEATwithASCII)
{
    testParse("SELECT REPEAT(CHR(65), 10) FROM table1", true);
}

// ENABLED: Parser now supports OVERLAY syntax and 4-arg REGEXP_REPLACE
TEST_F(TextSearchTest, OVERLAYwithREGEXP_REPLACE)
{
    testParse("SELECT OVERLAY(REGEXP_REPLACE(text, '\\d', 'X', 'g') PLACING 'NEW' FROM 1 FOR 3) FROM data", true);
}

// ===== NULL Handling Tests =====

TEST_F(TextSearchTest, ILIKE_NullValue)
{
    testParse("SELECT * FROM users WHERE name ILIKE NULL", true);
}

TEST_F(TextSearchTest, REGEXP_MATCHES_NullInput)
{
    testParse("SELECT REGEXP_MATCHES(NULL, 'pattern') FROM table1", true);
}

TEST_F(TextSearchTest, SPLIT_PART_NullDelimiter)
{
    testParse("SELECT SPLIT_PART('a,b,c', NULL, 1) FROM table1", true);
}

TEST_F(TextSearchTest, INITCAP_NullString)
{
    testParse("SELECT INITCAP(NULL) FROM table1", true);
}

TEST_F(TextSearchTest, STRPOS_NullValues)
{
    testParse("SELECT STRPOS(NULL, 'test'), STRPOS('test', NULL) FROM table1", true);
}

// ===== Unicode Handling Tests =====

TEST_F(TextSearchTest, ILIKE_Unicode)
{
    testParse("SELECT * FROM users WHERE name ILIKE '%café%'", true);
}

TEST_F(TextSearchTest, REGEXP_MATCHES_Unicode)
{
    testParse("SELECT REGEXP_MATCHES('héllo wörld', '[\\p{L}]+', 'g') FROM table1", true);
}

TEST_F(TextSearchTest, INITCAP_Unicode)
{
    testParse("SELECT INITCAP('josé garcía') FROM table1", true);
}

// ENABLED: Parser now supports REVERSE function with Unicode
TEST_F(TextSearchTest, REVERSE_Unicode)
{
    testParse("SELECT REVERSE('hello 世界') FROM table1", true);
}

// ===== Error Tests =====

TEST_F(TextSearchTest, SPLIT_PART_InvalidFieldNumber)
{
    testParse("SELECT SPLIT_PART('a,b,c', ',', 0) FROM table1", true); // Should parse, runtime error
}

TEST_F(TextSearchTest, CHR_InvalidCode)
{
    testParse("SELECT CHR(-1) FROM table1", true); // Should parse, runtime error
}

// ENABLED: Parser now supports OVERLAY syntax (this is a runtime error test)
TEST_F(TextSearchTest, OVERLAY_InvalidPosition)
{
    testParse("SELECT OVERLAY('text' PLACING 'new' FROM 0) FROM table1", true); // Should parse, runtime error
}

// ===== Aggregate and GROUP BY Tests =====

// ENABLED: Parser now supports regex operators in aggregation queries
TEST_F(TextSearchTest, REGEXinAggregation)
{
    testParse("SELECT COUNT(*) FROM logs WHERE message ~ 'ERROR' GROUP BY DATE(timestamp)", true);
}

TEST_F(TextSearchTest, ILIKEinHAVING)
{
    testParse("SELECT category, COUNT(*) FROM products GROUP BY category HAVING category ILIKE 'electronic%'", true);
}

TEST_F(TextSearchTest, StringFunctionsInGROUPBY)
{
    testParse("SELECT INITCAP(name), COUNT(*) FROM users GROUP BY INITCAP(name)", true);
}

// ===== Subquery Tests =====

TEST_F(TextSearchTest, REGEXinSubquery)
{
    testParse("SELECT * FROM users WHERE id IN (SELECT user_id FROM logs WHERE message ~ 'login')", true);
}

// ENABLED: Parser now supports derived table subqueries
TEST_F(TextSearchTest, SPLIT_PARTinSubquery)
{
    testParse("SELECT * FROM (SELECT SPLIT_PART(name, ' ', 1) as first_name FROM users) WHERE LENGTH(first_name) > 5", true);
}
