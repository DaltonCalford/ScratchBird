#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <string>
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
        conn_->setCurrentSchemaId(public_schema_id_);

        const ID system_user_id = db_.catalog_manager()->getSystemUserId(&ctx);
        conn_->setCurrentUser(system_user_id, true);

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

    Database db_{};
    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<ConnectionContext> conn_;
    std::unique_ptr<Executor> executor_;
    ID public_schema_id_{};
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
