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
    std::cout << "\n2. Testing UNION ALL parsing..." << std::endl;
    std::string sql = "SELECT 1 UNION ALL SELECT 2";
    auto query = parse_select_minimal(sql);
    std::cout << "Parse OK: " << query.ok << std::endl;
    if (query.compound) {
        std::cout << "Op: " << query.compound->op << ", ALL: " << query.compound->all << std::endl;
        if (query.compound->left && query.compound->left->leaf) {
            std::cout << "Left query projections: "
                      << query.compound->left->leaf->projections.size() << std::endl;
        }
        if (query.compound->right && query.compound->right->leaf) {
            std::cout << "Right query projections: "
                      << query.compound->right->leaf->projections.size() << std::endl;
        }
    }

    // Test 3: INTERSECT
    std::cout << "\n3. Testing INTERSECT parsing..." << std::endl;
    sql = "SELECT 1 INTERSECT SELECT 1";
    query = parse_select_minimal(sql);
    std::cout << "Parse OK: " << query.ok << std::endl;
    if (query.compound) {
        std::cout << "Op: " << query.compound->op << std::endl;
    }

    // Test 4: EXCEPT
    std::cout << "\n4. Testing EXCEPT parsing..." << std::endl;
    sql = "SELECT 1 EXCEPT SELECT 2";
    query = parse_select_minimal(sql);
    std::cout << "Parse OK: " << query.ok << std::endl;
    if (query.compound) {
        std::cout << "Op: " << query.compound->op << std::endl;
    }

    std::cout << "\n=== Set Operations Test Complete ===" << std::endl;
    return 0;
}
