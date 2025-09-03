#include <gtest/gtest.h>
#include "scratchbird/parser/parser.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include <sstream>

using namespace scratchbird::parser;
using namespace scratchbird::sblr;

class BytecodeGeneratorTest : public ::testing::Test {
protected:
    std::string generateAndDisassemble(const std::string& sql) {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);
        
        auto parse_result = parser.parseStatement();
        
        if (!parse_result.success()) {
            std::stringstream ss;
            ss << "Parse errors:\n";
            for (const auto& err : parse_result.errors()) {
                ss << "  " << err.message << "\n";
            }
            return ss.str();
        }
        
        BytecodeGenerator generator(parser.stringPool());
        auto bytecode_result = generator.generate(parse_result.statement());
        
        if (!bytecode_result.success()) {
            std::stringstream ss;
            ss << "Bytecode generation errors:\n";
            for (const auto& err : bytecode_result.errors()) {
                ss << "  " << err << "\n";
            }
            return ss.str();
        }
        
        return BytecodeDisassembler::disassemble(bytecode_result.bytecode());
    }
    
    std::vector<uint8_t> generateBytecode(const std::string& sql) {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);
        
        auto parse_result = parser.parseStatement();
        if (!parse_result.success()) {
            return {};
        }
        
        BytecodeGenerator generator(parser.stringPool());
        auto bytecode_result = generator.generate(parse_result.statement());
        
        return bytecode_result.bytecode();
    }
};

// ===== CREATE TABLE Tests =====

TEST_F(BytecodeGeneratorTest, CreateTableSimple) {
    std::string sql = "CREATE TABLE users (id INTEGER NOT NULL)";
    std::string disasm = generateAndDisassemble(sql);
    
    EXPECT_NE(disasm.find("VERSION 1"), std::string::npos);
    EXPECT_NE(disasm.find("CREATE_TABLE"), std::string::npos);
    EXPECT_NE(disasm.find("TABLE_REF \"users\""), std::string::npos);
    EXPECT_NE(disasm.find("COLUMN_DEF"), std::string::npos);
    EXPECT_NE(disasm.find("TYPE_INTEGER"), std::string::npos);
    EXPECT_NE(disasm.find("NOT_NULL"), std::string::npos);
    EXPECT_NE(disasm.find("END"), std::string::npos);
}

TEST_F(BytecodeGeneratorTest, CreateTableMultipleColumns) {
    std::string sql = "CREATE TABLE products ("
                     "id BIGINT NOT NULL, "
                     "name VARCHAR(100), "
                     "price DOUBLE"
                     ")";
    std::string disasm = generateAndDisassemble(sql);
    
    EXPECT_NE(disasm.find("BEGIN_LIST count=3"), std::string::npos);
    EXPECT_NE(disasm.find("TYPE_BIGINT"), std::string::npos);
    EXPECT_NE(disasm.find("TYPE_VARCHAR (100)"), std::string::npos);
    EXPECT_NE(disasm.find("TYPE_DOUBLE"), std::string::npos);
}

// ===== INSERT Tests =====

TEST_F(BytecodeGeneratorTest, InsertSimple) {
    std::string sql = "INSERT INTO users (id, name) VALUES (1, 'John')";
    std::string disasm = generateAndDisassemble(sql);
    
    EXPECT_NE(disasm.find("INSERT"), std::string::npos);
    EXPECT_NE(disasm.find("TABLE_REF \"users\""), std::string::npos);
    EXPECT_NE(disasm.find("COLUMN_REF \"id\""), std::string::npos);
    EXPECT_NE(disasm.find("COLUMN_REF \"name\""), std::string::npos);
    EXPECT_NE(disasm.find("LITERAL_INT64 1"), std::string::npos);
    EXPECT_NE(disasm.find("LITERAL_STRING \"John\""), std::string::npos);
}

TEST_F(BytecodeGeneratorTest, InsertNull) {
    std::string sql = "INSERT INTO users (id, name) VALUES (1, NULL)";
    std::string disasm = generateAndDisassemble(sql);
    
    EXPECT_NE(disasm.find("LITERAL_NULL"), std::string::npos);
}

TEST_F(BytecodeGeneratorTest, InsertExpressions) {
    std::string sql = "INSERT INTO calc (result) VALUES (10 + 20 * 3)";
    std::string disasm = generateAndDisassemble(sql);
    
    // Should see: 10, 20, 3, MULTIPLY, ADD (postfix notation)
    EXPECT_NE(disasm.find("LITERAL_INT64 10"), std::string::npos);
    EXPECT_NE(disasm.find("LITERAL_INT64 20"), std::string::npos);
    EXPECT_NE(disasm.find("LITERAL_INT64 3"), std::string::npos);
    EXPECT_NE(disasm.find("EXPR_MULTIPLY"), std::string::npos);
    EXPECT_NE(disasm.find("EXPR_ADD"), std::string::npos);
}

// ===== SELECT Tests =====

TEST_F(BytecodeGeneratorTest, SelectStar) {
    std::string sql = "SELECT * FROM users";
    std::string disasm = generateAndDisassemble(sql);
    
    EXPECT_NE(disasm.find("SELECT"), std::string::npos);
    EXPECT_NE(disasm.find("SELECT_STAR"), std::string::npos);
    EXPECT_NE(disasm.find("TABLE_REF \"users\""), std::string::npos);
}

TEST_F(BytecodeGeneratorTest, SelectColumns) {
    std::string sql = "SELECT id, name FROM users";
    std::string disasm = generateAndDisassemble(sql);
    
    EXPECT_NE(disasm.find("SELECT"), std::string::npos);
    EXPECT_NE(disasm.find("COLUMN_REF \"id\""), std::string::npos);
    EXPECT_NE(disasm.find("COLUMN_REF \"name\""), std::string::npos);
}

TEST_F(BytecodeGeneratorTest, SelectWhere) {
    std::string sql = "SELECT * FROM users WHERE id > 10";
    std::string disasm = generateAndDisassemble(sql);
    
    EXPECT_NE(disasm.find("WHERE_CLAUSE"), std::string::npos);
    EXPECT_NE(disasm.find("COLUMN_REF \"id\""), std::string::npos);
    EXPECT_NE(disasm.find("LITERAL_INT64 10"), std::string::npos);
    EXPECT_NE(disasm.find("EXPR_GT"), std::string::npos);
}

TEST_F(BytecodeGeneratorTest, SelectComplexExpression) {
    std::string sql = "SELECT price * 1.1 FROM products WHERE price >= 100.0";
    std::string disasm = generateAndDisassemble(sql);
    
    EXPECT_NE(disasm.find("COLUMN_REF \"price\""), std::string::npos);
    EXPECT_NE(disasm.find("LITERAL_DOUBLE 1.1"), std::string::npos);
    EXPECT_NE(disasm.find("EXPR_MULTIPLY"), std::string::npos);
    EXPECT_NE(disasm.find("LITERAL_DOUBLE 100"), std::string::npos);
    EXPECT_NE(disasm.find("EXPR_GE"), std::string::npos);
}

// ===== Bytecode Structure Tests =====

TEST_F(BytecodeGeneratorTest, BytecodeStructure) {
    std::string sql = "CREATE TABLE t1 (id INTEGER)";
    auto bytecode = generateBytecode(sql);
    
    ASSERT_GE(bytecode.size(), 3u);
    
    // Check version header
    EXPECT_EQ(bytecode[0], static_cast<uint8_t>(Opcode::VERSION));
    EXPECT_EQ(bytecode[1], SBLR_VERSION);
    
    // Check END marker
    EXPECT_EQ(bytecode.back(), static_cast<uint8_t>(Opcode::END));
}

TEST_F(BytecodeGeneratorTest, ListCounting) {
    std::string sql = "INSERT INTO t1 (a, b, c) VALUES (1, 2, 3)";
    std::string disasm = generateAndDisassemble(sql);
    
    // Should have two lists with count=3
    size_t count = 0;
    size_t pos = 0;
    while ((pos = disasm.find("BEGIN_LIST count=3", pos)) != std::string::npos) {
        count++;
        pos++;
    }
    EXPECT_EQ(count, 2u); // One for columns, one for values
}

// ===== Error Handling Tests =====

TEST_F(BytecodeGeneratorTest, EmptySQL) {
    auto bytecode = generateBytecode("");
    EXPECT_TRUE(bytecode.empty());
}

TEST_F(BytecodeGeneratorTest, InvalidSQL) {
    auto bytecode = generateBytecode("INVALID SQL");
    EXPECT_TRUE(bytecode.empty());
}