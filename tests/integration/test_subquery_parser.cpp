#include <iostream>
#include <sstream>
#include "scratchbird/parser/lexer.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/ast.h"

using namespace scratchbird::parser;

int main() {
    std::cout << "Testing Subquery Parser Implementation\n";
    std::cout << "========================================\n\n";

    // Test 1: Scalar subquery
    {
        std::cout << "Test 1: Scalar subquery\n";
        std::string sql = "SELECT * FROM users WHERE salary > (SELECT AVG(salary) FROM employees)";
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        try {
            auto result = parser.parseStatement();
            if (!result.success()) {
                std::cout << "  FAILED: Parse errors occurred\n";
                for (const auto& err : result.errors()) {
                    std::cout << "    Error: " << err.message << "\n";
                }
                for (const auto& err : result.errors()) {
                    std::cout << "    Error: " << err.message << "\n";
                }
            } else {
                std::cout << "  PASSED: Scalar subquery parsed successfully\n";
                std::ostringstream oss;
                ASTPrinter printer(oss, lexer.stringPool());
                result.statement()->accept(&printer);
                std::cout << "  AST: " << oss.str() << "\n";
            }
        } catch (...) {
            std::cout << "  FAILED: Exception during parsing\n";
        }
        std::cout << "\n";
    }

    // Test 2: EXISTS subquery
    {
        std::cout << "Test 2: EXISTS subquery\n";
        std::string sql = "SELECT * FROM orders WHERE EXISTS (SELECT 1 FROM order_items WHERE order_id = orders.id)";
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        try {
            auto result = parser.parseStatement();
            if (!result.success()) {
                std::cout << "  FAILED: Parse errors occurred\n";
                for (const auto& err : result.errors()) {
                    std::cout << "    Error: " << err.message << "\n";
                }
            } else {
                std::cout << "  PASSED: EXISTS subquery parsed successfully\n";
                std::ostringstream oss;
                ASTPrinter printer(oss, lexer.stringPool());
                result.statement()->accept(&printer);
                std::cout << "  AST: " << oss.str() << "\n";
            }
        } catch (...) {
            std::cout << "  FAILED: Exception during parsing\n";
        }
        std::cout << "\n";
    }

    // Test 3: IN subquery
    {
        std::cout << "Test 3: IN subquery\n";
        std::string sql = "SELECT * FROM products WHERE category_id IN (SELECT id FROM categories WHERE active = 1)";
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        try {
            auto result = parser.parseStatement();
            if (!result.success()) {
                std::cout << "  FAILED: Parse errors occurred\n";
                for (const auto& err : result.errors()) {
                    std::cout << "    Error: " << err.message << "\n";
                }
            } else {
                std::cout << "  PASSED: IN subquery parsed successfully\n";
                std::ostringstream oss;
                ASTPrinter printer(oss, lexer.stringPool());
                result.statement()->accept(&printer);
                std::cout << "  AST: " << oss.str() << "\n";
            }
        } catch (...) {
            std::cout << "  FAILED: Exception during parsing\n";
        }
        std::cout << "\n";
    }

    // Test 4: NOT IN subquery
    {
        std::cout << "Test 4: NOT IN subquery\n";
        std::string sql = "SELECT * FROM users WHERE id NOT IN (SELECT user_id FROM banned_users)";
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        try {
            auto result = parser.parseStatement();
            if (!result.success()) {
                std::cout << "  FAILED: Parse errors occurred\n";
                for (const auto& err : result.errors()) {
                    std::cout << "    Error: " << err.message << "\n";
                }
            } else {
                std::cout << "  PASSED: NOT IN subquery parsed successfully\n";
                std::ostringstream oss;
                ASTPrinter printer(oss, lexer.stringPool());
                result.statement()->accept(&printer);
                std::cout << "  AST: " << oss.str() << "\n";
            }
        } catch (...) {
            std::cout << "  FAILED: Exception during parsing\n";
        }
        std::cout << "\n";
    }

    std::cout << "========================================\n";
    std::cout << "Subquery Parser Tests Complete\n";

    return 0;
}
