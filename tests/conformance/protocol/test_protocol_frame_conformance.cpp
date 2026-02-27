#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "scratchbird/protocol/sbwp_protocol.h"
#include "scratchbird/protocol/adapters/postgresql_adapter.h"
#include "scratchbird/protocol/adapters/mysql_adapter.h"
#include "scratchbird/protocol/adapters/firebird_adapter.h"

namespace scratchbird::tests {
namespace {
namespace sbwp = scratchbird::protocol::sbwp;
namespace pg = scratchbird::protocol::pg;
namespace mysql = scratchbird::protocol::mysql;
namespace fb = scratchbird::protocol::firebird;

std::filesystem::path dbPath(const std::string& name) {
    return std::filesystem::path("build") / "database" / name;
}

void cleanupDb(const std::string& name) {
    std::error_code ec;
    std::filesystem::remove(dbPath(name), ec);
    std::filesystem::create_directories(dbPath(name).parent_path(), ec);
}

class PgHarness : public scratchbird::protocol::PostgresqlAdapter {
public:
    using scratchbird::protocol::PostgresqlAdapter::PostgresqlAdapter;

    core::Status parseIncoming(network::Connection* conn) {
        return parseMessage(conn);
    }

    core::Status processIncoming(network::Connection* conn) {
        return processMessage(conn);
    }

    core::Status forceAuthSuccess(network::Connection* conn) {
        return sendAuthResult(conn, true);
    }
};

class MySqlHarness : public scratchbird::protocol::MySqlAdapter {
public:
    using scratchbird::protocol::MySqlAdapter::MySqlAdapter;

    core::Status parseIncoming(network::Connection* conn) {
        return parseMessage(conn);
    }

    core::Status processIncoming(network::Connection* conn) {
        return processMessage(conn);
    }

    core::Status forceAuthSuccess(network::Connection* conn) {
        return sendAuthResult(conn, true);
    }
};

class FirebirdHarness : public scratchbird::protocol::FirebirdAdapter {
public:
    using scratchbird::protocol::FirebirdAdapter::FirebirdAdapter;

    core::Status parseIncoming(network::Connection* conn) {
        return parseMessage(conn);
    }

    core::Status processIncoming(network::Connection* conn) {
        return processMessage(conn);
    }
};

void writeU32BE(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

std::vector<uint8_t> buildPgFrontendMessage(uint8_t type, const std::vector<uint8_t>& payload, uint32_t length_override = 0) {
    std::vector<uint8_t> packet;
    packet.reserve(1 + 4 + payload.size());
    packet.push_back(type);
    const uint32_t len = length_override == 0 ? static_cast<uint32_t>(4 + payload.size()) : length_override;
    writeU32BE(packet, len);
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

std::vector<uint8_t> buildPgStartupPacket(uint32_t protocol_version) {
    std::vector<uint8_t> payload;
    writeU32BE(payload, protocol_version);
    payload.push_back(0);  // empty parameter list terminator

    std::vector<uint8_t> packet;
    const uint32_t len = static_cast<uint32_t>(4 + payload.size());
    writeU32BE(packet, len);
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

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

uint32_t readU32BE(const std::vector<uint8_t>& data, size_t offset = 0) {
    if (offset + 4 > data.size()) {
        return 0;
    }
    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
            static_cast<uint32_t>(data[offset + 3]);
}

std::vector<char> extractPgBackendMessageTypes(const std::vector<uint8_t>& stream) {
    std::vector<char> types;
    size_t offset = 0;
    while (offset + 5 <= stream.size()) {
        const char msg_type = static_cast<char>(stream[offset]);
        const uint32_t msg_len =
            (static_cast<uint32_t>(stream[offset + 1]) << 24) |
            (static_cast<uint32_t>(stream[offset + 2]) << 16) |
            (static_cast<uint32_t>(stream[offset + 3]) << 8) |
            static_cast<uint32_t>(stream[offset + 4]);
        if (msg_len < 4) {
            break;
        }
        const size_t frame_len = 1 + static_cast<size_t>(msg_len);
        if (offset + frame_len > stream.size()) {
            break;
        }
        types.push_back(msg_type);
        offset += frame_len;
    }
    return types;
}

TEST(ProtocolFrameConformance, SbwpRejectsUnsupportedVersionHeader) {
    std::vector<uint8_t> header(sbwp::kHeaderSize, 0);
    header[0] = 'S';
    header[1] = 'B';
    header[2] = 'W';
    header[3] = 'P';
    header[4] = 9;  // wrong major
    header[5] = 9;  // wrong minor
    header[6] = static_cast<uint8_t>(sbwp::MessageType::Query);

    sbwp::MessageHeader decoded;
    core::ErrorContext ctx;
    const auto status = sbwp::decodeHeader(header, decoded, &ctx);
    EXPECT_EQ(status, core::Status::PROTOCOL_VIOLATION);
}

TEST(ProtocolFrameConformance, SbwpRejectsOversizedPayloadLength) {
    sbwp::MessageHeader header;
    header.type = sbwp::MessageType::Query;
    header.flags = 0;
    header.sequence = 1;
    header.txn_id = 1;

    std::vector<uint8_t> frame = sbwp::encodeMessage(header, {});
    ASSERT_EQ(frame.size(), sbwp::kHeaderSize);

    const uint32_t oversized = static_cast<uint32_t>(sbwp::kMaxMessageSize + 1);
    frame[8] = static_cast<uint8_t>(oversized & 0xFF);
    frame[9] = static_cast<uint8_t>((oversized >> 8) & 0xFF);
    frame[10] = static_cast<uint8_t>((oversized >> 16) & 0xFF);
    frame[11] = static_cast<uint8_t>((oversized >> 24) & 0xFF);

    sbwp::MessageHeader decoded;
    core::ErrorContext ctx;
    const auto status = sbwp::decodeHeader(frame, decoded, &ctx);
    EXPECT_EQ(status, core::Status::PROTOCOL_VIOLATION);
}

TEST(ProtocolFrameConformance, PostgreSqlRejectsInvalidMessageLength) {
    cleanupDb("a55_negative_pg_invalid_len.sbdb");

    protocol::ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_negative_pg_invalid_len.sbdb").string();

    PgHarness adapter(cfg);
    network::Connection conn(nullptr, 901);
    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    const auto bad_packet = buildPgFrontendMessage(
        static_cast<uint8_t>(pg::FrontendMsg::QUERY),
        {},
        /*length_override=*/3);
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), bad_packet.begin(), bad_packet.end());

    EXPECT_EQ(adapter.parseIncoming(&conn), core::Status::INVALID_ARGUMENT);
}

TEST(ProtocolFrameConformance, PostgreSqlDowngradeRequestIsRefused) {
    cleanupDb("a55_negative_pg_downgrade.sbdb");

    protocol::ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_negative_pg_downgrade.sbdb").string();

    PgHarness adapter(cfg);
    network::Connection conn(nullptr, 902);

    const auto startup_v2_packet = buildPgStartupPacket((2u << 16));
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), startup_v2_packet.begin(), startup_v2_packet.end());

    ASSERT_EQ(adapter.parseIncoming(&conn), core::Status::OK);
    EXPECT_EQ(adapter.processIncoming(&conn), core::Status::NOT_SUPPORTED);

    const auto message_types = extractPgBackendMessageTypes(conn.getWriteBuffer());
    ASSERT_EQ(message_types.size(), 1u);
    EXPECT_EQ(message_types[0], pg::BackendMsg::ERROR_RESPONSE);
}

TEST(ProtocolFrameConformance, MySqlUnsupportedCommandReturnsErrPacket) {
    cleanupDb("a55_negative_mysql_unknown_cmd.sbdb");

    protocol::ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_negative_mysql_unknown_cmd.sbdb").string();

    MySqlHarness adapter(cfg);
    network::Connection conn(nullptr, 903);
    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    const std::vector<uint8_t> payload = {0xFF};
    const auto packet = buildMySqlWirePacket(payload, 0);
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());

    ASSERT_EQ(adapter.parseIncoming(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncoming(&conn), core::Status::OK);

    const auto err_payload = extractMySqlPayload(conn.getWriteBuffer());
    ASSERT_GE(err_payload.size(), 1u);
    EXPECT_EQ(err_payload[0], mysql::ERR_PACKET);
}

TEST(ProtocolFrameConformance, FirebirdUnsupportedOpcodeReturnsErrorResponse) {
    cleanupDb("a55_negative_firebird_unknown_opcode.sbdb");

    protocol::ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_negative_firebird_unknown_opcode.sbdb").string();

    FirebirdHarness adapter(cfg);
    network::Connection conn(nullptr, 904);

    std::vector<uint8_t> packet;
    writeU32BE(packet, 0x7FFFFFFF);
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());

    ASSERT_EQ(adapter.parseIncoming(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncoming(&conn), core::Status::OK);

    const auto& out = conn.getWriteBuffer();
    ASSERT_GE(out.size(), 4u);
    EXPECT_EQ(readU32BE(out), fb::Opcode::op_response);
}

} // namespace
} // namespace scratchbird::tests
