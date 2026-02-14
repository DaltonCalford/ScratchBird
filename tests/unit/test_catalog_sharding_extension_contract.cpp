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
#include "scratchbird/core/types.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

class CatalogShardingExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    ID schema_id_{};

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_sharding_extension_contract_" + std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());

        ASSERT_EQ(catalog_->createSchema("cat022_schema", "system", schema_id_, &ctx), Status::OK)
            << ctx.message;
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

    ID createSimpleTable(const std::string& name)
    {
        CatalogManager::ColumnInfo col{};
        col.column_name = "id";
        col.data_type = static_cast<uint16_t>(DataType::INT64);
        col.nullable = false;

        std::vector<CatalogManager::ColumnInfo> columns{col};
        ID table_id{};
        ErrorContext ctx;
        EXPECT_EQ(catalog_->createTable(schema_id_, name, columns, table_id, 0, &ctx), Status::OK)
            << ctx.message;
        return table_id;
    }
};

TEST_F(CatalogShardingExtensionContractTest, ShardingCatalogContracts)
{
    ErrorContext ctx;

    ID table_id = createSimpleTable("orders");
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

    CatalogManager::ClusterCatalogInfo duplicate_cluster = cluster;
    duplicate_cluster.cluster_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertClusterCatalogEntry(duplicate_cluster, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::ShardPolicyCatalogInfo policy{};
    policy.policy_id = generateUuidV7();
    policy.policy_name = "policy_a";
    policy.replication_factor = 3;
    policy.consistency_read = CatalogManager::ConsistencyLevel::QUORUM;
    policy.consistency_write = CatalogManager::ConsistencyLevel::QUORUM;
    policy.failover_mode = CatalogManager::FailoverMode::AUTO;
    policy.rebalance_mode = CatalogManager::RebalanceMode::MANUAL;
    policy.shard_key_required = true;
    policy.default_shard_count = 8;
    policy.shard_size_target_mb = 1024;
    policy.shard_growth_trigger_pct = 80;
    policy.rebalance_interval_ms = 300000;
    ASSERT_EQ(catalog_->upsertShardPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::ShardPolicyParamCatalogInfo invalid_param{};
    invalid_param.policy_param_id = generateUuidV7();
    invalid_param.policy_id = policy.policy_id;
    invalid_param.param_key = "threshold";
    invalid_param.param_type = CatalogManager::ShardPolicyParamType::U64;
    invalid_param.has_val_u64 = true;
    invalid_param.val_u64 = 10;
    invalid_param.has_val_bool = true;
    invalid_param.val_bool = true;
    EXPECT_EQ(catalog_->upsertShardPolicyParamCatalogEntry(invalid_param, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::ShardPolicyParamCatalogInfo param{};
    param.policy_param_id = generateUuidV7();
    param.policy_id = policy.policy_id;
    param.param_key = "threshold";
    param.param_type = CatalogManager::ShardPolicyParamType::U64;
    param.has_val_u64 = true;
    param.val_u64 = 10;
    ASSERT_EQ(catalog_->upsertShardPolicyParamCatalogEntry(param, &ctx), Status::OK) << ctx.message;

    CatalogManager::ShardKeyCatalogInfo shard_key{};
    shard_key.shard_key_id = generateUuidV7();
    shard_key.table_id = table_id;
    shard_key.shard_key_kind = CatalogManager::ShardKeyKind::HASH;
    shard_key.key_columns_id = generateUuidV7();
    shard_key.hash_function = CatalogManager::HashFunctionKind::MURMUR3;
    shard_key.key_version = 1;
    shard_key.is_active = true;
    ASSERT_EQ(catalog_->upsertShardKeyCatalogEntry(shard_key, &ctx), Status::OK) << ctx.message;

    CatalogManager::ShardKeyCatalogInfo duplicate_active_key = shard_key;
    duplicate_active_key.shard_key_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertShardKeyCatalogEntry(duplicate_active_key, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::ShardCatalogInfo shard{};
    shard.shard_id = generateUuidV7();
    shard.shard_name = "s0";
    shard.cluster_id = cluster_id;
    shard.shard_state = CatalogManager::ShardState::ONLINE;
    shard.shard_kind = CatalogManager::ShardKind::ROW;
    shard.policy_id = policy.policy_id;
    ASSERT_EQ(catalog_->upsertShardCatalogEntry(shard, &ctx), Status::OK) << ctx.message;

    CatalogManager::ShardScopeCatalogInfo scope{};
    scope.scope_id = generateUuidV7();
    scope.shard_id = shard.shard_id;
    scope.object_id = table_id;
    scope.object_kind = CatalogManager::ObjectType::TABLE;
    scope.shard_key_id = shard_key.shard_key_id;
    scope.is_primary_scope = true;
    ASSERT_EQ(catalog_->upsertShardScopeCatalogEntry(scope, &ctx), Status::OK) << ctx.message;

    CatalogManager::ShardRangeCatalogInfo range{};
    range.range_id = generateUuidV7();
    range.shard_id = shard.shard_id;
    range.range_kind = CatalogManager::ShardRangeKind::TOKEN;
    range.has_range_min_s64 = true;
    range.range_min_s64 = 0;
    range.has_range_max_s64 = true;
    range.range_max_s64 = 1000;
    range.inclusive_min = true;
    range.inclusive_max = false;
    ASSERT_EQ(catalog_->upsertShardRangeCatalogEntry(range, &ctx), Status::OK) << ctx.message;

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
    migration.bytes_copied = 200;
    migration.rows_total = 100;
    migration.rows_copied = 10;
    migration.throttle_state = CatalogManager::ThrottleState::LOW;
    migration.started_time = 1000;
    migration.updated_time = 1100;
    ASSERT_EQ(catalog_->upsertShardMigrationCatalogEntry(migration, &ctx), Status::OK) << ctx.message;

    CatalogManager::ShardZoneCatalogInfo zone{};
    zone.zone_id = generateUuidV7();
    zone.zone_name = "zone-us-east";
    zone.description = "us-east placement";
    ASSERT_EQ(catalog_->upsertShardZoneCatalogEntry(zone, &ctx), Status::OK) << ctx.message;

    CatalogManager::ShardZoneRangeCatalogInfo zone_range{};
    zone_range.zone_range_id = generateUuidV7();
    zone_range.zone_id = zone.zone_id;
    zone_range.range_id = range.range_id;
    ASSERT_EQ(catalog_->upsertShardZoneRangeCatalogEntry(zone_range, &ctx), Status::OK) << ctx.message;

    CatalogManager::ShardCatalogInfo fetched_shard{};
    ASSERT_EQ(catalog_->getShardCatalogEntry(shard.shard_id, fetched_shard, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(fetched_shard.shard_name, shard.shard_name);

    std::vector<CatalogManager::ShardReplicaCatalogInfo> replicas;
    ASSERT_EQ(catalog_->listShardReplicaCatalogEntries(shard.shard_id, replicas, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(replicas.size(), 1u);

    ASSERT_EQ(catalog_->deleteShardZoneRangeCatalogEntry(zone_range.zone_range_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteShardZoneCatalogEntry(zone.zone_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteShardMigrationCatalogEntry(migration.migration_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteShardReplicaCatalogEntry(replica.replica_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteNodeCatalogEntry(node_b.node_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteNodeCatalogEntry(node_a.node_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteShardRangeCatalogEntry(range.range_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteShardScopeCatalogEntry(scope.scope_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteShardCatalogEntry(shard.shard_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteShardKeyCatalogEntry(shard_key.shard_key_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteShardPolicyParamCatalogEntry(param.policy_param_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteShardPolicyCatalogEntry(policy.policy_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteClusterCatalogEntry(cluster.cluster_id, &ctx), Status::OK) << ctx.message;
}
