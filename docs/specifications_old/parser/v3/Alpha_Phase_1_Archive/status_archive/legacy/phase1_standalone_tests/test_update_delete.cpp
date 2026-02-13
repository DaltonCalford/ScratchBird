#include "scratchbird/parser/lexer.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/ast.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include <iostream>
#include <string>

using namespace scratchbird;

void testUpdateStatement()
{
    std::cout << "Testing UPDATE statement...\n";

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
        std::cout << "  FAILED: Expected UPDATE node\n";
        return;
    }

    auto *update_stmt = static_cast<parser::UpdateStmt*>(result.statement());
    std::cout << "  Table: " << lexer.stringPool().get(update_stmt->tableName()) << "\n";
    std::cout << "  Assignments: " << update_stmt->assignments().size() << "\n";
    std::cout << "  Has WHERE: " << (update_stmt->whereClause() ? "yes" : "no") << "\n";

    // Test bytecode generation
    sblr::BytecodeGenerator generator(lexer.stringPool());
    auto bytecode_result = generator.generate(result.statement());

    if (!bytecode_result.success())
    {
        std::cout << "  FAILED: Bytecode generation errors:\n";
        for (const auto &error : bytecode_result.errors())
        {
            std::cout << "    " << error << "\n";
        }
        return;
    }

    std::cout << "  Bytecode generated: " << bytecode_result.bytecode().size() << " bytes\n";
    std::cout << "  PASSED\n";
}

void testDeleteStatement()
{
    std::cout << "\nTesting DELETE statement...\n";

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
        std::cout << "  FAILED: Expected DELETE_STMT node\n";
        return;
    }

    auto *delete_stmt = static_cast<parser::DeleteStmt*>(result.statement());
    std::cout << "  Table: " << lexer.stringPool().get(delete_stmt->tableName()) << "\n";
    std::cout << "  Has WHERE: " << (delete_stmt->whereClause() ? "yes" : "no") << "\n";

    // Test bytecode generation
    sblr::BytecodeGenerator generator(lexer.stringPool());
    auto bytecode_result = generator.generate(result.statement());

    if (!bytecode_result.success())
    {
        std::cout << "  FAILED: Bytecode generation errors:\n";
        for (const auto &error : bytecode_result.errors())
        {
            std::cout << "    " << error << "\n";
        }
        return;
    }

    std::cout << "  Bytecode generated: " << bytecode_result.bytecode().size() << " bytes\n";
    std::cout << "  PASSED\n";
}

void testDeleteWithoutWhere()
{
    std::cout << "\nTesting DELETE without WHERE...\n";

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

    // Test bytecode generation
    sblr::BytecodeGenerator generator(lexer.stringPool());
    auto bytecode_result = generator.generate(result.statement());

    if (!bytecode_result.success())
    {
        std::cout << "  FAILED: Bytecode generation errors:\n";
        for (const auto &error : bytecode_result.errors())
        {
            std::cout << "    " << error << "\n";
        }
        return;
    }

    std::cout << "  Bytecode generated: " << bytecode_result.bytecode().size() << " bytes\n";
    std::cout << "  PASSED\n";
}

int main()
{
    std::cout << "=== UPDATE and DELETE Statement Tests ===\n\n";

    testUpdateStatement();
    testDeleteStatement();
    testDeleteWithoutWhere();

    std::cout << "\n=== All Tests Complete ===\n";
    return 0;
}
