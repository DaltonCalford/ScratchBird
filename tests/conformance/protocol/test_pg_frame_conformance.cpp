#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "scratchbird/protocol/adapters/postgresql_adapter.h"

#ifndef SB_PROTOCOL_CONFORMANCE_DIR
#define SB_PROTOCOL_CONFORMANCE_DIR "."
#endif

namespace scratchbird::tests {
namespace {
using scratchbird::protocol::PostgresqlAdapter;
using scratchbird::protocol::ProtocolAdapterConfig;
namespace pg = scratchbird::protocol::pg;

std::filesystem::path dbPath(const std::string& name) {
    return std::filesystem::path("build") / "database" / name;
}

void cleanupDb(const std::string& name) {
    std::error_code ec;
    std::filesystem::remove(dbPath(name), ec);
    std::filesystem::create_directories(dbPath(name).parent_path(), ec);
}

class PgHarness : public PostgresqlAdapter {
public:
    using PostgresqlAdapter::PostgresqlAdapter;

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

void writePgInt32(std::vector<uint8_t>& payload, uint32_t value) {
    payload.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    payload.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    payload.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(value & 0xFF));
}

void writePgCString(std::vector<uint8_t>& payload, const std::string& value) {
    payload.insert(payload.end(), value.begin(), value.end());
    payload.push_back('\0');
}

std::vector<uint8_t> buildPgFrontendMessage(uint8_t type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> packet;
    packet.reserve(1 + 4 + payload.size());
    packet.push_back(type);
    writePgInt32(packet, static_cast<uint32_t>(4 + payload.size()));
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

std::vector<uint8_t> buildPgStartupCancelRequest(uint32_t pid, uint32_t key) {
    std::vector<uint8_t> packet;
    packet.reserve(16);
    writePgInt32(packet, 16);
    writePgInt32(packet, static_cast<uint32_t>(pg::CANCEL_REQUEST));
    writePgInt32(packet, pid);
    writePgInt32(packet, key);
    return packet;
}

std::vector<uint8_t> buildPgParsePayload(const std::string& statement_name,
                                         const std::string& query) {
    std::vector<uint8_t> payload;
    writePgCString(payload, statement_name);
    writePgCString(payload, query);
    payload.push_back(0);
    payload.push_back(0);  // num_params int16
    return payload;
}

std::vector<uint8_t> buildPgBindPayload(const std::string& portal_name,
                                        const std::string& statement_name) {
    std::vector<uint8_t> payload;
    writePgCString(payload, portal_name);
    writePgCString(payload, statement_name);
    payload.push_back(0);
    payload.push_back(0);  // num_format_codes int16
    payload.push_back(0);
    payload.push_back(0);  // num_params int16
    payload.push_back(0);
    payload.push_back(0);  // num_result_formats int16
    return payload;
}

std::vector<uint8_t> buildPgExecutePayload(const std::string& portal_name, uint32_t max_rows) {
    std::vector<uint8_t> payload;
    writePgCString(payload, portal_name);
    writePgInt32(payload, max_rows);
    return payload;
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

core::Status sendPgFrontendPacket(PgHarness& adapter,
                                  network::Connection* conn,
                                  uint8_t type,
                                  const std::vector<uint8_t>& payload) {
    const auto packet = buildPgFrontendMessage(type, payload);
    auto& read_buffer = conn->getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());
    auto parse_status = adapter.parseIncoming(conn);
    if (parse_status != core::Status::OK) {
        return parse_status;
    }
    return adapter.processIncoming(conn);
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
const std::filesystem::path kGoldenPgDir = kProtocolRoot / "golden" / "pg";

TEST(PGFrameConformance, GoldenTraceMetadataExistsForRequiredScenarios) {
    const std::vector<std::pair<std::string, std::string>> required = {
        {"01_auth_handshake.trace", "auth_handshake"},
        {"02_empty_query.trace", "empty_query"},
        {"03_parse_bind_sync.trace", "parse_bind_sync"},
        {"04_cancel_request.trace", "cancel_request"},
        {"05_execute_missing_portal.trace", "execute_missing_portal"},
    };

    for (const auto& item : required) {
        const auto path = kGoldenPgDir / item.first;
        EXPECT_TRUE(std::filesystem::exists(path)) << path;
        const auto meta = readMetadata(path);
        ASSERT_TRUE(meta.count("scenario") > 0) << path;
        EXPECT_EQ(meta.at("scenario"), item.second);
    }
}

TEST(PGFrameConformance, AuthHandshakeFramesAreOrdered) {
    cleanupDb("a55_pg_frame_handshake.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_pg_frame_handshake.sbdb").string();

    PgHarness adapter(cfg);
    network::Connection conn(nullptr, 601);

    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);

    const auto types = extractPgBackendMessageTypes(conn.getWriteBuffer());
    ASSERT_GE(types.size(), 4u);
    EXPECT_EQ(types.front(), pg::BackendMsg::AUTHENTICATION);
    EXPECT_EQ(types[1], pg::BackendMsg::BACKEND_KEY_DATA);
    EXPECT_EQ(types.back(), pg::BackendMsg::READY_FOR_QUERY);
    EXPECT_TRUE(std::find(types.begin(), types.end(), pg::BackendMsg::PARAMETER_STATUS) != types.end());
}

TEST(PGFrameConformance, EmptyQueryFramesHaveDeterministicShape) {
    cleanupDb("a55_pg_frame_empty_query.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_pg_frame_empty_query.sbdb").string();

    PgHarness adapter(cfg);
    network::Connection conn(nullptr, 602);
    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    std::vector<uint8_t> empty_query_payload = {'\0'};
    ASSERT_EQ(sendPgFrontendPacket(adapter,
                                   &conn,
                                   static_cast<uint8_t>(pg::FrontendMsg::QUERY),
                                   empty_query_payload),
              core::Status::OK);

    const auto types = extractPgBackendMessageTypes(conn.getWriteBuffer());
    ASSERT_EQ(types.size(), 2u);
    EXPECT_EQ(types[0], pg::BackendMsg::EMPTY_QUERY_RESPONSE);
    EXPECT_EQ(types[1], pg::BackendMsg::READY_FOR_QUERY);
}

TEST(PGFrameConformance, ParseBindSyncFramesAreOrdered) {
    cleanupDb("a55_pg_frame_parse_bind_sync.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_pg_frame_parse_bind_sync.sbdb").string();

    PgHarness adapter(cfg);
    network::Connection conn(nullptr, 603);
    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    ASSERT_EQ(sendPgFrontendPacket(adapter,
                                   &conn,
                                   static_cast<uint8_t>(pg::FrontendMsg::PARSE),
                                   buildPgParsePayload("a55_stmt", "SELECT 1")),
              core::Status::OK);
    ASSERT_EQ(sendPgFrontendPacket(adapter,
                                   &conn,
                                   static_cast<uint8_t>(pg::FrontendMsg::BIND),
                                   buildPgBindPayload("a55_portal", "a55_stmt")),
              core::Status::OK);
    ASSERT_EQ(sendPgFrontendPacket(adapter,
                                   &conn,
                                   static_cast<uint8_t>(pg::FrontendMsg::SYNC),
                                   {}),
              core::Status::OK);

    const auto types = extractPgBackendMessageTypes(conn.getWriteBuffer());
    ASSERT_EQ(types.size(), 3u);
    EXPECT_EQ(types[0], pg::BackendMsg::PARSE_COMPLETE);
    EXPECT_EQ(types[1], pg::BackendMsg::BIND_COMPLETE);
    EXPECT_EQ(types[2], pg::BackendMsg::READY_FOR_QUERY);
}

TEST(PGFrameConformance, CancelRequestStartupFrameIsAccepted) {
    cleanupDb("a55_pg_frame_cancel.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_pg_frame_cancel.sbdb").string();

    PgHarness adapter(cfg);
    network::Connection conn(nullptr, 604);

    const auto cancel_packet = buildPgStartupCancelRequest(/*pid=*/1234, /*key=*/9999);
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), cancel_packet.begin(), cancel_packet.end());

    ASSERT_EQ(adapter.parseIncoming(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncoming(&conn), core::Status::OK);
}

TEST(PGFrameConformance, ExecuteMissingPortalReturnsErrorThenReadyOnSync) {
    cleanupDb("a55_pg_frame_missing_portal.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("a55_pg_frame_missing_portal.sbdb").string();

    PgHarness adapter(cfg);
    network::Connection conn(nullptr, 605);
    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    EXPECT_EQ(sendPgFrontendPacket(adapter,
                                   &conn,
                                   static_cast<uint8_t>(pg::FrontendMsg::EXECUTE),
                                   buildPgExecutePayload("missing_portal", 0)),
              core::Status::NOT_FOUND);

    auto pre_sync_types = extractPgBackendMessageTypes(conn.getWriteBuffer());
    ASSERT_EQ(pre_sync_types.size(), 1u);
    EXPECT_EQ(pre_sync_types[0], pg::BackendMsg::ERROR_RESPONSE);

    ASSERT_EQ(sendPgFrontendPacket(adapter,
                                   &conn,
                                   static_cast<uint8_t>(pg::FrontendMsg::SYNC),
                                   {}),
              core::Status::OK);

    const auto types = extractPgBackendMessageTypes(conn.getWriteBuffer());
    ASSERT_EQ(types.size(), 2u);
    EXPECT_EQ(types[0], pg::BackendMsg::ERROR_RESPONSE);
    EXPECT_EQ(types[1], pg::BackendMsg::READY_FOR_QUERY);
}

} // namespace
} // namespace scratchbird::tests
