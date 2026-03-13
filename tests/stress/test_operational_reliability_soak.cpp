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
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#ifdef __linux__
#include <unistd.h>
#endif

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/support_bundle_builder.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/workload_governance.h"
#include "test_helpers.h"

using scratchbird::core::CatalogManager;
using scratchbird::core::ConnectionContext;
using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::ID;
using scratchbird::core::ReadinessHealthState;
using scratchbird::core::Status;
using scratchbird::core::SupportBundleBuilder;
using scratchbird::core::SupportBundleRequest;
using scratchbird::core::SupportBundleResult;
using scratchbird::core::WorkloadGovernance;
using scratchbird::core::generateUuidV7;
using scratchbird::testing::TestDatabaseFile;

namespace {

auto sampleResidentBytes() -> std::uint64_t
{
#ifdef __linux__
    std::ifstream statm("/proc/self/statm");
    std::uint64_t pages_total = 0;
    std::uint64_t pages_resident = 0;
    if (!(statm >> pages_total >> pages_resident))
    {
        return 0;
    }
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0)
    {
        return 0;
    }
    return pages_resident * static_cast<std::uint64_t>(page_size);
#else
    return 0;
#endif
}

auto readFileContents(const std::string& path) -> std::string
{
    std::ifstream in(path);
    if (!in.is_open())
    {
        return {};
    }
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

auto admissionStatusRowForPolicy(
    const std::vector<WorkloadGovernance::AdmissionStatusRow>& rows,
    const std::string& policy_name)
    -> std::vector<WorkloadGovernance::AdmissionStatusRow>::const_iterator
{
    return std::find_if(rows.begin(), rows.end(), [&](const auto& row) {
        return row.policy_name == policy_name;
    });
}

class OperationalReliabilitySoakTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("operational_reliability_soak", ".sbdb");
        bundle_output_path_ = db_file_->path() + ".support_bundle";

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        const Status proc_status = db_.initializeProcArray(16, &ctx);
        if (proc_status != Status::OK && proc_status != Status::INVALID_ARGUMENT)
        {
            ASSERT_EQ(proc_status, Status::OK) << ctx.message;
        }

        ASSERT_EQ(db_.connect(conn_, &ctx), Status::OK) << ctx.message;
        ASSERT_NE(conn_, nullptr);
        ConnectionContext::setCurrent(conn_.get());
        ASSERT_EQ(conn_->initialize(&ctx), Status::OK) << ctx.message;

        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(db_.catalog_manager()->getSchema("PUBLIC", schema_info, &ctx), Status::OK)
            << ctx.message;
        public_schema_id_ = schema_info.schema_id;
        initializeSystemConnection(conn_.get());
    }

    void TearDown() override
    {
        std::remove(bundle_output_path_.c_str());
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        db_.close();
        db_file_.reset();
    }

    void initializeSystemConnection(ConnectionContext* connection)
    {
        ASSERT_NE(connection, nullptr);

        ErrorContext ctx;
        connection->setCurrentSchemaId(public_schema_id_);
        connection->set_current_schema("PUBLIC");
        connection->set_search_path({"PUBLIC"});
        if (system_user_id_ == ID{})
        {
            system_user_id_ = db_.catalog_manager()->getSystemUserId(&ctx);
            ASSERT_NE(system_user_id_, ID{}) << ctx.message;
        }
        connection->setCurrentUser(system_user_id_, true);
    }

    auto connectSystemSession() -> std::unique_ptr<ConnectionContext>
    {
        ErrorContext ctx;
        std::unique_ptr<ConnectionContext> connection;
        EXPECT_EQ(db_.connect(connection, &ctx), Status::OK) << ctx.message;
        EXPECT_NE(connection, nullptr);
        if (!connection)
        {
            return nullptr;
        }
        EXPECT_EQ(connection->initialize(&ctx), Status::OK) << ctx.message;
        initializeSystemConnection(connection.get());
        return connection;
    }

    void commitConnection(ConnectionContext* connection)
    {
        ASSERT_NE(connection, nullptr);
        ErrorContext ctx;
        ASSERT_EQ(connection->commit(&ctx), Status::OK) << ctx.message;
    }

    auto createCluster(const std::string& cluster_name) -> ID
    {
        ErrorContext ctx;
        CatalogManager::ClusterCatalogInfo cluster{};
        cluster.cluster_id = generateUuidV7();
        cluster.cluster_name = cluster_name;
        cluster.cluster_mode = CatalogManager::ClusterMode::CLUSTER;
        cluster.cluster_state = CatalogManager::ClusterState::ONLINE;
        cluster.consensus_mode = CatalogManager::ConsensusMode::RAFT;
        EXPECT_EQ(db_.catalog_manager()->upsertClusterCatalogEntry(cluster, &ctx), Status::OK)
            << ctx.message;
        return cluster.cluster_id;
    }

    auto createNode(const ID& cluster_id,
                    const std::string& node_name,
                    CatalogManager::ClusterNodeRole role,
                    uint16_t port) -> CatalogManager::NodeCatalogInfo
    {
        ErrorContext ctx;
        CatalogManager::NodeCatalogInfo node{};
        node.node_id = generateUuidV7();
        node.cluster_id = cluster_id;
        node.node_name = node_name;
        node.node_role = role;
        node.host = "127.0.0.1";
        node.port = port;
        node.transport = CatalogManager::ConnectionTransport::INET;
        node.state = CatalogManager::ClusterNodeState::ONLINE;
        EXPECT_EQ(db_.catalog_manager()->upsertNodeCatalogEntry(node, &ctx), Status::OK)
            << ctx.message;
        return node;
    }

    auto createWorkloadClass(const std::string& class_name,
                             const std::string& match_text) -> CatalogManager::WorkloadClassCatalogInfo
    {
        ErrorContext ctx;
        CatalogManager::WorkloadClassCatalogInfo klass{};
        klass.class_id = generateUuidV7();
        klass.class_name = class_name;
        klass.match_kind = CatalogManager::WorkloadMatchKind::QUERY_TYPE;
        klass.match_text = match_text;
        klass.priority = 1;
        klass.is_enabled = true;
        EXPECT_EQ(db_.catalog_manager()->upsertWorkloadClassCatalogEntry(klass, &ctx), Status::OK)
            << ctx.message;
        return klass;
    }

    auto createAdmissionPolicy(const std::string& policy_name,
                               uint32_t max_concurrent_sessions,
                               uint32_t max_concurrent_queries,
                               uint32_t max_queue_depth,
                               uint32_t queue_timeout_ms,
                               CatalogManager::AdmissionRejectMode reject_mode)
        -> CatalogManager::AdmissionPolicyCatalogInfo
    {
        ErrorContext ctx;
        CatalogManager::AdmissionPolicyCatalogInfo policy{};
        policy.policy_id = generateUuidV7();
        policy.policy_name = policy_name;
        policy.max_concurrent_sessions = max_concurrent_sessions;
        policy.max_concurrent_queries = max_concurrent_queries;
        policy.max_queue_depth = max_queue_depth;
        policy.queue_timeout_ms = queue_timeout_ms;
        policy.reject_mode = reject_mode;
        policy.cpu_reject_pct = 80;
        policy.mem_reject_pct = 80;
        policy.io_reject_pct = 80;
        policy.is_enabled = true;
        EXPECT_EQ(db_.catalog_manager()->upsertAdmissionPolicyCatalogEntry(policy, &ctx), Status::OK)
            << ctx.message;
        return policy;
    }

    void createAdmissionBinding(const CatalogManager::AdmissionPolicyCatalogInfo& policy,
                                const CatalogManager::WorkloadClassCatalogInfo& klass,
                                uint8_t priority = 1)
    {
        ErrorContext ctx;
        CatalogManager::AdmissionBindingCatalogInfo binding{};
        binding.binding_id = generateUuidV7();
        binding.policy_id = policy.policy_id;
        binding.target_kind = CatalogManager::AdmissionTargetKind::WORKLOAD_CLASS;
        binding.class_id = klass.class_id;
        binding.priority = priority;
        binding.is_enabled = true;
        ASSERT_EQ(db_.catalog_manager()->upsertAdmissionBindingCatalogEntry(binding, &ctx), Status::OK)
            << ctx.message;
    }

    auto createSloProfile(const std::string& profile_name,
                          CatalogManager::ClusterNodeRole role,
                          uint32_t short_window_minutes = 1,
                          uint32_t long_window_minutes = 1) -> CatalogManager::SloProfileCatalogInfo
    {
        ErrorContext ctx;
        CatalogManager::SloProfileCatalogInfo profile{};
        profile.slo_profile_id = generateUuidV7();
        profile.profile_name = profile_name;
        profile.role = role;
        profile.availability_target_pct = 99.95;
        profile.latency_p95_target_ms = 25;
        profile.latency_p99_target_ms = 75;
        profile.error_rate_target_pct = 1.0;
        profile.window_minutes = long_window_minutes;
        profile.short_burn_window_minutes = short_window_minutes;
        profile.long_burn_window_minutes = long_window_minutes;
        profile.moderate_burn_threshold = 2.0;
        profile.high_burn_threshold = 6.0;
        profile.critical_burn_threshold = 14.0;
        profile.version_u64 = 1;
        EXPECT_EQ(db_.catalog_manager()->upsertSloProfileCatalogEntry(profile, &ctx), Status::OK)
            << ctx.message;
        return profile;
    }

    void createSloBinding(const CatalogManager::SloProfileCatalogInfo& profile,
                          CatalogManager::ClusterNodeRole role,
                          uint64_t effective_from_time,
                          const ID& node_id)
    {
        ErrorContext ctx;
        CatalogManager::SloBindingCatalogInfo binding{};
        binding.slo_binding_id = generateUuidV7();
        binding.slo_profile_id = profile.slo_profile_id;
        binding.has_node_id = true;
        binding.node_id = node_id;
        binding.role = role;
        binding.priority_rank = 1;
        binding.effective_from_time = effective_from_time;
        binding.version_u64 = 1;
        ASSERT_EQ(db_.catalog_manager()->upsertSloBindingCatalogEntry(binding, &ctx), Status::OK)
            << ctx.message;
    }

    void createSloWindow(const ID& node_id,
                         CatalogManager::ClusterNodeRole role,
                         uint64_t window_start_time,
                         uint64_t window_end_time,
                         uint64_t request_count,
                         uint64_t success_count,
                         uint64_t error_count,
                         uint32_t latency_p95_ms,
                         uint32_t latency_p99_ms)
    {
        ErrorContext ctx;
        CatalogManager::SloWindowCatalogInfo window{};
        window.slo_window_id = generateUuidV7();
        window.node_id = node_id;
        window.role = role;
        window.window_start_time = window_start_time;
        window.window_end_time = window_end_time;
        window.request_count = request_count;
        window.success_count = success_count;
        window.error_count = error_count;
        window.latency_p95_ms = latency_p95_ms;
        window.latency_p99_ms = latency_p99_ms;
        window.availability_sli_pct = request_count == 0
            ? 100.0
            : (static_cast<double>(success_count) / static_cast<double>(request_count)) * 100.0;
        window.error_rate_sli_pct = request_count == 0
            ? 0.0
            : (static_cast<double>(error_count) / static_cast<double>(request_count)) * 100.0;
        window.version_u64 = 1;
        ASSERT_EQ(db_.catalog_manager()->upsertSloWindowCatalogEntry(window, &ctx), Status::OK)
            << ctx.message;
    }

    void createAutoscalePolicy(CatalogManager::ClusterNodeRole role)
    {
        ErrorContext ctx;
        CatalogManager::AutoscalePolicyCatalogInfo policy{};
        policy.autoscale_policy_id = generateUuidV7();
        policy.role = role;
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
        policy.version_u64 = 1;
        ASSERT_EQ(db_.catalog_manager()->upsertAutoscalePolicyCatalogEntry(policy, &ctx), Status::OK)
            << ctx.message;
    }

    void recordTelemetry(const ID& node_id,
                         CatalogManager::ClusterNodeRole role,
                         uint64_t sample_time,
                         uint8_t cpu_utilization_pct,
                         uint8_t queue_pressure_pct,
                         uint16_t current_node_count)
    {
        ErrorContext ctx;
        WorkloadGovernance::SloTelemetrySample sample{};
        sample.node_id = node_id;
        sample.role = role;
        sample.sample_time = sample_time;
        sample.cpu_utilization_pct = cpu_utilization_pct;
        sample.queue_pressure_pct = queue_pressure_pct;
        sample.current_node_count = current_node_count;
        ASSERT_EQ(db_.workload_governance()->recordSloTelemetrySample(sample, &ctx), Status::OK)
            << ctx.message;
    }

    auto createAlertRule(const std::string& name,
                         CatalogManager::AlertSeverity severity) -> CatalogManager::AlertRuleCatalogInfo
    {
        ErrorContext ctx;
        CatalogManager::AlertRuleCatalogInfo rule{};
        rule.rule_id = generateUuidV7();
        rule.rule_name = name;
        rule.rule_kind = CatalogManager::AlertRuleKind::EVENT;
        rule.severity = severity;
        rule.has_condition_text = true;
        rule.condition_text = "password=topsecret endpoint=https://ops.example/hook?token=abc";
        rule.throttle_interval_ms = 1000;
        EXPECT_EQ(db_.catalog_manager()->upsertAlertRuleCatalogEntry(rule, &ctx), Status::OK)
            << ctx.message;
        return rule;
    }

    auto createAlertTarget(const std::string& name) -> CatalogManager::AlertTargetCatalogInfo
    {
        ErrorContext ctx;
        CatalogManager::AlertTargetCatalogInfo target{};
        target.target_id = generateUuidV7();
        target.target_name = name;
        target.target_kind = CatalogManager::AlertTargetKind::WEBHOOK;
        target.endpoint = "https://user:secret@ops.example/internal?token=abc";
        EXPECT_EQ(db_.catalog_manager()->upsertAlertTargetCatalogEntry(target, &ctx), Status::OK)
            << ctx.message;
        return target;
    }

    void createAlertRoute(const CatalogManager::AlertRuleCatalogInfo& rule,
                          const CatalogManager::AlertTargetCatalogInfo& target)
    {
        ErrorContext ctx;
        CatalogManager::AlertRouteCatalogInfo route{};
        route.route_id = generateUuidV7();
        route.rule_id = rule.rule_id;
        route.target_id = target.target_id;
        route.route_kind = CatalogManager::AlertRouteKind::ESCALATION;
        route.severity_min = CatalogManager::AlertSeverity::INFO;
        route.severity_max = CatalogManager::AlertSeverity::CRITICAL;
        ASSERT_EQ(db_.catalog_manager()->upsertAlertRouteCatalogEntry(route, &ctx), Status::OK)
            << ctx.message;
    }

    void createAlertEvent(const CatalogManager::AlertRuleCatalogInfo& rule,
                          CatalogManager::AlertSeverity severity,
                          uint64_t event_time)
    {
        ErrorContext ctx;
        CatalogManager::AlertEventCatalogInfo event{};
        event.event_id = generateUuidV7();
        event.rule_id = rule.rule_id;
        event.severity = severity;
        event.event_state = CatalogManager::AlertEventState::OPEN;
        event.event_time = event_time;
        ASSERT_EQ(db_.catalog_manager()->upsertAlertEventCatalogEntry(event, &ctx), Status::OK)
            << ctx.message;
    }

    auto makeDescriptor(ConnectionContext* connection,
                        std::string sql = "SELECT 1") const -> WorkloadGovernance::QueryDescriptor
    {
        WorkloadGovernance::QueryDescriptor descriptor;
        descriptor.connection = connection;
        descriptor.sql = std::move(sql);
        descriptor.database_name = "scratchbird";
        descriptor.schema_name = "PUBLIC";
        return descriptor;
    }

    Database db_{};
    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<ConnectionContext> conn_;
    ID public_schema_id_{};
    ID system_user_id_{};
    std::string bundle_output_path_;
};

TEST_F(OperationalReliabilitySoakTest, SustainedGovernanceAndSupportBundleRemainStable)
{
    constexpr std::size_t kBurnRounds = 48;
    constexpr std::size_t kRecoveryRounds = 48;
    constexpr std::size_t kBundleEvery = 12;
    constexpr long long kMaxAllowedRssDeltaBytes = 64LL * 1024LL * 1024LL;

    ErrorContext ctx;
    const ID cluster_id = createCluster("cluster_ops_soak");
    const auto node = createNode(cluster_id,
                                 "node-ops-soak",
                                 CatalogManager::ClusterNodeRole::OLTP_DATA,
                                 7801);
    const auto profile = createSloProfile("ops_soak_profile",
                                          CatalogManager::ClusterNodeRole::OLTP_DATA);
    createSloBinding(profile,
                     CatalogManager::ClusterNodeRole::OLTP_DATA,
                     1000,
                     node.node_id);
    createAutoscalePolicy(CatalogManager::ClusterNodeRole::OLTP_DATA);
    createAdmissionPolicy("ap_ops_soak",
                          8,
                          100,
                          200,
                          500,
                          CatalogManager::AdmissionRejectMode::REJECT);

    const auto rule = createAlertRule("ops_critical", CatalogManager::AlertSeverity::CRITICAL);
    const auto target = createAlertTarget("pager_primary");
    createAlertRoute(rule, target);
    createAlertEvent(rule, CatalogManager::AlertSeverity::CRITICAL, 1000);
    commitConnection(conn_.get());

    SupportBundleBuilder builder(&db_);
    std::size_t bundle_count = 0;
    std::size_t blocked_bundle_count = 0;
    std::uint64_t max_redacted_fields = 0;
    uint64_t window_start = 1;
    const std::uint64_t rss_start = sampleResidentBytes();

    for (std::size_t round = 0; round < (kBurnRounds + kRecoveryRounds); ++round)
    {
        const bool burn_round = round < kBurnRounds;
        const uint64_t window_end = window_start + 60000;
        const uint64_t success_count = burn_round ? 80 : 100;
        const uint64_t error_count = burn_round ? 20 : 0;
        const uint32_t latency_p95_ms = burn_round ? 40 : 10;
        const uint32_t latency_p99_ms = burn_round ? 90 : 20;
        const uint8_t cpu_utilization_pct = burn_round ? 90 : 20;
        const uint8_t queue_pressure_pct = burn_round ? 85 : 10;
        const uint16_t current_node_count = burn_round ? 2 : 4;

        createSloWindow(node.node_id,
                        CatalogManager::ClusterNodeRole::OLTP_DATA,
                        window_start,
                        window_end,
                        100,
                        success_count,
                        error_count,
                        latency_p95_ms,
                        latency_p99_ms);
        recordTelemetry(node.node_id,
                        CatalogManager::ClusterNodeRole::OLTP_DATA,
                        window_end,
                        cpu_utilization_pct,
                        queue_pressure_pct,
                        current_node_count);
        ASSERT_EQ(db_.workload_governance()->evaluateSloPolicies(window_end, &ctx), Status::OK)
            << ctx.message;
        commitConnection(conn_.get());

        std::vector<WorkloadGovernance::SloStatusRow> slo_rows;
        ASSERT_EQ(db_.workload_governance()->snapshotSloStatus(slo_rows, window_end, &ctx), Status::OK)
            << ctx.message;
        ASSERT_FALSE(slo_rows.empty());
        EXPECT_EQ(slo_rows.front().profile_name, "ops_soak_profile");
        EXPECT_EQ(slo_rows.front().role, "OLTP_DATA");

        std::vector<WorkloadGovernance::ErrorBudgetStatusRow> budget_rows;
        ASSERT_EQ(db_.workload_governance()->snapshotErrorBudgetStatus(budget_rows, window_end, &ctx), Status::OK)
            << ctx.message;
        ASSERT_FALSE(budget_rows.empty());
        EXPECT_EQ(budget_rows.front().profile_name, "ops_soak_profile");

        if ((round + 1) % kBundleEvery == 0)
        {
            SupportBundleRequest request;
            request.output_path = bundle_output_path_;
            request.readiness.now_time = window_end;

            SupportBundleResult result;
            ASSERT_EQ(builder.generateSupportBundle(request, result, &ctx), Status::OK) << ctx.message;
            EXPECT_TRUE(result.redaction_enforced);
            EXPECT_GT(result.redacted_field_count, 0u);
            EXPECT_FALSE(result.manifest_preview.empty());
            ++bundle_count;
            max_redacted_fields = std::max(max_redacted_fields, result.redacted_field_count);
            if (result.safety.readiness.state == ReadinessHealthState::BLOCKED)
            {
                ++blocked_bundle_count;
            }

            const std::string contents = readFileContents(bundle_output_path_);
            ASSERT_FALSE(contents.empty());
            EXPECT_NE(contents.find("bundle_id="), std::string::npos);
            EXPECT_NE(contents.find("<redacted>"), std::string::npos);
            EXPECT_NE(contents.find("<endpoint>"), std::string::npos);
            EXPECT_EQ(contents.find("topsecret"), std::string::npos);
            EXPECT_EQ(contents.find("user:secret"), std::string::npos);
        }

        window_start = window_end + 1;
    }

    std::vector<CatalogManager::SloBurnEventCatalogInfo> burn_rows;
    ASSERT_EQ(db_.catalog_manager()->listSloBurnEventCatalogEntries(node.node_id, burn_rows, &ctx), Status::OK)
        << ctx.message;
    EXPECT_FALSE(burn_rows.empty());

    std::vector<CatalogManager::AdmissionTuningEventCatalogInfo> tuning_rows;
    ASSERT_EQ(db_.catalog_manager()->listAdmissionTuningEventCatalogEntries(
                  CatalogManager::ClusterNodeRole::OLTP_DATA,
                  tuning_rows,
                  &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(tuning_rows.empty());

    std::vector<CatalogManager::AutoscaleActionCatalogInfo> action_rows;
    ASSERT_EQ(db_.catalog_manager()->listAutoscaleActionCatalogEntries(
                  CatalogManager::ClusterNodeRole::OLTP_DATA,
                  action_rows,
                  &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(action_rows.empty());
    EXPECT_TRUE(std::any_of(action_rows.begin(),
                            action_rows.end(),
                            [](const auto& row) {
                                return row.action_kind ==
                                       CatalogManager::AutoscaleActionKind::SCALE_OUT;
                            }));
    EXPECT_TRUE(std::any_of(action_rows.begin(),
                            action_rows.end(),
                            [](const auto& row) {
                                return row.action_kind ==
                                       CatalogManager::AutoscaleActionKind::SCALE_IN;
                            }));

    const std::uint64_t rss_end = sampleResidentBytes();
    const long long rss_delta =
        (rss_start > 0 && rss_end > 0)
            ? static_cast<long long>(rss_end) - static_cast<long long>(rss_start)
            : 0;

    std::cout << "[Soak][OperationalReliability]"
              << " rounds=" << (kBurnRounds + kRecoveryRounds)
              << " bundles=" << bundle_count
              << " blocked_bundles=" << blocked_bundle_count
              << " burn_events=" << burn_rows.size()
              << " tuning_events=" << tuning_rows.size()
              << " autoscale_actions=" << action_rows.size()
              << " max_redacted_fields=" << max_redacted_fields
              << " rss_delta_bytes=" << rss_delta
              << std::endl;

    EXPECT_GT(bundle_count, 0u);
    EXPECT_GT(blocked_bundle_count, 0u);
    EXPECT_GT(max_redacted_fields, 0u);
    if (rss_start > 0 && rss_end > 0)
    {
        EXPECT_LE(rss_delta, kMaxAllowedRssDeltaBytes);
    }
}

TEST_F(OperationalReliabilitySoakTest, AdmissionCapacityRemainsBoundedUnderSaturationCycles)
{
    constexpr std::size_t kCycles = 256;
    constexpr long long kMaxAllowedRssDeltaBytes = 32LL * 1024LL * 1024LL;

    const auto klass = createWorkloadClass("wl_capacity", "select");
    const auto policy = createAdmissionPolicy("ap_capacity",
                                              8,
                                              1,
                                              0,
                                              0,
                                              CatalogManager::AdmissionRejectMode::REJECT);
    createAdmissionBinding(policy, klass);
    commitConnection(conn_.get());

    auto second_conn = connectSystemSession();
    auto third_conn = connectSystemSession();
    ASSERT_NE(second_conn, nullptr);
    ASSERT_NE(third_conn, nullptr);

    const std::uint64_t rss_start = sampleResidentBytes();
    std::size_t admitted_count = 0;
    std::size_t rejected_count = 0;

    std::vector<ConnectionContext*> holders{conn_.get(), second_conn.get(), third_conn.get()};

    for (std::size_t cycle = 0; cycle < kCycles; ++cycle)
    {
        ConnectionContext* owner = holders[cycle % holders.size()];

        WorkloadGovernance::AdmissionLease owner_lease;
        ErrorContext owner_ctx;
        auto owner_decision = db_.workload_governance()->acquire(makeDescriptor(owner),
                                                                 owner_lease,
                                                                 &owner_ctx);
        ASSERT_TRUE(owner_decision.admitted) << owner_decision.detail;
        ASSERT_TRUE(owner_lease.active());
        EXPECT_EQ(owner_decision.class_name, "wl_capacity");
        EXPECT_EQ(owner_decision.policy_name, "ap_capacity");
        ++admitted_count;

        std::vector<WorkloadGovernance::AdmissionStatusRow> rows;
        ASSERT_EQ(db_.workload_governance()->snapshotAdmissionStatus(rows, &owner_ctx), Status::OK)
            << owner_ctx.message;
        auto row_it = admissionStatusRowForPolicy(rows, "ap_capacity");
        ASSERT_NE(row_it, rows.end());
        EXPECT_EQ(row_it->active_queries, 1u);
        EXPECT_LE(row_it->active_queries, row_it->max_concurrent_queries);

        for (ConnectionContext* contender : holders)
        {
            if (contender == owner)
            {
                continue;
            }

            WorkloadGovernance::AdmissionLease rejected_lease;
            ErrorContext rejected_ctx;
            auto rejected = db_.workload_governance()->acquire(makeDescriptor(contender),
                                                               rejected_lease,
                                                               &rejected_ctx);
            EXPECT_FALSE(rejected.admitted);
            EXPECT_FALSE(rejected.queued);
            EXPECT_FALSE(rejected_lease.active());
            EXPECT_EQ(rejected.status, Status::CONFIGURATION_LIMIT_EXCEEDED);
            EXPECT_EQ(rejected.code, "GOV_1502");
            EXPECT_EQ(rejected.detail, "Admission rejected by max_concurrent_queries");
            ++rejected_count;
        }

        owner_lease.release();
    }

    std::vector<WorkloadGovernance::AdmissionStatusRow> rows;
    ErrorContext ctx;
    ASSERT_EQ(db_.workload_governance()->snapshotAdmissionStatus(rows, &ctx), Status::OK)
        << ctx.message;
    auto row_it = admissionStatusRowForPolicy(rows, "ap_capacity");
    ASSERT_NE(row_it, rows.end());
    EXPECT_EQ(row_it->active_queries, 0u);
    EXPECT_EQ(row_it->queued_queries, 0u);

    const std::uint64_t rss_end = sampleResidentBytes();
    const long long rss_delta =
        (rss_start > 0 && rss_end > 0)
            ? static_cast<long long>(rss_end) - static_cast<long long>(rss_start)
            : 0;

    std::cout << "[Capacity][OperationalReliability]"
              << " cycles=" << kCycles
              << " admitted=" << admitted_count
              << " rejected=" << rejected_count
              << " rss_delta_bytes=" << rss_delta
              << std::endl;

    EXPECT_EQ(admitted_count, kCycles);
    EXPECT_EQ(rejected_count, kCycles * 2);
    if (rss_start > 0 && rss_end > 0)
    {
        EXPECT_LE(rss_delta, kMaxAllowedRssDeltaBytes);
    }
}

} // namespace
