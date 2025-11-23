#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/lexer.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/sblr/executor.h"
#include <filesystem>

using namespace scratchbird;

TEST(CreateViewTest, BasicViewCreation)
{
    // Create database
    std::string db_path = "test_views.db";
    if (std::filesystem::exists(db_path))
        std::filesystem::remove_all(db_path);
    
    core::ErrorContext ctx;
    ASSERT_EQ(core::Database::create(db_path, 8192, &ctx), core::Status::OK) << "CREATE failed: " << ctx.message;

    core::Database db;
    ASSERT_EQ(db.open(db_path, &ctx), core::Status::OK) << "OPEN failed: " << ctx.message;
    
    // Create a table first
    std::string create_table = "CREATE TABLE users (id INT, name VARCHAR(100));";
    parser::Lexer lexer1(create_table);
    parser::ASTArena arena1;
    parser::Parser parser1(lexer1, arena1);
    auto stmt1 = parser1.parseStatement();
    ASSERT_TRUE(stmt1.success());
    
    sblr::BytecodeGenerator gen1(lexer1.stringPool(), &db);
    auto bc1 = gen1.generate(stmt1.statement());
    ASSERT_TRUE(bc1.success());
    
    sblr::Executor exec1(&db);
    auto result1 = exec1.execute(bc1.bytecode());
    ASSERT_TRUE(result1.success()) << result1.error();
    
    // Create view
    std::string create_view = "CREATE VIEW user_names AS SELECT name FROM users;";
    parser::Lexer lexer2(create_view);
    parser::ASTArena arena2;
    parser::Parser parser2(lexer2, arena2);
    auto stmt2 = parser2.parseStatement();
    ASSERT_TRUE(stmt2.success());
    
    sblr::BytecodeGenerator gen2(lexer2.stringPool(), &db);
    auto bc2 = gen2.generate(stmt2.statement());
    ASSERT_TRUE(bc2.success());
    
    sblr::Executor exec2(&db);
    auto result2 = exec2.execute(bc2.bytecode());
    ASSERT_TRUE(result2.success()) << result2.error();
    
    // Verify view exists in catalog
    core::CatalogManager::ViewInfo view_info;
    core::CatalogManager::SchemaInfo schema_info;
    ASSERT_EQ(db.catalog_manager()->getSchema("PUBLIC", schema_info, &ctx), core::Status::OK);
    ASSERT_EQ(db.catalog_manager()->getView(schema_info.schema_id, "user_names", view_info, &ctx), core::Status::OK);
    
    // Check that the definition is the actual SELECT query
    EXPECT_FALSE(view_info.definition.empty());
    EXPECT_NE(view_info.definition.find("SELECT"), std::string::npos);
    EXPECT_NE(view_info.definition.find("name"), std::string::npos);
    EXPECT_NE(view_info.definition.find("users"), std::string::npos);
    
    std::cout << "View definition: " << view_info.definition << std::endl;
    
    // Cleanup
    std::filesystem::remove_all(db_path);
}

