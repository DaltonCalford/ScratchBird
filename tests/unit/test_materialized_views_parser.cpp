#include <gtest/gtest.h>
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/lexer.h"
#include <iostream>

using namespace scratchbird;

// Test suite for materialized views parser
class MaterializedViewsParserTest : public ::testing::Test {
protected:
    void parseAndValidate(const std::string& sql, parser::ASTKind expected_kind) {
        parser::Lexer lexer(sql);
        parser::ASTArena arena;
        parser::Parser parser(lexer, arena);

        auto stmt = parser.parseStatement();
        ASSERT_TRUE(stmt.success()) << "Parser failed for: " << sql;
        ASSERT_NE(stmt.statement(), nullptr);
        EXPECT_EQ(stmt.statement()->kind(), expected_kind);
    }
};

// Test 1: CREATE MATERIALIZED VIEW parsing
TEST_F(MaterializedViewsParserTest, CreateMaterializedView)
{
    std::string sql = "CREATE MATERIALIZED VIEW user_summary AS SELECT name FROM users;";
    parser::Lexer lexer(sql);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);

    auto stmt = parser.parseStatement();
    ASSERT_TRUE(stmt.success()) << "Parser failed";
    ASSERT_NE(stmt.statement(), nullptr);

    auto* create_view = dynamic_cast<parser::CreateViewStmt*>(stmt.statement());
    ASSERT_NE(create_view, nullptr) << "Not a CreateViewStmt";
    EXPECT_TRUE(create_view->materialized()) << "Should be marked as materialized";

    std::cout << "✓ CREATE MATERIALIZED VIEW parsed correctly" << std::endl;
}

// Test 2: CREATE OR REPLACE MATERIALIZED VIEW
TEST_F(MaterializedViewsParserTest, CreateOrReplaceMaterializedView)
{
    std::string sql = "CREATE OR REPLACE MATERIALIZED VIEW mv_users AS SELECT id, name FROM users;";
    parser::Lexer lexer(sql);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);

    auto stmt = parser.parseStatement();
    ASSERT_TRUE(stmt.success());

    auto* create_view = dynamic_cast<parser::CreateViewStmt*>(stmt.statement());
    ASSERT_NE(create_view, nullptr);
    EXPECT_TRUE(create_view->materialized()) << "Should be materialized";
    EXPECT_TRUE(create_view->orReplace()) << "Should have OR REPLACE";

    std::cout << "✓ CREATE OR REPLACE MATERIALIZED VIEW parsed correctly" << std::endl;
}

// Test 3: Regular view should NOT be materialized
TEST_F(MaterializedViewsParserTest, RegularViewNotMaterialized)
{
    std::string sql = "CREATE VIEW user_names AS SELECT name FROM users;";
    parser::Lexer lexer(sql);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);

    auto stmt = parser.parseStatement();
    ASSERT_TRUE(stmt.success());

    auto* create_view = dynamic_cast<parser::CreateViewStmt*>(stmt.statement());
    ASSERT_NE(create_view, nullptr);
    EXPECT_FALSE(create_view->materialized()) << "Regular view should NOT be materialized";

    std::cout << "✓ Regular VIEW is not materialized" << std::endl;
}

// Test 4: REFRESH MATERIALIZED VIEW parsing
TEST_F(MaterializedViewsParserTest, RefreshMaterializedView)
{
    std::string sql = "REFRESH MATERIALIZED VIEW user_summary;";
    parser::Lexer lexer(sql);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);

    auto stmt = parser.parseStatement();
    ASSERT_TRUE(stmt.success()) << "Parser failed";
    ASSERT_NE(stmt.statement(), nullptr);

    auto* refresh = dynamic_cast<parser::RefreshMaterializedViewStmt*>(stmt.statement());
    ASSERT_NE(refresh, nullptr) << "Not a RefreshMaterializedViewStmt";
    EXPECT_FALSE(refresh->concurrently()) << "Should not be concurrent by default";

    std::cout << "✓ REFRESH MATERIALIZED VIEW parsed correctly" << std::endl;
}

// Test 5: REFRESH CONCURRENTLY MATERIALIZED VIEW
TEST_F(MaterializedViewsParserTest, RefreshConcurrentlyMaterializedView)
{
    std::string sql = "REFRESH CONCURRENTLY MATERIALIZED VIEW user_summary;";
    parser::Lexer lexer(sql);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);

    auto stmt = parser.parseStatement();
    ASSERT_TRUE(stmt.success()) << "Parser failed";
    ASSERT_NE(stmt.statement(), nullptr);

    auto* refresh = dynamic_cast<parser::RefreshMaterializedViewStmt*>(stmt.statement());
    ASSERT_NE(refresh, nullptr) << "Not a RefreshMaterializedViewStmt";
    EXPECT_TRUE(refresh->concurrently()) << "Should be marked as concurrent";

    std::cout << "✓ REFRESH CONCURRENTLY MATERIALIZED VIEW parsed correctly" << std::endl;
}

// Test 6: View name extraction from CREATE MATERIALIZED VIEW
TEST_F(MaterializedViewsParserTest, MaterializedViewNameExtraction)
{
    std::string sql = "CREATE MATERIALIZED VIEW my_mat_view AS SELECT * FROM users;";
    parser::Lexer lexer(sql);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);

    auto stmt = parser.parseStatement();
    ASSERT_TRUE(stmt.success());

    auto* create_view = dynamic_cast<parser::CreateViewStmt*>(stmt.statement());
    ASSERT_NE(create_view, nullptr);

    // Verify the view name was captured
    std::string view_name(lexer.stringPool().get(create_view->name()));
    EXPECT_EQ(view_name, "my_mat_view");

    std::cout << "✓ Materialized view name extracted: " << view_name << std::endl;
}

// Test 7: View name extraction from REFRESH
TEST_F(MaterializedViewsParserTest, RefreshViewNameExtraction)
{
    std::string sql = "REFRESH MATERIALIZED VIEW my_view;";
    parser::Lexer lexer(sql);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);

    auto stmt = parser.parseStatement();
    ASSERT_TRUE(stmt.success());

    auto* refresh = dynamic_cast<parser::RefreshMaterializedViewStmt*>(stmt.statement());
    ASSERT_NE(refresh, nullptr);

    std::string view_name(lexer.stringPool().get(refresh->name()));
    EXPECT_EQ(view_name, "my_view");

    std::cout << "✓ REFRESH view name extracted: " << view_name << std::endl;
}

// Test 8: Query definition preserved in materialized view
TEST_F(MaterializedViewsParserTest, MaterializedViewQueryDefinition)
{
    std::string sql = "CREATE MATERIALIZED VIEW user_stats AS SELECT COUNT(*) FROM users;";
    parser::Lexer lexer(sql);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);

    auto stmt = parser.parseStatement();
    ASSERT_TRUE(stmt.success());

    auto* create_view = dynamic_cast<parser::CreateViewStmt*>(stmt.statement());
    ASSERT_NE(create_view, nullptr);

    std::string query_text = create_view->queryDefinitionText();
    EXPECT_FALSE(query_text.empty());
    EXPECT_NE(query_text.find("COUNT"), std::string::npos);

    std::cout << "✓ Materialized view query definition preserved: " << query_text << std::endl;
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
