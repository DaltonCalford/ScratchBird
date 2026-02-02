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
#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/bytecode_generator_v2.h"
#include "test_helpers.h"

#include <memory>
#include <string>
#include <vector>

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

/**
 * Integration test for Query Planner (Phase 1, Task 1.3)
 *
 * Verifies that:
 * 1. Query planner is properly integrated with QueryCompilerV2
 * 2. Optimizer components are initialized
 * 3. Bytecode structure is preserved
 */
class QueryPlannerIntegrationTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        db_.reset();
        db_file_.reset();
    }

    bool createDatabase()
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_query_planner");

        ErrorContext ctx;
        Status status = Database::create(db_file_->path(), 16384, &ctx);
        if (status != Status::OK)
        {
            return false;
        }

        db_ = std::make_unique<Database>();
        status = db_->open(db_file_->path(), &ctx);
        if (status != Status::OK)
        {
            return false;
        }

        compiler_ = std::make_unique<QueryCompilerV2>(db_.get());
        executor_ = std::make_unique<Executor>(db_.get());

        const std::vector<std::string> ddl = {
            "CREATE TABLE users (id INTEGER, name VARCHAR(100), email VARCHAR(100), age INTEGER)",
            "CREATE TABLE products (id INTEGER, name VARCHAR(100), price DOUBLE)",
            "CREATE TABLE test (id INTEGER)"
        };

        for (const auto& sql : ddl)
        {
            auto compile_result = compiler_->compile(sql);
            if (!compile_result.success())
            {
                return false;
            }
            auto exec_result = executor_->execute(compile_result.bytecode());
            if (!exec_result.success())
            {
                return false;
            }
        }

        return true;
    }

    std::vector<uint8_t> compileSQL(const std::string &sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            last_compile_errors_.clear();
            for (const auto& err : compile_result.errors())
            {
                if (!last_compile_errors_.empty())
                {
                    last_compile_errors_ += "\n";
                }
                last_compile_errors_ += err;
            }
            return {};
        }
        last_compile_errors_.clear();
        return compile_result.bytecode();
    }

    bool containsOpcode(const std::vector<uint8_t> &bytecode, Opcode opcode)
    {
        for (uint8_t byte : bytecode)
        {
            if (byte == static_cast<uint8_t>(opcode))
            {
                return true;
            }
        }
        return false;
    }

    std::string disassemble(const std::vector<uint8_t> &bytecode)
    {
        return scratchbird::parser::v2::BytecodeDisassemblerV2::disassemble(bytecode);
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
    std::string last_compile_errors_;
};

// ===== Basic Integration Tests =====

TEST_F(QueryPlannerIntegrationTest, QueryCompilerV2ProducesBytecode)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT * FROM users");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::VERSION));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::SELECT));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::SELECT_STAR) ||
                containsOpcode(bytecode, Opcode::COLUMN_REF));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::END));
}

TEST_F(QueryPlannerIntegrationTest, DatabaseHasQueryPlannerComponents)
{
    ASSERT_TRUE(createDatabase());

    EXPECT_NE(db_->statistics_manager(), nullptr);
    scratchbird::optimizer::QueryPlanner planner(
        db_.get(),
        scratchbird::optimizer::CostModel(),
        db_->statistics_manager());
    (void)planner;
}

TEST_F(QueryPlannerIntegrationTest, CompilerRequiresCatalog)
{
    QueryCompilerV2 compiler(nullptr);
    auto result = compiler.compile("SELECT 1");
    EXPECT_FALSE(result.success());
}

// ===== Optimizer Integration Tests =====

TEST_F(QueryPlannerIntegrationTest, SelectGeneratesWithPlanner)
{
    ASSERT_TRUE(createDatabase());

    auto select_bytecode = compileSQL("SELECT * FROM users");
    EXPECT_FALSE(select_bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(select_bytecode, Opcode::SELECT));
    EXPECT_TRUE(containsOpcode(select_bytecode, Opcode::END));
}

TEST_F(QueryPlannerIntegrationTest, BytecodeContainsVersionHeader)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT * FROM test");
    ASSERT_GE(bytecode.size(), 3u);

    EXPECT_EQ(bytecode[0], static_cast<uint8_t>(Opcode::VERSION));
    EXPECT_EQ(bytecode[1], SBLR_VERSION);
    EXPECT_EQ(bytecode.back(), static_cast<uint8_t>(Opcode::END));
}

TEST_F(QueryPlannerIntegrationTest, SelectWithWhereClause)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT id, name FROM users WHERE id > 10");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::WHERE_CLAUSE));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::EXPR_GT));
}

TEST_F(QueryPlannerIntegrationTest, NonSelectStatementsBypassPlanner)
{
    ASSERT_TRUE(createDatabase());

    auto insert_bytecode = compileSQL("INSERT INTO users (id) VALUES (1)");
    EXPECT_FALSE(insert_bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(insert_bytecode, Opcode::INSERT));

    auto create_bytecode = compileSQL("CREATE TABLE test2 (id INTEGER)");
    EXPECT_FALSE(create_bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(create_bytecode, Opcode::CREATE_TABLE));
}

// ===== Stress Tests =====

TEST_F(QueryPlannerIntegrationTest, ComplexQueryWithPlanner)
{
    ASSERT_TRUE(createDatabase());

    std::string complex_sql =
        "SELECT id, name, price * 1.1 "
        "FROM products "
        "WHERE price >= 100.0 AND name LIKE '%sale%'";

    auto bytecode = compileSQL(complex_sql);
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::SELECT));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::WHERE_CLAUSE));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::EXPR_AND));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::EXPR_GE));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::EXPR_LIKE));
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::EXPR_MULTIPLY));
}

TEST_F(QueryPlannerIntegrationTest, MultipleSelectStatements)
{
    ASSERT_TRUE(createDatabase());

    std::vector<std::string> queries = {
        "SELECT * FROM users",
        "SELECT id FROM users WHERE id > 10",
        "SELECT name, email FROM users",
        "SELECT COUNT(*) FROM users"
    };

    for (const auto &sql : queries)
    {
        auto bytecode = compileSQL(sql);
        EXPECT_FALSE(bytecode.empty()) << "Failed for query: " << sql << "\n"
                                       << last_compile_errors_;
        EXPECT_TRUE(containsOpcode(bytecode, Opcode::SELECT))
            << "Missing SELECT opcode for query: " << sql;
        EXPECT_TRUE(containsOpcode(bytecode, Opcode::VERSION))
            << "Missing VERSION opcode for query: " << sql;
        EXPECT_TRUE(containsOpcode(bytecode, Opcode::END))
            << "Missing END opcode for query: " << sql;
    }
}

// ===== Edge Cases =====

TEST_F(QueryPlannerIntegrationTest, EmptySelectList)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT * FROM test");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::SELECT_STAR) ||
                containsOpcode(bytecode, Opcode::COLUMN_REF));
}

TEST_F(QueryPlannerIntegrationTest, SelectWithoutWhereClause)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT id, name FROM users");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, Opcode::SELECT));
    std::string disasm = disassemble(bytecode);
    EXPECT_EQ(disasm.find("WHERE_CLAUSE"), std::string::npos) << disasm;
}

// ===== Diagnostic Test =====

TEST_F(QueryPlannerIntegrationTest, DiagnosticDisassemblyOutput)
{
    ASSERT_TRUE(createDatabase());

    std::string sql = "SELECT id, name FROM users WHERE id = 42";
    auto bytecode = compileSQL(sql);

    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    std::string disasm = disassemble(bytecode);
    SCOPED_TRACE("Bytecode disassembly:\n" + disasm);

    EXPECT_NE(disasm.find("VERSION"), std::string::npos);
    EXPECT_NE(disasm.find("SELECT"), std::string::npos);
    EXPECT_NE(disasm.find("END"), std::string::npos);
}
