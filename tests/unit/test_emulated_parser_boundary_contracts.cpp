/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "scratchbird/protocol/adapters/protocol_adapter.h"
#include "scratchbird/protocol/adapters/mysql_adapter.h"
#include "scratchbird/protocol/adapters/postgresql_adapter.h"
#include "scratchbird/protocol/adapters/firebird_adapter.h"

// Include core types before firebird_parser_agent.h (header references core types).
#include "scratchbird/core/types.h"
#include "scratchbird/core/typed_value.h"

#include "scratchbird/ipc/parser_agent.h"
#include "scratchbird/ipc/mysql_parser_agent.h"
#include "scratchbird/ipc/postgresql_parser_agent.h"
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
};

class MySqlParserAgentHarness : public scratchbird::ipc::MySQLParserAgent {
public:
    using scratchbird::ipc::MySQLParserAgent::MySQLParserAgent;
    using scratchbird::ipc::MySQLParserAgent::mapProtocolErrorToSQLState;
    using scratchbird::ipc::MySQLParserAgent::mapSQLStateToProtocol;
};

class PostgresqlParserAgentHarness : public scratchbird::ipc::PostgreSQLParserAgent {
public:
    using scratchbird::ipc::PostgreSQLParserAgent::PostgreSQLParserAgent;
    using scratchbird::ipc::PostgreSQLParserAgent::mapProtocolErrorToSQLState;
    using scratchbird::ipc::PostgreSQLParserAgent::mapSQLStateToProtocol;
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

scratchbird::ipc::ParserAgentConfig makeParserAgentConfig(const std::string& protocol) {
    scratchbird::ipc::ParserAgentConfig cfg;
    cfg.name = "epfc024_" + protocol + "_agent";
    cfg.protocol = protocol;
    cfg.listen_endpoint = "127.0.0.1:0";
    cfg.ipc_endpoint = "/tmp/epfc024_" + protocol + ".sock";
    return cfg;
}

template <typename AdapterT>
scratchbird::core::Status compileSql(CompileHarness<AdapterT>& adapter, const std::string& sql) {
    std::vector<uint8_t> bytecode;
    std::string error;
    return adapter.runCompile(sql, bytecode, error);
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

std::vector<uint8_t> buildPostgresErrorResponsePacket(const std::string& sqlstate) {
    std::vector<uint8_t> body;
    body.push_back('S');
    body.insert(body.end(), {'E', 'R', 'R', 'O', 'R', '\0'});
    body.push_back('C');
    body.insert(body.end(), sqlstate.begin(), sqlstate.end());
    body.push_back('\0');
    body.push_back('M');
    body.insert(body.end(), {'x', '\0'});
    body.push_back('\0');

    std::vector<uint8_t> packet;
    packet.push_back('E');
    appendBe32(packet, static_cast<uint32_t>(4 + body.size()));
    packet.insert(packet.end(), body.begin(), body.end());
    return packet;
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

TEST(EmulatedParserBoundaryContractsTest, MySqlBoundaryRejectAcceptPack) {
    CompileHarness<scratchbird::protocol::MySqlAdapter> adapter(makeAdapterConfig("epfc024_mysql.sbdb"));

    EXPECT_EQ(scratchbird::core::Status::OK, compileSql(adapter, "SELECT 1"));
    EXPECT_EQ(scratchbird::core::Status::OK, compileSql(adapter, "SHOW TABLES"));
    EXPECT_NE(scratchbird::core::Status::OK,
              compileSql(adapter, "INSERT INTO t VALUES (1) RETURNING 1"));
    EXPECT_NE(scratchbird::core::Status::OK,
              compileSql(adapter, "SHOW UNKNOWN_BOUNDARY_VARIANT"));
}

TEST(EmulatedParserBoundaryContractsTest, PostgresqlBoundaryRejectAcceptPack) {
    CompileHarness<scratchbird::protocol::PostgresqlAdapter> adapter(
        makeAdapterConfig("epfc024_postgresql.sbdb"));

    EXPECT_EQ(scratchbird::core::Status::OK, compileSql(adapter, "SELECT 1"));
    EXPECT_EQ(scratchbird::core::Status::OK, compileSql(adapter, "VACUUM"));
    EXPECT_NE(scratchbird::core::Status::OK, compileSql(adapter, "SET AUTOCOMMIT = 1"));
    EXPECT_NE(scratchbird::core::Status::OK, compileSql(adapter, "SHOW TABLES"));
}

TEST(EmulatedParserBoundaryContractsTest, FirebirdBoundaryRejectAcceptPack) {
    CompileHarness<scratchbird::protocol::FirebirdAdapter> adapter(
        makeAdapterConfig("epfc024_firebird.sbdb"));

    EXPECT_EQ(scratchbird::core::Status::OK, compileSql(adapter, "SELECT 1 FROM RDB$DATABASE"));
    EXPECT_NE(scratchbird::core::Status::OK, compileSql(adapter, "SET TERM ^"));
    EXPECT_NE(scratchbird::core::Status::OK, compileSql(adapter, "DECLARE VARIABLE X INTEGER"));
    EXPECT_NE(scratchbird::core::Status::OK, compileSql(adapter, "BEGIN END"));
}

TEST(EmulatedParserBoundaryContractsTest, ErrorSqlStateTranslationDeterminism) {
    MySqlParserAgentHarness mysql_agent(makeParserAgentConfig("mysql"));
    PostgresqlParserAgentHarness pg_agent(makeParserAgentConfig("postgresql"));
    FirebirdParserAgentHarness fb_agent(makeParserAgentConfig("firebird"));

    EXPECT_EQ("42S02", mysql_agent.mapSQLStateToProtocol("42P01"));
    EXPECT_EQ("42601", pg_agent.mapSQLStateToProtocol("42000"));
    EXPECT_EQ("335544472", fb_agent.mapSQLStateToProtocol("28000"));

    std::array<char, 6> out{};

    const std::vector<uint8_t> mysql_err_wire = {
        0xFF, 0x28, 0x04, '#', '4', '2', '0', '0', '0'
    };
    mysql_agent.mapProtocolErrorToSQLState(mysql_err_wire, out.data());
    EXPECT_STREQ("42000", out.data());

    out.fill('\0');
    const std::vector<uint8_t> pg_err_wire = buildPostgresErrorResponsePacket("42P01");
    pg_agent.mapProtocolErrorToSQLState(pg_err_wire, out.data());
    EXPECT_STREQ("42P01", out.data());

    out.fill('\0');
    const std::vector<uint8_t> fb_err_wire =
        buildFirebirdErrorResponsePacket(335544472u, "28000");
    fb_agent.mapProtocolErrorToSQLState(fb_err_wire, out.data());
    EXPECT_STREQ("28000", out.data());
}
