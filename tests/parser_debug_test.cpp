#include "scratchbird/engine/ast.h"
#include "scratchbird/engine/parser.h"

#include <iostream>
#include <string>

using namespace scratchbird::engine;

void test_execute_block_parsing()
{
    std::cout << "=== PSQL Parser Debug Test ===" << std::endl;

    // Test cases with different formatting
    std::vector<std::string> test_cases = {
        // Simple case that works
        "EXECUTE BLOCK AS BEGIN END",

        // Complex cases that might not work
        R"(
        EXECUTE BLOCK
        AS
        DECLARE VARIABLE i INTEGER;
        BEGIN
            i = 42;
        END
        )",

        // Without extra whitespace
        "EXECUTE BLOCK AS DECLARE VARIABLE i INTEGER; BEGIN i = 42; END",

        // With parameters
        "EXECUTE BLOCK (input_val INTEGER = 10) RETURNS (output_val INTEGER) AS BEGIN output_val = "
        "input_val * 2; END"};

    for (size_t i = 0; i < test_cases.size(); ++i) {
        std::cout << "\n--- Test Case " << (i + 1) << " ---" << std::endl;
        std::cout << "SQL: " << test_cases[i] << std::endl;

        auto ast = parse_sql(test_cases[i]);
        std::cout << "AST kind: " << static_cast<int>(ast.kind)
                  << " (PsqlBlock = " << static_cast<int>(NodeKind::PsqlBlock) << ")" << std::endl;

        if (ast.kind == NodeKind::PsqlBlock) {
            std::cout << "✓ Recognized as PsqlBlock" << std::endl;
            std::cout << "  params_raw: '" << ast.psqlBlock.params_raw << "'" << std::endl;
            std::cout << "  returns_raw: '" << ast.psqlBlock.returns_raw << "'" << std::endl;
            std::cout << "  body statements: " << ast.psqlBlock.body.size() << std::endl;
            for (const auto& stmt : ast.psqlBlock.body) {
                std::cout << "    - " << stmt.raw << " (kind: " << static_cast<int>(stmt.kind)
                          << ")" << std::endl;
            }
        } else {
            std::cout << "✗ Not recognized as PsqlBlock" << std::endl;
        }
    }
}

int main()
{
    test_execute_block_parsing();
    return 0;
}
