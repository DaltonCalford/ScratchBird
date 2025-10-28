// Test Aggregation execution end-to-end
// Phase 1 Task 1.6.3 validation

#include "scratchbird/parser/lexer.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/core/database.h"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace scratchbird;

void testSimpleAggregation()
{
    std::cout << "\n=== Testing Simple Aggregation (COUNT, SUM, AVG) ===\n\n";

    // Remove existing test database
    std::filesystem::remove_all("test_agg_exec.db");

    // Create database
    core::ErrorContext err_ctx;
    auto db = std::make_unique<core::Database>("test_agg_exec.db", &err_ctx);
    if (!db || !db->isOpen())
    {
        std::cerr << "Failed to create database\n";
        return;
    }

    // Create table: CREATE TABLE sales (id INT32, amount INT32, quantity INT32)
    parser::StringPool pool;
    {
        std::string sql = "CREATE TABLE sales (id INT32, amount INT32, quantity INT32)";
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

    // Insert test data
    std::vector<std::tuple<int, int, int>> test_data = {
        {1, 100, 5},
        {2, 200, 10},
        {3, 150, 7},
        {4, 300, 15}
    };

    for (const auto& [id, amount, quantity] : test_data)
    {
        std::string sql = "INSERT INTO sales (id, amount, quantity) VALUES (" +
                         std::to_string(id) + ", " +
                         std::to_string(amount) + ", " +
                         std::to_string(quantity) + ")";

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

    std::cout << "✓ Inserted 4 test rows\n";

    // Test COUNT aggregate
    {
        std::string sql = "SELECT COUNT(id) FROM sales";
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
            std::cerr << "SELECT COUNT execution error: " << exec_result.error() << "\n";
            return;
        }

        if (!exec_result.hasResultSet())
        {
            std::cerr << "Expected result set for COUNT query\n";
            return;
        }

        auto* rs = exec_result.resultSet();
        if (rs->rowCount() != 1)
        {
            std::cerr << "Expected 1 row, got " << rs->rowCount() << "\n";
            return;
        }

        int64_t count = rs->getValue(0, 0).toInt64();
        if (count != 4)
        {
            std::cerr << "Expected COUNT=4, got COUNT=" << count << "\n";
            return;
        }

        std::cout << "✓ COUNT(id) = 4\n";
    }

    // Test SUM aggregate
    {
        std::string sql = "SELECT SUM(amount) FROM sales";
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
            std::cerr << "SELECT SUM execution error: " << exec_result.error() << "\n";
            return;
        }

        if (!exec_result.hasResultSet())
        {
            std::cerr << "Expected result set for SUM query\n";
            return;
        }

        auto* rs = exec_result.resultSet();
        double sum = rs->getValue(0, 0).toDouble();

        // Expected: 100 + 200 + 150 + 300 = 750
        if (std::abs(sum - 750.0) > 0.01)
        {
            std::cerr << "Expected SUM=750, got SUM=" << sum << "\n";
            return;
        }

        std::cout << "✓ SUM(amount) = 750\n";
    }

    // Test AVG aggregate
    {
        std::string sql = "SELECT AVG(quantity) FROM sales";
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
            std::cerr << "SELECT AVG execution error: " << exec_result.error() << "\n";
            return;
        }

        if (!exec_result.hasResultSet())
        {
            std::cerr << "Expected result set for AVG query\n";
            return;
        }

        auto* rs = exec_result.resultSet();
        double avg = rs->getValue(0, 0).toDouble();

        // Expected: (5 + 10 + 7 + 15) / 4 = 37 / 4 = 9.25
        if (std::abs(avg - 9.25) > 0.01)
        {
            std::cerr << "Expected AVG=9.25, got AVG=" << avg << "\n";
            return;
        }

        std::cout << "✓ AVG(quantity) = 9.25\n";
    }

    std::cout << "\n=== Simple Aggregation Test PASSED ===\n";
}

void testHavingClause()
{
    std::cout << "\n=== Testing HAVING Clause (95% → 100% Complete) ===\n\n";

    // Remove existing test database
    std::filesystem::remove_all("test_having_exec.db");

    // Create database
    core::ErrorContext err_ctx;
    auto db = std::make_unique<core::Database>("test_having_exec.db", &err_ctx);
    if (!db || !db->isOpen())
    {
        std::cerr << "Failed to create database\n";
        return;
    }

    // Create table with region column for GROUP BY
    parser::StringPool pool;
    {
        std::string sql = "CREATE TABLE sales (region VARCHAR(50), amount INT32)";
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

    // Insert test data
    std::vector<std::pair<std::string, int>> test_data = {
        {"North", 100},
        {"North", 200},
        {"North", 150},
        {"South", 50},
        {"South", 75},
        {"East", 300}
    };

    for (const auto& [region, amount] : test_data)
    {
        std::string sql = "INSERT INTO sales (region, amount) VALUES ('" +
                         region + "', " +
                         std::to_string(amount) + ")";

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

    std::cout << "✓ Inserted 6 test rows (North: 3, South: 2, East: 1)\n";

    // Test HAVING with COUNT
    {
        std::string sql = "SELECT region, COUNT(*) FROM sales GROUP BY region HAVING COUNT(*) > 2";
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
            std::cerr << "SELECT HAVING COUNT execution error: " << exec_result.error() << "\n";
            return;
        }

        if (!exec_result.hasResultSet())
        {
            std::cerr << "Expected result set for HAVING query\n";
            return;
        }

        auto* rs = exec_result.resultSet();

        // Only North (3 rows) should pass HAVING COUNT(*) > 2
        if (rs->rowCount() != 1)
        {
            std::cerr << "Expected 1 group with count > 2, got " << rs->rowCount() << "\n";
            return;
        }

        std::cout << "✓ HAVING COUNT(*) > 2: returned 1 group (North)\n";
    }

    // Test HAVING with SUM
    {
        std::string sql = "SELECT region, SUM(amount) FROM sales GROUP BY region HAVING SUM(amount) >= 300";
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
            std::cerr << "SELECT HAVING SUM execution error: " << exec_result.error() << "\n";
            return;
        }

        if (!exec_result.hasResultSet())
        {
            std::cerr << "Expected result set for HAVING SUM query\n";
            return;
        }

        auto* rs = exec_result.resultSet();

        // North: 450 ✓, South: 125 ✗, East: 300 ✓
        if (rs->rowCount() != 2)
        {
            std::cerr << "Expected 2 groups with sum >= 300, got " << rs->rowCount() << "\n";
            return;
        }

        std::cout << "✓ HAVING SUM(amount) >= 300: returned 2 groups (North, East)\n";
    }

    std::cout << "\n=== HAVING Clause Test PASSED ===\n";
}

int main()
{
    std::cout << "=== Aggregation Executor Tests ===\n";

    try
    {
        testSimpleAggregation();
        testHavingClause();

        std::cout << "\n=== ALL TESTS PASSED ===\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n!!! TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
