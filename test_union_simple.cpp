#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"
#include "scratchbird/engine/parser_select.h"

#include <iostream>

using namespace scratchbird::engine;

int main()
{
    std::cout << "=== Testing UNION parsing ===" << std::endl;
    
    // Test simple UNION parsing
    std::string sql = "SELECT 1 UNION SELECT 2";
    std::cout << "Parsing: " << sql << std::endl;
    
    auto query = parse_select_minimal(sql);
    std::cout << "Parse OK: " << query.ok << std::endl;
    if (!query.ok) {
        std::cout << "Parse error: " << query.error << std::endl;
        return 1;
    }
    
    std::cout << "Has compound: " << (query.compound ? "yes" : "no") << std::endl;
    if (query.compound) {
        std::cout << "Compound op: " << query.compound->op << std::endl;
        std::cout << "Compound all: " << query.compound->all << std::endl;
    }
    
    // Test execution
    std::cout << "Testing execution..." << std::endl;
    auto ast = parse_sql(sql);
    auto result = execute_ast(ast);
    
    if (!result.columns.empty() && result.columns[0] == "error") {
        std::cout << "Execution error: " << (result.rows.empty() ? "unknown" : result.rows[0][0]) << std::endl;
    } else {
        std::cout << "Execution succeeded. Columns: " << result.columns.size() << ", Rows: " << result.rows.size() << std::endl;
        for (const auto& row : result.rows) {
            for (const auto& col : row) {
                std::cout << col << " ";
            }
            std::cout << std::endl;
        }
    }
    
    return 0;
}