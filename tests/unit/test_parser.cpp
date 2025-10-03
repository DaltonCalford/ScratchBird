#include <gtest/gtest.h>
#include "scratchbird/parser/parser.h"
#include <sstream>

using namespace scratchbird::parser;

class ParserTest : public ::testing::Test
{
protected:
    std::string parseAndPrint(const std::string &sql)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();

        if (!result.success())
        {
            std::stringstream ss;
            ss << "Parse errors:\n";
            for (const auto &err : result.errors())
            {
                ss << "  Line " << err.location.line << ":" << err.location.column << " - "
                   << err.message << "\n";
            }
            return ss.str();
        }

        std::stringstream ss;
        ASTPrinter printer(ss, parser.stringPool());
        result.statement()->accept(&printer);
        return ss.str();
    }

    void expectParseSuccess(const std::string &sql)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();

        EXPECT_TRUE(result.success()) << "Failed to parse: " << sql;
        if (!result.success())
        {
            for (const auto &err : result.errors())
            {
                std::cerr << "Error at " << err.location.line << ":" << err.location.column << " - "
                          << err.message << std::endl;
            }
        }
    }

    void expectParseError(const std::string &sql)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success()) << "Expected parse error for: " << sql;
    }
};

// ===== CREATE TABLE Tests =====

TEST_F(ParserTest, CreateTableBasic)
{
    std::string sql = "CREATE TABLE users (id INTEGER NOT NULL, name VARCHAR(50))";
    std::string expected = "CREATE TABLE users (\n  id INTEGER NOT NULL,\n  name VARCHAR(50)\n)";

    EXPECT_EQ(parseAndPrint(sql), expected);
}

TEST_F(ParserTest, CreateTableAllTypes)
{
    std::string sql = "CREATE TABLE test ("
                      "a INTEGER, "
                      "b BIGINT NOT NULL, "
                      "c DOUBLE, "
                      "d VARCHAR(255) NOT NULL"
                      ")";

    expectParseSuccess(sql);
}

TEST_F(ParserTest, CreateTableSingleColumn)
{
    std::string sql = "CREATE TABLE t1 (id INTEGER)";
    expectParseSuccess(sql);
}

TEST_F(ParserTest, CreateTableErrors)
{
    expectParseError("CREATE TABLE");                  // Missing table name
    expectParseError("CREATE TABLE t1");               // Missing column list
    expectParseError("CREATE TABLE t1 ()");            // Empty column list
    expectParseError("CREATE TABLE t1 (id)");          // Missing type
    expectParseError("CREATE TABLE t1 (id INTEGER,)"); // Trailing comma
}

// ===== INSERT Tests =====

TEST_F(ParserTest, InsertBasic)
{
    std::string sql = "INSERT INTO users (id, name) VALUES (1, 'John')";
    std::string expected = "INSERT INTO users (id, name) VALUES (1, 'John')";

    EXPECT_EQ(parseAndPrint(sql), expected);
}

TEST_F(ParserTest, InsertNullValue)
{
    std::string sql = "INSERT INTO users (id, name) VALUES (1, NULL)";
    expectParseSuccess(sql);
}

TEST_F(ParserTest, InsertExpressions)
{
    std::string sql = "INSERT INTO calc (a, b, c) VALUES (1 + 2, 3 * 4, 5)";
    std::string expected = "INSERT INTO calc (a, b, c) VALUES ((1 + 2), (3 * 4), 5)";

    EXPECT_EQ(parseAndPrint(sql), expected);
}

TEST_F(ParserTest, InsertErrors)
{
    expectParseError("INSERT INTO");                      // Missing table
    expectParseError("INSERT INTO t1");                   // Missing column list
    expectParseError("INSERT INTO t1 ()");                // Empty column list
    expectParseError("INSERT INTO t1 (a) VALUES");        // Missing values
    expectParseError("INSERT INTO t1 (a) VALUES ()");     // Empty values
    expectParseError("INSERT INTO t1 (a, b) VALUES (1)"); // Mismatched count
}

// ===== SELECT Tests =====

TEST_F(ParserTest, SelectStar)
{
    std::string sql = "SELECT * FROM users";
    std::string expected = "SELECT * FROM users";

    EXPECT_EQ(parseAndPrint(sql), expected);
}

TEST_F(ParserTest, SelectColumns)
{
    std::string sql = "SELECT id, name FROM users";
    std::string expected = "SELECT id, name FROM users";

    EXPECT_EQ(parseAndPrint(sql), expected);
}

TEST_F(ParserTest, SelectWithWhere)
{
    std::string sql = "SELECT * FROM users WHERE id = 1";
    std::string expected = "SELECT * FROM users WHERE (id = 1)";

    EXPECT_EQ(parseAndPrint(sql), expected);
}

TEST_F(ParserTest, SelectComplexWhere)
{
    std::string sql = "SELECT id, name FROM users WHERE id > 10 + 5";
    std::string expected = "SELECT id, name FROM users WHERE (id > (10 + 5))";

    EXPECT_EQ(parseAndPrint(sql), expected);
}

TEST_F(ParserTest, SelectErrors)
{
    expectParseError("SELECT");                    // Missing select list
    expectParseError("SELECT *");                  // Missing FROM
    expectParseError("SELECT * FROM");             // Missing table
    expectParseError("SELECT FROM users");         // Empty select list
    expectParseError("SELECT * FROM users WHERE"); // Missing condition
}

// ===== Expression Tests =====

TEST_F(ParserTest, ExpressionPrecedence)
{
    // Test that 1 + 2 * 3 is parsed as 1 + (2 * 3)
    std::string sql = "SELECT 1 + 2 * 3 FROM t";
    std::string expected = "SELECT (1 + (2 * 3)) FROM t";

    EXPECT_EQ(parseAndPrint(sql), expected);
}

TEST_F(ParserTest, ExpressionParentheses)
{
    std::string sql = "SELECT (1 + 2) * 3 FROM t";
    std::string expected = "SELECT ((1 + 2) * 3) FROM t";

    EXPECT_EQ(parseAndPrint(sql), expected);
}

TEST_F(ParserTest, ComparisonOperators)
{
    expectParseSuccess("SELECT * FROM t WHERE a = 1");
    expectParseSuccess("SELECT * FROM t WHERE a <> 1");
    expectParseSuccess("SELECT * FROM t WHERE a < 1");
    expectParseSuccess("SELECT * FROM t WHERE a > 1");
    expectParseSuccess("SELECT * FROM t WHERE a <= 1");
    expectParseSuccess("SELECT * FROM t WHERE a >= 1");
}

TEST_F(ParserTest, MixedExpressions)
{
    std::string sql = "SELECT a + b, c * d, e / 2 FROM t WHERE x > y + 1";
    expectParseSuccess(sql);
}

TEST_F(ParserTest, CastExpression)
{
    expectParseSuccess("SELECT CAST(123 AS INTEGER) FROM t");
    expectParseSuccess("SELECT CAST(123 AS BIGINT) FROM t");
    expectParseSuccess("SELECT CAST(123 AS DOUBLE) FROM t");
    expectParseSuccess("SELECT CAST(123 AS VARCHAR(10)) FROM t");
    expectParseSuccess("SELECT CAST(col AS INTEGER) FROM t");
    expectParseSuccess("SELECT CAST(a + b AS DOUBLE) FROM t");
}

// ===== Semicolon Tests =====

TEST_F(ParserTest, OptionalSemicolon)
{
    expectParseSuccess("SELECT * FROM t");
    expectParseSuccess("SELECT * FROM t;");
}

TEST_F(ParserTest, MultipleStatements)
{
    // For now, we only parse one statement at a time
    expectParseError("SELECT * FROM t; SELECT * FROM t2");
}

// ===== Error Recovery Tests =====

TEST_F(ParserTest, ErrorRecovery)
{
    Lexer lexer("CREATE TABLE t1 (id INTEGER); INVALID SYNTAX; SELECT * FROM t2");
    ASTArena arena;
    Parser parser(lexer, arena);

    // First statement should parse
    auto result1 = parser.parseStatement();
    EXPECT_TRUE(result1.success());

    // Second statement should fail
    auto result2 = parser.parseStatement();
    EXPECT_FALSE(result2.success());

    // Third statement should parse (after recovery)
    auto result3 = parser.parseStatement();
    EXPECT_TRUE(result3.success());
}