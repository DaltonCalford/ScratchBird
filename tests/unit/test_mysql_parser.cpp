/**
 * MySQL Parser Unit Tests
 *
 * Tests the MySQL parser's ability to parse MySQL 8.0 SQL syntax
 * and generate correct SBLR bytecode.
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/mysql/mysql_parser.h"
#include "scratchbird/parser/mysql/mysql_lexer.h"

using namespace scratchbird::parser::mysql;

// ============================================================================
// Lexer Tests
// ============================================================================

class MySQLLexerTest : public ::testing::Test {
protected:
    void expectToken(Lexer& lexer, TokenType expected, const std::string& context = "") {
        Token t = lexer.nextToken();
        EXPECT_EQ(t.type, expected) << context;
    }

    void expectKeyword(Lexer& lexer, TokenType expected, const std::string& context = "") {
        Token t = lexer.nextToken();
        EXPECT_EQ(t.type, expected) << context;
    }
};

TEST_F(MySQLLexerTest, BasicKeywords) {
    Lexer lexer("SELECT FROM WHERE");

    expectKeyword(lexer, TokenType::KW_SELECT, "SELECT");
    expectKeyword(lexer, TokenType::KW_FROM, "FROM");
    expectKeyword(lexer, TokenType::KW_WHERE, "WHERE");
    expectToken(lexer, TokenType::END_OF_FILE, "EOF");
}

TEST_F(MySQLLexerTest, Identifiers) {
    Lexer lexer("table_name column123 _private $dollar");

    Token t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "table_name");

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "column123");

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "_private");

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "$dollar");
}

TEST_F(MySQLLexerTest, BacktickIdentifiers) {
    Lexer lexer("`table name` `SELECT` `column-with-dash`");

    Token t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::BACKTICK_IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "table name");

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::BACKTICK_IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "SELECT");  // Reserved word as identifier

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::BACKTICK_IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "column-with-dash");
}

TEST_F(MySQLLexerTest, IntegerLiterals) {
    Lexer lexer("42 0 123456789");

    Token t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(t.value.int_value, 42);

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(t.value.int_value, 0);

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(t.value.int_value, 123456789);
}

TEST_F(MySQLLexerTest, FloatLiterals) {
    Lexer lexer("3.14 0.5 1e10 2.5e-3");

    Token t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::FLOAT_LITERAL);
    EXPECT_NEAR(t.value.float_value, 3.14, 0.001);

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::FLOAT_LITERAL);
    EXPECT_NEAR(t.value.float_value, 0.5, 0.001);

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::FLOAT_LITERAL);
    EXPECT_NEAR(t.value.float_value, 1e10, 1e6);

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::FLOAT_LITERAL);
    EXPECT_NEAR(t.value.float_value, 2.5e-3, 0.0001);
}

TEST_F(MySQLLexerTest, StringLiterals) {
    Lexer lexer("'hello' \"world\" 'it\\'s'");

    Token t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "hello");

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "world");

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::STRING_LITERAL);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "it's");
}

TEST_F(MySQLLexerTest, HexLiterals) {
    Lexer lexer("0x1A 0xFF X'AB'");

    Token t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::HEX_LITERAL);
    EXPECT_EQ(t.value.int_value, 0x1A);

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::HEX_LITERAL);
    EXPECT_EQ(t.value.int_value, 0xFF);

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::HEX_LITERAL);
    EXPECT_EQ(t.value.int_value, 0xAB);
}

TEST_F(MySQLLexerTest, BitLiterals) {
    Lexer lexer("b'1010' B'1111'");

    Token t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::BIT_LITERAL);
    EXPECT_EQ(t.value.int_value, 10);  // 1010 binary = 10 decimal

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::BIT_LITERAL);
    EXPECT_EQ(t.value.int_value, 15);  // 1111 binary = 15 decimal
}

TEST_F(MySQLLexerTest, UserVariables) {
    Lexer lexer("@myvar @`quoted var`");

    Token t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::USER_VARIABLE);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "myvar");

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::USER_VARIABLE);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "quoted var");
}

TEST_F(MySQLLexerTest, SystemVariables) {
    Lexer lexer("@@version @@global.sql_mode");

    Token t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::SYSTEM_VARIABLE);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "version");

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::SYSTEM_VARIABLE);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "global.sql_mode");
}

TEST_F(MySQLLexerTest, Operators) {
    Lexer lexer("+ - * / % = <> != < > <= >= <=> -> ->>");

    expectToken(lexer, TokenType::PLUS);
    expectToken(lexer, TokenType::MINUS);
    expectToken(lexer, TokenType::STAR);
    expectToken(lexer, TokenType::SLASH);
    expectToken(lexer, TokenType::PERCENT);
    expectToken(lexer, TokenType::EQUAL);
    expectToken(lexer, TokenType::NOT_EQUAL);
    expectToken(lexer, TokenType::NOT_EQUAL);
    expectToken(lexer, TokenType::LESS_THAN);
    expectToken(lexer, TokenType::GREATER_THAN);
    expectToken(lexer, TokenType::LESS_EQUAL);
    expectToken(lexer, TokenType::GREATER_EQUAL);
    expectToken(lexer, TokenType::NULL_SAFE_EQUAL);
    expectToken(lexer, TokenType::ARROW);
    expectToken(lexer, TokenType::DOUBLE_ARROW);
}

TEST_F(MySQLLexerTest, Punctuation) {
    Lexer lexer("() [] {} , ; .");

    expectToken(lexer, TokenType::LEFT_PAREN);
    expectToken(lexer, TokenType::RIGHT_PAREN);
    expectToken(lexer, TokenType::LEFT_BRACKET);
    expectToken(lexer, TokenType::RIGHT_BRACKET);
    expectToken(lexer, TokenType::LEFT_BRACE);
    expectToken(lexer, TokenType::RIGHT_BRACE);
    expectToken(lexer, TokenType::COMMA);
    expectToken(lexer, TokenType::SEMICOLON);
    expectToken(lexer, TokenType::DOT);
}

TEST_F(MySQLLexerTest, Comments) {
    Lexer lexer("SELECT -- line comment\n"
                "FROM /* block\n"
                "comment */ WHERE # another comment\n"
                "TRUE");

    expectKeyword(lexer, TokenType::KW_SELECT);
    expectKeyword(lexer, TokenType::KW_FROM);
    expectKeyword(lexer, TokenType::KW_WHERE);
    expectKeyword(lexer, TokenType::KW_TRUE);
}

// ============================================================================
// Parser Tests
// ============================================================================

class MySQLParserTest : public ::testing::Test {
protected:
    void expectSuccess(const std::string& sql) {
        Parser parser(sql);
        auto result = parser.parseStatement();
        EXPECT_TRUE(result.success()) << "Failed to parse: " << sql;
        if (!result.success()) {
            for (const auto& err : result.errors()) {
                std::cerr << "Error: " << err.message << std::endl;
            }
        }
    }

    void expectError(const std::string& sql) {
        Parser parser(sql);
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success()) << "Expected error for: " << sql;
    }
};

TEST_F(MySQLParserTest, SimpleSelect) {
    expectSuccess("SELECT 1");
    expectSuccess("SELECT 1, 2, 3");
    expectSuccess("SELECT 'hello'");
    expectSuccess("SELECT 1 + 2");
    expectSuccess("SELECT *");
}

TEST_F(MySQLParserTest, SelectWithFrom) {
    expectSuccess("SELECT * FROM users");
    expectSuccess("SELECT id, name FROM users");
    expectSuccess("SELECT u.id, u.name FROM users u");
    expectSuccess("SELECT u.id, u.name FROM users AS u");
}

TEST_F(MySQLParserTest, SelectWithWhere) {
    expectSuccess("SELECT * FROM users WHERE id = 1");
    expectSuccess("SELECT * FROM users WHERE name = 'John'");
    expectSuccess("SELECT * FROM users WHERE id > 10 AND active = TRUE");
    expectSuccess("SELECT * FROM users WHERE id IN (1, 2, 3)");
    expectSuccess("SELECT * FROM users WHERE name LIKE '%test%'");
}

TEST_F(MySQLParserTest, SelectWithJoin) {
    expectSuccess("SELECT * FROM users JOIN orders ON users.id = orders.user_id");
    expectSuccess("SELECT * FROM users LEFT JOIN orders ON users.id = orders.user_id");
    expectSuccess("SELECT * FROM users RIGHT JOIN orders ON users.id = orders.user_id");
    expectSuccess("SELECT * FROM users INNER JOIN orders ON users.id = orders.user_id");
    expectSuccess("SELECT * FROM users CROSS JOIN orders");
}

TEST_F(MySQLParserTest, SelectWithGroupBy) {
    expectSuccess("SELECT department, COUNT(*) FROM employees GROUP BY department");
    expectSuccess("SELECT department, SUM(salary) FROM employees GROUP BY department HAVING SUM(salary) > 100000");
}

TEST_F(MySQLParserTest, SelectWithOrderBy) {
    expectSuccess("SELECT * FROM users ORDER BY name");
    expectSuccess("SELECT * FROM users ORDER BY name ASC");
    expectSuccess("SELECT * FROM users ORDER BY name DESC");
    expectSuccess("SELECT * FROM users ORDER BY name ASC, id DESC");
}

TEST_F(MySQLParserTest, SelectWithLimit) {
    expectSuccess("SELECT * FROM users LIMIT 10");
    expectSuccess("SELECT * FROM users LIMIT 10 OFFSET 20");
    expectSuccess("SELECT * FROM users LIMIT 20, 10");
}

TEST_F(MySQLParserTest, SelectDistinct) {
    expectSuccess("SELECT DISTINCT name FROM users");
    expectSuccess("SELECT DISTINCTROW name FROM users");
    expectSuccess("SELECT ALL name FROM users");
}

TEST_F(MySQLParserTest, SelectWithFunctions) {
    expectSuccess("SELECT COUNT(*) FROM users");
    expectSuccess("SELECT SUM(amount) FROM orders");
    expectSuccess("SELECT AVG(price) FROM products");
    expectSuccess("SELECT MIN(created_at), MAX(created_at) FROM events");
    expectSuccess("SELECT LENGTH('hello')");
    expectSuccess("SELECT UPPER(name) FROM users");
    expectSuccess("SELECT NOW()");
    expectSuccess("SELECT COALESCE(name, 'Unknown') FROM users");
}

TEST_F(MySQLParserTest, SelectWithCase) {
    expectSuccess("SELECT CASE WHEN status = 1 THEN 'Active' ELSE 'Inactive' END FROM users");
    expectSuccess("SELECT CASE status WHEN 1 THEN 'Active' WHEN 2 THEN 'Pending' ELSE 'Unknown' END FROM users");
}

TEST_F(MySQLParserTest, SelectWithSubquery) {
    expectSuccess("SELECT * FROM users WHERE id IN (SELECT user_id FROM orders)");
    expectSuccess("SELECT * FROM users WHERE EXISTS (SELECT 1 FROM orders WHERE orders.user_id = users.id)");
    expectSuccess("SELECT (SELECT COUNT(*) FROM orders) AS order_count");
}

TEST_F(MySQLParserTest, InsertBasic) {
    expectSuccess("INSERT INTO users (name, email) VALUES ('John', 'john@example.com')");
    expectSuccess("INSERT INTO users VALUES (1, 'John', 'john@example.com')");
    expectSuccess("INSERT INTO users (name) VALUES ('John'), ('Jane'), ('Bob')");
}

TEST_F(MySQLParserTest, InsertWithOnDuplicate) {
    expectSuccess("INSERT INTO users (id, name) VALUES (1, 'John') ON DUPLICATE KEY UPDATE name = 'John Updated'");
}

TEST_F(MySQLParserTest, UpdateBasic) {
    expectSuccess("UPDATE users SET name = 'John'");
    expectSuccess("UPDATE users SET name = 'John' WHERE id = 1");
    expectSuccess("UPDATE users SET name = 'John', email = 'john@test.com' WHERE id = 1");
}

TEST_F(MySQLParserTest, DeleteBasic) {
    expectSuccess("DELETE FROM users");
    expectSuccess("DELETE FROM users WHERE id = 1");
    expectSuccess("DELETE FROM users WHERE status = 'inactive' LIMIT 100");
}

TEST_F(MySQLParserTest, CreateTableBasic) {
    expectSuccess("CREATE TABLE users (id INT, name VARCHAR(100))");
    expectSuccess("CREATE TABLE users (id INT PRIMARY KEY, name VARCHAR(100) NOT NULL)");
    expectSuccess("CREATE TABLE users (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(100) DEFAULT '')");
}

TEST_F(MySQLParserTest, CreateTableIfNotExists) {
    expectSuccess("CREATE TABLE IF NOT EXISTS users (id INT)");
}

TEST_F(MySQLParserTest, CreateTableWithTypes) {
    expectSuccess("CREATE TABLE test (a TINYINT, b SMALLINT, c INT, d BIGINT)");
    expectSuccess("CREATE TABLE test (a FLOAT, b DOUBLE, c DECIMAL(10,2))");
    expectSuccess("CREATE TABLE test (a CHAR(10), b VARCHAR(255), c TEXT)");
    expectSuccess("CREATE TABLE test (a DATE, b TIME, c DATETIME, d TIMESTAMP)");
    expectSuccess("CREATE TABLE test (a BLOB, b BINARY(16), c VARBINARY(256))");
    expectSuccess("CREATE TABLE test (a JSON, b BOOL)");
}

TEST_F(MySQLParserTest, TransactionStatements) {
    expectSuccess("BEGIN");
    expectSuccess("BEGIN WORK");
    expectSuccess("START TRANSACTION");
    expectSuccess("COMMIT");
    expectSuccess("ROLLBACK");
    expectSuccess("SAVEPOINT my_savepoint");
    expectSuccess("ROLLBACK TO SAVEPOINT my_savepoint");
    expectSuccess("RELEASE SAVEPOINT my_savepoint");
}

TEST_F(MySQLParserTest, SetStatements) {
    expectSuccess("SET @var = 1");
    expectSuccess("SET @name = 'test'");
}

TEST_F(MySQLParserTest, ShowStatements) {
    expectSuccess("SHOW TABLES");
    expectSuccess("SHOW DATABASES");
    expectSuccess("SHOW COLUMNS FROM users");
    expectSuccess("SHOW INDEX FROM users");
    expectSuccess("SHOW CREATE TABLE users");
}

TEST_F(MySQLParserTest, UseStatement) {
    expectSuccess("USE mydb");
}

TEST_F(MySQLParserTest, DescribeStatement) {
    expectSuccess("DESCRIBE users");
}

TEST_F(MySQLParserTest, Expressions) {
    expectSuccess("SELECT 1 + 2 * 3");
    expectSuccess("SELECT (1 + 2) * 3");
    expectSuccess("SELECT -5");
    expectSuccess("SELECT 1 AND 2 OR 3");
    expectSuccess("SELECT NOT TRUE");
    expectSuccess("SELECT 5 BETWEEN 1 AND 10");
    expectSuccess("SELECT 1 IS NULL");
    expectSuccess("SELECT 1 IS NOT NULL");
}

TEST_F(MySQLParserTest, MySQLSpecificOperators) {
    expectSuccess("SELECT 1 <=> NULL");  // NULL-safe equal
    expectSuccess("SELECT 10 DIV 3");    // Integer division
    expectSuccess("SELECT 10 MOD 3");    // Modulo
    expectSuccess("SELECT 5 << 2");      // Left shift
    expectSuccess("SELECT 20 >> 2");     // Right shift
    expectSuccess("SELECT 5 & 3");       // Bitwise AND
    expectSuccess("SELECT 5 | 3");       // Bitwise OR
    expectSuccess("SELECT 5 ^ 3");       // Bitwise XOR
    expectSuccess("SELECT ~5");          // Bitwise NOT
}

TEST_F(MySQLParserTest, JSONOperators) {
    expectSuccess("SELECT JSON_EXTRACT(data, '$.name') FROM documents");
}

TEST_F(MySQLParserTest, Replace) {
    expectSuccess("REPLACE INTO users (id, name) VALUES (1, 'John')");
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(MySQLParserTest, MissingKeyword) {
    expectError("SELECT FROM users");       // Missing column list or *
    expectError("INSERT users VALUES (1)"); // Missing INTO
}

TEST_F(MySQLParserTest, UnterminatedString) {
    Lexer lexer("SELECT 'unterminated");
    Token t = lexer.nextToken();  // SELECT
    t = lexer.nextToken();        // Should be error
    EXPECT_EQ(t.type, TokenType::ERROR);
}

TEST_F(MySQLParserTest, InvalidSyntax) {
    expectError("SELEC * FROM users");  // Typo in SELECT
    expectError("SELECT * FORM users"); // Typo in FROM
}
