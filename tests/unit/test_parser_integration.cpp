#include <gtest/gtest.h>
#include "scratchbird/parser/parser.h"
#include <vector>
#include <string>

using namespace scratchbird::parser;

class ParserIntegrationTest : public ::testing::Test
{
protected:
    void testSQL(const std::string &sql, const std::string &expected_output)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "Parse failed for: " << sql;

        std::stringstream ss;
        ASTPrinter printer(ss, parser.stringPool());
        result.statement()->accept(&printer);

        EXPECT_EQ(ss.str(), expected_output);
    }
};

TEST_F(ParserIntegrationTest, RealWorldCreateTable)
{
    std::string sql = R"(
        CREATE TABLE employees (
            id INTEGER NOT NULL,
            first_name VARCHAR(50) NOT NULL,
            last_name VARCHAR(50) NOT NULL,
            salary DOUBLE,
            department_id INTEGER
        )
    )";

    std::string expected = "CREATE TABLE employees (\n"
                           "  id INTEGER NOT NULL,\n"
                           "  first_name VARCHAR(50) NOT NULL,\n"
                           "  last_name VARCHAR(50) NOT NULL,\n"
                           "  salary DOUBLE,\n"
                           "  department_id INTEGER\n"
                           ")";

    testSQL(sql, expected);
}

TEST_F(ParserIntegrationTest, RealWorldInsert)
{
    std::string sql = "INSERT INTO employees (id, first_name, last_name, salary) "
                      "VALUES (1001, 'John', 'Doe', 75000.50)";

    std::string expected = "INSERT INTO employees (id, first_name, last_name, salary) "
                           "VALUES (1001, 'John', 'Doe', 75000.500000)";

    testSQL(sql, expected);
}

TEST_F(ParserIntegrationTest, RealWorldSelect)
{
    std::string sql = "SELECT id, first_name, last_name FROM employees WHERE salary > 50000";

    std::string expected = "SELECT id, first_name, last_name FROM employees "
                           "WHERE (salary > 50000)";

    testSQL(sql, expected);
}

TEST_F(ParserIntegrationTest, ComplexWhereClause)
{
    std::string sql = "SELECT * FROM products WHERE price >= 10.99 + 5 * 2";

    std::string expected = "SELECT * FROM products WHERE (price >= (10.990000 + (5 * 2)))";

    testSQL(sql, expected);
}

TEST_F(ParserIntegrationTest, FullSQLScript)
{
    // Test parsing multiple statements in sequence
    std::vector<std::string> statements = {
        "CREATE TABLE departments (id INTEGER NOT NULL, name VARCHAR(100))",
        "INSERT INTO departments (id, name) VALUES (1, 'Engineering')",
        "INSERT INTO departments (id, name) VALUES (2, 'Sales')",
        "SELECT * FROM departments WHERE id = 1"};

    for (const auto &sql : statements)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        EXPECT_TRUE(result.success()) << "Failed to parse: " << sql;
    }
}

TEST_F(ParserIntegrationTest, ErrorMessages)
{
    struct TestCase
    {
        std::string sql;
        std::string expected_error;
    };

    std::vector<TestCase> cases = {
        {"CREATE TABLE", "Expected TABLE after CREATE"},
        {"INSERT INTO t1 (a, b) VALUES (1)", "Column count doesn't match value count"},
        {"SELECT * FROM", "Expected table name"},
        {"SELECT * FROM t WHERE", "Expected expression"}};

    for (const auto &tc : cases)
    {
        Lexer lexer(tc.sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success()) << "Expected error for: " << tc.sql;

        if (!result.errors().empty())
        {
            EXPECT_EQ(result.errors()[0].message, tc.expected_error) << "For SQL: " << tc.sql;
        }
    }
}