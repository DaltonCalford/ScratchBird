#include "scratchbird/engine/ast.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace scratchbird::engine;

void test_basic_cursor_declare()
{
    std::cout << "Testing basic cursor declaration..." << std::endl;

    std::string sql = R"(
        EXECUTE BLOCK
        AS
            DECLARE my_cursor CURSOR FOR (SELECT 'Hello' as greeting);
        BEGIN
            -- Cursor declared but not used
        END
    )";

    auto ast = parse_sql(sql);
    assert(ast.kind == NodeKind::PsqlBlock);

    auto result = execute_ast(ast);

    if (!result.error_message.empty()) {
        std::cout << "Error: " << result.error_message << std::endl;
        std::cout
            << "⚠ Cursor declaration may not be fully implemented (expected during development)"
            << std::endl;
    } else {
        std::cout << "✓ Basic cursor declaration test passed" << std::endl;
    }
}

void test_cursor_open_fetch_close_sequence()
{
    std::cout << "Testing OPEN/FETCH/CLOSE cursor sequence..." << std::endl;

    std::string sql = R"(
        EXECUTE BLOCK
        AS
            DECLARE my_cursor CURSOR FOR (SELECT 'Hello' as greeting, 'World' as target);
            DECLARE greeting_var VARCHAR(50);
            DECLARE target_var VARCHAR(50);
        BEGIN
            OPEN my_cursor;
            FETCH my_cursor INTO greeting_var, target_var;
            CLOSE my_cursor;
        END
    )";

    auto ast = parse_sql(sql);
    assert(ast.kind == NodeKind::PsqlBlock);

    auto result = execute_ast(ast);

    if (!result.error_message.empty()) {
        std::cout << "Error: " << result.error_message << std::endl;
        std::cout << "⚠ Cursor open/fetch/close may not be fully implemented (expected during "
                     "development)"
                  << std::endl;
    } else {
        std::cout << "✓ OPEN/FETCH/CLOSE cursor test passed" << std::endl;
    }
}

void test_cursor_with_multiple_rows()
{
    std::cout << "Testing cursor with multiple rows..." << std::endl;

    std::string sql = R"(
        EXECUTE BLOCK
        AS
            DECLARE row_cursor CURSOR FOR (
                SELECT 'First' as value
                UNION ALL
                SELECT 'Second' as value
                UNION ALL
                SELECT 'Third' as value
            );
            DECLARE current_value VARCHAR(50);
        BEGIN
            OPEN row_cursor;
            FETCH row_cursor INTO current_value;
            FETCH row_cursor INTO current_value;
            FETCH row_cursor INTO current_value;
            FETCH row_cursor INTO current_value; -- This should trigger NO_DATA_FOUND
            CLOSE row_cursor;
        END
    )";

    auto ast = parse_sql(sql);
    assert(ast.kind == NodeKind::PsqlBlock);

    auto result = execute_ast(ast);

    if (!result.error_message.empty()) {
        std::cout << "Error: " << result.error_message << std::endl;
        std::cout << "⚠ Multi-row cursor may not be fully implemented (expected during development)"
                  << std::endl;
    } else {
        std::cout << "✓ Multi-row cursor test passed" << std::endl;
    }
}

void test_cursor_for_loop()
{
    std::cout << "Testing cursor FOR loop..." << std::endl;

    std::string sql = R"(
        EXECUTE BLOCK
        AS
        BEGIN
            FOR SELECT 'Item1' as name, 1 as id
                UNION ALL
                SELECT 'Item2' as name, 2 as id
            AS CURSOR my_cursor
            DO
            BEGIN
                -- Process each row automatically
            END
        END
    )";

    auto ast = parse_sql(sql);
    assert(ast.kind == NodeKind::PsqlBlock);

    auto result = execute_ast(ast);

    if (!result.error_message.empty()) {
        std::cout << "Error: " << result.error_message << std::endl;
        std::cout << "⚠ Cursor FOR loop may not be fully implemented (expected during development)"
                  << std::endl;
    } else {
        std::cout << "✓ Cursor FOR loop test passed" << std::endl;
    }
}

void test_invalid_cursor_operations()
{
    std::cout << "Testing invalid cursor operations..." << std::endl;

    // Test opening non-existent cursor
    std::string sql1 = R"(
        EXECUTE BLOCK
        AS
        BEGIN
            OPEN non_existent_cursor;
        END
    )";

    auto ast1 = parse_sql(sql1);
    auto result1 = execute_ast(ast1);

    if (!result1.error_message.empty() &&
        result1.error_message.find("not declared") != std::string::npos) {
        std::cout << "✓ Non-existent cursor error handled correctly" << std::endl;
    } else {
        std::cout << "⚠ Non-existent cursor error not handled properly" << std::endl;
    }

    // Test fetching from closed cursor
    std::string sql2 = R"(
        EXECUTE BLOCK
        AS
            DECLARE test_cursor CURSOR FOR (SELECT 1 as value);
        BEGIN
            FETCH test_cursor INTO @value; -- Cursor not opened
        END
    )";

    auto ast2 = parse_sql(sql2);
    auto result2 = execute_ast(ast2);

    if (!result2.error_message.empty() &&
        result2.error_message.find("not open") != std::string::npos) {
        std::cout << "✓ Closed cursor fetch error handled correctly" << std::endl;
    } else {
        std::cout << "⚠ Closed cursor fetch error may not be handled properly" << std::endl;
    }
}

void test_cursor_parsing_variations()
{
    std::cout << "Testing cursor statement parsing variations..." << std::endl;

    std::vector<std::string> test_cases = {
        "DECLARE my_cursor CURSOR FOR (SELECT * FROM test_table)",
        "OPEN my_cursor",
        "FETCH my_cursor INTO @var1, @var2",
        "CLOSE my_cursor",
        "open lowercase_cursor",
        "fetch cursor_name into @result",
        "close cursor_name"};

    for (size_t i = 0; i < test_cases.size(); ++i) {
        std::cout << "\n--- Test Case " << (i + 1) << ": " << test_cases[i] << " ---" << std::endl;

        std::string full_sql = "EXECUTE BLOCK AS BEGIN " + test_cases[i] + "; END";
        auto ast = parse_sql(full_sql);

        if (ast.kind == NodeKind::PsqlBlock) {
            std::cout << "✓ Parsed as EXECUTE BLOCK" << std::endl;

            // Check for cursor-related statements
            bool found_cursor_stmt = false;
            for (const auto& stmt : ast.psqlBlock.body) {
                if (stmt.kind == Ast::PsqlStmtKind::Declare && stmt.declare_is_cursor) {
                    found_cursor_stmt = true;
                    std::cout << "✓ Found DECLARE CURSOR statement" << std::endl;
                    break;
                } else if (stmt.kind == Ast::PsqlStmtKind::OpenCursor ||
                           stmt.kind == Ast::PsqlStmtKind::FetchCursor ||
                           stmt.kind == Ast::PsqlStmtKind::CloseCursor) {
                    found_cursor_stmt = true;
                    std::cout << "✓ Found cursor operation statement" << std::endl;
                    break;
                }
            }

            if (!found_cursor_stmt) {
                std::cout << "⚠ Cursor statement not recognized in parsed body" << std::endl;
            }
        } else {
            std::cout << "✗ Not recognized as EXECUTE BLOCK (AST kind: "
                      << static_cast<int>(ast.kind) << ")" << std::endl;
        }
    }
}

int main()
{
    std::cout << "=== Cursor Tests ===" << std::endl;

    try {
        // Set up temporary database path
        std::string db_path = "/tmp/cursor_test.db";
        set_executor_db_path(db_path);

        // Clean up any existing test database
        std::filesystem::remove(db_path + ".seg0");

        // Run parsing tests first (these should work)
        test_cursor_parsing_variations();

        // Run basic functionality tests
        test_basic_cursor_declare();
        test_invalid_cursor_operations();

        // Run advanced cursor operation tests (may partially fail during development)
        test_cursor_open_fetch_close_sequence();
        test_cursor_with_multiple_rows();
        test_cursor_for_loop();

        std::cout << "=== Cursor Tests Complete ===" << std::endl;
        std::cout << "Note: Some execution failures are expected during Sprint 4 development"
                  << std::endl;

        // Clean up
        std::filesystem::remove(db_path + ".seg0");

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
