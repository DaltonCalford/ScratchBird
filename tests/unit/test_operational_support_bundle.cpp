#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/support_bundle_builder.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

using scratchbird::core::CatalogManager;
using scratchbird::core::ConnectionContext;
using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::ID;
using scratchbird::core::Status;
using scratchbird::core::SupportBundleBuilder;
using scratchbird::core::SupportBundleRequest;
using scratchbird::core::SupportBundleResult;
using scratchbird::core::generateUuidV7;
using scratchbird::sblr::ExecutionResult;
using scratchbird::sblr::Executor;
using scratchbird::sblr::QueryCompilerV3;
using scratchbird::testing::TestDatabaseFile;

namespace
{

auto joinErrors(const std::vector<std::string>& errors) -> std::string
{
    std::string joined;
    for (size_t i = 0; i < errors.size(); ++i)
    {
        if (i != 0)
        {
            joined.append("; ");
        }
        joined.append(errors[i]);
    }
    return joined;
}

auto resultSetHasRow(scratchbird::sblr::ResultSet* rs,
                     std::initializer_list<std::pair<size_t, std::string>> expected) -> bool
{
    if (rs == nullptr)
    {
        return false;
    }

    for (size_t row = 0; row < rs->rowCount(); ++row)
    {
        bool matches = true;
        for (const auto& [column, value] : expected)
        {
            if (column >= rs->columnCount() || rs->getValue(row, column).toString() != value)
            {
                matches = false;
                break;
            }
        }
        if (matches)
        {
            return true;
        }
    }
    return false;
}

class OperationalSupportBundleTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("operational_support_bundle", ".sbdb");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
        ASSERT_EQ(conn_->initialize(&ctx), Status::OK) << ctx.message;

        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(db_.catalog_manager()->getSchema("PUBLIC", schema_info, &ctx), Status::OK)
            << ctx.message;
        public_schema_id_ = schema_info.schema_id;

        initializeSystemConnection();

        executor_ = std::make_unique<Executor>(&db_);
        executor_->setConnectionContext(conn_.get());
        executor_->setCurrentSchema(public_schema_id_);
    }

    void TearDown() override
    {
        executor_.reset();
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        db_.close();
        db_file_.reset();
    }

    void initializeSystemConnection()
    {
        ErrorContext ctx;
        conn_->setCurrentSchemaId(public_schema_id_);
        conn_->set_current_schema("PUBLIC");
        conn_->set_search_path({"PUBLIC"});
        system_user_id_ = db_.catalog_manager()->getSystemUserId(&ctx);
        ASSERT_NE(system_user_id_, ID{}) << ctx.message;
        conn_->setCurrentUser(system_user_id_, true);
    }

    auto compileAndExecute(const std::string& sql) -> ExecutionResult
    {
        QueryCompilerV3 compiler(&db_);
        compiler.setCurrentSchema(public_schema_id_);
        auto compiled = compiler.compile(sql);
        EXPECT_TRUE(compiled.success()) << joinErrors(compiled.errors());
        if (!compiled.success())
        {
            return ExecutionResult(joinErrors(compiled.errors()));
        }
        return executor_->execute(compiled.bytecode());
    }

    auto createNode(const std::string& node_name,
                    CatalogManager::ClusterNodeRole role) -> CatalogManager::NodeCatalogInfo
    {
        ErrorContext ctx;

        CatalogManager::ClusterCatalogInfo cluster{};
        cluster.cluster_id = cluster_id_;
        cluster.cluster_name = "cluster_ops";
        cluster.cluster_mode = CatalogManager::ClusterMode::CLUSTER;
        cluster.cluster_state = CatalogManager::ClusterState::ONLINE;
        cluster.consensus_mode = CatalogManager::ConsensusMode::RAFT;
        const Status cluster_status =
            db_.catalog_manager()->upsertClusterCatalogEntry(cluster, &ctx);
        if (cluster_status != Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return {};
        }

        CatalogManager::NodeCatalogInfo node{};
        node.node_id = generateUuidV7();
        node.cluster_id = cluster.cluster_id;
        node.node_name = node_name;
        node.node_role = role;
        node.host = "127.0.0.1";
        node.port = 7801;
        node.transport = CatalogManager::ConnectionTransport::INET;
        node.state = CatalogManager::ClusterNodeState::ONLINE;
        const Status node_status = db_.catalog_manager()->upsertNodeCatalogEntry(node, &ctx);
        if (node_status != Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return {};
        }
        return node;
    }

    auto createSloProfile(const std::string& name,
                          CatalogManager::ClusterNodeRole role) -> CatalogManager::SloProfileCatalogInfo
    {
        ErrorContext ctx;
        CatalogManager::SloProfileCatalogInfo profile{};
        profile.slo_profile_id = generateUuidV7();
        profile.profile_name = name;
        profile.role = role;
        profile.availability_target_pct = 99.95;
        profile.latency_p95_target_ms = 20;
        profile.latency_p99_target_ms = 50;
        profile.error_rate_target_pct = 1.0;
        profile.window_minutes = 60;
        profile.short_burn_window_minutes = 5;
        profile.long_burn_window_minutes = 60;
        profile.critical_burn_threshold = 10.0;
        profile.high_burn_threshold = 5.0;
        profile.moderate_burn_threshold = 2.0;
        profile.version_u64 = 1;
        const Status profile_status =
            db_.catalog_manager()->upsertSloProfileCatalogEntry(profile, &ctx);
        if (profile_status != Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return {};
        }
        return profile;
    }

    void createSloBinding(const CatalogManager::SloProfileCatalogInfo& profile,
                          const CatalogManager::NodeCatalogInfo& node)
    {
        ErrorContext ctx;
        CatalogManager::SloBindingCatalogInfo binding{};
        binding.slo_binding_id = generateUuidV7();
        binding.slo_profile_id = profile.slo_profile_id;
        binding.has_node_id = true;
        binding.node_id = node.node_id;
        binding.role = node.node_role;
        binding.priority_rank = 1;
        binding.effective_from_time = 1;
        binding.version_u64 = 1;
        ASSERT_EQ(db_.catalog_manager()->upsertSloBindingCatalogEntry(binding, &ctx), Status::OK)
            << ctx.message;
    }

    void createSloWindow(const CatalogManager::NodeCatalogInfo& node)
    {
        ErrorContext ctx;
        CatalogManager::SloWindowCatalogInfo window{};
        window.slo_window_id = generateUuidV7();
        window.node_id = node.node_id;
        window.role = node.node_role;
        window.window_start_time = 1000;
        window.window_end_time = 61000;
        window.request_count = 100;
        window.success_count = 80;
        window.error_count = 20;
        window.latency_p95_ms = 35;
        window.latency_p99_ms = 80;
        window.availability_sli_pct = 80.0;
        window.error_rate_sli_pct = 20.0;
        window.version_u64 = 1;
        ASSERT_EQ(db_.catalog_manager()->upsertSloWindowCatalogEntry(window, &ctx), Status::OK)
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
        const Status rule_status = db_.catalog_manager()->upsertAlertRuleCatalogEntry(rule, &ctx);
        if (rule_status != Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return {};
        }
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
        const Status target_status =
            db_.catalog_manager()->upsertAlertTargetCatalogEntry(target, &ctx);
        if (target_status != Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return {};
        }
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

    auto createAlertEvent(const CatalogManager::AlertRuleCatalogInfo& rule,
                          CatalogManager::AlertSeverity severity,
                          uint64_t event_time) -> CatalogManager::AlertEventCatalogInfo
    {
        ErrorContext ctx;
        CatalogManager::AlertEventCatalogInfo event{};
        event.event_id = generateUuidV7();
        event.rule_id = rule.rule_id;
        event.severity = severity;
        event.event_state = CatalogManager::AlertEventState::OPEN;
        event.event_time = event_time;
        const Status event_status = db_.catalog_manager()->upsertAlertEventCatalogEntry(event, &ctx);
        if (event_status != Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return {};
        }
        return event;
    }

    void createPageAuditFinding()
    {
        ErrorContext ctx;
        CatalogManager::PageAuditFindingCatalogInfo finding{};
        finding.finding_id = generateUuidV7();
        finding.scan_mode = "DIAGNOSTIC";
        finding.trigger_source = "SWEEP_BACKGROUND";
        finding.page_id = 42;
        finding.page_type = "heap";
        finding.error_code = "PAGE_CHECKSUM_FAIL";
        finding.severity = "CRITICAL";
        finding.details_json =
            "{\"endpoint\":\"https://ops.example/internal?token=abc\",\"password\":\"topsecret\"}";
        ASSERT_EQ(db_.catalog_manager()->appendPageAuditFindingCatalogEntry(finding, &ctx), Status::OK)
            << ctx.message;
    }

    void createShadowCaptureManifest(const ID& tx_uuid)
    {
        ErrorContext ctx;
        CatalogManager::ShadowCaptureManifestCatalogInfo manifest{};
        manifest.tx_uuid = tx_uuid;
        manifest.capture_scope = "TRANSACTION";
        manifest.capture_format = "LOGICAL_TX_SUMMARY";
        manifest.payload_manifest =
            "source_manifest_path=https://user:secret@ops.example/forensic?token=abc\n"
            "password=shadow-secret\n";
        manifest.has_retention_deadline_time = true;
        manifest.retention_deadline_time = 900000;
        ASSERT_EQ(db_.catalog_manager()->appendShadowCaptureManifestCatalogEntry(manifest, &ctx), Status::OK)
            << ctx.message;
    }

    Database db_;
    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<ConnectionContext> conn_;
    std::unique_ptr<Executor> executor_;
    ID public_schema_id_{};
    ID system_user_id_{};
    ID cluster_id_ = generateUuidV7();
};

TEST_F(OperationalSupportBundleTest, ShowsAlertDashboardReadinessAndSupportBundleSafetyThroughSqlSurface)
{
    const auto node = createNode("node_ops", CatalogManager::ClusterNodeRole::OLTP_DATA);
    const auto profile = createSloProfile("ops_profile", CatalogManager::ClusterNodeRole::OLTP_DATA);
    createSloBinding(profile, node);
    createSloWindow(node);

    const auto rule = createAlertRule("vuln_open_ssl", CatalogManager::AlertSeverity::CRITICAL);
    const auto target = createAlertTarget("ops_webhook");
    createAlertRoute(rule, target);
    const auto event = createAlertEvent(rule, CatalogManager::AlertSeverity::CRITICAL, 1000);
    createPageAuditFinding();
    createShadowCaptureManifest(event.event_id);

    auto dashboard = compileAndExecute("SHOW ALERT DASHBOARD WINDOW MINUTES 10");
    ASSERT_TRUE(dashboard.success()) << dashboard.error();
    ASSERT_TRUE(dashboard.hasResultSet());
    EXPECT_TRUE(resultSetHasRow(dashboard.resultSet(),
                                {{1, "vuln_open_ssl"},
                                 {2, "CRITICAL"},
                                 {3, "OPEN"},
                                 {9, "1"},
                                 {10, "1"}}));

    auto readiness = compileAndExecute("SHOW READINESS HEALTH WINDOW MINUTES 10");
    ASSERT_TRUE(readiness.success()) << readiness.error();
    ASSERT_TRUE(readiness.hasResultSet());
    ASSERT_EQ(readiness.resultSet()->rowCount(), 1u);
    EXPECT_EQ(readiness.resultSet()->getValue(0, 0).toString(), "BLOCKED");
    EXPECT_EQ(readiness.resultSet()->getValue(0, 1).toString(), "1");
    EXPECT_EQ(readiness.resultSet()->getValue(0, 6).toString(), "1");

    auto support = compileAndExecute("SHOW SUPPORT BUNDLE SAFETY WINDOW MINUTES 10");
    ASSERT_TRUE(support.success()) << support.error();
    ASSERT_TRUE(support.hasResultSet());
    ASSERT_EQ(support.resultSet()->rowCount(), 1u);
    EXPECT_EQ(support.resultSet()->getValue(0, 0).toString(), "BLOCKED");
    EXPECT_EQ(support.resultSet()->getValue(0, 1).toString(), "true");
    EXPECT_EQ(support.resultSet()->getValue(0, 3).toString(), "1");
    EXPECT_EQ(support.resultSet()->getValue(0, 6).toString(), "1");
    EXPECT_EQ(support.resultSet()->getValue(0, 7).toString(), "1");
    EXPECT_EQ(support.resultSet()->getValue(0, 10).toString(), "1");
    EXPECT_EQ(support.resultSet()->getValue(0, 11).toString(), "1");
}

TEST_F(OperationalSupportBundleTest, GeneratedSupportBundleRedactsSensitiveFieldsAndKeepsForensicReferences)
{
    const auto node = createNode("node_bundle", CatalogManager::ClusterNodeRole::OLTP_DATA);
    const auto profile = createSloProfile("bundle_profile", CatalogManager::ClusterNodeRole::OLTP_DATA);
    createSloBinding(profile, node);
    createSloWindow(node);

    const auto rule = createAlertRule("cpu_critical", CatalogManager::AlertSeverity::CRITICAL);
    const auto target = createAlertTarget("pager_primary");
    createAlertRoute(rule, target);
    const auto event = createAlertEvent(rule, CatalogManager::AlertSeverity::CRITICAL, 1000);
    createPageAuditFinding();
    createShadowCaptureManifest(event.event_id);

    const std::string output_path = db_file_->path() + ".support_bundle";
    SupportBundleBuilder builder(&db_);
    SupportBundleRequest request;
    request.output_path = output_path;
    request.readiness.now_time = 61000;

    SupportBundleResult result;
    ErrorContext ctx;
    ASSERT_EQ(builder.generateSupportBundle(request, result, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(result.safety.readiness.state, scratchbird::core::ReadinessHealthState::BLOCKED);
    EXPECT_TRUE(result.redaction_enforced);
    EXPECT_GT(result.redacted_field_count, 0u);
    EXPECT_EQ(result.alert_row_count, 1u);

    std::ifstream in(output_path);
    ASSERT_TRUE(in.is_open());
    const std::string contents((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());

    EXPECT_NE(contents.find("bundle_id="), std::string::npos);
    EXPECT_NE(contents.find("event_id=" + event.event_id.toString()), std::string::npos);
    EXPECT_NE(contents.find("manifest_id="), std::string::npos);
    EXPECT_NE(contents.find("finding_id="), std::string::npos);
    EXPECT_NE(contents.find("<redacted>"), std::string::npos);
    EXPECT_NE(contents.find("<endpoint>"), std::string::npos);
    EXPECT_EQ(contents.find("topsecret"), std::string::npos);
    EXPECT_EQ(contents.find("shadow-secret"), std::string::npos);
    EXPECT_EQ(contents.find("user:secret"), std::string::npos);

    std::remove(output_path.c_str());
}

} // namespace
