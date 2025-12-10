// Quick integration test for Wave 1 features
#include <iostream>

#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/executor.h"

using namespace scratchbird::parser;
using namespace scratchbird::sblr;

int main() {
    int passed = 0;
    int total = 0;

    // Test 1: Spatial SQL - ST_POINT
    {
        total++;
        std::string sql = "SELECT ST_POINT(1.5, 2.5) FROM locations";
        std::cout << "\nTest 1: " << sql << std::endl;

        try {
            Lexer lexer(sql);
            ASTArena arena;
            Parser parser(lexer, arena);
            auto result = parser.parseStatement();

            if (result.success()) {
                BytecodeGenerator generator(parser.stringPool());
                auto bytecode_result = generator.generate(result.statement());

                if (bytecode_result.success()) {
                    std::cout << "✅ PASS: Spatial ST_POINT parses and generates bytecode" << std::endl;
                    passed++;
                } else {
                    std::cout << "❌ FAIL: Bytecode generation failed" << std::endl;
                }
            } else {
                std::cout << "❌ FAIL: Parse failed" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "❌ FAIL: Exception: " << e.what() << std::endl;
        }
    }

    // Test 2: Array literal
    {
        total++;
        std::string sql = "SELECT ARRAY[1, 2, 3] FROM table1";
        std::cout << "\nTest 2: " << sql << std::endl;

        try {
            Lexer lexer(sql);
            ASTArena arena;
            Parser parser(lexer, arena);
            auto result = parser.parseStatement();

            if (result.success()) {
                BytecodeGenerator generator(parser.stringPool());
                auto bytecode_result = generator.generate(result.statement());

                if (bytecode_result.success()) {
                    std::cout << "✅ PASS: Array literal parses and generates bytecode" << std::endl;
                    passed++;
                } else {
                    std::cout << "❌ FAIL: Bytecode generation failed" << std::endl;
                }
            } else {
                std::cout << "❌ FAIL: Parse failed" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "❌ FAIL: Exception: " << e.what() << std::endl;
        }
    }

    // Test 3: Regex operator ~
    {
        total++;
        std::string sql = "SELECT * FROM logs WHERE message ~ 'ERROR'";
        std::cout << "\nTest 3: " << sql << std::endl;

        try {
            Lexer lexer(sql);
            ASTArena arena;
            Parser parser(lexer, arena);
            auto result = parser.parseStatement();

            if (result.success()) {
                BytecodeGenerator generator(parser.stringPool());
                auto bytecode_result = generator.generate(result.statement());

                if (bytecode_result.success()) {
                    std::cout << "✅ PASS: Regex operator ~ parses and generates bytecode" << std::endl;
                    passed++;
                } else {
                    std::cout << "❌ FAIL: Bytecode generation failed" << std::endl;
                }
            } else {
                std::cout << "❌ FAIL: Parse failed" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "❌ FAIL: Exception: " << e.what() << std::endl;
        }
    }

    // Test 4: Text function INITCAP
    {
        total++;
        std::string sql = "SELECT INITCAP(name) FROM users";
        std::cout << "\nTest 4: " << sql << std::endl;

        try {
            Lexer lexer(sql);
            ASTArena arena;
            Parser parser(lexer, arena);
            auto result = parser.parseStatement();

            if (result.success()) {
                BytecodeGenerator generator(parser.stringPool());
                auto bytecode_result = generator.generate(result.statement());

                if (bytecode_result.success()) {
                    std::cout << "✅ PASS: INITCAP function parses and generates bytecode" << std::endl;
                    passed++;
                } else {
                    std::cout << "❌ FAIL: Bytecode generation failed" << std::endl;
                }
            } else {
                std::cout << "❌ FAIL: Parse failed" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "❌ FAIL: Exception: " << e.what() << std::endl;
        }
    }

    // Test 5: Array operator &&
    {
        total++;
        std::string sql = "SELECT * FROM table1 WHERE tags && ARRAY['a', 'b']";
        std::cout << "\nTest 5: " << sql << std::endl;

        try {
            Lexer lexer(sql);
            ASTArena arena;
            Parser parser(lexer, arena);
            auto result = parser.parseStatement();

            if (result.success()) {
                BytecodeGenerator generator(parser.stringPool());
                auto bytecode_result = generator.generate(result.statement());

                if (bytecode_result.success()) {
                    std::cout << "✅ PASS: Array operator && parses and generates bytecode" << std::endl;
                    passed++;
                } else {
                    std::cout << "❌ FAIL: Bytecode generation failed" << std::endl;
                }
            } else {
                std::cout << "❌ FAIL: Parse failed" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "❌ FAIL: Exception: " << e.what() << std::endl;
        }
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "WAVE 1 INTEGRATION TEST RESULTS" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Passed: " << passed << "/" << total << std::endl;
    std::cout << "Success Rate: " << (100.0 * passed / total) << "%" << std::endl;

    return (passed == total) ? 0 : 1;
}
