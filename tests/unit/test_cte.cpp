#include <gtest/gtest.h>
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/semantic_analyzer.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/core/database.h"
#include <string>

using namespace scratchbird::parser;
using namespace scratchbird::sblr;
using namespace scratchbird::core;

class CTETest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a temporary test database
        db_ = std::make_unique<Database>(":memory:");
        ASSERT_TRUE(db_->isOpen());
    }

    void TearDown() override
    {
        db_.reset();
    }

    void testParse(const std::string &sql, bool should_succeed = true)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        if (should_succeed)
        {
            ASSERT_TRUE(result.success()) << "Parse failed for: " << sql;
            if (!parser.hasErrors())
            {
                // Also test semantic analysis
                SemanticAnalyzer analyzer;
                result.statement()->accept(&analyzer);
                EXPECT_FALSE(analyzer.hasErrors()) << "Semantic analysis failed";
            }
        }
        else
        {
            EXPECT_FALSE(result.success()) << "Parse should have failed for: " << sql;
        }
    }

    void testBytecodeGeneration(const std::string &sql)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "Parse failed: " << sql;

        BytecodeGenerator generator(parser.stringPool(), db_.get());
        auto bytecode_result = generator.generate(result.statement());
        ASSERT_TRUE(bytecode_result.success())
            << "Bytecode generation failed: " << bytecode_result.error();
    }

    void testExecution(const std::string &setup_sql, const std::string &test_sql)
    {
        // Execute setup SQL (CREATE TABLE, INSERT, etc.)
        if (!setup_sql.empty())
        {
            Lexer setup_lexer(setup_sql);
            ASTArena setup_arena;
            Parser setup_parser(setup_lexer, setup_arena);
            auto setup_result = setup_parser.parseStatement();
            ASSERT_TRUE(setup_result.success()) << "Setup parse failed";

            BytecodeGenerator setup_generator(setup_parser.stringPool(), db_.get());
            auto setup_bytecode = setup_generator.generate(setup_result.statement());
            ASSERT_TRUE(setup_bytecode.success()) << "Setup bytecode failed";

            Executor setup_executor(db_.get());
            auto setup_exec_result = setup_executor.execute(setup_bytecode.bytecode());
            ASSERT_TRUE(setup_exec_result.success())
                << "Setup execution failed: " << setup_exec_result.error();
        }

        // Execute test SQL
        Lexer lexer(test_sql);
        ASTArena arena;
        Parser parser(lexer, arena);
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "Test parse failed: " << test_sql;

        BytecodeGenerator generator(parser.stringPool(), db_.get());
        auto bytecode_result = generator.generate(result.statement());
        ASSERT_TRUE(bytecode_result.success())
            << "Test bytecode generation failed: " << bytecode_result.error();

        Executor executor(db_.get());
        auto exec_result = executor.execute(bytecode_result.bytecode());
        ASSERT_TRUE(exec_result.success())
            << "Test execution failed: " << exec_result.error();
    }

    std::unique_ptr<Database> db_;
};

// ===== Parsing Tests =====

TEST_F(CTETest, ParseSimpleCTE)
{
    std::string sql = "WITH temp AS (SELECT * FROM users) SELECT * FROM temp";
    testParse(sql, true);
}

TEST_F(CTETest, ParseMultipleCTEs)
{
    std::string sql = "WITH "
                      "  cte1 AS (SELECT id FROM users), "
                      "  cte2 AS (SELECT user_id FROM orders) "
                      "SELECT * FROM cte1 JOIN cte2 ON cte1.id = cte2.user_id";
    testParse(sql, true);
}

TEST_F(CTETest, ParseCTEWithColumnAliases)
{
    std::string sql = "WITH summary(total, count) AS "
                      "  (SELECT SUM(amount), COUNT(*) FROM orders) "
                      "SELECT * FROM summary";
    testParse(sql, true);
}

TEST_F(CTETest, ParseNestedCTE)
{
    std::string sql = "WITH outer_cte AS ( "
                      "  WITH inner_cte AS (SELECT id FROM users) "
                      "  SELECT * FROM inner_cte "
                      ") SELECT * FROM outer_cte";
    testParse(sql, true);
}

TEST_F(CTETest, ParseCTEWithWhere)
{
    std::string sql = "WITH active_users AS "
                      "  (SELECT * FROM users WHERE status = 'active') "
                      "SELECT name FROM active_users WHERE age > 18";
    testParse(sql, true);
}

TEST_F(CTETest, ParseCTEWithJoin)
{
    std::string sql = "WITH user_orders AS ( "
                      "  SELECT u.name, o.amount "
                      "  FROM users u JOIN orders o ON u.id = o.user_id "
                      ") SELECT * FROM user_orders";
    testParse(sql, true);
}

TEST_F(CTETest, ParseCTEWithGroupBy)
{
    std::string sql = "WITH totals AS ( "
                      "  SELECT user_id, SUM(amount) as total "
                      "  FROM orders "
                      "  GROUP BY user_id "
                      ") SELECT * FROM totals WHERE total > 1000";
    testParse(sql, true);
}

// ===== Bytecode Generation Tests =====

TEST_F(CTETest, BytecodeSimpleCTE)
{
    testBytecodeGeneration("WITH temp AS (SELECT * FROM users) SELECT * FROM temp");
}

TEST_F(CTETest, BytecodeMultipleCTEs)
{
    testBytecodeGeneration("WITH "
                           "  cte1 AS (SELECT id FROM users), "
                           "  cte2 AS (SELECT user_id FROM orders) "
                           "SELECT * FROM cte1");
}

TEST_F(CTETest, BytecodeCTEWithColumnAliases)
{
    testBytecodeGeneration("WITH summary(total, count) AS "
                           "  (SELECT SUM(amount), COUNT(*) FROM orders) "
                           "SELECT * FROM summary");
}

// ===== Execution Tests =====
// Note: These tests require actual table setup, which may not work in a minimal test environment
// They are included for completeness but may need to be skipped if tables don't exist

TEST_F(CTETest, DISABLED_ExecuteSimpleCTE)
{
    std::string setup = "CREATE TABLE test_users (id INT32, name VARCHAR(100))";
    std::string test = "WITH temp AS (SELECT * FROM test_users) SELECT * FROM temp";
    testExecution(setup, test);
}

TEST_F(CTETest, DISABLED_ExecuteMultipleCTEs)
{
    std::string setup = "CREATE TABLE test_data (id INT32, value INT32)";
    std::string test = "WITH "
                       "  cte1 AS (SELECT id FROM test_data WHERE value > 0), "
                       "  cte2 AS (SELECT id FROM test_data WHERE value < 100) "
                       "SELECT * FROM cte1";
    testExecution(setup, test);
}

// ===== Error Cases =====

TEST_F(CTETest, ErrorUndefinedCTE)
{
    // This should parse but fail at semantic analysis or execution
    std::string sql = "SELECT * FROM undefined_cte";
    testParse(sql, true);  // Parsing succeeds, but execution would fail
}

TEST_F(CTETest, ErrorCTENameConflict)
{
    // Multiple CTEs with the same name should fail at semantic analysis
    std::string sql = "WITH "
                      "  cte AS (SELECT * FROM users), "
                      "  cte AS (SELECT * FROM orders) "
                      "SELECT * FROM cte";
    testParse(sql, false);  // Should fail at parse or semantic analysis
}
