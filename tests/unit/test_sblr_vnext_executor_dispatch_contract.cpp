#include <gtest/gtest.h>

#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/uuidv7.h"
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

auto parseUuidText(const std::string& raw, ID& out) -> bool
{
    std::string hex;
    hex.reserve(32);
    for (unsigned char ch : raw)
    {
        if (ch == '-' || ch == '{' || ch == '}')
        {
            continue;
        }
        if (!std::isxdigit(ch))
        {
            return false;
        }
        hex.push_back(static_cast<char>(std::tolower(ch)));
    }
    if (hex.size() != 32)
    {
        return false;
    }

    auto nibble = [](char c) -> int
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f')
        {
            return 10 + (c - 'a');
        }
        return -1;
    };

    ID parsed{};
    for (size_t i = 0; i < parsed.bytes.size(); ++i)
    {
        const int hi = nibble(hex[i * 2]);
        const int lo = nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0)
        {
            return false;
        }
        parsed.bytes[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    out = parsed;
    return true;
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

auto makeContainerWithInstruction(uint16_t opcode, Value::Bytes payload) -> std::vector<uint8_t>
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
    scratchbird::sblr::v3::encodeInstruction(stmt, stream);

    Instruction end;
    end.opcode = static_cast<uint16_t>(Opcode::SBLR3_END);
    end.flags = 0;
    end.payload = Value(Value::Bytes{});
    scratchbird::sblr::v3::encodeInstruction(end, stream);

    return makeContainerFromStream(stream);
}
} // namespace

class SBLRVNextExecutorDispatchContractTest : public ::testing::Test
{
protected:
    struct RemoteProjectionFixtureRows
    {
        std::string server_name;
        ID server_id{};
        ID user_mapping_id{};
        ID remote_connector_id{};
        ID remote_policy_id{};
        ID snapshot_id{};
        ID remote_object_id{};
    };

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

    auto executeVNextRaw(uint16_t opcode, Value::Bytes payload) -> ExecutionResult
    {
        return executeContainer(makeContainerWithInstruction(opcode, std::move(payload)));
    }

    auto installRemoteProjectionFixture(const std::string& server_name) -> RemoteProjectionFixtureRows
    {
        RemoteProjectionFixtureRows fixture{};
        fixture.server_name = server_name;

        auto* catalog = db_->catalog_manager();
        if (!catalog)
        {
            ADD_FAILURE() << "Catalog manager is null";
            return fixture;
        }

        ErrorContext ctx;
        auto expect_ok = [&](Status status, const std::string& where) -> bool {
            if (status == Status::OK)
            {
                return true;
            }
            ADD_FAILURE() << where << " failed: " << ctx.message;
            return false;
        };

        if (!expect_ok(catalog->createForeignServer(server_name,
                                                    "postgresql",
                                                    "127.0.0.1",
                                                    1,
                                                    "DATABASE=remote_test",
                                                    fixture.server_id,
                                                    &ctx),
                       "createForeignServer"))
        {
            return fixture;
        }

        const ID current_user_id = conn_ctx_->getCurrentUserId();
        if (current_user_id == ID{})
        {
            ADD_FAILURE() << "Current user ID is null";
            return fixture;
        }
        if (!expect_ok(catalog->createUserMapping(current_user_id,
                                                  fixture.server_id,
                                                  "remote_user",
                                                  "remote_secret",
                                                  fixture.user_mapping_id,
                                                  &ctx),
                       "createUserMapping"))
        {
            return fixture;
        }

        CatalogManager::RemoteConnectorCatalogInfo connector{};
        connector.remote_connector_id = scratchbird::core::generateUuidV7();
        connector.fdw_server_id = fixture.server_id;
        connector.fdw_id = scratchbird::core::generateUuidV7();
        connector.connector_name = server_name + "_connector";
        connector.engine_name = "postgresql";
        connector.has_engine_version_text = true;
        connector.engine_version_text = "18.0";
        connector.endpoint_uri = "tcp://127.0.0.1:1";
        connector.has_default_mapping_id = true;
        connector.default_mapping_id = fixture.user_mapping_id;
        connector.state = CatalogManager::RemoteConnectorState::READY;
        connector.module_checksum = 1;
        if (!expect_ok(catalog->upsertRemoteConnectorCatalogEntry(connector, &ctx),
                       "upsertRemoteConnectorCatalogEntry(initial)"))
        {
            return fixture;
        }
        fixture.remote_connector_id = connector.remote_connector_id;

        CatalogManager::RemotePassthroughPolicyCatalogInfo policy{};
        policy.remote_policy_id = scratchbird::core::generateUuidV7();
        policy.remote_connector_id = connector.remote_connector_id;
        policy.allow_query = true;
        policy.allow_dml = true;
        policy.allow_ddl = true;
        policy.allow_admin = true;
        policy.allow_procedural = true;
        policy.allow_join_local_txn = true;
        policy.timeout_ms = 100;
        policy.audit_level = "basic";
        if (!expect_ok(catalog->upsertRemotePassthroughPolicyCatalogEntry(policy, &ctx),
                       "upsertRemotePassthroughPolicyCatalogEntry"))
        {
            return fixture;
        }
        fixture.remote_policy_id = policy.remote_policy_id;

        connector.has_policy_id = true;
        connector.policy_id = policy.remote_policy_id;
        if (!expect_ok(catalog->upsertRemoteConnectorCatalogEntry(connector, &ctx),
                       "upsertRemoteConnectorCatalogEntry(policy-bind)"))
        {
            return fixture;
        }

        CatalogManager::RemoteMetadataSnapshotCatalogInfo snapshot{};
        snapshot.snapshot_id = scratchbird::core::generateUuidV7();
        snapshot.remote_connector_id = connector.remote_connector_id;
        snapshot.snapshot_seq = 1;
        snapshot.snapshot_kind = CatalogManager::RemoteSnapshotKind::FULL;
        snapshot.snapshot_status = CatalogManager::RemoteSnapshotStatus::COMPLETE;
        snapshot.has_engine_version_text = true;
        snapshot.engine_version_text = "18.0";
        snapshot.object_count = 1;
        snapshot.column_count = 2;
        snapshot.has_catalog_hash = true;
        snapshot.catalog_hash = 0xAA55;
        snapshot.started_time = 1000;
        snapshot.has_completed_time = true;
        snapshot.completed_time = 1001;
        if (!expect_ok(catalog->upsertRemoteMetadataSnapshotCatalogEntry(snapshot, &ctx),
                       "upsertRemoteMetadataSnapshotCatalogEntry"))
        {
            return fixture;
        }
        fixture.snapshot_id = snapshot.snapshot_id;

        CatalogManager::RemoteMetadataObjectCatalogInfo object_row{};
        object_row.remote_object_id = scratchbird::core::generateUuidV7();
        object_row.snapshot_id = snapshot.snapshot_id;
        object_row.remote_path = "public.orders";
        object_row.has_remote_schema_name = true;
        object_row.remote_schema_name = "public";
        object_row.remote_object_name = "orders";
        object_row.remote_object_kind = CatalogManager::RemoteObjectKind::TABLE;
        object_row.remote_signature = 0xABCD;
        object_row.has_mapped_local_schema_id = true;
        object_row.mapped_local_schema_id = default_schema_id_;
        object_row.is_supported = true;
        object_row.is_valid = true;
        if (!expect_ok(catalog->upsertRemoteMetadataObjectCatalogEntry(object_row, &ctx),
                       "upsertRemoteMetadataObjectCatalogEntry"))
        {
            return fixture;
        }
        fixture.remote_object_id = object_row.remote_object_id;

        CatalogManager::RemoteMetadataColumnCatalogInfo column_id{};
        column_id.remote_column_id = scratchbird::core::generateUuidV7();
        column_id.remote_object_id = object_row.remote_object_id;
        column_id.ordinal_position = 1;
        column_id.column_name = "id";
        column_id.remote_type_name = "bigint";
        column_id.is_nullable = false;
        column_id.is_valid = true;
        if (!expect_ok(catalog->upsertRemoteMetadataColumnCatalogEntry(column_id, &ctx),
                       "upsertRemoteMetadataColumnCatalogEntry(id)"))
        {
            return fixture;
        }

        CatalogManager::RemoteMetadataColumnCatalogInfo column_status{};
        column_status.remote_column_id = scratchbird::core::generateUuidV7();
        column_status.remote_object_id = object_row.remote_object_id;
        column_status.ordinal_position = 2;
        column_status.column_name = "status";
        column_status.remote_type_name = "text";
        column_status.is_nullable = true;
        column_status.is_valid = true;
        if (!expect_ok(catalog->upsertRemoteMetadataColumnCatalogEntry(column_status, &ctx),
                       "upsertRemoteMetadataColumnCatalogEntry(status)"))
        {
            return fixture;
        }

        return fixture;
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
        metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"});

    struct DispatchCase
    {
        uint16_t opcode;
        const char *symbol;
        Value::Object payload;
    };

    const std::array<DispatchCase, 13> cases = {{
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
        EXPECT_NE(result.error().find("BRG_0406"), std::string::npos) << result.error();
        EXPECT_NE(result.error().find(entry.symbol), std::string::npos) << result.error();
        EXPECT_EQ(result.error().find("V3 opcode not implemented in executor"), std::string::npos)
            << result.error();
    }

    EXPECT_EQ(reject_before + static_cast<double>(cases.size()),
              metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"}));
}

TEST_F(SBLRVNextExecutorDispatchContractTest, RetiredVacuumAliasCodepointIsRejected)
{
    const std::string metric = "scratchbird_vnext_executor_events_total";
    const double reject_before =
        metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "IRX_0403"});

    // 0x611E was the retired vacuum alias opcode codepoint.
    // The retirement path remains a schema-validated control command.
    const uint16_t retired_opcode = static_cast<uint16_t>(0x611E);
    scratchbird::sblr::v3::Instruction retired_inst;
    retired_inst.opcode = retired_opcode;
    retired_inst.flags = 0;
    retired_inst.payload = Value(Value::Object{{"action", Value(static_cast<uint64_t>(0))}});

    scratchbird::sblr::v3::Buffer retired_encoded;
    DecodeError retired_encode_err;
    EXPECT_TRUE(scratchbird::sblr::v3::encodeInstructionWithSchema(retired_inst, retired_encoded, retired_encode_err))
        << retired_encode_err.message;
    EXPECT_GE(retired_encoded.size(), 8u);
    const char* retired_schema_name = scratchbird::sblr::v3::opcodeName(retired_inst.opcode);
    EXPECT_STREQ("SBLR3_RETIRED_VACUUM_ALIAS", retired_schema_name ? retired_schema_name : "");

    size_t retired_offset = 0;
    scratchbird::sblr::v3::Instruction retired_decoded;
    DecodeError retired_decode_err;
    bool retired_decoded_ok = scratchbird::sblr::v3::decodeInstructionWithSchema(
        retired_encoded.data(), retired_encoded.size(), retired_offset, retired_decoded, retired_decode_err);
    EXPECT_TRUE(retired_decoded_ok) << retired_decode_err.message;

    ExecutionResult result = executeVNext(
        retired_opcode,
        Value::Object{{"action", Value(static_cast<uint64_t>(0))}});

    EXPECT_FALSE(result.success());
    EXPECT_NE(result.error().find("IRX_0403"), std::string::npos) << result.error();
    EXPECT_EQ(reject_before + 1.0,
              metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "IRX_0403"}));
}

TEST_F(SBLRVNextExecutorDispatchContractTest, UdrCompileDispatchAcceptsValidProfilePayload)
{
    ExecutionResult result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_COMPILE_DISPATCH),
        Value::Object{{"validate_only", Value(false)},
                      {"profile_id", Value(std::string("PostgreSQL"))},
                      {"payload_format", Value(std::string("SQL_TEXT"))},
                      {"payload_bytes", Value(std::string("SELECT 1"))},
                      {"session_signature", Value(std::string("sig_ok"))}});

    EXPECT_TRUE(result.success()) << result.error();
}

TEST_F(SBLRVNextExecutorDispatchContractTest, UdrCompileDispatchRejectsDeterministicallyByProfileContract)
{
    {
        ExecutionResult result = executeVNext(
            static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_COMPILE_DISPATCH),
            Value::Object{{"validate_only", Value(false)},
                          {"profile_id", Value(std::string("UNKNOWN_PROFILE"))},
                          {"payload_format", Value(std::string("SQL_TEXT"))},
                          {"payload_bytes", Value(std::string("SELECT 1"))},
                          {"session_signature", Value(std::string("sig_bad_profile"))}});

        EXPECT_FALSE(result.success());
        EXPECT_NE(result.error().find("UDR_1501"), std::string::npos) << result.error();
    }

    {
        ExecutionResult result = executeVNext(
            static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_COMPILE_DISPATCH),
            Value::Object{{"validate_only", Value(false)},
                          {"profile_id", Value(std::string("PostgreSQL"))},
                          {"payload_format", Value(std::string("RESP_ARRAY"))},
                          {"payload_bytes", Value(std::string("*1\\r\\n$4\\r\\nPING\\r\\n"))},
                          {"session_signature", Value(std::string("sig_bad_format"))}});

        EXPECT_FALSE(result.success());
        EXPECT_NE(result.error().find("UDR_1505"), std::string::npos) << result.error();
    }
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       UdrCompileDispatchNativePreferredAcceptsPolicyBoundRequest)
{
    ExecutionResult result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_COMPILE_DISPATCH),
        Value::Object{
            {"validate_only", Value(false)},
            {"profile_id", Value(std::string("PostgreSQL"))},
            {"payload_format", Value(std::string("SQL_TEXT"))},
            {"payload_bytes", Value(std::string("SELECT 1"))},
            {"session_signature", Value(std::string("sig_native_pref"))},
            {"artifact_preference", Value(std::string("NATIVE_PREFERRED"))},
            {"target_triples", Value(Value::List{
                                   Value(std::string("x86_64-pc-linux-gnu"))})},
            {"host_api_abi_version", Value(std::string("SB_HOST_API_V1"))},
            {"optimization_level", Value(std::string("O2"))},
            {"allow_interpreter_fallback", Value(true)},
            {"native_execution_mode", Value(std::string("PREFER_NATIVE_WITH_FALLBACK"))},
            {"native_artifact_udr_enabled", Value(true)},
            {"native_target_triples", Value(Value::List{
                                          Value(std::string("x86_64-pc-linux-gnu")),
                                          Value(std::string("x86_64-pc-windows-msvc"))})},
            {"native_host_api_abi_version", Value(std::string("SB_HOST_API_V1"))},
        });

    EXPECT_TRUE(result.success()) << result.error();
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       UdrCompileDispatchNativeRequiredRejectsWhenNativeCompilerDisabled)
{
    ExecutionResult result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_COMPILE_DISPATCH),
        Value::Object{
            {"validate_only", Value(false)},
            {"profile_id", Value(std::string("PostgreSQL"))},
            {"payload_format", Value(std::string("SQL_TEXT"))},
            {"payload_bytes", Value(std::string("SELECT 1"))},
            {"session_signature", Value(std::string("sig_native_req_disabled"))},
            {"artifact_preference", Value(std::string("NATIVE_REQUIRED"))},
            {"target_triples", Value(Value::List{
                                   Value(std::string("x86_64-pc-linux-gnu"))})},
            {"host_api_abi_version", Value(std::string("SB_HOST_API_V1"))},
            {"native_artifact_udr_enabled", Value(false)},
        });

    EXPECT_FALSE(result.success());
    EXPECT_NE(result.error().find("UDR_1516"), std::string::npos) << result.error();
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       UdrCompileDispatchNativeRequiredRejectsPolicyTargetAllowlistViolation)
{
    ExecutionResult result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_COMPILE_DISPATCH),
        Value::Object{
            {"validate_only", Value(false)},
            {"profile_id", Value(std::string("PostgreSQL"))},
            {"payload_format", Value(std::string("SQL_TEXT"))},
            {"payload_bytes", Value(std::string("SELECT 1"))},
            {"session_signature", Value(std::string("sig_native_req_allowlist"))},
            {"artifact_preference", Value(std::string("NATIVE_REQUIRED"))},
            {"target_triples", Value(Value::List{
                                   Value(std::string("x86_64-pc-windows-msvc"))})},
            {"host_api_abi_version", Value(std::string("SB_HOST_API_V1"))},
            {"native_target_triples", Value(Value::List{
                                          Value(std::string("x86_64-pc-linux-gnu"))})},
        });

    EXPECT_FALSE(result.success());
    EXPECT_NE(result.error().find("UDR_1517"), std::string::npos) << result.error();
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       UdrCompileDispatchNativePreferredRejectsMissingTargetTriples)
{
    ExecutionResult result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_COMPILE_DISPATCH),
        Value::Object{
            {"validate_only", Value(false)},
            {"profile_id", Value(std::string("PostgreSQL"))},
            {"payload_format", Value(std::string("SQL_TEXT"))},
            {"payload_bytes", Value(std::string("SELECT 1"))},
            {"session_signature", Value(std::string("sig_missing_target"))},
            {"artifact_preference", Value(std::string("NATIVE_PREFERRED"))},
            {"host_api_abi_version", Value(std::string("SB_HOST_API_V1"))},
        });

    EXPECT_FALSE(result.success());
    EXPECT_NE(result.error().find("UDR_1506"), std::string::npos) << result.error();
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       UdrEmbeddedSqlCompileValidateRejectsMalformedPayloadWithDeterministicCode)
{
    ExecutionResult result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_EMBEDDED_SQL_COMPILE),
        Value::Object{{"validate_only", Value(true)},
                      {"template_id", Value(std::string("tpl_1"))},
                      {"sql_text", Value(std::string(""))},
                      {"profile_id", Value(std::string("PostgreSQL"))},
                      {"session_signature", Value(std::string("sig_empty_sql"))}});

    EXPECT_FALSE(result.success());
    EXPECT_NE(result.error().find("UDR_1506"), std::string::npos) << result.error();
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       UdrEmbeddedSqlCompileNativeRequiredRejectsPolicyAbiMismatch)
{
    ExecutionResult result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_EMBEDDED_SQL_COMPILE),
        Value::Object{
            {"validate_only", Value(false)},
            {"template_id", Value(std::string("tpl_native"))},
            {"sql_text", Value(std::string("SELECT 42"))},
            {"profile_id", Value(std::string("PostgreSQL"))},
            {"session_signature", Value(std::string("sig_tpl_native"))},
            {"artifact_preference", Value(std::string("NATIVE_REQUIRED"))},
            {"target_triples", Value(Value::List{
                                   Value(std::string("x86_64-pc-linux-gnu"))})},
            {"host_api_abi_version", Value(std::string("SB_HOST_API_V2"))},
            {"native_host_api_abi_version", Value(std::string("SB_HOST_API_V1"))},
            {"allow_interpreter_fallback", Value(false)},
        });

    EXPECT_FALSE(result.success());
    EXPECT_NE(result.error().find("UDR_1520"), std::string::npos) << result.error();
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       CanonicalOpcodeWithDirectDispatchRoutesWithoutDeterministicBridgeCode)
{
    const std::string metric = "scratchbird_vnext_executor_events_total";
    const double reject_before =
        metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"});

    ExecutionResult result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_CREATE_DATABASE),
        Value::Object{{"flags", Value(static_cast<uint64_t>(0))},
                      {"name", Value(std::string("native_db"))},
                      {"encrypted", Value(false)},
                      {"options", Value(Value::Object{{"engine", Value(std::string("native"))}})}});

    ASSERT_TRUE(result.success()) << result.error();

    EXPECT_EQ(reject_before,
              metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"}));
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       SearchDslOpcodeRoutesWithoutDeterministicBridgeReject)
{
    const std::string metric = "scratchbird_vnext_executor_events_total";
    const double reject_before =
        metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"});

    ExecutionResult result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_OP_SEARCH_DSL_EVAL),
        Value::Object{{"dsl_payload_json", Value(std::string("{\"query\":{\"match_all\":{}}}"))},
                      {"target_index", Value(static_cast<uint64_t>(1))},
                      {"dsl_blob_ref", Value(static_cast<uint64_t>(1))},
                      {"scorer_id", Value(static_cast<uint64_t>(1))}});

    ASSERT_TRUE(result.success()) << result.error();

    EXPECT_EQ(reject_before,
              metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"}));
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       RedisOpcodesRouteWithoutDeterministicBridgeReject)
{
    const std::string metric = "scratchbird_vnext_executor_events_total";
    const double reject_before =
        metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"});

    const std::array<Opcode, 7> cases = {{
        Opcode::SBLR3_REDIS_STRING,
        Opcode::SBLR3_REDIS_HASH,
        Opcode::SBLR3_REDIS_LIST,
        Opcode::SBLR3_REDIS_SET,
        Opcode::SBLR3_REDIS_ZSET,
        Opcode::SBLR3_REDIS_STREAM,
        Opcode::SBLR3_REDIS_PUBSUB,
    }};

    for (const auto opcode : cases)
    {
        Instruction query_expr;
        query_expr.opcode = static_cast<uint16_t>(Opcode::SBLR3_LITERAL_STRING);
        query_expr.flags = 0;
        query_expr.payload =
            Value(Value::Object{{"value", Value(std::string("{\"op\":\"ping\"}"))}});

        ExecutionResult result = executeVNext(
            static_cast<uint16_t>(opcode),
            Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                          {"namespace_path",
                           Value(Value::List{Value(std::string("users")),
                                             Value(std::string("redis"))})},
                          {"query_expr",
                           Value(std::make_shared<Instruction>(std::move(query_expr)))},
                          {"options", Value(Value::Object{{"mode", Value(std::string("compat"))}})}});
        ASSERT_TRUE(result.success()) << result.error();
    }

    EXPECT_EQ(reject_before,
              metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"}));
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       FdwDropOpcodesRouteWithoutDeterministicBridgeReject)
{
    const std::string metric = "scratchbird_vnext_executor_events_total";
    const double reject_before =
        metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"});

    struct DispatchCase
    {
        Opcode opcode;
        uint64_t object_type;
        Value::List path;
    };

    const std::array<DispatchCase, 3> cases = {{
        {Opcode::SBLR3_DROP_FOREIGN_SERVER,
         31,
         Value::List{Value(std::string("missing_server"))}},
        {Opcode::SBLR3_DROP_FOREIGN_TABLE,
         32,
         Value::List{Value(std::string("public")), Value(std::string("missing_foreign_table"))}},
        {Opcode::SBLR3_DROP_USER_MAPPING,
         33,
         Value::List{Value(std::string("missing_server"))}},
    }};

    for (const auto& entry : cases)
    {
        ExecutionResult result = executeVNext(
            static_cast<uint16_t>(entry.opcode),
            Value::Object{{"flags", Value(static_cast<uint64_t>(0))},
                          {"object_type", Value(entry.object_type)},
                          {"path", Value(entry.path)}});
        EXPECT_FALSE(result.success());
        EXPECT_EQ(result.error().find("BRG_0406"), std::string::npos) << result.error();
        EXPECT_EQ(result.error().find("IRX_0403"), std::string::npos) << result.error();
    }

    EXPECT_EQ(reject_before,
              metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"}));
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       FdwAlterAndImportOpcodesRouteWithoutDeterministicBridgeReject)
{
    const std::string metric = "scratchbird_vnext_executor_events_total";
    const double reject_before =
        metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"});

    struct DispatchCase
    {
        Opcode opcode;
        Value::Object payload;
    };

    const std::array<DispatchCase, 6> cases = {{
        {Opcode::SBLR3_ALTER_FOREIGN_DATA_WRAPPER,
         Value::Object{{"name", Value(std::string("fdw_bridge"))},
                       {"options", Value(Value::Object{{"count", Value(static_cast<uint64_t>(0))}})}}},
        {Opcode::SBLR3_DROP_FOREIGN_DATA_WRAPPER,
         Value::Object{{"flags", Value(static_cast<uint64_t>(0))},
                       {"object_type", Value(static_cast<uint64_t>(30))},
                       {"path", Value(Value::List{Value(std::string("fdw_bridge"))})}}},
        {Opcode::SBLR3_ALTER_FOREIGN_SERVER,
         Value::Object{{"name", Value(std::string("missing_server"))},
                       {"type", Value(std::string("postgresql"))},
                       {"host", Value(std::string("localhost"))},
                       {"options", Value(Value::Object{{"count", Value(static_cast<uint64_t>(0))}})}}},
        {Opcode::SBLR3_ALTER_FOREIGN_TABLE,
         Value::Object{{"name", Value(Value::List{Value(std::string("public")),
                                                  Value(std::string("missing_foreign_table"))})},
                       {"server", Value(std::string("missing_server"))},
                       {"columns", Value(Value::List{})},
                       {"options", Value(Value::Object{{"count", Value(static_cast<uint64_t>(0))}})}}},
        {Opcode::SBLR3_ALTER_USER_MAPPING,
         Value::Object{{"server", Value(std::string("missing_server"))},
                       {"user", Value(std::string("PUBLIC"))},
                       {"options", Value(Value::Object{{"count", Value(static_cast<uint64_t>(0))}})}}},
        {Opcode::SBLR3_IMPORT_FOREIGN_SCHEMA,
         Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                       {"object_name", Value(std::string("missing_server"))},
                       {"options", Value(Value::Object{{"count", Value(static_cast<uint64_t>(0))}})}}},
    }};

    for (const auto& entry : cases)
    {
        ExecutionResult result = executeVNext(
            static_cast<uint16_t>(entry.opcode), entry.payload);
        EXPECT_EQ(result.error().find("BRG_0406"), std::string::npos) << result.error();
        EXPECT_EQ(result.error().find("IRX_0403"), std::string::npos) << result.error();
    }

    EXPECT_EQ(reject_before,
              metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"}));
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       RemoteOpcodeFamilyRoutesWithoutDeterministicBridgeReject)
{
    const std::string metric = "scratchbird_vnext_executor_events_total";
    const double reject_before =
        metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"});

    const std::array<Opcode, 14> cases = {{
        Opcode::SBLR3_ANALYZE_REMOTE_SERVER,
        Opcode::SBLR3_REFRESH_REMOTE_METADATA,
        Opcode::SBLR3_SHOW_REMOTE_CAPABILITIES,
        Opcode::SBLR3_SHOW_REMOTE_OBJECTS,
        Opcode::SBLR3_SHOW_REMOTE_COLUMNS,
        Opcode::SBLR3_SHOW_REMOTE_STATISTICS,
        Opcode::SBLR3_EXECUTE_REMOTE,
        Opcode::SBLR3_PREPARE_REMOTE,
        Opcode::SBLR3_EXECUTE_REMOTE_PREPARED,
        Opcode::SBLR3_DEALLOCATE_REMOTE_PREPARED,
        Opcode::SBLR3_BEGIN_REMOTE_TRANSACTION,
        Opcode::SBLR3_COMMIT_REMOTE_TRANSACTION,
        Opcode::SBLR3_ROLLBACK_REMOTE_TRANSACTION,
        Opcode::SBLR3_SHOW_REMOTE_SESSION_STATE,
    }};

    for (const auto opcode : cases)
    {
        ExecutionResult result = executeVNext(
            static_cast<uint16_t>(opcode),
            Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                          {"object_name", Value(std::string("missing_server"))},
                          {"options", Value(Value::Object{{"count", Value(static_cast<uint64_t>(0))}})}});
        EXPECT_FALSE(result.success()) << static_cast<uint16_t>(opcode);
        EXPECT_EQ(result.error().find("BRG_0406"), std::string::npos) << result.error();
        EXPECT_EQ(result.error().find("IRX_0403"), std::string::npos) << result.error();
        EXPECT_EQ(result.error().find("REMOTE_22"), std::string::npos) << result.error();
        EXPECT_NE(result.error().find("REMOTE_23"), std::string::npos) << result.error();
    }

    EXPECT_EQ(reject_before,
              metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"}));
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       AlterTablespaceOpcodeRoutesWithoutUnknownOpcodeReject)
{
    const std::string metric = "scratchbird_vnext_executor_events_total";
    const double reject_before =
        metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "IRX_0403"});

    ExecutionResult result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_ALTER_TABLESPACE),
        Value::Object{{"tablespace", Value(Value::List{Value(std::string("missing_ts"))})},
                      {"alterations",
                       Value(Value::List{
                           Value(Value::Object{{"action", Value(static_cast<uint64_t>(0))},
                                               {"autoextend_enabled", Value(false)}})})}});

    EXPECT_EQ(result.error().find("IRX_0403"), std::string::npos) << result.error();

    EXPECT_EQ(reject_before,
              metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "IRX_0403"}));
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       DropSequenceUserGroupOpcodesRouteWithoutUnknownOpcodeReject)
{
    const std::string metric = "scratchbird_vnext_executor_events_total";
    const double reject_before =
        metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "IRX_0403"});

    struct DispatchCase
    {
        Opcode opcode;
        Value::Object payload;
    };

    const std::array<DispatchCase, 3> cases = {{
        {Opcode::SBLR3_DROP_SEQUENCE,
         Value::Object{{"flags", Value(static_cast<uint64_t>(0x01))},
                       {"path", Value(Value::List{Value(std::string("missing_seq"))})}}},
        {Opcode::SBLR3_DROP_USER,
         Value::Object{{"flags", Value(static_cast<uint64_t>(0x01))},
                       {"path", Value(Value::List{Value(std::string("missing_user"))})}}},
        {Opcode::SBLR3_DROP_GROUP,
         Value::Object{{"flags", Value(static_cast<uint64_t>(0x01))},
                       {"path", Value(Value::List{Value(std::string("missing_group"))})}}},
    }};

    for (const auto& entry : cases)
    {
        ExecutionResult result = executeVNext(
            static_cast<uint16_t>(entry.opcode), entry.payload);
        EXPECT_EQ(result.error().find("IRX_0403"), std::string::npos) << result.error();
    }

    EXPECT_EQ(reject_before,
              metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "IRX_0403"}));
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       ShowRemoteObjectsFallsBackToCatalogSnapshotWhenRemoteRuntimeFails)
{
    const auto fixture =
        installRemoteProjectionFixture("remote_projection_fallback_objects");

    ExecutionResult result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_SHOW_REMOTE_OBJECTS),
        Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                      {"object_name", Value(fixture.server_name)},
                      {"object_path", Value(Value::List{Value(fixture.server_name)})},
                      {"options", Value(Value::Object{{"count", Value(static_cast<uint64_t>(0))}})}});

    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    auto* rs = result.resultSet();
    ASSERT_EQ(rs->columnCount(), 6u);
    EXPECT_EQ(rs->columnName(0), "remote_path");
    EXPECT_EQ(rs->columnName(1), "remote_schema_name");
    EXPECT_EQ(rs->columnName(2), "remote_object_name");
    EXPECT_EQ(rs->columnName(3), "remote_object_kind");
    EXPECT_EQ(rs->columnName(4), "local_schema_path");
    EXPECT_EQ(rs->columnName(5), "is_supported");
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_EQ(rs->getValue(0, 0).toString(), "public.orders");
    EXPECT_EQ(rs->getValue(0, 1).toString(), "public");
    EXPECT_EQ(rs->getValue(0, 2).toString(), "orders");
    EXPECT_EQ(rs->getValue(0, 3).toString(), "TABLE");
    EXPECT_FALSE(rs->getValue(0, 4).toString().empty());
    EXPECT_EQ(rs->getValue(0, 5).toString(), "true");
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       ShowRemoteColumnsFallsBackToCatalogSnapshotWhenRemoteRuntimeFails)
{
    const auto fixture =
        installRemoteProjectionFixture("remote_projection_fallback_columns");

    ExecutionResult result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_SHOW_REMOTE_COLUMNS),
        Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                      {"object_name", Value(fixture.server_name)},
                      {"object_path", Value(Value::List{Value(fixture.server_name)})},
                      {"options", Value(Value::Object{{"count", Value(static_cast<uint64_t>(0))}})}});

    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    auto* rs = result.resultSet();
    ASSERT_EQ(rs->columnCount(), 8u);
    EXPECT_EQ(rs->columnName(0), "remote_path");
    EXPECT_EQ(rs->columnName(1), "remote_schema_name");
    EXPECT_EQ(rs->columnName(2), "remote_object_name");
    EXPECT_EQ(rs->columnName(3), "column_name");
    EXPECT_EQ(rs->columnName(4), "remote_type_name");
    EXPECT_EQ(rs->columnName(5), "ordinal_position");
    EXPECT_EQ(rs->columnName(6), "is_nullable");
    EXPECT_EQ(rs->columnName(7), "local_schema_path");
    ASSERT_EQ(rs->rowCount(), 2u);
    EXPECT_EQ(rs->getValue(0, 0).toString(), "public.orders");
    EXPECT_EQ(rs->getValue(0, 1).toString(), "public");
    EXPECT_EQ(rs->getValue(0, 2).toString(), "orders");
    EXPECT_EQ(rs->getValue(0, 3).toString(), "id");
    EXPECT_EQ(rs->getValue(0, 6).toString(), "NO");
    EXPECT_EQ(rs->getValue(1, 3).toString(), "status");
    EXPECT_EQ(rs->getValue(1, 6).toString(), "YES");
    EXPECT_FALSE(rs->getValue(0, 7).toString().empty());
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       PrepareRemoteCatalogLifecyclePersistsAndDeallocates)
{
    const auto fixture = installRemoteProjectionFixture("remote_prepare_lifecycle");

    ExecutionResult prepare_result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_PREPARE_REMOTE),
        Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                      {"object_name", Value(fixture.server_name)},
                      {"options", Value(Value::Object{
                                      {"count", Value(static_cast<uint64_t>(2))},
                                      {"STATEMENT_NAME", Value(std::string("ps_orders"))},
                                      {"SQL_TEXT", Value(std::string("SELECT * FROM orders"))}})}});

    ASSERT_TRUE(prepare_result.success()) << prepare_result.error();
    ASSERT_NE(conn_ctx_->sessionId(), ID{});

    ErrorContext ctx;
    std::vector<CatalogManager::RemotePreparedStatementCatalogInfo> rows;
    ASSERT_EQ(db_->catalog_manager()->listRemotePreparedStatementCatalogEntries(conn_ctx_->sessionId(),
                                                                                rows,
                                                                                &ctx),
              Status::OK)
        << ctx.message;

    bool found_prepared = false;
    for (const auto& row : rows)
    {
        if (row.remote_connector_id == fixture.remote_connector_id &&
            row.statement_name == "ps_orders" && row.is_valid)
        {
            found_prepared = true;
            break;
        }
    }
    EXPECT_TRUE(found_prepared);

    ExecutionResult deallocate_result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_DEALLOCATE_REMOTE_PREPARED),
        Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                      {"object_name", Value(fixture.server_name)},
                      {"options", Value(Value::Object{
                                      {"count", Value(static_cast<uint64_t>(1))},
                                      {"STATEMENT_NAME", Value(std::string("ps_orders"))}})}});

    ASSERT_TRUE(deallocate_result.success()) << deallocate_result.error();

    rows.clear();
    ASSERT_EQ(db_->catalog_manager()->listRemotePreparedStatementCatalogEntries(conn_ctx_->sessionId(),
                                                                                rows,
                                                                                &ctx),
              Status::OK)
        << ctx.message;

    bool still_present = false;
    for (const auto& row : rows)
    {
        if (row.remote_connector_id == fixture.remote_connector_id &&
            row.statement_name == "ps_orders" && row.is_valid)
        {
            still_present = true;
            break;
        }
    }
    EXPECT_FALSE(still_present);
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       ExecuteRemotePreparedUsesCatalogPreparedStateInsteadOfStubReject)
{
    const auto fixture = installRemoteProjectionFixture("remote_prepare_execute");

    ExecutionResult prepare_result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_PREPARE_REMOTE),
        Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                      {"object_name", Value(fixture.server_name)},
                      {"options", Value(Value::Object{
                                      {"count", Value(static_cast<uint64_t>(2))},
                                      {"STATEMENT_NAME", Value(std::string("ps_exec"))},
                                      {"SQL_TEXT", Value(std::string("SELECT 1"))}})}});
    ASSERT_TRUE(prepare_result.success()) << prepare_result.error();

    ExecutionResult execute_prepared_result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_EXECUTE_REMOTE_PREPARED),
        Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                      {"object_name", Value(fixture.server_name)},
                      {"options", Value(Value::Object{
                                      {"count", Value(static_cast<uint64_t>(1))},
                                      {"STATEMENT_NAME", Value(std::string("ps_exec"))}})}});

    ASSERT_FALSE(execute_prepared_result.success());
    EXPECT_NE(execute_prepared_result.error().find("REMOTE_2390"), std::string::npos)
        << execute_prepared_result.error();
    EXPECT_EQ(execute_prepared_result.error().find("REMOTE_2312"), std::string::npos)
        << execute_prepared_result.error();
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       BeginCommitRemoteTransactionPersistsLifecycleForAutonomousMode)
{
    const auto fixture = installRemoteProjectionFixture("remote_txn_lifecycle");

    ExecutionResult begin_result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_BEGIN_REMOTE_TRANSACTION),
        Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                      {"object_name", Value(fixture.server_name)},
                      {"options", Value(Value::Object{
                                      {"count", Value(static_cast<uint64_t>(1))},
                                      {"TXN_MODE", Value(std::string("AUTONOMOUS"))}})}});
    ASSERT_TRUE(begin_result.success()) << begin_result.error();
    ASSERT_NE(conn_ctx_->sessionId(), ID{});

    ErrorContext ctx;
    std::vector<CatalogManager::RemoteTxnBindingCatalogInfo> binding_rows;
    ASSERT_EQ(db_->catalog_manager()->listRemoteTxnBindingCatalogEntries(fixture.remote_connector_id,
                                                                         binding_rows,
                                                                         &ctx),
              Status::OK)
        << ctx.message;

    ID binding_id{};
    bool found_active = false;
    for (const auto& row : binding_rows)
    {
        if (row.session_id == conn_ctx_->sessionId() && row.is_valid)
        {
            binding_id = row.remote_txn_binding_id;
            found_active = true;
            EXPECT_EQ(row.txn_mode, CatalogManager::RemoteTxnMode::AUTONOMOUS);
            EXPECT_EQ(row.txn_state, CatalogManager::RemoteTxnState::ACTIVE);
            break;
        }
    }
    ASSERT_TRUE(found_active);

    ExecutionResult commit_result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_COMMIT_REMOTE_TRANSACTION),
        Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                      {"object_name", Value(fixture.server_name)},
                      {"options", Value(Value::Object{
                                      {"count", Value(static_cast<uint64_t>(0))}})}});
    ASSERT_TRUE(commit_result.success()) << commit_result.error();

    CatalogManager::RemoteTxnBindingCatalogInfo binding_out{};
    ASSERT_EQ(db_->catalog_manager()->getRemoteTxnBindingCatalogEntry(binding_id, binding_out, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(binding_out.txn_state, CatalogManager::RemoteTxnState::COMMITTED);
    EXPECT_TRUE(binding_out.has_terminal_time);
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       ShowRemoteSessionStateReportsPreparedAndTransactionCounts)
{
    const auto fixture = installRemoteProjectionFixture("remote_session_state");

    ASSERT_TRUE(executeVNext(
                    static_cast<uint16_t>(Opcode::SBLR3_PREPARE_REMOTE),
                    Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                                  {"object_name", Value(fixture.server_name)},
                                  {"options", Value(Value::Object{
                                                  {"count", Value(static_cast<uint64_t>(2))},
                                                  {"STATEMENT_NAME", Value(std::string("ps_state"))},
                                                  {"SQL_TEXT", Value(std::string("SELECT 1"))}})}})
                    .success());
    ASSERT_TRUE(executeVNext(
                    static_cast<uint16_t>(Opcode::SBLR3_BEGIN_REMOTE_TRANSACTION),
                    Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                                  {"object_name", Value(fixture.server_name)},
                                  {"options", Value(Value::Object{
                                                  {"count", Value(static_cast<uint64_t>(1))},
                                                  {"TXN_MODE", Value(std::string("AUTONOMOUS"))}})}})
                    .success());

    ExecutionResult show_result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_SHOW_REMOTE_SESSION_STATE),
        Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                      {"object_name", Value(fixture.server_name)},
                      {"options", Value(Value::Object{
                                      {"count", Value(static_cast<uint64_t>(0))}})}});

    ASSERT_TRUE(show_result.success()) << show_result.error();
    ASSERT_TRUE(show_result.hasResultSet());
    ASSERT_NE(show_result.resultSet(), nullptr);
    auto* rs = show_result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_EQ(rs->getValue(0, 1).toString(), fixture.server_name);
    EXPECT_EQ(rs->getValue(0, 2).toString(), "1");
    EXPECT_EQ(rs->getValue(0, 3).toString(), "1");
    EXPECT_EQ(rs->getValue(0, 4).toString(), "AUTONOMOUS");
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       ExecuteRemoteCancelProducesCancelledAuditAndPreservesConnectorReadiness)
{
    const auto fixture = installRemoteProjectionFixture("remote_cancel_audit");
    const std::string request_id_text = "01234567-89ab-cdef-0123-456789abcdef";
    ID expected_request_id{};
    ASSERT_TRUE(parseUuidText(request_id_text, expected_request_id));

    ExecutionResult cancel_result = executeVNext(
        static_cast<uint16_t>(Opcode::SBLR3_EXECUTE_REMOTE),
        Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                      {"object_name", Value(fixture.server_name)},
                      {"options", Value(Value::Object{
                                      {"count", Value(static_cast<uint64_t>(3))},
                                      {"SQL_TEXT", Value(std::string("SELECT 1"))},
                                      {"CANCEL", Value(true)},
                                      {"REQUEST_ID", Value(request_id_text)}})}});

    ASSERT_FALSE(cancel_result.success());
    EXPECT_NE(cancel_result.error().find("REMOTE_2311"), std::string::npos)
        << cancel_result.error();

    ErrorContext ctx;
    CatalogManager::RemoteConnectorCatalogInfo connector{};
    ASSERT_EQ(db_->catalog_manager()->getRemoteConnectorCatalogEntry(fixture.remote_connector_id,
                                                                     connector,
                                                                     &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(connector.state, CatalogManager::RemoteConnectorState::READY);
    EXPECT_EQ(connector.failure_count, 0u);

    std::vector<CatalogManager::RemoteExecutionAuditCatalogInfo> audit_rows;
    ASSERT_EQ(db_->catalog_manager()->listRemoteExecutionAuditCatalogEntries(fixture.remote_connector_id,
                                                                             audit_rows,
                                                                             &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(audit_rows.size(), 1u);
    EXPECT_EQ(audit_rows.front().exec_status, CatalogManager::RemoteExecStatus::CANCELLED);
    EXPECT_EQ(audit_rows.front().request_id, expected_request_id);
}

TEST_F(SBLRVNextExecutorDispatchContractTest,
       ExecuteRemoteFailuresDriveConnectorDegradedThenFailedState)
{
    const auto fixture = installRemoteProjectionFixture("remote_degraded_state");

    auto execute_failure = [&]() -> ExecutionResult {
        return executeVNext(
            static_cast<uint16_t>(Opcode::SBLR3_EXECUTE_REMOTE),
            Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                          {"object_name", Value(fixture.server_name)},
                          {"options", Value(Value::Object{
                                          {"count", Value(static_cast<uint64_t>(1))},
                                          {"SQL_TEXT", Value(std::string("SELECT 1"))}})}});
    };

    ErrorContext ctx;
    CatalogManager::RemoteConnectorCatalogInfo connector{};

    for (int attempt = 1; attempt <= 3; ++attempt)
    {
        ExecutionResult result = execute_failure();
        ASSERT_FALSE(result.success());
        EXPECT_NE(result.error().find("REMOTE_2390"), std::string::npos) << result.error();

        ASSERT_EQ(db_->catalog_manager()->getRemoteConnectorCatalogEntry(fixture.remote_connector_id,
                                                                         connector,
                                                                         &ctx),
                  Status::OK)
            << ctx.message;
        EXPECT_EQ(connector.failure_count, static_cast<uint32_t>(attempt));
        if (attempt < 3)
        {
            EXPECT_EQ(connector.state, CatalogManager::RemoteConnectorState::DEGRADED);
        }
        else
        {
            EXPECT_EQ(connector.state, CatalogManager::RemoteConnectorState::FAILED);
        }
    }

    ExecutionResult failed_state_result = execute_failure();
    ASSERT_FALSE(failed_state_result.success());
    EXPECT_NE(failed_state_result.error().find("REMOTE_2304"), std::string::npos)
        << failed_state_result.error();

    std::vector<CatalogManager::RemoteExecutionAuditCatalogInfo> audit_rows;
    ASSERT_EQ(db_->catalog_manager()->listRemoteExecutionAuditCatalogEntries(fixture.remote_connector_id,
                                                                             audit_rows,
                                                                             &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(audit_rows.size(), 3u);
    for (const auto& row : audit_rows)
    {
        EXPECT_EQ(row.exec_status, CatalogManager::RemoteExecStatus::FAILED);
    }
}

TEST_F(SBLRVNextExecutorDispatchContractTest, BridgeOpcodeFamilyMatrixRejectsDeterministically)
{
    const std::string metric = "scratchbird_vnext_executor_events_total";
    const double reject_before =
        metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"});

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
        EXPECT_NE(result.error().find("BRG_0406"), std::string::npos) << result.error();
        EXPECT_NE(result.error().find(symbol), std::string::npos) << result.error();
        EXPECT_EQ(result.error().find("V3 opcode not implemented in executor"), std::string::npos)
            << result.error();
    };

    const std::array<Opcode, 6> bridge_expr = {{
        Opcode::SBLR3_OP_DOC_PATH_FILTER,
        Opcode::SBLR3_OP_TS_BUCKET_AGG,
        Opcode::SBLR3_OP_COL_SCAN,
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

    const std::array<Opcode, 17> bridge_control_admin = {{
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
    }};

    const std::array<Opcode, 20> bridge_multi_model = {{
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
              metricCounterValue(metric, {"vnext_opcode_dispatch", "reject", "BRG_0406"}));
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
