/**
 * Manual integration test for Query Planner (Phase 1, Task 1.3)
 *
 * This is a simple standalone program to verify the query planner
 * is properly integrated with bytecode generation.
 *
 * Compile and run:
 *   cd build
 *   g++ -std=c++20 -I ../include ../tests/manual_test_planner_integration.cpp \
 *       -L. -lscratchbird_core -lscratchbird_parser -lscratchbird_sblr \
 *       -o test_planner && ./test_planner
 */

#include "scratchbird/core/database.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include <iostream>
#include <filesystem>

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::parser;
using namespace scratchbird::sblr;

int main()
{
    std::cout << "=== Query Planner Integration Test (Phase 1, Task 1.3) ===\n\n";

    // Test 1: Verify bytecode generation WITHOUT planner (fallback mode)
    std::cout << "Test 1: Bytecode generation without planner (fallback mode)\n";
    {
        std::string sql = "SELECT * FROM users WHERE id > 10";

        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto parse_result = parser.parseStatement();
        if (!parse_result.success())
        {
            std::cout << "  FAILED: Parse error\n";
            return 1;
        }

        // Generate without database (forces fallback)
        BytecodeGenerator generator(parser.stringPool(), nullptr);
        auto bytecode_result = generator.generate(parse_result.statement());

        if (!bytecode_result.success())
        {
            std::cout << "  FAILED: Bytecode generation error\n";
            for (const auto &err : bytecode_result.errors())
            {
                std::cout << "    " << err << "\n";
            }
            return 1;
        }

        std::cout << "  PASSED: Generated " << bytecode_result.bytecode().size()
                  << " bytes of bytecode\n";
    }

    // Test 2: Verify Database has optimizer components
    std::cout << "\nTest 2: Database optimizer component initialization\n";
    {
        std::string test_db = "/tmp/test_planner_manual.db";
        std::filesystem::remove(test_db);

        ErrorContext ctx;

        // Create database
        Status status = Database::create(test_db, 16384, &ctx);
        if (status != Status::OK)
        {
            std::cout << "  FAILED: Could not create database: " << ctx.message << "\n";
            return 1;
        }

        // Open database
        Database db;
        status = db.open(test_db, &ctx);
        if (status != Status::OK)
        {
            std::cout << "  FAILED: Could not open database: " << ctx.message << "\n";
            std::filesystem::remove(test_db);
            return 1;
        }

        // Check optimizer components
        if (db.statistics_manager() == nullptr)
        {
            std::cout << "  FAILED: StatisticsManager not initialized\n";
            db.close();
            std::filesystem::remove(test_db);
            return 1;
        }

        if (db.query_planner() == nullptr)
        {
            std::cout << "  FAILED: QueryPlanner not initialized\n";
            db.close();
            std::filesystem::remove(test_db);
            return 1;
        }

        std::cout << "  PASSED: Optimizer components initialized\n";

        db.close();
        std::filesystem::remove(test_db);
    }

    // Test 3: Verify bytecode generation WITH planner available
    std::cout << "\nTest 3: Bytecode generation with planner available\n";
    {
        std::string test_db = "/tmp/test_planner_manual2.db";
        std::filesystem::remove(test_db);

        ErrorContext ctx;

        // Create and open database
        Status status = Database::create(test_db, 16384, &ctx);
        if (status != Status::OK)
        {
            std::cout << "  FAILED: Could not create database\n";
            return 1;
        }

        Database db;
        status = db.open(test_db, &ctx);
        if (status != Status::OK)
        {
            std::cout << "  FAILED: Could not open database\n";
            std::filesystem::remove(test_db);
            return 1;
        }

        // Generate bytecode with planner available
        std::string sql = "SELECT id, name FROM users WHERE id = 42";

        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto parse_result = parser.parseStatement();
        if (!parse_result.success())
        {
            std::cout << "  FAILED: Parse error\n";
            db.close();
            std::filesystem::remove(test_db);
            return 1;
        }

        // Pass database to generator (enables planner)
        BytecodeGenerator generator(parser.stringPool(), &db);
        auto bytecode_result = generator.generate(parse_result.statement());

        if (!bytecode_result.success())
        {
            std::cout << "  FAILED: Bytecode generation error\n";
            for (const auto &err : bytecode_result.errors())
            {
                std::cout << "    " << err << "\n";
            }
            db.close();
            std::filesystem::remove(test_db);
            return 1;
        }

        std::cout << "  PASSED: Generated " << bytecode_result.bytecode().size()
                  << " bytes of bytecode with planner\n";

        // Verify bytecode structure
        const auto &bytecode = bytecode_result.bytecode();
        if (bytecode.size() < 3)
        {
            std::cout << "  FAILED: Bytecode too small\n";
            db.close();
            std::filesystem::remove(test_db);
            return 1;
        }

        if (bytecode[0] != static_cast<uint8_t>(Opcode::VERSION))
        {
            std::cout << "  FAILED: Missing VERSION header\n";
            db.close();
            std::filesystem::remove(test_db);
            return 1;
        }

        if (bytecode.back() != static_cast<uint8_t>(Opcode::END))
        {
            std::cout << "  FAILED: Missing END marker\n";
            db.close();
            std::filesystem::remove(test_db);
            return 1;
        }

        std::cout << "  PASSED: Bytecode structure valid\n";

        db.close();
        std::filesystem::remove(test_db);
    }

    std::cout << "\n=== All tests PASSED ===\n";
    std::cout << "\nQuery Planner Integration (Phase 1, Task 1.3) verified:\n";
    std::cout << "  ✓ BytecodeGenerator accepts Database pointer\n";
    std::cout << "  ✓ Database initializes optimizer components\n";
    std::cout << "  ✓ Bytecode generation works with and without planner\n";
    std::cout << "  ✓ Graceful fallback when planner unavailable\n";

    return 0;
}
