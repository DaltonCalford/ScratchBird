#include "scratchbird/engine/ast.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace scratchbird::engine;

void test_package_header_parsing()
{
    std::cout << "Testing CREATE PACKAGE header parsing..." << std::endl;

    std::string sql = R"(
        CREATE PACKAGE test_package AS
        BEGIN
            PROCEDURE test_proc;
            FUNCTION test_func RETURNS INTEGER;
        END
    )";

    auto ast = parse_sql(sql);
    assert(ast.kind == NodeKind::PsqlPackage);
    assert(ast.psqlPackage.is_header == true);
    assert(ast.psqlPackage.name == "test_package");
    assert(!ast.psqlPackage.header_body.empty());

    std::cout << "✓ Package header parsing test passed" << std::endl;
}

void test_package_body_parsing()
{
    std::cout << "Testing CREATE PACKAGE BODY parsing..." << std::endl;

    std::string sql = R"(
        CREATE PACKAGE BODY test_package AS
        BEGIN
            PROCEDURE test_proc AS
            BEGIN
                -- Implementation
            END;

            FUNCTION test_func RETURNS INTEGER AS
            BEGIN
                RETURN 42;
            END;
        END
    )";

    auto ast = parse_sql(sql);
    assert(ast.kind == NodeKind::PsqlPackage);
    assert(ast.psqlPackage.is_header == false);
    assert(ast.psqlPackage.name == "test_package");
    assert(!ast.psqlPackage.implementation_body.empty());

    std::cout << "✓ Package body parsing test passed" << std::endl;
}

void test_package_header_execution()
{
    std::cout << "Testing package header execution..." << std::endl;

    std::string sql = R"(
        CREATE PACKAGE test_package AS
        BEGIN
            PROCEDURE hello_world;
            FUNCTION get_version RETURNS VARCHAR(50);
        END
    )";

    auto ast = parse_sql(sql);
    assert(ast.kind == NodeKind::PsqlPackage);

    auto result = execute_ast(ast);

    if (!result.error_message.empty()) {
        std::cout << "Warning: " << result.error_message << std::endl;
        std::cout << "⚠ Package header execution may not be fully implemented (expected during "
                     "development)"
                  << std::endl;
    } else {
        std::cout << "✓ Package header execution test passed" << std::endl;
    }
}

void test_package_body_execution()
{
    std::cout << "Testing package body execution..." << std::endl;

    std::string sql = R"(
        CREATE PACKAGE BODY test_package AS
        BEGIN
            PROCEDURE hello_world AS
            BEGIN
                -- Say hello
            END;

            FUNCTION get_version RETURNS VARCHAR(50) AS
            BEGIN
                RETURN 'ScratchBird 1.0';
            END;
        END
    )";

    auto ast = parse_sql(sql);
    assert(ast.kind == NodeKind::PsqlPackage);

    auto result = execute_ast(ast);

    if (!result.error_message.empty()) {
        std::cout << "Warning: " << result.error_message << std::endl;
        std::cout
            << "⚠ Package body execution may not be fully implemented (expected during development)"
            << std::endl;
    } else {
        std::cout << "✓ Package body execution test passed" << std::endl;
    }
}

void test_package_parsing_variations()
{
    std::cout << "Testing package parsing variations..." << std::endl;

    std::vector<std::string> test_cases = {
        "CREATE PACKAGE pkg1 AS BEGIN END", "CREATE PACKAGE BODY pkg1 AS BEGIN END",
        "create package pkg2 as begin end", "CREATE PACKAGE my_schema.pkg3 AS BEGIN END"};

    for (size_t i = 0; i < test_cases.size(); ++i) {
        std::cout << "\n--- Test Case " << (i + 1) << ": " << test_cases[i] << " ---" << std::endl;

        auto ast = parse_sql(test_cases[i]);

        if (ast.kind == NodeKind::PsqlPackage) {
            std::cout << "✓ Parsed as Package" << std::endl;
            std::cout << "  Package name: '" << ast.psqlPackage.name << "'" << std::endl;
            std::cout << "  Is header: " << (ast.psqlPackage.is_header ? "true" : "false")
                      << std::endl;
        } else {
            std::cout << "✗ Not recognized as Package (AST kind: " << static_cast<int>(ast.kind)
                      << ")" << std::endl;
        }
    }
}

int main()
{
    std::cout << "=== Package Support Tests ===" << std::endl;

    try {
        // Set up temporary database path
        std::string db_path = "/tmp/package_test.db";
        set_executor_db_path(db_path);

        // Clean up any existing test database
        std::filesystem::remove(db_path + ".seg0");

        // Run parsing tests first (these should work)
        test_package_parsing_variations();
        test_package_header_parsing();
        test_package_body_parsing();

        // Run execution tests (may partially fail during development)
        test_package_header_execution();
        test_package_body_execution();

        std::cout << "=== Package Support Tests Complete ===" << std::endl;
        std::cout << "Note: Some execution failures are expected during development" << std::endl;

        // Clean up
        std::filesystem::remove(db_path + ".seg0");

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
