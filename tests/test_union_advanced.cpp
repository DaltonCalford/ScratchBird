#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"
#include "scratchbird/engine/parser_select.h"

#include <iostream>

using namespace scratchbird::engine;

int main()
{
    std::cout << "=== Testing Set Operations Implementation ===" << std::endl;

    // Test 1: Simple literals that should work without database
    std::cout << "\n1. Testing SELECT 1..." << std::endl;
    auto result = execute_ast(parse_sql("SELECT 1"));
    std::cout << "Result: " << result.columns.size() << " columns, " << result.rows.size()
              << " rows" << std::endl;
    if (!result.rows.empty()) {
        std::cout << "Value: " << result.rows[0][0] << std::endl;
    }

    // Test 2: Simple UNION ALL of literals
    std::cout << "\n2. Testing UNION ALL execution..." << std::endl;
    std::string sql = "SELECT 1 UNION ALL SELECT 2";
    auto result2 = execute_select_sql(sql);
    std::cout << "Result: " << result2.columns.size() << " columns, " << result2.rows.size()
              << " rows" << std::endl;
    for (const auto& row : result2.rows) {
        std::cout << "Row: " << row[0] << std::endl;
    }

    // Test 3: INTERSECT
    std::cout << "\n3. Testing INTERSECT execution..." << std::endl;
    sql = "SELECT 1 INTERSECT SELECT 1";
    auto result3 = execute_select_sql(sql);
    std::cout << "Result: " << result3.columns.size() << " columns, " << result3.rows.size()
              << " rows" << std::endl;
    for (const auto& row : result3.rows) {
        std::cout << "Row: " << row[0] << std::endl;
    }

    // Test 4: EXCEPT
    std::cout << "\n4. Testing EXCEPT execution..." << std::endl;
    sql = "SELECT 1 EXCEPT SELECT 2";
    auto result4 = execute_select_sql(sql);
    std::cout << "Result: " << result4.columns.size() << " columns, " << result4.rows.size()
              << " rows" << std::endl;
    for (const auto& row : result4.rows) {
        std::cout << "Row: " << row[0] << std::endl;
    }

    std::cout << "\n=== Set Operations Test Complete ===" << std::endl;
    return 0;
}
