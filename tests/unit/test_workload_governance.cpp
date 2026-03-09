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
