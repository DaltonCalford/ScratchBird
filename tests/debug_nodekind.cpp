#include "scratchbird/engine/ast.h"
#include "scratchbird/engine/parser.h"

#include <iostream>
#include <string>

using namespace scratchbird::engine;

int main()
{
    std::cout << "=== NodeKind Debug ===" << std::endl;

    std::vector<std::string> test_sqls = {"CREATE TABLE test (id INTEGER)",
                                          "INSERT INTO test VALUES (1)", "SELECT * FROM test",
                                          "UPDATE test SET id = 2", "DELETE FROM test"};

    for (const auto& sql : test_sqls) {
        auto ast = parse_sql(sql);
        std::cout << "SQL: " << sql << std::endl;
        std::cout << "NodeKind: " << static_cast<int>(ast.kind) << std::endl;

        if (ast.kind == NodeKind::Unknown) {
            std::cout << "  -> Unknown NodeKind!" << std::endl;
        } else if (ast.kind == NodeKind::DdlTable) {
            std::cout << "  -> DDL Table" << std::endl;
        } else if (ast.kind == NodeKind::SelectLiteral) {
            std::cout << "  -> Select Literal" << std::endl;
        } else {
            std::cout << "  -> Other NodeKind: " << static_cast<int>(ast.kind) << std::endl;
        }
        std::cout << std::endl;
    }

    return 0;
}
