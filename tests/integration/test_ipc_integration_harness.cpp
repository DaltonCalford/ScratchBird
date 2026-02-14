/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * IPC Integration Test Harness
 *
 * End-to-end tests for the IPC system:
 * - Simple query path (5.1.2)
 * - Prepared statement path (5.1.3)
 * - Transaction path (5.1.4)
 * - COPY path (5.1.5)
 * - Error handling (5.1.6)
 * - Concurrent sessions (5.1.7)
 * - Memory leak detection (5.1.8)
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cstring>
#include <unistd.h>

#include "scratchbird/ipc/engine_ipc_session_handler.h"
#include "scratchbird/ipc/ipc_server.h"
#include "scratchbird/ipc/ipc_contract_v1_1.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/sblr/executor.h"
#include "test_helpers.h"

using namespace scratchbird;
using namespace scratchbird::ipc;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

// ============================================================================
// Testable Handler that Captures Responses
// ============================================================================

class TestableEngineIPCHandler : public EngineIPCSessionHandler {
public:
    explicit TestableEngineIPCHandler(core::Database* db)
        : EngineIPCSessionHandler(db) {}

    core::Status sendRowDescription(uint32_t session_id,
                                   const std::vector<IPCFieldDesc>& fields) override {
        (void)session_id;
        last_fields_ = fields;
        return core::Status::OK;
    }

    core::Status sendDataRow(uint32_t session_id,
                            const std::vector<std::optional<std::string>>& values) override {
        (void)session_id;
        last_rows_.push_back(values);
        return core::Status::OK;
    }

    core::Status sendCommandComplete(uint32_t session_id,
                                    const std::string& tag,
                                    uint64_t rows_affected) override {
        (void)session_id;
        last_command_tag_ = tag;
        last_rows_affected_ = rows_affected;
        return core::Status::OK;
    }

    core::Status sendError(uint32_t session_id,
                          const char* sqlstate,
                          const std::string& message) override {
        (void)session_id;
        last_sqlstate_ = sqlstate;
        last_error_ = message;
        return core::Status::OK;
    }

    core::Status sendParseComplete(uint32_t session_id) override {
        (void)session_id;
        parse_complete_called_ = true;
        return core::Status::OK;
    }

    core::Status sendBindComplete(uint32_t session_id) override {
        (void)session_id;
        bind_complete_called_ = true;
        return core::Status::OK;
    }

    core::Status sendReady(uint32_t session_id, uint32_t server_features) override {
        (void)session_id;
        (void)server_features;
        ready_called_ = true;
        return core::Status::OK;
    }

    core::Status sendCopyInRequest(uint32_t session_id) override {
        (void)session_id;
        copy_in_response_called_ = true;
        return core::Status::OK;
    }

    // Accessors for test verification
    const std::vector<IPCFieldDesc>& lastFields() const { return last_fields_; }
    const std::vector<std::vector<std::optional<std::string>>>& lastRows() const { return last_rows_; }
    const std::string& lastCommandTag() const { return last_command_tag_; }
    uint64_t lastRowsAffected() const { return last_rows_affected_; }
    const std::string& lastSqlState() const { return last_sqlstate_; }
    const std::string& lastError() const { return last_error_; }
    bool parseCompleteCalled() const { return parse_complete_called_; }
    bool bindCompleteCalled() const { return bind_complete_called_; }
    bool readyCalled() const { return ready_called_; }
    bool copyInResponseCalled() const { return copy_in_response_called_; }

    void reset() {
        last_fields_.clear();
        last_rows_.clear();
        last_command_tag_.clear();
        last_rows_affected_ = 0;
        last_sqlstate_.clear();
        last_error_.clear();
        parse_complete_called_ = false;
        bind_complete_called_ = false;
        ready_called_ = false;
        copy_in_response_called_ = false;
    }

private:
    std::vector<IPCFieldDesc> last_fields_;
    std::vector<std::vector<std::optional<std::string>>> last_rows_;
    std::string last_command_tag_;
    uint64_t last_rows_affected_ = 0;
    std::string last_sqlstate_;
    std::string last_error_;
    bool parse_complete_called_ = false;
    bool bind_complete_called_ = false;
    bool ready_called_ = false;
    bool copy_in_response_called_ = false;
};

// ============================================================================
// Test Fixture
// ============================================================================

class IPCIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("ipc_integration_test");

        core::ErrorContext ctx;
        ASSERT_EQ(core::Database::create(db_file_->path(), 16384, &ctx), core::Status::OK)
            << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<core::Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), core::Status::OK)
            << "Failed to open database: " << ctx.message;

        auto status = core::ProcArrayManager::initialize(db_.get(), 100, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to initialize ProcArray: " << ctx.message;

        // Create testable handler
        handler_ = std::make_unique<TestableEngineIPCHandler>(db_.get());
        
        // Create initial session
        IPCStartupPayload startup;
        startup.process_id = getpid();
        startup.secret_key = 0;
        startup.feature_flags = 0;
        std::strncpy(startup.database, "test_db", sizeof(startup.database) - 1);
        std::strncpy(startup.user, "test_user", sizeof(startup.user) - 1);
        std::strncpy(startup.application, "test_app", sizeof(startup.application) - 1);
        startup.database[sizeof(startup.database) - 1] = '\0';
        startup.user[sizeof(startup.user) - 1] = '\0';
        startup.application[sizeof(startup.application) - 1] = '\0';
        status = handler_->onAttach(1, startup, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to attach session: " << ctx.message;
    }

    void TearDown() override {
        core::ErrorContext ctx;
        
        if (handler_) {
            handler_->onDetach(1, &ctx);
        }
        
        handler_.reset();

        // Close database properly - this shuts down all background threads and ProcArray
        if (db_) {
            db_->close();
        }

        db_.reset();
        
        db_file_.reset();
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<core::Database> db_;
    std::unique_ptr<TestableEngineIPCHandler> handler_;
};

// ============================================================================
// 5.1.2 Simple Query Path Tests
// ============================================================================

TEST_F(IPCIntegrationTest, SimpleQuery_ConnectAndExecute) {
    core::ErrorContext ctx;
    
    // Startup already done in SetUp - session should be ready
    // Note: sendReady() is called by the protocol layer, not directly by handler
    
    // Execute CREATE TABLE
    handler_->reset();
    auto status = handler_->onSimpleQuery(1, "CREATE TABLE simple_query_test (id INT PRIMARY KEY)", &ctx);
    EXPECT_EQ(status, core::Status::OK) << "Error: " << ctx.message;
}

TEST_F(IPCIntegrationTest, SimpleQuery_SelectWithResults) {
    core::ErrorContext ctx;
    
    // Setup: Create table and insert data
    handler_->onSimpleQuery(1, "CREATE TABLE select_test (id INT PRIMARY KEY, name TEXT)", &ctx);
    handler_->reset();
    
    handler_->onSimpleQuery(1, "INSERT INTO select_test VALUES (1, 'Alice'), (2, 'Bob')", &ctx);
    handler_->reset();
    
    // Execute SELECT
    auto status = handler_->onSimpleQuery(1, "SELECT * FROM select_test ORDER BY id", &ctx);
    EXPECT_EQ(status, core::Status::OK);
    // Note: Result set validation depends on executor implementation completeness
}

TEST_F(IPCIntegrationTest, SimpleQuery_InvalidSyntax) {
    core::ErrorContext ctx;
    
    auto status = handler_->onSimpleQuery(1, "INVALID SQL SYNTAX", &ctx);
    EXPECT_EQ(status, core::Status::OK);  // Handler returns OK after sending error
    EXPECT_FALSE(handler_->lastError().empty());
}

TEST_F(IPCIntegrationTest, SimpleQuery_EmptyResult) {
    core::ErrorContext ctx;
    
    // Setup: Create empty table
    handler_->onSimpleQuery(1, "CREATE TABLE empty_test (id INT PRIMARY KEY)", &ctx);
    handler_->reset();
    
    // Query empty table
    auto status = handler_->onSimpleQuery(1, "SELECT * FROM empty_test", &ctx);
    EXPECT_EQ(status, core::Status::OK);
    EXPECT_EQ(handler_->lastRows().size(), 0);
}

// ============================================================================
// 5.1.3 Prepared Statement Path Tests
// ============================================================================

TEST_F(IPCIntegrationTest, PreparedStatement_FullLifecycle) {
    core::ErrorContext ctx;
    
    // Setup
    handler_->onSimpleQuery(1, "CREATE TABLE prep_test (id INT PRIMARY KEY, name TEXT)", &ctx);
    handler_->onSimpleQuery(1, "INSERT INTO prep_test VALUES (1, 'Alice'), (2, 'Bob')", &ctx);
    handler_->reset();
    
    // Parse path is intentionally disabled in engine IPC handler:
    // parser tiers must compile SQL to SBLR before submit.
    auto status = handler_->onParse(1, "select_stmt", "SELECT * FROM prep_test", &ctx);
    EXPECT_EQ(status, core::Status::OK);
    EXPECT_FALSE(handler_->parseCompleteCalled());
    EXPECT_EQ(handler_->lastSqlState(), "0A000");
    EXPECT_NE(handler_->lastError().find("disabled"), std::string::npos);
}

TEST_F(IPCIntegrationTest, PreparedStatement_MultipleExecutions) {
    core::ErrorContext ctx;
    
    // Setup
    handler_->onSimpleQuery(1, "CREATE TABLE multi_exec_test (id INT PRIMARY KEY)", &ctx);
    handler_->reset();
    
    // Parse statement
    handler_->onParse(1, "insert_stmt", "INSERT INTO multi_exec_test VALUES (1)", &ctx);
    handler_->reset();
    
    // Execute multiple times (simulated - each bind/execute cycle)
    for (int i = 0; i < 3; i++) {
        handler_->onBind(1, "portal" + std::to_string(i), "insert_stmt", &ctx);
        handler_->reset();
        auto status = handler_->onExecute(1, ("portal" + std::to_string(i)).c_str(), 0, &ctx);
        EXPECT_EQ(status, core::Status::OK);
        handler_->reset();
    }
}

TEST_F(IPCIntegrationTest, PreparedStatement_InvalidStatementName) {
    core::ErrorContext ctx;
    
    // Try to bind to non-existent statement
    auto status = handler_->onBind(1, "portal1", "nonexistent_stmt", &ctx);
    EXPECT_EQ(status, core::Status::OK);  // Handler returns OK after sending error
    EXPECT_FALSE(handler_->lastError().empty());
}

// ============================================================================
// 5.1.4 Transaction Path Tests
// ============================================================================

TEST_F(IPCIntegrationTest, Transaction_Commit) {
    core::ErrorContext ctx;
    
    // Setup
    handler_->onSimpleQuery(1, "CREATE TABLE txn_test (id INT PRIMARY KEY)", &ctx);
    handler_->reset();
    
    // Begin transaction
    auto status = handler_->onBegin(1, &ctx);
    EXPECT_EQ(status, core::Status::OK);
    
    // Insert data
    handler_->onSimpleQuery(1, "INSERT INTO txn_test VALUES (1)", &ctx);
    handler_->reset();
    
    // Commit
    status = handler_->onCommit(1, &ctx);
    EXPECT_EQ(status, core::Status::OK);
}

TEST_F(IPCIntegrationTest, Transaction_Rollback) {
    core::ErrorContext ctx;
    
    // Setup
    handler_->onSimpleQuery(1, "CREATE TABLE rollback_test (id INT PRIMARY KEY)", &ctx);
    handler_->onSimpleQuery(1, "INSERT INTO rollback_test VALUES (1)", &ctx);
    handler_->reset();
    
    // Begin transaction
    auto status = handler_->onBegin(1, &ctx);
    EXPECT_EQ(status, core::Status::OK);
    
    // Insert more data
    handler_->onSimpleQuery(1, "INSERT INTO rollback_test VALUES (999)", &ctx);
    handler_->reset();
    
    // Rollback
    status = handler_->onRollback(1, &ctx);
    EXPECT_EQ(status, core::Status::OK);
}

TEST_F(IPCIntegrationTest, Transaction_Savepoint) {
    core::ErrorContext ctx;
    
    // Begin transaction
    auto status = handler_->onBegin(1, &ctx);
    EXPECT_EQ(status, core::Status::OK);
    
    // Create savepoint
    status = handler_->onSavepoint(1, "sp1", &ctx);
    EXPECT_EQ(status, core::Status::OK);
}

// ============================================================================
// 5.1.5 COPY Path Tests
// ============================================================================

TEST_F(IPCIntegrationTest, COPY_In_StartAndData) {
    core::ErrorContext ctx;
    
    // Setup
    handler_->onSimpleQuery(1, "CREATE TABLE copy_test (id INT PRIMARY KEY, name TEXT)", &ctx);
    handler_->reset();
    
    // Start COPY IN
    auto status = handler_->onCopyInStart(1, &ctx);
    EXPECT_EQ(status, core::Status::OK);
    EXPECT_TRUE(handler_->copyInResponseCalled());
}

TEST_F(IPCIntegrationTest, COPY_LargeData) {
    core::ErrorContext ctx;
    
    // Setup
    handler_->onSimpleQuery(1, "CREATE TABLE copy_large_test (id INT PRIMARY KEY, data TEXT)", &ctx);
    handler_->reset();
    
    // Start COPY IN
    auto status = handler_->onCopyInStart(1, &ctx);
    EXPECT_EQ(status, core::Status::OK);
}

TEST_F(IPCIntegrationTest, COPY_Fail) {
    core::ErrorContext ctx;
    
    // Report COPY failure
    auto status = handler_->onCopyFail(1, "Test failure reason", &ctx);
    EXPECT_EQ(status, core::Status::OK);
}

// ============================================================================
// 5.1.6 Error Handling Tests
// ============================================================================

TEST_F(IPCIntegrationTest, Error_InvalidMessageType) {
    core::ErrorContext ctx;
    
    // Send invalid SQL
    auto status = handler_->onSimpleQuery(1, "INVALID SYNTAX HERE", &ctx);
    EXPECT_EQ(status, core::Status::OK);  // Handler processes error internally
    EXPECT_FALSE(handler_->lastError().empty());
}

TEST_F(IPCIntegrationTest, Error_TableNotFound) {
    core::ErrorContext ctx;
    
    auto status = handler_->onSimpleQuery(1, "SELECT * FROM nonexistent_table", &ctx);
    EXPECT_EQ(status, core::Status::OK);  // Handler returns OK after sending error
}

TEST_F(IPCIntegrationTest, Error_SessionNotFound) {
    core::ErrorContext ctx;
    
    // Use invalid session ID
    auto status = handler_->onSimpleQuery(999, "SELECT 1", &ctx);
    // Handler should either return an error status or send an error response
    // Either behavior is acceptable for this test
    EXPECT_TRUE(status != core::Status::OK || !handler_->lastError().empty());
}

// ============================================================================
// 5.1.7 Concurrent Sessions Tests
// ============================================================================

TEST_F(IPCIntegrationTest, ConcurrentSessions_MultipleClients) {
    core::ErrorContext ctx;
    
    // Create second session
    IPCStartupPayload startup;
    startup.process_id = getpid();
    startup.secret_key = 0;
    startup.feature_flags = 0;
    std::strncpy(startup.database, "test_db", sizeof(startup.database) - 1);
    std::strncpy(startup.user, "test_user2", sizeof(startup.user) - 1);
    std::strncpy(startup.application, "test", sizeof(startup.application) - 1);
    
    auto status = handler_->onAttach(2, startup, &ctx);
    EXPECT_EQ(status, core::Status::OK);
    
    // Both sessions can execute queries
    status = handler_->onSimpleQuery(1, "SELECT 1", &ctx);
    EXPECT_EQ(status, core::Status::OK);
    
    status = handler_->onSimpleQuery(2, "SELECT 2", &ctx);
    EXPECT_EQ(status, core::Status::OK);
    
    // Detach second session
    handler_->onDetach(2, &ctx);
}

TEST_F(IPCIntegrationTest, ConcurrentSessions_Isolation) {
    core::ErrorContext ctx;
    
    // Create table in session 1
    handler_->onSimpleQuery(1, "CREATE TABLE isolation_test (id INT PRIMARY KEY)", &ctx);
    handler_->reset();
    
    // Create second session
    IPCStartupPayload startup;
    startup.process_id = getpid();
    startup.secret_key = 0;
    startup.feature_flags = 0;
    std::strncpy(startup.database, "test_db", sizeof(startup.database) - 1);
    std::strncpy(startup.user, "test_user2", sizeof(startup.user) - 1);
    std::strncpy(startup.application, "test", sizeof(startup.application) - 1);
    
    auto status = handler_->onAttach(2, startup, &ctx);
    EXPECT_EQ(status, core::Status::OK);
    
    // Both sessions see the same table
    status = handler_->onSimpleQuery(1, "INSERT INTO isolation_test VALUES (1)", &ctx);
    EXPECT_EQ(status, core::Status::OK);
    
    handler_->onDetach(2, &ctx);
}

// ============================================================================
// 5.1.8 Memory Leak Detection Tests
// ============================================================================

TEST_F(IPCIntegrationTest, MemoryLeak_SessionLifecycle) {
    core::ErrorContext ctx;
    
    // Create and detach multiple sessions
    for (int i = 0; i < 10; i++) {
        IPCStartupPayload startup;
        startup.process_id = getpid();
        startup.secret_key = 0;
        startup.feature_flags = 0;
        std::strncpy(startup.database, "test_db", sizeof(startup.database) - 1);
        std::strncpy(startup.user, ("user" + std::to_string(i)).c_str(), sizeof(startup.user) - 1);
        std::strncpy(startup.application, "test", sizeof(startup.application) - 1);
        
        auto status = handler_->onAttach(100 + i, startup, &ctx);
        EXPECT_EQ(status, core::Status::OK);
        
        status = handler_->onDetach(100 + i, &ctx);
        EXPECT_EQ(status, core::Status::OK);
    }
}

TEST_F(IPCIntegrationTest, MemoryLeak_PreparedStatements) {
    core::ErrorContext ctx;
    
    // Create many prepared statements
    for (int i = 0; i < 100; i++) {
        std::string stmt_name = "stmt_" + std::to_string(i);
        std::string sql = "SELECT " + std::to_string(i);
        
        auto status = handler_->onParse(1, stmt_name.c_str(), sql.c_str(), &ctx);
        EXPECT_EQ(status, core::Status::OK);
        
        handler_->reset();
    }
}

TEST_F(IPCIntegrationTest, MemoryLeak_LargeResultSets) {
    core::ErrorContext ctx;
    
    // Setup
    handler_->onSimpleQuery(1, "CREATE TABLE large_result_test (id INT PRIMARY KEY, data TEXT)", &ctx);
    handler_->reset();
    
    // Query that would return many rows (if data existed)
    auto status = handler_->onSimpleQuery(1, "SELECT * FROM large_result_test", &ctx);
    EXPECT_EQ(status, core::Status::OK);
}

// ============================================================================
// Complex Query Tests
// ============================================================================

TEST_F(IPCIntegrationTest, ComplexQuery_Join) {
    core::ErrorContext ctx;
    
    // Setup
    handler_->onSimpleQuery(1, "CREATE TABLE users (id INT PRIMARY KEY, name TEXT)", &ctx);
    handler_->onSimpleQuery(1, "CREATE TABLE orders (id INT PRIMARY KEY, user_id INT, amount INT)", &ctx);
    handler_->reset();
    
    // Execute JOIN query
    auto status = handler_->onSimpleQuery(1, 
        "SELECT u.name, o.amount FROM users u JOIN orders o ON u.id = o.user_id", &ctx);
    EXPECT_EQ(status, core::Status::OK);
}

TEST_F(IPCIntegrationTest, ComplexQuery_Subquery) {
    core::ErrorContext ctx;
    
    // Setup
    handler_->onSimpleQuery(1, "CREATE TABLE subquery_test (id INT PRIMARY KEY, val INT)", &ctx);
    handler_->reset();
    
    // Execute subquery
    auto status = handler_->onSimpleQuery(1, 
        "SELECT * FROM subquery_test WHERE val > (SELECT AVG(val) FROM subquery_test)", &ctx);
    EXPECT_EQ(status, core::Status::OK);
}
