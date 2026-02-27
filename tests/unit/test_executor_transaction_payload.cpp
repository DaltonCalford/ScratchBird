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
#include <sstream>
#include <vector>
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/firebird_query_compiler.h"
#include "scratchbird/sblr/postgresql_query_compiler.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

using scratchbird::core::ConnectionContext;
using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::IsolationLevel;
using scratchbird::core::ReadCommittedMode;
using scratchbird::core::Status;
using scratchbird::sblr::Executor;
using scratchbird::sblr::FirebirdQueryCompiler;
using scratchbird::sblr::PostgreSQLQueryCompiler;
using scratchbird::sblr::QueryCompilerV3;
using scratchbird::testing::TestDatabaseFile;

namespace {

std::string joinErrors(const std::vector<std::string>& errors) {
    std::ostringstream oss;
    for (size_t i = 0; i < errors.size(); ++i) {
        if (i > 0) {
            oss << "; ";
        }
        oss << errors[i];
    }
    return oss.str();
}

} // namespace

class ExecutorTransactionPayloadTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("txn_payload", ".sbdb");
        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.connect(conn_, &ctx), Status::OK) << ctx.message;

        // Set current schema to PUBLIC
        scratchbird::core::CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(db_.catalog_manager()->getSchema("PUBLIC", schema_info, &ctx), Status::OK)
            << ctx.message;
        conn_->setCurrentSchemaId(schema_info.schema_id);
        auto system_user_id = db_.catalog_manager()->getSystemUserId(&ctx);
        conn_->setCurrentUser(system_user_id, true);
        ConnectionContext::setCurrent(conn_.get());

        executor_ = std::make_unique<Executor>(&db_);
        executor_->setConnectionContext(conn_.get());
    }

    void TearDown() override {
        executor_.reset();
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        db_.close();
        db_file_.reset();
    }

    scratchbird::sblr::CompilationResultV3 compile(const std::string& sql) {
        QueryCompilerV3 compiler(&db_);
        return compiler.compile(sql);
    }

    scratchbird::sblr::PostgreSQLCompilationResult compilePostgreSQL(const std::string& sql) {
        PostgreSQLQueryCompiler compiler(nullptr);
        compiler.setDefaultSchema("main");
        return compiler.compile(sql);
    }

    scratchbird::sblr::FirebirdCompilationResult compileFirebird(const std::string& sql) {
        FirebirdQueryCompiler compiler(&db_);
        return compiler.compile(sql);
    }

    void startTransaction() {
        auto compiled = compile("START TRANSACTION");
        ASSERT_TRUE(compiled.success()) << joinErrors(compiled.errors());
        auto result = executor_->execute(compiled.bytecode());
        ASSERT_TRUE(result.success()) << result.error();
    }

    Database db_{};
    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> conn_;
};

TEST_F(ExecutorTransactionPayloadTest, StartTransactionReadConsistencySetsIsolation) {
    auto compiled = compile(
        "START TRANSACTION ISOLATION LEVEL READ COMMITTED READ CONSISTENCY");
    ASSERT_TRUE(compiled.success()) << joinErrors(compiled.errors());

    auto result = executor_->execute(compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();
    EXPECT_EQ(conn_->getIsolationLevel(),
              IsolationLevel::READ_COMMITTED_READ_CONSISTENCY);
    EXPECT_EQ(conn_->getReadCommittedMode(), ReadCommittedMode::READ_CONSISTENCY);
}

TEST_F(ExecutorTransactionPayloadTest, StartTransactionNoRecordVersionKeepsReadCommitted) {
    auto compiled = compile(
        "START TRANSACTION ISOLATION LEVEL READ COMMITTED NO RECORD VERSION");
    ASSERT_TRUE(compiled.success()) << joinErrors(compiled.errors());

    auto result = executor_->execute(compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();
    EXPECT_EQ(conn_->getIsolationLevel(), IsolationLevel::READ_COMMITTED);
    EXPECT_EQ(conn_->getReadCommittedMode(),
              ReadCommittedMode::NO_RECORD_VERSION);
}

TEST_F(ExecutorTransactionPayloadTest, AutocommitOnCommitsAfterStatement) {
    auto set_compiled = compile("SET AUTOCOMMIT ON");
    ASSERT_TRUE(set_compiled.success()) << joinErrors(set_compiled.errors());
    auto result = executor_->execute(set_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    uint64_t xid_before = conn_->getCurrentXid();

    auto ddl_compiled = compile("CREATE TABLE autocommit_on_test (id INT)");
    ASSERT_TRUE(ddl_compiled.success()) << joinErrors(ddl_compiled.errors());
    result = executor_->execute(ddl_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    uint64_t xid_after = conn_->getCurrentXid();
    EXPECT_NE(xid_before, xid_after);
}

TEST_F(ExecutorTransactionPayloadTest, AutocommitOffKeepsXid) {
    auto set_compiled = compile("SET AUTOCOMMIT OFF");
    ASSERT_TRUE(set_compiled.success()) << joinErrors(set_compiled.errors());
    auto result = executor_->execute(set_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    uint64_t xid_before = conn_->getCurrentXid();

    auto ddl_compiled = compile("CREATE TABLE autocommit_off_test (id INT)");
    ASSERT_TRUE(ddl_compiled.success()) << joinErrors(ddl_compiled.errors());
    result = executor_->execute(ddl_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    uint64_t xid_after = conn_->getCurrentXid();
    EXPECT_EQ(xid_before, xid_after);
}

TEST_F(ExecutorTransactionPayloadTest, PrepareCommitPrepared) {
    auto *txn_manager = db_.transaction_manager();
    ASSERT_NE(txn_manager, nullptr);

    uint64_t prepared_xid = conn_->getCurrentXid();

    auto prepare_compiled = compile("PREPARE TRANSACTION 'gid_commit_test'");
    ASSERT_TRUE(prepare_compiled.success()) << joinErrors(prepare_compiled.errors());
    auto result = executor_->execute(prepare_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    scratchbird::core::TransactionState state = scratchbird::core::TransactionState::ACTIVE;
    ErrorContext err_ctx;
    ASSERT_EQ(txn_manager->getTransactionState(prepared_xid, state, &err_ctx), Status::OK)
        << err_ctx.message;
    EXPECT_EQ(state, scratchbird::core::TransactionState::PREPARED);

    auto commit_compiled = compile("COMMIT PREPARED 'gid_commit_test'");
    ASSERT_TRUE(commit_compiled.success()) << joinErrors(commit_compiled.errors());
    result = executor_->execute(commit_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    ASSERT_EQ(txn_manager->getTransactionState(prepared_xid, state, &err_ctx), Status::OK)
        << err_ctx.message;
    EXPECT_EQ(state, scratchbird::core::TransactionState::COMMITTED);
}

TEST_F(ExecutorTransactionPayloadTest, PrepareRollbackPrepared) {
    auto *txn_manager = db_.transaction_manager();
    ASSERT_NE(txn_manager, nullptr);

    uint64_t prepared_xid = conn_->getCurrentXid();

    auto prepare_compiled = compile("PREPARE TRANSACTION 'gid_rollback_test'");
    ASSERT_TRUE(prepare_compiled.success()) << joinErrors(prepare_compiled.errors());
    auto result = executor_->execute(prepare_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    scratchbird::core::TransactionState state = scratchbird::core::TransactionState::ACTIVE;
    ErrorContext err_ctx;
    ASSERT_EQ(txn_manager->getTransactionState(prepared_xid, state, &err_ctx), Status::OK)
        << err_ctx.message;
    EXPECT_EQ(state, scratchbird::core::TransactionState::PREPARED);

    auto rollback_compiled = compile("ROLLBACK PREPARED 'gid_rollback_test'");
    ASSERT_TRUE(rollback_compiled.success()) << joinErrors(rollback_compiled.errors());
    result = executor_->execute(rollback_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    ASSERT_EQ(txn_manager->getTransactionState(prepared_xid, state, &err_ctx), Status::OK)
        << err_ctx.message;
    EXPECT_EQ(state, scratchbird::core::TransactionState::ABORTED);
}

TEST_F(ExecutorTransactionPayloadTest, SavepointSqlCreatesExpectedSavepoint) {
    startTransaction();

    auto savepoint_compiled = compile("SAVEPOINT blr_sp_1");
    ASSERT_TRUE(savepoint_compiled.success()) << joinErrors(savepoint_compiled.errors());
    auto result = executor_->execute(savepoint_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    ErrorContext err_ctx;
    auto dup_status = conn_->createSavepoint("blr_sp_1", &err_ctx);
    EXPECT_EQ(dup_status, Status::INVALID_ARGUMENT) << err_ctx.message;

    ErrorContext release_ctx;
    EXPECT_EQ(conn_->releaseSavepoint("blr_sp_1", &release_ctx), Status::OK)
        << release_ctx.message;
}

TEST_F(ExecutorTransactionPayloadTest, SavepointSqlReleaseAllowsReuse) {
    startTransaction();

    auto savepoint_compiled = compile("SAVEPOINT blr_sp_1");
    ASSERT_TRUE(savepoint_compiled.success()) << joinErrors(savepoint_compiled.errors());
    auto result = executor_->execute(savepoint_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto release_compiled = compile("RELEASE SAVEPOINT blr_sp_1");
    ASSERT_TRUE(release_compiled.success()) << joinErrors(release_compiled.errors());
    result = executor_->execute(release_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    ErrorContext err_ctx;
    EXPECT_EQ(conn_->createSavepoint("blr_sp_1", &err_ctx), Status::OK)
        << err_ctx.message;

    ErrorContext release_ctx;
    EXPECT_EQ(conn_->releaseSavepoint("blr_sp_1", &release_ctx), Status::OK)
        << release_ctx.message;
}

TEST_F(ExecutorTransactionPayloadTest, ExistsSubqueryReturnsDeterministicBoolean) {
    auto create_compiled = compile("CREATE TABLE exists_eval_test (id INT PRIMARY KEY)");
    ASSERT_TRUE(create_compiled.success()) << joinErrors(create_compiled.errors());
    auto result = executor_->execute(create_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto insert_compiled = compile("INSERT INTO exists_eval_test(id) VALUES (1)");
    ASSERT_TRUE(insert_compiled.success()) << joinErrors(insert_compiled.errors());
    result = executor_->execute(insert_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto exists_true_compiled = compile(
        "SELECT EXISTS(SELECT 1 FROM exists_eval_test WHERE id = 1)");
    ASSERT_TRUE(exists_true_compiled.success()) << joinErrors(exists_true_compiled.errors());
    result = executor_->execute(exists_true_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    ASSERT_FALSE(result.resultSet()->getValue(0, 0).isNull());
    EXPECT_TRUE(result.resultSet()->getValue(0, 0).getBool());

    auto exists_false_compiled = compile(
        "SELECT EXISTS(SELECT 1 FROM exists_eval_test WHERE id = 999)");
    ASSERT_TRUE(exists_false_compiled.success()) << joinErrors(exists_false_compiled.errors());
    result = executor_->execute(exists_false_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    ASSERT_FALSE(result.resultSet()->getValue(0, 0).isNull());
    EXPECT_FALSE(result.resultSet()->getValue(0, 0).getBool());
}

TEST_F(ExecutorTransactionPayloadTest, RollbackToSavepointRestoresPreUpdateRowImage) {
    startTransaction();

    auto create_compiled = compile("CREATE TABLE savepoint_restore_test (id INT PRIMARY KEY, val INT)");
    ASSERT_TRUE(create_compiled.success()) << joinErrors(create_compiled.errors());
    auto result = executor_->execute(create_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto insert_compiled = compile("INSERT INTO savepoint_restore_test(id, val) VALUES (1, 10)");
    ASSERT_TRUE(insert_compiled.success()) << joinErrors(insert_compiled.errors());
    result = executor_->execute(insert_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto savepoint_compiled = compile("SAVEPOINT sp_rollback_restore");
    ASSERT_TRUE(savepoint_compiled.success()) << joinErrors(savepoint_compiled.errors());
    result = executor_->execute(savepoint_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto update_compiled = compile("UPDATE savepoint_restore_test SET val = 11 WHERE id = 1");
    ASSERT_TRUE(update_compiled.success()) << joinErrors(update_compiled.errors());
    result = executor_->execute(update_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto rollback_compiled = compile("ROLLBACK TO SAVEPOINT sp_rollback_restore");
    ASSERT_TRUE(rollback_compiled.success()) << joinErrors(rollback_compiled.errors());
    result = executor_->execute(rollback_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto select_compiled = compile("SELECT val FROM savepoint_restore_test WHERE id = 1");
    ASSERT_TRUE(select_compiled.success()) << joinErrors(select_compiled.errors());
    result = executor_->execute(select_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    ASSERT_FALSE(result.resultSet()->getValue(0, 0).isNull());
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toInt64(), 10);
}

TEST_F(ExecutorTransactionPayloadTest, PostgresCompilerExistsReturnsBoolean) {
    auto create_compiled = compilePostgreSQL("CREATE TABLE pg_exists_eval_test (id INT PRIMARY KEY)");
    ASSERT_TRUE(create_compiled.success()) << joinErrors(create_compiled.errors());
    auto result = executor_->execute(create_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto insert_compiled = compilePostgreSQL("INSERT INTO pg_exists_eval_test(id) VALUES (1)");
    ASSERT_TRUE(insert_compiled.success()) << joinErrors(insert_compiled.errors());
    result = executor_->execute(insert_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto exists_compiled =
        compilePostgreSQL("SELECT EXISTS(SELECT 1 FROM pg_exists_eval_test WHERE id = 1)");
    ASSERT_TRUE(exists_compiled.success()) << joinErrors(exists_compiled.errors());
    result = executor_->execute(exists_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 1U);
    ASSERT_EQ(result.resultSet()->columnCount(), 1U);
    EXPECT_FALSE(result.resultSet()->getValue(0, 0).isNull());
    EXPECT_TRUE(result.resultSet()->getValue(0, 0).getBool());
}

TEST_F(ExecutorTransactionPayloadTest, PostgresCompilerWhereNotExistsWithoutFromAppliesPredicate) {
    auto create_compiled =
        compilePostgreSQL("CREATE TABLE pg_exists_where_test (id INT PRIMARY KEY)");
    ASSERT_TRUE(create_compiled.success()) << joinErrors(create_compiled.errors());
    auto result = executor_->execute(create_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto insert_compiled = compilePostgreSQL("INSERT INTO pg_exists_where_test(id) VALUES (1)");
    ASSERT_TRUE(insert_compiled.success()) << joinErrors(insert_compiled.errors());
    result = executor_->execute(insert_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto false_predicate_compiled = compilePostgreSQL(
        "SELECT 'fail' WHERE NOT EXISTS (SELECT 1 FROM pg_exists_where_test WHERE id = 1)");
    ASSERT_TRUE(false_predicate_compiled.success())
        << joinErrors(false_predicate_compiled.errors());
    result = executor_->execute(false_predicate_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    EXPECT_EQ(result.resultSet()->rowCount(), 0U);

    auto true_predicate_compiled = compilePostgreSQL(
        "SELECT 'pass' WHERE NOT EXISTS (SELECT 1 FROM pg_exists_where_test WHERE id = 999)");
    ASSERT_TRUE(true_predicate_compiled.success())
        << joinErrors(true_predicate_compiled.errors());
    result = executor_->execute(true_predicate_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 1U);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "pass");
}

TEST_F(ExecutorTransactionPayloadTest, PostgresCompilerRollbackToSavepointRestoresSameTxnInsert) {
    auto create_compiled = compilePostgreSQL(
        "CREATE TABLE pg_savepoint_same_txn_test (id INT PRIMARY KEY, val INT)");
    ASSERT_TRUE(create_compiled.success()) << joinErrors(create_compiled.errors());
    auto result = executor_->execute(create_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto begin_compiled = compilePostgreSQL("BEGIN");
    ASSERT_TRUE(begin_compiled.success()) << joinErrors(begin_compiled.errors());
    result = executor_->execute(begin_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto insert_compiled = compilePostgreSQL(
        "INSERT INTO pg_savepoint_same_txn_test(id, val) VALUES (1, 30)");
    ASSERT_TRUE(insert_compiled.success()) << joinErrors(insert_compiled.errors());
    result = executor_->execute(insert_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto savepoint_compiled = compilePostgreSQL("SAVEPOINT sp1");
    ASSERT_TRUE(savepoint_compiled.success()) << joinErrors(savepoint_compiled.errors());
    result = executor_->execute(savepoint_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto update_compiled =
        compilePostgreSQL("UPDATE pg_savepoint_same_txn_test SET val = 31 WHERE id = 1");
    ASSERT_TRUE(update_compiled.success()) << joinErrors(update_compiled.errors());
    result = executor_->execute(update_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto rollback_to_compiled = compilePostgreSQL("ROLLBACK TO SAVEPOINT sp1");
    ASSERT_TRUE(rollback_to_compiled.success()) << joinErrors(rollback_to_compiled.errors());
    result = executor_->execute(rollback_to_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto commit_compiled = compilePostgreSQL("COMMIT");
    ASSERT_TRUE(commit_compiled.success()) << joinErrors(commit_compiled.errors());
    result = executor_->execute(commit_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto verify_compiled = compilePostgreSQL("SELECT val FROM pg_savepoint_same_txn_test WHERE id = 1");
    ASSERT_TRUE(verify_compiled.success()) << joinErrors(verify_compiled.errors());
    result = executor_->execute(verify_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 1U);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toInt64(), 30);
}

TEST_F(ExecutorTransactionPayloadTest, FirebirdSetTransactionExecutesWithoutUnknownOpcode) {
    auto compiled = compileFirebird("SET TRANSACTION");
    ASSERT_TRUE(compiled.success()) << joinErrors(compiled.errors());

    auto result = executor_->execute(compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();
}

TEST_F(ExecutorTransactionPayloadTest, FirebirdSetAutoddlOffIsAcceptedAsSessionVariable) {
    auto compiled = compileFirebird("SET AUTODDL OFF");
    ASSERT_TRUE(compiled.success()) << joinErrors(compiled.errors());

    conn_->set_dialect_tag("FIREBIRD");
    auto result = executor_->execute(compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();
}

TEST_F(ExecutorTransactionPayloadTest, FirebirdIifExistsReturnsPass) {
    auto ddl = compileFirebird(
        "RECREATE TABLE fb_iif_eval (id INTEGER NOT NULL PRIMARY KEY)");
    ASSERT_TRUE(ddl.success()) << joinErrors(ddl.errors());
    auto result = executor_->execute(ddl.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto insert = compileFirebird("INSERT INTO fb_iif_eval(id) VALUES (1)");
    ASSERT_TRUE(insert.success()) << joinErrors(insert.errors());
    result = executor_->execute(insert.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    auto query = compileFirebird(
        "SELECT IIF(EXISTS(SELECT 1 FROM fb_iif_eval WHERE id = 1), 'PASS', 'FAIL') "
        "FROM fb_iif_eval");
    ASSERT_TRUE(query.success()) << joinErrors(query.errors());
    result = executor_->execute(query.bytecode());
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 1U);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "PASS");
}
