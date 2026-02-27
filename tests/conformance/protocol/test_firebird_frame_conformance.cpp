#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "scratchbird/protocol/adapters/firebird_adapter.h"

#ifndef SB_PROTOCOL_CONFORMANCE_DIR
#define SB_PROTOCOL_CONFORMANCE_DIR "."
#endif

namespace scratchbird::tests {
namespace {
using scratchbird::protocol::FirebirdAdapter;
using scratchbird::protocol::ProtocolAdapterConfig;
namespace fb = scratchbird::protocol::firebird;

std::filesystem::path dbPath(const std::string& name) {
    return std::filesystem::path("build") / "database" / name;
}

void cleanupDb(const std::string& name) {
    std::error_code ec;
    std::filesystem::remove(dbPath(name), ec);
    std::filesystem::create_directories(dbPath(name).parent_path(), ec);
}

class FirebirdHarness : public FirebirdAdapter {
public:
    using FirebirdAdapter::FirebirdAdapter;

    core::Status parseIncoming(network::Connection* conn) {
        return parseMessage(conn);
    }

    core::Status processIncoming(network::Connection* conn) {
        return processMessage(conn);
    }

    core::Status sendProtocolErrorForTest(network::Connection* conn,
                                          uint32_t error_code,
                                          const std::string& sqlstate,
                                          const std::string& message) {
        return sendProtocolError(conn, error_code, sqlstate, message);
    }
};

void writeU32BE(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

std::vector<uint8_t> buildFirebirdPacket(uint32_t opcode, const std::vector<uint8_t>& body = {}) {
    std::vector<uint8_t> packet;
    packet.reserve(4 + body.size());
    writeU32BE(packet, opcode);
    packet.insert(packet.end(), body.begin(), body.end());
    return packet;
}

std::vector<uint8_t> buildConnectBody() {
    std::vector<uint8_t> body;
    // handleConnect reads 6 uint32 values after opcode; keep deterministic.
    writeU32BE(body, fb::Opcode::op_attach);              // requested operation
    writeU32BE(body, fb::DEFAULT_PROTOCOL_VERSION);       // client version
    writeU32BE(body, fb::ARCH_GENERIC);                   // architecture
    writeU32BE(body, 1);                                  // min type
    writeU32BE(body, 1);                                  // max type
    writeU32BE(body, 0);                                  // preference/protocol bitmap placeholder
    return body;
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
const std::filesystem::path kGoldenFirebirdDir = kProtocolRoot / "golden" / "firebird";

TEST(FirebirdFrameConformance, GoldenTraceMetadataExistsForRequiredScenarios) {
    const std::vector<std::pair<std::string, std::string>> required = {
        {"01_connect_accept.trace", "connect_accept"},
        {"02_ping.trace", "ping"},
        {"03_cancel.trace", "cancel"},
        {"04_invalid_opcode.trace", "invalid_opcode"},
        {"05_protocol_error.trace", "protocol_error"},
    };

    for (const auto& item : required) {
        const auto path = kGoldenFirebirdDir / item.first;
        EXPECT_TRUE(std::filesystem::exists(path)) << path;
        const auto meta = readMetadata(path);
        ASSERT_TRUE(meta.count("scenario") > 0) << path;
        EXPECT_EQ(meta.at("scenario"), item.second);
    }
}

TEST(FirebirdFrameConformance, ConnectPacketReturnsAcceptOpcode) {
    cleanupDb("a55_firebird_frame_connect.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_firebird_frame_connect.sbdb").string();
    cfg.require_authentication = false;

    FirebirdHarness adapter(cfg);
    network::Connection conn(nullptr, 801);

    const auto packet = buildFirebirdPacket(fb::Opcode::op_connect, buildConnectBody());
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());

    ASSERT_EQ(adapter.parseIncoming(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncoming(&conn), core::Status::OK);

    const auto& response = conn.getWriteBuffer();
    ASSERT_GE(response.size(), 4u);
    EXPECT_EQ(readU32BE(response), fb::Opcode::op_accept);
}

TEST(FirebirdFrameConformance, PingAndCancelReturnResponseOpcode) {
    cleanupDb("a55_firebird_frame_ping_cancel.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_firebird_frame_ping_cancel.sbdb").string();

    FirebirdHarness adapter(cfg);
    network::Connection conn(nullptr, 802);

    auto send_op = [&](uint32_t opcode) {
        conn.clearWriteBuffer();
        auto packet = buildFirebirdPacket(opcode);
        auto& read_buffer = conn.getReadBuffer();
        read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());
        ASSERT_EQ(adapter.parseIncoming(&conn), core::Status::OK);
        ASSERT_EQ(adapter.processIncoming(&conn), core::Status::OK);
        const auto& out = conn.getWriteBuffer();
        ASSERT_GE(out.size(), 4u);
        EXPECT_EQ(readU32BE(out), fb::Opcode::op_response);
    };

    send_op(fb::Opcode::op_ping);
    send_op(fb::Opcode::op_cancel);
}

TEST(FirebirdFrameConformance, InvalidOpcodeReturnsErrorResponseOpcode) {
    cleanupDb("a55_firebird_frame_invalid_opcode.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_firebird_frame_invalid_opcode.sbdb").string();

    FirebirdHarness adapter(cfg);
    network::Connection conn(nullptr, 803);

    const uint32_t invalid_opcode = 0x7FFFFFFF;
    auto packet = buildFirebirdPacket(invalid_opcode);
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());

    ASSERT_EQ(adapter.parseIncoming(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncoming(&conn), core::Status::OK);

    const auto& out = conn.getWriteBuffer();
    ASSERT_GE(out.size(), 4u);
    EXPECT_EQ(readU32BE(out), fb::Opcode::op_response);
}

TEST(FirebirdFrameConformance, ProtocolErrorUsesResponseOpcode) {
    cleanupDb("a55_firebird_frame_protocol_error.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_firebird_frame_protocol_error.sbdb").string();

    FirebirdHarness adapter(cfg);
    network::Connection conn(nullptr, 804);

    ASSERT_EQ(adapter.sendProtocolErrorForTest(
                  &conn,
                  static_cast<uint32_t>(core::Status::INVALID_ARGUMENT),
                  "",
                  "invalid argument"),
              core::Status::OK);

    const auto& out = conn.getWriteBuffer();
    ASSERT_GE(out.size(), 4u);
    EXPECT_EQ(readU32BE(out), fb::Opcode::op_response);
}

} // namespace
} // namespace scratchbird::tests
