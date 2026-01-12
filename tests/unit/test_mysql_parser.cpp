/**
 * MySQL Parser Unit Tests
 *
 * Tests the MySQL parser's ability to parse MySQL 8.0 SQL syntax
 * and generate correct SBLR bytecode.
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/mysql/mysql_parser.h"
#include "scratchbird/parser/mysql/mysql_lexer.h"
#include "scratchbird/sblr/opcodes.h"

using namespace scratchbird::parser::mysql;
namespace sblr = scratchbird::sblr;

namespace {

bool readExtendedHeader(const std::vector<uint8_t>& bytecode,
                        sblr::ExtendedOpcode expected,
                        size_t* offset) {
    if (bytecode.size() < 5) {
        return false;
    }
    if (bytecode[0] != static_cast<uint8_t>(sblr::Opcode::VERSION)) {
        return false;
    }
    if (bytecode[1] != static_cast<uint8_t>(sblr::SBLR_VERSION)) {
        return false;
    }
    if (bytecode[2] != static_cast<uint8_t>(sblr::Opcode::EXTENDED_OPCODE)) {
        return false;
    }
    uint16_t opcode = sblr::readInt16(&bytecode[3]);
    if (opcode != static_cast<uint16_t>(expected)) {
        return false;
    }
    *offset = 5;
    return true;
}

bool hasExtendedOpcode(const std::vector<uint8_t>& bytecode,
                       sblr::ExtendedOpcode expected) {
    uint16_t target = static_cast<uint16_t>(expected);
    for (size_t i = 0; i + 2 < bytecode.size(); ++i) {
        if (bytecode[i] == static_cast<uint8_t>(sblr::Opcode::EXTENDED_OPCODE)) {
            uint16_t opcode = sblr::readInt16(&bytecode[i + 1]);
            if (opcode == target) {
                return true;
            }
            i += 2;
        }
    }
    return false;
}

std::vector<uint16_t> placeholderPositions(const std::vector<uint8_t>& bytecode) {
    std::vector<uint16_t> positions;
    uint16_t target = static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_PLACEHOLDER);
    for (size_t i = 0; i + 5 < bytecode.size(); ++i) {
        if (bytecode[i] == static_cast<uint8_t>(sblr::Opcode::EXTENDED_OPCODE)) {
            uint16_t opcode = sblr::readInt16(&bytecode[i + 1]);
            if (opcode == target) {
                positions.push_back(sblr::readInt16(&bytecode[i + 3]));
            }
        }
    }
    return positions;
}

}  // namespace

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
    expectSuccess("CREATE TABLE test (g GEOMETRY, p POINT, l LINESTRING, poly POLYGON)");
}

TEST_F(MySQLParserTest, CreateTableWithExpressionDefault) {
    expectSuccess("CREATE TABLE test (a INT DEFAULT (1 + 2), b TEXT DEFAULT 'x')");
}

TEST_F(MySQLParserTest, CreateTableWithGeneratedColumn) {
    expectSuccess("CREATE TABLE test (a INT, b INT GENERATED ALWAYS AS (a + 1) STORED)");
    expectSuccess("CREATE TABLE test (a INT, b INT AS (a + 1) VIRTUAL)");
}

TEST_F(MySQLParserTest, CreateTableWithConstraints) {
    expectSuccess("CREATE TABLE test (id INT, name VARCHAR(50), PRIMARY KEY (id))");
    expectSuccess("CREATE TABLE test (id INT, name VARCHAR(50), UNIQUE KEY uq_name (name))");
    expectSuccess("CREATE TABLE test (id INT, role_id INT, FOREIGN KEY (role_id) REFERENCES roles(id))");
    expectSuccess("CREATE TABLE test (id INT, CHECK (id > 0))");
}

TEST_F(MySQLParserTest, CreateTableOptionAllowlist) {
    expectSuccess("CREATE TABLE test (id INT) ENGINE=InnoDB");
    expectSuccess("CREATE TABLE test (id INT) DEFAULT CHARSET=utf8");
    expectError("CREATE TABLE test (id INT) WITH (fillfactor=90)");
    expectError("CREATE TABLE test (id INT) INHERITS (parent)");
    expectError("CREATE TABLE test (id INT) FOO=1");
}

TEST_F(MySQLParserTest, IndexAlgorithmGuardrails) {
    expectSuccess("CREATE TABLE test (id INT, INDEX idx USING BTREE (id))");
    expectSuccess("CREATE TABLE test (id INT, INDEX idx (id) USING HASH)");
    expectError("CREATE TABLE test (id INT, INDEX idx (id) USING GIN)");
}

TEST_F(MySQLParserTest, RejectDomainStatements) {
    expectError("CREATE DOMAIN test.email AS TEXT");
    expectError("ALTER DOMAIN test.email SET DEFAULT 'x'");
    expectError("DROP DOMAIN test.email");
}

TEST_F(MySQLParserTest, TransactionStatements) {
    expectSuccess("BEGIN");
    expectSuccess("BEGIN WORK");
    expectSuccess("START TRANSACTION");
    expectSuccess("START TRANSACTION READ ONLY");
    expectSuccess("START TRANSACTION READ WRITE");
    expectSuccess("START TRANSACTION WITH CONSISTENT SNAPSHOT");
    expectSuccess("COMMIT");
    expectSuccess("ROLLBACK");
    expectSuccess("SAVEPOINT my_savepoint");
    expectSuccess("ROLLBACK TO SAVEPOINT my_savepoint");
    expectSuccess("RELEASE SAVEPOINT my_savepoint");
}

TEST_F(MySQLParserTest, SetStatements) {
    expectSuccess("SET @var = 1");
    expectSuccess("SET @name = 'test'");
    expectSuccess("SET TRANSACTION ISOLATION LEVEL READ COMMITTED");
    expectSuccess("SET TRANSACTION READ ONLY");
    expectSuccess("SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ, READ WRITE");
    expectSuccess("SET AUTOCOMMIT = 0");
    expectSuccess("SET AUTOCOMMIT = OFF");
}

TEST_F(MySQLParserTest, SetAutocommitEmitsExtendedOpcode) {
    Parser parser("SET AUTOCOMMIT = 0");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success()) << "Failed to parse SET AUTOCOMMIT";

    const auto& bytecode = result.bytecode();
    size_t offset = 0;
    ASSERT_TRUE(readExtendedHeader(bytecode, sblr::ExtendedOpcode::EXT_SET_AUTOCOMMIT, &offset));
    ASSERT_LT(offset + 2, bytecode.size());

    EXPECT_EQ(bytecode[offset++], 0);
    EXPECT_EQ(bytecode[offset++],
              static_cast<uint8_t>(sblr::TransactionConflictAction::DEFAULT));
}

TEST_F(MySQLParserTest, SetAutocommitConflictRejected) {
    expectError("SET AUTOCOMMIT = 1 ON CONFLICT ERROR 42");
}

TEST_F(MySQLParserTest, TransactionGuardrails) {
    expectError("START TRANSACTION ON CONFLICT COMMIT");
    expectError("SET TRANSACTION AUTOCOMMIT ON");
}

TEST_F(MySQLParserTest, AlterDatabaseGuardrails) {
    expectError("ALTER DATABASE testdb RENAME TO otherdb");
    expectError("ALTER SCHEMA testdb RENAME TO otherdb");
}

TEST_F(MySQLParserTest, QualifiedNameGuardrails) {
    expectError("SHOW COLUMNS FROM a.b.c");
    expectError("SHOW CREATE TABLE a.b.c");
    expectError("DESCRIBE a.b.c");
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

TEST_F(MySQLParserTest, NullSafeEqualEmitsExtendedOpcode) {
    Parser parser("SELECT 1 <=> NULL");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success()) << "Failed to parse NULL-safe equality";
    EXPECT_TRUE(hasExtendedOpcode(result.bytecode(), sblr::ExtendedOpcode::EXT_NULL_SAFE_EQ));
}

TEST_F(MySQLParserTest, LikeEscapeEmitsExtendedOpcode) {
    Parser parser("SELECT 'abc' LIKE 'a!%' ESCAPE '!'");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success()) << "Failed to parse LIKE ... ESCAPE";
    EXPECT_TRUE(hasExtendedOpcode(result.bytecode(), sblr::ExtendedOpcode::EXT_LIKE_ESCAPE));
}

TEST_F(MySQLParserTest, PlaceholderEmitsExtendedOpcode) {
    Parser parser("SELECT ? + ?");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success()) << "Failed to parse placeholders";
    EXPECT_TRUE(hasExtendedOpcode(result.bytecode(), sblr::ExtendedOpcode::EXT_PLACEHOLDER));
    auto positions = placeholderPositions(result.bytecode());
    ASSERT_EQ(positions.size(), 2u);
    EXPECT_EQ(positions[0], 1);
    EXPECT_EQ(positions[1], 2);
}

TEST_F(MySQLParserTest, JSONOperators) {
    expectSuccess("SELECT JSON_EXTRACT(data, '$.name') FROM documents");
}

TEST_F(MySQLParserTest, Replace) {
    expectSuccess("REPLACE INTO users (id, name) VALUES (1, 'John')");
}

TEST_F(MySQLParserTest, AlterTableColumnActions) {
    expectSuccess("ALTER TABLE users ADD COLUMN age INT");
    expectSuccess("ALTER TABLE users DROP COLUMN age");
    expectSuccess("ALTER TABLE users MODIFY COLUMN age BIGINT");
    expectSuccess("ALTER TABLE users CHANGE COLUMN age age BIGINT");
    expectSuccess("ALTER TABLE users RENAME COLUMN age TO age_years");
}

TEST_F(MySQLParserTest, CreateTableWithOptions) {
    expectSuccess("CREATE TABLE users (id INT) ENGINE=InnoDB AUTO_INCREMENT=1 "
                  "DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='test'");
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
