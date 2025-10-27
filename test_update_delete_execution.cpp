// Test UPDATE and DELETE execution end-to-end
// Phase 1 Task 1.6.1 and 1.6.2 validation

#include "scratchbird/parser/lexer.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/semantic_analyzer.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/core/database.h"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace scratchbird;

void testUpdateExecution()
{
    std::cout << "\n=== Testing UPDATE Execution ===\n\n";

    // Remove existing test database
    std::filesystem::remove_all("test_update_delete_exec.db");

    // Create database
    core::ErrorContext err_ctx;
    auto db = std::make_unique<core::Database>("test_update_delete_exec.db", &err_ctx);
    if (!db || !db->isOpen())
    {
        std::cerr << "Failed to create database\n";
        return;
    }

    // Create table: CREATE TABLE users (id INT32, name VARCHAR(50), age INT32)
    parser::StringPool pool;
    {
        std::string sql = "CREATE TABLE users (id INT32, name VARCHAR(50), age INT32)";
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
    {
        std::string sql = "INSERT INTO users (id, name, age) VALUES (1, 'Alice', 30)";
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
        std::cout << "✓ Inserted Alice (id=1, age=30)\n";
    }

    {
        std::string sql = "INSERT INTO users (id, name, age) VALUES (2, 'Bob', 25)";
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
        std::cout << "✓ Inserted Bob (id=2, age=25)\n";
    }

    // Test UPDATE with WHERE clause
    {
        std::string sql = "UPDATE users SET age = 31 WHERE id = 1";
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
            std::cerr << "UPDATE execution error: " << exec_result.error() << "\n";
            return;
        }

        std::cout << "✓ UPDATE executed successfully (SET age=31 WHERE id=1)\n";
    }

    // Verify the update worked
    {
        std::string sql = "SELECT id, name, age FROM users WHERE id = 1";
        parser::Lexer lexer(sql, pool);
        parser::Parser parser_(lexer, pool);
        auto stmt = parser_.parse();

        sblr::BytecodeGenerator generator(pool, db.get());
        auto bytecode_result = generator.generate(stmt.get());
        sblr::Executor executor(db.get());
        auto exec_result = executor.execute(bytecode_result.bytecode());

        if (!exec_result.success() || !exec_result.hasResultSet())
        {
            std::cerr << "SELECT verification error\n";
            return;
        }

        auto* rs = exec_result.resultSet();
        if (rs->rowCount() != 1)
        {
            std::cerr << "Expected 1 row, got " << rs->rowCount() << "\n";
            return;
        }

        int64_t age = rs->getValue(0, 2).toInt64();
        if (age != 31)
        {
            std::cerr << "Expected age=31, got age=" << age << "\n";
            return;
        }

        std::cout << "✓ Verified: Alice's age is now 31\n";
    }

    std::cout << "\n=== UPDATE Test PASSED ===\n";
}

void testDeleteExecution()
{
    std::cout << "\n=== Testing DELETE Execution ===\n\n";

    // Remove existing test database
    std::filesystem::remove_all("test_delete_exec.db");

    // Create database
    core::ErrorContext err_ctx;
    auto db = std::make_unique<core::Database>("test_delete_exec.db", &err_ctx);
    if (!db || !db->isOpen())
    {
        std::cerr << "Failed to create database\n";
        return;
    }

    // Create table
    parser::StringPool pool;
    {
        std::string sql = "CREATE TABLE products (id INT32, name VARCHAR(50), price INT32)";
        parser::Lexer lexer(sql, pool);
        parser::Parser parser_(lexer, pool);
        auto stmt = parser_.parse();

        sblr::BytecodeGenerator generator(pool, db.get());
        auto bytecode_result = generator.generate(stmt.get());
        sblr::Executor executor(db.get());
        auto exec_result = executor.execute(bytecode_result.bytecode());

        if (!exec_result.success())
        {
            std::cerr << "Table creation error: " << exec_result.error() << "\n";
            return;
        }

        std::cout << "✓ Table created\n";
    }

    // Insert test data
    {
        std::string sql = "INSERT INTO products (id, name, price) VALUES (1, 'Widget', 100)";
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
        std::cout << "✓ Inserted Widget (id=1, price=100)\n";
    }

    {
        std::string sql = "INSERT INTO products (id, name, price) VALUES (2, 'Gadget', 200)";
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
        std::cout << "✓ Inserted Gadget (id=2, price=200)\n";
    }

    // Test DELETE with WHERE clause
    {
        std::string sql = "DELETE FROM products WHERE id = 1";
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
            std::cerr << "DELETE execution error: " << exec_result.error() << "\n";
            return;
        }

        std::cout << "✓ DELETE executed successfully (WHERE id=1)\n";
    }

    // Verify the delete worked
    {
        std::string sql = "SELECT id, name, price FROM products";
        parser::Lexer lexer(sql, pool);
        parser::Parser parser_(lexer, pool);
        auto stmt = parser_.parse();

        sblr::BytecodeGenerator generator(pool, db.get());
        auto bytecode_result = generator.generate(stmt.get());
        sblr::Executor executor(db.get());
        auto exec_result = executor.execute(bytecode_result.bytecode());

        if (!exec_result.success() || !exec_result.hasResultSet())
        {
            std::cerr << "SELECT verification error\n";
            return;
        }

        auto* rs = exec_result.resultSet();
        if (rs->rowCount() != 1)
        {
            std::cerr << "Expected 1 row, got " << rs->rowCount() << "\n";
            return;
        }

        int64_t id = rs->getValue(0, 0).toInt64();
        if (id != 2)
        {
            std::cerr << "Expected id=2, got id=" << id << "\n";
            return;
        }

        std::cout << "✓ Verified: Only Gadget (id=2) remains\n";
    }

    std::cout << "\n=== DELETE Test PASSED ===\n";
}

int main()
{
    std::cout << "=== UPDATE/DELETE Executor Tests ===\n";

    try
    {
        testUpdateExecution();
        testDeleteExecution();

        std::cout << "\n=== ALL TESTS PASSED ===\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n!!! TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
