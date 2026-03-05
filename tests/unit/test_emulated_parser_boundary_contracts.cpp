/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#include <gtest/gtest.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>

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

bool writeAllFd(int fd, const uint8_t* data, size_t len) {
    while (len > 0) {
        const ssize_t n = ::send(fd, data, len, 0);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return false;
        }
        data += static_cast<size_t>(n);
        len -= static_cast<size_t>(n);
    }
    return true;
}

bool readAllFd(int fd, uint8_t* data, size_t len) {
    while (len > 0) {
        const ssize_t n = ::recv(fd, data, len, MSG_WAITALL);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return false;
        }
        data += static_cast<size_t>(n);
        len -= static_cast<size_t>(n);
    }
    return true;
}

bool sendMySqlCommandPacket(int fd, uint8_t cmd, const std::vector<uint8_t>& args = {}) {
    std::vector<uint8_t> payload;
    payload.reserve(1 + args.size());
    payload.push_back(cmd);
    payload.insert(payload.end(), args.begin(), args.end());

    uint8_t header[4] = {
        static_cast<uint8_t>(payload.size() & 0xFF),
        static_cast<uint8_t>((payload.size() >> 8) & 0xFF),
        static_cast<uint8_t>((payload.size() >> 16) & 0xFF),
        0x00
    };

    if (!writeAllFd(fd, header, sizeof(header))) {
        return false;
    }
    if (!payload.empty() && !writeAllFd(fd, payload.data(), payload.size())) {
        return false;
    }
    return true;
}

bool recvMySqlPacketPayload(int fd, std::vector<uint8_t>& payload) {
    uint8_t header[4] = {};
    if (!readAllFd(fd, header, sizeof(header))) {
        return false;
    }
    const uint32_t len = static_cast<uint32_t>(header[0]) |
                         (static_cast<uint32_t>(header[1]) << 8) |
                         (static_cast<uint32_t>(header[2]) << 16);
    payload.resize(len);
    if (len == 0) {
        return true;
    }
    return readAllFd(fd, payload.data(), payload.size());
}

struct ParsedMySqlError {
    bool is_error = false;
    uint16_t code = 0;
    std::string sqlstate;
    std::string message;
};

ParsedMySqlError parseMySqlErrorPayload(const std::vector<uint8_t>& payload) {
    ParsedMySqlError parsed;
    if (payload.empty() || payload[0] != 0xFF) {
        return parsed;
    }
    parsed.is_error = true;
    if (payload.size() >= 3) {
        parsed.code = static_cast<uint16_t>(payload[1]) |
                      (static_cast<uint16_t>(payload[2]) << 8);
    }
    if (payload.size() >= 9 && payload[3] == '#') {
        parsed.sqlstate.assign(reinterpret_cast<const char*>(payload.data() + 4), 5);
        parsed.message.assign(reinterpret_cast<const char*>(payload.data() + 9),
                              payload.size() - 9);
    } else if (payload.size() > 3) {
        parsed.message.assign(reinterpret_cast<const char*>(payload.data() + 3),
                              payload.size() - 3);
    }
    return parsed;
}

struct ParsedMySqlOk {
    bool is_ok = false;
    uint16_t warnings = 0;
    std::string info;
};

bool parseLenEncInt(const std::vector<uint8_t>& payload, size_t& offset, uint64_t& value) {
    if (offset >= payload.size()) {
        return false;
    }
    const uint8_t first = payload[offset++];
    if (first < 0xFB) {
        value = first;
        return true;
    }
    if (first == 0xFC) {
        if (offset + 2 > payload.size()) return false;
        value = static_cast<uint64_t>(payload[offset]) |
                (static_cast<uint64_t>(payload[offset + 1]) << 8);
        offset += 2;
        return true;
    }
    if (first == 0xFD) {
        if (offset + 3 > payload.size()) return false;
        value = static_cast<uint64_t>(payload[offset]) |
                (static_cast<uint64_t>(payload[offset + 1]) << 8) |
                (static_cast<uint64_t>(payload[offset + 2]) << 16);
        offset += 3;
        return true;
    }
    if (first == 0xFE) {
        if (offset + 8 > payload.size()) return false;
        value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= (static_cast<uint64_t>(payload[offset + i]) << (i * 8));
        }
        offset += 8;
        return true;
    }
    return false;
}

ParsedMySqlOk parseMySqlOkPayload(const std::vector<uint8_t>& payload) {
    ParsedMySqlOk parsed;
    if (payload.empty() || payload[0] != 0x00) {
        return parsed;
    }
    parsed.is_ok = true;
    size_t offset = 1;
    uint64_t ignored = 0;
    if (!parseLenEncInt(payload, offset, ignored)) {
        return parsed;
    }
    if (!parseLenEncInt(payload, offset, ignored)) {
        return parsed;
    }
    if (offset + 4 <= payload.size()) {
        offset += 2;  // status flags
        parsed.warnings = static_cast<uint16_t>(payload[offset]) |
                          (static_cast<uint16_t>(payload[offset + 1]) << 8);
        offset += 2;
    }
    if (offset < payload.size()) {
        parsed.info.assign(reinterpret_cast<const char*>(payload.data() + offset),
                           payload.size() - offset);
    }
    return parsed;
}

std::vector<uint8_t> encodeLe32(uint32_t value) {
    return {
        static_cast<uint8_t>(value & 0xFF),
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>((value >> 16) & 0xFF),
        static_cast<uint8_t>((value >> 24) & 0xFF)
    };
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

TEST(EmulatedParserBoundaryContractsTest, MySqlUnsupportedComRejectContractDeterministic) {
    MySqlParserAgentHarness mysql_agent(makeParserAgentConfig("mysql"));

    struct RejectCase {
        uint8_t cmd;
        const char* command;
        const char* row;
    };

    const std::array<RejectCase, 7> reject_cases = {{
        {0x00, "COM_SLEEP", "EPFC-040"},
        {0x0F, "COM_TIME", "EPFC-042"},
        {0x10, "COM_DELAYED_INSERT", "EPFC-043"},
        {0x0B, "COM_CONNECT", "EPFC-044"},
        {0x14, "COM_CONNECT_OUT", "EPFC-045"},
        {0x1D, "COM_DAEMON", "EPFC-049"},
        {0x22, "COM_END", "EPFC-053"}
    }};

    for (const auto& tc : reject_cases) {
        int sockets[2] = {-1, -1};
        ASSERT_EQ(0, ::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));

        scratchbird::ipc::MySQLClientState state;
        state.client_fd = sockets[0];
        state.state = scratchbird::ipc::MySQLClientState::READY;

        scratchbird::core::ErrorContext ctx;
        ASSERT_TRUE(sendMySqlCommandPacket(sockets[1], tc.cmd)) << tc.command;

        const auto status = mysql_agent.handleCommand(state, &ctx);
        EXPECT_EQ(scratchbird::core::Status::OK, status) << tc.command;

        std::vector<uint8_t> payload;
        ASSERT_TRUE(recvMySqlPacketPayload(sockets[1], payload)) << tc.command;
        const ParsedMySqlError err = parseMySqlErrorPayload(payload);
        ASSERT_TRUE(err.is_error) << tc.command;
        EXPECT_EQ(1235u, err.code) << tc.command;
        EXPECT_EQ("42000", err.sqlstate) << tc.command;
        EXPECT_NE(std::string::npos, err.message.find(tc.command)) << tc.command;
        EXPECT_NE(std::string::npos, err.message.find(tc.row)) << tc.command;

        ::close(sockets[0]);
        ::close(sockets[1]);
    }
}

TEST(EmulatedParserBoundaryContractsTest, MySqlProcessKillAndCloneContracts) {
    MySqlParserAgentHarness mysql_agent(makeParserAgentConfig("mysql"));

    // COM_PROCESS_KILL with non-zero thread id should route to KILL SQL path and return OK.
    {
        int sockets[2] = {-1, -1};
        ASSERT_EQ(0, ::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));

        scratchbird::ipc::MySQLClientState state;
        state.client_fd = sockets[0];
        state.state = scratchbird::ipc::MySQLClientState::READY;

        scratchbird::core::ErrorContext ctx;
        ASSERT_TRUE(sendMySqlCommandPacket(sockets[1], 0x0C, encodeLe32(7)));
        EXPECT_EQ(scratchbird::core::Status::OK, mysql_agent.handleCommand(state, &ctx));

        std::vector<uint8_t> payload;
        ASSERT_TRUE(recvMySqlPacketPayload(sockets[1], payload));
        const ParsedMySqlOk ok = parseMySqlOkPayload(payload);
        ASSERT_TRUE(ok.is_ok);

        ::close(sockets[0]);
        ::close(sockets[1]);
    }

    // COM_CLONE should emit deterministic simulated-success OK contract with warning/info payload.
    {
        int sockets[2] = {-1, -1};
        ASSERT_EQ(0, ::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));

        scratchbird::ipc::MySQLClientState state;
        state.client_fd = sockets[0];
        state.state = scratchbird::ipc::MySQLClientState::READY;

        scratchbird::core::ErrorContext ctx;
        ASSERT_TRUE(sendMySqlCommandPacket(sockets[1], 0x20));
        EXPECT_EQ(scratchbird::core::Status::OK, mysql_agent.handleCommand(state, &ctx));

        std::vector<uint8_t> payload;
        ASSERT_TRUE(recvMySqlPacketPayload(sockets[1], payload));
        const ParsedMySqlOk ok = parseMySqlOkPayload(payload);
        ASSERT_TRUE(ok.is_ok);
        EXPECT_EQ(1u, ok.warnings);
        EXPECT_NE(std::string::npos, ok.info.find("COM_CLONE simulated"));

        ::close(sockets[0]);
        ::close(sockets[1]);
    }
}

TEST(EmulatedParserBoundaryContractsTest, MySqlDeferredComRejectContractDeterministic) {
    MySqlParserAgentHarness mysql_agent(makeParserAgentConfig("mysql"));

    struct DeferredCase {
        uint8_t cmd;
        const char* command;
    };

    const std::array<DeferredCase, 5> deferred_cases = {{
        {0x15, "COM_REGISTER_SLAVE"},
        {0x12, "COM_BINLOG_DUMP"},
        {0x13, "COM_TABLE_DUMP"},
        {0x1E, "COM_BINLOG_DUMP_GTID"},
        {0x21, "COM_SUBSCRIBE_GROUP_REPLICATION_STREAM"}
    }};

    for (const auto& tc : deferred_cases) {
        int sockets[2] = {-1, -1};
        ASSERT_EQ(0, ::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));

        scratchbird::ipc::MySQLClientState state;
        state.client_fd = sockets[0];
        state.state = scratchbird::ipc::MySQLClientState::READY;

        scratchbird::core::ErrorContext ctx;
        ASSERT_TRUE(sendMySqlCommandPacket(sockets[1], tc.cmd)) << tc.command;

        const auto status = mysql_agent.handleCommand(state, &ctx);
        EXPECT_EQ(scratchbird::core::Status::OK, status) << tc.command;

        std::vector<uint8_t> payload;
        ASSERT_TRUE(recvMySqlPacketPayload(sockets[1], payload)) << tc.command;
        const ParsedMySqlError err = parseMySqlErrorPayload(payload);
        ASSERT_TRUE(err.is_error) << tc.command;
        EXPECT_EQ(1235u, err.code) << tc.command;
        EXPECT_EQ("42000", err.sqlstate) << tc.command;
        EXPECT_NE(std::string::npos, err.message.find(tc.command)) << tc.command;
        EXPECT_NE(std::string::npos, err.message.find("not yet supported")) << tc.command;

        ::close(sockets[0]);
        ::close(sockets[1]);
    }
}
