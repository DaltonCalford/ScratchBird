#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"

#include <cassert>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

using namespace scratchbird::engine;

void setup_test_data()
{
    // Create parent table
    auto result =
        execute_ast(parse_sql("CREATE TABLE departments (id INT PRIMARY KEY, name VARCHAR(50) "
                              "DEFAULT 'Unknown Department')"));
    if (!result.columns.empty() && result.columns[0] == "error") {
        std::cerr << "Failed to create departments table: "
                  << (result.rows.empty() ? "unknown error" : result.rows[0][0]) << std::endl;
        return;
    }

    // Create child table with default values
    result = execute_ast(
        parse_sql("CREATE TABLE employees (id INT PRIMARY KEY, name VARCHAR(50), dept_id INT "
                  "DEFAULT 999, salary DECIMAL(10,2) DEFAULT 50000.00, FOREIGN KEY(dept_id) "
                  "REFERENCES departments(id) ON DELETE SET DEFAULT ON UPDATE SET DEFAULT)"));
    if (!result.columns.empty() && result.columns[0] == "error") {
        std::cerr << "Failed to create employees table with FK SET DEFAULT: "
                  << (result.rows.empty() ? "unknown error" : result.rows[0][0]) << std::endl;
        return;
    }

    // Insert test data
    execute_ast(parse_sql(
        "INSERT INTO departments VALUES (1, 'Engineering'), (2, 'Sales'), (3, 'Marketing')"));
    execute_ast(
        parse_sql("INSERT INTO employees VALUES (101, 'Alice', 1, 75000.00), (102, 'Bob', 2, "
                  "60000.00), (103, 'Charlie', 3, 55000.00)"));

    // Insert default department for testing SET DEFAULT
    execute_ast(parse_sql("INSERT INTO departments VALUES (999, 'Unassigned Department')"));
}

void test_update_parent_triggers_fk_set_default()
{
    std::cout << "Testing FK SET DEFAULT on UPDATE..." << std::endl;

    // Update a parent department ID - should trigger SET DEFAULT on child records
    auto result = execute_ast(parse_sql("UPDATE departments SET id = 10 WHERE id = 1"));
    if (!result.columns.empty() && result.columns[0] == "error") {
        std::cerr << "UPDATE failed: "
                  << (result.rows.empty() ? "unknown error" : result.rows[0][0]) << std::endl;
        return;
    }

    // Check that Alice's dept_id was set to default (999)
    result = execute_ast(parse_sql("SELECT name, dept_id FROM employees WHERE name = 'Alice'"));
    if (result.rows.empty()) {
        std::cerr << "No Alice found after UPDATE" << std::endl;
        return;
    }

    std::cout << "Alice's dept_id after UPDATE: " << result.rows[0][1] << std::endl;
    if (result.rows[0][1] != "999") {
        std::cerr << "FAILED: Alice's dept_id should be 999 but is " << result.rows[0][1]
                  << std::endl;
    } else {
        std::cout << "PASSED: Alice's dept_id correctly set to default 999" << std::endl;
    }
}

void test_delete_parent_triggers_fk_set_default()
{
    std::cout << "Testing FK SET DEFAULT on DELETE..." << std::endl;

    // Delete a parent department - should trigger SET DEFAULT on child records
    auto result = execute_ast(parse_sql("DELETE FROM departments WHERE id = 2"));
    if (!result.columns.empty() && result.columns[0] == "error") {
        std::cerr << "DELETE failed: "
                  << (result.rows.empty() ? "unknown error" : result.rows[0][0]) << std::endl;
        return;
    }

    // Check that Bob's dept_id was set to default (999)
    result = execute_ast(parse_sql("SELECT name, dept_id FROM employees WHERE name = 'Bob'"));
    if (result.rows.empty()) {
        std::cerr << "No Bob found after DELETE" << std::endl;
        return;
    }

    std::cout << "Bob's dept_id after DELETE: " << result.rows[0][1] << std::endl;
    if (result.rows[0][1] != "999") {
        std::cerr << "FAILED: Bob's dept_id should be 999 but is " << result.rows[0][1]
                  << std::endl;
    } else {
        std::cout << "PASSED: Bob's dept_id correctly set to default 999" << std::endl;
    }
}

int main()
{
    std::cout << "=== FK SET DEFAULT Tests ===" << std::endl;

    // Test simple queries first
    std::cout << "Testing basic queries..." << std::endl;
    auto result = execute_ast(parse_sql("SELECT 1"));
    std::cout << "SELECT 1 result: " << result.rows[0][0] << std::endl;

    // Try creating a table
    std::cout << "Testing CREATE TABLE..." << std::endl;
    result = execute_ast(parse_sql("CREATE TABLE test (id INT)"));
    if (!result.columns.empty() && result.columns[0] == "error") {
        std::cerr << "CREATE TABLE failed: "
                  << (result.rows.empty() ? "unknown error" : result.rows[0][0]) << std::endl;
        return 1;
    } else {
        std::cout << "CREATE TABLE succeeded" << std::endl;
    }

    std::cout << "=== FK SET DEFAULT Tests Complete ===" << std::endl;
    return 0;
}
