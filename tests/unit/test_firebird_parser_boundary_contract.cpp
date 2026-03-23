/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "scratchbird/protocol/adapters/firebird_adapter.h"

// Include core types before firebird_parser_agent.h (header references core types).
#include "scratchbird/core/types.h"
#include "scratchbird/core/typed_value.h"

#include "scratchbird/ipc/firebird_parser_agent.h"

namespace {

template <typename AdapterT>
class CompileHarness : public AdapterT {
public:
    using AdapterT::AdapterT;

    scratchbird::core::Status runCompile(const std::string& sql,
                                         std::vector<uint8_t>& bytecode_out,
                                         std::string& error_out) {
        return AdapterT::compileQuery(sql, bytecode_out, error_out);
    }

    void applyFirebirdSessionSchemaContextForTest(scratchbird::core::ErrorContext* ctx) {
        if constexpr (std::is_base_of_v<scratchbird::protocol::FirebirdAdapter, AdapterT>) {
            AdapterT::applyFirebirdSessionSchemaContextForTest(ctx);
        }
    }

    scratchbird::core::ConnectionContext* connectionContextForTest() {
        return AdapterT::connection_ctx_.get();
    }
};

class FirebirdParserAgentHarness : public scratchbird::ipc::FirebirdParserAgent {
public:
    using scratchbird::ipc::FirebirdParserAgent::FirebirdParserAgent;
    using scratchbird::ipc::FirebirdParserAgent::mapProtocolErrorToSQLState;
    using scratchbird::ipc::FirebirdParserAgent::mapSQLStateToProtocol;
};

scratchbird::protocol::ProtocolAdapterConfig makeAdapterConfig(const std::string& name) {
    scratchbird::protocol::ProtocolAdapterConfig cfg;
    cfg.database_path = (std::filesystem::path("build") / "database" / name).string();
    cfg.auto_create_db = true;
    return cfg;
}

void cleanupDb(const std::string& name) {
    std::error_code ec;
    const auto path = std::filesystem::path("build") / "database" / name;
    std::filesystem::remove(path, ec);
    std::filesystem::create_directories(path.parent_path(), ec);
}

scratchbird::ipc::ParserAgentConfig makeParserAgentConfig() {
    scratchbird::ipc::ParserAgentConfig cfg;
    cfg.name = "fb_boundary_contract_agent";
    cfg.protocol = "firebird";
    cfg.listen_endpoint = "127.0.0.1:0";
    cfg.ipc_endpoint = "/tmp/fb_boundary_contract.sock";
    return cfg;
}

template <typename AdapterT>
scratchbird::core::Status compileSql(CompileHarness<AdapterT>& adapter, const std::string& sql) {
    std::vector<uint8_t> bytecode;
    std::string error;
    return adapter.runCompile(sql, bytecode, error);
}

template <typename AdapterT>
scratchbird::core::Status compileSql(CompileHarness<AdapterT>& adapter,
                                     const std::string& sql,
                                     std::string& error_out) {
    std::vector<uint8_t> bytecode;
    return adapter.runCompile(sql, bytecode, error_out);
}

void appendBe32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void appendBe64(std::vector<uint8_t>& out, uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<uint8_t>((value >> shift) & 0xFF));
    }
}

void appendXdrBuffer(std::vector<uint8_t>& out, const std::string& value) {
    appendBe32(out, static_cast<uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
    while (out.size() % 4 != 0) {
        out.push_back(0);
    }
}

std::vector<uint8_t> buildFirebirdErrorResponsePacket(uint32_t gds_code,
                                                      const std::string& sqlstate) {
    // op_response + handle + object_id + data_buffer(0) + status vector
    std::vector<uint8_t> packet;
    appendBe32(packet, 9);  // op_response
    appendBe32(packet, 0);  // handle
    appendBe64(packet, 0);  // object_id
    appendBe32(packet, 0);  // data length

    appendBe32(packet, 1);  // isc_arg_gds
    appendBe32(packet, gds_code);
    if (!sqlstate.empty()) {
        appendBe32(packet, 19);  // isc_arg_sql_state
        appendXdrBuffer(packet, sqlstate);
    }
    appendBe32(packet, 0);  // isc_arg_end
    return packet;
}

}  // namespace

TEST(FirebirdParserBoundaryContractTest, RejectAcceptPack) {
    cleanupDb("fb_boundary_contract.sbdb");
    CompileHarness<scratchbird::protocol::FirebirdAdapter> adapter(
        makeAdapterConfig("fb_boundary_contract.sbdb"));

    scratchbird::core::ErrorContext schema_ctx;
    adapter.applyFirebirdSessionSchemaContextForTest(&schema_ctx);
    ASSERT_TRUE(schema_ctx.message.empty()) << schema_ctx.message;
    ASSERT_NE(adapter.connectionContextForTest(), nullptr);

    std::string error;
    EXPECT_EQ(scratchbird::core::Status::OK, compileSql(adapter, "SELECT 1 FROM RDB$DATABASE", error))
        << error;

    error.clear();
    EXPECT_NE(scratchbird::core::Status::OK, compileSql(adapter, "SET TERM ^", error))
        << "Unexpected acceptance: " << error;

    error.clear();
    EXPECT_NE(scratchbird::core::Status::OK, compileSql(adapter, "DECLARE VARIABLE X INTEGER", error))
        << "Unexpected acceptance: " << error;

    error.clear();
    EXPECT_NE(scratchbird::core::Status::OK, compileSql(adapter, "BEGIN END", error))
        << "Unexpected acceptance: " << error;
}

TEST(FirebirdParserBoundaryContractTest, ErrorSqlStateTranslationDeterminism) {
    FirebirdParserAgentHarness agent(makeParserAgentConfig());

    EXPECT_EQ("335544472", agent.mapSQLStateToProtocol("28000"));

    std::array<char, 6> out{};
    const std::vector<uint8_t> wire = buildFirebirdErrorResponsePacket(335544472u, "28000");
    agent.mapProtocolErrorToSQLState(wire, out.data());
    EXPECT_STREQ("28000", out.data());
}
