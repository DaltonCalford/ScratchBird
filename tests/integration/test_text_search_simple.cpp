#include <iostream>
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/semantic_analyzer.h"

int main() {
    using namespace scratchbird::parser;
    
    std::string sql = "SELECT * FROM users WHERE email ILIKE '%@gmail.com'";
    
    Lexer lexer(sql);
    ASTArena arena;
    Parser parser(lexer, arena);
    
    auto result = parser.parseStatement();
    if (!result.success()) {
        std::cout << "Parse failed: " << result.error() << std::endl;
        return 1;
    }
    
    SemanticAnalyzer analyzer(parser.stringPool());
    auto semantic_result = analyzer.analyze(result.statement());
    if (!semantic_result->success()) {
        std::cout << "Semantic analysis failed: " << semantic_result->error() << std::endl;
        return 1;
    }
    
    std::cout << "Success!" << std::endl;
    return 0;
}
