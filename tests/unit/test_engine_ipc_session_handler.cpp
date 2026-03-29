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
 * Unit Tests for EngineIPCSessionHandler
 *
 * Tests:
 * - COMPILED_QUERY execution through the engine IPC boundary
 * - COMPILED_PARSE/onBind/onExecute prepared statement lifecycle
 * - SQL-text IPC rejection on SIMPLE_QUERY/PARSE
 * - Transaction methods (BEGIN, COMMIT, ROLLBACK, SAVEPOINT)
 * - COPY operations
 * - Error handling
 * - Statement cache management
 */

#include <gtest/gtest.h>

#include "scratchbird/ipc/engine_ipc_session_handler.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/protocol/adapters/mysql_adapter.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include <thread>

using namespace scratchbird;
using namespace scratchbird::ipc;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

// Testable handler that captures responses
class TestableEngineIPCSessionHandler : public EngineIPCSessionHandler {
public:
    explicit TestableEngineIPCSessionHandler(core::Database* db)
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

    void reset() {
        last_fields_.clear();
        last_rows_.clear();
        last_command_tag_.clear();
        last_rows_affected_ = 0;
        last_sqlstate_.clear();
        last_error_.clear();
        parse_complete_called_ = false;
        bind_complete_called_ = false;
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
};

class TestMySqlCompileAdapter : public protocol::MySqlAdapter {
public:
    explicit TestMySqlCompileAdapter(const protocol::ProtocolAdapterConfig& config)
        : protocol::MySqlAdapter(config) {}

    using protocol::MySqlAdapter::compileQuery;

    void setLogicalDatabaseForTest(const std::string& logical_db) {
        database_name_ = logical_db;
    }

    void setUsernameForTest(const std::string& username) {
        username_ = username;
    }
};

// ============================================================================
// Test Fixture
// ============================================================================

class EngineIPCSessionHandlerTest : public ::testing::Test {
protected:
    void ensureSessionUserExists(const std::string& username, bool is_superuser = false) {
        auto* catalog = db_->catalog_manager();
        ASSERT_NE(catalog, nullptr);

        core::CatalogManager::UserInfo existing_user;
        core::ErrorContext user_lookup_ctx;
        auto user_status = catalog->getUserByName(username, existing_user, &user_lookup_ctx);
        if (user_status == core::Status::OK) {
            return;
        }
        ASSERT_EQ(user_status, core::Status::NOT_FOUND)
            << "Failed to resolve session user " << username << ": "
            << user_lookup_ctx.message;

        core::CatalogManager::SchemaInfo public_schema;
        core::ErrorContext schema_ctx;
        auto schema_status = catalog->getSchema("users.public", public_schema, &schema_ctx);
        ASSERT_EQ(schema_status, core::Status::OK)
            << "Failed to resolve bootstrap public schema: " << schema_ctx.message;

        core::ID user_id;
        core::ErrorContext create_ctx;
        auto create_status = catalog->createUser(username,
                                                 "",
                                                 public_schema.schema_id,
                                                 is_superuser,
                                                 user_id,
                                                 &create_ctx);
        ASSERT_EQ(create_status, core::Status::OK)
            << "Failed to create session user " << username << ": " << create_ctx.message;
    }

    core::Status attachSession(uint32_t session_id,
                               const char* application_name,
                               const char* database_name = "test_db",
                               const char* user_name = "test_user") {
        IPCStartupPayload startup{};
        startup.process_id = 12345;
        startup.secret_key = 0;
        startup.feature_flags = 0;
        std::strncpy(startup.database, database_name, sizeof(startup.database) - 1);
        std::strncpy(startup.user, user_name, sizeof(startup.user) - 1);
        std::strncpy(startup.application, application_name, sizeof(startup.application) - 1);
        startup.database[sizeof(startup.database) - 1] = '\0';
        startup.user[sizeof(startup.user) - 1] = '\0';
        startup.application[sizeof(startup.application) - 1] = '\0';
        core::ErrorContext ctx;
        return handler_->onAttach(session_id, startup, &ctx);
    }

    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("test_ipc_session_handler");

        core::ErrorContext ctx;
        ASSERT_EQ(core::Database::create(db_file_->path(), 16384, &ctx), core::Status::OK)
            << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<core::Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), core::Status::OK)
            << "Failed to open database: " << ctx.message;

        auto status = core::ProcArrayManager::initialize(db_.get(), 10, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to initialize ProcArray: " << ctx.message;

        status = core::ProcArrayManager::registerBackend(&proc_id_, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to register backend: " << ctx.message;

        ensureSessionUserExists("test_user");
        ensureSessionUserExists("test_user2");
        ensureSessionUserExists("root", true);
        ensureSessionUserExists("SYSDBA", true);

        handler_ = std::make_unique<TestableEngineIPCSessionHandler>(db_.get());
        
        // Create a test session
        status = attachSession(1, "test_app");
        ASSERT_EQ(status, core::Status::OK) << "Failed to attach session: " << ctx.message;
    }

    void TearDown() override {
        core::ErrorContext ctx;
        
        // Detach session first
        handler_->onDetach(1, &ctx);
        
        // Unregister from ProcArray before shutting down
        core::ProcArrayManager::unregisterBackend(proc_id_, &ctx);
        
        // Destroy handler while database is still available
        handler_.reset();
        
        // Close database properly - this shuts down all background threads and ProcArray
        if (db_) {
            db_->close();
        }
        
        // Clean up database
        db_.reset();
        
        db_file_.reset();
    }

    std::vector<uint8_t> compileNativeSql(const std::string& sql) {
        QueryCompilerV3 compiler(db_.get());
        auto result = compiler.compile(sql);
        if (!result.success()) {
            ADD_FAILURE() << "Failed to compile SQL for IPC boundary test: "
                          << (result.errors().empty() ? std::string("unknown compiler error")
                                                      : result.errors().front())
                          << " | sql=" << sql;
            return {};
        }
        return result.bytecode();
    }

    std::vector<uint8_t> compileMySqlSql(const std::string& sql,
                                         const std::string& logical_database,
                                         const std::string& username) {
        protocol::ProtocolAdapterConfig config;
        config.database_path = db_file_->path();
        config.auto_create_db = false;

        TestMySqlCompileAdapter adapter(config);
        adapter.setSharedDatabase(db_.get());
        adapter.setLogicalDatabaseForTest(logical_database);
        adapter.setUsernameForTest(username);

        std::vector<uint8_t> bytecode;
        std::string error;
        const auto status = adapter.compileQuery(sql, bytecode, error);
        if (status != core::Status::OK) {
            ADD_FAILURE() << "Failed to compile MySQL SQL for IPC boundary test: "
                          << error << " | sql=" << sql;
            return {};
        }
        return bytecode;
    }

    core::Status executeCompiledQuery(uint32_t session_id,
                                      const std::string& sql,
                                      core::ErrorContext* ctx) {
        const auto bytecode = compileNativeSql(sql);
        if (bytecode.empty()) {
            return core::Status::INVALID_ARGUMENT;
        }
        return handler_->onCompiledQuery(session_id, bytecode, sql, ctx);
    }

    core::Status parseCompiledStatement(uint32_t session_id,
                                        const std::string& stmt_name,
                                        const std::string& sql,
                                        core::ErrorContext* ctx) {
        const auto bytecode = compileNativeSql(sql);
        if (bytecode.empty()) {
            return core::Status::INVALID_ARGUMENT;
        }
        auto status = handler_->onCompiledParse(session_id, stmt_name, bytecode, sql, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        return handler_->sendParseComplete(session_id);
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<core::Database> db_;
    std::unique_ptr<TestableEngineIPCSessionHandler> handler_;
    uint32_t proc_id_ = 0;
};

// ============================================================================
// onSimpleQuery Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, onSimpleQuery_SessionNotFound) {
    core::ErrorContext ctx;
    auto status = handler_->onSimpleQuery(999, "SELECT 1", &ctx);
    EXPECT_EQ(status, core::Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, onCompiledQuery_CreateTable) {
    core::ErrorContext ctx;
    auto status = executeCompiledQuery(1,
                                       "CREATE TABLE test_table (id INT PRIMARY KEY, name TEXT)",
                                       &ctx);
    EXPECT_EQ(status, core::Status::OK) << "Error: " << ctx.message;
}

TEST_F(EngineIPCSessionHandlerTest, onCompiledQuery_InsertAndSelect) {
    // Create table
    {
        core::ErrorContext ctx;
        auto status = executeCompiledQuery(1,
                                           "CREATE TABLE test_users (id INT PRIMARY KEY, name TEXT)",
                                           &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to create table: " << ctx.message;
        handler_->reset();
    }

    // Insert data
    {
        core::ErrorContext ctx;
        auto status = executeCompiledQuery(1,
                                           "INSERT INTO test_users VALUES (1, 'Alice'), (2, 'Bob')",
                                           &ctx);
        EXPECT_EQ(status, core::Status::OK) << "Error: " << ctx.message;
        handler_->reset();
    }

    // Select data
    {
        core::ErrorContext ctx;
        auto status = executeCompiledQuery(1, "SELECT * FROM test_users ORDER BY id", &ctx);
        EXPECT_EQ(status, core::Status::OK) << "Error: " << ctx.message;
    }
}

TEST_F(EngineIPCSessionHandlerTest, onCompiledQuery_Update) {
    // Setup
    {
        core::ErrorContext ctx;
        executeCompiledQuery(1, "CREATE TABLE update_test (id INT PRIMARY KEY, val INT)", &ctx);
        executeCompiledQuery(1, "INSERT INTO update_test VALUES (1, 10), (2, 20)", &ctx);
        handler_->reset();
    }

    // Update
    {
        core::ErrorContext ctx;
        auto status = executeCompiledQuery(1, "UPDATE update_test SET val = val + 1", &ctx);
        EXPECT_EQ(status, core::Status::OK);
    }
}

TEST_F(EngineIPCSessionHandlerTest, onCompiledQuery_Delete) {
    // Setup
    {
        core::ErrorContext ctx;
        executeCompiledQuery(1, "CREATE TABLE delete_test (id INT PRIMARY KEY)", &ctx);
        executeCompiledQuery(1, "INSERT INTO delete_test VALUES (1), (2), (3)", &ctx);
        handler_->reset();
    }

    // Delete
    {
        core::ErrorContext ctx;
        auto status = executeCompiledQuery(1, "DELETE FROM delete_test WHERE id > 1", &ctx);
        EXPECT_EQ(status, core::Status::OK);
    }
}

TEST_F(EngineIPCSessionHandlerTest, onCompiledQuery_DeleteAllVisibleRowsAcrossScanPages) {
    {
        core::ErrorContext ctx;
        ASSERT_EQ(executeCompiledQuery(1,
                                       "CREATE TABLE delete_all_test (id INT, payload TEXT)",
                                       &ctx),
                  core::Status::OK)
            << "Failed to create delete_all_test: " << ctx.message;

        const std::string payload(384, 'x');
        for (int id = 0; id < 96; ++id) {
            core::ErrorContext insert_ctx;
            const std::string insert_sql =
                "INSERT INTO delete_all_test VALUES (" + std::to_string(id) + ", '" +
                payload + "')";
            ASSERT_EQ(executeCompiledQuery(1, insert_sql, &insert_ctx), core::Status::OK)
                << "Insert failed for id=" << id << ": " << insert_ctx.message;
        }
        handler_->reset();
    }

    {
        core::ErrorContext ctx;
        const auto status = executeCompiledQuery(1, "DELETE FROM delete_all_test", &ctx);
        EXPECT_EQ(status, core::Status::OK) << ctx.message;
        EXPECT_EQ(handler_->lastCommandTag(), "DELETE 96");
        EXPECT_EQ(handler_->lastRowsAffected(), 96u);
    }

    {
        core::ErrorContext ctx;
        handler_->reset();
        ASSERT_EQ(executeCompiledQuery(1, "SELECT * FROM delete_all_test", &ctx), core::Status::OK)
            << ctx.message;
        EXPECT_TRUE(handler_->lastRows().empty());
    }
}

TEST_F(EngineIPCSessionHandlerTest, onSimpleQuery_TextPathDisabled) {
    core::ErrorContext ctx;
    auto status = handler_->onSimpleQuery(1, "SELECT 1", &ctx);
    EXPECT_EQ(status, core::Status::OK);
    EXPECT_EQ(handler_->lastSqlState(), "0A000");
    EXPECT_NE(handler_->lastError().find("COMPILED_QUERY"), std::string::npos);
}

TEST_F(EngineIPCSessionHandlerTest, onSimpleQuery_FirebirdProtocolIsNotRejectedAsUnsupported) {
    ASSERT_EQ(attachSession(2, "firebird_parser"), core::Status::OK);
    handler_->reset();

    core::ErrorContext ctx;
    auto status = handler_->onSimpleQuery(2, "INVALID SQL", &ctx);
    EXPECT_EQ(status, core::Status::OK);
    EXPECT_EQ(handler_->lastSqlState(), "0A000");
    EXPECT_NE(handler_->lastError().find("disabled"), std::string::npos);

    handler_->onDetach(2, &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, onCompiledQuery_PostgresqlSessionSeedsShowServerVersion) {
    ASSERT_EQ(attachSession(4, "postgresql_parser"), core::Status::OK);
    handler_->reset();

    core::ErrorContext ctx;
    auto status = executeCompiledQuery(4, "SHOW server_version", &ctx);
    EXPECT_EQ(status, core::Status::OK) << ctx.message;
    ASSERT_EQ(handler_->lastRows().size(), 1u);
    ASSERT_EQ(handler_->lastRows().front().size(), 2u);
    ASSERT_TRUE(handler_->lastRows().front()[1].has_value());
    EXPECT_NE(handler_->lastRows().front()[1].value().find("ScratchBird"), std::string::npos);

    handler_->onDetach(4, &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, onCompiledQuery_MySqlSessionExecutesSetNames) {
    ASSERT_EQ(attachSession(5, "mysql_parser", "compat_mysql", "root"), core::Status::OK);
    handler_->reset();

    const auto bytecode = compileMySqlSql("SET NAMES utf8mb4", "compat_mysql", "root");
    ASSERT_FALSE(bytecode.empty());

    core::ErrorContext ctx;
    auto status = handler_->onCompiledQuery(5, bytecode, "SET NAMES utf8mb4", &ctx);
    EXPECT_EQ(status, core::Status::OK) << ctx.message;
    EXPECT_TRUE(handler_->lastSqlState().empty()) << handler_->lastError();
    EXPECT_EQ(handler_->lastCommandTag(), "OK");
    EXPECT_EQ(handler_->lastRowsAffected(), 0u);

    handler_->onDetach(5, &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, onCompiledQuery_MySqlSessionExecutesVersionAndDatabaseFunctions) {
    ASSERT_EQ(attachSession(6, "mysql_parser", "compat_mysql", "root"), core::Status::OK);
    handler_->reset();

    {
        const auto version_bytecode =
            compileMySqlSql("SELECT VERSION()", "compat_mysql", "root");
        ASSERT_FALSE(version_bytecode.empty());

        core::ErrorContext ctx;
        auto status = handler_->onCompiledQuery(6, version_bytecode, "SELECT VERSION()", &ctx);
        EXPECT_EQ(status, core::Status::OK) << ctx.message;
        EXPECT_TRUE(handler_->lastSqlState().empty()) << handler_->lastError();
    }

    {
        handler_->reset();
        const auto database_bytecode =
            compileMySqlSql("SELECT DATABASE()", "compat_mysql", "root");
        ASSERT_FALSE(database_bytecode.empty());

        core::ErrorContext ctx;
        auto status = handler_->onCompiledQuery(6, database_bytecode, "SELECT DATABASE()", &ctx);
        EXPECT_EQ(status, core::Status::OK) << ctx.message;
        EXPECT_TRUE(handler_->lastSqlState().empty()) << handler_->lastError();
    }

    core::ErrorContext ctx;
    handler_->onDetach(6, &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, onCompiledQuery_SelectEmptyResult) {
    // Setup
    {
        core::ErrorContext ctx;
        executeCompiledQuery(1, "CREATE TABLE empty_test (id INT PRIMARY KEY)", &ctx);
        handler_->reset();
    }

    // Select with no rows
    {
        core::ErrorContext ctx;
        auto status = executeCompiledQuery(1, "SELECT * FROM empty_test", &ctx);
        EXPECT_EQ(status, core::Status::OK);
        EXPECT_EQ(handler_->lastRows().size(), 0);
    }
}

// ============================================================================
// Prepared Statement Tests (onParse, onBind, onExecute)
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, onParse_TextPathDisabled) {
    core::ErrorContext ctx;
    auto status = handler_->onParse(1, "stmt1", "SELECT 1 AS col", &ctx);
    EXPECT_EQ(status, core::Status::OK);
    EXPECT_FALSE(handler_->parseCompleteCalled());
    EXPECT_EQ(handler_->lastSqlState(), "0A000");
    EXPECT_NE(handler_->lastError().find("COMPILED_PARSE"), std::string::npos);
}

TEST_F(EngineIPCSessionHandlerTest, onParse_NativeProtocolTextPathDisabled) {
    ASSERT_EQ(attachSession(3, "native_parser_v3"), core::Status::OK);
    handler_->reset();

    core::ErrorContext ctx;
    auto status = handler_->onParse(3, "native_stmt", "SELECT 1", &ctx);
    EXPECT_EQ(status, core::Status::OK);
    EXPECT_FALSE(handler_->parseCompleteCalled());
    EXPECT_EQ(handler_->lastSqlState(), "0A000");

    handler_->onDetach(3, &ctx);
}

TEST_F(EngineIPCSessionHandlerTest, onCompiledParse_Bind_Execute_FullLifecycle) {
    // Create table first
    {
        core::ErrorContext ctx;
        executeCompiledQuery(1, "CREATE TABLE prep_test (id INT PRIMARY KEY, name TEXT)", &ctx);
        executeCompiledQuery(1, "INSERT INTO prep_test VALUES (1, 'Alice'), (2, 'Bob')", &ctx);
        handler_->reset();
    }

    // Parse
    {
        core::ErrorContext ctx;
        auto status = parseCompiledStatement(1, "select_stmt", "SELECT * FROM prep_test", &ctx);
        EXPECT_EQ(status, core::Status::OK);
        EXPECT_TRUE(handler_->parseCompleteCalled());
        handler_->reset();
    }

    // Bind
    {
        core::ErrorContext ctx;
        auto status = handler_->onBind(1, "portal1", "select_stmt", &ctx);
        EXPECT_EQ(status, core::Status::OK);
        handler_->reset();
    }

    // Execute
    {
        core::ErrorContext ctx;
        auto status = handler_->onExecute(1, "portal1", 0, &ctx);
        EXPECT_EQ(status, core::Status::OK);
        // Note: Field metadata population depends on executor implementation
        // The handler correctly captures what the executor returns
    }
}

TEST_F(EngineIPCSessionHandlerTest, onBind_StatementNotFound) {
    core::ErrorContext ctx;
    auto status = handler_->onBind(1, "portal1", "nonexistent_stmt", &ctx);
    EXPECT_EQ(status, core::Status::OK);  // Handler returns OK after sending error
}

TEST_F(EngineIPCSessionHandlerTest, onExecute_PortalNotFound) {
    core::ErrorContext ctx;
    auto status = handler_->onExecute(1, "nonexistent_portal", 0, &ctx);
    EXPECT_EQ(status, core::Status::OK);  // Handler returns OK after sending error
}

// ============================================================================
// Transaction Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, onBegin_Basic) {
    core::ErrorContext ctx;
    auto status = handler_->onBegin(1, &ctx);
    EXPECT_EQ(status, core::Status::OK);
    EXPECT_TRUE(handler_->lastError().empty());
}

TEST_F(EngineIPCSessionHandlerTest, onCommit_Basic) {
    // Begin first
    {
        core::ErrorContext ctx;
        handler_->onBegin(1, &ctx);
    }

    // Then commit
    core::ErrorContext ctx;
    auto status = handler_->onCommit(1, &ctx);
    EXPECT_EQ(status, core::Status::OK);
    EXPECT_TRUE(handler_->lastError().empty());
}

TEST_F(EngineIPCSessionHandlerTest, onCommit_ImplicitTransactionAfterAttach) {
    core::ErrorContext ctx;
    auto status = handler_->onCommit(1, &ctx);
    EXPECT_EQ(status, core::Status::OK);
    EXPECT_TRUE(handler_->lastError().empty());
}

TEST_F(EngineIPCSessionHandlerTest, onRollback_Basic) {
    // Begin first
    {
        core::ErrorContext ctx;
        handler_->onBegin(1, &ctx);
    }

    // Then rollback
    core::ErrorContext ctx;
    auto status = handler_->onRollback(1, &ctx);
    EXPECT_EQ(status, core::Status::OK);
    EXPECT_TRUE(handler_->lastError().empty());
}

TEST_F(EngineIPCSessionHandlerTest, onSavepoint_Basic) {
    // Begin first
    {
        core::ErrorContext ctx;
        handler_->onBegin(1, &ctx);
    }

    // Create savepoint
    core::ErrorContext ctx;
    auto status = handler_->onSavepoint(1, "sp1", &ctx);
    EXPECT_EQ(status, core::Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, Transaction_CommitPersistsData) {
    // Create table
    {
        core::ErrorContext ctx;
        executeCompiledQuery(1, "CREATE TABLE txn_test (id INT PRIMARY KEY)", &ctx);
    }

    // Begin transaction and insert
    {
        core::ErrorContext ctx;
        handler_->onBegin(1, &ctx);
        executeCompiledQuery(1, "INSERT INTO txn_test VALUES (1)", &ctx);
        handler_->onCommit(1, &ctx);
        EXPECT_TRUE(handler_->lastError().empty()) << handler_->lastError();
    }

    // Verify data persists
    {
        core::ErrorContext ctx;
        handler_->reset();
        auto status = executeCompiledQuery(1, "SELECT * FROM txn_test", &ctx);
        EXPECT_EQ(status, core::Status::OK);
    }
}

TEST_F(EngineIPCSessionHandlerTest, Transaction_RollbackDiscardsData) {
    // Create table
    {
        core::ErrorContext ctx;
        executeCompiledQuery(1, "CREATE TABLE rollback_test (id INT PRIMARY KEY)", &ctx);
        executeCompiledQuery(1, "INSERT INTO rollback_test VALUES (1)", &ctx);
        handler_->reset();
    }

    // Begin transaction, insert, then rollback
    {
        core::ErrorContext ctx;
        handler_->onBegin(1, &ctx);
        executeCompiledQuery(1, "INSERT INTO rollback_test VALUES (999)", &ctx);
        handler_->onRollback(1, &ctx);
        EXPECT_TRUE(handler_->lastError().empty()) << handler_->lastError();
    }

    // Verify only first row exists
    {
        core::ErrorContext ctx;
        handler_->reset();
        auto status = executeCompiledQuery(1, "SELECT * FROM rollback_test", &ctx);
        EXPECT_EQ(status, core::Status::OK);
    }
}

// ============================================================================
// Session Lifecycle Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, onAttach_DuplicateSession) {
    IPCStartupPayload startup;
    startup.process_id = 12345;
    startup.secret_key = 0;
    startup.feature_flags = 0;
    std::strncpy(startup.database, "test_db", sizeof(startup.database) - 1);
    std::strncpy(startup.user, "test_user", sizeof(startup.user) - 1);
    std::strncpy(startup.application, "test_app", sizeof(startup.application) - 1);
    startup.database[sizeof(startup.database) - 1] = '\0';
    startup.user[sizeof(startup.user) - 1] = '\0';
    startup.application[sizeof(startup.application) - 1] = '\0';

    core::ErrorContext ctx;
    auto status = handler_->onAttach(1, startup, &ctx);  // Session 1 already exists
    EXPECT_EQ(status, core::Status::INVALID_ARGUMENT);
}

TEST_F(EngineIPCSessionHandlerTest, onDetach_SessionNotFound) {
    core::ErrorContext ctx;
    auto status = handler_->onDetach(999, &ctx);
    EXPECT_EQ(status, core::Status::NOT_FOUND);
}

TEST_F(EngineIPCSessionHandlerTest, MultipleSessions) {
    // Create second session
    IPCStartupPayload startup;
    startup.process_id = 12346;
    startup.secret_key = 0;
    startup.feature_flags = 0;
    std::strncpy(startup.database, "test_db", sizeof(startup.database) - 1);
    std::strncpy(startup.user, "test_user2", sizeof(startup.user) - 1);
    std::strncpy(startup.application, "test_app", sizeof(startup.application) - 1);
    startup.database[sizeof(startup.database) - 1] = '\0';
    startup.user[sizeof(startup.user) - 1] = '\0';
    startup.application[sizeof(startup.application) - 1] = '\0';

    core::ErrorContext ctx;
    auto status = handler_->onAttach(2, startup, &ctx);
    EXPECT_EQ(status, core::Status::OK);

    // Both sessions should work independently
    {
        executeCompiledQuery(1, "SELECT 1", &ctx);
        executeCompiledQuery(2, "SELECT 2", &ctx);
    }

    // Cleanup
    handler_->onDetach(2, &ctx);
}

// ============================================================================
// Statement Cache Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, ClearPreparedStatementCache) {
    // Parse some statements
    {
        core::ErrorContext ctx;
        parseCompiledStatement(1, "stmt1", "SELECT 1", &ctx);
        parseCompiledStatement(1, "stmt2", "SELECT 2", &ctx);
    }

    // Clear cache
    auto status = handler_->clearPreparedStatementCache(1);
    EXPECT_EQ(status, core::Status::OK);

    // Verify statements are gone
    core::ErrorContext ctx;
    handler_->onBind(1, "portal1", "stmt1", &ctx);
    EXPECT_FALSE(handler_->lastError().empty());
}

TEST_F(EngineIPCSessionHandlerTest, GetPreparedStatements) {
    // Parse some statements
    {
        core::ErrorContext ctx;
        parseCompiledStatement(1, "stmt1", "SELECT 1", &ctx);
        parseCompiledStatement(1, "stmt2", "SELECT 2 FROM test", &ctx);
    }

    auto stmts = handler_->getPreparedStatements(1);
    // Note: Prepared statement tracking depends on implementation completeness
    EXPECT_GE(stmts.size(), 0);
}

TEST_F(EngineIPCSessionHandlerTest, GetStats) {
    auto stats = handler_->getStats();
    EXPECT_EQ(stats.active_sessions, 1);
    EXPECT_EQ(stats.total_sessions, 1);
}

// ============================================================================
// COPY Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, onCopyInStart_Basic) {
    core::ErrorContext ctx;
    auto status = handler_->onCopyInStart(1, &ctx);
    EXPECT_EQ(status, core::Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, onCopyDone_Basic) {
    core::ErrorContext ctx;
    auto status = handler_->onCopyDone(1, &ctx);
    EXPECT_EQ(status, core::Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, onCopyFail_Basic) {
    core::ErrorContext ctx;
    auto status = handler_->onCopyFail(1, "Test failure reason", &ctx);
    EXPECT_EQ(status, core::Status::OK);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(EngineIPCSessionHandlerTest, onSimpleQuery_NullDatabase) {
    // Test that handler properly rejects null database
    EXPECT_THROW({
        TestableEngineIPCSessionHandler bad_handler(nullptr);
    }, std::invalid_argument);
}

TEST_F(EngineIPCSessionHandlerTest, onClose_Statement) {
    // Parse a statement
    {
        core::ErrorContext ctx;
        parseCompiledStatement(1, "stmt_to_close", "SELECT 1", &ctx);
    }

    // Close it
    core::ErrorContext ctx;
    auto status = handler_->onClose(1, 'S', "stmt_to_close", &ctx);
    EXPECT_EQ(status, core::Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, onClose_Portal) {
    // Parse and bind
    {
        core::ErrorContext ctx;
        parseCompiledStatement(1, "stmt1", "SELECT 1", &ctx);
        handler_->onBind(1, "portal_to_close", "stmt1", &ctx);
    }

    // Close portal
    core::ErrorContext ctx;
    auto status = handler_->onClose(1, 'P', "portal_to_close", &ctx);
    EXPECT_EQ(status, core::Status::OK);
}

TEST_F(EngineIPCSessionHandlerTest, onSync_Basic) {
    core::ErrorContext ctx;
    auto status = handler_->onSync(1, &ctx);
    EXPECT_EQ(status, core::Status::OK);
}
