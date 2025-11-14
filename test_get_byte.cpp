#include <iostream>
#include <scratchbird/parser/lexer.h>
#include <scratchbird/parser/parser.h>

using namespace scratchbird;

int main() {
    std::string sql = "SELECT GET_BYTE('hello', 0)";
    parser::Lexer lexer(sql);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);
    auto result = parser.parseStatement();
    
    if (result.success()) {
        std::cout << "Parse succeeded!" << std::endl;
    } else {
        std::cout << "Parse failed with " << result.errors().size() << " errors" << std::endl;
        for (const auto& err : result.errors()) {
            std::cout << "  " << err.message << std::endl;
        }
    }
    return 0;
}
