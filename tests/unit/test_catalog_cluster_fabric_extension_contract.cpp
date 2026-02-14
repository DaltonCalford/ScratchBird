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

#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

class CatalogClusterFabricExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    ID schema_id_{};
    ID system_user_id_{};
    ID node_id_{};

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_cluster_fabric_extension_contract_" +
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

        ASSERT_EQ(catalog_->createSchema("cat029_schema", "system", schema_id_, &ctx), Status::OK)
            << ctx.message;
        system_user_id_ = catalog_->getSystemUserId(&ctx);
        ASSERT_NE(system_user_id_, ID{});

        CatalogManager::NodeCatalogInfo node{};
        node_id_ = generateUuidV7();
        node.node_id = node_id_;
        node.cluster_id = generateUuidV7();
        node.node_name = "cat029-node-a";
        node.node_role = CatalogManager::ClusterNodeRole::METADATA;
        node.host = "127.0.0.1";
        node.port = 7650;
        node.transport = CatalogManager::ConnectionTransport::INET;
        node.state = CatalogManager::ClusterNodeState::ONLINE;
        ASSERT_EQ(catalog_->upsertNodeCatalogEntry(node, &ctx), Status::OK) << ctx.message;
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

TEST_F(CatalogClusterFabricExtensionContractTest, ClusterFabricCatalogContracts)
{
    ErrorContext ctx;

    CatalogManager::ClusterFabricLinkCatalogInfo link{};
    link.cluster_fabric_link_id = generateUuidV7();
    link.link_name = "cat029-link";
    link.scope_kind = CatalogManager::FabricScopeKind::CLUSTER;
    link.remote_node_id = node_id_;
    link.transport_kind = static_cast<uint8_t>(CatalogManager::ConnectionTransport::INET);
    link.link_state = CatalogManager::FabricLinkState::INIT;
    link.mode_version = 1;
    link.max_sessions = 32;
    link.max_tasks = 256;
    link.heartbeat_interval_ms = 1000;
    link.miss_threshold = 3;
    link.fail_threshold = 5;
    link.created_by_id = system_user_id_;
    ASSERT_EQ(catalog_->upsertClusterFabricLinkCatalogEntry(link, &ctx), Status::OK) << ctx.message;

    CatalogManager::ClusterFabricLinkCatalogInfo dup_link = link;
    dup_link.cluster_fabric_link_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertClusterFabricLinkCatalogEntry(dup_link, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::ClusterFabricLinkCatalogInfo stale_update = link;
    stale_update.link_state = CatalogManager::FabricLinkState::READY;
    stale_update.mode_version = 1;
    EXPECT_EQ(catalog_->upsertClusterFabricLinkCatalogEntry(stale_update, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::ClusterFabricLinkCatalogInfo link_update = link;
    link_update.link_state = CatalogManager::FabricLinkState::READY;
    link_update.mode_version = 2;
    link_update.has_last_ready_time = true;
    link_update.last_ready_time = 1000;
    ASSERT_EQ(catalog_->upsertClusterFabricLinkCatalogEntry(link_update, &ctx), Status::OK) << ctx.message;

    CatalogManager::ClusterFabricSessionCatalogInfo session{};
    session.cluster_fabric_session_id = generateUuidV7();
    session.cluster_fabric_link_id = link.cluster_fabric_link_id;
    session.session_id = generateUuidV7();
    session.effective_user_id = system_user_id_;
    session.effective_schema_id = schema_id_;
    session.session_state = CatalogManager::FabricSessionState::ACTIVE;
    session.opened_time = 2000;
    ASSERT_EQ(catalog_->upsertClusterFabricSessionCatalogEntry(session, &ctx), Status::OK) << ctx.message;

    CatalogManager::ClusterFabricSessionCatalogInfo invalid_session = session;
    invalid_session.cluster_fabric_session_id = generateUuidV7();
    invalid_session.session_state = CatalogManager::FabricSessionState::CLOSED;
    invalid_session.has_closed_time = false;
    EXPECT_EQ(catalog_->upsertClusterFabricSessionCatalogEntry(invalid_session, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::ClusterFabricErrorCatalogInfo fabric_error{};
    fabric_error.cluster_fabric_error_id = generateUuidV7();
    fabric_error.cluster_fabric_link_id = link.cluster_fabric_link_id;
    fabric_error.error_class = CatalogManager::FabricErrorClass::TASK;
    fabric_error.source_component = "fabric-worker";
    fabric_error.source_code = "E_CHUNK_TIMEOUT";
    fabric_error.message_text = "timeout on chunk ack";
    fabric_error.recoverable = true;
    fabric_error.first_seen_time = 2100;
    fabric_error.last_seen_time = 2100;
    fabric_error.occurrence_count = 1;
    fabric_error.is_open = true;
    ASSERT_EQ(catalog_->upsertClusterFabricErrorCatalogEntry(fabric_error, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ClusterFabricTxnCatalogInfo txn{};
    txn.cluster_fabric_txn_id = generateUuidV7();
    txn.cluster_fabric_session_id = session.cluster_fabric_session_id;
    txn.transaction_id = generateUuidV7();
    txn.txn_state = CatalogManager::FabricTxnState::ACTIVE;
    txn.begin_time = 2200;
    ASSERT_EQ(catalog_->upsertClusterFabricTxnCatalogEntry(txn, &ctx), Status::OK) << ctx.message;

    CatalogManager::ClusterFabricTaskCatalogInfo invalid_task{};
    invalid_task.cluster_fabric_task_id = generateUuidV7();
    invalid_task.cluster_fabric_link_id = link.cluster_fabric_link_id;
    invalid_task.task_kind = CatalogManager::FabricTaskKind::PASSTHROUGH_SBLR_EXECUTE;
    invalid_task.task_state = CatalogManager::FabricTaskState::QUEUED;
    invalid_task.submitted_time = 2300;
    EXPECT_EQ(catalog_->upsertClusterFabricTaskCatalogEntry(invalid_task, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::ClusterFabricTaskCatalogInfo task{};
    task.cluster_fabric_task_id = generateUuidV7();
    task.cluster_fabric_link_id = link.cluster_fabric_link_id;
    task.has_cluster_fabric_session_id = true;
    task.cluster_fabric_session_id = session.cluster_fabric_session_id;
    task.has_cluster_fabric_txn_id = true;
    task.cluster_fabric_txn_id = txn.cluster_fabric_txn_id;
    task.task_kind = CatalogManager::FabricTaskKind::PASSTHROUGH_SBLR_EXECUTE;
    task.task_state = CatalogManager::FabricTaskState::RUNNING;
    task.priority = 7;
    task.has_sblr_artifact_id = true;
    task.sblr_artifact_id = generateUuidV7();
    task.has_source_object_id = true;
    task.source_object_id = db_->uuid();
    task.has_target_object_id = true;
    task.target_object_id = db_->uuid();
    task.has_last_error_id = true;
    task.last_error_id = fabric_error.cluster_fabric_error_id;
    task.submitted_time = 2300;
    task.has_started_time = true;
    task.started_time = 2301;
    ASSERT_EQ(catalog_->upsertClusterFabricTaskCatalogEntry(task, &ctx), Status::OK) << ctx.message;

    CatalogManager::ClusterFabricTaskChunkCatalogInfo invalid_chunk{};
    invalid_chunk.cluster_fabric_task_chunk_id = generateUuidV7();
    invalid_chunk.cluster_fabric_task_id = task.cluster_fabric_task_id;
    invalid_chunk.chunk_seq = 0;
    invalid_chunk.chunk_total = 2;
    invalid_chunk.chunk_bytes = 1024;
    invalid_chunk.chunk_checksum = 1234;
    invalid_chunk.is_final_chunk = true;
    invalid_chunk.sent_time = 2400;
    EXPECT_EQ(catalog_->upsertClusterFabricTaskChunkCatalogEntry(invalid_chunk, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::ClusterFabricTaskChunkCatalogInfo chunk{};
    chunk.cluster_fabric_task_chunk_id = generateUuidV7();
    chunk.cluster_fabric_task_id = task.cluster_fabric_task_id;
    chunk.chunk_seq = 1;
    chunk.chunk_total = 2;
    chunk.chunk_bytes = 1024;
    chunk.chunk_checksum = 5678;
    chunk.is_final_chunk = true;
    chunk.sent_time = 2400;
    chunk.has_acked_time = true;
    chunk.acked_time = 2401;
    ASSERT_EQ(catalog_->upsertClusterFabricTaskChunkCatalogEntry(chunk, &ctx), Status::OK) << ctx.message;

    CatalogManager::ClusterFabricEventCatalogInfo event{};
    event.cluster_fabric_event_id = generateUuidV7();
    event.cluster_fabric_link_id = link.cluster_fabric_link_id;
    event.has_cluster_fabric_session_id = true;
    event.cluster_fabric_session_id = session.cluster_fabric_session_id;
    event.has_cluster_fabric_task_id = true;
    event.cluster_fabric_task_id = task.cluster_fabric_task_id;
    event.event_kind = "task_started";
    event.event_time = 2500;
    event.has_event_payload = true;
    event.event_payload = "{\"kind\":\"passthrough\"}";
    event.has_actor_id = true;
    event.actor_id = system_user_id_;
    ASSERT_EQ(catalog_->upsertClusterFabricEventCatalogEntry(event, &ctx), Status::OK) << ctx.message;

    CatalogManager::ClusterFabricTaskCatalogInfo task_terminal = task;
    task_terminal.task_state = CatalogManager::FabricTaskState::SUCCESS;
    task_terminal.has_finished_time = true;
    task_terminal.finished_time = 2600;
    ASSERT_EQ(catalog_->upsertClusterFabricTaskCatalogEntry(task_terminal, &ctx), Status::OK) << ctx.message;

    CatalogManager::ClusterFabricLinkCatalogInfo link_out{};
    ASSERT_EQ(catalog_->getClusterFabricLinkCatalogEntry(link.cluster_fabric_link_id, link_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(link_out.mode_version, 2u);

    CatalogManager::ClusterFabricSessionCatalogInfo session_out{};
    ASSERT_EQ(
        catalog_->getClusterFabricSessionCatalogEntry(session.cluster_fabric_session_id, session_out, &ctx),
        Status::OK) << ctx.message;
    EXPECT_EQ(session_out.session_state, CatalogManager::FabricSessionState::ACTIVE);

    CatalogManager::ClusterFabricTxnCatalogInfo txn_out{};
    ASSERT_EQ(catalog_->getClusterFabricTxnCatalogEntry(txn.cluster_fabric_txn_id, txn_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(txn_out.txn_state, CatalogManager::FabricTxnState::ACTIVE);

    CatalogManager::ClusterFabricTaskCatalogInfo task_out{};
    ASSERT_EQ(catalog_->getClusterFabricTaskCatalogEntry(task.cluster_fabric_task_id, task_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(task_out.task_state, CatalogManager::FabricTaskState::SUCCESS);

    CatalogManager::ClusterFabricTaskChunkCatalogInfo chunk_out{};
    ASSERT_EQ(
        catalog_->getClusterFabricTaskChunkCatalogEntry(chunk.cluster_fabric_task_chunk_id, chunk_out, &ctx),
        Status::OK) << ctx.message;
    EXPECT_TRUE(chunk_out.is_final_chunk);

    CatalogManager::ClusterFabricEventCatalogInfo event_out{};
    ASSERT_EQ(catalog_->getClusterFabricEventCatalogEntry(event.cluster_fabric_event_id, event_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(event_out.event_kind, "task_started");

    CatalogManager::ClusterFabricErrorCatalogInfo error_out{};
    ASSERT_EQ(
        catalog_->getClusterFabricErrorCatalogEntry(fabric_error.cluster_fabric_error_id, error_out, &ctx),
        Status::OK) << ctx.message;
    EXPECT_EQ(error_out.error_class, CatalogManager::FabricErrorClass::TASK);

    std::vector<CatalogManager::ClusterFabricTaskCatalogInfo> tasks;
    ASSERT_EQ(catalog_->listClusterFabricTaskCatalogEntries(link.cluster_fabric_link_id, tasks, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(tasks.size(), 1u);

    ASSERT_EQ(catalog_->deleteClusterFabricEventCatalogEntry(event.cluster_fabric_event_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteClusterFabricTaskChunkCatalogEntry(chunk.cluster_fabric_task_chunk_id, &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteClusterFabricTaskCatalogEntry(task.cluster_fabric_task_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteClusterFabricTxnCatalogEntry(txn.cluster_fabric_txn_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteClusterFabricSessionCatalogEntry(session.cluster_fabric_session_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteClusterFabricErrorCatalogEntry(fabric_error.cluster_fabric_error_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteClusterFabricLinkCatalogEntry(link.cluster_fabric_link_id, &ctx), Status::OK)
        << ctx.message;
}
