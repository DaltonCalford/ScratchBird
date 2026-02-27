#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "scratchbird/protocol/adapters/mysql_adapter.h"

#ifndef SB_PROTOCOL_CONFORMANCE_DIR
#define SB_PROTOCOL_CONFORMANCE_DIR "."
#endif

namespace scratchbird::tests {
namespace {
using scratchbird::protocol::MySqlAdapter;
using scratchbird::protocol::ProtocolAdapterConfig;
namespace mysql = scratchbird::protocol::mysql;

std::filesystem::path dbPath(const std::string& name) {
    return std::filesystem::path("build") / "database" / name;
}

void cleanupDb(const std::string& name) {
    std::error_code ec;
    std::filesystem::remove(dbPath(name), ec);
    std::filesystem::create_directories(dbPath(name).parent_path(), ec);
}

class MySqlHarness : public MySqlAdapter {
public:
    using MySqlAdapter::MySqlAdapter;

    core::Status parseIncoming(network::Connection* conn) {
        return parseMessage(conn);
    }

    core::Status processIncoming(network::Connection* conn) {
        return processMessage(conn);
    }

    core::Status sendGreetingForTest(network::Connection* conn) {
        return sendGreeting(conn);
    }

    core::Status forceAuthSuccess(network::Connection* conn) {
        return sendAuthResult(conn, true);
    }

    core::Status sendProtocolErrorForTest(network::Connection* conn,
                                          uint32_t error_code,
                                          const std::string& sqlstate,
                                          const std::string& message) {
        return sendProtocolError(conn, error_code, sqlstate, message);
    }

    void setClientCapabilitiesForTest(uint32_t capabilities) {
        MySqlAdapter::setClientCapabilitiesForTest(capabilities);
    }
};

std::vector<uint8_t> buildMySqlWirePacket(const std::vector<uint8_t>& payload, uint8_t sequence = 0) {
    std::vector<uint8_t> packet;
    packet.reserve(4 + payload.size());
    packet.push_back(static_cast<uint8_t>(payload.size() & 0xFF));
    packet.push_back(static_cast<uint8_t>((payload.size() >> 8) & 0xFF));
    packet.push_back(static_cast<uint8_t>((payload.size() >> 16) & 0xFF));
    packet.push_back(sequence);
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

std::vector<uint8_t> extractMySqlPayload(const std::vector<uint8_t>& wire_packet) {
    if (wire_packet.size() < 4) {
        return {};
    }
    const size_t payload_size =
        static_cast<size_t>(wire_packet[0]) |
        (static_cast<size_t>(wire_packet[1]) << 8) |
        (static_cast<size_t>(wire_packet[2]) << 16);
    if (wire_packet.size() < 4 + payload_size) {
        return {};
    }
    return std::vector<uint8_t>(wire_packet.begin() + 4, wire_packet.begin() + 4 + payload_size);
}

uint16_t readMySqlErrorCode(const std::vector<uint8_t>& payload) {
    if (payload.size() < 3) {
        return 0;
    }
    return static_cast<uint16_t>(payload[1]) |
           (static_cast<uint16_t>(payload[2]) << 8);
}

std::string readMySqlSqlState(const std::vector<uint8_t>& payload) {
    if (payload.size() < 9 || payload[3] != '#') {
        return std::string{};
    }
    return std::string(reinterpret_cast<const char*>(payload.data() + 4), 5);
}

std::map<std::string, std::string> readMetadata(const std::filesystem::path& file) {
    std::ifstream in(file);
    std::map<std::string, std::string> fields;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const size_t sep = line.find('=');
        if (sep == std::string::npos || sep == 0 || sep + 1 >= line.size()) {
            continue;
        }
        fields[line.substr(0, sep)] = line.substr(sep + 1);
    }
    return fields;
}

const std::filesystem::path kProtocolRoot = std::filesystem::path(SB_PROTOCOL_CONFORMANCE_DIR);
const std::filesystem::path kGoldenMySqlDir = kProtocolRoot / "golden" / "mysql";

TEST(MySQLFrameConformance, GoldenTraceMetadataExistsForRequiredScenarios) {
    const std::vector<std::pair<std::string, std::string>> required = {
        {"01_greeting.trace", "greeting"},
        {"02_ping.trace", "ping"},
        {"03_stmt_prepare.trace", "stmt_prepare"},
        {"04_stmt_execute_missing.trace", "stmt_execute_missing"},
        {"05_protocol_error.trace", "protocol_error"},
    };

    for (const auto& item : required) {
        const auto path = kGoldenMySqlDir / item.first;
        EXPECT_TRUE(std::filesystem::exists(path)) << path;
        const auto meta = readMetadata(path);
        ASSERT_TRUE(meta.count("scenario") > 0) << path;
        EXPECT_EQ(meta.at("scenario"), item.second);
    }
}

TEST(MySQLFrameConformance, GreetingPacketHeaderHasValidPayloadLength) {
    cleanupDb("a55_mysql_frame_greeting.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_mysql_frame_greeting.sbdb").string();

    MySqlHarness adapter(cfg);
    network::Connection conn(nullptr, 701);

    ASSERT_EQ(adapter.sendGreetingForTest(&conn), core::Status::OK);
    const auto& wire_packet = conn.getWriteBuffer();
    ASSERT_GE(wire_packet.size(), 4u);

    const size_t payload_size =
        static_cast<size_t>(wire_packet[0]) |
        (static_cast<size_t>(wire_packet[1]) << 8) |
        (static_cast<size_t>(wire_packet[2]) << 16);
    EXPECT_GT(payload_size, 0u);
    EXPECT_EQ(wire_packet.size(), payload_size + 4u);
}

TEST(MySQLFrameConformance, ComPingReturnsOkPacket) {
    cleanupDb("a55_mysql_frame_ping.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_mysql_frame_ping.sbdb").string();

    MySqlHarness adapter(cfg);
    network::Connection conn(nullptr, 702);
    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    std::vector<uint8_t> payload = {mysql::Command::COM_PING};
    const auto packet = buildMySqlWirePacket(payload, 0);
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());

    ASSERT_EQ(adapter.parseIncoming(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncoming(&conn), core::Status::OK);

    const auto response_payload = extractMySqlPayload(conn.getWriteBuffer());
    ASSERT_GE(response_payload.size(), 1u);
    EXPECT_EQ(response_payload[0], mysql::OK_PACKET);
}

TEST(MySQLFrameConformance, ComStmtPrepareReturnsPrepareOkPacket) {
    cleanupDb("a55_mysql_frame_stmt_prepare.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_mysql_frame_stmt_prepare.sbdb").string();

    MySqlHarness adapter(cfg);
    network::Connection conn(nullptr, 703);
    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    std::vector<uint8_t> payload;
    payload.push_back(mysql::Command::COM_STMT_PREPARE);
    const std::string sql = "SELECT 1";
    payload.insert(payload.end(), sql.begin(), sql.end());

    const auto packet = buildMySqlWirePacket(payload, 0);
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());

    ASSERT_EQ(adapter.parseIncoming(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncoming(&conn), core::Status::OK);

    const auto response_payload = extractMySqlPayload(conn.getWriteBuffer());
    ASSERT_GE(response_payload.size(), 12u);
    EXPECT_EQ(response_payload[0], mysql::OK_PACKET);

    const uint32_t stmt_id =
        static_cast<uint32_t>(response_payload[1]) |
        (static_cast<uint32_t>(response_payload[2]) << 8) |
        (static_cast<uint32_t>(response_payload[3]) << 16) |
        (static_cast<uint32_t>(response_payload[4]) << 24);
    EXPECT_GT(stmt_id, 0u);
}

TEST(MySQLFrameConformance, ComStmtExecuteUnknownStatementReturnsError) {
    cleanupDb("a55_mysql_frame_stmt_execute_missing.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_mysql_frame_stmt_execute_missing.sbdb").string();

    MySqlHarness adapter(cfg);
    adapter.setClientCapabilitiesForTest(mysql::Capability::PROTOCOL_41);
    network::Connection conn(nullptr, 704);
    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    std::vector<uint8_t> payload;
    payload.push_back(mysql::Command::COM_STMT_EXECUTE);
    payload.push_back(0xEF);
    payload.push_back(0xBE);
    payload.push_back(0xAD);
    payload.push_back(0xDE);

    const auto packet = buildMySqlWirePacket(payload, 0);
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());

    ASSERT_EQ(adapter.parseIncoming(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncoming(&conn), core::Status::OK);

    const auto response_payload = extractMySqlPayload(conn.getWriteBuffer());
    ASSERT_GE(response_payload.size(), 9u);
    EXPECT_EQ(response_payload[0], mysql::ERR_PACKET);
    EXPECT_EQ(readMySqlErrorCode(response_payload), mysql::ErrorCode::UNKNOWN_ERROR);
    EXPECT_EQ(readMySqlSqlState(response_payload), "HY000");
}

TEST(MySQLFrameConformance, ProtocolErrorMappingInvalidAuthorizationUses28000) {
    cleanupDb("a55_mysql_frame_protocol_error.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_mysql_frame_protocol_error.sbdb").string();

    MySqlHarness adapter(cfg);
    adapter.setClientCapabilitiesForTest(mysql::Capability::PROTOCOL_41);
    network::Connection conn(nullptr, 705);

    ASSERT_EQ(adapter.sendProtocolErrorForTest(
                  &conn,
                  static_cast<uint32_t>(core::Status::INVALID_AUTHORIZATION),
                  "",
                  "Access denied"),
              core::Status::OK);

    const auto payload = extractMySqlPayload(conn.getWriteBuffer());
    ASSERT_GE(payload.size(), 9u);
    EXPECT_EQ(payload[0], mysql::ERR_PACKET);
    EXPECT_EQ(readMySqlErrorCode(payload), mysql::ErrorCode::ACCESS_DENIED);
    EXPECT_EQ(readMySqlSqlState(payload), "28000");
}

} // namespace
} // namespace scratchbird::tests
