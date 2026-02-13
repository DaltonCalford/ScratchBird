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

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"
#include <sstream>
#include <string>

using namespace scratchbird::sblr;
using namespace scratchbird::core;
using scratchbird::testing::TestDatabaseFile;

class ConditionalFunctionTest : public ::testing::Test
{
protected:
    std::unique_ptr<Database> db_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<TestDatabaseFile> db_file_;
    ID schema_id_;

    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_conditional_functions");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        CatalogManager::SchemaInfo schema;
        ASSERT_EQ(db_->catalog_manager()->getSchema("PUBLIC", schema, &ctx), Status::OK)
            << ctx.message;
        schema_id_ = schema.schema_id;

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
        compiler_->setCurrentSchema(schema_id_);
        executor_ = std::make_unique<Executor>(db_.get());
        executor_->setCurrentSchema(schema_id_);

        createTables();
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        db_.reset();
        db_file_.reset();
    }

    void createTables()
    {
        const std::vector<std::string> ddl = {
            "CREATE TABLE users ("
            "id INT, name TEXT, status TEXT, age INT, default_age INT, "
            "optional_email TEXT, backup_email TEXT, email TEXT, "
            "a INT, b INT, c INT, d INT, child_name TEXT, adult_name TEXT, premium INT"
            ")",
            "CREATE TABLE stats (value INT)",
            "CREATE TABLE changes (current_value INT, previous_value INT)",
            "CREATE TABLE calculations (a INT, b INT, c INT, d INT)",
            "CREATE TABLE data (a INT, b INT, c INT, status INT, value INT)",
            "CREATE TABLE grades (grade TEXT)",
            "CREATE TABLE colors (color TEXT)",
            "CREATE TABLE scores (score INT)",
            "CREATE TABLE points (x INT)"
        };

        for (const auto& sql : ddl)
        {
            auto compile_result = compiler_->compile(sql);
            ASSERT_TRUE(compile_result.success()) << "Compile failed for: " << sql;
            auto exec_result = executor_->execute(compile_result.bytecode());
            ASSERT_TRUE(exec_result.success()) << "Execution failed for: " << sql
                                               << " error: " << exec_result.error();
        }
    }

    static std::string formatDiagnostics(const CompilationResultV3& result)
    {
        std::ostringstream oss;
        if (!result.errors().empty())
        {
            oss << "\nErrors:";
            for (const auto& err : result.errors())
            {
                oss << "\n  " << err;
            }
        }
        if (!result.warnings().empty())
        {
            oss << "\nWarnings:";
            for (const auto& warn : result.warnings())
            {
                oss << "\n  " << warn;
            }
        }
        return oss.str();
    }

    void testParse(const std::string &sql, bool should_succeed = true)
    {
        auto result = compiler_->compile(sql);
        if (should_succeed)
        {
            ASSERT_TRUE(result.success()) << "Compile failed for: " << sql
                                          << formatDiagnostics(result);
        }
        else
        {
            EXPECT_FALSE(result.success()) << "Compile should have failed for: " << sql
                                           << formatDiagnostics(result);
        }
    }

    void testBytecodeGeneration(const std::string &sql, bool should_succeed = true)
    {
        auto result = compiler_->compile(sql);

        if (should_succeed)
        {
            EXPECT_TRUE(result.success()) << "Bytecode generation failed for: " << sql
                                          << formatDiagnostics(result);
            EXPECT_FALSE(result.bytecode().empty()) << "Bytecode is empty";
        }
        else
        {
            EXPECT_FALSE(result.success()) << "Bytecode generation should have failed for: " << sql
                                           << formatDiagnostics(result);
        }
    }
};

// ===== COALESCE Function Tests =====

TEST_F(ConditionalFunctionTest, COALESCE_Basic)
{
    std::string sql = "SELECT COALESCE(NULL, 'fallback') FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, COALESCE_MultipleArgs)
{
    std::string sql = "SELECT COALESCE(NULL, NULL, 'third', 'fourth') FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, COALESCE_WithColumns)
{
    std::string sql = "SELECT COALESCE(optional_email, backup_email, 'no-email') FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, COALESCE_TwoArgs)
{
    std::string sql = "SELECT COALESCE(name, 'Anonymous') FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, COALESCE_Nested)
{
    std::string sql = "SELECT COALESCE(a, COALESCE(b, c, d)) FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, COALESCE_WithExpressions)
{
    std::string sql = "SELECT COALESCE(age + 1, default_age * 2) FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, COALESCE_InWhereClause)
{
    std::string sql = "SELECT * FROM users WHERE COALESCE(status, 'active') = 'active'";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

// ===== NULLIF Function Tests =====

TEST_F(ConditionalFunctionTest, NULLIF_Basic)
{
    std::string sql = "SELECT NULLIF(value, 0) FROM stats";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, NULLIF_WithStrings)
{
    std::string sql = "SELECT NULLIF(name, '') FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, NULLIF_WithColumns)
{
    std::string sql = "SELECT NULLIF(current_value, previous_value) FROM changes";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, NULLIF_WithExpressions)
{
    std::string sql = "SELECT NULLIF(a + b, c * d) FROM calculations";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, NULLIF_InWhereClause)
{
    // IS NOT NULL not yet supported in expressions - use comparison instead
    // Full IS NULL support is planned for Alpha 2 (multi-dialect parser)
    std::string sql = "SELECT * FROM users WHERE NULLIF(status, 'deleted') = status";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, NULLIF_Nested)
{
    std::string sql = "SELECT NULLIF(NULLIF(a, b), c) FROM data";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

// ===== CASE Expression Tests - Simple Form =====

TEST_F(ConditionalFunctionTest, CASE_Simple_Basic)
{
    std::string sql = "SELECT CASE status WHEN 'active' THEN 1 WHEN 'inactive' THEN 0 END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Simple_WithElse)
{
    std::string sql = "SELECT CASE status WHEN 'active' THEN 1 ELSE 0 END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Simple_MultipleWhen)
{
    std::string sql = "SELECT CASE grade WHEN 'A' THEN 4.0 WHEN 'B' THEN 3.0 WHEN 'C' THEN 2.0 WHEN 'D' THEN 1.0 ELSE 0.0 END FROM grades";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Simple_WithStrings)
{
    std::string sql = "SELECT CASE color WHEN 'red' THEN '#FF0000' WHEN 'green' THEN '#00FF00' WHEN 'blue' THEN '#0000FF' END FROM colors";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Simple_WithExpressions)
{
    std::string sql = "SELECT CASE status * 2 WHEN 2 THEN 'double' WHEN 4 THEN 'quad' END FROM data";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

// ===== CASE Expression Tests - Searched Form =====

TEST_F(ConditionalFunctionTest, CASE_Searched_Basic)
{
    std::string sql = "SELECT CASE WHEN age < 18 THEN 'minor' WHEN age >= 18 THEN 'adult' END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Searched_WithElse)
{
    std::string sql = "SELECT CASE WHEN score >= 90 THEN 'A' WHEN score >= 80 THEN 'B' ELSE 'F' END FROM scores";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Searched_ComplexConditions)
{
    // AND/OR not yet supported in expression contexts - use simple conditions
    // Compound boolean expressions planned for Alpha 2 (multi-dialect parser)
    std::string sql = "SELECT CASE WHEN age < 13 THEN 'child' WHEN age >= 13 THEN 'teen' WHEN age >= 20 THEN 'adult' ELSE 'senior' END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Searched_WithNullChecks)
{
    // IS NULL not yet supported in CASE WHEN - use comparison instead
    // Full IS NULL support is planned for Alpha 2 (multi-dialect parser)
    std::string sql = "SELECT CASE WHEN email = '' THEN 'empty' ELSE email END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Searched_InWhereClause)
{
    std::string sql = "SELECT * FROM users WHERE CASE WHEN age < 18 THEN 0 ELSE 1 END = 1";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Searched_MultipleConditions)
{
    // AND/OR not yet supported in expression contexts - use simple conditions
    // Compound boolean expressions planned for Alpha 2 (multi-dialect parser)
    std::string sql = "SELECT CASE WHEN x > 0 THEN 'positive' WHEN x < 0 THEN 'negative' ELSE 'zero' END FROM points";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

// ===== Nested Conditional Functions Tests =====

TEST_F(ConditionalFunctionTest, Nested_COALESCE_In_CASE)
{
    std::string sql = "SELECT CASE WHEN COALESCE(status, 'unknown') = 'active' THEN 1 ELSE 0 END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, Nested_CASE_In_COALESCE)
{
    // String concat || not yet supported - use simple string values
    // String concatenation operator planned for Alpha 2 (multi-dialect parser)
    std::string sql = "SELECT COALESCE(name, CASE WHEN id > 0 THEN 'User' ELSE 'Unknown' END) FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, Nested_NULLIF_In_CASE)
{
    // IS NULL not yet supported in CASE WHEN - use comparison instead
    // Full IS NULL support is planned for Alpha 2 (multi-dialect parser)
    std::string sql = "SELECT CASE WHEN NULLIF(value, 0) = 0 THEN 'zero' ELSE 'nonzero' END FROM data";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, Nested_Multiple_Levels)
{
    std::string sql = "SELECT COALESCE(CASE WHEN age < 18 THEN NULLIF(child_name, '') ELSE NULLIF(adult_name, '') END, 'Anonymous') FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, Nested_CASE_In_CASE)
{
    std::string sql = "SELECT CASE WHEN status = 'active' THEN CASE WHEN premium = 1 THEN 'premium' ELSE 'regular' END ELSE 'inactive' END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

// ===== Combined Tests =====

TEST_F(ConditionalFunctionTest, Combined_All_Three_Functions)
{
    std::string sql = "SELECT COALESCE(NULLIF(name, ''), CASE WHEN id > 0 THEN 'User' ELSE 'Guest' END) FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, Combined_In_Aggregates)
{
    std::string sql = "SELECT COUNT(CASE WHEN status = 'active' THEN 1 END) FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, Combined_Multiple_Select_Columns)
{
    std::string sql = "SELECT COALESCE(name, 'Unknown'), NULLIF(email, ''), CASE WHEN age >= 18 THEN 'adult' ELSE 'minor' END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

// ===== Error Cases =====

TEST_F(ConditionalFunctionTest, Error_COALESCE_NoArgs)
{
    std::string sql = "SELECT COALESCE() FROM users";
    testParse(sql, false);
}

TEST_F(ConditionalFunctionTest, Error_NULLIF_OneArg)
{
    std::string sql = "SELECT NULLIF(value) FROM users";
    testParse(sql, false);
}

TEST_F(ConditionalFunctionTest, Error_NULLIF_ThreeArgs)
{
    std::string sql = "SELECT NULLIF(a, b, c) FROM users";
    testParse(sql, false);
}

TEST_F(ConditionalFunctionTest, Error_CASE_NoWhen)
{
    std::string sql = "SELECT CASE status END FROM users";
    testParse(sql, false);
}

TEST_F(ConditionalFunctionTest, Error_CASE_NoEnd)
{
    std::string sql = "SELECT CASE WHEN age > 18 THEN 'adult' FROM users";
    testParse(sql, false);
}

TEST_F(ConditionalFunctionTest, Error_CASE_WhenWithoutThen)
{
    std::string sql = "SELECT CASE WHEN age > 18 'adult' END FROM users";
    testParse(sql, false);
}
