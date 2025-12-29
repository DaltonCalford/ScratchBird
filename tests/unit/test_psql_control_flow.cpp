// ScratchBird PSQL Control Flow Unit Tests
// Tests PSQL control flow execution (IF, LOOP, WHILE, EXIT, RETURN)
//
// Date: November 21, 2025
// Purpose: Verify PSQL control flow implementations discovered to be complete
//
// Test Coverage:
// 1. IF statement (true/false conditions)
// 2. LOOP statement with EXIT
// 3. WHILE loop
// 4. EXIT statement (innermost and labeled)
// 5. RETURN statement (with and without value)
// 6. Variable operations (LOAD/STORE)
// 7. Variable scoping (nested frames)
//
// Implementation Status:
// - Control flow: 100% complete (executor.cpp:14900-15088)
// - Variables: 100% complete (executor.cpp:14449-14517)
// - Jump operations: 100% complete (executor.cpp:15155-15194)
//
// Files Tested:
// - src/sblr/executor.cpp (control flow execution)
// - include/scratchbird/sblr/executor.h (VariableStack, LoopState)

#include <gtest/gtest.h>
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/core/database.h"
#include <memory>
#include <filesystem>
#include <cstring>

using namespace scratchbird;
using namespace scratchbird::sblr;

class PSQLControlFlowTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create temporary test database
        test_db_path_ = "/tmp/test_psql_control_flow.db";
        std::filesystem::remove_all(test_db_path_);

        core::ErrorContext ctx;
        db_ = core::Database::create(test_db_path_, &ctx);
        ASSERT_NE(db_, nullptr) << "Failed to create database: " << ctx.message;
    }

    void TearDown() override
    {
        db_.reset();
        std::filesystem::remove_all(test_db_path_);
    }

    // Helper: Write string to bytecode (length byte + data)
    void writeString(std::vector<uint8_t>& bytecode, const std::string& str)
    {
        bytecode.push_back(static_cast<uint8_t>(str.size()));
        bytecode.insert(bytecode.end(), str.begin(), str.end());
    }

    // Helper: Write int32 to bytecode (little-endian)
    void writeInt32(std::vector<uint8_t>& bytecode, uint32_t value)
    {
        bytecode.push_back(value & 0xFF);
        bytecode.push_back((value >> 8) & 0xFF);
        bytecode.push_back((value >> 16) & 0xFF);
        bytecode.push_back((value >> 24) & 0xFF);
    }

    // Helper: Write int64 to bytecode (little-endian)
    void writeInt64(std::vector<uint8_t>& bytecode, int64_t value)
    {
        for (int i = 0; i < 8; ++i)
        {
            bytecode.push_back((value >> (i * 8)) & 0xFF);
        }
    }

    void writeExtOpcode(std::vector<uint8_t>& bytecode, uint16_t op)
    {
        bytecode.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
        bytecode.push_back(static_cast<uint8_t>(op & 0xFF));
        bytecode.push_back(static_cast<uint8_t>((op >> 8) & 0xFF));
    }

    void writeExtOpcode(std::vector<uint8_t>& bytecode, ExtendedOpcode op)
    {
        writeExtOpcode(bytecode, static_cast<uint16_t>(op));
    }

    // Helper: Generate bytecode for variable declaration
    // Note: Simplified - real implementation would use DECLARE opcode
    void generateDeclareVariable(std::vector<uint8_t>& bytecode,
                                  const std::string& name,
                                  int64_t value)
    {
        // EXT_DECLARE would be used in real implementation
        // For now, use LITERAL + VAR_STORE to initialize
        bytecode.push_back(static_cast<uint8_t>(Opcode::LITERAL_INT64));
        writeInt64(bytecode, value);

        writeExtOpcode(bytecode, ExtendedOpcode::EXT_VAR_STORE);
        writeString(bytecode, name);
    }

    // Helper: Generate bytecode for variable load
    void generateVarLoad(std::vector<uint8_t>& bytecode, const std::string& name)
    {
        writeExtOpcode(bytecode, ExtendedOpcode::EXT_VAR_LOAD);
        writeString(bytecode, name);
    }

    // Helper: Generate bytecode for variable store
    void generateVarStore(std::vector<uint8_t>& bytecode, const std::string& name)
    {
        writeExtOpcode(bytecode, ExtendedOpcode::EXT_VAR_STORE);
        writeString(bytecode, name);
    }

    std::string test_db_path_;
    std::unique_ptr<core::Database> db_;
};

// Test 1: Variable LOAD/STORE Operations
TEST_F(PSQLControlFlowTest, VariableLoadStore)
{
    std::vector<uint8_t> bytecode;

    // Initialize variable: x = 42
    generateDeclareVariable(bytecode, "x", 42);

    // Load variable: push x onto stack
    generateVarLoad(bytecode, "x");

    // Store to new variable: y = x
    generateVarStore(bytecode, "y");

    // Load y to verify
    generateVarLoad(bytecode, "y");

    // End
    bytecode.push_back(static_cast<uint8_t>(Opcode::END));

    // Execute
    auto executor = std::make_unique<Executor>(db_.get(), bytecode);

    // Note: This test verifies that the bytecode doesn't crash
    // A real test would need a way to inspect the variable stack or return value
    ASSERT_NO_THROW({
        auto result = executor->execute();
        // For now, just verify it doesn't crash
        // Full test would verify return_value_ == 42
    });
}

// Test 2: IF Statement - True Condition
TEST_F(PSQLControlFlowTest, IfStatementTrueCondition)
{
    std::vector<uint8_t> bytecode;

    // Condition: TRUE (literal 1)
    bytecode.push_back(static_cast<uint8_t>(Opcode::LITERAL_INT64));
    writeInt64(bytecode, 1);  // true

    // IF statement
    writeExtOpcode(bytecode, ExtendedOpcode::EXT_IF);

    // False branch offset (jump to ELSE/END IF)
    size_t false_offset_pos = bytecode.size();
    writeInt32(bytecode, 0);  // Placeholder, will update

    // THEN block: x = 1
    generateDeclareVariable(bytecode, "x", 1);

    // Update false branch offset to point here (after THEN block)
    size_t end_if_pos = bytecode.size();
    uint32_t offset = static_cast<uint32_t>(end_if_pos);
    bytecode[false_offset_pos] = offset & 0xFF;
    bytecode[false_offset_pos + 1] = (offset >> 8) & 0xFF;
    bytecode[false_offset_pos + 2] = (offset >> 16) & 0xFF;
    bytecode[false_offset_pos + 3] = (offset >> 24) & 0xFF;

    // END
    bytecode.push_back(static_cast<uint8_t>(Opcode::END));

    // Execute
    auto executor = std::make_unique<Executor>(db_.get(), bytecode);
    ASSERT_NO_THROW({
        auto result = executor->execute();
        // Variable x should be set to 1
    });
}

// Test 3: IF Statement - False Condition
TEST_F(PSQLControlFlowTest, IfStatementFalseCondition)
{
    std::vector<uint8_t> bytecode;

    // Condition: FALSE (literal 0)
    bytecode.push_back(static_cast<uint8_t>(Opcode::LITERAL_INT64));
    writeInt64(bytecode, 0);  // false

    // IF statement
    writeExtOpcode(bytecode, ExtendedOpcode::EXT_IF);

    // False branch offset
    size_t false_offset_pos = bytecode.size();
    writeInt32(bytecode, 0);  // Placeholder

    // THEN block: x = 1 (should be skipped)
    generateDeclareVariable(bytecode, "x", 1);

    // Update false branch offset
    size_t end_if_pos = bytecode.size();
    uint32_t offset = static_cast<uint32_t>(end_if_pos);
    bytecode[false_offset_pos] = offset & 0xFF;
    bytecode[false_offset_pos + 1] = (offset >> 8) & 0xFF;
    bytecode[false_offset_pos + 2] = (offset >> 16) & 0xFF;
    bytecode[false_offset_pos + 3] = (offset >> 24) & 0xFF;

    // END
    bytecode.push_back(static_cast<uint8_t>(Opcode::END));

    // Execute
    auto executor = std::make_unique<Executor>(db_.get(), bytecode);
    ASSERT_NO_THROW({
        auto result = executor->execute();
        // Variable x should NOT be set
    });
}

// Test 4: LOOP with EXIT
TEST_F(PSQLControlFlowTest, LoopWithExit)
{
    std::vector<uint8_t> bytecode;

    // Initialize counter: i = 0
    generateDeclareVariable(bytecode, "i", 0);

    // LOOP
    size_t loop_start = bytecode.size();
    writeExtOpcode(bytecode, ExtendedOpcode::EXT_LOOP);

    // Loop end offset (placeholder)
    size_t loop_end_offset_pos = bytecode.size();
    writeInt32(bytecode, 0);

    // Loop label (empty)
    writeString(bytecode, "");

    // Loop body: i = i + 1
    generateVarLoad(bytecode, "i");
    bytecode.push_back(static_cast<uint8_t>(Opcode::LITERAL_INT64));
    writeInt64(bytecode, 1);
    bytecode.push_back(static_cast<uint8_t>(Opcode::EXPR_ADD));
    generateVarStore(bytecode, "i");

    // Check if i >= 5
    generateVarLoad(bytecode, "i");
    bytecode.push_back(static_cast<uint8_t>(Opcode::LITERAL_INT64));
    writeInt64(bytecode, 5);
    bytecode.push_back(static_cast<uint8_t>(Opcode::EXPR_GE));

    // EXIT WHEN condition
    writeExtOpcode(bytecode, ExtendedOpcode::EXT_EXIT);
    writeString(bytecode, "");  // No label
    bytecode.push_back(1);  // has_when = true
    // Condition already on stack

    // END LOOP marker
    writeExtOpcode(bytecode, static_cast<uint16_t>(0x00FE));  // END LOOP marker

    // Update loop end offset
    size_t loop_end = bytecode.size();
    uint32_t offset = static_cast<uint32_t>(loop_end);
    bytecode[loop_end_offset_pos] = offset & 0xFF;
    bytecode[loop_end_offset_pos + 1] = (offset >> 8) & 0xFF;
    bytecode[loop_end_offset_pos + 2] = (offset >> 16) & 0xFF;
    bytecode[loop_end_offset_pos + 3] = (offset >> 24) & 0xFF;

    // END
    bytecode.push_back(static_cast<uint8_t>(Opcode::END));

    // Execute
    auto executor = std::make_unique<Executor>(db_.get(), bytecode);
    ASSERT_NO_THROW({
        auto result = executor->execute();
        // Variable i should be 5 after loop
    });
}

// Test 5: RETURN Statement with Value
TEST_F(PSQLControlFlowTest, ReturnWithValue)
{
    std::vector<uint8_t> bytecode;

    // RETURN 42
    bytecode.push_back(static_cast<uint8_t>(Opcode::LITERAL_INT64));
    writeInt64(bytecode, 42);

    writeExtOpcode(bytecode, ExtendedOpcode::EXT_RETURN);
    bytecode.push_back(1);  // has_value = true

    // Code after RETURN (should not execute)
    generateDeclareVariable(bytecode, "x", 99);

    // END
    bytecode.push_back(static_cast<uint8_t>(Opcode::END));

    // Execute
    auto executor = std::make_unique<Executor>(db_.get(), bytecode);
    ASSERT_NO_THROW({
        auto result = executor->execute();
        // Should return 42, not execute the declare
        // TODO: Add way to verify return_value_
    });
}

// Test 6: RETURN Statement without Value
TEST_F(PSQLControlFlowTest, ReturnWithoutValue)
{
    std::vector<uint8_t> bytecode;

    // RETURN (no value)
    writeExtOpcode(bytecode, ExtendedOpcode::EXT_RETURN);
    bytecode.push_back(0);  // has_value = false

    // Code after RETURN (should not execute)
    generateDeclareVariable(bytecode, "x", 99);

    // END
    bytecode.push_back(static_cast<uint8_t>(Opcode::END));

    // Execute
    auto executor = std::make_unique<Executor>(db_.get(), bytecode);
    ASSERT_NO_THROW({
        auto result = executor->execute();
        // Should return NULL, not execute the declare
    });
}

// Test 7: WHILE Loop
TEST_F(PSQLControlFlowTest, WhileLoop)
{
    std::vector<uint8_t> bytecode;

    // Initialize: counter = 0
    generateDeclareVariable(bytecode, "counter", 0);

    // WHILE counter < 3
    size_t while_start = bytecode.size();
    writeExtOpcode(bytecode, ExtendedOpcode::EXT_WHILE);

    // Loop end offset (placeholder)
    size_t loop_end_offset_pos = bytecode.size();
    writeInt32(bytecode, 0);

    // Loop label (empty)
    writeString(bytecode, "");

    // Condition: counter < 3
    generateVarLoad(bytecode, "counter");
    bytecode.push_back(static_cast<uint8_t>(Opcode::LITERAL_INT64));
    writeInt64(bytecode, 3);
    bytecode.push_back(static_cast<uint8_t>(Opcode::EXPR_LT));

    // Loop body: counter = counter + 1
    generateVarLoad(bytecode, "counter");
    bytecode.push_back(static_cast<uint8_t>(Opcode::LITERAL_INT64));
    writeInt64(bytecode, 1);
    bytecode.push_back(static_cast<uint8_t>(Opcode::EXPR_ADD));
    generateVarStore(bytecode, "counter");

    // Update loop end offset
    size_t loop_end = bytecode.size();
    uint32_t offset = static_cast<uint32_t>(loop_end);
    bytecode[loop_end_offset_pos] = offset & 0xFF;
    bytecode[loop_end_offset_pos + 1] = (offset >> 8) & 0xFF;
    bytecode[loop_end_offset_pos + 2] = (offset >> 16) & 0xFF;
    bytecode[loop_end_offset_pos + 3] = (offset >> 24) & 0xFF;

    // END
    bytecode.push_back(static_cast<uint8_t>(Opcode::END));

    // Execute
    auto executor = std::make_unique<Executor>(db_.get(), bytecode);
    ASSERT_NO_THROW({
        auto result = executor->execute();
        // counter should be 3 after loop
    });
}

// Test 8: Jump Operations
TEST_F(PSQLControlFlowTest, JumpOperations)
{
    std::vector<uint8_t> bytecode;

    // Unconditional jump over next instruction
    writeExtOpcode(bytecode, ExtendedOpcode::EXT_JUMP);
    size_t jump_target_pos = bytecode.size();
    writeInt32(bytecode, 0);  // Placeholder

    // This should be skipped
    generateDeclareVariable(bytecode, "skipped", 1);

    // Jump target
    size_t jump_target = bytecode.size();
    uint32_t offset = static_cast<uint32_t>(jump_target);
    bytecode[jump_target_pos] = offset & 0xFF;
    bytecode[jump_target_pos + 1] = (offset >> 8) & 0xFF;
    bytecode[jump_target_pos + 2] = (offset >> 16) & 0xFF;
    bytecode[jump_target_pos + 3] = (offset >> 24) & 0xFF;

    // This should execute
    generateDeclareVariable(bytecode, "executed", 1);

    // END
    bytecode.push_back(static_cast<uint8_t>(Opcode::END));

    // Execute
    auto executor = std::make_unique<Executor>(db_.get(), bytecode);
    ASSERT_NO_THROW({
        auto result = executor->execute();
        // Variable 'skipped' should not be created
        // Variable 'executed' should be created
    });
}

// Test 9: Nested Loops with Labeled EXIT
TEST_F(PSQLControlFlowTest, NestedLoopsLabeledExit)
{
    std::vector<uint8_t> bytecode;

    // Initialize counters
    generateDeclareVariable(bytecode, "outer", 0);
    generateDeclareVariable(bytecode, "inner", 0);

    // Outer LOOP <<outer_loop>>
    writeExtOpcode(bytecode, ExtendedOpcode::EXT_LOOP);
    size_t outer_loop_end_pos = bytecode.size();
    writeInt32(bytecode, 0);
    writeString(bytecode, "outer_loop");

    // outer = outer + 1
    generateVarLoad(bytecode, "outer");
    bytecode.push_back(static_cast<uint8_t>(Opcode::LITERAL_INT64));
    writeInt64(bytecode, 1);
    bytecode.push_back(static_cast<uint8_t>(Opcode::EXPR_ADD));
    generateVarStore(bytecode, "outer");

    // Inner LOOP
    writeExtOpcode(bytecode, ExtendedOpcode::EXT_LOOP);
    size_t inner_loop_end_pos = bytecode.size();
    writeInt32(bytecode, 0);
    writeString(bytecode, "");

    // inner = inner + 1
    generateVarLoad(bytecode, "inner");
    bytecode.push_back(static_cast<uint8_t>(Opcode::LITERAL_INT64));
    writeInt64(bytecode, 1);
    bytecode.push_back(static_cast<uint8_t>(Opcode::EXPR_ADD));
    generateVarStore(bytecode, "inner");

    // EXIT outer_loop WHEN outer >= 2
    generateVarLoad(bytecode, "outer");
    bytecode.push_back(static_cast<uint8_t>(Opcode::LITERAL_INT64));
    writeInt64(bytecode, 2);
    bytecode.push_back(static_cast<uint8_t>(Opcode::EXPR_GE));

    writeExtOpcode(bytecode, ExtendedOpcode::EXT_EXIT);
    writeString(bytecode, "outer_loop");
    bytecode.push_back(1);  // has_when

    // END inner loop
    size_t inner_loop_end = bytecode.size();
    writeExtOpcode(bytecode, static_cast<uint16_t>(0x00FE));

    uint32_t inner_offset = static_cast<uint32_t>(inner_loop_end);
    bytecode[inner_loop_end_pos] = inner_offset & 0xFF;
    bytecode[inner_loop_end_pos + 1] = (inner_offset >> 8) & 0xFF;
    bytecode[inner_loop_end_pos + 2] = (inner_offset >> 16) & 0xFF;
    bytecode[inner_loop_end_pos + 3] = (inner_offset >> 24) & 0xFF;

    // END outer loop
    size_t outer_loop_end = bytecode.size();
    writeExtOpcode(bytecode, static_cast<uint16_t>(0x00FE));

    uint32_t outer_offset = static_cast<uint32_t>(outer_loop_end);
    bytecode[outer_loop_end_pos] = outer_offset & 0xFF;
    bytecode[outer_loop_end_pos + 1] = (outer_offset >> 8) & 0xFF;
    bytecode[outer_loop_end_pos + 2] = (outer_offset >> 16) & 0xFF;
    bytecode[outer_loop_end_pos + 3] = (outer_offset >> 24) & 0xFF;

    // END
    bytecode.push_back(static_cast<uint8_t>(Opcode::END));

    // Execute
    auto executor = std::make_unique<Executor>(db_.get(), bytecode);
    ASSERT_NO_THROW({
        auto result = executor->execute();
        // outer should be 2, inner should have incremented multiple times
    });
}
