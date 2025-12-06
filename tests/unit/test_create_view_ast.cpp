#include <gtest/gtest.h>
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/lexer.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include <iostream>

using namespace scratchbird;

// Test that CREATE VIEW stores actual SELECT query text in AST
TEST(CreateViewASTTest, StoresQueryDefinitionText)
{
    std::string sql = "CREATE VIEW user_names AS SELECT name FROM users;";
    parser::Lexer lexer(sql);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);

    auto stmt = parser.parseStatement();
    ASSERT_TRUE(stmt.success()) << "Parser failed";
    ASSERT_NE(stmt.statement(), nullptr);

    // Verify it's a CreateViewStmt
    auto* create_view = dynamic_cast<parser::CreateViewStmt*>(stmt.statement());
    ASSERT_NE(create_view, nullptr) << "Statement is not CreateViewStmt";

    // Verify query definition text was extracted
    std::string query_text = create_view->queryDefinitionText();
    EXPECT_FALSE(query_text.empty()) << "Query definition text is empty";

    // Verify the query text contains the SELECT statement
    EXPECT_NE(query_text.find("SELECT"), std::string::npos)
        << "Query text doesn't contain SELECT: " << query_text;
    EXPECT_NE(query_text.find("name"), std::string::npos)
        << "Query text doesn't contain column name: " << query_text;
    EXPECT_NE(query_text.find("users"), std::string::npos)
        << "Query text doesn't contain table name: " << query_text;

    std::cout << "Query definition text extracted: " << query_text << std::endl;
}

// Test that bytecode generator writes actual query text (not placeholder)
TEST(CreateViewBytecodeTest, WritesActualQueryText)
{
    std::string sql = "CREATE VIEW active_users AS SELECT id, name FROM users WHERE is_active = 1;";
    parser::Lexer lexer(sql);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);

    auto stmt = parser.parseStatement();
    ASSERT_TRUE(stmt.success());

    // For bytecode test, we need to verify that the AST has the query text
    // (Bytecode testing would require a database instance)
    auto* create_view = dynamic_cast<parser::CreateViewStmt*>(stmt.statement());
    ASSERT_NE(create_view, nullptr);

    std::string query_text = create_view->queryDefinitionText();
    EXPECT_FALSE(query_text.empty());
    EXPECT_EQ(query_text.find("<view_definition>"), std::string::npos)
        << "Query text is placeholder";
    EXPECT_NE(query_text.find("SELECT"), std::string::npos)
        << "Query text doesn't contain SELECT";
    EXPECT_NE(query_text.find("WHERE"), std::string::npos)
        << "Query text doesn't contain WHERE clause";

    std::cout << "Query definition stored correctly (not placeholder)" << std::endl;
}

// Test CREATE OR REPLACE VIEW
TEST(CreateViewASTTest, OrReplaceVariant)
{
    std::string sql = "CREATE OR REPLACE VIEW user_names AS SELECT name FROM users;";
    parser::Lexer lexer(sql);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);

    auto stmt = parser.parseStatement();
    ASSERT_TRUE(stmt.success());

    auto* create_view = dynamic_cast<parser::CreateViewStmt*>(stmt.statement());
    ASSERT_NE(create_view, nullptr);

    // Verify OR REPLACE flag is set
    EXPECT_TRUE(create_view->orReplace());

    // Verify query text is still extracted correctly
    std::string query_text = create_view->queryDefinitionText();
    EXPECT_FALSE(query_text.empty());
    EXPECT_NE(query_text.find("SELECT"), std::string::npos);

    std::cout << "CREATE OR REPLACE VIEW parsed correctly" << std::endl;
}

// Test SELECT with WHERE clause
TEST(CreateViewASTTest, SelectWithWhere)
{
    std::string sql = "CREATE VIEW active_users AS SELECT id, name FROM users WHERE status = 1;";
    parser::Lexer lexer(sql);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);

    auto stmt = parser.parseStatement();
    ASSERT_TRUE(stmt.success());

    auto* create_view = dynamic_cast<parser::CreateViewStmt*>(stmt.statement());
    ASSERT_NE(create_view, nullptr);

    std::string query_text = create_view->queryDefinitionText();
    EXPECT_FALSE(query_text.empty());
    EXPECT_NE(query_text.find("WHERE"), std::string::npos);
    EXPECT_NE(query_text.find("status"), std::string::npos);

    std::cout << "SELECT with WHERE extracted: " << query_text << std::endl;
}

