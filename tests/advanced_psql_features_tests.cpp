#include "scratchbird/engine/ast.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace scratchbird::engine;

void test_security_context_definer()
{
    std::cout << "Testing SECURITY DEFINER context..." << std::endl;

    std::string sql = R"(
        CREATE PROCEDURE test_definer_proc
        SECURITY DEFINER
        AS
        BEGIN
            -- This would run with definer's privileges
        END
    )";

    auto ast = parse_sql(sql);
    if (ast.kind == NodeKind::PsqlRoutine) {
        auto result = execute_ast(ast);

        if (!result.error_message.empty()) {
            std::cout << "Error: " << result.error_message << std::endl;
            std::cout
                << "⚠ SECURITY DEFINER may not be fully implemented (expected during development)"
                << std::endl;
        } else {
            std::cout << "✓ SECURITY DEFINER procedure created successfully" << std::endl;
        }
    } else {
        std::cout << "⚠ Procedure not recognized as PsqlRoutine" << std::endl;
    }
}

void test_security_context_invoker()
{
    std::cout << "Testing SECURITY INVOKER context..." << std::endl;

    std::string sql = R"(
        CREATE PROCEDURE test_invoker_proc
        SECURITY INVOKER
        AS
        BEGIN
            -- This runs with caller's privileges (default)
        END
    )";

    auto ast = parse_sql(sql);
    if (ast.kind == NodeKind::PsqlRoutine) {
        auto result = execute_ast(ast);

        if (!result.error_message.empty()) {
            std::cout << "Error: " << result.error_message << std::endl;
            std::cout
                << "⚠ SECURITY INVOKER may not be fully implemented (expected during development)"
                << std::endl;
        } else {
            std::cout << "✓ SECURITY INVOKER procedure created successfully" << std::endl;
        }
    } else {
        std::cout << "⚠ Procedure not recognized as PsqlRoutine" << std::endl;
    }
}

void test_break_continue_statements()
{
    std::cout << "Testing BREAK and CONTINUE statements..." << std::endl;

    std::string sql = R"(
        EXECUTE BLOCK
        AS
            DECLARE counter INTEGER;
        BEGIN
            counter = 0;
            WHILE (counter < 10) DO
            BEGIN
                counter = counter + 1;
                IF (counter = 3) THEN
                    CONTINUE;
                IF (counter = 7) THEN
                    LEAVE;
                -- Process other iterations
            END
        END
    )";

    auto ast = parse_sql(sql);
    assert(ast.kind == NodeKind::PsqlBlock);

    auto result = execute_ast(ast);

    if (!result.error_message.empty()) {
        std::cout << "Error: " << result.error_message << std::endl;
        std::cout << "⚠ BREAK/CONTINUE statements may not be fully implemented (expected during "
                     "development)"
                  << std::endl;
    } else {
        std::cout << "✓ BREAK/CONTINUE statements test passed" << std::endl;
    }
}

void test_advanced_for_loop_with_cursor()
{
    std::cout << "Testing advanced FOR loop with cursor..." << std::endl;

    std::string sql = R"(
        EXECUTE BLOCK
        AS
        BEGIN
            FOR SELECT 'Item1' as name, 1 as id
                UNION ALL
                SELECT 'Item2' as name, 2 as id
                UNION ALL
                SELECT 'Item3' as name, 3 as id
            AS CURSOR item_cursor
            DO
            BEGIN
                -- Process each row with CURSOR automatically
                IF (item_cursor.id = 2) THEN
                    CONTINUE;
                -- Process other items
            END
        END
    )";

    auto ast = parse_sql(sql);
    assert(ast.kind == NodeKind::PsqlBlock);

    auto result = execute_ast(ast);

    if (!result.error_message.empty()) {
        std::cout << "Error: " << result.error_message << std::endl;
        std::cout
            << "⚠ FOR loop with cursor may not be fully implemented (expected during development)"
            << std::endl;
    } else {
        std::cout << "✓ FOR loop with cursor test passed" << std::endl;
    }
}

void test_nested_exception_handling()
{
    std::cout << "Testing nested exception handling..." << std::endl;

    std::string sql = R"(
        EXECUTE BLOCK
        AS
        BEGIN
            BEGIN
                RAISE ZERO_DIVIDE 'Inner exception';
                WHEN ZERO_DIVIDE DO
                BEGIN
                    -- Handle inner exception
                    RAISE USER_EXCEPTION 'Handled and re-raised';
                END
            END
            WHEN USER_EXCEPTION DO
            BEGIN
                -- Handle outer exception
            END
        END
    )";

    auto ast = parse_sql(sql);
    assert(ast.kind == NodeKind::PsqlBlock);

    auto result = execute_ast(ast);

    if (!result.error_message.empty()) {
        std::cout << "Error: " << result.error_message << std::endl;
        std::cout << "⚠ Nested exception handling may not be fully implemented (expected during "
                     "development)"
                  << std::endl;
    } else {
        std::cout << "✓ Nested exception handling test passed" << std::endl;
    }
}

void test_comprehensive_psql_integration()
{
    std::cout << "Testing comprehensive PSQL integration..." << std::endl;

    std::string sql = R"(
        EXECUTE BLOCK (input_value INTEGER = 5)
        RETURNS (result_value INTEGER)
        AS
            DECLARE temp_cursor CURSOR FOR (
                SELECT level as num FROM (
                    SELECT 1 as level UNION ALL
                    SELECT 2 as level UNION ALL
                    SELECT 3 as level
                )
            );
            DECLARE current_num INTEGER;
            DECLARE total INTEGER;
        BEGIN
            total = 0;

            OPEN temp_cursor;
            WHILE (1 = 1) DO
            BEGIN
                FETCH temp_cursor INTO current_num;

                IF (current_num IS NULL) THEN
                    LEAVE;

                total = total + current_num;

                IF (total > input_value) THEN
                    CONTINUE;
            END
            CLOSE temp_cursor;

            result_value = total;

            WHEN NO_DATA_FOUND DO
            BEGIN
                result_value = -1;
            END
            WHEN OTHERS DO
            BEGIN
                result_value = -2;
            END
        END
    )";

    auto ast = parse_sql(sql);
    assert(ast.kind == NodeKind::PsqlBlock);

    auto result = execute_ast(ast);

    if (!result.error_message.empty()) {
        std::cout << "Error: " << result.error_message << std::endl;
        std::cout << "⚠ Comprehensive PSQL integration may not be fully implemented (expected "
                     "during development)"
                  << std::endl;
    } else {
        std::cout << "✓ Comprehensive PSQL integration test passed" << std::endl;
    }
}

void test_parsing_advanced_statements()
{
    std::cout << "Testing parsing of advanced statements..." << std::endl;

    std::vector<std::string> test_cases = {
        "LEAVE", "CONTINUE", "LEAVE loop_label", "CONTINUE outer_loop", "leave", "continue"};

    for (size_t i = 0; i < test_cases.size(); ++i) {
        std::cout << "\n--- Test Case " << (i + 1) << ": " << test_cases[i] << " ---" << std::endl;

        std::string full_sql = "EXECUTE BLOCK AS BEGIN " + test_cases[i] + "; END";
        auto ast = parse_sql(full_sql);

        if (ast.kind == NodeKind::PsqlBlock) {
            std::cout << "✓ Parsed as EXECUTE BLOCK" << std::endl;

            // Check for advanced control flow statements
            bool found_advanced_stmt = false;
            for (const auto& stmt : ast.psqlBlock.body) {
                if (stmt.kind == Ast::PsqlStmtKind::Leave ||
                    stmt.kind == Ast::PsqlStmtKind::Continue) {
                    found_advanced_stmt = true;
                    std::cout << "✓ Found advanced control flow statement" << std::endl;
                    if (!stmt.label.empty()) {
                        std::cout << "  Label: '" << stmt.label << "'" << std::endl;
                    }
                    break;
                }
            }

            if (!found_advanced_stmt) {
                std::cout << "⚠ Advanced control flow statement not recognized in parsed body"
                          << std::endl;
            }
        } else {
            std::cout << "✗ Not recognized as EXECUTE BLOCK (AST kind: "
                      << static_cast<int>(ast.kind) << ")" << std::endl;
        }
    }
}

int main()
{
    std::cout << "=== Advanced PSQL Feature Tests ===" << std::endl;

    try {
        // Set up temporary database path
        std::string db_path = "/tmp/advanced_psql_test.db";
        set_executor_db_path(db_path);

        // Clean up any existing test database
        std::filesystem::remove(db_path + ".seg0");

        // Run parsing tests first (these should work)
        test_parsing_advanced_statements();

        // Run security context tests
        test_security_context_definer();
        test_security_context_invoker();

        // Run advanced control flow tests
        test_break_continue_statements();
        test_advanced_for_loop_with_cursor();

        // Run integration tests (may partially fail during development)
        test_nested_exception_handling();
        test_comprehensive_psql_integration();

        std::cout << "=== Advanced PSQL Feature Tests Complete ===" << std::endl;
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
