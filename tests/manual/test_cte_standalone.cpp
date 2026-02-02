/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// Standalone CTE (Common Table Expressions) Test
// This test can be compiled and run independently to verify CTE functionality
//
// Compile: g++ -std=c++17 -I../../include -L../../build/src -o test_cte test_cte_standalone.cpp -lscratchbird_core -lscratchbird_parser -lscratchbird_sblr -lpthread
// Run: ./test_cte

#include <iostream>
#include <memory>
#include <filesystem>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/proc_array.h"

#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/executor.h"

using namespace scratchbird;
using namespace scratchbird::sblr;

class CTETest
{
private:
    std::string test_db_path_;
    std::unique_ptr<core::Database> db_;
    std::unique_ptr<core::ConnectionContext> conn_ctx_;
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
    core::ID schema_id_{};
    uint32_t proc_id_ = 0;

public:
    CTETest() : test_db_path_("test_cte_standalone.db") {}

    ~CTETest()
    {
        cleanup();
    }

    bool setup()
    {
        std::filesystem::remove_all(test_db_path_);

        core::ErrorContext ctx;
        db_ = core::Database::create(test_db_path_, &ctx);
        if (!db_)
        {
            std::cerr << "Failed to create database: " << ctx.message << std::endl;
            return false;
        }

        auto status = core::ProcArrayManager::initialize(db_.get(), 10, &ctx);
        if (status != core::Status::OK)
        {
            std::cerr << "Failed to initialize ProcArray: " << ctx.message << std::endl;
            return false;
        }

        status = core::ProcArrayManager::registerBackend(&proc_id_, &ctx);
        if (status != core::Status::OK)
        {
            std::cerr << "Failed to register backend: " << ctx.message << std::endl;
            return false;
        }

        conn_ctx_ = std::make_unique<core::ConnectionContext>(db_.get(), proc_id_);
        status = conn_ctx_->initialize(&ctx);
        if (status != core::Status::OK)
        {
            std::cerr << "Failed to initialize connection context: " << ctx.message << std::endl;
            return false;
        }

        core::CatalogManager::SchemaInfo schema;
        status = db_->catalog_manager()->getSchema("PUBLIC", schema, &ctx);
        if (status != core::Status::OK)
        {
            std::cerr << "Failed to get PUBLIC schema: " << ctx.message << std::endl;
            return false;
        }
        schema_id_ = schema.schema_id;

        compiler_ = std::make_unique<QueryCompilerV2>(db_.get());
        compiler_->setCurrentSchema(schema_id_);

        executor_ = std::make_unique<Executor>(db_.get());
        executor_->setConnectionContext(conn_ctx_.get());
        executor_->setCurrentSchema(schema_id_);

        return createTestTable();
    }

    void cleanup()
    {
        executor_.reset();
        compiler_.reset();
        conn_ctx_.reset();

        if (db_)
        {
            core::ErrorContext ctx;
            core::ProcArrayManager::unregisterBackend(proc_id_, &ctx);
            core::ProcArrayManager::shutdown(&ctx);
        }

        db_.reset();
        std::filesystem::remove_all(test_db_path_);
    }

    bool createTestTable()
    {
        std::string create_sql = R"(
            CREATE TABLE employees (
                id INT NOT NULL,
                name VARCHAR(50),
                department VARCHAR(50),
                salary INT
            )
        )";

        if (!executeSQL(create_sql))
        {
            std::cerr << "Failed to create table" << std::endl;
            return false;
        }

        std::vector<std::string> inserts = {
            "INSERT INTO employees VALUES (1, 'Alice', 'Engineering', 100000)",
            "INSERT INTO employees VALUES (2, 'Bob', 'Engineering', 95000)",
            "INSERT INTO employees VALUES (3, 'Charlie', 'Sales', 80000)",
            "INSERT INTO employees VALUES (4, 'Diana', 'Sales', 85000)",
            "INSERT INTO employees VALUES (5, 'Eve', 'HR', 70000)"
        };

        for (const auto &sql : inserts)
        {
            if (!executeSQL(sql))
            {
                std::cerr << "Failed to insert data: " << sql << std::endl;
                return false;
            }
        }

        core::ErrorContext ctx;
        auto status = conn_ctx_->commit(&ctx);
        if (status != core::Status::OK)
        {
            std::cerr << "Failed to commit: " << ctx.message << std::endl;
            return false;
        }

        return true;
    }

    bool executeSQL(const std::string &sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            std::cerr << "Compile error: "
                      << (compile_result.errors().empty() ? "unknown" : compile_result.errors().front())
                      << std::endl;
            return false;
        }

        auto result = executor_->execute(compile_result.bytecode());
        if (!result.success())
        {
            std::cerr << "Execution error: " << result.error() << std::endl;
            return false;
        }

        return true;
    }

    ExecutionResult executeSQLWithResult(const std::string &sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            auto message = compile_result.errors().empty()
                ? "Compile error"
                : compile_result.errors().front();
            return ExecutionResult(message);
        }

        return executor_->execute(compile_result.bytecode());
    }

    bool testSingleCTE()
    {
        std::cout << "\n[TEST] Single CTE - Engineering employees" << std::endl;

        std::string sql = R"(
            WITH eng_employees AS (
                SELECT id, name, salary
                FROM employees
                WHERE department = 'Engineering'
            )
            SELECT * FROM eng_employees
        )";

        auto result = executeSQLWithResult(sql);
        if (!result.success())
        {
            std::cerr << "FAIL: Query failed: " << result.error() << std::endl;
            return false;
        }

        if (!result.hasResultSet())
        {
            std::cerr << "FAIL: Expected result set" << std::endl;
            return false;
        }

        auto *rs = result.resultSet();
        if (rs->rowCount() != 2)
        {
            std::cerr << "FAIL: Expected 2 rows, got " << rs->rowCount() << std::endl;
            return false;
        }

        std::cout << "PASS: Got " << rs->rowCount() << " rows as expected" << std::endl;
        return true;
    }

    bool testMultipleCTEs()
    {
        std::cout << "\n[TEST] Multiple CTEs - Sales employees" << std::endl;

        std::string sql = R"(
            WITH
                eng_employees AS (
                    SELECT id, name, salary FROM employees WHERE department = 'Engineering'
                ),
                sales_employees AS (
                    SELECT id, name, salary FROM employees WHERE department = 'Sales'
                )
            SELECT * FROM sales_employees
        )";

        auto result = executeSQLWithResult(sql);
        if (!result.success())
        {
            std::cerr << "FAIL: Query failed: " << result.error() << std::endl;
            return false;
        }

        if (!result.hasResultSet())
        {
            std::cerr << "FAIL: Expected result set" << std::endl;
            return false;
        }

        auto *rs = result.resultSet();
        if (rs->rowCount() != 2)
        {
            std::cerr << "FAIL: Expected 2 rows, got " << rs->rowCount() << std::endl;
            return false;
        }

        std::cout << "PASS: Got " << rs->rowCount() << " rows as expected" << std::endl;
        return true;
    }

    bool testCTEWithColumnAliases()
    {
        std::cout << "\n[TEST] CTE with column aliases" << std::endl;

        std::string sql = R"(
            WITH high_earners (emp_id, emp_name, emp_salary) AS (
                SELECT id, name, salary FROM employees WHERE salary > 80000
            )
            SELECT emp_name, emp_salary FROM high_earners
        )";

        auto result = executeSQLWithResult(sql);
        if (!result.success())
        {
            std::cerr << "FAIL: Query failed: " << result.error() << std::endl;
            return false;
        }

        if (!result.hasResultSet())
        {
            std::cerr << "FAIL: Expected result set" << std::endl;
            return false;
        }

        auto *rs = result.resultSet();
        if (rs->rowCount() < 3)
        {
            std::cerr << "FAIL: Expected at least 3 rows, got " << rs->rowCount() << std::endl;
            return false;
        }

        std::cout << "PASS: Got " << rs->rowCount() << " rows as expected" << std::endl;
        return true;
    }

    bool testCTEWithWhere()
    {
        std::cout << "\n[TEST] CTE with WHERE clause in main query" << std::endl;

        std::string sql = R"(
            WITH dept_employees AS (
                SELECT department, name FROM employees
            )
            SELECT * FROM dept_employees WHERE department = 'HR'
        )";

        auto result = executeSQLWithResult(sql);
        if (!result.success())
        {
            std::cerr << "FAIL: Query failed: " << result.error() << std::endl;
            return false;
        }

        if (!result.hasResultSet())
        {
            std::cerr << "FAIL: Expected result set" << std::endl;
            return false;
        }

        auto *rs = result.resultSet();
        if (rs->rowCount() != 1)
        {
            std::cerr << "FAIL: Expected 1 row, got " << rs->rowCount() << std::endl;
            return false;
        }

        std::cout << "PASS: Got " << rs->rowCount() << " row as expected" << std::endl;
        return true;
    }

    bool runAllTests()
    {
        if (!setup())
        {
            std::cerr << "Setup failed" << std::endl;
            return false;
        }

        int passed = 0;
        int failed = 0;

        if (testSingleCTE())
            passed++;
        else
            failed++;

        if (testMultipleCTEs())
            passed++;
        else
            failed++;

        if (testCTEWithColumnAliases())
            passed++;
        else
            failed++;

        if (testCTEWithWhere())
            passed++;
        else
            failed++;

        std::cout << "\n========================================" << std::endl;
        std::cout << "Test Results: " << passed << " passed, " << failed << " failed" << std::endl;
        std::cout << "========================================" << std::endl;

        return failed == 0;
    }
};

int main()
{
    std::cout << "ScratchBird CTE (Common Table Expressions) Standalone Test" << std::endl;
    std::cout << "==========================================================" << std::endl;

    CTETest test;
    bool success = test.runAllTests();

    return success ? 0 : 1;
}
