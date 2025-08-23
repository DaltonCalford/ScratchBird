#include "scratchbird/capi.h"
#include "scratchbird/engine/ast.h"
#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <unistd.h>

using namespace scratchbird::engine;

// Database setup utility functions
static std::string tempdb()
{
    return std::string("/tmp/stored_proc_test_") + std::to_string(::getpid());
}

static void create_db_and_set_path(const std::string& base)
{
    SB_CreateDbOptions o{};
    o.page_size = 4096;
    SB_Database* db = nullptr;
    auto st = sb_create_database(base.c_str(), &o, &db);
    (void)st;
    if (db)
        sb_close_database(db);
    set_executor_db_path(base);
    // Ensure catalog roots are bootstrapped before any DDL
    CatalogManager cm(base);
    cm.bootstrap_if_needed();
}

static void cleanup_db(const std::string& base)
{
    // Best-effort: remove segment files base.seg0..base.seg15
    for (int i = 0; i < 16; ++i) {
        std::string seg = base + ".seg" + std::to_string(i);
        unlink(seg.c_str());
    }
    // Also try to remove any .bootstrap.sql file
    std::string bootstrap = base + ".bootstrap.sql";
    unlink(bootstrap.c_str());
}

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
        // Set up temporary database with proper initialization
        std::string db_path = tempdb();
        create_db_and_set_path(db_path);

        // Run tests
        test_create_simple_procedure();
        test_create_function_with_parameters();
        test_create_procedure_with_variables();

        std::cout << "=== All Stored Procedure Tests Passed! ===" << std::endl;

        // Clean up
        cleanup_db(db_path);

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
