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

class CatalogIncidentHealingAlertExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_incident_healing_alert_extension_contract_" +
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

TEST_F(CatalogIncidentHealingAlertExtensionContractTest, IncidentHealingAlertCatalogContracts)
{
    ErrorContext ctx;

    CatalogManager::ClusterCatalogInfo cluster{};
    cluster.cluster_id = generateUuidV7();
    cluster.cluster_name = "cluster-alpha";
    cluster.cluster_mode = CatalogManager::ClusterMode::CLUSTER;
    cluster.cluster_state = CatalogManager::ClusterState::ONLINE;
    cluster.consensus_mode = CatalogManager::ConsensusMode::RAFT;
    ASSERT_EQ(catalog_->upsertClusterCatalogEntry(cluster, &ctx), Status::OK) << ctx.message;

    CatalogManager::NodeCatalogInfo node{};
    node.node_id = generateUuidV7();
    node.cluster_id = cluster.cluster_id;
    node.node_name = "node-1";
    node.node_role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    node.host = "127.0.0.1";
    node.port = 7801;
    node.transport = CatalogManager::ConnectionTransport::INET;
    node.state = CatalogManager::ClusterNodeState::ONLINE;
    ASSERT_EQ(catalog_->upsertNodeCatalogEntry(node, &ctx), Status::OK) << ctx.message;

    ID system_user = catalog_->getSystemUserId(&ctx);
    ASSERT_NE(system_user, ID{}) << ctx.message;

    CatalogManager::ClusterPolicyCatalogInfo policy{};
    policy.policy_id = generateUuidV7();
    policy.cluster_id = cluster.cluster_id;
    policy.policy_name = "cluster-routing-policy";
    policy.policy_kind = CatalogManager::ClusterPolicyKind::ROUTING;
    policy.is_active = true;
    ASSERT_EQ(catalog_->upsertClusterPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::FailureDetectorCatalogInfo detector{};
    detector.detector_id = generateUuidV7();
    detector.cluster_id = cluster.cluster_id;
    detector.detector_kind = CatalogManager::FailureDetectorKind::PHI;
    detector.heartbeat_interval_ms = 1000;
    detector.has_phi_threshold = true;
    detector.phi_threshold = 8.0;
    detector.grace_startup_ms = 5000;
    ASSERT_EQ(catalog_->upsertFailureDetectorCatalogEntry(detector, &ctx), Status::OK) << ctx.message;

    CatalogManager::AlertRuleCatalogInfo invalid_rule{};
    invalid_rule.rule_id = generateUuidV7();
    invalid_rule.rule_name = "invalid_rule";
    invalid_rule.rule_kind = CatalogManager::AlertRuleKind::METRIC;
    invalid_rule.severity = CatalogManager::AlertSeverity::CRITICAL;
    invalid_rule.has_condition_text = true;
    invalid_rule.condition_text = "cpu > 90";
    invalid_rule.has_condition_sblr_uuid = true;
    invalid_rule.condition_sblr_uuid = generateUuidV7();
    invalid_rule.throttle_interval_ms = 1000;
    EXPECT_EQ(catalog_->upsertAlertRuleCatalogEntry(invalid_rule, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::AlertRuleCatalogInfo rule{};
    rule.rule_id = generateUuidV7();
    rule.rule_name = "cpu_critical";
    rule.rule_kind = CatalogManager::AlertRuleKind::METRIC;
    rule.severity = CatalogManager::AlertSeverity::CRITICAL;
    rule.has_condition_text = true;
    rule.condition_text = "cpu > 90";
    rule.throttle_interval_ms = 1000;
    ASSERT_EQ(catalog_->upsertAlertRuleCatalogEntry(rule, &ctx), Status::OK) << ctx.message;

    CatalogManager::AlertTargetCatalogInfo target{};
    target.target_id = generateUuidV7();
    target.target_name = "ops_webhook";
    target.target_kind = CatalogManager::AlertTargetKind::WEBHOOK;
    target.endpoint = "https://alerts.example/api";
    ASSERT_EQ(catalog_->upsertAlertTargetCatalogEntry(target, &ctx), Status::OK) << ctx.message;

    CatalogManager::AlertRouteCatalogInfo route{};
    route.route_id = generateUuidV7();
    route.rule_id = rule.rule_id;
    route.target_id = target.target_id;
    route.route_kind = CatalogManager::AlertRouteKind::IMMEDIATE;
    route.severity_min = CatalogManager::AlertSeverity::WARNING;
    route.severity_max = CatalogManager::AlertSeverity::CRITICAL;
    ASSERT_EQ(catalog_->upsertAlertRouteCatalogEntry(route, &ctx), Status::OK) << ctx.message;

    CatalogManager::AlertEventCatalogInfo event{};
    event.event_id = generateUuidV7();
    event.rule_id = rule.rule_id;
    event.severity = CatalogManager::AlertSeverity::CRITICAL;
    event.event_state = CatalogManager::AlertEventState::OPEN;
    event.event_time = 10000;
    ASSERT_EQ(catalog_->upsertAlertEventCatalogEntry(event, &ctx), Status::OK) << ctx.message;

    CatalogManager::AlertAckCatalogInfo ack{};
    ack.ack_id = generateUuidV7();
    ack.event_id = event.event_id;
    ack.user_id = system_user;
    ack.ack_time = 11000;
    ack.has_comment = true;
    ack.comment = "acknowledged by operator";
    ASSERT_EQ(catalog_->upsertAlertAckCatalogEntry(ack, &ctx), Status::OK) << ctx.message;

    CatalogManager::AlertSilenceCatalogInfo silence{};
    silence.silence_id = generateUuidV7();
    silence.scope_kind = CatalogManager::AlertSilenceScope::RULE;
    silence.has_scope_uuid = true;
    silence.scope_uuid = rule.rule_id;
    silence.starts_time = 12000;
    silence.ends_time = 15000;
    silence.created_by_uuid = system_user;
    silence.has_reason = true;
    silence.reason = "maintenance window";
    ASSERT_EQ(catalog_->upsertAlertSilenceCatalogEntry(silence, &ctx), Status::OK) << ctx.message;

    CatalogManager::NetworkPartitionEventCatalogInfo partition{};
    partition.partition_id = generateUuidV7();
    partition.cluster_id = cluster.cluster_id;
    partition.partition_state = CatalogManager::PartitionState::OPEN;
    partition.opened_time = 20000;
    partition.quorum_reachable = false;
    partition.local_node_id = node.node_id;
    partition.has_description = true;
    partition.description = "inter-az network split";
    ASSERT_EQ(catalog_->upsertNetworkPartitionEventCatalogEntry(partition, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::NetworkPartitionMemberCatalogInfo member{};
    member.member_id = generateUuidV7();
    member.partition_id = partition.partition_id;
    member.node_id = node.node_id;
    member.side_id = 1;
    member.reachable = false;
    ASSERT_EQ(catalog_->upsertNetworkPartitionMemberCatalogEntry(member, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::HealingPolicyCatalogInfo healing_policy{};
    healing_policy.policy_id = generateUuidV7();
    healing_policy.policy_name = "partition_auto_heal";
    healing_policy.trigger_kind = CatalogManager::HealingTriggerKind::PARTITION;
    healing_policy.min_severity = CatalogManager::AlertSeverity::WARNING;
    ASSERT_EQ(catalog_->upsertHealingPolicyCatalogEntry(healing_policy, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::HealingActionCatalogInfo healing_action{};
    healing_action.action_id = generateUuidV7();
    healing_action.policy_id = healing_policy.policy_id;
    healing_action.action_kind = CatalogManager::HealingActionKind::REBALANCE_SHARDS;
    healing_action.action_order = 1;
    healing_action.max_retries = 3;
    healing_action.cooldown_ms = 30000;
    ASSERT_EQ(catalog_->upsertHealingActionCatalogEntry(healing_action, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::HealingActionParamCatalogInfo invalid_param{};
    invalid_param.param_id = generateUuidV7();
    invalid_param.action_id = healing_action.action_id;
    invalid_param.param_key = "rebalance_parallelism";
    invalid_param.param_type = CatalogManager::HealingParamType::INT;
    invalid_param.has_val_bool = true;
    invalid_param.val_bool = true;
    EXPECT_EQ(catalog_->upsertHealingActionParamCatalogEntry(invalid_param, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::HealingActionParamCatalogInfo param{};
    param.param_id = generateUuidV7();
    param.action_id = healing_action.action_id;
    param.param_key = "rebalance_parallelism";
    param.param_type = CatalogManager::HealingParamType::INT;
    param.has_val_i64 = true;
    param.val_i64 = 8;
    ASSERT_EQ(catalog_->upsertHealingActionParamCatalogEntry(param, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::HealingRunCatalogInfo run{};
    run.run_id = generateUuidV7();
    run.policy_id = healing_policy.policy_id;
    run.has_trigger_event_id = true;
    run.trigger_event_id = event.event_id;
    run.state = CatalogManager::HealingRunState::RUNNING;
    run.started_time = 21000;
    ASSERT_EQ(catalog_->upsertHealingRunCatalogEntry(run, &ctx), Status::OK) << ctx.message;

    CatalogManager::HealingStepCatalogInfo step{};
    step.step_id = generateUuidV7();
    step.run_id = run.run_id;
    step.action_id = healing_action.action_id;
    step.step_index = 1;
    step.state = CatalogManager::HealingStepState::RUNNING;
    step.has_started_time = true;
    step.started_time = 21100;
    ASSERT_EQ(catalog_->upsertHealingStepCatalogEntry(step, &ctx), Status::OK) << ctx.message;

    std::vector<CatalogManager::AlertRouteCatalogInfo> routes;
    ASSERT_EQ(catalog_->listAlertRouteCatalogEntries(rule.rule_id, routes, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(routes.size(), 1u);

    std::vector<CatalogManager::NetworkPartitionMemberCatalogInfo> members;
    ASSERT_EQ(catalog_->listNetworkPartitionMemberCatalogEntries(partition.partition_id, members, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(members.size(), 1u);

    std::vector<CatalogManager::HealingStepCatalogInfo> steps;
    ASSERT_EQ(catalog_->listHealingStepCatalogEntries(run.run_id, steps, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(steps.size(), 1u);

    ASSERT_EQ(catalog_->deleteHealingStepCatalogEntry(step.step_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteHealingRunCatalogEntry(run.run_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteHealingActionParamCatalogEntry(param.param_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteHealingActionCatalogEntry(healing_action.action_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteHealingPolicyCatalogEntry(healing_policy.policy_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteNetworkPartitionMemberCatalogEntry(member.member_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteNetworkPartitionEventCatalogEntry(partition.partition_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteAlertSilenceCatalogEntry(silence.silence_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteAlertAckCatalogEntry(ack.ack_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteAlertEventCatalogEntry(event.event_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteAlertRouteCatalogEntry(route.route_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteAlertTargetCatalogEntry(target.target_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteAlertRuleCatalogEntry(rule.rule_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteFailureDetectorCatalogEntry(detector.detector_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteClusterPolicyCatalogEntry(policy.policy_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteNodeCatalogEntry(node.node_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteClusterCatalogEntry(cluster.cluster_id, &ctx), Status::OK) << ctx.message;
}
