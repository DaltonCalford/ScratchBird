#include <iostream>
#include <scratchbird/parser/lexer.h>
#include <scratchbird/parser/parser.h>
#include <scratchbird/sblr/bytecode_generator.h>
#include <scratchbird/core/database.h>

using namespace scratchbird;

int main() {
    // Test 1: Parse
    std::string sql = "SELECT GET_BYTE('hello', 0)";
    std::cout << "Testing: " << sql << std::endl;
    
    parser::Lexer lexer(sql);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);
    auto result = parser.parseStatement();
    
    if (!result.success()) {
        std::cout << "Parse FAILED" << std::endl;
        return 1;
    }
    std::cout << "Parse OK" << std::endl;
    
    // Test 2: Bytecode generation (need minimal database)
    core::ErrorContext ctx;
    auto status = core::Database::create("test_debug.db", 16384, &ctx);
    if (status != core::Status::OK) {
        std::cout << "Database create failed (may already exist)" << std::endl;
    }
    
    core::Database db;
    status = db.open("test_debug.db", &ctx);
    if (status != core::Status::OK) {
        std::cout << "Database open FAILED: " << ctx.message << std::endl;
        return 1;
    }
    std::cout << "Database OK" << std::endl;
    
    sblr::BytecodeGenerator generator(lexer.stringPool(), &db);
    auto bytecode_result = generator.generate(result.statement());
    
    if (!bytecode_result.success()) {
        std::cout << "Bytecode generation FAILED:" << std::endl;
        for (const auto& err : bytecode_result.errors()) {
            std::cout << "  " << err << std::endl;
        }
        return 1;
    }
    std::cout << "Bytecode generation OK (" << bytecode_result.bytecode().size() << " bytes)" << std::endl;
    
    // Print bytecode for inspection
    std::cout << "Bytecode hex: ";
    for (size_t i = 0; i < std::min(bytecode_result.bytecode().size(), size_t(50)); i++) {
        printf("%02x ", bytecode_result.bytecode()[i]);
    }
    std::cout << std::endl;
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
