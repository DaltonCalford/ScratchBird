// Test LIMIT/OFFSET execution end-to-end
// Phase 1 Task 1.6.5 validation

#include "scratchbird/parser/lexer.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/core/database.h"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace scratchbird;

void testLimitOffset()
{
    std::cout << "\n=== Testing LIMIT/OFFSET Execution ===\n\n";

    // Remove existing test database
    std::filesystem::remove_all("test_limit_exec.db");

    // Create database
    core::ErrorContext err_ctx;
    auto db = std::make_unique<core::Database>("test_limit_exec.db", &err_ctx);
    if (!db || !db->isOpen())
    {
        std::cerr << "Failed to create database\n";
        return;
    }

    // Create table
    parser::StringPool pool;
    {
        std::string sql = "CREATE TABLE numbers (id INT32, value INT32)";
        parser::Lexer lexer(sql, pool);
        parser::Parser parser_(lexer, pool);
        auto stmt = parser_.parse();

        if (!stmt || parser_.hasErrors())
        {
            std::cerr << "Parse error: " << parser_.errors()[0] << "\n";
            return;
        }

        sblr::BytecodeGenerator generator(pool, db.get());
        auto bytecode_result = generator.generate(stmt.get());

        if (!bytecode_result.success())
        {
            std::cerr << "Bytecode generation error: " << bytecode_result.errors()[0] << "\n";
            return;
        }

        sblr::Executor executor(db.get());
        auto exec_result = executor.execute(bytecode_result.bytecode());

        if (!exec_result.success())
        {
            std::cerr << "Execution error: " << exec_result.error() << "\n";
            return;
        }

        std::cout << "✓ Table created\n";
    }

    // Insert 10 rows (id=1..10, value=10..100)
    for (int i = 1; i <= 10; i++)
    {
        std::string sql = "INSERT INTO numbers (id, value) VALUES (" +
                         std::to_string(i) + ", " +
                         std::to_string(i * 10) + ")";

        parser::Lexer lexer(sql, pool);
        parser::Parser parser_(lexer, pool);
        auto stmt = parser_.parse();

        sblr::BytecodeGenerator generator(pool, db.get());
        auto bytecode_result = generator.generate(stmt.get());
        sblr::Executor executor(db.get());
        auto exec_result = executor.execute(bytecode_result.bytecode());

        if (!exec_result.success())
        {
            std::cerr << "Insert error: " << exec_result.error() << "\n";
            return;
        }
    }

    std::cout << "✓ Inserted 10 rows (id=1..10, value=10..100)\n";

    // Test LIMIT only
    {
        std::string sql = "SELECT id, value FROM numbers ORDER BY id ASC LIMIT 3";
        parser::Lexer lexer(sql, pool);
        parser::Parser parser_(lexer, pool);
        auto stmt = parser_.parse();

        if (!stmt || parser_.hasErrors())
        {
            std::cerr << "Parse error: " << parser_.errors()[0] << "\n";
            return;
        }

        sblr::BytecodeGenerator generator(pool, db.get());
        auto bytecode_result = generator.generate(stmt.get());

        if (!bytecode_result.success())
        {
            std::cerr << "Bytecode generation error: " << bytecode_result.errors()[0] << "\n";
            return;
        }

        sblr::Executor executor(db.get());
        auto exec_result = executor.execute(bytecode_result.bytecode());

        if (!exec_result.success())
        {
            std::cerr << "SELECT LIMIT execution error: " << exec_result.error() << "\n";
            return;
        }

        if (!exec_result.hasResultSet())
        {
            std::cerr << "Expected result set for LIMIT query\n";
            return;
        }

        auto* rs = exec_result.resultSet();
        if (rs->rowCount() != 3)
        {
            std::cerr << "Expected 3 rows with LIMIT 3, got " << rs->rowCount() << "\n";
            return;
        }

        // Verify we got the first 3 rows (id=1,2,3)
        for (size_t i = 0; i < rs->rowCount(); i++)
        {
            int64_t id = rs->getValue(i, 0).toInt64();
            if (id != static_cast<int64_t>(i + 1))
            {
                std::cerr << "Expected id=" << (i + 1) << " at row " << i << ", got id=" << id << "\n";
                return;
            }
        }

        std::cout << "✓ LIMIT 3: returned rows with id=[1, 2, 3]\n";
    }

    // Test OFFSET only
    {
        std::string sql = "SELECT id FROM numbers ORDER BY id ASC OFFSET 7";
        parser::Lexer lexer(sql, pool);
        parser::Parser parser_(lexer, pool);
        auto stmt = parser_.parse();

        if (!stmt || parser_.hasErrors())
        {
            std::cerr << "Parse error: " << parser_.errors()[0] << "\n";
            return;
        }

        sblr::BytecodeGenerator generator(pool, db.get());
        auto bytecode_result = generator.generate(stmt.get());

        if (!bytecode_result.success())
        {
            std::cerr << "Bytecode generation error: " << bytecode_result.errors()[0] << "\n";
            return;
        }

        sblr::Executor executor(db.get());
        auto exec_result = executor.execute(bytecode_result.bytecode());

        if (!exec_result.success())
        {
            std::cerr << "SELECT OFFSET execution error: " << exec_result.error() << "\n";
            return;
        }

        if (!exec_result.hasResultSet())
        {
            std::cerr << "Expected result set for OFFSET query\n";
            return;
        }

        auto* rs = exec_result.resultSet();
        if (rs->rowCount() != 3)
        {
            std::cerr << "Expected 3 rows with OFFSET 7 (10 total - 7 skipped), got " << rs->rowCount() << "\n";
            return;
        }

        // Verify we got rows id=8,9,10
        for (size_t i = 0; i < rs->rowCount(); i++)
        {
            int64_t id = rs->getValue(i, 0).toInt64();
            int64_t expected_id = 8 + i;
            if (id != expected_id)
            {
                std::cerr << "Expected id=" << expected_id << " at row " << i << ", got id=" << id << "\n";
                return;
            }
        }

        std::cout << "✓ OFFSET 7: returned rows with id=[8, 9, 10]\n";
    }

    // Test LIMIT with OFFSET
    {
        std::string sql = "SELECT id, value FROM numbers ORDER BY id ASC LIMIT 3 OFFSET 5";
        parser::Lexer lexer(sql, pool);
        parser::Parser parser_(lexer, pool);
        auto stmt = parser_.parse();

        if (!stmt || parser_.hasErrors())
        {
            std::cerr << "Parse error: " << parser_.errors()[0] << "\n";
            return;
        }

        sblr::BytecodeGenerator generator(pool, db.get());
        auto bytecode_result = generator.generate(stmt.get());

        if (!bytecode_result.success())
        {
            std::cerr << "Bytecode generation error: " << bytecode_result.errors()[0] << "\n";
            return;
        }

        sblr::Executor executor(db.get());
        auto exec_result = executor.execute(bytecode_result.bytecode());

        if (!exec_result.success())
        {
            std::cerr << "SELECT LIMIT OFFSET execution error: " << exec_result.error() << "\n";
            return;
        }

        if (!exec_result.hasResultSet())
        {
            std::cerr << "Expected result set for LIMIT OFFSET query\n";
            return;
        }

        auto* rs = exec_result.resultSet();
        if (rs->rowCount() != 3)
        {
            std::cerr << "Expected 3 rows with LIMIT 3 OFFSET 5, got " << rs->rowCount() << "\n";
            return;
        }

        // Verify we got rows id=6,7,8 (skip 5, take 3)
        for (size_t i = 0; i < rs->rowCount(); i++)
        {
            int64_t id = rs->getValue(i, 0).toInt64();
            int64_t expected_id = 6 + i;
            if (id != expected_id)
            {
                std::cerr << "Expected id=" << expected_id << " at row " << i << ", got id=" << id << "\n";
                return;
            }
        }

        std::cout << "✓ LIMIT 3 OFFSET 5: returned rows with id=[6, 7, 8]\n";
    }

    // Test LIMIT without ORDER BY
    {
        std::string sql = "SELECT id FROM numbers LIMIT 2";
        parser::Lexer lexer(sql, pool);
        parser::Parser parser_(lexer, pool);
        auto stmt = parser_.parse();

        if (!stmt || parser_.hasErrors())
        {
            std::cerr << "Parse error: " << parser_.errors()[0] << "\n";
            return;
        }

        sblr::BytecodeGenerator generator(pool, db.get());
        auto bytecode_result = generator.generate(stmt.get());

        if (!bytecode_result.success())
        {
            std::cerr << "Bytecode generation error: " << bytecode_result.errors()[0] << "\n";
            return;
        }

        sblr::Executor executor(db.get());
        auto exec_result = executor.execute(bytecode_result.bytecode());

        if (!exec_result.success())
        {
            std::cerr << "SELECT LIMIT (no ORDER BY) execution error: " << exec_result.error() << "\n";
            return;
        }

        if (!exec_result.hasResultSet())
        {
            std::cerr << "Expected result set for LIMIT without ORDER BY\n";
            return;
        }

        auto* rs = exec_result.resultSet();
        if (rs->rowCount() != 2)
        {
            std::cerr << "Expected 2 rows with LIMIT 2, got " << rs->rowCount() << "\n";
            return;
        }

        std::cout << "✓ LIMIT 2 (no ORDER BY): returned 2 rows\n";
    }

    std::cout << "\n=== LIMIT/OFFSET Test PASSED ===\n";
}

int main()
{
    std::cout << "=== LIMIT/OFFSET Executor Tests ===\n";

    try
    {
        testLimitOffset();

        std::cout << "\n=== ALL TESTS PASSED ===\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n!!! TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
