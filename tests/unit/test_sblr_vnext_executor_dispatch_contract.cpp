#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "test_helpers.h"

using scratchbird::core::CatalogManager;
using scratchbird::core::ConnectionContext;
using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::ID;
using scratchbird::core::Status;
using scratchbird::sblr::ExecutionResult;
using scratchbird::sblr::Executor;
using scratchbird::sblr::v3::Buffer;
using scratchbird::sblr::v3::Container;
using scratchbird::sblr::v3::DecodeError;
using scratchbird::sblr::v3::Instruction;
using scratchbird::sblr::v3::Opcode;
using scratchbird::sblr::v3::Value;

namespace
{
auto metricCounterValue(const std::string& metric_name,
                        const std::vector<std::string>& labels) -> double
{
    auto* metric = scratchbird::core::MetricsRegistry::getInstance().get(metric_name);
    if (metric == nullptr)
    {
        return 0.0;
    }
    auto* counter = dynamic_cast<scratchbird::core::Counter*>(metric);
    if (counter == nullptr)
    {
        return 0.0;
    }
    return counter->get(labels);
}

auto makeContainerFromStream(const std::vector<uint8_t> &stream) -> std::vector<uint8_t>
{
    Container c;
    std::memcpy(c.header.magic, "SBL3", 4);
    c.header.version_major = 3;
    c.header.version_minor = 0;
    c.header.version_patch = 0;
    c.header.flags = 0;
    c.header.timestamp_utc = 0;
    std::memset(c.header.module_id, 0, sizeof(c.header.module_id));

    c.metadata.module_name = "vnext_dispatch_contract";
    c.metadata.module_version = "1";
    c.metadata.dialect_id = 0;
    c.metadata.target_platform = 0;
    c.bytecode_stream = stream;

    std::vector<uint8_t> encoded;
    std::string err;
    EXPECT_TRUE(scratchbird::sblr::v3::encodeContainer(c, encoded, err)) << err;
    return encoded;
}

auto makeContainerWithInstruction(uint16_t opcode, Value::Object payload) -> std::vector<uint8_t>
{
    Buffer stream;
    DecodeError err;

    Instruction version;
    version.opcode = static_cast<uint16_t>(Opcode::SBLR3_VERSION);
    version.flags = 0;
    version.payload = Value(Value::Bytes{0x03, 0x00, 0x00, 0x00, 0x00, 0x00});
    EXPECT_TRUE(scratchbird::sblr::v3::encodeInstructionWithSchema(version, stream, err))
        << err.message;

    Instruction stmt;
    stmt.opcode = opcode;
    stmt.flags = 0;
    stmt.payload = Value(std::move(payload));
    EXPECT_TRUE(scratchbird::sblr::v3::encodeInstructionWithSchema(stmt, stream, err))
        << err.message;

    Instruction end;
    end.opcode = static_cast<uint16_t>(Opcode::SBLR3_END);
    end.flags = 0;
    end.payload = Value(Value::Bytes{});
    EXPECT_TRUE(scratchbird::sblr::v3::encodeInstructionWithSchema(end, stream, err))
        << err.message;

    return makeContainerFromStream(stream);
}
} // namespace

class SBLRVNextExecutorDispatchContractTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_sblr_vnext_executor_dispatch", ".db");
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, 8192, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK) << ctx.message;

        Status proc_status = db_->initializeProcArray(8, &ctx);
        if (proc_status != Status::OK && proc_status != Status::INVALID_ARGUMENT)
        {
            ASSERT_EQ(proc_status, Status::OK) << ctx.message;
        }

        ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_ctx_.get());
        ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK) << ctx.message;

        ID system_user_id = db_->catalog_manager()->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user_id, true);

        std::vector<CatalogManager::SchemaInfo> schemas;
        ASSERT_EQ(db_->catalog_manager()->listSchemas(schemas, &ctx), Status::OK) << ctx.message;
        ASSERT_FALSE(schemas.empty());
        default_schema_id_ = schemas.front().schema_id;
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        db_.reset();
        std::filesystem::remove(test_db_path_);
    }

    auto executeContainer(const std::vector<uint8_t> &container_bytes) -> ExecutionResult
    {
        Executor executor(db_.get());
        executor.setConnectionContext(conn_ctx_.get());
        executor.setCurrentSchema(default_schema_id_);
        return executor.execute(container_bytes);
    }

    auto executeVNext(uint16_t opcode, Value::Object payload) -> ExecutionResult
    {
        return executeContainer(makeContainerWithInstruction(opcode, std::move(payload)));
    }

    std::string test_db_path_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<ConnectionContext> conn_ctx_;
    ID default_schema_id_{};
};

TEST_F(SBLRVNextExecutorDispatchContractTest, KnownVNextOpcodesRejectWithDeterministicBridgeCode)
{
    const std::string metric = "scratchbird_vnext_executor_events_total";
    const double reject_before =
        metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "IRX_0406"});

    struct DispatchCase
    {
        uint16_t opcode;
        const char *symbol;
        Value::Object payload;
    };

    const std::array<DispatchCase, 16> cases = {{
        {static_cast<uint16_t>(Opcode::SBLR3_OP_DOC_PATH_FILTER),
         "SBLR3_OP_DOC_PATH_FILTER",
         Value::Object{{"path_id", Value(static_cast<uint64_t>(11))},
                       {"cmp", Value(static_cast<uint64_t>(0))},
                       {"value_ref", Value(static_cast<uint64_t>(14))}}},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_TS_BUCKET_AGG),
         "SBLR3_OP_TS_BUCKET_AGG",
         Value::Object{{"bucket_ns", Value(static_cast<uint64_t>(120000000000ULL))},
                       {"agg_count", Value(static_cast<uint64_t>(2))},
                       {"agg_refs", Value(Value::Bytes{0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00})}}},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_COL_SCAN),
         "SBLR3_OP_COL_SCAN",
         Value::Object{{"table_id", Value(static_cast<uint64_t>(22))},
                       {"proj_bitmap", Value(Value::Bytes{0x0F})},
                       {"predicate_bitmap", Value(Value::Bytes{0xAA, 0xBB})}}},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_SEARCH_DSL_EVAL),
         "SBLR3_OP_SEARCH_DSL_EVAL",
         Value::Object{{"dsl_blob_ref", Value(static_cast<uint64_t>(5001))},
                       {"scorer_id", Value(static_cast<uint64_t>(1))}}},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_VECTOR_ANN),
         "SBLR3_OP_VECTOR_ANN",
         Value::Object{{"index_id", Value(static_cast<uint64_t>(99))},
                       {"metric", Value(static_cast<uint64_t>(2))},
                       {"topk", Value(static_cast<uint64_t>(10))},
                       {"ef", Value(static_cast<uint64_t>(64))}}},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_EXCHANGE),
         "SBLR3_OP_HYBRID_BRIDGE_EXCHANGE",
         Value::Object{{"src_track", Value(static_cast<uint64_t>(1))},
                       {"dst_track", Value(static_cast<uint64_t>(2))},
                       {"mode", Value(static_cast<uint64_t>(3))}}},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_MATERIALIZE),
         "SBLR3_OP_HYBRID_BRIDGE_MATERIALIZE",
         Value::Object{{"buffer_class", Value(static_cast<uint64_t>(2))},
                       {"row_shape_ref", Value(static_cast<uint64_t>(77))}}},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_COMPILE_DISPATCH),
         "SBLR3_OP_UDR_COMPILE_DISPATCH",
         Value::Object{{"validate_only", Value(false)},
                       {"profile_id", Value(std::string("native"))},
                       {"payload_format", Value(std::string("SQL_TEXT"))},
                       {"payload_bytes", Value(std::string("payload_1"))},
                       {"session_signature", Value(std::string("sig_1"))}}},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_EMBEDDED_SQL_COMPILE),
         "SBLR3_OP_UDR_EMBEDDED_SQL_COMPILE",
         Value::Object{{"validate_only", Value(true)},
                       {"template_id", Value(std::string("tpl_1"))},
                       {"sql_text", Value(std::string("SELECT 1"))},
                       {"profile_id", Value(std::string("native"))},
                       {"session_signature", Value(std::string("sig_2"))}}},
        {static_cast<uint16_t>(Opcode::SBLR3_SESSION_RESET),
         "SBLR3_SESSION_RESET",
         Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                       {"options", Value(Value::Object{{"scope", Value(std::string("session"))}})}}},
        {static_cast<uint16_t>(Opcode::SBLR3_CREATE_DATABASE_EMULATED),
         "SBLR3_CREATE_DATABASE_EMULATED",
         Value::Object{{"flags", Value(static_cast<uint64_t>(0))},
                       {"name", Value(std::string("emulated_db"))},
                       {"encrypted", Value(false)},
                       {"options", Value(Value::Object{{"engine", Value(std::string("postgresql"))}})}}},
        {static_cast<uint16_t>(Opcode::SBLR3_IDX_SET_OPTIONS),
         "SBLR3_IDX_SET_OPTIONS",
         Value::Object{{"index", Value(Value::List{Value(std::string("users")),
                                                   Value(std::string("public")),
                                                   Value(std::string("idx_users_name"))})},
                       {"action", Value(static_cast<uint64_t>(1))},
                       {"options", Value(Value::Object{{"fillfactor", Value(static_cast<uint64_t>(90))}})}}},
        {static_cast<uint16_t>(Opcode::SBLR3_CQL_KEYSPACE),
         "SBLR3_CQL_KEYSPACE",
         Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                       {"namespace_path", Value(Value::List{Value(std::string("users")),
                                                           Value(std::string("cassandra"))})},
                       {"options", Value(Value::Object{{"replication", Value(std::string("simple"))}})}}},
        {static_cast<uint16_t>(Opcode::SBLR3_CLUSTER_WORKLOAD_CLASS),
         "SBLR3_CLUSTER_WORKLOAD_CLASS",
         Value::Object{{"action", Value(static_cast<uint64_t>(2))},
                       {"object_name", Value(std::string("oltp_default"))},
                       {"options", Value(Value::Object{{"priority", Value(static_cast<uint64_t>(5))}})}}},
        {static_cast<uint16_t>(Opcode::SBLR3_SECURITY_ENCRYPTION_PROFILE),
         "SBLR3_SECURITY_ENCRYPTION_PROFILE",
         Value::Object{{"action", Value(static_cast<uint64_t>(3))},
                       {"object_name", Value(std::string("default_profile"))},
                       {"options", Value(Value::Object{{"cipher", Value(std::string("aes-256-gcm"))}})}}},
        {static_cast<uint16_t>(Opcode::SBLR3_SERVICE_CHANNEL_BACKUP),
         "SBLR3_SERVICE_CHANNEL_BACKUP",
         Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                       {"object_name", Value(std::string("backup"))},
                       {"options", Value(Value::Object{{"mode", Value(std::string("full"))}})}}},
    }};

    for (const auto &entry : cases)
    {
        ExecutionResult result = executeVNext(entry.opcode, entry.payload);
        EXPECT_FALSE(result.success()) << "expected deterministic reject for " << entry.symbol;
        EXPECT_NE(result.error().find("IRX_0406"), std::string::npos) << result.error();
        EXPECT_NE(result.error().find(entry.symbol), std::string::npos) << result.error();
        EXPECT_EQ(result.error().find("V3 opcode not implemented in executor"), std::string::npos)
            << result.error();
    }

    EXPECT_EQ(reject_before + static_cast<double>(cases.size()),
              metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "IRX_0406"}));
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       CanonicalOpcodeWithoutDirectDispatchRejectsWithDeterministicBridgeCode)
{
    const std::string metric = "scratchbird_vnext_executor_events_total";
    const double reject_before =
        metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "IRX_0406"});

    ExecutionResult result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_CREATE_DATABASE),
        Value::Object{{"flags", Value(static_cast<uint64_t>(0))},
                      {"name", Value(std::string("native_db"))},
                      {"encrypted", Value(false)},
                      {"options", Value(Value::Object{{"engine", Value(std::string("native"))}})}});

    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("IRX_0406"), std::string::npos) << result.error();
    EXPECT_NE(result.error().find("SBLR3_CREATE_DATABASE"), std::string::npos) << result.error();
    EXPECT_EQ(result.error().find("V3 opcode not implemented in executor"), std::string::npos)
        << result.error();

    EXPECT_EQ(reject_before + 1.0,
              metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "IRX_0406"}));
}

TEST_F(SBLRVNextExecutorDispatchContractTest, BridgeOpcodeFamilyMatrixRejectsDeterministically)
{
    const std::string metric = "scratchbird_vnext_executor_events_total";
    const double reject_before =
        metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "IRX_0406"});

    auto makeDocPathPayload = []() -> Value::Object {
        return Value::Object{
            {"path_id", Value(static_cast<uint64_t>(11))},
            {"cmp", Value(static_cast<uint64_t>(0))},
            {"value_ref", Value(static_cast<uint64_t>(14))}};
    };
    auto makeTsBucketPayload = []() -> Value::Object {
        return Value::Object{
            {"bucket_ns", Value(static_cast<uint64_t>(120000000000ULL))},
            {"agg_count", Value(static_cast<uint64_t>(2))},
            {"agg_refs", Value(Value::Bytes{0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00})}};
    };
    auto makeColScanPayload = []() -> Value::Object {
        return Value::Object{
            {"table_id", Value(static_cast<uint64_t>(22))},
            {"proj_bitmap", Value(Value::Bytes{0x0F})},
            {"predicate_bitmap", Value(Value::Bytes{0xAA, 0xBB})}};
    };
    auto makeSearchDslPayload = []() -> Value::Object {
        return Value::Object{
            {"dsl_blob_ref", Value(static_cast<uint64_t>(5001))},
            {"scorer_id", Value(static_cast<uint64_t>(1))}};
    };
    auto makeVectorAnnPayload = []() -> Value::Object {
        return Value::Object{
            {"index_id", Value(static_cast<uint64_t>(99))},
            {"metric", Value(static_cast<uint64_t>(2))},
            {"topk", Value(static_cast<uint64_t>(10))},
            {"ef", Value(static_cast<uint64_t>(64))}};
    };
    auto makeHybridExchangePayload = []() -> Value::Object {
        return Value::Object{
            {"src_track", Value(static_cast<uint64_t>(1))},
            {"dst_track", Value(static_cast<uint64_t>(2))},
            {"mode", Value(static_cast<uint64_t>(3))}};
    };
    auto makeHybridMaterializePayload = []() -> Value::Object {
        return Value::Object{
            {"buffer_class", Value(static_cast<uint64_t>(2))},
            {"row_shape_ref", Value(static_cast<uint64_t>(77))}};
    };
    auto makeCreateDatabasePayload = []() -> Value::Object {
        return Value::Object{
            {"flags", Value(static_cast<uint64_t>(0))},
            {"name", Value(std::string("emulated_db"))},
            {"encrypted", Value(false)},
            {"options", Value(Value::Object{{"engine", Value(std::string("postgresql"))}})}};
    };
    auto makeCreateDomainPayload = []() -> Value::Object {
        const auto type_spec = scratchbird::sblr::v3::TypeSpec{
            static_cast<uint16_t>(Opcode::SBLR3_TYPE_INTEGER),
            Value::Bytes{}};
        return Value::Object{
            {"flags", Value(static_cast<uint64_t>(0))},
            {"path", Value(Value::List{Value(std::string("users")), Value(std::string("public"))})},
            {"name", Value(std::string("dom_bridge"))},
            {"type", Value(type_spec)},
            {"domain_kind", Value(static_cast<uint64_t>(0))},
            {"constraints", Value(Value::List{})}};
    };
    auto makeIndexBridgePayload = []() -> Value::Object {
        return Value::Object{
            {"index", Value(Value::List{Value(std::string("users")),
                                        Value(std::string("public")),
                                        Value(std::string("idx_users_name"))})},
            {"action", Value(static_cast<uint64_t>(1))},
            {"options", Value(Value::Object{{"fillfactor", Value(static_cast<uint64_t>(90))}})}};
    };
    auto makeControlPayload = []() -> Value::Object {
        return Value::Object{
            {"action", Value(static_cast<uint64_t>(1))},
            {"object_name", Value(std::string("control_object"))},
            {"options", Value(Value::Object{{"scope", Value(std::string("session"))}})}};
    };
    auto makeMultiModelPayload = []() -> Value::Object {
        return Value::Object{
            {"action", Value(static_cast<uint64_t>(1))},
            {"namespace_path", Value(Value::List{Value(std::string("users")),
                                                Value(std::string("remote"))})},
            {"options", Value(Value::Object{{"mode", Value(std::string("compat"))}})}};
    };

    auto assertBridgeReject = [&](Opcode opcode, Value::Object payload) {
        const char* symbol_c = scratchbird::sblr::v3::opcodeName(static_cast<uint16_t>(opcode));
        const std::string symbol = symbol_c ? symbol_c : "UNKNOWN";
        ExecutionResult result = executeVNext(static_cast<uint16_t>(opcode), std::move(payload));
        EXPECT_FALSE(result.success()) << "expected deterministic reject for " << symbol;
        EXPECT_NE(result.error().find("IRX_0406"), std::string::npos) << result.error();
        EXPECT_NE(result.error().find(symbol), std::string::npos) << result.error();
        EXPECT_EQ(result.error().find("V3 opcode not implemented in executor"), std::string::npos)
            << result.error();
    };

    const std::array<Opcode, 7> bridge_expr = {{
        Opcode::SBLR3_OP_DOC_PATH_FILTER,
        Opcode::SBLR3_OP_TS_BUCKET_AGG,
        Opcode::SBLR3_OP_COL_SCAN,
        Opcode::SBLR3_OP_SEARCH_DSL_EVAL,
        Opcode::SBLR3_OP_VECTOR_ANN,
        Opcode::SBLR3_OP_HYBRID_BRIDGE_EXCHANGE,
        Opcode::SBLR3_OP_HYBRID_BRIDGE_MATERIALIZE,
    }};

    const std::array<Opcode, 1> bridge_create_db = {{
        Opcode::SBLR3_CREATE_DATABASE_EMULATED,
    }};

    const std::array<Opcode, 4> bridge_domain = {{
        Opcode::SBLR3_CREATE_DOMAIN_RECORD,
        Opcode::SBLR3_CREATE_DOMAIN_ENUM,
        Opcode::SBLR3_CREATE_DOMAIN_SET,
        Opcode::SBLR3_CREATE_DOMAIN_RANGE,
    }};

    const std::array<Opcode, 7> bridge_index = {{
        Opcode::SBLR3_IDX_SET_OPTIONS,
        Opcode::SBLR3_IDX_RESET_OPTIONS,
        Opcode::SBLR3_IDX_RELOCATE,
        Opcode::SBLR3_IDX_DEFAULTS_SET,
        Opcode::SBLR3_IDX_DEFAULTS_RESET,
        Opcode::SBLR3_IDX_SHOW_HEALTH,
        Opcode::SBLR3_IDX_SHOW_CONTENTION,
    }};

    const std::array<Opcode, 18> bridge_control_admin = {{
        Opcode::SBLR3_SESSION_RESET,
        Opcode::SBLR3_CONFIG_RESET,
        Opcode::SBLR3_CONFIG_HISTORY,
        Opcode::SBLR3_CONFIG_RELOAD,
        Opcode::SBLR3_CONFIG_RESOURCE_BUNDLES_SHOW,
        Opcode::SBLR3_CONFIG_RESOURCE_BUNDLE_VALIDATE,
        Opcode::SBLR3_CONFIG_RESOURCE_BUNDLE_ACTIVATE,
        Opcode::SBLR3_TEXTSEARCH_CREATE_DICTIONARY,
        Opcode::SBLR3_TEXTSEARCH_ALTER_DICTIONARY,
        Opcode::SBLR3_TEXTSEARCH_DROP_DICTIONARY,
        Opcode::SBLR3_TEXTSEARCH_CREATE_CONFIGURATION,
        Opcode::SBLR3_TEXTSEARCH_ALTER_CONFIGURATION,
        Opcode::SBLR3_TEXTSEARCH_DROP_CONFIGURATION,
        Opcode::SBLR3_TEXTSEARCH_LOAD_DICTIONARY_DATA,
        Opcode::SBLR3_ADMIN_BACKUP,
        Opcode::SBLR3_ADMIN_RESTORE,
        Opcode::SBLR3_ADMIN_VALIDATE,
        Opcode::SBLR3_ADMIN_VACUUM_ALIAS,
    }};

    const std::array<Opcode, 27> bridge_multi_model = {{
        Opcode::SBLR3_CQL_KEYSPACE,
        Opcode::SBLR3_CQL_BATCH,
        Opcode::SBLR3_CQL_TTL,
        Opcode::SBLR3_CQL_WRITETIME,
        Opcode::SBLR3_MONGO_FIND,
        Opcode::SBLR3_MONGO_AGGREGATE,
        Opcode::SBLR3_MONGO_FIND_AND_MODIFY,
        Opcode::SBLR3_MONGO_BULK_WRITE,
        Opcode::SBLR3_CYPHER_MATCH,
        Opcode::SBLR3_CYPHER_MERGE,
        Opcode::SBLR3_CYPHER_UNWIND,
        Opcode::SBLR3_CYPHER_CALL,
        Opcode::SBLR3_REDIS_STRING,
        Opcode::SBLR3_REDIS_HASH,
        Opcode::SBLR3_REDIS_LIST,
        Opcode::SBLR3_REDIS_SET,
        Opcode::SBLR3_REDIS_ZSET,
        Opcode::SBLR3_REDIS_STREAM,
        Opcode::SBLR3_REDIS_PUBSUB,
        Opcode::SBLR3_MILVUS_CREATE_COLLECTION,
        Opcode::SBLR3_MILVUS_DROP_COLLECTION,
        Opcode::SBLR3_MILVUS_CREATE_INDEX,
        Opcode::SBLR3_MILVUS_DROP_INDEX,
        Opcode::SBLR3_MILVUS_INSERT,
        Opcode::SBLR3_MILVUS_DELETE,
        Opcode::SBLR3_MILVUS_SEARCH,
        Opcode::SBLR3_MILVUS_QUERY,
    }};

    const std::array<Opcode, 38> bridge_control_cluster_security = {{
        Opcode::SBLR3_CLUSTER_WORKLOAD_CLASS,
        Opcode::SBLR3_CLUSTER_WORKLOAD_ROUTE,
        Opcode::SBLR3_CLUSTER_ADMISSION_POLICY,
        Opcode::SBLR3_CLUSTER_ADMISSION_BINDING,
        Opcode::SBLR3_CLUSTER_SET_STATE,
        Opcode::SBLR3_CLUSTER_SHOW_STATE,
        Opcode::SBLR3_CLUSTER_SHOW_ROUTING_PLAN,
        Opcode::SBLR3_CLUSTER_SHOW_ADMISSION_STATUS,
        Opcode::SBLR3_ALERT_RULE_DDL,
        Opcode::SBLR3_ALERT_TARGET_DDL,
        Opcode::SBLR3_ALERT_ROUTE_DDL,
        Opcode::SBLR3_ALERT_SILENCE_DDL,
        Opcode::SBLR3_ALERT_ACK,
        Opcode::SBLR3_ALERT_SHOW,
        Opcode::SBLR3_HEALING_POLICY_DDL,
        Opcode::SBLR3_HEALING_ACTION_DDL,
        Opcode::SBLR3_HEALING_RUN,
        Opcode::SBLR3_HEALING_SHOW_RUNS,
        Opcode::SBLR3_JOB_TYPE_DDL,
        Opcode::SBLR3_JOB_TYPE_PARAM_SET,
        Opcode::SBLR3_SHARD_POLICY_DDL,
        Opcode::SBLR3_SHARD_DDL,
        Opcode::SBLR3_SHARD_REPLICA_DDL,
        Opcode::SBLR3_SHARD_MIGRATE,
        Opcode::SBLR3_SHARD_SHOW,
        Opcode::SBLR3_CUBE_DDL,
        Opcode::SBLR3_CUBE_REFRESH,
        Opcode::SBLR3_CUBE_SHOW_STATS,
        Opcode::SBLR3_SECURITY_ENCRYPTION_PROFILE,
        Opcode::SBLR3_SECURITY_ENCRYPTION_KEY,
        Opcode::SBLR3_SECURITY_KEY_SHARD_SUBMIT,
        Opcode::SBLR3_SECURITY_UNLOCK_DATABASE,
        Opcode::SBLR3_SECURITY_CERT_DDL,
        Opcode::SBLR3_SECURITY_PRIVATE_KEY_ROTATE,
        Opcode::SBLR3_SECURITY_SHOW_STATUS,
        Opcode::SBLR3_SERVICE_CHANNEL_BACKUP,
        Opcode::SBLR3_SERVICE_CHANNEL_EVENTS,
        Opcode::SBLR3_SERVICE_CHANNEL_PROGRESS,
    }};

    for (const auto opcode : bridge_expr)
    {
        switch (opcode)
        {
            case Opcode::SBLR3_OP_DOC_PATH_FILTER:
                assertBridgeReject(opcode, makeDocPathPayload());
                break;
            case Opcode::SBLR3_OP_TS_BUCKET_AGG:
                assertBridgeReject(opcode, makeTsBucketPayload());
                break;
            case Opcode::SBLR3_OP_COL_SCAN:
                assertBridgeReject(opcode, makeColScanPayload());
                break;
            case Opcode::SBLR3_OP_SEARCH_DSL_EVAL:
                assertBridgeReject(opcode, makeSearchDslPayload());
                break;
            case Opcode::SBLR3_OP_VECTOR_ANN:
                assertBridgeReject(opcode, makeVectorAnnPayload());
                break;
            case Opcode::SBLR3_OP_HYBRID_BRIDGE_EXCHANGE:
                assertBridgeReject(opcode, makeHybridExchangePayload());
                break;
            case Opcode::SBLR3_OP_HYBRID_BRIDGE_MATERIALIZE:
                assertBridgeReject(opcode, makeHybridMaterializePayload());
                break;
            default:
                break;
        }
    }

    for (const auto opcode : bridge_create_db)
    {
        assertBridgeReject(opcode, makeCreateDatabasePayload());
    }
    for (const auto opcode : bridge_domain)
    {
        assertBridgeReject(opcode, makeCreateDomainPayload());
    }
    for (const auto opcode : bridge_index)
    {
        assertBridgeReject(opcode, makeIndexBridgePayload());
    }
    for (const auto opcode : bridge_control_admin)
    {
        assertBridgeReject(opcode, makeControlPayload());
    }
    for (const auto opcode : bridge_multi_model)
    {
        assertBridgeReject(opcode, makeMultiModelPayload());
    }
    for (const auto opcode : bridge_control_cluster_security)
    {
        assertBridgeReject(opcode, makeControlPayload());
    }

    const size_t total_cases = bridge_expr.size() + bridge_create_db.size() + bridge_domain.size() +
                               bridge_index.size() + bridge_control_admin.size() +
                               bridge_multi_model.size() + bridge_control_cluster_security.size();
    EXPECT_EQ(reject_before + static_cast<double>(total_cases),
              metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "IRX_0406"}));
}

TEST_F(SBLRVNextExecutorDispatchContractTest, UnknownVNextOpcodeRejectedBeforeDispatch)
{
    Buffer stream;
    DecodeError err;

    Instruction version;
    version.opcode = static_cast<uint16_t>(Opcode::SBLR3_VERSION);
    version.flags = 0;
    version.payload = Value(Value::Bytes{0x03, 0x00, 0x00, 0x00, 0x00, 0x00});
    ASSERT_TRUE(scratchbird::sblr::v3::encodeInstructionWithSchema(version, stream, err))
        << err.message;

    // Raw instruction header for unknown opcode 0x60FE with empty payload.
    stream.push_back(0xFE);
    stream.push_back(0x60);
    stream.push_back(0x00);
    stream.push_back(0x00);
    stream.push_back(0x00);
    stream.push_back(0x00);
    stream.push_back(0x00);
    stream.push_back(0x00);

    Instruction end;
    end.opcode = static_cast<uint16_t>(Opcode::SBLR3_END);
    end.flags = 0;
    end.payload = Value(Value::Bytes{});
    ASSERT_TRUE(scratchbird::sblr::v3::encodeInstructionWithSchema(end, stream, err))
        << err.message;

    ExecutionResult result = executeContainer(makeContainerFromStream(stream));
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("SBLR-E-0011"), std::string::npos) << result.error();
}
