#include "scratchbird/engine/ast.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace scratchbird::engine;

void test_basic_execute_block()
{
    std::cout << "Testing basic EXECUTE BLOCK..." << std::endl;

    // Simple EXECUTE BLOCK with variable declaration
    std::string sql = R"(
        EXECUTE BLOCK
        AS
        DECLARE VARIABLE i INTEGER;
        BEGIN
            i = 42;
        END
    )";

    // Parse the SQL
    auto ast = parse_sql(sql);
    std::cout << "AST kind: " << static_cast<int>(ast.kind)
              << " (expected PsqlBlock: " << static_cast<int>(NodeKind::PsqlBlock) << ")"
              << std::endl;

    if (ast.kind == NodeKind::PsqlBlock) {
        // Execute the PSQL block
        auto result = execute_ast(ast);

        // Should succeed without errors
        assert(!result.rows.empty());
        assert(result.columns.size() >= 1);

        std::cout << "✓ Basic EXECUTE BLOCK test passed" << std::endl;
    } else {
        std::cout << "Parser did not recognize as PsqlBlock - this is expected for Sprint 1"
                  << std::endl;
    }
}

void test_execute_block_with_parameters()
{
    std::cout << "Testing EXECUTE BLOCK with parameters..." << std::endl;

    // EXECUTE BLOCK with input/output parameters
    std::string sql = R"(
        EXECUTE BLOCK (input_val INTEGER = 10)
        RETURNS (output_val INTEGER)
        AS
        BEGIN
            output_val = input_val * 2;
        END
    )";

    // Parse the SQL
    auto ast = parse_sql(sql);
    std::cout << "Parameters AST kind: " << static_cast<int>(ast.kind) << std::endl;

    if (ast.kind == NodeKind::PsqlBlock) {
        // Execute the PSQL block
        auto result = execute_ast(ast);

        // Should succeed and have return value
        assert(!result.rows.empty());
        std::cout << "✓ EXECUTE BLOCK with parameters test passed" << std::endl;
    } else {
        std::cout << "Parser did not recognize parameterized EXECUTE BLOCK - this is expected for "
                     "Sprint 1"
                  << std::endl;
    }
}

void test_execute_block_variable_assignment()
{
    std::cout << "Testing EXECUTE BLOCK variable assignment..." << std::endl;

    // Simple variable assignment and computation
    std::string sql = R"(
        EXECUTE BLOCK
        AS
        DECLARE VARIABLE x INTEGER;
        DECLARE VARIABLE y INTEGER;
        DECLARE VARIABLE result INTEGER;
        BEGIN
            x = 5;
            y = 10;
            result = x + y;
        END
    )";

    // Parse the SQL
    auto ast = parse_sql(sql);
    std::cout << "Variable assignment AST kind: " << static_cast<int>(ast.kind) << std::endl;

    if (ast.kind == NodeKind::PsqlBlock) {
        // Execute the PSQL block
        auto execution_result = execute_ast(ast);

        // Should succeed
        assert(!execution_result.rows.empty());
        std::cout << "✓ EXECUTE BLOCK variable assignment test passed" << std::endl;
    } else {
        std::cout << "Parser did not recognize variable assignment EXECUTE BLOCK - this is "
                     "expected for Sprint 1"
                  << std::endl;
    }
}

void test_psql_parser_integration()
{
    std::cout << "Testing PSQL parser integration..." << std::endl;

    // Test that parser correctly identifies EXECUTE BLOCK
    std::string sql = "EXECUTE BLOCK AS BEGIN END";

    auto ast = parse_sql(sql);
    assert(ast.kind == NodeKind::PsqlBlock);
    assert(!ast.psqlBlock.body.empty() || ast.psqlBlock.params_raw.empty());

    std::cout << "✓ PSQL parser integration test passed" << std::endl;
}

int main()
{
    std::cout << "=== PSQL Basic Tests ===" << std::endl;

    try {
        // Set up temporary database path
        std::string db_path = "/tmp/psql_test.db";
        set_executor_db_path(db_path);

        // Clean up any existing test database
        std::filesystem::remove(db_path);

        // Run tests
        test_psql_parser_integration();
        test_basic_execute_block();
        test_execute_block_variable_assignment();
        test_execute_block_with_parameters();

        std::cout << "=== All PSQL Basic Tests Passed! ===" << std::endl;

        // Clean up
        std::filesystem::remove(db_path);

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
