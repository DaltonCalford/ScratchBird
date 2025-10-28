// Test ORDER BY execution end-to-end
// Phase 1 Task 1.6.4 validation

#include "scratchbird/parser/lexer.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/core/database.h"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace scratchbird;

void testSimpleSorting()
{
    std::cout << "\n=== Testing Simple Sorting (ASC/DESC) ===\n\n";

    // Remove existing test database
    std::filesystem::remove_all("test_sort_exec.db");

    // Create database
    core::ErrorContext err_ctx;
    auto db = std::make_unique<core::Database>("test_sort_exec.db", &err_ctx);
    if (!db || !db->isOpen())
    {
        std::cerr << "Failed to create database\n";
        return;
    }

    // Create table
    parser::StringPool pool;
    {
        std::string sql = "CREATE TABLE employees (id INT32, name VARCHAR(50), age INT32, salary INT32)";
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

    // Insert test data (unsorted)
    std::vector<std::tuple<int, std::string, int, int>> test_data = {
        {3, "Charlie", 35, 75000},
        {1, "Alice", 30, 60000},
        {4, "David", 28, 55000},
        {2, "Bob", 32, 70000}
    };

    for (const auto& [id, name, age, salary] : test_data)
    {
        std::string sql = "INSERT INTO employees (id, name, age, salary) VALUES (" +
                         std::to_string(id) + ", '" +
                         name + "', " +
                         std::to_string(age) + ", " +
                         std::to_string(salary) + ")";

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

    std::cout << "✓ Inserted 4 test rows (unsorted)\n";

    // Test ORDER BY id ASC
    {
        std::string sql = "SELECT id, name FROM employees ORDER BY id ASC";
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
            std::cerr << "SELECT ORDER BY execution error: " << exec_result.error() << "\n";
            return;
        }

        if (!exec_result.hasResultSet())
        {
            std::cerr << "Expected result set for ORDER BY query\n";
            return;
        }

        auto* rs = exec_result.resultSet();
        if (rs->rowCount() != 4)
        {
            std::cerr << "Expected 4 rows, got " << rs->rowCount() << "\n";
            return;
        }

        // Verify ascending order by id
        for (size_t i = 0; i < rs->rowCount(); i++)
        {
            int64_t id = rs->getValue(i, 0).toInt64();
            if (id != static_cast<int64_t>(i + 1))
            {
                std::cerr << "Expected id=" << (i + 1) << " at row " << i << ", got id=" << id << "\n";
                return;
            }
        }

        std::cout << "✓ ORDER BY id ASC: ids are [1, 2, 3, 4]\n";
    }

    // Test ORDER BY age DESC
    {
        std::string sql = "SELECT name, age FROM employees ORDER BY age DESC";
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
            std::cerr << "SELECT ORDER BY DESC execution error: " << exec_result.error() << "\n";
            return;
        }

        if (!exec_result.hasResultSet())
        {
            std::cerr << "Expected result set for ORDER BY DESC query\n";
            return;
        }

        auto* rs = exec_result.resultSet();

        // Verify descending order by age: 35, 32, 30, 28
        std::vector<int64_t> expected_ages = {35, 32, 30, 28};
        for (size_t i = 0; i < rs->rowCount(); i++)
        {
            int64_t age = rs->getValue(i, 1).toInt64();
            if (age != expected_ages[i])
            {
                std::cerr << "Expected age=" << expected_ages[i] << " at row " << i << ", got age=" << age << "\n";
                return;
            }
        }

        std::cout << "✓ ORDER BY age DESC: ages are [35, 32, 30, 28]\n";
    }

    // Test multi-key sorting: ORDER BY age DESC, name ASC
    {
        // Add another row with same age as Charlie
        std::string sql = "INSERT INTO employees (id, name, age, salary) VALUES (5, 'Aaron', 35, 80000)";
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

        std::cout << "✓ Added Aaron (age=35) for multi-key sort test\n";
    }

    {
        std::string sql = "SELECT name, age FROM employees ORDER BY age DESC, name ASC";
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
            std::cerr << "Multi-key ORDER BY execution error: " << exec_result.error() << "\n";
            return;
        }

        if (!exec_result.hasResultSet())
        {
            std::cerr << "Expected result set for multi-key ORDER BY\n";
            return;
        }

        auto* rs = exec_result.resultSet();

        // First two rows should be age=35, sorted by name: Aaron, Charlie
        std::string name0 = rs->getValue(0, 0).toString();
        std::string name1 = rs->getValue(1, 0).toString();

        if (name0 != "Aaron" || name1 != "Charlie")
        {
            std::cerr << "Expected [Aaron, Charlie] for age=35, got [" << name0 << ", " << name1 << "]\n";
            return;
        }

        std::cout << "✓ Multi-key ORDER BY age DESC, name ASC: [Aaron, Charlie] for age=35\n";
    }

    std::cout << "\n=== Sorting Test PASSED ===\n";
}

int main()
{
    std::cout << "=== Sorting Executor Tests ===\n";

    try
    {
        testSimpleSorting();

        std::cout << "\n=== ALL TESTS PASSED ===\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n!!! TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
