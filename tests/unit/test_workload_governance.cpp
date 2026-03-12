#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <initializer_list>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/workload_governance.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

using scratchbird::core::CatalogManager;
using scratchbird::core::ConnectionContext;
using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::ID;
using scratchbird::core::Status;
using scratchbird::core::WorkloadGovernance;
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
        if (i > 0)
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

auto admissionStatusRowForPolicy(
    const std::vector<WorkloadGovernance::AdmissionStatusRow>& rows,
    const std::string& policy_name) -> std::vector<WorkloadGovernance::AdmissionStatusRow>::const_iterator
{
    return std::find_if(rows.begin(), rows.end(), [&](const auto& row) {
        return row.policy_name == policy_name;
    });
}

} // namespace

class WorkloadGovernanceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("workload_governance", ".sbdb");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        Status proc_status = db_.initializeProcArray(8, &ctx);
        if (proc_status != Status::OK && proc_status != Status::INVALID_ARGUMENT)
        {
            ASSERT_EQ(proc_status, Status::OK) << ctx.message;
        }

        ASSERT_EQ(db_.connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
        ASSERT_EQ(conn_->initialize(&ctx), Status::OK) << ctx.message;

        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(db_.catalog_manager()->getSchema("PUBLIC", schema_info, &ctx), Status::OK)
            << ctx.message;
        public_schema_id_ = schema_info.schema_id;
        initializeSystemConnection(conn_.get());

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

    auto makeDescriptor(ConnectionContext* connection,
                        std::string sql = "SELECT 1") const
        -> WorkloadGovernance::QueryDescriptor
    {
        WorkloadGovernance::QueryDescriptor descriptor;
        descriptor.connection = connection;
        descriptor.sql = std::move(sql);
        descriptor.database_name = "scratchbird";
        descriptor.schema_name = "PUBLIC";
        return descriptor;
    }

    auto createNode(const ID& cluster_id,
                    const std::string& node_name,
                    CatalogManager::ClusterNodeRole role,
                    uint16_t port = 7610) -> CatalogManager::NodeCatalogInfo
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
        EXPECT_EQ(db_.catalog_manager()->upsertNodeCatalogEntry(node, &ctx), Status::OK) << ctx.message;
        return node;
    }

    auto createSloProfile(const std::string& profile_name,
                          CatalogManager::ClusterNodeRole role,
                          double availability_target_pct,
                          double error_rate_target_pct,
                          uint32_t short_window_minutes = 5,
                          uint32_t long_window_minutes = 60,
                          double moderate_burn_threshold = 2.0,
                          double high_burn_threshold = 6.0,
                          double critical_burn_threshold = 14.0)
        -> CatalogManager::SloProfileCatalogInfo
    {
        ErrorContext ctx;
        CatalogManager::SloProfileCatalogInfo profile{};
        profile.slo_profile_id = generateUuidV7();
        profile.profile_name = profile_name;
        profile.role = role;
        profile.availability_target_pct = availability_target_pct;
        profile.latency_p95_target_ms = 25;
        profile.latency_p99_target_ms = 75;
        profile.error_rate_target_pct = error_rate_target_pct;
        profile.window_minutes = long_window_minutes;
        profile.short_burn_window_minutes = short_window_minutes;
        profile.long_burn_window_minutes = long_window_minutes;
        profile.moderate_burn_threshold = moderate_burn_threshold;
        profile.high_burn_threshold = high_burn_threshold;
        profile.critical_burn_threshold = critical_burn_threshold;
        profile.version_u64 = 1;
        EXPECT_EQ(db_.catalog_manager()->upsertSloProfileCatalogEntry(profile, &ctx), Status::OK)
            << ctx.message;
        return profile;
    }

    auto createSloBinding(const CatalogManager::SloProfileCatalogInfo& profile,
                          CatalogManager::ClusterNodeRole role,
                          uint64_t effective_from_time,
                          uint16_t priority_rank = 1,
                          const ID* node_id = nullptr) -> CatalogManager::SloBindingCatalogInfo
    {
        ErrorContext ctx;
        CatalogManager::SloBindingCatalogInfo binding{};
        binding.slo_binding_id = generateUuidV7();
        binding.slo_profile_id = profile.slo_profile_id;
        binding.role = role;
        binding.priority_rank = priority_rank;
        binding.effective_from_time = effective_from_time;
        binding.version_u64 = 1;
        if (node_id != nullptr)
        {
            binding.has_node_id = true;
            binding.node_id = *node_id;
        }
        EXPECT_EQ(db_.catalog_manager()->upsertSloBindingCatalogEntry(binding, &ctx), Status::OK)
            << ctx.message;
        return binding;
    }

    auto createSloWindow(const ID& node_id,
                         CatalogManager::ClusterNodeRole role,
                         uint64_t window_start_time,
                         uint64_t window_end_time,
                         uint64_t request_count,
                         uint64_t success_count,
                         uint64_t error_count,
                         uint32_t latency_p95_ms = 0,
                         uint32_t latency_p99_ms = 0) -> CatalogManager::SloWindowCatalogInfo
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
        EXPECT_EQ(db_.catalog_manager()->upsertSloWindowCatalogEntry(window, &ctx), Status::OK)
            << ctx.message;
        return window;
    }

    auto createAutoscalePolicy(CatalogManager::ClusterNodeRole role,
                               uint16_t min_nodes,
                               uint16_t max_nodes,
                               uint16_t scale_out_step = 1,
                               uint16_t scale_in_step = 1,
                               uint32_t scale_out_cooldown_ms = 30000,
                               uint32_t scale_in_cooldown_ms = 60000)
        -> CatalogManager::AutoscalePolicyCatalogInfo
    {
        ErrorContext ctx;
        CatalogManager::AutoscalePolicyCatalogInfo policy{};
        policy.autoscale_policy_id = generateUuidV7();
        policy.role = role;
        policy.min_nodes = min_nodes;
        policy.max_nodes = max_nodes;
        policy.scale_out_step = scale_out_step;
        policy.scale_in_step = scale_in_step;
        policy.scale_out_cooldown_ms = scale_out_cooldown_ms;
        policy.scale_in_cooldown_ms = scale_in_cooldown_ms;
        policy.cpu_scale_out_pct = 80;
        policy.queue_scale_out_pct = 70;
        policy.slo_burn_scale_out_threshold = 1.5;
        policy.slo_recovery_scale_in_threshold = 0.7;
        policy.version_u64 = 1;
        EXPECT_EQ(db_.catalog_manager()->upsertAutoscalePolicyCatalogEntry(policy, &ctx), Status::OK)
            << ctx.message;
        return policy;
    }

    auto createAdmissionPolicy(const std::string& policy_name,
                               uint32_t max_concurrent_queries,
                               uint32_t max_queue_depth,
                               uint32_t queue_timeout_ms,
                               CatalogManager::AdmissionRejectMode reject_mode =
                                   CatalogManager::AdmissionRejectMode::QUEUE)
        -> CatalogManager::AdmissionPolicyCatalogInfo
    {
        ErrorContext ctx;
        CatalogManager::AdmissionPolicyCatalogInfo policy{};
        policy.policy_id = generateUuidV7();
        policy.policy_name = policy_name;
        policy.max_concurrent_sessions = 8;
        policy.max_concurrent_queries = max_concurrent_queries;
        policy.max_queue_depth = max_queue_depth;
        policy.reject_mode = reject_mode;
        policy.queue_timeout_ms = queue_timeout_ms;
        policy.cpu_reject_pct = 80;
        policy.mem_reject_pct = 80;
        policy.io_reject_pct = 80;
        policy.is_enabled = true;
        EXPECT_EQ(db_.catalog_manager()->upsertAdmissionPolicyCatalogEntry(policy, &ctx), Status::OK)
            << ctx.message;
        return policy;
    }

    Database db_{};
    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<ConnectionContext> conn_;
    std::unique_ptr<Executor> executor_;
    ID public_schema_id_{};
    ID system_user_id_{};
};

TEST_F(WorkloadGovernanceTest, CompilesAndExecutesClusterGovernanceSql)
{
    auto class_result = compileAndExecute(
        "CREATE CLUSTER WORKLOAD CLASS wl_oltp 'MATCH=QUERY_TYPE:select;PRIORITY=5'");
    ASSERT_TRUE(class_result.success()) << class_result.error();

    auto route_result = compileAndExecute(
        "CREATE CLUSTER WORKLOAD ROUTE rt_router "
        "'CLASS=wl_oltp;TARGET_KIND=ROLE;TARGET_LABEL=router;ROLE=ROUTER;TRANSPORT=LOCAL;ROUTE_WEIGHT=10'");
    ASSERT_TRUE(route_result.success()) << route_result.error();

    auto policy_result = compileAndExecute(
        "CREATE CLUSTER ADMISSION POLICY ap_ingress "
        "'MAX_CONCURRENT_SESSIONS=8;MAX_CONCURRENT_QUERIES=1;MAX_QUEUE_DEPTH=1;QUEUE_TIMEOUT_MS=25;REJECT_MODE=REJECT'");
    ASSERT_TRUE(policy_result.success()) << policy_result.error();

    auto binding_result = compileAndExecute(
        "CREATE CLUSTER ADMISSION BINDING ab_ingress 'POLICY=ap_ingress;CLASS=wl_oltp;PRIORITY=1'");
    ASSERT_TRUE(binding_result.success()) << binding_result.error();

    auto routing_result = compileAndExecute("SHOW CLUSTER ROUTING PLAN");
    ASSERT_TRUE(routing_result.success()) << routing_result.error();
    ASSERT_TRUE(routing_result.hasResultSet());
    ASSERT_NE(routing_result.resultSet(), nullptr);
    EXPECT_TRUE(resultSetHasRow(routing_result.resultSet(),
                                {{0, "wl_oltp"}, {2, "rt_router"}, {3, "ROLE"}, {4, "router"}}));

    auto admission_result = compileAndExecute("SHOW CLUSTER ADMISSION STATUS");
    ASSERT_TRUE(admission_result.success()) << admission_result.error();
    ASSERT_TRUE(admission_result.hasResultSet());
    ASSERT_NE(admission_result.resultSet(), nullptr);
    EXPECT_TRUE(resultSetHasRow(admission_result.resultSet(),
                                {{1, "wl_oltp"}, {2, "ap_ingress"}, {3, "REJECT"}}));
}

TEST_F(WorkloadGovernanceTest, RejectsSecondConcurrentQueryAtConfiguredLimit)
{
    auto class_result = compileAndExecute(
        "CREATE CLUSTER WORKLOAD CLASS wl_select 'MATCH=QUERY_TYPE:select;PRIORITY=5'");
    ASSERT_TRUE(class_result.success()) << class_result.error();

    auto policy_result = compileAndExecute(
        "CREATE CLUSTER ADMISSION POLICY ap_limit "
        "'MAX_CONCURRENT_QUERIES=1;MAX_QUEUE_DEPTH=0;QUEUE_TIMEOUT_MS=0;REJECT_MODE=REJECT'");
    ASSERT_TRUE(policy_result.success()) << policy_result.error();

    auto binding_result = compileAndExecute(
        "CREATE CLUSTER ADMISSION BINDING ab_limit 'POLICY=ap_limit;CLASS=wl_select;PRIORITY=1'");
    ASSERT_TRUE(binding_result.success()) << binding_result.error();

    WorkloadGovernance::QueryDescriptor descriptor;
    descriptor.connection = conn_.get();
    descriptor.sql = "SELECT 1";
    descriptor.database_name = "scratchbird";
    descriptor.schema_name = "PUBLIC";

    WorkloadGovernance::AdmissionLease first_lease;
    ErrorContext ctx;
    auto first = db_.workload_governance()->acquire(descriptor, first_lease, &ctx);
    ASSERT_TRUE(first.admitted) << first.detail;
    ASSERT_TRUE(first_lease.active());
    EXPECT_EQ(first.class_name, "wl_select");
    EXPECT_EQ(first.policy_name, "ap_limit");

    std::vector<WorkloadGovernance::AdmissionStatusRow> rows;
    ASSERT_EQ(db_.workload_governance()->snapshotAdmissionStatus(rows, &ctx), Status::OK)
        << ctx.message;
    auto row_it = admissionStatusRowForPolicy(rows, "ap_limit");
    ASSERT_NE(row_it, rows.end());
    EXPECT_EQ(row_it->active_queries, 1u);

    ErrorContext reject_ctx;
    WorkloadGovernance::AdmissionLease second_lease;
    auto second = db_.workload_governance()->acquire(descriptor, second_lease, &reject_ctx);
    EXPECT_FALSE(second.admitted);
    EXPECT_FALSE(second.queued);
    EXPECT_FALSE(second_lease.active());
    EXPECT_EQ(second.status, Status::CONFIGURATION_LIMIT_EXCEEDED);
    EXPECT_EQ(second.code, "GOV_1502");
    EXPECT_EQ(second.detail, "Admission rejected by max_concurrent_queries");

    first_lease.release();

    rows.clear();
    ASSERT_EQ(db_.workload_governance()->snapshotAdmissionStatus(rows, &ctx), Status::OK)
        << ctx.message;
    row_it = admissionStatusRowForPolicy(rows, "ap_limit");
    ASSERT_NE(row_it, rows.end());
    EXPECT_EQ(row_it->active_queries, 0u);
}

TEST_F(WorkloadGovernanceTest, RejectsQueuedAdmissionPolicyWithoutPositiveQueueControls)
{
    auto class_result = compileAndExecute(
        "CREATE CLUSTER WORKLOAD CLASS wl_queue 'MATCH=QUERY_TYPE:select;PRIORITY=5'");
    ASSERT_TRUE(class_result.success()) << class_result.error();

    auto zero_depth_result = compileAndExecute(
        "CREATE CLUSTER ADMISSION POLICY ap_queue_bad_depth "
        "'MAX_CONCURRENT_QUERIES=1;MAX_QUEUE_DEPTH=0;QUEUE_TIMEOUT_MS=25;REJECT_MODE=QUEUE'");
    ASSERT_FALSE(zero_depth_result.success());
    EXPECT_EQ(zero_depth_result.error(),
              "Invalid MAX_QUEUE_DEPTH for queued admission policy");

    auto zero_timeout_result = compileAndExecute(
        "CREATE CLUSTER ADMISSION POLICY ap_queue_bad_timeout "
        "'MAX_CONCURRENT_QUERIES=1;MAX_QUEUE_DEPTH=1;QUEUE_TIMEOUT_MS=0;REJECT_MODE=QUEUE'");
    ASSERT_FALSE(zero_timeout_result.success());
    EXPECT_EQ(zero_timeout_result.error(),
              "Invalid QUEUE_TIMEOUT_MS for queued admission policy");
}

TEST_F(WorkloadGovernanceTest, SelectsHighestPriorityMetadataMatchAndBinding)
{
    conn_->setSessionVariable("APPLICATION_NAME", "etl-cli");
    conn_->setSessionVariable("RESOURCE_TAG", "batch");

    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER WORKLOAD CLASS wl_select "
                    "'MATCH=QUERY_TYPE:select;PRIORITY=1'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER WORKLOAD CLASS wl_app "
                    "'MATCH=CLIENT_APP:etl-cli;PRIORITY=5'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER WORKLOAD CLASS wl_tag "
                    "'MATCH=RESOURCE_TAG:batch;PRIORITY=9'")
                    .success());

    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER ADMISSION POLICY ap_select "
                    "'MAX_CONCURRENT_SESSIONS=8;MAX_CONCURRENT_QUERIES=2;MAX_QUEUE_DEPTH=0;QUEUE_TIMEOUT_MS=0;REJECT_MODE=REJECT'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER ADMISSION POLICY ap_app "
                    "'MAX_CONCURRENT_SESSIONS=8;MAX_CONCURRENT_QUERIES=2;MAX_QUEUE_DEPTH=0;QUEUE_TIMEOUT_MS=0;REJECT_MODE=REJECT'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER ADMISSION POLICY ap_tag "
                    "'MAX_CONCURRENT_SESSIONS=8;MAX_CONCURRENT_QUERIES=2;MAX_QUEUE_DEPTH=0;QUEUE_TIMEOUT_MS=0;REJECT_MODE=REJECT'")
                    .success());

    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER ADMISSION BINDING ab_select "
                    "'POLICY=ap_select;CLASS=wl_select;PRIORITY=3'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER ADMISSION BINDING ab_app "
                    "'POLICY=ap_app;CLASS=wl_app;PRIORITY=2'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER ADMISSION BINDING ab_tag "
                    "'POLICY=ap_tag;CLASS=wl_tag;PRIORITY=1'")
                    .success());

    WorkloadGovernance::AdmissionLease lease;
    ErrorContext ctx;
    auto decision = db_.workload_governance()->acquire(makeDescriptor(conn_.get()), lease, &ctx);
    ASSERT_TRUE(decision.admitted) << decision.detail;
    ASSERT_TRUE(lease.active());
    EXPECT_EQ(decision.class_name, "wl_tag");
    EXPECT_EQ(decision.policy_name, "ap_tag");
}

TEST_F(WorkloadGovernanceTest, AppliesClusterScopeBindingWithoutClassMatch)
{
    const std::string cluster_target_uuid = generateUuidV7().toString();

    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER ADMISSION POLICY ap_cluster "
                    "'MAX_CONCURRENT_SESSIONS=8;MAX_CONCURRENT_QUERIES=2;MAX_QUEUE_DEPTH=0;QUEUE_TIMEOUT_MS=0;REJECT_MODE=REJECT'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER ADMISSION BINDING ab_cluster "
                    "'POLICY=ap_cluster;TARGET_KIND=CLUSTER;TARGET_UUID=" + cluster_target_uuid +
                        ";PRIORITY=7'")
                    .success());

    WorkloadGovernance::AdmissionLease lease;
    ErrorContext ctx;
    auto decision = db_.workload_governance()->acquire(
        makeDescriptor(conn_.get(), "UPDATE scratchbird_test SET x = 1"), lease, &ctx);
    ASSERT_TRUE(decision.admitted) << decision.detail;
    ASSERT_TRUE(lease.active());
    EXPECT_TRUE(decision.class_name.empty());
    EXPECT_EQ(decision.policy_name, "ap_cluster");

    std::vector<WorkloadGovernance::AdmissionStatusRow> rows;
    ASSERT_EQ(db_.workload_governance()->snapshotAdmissionStatus(rows, &ctx), Status::OK)
        << ctx.message;
    auto row_it = admissionStatusRowForPolicy(rows, "ap_cluster");
    ASSERT_NE(row_it, rows.end());
    EXPECT_EQ(row_it->scope, "CLUSTER");
}

TEST_F(WorkloadGovernanceTest, QueuesAndAdmitsWaitingQueryAfterLeaseRelease)
{
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER WORKLOAD CLASS wl_queue "
                    "'MATCH=QUERY_TYPE:select;PRIORITY=5'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER ADMISSION POLICY ap_queue "
                    "'MAX_CONCURRENT_SESSIONS=8;MAX_CONCURRENT_QUERIES=1;MAX_QUEUE_DEPTH=1;QUEUE_TIMEOUT_MS=1000;REJECT_MODE=QUEUE'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER ADMISSION BINDING ab_queue "
                    "'POLICY=ap_queue;CLASS=wl_queue;PRIORITY=1'")
                    .success());

    // Make governance catalog entries visible before a second session acquires against them.
    commitConnection(conn_.get());

    auto second_conn = connectSystemSession();
    ASSERT_NE(second_conn, nullptr);

    WorkloadGovernance::AdmissionLease first_lease;
    ErrorContext first_ctx;
    auto first = db_.workload_governance()->acquire(makeDescriptor(conn_.get()), first_lease, &first_ctx);
    ASSERT_TRUE(first.admitted) << first.detail;
    ASSERT_TRUE(first_lease.active());

    WorkloadGovernance::AdmissionDecision second_decision;
    WorkloadGovernance::AdmissionLease second_lease;
    ErrorContext second_ctx;
    std::thread waiter([&]() {
        second_decision = db_.workload_governance()->acquire(
            makeDescriptor(second_conn.get()), second_lease, &second_ctx);
    });

    bool queued_seen = false;
    for (int attempt = 0; attempt < 50; ++attempt)
    {
        std::vector<WorkloadGovernance::AdmissionStatusRow> rows;
        ErrorContext snapshot_ctx;
        ASSERT_EQ(db_.workload_governance()->snapshotAdmissionStatus(rows, &snapshot_ctx), Status::OK)
            << snapshot_ctx.message;
        auto row_it = admissionStatusRowForPolicy(rows, "ap_queue");
        ASSERT_NE(row_it, rows.end());
        if (row_it->queued_queries == 1u)
        {
            queued_seen = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(queued_seen);

    first_lease.release();
    waiter.join();

    ASSERT_TRUE(second_decision.admitted) << second_decision.detail;
    EXPECT_TRUE(second_decision.queued);
    ASSERT_TRUE(second_lease.active());
}

TEST_F(WorkloadGovernanceTest, RejectsSecondSessionWhenSessionCapIsReached)
{
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER WORKLOAD CLASS wl_sessions "
                    "'MATCH=QUERY_TYPE:select;PRIORITY=5'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER ADMISSION POLICY ap_sessions "
                    "'MAX_CONCURRENT_SESSIONS=1;MAX_CONCURRENT_QUERIES=4;MAX_QUEUE_DEPTH=0;QUEUE_TIMEOUT_MS=0;REJECT_MODE=REJECT'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER ADMISSION BINDING ab_sessions "
                    "'POLICY=ap_sessions;CLASS=wl_sessions;PRIORITY=1'")
                    .success());

    // Make governance catalog entries visible before a second session acquires against them.
    commitConnection(conn_.get());

    auto second_conn = connectSystemSession();
    ASSERT_NE(second_conn, nullptr);

    WorkloadGovernance::AdmissionLease first_lease;
    ErrorContext first_ctx;
    auto first = db_.workload_governance()->acquire(makeDescriptor(conn_.get()), first_lease, &first_ctx);
    ASSERT_TRUE(first.admitted) << first.detail;
    ASSERT_TRUE(first_lease.active());

    WorkloadGovernance::AdmissionLease second_lease;
    ErrorContext second_ctx;
    auto second = db_.workload_governance()->acquire(
        makeDescriptor(second_conn.get()), second_lease, &second_ctx);
    EXPECT_FALSE(second.admitted);
    EXPECT_FALSE(second_lease.active());
    EXPECT_EQ(second.status, Status::TOO_MANY_CONNECTIONS);
    EXPECT_EQ(second.code, "GOV_1501");
    EXPECT_EQ(second.detail, "Admission rejected by max_concurrent_sessions");
}

TEST_F(WorkloadGovernanceTest, AltersAndDropsGovernanceObjectsThroughSqlSurface)
{
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER WORKLOAD CLASS wl_manage "
                    "'MATCH=QUERY_TYPE:select;PRIORITY=2'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER WORKLOAD ROUTE rt_manage "
                    "'CLASS=wl_manage;TARGET_KIND=ROLE;TARGET_LABEL=router_a;ROLE=ROUTER;TRANSPORT=LOCAL;ROUTE_WEIGHT=10'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER ADMISSION POLICY ap_manage "
                    "'MAX_CONCURRENT_SESSIONS=8;MAX_CONCURRENT_QUERIES=2;MAX_QUEUE_DEPTH=0;QUEUE_TIMEOUT_MS=0;REJECT_MODE=REJECT'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "CREATE CLUSTER ADMISSION BINDING ab_manage "
                    "'POLICY=ap_manage;CLASS=wl_manage;PRIORITY=1'")
                    .success());

    ASSERT_TRUE(compileAndExecute(
                    "ALTER CLUSTER WORKLOAD CLASS wl_manage "
                    "'MATCH=CLIENT_APP:rewrite-cli;PRIORITY=7'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "ALTER CLUSTER WORKLOAD ROUTE rt_manage "
                    "'CLASS=wl_manage;TARGET_KIND=ROLE;TARGET_LABEL=router_b;ROLE=ROUTER;TRANSPORT=LOCAL;ROUTE_WEIGHT=42'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "ALTER CLUSTER ADMISSION POLICY ap_manage "
                    "'MAX_CONCURRENT_SESSIONS=6;MAX_CONCURRENT_QUERIES=3;MAX_QUEUE_DEPTH=1;QUEUE_TIMEOUT_MS=50;REJECT_MODE=QUEUE'")
                    .success());
    ASSERT_TRUE(compileAndExecute(
                    "ALTER CLUSTER ADMISSION BINDING ab_manage "
                    "'POLICY=ap_manage;CLASS=wl_manage;PRIORITY=4;ENABLED=0'")
                    .success());

    auto routing_result = compileAndExecute("SHOW CLUSTER ROUTING PLAN");
    ASSERT_TRUE(routing_result.success()) << routing_result.error();
    ASSERT_TRUE(routing_result.hasResultSet());
    EXPECT_TRUE(resultSetHasRow(routing_result.resultSet(),
                                {{0, "wl_manage"}, {2, "rt_manage"}, {4, "router_b"}, {8, "42"}}));

    auto admission_result = compileAndExecute("SHOW CLUSTER ADMISSION STATUS");
    ASSERT_TRUE(admission_result.success()) << admission_result.error();
    ASSERT_TRUE(admission_result.hasResultSet());
    EXPECT_TRUE(resultSetHasRow(admission_result.resultSet(),
                                {{1, "wl_manage"}, {2, "ap_manage"}, {3, "QUEUE"}, {4, "4"}, {15, "false"}}));

    ASSERT_TRUE(compileAndExecute("DROP CLUSTER ADMISSION BINDING ab_manage").success());
    ASSERT_TRUE(compileAndExecute("DROP CLUSTER ADMISSION POLICY ap_manage").success());
    ASSERT_TRUE(compileAndExecute("DROP CLUSTER WORKLOAD ROUTE rt_manage").success());
    ASSERT_TRUE(compileAndExecute("DROP CLUSTER WORKLOAD CLASS wl_manage").success());
    ASSERT_TRUE(compileAndExecute("DROP CLUSTER IF EXISTS ADMISSION POLICY ap_manage").success());

    routing_result = compileAndExecute("SHOW CLUSTER ROUTING PLAN");
    ASSERT_TRUE(routing_result.success()) << routing_result.error();
    EXPECT_FALSE(resultSetHasRow(routing_result.resultSet(), {{0, "wl_manage"}}));

    admission_result = compileAndExecute("SHOW CLUSTER ADMISSION STATUS");
    ASSERT_TRUE(admission_result.success()) << admission_result.error();
    EXPECT_FALSE(resultSetHasRow(admission_result.resultSet(), {{2, "ap_manage"}}));
}

TEST_F(WorkloadGovernanceTest, ResolvesNodeSpecificSloBindingAndComputesBudgetStatus)
{
    ErrorContext ctx;
    const ID cluster_id = generateUuidV7();
    const auto node = createNode(cluster_id, "node-oltp-a", CatalogManager::ClusterNodeRole::OLTP_DATA);
    const auto default_profile = createSloProfile(
        "oltp_default",
        CatalogManager::ClusterNodeRole::OLTP_DATA,
        99.0,
        5.0);
    const auto specific_profile = createSloProfile(
        "oltp_node_specific",
        CatalogManager::ClusterNodeRole::OLTP_DATA,
        99.0,
        5.0);
    createSloBinding(default_profile,
                     CatalogManager::ClusterNodeRole::OLTP_DATA,
                     1000,
                     1,
                     nullptr);
    createSloBinding(specific_profile,
                     CatalogManager::ClusterNodeRole::OLTP_DATA,
                     1000,
                     10,
                     &node.node_id);
    createSloWindow(node.node_id,
                    CatalogManager::ClusterNodeRole::OLTP_DATA,
                    1000,
                    61000,
                    100,
                    96,
                    4,
                    17,
                    46);

    std::vector<WorkloadGovernance::SloStatusRow> slo_rows;
    ASSERT_EQ(db_.workload_governance()->snapshotSloStatus(slo_rows, 61000, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(slo_rows.size(), 1u);
    EXPECT_EQ(slo_rows.front().profile_name, "oltp_node_specific");
    EXPECT_TRUE(slo_rows.front().binding_present);
    EXPECT_TRUE(slo_rows.front().metrics_present);
    EXPECT_NEAR(slo_rows.front().availability_sli_pct, 96.0, 0.0001);
    EXPECT_NEAR(slo_rows.front().error_rate_sli_pct, 4.0, 0.0001);
    EXPECT_NEAR(slo_rows.front().long_burn_rate, 0.8, 0.0001);
    EXPECT_EQ(slo_rows.front().burn_severity, "NONE");

    std::vector<WorkloadGovernance::ErrorBudgetStatusRow> budget_rows;
    ASSERT_EQ(db_.workload_governance()->snapshotErrorBudgetStatus(budget_rows, 61000, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(budget_rows.size(), 1u);
    EXPECT_NEAR(budget_rows.front().allowed_bad_requests, 5.0, 0.0001);
    EXPECT_NEAR(budget_rows.front().observed_bad_requests, 4.0, 0.0001);
    EXPECT_NEAR(budget_rows.front().remaining_bad_requests, 1.0, 0.0001);
    EXPECT_NEAR(budget_rows.front().remaining_budget_pct, 20.0, 0.0001);
}

TEST_F(WorkloadGovernanceTest, ShowsSloBudgetAutoscaleAndTuningStateThroughSqlSurface)
{
    ErrorContext ctx;
    const ID cluster_id = generateUuidV7();
    const auto node = createNode(cluster_id, "node-oltp-sql", CatalogManager::ClusterNodeRole::OLTP_DATA);
    const auto profile = createSloProfile(
        "oltp_show",
        CatalogManager::ClusterNodeRole::OLTP_DATA,
        99.95,
        1.0);
    createSloBinding(profile,
                     CatalogManager::ClusterNodeRole::OLTP_DATA,
                     1000,
                     1,
                     &node.node_id);
    createSloWindow(node.node_id,
                    CatalogManager::ClusterNodeRole::OLTP_DATA,
                    1000,
                    61000,
                    100,
                    80,
                    20,
                    35,
                    80);
    createAdmissionPolicy("ap_slo_history", 100, 200, 500);
    createAutoscalePolicy(CatalogManager::ClusterNodeRole::OLTP_DATA, 2, 8, 2, 1, 30000, 60000);
    for (uint64_t sample_time = 56000; sample_time <= 60000; sample_time += 1000)
    {
        WorkloadGovernance::SloTelemetrySample sample{};
        sample.node_id = node.node_id;
        sample.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
        sample.sample_time = sample_time;
        sample.cpu_utilization_pct = 90;
        sample.queue_pressure_pct = 85;
        sample.current_node_count = 2;
        ASSERT_EQ(db_.workload_governance()->recordSloTelemetrySample(sample, &ctx), Status::OK)
            << ctx.message;
    }

    ASSERT_EQ(db_.workload_governance()->evaluateSloPolicies(61000, &ctx), Status::OK)
        << ctx.message;

    auto slo_result = compileAndExecute("SHOW SLO STATUS ROLE OLTP_DATA");
    ASSERT_TRUE(slo_result.success()) << slo_result.error();
    ASSERT_TRUE(slo_result.hasResultSet());
    EXPECT_TRUE(resultSetHasRow(slo_result.resultSet(),
                                {{0, "OLTP_DATA"},
                                 {1, "node-oltp-sql"},
                                 {2, "oltp_show"},
                                 {12, "CRITICAL"}}));

    auto budget_result = compileAndExecute("SHOW ERROR BUDGET STATUS ROLE OLTP_DATA");
    ASSERT_TRUE(budget_result.success()) << budget_result.error();
    ASSERT_TRUE(budget_result.hasResultSet());
    EXPECT_TRUE(resultSetHasRow(budget_result.resultSet(),
                                {{0, "OLTP_DATA"},
                                 {1, "node-oltp-sql"},
                                 {2, "oltp_show"},
                                 {9, "CRITICAL"}}));

    auto autoscale_result =
        compileAndExecute("SHOW AUTOSCALE ACTIONS ROLE OLTP_DATA WINDOW MINUTES 10");
    ASSERT_TRUE(autoscale_result.success()) << autoscale_result.error();
    ASSERT_TRUE(autoscale_result.hasResultSet());
    EXPECT_TRUE(resultSetHasRow(autoscale_result.resultSet(),
                                {{0, "OLTP_DATA"}, {1, "SCALE_OUT"}, {3, "2"}}));

    auto tuning_result =
        compileAndExecute("SHOW ADMISSION TUNING HISTORY ROLE OLTP_DATA WINDOW MINUTES 10");
    ASSERT_TRUE(tuning_result.success()) << tuning_result.error();
    ASSERT_TRUE(tuning_result.hasResultSet());
    EXPECT_TRUE(resultSetHasRow(tuning_result.resultSet(),
                                {{0, "OLTP_DATA"}, {1, "100"}, {2, "80"}, {7, "slo burn CRITICAL"}}));
}

TEST_F(WorkloadGovernanceTest, PersistsBurnEvidenceAndTightensAdmissionWithinBounds)
{
    ErrorContext ctx;
    const ID cluster_id = generateUuidV7();
    const auto node = createNode(cluster_id, "node-oltp-b", CatalogManager::ClusterNodeRole::OLTP_DATA);
    const auto profile = createSloProfile(
        "oltp_critical",
        CatalogManager::ClusterNodeRole::OLTP_DATA,
        99.95,
        1.0);
    createSloBinding(profile,
                     CatalogManager::ClusterNodeRole::OLTP_DATA,
                     1000,
                     1,
                     &node.node_id);
    createSloWindow(node.node_id,
                    CatalogManager::ClusterNodeRole::OLTP_DATA,
                    1000,
                    61000,
                    100,
                    80,
                    20,
                    35,
                    80);
    const auto policy = createAdmissionPolicy("ap_slo_govern", 100, 200, 500);

    ASSERT_EQ(db_.workload_governance()->evaluateSloPolicies(61000, &ctx), Status::OK)
        << ctx.message;

    std::vector<CatalogManager::SloBurnEventCatalogInfo> burn_rows;
    ASSERT_EQ(db_.catalog_manager()->listSloBurnEventCatalogEntries(node.node_id, burn_rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(burn_rows.size(), 1u);
    EXPECT_EQ(burn_rows.front().burn_severity, CatalogManager::SloBurnSeverity::CRITICAL);
    EXPECT_EQ(burn_rows.front().action_plan, CatalogManager::SloActionPlan::INCIDENT_PAGE);

    CatalogManager::AdmissionPolicyCatalogInfo policy_out{};
    ASSERT_EQ(db_.catalog_manager()->getAdmissionPolicyCatalogEntry(policy.policy_id, policy_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(policy_out.max_concurrent_queries, 80u);
    EXPECT_EQ(policy_out.max_queue_depth, 160u);
    EXPECT_EQ(policy_out.queue_timeout_ms, 400u);

    std::vector<CatalogManager::AdmissionTuningEventCatalogInfo> tuning_rows;
    ASSERT_EQ(db_.catalog_manager()->listAdmissionTuningEventCatalogEntries(
                  CatalogManager::ClusterNodeRole::OLTP_DATA,
                  tuning_rows,
                  &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(tuning_rows.size(), 1u);
    EXPECT_EQ(tuning_rows.front().old_max_concurrent_queries, 100u);
    EXPECT_EQ(tuning_rows.front().new_max_concurrent_queries, 80u);
    EXPECT_EQ(tuning_rows.front().reason, "slo burn CRITICAL");
}

TEST_F(WorkloadGovernanceTest, TriggersScaleOutAndCooldownThenScaleInFromRecordedTelemetry)
{
    ErrorContext ctx;
    const ID cluster_id = generateUuidV7();
    const auto node = createNode(cluster_id, "node-oltp-c", CatalogManager::ClusterNodeRole::OLTP_DATA);
    const auto profile = createSloProfile(
        "oltp_autoscale",
        CatalogManager::ClusterNodeRole::OLTP_DATA,
        99.95,
        1.0,
        1,
        1);
    createSloBinding(profile,
                     CatalogManager::ClusterNodeRole::OLTP_DATA,
                     1000,
                     1,
                     &node.node_id);
    createAutoscalePolicy(CatalogManager::ClusterNodeRole::OLTP_DATA, 2, 8, 2, 1, 30000, 60000);

    createSloWindow(node.node_id,
                    CatalogManager::ClusterNodeRole::OLTP_DATA,
                    1,
                    60000,
                    100,
                    80,
                    20,
                    40,
                    90);
    for (uint64_t sample_time = 56000; sample_time <= 60000; sample_time += 1000)
    {
        WorkloadGovernance::SloTelemetrySample sample{};
        sample.node_id = node.node_id;
        sample.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
        sample.sample_time = sample_time;
        sample.cpu_utilization_pct = 90;
        sample.queue_pressure_pct = 85;
        sample.current_node_count = 2;
        ASSERT_EQ(db_.workload_governance()->recordSloTelemetrySample(sample, &ctx), Status::OK)
            << ctx.message;
    }

    ASSERT_EQ(db_.workload_governance()->evaluateSloPolicies(60000, &ctx), Status::OK)
        << ctx.message;

    std::vector<CatalogManager::AutoscaleActionCatalogInfo> action_rows;
    ASSERT_EQ(db_.catalog_manager()->listAutoscaleActionCatalogEntries(
                  CatalogManager::ClusterNodeRole::OLTP_DATA,
                  action_rows,
                  &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(action_rows.size(), 1u);
    EXPECT_EQ(action_rows.front().action_kind, CatalogManager::AutoscaleActionKind::SCALE_OUT);
    EXPECT_EQ(action_rows.front().applied_count_delta, 2);

    createSloWindow(node.node_id,
                    CatalogManager::ClusterNodeRole::OLTP_DATA,
                    60001,
                    65000,
                    100,
                    80,
                    20,
                    40,
                    90);
    for (uint64_t sample_time = 61000; sample_time <= 65000; sample_time += 1000)
    {
        WorkloadGovernance::SloTelemetrySample sample{};
        sample.node_id = node.node_id;
        sample.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
        sample.sample_time = sample_time;
        sample.cpu_utilization_pct = 90;
        sample.queue_pressure_pct = 85;
        sample.current_node_count = 4;
        ASSERT_EQ(db_.workload_governance()->recordSloTelemetrySample(sample, &ctx), Status::OK)
            << ctx.message;
    }

    ASSERT_EQ(db_.workload_governance()->evaluateSloPolicies(65000, &ctx), Status::OK)
        << ctx.message;
    action_rows.clear();
    ASSERT_EQ(db_.catalog_manager()->listAutoscaleActionCatalogEntries(
                  CatalogManager::ClusterNodeRole::OLTP_DATA,
                  action_rows,
                  &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(action_rows.size(), 1u);

    uint64_t window_start = 120000;
    for (int i = 0; i < 24; ++i)
    {
        createSloWindow(node.node_id,
                        CatalogManager::ClusterNodeRole::OLTP_DATA,
                        window_start,
                        window_start + 60000,
                        100,
                        100,
                        0,
                        10,
                        20);
        window_start += 60000;
    }
    uint64_t telemetry_time = 1560000;
    for (int i = 0; i < 24; ++i)
    {
        WorkloadGovernance::SloTelemetrySample sample{};
        sample.node_id = node.node_id;
        sample.role = CatalogManager::ClusterNodeRole::OLTP_DATA;
        sample.sample_time = telemetry_time;
        sample.cpu_utilization_pct = 20;
        sample.queue_pressure_pct = 10;
        sample.current_node_count = 4;
        ASSERT_EQ(db_.workload_governance()->recordSloTelemetrySample(sample, &ctx), Status::OK)
            << ctx.message;
        telemetry_time += 1000;
    }

    ASSERT_EQ(db_.workload_governance()->evaluateSloPolicies(telemetry_time - 1000, &ctx), Status::OK)
        << ctx.message;
    action_rows.clear();
    ASSERT_EQ(db_.catalog_manager()->listAutoscaleActionCatalogEntries(
                  CatalogManager::ClusterNodeRole::OLTP_DATA,
                  action_rows,
                  &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(action_rows.size(), 2u);
    EXPECT_EQ(action_rows.back().action_kind, CatalogManager::AutoscaleActionKind::SCALE_IN);
    EXPECT_EQ(action_rows.back().applied_count_delta, -1);
}
