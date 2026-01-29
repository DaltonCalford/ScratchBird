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

size_t findExtendedOpcodeOffset(const std::vector<uint8_t>& bytecode,
                                sblr::ExtendedOpcode expected) {
    uint16_t target = static_cast<uint16_t>(expected);
    for (size_t i = 0; i + 2 < bytecode.size(); ++i) {
        if (bytecode[i] == static_cast<uint8_t>(sblr::Opcode::EXTENDED_OPCODE)) {
            uint16_t opcode = sblr::readInt16(&bytecode[i + 1]);
            if (opcode == target) {
                return i;
            }
            i += 2;
        }
    }
    return bytecode.size();
}

size_t findOpcodeOffset(const std::vector<uint8_t>& bytecode,
                        sblr::Opcode expected) {
    uint8_t target = static_cast<uint8_t>(expected);
    for (size_t i = 0; i < bytecode.size(); ++i) {
        if (bytecode[i] == target) {
            return i;
        }
    }
    return bytecode.size();
}

bool endsWith(const std::string& value, const std::string& suffix) {
    if (suffix.size() > value.size()) {
        return false;
    }
    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

bool readUVarintAt(const std::vector<uint8_t>& bytecode,
                   size_t& offset,
                   uint64_t& value_out);
bool readVarStringAt(const std::vector<uint8_t>& bytecode,
                     size_t& offset,
                     std::string& out);

struct TableRefPayload {
    std::string name;
    std::string alias;
};

std::vector<TableRefPayload> collectTableRefs(const std::vector<uint8_t>& bytecode) {
    std::vector<TableRefPayload> refs;
    size_t offset = 0;
    while (offset < bytecode.size()) {
        if (bytecode[offset] != static_cast<uint8_t>(sblr::Opcode::TABLE_REF)) {
            ++offset;
            continue;
        }
        ++offset;
        if (offset >= bytecode.size()) {
            break;
        }
        uint8_t ref_kind = bytecode[offset++];
        if (ref_kind != 0) {
            if (offset + 16 > bytecode.size()) {
                break;
            }
            offset += 16;
        }

        std::string name;
        if (!readVarStringAt(bytecode, offset, name)) {
            break;
        }

        std::string alias;
        if (!readVarStringAt(bytecode, offset, alias)) {
            break;
        }

        refs.push_back({name, alias});
    }
    return refs;
}

bool readUVarintAt(const std::vector<uint8_t>& bytecode,
                   size_t& offset,
                   uint64_t& value_out) {
    if (offset >= bytecode.size()) {
        return false;
    }
    size_t bytes_read = 0;
    if (!sblr::readUVarint(&bytecode[offset],
                           bytecode.size() - offset,
                           value_out,
                           bytes_read)) {
        return false;
    }
    offset += bytes_read;
    return true;
}

bool readVarStringAt(const std::vector<uint8_t>& bytecode,
                     size_t& offset,
                     std::string& out) {
    uint64_t length = 0;
    if (!readUVarintAt(bytecode, offset, length)) {
        return false;
    }
    if (offset + length > bytecode.size()) {
        return false;
    }
    out.assign(reinterpret_cast<const char*>(&bytecode[offset]),
               static_cast<size_t>(length));
    offset += static_cast<size_t>(length);
    return true;
}

constexpr uint8_t kDomainKindRecord = 1;
constexpr uint8_t kDomainKindEnum = 2;

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
    Parser parser("SELECT data[1]");
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

TEST_F(PostgreSQLParserTest, SelectWithJoinUsingIsRejected) {
    expectError("SELECT * FROM users JOIN orders USING (id)");
}

TEST_F(PostgreSQLParserTest, SelectJoinBytecodeShape) {
    Parser parser("SELECT * FROM users u JOIN orders o ON users.id = orders.user_id");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());

    const auto& bc = result.bytecode();
    size_t offset = findOpcodeOffset(bc, sblr::Opcode::SELECT);
    ASSERT_NE(offset, bc.size());
    offset += 1;

    ASSERT_LT(offset + 1, bc.size());
    uint8_t distinct_flag = bc[offset++];
    EXPECT_EQ(distinct_flag, 0u);

    ASSERT_LT(offset + 1, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::BEGIN_LIST));

    uint64_t select_count = 0;
    ASSERT_TRUE(readUVarintAt(bc, offset, select_count));
    EXPECT_EQ(select_count, 1u);
    ASSERT_LT(offset + 1, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::SELECT_STAR));
    ASSERT_LT(offset + 1, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::END_LIST));

    ASSERT_LT(offset + 1, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::BEGIN_LIST));
    uint64_t from_count = 0;
    ASSERT_TRUE(readUVarintAt(bc, offset, from_count));
    EXPECT_EQ(from_count, 1u);
    ASSERT_LT(offset + 1, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::TABLE_REF));
    ASSERT_LT(offset + 1, bc.size());
    EXPECT_EQ(bc[offset++], 0u);
    std::string from_table;
    ASSERT_TRUE(readVarStringAt(bc, offset, from_table));
    EXPECT_TRUE(from_table == "users" || endsWith(from_table, "/users"));
    std::string from_alias;
    ASSERT_TRUE(readVarStringAt(bc, offset, from_alias));
    EXPECT_EQ(from_alias, "u");
    ASSERT_LT(offset + 1, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::END_LIST));

    ASSERT_LT(offset + 2, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::JOIN_TYPE));
    EXPECT_EQ(bc[offset++], 0u);
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::TABLE_REF));
    ASSERT_LT(offset + 1, bc.size());
    EXPECT_EQ(bc[offset++], 0u);
    std::string join_table;
    ASSERT_TRUE(readVarStringAt(bc, offset, join_table));
    EXPECT_TRUE(join_table == "orders" || endsWith(join_table, "/orders"));
    std::string join_alias;
    ASSERT_TRUE(readVarStringAt(bc, offset, join_alias));
    EXPECT_EQ(join_alias, "o");

    ASSERT_LT(offset + 1, bc.size());
    EXPECT_EQ(bc[offset], static_cast<uint8_t>(sblr::Opcode::JOIN_CONDITION));
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

TEST_F(PostgreSQLParserTest, SelectWithForClause) {
    expectSuccess("SELECT * FROM users FOR UPDATE");
    expectSuccess("SELECT * FROM users FOR SHARE");
    expectSuccess("SELECT * FROM users FOR KEY SHARE");
    expectSuccess("SELECT * FROM users FOR NO KEY UPDATE");
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

TEST_F(PostgreSQLParserTest, InsertOnConflictBytecodeShape) {
    Parser parser(
        "INSERT INTO users (id, name) VALUES (1, 'John') "
        "ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name"
    );
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());

    const auto& bc = result.bytecode();
    size_t offset = findExtendedOpcodeOffset(bc, sblr::ExtendedOpcode::EXT_ON_CONFLICT);
    ASSERT_NE(offset, bc.size());
    offset += 3;

    ASSERT_LT(offset + 3, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::EXTENDED_OPCODE));
    EXPECT_EQ(sblr::readInt16(&bc[offset]),
              static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_COLUMN));
    offset += 2;

    ASSERT_LT(offset + 1, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::BEGIN_LIST));
    uint64_t col_count = 0;
    ASSERT_TRUE(readUVarintAt(bc, offset, col_count));
    EXPECT_EQ(col_count, 1u);
    ASSERT_LT(offset + 1, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::COLUMN_REF));
    std::string col_name;
    ASSERT_TRUE(readVarStringAt(bc, offset, col_name));
    EXPECT_EQ(col_name, "id");
    ASSERT_LT(offset + 1, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::END_LIST));

    ASSERT_LT(offset + 3, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::EXTENDED_OPCODE));
    EXPECT_EQ(sblr::readInt16(&bc[offset]),
              static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_UPDATE));
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

TEST_F(PostgreSQLParserTest, UpdateFromBytecodeShape) {
    Parser parser("UPDATE users SET name = o.customer_name FROM orders o WHERE users.id = o.user_id");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());
    auto refs = collectTableRefs(result.bytecode());

    bool has_users = false;
    bool has_orders = false;
    for (const auto& ref : refs) {
        if (ref.name == "users" || endsWith(ref.name, "/users")) {
            has_users = true;
        } else if (ref.name == "orders" || endsWith(ref.name, "/orders")) {
            has_orders = true;
        }
    }
    EXPECT_TRUE(has_users);
    EXPECT_TRUE(has_orders);
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

TEST_F(PostgreSQLParserTest, DeleteUsingBytecodeShape) {
    Parser parser("DELETE FROM users USING orders o WHERE users.id = o.user_id");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());
    auto refs = collectTableRefs(result.bytecode());

    bool has_users = false;
    bool has_orders = false;
    for (const auto& ref : refs) {
        if (ref.name == "users" || endsWith(ref.name, "/users")) {
            has_users = true;
        } else if (ref.name == "orders" || endsWith(ref.name, "/orders")) {
            has_orders = true;
        }
    }
    EXPECT_TRUE(has_users);
    EXPECT_TRUE(has_orders);
}

TEST_F(PostgreSQLParserTest, DeleteWithReturning) {
    expectSuccess("DELETE FROM users WHERE id = 1 RETURNING *");
}

// ============================================================================
// MERGE Statement Tests
// ============================================================================

TEST_F(PostgreSQLParserTest, MergeBasic) {
    expectSuccess(
        "MERGE INTO users u USING staging s ON (u.id = s.id) "
        "WHEN MATCHED THEN UPDATE SET name = s.name "
        "WHEN NOT MATCHED THEN INSERT (id, name) VALUES (s.id, s.name)"
    );
}

TEST_F(PostgreSQLParserTest, MergeBytecodeShape) {
    Parser parser(
        "MERGE INTO users u USING staging s ON (u.id = s.id) "
        "WHEN MATCHED THEN UPDATE SET name = s.name "
        "WHEN NOT MATCHED THEN INSERT (id, name) VALUES (s.id, s.name)"
    );
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());

    const auto& bc = result.bytecode();
    size_t offset = findExtendedOpcodeOffset(bc, sblr::ExtendedOpcode::EXT_MERGE_START);
    ASSERT_NE(offset, bc.size());
    offset += 3;

    std::string target;
    ASSERT_TRUE(readVarStringAt(bc, offset, target));
    EXPECT_TRUE(target == "users" || endsWith(target, "/users"));

    ASSERT_LT(offset + 3, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::EXTENDED_OPCODE));
    EXPECT_EQ(sblr::readInt16(&bc[offset]),
              static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_SOURCE));
    offset += 2;

    std::string source;
    ASSERT_TRUE(readVarStringAt(bc, offset, source));
    EXPECT_TRUE(source == "staging" || endsWith(source, "/staging"));

    ASSERT_LT(offset + 3, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::EXTENDED_OPCODE));
    EXPECT_EQ(sblr::readInt16(&bc[offset]),
              static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_ON));
    offset += 2;

    ASSERT_LE(offset + 4, bc.size());
    uint32_t on_len = sblr::readInt32(&bc[offset]);
    offset += 4;
    EXPECT_GT(on_len, 0u);
    offset += on_len;

    ASSERT_LT(offset + 3, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::EXTENDED_OPCODE));
    EXPECT_EQ(sblr::readInt16(&bc[offset]),
              static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_WHEN_MATCHED));
    offset += 2;

    ASSERT_LT(offset + 2, bc.size());
    uint8_t has_condition = bc[offset++];
    EXPECT_EQ(has_condition, 0u);
    uint8_t is_delete = bc[offset++];
    EXPECT_EQ(is_delete, 0u);

    ASSERT_LE(offset + 4, bc.size());
    uint32_t assign_count = sblr::readInt32(&bc[offset]);
    offset += 4;
    EXPECT_EQ(assign_count, 1u);

    std::string assign_col;
    ASSERT_TRUE(readVarStringAt(bc, offset, assign_col));
    EXPECT_EQ(assign_col, "name");
    ASSERT_LE(offset + 4, bc.size());
    uint32_t assign_len = sblr::readInt32(&bc[offset]);
    offset += 4;
    EXPECT_GT(assign_len, 0u);
    offset += assign_len;

    ASSERT_LT(offset + 3, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::EXTENDED_OPCODE));
    EXPECT_EQ(sblr::readInt16(&bc[offset]),
              static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_WHEN_NOT_MATCHED));
    offset += 2;

    ASSERT_LT(offset + 1, bc.size());
    has_condition = bc[offset++];
    EXPECT_EQ(has_condition, 0u);

    ASSERT_LE(offset + 4, bc.size());
    uint32_t insert_col_count = sblr::readInt32(&bc[offset]);
    offset += 4;
    EXPECT_EQ(insert_col_count, 2u);

    std::string insert_col;
    ASSERT_TRUE(readVarStringAt(bc, offset, insert_col));
    EXPECT_EQ(insert_col, "id");
    ASSERT_TRUE(readVarStringAt(bc, offset, insert_col));
    EXPECT_EQ(insert_col, "name");

    for (uint32_t i = 0; i < insert_col_count; ++i) {
        ASSERT_LE(offset + 4, bc.size());
        uint32_t expr_len = sblr::readInt32(&bc[offset]);
        offset += 4;
        EXPECT_GT(expr_len, 0u);
        offset += expr_len;
    }

    ASSERT_LT(offset + 3, bc.size());
    EXPECT_EQ(bc[offset++], static_cast<uint8_t>(sblr::Opcode::EXTENDED_OPCODE));
    EXPECT_EQ(sblr::readInt16(&bc[offset]),
              static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_END));
}

TEST_F(PostgreSQLParserTest, MergeInsertRequiresColumnList) {
    expectError(
        "MERGE INTO users USING staging ON (users.id = staging.id) "
        "WHEN NOT MATCHED THEN INSERT VALUES (staging.id)"
    );
}

TEST_F(PostgreSQLParserTest, MergeUsingSubqueryRejected) {
    expectError(
        "MERGE INTO users USING (SELECT id, name FROM staging) s "
        "ON (users.id = s.id) "
        "WHEN NOT MATCHED THEN INSERT (id, name) VALUES (s.id, s.name)"
    );
}

TEST_F(PostgreSQLParserTest, MergeNotMatchedBySourceEmitsOpcode) {
    Parser parser(
        "MERGE INTO users u USING staging s ON (u.id = s.id) "
        "WHEN NOT MATCHED BY SOURCE THEN DELETE"
    );
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());
    EXPECT_TRUE(hasExtendedOpcode(result.bytecode(),
                                  sblr::ExtendedOpcode::EXT_MERGE_WHEN_NOT_MATCHED_SOURCE));
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

TEST_F(PostgreSQLParserTest, CreateTableWithExpressionDefault) {
    expectSuccess("CREATE TABLE test (a INT DEFAULT (1 + 2), b TEXT DEFAULT 'x')");
}

TEST_F(PostgreSQLParserTest, CreateTableWithGeneratedColumn) {
    expectSuccess("CREATE TABLE test (a INT, b INT GENERATED ALWAYS AS (a + 1) STORED)");
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
    expectSuccess("CREATE INDEX idx_include ON users (id) INCLUDE (name)");
}

TEST_F(PostgreSQLParserTest, CreateIndexWithPredicate) {
    expectSuccess("CREATE INDEX idx_active ON users (id) WHERE id > 0");
}

TEST_F(PostgreSQLParserTest, CreateView) {
    expectSuccess("CREATE VIEW active_users AS SELECT * FROM users WHERE active = TRUE");
    expectSuccess("CREATE OR REPLACE VIEW active_users AS SELECT * FROM users WHERE active = TRUE");
    expectSuccess("CREATE VIEW user_counts AS SELECT department, COUNT(*) FROM users GROUP BY department");
}

TEST_F(PostgreSQLParserTest, CreateDatabaseSchemaSequence) {
    expectSuccess("CREATE DATABASE demo");
    expectSuccess("CREATE SCHEMA analytics");
    expectSuccess("CREATE SEQUENCE seq_orders START WITH 10 INCREMENT BY 5");
}

TEST_F(PostgreSQLParserTest, CreateFunctionProcedureTrigger) {
    expectSuccess("CREATE FUNCTION add_one(x INT) RETURNS INT AS $$ SELECT x + 1 $$");
    expectSuccess("CREATE PROCEDURE do_work(x INT) AS $$ SELECT x $$");
    expectSuccess("CREATE TRIGGER trg_users BEFORE INSERT ON users FOR EACH ROW EXECUTE FUNCTION audit()");
}

TEST_F(PostgreSQLParserTest, CreateIndexUnsupportedFeatures) {
    expectError("CREATE INDEX idx_expr ON users ((lower(name)))");
    expectError("CREATE INDEX idx_ts ON users (id) TABLESPACE fast_ts");
}

TEST_F(PostgreSQLParserTest, CreateDomainBasic) {
    Parser parser("CREATE DOMAIN status_domain AS INTEGER DEFAULT 1");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());
    EXPECT_TRUE(hasExtendedOpcode(result.bytecode(), sblr::ExtendedOpcode::EXT_CREATE_DOMAIN));
}

TEST_F(PostgreSQLParserTest, CreateDomainRejectsScratchBirdExtensions) {
    expectError("CREATE DOMAIN status_domain AS INT WITH SECURITY (MASKING = FULL)");
    expectError("CREATE DOMAIN status_domain AS INT INHERITS (base_status)");
}

TEST_F(PostgreSQLParserTest, CreateTypeEnumMapsToDomain) {
    Parser parser("CREATE TYPE mood AS ENUM ('sad', 'ok')");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());
    size_t offset = findExtendedOpcodeOffset(result.bytecode(), sblr::ExtendedOpcode::EXT_CREATE_DOMAIN);
    ASSERT_NE(offset, result.bytecode().size());
    EXPECT_EQ(result.bytecode()[offset + 4], kDomainKindEnum);
}

TEST_F(PostgreSQLParserTest, CreateTypeCompositeMapsToDomain) {
    Parser parser("CREATE TYPE point_type AS (x INTEGER, y INTEGER)");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());
    size_t offset = findExtendedOpcodeOffset(result.bytecode(), sblr::ExtendedOpcode::EXT_CREATE_DOMAIN);
    ASSERT_NE(offset, result.bytecode().size());
    EXPECT_EQ(result.bytecode()[offset + 4], kDomainKindRecord);
}

TEST_F(PostgreSQLParserTest, CreateTypeCompositeRejectsConstraints) {
    expectError("CREATE TYPE point_type AS (x INT NOT NULL, y INT)");
}

TEST_F(PostgreSQLParserTest, CreateTypeRangeRejected) {
    expectError("CREATE TYPE numrange AS RANGE (SUBTYPE = int4)");
}

TEST_F(PostgreSQLParserTest, CreateTableUnsupportedFeatures) {
    expectError("CREATE TABLE users (id INT) INHERITS (base_users)");
    expectError("CREATE TABLE users (id INT, CHECK (id > 0))");
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

TEST_F(PostgreSQLParserTest, AlterTableBasic) {
    expectSuccess("ALTER TABLE users ADD COLUMN age INT");
    expectSuccess("ALTER TABLE users DROP COLUMN age");
    expectSuccess("ALTER TABLE users ALTER COLUMN age TYPE BIGINT");
    expectSuccess("ALTER TABLE users RENAME TO users_new");
}

TEST_F(PostgreSQLParserTest, AlterTableUnsupportedFeatures) {
    expectError("ALTER TABLE users ADD COLUMN age INT NOT NULL");
    expectError("ALTER TABLE users DROP CONSTRAINT users_pkey");
    expectError("ALTER TABLE users ALTER COLUMN age SET DEFAULT 0");
}

TEST_F(PostgreSQLParserTest, TruncateStatements) {
    expectSuccess("TRUNCATE users");
    expectError("TRUNCATE users RESTART IDENTITY");
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
    expectError("CREATE DATABASE demo WITH TABLESPACE = ts1");
    expectError("CREATE TABLE users (id INT) TABLESPACE ts1");
    expectError("CREATE INDEX idx_users_id ON users (id) TABLESPACE ts1");
    expectError("CREATE TABLE a.b.c (id INT)");
    expectError("SELECT a.b.c.d FROM t");
}

// ============================================================================
// Utility Statement Tests
// ============================================================================

TEST_F(PostgreSQLParserTest, SetAndShowStatements) {
    expectSuccess("SET search_path = public");
    expectSuccess("SET LOCAL search_path = public");
    expectSuccess("SHOW search_path");
    expectSuccess("SET TRANSACTION ISOLATION LEVEL READ COMMITTED");
    expectSuccess("SET TRANSACTION READ ONLY");
    expectSuccess("SET CONSTRAINTS ALL DEFERRED");
}

TEST_F(PostgreSQLParserTest, AnalyzeStatements) {
    expectSuccess("ANALYZE");
    expectSuccess("ANALYZE VERBOSE users");
    expectSuccess("ANALYZE users (id, name)");
}

TEST_F(PostgreSQLParserTest, CopyStatements) {
    expectSuccess("COPY users FROM STDIN");
    expectSuccess("COPY users TO STDOUT");
    expectSuccess("COPY users FROM 'file.csv' WITH (FORMAT csv, HEADER)");
    expectError("COPY users FROM STDIN WITH (FORMAT xml)");
    expectError("COPY users FROM STDIN WITH (DELIMITER '||')");
}

TEST_F(PostgreSQLParserTest, CopyBytecodeShape) {
    Parser parser(
        "COPY users (id, name) FROM 'file.csv' "
        "WITH (FORMAT csv, DELIMITER ',', NULL '', HEADER, QUOTE '\"', ESCAPE '/', ENCODING 'UTF8')"
    );
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());

    const auto& bc = result.bytecode();
    size_t offset = findExtendedOpcodeOffset(bc, sblr::ExtendedOpcode::EXT_COPY);
    ASSERT_NE(offset, bc.size());
    offset += 3;

    std::string table_path;
    ASSERT_TRUE(readVarStringAt(bc, offset, table_path));
    EXPECT_TRUE(table_path == "users" || endsWith(table_path, "/users"));

    ASSERT_LT(offset + 1, bc.size());
    uint8_t direction = bc[offset++];
    EXPECT_EQ(direction, 1u);

    std::string target;
    ASSERT_TRUE(readVarStringAt(bc, offset, target));
    EXPECT_EQ(target, "file.csv");

    ASSERT_LE(offset + 4, bc.size());
    uint32_t col_count = sblr::readInt32(&bc[offset]);
    offset += 4;
    EXPECT_EQ(col_count, 2u);

    std::string col;
    ASSERT_TRUE(readVarStringAt(bc, offset, col));
    EXPECT_EQ(col, "id");
    ASSERT_TRUE(readVarStringAt(bc, offset, col));
    EXPECT_EQ(col, "name");

    ASSERT_LT(offset + 1, bc.size());
    uint8_t format = bc[offset++];
    EXPECT_EQ(format, 2u);

    std::string delimiter;
    ASSERT_TRUE(readVarStringAt(bc, offset, delimiter));
    EXPECT_EQ(delimiter, ",");

    std::string null_str;
    ASSERT_TRUE(readVarStringAt(bc, offset, null_str));
    EXPECT_EQ(null_str, "");

    ASSERT_LT(offset + 1, bc.size());
    uint8_t header = bc[offset++];
    EXPECT_EQ(header, 1u);

    std::string quote;
    ASSERT_TRUE(readVarStringAt(bc, offset, quote));
    EXPECT_EQ(quote, "\"");

    std::string escape;
    ASSERT_TRUE(readVarStringAt(bc, offset, escape));
    EXPECT_EQ(escape, "/");

    std::string encoding;
    ASSERT_TRUE(readVarStringAt(bc, offset, encoding));
    EXPECT_EQ(encoding, "UTF8");
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
    // The default schema should be /remote/emulation/postgresql/localhost/
    // This would be verified in the generated bytecode if we had bytecode inspection
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success());
}
