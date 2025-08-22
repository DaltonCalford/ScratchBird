#include "scratchbird/engine/ast.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace scratchbird::engine;

void test_create_simple_procedure()
{
    std::cout << "Testing CREATE PROCEDURE..." << std::endl;

    // Simple stored procedure without parameters
    std::string sql = R"(
        CREATE PROCEDURE test_simple_proc
        AS
        BEGIN
            -- Do nothing
        END
    )";

    // Parse the SQL
    auto ast = parse_sql(sql);
    std::cout << "AST kind: " << static_cast<int>(ast.kind)
              << " (expected PsqlRoutine: " << static_cast<int>(NodeKind::PsqlRoutine) << ")"
              << std::endl;

    if (ast.kind == NodeKind::PsqlRoutine) {
        std::cout << "✓ Parser recognized as PsqlRoutine" << std::endl;
        std::cout << "  Name: '" << ast.psqlRoutine.name << "'" << std::endl;
        std::cout << "  Kind: '" << ast.psqlRoutine.kind << "'" << std::endl;
        std::cout << "  Body: '" << ast.psqlRoutine.body_raw << "'" << std::endl;

        // Execute the CREATE PROCEDURE
        auto result = execute_ast(ast);

        // Should succeed
        assert(!result.rows.empty());
        assert(result.columns.size() >= 1);
        std::cout << "✓ CREATE PROCEDURE test passed" << std::endl;
    } else {
        std::cout << "✗ Parser did not recognize as PsqlRoutine" << std::endl;
        assert(false && "CREATE PROCEDURE parsing failed");
    }
}

void test_create_function_with_parameters()
{
    std::cout << "Testing CREATE FUNCTION with parameters..." << std::endl;

    // Function with input and return parameters
    std::string sql = R"(
        CREATE FUNCTION calculate_sum(IN x INTEGER, IN y INTEGER)
        RETURNS INTEGER
        AS
        BEGIN
            RETURN x + y;
        END
    )";

    // Parse the SQL
    auto ast = parse_sql(sql);
    std::cout << "Parameters AST kind: " << static_cast<int>(ast.kind) << std::endl;

    if (ast.kind == NodeKind::PsqlRoutine) {
        std::cout << "✓ Parser recognized function as PsqlRoutine" << std::endl;
        std::cout << "  Name: '" << ast.psqlRoutine.name << "'" << std::endl;
        std::cout << "  Kind: '" << ast.psqlRoutine.kind << "'" << std::endl;
        std::cout << "  Parameters: " << ast.psqlRoutine.params.size() << std::endl;

        for (const auto& param : ast.psqlRoutine.params) {
            std::cout << "    - " << param.first << " : " << param.second << std::endl;
        }

        std::cout << "  Returns: '" << ast.psqlRoutine.returns << "'" << std::endl;

        // Execute the CREATE FUNCTION
        auto result = execute_ast(ast);

        // Should succeed
        assert(!result.rows.empty());
        std::cout << "✓ CREATE FUNCTION with parameters test passed" << std::endl;
    } else {
        std::cout << "✗ Parser did not recognize function as PsqlRoutine" << std::endl;
        assert(false && "CREATE FUNCTION parsing failed");
    }
}

void test_create_procedure_with_variables()
{
    std::cout << "Testing CREATE PROCEDURE with local variables..." << std::endl;

    // Procedure with local variable declarations
    std::string sql = R"(
        CREATE PROCEDURE test_vars_proc(IN input_val INTEGER)
        AS
        DECLARE VARIABLE local_sum INTEGER;
        DECLARE VARIABLE multiplier INTEGER;
        BEGIN
            multiplier = 2;
            local_sum = input_val * multiplier;
        END
    )";

    // Parse the SQL
    auto ast = parse_sql(sql);

    if (ast.kind == NodeKind::PsqlRoutine) {
        std::cout << "✓ Complex procedure parsing succeeded" << std::endl;

        // Execute the CREATE PROCEDURE
        auto result = execute_ast(ast);

        assert(!result.rows.empty());
        std::cout << "✓ CREATE PROCEDURE with variables test passed" << std::endl;
    } else {
        std::cout << "✗ Complex procedure parsing failed" << std::endl;
        assert(false && "Complex CREATE PROCEDURE parsing failed");
    }
}

int main()
{
    std::cout << "=== Stored Procedure Tests ===" << std::endl;

    try {
        // Set up temporary database path
        std::string db_path = "/tmp/stored_proc_test.db";
        set_executor_db_path(db_path);

        // Clean up any existing test database
        std::filesystem::remove(db_path + ".seg0");

        // Run tests
        test_create_simple_procedure();
        test_create_function_with_parameters();
        test_create_procedure_with_variables();

        std::cout << "=== All Stored Procedure Tests Passed! ===" << std::endl;

        // Clean up
        std::filesystem::remove(db_path + ".seg0");

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
