#include <gtest/gtest.h>
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/semantic_analyzer.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include <iostream>

using namespace scratchbird::parser;
using namespace scratchbird::sblr;

class SQLToBytecodeTest : public ::testing::Test
{
protected:
    void testFullPipeline(const std::string &sql, bool skip_semantic = false)
    {
        std::cout << "\n=== SQL: " << sql << " ===\n";

        // 1. Parse
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto parse_result = parser.parseStatement();
        ASSERT_TRUE(parse_result.success()) << "Parse failed";

        // 2. Semantic Analysis (optional - requires table definitions)
        if (!skip_semantic)
        {
            SemanticAnalyzer analyzer(parser.stringPool());
            auto semantic_result = analyzer.analyze(parse_result.statement());
            ASSERT_TRUE(semantic_result.success()) << "Semantic analysis failed";
        }

        // 3. Generate bytecode
        BytecodeGenerator generator(parser.stringPool());
        auto bytecode_result = generator.generate(parse_result.statement());
        ASSERT_TRUE(bytecode_result.success()) << "Bytecode generation failed";

        // 4. Show disassembly
        std::string disasm = BytecodeDisassembler::disassemble(bytecode_result.bytecode());
        std::cout << "Bytecode:\n" << disasm << std::endl;

        // Basic validation
        EXPECT_GT(bytecode_result.bytecode().size(), 3u);
        EXPECT_EQ(bytecode_result.bytecode()[0], static_cast<uint8_t>(Opcode::VERSION));
        EXPECT_EQ(bytecode_result.bytecode().back(), static_cast<uint8_t>(Opcode::END));
    }
};

TEST_F(SQLToBytecodeTest, CreateTable)
{
    testFullPipeline("CREATE TABLE users ("
                     "id INTEGER NOT NULL, "
                     "name VARCHAR(50), "
                     "email VARCHAR(100) NOT NULL"
                     ")",
                     true); // Skip semantic analysis
}

TEST_F(SQLToBytecodeTest, Insert)
{
    testFullPipeline("INSERT INTO users (id, name, email) "
                     "VALUES (1, 'John Doe', 'john@example.com')",
                     true);
}

TEST_F(SQLToBytecodeTest, Select)
{
    testFullPipeline("SELECT id, name FROM users WHERE id > 100", true);
}

TEST_F(SQLToBytecodeTest, ComplexExpression)
{
    testFullPipeline("SELECT price * 1.1 + 5 FROM products WHERE price >= 50.0", true);
}

// Example showing complete workflow
TEST_F(SQLToBytecodeTest, CompleteExample)
{
    // This would be the full workflow in practice
    std::vector<std::string> statements = {
        "CREATE TABLE orders (id BIGINT NOT NULL, total DOUBLE, status VARCHAR(20))",
        "INSERT INTO orders (id, total, status) VALUES (1001, 299.99, 'pending')",
        "SELECT * FROM orders WHERE total > 100.0"};

    for (const auto &sql : statements)
    {
        testFullPipeline(sql, true); // Skip semantic analysis
    }
}

// ===== Spatial SQL Integration Tests (Phase 2 Task 9.1) =====

TEST_F(SQLToBytecodeTest, SpatialFunction_ST_Point)
{
    testFullPipeline("SELECT ST_POINT(1.5, 2.5) FROM locations", true);
}

TEST_F(SQLToBytecodeTest, SpatialFunction_ST_AsText)
{
    testFullPipeline("SELECT ST_ASTEXT(ST_POINT(1.0, 2.0)) FROM locations", true);
}

TEST_F(SQLToBytecodeTest, SpatialFunction_ST_GeometryType)
{
    testFullPipeline("SELECT ST_GEOMETRYTYPE(ST_POINT(0.0, 0.0)) FROM locations", true);
}

TEST_F(SQLToBytecodeTest, SpatialFunction_ST_IsValid)
{
    testFullPipeline("SELECT ST_ISVALID(ST_POINT(1.0, 2.0)) FROM locations", true);
}

TEST_F(SQLToBytecodeTest, SpatialFunction_ST_MakeLine)
{
    testFullPipeline("SELECT ST_MAKELINE(ST_POINT(0.0, 0.0), ST_POINT(1.0, 1.0)) FROM routes", true);
}

TEST_F(SQLToBytecodeTest, SpatialFunction_ST_MakePolygon)
{
    testFullPipeline("SELECT ST_MAKEPOLYGON(ST_MAKELINE(ST_POINT(0.0, 0.0), ST_POINT(1.0, 0.0))) FROM regions", true);
}

TEST_F(SQLToBytecodeTest, SpatialFunction_ST_AsBinary)
{
    testFullPipeline("SELECT ST_ASBINARY(ST_POINT(5.5, 10.5)) FROM locations", true);
}

TEST_F(SQLToBytecodeTest, SpatialComplexQuery)
{
    // Test query with nested spatial functions
    // Uses ST_ASTEXT on a ST_POINT, similar to how other spatial tests work
    testFullPipeline(
        "SELECT ST_ASTEXT(ST_POINT(5.0, 10.0)) FROM locations",
        true);
}