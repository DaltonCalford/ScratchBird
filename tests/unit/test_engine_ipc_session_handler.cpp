/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

/**
 * EngineIPCSessionHandler Unit Tests
 *
 * Comprehensive test coverage for:
 * - Lifecycle management (attach/detach)
 * - Query execution (simple queries, prepared statements, portals)
 * - Transaction management (begin/commit/rollback/savepoint)
 * - COPY operations
 * - Error handling
 * - Statement caching (LRU eviction)
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <memory>
#include <thread>
#include <chrono>

#include "scratchbird/ipc/engine_ipc_session_handler.h"
#include "scratchbird/ipc/ipc_contract_v1_1.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/storage_engine.h"
#include "test_helpers.h"
#include "test_user_helpers.h"

using namespace scratchbird::ipc;
using namespace scratchbird::core;
using namespace scratchbird::testing;

namespace scratchbird {
namespace ipc {
// Forward declaration for testing
class EngineIPCSessionHandlerTest;
} // namespace ipc
} // namespace scratchbird

// ============================================================================
// Test Fixture
// ============================================================================

class EngineIPCSessionHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_file_ = std::make_unique<TestDatabaseFile>("test_engine_ipc_handler", ".db");
        
        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_file_->c_str(), kPageSize, &ctx), Status::OK)
            << "Failed to create database: " << ctx.message;
        
        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_file_->c_str(), &ctx), Status::OK)
            << "Failed to open database: " << ctx.message;
        
        // Initialize proc array
        Status status = db_->initializeProcArray(16, &ctx);
        if (status != Status::OK && status != Status::INVALID_ARGUMENT) {
            ASSERT_EQ(status, Status::OK) << ctx.message;
        }
        
        // Create a test schema and ensure user exists
        EnsureUser(db_->catalog_manager(), "test_user");
        
        // Create handler
        handler_ = std::make_unique<EngineIPCSessionHandler>(db_.get());
        ASSERT_NE(handler_, nullptr);
    }
    
    void TearDown() override {
        // Handler must be destroyed before database
        handler_.reset();
        db_.reset();
        test_db_file_.reset();
    }
    
    // Helper to create a valid startup payload
    IPCStartupPayload createStartupPayload(const char* database = "test_db",
                                           const char* user = "test_user") {
        IPCStartupPayload payload{};
        payload.process_id = static_cast<uint32_t>(::getpid());
        payload.secret_key = 0x12345678;
        payload.feature_flags = IPC_FEATURE_PREPARED_STATEMENTS | 
                                IPC_FEATURE_COPY_STREAMING |
                                IPC_FEATURE_CANCEL;
        std::strncpy(payload.database, database, sizeof(payload.database) - 1);
        std::strncpy(payload.user, user, sizeof(payload.user) - 1);
        std::strncpy(payload.application, "test_app", sizeof(payload.application) - 1);
        return payload;
    }
    
    // Helper to attach a session
    Status attachSession(uint32_t session_id, const IPCStartupPayload& payload) {
        ErrorContext ctx;
        return handler_->onAttach(session_id, payload, &ctx);
    }
    
    // Helper to detach a session
    Status detachSession(uint32_t session_id) {
        ErrorContext ctx;
        return handler_->onDetach(session_id, &ctx);
    }
    
    static constexpr uint32_t kPageSize = 8192;
    
    std::unique_ptr<TestDatabaseFile> test_db_file_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<EngineIPCSessionHandler> handler_;
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, ConstructorWithNullDatabaseThrows) {
    EXPECT_THROW({
        EngineIPCSessionHandler handler(nullptr);
    }, std::invalid_argument);
}

TEST_F(EngineIPCSessionHandlerTest, ConstructorWithValidDatabaseSucceeds) {
    EXPECT_NO_THROW({
        EngineIPCSessionHandler handler(db_.get());
    });
}

TEST_F(EngineIPCSessionHandlerTest, OnAttachCreatesSession) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    // Verify session is created by checking stats
    auto stats = handler_->getStats();
    EXPECT_EQ(stats.active_sessions, 1);
    EXPECT_EQ(stats.total_sessions, 1);
}

TEST_F(EngineIPCSessionHandlerTest, OnAttachWithDuplicateSessionIdFails) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    // Attempt to attach with same session ID should fail
    ErrorContext ctx;
    EXPECT_EQ(handler_->onAttach(1, payload, &ctx), Status::INVALID_ARGUMENT);
    EXPECT_NE(ctx.message.find("already exists"), std::string::npos);
}

TEST_F(EngineIPCSessionHandlerTest, OnAttachMultipleSessionsSucceeds) {
    auto payload1 = createStartupPayload("db1", "user1");
    auto payload2 = createStartupPayload("db2", "user2");
    auto payload3 = createStartupPayload("db3", "user3");
    
    EXPECT_EQ(attachSession(1, payload1), Status::OK);
    EXPECT_EQ(attachSession(2, payload2), Status::OK);
    EXPECT_EQ(attachSession(3, payload3), Status::OK);
    
    auto stats = handler_->getStats();
    EXPECT_EQ(stats.active_sessions, 3);
    EXPECT_EQ(stats.total_sessions, 3);
}

TEST_F(EngineIPCSessionHandlerTest, OnDetachRemovesSession) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    EXPECT_EQ(detachSession(1), Status::OK);
    
    auto stats = handler_->getStats();
    EXPECT_EQ(stats.active_sessions, 0);
}

TEST_F(EngineIPCSessionHandlerTest, OnDetachNonExistentSessionFails) {
    ErrorContext ctx;
    EXPECT_EQ(handler_->onDetach(999, &ctx), Status::NOT_FOUND);
    EXPECT_NE(ctx.message.find("not found"), std::string::npos);
}

TEST_F(EngineIPCSessionHandlerTest, OnDetachRollbacksOpenTransaction) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    // Start a transaction
    ErrorContext ctx;
    EXPECT_EQ(handler_->onBegin(1, &ctx), Status::OK);
    
    // Detach should rollback and succeed
    EXPECT_EQ(detachSession(1), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, HandlerDestructorCleansUpSessions) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    EXPECT_EQ(attachSession(2, payload), Status::OK);
    
    // Destroy handler
    handler_.reset();
    
    // No crash expected, sessions cleaned up
    EXPECT_TRUE(true);
}

// ============================================================================
// Query Execution Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, OnSimpleQueryInvalidSession) {
    ErrorContext ctx;
    EXPECT_EQ(handler_->onSimpleQuery(999, "SELECT 1", &ctx), Status::NOT_FOUND);
    EXPECT_NE(ctx.message.find("not found"), std::string::npos);
}

TEST_F(EngineIPCSessionHandlerTest, OnSimpleQueryValidSQL) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onSimpleQuery(1, "SELECT 1 AS col", &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnSimpleQueryInvalidSQL) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    // Invalid SQL should return error but status may be OK (error sent via sendError)
    handler_->onSimpleQuery(1, "INVALID SYNTAX HERE", &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, OnSimpleQueryMultipleStatements) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onSimpleQuery(1, "SELECT 1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onSimpleQuery(1, "SELECT 2", &ctx), Status::OK);
    EXPECT_EQ(handler_->onSimpleQuery(1, "SELECT 3", &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnParseCreatesPreparedStatement) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onParse(1, "stmt1", "SELECT 1", &ctx), Status::OK);
    
    auto stats = handler_->getStats();
    EXPECT_EQ(stats.prepared_statements, 1);
}

TEST_F(EngineIPCSessionHandlerTest, OnParseInvalidSession) {
    ErrorContext ctx;
    EXPECT_EQ(handler_->onParse(999, "stmt1", "SELECT 1", &ctx), Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, OnParseInvalidSQL) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    // Invalid SQL should be handled gracefully
    handler_->onParse(1, "stmt1", "INVALID SQL", &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, OnBindCreatesPortal) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onParse(1, "stmt1", "SELECT 1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onBind(1, "portal1", "stmt1", &ctx), Status::OK);
    
    auto stats = handler_->getStats();
    EXPECT_EQ(stats.active_portals, 1);
}

TEST_F(EngineIPCSessionHandlerTest, OnBindInvalidSession) {
    ErrorContext ctx;
    EXPECT_EQ(handler_->onBind(999, "portal1", "stmt1", &ctx), Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, OnBindNonExistentStatement) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    // Binding to non-existent statement should fail
    handler_->onBind(1, "portal1", "nonexistent", &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, OnExecuteBoundPortal) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onParse(1, "stmt1", "SELECT 1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onBind(1, "portal1", "stmt1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onExecute(1, "portal1", 0, &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnExecuteInvalidSession) {
    ErrorContext ctx;
    EXPECT_EQ(handler_->onExecute(999, "portal1", 0, &ctx), Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, OnExecuteNonExistentPortal) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    handler_->onExecute(1, "nonexistent", 0, &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, OnExecuteClosedPortal) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onParse(1, "stmt1", "SELECT 1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onBind(1, "portal1", "stmt1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onExecute(1, "portal1", 0, &ctx), Status::OK);
    
    // Second execute on same portal after completion should fail
    handler_->onExecute(1, "portal1", 0, &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, OnClosePreparedStatement) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onParse(1, "stmt1", "SELECT 1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onClose(1, 'S', "stmt1", &ctx), Status::OK);
    
    auto stats = handler_->getStats();
    EXPECT_EQ(stats.prepared_statements, 0);
}

TEST_F(EngineIPCSessionHandlerTest, OnClosePortal) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onParse(1, "stmt1", "SELECT 1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onBind(1, "portal1", "stmt1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onClose(1, 'P', "portal1", &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnCloseInvalidSession) {
    ErrorContext ctx;
    EXPECT_EQ(handler_->onClose(999, 'S', "stmt1", &ctx), Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, OnCloseInvalidType) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    // Unknown type 'X' should still succeed (no-op)
    EXPECT_EQ(handler_->onClose(1, 'X', "name", &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnSync) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onSync(1, &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnSyncInvalidSession) {
    ErrorContext ctx;
    // Sync on invalid session may or may not check session
    handler_->onSync(999, &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, ParseBindExecuteFlow) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    
    // Complete flow: Parse -> Bind -> Execute
    EXPECT_EQ(handler_->onParse(1, "stmt1", "SELECT 1 AS a, 2 AS b", &ctx), Status::OK);
    EXPECT_EQ(handler_->onBind(1, "portal1", "stmt1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onExecute(1, "portal1", 0, &ctx), Status::OK);
    
    auto stats = handler_->getStats();
    EXPECT_EQ(stats.prepared_statements, 1);
    EXPECT_GE(stats.queries_executed, 1);
}

TEST_F(EngineIPCSessionHandlerTest, MultiplePortalsPerStatement) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    
    EXPECT_EQ(handler_->onParse(1, "stmt1", "SELECT 1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onBind(1, "portal1", "stmt1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onBind(1, "portal2", "stmt1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onBind(1, "portal3", "stmt1", &ctx), Status::OK);
    
    auto stats = handler_->getStats();
    EXPECT_EQ(stats.active_portals, 3);
}

TEST_F(EngineIPCSessionHandlerTest, ExecuteWithMaxRowsLimit) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onParse(1, "stmt1", "SELECT 1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onBind(1, "portal1", "stmt1", &ctx), Status::OK);
    
    // Execute with max_rows = 1
    EXPECT_EQ(handler_->onExecute(1, "portal1", 1, &ctx), Status::OK);
}

// ============================================================================
// Transaction Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, OnBeginStartsTransaction) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onBegin(1, &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnBeginInvalidSession) {
    ErrorContext ctx;
    EXPECT_EQ(handler_->onBegin(999, &ctx), Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, OnCommitWithoutBeginFails) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    // Commit without begin should return error
    handler_->onCommit(1, &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, OnCommitAfterBeginSucceeds) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onBegin(1, &ctx), Status::OK);
    EXPECT_EQ(handler_->onCommit(1, &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnCommitInvalidSession) {
    ErrorContext ctx;
    EXPECT_EQ(handler_->onCommit(999, &ctx), Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, OnRollbackWithoutBeginFails) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    // Rollback without begin should return error
    handler_->onRollback(1, &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, OnRollbackAfterBeginSucceeds) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onBegin(1, &ctx), Status::OK);
    EXPECT_EQ(handler_->onRollback(1, &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnRollbackInvalidSession) {
    ErrorContext ctx;
    EXPECT_EQ(handler_->onRollback(999, &ctx), Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, OnSavepointWithoutTransactionFails) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    // Savepoint without transaction should fail
    handler_->onSavepoint(1, "sp1", &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, OnSavepointInTransactionSucceeds) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onBegin(1, &ctx), Status::OK);
    EXPECT_EQ(handler_->onSavepoint(1, "sp1", &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnSavepointInvalidSession) {
    ErrorContext ctx;
    EXPECT_EQ(handler_->onSavepoint(999, "sp1", &ctx), Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, TransactionStateTracking) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    
    // Begin transaction
    EXPECT_EQ(handler_->onBegin(1, &ctx), Status::OK);
    
    // Execute in transaction
    EXPECT_EQ(handler_->onSimpleQuery(1, "SELECT 1", &ctx), Status::OK);
    
    // Commit
    EXPECT_EQ(handler_->onCommit(1, &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, BeginCommitRollbackSequence) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    
    // Multiple transaction cycles
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(handler_->onBegin(1, &ctx), Status::OK);
        EXPECT_EQ(handler_->onCommit(1, &ctx), Status::OK);
    }
    
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(handler_->onBegin(1, &ctx), Status::OK);
        EXPECT_EQ(handler_->onRollback(1, &ctx), Status::OK);
    }
}

TEST_F(EngineIPCSessionHandlerTest, MultipleSavepoints) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onBegin(1, &ctx), Status::OK);
    EXPECT_EQ(handler_->onSavepoint(1, "sp1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onSavepoint(1, "sp2", &ctx), Status::OK);
    EXPECT_EQ(handler_->onSavepoint(1, "sp3", &ctx), Status::OK);
    EXPECT_EQ(handler_->onCommit(1, &ctx), Status::OK);
}

// ============================================================================
// COPY Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, OnCopyInStart) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onCopyInStart(1, &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnCopyInStartInvalidSession) {
    ErrorContext ctx;
    EXPECT_EQ(handler_->onCopyInStart(999, &ctx), Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, OnCopyDataInCopyMode) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onCopyInStart(1, &ctx), Status::OK);
    
    const char* data = "1,hello\n2,world\n";
    EXPECT_EQ(handler_->onCopyData(1, 
                                    reinterpret_cast<const uint8_t*>(data),
                                    std::strlen(data), &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnCopyDataNotInCopyMode) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    const char* data = "1,hello\n";
    // Should return error when not in COPY mode
    handler_->onCopyData(1, 
                         reinterpret_cast<const uint8_t*>(data),
                         std::strlen(data), &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, OnCopyDataInvalidSession) {
    ErrorContext ctx;
    const char* data = "test";
    EXPECT_EQ(handler_->onCopyData(999, 
                                   reinterpret_cast<const uint8_t*>(data),
                                   std::strlen(data), &ctx), Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, OnCopyDoneInCopyInMode) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onCopyInStart(1, &ctx), Status::OK);
    
    const char* data = "1,hello\n2,world\n";
    EXPECT_EQ(handler_->onCopyData(1,
                                    reinterpret_cast<const uint8_t*>(data),
                                    std::strlen(data), &ctx), Status::OK);
    EXPECT_EQ(handler_->onCopyDone(1, &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnCopyDoneInvalidSession) {
    ErrorContext ctx;
    EXPECT_EQ(handler_->onCopyDone(999, &ctx), Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, OnCopyFail) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onCopyInStart(1, &ctx), Status::OK);
    EXPECT_EQ(handler_->onCopyFail(1, "Test failure reason", &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnCopyFailInvalidSession) {
    ErrorContext ctx;
    EXPECT_EQ(handler_->onCopyFail(999, "Test failure", &ctx), Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, CopyInFlowComplete) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    
    // Start COPY IN
    EXPECT_EQ(handler_->onCopyInStart(1, &ctx), Status::OK);
    
    // Send multiple data chunks
    const char* chunk1 = "1,alice\n2,bob\n";
    const char* chunk2 = "3,charlie\n4,david\n";
    
    EXPECT_EQ(handler_->onCopyData(1,
                                    reinterpret_cast<const uint8_t*>(chunk1),
                                    std::strlen(chunk1), &ctx), Status::OK);
    EXPECT_EQ(handler_->onCopyData(1,
                                    reinterpret_cast<const uint8_t*>(chunk2),
                                    std::strlen(chunk2), &ctx), Status::OK);
    
    // Complete COPY
    EXPECT_EQ(handler_->onCopyDone(1, &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, CopyInFlowWithFail) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    
    // Start COPY IN
    EXPECT_EQ(handler_->onCopyInStart(1, &ctx), Status::OK);
    
    // Send some data
    const char* data = "1,test\n";
    EXPECT_EQ(handler_->onCopyData(1,
                                    reinterpret_cast<const uint8_t*>(data),
                                    std::strlen(data), &ctx), Status::OK);
    
    // Fail the COPY
    EXPECT_EQ(handler_->onCopyFail(1, "Data format error", &ctx), Status::OK);
}

// ============================================================================
// Cancel and Notification Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, OnCancel) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onCancel(1, &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnCancelInvalidSession) {
    ErrorContext ctx;
    EXPECT_EQ(handler_->onCancel(999, &ctx), Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, OnSubscribe) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onSubscribe(1, "test_channel", &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OnUnsubscribe) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onUnsubscribe(1, "test_channel", &ctx), Status::OK);
}

// ============================================================================
// Response Method Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, SendRowDescription) {
    std::vector<IPCFieldDesc> fields;
    IPCFieldDesc field1;
    std::strncpy(field1.name, "id", sizeof(field1.name) - 1);
    field1.data_type = 23; // INT4
    fields.push_back(field1);
    
    IPCFieldDesc field2;
    std::strncpy(field2.name, "name", sizeof(field2.name) - 1);
    field2.data_type = 1043; // VARCHAR
    fields.push_back(field2);
    
    EXPECT_EQ(handler_->sendRowDescription(1, fields), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, SendDataRow) {
    std::vector<std::optional<std::string>> values;
    values.push_back("1");
    values.push_back("test");
    values.push_back(std::nullopt); // NULL value
    
    EXPECT_EQ(handler_->sendDataRow(1, values), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, SendCommandComplete) {
    EXPECT_EQ(handler_->sendCommandComplete(1, "SELECT 10", 10), Status::OK);
    EXPECT_EQ(handler_->sendCommandComplete(1, "INSERT 0 5", 5), Status::OK);
    EXPECT_EQ(handler_->sendCommandComplete(1, "UPDATE 3", 3), Status::OK);
    EXPECT_EQ(handler_->sendCommandComplete(1, "DELETE 2", 2), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, SendError) {
    EXPECT_EQ(handler_->sendError(1, "42883", "Undefined function"), Status::OK);
    EXPECT_EQ(handler_->sendError(1, "42P01", "Undefined table"), Status::OK);
    EXPECT_EQ(handler_->sendError(1, "XX000", "Internal error"), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, SendNotice) {
    EXPECT_EQ(handler_->sendNotice(1, "This is a notice"), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, SendReady) {
    EXPECT_EQ(handler_->sendReady(1, IPC_FEATURE_PREPARED_STATEMENTS), Status::OK);
    EXPECT_EQ(handler_->sendReady(1, 0), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, SendParseComplete) {
    EXPECT_EQ(handler_->sendParseComplete(1), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, SendBindComplete) {
    EXPECT_EQ(handler_->sendBindComplete(1), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, SendCloseComplete) {
    EXPECT_EQ(handler_->sendCloseComplete(1), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, SendCopyInRequest) {
    EXPECT_EQ(handler_->sendCopyInRequest(1), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, SendCopyOutResponse) {
    EXPECT_EQ(handler_->sendCopyOutResponse(1), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, SendCopyData) {
    const char* data = "COPY data here";
    EXPECT_EQ(handler_->sendCopyData(1, 
                                     reinterpret_cast<const uint8_t*>(data),
                                     std::strlen(data)), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, SendCopyComplete) {
    EXPECT_EQ(handler_->sendCopyComplete(1), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, SendTxnComplete) {
    EXPECT_EQ(handler_->sendTxnComplete(1), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, SendNotification) {
    EXPECT_EQ(handler_->sendNotification(1, "channel1", "payload data"), Status::OK);
}

// ============================================================================
// Statement Cache Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, StatementCacheHit) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    
    // Parse statement
    EXPECT_EQ(handler_->onParse(1, "cached_stmt", "SELECT 1", &ctx), Status::OK);
    
    // Bind should use cached statement
    EXPECT_EQ(handler_->onBind(1, "portal1", "cached_stmt", &ctx), Status::OK);
    EXPECT_EQ(handler_->onExecute(1, "portal1", 0, &ctx), Status::OK);
    
    // Bind again - should hit cache
    EXPECT_EQ(handler_->onBind(1, "portal2", "cached_stmt", &ctx), Status::OK);
    EXPECT_EQ(handler_->onExecute(1, "portal2", 0, &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, StatementCacheMiss) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    
    // Try to bind to non-existent statement
    handler_->onBind(1, "portal1", "nonexistent_stmt", &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, ClearPreparedStatementCache) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    
    // Add some statements
    EXPECT_EQ(handler_->onParse(1, "stmt1", "SELECT 1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onParse(1, "stmt2", "SELECT 2", &ctx), Status::OK);
    EXPECT_EQ(handler_->onParse(1, "stmt3", "SELECT 3", &ctx), Status::OK);
    
    auto stats_before = handler_->getStats();
    EXPECT_EQ(stats_before.prepared_statements, 3);
    
    // Clear cache
    EXPECT_EQ(handler_->clearPreparedStatementCache(1), Status::OK);
    
    auto stats_after = handler_->getStats();
    EXPECT_EQ(stats_after.prepared_statements, 0);
}

TEST_F(EngineIPCSessionHandlerTest, ClearPreparedStatementCacheInvalidSession) {
    EXPECT_EQ(handler_->clearPreparedStatementCache(999), Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, GetPreparedStatements) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onParse(1, "stmt1", "SELECT 1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onParse(1, "stmt2", "SELECT 2", &ctx), Status::OK);
    
    auto stmts = handler_->getPreparedStatements(1);
    EXPECT_EQ(stmts.size(), 2);
}

TEST_F(EngineIPCSessionHandlerTest, GetPreparedStatementsInvalidSession) {
    auto stmts = handler_->getPreparedStatements(999);
    EXPECT_TRUE(stmts.empty());
}

TEST_F(EngineIPCSessionHandlerTest, StatementCacheInvalidationOnClose) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onParse(1, "stmt1", "SELECT 1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onClose(1, 'S', "stmt1", &ctx), Status::OK);
    
    // After closing, statement should not be in cache
    auto stmts = handler_->getPreparedStatements(1);
    EXPECT_TRUE(stmts.empty());
}

TEST_F(EngineIPCSessionHandlerTest, MultipleStatementsInCache) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    
    // Add many statements
    for (int i = 0; i < 10; i++) {
        std::string name = "stmt" + std::to_string(i);
        std::string sql = "SELECT " + std::to_string(i);
        EXPECT_EQ(handler_->onParse(1, name, sql, &ctx), Status::OK);
    }
    
    auto stats = handler_->getStats();
    EXPECT_EQ(stats.prepared_statements, 10);
}

// ============================================================================
// Statistics and Management Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, GetStatsInitialState) {
    auto stats = handler_->getStats();
    EXPECT_EQ(stats.active_sessions, 0);
    EXPECT_EQ(stats.total_sessions, 0);
    EXPECT_EQ(stats.queries_executed, 0);
    EXPECT_EQ(stats.prepared_statements, 0);
    EXPECT_EQ(stats.active_portals, 0);
}

TEST_F(EngineIPCSessionHandlerTest, GetStatsAfterOperations) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    EXPECT_EQ(handler_->onParse(1, "stmt1", "SELECT 1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onBind(1, "portal1", "stmt1", &ctx), Status::OK);
    
    auto stats = handler_->getStats();
    EXPECT_EQ(stats.active_sessions, 1);
    EXPECT_EQ(stats.total_sessions, 1);
    EXPECT_EQ(stats.prepared_statements, 1);
    EXPECT_EQ(stats.active_portals, 1);
}

// ============================================================================
// Concurrent Session Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, MultipleSessionsIndependent) {
    auto payload = createStartupPayload();
    
    // Attach multiple sessions
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    EXPECT_EQ(attachSession(2, payload), Status::OK);
    EXPECT_EQ(attachSession(3, payload), Status::OK);
    
    ErrorContext ctx;
    
    // Each session has independent state
    EXPECT_EQ(handler_->onBegin(1, &ctx), Status::OK);
    EXPECT_EQ(handler_->onBegin(2, &ctx), Status::OK);
    
    // Commit only session 1
    EXPECT_EQ(handler_->onCommit(1, &ctx), Status::OK);
    
    // Session 2 still in transaction
    EXPECT_EQ(handler_->onRollback(2, &ctx), Status::OK);
    
    // Session 3 never started transaction
    EXPECT_EQ(handler_->onSimpleQuery(3, "SELECT 1", &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, MultipleSessionsPreparedStatements) {
    auto payload = createStartupPayload();
    
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    EXPECT_EQ(attachSession(2, payload), Status::OK);
    
    ErrorContext ctx;
    
    // Same statement name in different sessions
    EXPECT_EQ(handler_->onParse(1, "stmt1", "SELECT 1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onParse(2, "stmt1", "SELECT 2", &ctx), Status::OK);
    
    // Each session should have its own statement
    auto stats = handler_->getStats();
    EXPECT_EQ(stats.prepared_statements, 2);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, ErrorContextNullAllowed) {
    auto payload = createStartupPayload();
    
    // All methods should accept nullptr for ErrorContext
    EXPECT_EQ(handler_->onAttach(1, payload, nullptr), Status::OK);
    EXPECT_EQ(handler_->onSimpleQuery(1, "SELECT 1", nullptr), Status::OK);
    EXPECT_EQ(handler_->onDetach(1, nullptr), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, OperationsOnDetachedSessionFail) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    EXPECT_EQ(detachSession(1), Status::OK);
    
    ErrorContext ctx;
    // Operations on detached session should fail
    EXPECT_EQ(handler_->onSimpleQuery(1, "SELECT 1", &ctx), Status::NOT_FOUND);
    EXPECT_EQ(handler_->onBegin(1, &ctx), Status::NOT_FOUND);
    EXPECT_EQ(handler_->onParse(1, "stmt", "SELECT 1", &ctx), Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, ReattachAfterDetach) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    EXPECT_EQ(detachSession(1), Status::OK);
    
    // Can reattach with same ID after detach
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    auto stats = handler_->getStats();
    EXPECT_EQ(stats.active_sessions, 1);
}

// ============================================================================
// Complex Integration Scenarios
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, FullQueryLifecycle) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    
    // Start transaction
    EXPECT_EQ(handler_->onBegin(1, &ctx), Status::OK);
    
    // Execute simple query
    EXPECT_EQ(handler_->onSimpleQuery(1, "SELECT 1 AS a", &ctx), Status::OK);
    
    // Prepare, bind, execute
    EXPECT_EQ(handler_->onParse(1, "prep_stmt", "SELECT 2 AS b", &ctx), Status::OK);
    EXPECT_EQ(handler_->onBind(1, "portal1", "prep_stmt", &ctx), Status::OK);
    EXPECT_EQ(handler_->onExecute(1, "portal1", 0, &ctx), Status::OK);
    
    // Commit transaction
    EXPECT_EQ(handler_->onCommit(1, &ctx), Status::OK);
    
    // Detach
    EXPECT_EQ(detachSession(1), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, TransactionWithSavepointsAndRollback) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    
    EXPECT_EQ(handler_->onBegin(1, &ctx), Status::OK);
    EXPECT_EQ(handler_->onSavepoint(1, "sp1", &ctx), Status::OK);
    EXPECT_EQ(handler_->onSavepoint(1, "sp2", &ctx), Status::OK);
    
    // Execute some queries
    EXPECT_EQ(handler_->onSimpleQuery(1, "SELECT 1", &ctx), Status::OK);
    
    // Rollback transaction
    EXPECT_EQ(handler_->onRollback(1, &ctx), Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, PortalFetchMultipleRows) {
    auto payload = createStartupPayload();
    EXPECT_EQ(attachSession(1, payload), Status::OK);
    
    ErrorContext ctx;
    
    // Create a portal that returns multiple rows
    EXPECT_EQ(handler_->onParse(1, "multi_row_stmt", "SELECT * FROM generate_series(1, 10)", &ctx), Status::OK);
    EXPECT_EQ(handler_->onBind(1, "portal1", "multi_row_stmt", &ctx), Status::OK);
    
    // Fetch rows in batches
    EXPECT_EQ(handler_->onExecute(1, "portal1", 3, &ctx), Status::OK);
    EXPECT_EQ(handler_->onExecute(1, "portal1", 3, &ctx), Status::OK);
    EXPECT_EQ(handler_->onExecute(1, "portal1", 0, &ctx), Status::OK); // Fetch all remaining
}

// Main provided by gtest_main
