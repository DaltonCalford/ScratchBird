/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "test_helpers.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace scratchbird::core;

class StoredCodeDependencyTest : public ::testing::Test
{
protected:
    std::string test_db_path;
    std::string udr_path_;
    Database *db = nullptr;
    CatalogManager *catalog = nullptr;
    ID schema_id;
    ID system_user_id_{};

    void SetUp() override
    {
        test_db_path = "/tmp/test_code_dep_" + std::to_string(getpid()) + ".sbdb";
        udr_path_ = scratchbird::testing::uniqueTestDbPath("libudr", ".so");
        std::remove(test_db_path.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path, 16384, &ctx), Status::OK);

        db = new Database();
        ASSERT_EQ(db->open(test_db_path, &ctx), Status::OK);

        catalog = db->catalog_manager();
        ASSERT_NE(catalog, nullptr);

        // Get PUBLIC schema
        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(catalog->getSchema("PUBLIC", schema_info, &ctx), Status::OK);
        schema_id = schema_info.schema_id;

        system_user_id_ = catalog->getSystemUserId(&ctx);
    }

    void TearDown() override
    {
        if (db)
        {
            db->close();
            delete db;
            db = nullptr;
        }
        std::remove(test_db_path.c_str());
        std::remove((test_db_path + "-lock").c_str());
    }

    // Helper: Create a simple table
    ID createTestTable(const std::string& table_name)
    {
        ErrorContext ctx;
        std::vector<CatalogManager::ColumnInfo> columns;

        CatalogManager::ColumnInfo col;
        col.column_id = generateUuidV7();
        col.column_name = "id";
        col.data_type = static_cast<uint16_t>(DataType::INT32);
        col.nullable = false;
        col.ordinal = 0;
        columns.push_back(col);

        ID table_id;
        Status status = catalog->createTable(schema_id, table_name, columns, table_id, 0, &ctx);
        if (status != Status::OK) {
            ADD_FAILURE() << "Failed to create table: " << ctx.message;
        }
        return table_id;
    }

    // Helper: Create function and manually record dependencies
    ID createTestFunction(const std::string& func_name, const std::string& body,
                         const std::vector<std::pair<ID, CatalogManager::ObjectType>>& dependencies)
    {
        ErrorContext ctx;

        CatalogManager::FunctionInfo info;
        info.function_id = generateUuidV7();
        info.schema_id = schema_id;
        info.name = func_name;
        info.owner_id = system_user_id_;
        info.return_type = DataType::INT32;
        info.or_replace = false;
        info.deterministic = false;
        info.sql_security = CatalogManager::FunctionInfo::SqlSecurity::DEFINER;
        info.source_text = body;
        info.created_time = info.modified_time = 0;
        info.referenced_objects = dependencies;

        Status status = catalog->registerFunction(info, &ctx);
        if (status != Status::OK) {
            ADD_FAILURE() << "Failed to create function: " << ctx.message;
            return ID{};
        }

        return info.function_id;
    }

    // Helper: Create procedure and manually record dependencies
    ID createTestProcedure(const std::string& proc_name, const std::string& body,
                          const std::vector<std::pair<ID, CatalogManager::ObjectType>>& dependencies)
    {
        ErrorContext ctx;

        CatalogManager::ProcedureInfo info;
        info.procedure_id = generateUuidV7();
        info.schema_id = schema_id;
        info.name = proc_name;
        info.owner_id = system_user_id_;
        info.or_replace = false;
        info.sql_security = CatalogManager::ProcedureInfo::SqlSecurity::DEFINER;
        info.source_text = body;
        info.created_time = info.modified_time = 0;
        info.referenced_objects = dependencies;

        Status status = catalog->registerProcedure(info, &ctx);
        if (status != Status::OK) {
            ADD_FAILURE() << "Failed to create procedure: " << ctx.message;
            return ID{};
        }

        return info.procedure_id;
    }

    ID createTestPackage(const std::string& package_name)
    {
        ErrorContext ctx;
        ID package_id;
        Status status = catalog->createPackage(schema_id, package_name,
                                               "package header", "package body",
                                               package_id, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to create package: " << ctx.message;
        return package_id;
    }

    ID createTestUDR(const std::string& udr_name)
    {
        ErrorContext ctx;
        ID udr_id;
        Status status = catalog->createUDR(schema_id, udr_name,
                                           udr_path_, "entry",
                                           CatalogManager::UDRType::FUNCTION,
                                           "signature", udr_id, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to create UDR: " << ctx.message;
        return udr_id;
    }
};

// ========================================================================
// TEST 1: CREATE FUNCTION registers dependencies on tables
// ========================================================================

TEST_F(StoredCodeDependencyTest, CreateFunctionRegistersDependencies)
{
    ErrorContext ctx;

    // Create base table
    ID table_id = createTestTable("employees");

    // Create function with dependency on table
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps;
    deps.emplace_back(table_id, CatalogManager::ObjectType::TABLE);

    ID func_id = createTestFunction("count_employees", "SELECT COUNT(*) FROM employees", deps);
    ASSERT_NE(func_id.bytes[0], 0);  // Valid UUID

    // Verify dependency was recorded
    std::vector<CatalogManager::DependencyInfo> dependencies;
    ASSERT_EQ(catalog->getDependenciesFor(func_id, dependencies, &ctx), Status::OK);

    ASSERT_EQ(dependencies.size(), 1);
    EXPECT_EQ(dependencies[0].referenced_object_id, table_id);
    EXPECT_EQ(dependencies[0].referenced_type, CatalogManager::ObjectType::TABLE);
}

// ========================================================================
// TEST 2: CREATE PROCEDURE registers dependencies on tables
// ========================================================================

TEST_F(StoredCodeDependencyTest, CreateProcedureRegistersDependencies)
{
    ErrorContext ctx;

    // Create base table
    ID table_id = createTestTable("products");

    // Create procedure with dependency on table
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps;
    deps.emplace_back(table_id, CatalogManager::ObjectType::TABLE);

    ID proc_id = createTestProcedure("update_products", "UPDATE products SET id=1", deps);
    ASSERT_NE(proc_id.bytes[0], 0);  // Valid UUID

    // Verify dependency was recorded
    std::vector<CatalogManager::DependencyInfo> dependencies;
    ASSERT_EQ(catalog->getDependenciesFor(proc_id, dependencies, &ctx), Status::OK);

    ASSERT_EQ(dependencies.size(), 1);
    EXPECT_EQ(dependencies[0].referenced_object_id, table_id);
    EXPECT_EQ(dependencies[0].referenced_type, CatalogManager::ObjectType::TABLE);
}

// ========================================================================
// TEST 3: DROP FUNCTION fails if another function calls it
// ========================================================================

TEST_F(StoredCodeDependencyTest, DropFunctionFailsIfCalledByAnotherFunction)
{
    ErrorContext ctx;

    // Create table
    ID table_id = createTestTable("orders");

    // Create base function
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps1;
    deps1.emplace_back(table_id, CatalogManager::ObjectType::TABLE);
    ID base_func_id = createTestFunction("get_order_count", "SELECT COUNT(*) FROM orders", deps1);

    // Create another function that depends on the first
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps2;
    deps2.emplace_back(base_func_id, CatalogManager::ObjectType::FUNCTION);
    ID caller_func_id = createTestFunction("check_orders", "SELECT get_order_count()", deps2);

    // Try to drop base function - should fail
    Status status = catalog->dropFunction("get_order_count", false, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    EXPECT_NE(std::string(ctx.message).find("depend"), std::string::npos)
        << "Error message should mention dependencies: " << ctx.message;

    // Verify base function still exists
    CatalogManager::FunctionInfo func_info;
    EXPECT_EQ(catalog->getFunction("get_order_count", func_info, &ctx), Status::OK);
}

// ========================================================================
// TEST 4: DROP FUNCTION succeeds after dropping dependent
// ========================================================================

TEST_F(StoredCodeDependencyTest, DropFunctionSucceedsAfterDroppingDependent)
{
    ErrorContext ctx;

    // Create table
    ID table_id = createTestTable("customers");

    // Create base function
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps1;
    deps1.emplace_back(table_id, CatalogManager::ObjectType::TABLE);
    ID base_func_id = createTestFunction("get_customer_count", "SELECT COUNT(*) FROM customers", deps1);

    // Create caller function
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps2;
    deps2.emplace_back(base_func_id, CatalogManager::ObjectType::FUNCTION);
    ID caller_func_id = createTestFunction("check_customers", "SELECT get_customer_count()", deps2);

    // Drop caller function first
    ASSERT_EQ(catalog->dropFunction("check_customers", false, &ctx), Status::OK);

    // Now drop base function - should succeed
    ASSERT_EQ(catalog->dropFunction("get_customer_count", false, &ctx), Status::OK);

    // Verify base function no longer exists
    CatalogManager::FunctionInfo func_info;
    EXPECT_EQ(catalog->getFunction("get_customer_count", func_info, &ctx), Status::NOT_FOUND);
}

// ========================================================================
// TEST 5: DROP PROCEDURE fails if another procedure/function calls it
// ========================================================================

TEST_F(StoredCodeDependencyTest, DropProcedureFailsIfCalled)
{
    ErrorContext ctx;

    // Create table
    ID table_id = createTestTable("inventory");

    // Create base procedure
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps1;
    deps1.emplace_back(table_id, CatalogManager::ObjectType::TABLE);
    ID base_proc_id = createTestProcedure("update_inventory", "UPDATE inventory SET id=1", deps1);

    // Create function that depends on procedure
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps2;
    deps2.emplace_back(base_proc_id, CatalogManager::ObjectType::PROCEDURE);
    ID func_id = createTestFunction("run_inventory_update", "CALL update_inventory()", deps2);

    // Try to drop procedure - should fail
    Status status = catalog->dropProcedure("update_inventory", false, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    EXPECT_NE(std::string(ctx.message).find("depend"), std::string::npos)
        << "Error message should mention dependencies: " << ctx.message;
}

// ========================================================================
// TEST 6: DROP TABLE blocked by function dependency
// ========================================================================

TEST_F(StoredCodeDependencyTest, DropTableBlockedByFunctionDependency)
{
    ErrorContext ctx;

    // Create table
    ID table_id = createTestTable("sales");

    // Create function depending on table
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps;
    deps.emplace_back(table_id, CatalogManager::ObjectType::TABLE);
    ID func_id = createTestFunction("get_sales", "SELECT * FROM sales", deps);

    // Try to drop table - should fail
    Status status = catalog->dropTable(table_id, false, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    EXPECT_NE(std::string(ctx.message).find("depend"), std::string::npos)
        << "Error message should mention dependencies: " << ctx.message;

    // Drop function first
    ASSERT_EQ(catalog->dropFunction("get_sales", false, &ctx), Status::OK);

    // Now can drop table
    ASSERT_EQ(catalog->dropTable(table_id, false, &ctx), Status::OK);
}

// ========================================================================
// TEST 7: DROP FUNCTION clears dependencies
// ========================================================================

TEST_F(StoredCodeDependencyTest, DropFunctionClearsDependencies)
{
    ErrorContext ctx;

    // Create table
    ID table_id = createTestTable("users");

    // Create function with dependency
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps;
    deps.emplace_back(table_id, CatalogManager::ObjectType::TABLE);
    ID func_id = createTestFunction("count_users", "SELECT COUNT(*) FROM users", deps);

    // Verify dependency exists
    std::vector<CatalogManager::DependencyInfo> dependencies;
    ASSERT_EQ(catalog->getDependenciesFor(func_id, dependencies, &ctx), Status::OK);
    ASSERT_EQ(dependencies.size(), 1);

    // Drop function
    ASSERT_EQ(catalog->dropFunction("count_users", false, &ctx), Status::OK);

    // Verify dependencies were cleared
    EXPECT_EQ(catalog->getDependenciesFor(func_id, dependencies, &ctx), Status::OK);
    EXPECT_EQ(dependencies.size(), 0);
}

// ========================================================================
// TEST 8: Complex dependency chain: Function → Function → Table
// ========================================================================

TEST_F(StoredCodeDependencyTest, ComplexFunctionChain)
{
    ErrorContext ctx;

    // Create table
    ID table_id = createTestTable("accounts");

    // Create function level 1
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps1;
    deps1.emplace_back(table_id, CatalogManager::ObjectType::TABLE);
    ID func1_id = createTestFunction("get_account_count", "SELECT COUNT(*) FROM accounts", deps1);

    // Create function level 2
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps2;
    deps2.emplace_back(func1_id, CatalogManager::ObjectType::FUNCTION);
    ID func2_id = createTestFunction("account_summary", "SELECT get_account_count()", deps2);

    // Create function level 3
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps3;
    deps3.emplace_back(func2_id, CatalogManager::ObjectType::FUNCTION);
    ID func3_id = createTestFunction("full_report", "SELECT account_summary()", deps3);

    // Cannot drop func1 (func2 depends on it)
    EXPECT_EQ(catalog->dropFunction("get_account_count", false, &ctx), Status::CONSTRAINT_VIOLATION);

    // Cannot drop func2 (func3 depends on it)
    EXPECT_EQ(catalog->dropFunction("account_summary", false, &ctx), Status::CONSTRAINT_VIOLATION);

    // Can drop func3 (no dependents)
    ASSERT_EQ(catalog->dropFunction("full_report", false, &ctx), Status::OK);

    // Now can drop func2
    ASSERT_EQ(catalog->dropFunction("account_summary", false, &ctx), Status::OK);

    // Now can drop func1
    ASSERT_EQ(catalog->dropFunction("get_account_count", false, &ctx), Status::OK);

    // Table should still exist
    CatalogManager::TableInfo table_info;
    EXPECT_EQ(catalog->getTable(table_id, table_info, &ctx), Status::OK);
}

// ========================================================================
// TEST 9: Multiple functions depending on same table
// ========================================================================

TEST_F(StoredCodeDependencyTest, MultipleFunctionsSameTable)
{
    ErrorContext ctx;

    // Create table
    ID table_id = createTestTable("logs");

    // Create multiple functions on same table
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps;
    deps.emplace_back(table_id, CatalogManager::ObjectType::TABLE);

    ID func1_id = createTestFunction("count_logs", "SELECT COUNT(*) FROM logs", deps);
    ID func2_id = createTestFunction("recent_logs", "SELECT * FROM logs LIMIT 10", deps);
    ID func3_id = createTestFunction("log_summary", "SELECT MAX(id) FROM logs", deps);

    // Cannot drop table (3 functions depend on it)
    EXPECT_EQ(catalog->dropTable(table_id, false, &ctx), Status::CONSTRAINT_VIOLATION);

    // Drop all functions
    ASSERT_EQ(catalog->dropFunction("count_logs", false, &ctx), Status::OK);
    ASSERT_EQ(catalog->dropFunction("recent_logs", false, &ctx), Status::OK);
    ASSERT_EQ(catalog->dropFunction("log_summary", false, &ctx), Status::OK);

    // Now can drop table
    ASSERT_EQ(catalog->dropTable(table_id, false, &ctx), Status::OK);
}

// ========================================================================
// TEST 10: Mixed dependencies: Function calls procedure that accesses table
// ========================================================================

TEST_F(StoredCodeDependencyTest, MixedFunctionProcedureDependencies)
{
    ErrorContext ctx;

    // Create table
    ID table_id = createTestTable("tasks");

    // Create procedure depending on table
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps_proc;
    deps_proc.emplace_back(table_id, CatalogManager::ObjectType::TABLE);
    ID proc_id = createTestProcedure("update_tasks", "UPDATE tasks SET id=1", deps_proc);

    // Create function depending on procedure
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps_func;
    deps_func.emplace_back(proc_id, CatalogManager::ObjectType::PROCEDURE);
    ID func_id = createTestFunction("run_update", "CALL update_tasks()", deps_func);

    // Cannot drop table (procedure depends on it)
    EXPECT_EQ(catalog->dropTable(table_id, false, &ctx), Status::CONSTRAINT_VIOLATION);

    // Cannot drop procedure (function depends on it)
    EXPECT_EQ(catalog->dropProcedure("update_tasks", false, &ctx), Status::CONSTRAINT_VIOLATION);

    // Can drop function (no dependents)
    ASSERT_EQ(catalog->dropFunction("run_update", false, &ctx), Status::OK);

    // Now can drop procedure
    ASSERT_EQ(catalog->dropProcedure("update_tasks", false, &ctx), Status::OK);

    // Now can drop table
    ASSERT_EQ(catalog->dropTable(table_id, false, &ctx), Status::OK);
}

// ========================================================================
// TEST 11: DROP PACKAGE fails if external code depends on it
// ========================================================================

TEST_F(StoredCodeDependencyTest, DropPackageFailsIfDependentExists)
{
    ErrorContext ctx;

    ID package_id = createTestPackage("pkg_test");

    CatalogManager::FunctionInfo func;
    func.function_id = generateUuidV7();
    func.schema_id = schema_id;
    func.name = "pkg_func";
    func.owner_id = system_user_id_;
    func.return_type = DataType::INT32;
    func.source_text = "select 1";
    ASSERT_EQ(catalog->registerFunction(func, &ctx), Status::OK);

    ID dep_id;
    ASSERT_EQ(catalog->createDependency(
        func.function_id, CatalogManager::ObjectType::FUNCTION,
        package_id, CatalogManager::ObjectType::PACKAGE,
        CatalogManager::DependencyType::NORMAL, dep_id, &ctx), Status::OK);

    Status status = catalog->dropPackage(package_id, false, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    EXPECT_NE(std::string(ctx.message).find("package"), std::string::npos) << ctx.message;
}

// ========================================================================
// TEST 12: DROP UDR fails if external code depends on it
// ========================================================================

TEST_F(StoredCodeDependencyTest, DropUDRFailsIfDependentExists)
{
    ErrorContext ctx;

    ID udr_id = createTestUDR("udr_test");

    CatalogManager::ProcedureInfo proc;
    proc.procedure_id = generateUuidV7();
    proc.schema_id = schema_id;
    proc.name = "proc_udr";
    proc.owner_id = system_user_id_;
    proc.source_text = "select 1";
    ASSERT_EQ(catalog->registerProcedure(proc, &ctx), Status::OK);

    ID dep_id;
    ASSERT_EQ(catalog->createDependency(
        proc.procedure_id, CatalogManager::ObjectType::PROCEDURE,
        udr_id, CatalogManager::ObjectType::UDR,
        CatalogManager::DependencyType::NORMAL, dep_id, &ctx), Status::OK);

    Status status = catalog->dropUDR(udr_id, false, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    EXPECT_NE(std::string(ctx.message).find("udr"), std::string::npos) << ctx.message;
}
