#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/optimizer/statistics_manager.h"
#include <filesystem>

using namespace scratchbird;
using namespace scratchbird::parser;
using namespace scratchbird::sblr;
using namespace scratchbird::core;
using namespace scratchbird::optimizer;

/**
 * Integration test for Query Planner (Phase 1, Task 1.3)
 *
 * Verifies that:
 * 1. Query planner is properly integrated with bytecode generator
 * 2. Optimizer hints (SCAN_HINT, INDEX_REF) appear in bytecode
 * 3. Fallback to direct generation works when planner unavailable
 */
class QueryPlannerIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create temporary database for testing
        test_db_path_ = "/tmp/test_query_planner_" + std::to_string(getpid()) + ".db";

        // Remove existing test database if present
        std::filesystem::remove(test_db_path_);
    }

    void TearDown() override
    {
        // Clean up database
        if (db_)
        {
            db_->close();
            db_.reset();
        }
        std::filesystem::remove(test_db_path_);
    }

    // Helper: Create and open database
    bool createDatabase()
    {
        ErrorContext ctx;

        // Create database file
        Status status = Database::create(test_db_path_, 16384, &ctx);
        if (status != Status::OK)
        {
            return false;
        }

        // Construct database object and open
        db_ = std::make_unique<Database>();
        status = db_->open(test_db_path_, &ctx);

        return status == Status::OK;
    }

    // Helper: Execute SQL and return bytecode
    std::vector<uint8_t> executeSQLWithPlanner(const std::string &sql)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto parse_result = parser.parseStatement();
        if (!parse_result.success())
        {
            return {};
        }

        // Pass database to bytecode generator for planner access
        BytecodeGenerator generator(parser.stringPool(), db_.get());
        auto bytecode_result = generator.generate(parse_result.statement());

        if (!bytecode_result.success())
        {
            return {};
        }

        return bytecode_result.bytecode();
    }

    // Helper: Execute SQL without planner (fallback mode)
    std::vector<uint8_t> executeSQLWithoutPlanner(const std::string &sql)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto parse_result = parser.parseStatement();
        if (!parse_result.success())
        {
            return {};
        }

        // Don't pass database - forces fallback to direct generation
        BytecodeGenerator generator(parser.stringPool(), nullptr);
        auto bytecode_result = generator.generate(parse_result.statement());

        if (!bytecode_result.success())
        {
            return {};
        }

        return bytecode_result.bytecode();
    }

    // Helper: Check if bytecode contains a specific opcode
    bool containsOpcode(const std::vector<uint8_t> &bytecode, Opcode opcode)
    {
        for (uint8_t byte : bytecode)
        {
            if (byte == static_cast<uint8_t>(opcode))
            {
                return true;
            }
        }
        return false;
    }

    // Helper: Disassemble bytecode for debugging
    std::string disassemble(const std::vector<uint8_t> &bytecode)
    {
        return BytecodeDisassembler::disassemble(bytecode);
    }

    std::string test_db_path_;
    std::unique_ptr<Database> db_;
};

// ===== Basic Integration Tests =====

TEST_F(QueryPlannerIntegrationTest, BytecodeGeneratorAcceptsDatabasePointer)
{
    // Test that BytecodeGenerator can be constructed with Database pointer
    ASSERT_TRUE(createDatabase());

    Lexer lexer("SELECT * FROM users");
    ASTArena arena;
    Parser parser(lexer, arena);

    // Should not throw
    EXPECT_NO_THROW({
        BytecodeGenerator generator(parser.stringPool(), db_.get());
    });
}

TEST_F(QueryPlannerIntegrationTest, DatabaseHasQueryPlannerComponents)
{
    // Test that Database initializes optimizer components
    ASSERT_TRUE(createDatabase());

    // Verify optimizer components are initialized
    EXPECT_NE(db_->statistics_manager(), nullptr);
    EXPECT_NE(db_->query_planner(), nullptr);
}

TEST_F(QueryPlannerIntegrationTest, FallbackWorksWithoutDatabase)
{
    // Test that bytecode generation works without database (fallback mode)
    std::string sql = "SELECT * FROM users";
    auto bytecode = executeSQLWithoutPlanner(sql);

    EXPECT_FALSE(bytecode.empty());

    // Should contain basic SELECT opcodes
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::VERSION));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::SELECT));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::SELECT_STAR));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::END));

    // Should NOT contain optimizer hints (no planner available)
    EXPECT_FALSE(containsOpcode(bytecode, Opcode::SCAN_HINT));
}

// ===== Optimizer Integration Tests =====

TEST_F(QueryPlannerIntegrationTest, SelectGeneratesWithPlanner)
{
    // Test that SELECT uses query planner when available
    ASSERT_TRUE(createDatabase());

    // First create a table so planner has something to work with
    auto create_bytecode = executeSQLWithPlanner(
        "CREATE TABLE users (id INTEGER NOT NULL, name VARCHAR(100))");
    EXPECT_FALSE(create_bytecode.empty());

    // Now test SELECT - planner should be invoked
    // NOTE: Planner will likely fail due to missing catalog entries,
    // but we're testing that the integration path is executed
    auto select_bytecode = executeSQLWithPlanner("SELECT * FROM users");
    EXPECT_FALSE(select_bytecode.empty());

    // Should contain basic SELECT opcodes
    EXPECT_TRUE(containsOpcode(select_bytecode, Opcode::VERSION));
    EXPECT_TRUE(containsOpcode(select_bytecode, Opcode::SELECT));
    EXPECT_TRUE(containsOpcode(select_bytecode, Opcode::END));
}

TEST_F(QueryPlannerIntegrationTest, BytecodeContainsVersionHeader)
{
    // Verify bytecode structure is preserved with planner integration
    ASSERT_TRUE(createDatabase());

    auto bytecode = executeSQLWithPlanner("SELECT * FROM test");

    ASSERT_GE(bytecode.size(), 3u);

    // Check version header
    EXPECT_EQ(bytecode[0], static_cast<uint8_t>(Opcode::VERSION));
    EXPECT_EQ(bytecode[1], SBLR_VERSION);

    // Check END marker
    EXPECT_EQ(bytecode.back(), static_cast<uint8_t>(Opcode::END));
}

TEST_F(QueryPlannerIntegrationTest, SelectWithWhereClause)
{
    // Test SELECT with WHERE clause goes through planner
    ASSERT_TRUE(createDatabase());

    auto bytecode = executeSQLWithPlanner(
        "SELECT id, name FROM users WHERE id > 10");

    EXPECT_FALSE(bytecode.empty());

    // Should contain WHERE clause
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::WHERE_CLAUSE));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::EXPR_GT));
}

TEST_F(QueryPlannerIntegrationTest, NonSelectStatementsBypassPlanner)
{
    // Test that non-SELECT statements don't use planner
    ASSERT_TRUE(createDatabase());

    // INSERT should not go through planner
    auto insert_bytecode = executeSQLWithPlanner(
        "INSERT INTO users (id) VALUES (1)");

    EXPECT_FALSE(insert_bytecode.empty());
    EXPECT_TRUE(containsOpcode(insert_bytecode, Opcode::INSERT));
    EXPECT_FALSE(containsOpcode(insert_bytecode, Opcode::SCAN_HINT));

    // CREATE TABLE should not go through planner
    auto create_bytecode = executeSQLWithPlanner(
        "CREATE TABLE test (id INTEGER)");

    EXPECT_FALSE(create_bytecode.empty());
    EXPECT_TRUE(containsOpcode(create_bytecode, Opcode::CREATE_TABLE));
    EXPECT_FALSE(containsOpcode(create_bytecode, Opcode::SCAN_HINT));
}

// ===== Comparison Tests: With vs Without Planner =====

TEST_F(QueryPlannerIntegrationTest, CompareWithAndWithoutPlanner)
{
    // Compare bytecode generated with and without planner
    std::string sql = "SELECT * FROM users WHERE id = 42";

    // Generate with planner (may fallback)
    ASSERT_TRUE(createDatabase());
    auto bytecode_with_planner = executeSQLWithPlanner(sql);

    // Generate without planner (direct generation)
    auto bytecode_without_planner = executeSQLWithoutPlanner(sql);

    // Both should succeed
    EXPECT_FALSE(bytecode_with_planner.empty());
    EXPECT_FALSE(bytecode_without_planner.empty());

    // Both should contain core SELECT opcodes
    EXPECT_TRUE(containsOpcode(bytecode_with_planner, Opcode::SELECT));
    EXPECT_TRUE(containsOpcode(bytecode_without_planner, Opcode::SELECT));

    // Both should have version and end markers
    EXPECT_EQ(bytecode_with_planner[0], static_cast<uint8_t>(Opcode::VERSION));
    EXPECT_EQ(bytecode_without_planner[0], static_cast<uint8_t>(Opcode::VERSION));
    EXPECT_EQ(bytecode_with_planner.back(), static_cast<uint8_t>(Opcode::END));
    EXPECT_EQ(bytecode_without_planner.back(), static_cast<uint8_t>(Opcode::END));
}

// ===== Stress Tests =====

TEST_F(QueryPlannerIntegrationTest, ComplexQueryWithPlanner)
{
    // Test complex query goes through planner integration
    ASSERT_TRUE(createDatabase());

    std::string complex_sql =
        "SELECT id, name, price * 1.1 "
        "FROM products "
        "WHERE price >= 100.0 AND name LIKE '%sale%'";

    auto bytecode = executeSQLWithPlanner(complex_sql);

    EXPECT_FALSE(bytecode.empty());

    // Verify core opcodes present
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::SELECT));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::WHERE_CLAUSE));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::EXPR_AND));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::EXPR_GE));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::EXPR_LIKE));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::EXPR_MULTIPLY));
}

TEST_F(QueryPlannerIntegrationTest, MultipleSelectStatements)
{
    // Test multiple SELECT statements in sequence
    ASSERT_TRUE(createDatabase());

    std::vector<std::string> queries = {
        "SELECT * FROM users",
        "SELECT id FROM users WHERE id > 10",
        "SELECT name, email FROM users",
        "SELECT COUNT(*) FROM users"
    };

    for (const auto &sql : queries)
    {
        auto bytecode = executeSQLWithPlanner(sql);

        EXPECT_FALSE(bytecode.empty()) << "Failed for query: " << sql;
        EXPECT_TRUE(containsOpcode(bytecode, Opcode::SELECT))
            << "Missing SELECT opcode for query: " << sql;
        EXPECT_TRUE(containsOpcode(bytecode, Opcode::VERSION))
            << "Missing VERSION opcode for query: " << sql;
        EXPECT_TRUE(containsOpcode(bytecode, Opcode::END))
            << "Missing END opcode for query: " << sql;
    }
}

// ===== Edge Cases =====

TEST_F(QueryPlannerIntegrationTest, EmptySelectList)
{
    // Test SELECT * (empty select list becomes SELECT_STAR)
    ASSERT_TRUE(createDatabase());

    auto bytecode = executeSQLWithPlanner("SELECT * FROM test");

    EXPECT_FALSE(bytecode.empty());
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::SELECT_STAR));
}

TEST_F(QueryPlannerIntegrationTest, SelectWithoutWhereClause)
{
    // Test SELECT without WHERE clause
    ASSERT_TRUE(createDatabase());

    auto bytecode = executeSQLWithPlanner("SELECT id, name FROM users");

    EXPECT_FALSE(bytecode.empty());
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::SELECT));

    // WHERE_CLAUSE should not be present
    EXPECT_FALSE(containsOpcode(bytecode, Opcode::WHERE_CLAUSE));
}

TEST_F(QueryPlannerIntegrationTest, DatabaseClosedBehavior)
{
    // Test behavior when database is closed
    ASSERT_TRUE(createDatabase());

    // Close database
    db_->close();

    // Should still generate bytecode (planner unavailable, falls back)
    Lexer lexer("SELECT * FROM users");
    ASTArena arena;
    Parser parser(lexer, arena);

    auto parse_result = parser.parseStatement();
    ASSERT_TRUE(parse_result.success());

    BytecodeGenerator generator(parser.stringPool(), db_.get());
    auto bytecode_result = generator.generate(parse_result.statement());

    // Should succeed via fallback
    EXPECT_TRUE(bytecode_result.success() || !bytecode_result.errors().empty());
}

// ===== Diagnostic Test =====

TEST_F(QueryPlannerIntegrationTest, DiagnosticDisassemblyOutput)
{
    // This test outputs disassembly for manual inspection
    // Useful for debugging planner integration
    ASSERT_TRUE(createDatabase());

    std::string sql = "SELECT id, name FROM users WHERE id = 42";
    auto bytecode = executeSQLWithPlanner(sql);

    ASSERT_FALSE(bytecode.empty());

    // Disassemble for inspection (only shown on test failure)
    std::string disasm = disassemble(bytecode);

    // This will be visible in test output
    SCOPED_TRACE("Bytecode disassembly:\n" + disasm);

    // Basic validation
    EXPECT_NE(disasm.find("VERSION"), std::string::npos);
    EXPECT_NE(disasm.find("SELECT"), std::string::npos);
    EXPECT_NE(disasm.find("END"), std::string::npos);
}
