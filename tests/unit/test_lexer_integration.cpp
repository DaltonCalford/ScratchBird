#include <gtest/gtest.h>
#include "scratchbird/parser/lexer.h"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace scratchbird::parser;

class LexerIntegrationTest : public ::testing::Test {
protected:
    struct TokenInfo {
        TokenType type;
        std::string text;
        int line;
        int column;
    };
    
    std::vector<TokenInfo> tokenizeWithInfo(const std::string& input) {
        Lexer lexer(input);
        std::vector<TokenInfo> tokens;
        
        Token tok;
        do {
            tok = lexer.nextToken();
            if (tok.type != TokenType::END_OF_FILE) {
                TokenInfo info;
                info.type = tok.type;
                info.line = tok.location.line;
                info.column = tok.location.column;
                
                // Get token text based on type
                switch (tok.type) {
                    case TokenType::IDENTIFIER:
                    case TokenType::STRING_LITERAL:
                        info.text = std::string(lexer.stringPool().get(tok.value.string_id));
                        break;
                    case TokenType::INTEGER_LITERAL:
                        info.text = std::to_string(tok.value.int_value);
                        break;
                    case TokenType::FLOAT_LITERAL:
                        info.text = std::to_string(tok.value.float_value);
                        break;
                    default:
                        info.text = tokenTypeToString(tok.type);
                        break;
                }
                
                tokens.push_back(info);
            }
        } while (tok.type != TokenType::END_OF_FILE && tok.type != TokenType::ERROR);
        
        return tokens;
    }
    
    void verifyTokenSequence(const std::string& sql, 
                            const std::vector<TokenType>& expectedTypes) {
        auto tokens = tokenizeWithInfo(sql);
        
        ASSERT_EQ(tokens.size(), expectedTypes.size()) 
            << "Token count mismatch for: " << sql;
            
        for (size_t i = 0; i < expectedTypes.size(); i++) {
            EXPECT_EQ(tokens[i].type, expectedTypes[i])
                << "Token " << i << " mismatch. Expected: " 
                << tokenTypeToString(expectedTypes[i])
                << ", Got: " << tokenTypeToString(tokens[i].type)
                << " (" << tokens[i].text << ")";
        }
    }
};

// ===== DDL (Data Definition Language) Tests =====

TEST_F(LexerIntegrationTest, CreateTableBasic) {
    const char* sql = R"(
        CREATE TABLE users (
            id INTEGER NOT NULL,
            username VARCHAR(50),
            email VARCHAR(100)
        )
    )";
    
    auto tokens = tokenizeWithInfo(sql);
    
    // Verify first few tokens
    EXPECT_EQ(tokens[0].type, TokenType::KW_CREATE);
    EXPECT_EQ(tokens[0].line, 2);
    
    EXPECT_EQ(tokens[1].type, TokenType::KW_TABLE);
    EXPECT_EQ(tokens[2].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[2].text, "users");
    
    // Count specific token types
    int parenCount = 0;
    int commaCount = 0;
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::LEFT_PAREN) parenCount++;
        if (tok.type == TokenType::COMMA) commaCount++;
    }
    
    EXPECT_EQ(parenCount, 4); // Main parens + 2 VARCHAR parens
    EXPECT_EQ(commaCount, 2); // Between columns
}

TEST_F(LexerIntegrationTest, CreateTableWithConstraints) {
    const char* sql = R"(
        CREATE TABLE orders (
            order_id BIGINT NOT NULL,
            customer_id INTEGER NOT NULL,
            order_date TIMESTAMP,
            total_amount DOUBLE,
            PRIMARY KEY (order_id),
            FOREIGN KEY (customer_id) REFERENCES customers(id)
        )
    )";
    
    // This tests keywords we might add later, for now some will be identifiers
    auto tokens = tokenizeWithInfo(sql);
    
    bool foundBigint = false;
    bool foundDouble = false;
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::KW_BIGINT) foundBigint = true;
        if (tok.type == TokenType::KW_DOUBLE) foundDouble = true;
    }
    
    EXPECT_TRUE(foundBigint);
    EXPECT_TRUE(foundDouble);
}

// ===== DML (Data Manipulation Language) Tests =====

TEST_F(LexerIntegrationTest, InsertStatements) {
    const char* sql = R"(
        INSERT INTO users (id, username, email) 
        VALUES (1, 'john_doe', 'john@example.com');
        
        INSERT INTO users VALUES 
            (2, 'jane_smith', 'jane@example.com'),
            (3, 'bob_wilson', 'bob@example.com');
    )";
    
    auto tokens = tokenizeWithInfo(sql);
    
    // Count INSERT keywords
    int insertCount = 0;
    int valuesCount = 0;
    int stringCount = 0;
    
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::KW_INSERT) insertCount++;
        if (tok.type == TokenType::KW_VALUES) valuesCount++;
        if (tok.type == TokenType::STRING_LITERAL) stringCount++;
    }
    
    EXPECT_EQ(insertCount, 2);
    EXPECT_EQ(valuesCount, 2);
    EXPECT_EQ(stringCount, 6); // 6 string literals total
}

TEST_F(LexerIntegrationTest, SelectQueries) {
    const char* sql = R"(
        SELECT * FROM users;
        
        SELECT id, username 
        FROM users 
        WHERE id = 1;
        
        SELECT u.username, o.total_amount
        FROM users u
        JOIN orders o ON u.id = o.customer_id
        WHERE o.total_amount > 100.50;
    )";
    
    auto tokens = tokenizeWithInfo(sql);
    
    // Verify qualified identifiers (table.column)
    bool foundDotNotation = false;
    for (size_t i = 1; i < tokens.size() - 1; i++) {
        if (tokens[i].type == TokenType::DOT &&
            tokens[i-1].type == TokenType::IDENTIFIER &&
            tokens[i+1].type == TokenType::IDENTIFIER) {
            foundDotNotation = true;
            break;
        }
    }
    
    EXPECT_TRUE(foundDotNotation);
    
    // Verify float literal
    bool foundFloat = false;
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::FLOAT_LITERAL && tok.text.find("100.5") == 0) {
            foundFloat = true;
            break;
        }
    }
    EXPECT_TRUE(foundFloat);
}

TEST_F(LexerIntegrationTest, UpdateAndDelete) {
    const char* sql = R"(
        UPDATE users 
        SET email = 'newemail@example.com' 
        WHERE username = 'john_doe';
        
        DELETE FROM orders 
        WHERE order_date < '2023-01-01';
    )";
    
    // Note: UPDATE, DELETE, SET might not be keywords in Alpha phase
    auto tokens = tokenizeWithInfo(sql);
    
    // At minimum, should tokenize without errors
    for (const auto& tok : tokens) {
        EXPECT_NE(tok.type, TokenType::ERROR);
    }
}

// ===== Complex SQL Features =====

TEST_F(LexerIntegrationTest, SubqueriesAndCTEs) {
    const char* sql = R"(
        WITH high_value_customers AS (
            SELECT customer_id, SUM(total_amount) as total_spent
            FROM orders
            GROUP BY customer_id
            HAVING SUM(total_amount) > 1000
        )
        SELECT u.username, h.total_spent
        FROM users u
        JOIN high_value_customers h ON u.id = h.customer_id
        ORDER BY h.total_spent DESC
        LIMIT 10;
    )";
    
    auto tokens = tokenizeWithInfo(sql);
    
    // Should handle all tokens even if some keywords aren't recognized
    EXPECT_GT(tokens.size(), 50); // Complex query should have many tokens
    
    // Verify parentheses are balanced
    int parenDepth = 0;
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::LEFT_PAREN) parenDepth++;
        if (tok.type == TokenType::RIGHT_PAREN) parenDepth--;
        EXPECT_GE(parenDepth, 0) << "Unmatched right parenthesis";
    }
    EXPECT_EQ(parenDepth, 0) << "Unmatched left parenthesis";
}

// ===== SQL Dialect Variations =====

TEST_F(LexerIntegrationTest, MySQLStyleQuotes) {
    // MySQL uses backticks for identifiers, we should reject
    const char* sql = "SELECT `column_name` FROM `table_name`";
    
    SimpleErrorReporter reporter;
    Lexer lexer(sql);
    lexer.setErrorReporter(&reporter);
    
    auto tokens = tokenizeWithInfo(sql);
    
    // Should either error on backticks or treat as invalid
    EXPECT_TRUE(reporter.hasErrors() || 
                std::any_of(tokens.begin(), tokens.end(), 
                           [](const TokenInfo& t) { return t.type == TokenType::ERROR; }));
}

TEST_F(LexerIntegrationTest, PostgreSQLArraysAndCasts) {
    // PostgreSQL specific syntax that we might not support
    const char* sql = R"(
        SELECT ARRAY[1,2,3] as numbers;
        SELECT '123'::INTEGER as num;
        SELECT column_name FROM table_name WHERE id = ANY($1);
    )";
    
    auto tokens = tokenizeWithInfo(sql);
    
    // Should at least tokenize basic parts correctly
    EXPECT_EQ(tokens[0].type, TokenType::KW_SELECT);
    
    // Array syntax should tokenize as identifier + brackets
    auto arrayPos = std::find_if(tokens.begin(), tokens.end(),
                               [](const TokenInfo& t) { 
                                   return t.type == TokenType::IDENTIFIER && t.text == "ARRAY"; 
                               });
    EXPECT_NE(arrayPos, tokens.end());
}

// ===== Real-World SQL Examples =====

TEST_F(LexerIntegrationTest, EcommerceQueries) {
    const char* sql = R"(
        -- Find top selling products
        SELECT 
            p.product_name,
            p.price,
            COUNT(oi.product_id) as times_ordered,
            SUM(oi.quantity) as total_quantity
        FROM products p
        JOIN order_items oi ON p.id = oi.product_id
        JOIN orders o ON oi.order_id = o.id
        WHERE o.order_date >= '2024-01-01'
          AND o.status = 'completed'
        GROUP BY p.id, p.product_name, p.price
        HAVING COUNT(oi.product_id) > 10
        ORDER BY total_quantity DESC
        LIMIT 20;
    )";
    
    auto tokens = tokenizeWithInfo(sql);
    
    // Verify comment is handled
    EXPECT_EQ(tokens[0].type, TokenType::KW_SELECT);
    EXPECT_EQ(tokens[0].line, 3); // After comment line
    
    // Count aliases (as keyword)
    int asCount = 0;
    for (size_t i = 0; i < tokens.size(); i++) {
        if (tokens[i].type == TokenType::IDENTIFIER && tokens[i].text == "as") {
            asCount++;
        }
    }
    EXPECT_EQ(asCount, 2); // 'as times_ordered' and 'as total_quantity'
}

TEST_F(LexerIntegrationTest, AnalyticsQuery) {
    const char* sql = R"(
        SELECT 
            DATE_TRUNC('month', created_at) as month,
            COUNT(*) as user_count,
            COUNT(CASE WHEN status = 'active' THEN 1 END) as active_users,
            ROUND(
                COUNT(CASE WHEN status = 'active' THEN 1 END) * 100.0 / COUNT(*), 
                2
            ) as active_percentage
        FROM users
        WHERE created_at >= '2023-01-01'
        GROUP BY DATE_TRUNC('month', created_at)
        ORDER BY month;
    )";
    
    auto tokens = tokenizeWithInfo(sql);
    
    // Verify nested function calls parse correctly
    int functionCallDepth = 0;
    int maxDepth = 0;
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::LEFT_PAREN) {
            functionCallDepth++;
            maxDepth = std::max(maxDepth, functionCallDepth);
        }
        if (tok.type == TokenType::RIGHT_PAREN) {
            functionCallDepth--;
        }
    }
    
    EXPECT_GE(maxDepth, 2); // Nested function calls
    EXPECT_EQ(functionCallDepth, 0); // Balanced parentheses
}

// ===== Error Recovery Tests =====

TEST_F(LexerIntegrationTest, RecoverFromErrors) {
    const char* sql = R"(
        SELECT * FROM users WHERE name = 'unclosed string
        SELECT * FROM orders;  -- This should still parse
        
        INSERT INTO @invalid_table VALUES (1, 2, 3);
        INSERT INTO valid_table VALUES (4, 5, 6);  -- Should parse
    )";
    
    SimpleErrorReporter reporter;
    Lexer lexer(sql);
    lexer.setErrorReporter(&reporter);
    
    auto tokens = tokenizeWithInfo(sql);
    
    // Should have errors but still find valid tokens
    EXPECT_TRUE(reporter.hasErrors());
    
    // Count successful SELECT and INSERT tokens
    int selectCount = 0;
    int insertCount = 0;
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::KW_SELECT) selectCount++;
        if (tok.type == TokenType::KW_INSERT) insertCount++;
    }
    
    EXPECT_GE(selectCount, 1); // At least one SELECT should be found
    EXPECT_GE(insertCount, 1); // At least one INSERT should be found
}

// ===== Performance with Real SQL Files =====

TEST_F(LexerIntegrationTest, LargeSchemaDefinition) {
    // Generate a large schema with many tables
    std::stringstream schema;
    
    for (int i = 0; i < 100; i++) {
        schema << "CREATE TABLE table_" << i << " (\n";
        schema << "    id INTEGER NOT NULL,\n";
        
        for (int j = 0; j < 20; j++) {
            schema << "    column_" << j << " VARCHAR(255)";
            if (j < 19) schema << ",";
            schema << "\n";
        }
        
        schema << ");\n\n";
    }
    
    std::string sql = schema.str();
    
    auto start = std::chrono::high_resolution_clock::now();
    auto tokens = tokenizeWithInfo(sql);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_GT(tokens.size(), 10000); // Many tokens
    EXPECT_LT(duration.count(), 1000) << "Should tokenize large schema quickly";
    
    // Verify all CREATE keywords found
    int createCount = 0;
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::KW_CREATE) createCount++;
    }
    EXPECT_EQ(createCount, 100);
}

// ===== SQL Standard Compliance =====

TEST_F(LexerIntegrationTest, CaseSensitivity) {
    // SQL keywords should be case-insensitive
    const char* sql = R"(
        select * from users;
        SELECT * FROM users;
        SeLeCt * FrOm users;
        CREATE table USERS (ID integer);
    )";
    
    auto tokens = tokenizeWithInfo(sql);
    
    int selectCount = 0;
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::KW_SELECT) selectCount++;
    }
    
    EXPECT_EQ(selectCount, 3);
}

TEST_F(LexerIntegrationTest, ReservedWords) {
    // Test that reserved words are recognized even in different contexts
    const char* sql = R"(
        SELECT "SELECT" FROM "FROM" WHERE "WHERE" = 'CREATE';
        SELECT * FROM table_name WHERE column_from = 'value';
    )";
    
    auto tokens = tokenizeWithInfo(sql);
    
    // First SELECT should be keyword
    EXPECT_EQ(tokens[0].type, TokenType::KW_SELECT);
    
    // "SELECT" in quotes should be parsed (as string if we support quoted identifiers)
    // or error if we don't support double quotes
}

// ===== Multi-Statement Scripts =====

TEST_F(LexerIntegrationTest, TransactionScript) {
    const char* sql = R"(
        BEGIN TRANSACTION;
        
        UPDATE accounts 
        SET balance = balance - 100.00 
        WHERE account_id = 1001;
        
        UPDATE accounts 
        SET balance = balance + 100.00 
        WHERE account_id = 2001;
        
        INSERT INTO transaction_log (from_account, to_account, amount, timestamp)
        VALUES (1001, 2001, 100.00, CURRENT_TIMESTAMP);
        
        COMMIT;
    )";
    
    auto tokens = tokenizeWithInfo(sql);
    
    // Count semicolons (statement terminators)
    int semicolonCount = 0;
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::SEMICOLON) semicolonCount++;
    }
    
    EXPECT_EQ(semicolonCount, 5); // 5 statements
    
    // Verify float literals parsed correctly
    int floatCount = 0;
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::FLOAT_LITERAL) {
            floatCount++;
            // Should be 100.00
            EXPECT_DOUBLE_EQ(tok.text.find("100.0"), 0);
        }
    }
    EXPECT_GT(floatCount, 0);
}

// ===== Special SQL Constructs =====

TEST_F(LexerIntegrationTest, NullHandling) {
    const char* sql = R"(
        SELECT * FROM users WHERE email IS NULL;
        SELECT * FROM users WHERE email IS NOT NULL;
        INSERT INTO users VALUES (1, NULL, NULL);
        UPDATE users SET email = NULL WHERE id = 1;
    )";
    
    auto tokens = tokenizeWithInfo(sql);
    
    // Count NULL keywords
    int nullCount = 0;
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::KW_NULL) nullCount++;
    }
    
    EXPECT_EQ(nullCount, 4);
}

TEST_F(LexerIntegrationTest, ComparisonOperators) {
    const char* sql = R"(
        SELECT * FROM products 
        WHERE price > 10 
          AND price <= 100 
          AND discount <> 0
          AND status = 'active'
          AND rating >= 4.5;
    )";
    
    verifyTokenSequence(sql, {
        TokenType::KW_SELECT, TokenType::STAR, TokenType::KW_FROM, TokenType::IDENTIFIER,
        TokenType::KW_WHERE, TokenType::IDENTIFIER, TokenType::GREATER_THAN, TokenType::INTEGER_LITERAL,
        TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::LESS_EQUAL, TokenType::INTEGER_LITERAL,
        TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::NOT_EQUAL, TokenType::INTEGER_LITERAL,
        TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::EQUAL, TokenType::STRING_LITERAL,
        TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::GREATER_EQUAL, TokenType::FLOAT_LITERAL,
        TokenType::SEMICOLON
    });
}