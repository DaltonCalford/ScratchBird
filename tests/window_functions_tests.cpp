#include "scratchbird/capi.h"
#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace scratchbird::engine
{

    static std::string tempdb()
    {
        // Use project-local temp dir to avoid /tmp space constraints
        const char* root = "/home/dcalford/CliWork/ScratchBird/temp";
        mkdir(root, 0755);
        std::ostringstream oss;
        oss << root << "/db_" << getpid() << "_" << (unsigned long long)time(nullptr);
        return oss.str();
    }

    static void create_db_and_set_path(const std::string& base)
    {
        SB_CreateDbOptions o{};
        o.page_size = 4096;
        SB_Database* db = nullptr;
        auto st = sb_create_database(base.c_str(), &o, &db);
        (void)st;
        if (db)
            sb_close_database(db);
        set_executor_db_path(base);
        // Ensure catalog roots are bootstrapped before any DDL
        CatalogManager cm(get_executor_db_path());
        cm.bootstrap_if_needed();
        // Ensure default schema 'public' exists for SELECT executor and trigger runners
        if (!cm.lookup_schema_oid_by_name("public")) {
            UuidBytes gen{};
            {
                std::hash<std::string> h;
                auto v = h(std::string("public"));
                memcpy(gen.data(), &v, std::min(sizeof(v), gen.size()));
            }
            cm.create_schema(gen, "public", std::nullopt, "public schema");
        }
    }

    // Helper function to execute SQL and check for errors
    ExecutionResult execute_sql(const std::string& sql)
    {
        std::string sql_upper = sql;
        std::transform(sql_upper.begin(), sql_upper.end(), sql_upper.begin(), ::toupper);

        // Remove leading whitespace
        size_t start = sql_upper.find_first_not_of(" \t\n\r");
        if (start != std::string::npos) {
            sql_upper = sql_upper.substr(start);
        }

        // Route to appropriate executor based on SQL type
        if (sql_upper.substr(0, 6) == "INSERT") {
            return execute_insert_sql(sql);
        } else if (sql_upper.substr(0, 6) == "UPDATE") {
            return execute_update_sql(sql);
        } else if (sql_upper.substr(0, 6) == "DELETE") {
            return execute_delete_sql(sql);
        } else if (sql_upper.substr(0, 6) == "SELECT") {
            return execute_select_sql(sql);
        } else {
            // For DDL statements, use AST-based execution
            auto ast = parse_sql(sql);
            return execute_ast(ast);
        }
    }

    bool has_error(const ExecutionResult& result)
    {
        return result.columns.size() > 0 && result.columns[0] == "error";
    }

    std::string get_error_message(const ExecutionResult& result)
    {
        if (has_error(result) && !result.rows.empty()) {
            return result.rows[0][0];
        }
        return "unknown error";
    }

    void print_result(const ExecutionResult& result)
    {
        if (has_error(result)) {
            std::cout << "❌ Error: " << get_error_message(result) << std::endl;
            return;
        }

        // Print columns
        for (size_t i = 0; i < result.columns.size(); ++i) {
            std::cout << result.columns[i];
            if (i < result.columns.size() - 1)
                std::cout << " | ";
        }
        std::cout << std::endl;

        // Print separator
        for (size_t i = 0; i < result.columns.size(); ++i) {
            std::cout << std::string(result.columns[i].length(), '-');
            if (i < result.columns.size() - 1)
                std::cout << "-+-";
        }
        std::cout << std::endl;

        // Print rows
        for (const auto& row : result.rows) {
            for (size_t i = 0; i < row.size() && i < result.columns.size(); ++i) {
                std::cout << row[i];
                if (i < std::min(row.size(), result.columns.size()) - 1)
                    std::cout << " | ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    void test_window_functions()
    {
        std::cout << "=== Testing Window Functions ===" << std::endl;

        // Setup database
        std::string db_path = tempdb();
        create_db_and_set_path(db_path);
        std::cout << "✅ Database created at: " << db_path << std::endl;

        try {
            // Setup test data
            auto result = execute_sql(R"(
            CREATE TABLE sales (
                id INTEGER PRIMARY KEY,
                department VARCHAR(50),
                employee VARCHAR(50),
                sales_amount DECIMAL(10,2),
                quarter INTEGER
            )
        )");

            if (has_error(result)) {
                std::cerr << "Failed to create table: " << get_error_message(result) << std::endl;
                return;
            }
            std::cout << "✅ Created sales table" << std::endl;

            // Insert test data
            const std::vector<std::string> inserts = {
                "INSERT INTO sales VALUES (1, 'Engineering', 'Alice', 50000, 1)",
                "INSERT INTO sales VALUES (2, 'Engineering', 'Bob', 75000, 1)",
                "INSERT INTO sales VALUES (3, 'Sales', 'Charlie', 60000, 1)",
                "INSERT INTO sales VALUES (4, 'Sales', 'David', 80000, 1)",
                "INSERT INTO sales VALUES (5, 'Engineering', 'Alice', 55000, 2)",
                "INSERT INTO sales VALUES (6, 'Engineering', 'Bob', 80000, 2)",
                "INSERT INTO sales VALUES (7, 'Sales', 'Charlie', 65000, 2)",
                "INSERT INTO sales VALUES (8, 'Sales', 'David', 85000, 2)"};

            for (const auto& insert_sql : inserts) {
                result = execute_sql(insert_sql);
                if (has_error(result)) {
                    std::cerr << "Failed to insert data: " << get_error_message(result)
                              << std::endl;
                    return;
                }
            }
            std::cout << "✅ Inserted test data (8 rows)" << std::endl;

            // Verify data exists
            std::cout << "\n🔍 Debug: Verify data exists" << std::endl;
            result = execute_sql("SELECT COUNT(*) as row_count FROM sales");
            print_result(result);

            std::cout << "\n🔍 Debug: Show all data" << std::endl;
            result = execute_sql("SELECT * FROM sales ORDER BY id");
            print_result(result);

            // Test 1: ROW_NUMBER() OVER() - Simple numbering
            std::cout << "\n🔍 Test 1: ROW_NUMBER() OVER() - Simple numbering" << std::endl;
            result = execute_sql("SELECT employee, sales_amount, ROW_NUMBER() OVER() as row_num "
                                 "FROM sales ORDER BY id");
            print_result(result);

            // Test 2: ROW_NUMBER() OVER(PARTITION BY department)
            std::cout << "\n🔍 Test 2: ROW_NUMBER() OVER(PARTITION BY department)" << std::endl;
            result = execute_sql(
                "SELECT department, employee, sales_amount, ROW_NUMBER() OVER(PARTITION BY "
                "department) as dept_row_num FROM sales ORDER BY department, employee");
            print_result(result);

            // Test 3: RANK() OVER(ORDER BY sales_amount)
            std::cout << "\n🔍 Test 3: RANK() OVER(ORDER BY sales_amount)" << std::endl;
            result = execute_sql("SELECT employee, sales_amount, RANK() OVER(ORDER BY sales_amount "
                                 "DESC) as sales_rank FROM sales ORDER BY sales_amount DESC");
            print_result(result);

            // Test 4: RANK() OVER(PARTITION BY department ORDER BY sales_amount)
            std::cout << "\n🔍 Test 4: RANK() OVER(PARTITION BY department ORDER BY sales_amount)"
                      << std::endl;
            result = execute_sql("SELECT department, employee, sales_amount, RANK() OVER(PARTITION "
                                 "BY department ORDER BY sales_amount DESC) as dept_rank FROM "
                                 "sales ORDER BY department, sales_amount DESC");
            print_result(result);

            // Test 5: SUM() OVER() - Running total
            std::cout << "\n🔍 Test 5: SUM() OVER() - Running total of all sales" << std::endl;
            result = execute_sql("SELECT employee, sales_amount, SUM(sales_amount) OVER() as "
                                 "running_total FROM sales ORDER BY id");
            print_result(result);

            // Test 6: SUM() OVER(PARTITION BY department) - Running total by department
            std::cout << "\n🔍 Test 6: SUM() OVER(PARTITION BY department) - Department totals"
                      << std::endl;
            result = execute_sql(
                "SELECT department, employee, sales_amount, SUM(sales_amount) OVER(PARTITION BY "
                "department) as dept_total FROM sales ORDER BY department, employee");
            print_result(result);

            // Test 7: SUM() OVER(ORDER BY id) - Running sum ordered by id
            std::cout << "\n🔍 Test 7: SUM() OVER(ORDER BY id) - Running sum ordered by ID"
                      << std::endl;
            result = execute_sql("SELECT id, employee, sales_amount, SUM(sales_amount) OVER(ORDER "
                                 "BY id) as cumulative_sum FROM sales ORDER BY id");
            print_result(result);

            // Test 8: Complex combination - Multiple window functions
            std::cout << "\n🔍 Test 8: Multiple window functions in one query" << std::endl;
            result = execute_sql(R"(
            SELECT
                department,
                employee,
                sales_amount,
                ROW_NUMBER() OVER(PARTITION BY department ORDER BY sales_amount DESC) as dept_row,
                RANK() OVER(PARTITION BY department ORDER BY sales_amount DESC) as dept_rank,
                SUM(sales_amount) OVER(PARTITION BY department) as dept_total
            FROM sales
            ORDER BY department, sales_amount DESC
        )");
            print_result(result);

            // Test 9: Error cases
            std::cout << "\n🔍 Test 9: Error cases" << std::endl;

            // Invalid column in SUM
            result = execute_sql("SELECT SUM(nonexistent_col) OVER() FROM sales");
            if (has_error(result)) {
                std::cout << "✅ Correctly failed for invalid column: " << get_error_message(result)
                          << std::endl;
            } else {
                std::cout << "❌ Should have failed for invalid column" << std::endl;
            }

            std::cout << "\n🎯 Window Functions Test Summary:" << std::endl;
            std::cout << "   - ROW_NUMBER() with and without PARTITION BY" << std::endl;
            std::cout << "   - RANK() with ORDER BY and PARTITION BY" << std::endl;
            std::cout << "   - SUM() OVER() running totals and partitioned sums" << std::endl;
            std::cout << "   - Multiple window functions in single query" << std::endl;
            std::cout << "   - Error handling for invalid columns" << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Exception during window function tests: " << e.what() << std::endl;
        }
    }

} // namespace scratchbird::engine

int main()
{
    scratchbird::engine::test_window_functions();
    return 0;
}
