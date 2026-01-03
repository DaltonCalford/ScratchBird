/**
 * PostgreSQL Parser Unit Tests
 *
 * Tests the PostgreSQL parser's ability to parse PostgreSQL 16 SQL syntax
 * and generate correct SBLR bytecode.
 *
 * NOTE: Some tests are disabled pending full parser implementation.
 * The PostgreSQL parser is functional for basic SQL operations.
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/postgresql/pg_parser.h"
#include "scratchbird/parser/postgresql/pg_lexer.h"
#include "scratchbird/sblr/opcodes.h"

using namespace scratchbird::parser::postgresql;
namespace sblr = scratchbird::sblr;

namespace {

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

class PostgreSQLLexerTest : public ::testing::Test {
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

TEST_F(PostgreSQLLexerTest, BasicKeywords) {
    Lexer lexer("SELECT FROM WHERE");

    expectKeyword(lexer, TokenType::KW_SELECT, "SELECT");
    expectKeyword(lexer, TokenType::KW_FROM, "FROM");
    expectKeyword(lexer, TokenType::KW_WHERE, "WHERE");
    expectToken(lexer, TokenType::END_OF_FILE, "EOF");
}

TEST_F(PostgreSQLLexerTest, Identifiers) {
    Lexer lexer("table_name column123 _private");

    Token t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "table_name");

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "column123");

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "_private");
}

TEST_F(PostgreSQLLexerTest, QuotedIdentifiers) {
    Lexer lexer("\"Table Name\" \"SELECT\" \"column-with-dash\"");

    Token t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::QUOTED_IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "Table Name");

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::QUOTED_IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "SELECT");  // Reserved word as identifier

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::QUOTED_IDENTIFIER);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "column-with-dash");
}

TEST_F(PostgreSQLLexerTest, IntegerLiterals) {
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

TEST_F(PostgreSQLLexerTest, FloatLiterals) {
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

TEST_F(PostgreSQLLexerTest, StringLiterals) {
    Lexer lexer("'hello' 'world' 'it''s'");

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

TEST_F(PostgreSQLLexerTest, DollarQuotedStrings) {
    Lexer lexer("$$hello world$$");

    Token t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::DOLLAR_STRING);
    EXPECT_EQ(lexer.stringPool().get(t.value.string_id), "hello world");
}

TEST_F(PostgreSQLLexerTest, EscapeStrings) {
    Lexer lexer("E'hello\\nworld'");

    Token t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::ESCAPE_STRING);
}

TEST_F(PostgreSQLLexerTest, PositionalParameters) {
    Lexer lexer("$1 $2 $10");

    Token t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::PARAMETER);
    EXPECT_EQ(t.value.int_value, 1);

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::PARAMETER);
    EXPECT_EQ(t.value.int_value, 2);

    t = lexer.nextToken();
    EXPECT_EQ(t.type, TokenType::PARAMETER);
    EXPECT_EQ(t.value.int_value, 10);
}

TEST_F(PostgreSQLLexerTest, Operators) {
    // Note: <> returns NOT_EQUAL in this lexer
    Lexer lexer("+ - * / % = <> != < > <= >= :: -> ->>");

    expectToken(lexer, TokenType::PLUS);
    expectToken(lexer, TokenType::MINUS);
    expectToken(lexer, TokenType::STAR);
    expectToken(lexer, TokenType::SLASH);
    expectToken(lexer, TokenType::PERCENT);
    expectToken(lexer, TokenType::EQUAL);
    expectToken(lexer, TokenType::NOT_EQUAL);   // <> returns NOT_EQUAL
    expectToken(lexer, TokenType::NOT_EQUAL);   // != also returns NOT_EQUAL
    expectToken(lexer, TokenType::LESS_THAN);
    expectToken(lexer, TokenType::GREATER_THAN);
    expectToken(lexer, TokenType::LESS_EQUAL);
    expectToken(lexer, TokenType::GREATER_EQUAL);
    expectToken(lexer, TokenType::DOUBLE_COLON);
    expectToken(lexer, TokenType::ARROW);
    expectToken(lexer, TokenType::DOUBLE_ARROW);
}

TEST_F(PostgreSQLLexerTest, Punctuation) {
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

TEST_F(PostgreSQLLexerTest, Comments) {
    Lexer lexer("SELECT -- line comment\n"
                "FROM /* block\n"
                "comment */ WHERE");

    expectKeyword(lexer, TokenType::KW_SELECT);
    expectKeyword(lexer, TokenType::KW_FROM);
    expectKeyword(lexer, TokenType::KW_WHERE);
}

// ============================================================================
// Parser Tests
// ============================================================================

class PostgreSQLParserTest : public ::testing::Test {
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

TEST_F(PostgreSQLParserTest, IsDistinctFromUsesNullSafeEquality) {
    Parser parser("SELECT 1 IS DISTINCT FROM NULL");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success()) << "Failed to parse IS DISTINCT FROM";
    EXPECT_TRUE(hasExtendedOpcode(result.bytecode(), sblr::ExtendedOpcode::EXT_NULL_SAFE_EQ));
}

TEST_F(PostgreSQLParserTest, LikeEscapeEmitsExtendedOpcode) {
    Parser parser("SELECT 'abc' LIKE 'a!%' ESCAPE '!'");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success()) << "Failed to parse LIKE ... ESCAPE";
    EXPECT_TRUE(hasExtendedOpcode(result.bytecode(), sblr::ExtendedOpcode::EXT_LIKE_ESCAPE));
}

TEST_F(PostgreSQLParserTest, PlaceholderEmitsExtendedOpcode) {
    Parser parser("SELECT $1 + $2");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success()) << "Failed to parse placeholders";
    EXPECT_TRUE(hasExtendedOpcode(result.bytecode(), sblr::ExtendedOpcode::EXT_PLACEHOLDER));
    auto positions = placeholderPositions(result.bytecode());
    ASSERT_EQ(positions.size(), 2u);
    EXPECT_EQ(positions[0], 1);
    EXPECT_EQ(positions[1], 2);
}

TEST_F(PostgreSQLParserTest, ArraySubscriptEmitsExtendedOpcode) {
    Parser parser("SELECT data[1] FROM items");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success()) << "Failed to parse array subscript";
    EXPECT_TRUE(hasExtendedOpcode(result.bytecode(), sblr::ExtendedOpcode::EXT_ARRAY_SUBSCRIPT));
}

// ============================================================================
// SELECT Statement Tests
// ============================================================================

TEST_F(PostgreSQLParserTest, SimpleSelect) {
    expectSuccess("SELECT 1");
    expectSuccess("SELECT 1, 2, 3");
    expectSuccess("SELECT 'hello'");
    expectSuccess("SELECT 1 + 2");
    expectSuccess("SELECT *");
}

TEST_F(PostgreSQLParserTest, SelectWithFrom) {
    expectSuccess("SELECT * FROM users");
    expectSuccess("SELECT id, name FROM users");
    expectSuccess("SELECT u.id, u.name FROM users u");
    expectSuccess("SELECT u.id, u.name FROM users AS u");
}

TEST_F(PostgreSQLParserTest, SelectWithWhere) {
    expectSuccess("SELECT * FROM users WHERE id = 1");
    expectSuccess("SELECT * FROM users WHERE name = 'John'");
    expectSuccess("SELECT * FROM users WHERE id IN (1, 2, 3)");
    expectSuccess("SELECT * FROM users WHERE name LIKE '%test%'");
    expectSuccess("SELECT * FROM users WHERE name ILIKE '%TEST%'");
}

TEST_F(PostgreSQLParserTest, SelectWithJoin) {
    expectSuccess("SELECT * FROM users JOIN orders ON users.id = orders.user_id");
    expectSuccess("SELECT * FROM users LEFT JOIN orders ON users.id = orders.user_id");
    expectSuccess("SELECT * FROM users RIGHT JOIN orders ON users.id = orders.user_id");
    expectSuccess("SELECT * FROM users FULL JOIN orders ON users.id = orders.user_id");
    expectSuccess("SELECT * FROM users INNER JOIN orders ON users.id = orders.user_id");
    expectSuccess("SELECT * FROM users CROSS JOIN orders");
    expectSuccess("SELECT * FROM users NATURAL JOIN orders");
}

TEST_F(PostgreSQLParserTest, SelectWithGroupBy) {
    expectSuccess("SELECT department, COUNT(*) FROM employees GROUP BY department");
}

TEST_F(PostgreSQLParserTest, SelectWithOrderBy) {
    expectSuccess("SELECT * FROM users ORDER BY name");
    expectSuccess("SELECT * FROM users ORDER BY name ASC");
    expectSuccess("SELECT * FROM users ORDER BY name DESC");
    expectSuccess("SELECT * FROM users ORDER BY name ASC NULLS FIRST");
    expectSuccess("SELECT * FROM users ORDER BY name DESC NULLS LAST");
    expectSuccess("SELECT * FROM users ORDER BY name ASC, id DESC");
}

TEST_F(PostgreSQLParserTest, SelectWithLimit) {
    expectSuccess("SELECT * FROM users LIMIT 10");
    expectSuccess("SELECT * FROM users LIMIT 10 OFFSET 20");
    expectSuccess("SELECT * FROM users OFFSET 20");
    expectSuccess("SELECT * FROM users FETCH FIRST 10 ROWS ONLY");
}

TEST_F(PostgreSQLParserTest, SelectDistinct) {
    expectSuccess("SELECT DISTINCT name FROM users");
    expectSuccess("SELECT DISTINCT ON (department) name FROM employees");
    expectSuccess("SELECT ALL name FROM users");
}

TEST_F(PostgreSQLParserTest, SelectWithFunctions) {
    expectSuccess("SELECT COUNT(*) FROM users");
    expectSuccess("SELECT SUM(amount) FROM orders");
    expectSuccess("SELECT AVG(price) FROM products");
    expectSuccess("SELECT MIN(created_at), MAX(created_at) FROM events");
    expectSuccess("SELECT LENGTH('hello')");
    expectSuccess("SELECT UPPER(name) FROM users");
    expectSuccess("SELECT NOW()");
}

TEST_F(PostgreSQLParserTest, SelectWithCase) {
    expectSuccess("SELECT CASE WHEN status = 1 THEN 'Active' ELSE 'Inactive' END FROM users");
    expectSuccess("SELECT CASE status WHEN 1 THEN 'Active' WHEN 2 THEN 'Pending' ELSE 'Unknown' END FROM users");
}

TEST_F(PostgreSQLParserTest, SelectWithSubquery) {
    expectSuccess("SELECT * FROM users WHERE id IN (SELECT user_id FROM orders)");
    expectSuccess("SELECT * FROM users WHERE EXISTS (SELECT 1 FROM orders WHERE orders.user_id = users.id)");
    expectSuccess("SELECT (SELECT COUNT(*) FROM orders) AS order_count");
}

TEST_F(PostgreSQLParserTest, SelectWithCTE) {
    expectSuccess("WITH cte AS (SELECT * FROM users) SELECT * FROM cte");
}

TEST_F(PostgreSQLParserTest, SelectWithWindowFunction) {
    expectSuccess("SELECT ROW_NUMBER() OVER () FROM users");
    expectSuccess("SELECT ROW_NUMBER() OVER (ORDER BY name) FROM users");
    expectSuccess("SELECT ROW_NUMBER() OVER (PARTITION BY department ORDER BY name) FROM employees");
}

// ============================================================================
// INSERT Statement Tests
// ============================================================================

TEST_F(PostgreSQLParserTest, InsertBasic) {
    expectSuccess("INSERT INTO users (name, email) VALUES ('John', 'john@example.com')");
    expectSuccess("INSERT INTO users VALUES (1, 'John', 'john@example.com')");
    expectSuccess("INSERT INTO users (name) VALUES ('John'), ('Jane'), ('Bob')");
}

TEST_F(PostgreSQLParserTest, InsertWithOnConflict) {
    expectSuccess("INSERT INTO users (id, name) VALUES (1, 'John') ON CONFLICT (id) DO NOTHING");
    expectSuccess("INSERT INTO users (id, name) VALUES (1, 'John') ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name");
}

TEST_F(PostgreSQLParserTest, InsertWithReturning) {
    expectSuccess("INSERT INTO users (name) VALUES ('John') RETURNING id");
    expectSuccess("INSERT INTO users (name) VALUES ('John') RETURNING *");
    expectSuccess("INSERT INTO users (name) VALUES ('John') RETURNING id, name");
}

TEST_F(PostgreSQLParserTest, InsertWithDefault) {
    expectSuccess("INSERT INTO users (name, status) VALUES ('John', DEFAULT)");
    expectSuccess("INSERT INTO users DEFAULT VALUES");
}

// ============================================================================
// UPDATE Statement Tests
// ============================================================================

TEST_F(PostgreSQLParserTest, UpdateBasic) {
    expectSuccess("UPDATE users SET name = 'John'");
    expectSuccess("UPDATE users SET name = 'John' WHERE id = 1");
    expectSuccess("UPDATE users SET name = 'John', email = 'john@test.com' WHERE id = 1");
}

TEST_F(PostgreSQLParserTest, UpdateWithFrom) {
    expectSuccess("UPDATE users SET name = o.customer_name FROM orders o WHERE users.id = o.user_id");
}

TEST_F(PostgreSQLParserTest, UpdateWithReturning) {
    expectSuccess("UPDATE users SET name = 'John' WHERE id = 1 RETURNING *");
}

// ============================================================================
// DELETE Statement Tests
// ============================================================================

TEST_F(PostgreSQLParserTest, DeleteBasic) {
    expectSuccess("DELETE FROM users");
    expectSuccess("DELETE FROM users WHERE id = 1");
}

TEST_F(PostgreSQLParserTest, DeleteWithUsing) {
    expectSuccess("DELETE FROM users USING orders WHERE users.id = orders.user_id");
}

TEST_F(PostgreSQLParserTest, DeleteWithReturning) {
    expectSuccess("DELETE FROM users WHERE id = 1 RETURNING *");
}

// ============================================================================
// DDL Statement Tests
// ============================================================================

TEST_F(PostgreSQLParserTest, CreateTableBasic) {
    expectSuccess("CREATE TABLE users (id INT, name VARCHAR(100))");
    expectSuccess("CREATE TABLE users (id INTEGER PRIMARY KEY, name VARCHAR(100) NOT NULL)");
    expectSuccess("CREATE TABLE users (id SERIAL PRIMARY KEY, name VARCHAR(100) DEFAULT '')");
}

TEST_F(PostgreSQLParserTest, CreateTableIfNotExists) {
    expectSuccess("CREATE TABLE IF NOT EXISTS users (id INT)");
}

TEST_F(PostgreSQLParserTest, CreateTableWithTypes) {
    expectSuccess("CREATE TABLE test (a SMALLINT, b INT, c BIGINT)");
    expectSuccess("CREATE TABLE test (a REAL, b DOUBLE PRECISION, c NUMERIC(10,2))");
    expectSuccess("CREATE TABLE test (a CHAR(10), b VARCHAR(255), c TEXT)");
    expectSuccess("CREATE TABLE test (a DATE, b TIME, c TIMESTAMP)");
    expectSuccess("CREATE TABLE test (a BYTEA, b UUID)");
    expectSuccess("CREATE TABLE test (a JSON, b JSONB, c BOOLEAN)");
}

TEST_F(PostgreSQLParserTest, CreateTableTemporary) {
    expectSuccess("CREATE TEMPORARY TABLE temp_users (id INT)");
    expectSuccess("CREATE TEMP TABLE temp_users (id INT)");
    expectSuccess("CREATE UNLOGGED TABLE fast_log (id INT)");
}

TEST_F(PostgreSQLParserTest, CreateIndex) {
    expectSuccess("CREATE INDEX idx_name ON users (name)");
    expectSuccess("CREATE UNIQUE INDEX idx_email ON users (email)");
    expectSuccess("CREATE INDEX idx_name ON users (name DESC NULLS LAST)");
    expectSuccess("CREATE INDEX idx_name ON users USING btree (name)");
    expectSuccess("CREATE INDEX idx_name ON users USING gin (tags)");
    expectSuccess("CREATE INDEX idx_name ON users USING gist (location)");
}

TEST_F(PostgreSQLParserTest, CreateView) {
    expectSuccess("CREATE VIEW active_users AS SELECT * FROM users WHERE active = TRUE");
    expectSuccess("CREATE OR REPLACE VIEW active_users AS SELECT * FROM users WHERE active = TRUE");
    expectSuccess("CREATE VIEW user_counts AS SELECT department, COUNT(*) FROM users GROUP BY department");
}

TEST_F(PostgreSQLParserTest, DropTable) {
    expectSuccess("DROP TABLE users");
    expectSuccess("DROP TABLE IF EXISTS users");
    expectSuccess("DROP TABLE users CASCADE");
}

TEST_F(PostgreSQLParserTest, DropIndex) {
    expectSuccess("DROP INDEX idx_name");
}

TEST_F(PostgreSQLParserTest, DropView) {
    expectSuccess("DROP VIEW active_users");
}

// ============================================================================
// Transaction Statement Tests
// ============================================================================

TEST_F(PostgreSQLParserTest, TransactionStatements) {
    expectSuccess("BEGIN");
    expectSuccess("COMMIT");
    expectSuccess("ROLLBACK");
    expectSuccess("SAVEPOINT my_savepoint");
    expectSuccess("ROLLBACK TO SAVEPOINT my_savepoint");
    expectSuccess("RELEASE SAVEPOINT my_savepoint");
}

TEST_F(PostgreSQLParserTest, DialectGuardrails) {
    expectError("BEGIN AUTOCOMMIT ON");
    expectError("SET TRANSACTION AUTOCOMMIT ON");
    expectError("SET TRANSACTION ON CONFLICT COMMIT");
    expectError("SET AUTOCOMMIT = 1");
    expectError("SHOW TABLES");
    expectError("SHOW DATABASES");
    expectError("SHOW COLUMNS FROM users");
    expectError("SHOW INDEXES FROM users");
}

// ============================================================================
// GRANT/REVOKE Statement Tests
// ============================================================================

TEST_F(PostgreSQLParserTest, GrantStatements) {
    expectSuccess("GRANT SELECT ON users TO reader");
}

TEST_F(PostgreSQLParserTest, RevokeStatements) {
    expectSuccess("REVOKE SELECT ON users FROM reader");
    expectSuccess("REVOKE ALL ON users FROM PUBLIC");
}

// ============================================================================
// EXPLAIN Statement Tests
// ============================================================================

TEST_F(PostgreSQLParserTest, ExplainStatements) {
    expectSuccess("EXPLAIN SELECT * FROM users");
    expectSuccess("EXPLAIN ANALYZE SELECT * FROM users");
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(PostgreSQLParserTest, MissingKeyword) {
    expectError("SELECT FROM users");       // Missing column list or *
    expectError("INSERT users VALUES (1)"); // Missing INTO
}

TEST_F(PostgreSQLParserTest, UnterminatedString) {
    Lexer lexer("SELECT 'unterminated");
    Token t = lexer.nextToken();  // SELECT
    t = lexer.nextToken();        // Should be error
    EXPECT_EQ(t.type, TokenType::ERROR);
}

TEST_F(PostgreSQLParserTest, InvalidSyntax) {
    expectError("SELEC * FROM users");  // Typo in SELECT
    expectError("SELECT * FORM users"); // Typo in FROM
}

// ============================================================================
// Default Schema Path Test
// ============================================================================

TEST_F(PostgreSQLParserTest, DefaultSchemaPath) {
    Parser parser("SELECT * FROM users");
    // The default schema should be /remote/emulated/postgresql/localhost/
    // This would be verified in the generated bytecode if we had bytecode inspection
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success());
}
