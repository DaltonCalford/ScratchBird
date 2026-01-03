/**
 * Manual integration test for Query Planner (Phase 1, Task 1.3)
 *
 * This is a simple standalone program to verify the query planner
 * is properly integrated with bytecode generation via QueryCompilerV2.
 *
 * Compile and run:
 *   cd build
 *   g++ -std=c++20 -I ../include ../tests/integration/manual_test_planner_integration.cpp \
 *       -L. -lscratchbird_core -lscratchbird_parser -lscratchbird_sblr \
 *       -o test_planner && ./test_planner
 */

#include "scratchbird/core/database.h"
#include "scratchbird/sblr/query_compiler_v2.h"

#include <filesystem>
#include <iostream>

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::sblr;

int main()
{
    std::cout << "=== Query Planner Integration Test (Phase 1, Task 1.3) ===\n\n";

    std::string test_db = "/tmp/test_planner_manual.db";
    std::filesystem::remove(test_db);

    ErrorContext ctx;

    // Create and open database
    Status status = Database::create(test_db, 16384, &ctx);
    if (status != Status::OK)
    {
        std::cout << "FAILED: Could not create database: " << ctx.message << "\n";
        return 1;
    }

    Database db;
    status = db.open(test_db, &ctx);
    if (status != Status::OK)
    {
        std::cout << "FAILED: Could not open database: " << ctx.message << "\n";
        std::filesystem::remove(test_db);
        return 1;
    }

    // Check optimizer components
    if (db.statistics_manager() == nullptr)
    {
        std::cout << "FAILED: StatisticsManager not initialized\n";
        db.close();
        std::filesystem::remove(test_db);
        return 1;
    }

    if (db.query_planner() == nullptr)
    {
        std::cout << "FAILED: QueryPlanner not initialized\n";
        db.close();
        std::filesystem::remove(test_db);
        return 1;
    }

    std::cout << "PASSED: Optimizer components initialized\n";

    // Create minimal schema for the query compiler
    Executor executor(&db);
    auto create_result = compiler.compile("CREATE TABLE users (id INT, name VARCHAR(100))");
    if (!create_result.success())
    {
        std::cout << "FAILED: Compile error creating table\n";
        for (const auto &err : create_result.errors())
        {
            std::cout << "  " << err << "\n";
        }
        db.close();
        std::filesystem::remove(test_db);
        return 1;
    }

    auto create_exec = executor.execute(create_result.bytecode());
    if (!create_exec.success())
    {
        std::cout << "FAILED: Execution error creating table: " << create_exec.error() << "\n";
        db.close();
        std::filesystem::remove(test_db);
        return 1;
    }

    // Verify bytecode generation with planner available
    std::string sql = "SELECT id, name FROM users WHERE id = 42";
    QueryCompilerV2 compiler(&db);
    auto compile_result = compiler.compile(sql);

    if (!compile_result.success())
    {
        std::cout << "FAILED: Compile error\n";
        for (const auto &err : compile_result.errors())
        {
            std::cout << "  " << err << "\n";
        }
        db.close();
        std::filesystem::remove(test_db);
        return 1;
    }

    const auto &bytecode = compile_result.bytecode();
    if (bytecode.size() < 3)
    {
        std::cout << "FAILED: Bytecode too small\n";
        db.close();
        std::filesystem::remove(test_db);
        return 1;
    }

    if (bytecode[0] != static_cast<uint8_t>(Opcode::VERSION))
    {
        std::cout << "FAILED: Missing VERSION header\n";
        db.close();
        std::filesystem::remove(test_db);
        return 1;
    }

    if (bytecode.back() != static_cast<uint8_t>(Opcode::END))
    {
        std::cout << "FAILED: Missing END marker\n";
        db.close();
        std::filesystem::remove(test_db);
        return 1;
    }

    std::cout << "PASSED: Generated " << bytecode.size() << " bytes of bytecode\n";
    std::cout << "PASSED: Bytecode structure valid\n";

    db.close();
    std::filesystem::remove(test_db);

    std::cout << "\n=== All tests PASSED ===\n";
    std::cout << "\nQuery Planner Integration verified:\n";
    std::cout << "  ✓ Database initializes optimizer components\n";
    std::cout << "  ✓ QueryCompilerV2 generates bytecode with planner available\n";

    return 0;
}
