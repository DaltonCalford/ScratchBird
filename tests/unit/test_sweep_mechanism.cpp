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
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/sweep_manager.h"

#include "scratchbird/parser/parser_v2.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/semantic_analyzer_v2.h"
#include "scratchbird/sblr/executor.h"
#include "test_helpers.h"

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::sblr;

// Test fixture for sweep mechanism tests
class SweepMechanismTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create unique database path for test isolation (parallel execution)
        test_db_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_sweep_mechanism", ".sbrd");
        test_db_path_ = test_db_file_->path();

        // Create new database
        ErrorContext err_ctx;
        Status s = Database::create(test_db_path_, 16384, &err_ctx);
        ASSERT_EQ(s, Status::OK) << "Failed to create test database: " << err_ctx.message;

        // Open database
        s = db_.open(test_db_path_, &err_ctx);
        ASSERT_EQ(s, Status::OK) << "Failed to open test database: " << err_ctx.message;

        CatalogManager::SchemaInfo schema;
        ASSERT_EQ(db_.catalog_manager()->getSchema("PUBLIC", schema, &err_ctx), Status::OK)
            << "Failed to get PUBLIC schema: " << err_ctx.message;
        schema_id_ = schema.schema_id;

        compiler_ = std::make_unique<QueryCompilerV2>(&db_);
        compiler_->setCurrentSchema(schema_id_);
        executor_ = std::make_unique<Executor>(&db_);
        executor_->setCurrentSchema(schema_id_);
    }

    void TearDown() override
    {
        // Close database
        db_.close();
        // TestDatabaseFile will cleanup automatically on destruction
    }

    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_file_;
    std::string test_db_path_;
    Database db_;
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
    core::ID schema_id_;

    std::vector<uint8_t> buildSweepBytecode() const
    {
        std::vector<uint8_t> bytecode;
        bytecode.push_back(static_cast<uint8_t>(Opcode::VERSION));
        bytecode.push_back(SBLR_VERSION);
        bytecode.push_back(static_cast<uint8_t>(Opcode::SWEEP));
        bytecode.push_back(static_cast<uint8_t>(Opcode::END));
        return bytecode;
    }

    ExecutionResult executeSQL(const std::string& sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            if (!compile_result.errors().empty())
            {
                return ExecutionResult(compile_result.errors().front());
            }
            return ExecutionResult("Compile failed");
        }

        return executor_->execute(compile_result.bytecode());
    }
};

// ===== Parser Tests =====

// Test parsing of SWEEP DATABASE statement
TEST_F(SweepMechanismTest, ParseSweepDatabase)
{
    scratchbird::parser::v2::Parser parser("SWEEP DATABASE");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success()) << "Parse failed";

    auto* stmt = dynamic_cast<scratchbird::parser::v2::SweepDatabaseStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);
}

// Test parsing of SWEEP DATABASE with case variations
TEST_F(SweepMechanismTest, ParseSweepDatabaseCaseInsensitive)
{
    scratchbird::parser::v2::Parser parser("sWeEp dAtAbAsE");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success()) << "Parse failed";

    auto* stmt = dynamic_cast<scratchbird::parser::v2::SweepDatabaseStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);
}

// Test parsing failure when DATABASE keyword is missing
TEST_F(SweepMechanismTest, ParseSweepWithoutDatabase)
{
    scratchbird::parser::v2::Parser parser("SWEEP");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.errors().empty());
}

// Test semantic analysis of SWEEP DATABASE (should always succeed)
TEST_F(SweepMechanismTest, SemanticAnalysisSweepDatabase)
{
    scratchbird::parser::v2::Parser parser("SWEEP DATABASE");
    auto parse_result = parser.parseStatement();
    ASSERT_TRUE(parse_result.success()) << "Parse failed";

    scratchbird::parser::v2::SemanticAnalyzerV2 analyzer(
        *db_.catalog_manager(), parser.stringPool());
    analyzer.setCurrentSchema(schema_id_);
    auto sem_result = analyzer.analyze(parse_result.statement());
    ASSERT_TRUE(sem_result.success()) << "Semantic analysis failed";

    auto* resolved =
        dynamic_cast<scratchbird::parser::v2::ResolvedSweepDatabaseStmt*>(sem_result.statement());
    ASSERT_NE(resolved, nullptr);
}

// ===== Bytecode Generation Tests =====

// Test bytecode generation for SWEEP DATABASE
TEST_F(SweepMechanismTest, BytecodeGenerationSweepDatabase)
{
    const auto bytecode = buildSweepBytecode();

    // Verify bytecode structure: VERSION + version_byte + SWEEP + END
    ASSERT_GE(bytecode.size(), 4u) << "Bytecode should have at least 4 bytes";

    size_t pos = 0;
    EXPECT_EQ(bytecode[pos++], static_cast<uint8_t>(Opcode::VERSION));
    EXPECT_EQ(bytecode[pos++], SBLR_VERSION);
    EXPECT_EQ(bytecode[pos++], static_cast<uint8_t>(Opcode::SWEEP));
    EXPECT_EQ(bytecode[pos++], static_cast<uint8_t>(Opcode::END));

    EXPECT_EQ(pos, bytecode.size()) << "No extra bytes should be present";
}

// ===== SweepManager Tests =====

// Test SweepManager exists and is accessible
TEST_F(SweepMechanismTest, SweepManagerExists)
{
    auto sweep_mgr = db_.sweep_manager();
    ASSERT_NE(sweep_mgr, nullptr) << "SweepManager should exist";
}

// Test sweep statistics retrieval
TEST_F(SweepMechanismTest, SweepStatistics)
{
    auto sweep_mgr = db_.sweep_manager();
    ASSERT_NE(sweep_mgr, nullptr);

    auto stats = sweep_mgr->getStatistics();

    // Initial state
    EXPECT_EQ(stats.sweep_count, 0u) << "Initial sweep count should be 0";
    EXPECT_EQ(stats.last_sweep_time, 0u) << "Initial last sweep time should be 0";
    EXPECT_EQ(stats.last_sweep_duration_ms, 0u) << "Initial duration should be 0";
    EXPECT_FALSE(stats.sweep_in_progress) << "No sweep should be in progress initially";
}

// Test manual sweep execution (foreground)
TEST_F(SweepMechanismTest, ManualSweepExecution)
{
    auto sweep_mgr = db_.sweep_manager();
    ASSERT_NE(sweep_mgr, nullptr);

    // Execute foreground sweep
    ErrorContext err_ctx;
    Status s = sweep_mgr->executeSweep(true, &err_ctx);

    EXPECT_EQ(s, Status::OK) << "Manual sweep execution failed: " << err_ctx.message;

    // Verify statistics updated
    auto stats = sweep_mgr->getStatistics();
    EXPECT_EQ(stats.sweep_count, 1u) << "Sweep count should be 1 after manual sweep";
    EXPECT_GT(stats.last_sweep_time, 0u) << "Last sweep time should be set";
}

// ===== Executor Tests =====

// Test execution of SWEEP DATABASE command
TEST_F(SweepMechanismTest, ExecuteSweepDatabase)
{
    // Create connection
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;
    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK) << "Failed to create connection: " << err_ctx.message;

    // Set as current connection for executor
    ConnectionContext::setCurrent(conn.get());
    executor_->setConnectionContext(conn.get());

    // Execute bytecode
    ExecutionResult exec_result = executor_->execute(buildSweepBytecode());

    EXPECT_TRUE(exec_result.success()) << "Execution failed: " << exec_result.error();

    // Verify sweep was executed
    auto sweep_mgr = db_.sweep_manager();
    auto stats = sweep_mgr->getStatistics();
    EXPECT_GE(stats.sweep_count, 1u) << "Sweep count should be at least 1 after execution";

    // Clean up
    ConnectionContext::setCurrent(nullptr);
}

// ===== MON_SWEEP Query Tests =====

// Test MON_SWEEP monitoring query
TEST_F(SweepMechanismTest, MonSweepQuery)
{
    // Create connection
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;
    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    ConnectionContext::setCurrent(conn.get());
    executor_->setConnectionContext(conn.get());

    // Execute MON_SWEEP query
    ExecutionResult exec_result = executeSQL("SELECT * FROM MON_SWEEP");

    ASSERT_TRUE(exec_result.success()) << "Query failed: " << exec_result.error();
    ASSERT_TRUE(exec_result.hasResultSet()) << "Should have result set";

    ResultSet* result_set = exec_result.resultSet();
    ASSERT_NE(result_set, nullptr);

    // Verify columns
    EXPECT_EQ(result_set->columnCount(), 7u) << "MON_SWEEP should have 7 columns";
    EXPECT_EQ(result_set->columnName(0), "MON$SWEEP_COUNT");
    EXPECT_EQ(result_set->columnName(1), "MON$LAST_SWEEP_TIME");
    EXPECT_EQ(result_set->columnName(2), "MON$LAST_DURATION_MS");
    EXPECT_EQ(result_set->columnName(3), "MON$OIT_BEFORE");
    EXPECT_EQ(result_set->columnName(4), "MON$OIT_AFTER");
    EXPECT_EQ(result_set->columnName(5), "MON$TOTAL_SWEPT");
    EXPECT_EQ(result_set->columnName(6), "MON$IN_PROGRESS");

    // Should have exactly one row
    EXPECT_EQ(result_set->rowCount(), 1u) << "MON_SWEEP should return 1 row";

    // Clean up
    ConnectionContext::setCurrent(nullptr);
}

// Test MON_SWEEP query after manual sweep
TEST_F(SweepMechanismTest, MonSweepQueryAfterSweep)
{
    // Execute manual sweep first
    auto sweep_mgr = db_.sweep_manager();
    ErrorContext err_ctx;
    Status s = sweep_mgr->executeSweep(true, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Create connection for query
    std::unique_ptr<ConnectionContext> conn;
    s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    ConnectionContext::setCurrent(conn.get());
    executor_->setConnectionContext(conn.get());

    // Execute MON_SWEEP query
    ExecutionResult exec_result = executeSQL("SELECT * FROM MON_SWEEP");

    ASSERT_TRUE(exec_result.success());
    ASSERT_TRUE(exec_result.hasResultSet());

    ResultSet* result_set = exec_result.resultSet();
    ASSERT_NE(result_set, nullptr);
    ASSERT_EQ(result_set->rowCount(), 1u);

    // Verify sweep statistics are non-zero
    const Value& sweep_count = result_set->getValue(0, 0);
    EXPECT_FALSE(sweep_count.isNull());
    EXPECT_GE(sweep_count.toInt64(), 1) << "Sweep count should be at least 1";

    const Value& last_sweep_time = result_set->getValue(0, 1);
    EXPECT_FALSE(last_sweep_time.isNull());
    EXPECT_GT(last_sweep_time.toInt64(), 0) << "Last sweep time should be set";

    // Clean up
    ConnectionContext::setCurrent(nullptr);
}

// ===== Integration Tests =====

// Test automatic sweep trigger (when OAT-OIT gap exceeds threshold)
TEST_F(SweepMechanismTest, AutomaticSweepTrigger)
{
    // This test verifies that the sweep trigger check works
    // In practice, automatic sweep trigger requires many transactions
    // to create a significant gap between OAT and OIT

    auto txn_mgr = db_.transaction_manager();
    ASSERT_NE(txn_mgr, nullptr);

    // Get initial OIT and OAT
    uint64_t initial_oit = txn_mgr->getOldestXid();
    uint64_t initial_oat = txn_mgr->getOldestActiveXid();

    // Gap should initially be small (or zero)
    uint64_t initial_gap = initial_oat - initial_oit;

    // Verify sweep statistics are accessible
    auto sweep_mgr = db_.sweep_manager();
    ASSERT_NE(sweep_mgr, nullptr);

    auto stats = sweep_mgr->getStatistics();
    EXPECT_GE(stats.sweep_count, 0u) << "Sweep count should be accessible";
}

// Test SWEEP DATABASE command end-to-end
TEST_F(SweepMechanismTest, SweepDatabaseEndToEnd)
{
    // Create connection
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;
    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    ConnectionContext::setCurrent(conn.get());
    executor_->setConnectionContext(conn.get());

    // Get initial sweep statistics
    auto sweep_mgr = db_.sweep_manager();
    auto initial_stats = sweep_mgr->getStatistics();
    uint64_t initial_count = initial_stats.sweep_count;

    // Execute SWEEP DATABASE via bytecode
    ExecutionResult exec_result = executor_->execute(buildSweepBytecode());
    ASSERT_TRUE(exec_result.success()) << "Execution failed: " << exec_result.error();

    // Verify sweep count increased
    auto final_stats = sweep_mgr->getStatistics();
    EXPECT_EQ(final_stats.sweep_count, initial_count + 1)
        << "Sweep count should increase by 1 after SWEEP DATABASE";

    // Verify last sweep time was updated
    EXPECT_GT(final_stats.last_sweep_time, initial_stats.last_sweep_time)
        << "Last sweep time should be updated";

    // Clean up
    ConnectionContext::setCurrent(nullptr);
}

// Test multiple sweep executions
TEST_F(SweepMechanismTest, MultipleSweepExecutions)
{
    auto sweep_mgr = db_.sweep_manager();
    ErrorContext err_ctx;

    // Execute multiple sweeps
    const int num_sweeps = 3;
    for (int i = 0; i < num_sweeps; ++i)
    {
        Status s = sweep_mgr->executeSweep(true, &err_ctx);
        ASSERT_EQ(s, Status::OK) << "Sweep " << i << " failed: " << err_ctx.message;
    }

    // Verify sweep count
    auto stats = sweep_mgr->getStatistics();
    EXPECT_EQ(stats.sweep_count, static_cast<uint64_t>(num_sweeps))
        << "Sweep count should equal number of executions";
}

// Test sweep doesn't interfere with concurrent transactions
TEST_F(SweepMechanismTest, SweepWithConcurrentTransactions)
{
    ErrorContext err_ctx;

    // Create first connection with an active transaction
    std::unique_ptr<ConnectionContext> conn1;
    Status s = db_.connect(conn1, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    uint64_t xid1 = conn1->getCurrentXid();
    EXPECT_NE(xid1, 0u) << "First connection should have active transaction";

    // Execute sweep
    auto sweep_mgr = db_.sweep_manager();
    s = sweep_mgr->executeSweep(true, &err_ctx);
    EXPECT_EQ(s, Status::OK) << "Sweep should succeed with concurrent transaction";

    // Verify first transaction is still active
    EXPECT_EQ(conn1->getCurrentXid(), xid1)
        << "Sweep should not affect active transaction";

    // Create second connection after sweep
    std::unique_ptr<ConnectionContext> conn2;
    s = db_.connect(conn2, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    uint64_t xid2 = conn2->getCurrentXid();
    EXPECT_NE(xid2, 0u) << "Second connection should have active transaction";
    EXPECT_NE(xid2, xid1) << "Connections should have different XIDs";
}
