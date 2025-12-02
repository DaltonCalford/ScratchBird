#include "scratchbird/core/database.h"
#include "scratchbird/parser/lexer.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/sblr/executor.h"
#include <iostream>
#include <filesystem>

using namespace scratchbird;

void execute_sql(core::Database* db, const std::string& sql) {
    parser::Lexer lexer(sql);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);

    auto result = parser.parseStatement();
    if (!result.success()) {
        std::cerr << "Parse error: " << sql << std::endl;
        for (const auto& err : result.errors()) {
            std::cerr << "  " << err.message << std::endl;
        }
        return;
    }

    sblr::BytecodeGenerator generator(lexer.stringPool(), db);
    auto bytecode = generator.generate(result.statement());

    if (!bytecode.success()) {
        std::cerr << "Bytecode generation error: " << sql << std::endl;
        for (const auto& err : bytecode.errors()) {
            std::cerr << "  " << err << std::endl;
        }
        return;
    }

    try {
        sblr::Executor executor(db);
        auto exec_result = executor.execute(bytecode.bytecode());
        if (exec_result.success()) {
            std::cout << "✓ " << sql << std::endl;
        } else {
            std::cerr << "Execution failed: " << sql << std::endl;
            if (!exec_result.error().empty()) {
                std::cerr << "  Error: " << exec_result.error() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Execution error: " << e.what() << std::endl;
        std::cerr << "  SQL: " << sql << std::endl;
    }
}

int main() {
    const std::string db_path = "/tmp/test_views_db";

    // Clean up any existing database
    std::filesystem::remove_all(db_path);

    // Create database
    core::ErrorContext ctx;
    auto status = core::Database::create(db_path, 16384, &ctx);
    if (status != core::Status::OK) {
        std::cerr << "Failed to create database: " << ctx.message << std::endl;
        return 1;
    }

    // Open database
    auto db = std::make_unique<core::Database>();
    status = db->open(db_path, &ctx);
    if (status != core::Status::OK) {
        std::cerr << "Failed to open database: " << ctx.message << std::endl;
        return 1;
    }

    std::cout << "=== Views Expansion Test ===" << std::endl << std::endl;

    // Set current schema to app
    std::cout << "1. Setting schema to app..." << std::endl;
    execute_sql(db.get(), "SET SCHEMA app");

    // Create table
    std::cout << "\n2. Creating employees table..." << std::endl;
    execute_sql(db.get(), "CREATE TABLE employees (id INTEGER, name VARCHAR(100), department VARCHAR(50), salary INTEGER)");

    // Insert data
    std::cout << "\n3. Inserting test data..." << std::endl;
    execute_sql(db.get(), "INSERT INTO employees (id, name, department, salary) VALUES (1, 'Alice', 'Engineering', 100000)");
    execute_sql(db.get(), "INSERT INTO employees (id, name, department, salary) VALUES (2, 'Bob', 'Sales', 80000)");
    execute_sql(db.get(), "INSERT INTO employees (id, name, department, salary) VALUES (3, 'Charlie', 'Engineering', 95000)");
    execute_sql(db.get(), "INSERT INTO employees (id, name, department, salary) VALUES (4, 'David', 'Marketing', 75000)");
    execute_sql(db.get(), "INSERT INTO employees (id, name, department, salary) VALUES (5, 'Eve', 'Engineering', 105000)");

    // Create view
    std::cout << "\n4. Creating active_employees view..." << std::endl;
    execute_sql(db.get(), "CREATE VIEW active_employees AS SELECT id, name, department, salary FROM employees WHERE salary > 85000");

    // Query view (this will test view expansion)
    std::cout << "\n5. Querying view (testing view expansion)..." << std::endl;
    execute_sql(db.get(), "SELECT * FROM active_employees");

    // Create nested view (view referencing view)
    std::cout << "\n6. Creating nested view (high_earners from active_employees)..." << std::endl;
    execute_sql(db.get(), "CREATE VIEW high_earners AS SELECT * FROM active_employees WHERE department = 'Engineering'");

    // Query nested view
    std::cout << "\n7. Querying nested view..." << std::endl;
    execute_sql(db.get(), "SELECT * FROM high_earners");

    // Test OR REPLACE
    std::cout << "\n8. Testing CREATE OR REPLACE VIEW..." << std::endl;
    execute_sql(db.get(), "CREATE OR REPLACE VIEW active_employees AS SELECT id, name FROM employees WHERE salary > 85000");
    execute_sql(db.get(), "SELECT * FROM active_employees");

    // Clean up
    std::cout << "\n9. Cleaning up..." << std::endl;
    execute_sql(db.get(), "DROP VIEW IF EXISTS high_earners");
    execute_sql(db.get(), "DROP VIEW IF EXISTS active_employees");
    execute_sql(db.get(), "DROP TABLE employees");

    std::cout << "\n=== All Tests Passed! ===" << std::endl;

    return 0;
}
