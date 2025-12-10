

#include "scratchbird/sblr/query_compiler_v2.h"
#include <iostream>
#include <cassert>
#include <sstream>

using namespace scratchbird;
using namespace scratchbird::parser;
using namespace scratchbird::sblr;

bool testArrayLiteralParsing() {
    std::cout << "=== Test 1: Array Literal Parsing ===\n";

    std::string sql = "SELECT ARRAY[1, 2, 3, 4, 5]";
    Lexer lexer(sql);
    ASTArena arena;
    Parser parser(lexer, arena);

    auto result = parser.parseStatement();
    if (!result.success()) {
        std::cout << "  FAILED: Parse errors\n";
        for (const auto& err : result.errors()) {
            std::cout << "    " << err.message << "\n";
        }
        return false;
    }

    auto stmt = result.statement();
    if (stmt->kind() != ASTKind::SELECT) {
        std::cout << "  FAILED: Not a SELECT statement\n";
        return false;
    }

    auto select = static_cast<SelectStmt*>(stmt);
    if (select->selectList().size() != 1) {
        std::cout << "  FAILED: Expected 1 select item\n";
        return false;
    }

    auto select_item = select->selectList()[0];
    auto expr = select_item.expr;
    if (expr->kind() != ASTKind::LITERAL) {
        std::cout << "  FAILED: Expected literal expression\n";
        return false;
    }

    auto array_lit = static_cast<ArrayLiteral*>(expr);
    if (array_lit->elements().size() != 5) {
        std::cout << "  FAILED: Expected 5 elements, got " << array_lit->elements().size() << "\n";
        return false;
    }

    std::cout << "  ✓ ARRAY[1,2,3,4,5] parsed successfully\n";
    return true;
}

bool testNestedArrays() {
    std::cout << "\n=== Test 2: Nested Arrays ===\n";

    std::string sql = "SELECT ARRAY[ARRAY[1,2], ARRAY[3,4]]";
    Lexer lexer(sql);
    ASTArena arena;
    Parser parser(lexer, arena);

    auto result = parser.parseStatement();
    if (!result.success()) {
        std::cout << "  FAILED: Parse errors\n";
        return false;
    }

    auto stmt = result.statement();
    auto select = static_cast<SelectStmt*>(stmt);
    auto select_item = select->selectList()[0];
    auto expr = select_item.expr;

    if (expr->kind() != ASTKind::LITERAL) {
        std::cout << "  FAILED: Expected array literal\n";
        return false;
    }

    auto outer_array = static_cast<ArrayLiteral*>(expr);
    if (outer_array->elements().size() != 2) {
        std::cout << "  FAILED: Expected 2 nested arrays\n";
        return false;
    }

    if (outer_array->elements()[0]->kind() != ASTKind::LITERAL) {
        std::cout << "  FAILED: First element should be array literal\n";
        return false;
    }

    std::cout << "  ✓ Nested arrays parsed successfully\n";
    return true;
}

bool testArrayWithExpressions() {
    std::cout << "\n=== Test 3: Arrays with Expressions ===\n";

    std::string sql = "SELECT ARRAY[1 + 2, 3 * 4, 5]";
    Lexer lexer(sql);
    ASTArena arena;
    Parser parser(lexer, arena);

    auto result = parser.parseStatement();
    if (!result.success()) {
        std::cout << "  FAILED: Parse errors\n";
        return false;
    }

    auto stmt = result.statement();
    auto select = static_cast<SelectStmt*>(stmt);
    auto select_item = select->selectList()[0];
    auto expr = select_item.expr;
    auto array_lit = static_cast<ArrayLiteral*>(expr);

    if (array_lit->elements().size() != 3) {
        std::cout << "  FAILED: Expected 3 elements\n";
        return false;
    }

    // First element should be binary op (1+2)
    if (array_lit->elements()[0]->kind() != ASTKind::BINARY_OP) {
        std::cout << "  FAILED: First element should be binary operation\n";
        return false;
    }

    std::cout << "  ✓ Arrays with expressions parsed successfully\n";
    return true;
}

bool testEmptyArray() {
    std::cout << "\n=== Test 4: Empty Array ===\n";

    std::string sql = "SELECT ARRAY[]";
    Lexer lexer(sql);
    ASTArena arena;
    Parser parser(lexer, arena);

    auto result = parser.parseStatement();
    if (!result.success()) {
        std::cout << "  FAILED: Parse errors\n";
        return false;
    }

    auto stmt = result.statement();
    auto select = static_cast<SelectStmt*>(stmt);
    auto select_item = select->selectList()[0];
    auto expr = select_item.expr;
    auto array_lit = static_cast<ArrayLiteral*>(expr);

    if (array_lit->elements().size() != 0) {
        std::cout << "  FAILED: Expected empty array\n";
        return false;
    }

    std::cout << "  ✓ Empty array ARRAY[] parsed successfully\n";
    return true;
}

bool testArrayFunctions() {
    std::cout << "\n=== Test 5: Array Functions ===\n";

    struct TestCase {
        std::string sql;
        std::string name;
    };

    std::vector<TestCase> tests = {
        {"SELECT ARRAY_APPEND(ARRAY[1,2], 3)", "ARRAY_APPEND"},
        {"SELECT ARRAY_PREPEND(0, ARRAY[1,2,3])", "ARRAY_PREPEND"},
        {"SELECT ARRAY_CAT(ARRAY[1,2], ARRAY[3,4])", "ARRAY_CAT"},
        {"SELECT ARRAY_LENGTH(ARRAY[1,2,3,4,5])", "ARRAY_LENGTH"},
        {"SELECT UNNEST(ARRAY[1,2,3])", "UNNEST"}
    };

    for (const auto& test : tests) {
        Lexer lexer(test.sql);
        ASTArena arena;
        Parser parser(lexer, arena);
        auto result = parser.parseStatement();

        if (!result.success()) {
            std::cout << "  FAILED: " << test.name << " parse error\n";
            return false;
        }
        std::cout << "  ✓ " << test.name << " parsed\n";
    }

    return true;
}

bool testArrayBytecodeGeneration() {
    std::cout << "\n=== Test 6: Array Bytecode Generation ===\n";

    std::string sql = "SELECT ARRAY[1, 2, 3]";
    Lexer lexer(sql);
    ASTArena arena;
    Parser parser(lexer, arena);

    auto result = parser.parseStatement();
    if (!result.success()) {
        std::cout << "  FAILED: Parse error\n";
        return false;
    }

    BytecodeGenerator generator(lexer.stringPool());
    auto bc_result = generator.generate(result.statement());

    if (!bc_result.success()) {
        std::cout << "  FAILED: Bytecode generation failed\n";
        for (const auto& err : bc_result.errors()) {
            std::cout << "    " << err << "\n";
        }
        return false;
    }

    if (bc_result.bytecode().empty()) {
        std::cout << "  FAILED: No bytecode generated\n";
        return false;
    }

    // Verify bytecode contains EXTENDED_OPCODE and EXT_ARRAY_CONSTRUCT
    const auto& bc = bc_result.bytecode();
    bool found_extended = false;
    bool found_construct = false;

    for (size_t i = 0; i < bc.size(); i++) {
        if (bc[i] == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE)) {
            found_extended = true;
            if (i + 1 < bc.size() && bc[i + 1] == static_cast<uint8_t>(Opcode::EXT_ARRAY_CONSTRUCT)) {
                found_construct = true;
                break;
            }
        }
    }

    if (!found_extended || !found_construct) {
        std::cout << "  FAILED: Missing EXTENDED_OPCODE or EXT_ARRAY_CONSTRUCT\n";
        return false;
    }

    std::cout << "  ✓ Array bytecode generated successfully (" << bc.size() << " bytes)\n";
    return true;
}

bool testArrayWithStrings() {
    std::cout << "\n=== Test 7: Array with String Literals ===\n";

    std::string sql = "SELECT ARRAY['foo', 'bar', 'baz']";
    Lexer lexer(sql);
    ASTArena arena;
    Parser parser(lexer, arena);

    auto result = parser.parseStatement();
    if (!result.success()) {
        std::cout << "  FAILED: Parse error\n";
        return false;
    }

    auto stmt = result.statement();
    auto select = static_cast<SelectStmt*>(stmt);
    auto select_item = select->selectList()[0];
    auto expr = select_item.expr;
    auto array_lit = static_cast<ArrayLiteral*>(expr);

    if (array_lit->elements().size() != 3) {
        std::cout << "  FAILED: Expected 3 string elements\n";
        return false;
    }

    std::cout << "  ✓ String array ARRAY['foo','bar','baz'] parsed successfully\n";
    return true;
}

bool testComplexArrayQuery() {
    std::cout << "\n=== Test 8: Complex Array Query ===\n";

    std::string sql = R"(
        SELECT
            ARRAY[id, id * 2, id * 3] as multiples,
            ARRAY_LENGTH(ARRAY[1,2,3,4,5]) as len,
            ARRAY_APPEND(ARRAY[1,2], id) as with_id
        FROM users
        WHERE id > 0
    )";

    Lexer lexer(sql);
    ASTArena arena;
    Parser parser(lexer, arena);

    auto result = parser.parseStatement();
    if (!result.success()) {
        std::cout << "  FAILED: Parse error\n";
        for (const auto& err : result.errors()) {
            std::cout << "    " << err.message << "\n";
        }
        return false;
    }

    std::cout << "  ✓ Complex array query parsed successfully\n";
    return true;
}

int main() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  Array Functions SQL Integration Test\n";
    std::cout << "  Phase 2, Task 12 Verification\n";
    std::cout << "========================================\n\n";

    int passed = 0;
    int total = 8;

    if (testArrayLiteralParsing()) passed++;
    if (testNestedArrays()) passed++;
    if (testArrayWithExpressions()) passed++;
    if (testEmptyArray()) passed++;
    if (testArrayFunctions()) passed++;
    if (testArrayBytecodeGeneration()) passed++;
    if (testArrayWithStrings()) passed++;
    if (testComplexArrayQuery()) passed++;

    std::cout << "\n========================================\n";
    if (passed == total) {
        std::cout << "  ✅ ALL " << total << " ARRAY TESTS PASSED\n";
        std::cout << "  Task 12: Array Functions - COMPLETE\n";
        std::cout << "========================================\n\n";
        return 0;
    } else {
        std::cout << "  ❌ " << (total - passed) << " / " << total << " TESTS FAILED\n";
        std::cout << "========================================\n\n";
        return 1;
    }
}
