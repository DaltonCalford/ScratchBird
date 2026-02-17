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

    const std::array<DispatchCase, 7> cases = {{
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
