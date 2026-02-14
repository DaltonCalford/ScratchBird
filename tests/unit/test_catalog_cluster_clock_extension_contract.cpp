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

class CatalogClusterClockExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_cluster_clock_extension_contract_" +
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

TEST_F(CatalogClusterClockExtensionContractTest, ClockCatalogContracts)
{
    ErrorContext ctx;
    ID cluster_id = generateUuidV7();
    ID node_uuid = generateUuidV7();

    CatalogManager::ClockPolicyCatalogInfo invalid_policy{};
    invalid_policy.clock_policy_id = generateUuidV7();
    invalid_policy.policy_name = "invalid";
    invalid_policy.warn_skew_ms = 50;
    invalid_policy.soft_skew_ms = 40;
    invalid_policy.hard_skew_ms = 100;
    invalid_policy.sample_interval_ms = 1000;
    invalid_policy.stale_after_ms = 2000;
    EXPECT_EQ(catalog_->upsertClockPolicyCatalogEntry(invalid_policy, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::ClockPolicyCatalogInfo policy{};
    policy.clock_policy_id = generateUuidV7();
    policy.policy_name = "cluster_default";
    policy.warn_skew_ms = 25;
    policy.soft_skew_ms = 100;
    policy.hard_skew_ms = 250;
    policy.max_jitter_ms = 50;
    policy.sample_interval_ms = 1000;
    policy.stale_after_ms = 5000;
    policy.skew_guard_ms = 20;
    policy.node_quarantine_on_hard_skew = true;
    ASSERT_EQ(catalog_->upsertClockPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::ClockPolicyCatalogInfo duplicate_policy{};
    duplicate_policy.clock_policy_id = generateUuidV7();
    duplicate_policy.policy_name = policy.policy_name;
    duplicate_policy.warn_skew_ms = 10;
    duplicate_policy.soft_skew_ms = 20;
    duplicate_policy.hard_skew_ms = 30;
    duplicate_policy.sample_interval_ms = 1000;
    duplicate_policy.stale_after_ms = 2000;
    EXPECT_EQ(catalog_->upsertClockPolicyCatalogEntry(duplicate_policy, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::ClockSourceCatalogInfo source{};
    source.clock_source_id = generateUuidV7();
    source.clock_policy_id = policy.clock_policy_id;
    source.source_kind = CatalogManager::ClockSourceKind::NTP;
    source.endpoint = "ntp1.example.net";
    source.priority_rank = 1;
    source.is_enabled = true;
    ASSERT_EQ(catalog_->upsertClockSourceCatalogEntry(source, &ctx), Status::OK) << ctx.message;

    CatalogManager::ClockSourceCatalogInfo duplicate_source{};
    duplicate_source.clock_source_id = generateUuidV7();
    duplicate_source.clock_policy_id = policy.clock_policy_id;
    duplicate_source.source_kind = CatalogManager::ClockSourceKind::PTP;
    duplicate_source.endpoint = "ptp.example.net";
    duplicate_source.priority_rank = 1;
    EXPECT_EQ(catalog_->upsertClockSourceCatalogEntry(duplicate_source, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::NodeCatalogInfo node{};
    node.node_id = node_uuid;
    node.cluster_id = cluster_id;
    node.node_name = "node-a";
    node.node_role = CatalogManager::ClusterNodeRole::METADATA;
    node.host = "127.0.0.1";
    node.port = 7500;
    node.transport = CatalogManager::ConnectionTransport::INET;
    node.state = CatalogManager::ClusterNodeState::ONLINE;
    ASSERT_EQ(catalog_->upsertNodeCatalogEntry(node, &ctx), Status::OK) << ctx.message;

    CatalogManager::NodeClockStateCatalogInfo node_clock_state{};
    node_clock_state.node_clock_state_id = generateUuidV7();
    node_clock_state.node_id = node_uuid;
    node_clock_state.clock_policy_id = policy.clock_policy_id;
    node_clock_state.clock_state = CatalogManager::ClockStateLabel::WARN;
    node_clock_state.offset_ms = 120;
    node_clock_state.jitter_ms = 25;
    node_clock_state.sample_count = 32;
    node_clock_state.last_sync_time = 10000;
    node_clock_state.last_transition_time = 10100;
    node_clock_state.logical_counter = 5;
    ASSERT_EQ(catalog_->upsertNodeClockStateCatalogEntry(node_clock_state, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::NodeClockStateCatalogInfo duplicate_node_clock_state{};
    duplicate_node_clock_state.node_clock_state_id = generateUuidV7();
    duplicate_node_clock_state.node_id = node_uuid;
    duplicate_node_clock_state.clock_policy_id = policy.clock_policy_id;
    duplicate_node_clock_state.clock_state = CatalogManager::ClockStateLabel::SOFT_SKEW;
    duplicate_node_clock_state.last_sync_time = 11000;
    duplicate_node_clock_state.last_transition_time = 11100;
    EXPECT_EQ(catalog_->upsertNodeClockStateCatalogEntry(duplicate_node_clock_state, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::ClockViolationEventCatalogInfo invalid_event{};
    invalid_event.clock_violation_event_id = generateUuidV7();
    invalid_event.node_id = node_uuid;
    invalid_event.clock_policy_id = policy.clock_policy_id;
    invalid_event.clock_state = CatalogManager::ClockStateLabel::HEALTHY;
    invalid_event.action_taken = CatalogManager::ClockActionTaken::NONE;
    invalid_event.event_time = 20000;
    EXPECT_EQ(catalog_->upsertClockViolationEventCatalogEntry(invalid_event, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::ClockViolationEventCatalogInfo event{};
    event.clock_violation_event_id = generateUuidV7();
    event.node_id = node_uuid;
    event.clock_policy_id = policy.clock_policy_id;
    event.clock_state = CatalogManager::ClockStateLabel::HARD_SKEW;
    event.offset_ms = 450;
    event.jitter_ms = 80;
    event.action_taken = CatalogManager::ClockActionTaken::QUARANTINE;
    event.event_time = 20000;
    ASSERT_EQ(catalog_->upsertClockViolationEventCatalogEntry(event, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ClockViolationEventCatalogInfo loaded_event{};
    ASSERT_EQ(catalog_->getClockViolationEventCatalogEntry(event.clock_violation_event_id, loaded_event, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(loaded_event.clock_state, CatalogManager::ClockStateLabel::HARD_SKEW);
    EXPECT_EQ(loaded_event.action_taken, CatalogManager::ClockActionTaken::QUARANTINE);

    std::vector<CatalogManager::ClockSourceCatalogInfo> sources;
    ASSERT_EQ(catalog_->listClockSourceCatalogEntries(policy.clock_policy_id, sources, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(sources.size(), 1u);

    ASSERT_EQ(catalog_->deleteClockViolationEventCatalogEntry(event.clock_violation_event_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteNodeClockStateCatalogEntry(node_clock_state.node_clock_state_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteNodeCatalogEntry(node.node_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteClockSourceCatalogEntry(source.clock_source_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteClockPolicyCatalogEntry(policy.clock_policy_id, &ctx), Status::OK)
        << ctx.message;
}

TEST_F(CatalogClusterClockExtensionContractTest, NodeCatalogContracts)
{
    ErrorContext ctx;
    ID cluster_id = generateUuidV7();

    CatalogManager::NodeCatalogInfo node{};
    node.node_id = generateUuidV7();
    node.cluster_id = cluster_id;
    node.node_name = "node-001";
    node.node_role = CatalogManager::ClusterNodeRole::METADATA;
    node.host = "127.0.0.1";
    node.port = 7100;
    node.transport = CatalogManager::ConnectionTransport::INET;
    node.region = "us-east";
    node.zone = "us-east-1a";
    node.rack = "rack-a";
    node.state = CatalogManager::ClusterNodeState::ONLINE;
    node.has_last_heartbeat_time = true;
    node.last_heartbeat_time = 100;
    ASSERT_EQ(catalog_->upsertNodeCatalogEntry(node, &ctx), Status::OK) << ctx.message;

    CatalogManager::NodeCatalogInfo duplicate_name{};
    duplicate_name.node_id = generateUuidV7();
    duplicate_name.cluster_id = cluster_id;
    duplicate_name.node_name = node.node_name;
    duplicate_name.node_role = CatalogManager::ClusterNodeRole::ROUTER;
    duplicate_name.host = "127.0.0.2";
    duplicate_name.port = 7101;
    duplicate_name.transport = CatalogManager::ConnectionTransport::INET;
    duplicate_name.state = CatalogManager::ClusterNodeState::ONLINE;
    EXPECT_EQ(catalog_->upsertNodeCatalogEntry(duplicate_name, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::NodeCatalogInfo loaded_node{};
    ASSERT_EQ(catalog_->getNodeCatalogEntry(node.node_id, loaded_node, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(loaded_node.node_name, node.node_name);
    EXPECT_EQ(loaded_node.host, node.host);
    EXPECT_EQ(loaded_node.port, node.port);

    std::vector<CatalogManager::NodeCatalogInfo> nodes;
    ASSERT_EQ(catalog_->listNodeCatalogEntries(cluster_id, nodes, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(nodes.size(), 1u);

    CatalogManager::NodeRoleBindingCatalogInfo binding{};
    binding.binding_id = generateUuidV7();
    binding.node_id = node.node_id;
    binding.role = CatalogManager::ClusterNodeRole::METADATA;
    binding.is_primary = true;
    ASSERT_EQ(catalog_->upsertNodeRoleBindingCatalogEntry(binding, &ctx), Status::OK) << ctx.message;

    CatalogManager::NodeRoleBindingCatalogInfo duplicate_binding{};
    duplicate_binding.binding_id = generateUuidV7();
    duplicate_binding.node_id = node.node_id;
    duplicate_binding.role = CatalogManager::ClusterNodeRole::METADATA;
    EXPECT_EQ(catalog_->upsertNodeRoleBindingCatalogEntry(duplicate_binding, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::NodeServiceCatalogInfo service{};
    service.service_id = generateUuidV7();
    service.node_id = node.node_id;
    service.role = CatalogManager::ClusterNodeRole::METADATA;
    service.service_type = CatalogManager::ClusterServiceType::ADMIN;
    service.transport = CatalogManager::ConnectionTransport::INET;
    service.host = "127.0.0.1";
    service.port = 9001;
    service.state = CatalogManager::ClusterServiceState::ONLINE;
    ASSERT_EQ(catalog_->upsertNodeServiceCatalogEntry(service, &ctx), Status::OK) << ctx.message;

    CatalogManager::NodeServiceCatalogInfo duplicate_service{};
    duplicate_service.service_id = generateUuidV7();
    duplicate_service.node_id = node.node_id;
    duplicate_service.role = CatalogManager::ClusterNodeRole::METADATA;
    duplicate_service.service_type = CatalogManager::ClusterServiceType::ADMIN;
    duplicate_service.transport = CatalogManager::ConnectionTransport::INET;
    duplicate_service.host = "127.0.0.1";
    duplicate_service.port = 9001;
    duplicate_service.state = CatalogManager::ClusterServiceState::ONLINE;
    EXPECT_EQ(catalog_->upsertNodeServiceCatalogEntry(duplicate_service, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::NodeCapabilityCatalogInfo capability{};
    capability.capability_id = generateUuidV7();
    capability.node_id = node.node_id;
    capability.capability_key = "max_parallel_workers";
    capability.capability_value = "32";
    ASSERT_EQ(catalog_->upsertNodeCapabilityCatalogEntry(capability, &ctx), Status::OK) << ctx.message;

    CatalogManager::NodeCapabilityCatalogInfo duplicate_capability{};
    duplicate_capability.capability_id = generateUuidV7();
    duplicate_capability.node_id = node.node_id;
    duplicate_capability.capability_key = capability.capability_key;
    duplicate_capability.capability_value = "64";
    EXPECT_EQ(catalog_->upsertNodeCapabilityCatalogEntry(duplicate_capability, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::NodeCapabilityCatalogInfo loaded_capability{};
    ASSERT_EQ(catalog_->getNodeCapabilityCatalogEntry(capability.capability_id, loaded_capability, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(loaded_capability.capability_key, capability.capability_key);
    EXPECT_EQ(loaded_capability.capability_value, capability.capability_value);

    std::vector<CatalogManager::NodeServiceCatalogInfo> services;
    ASSERT_EQ(catalog_->listNodeServiceCatalogEntries(node.node_id, services, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(services.size(), 1u);

    std::vector<CatalogManager::NodeRoleBindingCatalogInfo> bindings;
    ASSERT_EQ(catalog_->listNodeRoleBindingCatalogEntries(node.node_id, bindings, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(bindings.size(), 1u);

    std::vector<CatalogManager::NodeCapabilityCatalogInfo> capabilities;
    ASSERT_EQ(catalog_->listNodeCapabilityCatalogEntries(node.node_id, capabilities, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(capabilities.size(), 1u);

    ASSERT_EQ(catalog_->deleteNodeCapabilityCatalogEntry(capability.capability_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteNodeServiceCatalogEntry(service.service_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteNodeRoleBindingCatalogEntry(binding.binding_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteNodeCatalogEntry(node.node_id, &ctx), Status::OK) << ctx.message;
}
