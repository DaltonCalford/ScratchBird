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

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/mga_failpoint_manager.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;
using namespace scratchbird::catalog;

class CatalogVirtualOverlayConformanceContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    CatalogManager::SessionInfo session_{};
    ID schema_id_{};
    ID system_user_id_{};

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_virtual_overlay_contract_" + std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());

        ASSERT_EQ(catalog_->createSchema("cat034_schema", "system", schema_id_, &ctx), Status::OK)
            << ctx.message;

        system_user_id_ = catalog_->getSystemUserId(&ctx);
        ASSERT_NE(system_user_id_, ID{});

        ASSERT_EQ(catalog_->createSession(system_user_id_, ID{}, "mysql", session_, &ctx), Status::OK)
            << ctx.message;
        conn_->setSessionContext(session_.session_id, session_.authkey_id, session_.emulation_mode,
                                 session_.policy_epoch_global, session_.policy_epoch_table);
        conn_->beginStatementTracking("SELECT 1");
    }

    void TearDown() override
    {
        if (conn_) {
            conn_->endStatementTrackingSuccess(0);
        }
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_) {
            db_->close();
            db_.reset();
            catalog_ = nullptr;
        }
        std::remove(db_path_.c_str());
    }

    ID createSimpleTable(const std::string& table_name)
    {
        CatalogManager::ColumnInfo col{};
        col.column_name = "id";
        col.data_type = static_cast<uint16_t>(DataType::INT64);
        col.nullable = false;
        std::vector<CatalogManager::ColumnInfo> cols{col};
        ID table_id{};
        ErrorContext ctx;
        EXPECT_EQ(catalog_->createTable(schema_id_, table_name, cols, table_id, 0, &ctx), Status::OK)
            << ctx.message;
        return table_id;
    }
};

TEST_F(CatalogVirtualOverlayConformanceContractTest, VirtualOverlayConformance)
{
    ErrorContext ctx;

    ID fdw_server_id{};
    ASSERT_EQ(catalog_->createForeignServer("cat034_remote_server", "postgresql", "127.0.0.1", 5432, "{}",
                                            fdw_server_id, &ctx),
              Status::OK)
        << ctx.message;

    ID user_mapping_id{};
    ASSERT_EQ(catalog_->createUserMapping(system_user_id_, fdw_server_id, "sb_user", "sb_secret",
                                          user_mapping_id, &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::RemoteConnectorCatalogInfo connector{};
    connector.remote_connector_id = generateUuidV7();
    connector.fdw_server_id = fdw_server_id;
    connector.fdw_id = generateUuidV7();
    connector.connector_name = "cat034_connector";
    connector.engine_name = "postgresql";
    connector.endpoint_uri = "tcp://127.0.0.1:5432";
    connector.has_default_mapping_id = true;
    connector.default_mapping_id = user_mapping_id;
    connector.has_engine_version_text = true;
    connector.engine_version_text = "18.0";
    connector.state = CatalogManager::RemoteConnectorState::READY;
    connector.failure_count = 1;
    connector.has_last_probe_time = true;
    connector.last_probe_time = 1000;
    connector.has_last_ready_time = true;
    connector.last_ready_time = 1001;
    connector.module_checksum = 42;
    ASSERT_EQ(catalog_->upsertRemoteConnectorCatalogEntry(connector, &ctx), Status::OK) << ctx.message;

    CatalogManager::RemoteErrorCatalogInfo remote_error{};
    remote_error.remote_error_id = generateUuidV7();
    remote_error.remote_connector_id = connector.remote_connector_id;
    remote_error.error_class = CatalogManager::RemoteErrorClass::EXECUTION;
    remote_error.mapped_code = "SB_REMOTE_EXEC";
    remote_error.message_text = "relation not found";
    remote_error.first_seen_time = 1010;
    remote_error.last_seen_time = 1010;
    remote_error.occurrence_count = 1;
    remote_error.is_open = true;
    ASSERT_EQ(catalog_->upsertRemoteErrorCatalogEntry(remote_error, &ctx), Status::OK) << ctx.message;

    CatalogManager::RemoteExecutionAuditCatalogInfo audit{};
    audit.remote_exec_audit_id = generateUuidV7();
    audit.remote_connector_id = connector.remote_connector_id;
    audit.session_id = session_.session_id;
    audit.request_id = generateUuidV7();
    audit.operation_class = CatalogManager::RemoteOperationClass::QUERY;
    audit.statement_fingerprint = 0x10u;
    audit.exec_status = CatalogManager::RemoteExecStatus::FAILED;
    audit.rows_returned = 0;
    audit.rows_affected = 0;
    audit.bytes_in = 64;
    audit.bytes_out = 32;
    audit.latency_ms = 5;
    audit.started_time = 1030;
    audit.finished_time = 1031;
    audit.has_error_id = true;
    audit.error_id = remote_error.remote_error_id;
    ASSERT_EQ(catalog_->upsertRemoteExecutionAuditCatalogEntry(audit, &ctx), Status::OK) << ctx.message;

    CatalogManager::RemotePreparedStatementCatalogInfo prepared{};
    prepared.remote_prepared_id = generateUuidV7();
    prepared.remote_connector_id = connector.remote_connector_id;
    prepared.session_id = session_.session_id;
    prepared.statement_name = "orders_by_id";
    prepared.statement_fingerprint = 0x11u;
    prepared.command_text = "select * from orders where id = $1";
    prepared.remote_handle = "prep_001";
    prepared.created_time = 1040;
    prepared.last_used_time = 1041;
    ASSERT_EQ(catalog_->upsertRemotePreparedStatementCatalogEntry(prepared, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::PublicationCatalogInfo publication{};
    publication.publication_id = generateUuidV7();
    publication.publication_name = "cat034_publication";
    publication.owner_id = system_user_id_;
    publication.publish_insert = true;
    publication.publish_update = true;
    publication.publish_delete = true;
    publication.publish_truncate = true;
    ASSERT_EQ(catalog_->upsertPublicationCatalogEntry(publication, &ctx), Status::OK) << ctx.message;

    CatalogManager::SubscriptionCatalogInfo subscription{};
    subscription.subscription_id = generateUuidV7();
    subscription.subscription_name = "cat034_subscription";
    subscription.owner_id = system_user_id_;
    subscription.enabled = true;
    subscription.sync_commit = true;
    subscription.copy_data = true;
    subscription.create_slot = true;
    subscription.refresh_on_start = true;
    ASSERT_EQ(catalog_->upsertSubscriptionCatalogEntry(subscription, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationChannelCatalogInfo channel{};
    channel.replication_channel_id = generateUuidV7();
    channel.channel_name = "cat034_channel";
    channel.direction = CatalogManager::ReplicationDirection::ONE_WAY;
    channel.channel_state = CatalogManager::ReplicationChannelState::STREAMING;
    channel.mode_version = 7;
    channel.has_publication_id = true;
    channel.publication_id = publication.publication_id;
    channel.has_subscription_id = true;
    channel.subscription_id = subscription.subscription_id;
    channel.created_by_id = system_user_id_;
    ASSERT_EQ(catalog_->upsertReplicationChannelCatalogEntry(channel, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationOriginCatalogInfo origin{};
    origin.origin_id = generateUuidV7();
    origin.origin_name = "origin_cat034";
    origin.origin_scope = "cluster";
    origin.origin_priority = 1;
    ASSERT_EQ(catalog_->upsertReplicationOriginCatalogEntry(origin, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationChannelMemberCatalogInfo member{};
    member.channel_member_id = generateUuidV7();
    member.replication_channel_id = channel.replication_channel_id;
    member.member_name = "publisher";
    member.member_role = CatalogManager::ReplicationMemberRole::PUBLISHER;
    member.local_endpoint = true;
    member.priority_rank = 1;
    member.origin_id = origin.origin_id;
    ASSERT_EQ(catalog_->upsertReplicationChannelMemberCatalogEntry(member, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ReplicationErrorCatalogInfo repl_error{};
    repl_error.replication_error_id = generateUuidV7();
    repl_error.replication_channel_id = channel.replication_channel_id;
    repl_error.source_component = "apply_worker";
    repl_error.source_code = "E_DUP";
    repl_error.message_text = "duplicate key";
    repl_error.recoverable = true;
    repl_error.first_seen_time = 2000;
    repl_error.last_seen_time = 2000;
    repl_error.occurrence_count = 1;
    repl_error.is_open = true;
    ASSERT_EQ(catalog_->upsertReplicationErrorCatalogEntry(repl_error, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationCursorCatalogInfo cursor{};
    cursor.replication_cursor_id = generateUuidV7();
    cursor.replication_channel_id = channel.replication_channel_id;
    cursor.channel_member_id = member.channel_member_id;
    cursor.cursor_name = "cursor_cat034";
    cursor.cursor_state = CatalogManager::ReplicationCursorState::ACTIVE;
    cursor.cursor_payload = "{\"lsn\":\"0/1\"}";
    cursor.source_commit_seq = 10;
    cursor.applied_commit_seq = 9;
    cursor.lag_ms = 33;
    cursor.has_applied_time = true;
    cursor.applied_time = 2010;
    cursor.has_heartbeat_time = true;
    cursor.heartbeat_time = 2011;
    cursor.has_last_error_id = true;
    cursor.last_error_id = repl_error.replication_error_id;
    ASSERT_EQ(catalog_->upsertReplicationCursorCatalogEntry(cursor, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationTxnBatchCatalogInfo batch{};
    batch.replication_batch_id = generateUuidV7();
    batch.replication_channel_id = channel.replication_channel_id;
    batch.source_member_id = member.channel_member_id;
    batch.origin_id = origin.origin_id;
    batch.source_txn_id = "tx-1";
    batch.source_commit_seq = 10;
    batch.source_commit_time = 2010;
    batch.txn_state = CatalogManager::ReplicationTxnState::CONFLICT;
    batch.change_count = 1;
    batch.payload_bytes = 128;
    batch.batch_checksum = 0xDEADBEEFu;
    batch.received_time = 2012;
    ASSERT_EQ(catalog_->upsertReplicationTxnBatchCatalogEntry(batch, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationConflictCatalogInfo conflict{};
    conflict.replication_conflict_id = generateUuidV7();
    conflict.replication_channel_id = channel.replication_channel_id;
    conflict.replication_batch_id = batch.replication_batch_id;
    conflict.conflict_kind = CatalogManager::ReplicationConflictKind::UPDATE_UPDATE;
    conflict.source_origin_id = origin.origin_id;
    conflict.target_object_id = db_->uuid();
    conflict.source_commit_seq = 10;
    conflict.source_payload = "{\"before\":1,\"after\":2}";
    conflict.resolution_state = CatalogManager::ReplicationResolutionState::OPEN;
    ASSERT_EQ(catalog_->upsertReplicationConflictCatalogEntry(conflict, &ctx), Status::OK) << ctx.message;

    ID table_id = createSimpleTable("orders_local");

    ID cluster_id = generateUuidV7();
    CatalogManager::ClusterCatalogInfo cluster{};
    cluster.cluster_id = cluster_id;
    cluster.cluster_name = "alpha";
    cluster.cluster_mode = CatalogManager::ClusterMode::CLUSTER;
    cluster.cluster_state = CatalogManager::ClusterState::ONLINE;
    cluster.consensus_mode = CatalogManager::ConsensusMode::RAFT;
    cluster.config_version = 1;
    cluster.cluster_state_version = 1;
    ASSERT_EQ(catalog_->upsertClusterCatalogEntry(cluster, &ctx), Status::OK) << ctx.message;

    CatalogManager::ShardPolicyCatalogInfo policy{};
    policy.policy_id = generateUuidV7();
    policy.policy_name = "policy_cat034";
    policy.replication_factor = 2;
    policy.consistency_read = CatalogManager::ConsistencyLevel::QUORUM;
    policy.consistency_write = CatalogManager::ConsistencyLevel::QUORUM;
    policy.failover_mode = CatalogManager::FailoverMode::AUTO;
    policy.rebalance_mode = CatalogManager::RebalanceMode::MANUAL;
    policy.shard_key_required = true;
    policy.default_shard_count = 2;
    policy.shard_size_target_mb = 128;
    policy.shard_growth_trigger_pct = 80;
    policy.rebalance_interval_ms = 10000;
    ASSERT_EQ(catalog_->upsertShardPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::ShardKeyCatalogInfo shard_key{};
    shard_key.shard_key_id = generateUuidV7();
    shard_key.table_id = table_id;
    shard_key.shard_key_kind = CatalogManager::ShardKeyKind::HASH;
    shard_key.key_columns_id = generateUuidV7();
    shard_key.hash_function = CatalogManager::HashFunctionKind::MURMUR3;
    shard_key.key_version = 1;
    shard_key.is_active = true;
    ASSERT_EQ(catalog_->upsertShardKeyCatalogEntry(shard_key, &ctx), Status::OK) << ctx.message;

    CatalogManager::ShardCatalogInfo shard{};
    shard.shard_id = generateUuidV7();
    shard.shard_name = "shard_0";
    shard.cluster_id = cluster_id;
    shard.shard_state = CatalogManager::ShardState::ONLINE;
    shard.shard_kind = CatalogManager::ShardKind::ROW;
    shard.policy_id = policy.policy_id;
    ASSERT_EQ(catalog_->upsertShardCatalogEntry(shard, &ctx), Status::OK) << ctx.message;

    CatalogManager::NodeCatalogInfo node_a{};
    node_a.node_id = generateUuidV7();
    node_a.cluster_id = cluster_id;
    node_a.node_name = "node-a";
    node_a.node_role = CatalogManager::ClusterNodeRole::METADATA;
    node_a.host = "127.0.0.1";
    node_a.port = 7101;
    node_a.transport = CatalogManager::ConnectionTransport::INET;
    node_a.state = CatalogManager::ClusterNodeState::ONLINE;
    ASSERT_EQ(catalog_->upsertNodeCatalogEntry(node_a, &ctx), Status::OK) << ctx.message;

    CatalogManager::NodeCatalogInfo node_b = node_a;
    node_b.node_id = generateUuidV7();
    node_b.node_name = "node-b";
    node_b.port = 7102;
    ASSERT_EQ(catalog_->upsertNodeCatalogEntry(node_b, &ctx), Status::OK) << ctx.message;

    CatalogManager::ShardReplicaCatalogInfo replica{};
    replica.replica_id = generateUuidV7();
    replica.shard_id = shard.shard_id;
    replica.node_id = node_a.node_id;
    replica.replica_role = CatalogManager::ReplicaRole::PRIMARY;
    replica.replica_state = CatalogManager::ReplicaState::ONLINE;
    replica.is_voting = true;
    replica.weight = 100;
    ASSERT_EQ(catalog_->upsertShardReplicaCatalogEntry(replica, &ctx), Status::OK) << ctx.message;

    CatalogManager::ShardMigrationCatalogInfo migration{};
    migration.migration_id = generateUuidV7();
    migration.shard_id = shard.shard_id;
    migration.source_node_id = node_a.node_id;
    migration.target_node_id = node_b.node_id;
    migration.state = CatalogManager::ShardMigrationState::RUNNING;
    migration.bytes_total = 1000;
    migration.bytes_copied = 250;
    migration.rows_total = 100;
    migration.rows_copied = 25;
    migration.throttle_state = CatalogManager::ThrottleState::LOW;
    migration.started_time = 3000;
    migration.updated_time = 3010;
    ASSERT_EQ(catalog_->upsertShardMigrationCatalogEntry(migration, &ctx), Status::OK) << ctx.message;

    VirtualResultSet result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::SCRATCHBIRD, "sys", "migration_status", "", result, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(result.empty());

    result = {};
    ASSERT_EQ(executeVirtualQuery(ProtocolType::SCRATCHBIRD, "sys", "migration_audit_summary", "", result, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(result.empty());

    result = {};
    ASSERT_EQ(executeVirtualQuery(ProtocolType::SCRATCHBIRD, "sys", "replication_channel_status", "", result,
                                  &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(result.empty());

    result = {};
    ASSERT_EQ(executeVirtualQuery(ProtocolType::SCRATCHBIRD, "sys", "replication_conflict_queue", "", result,
                                  &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(result.empty());

    result = {};
    ASSERT_EQ(executeVirtualQuery(ProtocolType::SCRATCHBIRD, "sys", "replication_cursor_status", "", result,
                                  &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(result.empty());

    result = {};
    ASSERT_EQ(executeVirtualQuery(ProtocolType::SCRATCHBIRD, "sys", "shard_status", "", result, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(result.empty());

    result = {};
    ASSERT_EQ(executeVirtualQuery(ProtocolType::SCRATCHBIRD, "sys", "shard_migrations", "", result, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(result.empty());

    result = {};
    ASSERT_EQ(executeVirtualQuery(ProtocolType::SCRATCHBIRD, "sys", "prepared_statement", "", result, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(result.empty());

    VirtualResultSet pg_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::POSTGRESQL, "pg_catalog", "pg_stat_activity", "", pg_result, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(pg_result.empty());

    VirtualResultSet mysql_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema", "processlist", "", mysql_result,
                                  &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(mysql_result.empty());

    VirtualResultSet firebird_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::FIREBIRD, "rdb", "RDB$DATABASE", "", firebird_result, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(firebird_result.empty());

    VirtualResultSet cassandra_local;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::CASSANDRA, "system", "local", "", cassandra_local, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(cassandra_local.empty());

    VirtualResultSet cassandra_tables;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::CASSANDRA, "system_schema", "tables", "", cassandra_tables, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(cassandra_tables.empty());
}

TEST_F(CatalogVirtualOverlayConformanceContractTest, SysMgaObservabilityViewsAreQueryable)
{
    ErrorContext ctx;
    const uint64_t now_us = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    ID table_id = createSimpleTable("mga_view_table");

    CatalogManager::RuntimeTransactionCatalogInfo tx{};
    tx.txid = 77;
    tx.tx_uuid = generateUuidV7();
    tx.database_id = db_->uuid();
    tx.session_id = session_.session_id;
    tx.user_id = system_user_id_;
    tx.isolation_level = static_cast<uint8_t>(IsolationLevel::SNAPSHOT);
    tx.state = CatalogManager::RuntimeTransactionState::IN_PROGRESS;
    tx.start_time = now_us - 8'000'000;
    tx.created_time = now_us - 8'000'000;
    tx.last_modified_time = now_us - 1'000'000;
    ASSERT_EQ(catalog_->upsertRuntimeTransactionCatalogEntry(tx, &ctx), Status::OK) << ctx.message;

    CatalogManager::TransactionHistoryEntry history{};
    history.thread_id = 9;
    history.event_id = 300;
    history.end_event_id = 301;
    history.trx_id = 66;
    history.start_oit = 60;
    history.end_oit = 61;
    history.start_oat = 66;
    history.end_oat = 67;
    history.start_ost = 66;
    history.end_ost = 67;
    history.timer_start = now_us - 12'000'000;
    history.timer_end = now_us - 11'000'000;
    history.timer_wait = 1'000'000;
    history.committed = true;
    ASSERT_EQ(catalog_->recordTransactionHistory(history, &ctx), Status::OK) << ctx.message;

    CatalogManager::WaitHistoryEntry wait{};
    wait.thread_id = 9;
    wait.blocker_thread_id = 10;
    wait.event_id = 400;
    wait.timer_start = now_us - 3'000'000;
    wait.timer_end = now_us - 2'000'000;
    wait.timer_wait = 1'000'000;
    wait.has_blocker_txid = true;
    wait.blocker_txid = 77;
    wait.has_victim_txid = true;
    wait.victim_txid = 66;
    wait.requested_mode = static_cast<uint8_t>(LockMode::LOCK_EXCLUSIVE);
    wait.blocker_mode = static_cast<uint8_t>(LockMode::LOCK_SHARE);
    wait.outcome_code = "WAIT_GRANTED";
    wait.blocker_identity = session_.session_id.toString();
    wait.victim_identity = "victim-session";
    ASSERT_EQ(catalog_->recordWaitHistory(wait, &ctx), Status::OK) << ctx.message;

    std::vector<MgaFailpointDefinition> failpoints{
        {std::string(MgaFailpointTriggers::kAfterTipLoadBeforeActiveNormalization),
         MgaFailpointAction::MARK_ONLY,
         1,
         Status::OK,
         0,
         "startup_probe"}};
    ASSERT_EQ(db_->mga_failpoint_manager()->installSeed("overlay-seed", failpoints, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(db_->mga_failpoint_manager()->trip(
                  MgaFailpointTriggers::kAfterTipLoadBeforeActiveNormalization,
                  {},
                  &ctx),
              Status::OK)
        << ctx.message;

    StorageEngine::FragmentationAdvisory advisory{};
    advisory.page_id = 12;
    advisory.reclaimable_bytes = 256;
    advisory.rewrite_recommended = true;
    db_->storage_engine()->publishFragmentationAdvisory(table_id, advisory.page_id, advisory);

    VirtualResultSet result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::SCRATCHBIRD, "sys", "sb_mga_runtime_metrics", "", result, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(result.empty());

    result = {};
    ASSERT_EQ(executeVirtualQuery(ProtocolType::SCRATCHBIRD, "sys", "sb_mga_active_transactions", "", result, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(result.empty());

    result = {};
    ASSERT_EQ(executeVirtualQuery(ProtocolType::SCRATCHBIRD, "sys", "sb_mga_cleanup_debt", "", result, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(result.empty());

    result = {};
    ASSERT_EQ(executeVirtualQuery(ProtocolType::SCRATCHBIRD, "sys", "sb_mga_failpoint_events", "", result, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(result.empty());

    result = {};
    ASSERT_EQ(executeVirtualQuery(ProtocolType::SCRATCHBIRD, "sys", "sb_mga_snapshot_blockers", "", result, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(result.empty());

    result = {};
    ASSERT_EQ(executeVirtualQuery(ProtocolType::SCRATCHBIRD, "sys", "sb_mga_transaction_history", "", result, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(result.empty());

    result = {};
    ASSERT_EQ(executeVirtualQuery(ProtocolType::SCRATCHBIRD, "sys", "sb_mga_wait_history", "", result, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(result.empty());
}
