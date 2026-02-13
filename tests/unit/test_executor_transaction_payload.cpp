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
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

using scratchbird::core::ConnectionContext;
using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::IsolationLevel;
using scratchbird::core::ReadCommittedMode;
using scratchbird::core::Status;
using scratchbird::sblr::Executor;
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

void appendExtendedOpcode(std::vector<uint8_t>& bytecode, uint16_t opcode) {
    bytecode.push_back(static_cast<uint8_t>(scratchbird::sblr::Opcode::EXTENDED_OPCODE));
    bytecode.push_back(static_cast<uint8_t>(opcode & 0xFF));
    bytecode.push_back(static_cast<uint8_t>((opcode >> 8) & 0xFF));
}

void appendExtendedOpcode(std::vector<uint8_t>& bytecode,
                          scratchbird::sblr::ExtendedOpcode opcode) {
    appendExtendedOpcode(bytecode, static_cast<uint16_t>(opcode));
}

std::vector<uint8_t> buildBlrSavepointBeginBytecode() {
    std::vector<uint8_t> bytecode;
    bytecode.push_back(static_cast<uint8_t>(scratchbird::sblr::Opcode::VERSION));
    bytecode.push_back(scratchbird::sblr::SBLR_VERSION);
    appendExtendedOpcode(bytecode, scratchbird::sblr::ExtendedOpcode::EXT_SAVEPOINT_BEGIN);
    return bytecode;
}

std::vector<uint8_t> buildBlrSavepointBlockBytecode() {
    std::vector<uint8_t> bytecode;
    bytecode.push_back(static_cast<uint8_t>(scratchbird::sblr::Opcode::VERSION));
    bytecode.push_back(scratchbird::sblr::SBLR_VERSION);
    appendExtendedOpcode(bytecode, scratchbird::sblr::ExtendedOpcode::EXT_BLOCK);
    bytecode.push_back(0); // variable declaration count
    appendExtendedOpcode(bytecode, scratchbird::sblr::ExtendedOpcode::EXT_SAVEPOINT_BEGIN);
    appendExtendedOpcode(bytecode, scratchbird::sblr::ExtendedOpcode::EXT_SAVEPOINT_END);
    appendExtendedOpcode(bytecode, 0x00FF); // block end marker
    return bytecode;
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

        executor_ = std::make_unique<Executor>(&db_);
        executor_->setConnectionContext(conn_.get());
    }

    void TearDown() override {
        executor_.reset();
        conn_.reset();
        db_.close();
        db_file_.reset();
    }

    scratchbird::sblr::CompilationResultV3 compile(const std::string& sql) {
        QueryCompilerV3 compiler(&db_);
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

TEST_F(ExecutorTransactionPayloadTest, BlrSavepointBeginCreatesImplicitSavepoint) {
    startTransaction();

    auto result = executor_->execute(buildBlrSavepointBeginBytecode());
    ASSERT_TRUE(result.success()) << result.error();

    ErrorContext err_ctx;
    auto dup_status = conn_->createSavepoint("blr_sp_1", &err_ctx);
    EXPECT_EQ(dup_status, Status::INVALID_ARGUMENT) << err_ctx.message;

    ErrorContext release_ctx;
    EXPECT_EQ(conn_->releaseSavepoint("blr_sp_1", &release_ctx), Status::OK)
        << release_ctx.message;
}

TEST_F(ExecutorTransactionPayloadTest, BlrSavepointBlockReleasesImplicitSavepoint) {
    startTransaction();

    auto result = executor_->execute(buildBlrSavepointBlockBytecode());
    ASSERT_TRUE(result.success()) << result.error();

    ErrorContext err_ctx;
    EXPECT_EQ(conn_->createSavepoint("blr_sp_1", &err_ctx), Status::OK)
        << err_ctx.message;

    ErrorContext release_ctx;
    EXPECT_EQ(conn_->releaseSavepoint("blr_sp_1", &release_ctx), Status::OK)
        << release_ctx.message;
}
