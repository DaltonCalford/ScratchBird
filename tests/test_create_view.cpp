#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"

#include <iostream>

using namespace scratchbird::engine;

int main()
{
    std::cout << "=== Testing CREATE VIEW ===" << std::endl;

    // Test 1: Parse CREATE VIEW
    std::cout << "\n1. Testing CREATE VIEW parsing..." << std::endl;
    std::string sql = "CREATE VIEW test_view AS SELECT 1 AS num";
    auto ast = parse_sql(sql);
    std::cout << "Parse result - Kind: " << (int)ast.kind << std::endl;
    if (ast.kind == NodeKind::DdlView) {
        std::cout << "View name: " << ast.ddlView.name << std::endl;
        std::cout << "View body: " << ast.ddlView.body_raw << std::endl;
        std::cout << "View columns: " << ast.ddlView.columns_raw << std::endl;
    } else {
        std::cout << "Not parsed as DdlView" << std::endl;
    }

    // Test 2: Try to execute CREATE VIEW (should fail gracefully without database)
    std::cout << "\n2. Testing CREATE VIEW execution (expected to fail without DB)..." << std::endl;
    auto result = execute_ast(ast);
    if (!result.columns.empty() && result.columns[0] == "error") {
        std::cout << "Execution error (expected): "
                  << (result.rows.empty() ? "unknown" : result.rows[0][0]) << std::endl;
    } else {
        std::cout << "Execution result - Columns: " << result.columns.size()
                  << ", Rows: " << result.rows.size() << std::endl;
        if (!result.rows.empty() && !result.rows[0].empty()) {
            std::cout << "Result: " << result.rows[0][0] << std::endl;
        }
    }

    std::cout << "\n=== CREATE VIEW Test Complete ===" << std::endl;
    return 0;
}
