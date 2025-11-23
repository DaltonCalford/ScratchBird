#include "scratchbird/parser/lexer.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/ast.h"
#include <iostream>
#include <string>

using namespace scratchbird;

void testUpdateStatement()
{
    std::cout << "Testing UPDATE statement parsing...\n";

    std::string sql = "UPDATE users SET name = 'John', age = 25 WHERE id = 1;";

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
    if (result.statement()->kind() != parser::ASTKind::UPDATE)
    {
        std::cout << "  FAILED: Expected UPDATE node, got kind="
                  << static_cast<int>(result.statement()->kind()) << "\n";
        return;
    }

    auto *update_stmt = static_cast<parser::UpdateStmt*>(result.statement());
    std::cout << "  Table: " << lexer.stringPool().get(update_stmt->tableName()) << "\n";
    std::cout << "  Assignments: " << update_stmt->assignments().size() << "\n";

    for (size_t i = 0; i < update_stmt->assignments().size(); i++)
    {
        const auto &assign = update_stmt->assignments()[i];
        std::cout << "    " << i << ". "
                  << lexer.stringPool().get(assign.column_name) << " = <expr>\n";
    }

    std::cout << "  Has WHERE: " << (update_stmt->whereClause() ? "yes" : "no") << "\n";
    std::cout << "  PASSED\n";
}

void testDeleteStatement()
{
    std::cout << "\nTesting DELETE statement parsing...\n";

    std::string sql = "DELETE FROM users WHERE id = 1;";

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
    if (result.statement()->kind() != parser::ASTKind::DELETE_STMT)
    {
        std::cout << "  FAILED: Expected DELETE_STMT node, got kind="
                  << static_cast<int>(result.statement()->kind()) << "\n";
        return;
    }

    auto *delete_stmt = static_cast<parser::DeleteStmt*>(result.statement());
    std::cout << "  Table: " << lexer.stringPool().get(delete_stmt->tableName()) << "\n";
    std::cout << "  Has WHERE: " << (delete_stmt->whereClause() ? "yes" : "no") << "\n";
    std::cout << "  PASSED\n";
}

void testDeleteWithoutWhere()
{
    std::cout << "\nTesting DELETE without WHERE parsing...\n";

    std::string sql = "DELETE FROM users;";

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

    auto *delete_stmt = static_cast<parser::DeleteStmt*>(result.statement());
    std::cout << "  Table: " << lexer.stringPool().get(delete_stmt->tableName()) << "\n";
    std::cout << "  Has WHERE: " << (delete_stmt->whereClause() ? "yes" : "no") << "\n";
    std::cout << "  PASSED\n";
}

void testUpdateWithoutWhere()
{
    std::cout << "\nTesting UPDATE without WHERE parsing...\n";

    std::string sql = "UPDATE products SET price = 10.99;";

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

    auto *update_stmt = static_cast<parser::UpdateStmt*>(result.statement());
    std::cout << "  Table: " << lexer.stringPool().get(update_stmt->tableName()) << "\n";
    std::cout << "  Assignments: " << update_stmt->assignments().size() << "\n";
    std::cout << "  Has WHERE: " << (update_stmt->whereClause() ? "yes" : "no") << "\n";
    std::cout << "  PASSED\n";
}

int main()
{
    std::cout << "=== UPDATE and DELETE Statement Parser Tests ===\n\n";

    testUpdateStatement();
    testDeleteStatement();
    testDeleteWithoutWhere();
    testUpdateWithoutWhere();

    std::cout << "\n=== All Tests Complete ===\n";
    return 0;
}
