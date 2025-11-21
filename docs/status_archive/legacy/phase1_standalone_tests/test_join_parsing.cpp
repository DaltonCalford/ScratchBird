#include "scratchbird/parser/lexer.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/ast.h"
#include <iostream>
#include <string>
#include <sstream>

using namespace scratchbird;

void testJoinParsing(const std::string &sql, const std::string &test_name)
{
    std::cout << "\nTesting: " << test_name << "\n";
    std::cout << "SQL: " << sql << "\n";

    parser::ASTArena arena;
    parser::Lexer lexer(sql);
    parser::Parser parser(lexer, arena);

    auto result = parser.parseStatement();

    if (!result.success())
    {
        std::cout << "  FAILED: Parse errors:\n";
        for (const auto &error : result.errors())
        {
            std::cout << "    " << error.message << "\n";
        }
        return;
    }

    std::cout << "  Parsed successfully\n";

    // Check AST node type
    if (result.statement()->kind() != parser::ASTKind::SELECT)
    {
        std::cout << "  FAILED: Expected SELECT node\n";
        return;
    }

    auto *select_stmt = static_cast<parser::SelectStmt*>(result.statement());

    // Print AST using ASTPrinter
    std::ostringstream oss;
    parser::ASTPrinter printer(oss, lexer.stringPool());
    select_stmt->accept(&printer);

    std::cout << "  AST: " << oss.str() << "\n";
    std::cout << "  Has JOINs: " << (select_stmt->hasJoins() ? "yes" : "no") << "\n";

    if (select_stmt->hasJoins())
    {
        std::cout << "  Number of JOINs: " << select_stmt->fromClause().joins.size() << "\n";
    }

    std::cout << "  PASSED\n";
}

int main()
{
    std::cout << "=== JOIN Parsing Tests ===\n";

    // Test 1: Simple INNER JOIN
    testJoinParsing(
        "SELECT * FROM users INNER JOIN orders ON users.id = orders.user_id;",
        "Simple INNER JOIN with ON"
    );

    // Test 2: Multiple JOINs
    testJoinParsing(
        "SELECT * FROM users JOIN orders ON users.id = orders.user_id JOIN products ON orders.product_id = products.id;",
        "Multiple INNER JOINs"
    );

    // Test 3: LEFT OUTER JOIN
    testJoinParsing(
        "SELECT * FROM users LEFT OUTER JOIN orders ON users.id = orders.user_id;",
        "LEFT OUTER JOIN"
    );

    // Test 4: RIGHT JOIN
    testJoinParsing(
        "SELECT * FROM users RIGHT JOIN orders ON users.id = orders.user_id;",
        "RIGHT JOIN"
    );

    // Test 5: FULL OUTER JOIN
    testJoinParsing(
        "SELECT * FROM users FULL OUTER JOIN orders ON users.id = orders.user_id;",
        "FULL OUTER JOIN"
    );

    // Test 6: CROSS JOIN
    testJoinParsing(
        "SELECT * FROM users CROSS JOIN orders;",
        "CROSS JOIN"
    );

    // Test 7: JOIN with USING clause
    testJoinParsing(
        "SELECT * FROM users JOIN orders USING (user_id);",
        "JOIN with USING clause"
    );

    // Test 8: JOIN with table aliases
    testJoinParsing(
        "SELECT * FROM users u JOIN orders o ON u.id = o.user_id;",
        "JOIN with table aliases"
    );

    // Test 9: NATURAL JOIN
    testJoinParsing(
        "SELECT * FROM users NATURAL JOIN orders;",
        "NATURAL JOIN"
    );

    // Test 10: No JOIN (backwards compatibility)
    testJoinParsing(
        "SELECT * FROM users WHERE id = 1;",
        "Simple SELECT without JOIN (backwards compat)"
    );

    std::cout << "\n=== All Tests Complete ===\n";
    return 0;
}
