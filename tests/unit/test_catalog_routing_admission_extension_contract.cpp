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

class CatalogRoutingAdmissionExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_routing_admission_extension_contract_" +
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

TEST_F(CatalogRoutingAdmissionExtensionContractTest, RoutingAndAdmissionCatalogContracts)
{
    ErrorContext ctx;

    CatalogManager::WorkloadClassCatalogInfo klass{};
    klass.class_id = generateUuidV7();
    klass.class_name = "oltp_app";
    klass.match_kind = CatalogManager::WorkloadMatchKind::ROLE;
    klass.match_text = "role=app";
    klass.priority = 10;
    klass.has_max_latency_ms = true;
    klass.max_latency_ms = 250;
    klass.allow_cross_shard = false;
    ASSERT_EQ(catalog_->upsertWorkloadClassCatalogEntry(klass, &ctx), Status::OK) << ctx.message;

    CatalogManager::WorkloadClassCatalogInfo duplicate_class = klass;
    duplicate_class.class_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertWorkloadClassCatalogEntry(duplicate_class, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::WorkloadClassCatalogInfo klass_out{};
    ASSERT_EQ(catalog_->getWorkloadClassCatalogEntry(klass.class_id, klass_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(klass_out.class_name, "oltp_app");
    EXPECT_EQ(klass_out.match_text, "role=app");

    CatalogManager::WorkloadRouteCatalogInfo route{};
    route.route_id = generateUuidV7();
    route.class_id = klass.class_id;
    route.route_name = "oltp_primary";
    route.target_kind = CatalogManager::RouteTargetKind::ROLE;
    route.target_label = "oltp";
    route.has_role = true;
    route.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    route.transport = CatalogManager::ConnectionTransport::INET;
    route.route_weight = 100;
    ASSERT_EQ(catalog_->upsertWorkloadRouteCatalogEntry(route, &ctx), Status::OK) << ctx.message;

    CatalogManager::WorkloadRouteCatalogInfo duplicate_route = route;
    duplicate_route.route_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertWorkloadRouteCatalogEntry(duplicate_route, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::AdmissionPolicyCatalogInfo policy{};
    policy.policy_id = generateUuidV7();
    policy.policy_name = "strict_oltp";
    policy.max_concurrent_sessions = 500;
    policy.max_concurrent_queries = 200;
    policy.max_queue_depth = 1000;
    policy.cpu_reject_pct = 95;
    policy.mem_reject_pct = 95;
    policy.io_reject_pct = 95;
    policy.reject_mode = CatalogManager::AdmissionRejectMode::QUEUE;
    policy.queue_timeout_ms = 2000;
    ASSERT_EQ(catalog_->upsertAdmissionPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::AdmissionBindingCatalogInfo binding{};
    binding.binding_id = generateUuidV7();
    binding.policy_id = policy.policy_id;
    binding.target_kind = CatalogManager::AdmissionTargetKind::WORKLOAD_CLASS;
    binding.class_id = klass.class_id;
    binding.priority = 1;
    ASSERT_EQ(catalog_->upsertAdmissionBindingCatalogEntry(binding, &ctx), Status::OK) << ctx.message;

    std::vector<CatalogManager::WorkloadClassCatalogInfo> class_rows;
    ASSERT_EQ(catalog_->listWorkloadClassCatalogEntries(class_rows, &ctx), Status::OK) << ctx.message;
    ASSERT_FALSE(class_rows.empty());

    std::vector<CatalogManager::WorkloadRouteCatalogInfo> route_rows;
    ASSERT_EQ(catalog_->listWorkloadRouteCatalogEntries(klass.class_id, route_rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_FALSE(route_rows.empty());

    std::vector<CatalogManager::AdmissionPolicyCatalogInfo> policy_rows;
    ASSERT_EQ(catalog_->listAdmissionPolicyCatalogEntries(policy_rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_FALSE(policy_rows.empty());

    std::vector<CatalogManager::AdmissionBindingCatalogInfo> binding_rows;
    ASSERT_EQ(catalog_->listAdmissionBindingCatalogEntries(policy.policy_id, binding_rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_FALSE(binding_rows.empty());

    ASSERT_EQ(catalog_->deleteAdmissionBindingCatalogEntry(binding.binding_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::AdmissionBindingCatalogInfo binding_out{};
    EXPECT_EQ(catalog_->getAdmissionBindingCatalogEntry(binding.binding_id, binding_out, &ctx),
              Status::NOT_FOUND);
}

TEST_F(CatalogRoutingAdmissionExtensionContractTest, SloAutoscaleAdmissionTuningCatalogContracts)
{
    ErrorContext ctx;
    ID cluster_id = generateUuidV7();

    CatalogManager::NodeCatalogInfo node{};
    node.node_id = generateUuidV7();
    node.cluster_id = cluster_id;
    node.node_name = "node-oltp-a";
    node.node_role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    node.host = "127.0.0.1";
    node.port = 7610;
    node.transport = CatalogManager::ConnectionTransport::INET;
    node.state = CatalogManager::ClusterNodeState::ONLINE;
    ASSERT_EQ(catalog_->upsertNodeCatalogEntry(node, &ctx), Status::OK) << ctx.message;

    CatalogManager::SloProfileCatalogInfo invalid_profile{};
    invalid_profile.slo_profile_id = generateUuidV7();
    invalid_profile.profile_name = "invalid_profile";
    invalid_profile.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    invalid_profile.availability_target_pct = 99.95;
    invalid_profile.latency_p95_target_ms = 100;
    invalid_profile.latency_p99_target_ms = 80;
    invalid_profile.error_rate_target_pct = 0.10;
    invalid_profile.window_minutes = 10;
    invalid_profile.short_burn_window_minutes = 5;
    invalid_profile.long_burn_window_minutes = 10;
    invalid_profile.moderate_burn_threshold = 1.0;
    invalid_profile.high_burn_threshold = 2.0;
    invalid_profile.critical_burn_threshold = 3.0;
    EXPECT_EQ(catalog_->upsertSloProfileCatalogEntry(invalid_profile, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::SloProfileCatalogInfo profile{};
    profile.slo_profile_id = generateUuidV7();
    profile.profile_name = "oltp_slo_profile";
    profile.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    profile.availability_target_pct = 99.95;
    profile.latency_p95_target_ms = 25;
    profile.latency_p99_target_ms = 75;
    profile.error_rate_target_pct = 0.10;
    profile.window_minutes = 10;
    profile.short_burn_window_minutes = 5;
    profile.long_burn_window_minutes = 10;
    profile.moderate_burn_threshold = 1.0;
    profile.high_burn_threshold = 2.0;
    profile.critical_burn_threshold = 3.0;
    profile.version_u64 = 7;
    ASSERT_EQ(catalog_->upsertSloProfileCatalogEntry(profile, &ctx), Status::OK) << ctx.message;

    CatalogManager::SloProfileCatalogInfo duplicate_profile = profile;
    duplicate_profile.slo_profile_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertSloProfileCatalogEntry(duplicate_profile, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::SloProfileCatalogInfo profile_out{};
    ASSERT_EQ(catalog_->getSloProfileCatalogEntry(profile.slo_profile_id, profile_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(profile_out.profile_name, profile.profile_name);

    CatalogManager::SloBindingCatalogInfo missing_profile_binding{};
    missing_profile_binding.slo_binding_id = generateUuidV7();
    missing_profile_binding.slo_profile_id = generateUuidV7();
    missing_profile_binding.has_node_id = true;
    missing_profile_binding.node_id = node.node_id;
    missing_profile_binding.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    missing_profile_binding.priority_rank = 1;
    missing_profile_binding.effective_from_time = 1000;
    EXPECT_EQ(catalog_->upsertSloBindingCatalogEntry(missing_profile_binding, &ctx), Status::NOT_FOUND);

    CatalogManager::SloBindingCatalogInfo binding{};
    binding.slo_binding_id = generateUuidV7();
    binding.slo_profile_id = profile.slo_profile_id;
    binding.has_node_id = true;
    binding.node_id = node.node_id;
    binding.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    binding.priority_rank = 1;
    binding.effective_from_time = 1000;
    binding.has_effective_to_time = true;
    binding.effective_to_time = 2000;
    binding.version_u64 = 1;
    ASSERT_EQ(catalog_->upsertSloBindingCatalogEntry(binding, &ctx), Status::OK) << ctx.message;

    CatalogManager::SloBindingCatalogInfo binding_out{};
    ASSERT_EQ(catalog_->getSloBindingCatalogEntry(binding.slo_binding_id, binding_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(binding_out.slo_profile_id, profile.slo_profile_id);
    EXPECT_EQ(binding_out.node_id, node.node_id);

    CatalogManager::SloWindowCatalogInfo invalid_window{};
    invalid_window.slo_window_id = generateUuidV7();
    invalid_window.node_id = node.node_id;
    invalid_window.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    invalid_window.window_start_time = 1000;
    invalid_window.window_end_time = 1200;
    invalid_window.request_count = 10;
    invalid_window.success_count = 8;
    invalid_window.error_count = 3;
    EXPECT_EQ(catalog_->upsertSloWindowCatalogEntry(invalid_window, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::SloWindowCatalogInfo window{};
    window.slo_window_id = generateUuidV7();
    window.node_id = node.node_id;
    window.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    window.window_start_time = 1000;
    window.window_end_time = 1600;
    window.request_count = 100;
    window.success_count = 96;
    window.error_count = 4;
    window.latency_p95_ms = 17;
    window.latency_p99_ms = 46;
    window.availability_sli_pct = 99.0;
    window.error_rate_sli_pct = 1.0;
    window.version_u64 = 2;
    ASSERT_EQ(catalog_->upsertSloWindowCatalogEntry(window, &ctx), Status::OK) << ctx.message;

    CatalogManager::SloWindowCatalogInfo window_out{};
    ASSERT_EQ(catalog_->getSloWindowCatalogEntry(window.slo_window_id, window_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(window_out.request_count, 100u);

    CatalogManager::SloBurnEventCatalogInfo invalid_event{};
    invalid_event.slo_burn_event_id = generateUuidV7();
    invalid_event.node_id = node.node_id;
    invalid_event.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    invalid_event.slo_profile_id = profile.slo_profile_id;
    invalid_event.short_burn_rate = 2.4;
    invalid_event.long_burn_rate = 1.6;
    invalid_event.burn_severity = CatalogManager::SloBurnSeverity::HIGH;
    invalid_event.action_plan = CatalogManager::SloActionPlan::SCALE_OUT;
    invalid_event.event_time = 1700;
    invalid_event.has_resolved_time = true;
    invalid_event.resolved_time = 1600;
    EXPECT_EQ(catalog_->upsertSloBurnEventCatalogEntry(invalid_event, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::SloBurnEventCatalogInfo burn_event{};
    burn_event.slo_burn_event_id = generateUuidV7();
    burn_event.node_id = node.node_id;
    burn_event.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    burn_event.slo_profile_id = profile.slo_profile_id;
    burn_event.short_burn_rate = 2.4;
    burn_event.long_burn_rate = 1.6;
    burn_event.burn_severity = CatalogManager::SloBurnSeverity::HIGH;
    burn_event.action_plan = CatalogManager::SloActionPlan::SCALE_OUT_AND_TIGHTEN;
    burn_event.event_time = 1700;
    ASSERT_EQ(catalog_->upsertSloBurnEventCatalogEntry(burn_event, &ctx), Status::OK) << ctx.message;

    CatalogManager::SloBurnEventCatalogInfo burn_out{};
    ASSERT_EQ(catalog_->getSloBurnEventCatalogEntry(burn_event.slo_burn_event_id, burn_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(burn_out.burn_severity, CatalogManager::SloBurnSeverity::HIGH);

    CatalogManager::AutoscalePolicyCatalogInfo invalid_policy{};
    invalid_policy.autoscale_policy_id = generateUuidV7();
    invalid_policy.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    invalid_policy.min_nodes = 4;
    invalid_policy.max_nodes = 2;
    invalid_policy.scale_out_step = 1;
    invalid_policy.scale_in_step = 1;
    EXPECT_EQ(catalog_->upsertAutoscalePolicyCatalogEntry(invalid_policy, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::AutoscalePolicyCatalogInfo policy{};
    policy.autoscale_policy_id = generateUuidV7();
    policy.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    policy.min_nodes = 2;
    policy.max_nodes = 8;
    policy.scale_out_step = 2;
    policy.scale_in_step = 1;
    policy.scale_out_cooldown_ms = 30000;
    policy.scale_in_cooldown_ms = 60000;
    policy.cpu_scale_out_pct = 80;
    policy.queue_scale_out_pct = 70;
    policy.slo_burn_scale_out_threshold = 1.5;
    policy.slo_recovery_scale_in_threshold = 0.7;
    policy.version_u64 = 4;
    ASSERT_EQ(catalog_->upsertAutoscalePolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::AutoscalePolicyCatalogInfo duplicate_policy = policy;
    duplicate_policy.autoscale_policy_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertAutoscalePolicyCatalogEntry(duplicate_policy, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::AutoscaleActionCatalogInfo invalid_action{};
    invalid_action.autoscale_action_id = generateUuidV7();
    invalid_action.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    invalid_action.action_kind = CatalogManager::AutoscaleActionKind::SCALE_OUT;
    invalid_action.requested_count_delta = 2;
    invalid_action.applied_count_delta = 0;
    invalid_action.trigger_burn_rate = 2.2;
    invalid_action.policy_version_u64 = policy.version_u64;
    invalid_action.action_time = 2000;
    invalid_action.has_completed_time = true;
    invalid_action.completed_time = 1900;
    invalid_action.action_state = CatalogManager::AutoscaleActionState::FAILED;
    EXPECT_EQ(catalog_->upsertAutoscaleActionCatalogEntry(invalid_action, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::AutoscaleActionCatalogInfo action{};
    action.autoscale_action_id = generateUuidV7();
    action.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    action.action_kind = CatalogManager::AutoscaleActionKind::SCALE_OUT;
    action.requested_count_delta = 2;
    action.applied_count_delta = 2;
    action.trigger_reason = "high burn rate";
    action.trigger_burn_rate = 2.2;
    action.policy_version_u64 = policy.version_u64;
    action.action_time = 2000;
    action.has_completed_time = true;
    action.completed_time = 2100;
    action.action_state = CatalogManager::AutoscaleActionState::APPLIED;
    ASSERT_EQ(catalog_->upsertAutoscaleActionCatalogEntry(action, &ctx), Status::OK) << ctx.message;

    CatalogManager::AutoscaleActionCatalogInfo action_out{};
    ASSERT_EQ(catalog_->getAutoscaleActionCatalogEntry(action.autoscale_action_id, action_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(action_out.action_kind, CatalogManager::AutoscaleActionKind::SCALE_OUT);
    EXPECT_EQ(action_out.trigger_reason, "high burn rate");

    CatalogManager::AdmissionTuningEventCatalogInfo invalid_tuning{};
    invalid_tuning.admission_tuning_event_id = generateUuidV7();
    invalid_tuning.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    invalid_tuning.old_max_concurrent_queries = 100;
    invalid_tuning.new_max_concurrent_queries = 0;
    invalid_tuning.old_max_queue_depth = 200;
    invalid_tuning.new_max_queue_depth = 250;
    invalid_tuning.old_queue_timeout_ms = 500;
    invalid_tuning.new_queue_timeout_ms = 750;
    invalid_tuning.event_time = 2300;
    EXPECT_EQ(catalog_->upsertAdmissionTuningEventCatalogEntry(invalid_tuning, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::AdmissionTuningEventCatalogInfo tuning{};
    tuning.admission_tuning_event_id = generateUuidV7();
    tuning.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    tuning.old_max_concurrent_queries = 100;
    tuning.new_max_concurrent_queries = 90;
    tuning.old_max_queue_depth = 200;
    tuning.new_max_queue_depth = 180;
    tuning.old_queue_timeout_ms = 500;
    tuning.new_queue_timeout_ms = 450;
    tuning.reason = "slo burn moderate";
    tuning.policy_version_u64 = 5;
    tuning.event_time = 2300;
    ASSERT_EQ(catalog_->upsertAdmissionTuningEventCatalogEntry(tuning, &ctx), Status::OK) << ctx.message;

    CatalogManager::AdmissionTuningEventCatalogInfo tuning_out{};
    ASSERT_EQ(catalog_->getAdmissionTuningEventCatalogEntry(tuning.admission_tuning_event_id, tuning_out, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(tuning_out.reason, "slo burn moderate");

    std::vector<CatalogManager::SloBindingCatalogInfo> binding_rows;
    ASSERT_EQ(catalog_->listSloBindingCatalogEntries(profile.slo_profile_id, binding_rows, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(binding_rows.size(), 1u);

    std::vector<CatalogManager::SloWindowCatalogInfo> window_rows;
    ASSERT_EQ(catalog_->listSloWindowCatalogEntries(node.node_id, window_rows, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(window_rows.size(), 1u);

    std::vector<CatalogManager::SloBurnEventCatalogInfo> burn_rows;
    ASSERT_EQ(catalog_->listSloBurnEventCatalogEntries(node.node_id, burn_rows, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(burn_rows.size(), 1u);

    std::vector<CatalogManager::AutoscalePolicyCatalogInfo> policy_rows;
    ASSERT_EQ(catalog_->listAutoscalePolicyCatalogEntries(policy_rows, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(policy_rows.size(), 1u);

    std::vector<CatalogManager::AutoscaleActionCatalogInfo> action_rows;
    ASSERT_EQ(catalog_->listAutoscaleActionCatalogEntries(CatalogManager::ClusterNodeRole::OLTP_DATA,
                                                           action_rows, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(action_rows.size(), 1u);

    std::vector<CatalogManager::AdmissionTuningEventCatalogInfo> tuning_rows;
    ASSERT_EQ(catalog_->listAdmissionTuningEventCatalogEntries(CatalogManager::ClusterNodeRole::OLTP_DATA,
                                                               tuning_rows, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(tuning_rows.size(), 1u);

    ASSERT_EQ(catalog_->deleteAdmissionTuningEventCatalogEntry(tuning.admission_tuning_event_id, &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteAutoscaleActionCatalogEntry(action.autoscale_action_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteAutoscalePolicyCatalogEntry(policy.autoscale_policy_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteSloBurnEventCatalogEntry(burn_event.slo_burn_event_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteSloWindowCatalogEntry(window.slo_window_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteSloBindingCatalogEntry(binding.slo_binding_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteSloProfileCatalogEntry(profile.slo_profile_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteNodeCatalogEntry(node.node_id, &ctx), Status::OK)
        << ctx.message;
}
