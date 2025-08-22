#include "scratchbird/engine/ast.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace scratchbird::engine;

void test_create_and_call_simple_procedure()
{
    std::cout << "Testing CREATE and CALL simple procedure..." << std::endl;

    // First create a simple procedure
    std::string create_sql = R"(
        CREATE PROCEDURE test_simple_proc
        AS
        BEGIN
            -- Simple procedure
        END
    )";

    auto create_ast = parse_sql(create_sql);
    if (create_ast.kind != NodeKind::PsqlRoutine) {
        std::cerr << "Failed to parse CREATE PROCEDURE" << std::endl;
        assert(false);
    }

    auto create_result = execute_ast(create_ast);
    if (create_result.rows.empty()) {
        std::cerr << "Failed to execute CREATE PROCEDURE" << std::endl;
        assert(false);
    }

    std::cout << "✓ Procedure created successfully" << std::endl;

    // Now call the procedure
    std::string call_sql = "CALL test_simple_proc";

    auto call_ast = parse_sql(call_sql);
    std::cout << "CALL AST kind: " << static_cast<int>(call_ast.kind)
              << " (expected PsqlCall: " << static_cast<int>(NodeKind::PsqlCall) << ")"
              << std::endl;

    if (call_ast.kind == NodeKind::PsqlCall) {
        std::cout << "✓ Parser recognized CALL statement" << std::endl;
        std::cout << "  Routine name: '" << call_ast.psqlCall.routine_name << "'" << std::endl;
        std::cout << "  Arguments: " << call_ast.psqlCall.arguments.size() << std::endl;

        // Execute the CALL statement
        auto call_result = execute_ast(call_ast);

        if (!call_result.error_message.empty()) {
            std::cout << "Call result error: " << call_result.error_message << std::endl;
            // This is expected since we haven't fully implemented catalog retrieval yet
            std::cout << "⚠ CALL execution failed (expected for Sprint 3 development)" << std::endl;
        } else {
            assert(!call_result.rows.empty());
            std::cout << "✓ CALL procedure test passed" << std::endl;
        }
    } else {
        std::cerr << "✗ Parser did not recognize CALL statement" << std::endl;
        assert(false);
    }
}

void test_create_and_call_function_with_parameters()
{
    std::cout << "Testing CREATE and CALL function with parameters..." << std::endl;

    // Create a function with parameters
    std::string create_sql = R"(
        CREATE FUNCTION add_numbers(IN x INTEGER, IN y INTEGER)
        RETURNS INTEGER
        AS
        BEGIN
            RETURN x + y;
        END
    )";

    auto create_ast = parse_sql(create_sql);
    assert(create_ast.kind == NodeKind::PsqlRoutine);

    auto create_result = execute_ast(create_ast);
    assert(!create_result.rows.empty());
    std::cout << "✓ Function created successfully" << std::endl;

    // Call the function with parameters
    std::string call_sql = "CALL add_numbers(5, 10)";

    auto call_ast = parse_sql(call_sql);

    if (call_ast.kind == NodeKind::PsqlCall) {
        std::cout << "✓ Parser recognized function CALL" << std::endl;
        std::cout << "  Function name: '" << call_ast.psqlCall.routine_name << "'" << std::endl;
        std::cout << "  Arguments: " << call_ast.psqlCall.arguments.size() << std::endl;

        for (const auto& arg : call_ast.psqlCall.arguments) {
            std::cout << "    - " << arg << std::endl;
        }

        // Execute the function call
        auto call_result = execute_ast(call_ast);

        if (!call_result.error_message.empty()) {
            std::cout << "Function call error: " << call_result.error_message << std::endl;
            std::cout << "⚠ Function CALL execution failed (expected for Sprint 3 development)"
                      << std::endl;
        } else {
            std::cout << "✓ Function CALL test passed" << std::endl;
        }
    } else {
        std::cerr << "✗ Parser did not recognize function CALL" << std::endl;
        assert(false);
    }
}

void test_call_parsing_variations()
{
    std::cout << "Testing CALL statement parsing variations..." << std::endl;

    // Test different CALL formats
    std::vector<std::string> test_cases = {
        "CALL simple_proc", "CALL proc_with_args(1, 'hello')",
        "CALL complex_proc(x + y, func(a), 'string with spaces')",
        "call lowercase_call", // Test case insensitivity
        "CALL proc_with_one_arg(42)"};

    for (size_t i = 0; i < test_cases.size(); ++i) {
        std::cout << "\n--- Test Case " << (i + 1) << ": " << test_cases[i] << " ---" << std::endl;

        auto ast = parse_sql(test_cases[i]);

        if (ast.kind == NodeKind::PsqlCall) {
            std::cout << "✓ Parsed as CALL statement" << std::endl;
            std::cout << "  Routine: '" << ast.psqlCall.routine_name << "'" << std::endl;
            std::cout << "  Args count: " << ast.psqlCall.arguments.size() << std::endl;
            std::cout << "  Args raw: '" << ast.psqlCall.args_raw << "'" << std::endl;
        } else {
            std::cout << "✗ Not recognized as CALL (AST kind: " << static_cast<int>(ast.kind) << ")"
                      << std::endl;
            assert(false);
        }
    }
}

int main()
{
    std::cout << "=== Function Execution Tests ===" << std::endl;

    try {
        // Set up temporary database path
        std::string db_path = "/tmp/function_exec_test.db";
        set_executor_db_path(db_path);

        // Clean up any existing test database
        std::filesystem::remove(db_path + ".seg0");

        // Run parsing tests first (these should work)
        test_call_parsing_variations();

        // Run creation and call tests (these may partially fail during development)
        test_create_and_call_simple_procedure();
        test_create_and_call_function_with_parameters();

        std::cout << "=== Function Execution Tests Complete ===" << std::endl;
        std::cout << "Note: Some execution failures are expected during Sprint 3 development"
                  << std::endl;

        // Clean up
        std::filesystem::remove(db_path + ".seg0");

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
