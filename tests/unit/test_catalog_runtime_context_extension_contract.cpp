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

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#define private public
#include "scratchbird/core/catalog_manager.h"
#undef private
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

class CatalogRuntimeContextExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_runtime_context_extension_contract_" +
                   std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_)
        {
            db_->close();
            db_.reset();
            catalog_ = nullptr;
        }
        std::remove(db_path_.c_str());
    }
};

TEST_F(CatalogRuntimeContextExtensionContractTest, ConnectionContracts)
{
    ErrorContext ctx;

    CatalogManager::RuntimeConnectionCatalogInfo invalid_transport{};
    invalid_transport.connection_id = generateUuidV7();
    invalid_transport.database_id = db_->uuid();
    invalid_transport.server_instance_id = generateUuidV7();
    invalid_transport.transport = static_cast<CatalogManager::ConnectionTransport>(99);
    invalid_transport.protocol = "native";
    invalid_transport.auth_method = CatalogManager::ConnectionAuthMethod::PASSWORD;
    EXPECT_EQ(catalog_->upsertRuntimeConnectionCatalogEntry(invalid_transport, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::RuntimeConnectionCatalogInfo invalid_protocol{};
    invalid_protocol.connection_id = generateUuidV7();
    invalid_protocol.database_id = db_->uuid();
    invalid_protocol.server_instance_id = generateUuidV7();
    invalid_protocol.transport = CatalogManager::ConnectionTransport::INET;
    invalid_protocol.protocol = "oracle";
    invalid_protocol.auth_method = CatalogManager::ConnectionAuthMethod::PASSWORD;
    EXPECT_EQ(catalog_->upsertRuntimeConnectionCatalogEntry(invalid_protocol, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::RuntimeConnectionCatalogInfo connection{};
    connection.connection_id = generateUuidV7();
    connection.database_id = db_->uuid();
    connection.server_instance_id = generateUuidV7();
    connection.transport = CatalogManager::ConnectionTransport::INET;
    connection.protocol = "postgresql";
    connection.client_host = "10.1.1.50";
    connection.has_client_port = true;
    connection.client_port = 54000;
    connection.server_host = "127.0.0.1";
    connection.has_server_port = true;
    connection.server_port = 5432;
    connection.is_tls = true;
    connection.tls_profile_name = "sb_tls_default";
    connection.client_os = "linux";
    connection.client_app = "psql";
    connection.client_version = "18.1";
    connection.client_exec_path = "/usr/bin/psql";
    connection.has_client_pid = true;
    connection.client_pid = 12345;
    connection.auth_method = CatalogManager::ConnectionAuthMethod::SCRAM_SHA_256;
    connection.auth_policy_name = "default_remote";
    connection.has_route_fingerprint = true;
    connection.route_fingerprint = 0xAABBCCDDu;
    connection.created_time = 1000;
    connection.last_activity_time = 1010;
    ASSERT_EQ(catalog_->upsertRuntimeConnectionCatalogEntry(connection, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::RuntimeConnectionCatalogInfo loaded{};
    ASSERT_EQ(catalog_->getRuntimeConnectionCatalogEntry(connection.connection_id, loaded, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(loaded.transport, CatalogManager::ConnectionTransport::INET);
    EXPECT_EQ(loaded.protocol, "postgresql");
    EXPECT_EQ(loaded.client_host, "10.1.1.50");
    EXPECT_TRUE(loaded.has_client_port);
    EXPECT_EQ(loaded.client_port, 54000u);
    EXPECT_TRUE(loaded.has_route_fingerprint);
    EXPECT_EQ(loaded.route_fingerprint, 0xAABBCCDDu);

    std::vector<CatalogManager::RuntimeConnectionCatalogInfo> rows;
    ASSERT_EQ(catalog_->listRuntimeConnectionCatalogEntries(db_->uuid(), rows, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(rows.size(), 1u);

    ASSERT_EQ(catalog_->deleteRuntimeConnectionCatalogEntry(connection.connection_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->getRuntimeConnectionCatalogEntry(connection.connection_id, loaded, &ctx),
              Status::NOT_FOUND);
}

TEST_F(CatalogRuntimeContextExtensionContractTest, TransactionContracts)
{
    ErrorContext ctx;

    ID system_user_id = catalog_->getSystemUserId(&ctx);
    ASSERT_NE(system_user_id, ID{}) << ctx.message;

    CatalogManager::SessionInfo session{};
    ASSERT_EQ(catalog_->createSession(system_user_id, ID{}, "native", session, &ctx), Status::OK)
        << ctx.message;
    ASSERT_NE(session.current_schema_id, ID{});

    CatalogManager::RuntimeConnectionCatalogInfo connection{};
    connection.connection_id = generateUuidV7();
    connection.database_id = db_->uuid();
    connection.server_instance_id = generateUuidV7();
    connection.transport = CatalogManager::ConnectionTransport::IPC;
    connection.protocol = "native";
    connection.auth_method = CatalogManager::ConnectionAuthMethod::PASSWORD;
    connection.created_time = 2000;
    connection.last_activity_time = 2001;
    ASSERT_EQ(catalog_->upsertRuntimeConnectionCatalogEntry(connection, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::RuntimeTransactionCatalogInfo invalid_inprogress{};
    invalid_inprogress.txid = 100;
    invalid_inprogress.tx_uuid = generateUuidV7();
    invalid_inprogress.database_id = db_->uuid();
    invalid_inprogress.session_id = session.session_id;
    invalid_inprogress.emulation_engine = CatalogManager::EmulationEngine::NATIVE;
    invalid_inprogress.isolation_level =
        static_cast<uint8_t>(IsolationLevel::READ_COMMITTED);
    invalid_inprogress.state = CatalogManager::RuntimeTransactionState::IN_PROGRESS;
    invalid_inprogress.start_time = 3000;
    invalid_inprogress.has_end_time = true;
    invalid_inprogress.end_time = 3001;
    EXPECT_EQ(catalog_->upsertRuntimeTransactionCatalogEntry(invalid_inprogress, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::RuntimeTransactionCatalogInfo invalid_terminal{};
    invalid_terminal.txid = 101;
    invalid_terminal.tx_uuid = generateUuidV7();
    invalid_terminal.database_id = db_->uuid();
    invalid_terminal.session_id = session.session_id;
    invalid_terminal.emulation_engine = CatalogManager::EmulationEngine::NATIVE;
    invalid_terminal.isolation_level =
        static_cast<uint8_t>(IsolationLevel::SNAPSHOT);
    invalid_terminal.state = CatalogManager::RuntimeTransactionState::COMMITTED;
    invalid_terminal.start_time = 3002;
    invalid_terminal.has_end_time = false;
    EXPECT_EQ(catalog_->upsertRuntimeTransactionCatalogEntry(invalid_terminal, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::RuntimeTransactionCatalogInfo missing_session{};
    missing_session.txid = 102;
    missing_session.tx_uuid = generateUuidV7();
    missing_session.database_id = db_->uuid();
    missing_session.session_id = generateUuidV7();
    missing_session.emulation_engine = CatalogManager::EmulationEngine::NATIVE;
    missing_session.isolation_level =
        static_cast<uint8_t>(IsolationLevel::READ_COMMITTED_READ_CONSISTENCY);
    missing_session.state = CatalogManager::RuntimeTransactionState::IN_PROGRESS;
    missing_session.start_time = 3003;
    EXPECT_EQ(catalog_->upsertRuntimeTransactionCatalogEntry(missing_session, &ctx),
              Status::NOT_FOUND);

    CatalogManager::RuntimeTransactionCatalogInfo missing_connection{};
    missing_connection.txid = 103;
    missing_connection.tx_uuid = generateUuidV7();
    missing_connection.database_id = db_->uuid();
    missing_connection.session_id = session.session_id;
    missing_connection.connection_id = generateUuidV7();
    missing_connection.emulation_engine = CatalogManager::EmulationEngine::NATIVE;
    missing_connection.isolation_level =
        static_cast<uint8_t>(IsolationLevel::READ_COMMITTED);
    missing_connection.state = CatalogManager::RuntimeTransactionState::IN_PROGRESS;
    missing_connection.start_time = 3004;
    EXPECT_EQ(catalog_->upsertRuntimeTransactionCatalogEntry(missing_connection, &ctx),
              Status::NOT_FOUND);

    CatalogManager::RuntimeTransactionCatalogInfo tx{};
    tx.txid = 200;
    tx.tx_uuid = generateUuidV7();
    tx.database_id = db_->uuid();
    tx.session_id = session.session_id;
    tx.connection_id = connection.connection_id;
    tx.user_id = system_user_id;
    tx.emulation_engine = CatalogManager::EmulationEngine::NATIVE;
    tx.isolation_level = static_cast<uint8_t>(IsolationLevel::READ_COMMITTED);
    tx.read_only = false;
    tx.autocommit = true;
    tx.state = CatalogManager::RuntimeTransactionState::IN_PROGRESS;
    tx.start_time = 4000;
    ASSERT_EQ(catalog_->upsertRuntimeTransactionCatalogEntry(tx, &ctx), Status::OK)
        << ctx.message;

    tx.state = CatalogManager::RuntimeTransactionState::COMMITTED;
    tx.has_end_time = true;
    tx.end_time = 4010;
    tx.has_commit_seqno = true;
    tx.commit_seqno = 17;
    tx.schema_epoch_uuid = generateUuidV7();
    tx.forensic_snapshot_capsule_uuid = generateUuidV7();
    tx.has_last_statement_hash = true;
    tx.last_statement_hash = 0x11223344ULL;
    tx.has_last_statement_time = true;
    tx.last_statement_time = 4009;
    tx.has_last_error_code = true;
    tx.last_error_code = 0;
    tx.last_sqlstate = "00000";
    ASSERT_EQ(catalog_->upsertRuntimeTransactionCatalogEntry(tx, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::RuntimeTransactionCatalogInfo loaded{};
    ASSERT_EQ(catalog_->getRuntimeTransactionCatalogEntry(200, loaded, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(loaded.state, CatalogManager::RuntimeTransactionState::COMMITTED);
    EXPECT_EQ(loaded.tx_uuid, tx.tx_uuid);
    EXPECT_TRUE(loaded.has_end_time);
    EXPECT_EQ(loaded.end_time, 4010u);
    EXPECT_TRUE(loaded.has_commit_seqno);
    EXPECT_EQ(loaded.commit_seqno, 17u);
    EXPECT_EQ(loaded.schema_epoch_uuid, tx.schema_epoch_uuid);
    EXPECT_EQ(loaded.forensic_snapshot_capsule_uuid, tx.forensic_snapshot_capsule_uuid);
    EXPECT_EQ(loaded.last_sqlstate, "00000");

    std::vector<CatalogManager::RuntimeTransactionCatalogInfo> tx_rows;
    ASSERT_EQ(catalog_->listRuntimeTransactionCatalogEntries(db_->uuid(), tx_rows, &ctx), Status::OK)
        << ctx.message;
    EXPECT_TRUE(std::any_of(tx_rows.begin(), tx_rows.end(),
                            [](const CatalogManager::RuntimeTransactionCatalogInfo& row) {
                                return row.txid == 200;
                            }));

    ASSERT_EQ(catalog_->deleteRuntimeTransactionCatalogEntry(200, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->getRuntimeTransactionCatalogEntry(200, loaded, &ctx), Status::NOT_FOUND);
}

TEST_F(CatalogRuntimeContextExtensionContractTest,
       TerminalTransactionUpsertHealsInvalidRemoteTxnBindingCatalogPage)
{
    ErrorContext ctx;

    const ID system_user_id = catalog_->getSystemUserId(&ctx);
    ASSERT_NE(system_user_id, ID{}) << ctx.message;

    CatalogManager::SessionInfo session{};
    ASSERT_EQ(catalog_->createSession(system_user_id, ID{}, "native", session, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::RuntimeTransactionCatalogInfo tx{};
    tx.txid = 9100;
    tx.tx_uuid = generateUuidV7();
    tx.database_id = db_->uuid();
    tx.session_id = session.session_id;
    tx.user_id = system_user_id;
    tx.emulation_engine = CatalogManager::EmulationEngine::NATIVE;
    tx.isolation_level = static_cast<uint8_t>(IsolationLevel::READ_COMMITTED);
    tx.state = CatalogManager::RuntimeTransactionState::IN_PROGRESS;
    tx.start_time = 5000;
    ASSERT_EQ(catalog_->upsertRuntimeTransactionCatalogEntry(tx, &ctx), Status::OK)
        << ctx.message;

    catalog_->remote_txn_binding_table_page_ = CatalogManager::CATALOG_ROOT_PAGE;

    tx.state = CatalogManager::RuntimeTransactionState::ABORTED;
    tx.has_end_time = true;
    tx.end_time = 5001;
    ASSERT_EQ(catalog_->upsertRuntimeTransactionCatalogEntry(tx, &ctx), Status::OK)
        << ctx.message;
    EXPECT_NE(catalog_->remote_txn_binding_table_page_, 0u);
    EXPECT_NE(catalog_->remote_txn_binding_table_page_, CatalogManager::CATALOG_ROOT_PAGE);

    CatalogManager::RuntimeTransactionCatalogInfo loaded{};
    ASSERT_EQ(catalog_->getRuntimeTransactionCatalogEntry(tx.txid, loaded, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(loaded.state, CatalogManager::RuntimeTransactionState::ABORTED);
}

TEST_F(CatalogRuntimeContextExtensionContractTest, TransactionLineageContracts)
{
    ErrorContext ctx;

    const ID tx_uuid = generateUuidV7();

    CatalogManager::TransactionLineageEventCatalogInfo invalid_first{};
    invalid_first.tx_uuid = tx_uuid;
    invalid_first.txid = 700;
    invalid_first.event_kind = CatalogManager::TransactionLineageEventKind::TX_CONTEXT_BOUND;
    invalid_first.payload_json = "{\"transaction_outcome\":\"in_progress\"}";
    EXPECT_EQ(catalog_->appendTransactionLineageEventCatalogEntry(invalid_first, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::TransactionLineageEventCatalogInfo begin{};
    begin.tx_uuid = tx_uuid;
    begin.txid = 700;
    begin.event_kind = CatalogManager::TransactionLineageEventKind::TX_BEGIN;
    begin.payload_json = "{\"transaction_outcome\":\"in_progress\"}";
    ASSERT_EQ(catalog_->appendTransactionLineageEventCatalogEntry(begin, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(begin.event_seq, 1u);

    CatalogManager::TransactionLineageEventCatalogInfo context{};
    context.tx_uuid = tx_uuid;
    context.txid = 700;
    context.event_kind = CatalogManager::TransactionLineageEventKind::TX_CONTEXT_BOUND;
    context.connection_id = conn_->attachmentId();
    context.payload_json = "{\"connection_uuid\":\"" + conn_->attachmentId().toString() + "\"}";
    ASSERT_EQ(catalog_->appendTransactionLineageEventCatalogEntry(context, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(context.event_seq, 2u);

    CatalogManager::TransactionLineageEventCatalogInfo commit{};
    commit.tx_uuid = tx_uuid;
    commit.txid = 700;
    commit.event_kind = CatalogManager::TransactionLineageEventKind::TX_COMMIT;
    commit.payload_json = "{\"transaction_outcome\":\"committed\"}";
    ASSERT_EQ(catalog_->appendTransactionLineageEventCatalogEntry(commit, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(commit.event_seq, 3u);

    CatalogManager::TransactionLineageEventCatalogInfo duplicate_terminal{};
    duplicate_terminal.tx_uuid = tx_uuid;
    duplicate_terminal.txid = 700;
    duplicate_terminal.event_kind = CatalogManager::TransactionLineageEventKind::TX_ROLLBACK;
    duplicate_terminal.payload_json = "{\"transaction_outcome\":\"aborted\"}";
    EXPECT_EQ(catalog_->appendTransactionLineageEventCatalogEntry(duplicate_terminal, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::TransactionLineageEventCatalogInfo loaded{};
    ASSERT_EQ(catalog_->getTransactionLineageEventCatalogEntry(commit.lineage_event_id, loaded, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(loaded.event_kind, CatalogManager::TransactionLineageEventKind::TX_COMMIT);
    EXPECT_EQ(loaded.event_seq, 3u);

    std::vector<CatalogManager::TransactionLineageEventCatalogInfo> rows;
    ASSERT_EQ(catalog_->listTransactionLineageEventCatalogEntries(tx_uuid, 700, rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(rows[0].event_kind, CatalogManager::TransactionLineageEventKind::TX_BEGIN);
    EXPECT_EQ(rows[1].event_kind, CatalogManager::TransactionLineageEventKind::TX_CONTEXT_BOUND);
    EXPECT_EQ(rows[2].event_kind, CatalogManager::TransactionLineageEventKind::TX_COMMIT);
}

TEST_F(CatalogRuntimeContextExtensionContractTest, LiveTransactionPersistsRetainedLineage)
{
    ErrorContext ctx;

    const ID system_user_id = catalog_->getSystemUserId(&ctx);
    ASSERT_NE(system_user_id, ID{}) << ctx.message;

    CatalogManager::SessionInfo session{};
    ASSERT_EQ(catalog_->createSession(system_user_id, ID{}, "native", session, &ctx), Status::OK)
        << ctx.message;

    conn_->setSessionContext(session.session_id, ID{}, "native", 0, 0);
    conn_->setCurrentUser(system_user_id, false);

    const uint64_t txid = conn_->getCurrentXid();
    const ID tx_uuid = conn_->getCurrentTransactionUuid();
    ASSERT_NE(txid, 0u);
    ASSERT_NE(tx_uuid, ID{});

    ASSERT_EQ(conn_->commit(&ctx), Status::OK) << ctx.message;

    CatalogManager::RuntimeTransactionCatalogInfo tx_row{};
    ASSERT_EQ(catalog_->getRuntimeTransactionCatalogEntry(txid, tx_row, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(tx_row.state, CatalogManager::RuntimeTransactionState::COMMITTED);
    EXPECT_EQ(tx_row.tx_uuid, tx_uuid);
    EXPECT_EQ(tx_row.session_id, session.session_id);
    EXPECT_EQ(tx_row.user_id, system_user_id);
    EXPECT_TRUE(tx_row.has_end_time);
    EXPECT_GE(tx_row.end_time, tx_row.start_time);
    EXPECT_TRUE(tx_row.has_commit_seqno);
    EXPECT_GT(tx_row.commit_seqno, 0u);
    EXPECT_NE(tx_row.forensic_snapshot_capsule_uuid, ID{});

    std::vector<CatalogManager::TransactionLineageEventCatalogInfo> rows;
    ASSERT_EQ(catalog_->listTransactionLineageEventCatalogEntries(tx_uuid, txid, rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(rows.size(), 3u);

    CatalogManager::ForensicSnapshotCapsuleCatalogInfo capsule{};
    ASSERT_EQ(catalog_->getForensicSnapshotCapsuleCatalogEntry(
                  tx_row.forensic_snapshot_capsule_uuid, capsule, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(capsule.tx_uuid, tx_uuid);
    EXPECT_EQ(capsule.txid, txid);
    EXPECT_EQ(capsule.snapshot_kind, "TRANSACTION_START");
    EXPECT_EQ(capsule.status, "COMMITTED");
    EXPECT_TRUE(capsule.has_commit_seqno);
    EXPECT_EQ(capsule.commit_seqno, tx_row.commit_seqno);
    EXPECT_EQ(capsule.lineage_root_event_id, rows.front().lineage_event_id);
    EXPECT_EQ(rows[0].event_kind, CatalogManager::TransactionLineageEventKind::TX_BEGIN);
    EXPECT_EQ(rows[1].event_kind, CatalogManager::TransactionLineageEventKind::TX_CONTEXT_BOUND);
    EXPECT_EQ(rows[2].event_kind, CatalogManager::TransactionLineageEventKind::TX_COMMIT);
    EXPECT_EQ(rows[1].user_id, system_user_id);
    EXPECT_EQ(rows[1].session_id, session.session_id);
    EXPECT_EQ(rows[1].connection_id, conn_->attachmentId());
    EXPECT_NE(rows[0].payload_json.find(db_->uuid().toString()), std::string::npos);
    EXPECT_NE(rows[2].payload_json.find("committed"), std::string::npos);
}

TEST_F(CatalogRuntimeContextExtensionContractTest, LiveTransactionCommitSurvivesClosedSessionBinding)
{
    ErrorContext ctx;

    const ID system_user_id = catalog_->getSystemUserId(&ctx);
    ASSERT_NE(system_user_id, ID{}) << ctx.message;

    CatalogManager::SessionInfo session{};
    ASSERT_EQ(catalog_->createSession(system_user_id, ID{}, "native", session, &ctx), Status::OK)
        << ctx.message;

    conn_->setSessionContext(session.session_id, ID{}, "native", 0, 0);
    conn_->setCurrentUser(system_user_id, false);

    const uint64_t txid = conn_->getCurrentXid();
    const ID tx_uuid = conn_->getCurrentTransactionUuid();
    ASSERT_NE(txid, 0u);
    ASSERT_NE(tx_uuid, ID{});

    ASSERT_EQ(catalog_->closeSession(session.session_id, &ctx), Status::OK) << ctx.message;

    ErrorContext commit_ctx;
    ASSERT_EQ(conn_->commit(&commit_ctx), Status::OK) << commit_ctx.message;

    CatalogManager::RuntimeTransactionCatalogInfo tx_row{};
    ASSERT_EQ(catalog_->getRuntimeTransactionCatalogEntry(txid, tx_row, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(tx_row.tx_uuid, tx_uuid);
    EXPECT_EQ(tx_row.state, CatalogManager::RuntimeTransactionState::COMMITTED);
    EXPECT_EQ(tx_row.session_id, ID{});
}

TEST_F(CatalogRuntimeContextExtensionContractTest, CloseSessionRejectsCorruptSessionCatalogPage)
{
    ErrorContext ctx;

    const ID system_user_id = catalog_->getSystemUserId(&ctx);
    ASSERT_NE(system_user_id, ID{}) << ctx.message;

    CatalogManager::SessionInfo session{};
    ASSERT_EQ(catalog_->createSession(system_user_id, ID{}, "native", session, &ctx), Status::OK)
        << ctx.message;
    ASSERT_NE(catalog_->sessions_table_page_, 0u);

    BufferPool* bp = db_->buffer_pool();
    ASSERT_NE(bp, nullptr);

    void* page_buffer = nullptr;
    ASSERT_EQ(bp->pinPage(catalog_->sessions_table_page_, &page_buffer, &ctx), Status::OK)
        << ctx.message;

    auto* heap = reinterpret_cast<CatalogHeapPage*>(page_buffer);
    heap->record_count = static_cast<uint32_t>(db_->page_size());

    ASSERT_EQ(bp->unpinPage(catalog_->sessions_table_page_, true, &ctx), Status::OK)
        << ctx.message;

    ErrorContext close_ctx;
    EXPECT_EQ(catalog_->closeSession(session.session_id, &close_ctx), Status::PAGE_CORRUPT);
    EXPECT_NE(std::string(close_ctx.message).find("Catalog heap page"),
              std::string::npos);
}

TEST_F(CatalogRuntimeContextExtensionContractTest, AnonymousConnectionCommitSkipsRetainedTransactionEvidence)
{
    ErrorContext ctx;

    const uint64_t txid = conn_->getCurrentXid();
    const ID tx_uuid = conn_->getCurrentTransactionUuid();
    ASSERT_NE(txid, 0u);
    ASSERT_NE(tx_uuid, ID{});

    ASSERT_EQ(conn_->commit(&ctx), Status::OK) << ctx.message;

    CatalogManager::RuntimeTransactionCatalogInfo tx_row{};
    EXPECT_EQ(catalog_->getRuntimeTransactionCatalogEntry(txid, tx_row, &ctx), Status::NOT_FOUND);

    std::vector<CatalogManager::TransactionLineageEventCatalogInfo> rows;
    ASSERT_EQ(catalog_->listTransactionLineageEventCatalogEntries(tx_uuid, txid, rows, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_TRUE(rows.empty());
}
