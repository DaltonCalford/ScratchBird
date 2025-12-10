

#include "scratchbird/sblr/query_compiler_v2.h"
#include <iostream>
#include <vector>

using namespace scratchbird;
using namespace scratchbird::parser;
using namespace scratchbird::sblr;

bool testILIKEOperator() {
    std::cout << "=== Test 1: ILIKE Operator ===\n";

    std::string sql = "SELECT * FROM users WHERE name ILIKE '%john%'";
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

    std::cout << "  ✓ ILIKE operator parsed successfully\n";
    return true;
}

bool testRegexOperators() {
    std::cout << "\n=== Test 2: Regex Operators (~, ~*, !~, !~*) ===\n";

    struct TestCase {
        std::string sql;
        std::string name;
    };

    std::vector<TestCase> tests = {
        {"SELECT * FROM users WHERE email ~ '^[a-z]+@example\\.com$'", "~ (case-sensitive match)"},
        {"SELECT * FROM products WHERE name ~* 'laptop'", "~* (case-insensitive match)"},
        {"SELECT * FROM logs WHERE message !~ 'ERROR'", "!~ (case-sensitive not match)"},
        {"SELECT * FROM data WHERE value !~* 'test'", "!~* (case-insensitive not match)"}
    };

    for (const auto& test : tests) {
        Lexer lexer(test.sql);
        ASTArena arena;
        Parser parser(lexer, arena);
        auto result = parser.parseStatement();

        if (!result.success()) {
            std::cout << "  FAILED: " << test.name << "\n";
            return false;
        }
        std::cout << "  ✓ " << test.name << " parsed\n";
    }

    return true;
}

bool testRegexpFunctions() {
    std::cout << "\n=== Test 3: REGEXP Functions ===\n";

    struct TestCase {
        std::string sql;
        std::string name;
    };

    std::vector<TestCase> tests = {
        {"SELECT REGEXP_MATCHES('foobarbaz', 'b..')", "REGEXP_MATCHES basic"},
        {"SELECT REGEXP_MATCHES('test123test', '[0-9]+', 'g')", "REGEXP_MATCHES with flags"},
        {"SELECT REGEXP_REPLACE('Hello World', 'World', 'Universe')", "REGEXP_REPLACE basic"},
        {"SELECT REGEXP_REPLACE('foo bar', '\\s+', '_', 'g')", "REGEXP_REPLACE global"},
        {"SELECT REGEXP_SPLIT_TO_ARRAY('a,b,c,d', ',')", "REGEXP_SPLIT_TO_ARRAY"},
        {"SELECT REGEXP_SPLIT_TO_TABLE('x:y:z', ':')", "REGEXP_SPLIT_TO_TABLE"}
    };

    for (const auto& test : tests) {
        Lexer lexer(test.sql);
        ASTArena arena;
        Parser parser(lexer, arena);
        auto result = parser.parseStatement();

        if (!result.success()) {
            std::cout << "  FAILED: " << test.name << "\n";
            for (const auto& err : result.errors()) {
                std::cout << "    " << err.message << "\n";
            }
            return false;
        }
        std::cout << "  ✓ " << test.name << " parsed\n";
    }

    return true;
}

bool testStringUtilityFunctions() {
    std::cout << "\n=== Test 4: String Utility Functions ===\n";

    struct TestCase {
        std::string sql;
        std::string name;
    };

    std::vector<TestCase> tests = {
        {"SELECT STRPOS('hello world', 'world')", "STRPOS"},
        {"SELECT POSITION('test' IN 'this is a test')", "POSITION"},
        {"SELECT SPLIT_PART('a|b|c', '|', 2)", "SPLIT_PART"},
        {"SELECT OVERLAY('hello' PLACING 'XX' FROM 2 FOR 3)", "OVERLAY"}
    };

    for (const auto& test : tests) {
        Lexer lexer(test.sql);
        ASTArena arena;
        Parser parser(lexer, arena);
        auto result = parser.parseStatement();

        if (!result.success()) {
            std::cout << "  FAILED: " << test.name << "\n";
            for (const auto& err : result.errors()) {
                std::cout << "    " << err.message << "\n";
            }
            return false;
        }
        std::cout << "  ✓ " << test.name << " parsed\n";
    }

    return true;
}

bool testCaseConversionFunctions() {
    std::cout << "\n=== Test 5: Case Conversion Functions ===\n";

    struct TestCase {
        std::string sql;
        std::string name;
    };

    std::vector<TestCase> tests = {
        {"SELECT INITCAP('hello world')", "INITCAP"},
        {"SELECT ASCII('A')", "ASCII"},
        {"SELECT CHR(65)", "CHR"},
        {"SELECT REPEAT('*', 5)", "REPEAT"},
        {"SELECT REVERSE('hello')", "REVERSE"}
    };

    for (const auto& test : tests) {
        Lexer lexer(test.sql);
        ASTArena arena;
        Parser parser(lexer, arena);
        auto result = parser.parseStatement();

        if (!result.success()) {
            std::cout << "  FAILED: " << test.name << "\n";
            for (const auto& err : result.errors()) {
                std::cout << "    " << err.message << "\n";
            }
            return false;
        }
        std::cout << "  ✓ " << test.name << " parsed\n";
    }

    return true;
}

bool testBytecodeGenerationILIKE() {
    std::cout << "\n=== Test 6: ILIKE Bytecode Generation ===\n";

    std::string sql = "SELECT * FROM users WHERE name ILIKE 'john%'";
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

    // Verify bytecode contains EXPR_ILIKE
    const auto& bc = bc_result.bytecode();
    bool found_ilike = false;

    for (size_t i = 0; i < bc.size(); i++) {
        if (bc[i] == static_cast<uint8_t>(Opcode::EXPR_ILIKE)) {
            found_ilike = true;
            break;
        }
    }

    if (!found_ilike) {
        std::cout << "  FAILED: EXPR_ILIKE opcode not found\n";
        return false;
    }

    std::cout << "  ✓ ILIKE bytecode generated successfully (" << bc.size() << " bytes)\n";
    return true;
}

bool testBytecodeGenerationRegex() {
    std::cout << "\n=== Test 7: Regex Operator Bytecode Generation ===\n";

    std::string sql = "SELECT * FROM users WHERE email ~ '@example\\.com$'";
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
        return false;
    }

    // Verify bytecode contains EXTENDED_OPCODE and EXT_REGEX_MATCH
    const auto& bc = bc_result.bytecode();
    bool found_regex = false;

    for (size_t i = 0; i < bc.size(); i++) {
        if (bc[i] == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE)) {
            if (i + 1 < bc.size() && bc[i + 1] == static_cast<uint8_t>(Opcode::EXT_REGEX_MATCH)) {
                found_regex = true;
                break;
            }
        }
    }

    if (!found_regex) {
        std::cout << "  FAILED: EXT_REGEX_MATCH opcode not found\n";
        return false;
    }

    std::cout << "  ✓ Regex operator bytecode generated successfully (" << bc.size() << " bytes)\n";
    return true;
}

bool testComplexTextSearchQuery() {
    std::cout << "\n=== Test 8: Complex Text Search Query ===\n";

    std::string sql = R"(
        SELECT
            id,
            INITCAP(name) as formatted_name,
            STRPOS(email, '@') as at_position,
            REGEXP_MATCHES(phone, '\d{3}-\d{3}-\d{4}') as phone_match
        FROM contacts
        WHERE
            name ILIKE '%smith%'
            OR email ~ '@(gmail|yahoo)\.com$'
            AND POSITION('test' IN description) > 0
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

    std::cout << "  ✓ Complex text search query parsed successfully\n";
    return true;
}

int main() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  Text Search Functions Integration Test\n";
    std::cout << "  Phase 2, Task 13 Verification\n";
    std::cout << "========================================\n\n";

    int passed = 0;
    int total = 8;

    if (testILIKEOperator()) passed++;
    if (testRegexOperators()) passed++;
    if (testRegexpFunctions()) passed++;
    if (testStringUtilityFunctions()) passed++;
    if (testCaseConversionFunctions()) passed++;
    if (testBytecodeGenerationILIKE()) passed++;
    if (testBytecodeGenerationRegex()) passed++;
    if (testComplexTextSearchQuery()) passed++;

    std::cout << "\n========================================\n";
    if (passed == total) {
        std::cout << "  ✅ ALL " << total << " TEXT SEARCH TESTS PASSED\n";
        std::cout << "  Task 13: Text Search Functions - COMPLETE\n";
        std::cout << "========================================\n\n";
        return 0;
    } else {
        std::cout << "  ❌ " << (total - passed) << " / " << total << " TESTS FAILED\n";
        std::cout << "========================================\n\n";
        return 1;
    }
}
