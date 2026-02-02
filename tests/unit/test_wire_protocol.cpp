/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Unit Tests for Wire Protocol
 *
 * ScratchBird Local Server Architecture - Phase 2
 *
 * Tests:
 * - Message header serialization/deserialization
 * - Payload encoding/decoding for all data types
 * - Protocol codec for all message types
 * - ProtocolSession message I/O
 * - Edge cases and error handling
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <filesystem>
#include <unistd.h>

#include "scratchbird/protocol/wire_protocol.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/core/error_context.h"
#include "test_helpers.h"

using namespace scratchbird::protocol;
using namespace scratchbird::server;
using namespace scratchbird::core;

namespace {
bool isNetworkRestrictedError(const ErrorContext& ctx) {
    return ctx.message.find("Operation not permitted") != std::string::npos ||
           ctx.message.find("Permission denied") != std::string::npos;
}
} // namespace

// ============================================================================
// Message Header Tests
// ============================================================================

class MessageHeaderTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MessageHeaderTest, DefaultConstructor) {
    Message msg;
    EXPECT_EQ(msg.getPayloadLength(), 0u);
    EXPECT_TRUE(msg.isValid());
}

TEST_F(MessageHeaderTest, TypedConstructor) {
    Message msg(MessageType::QUERY);
    EXPECT_EQ(msg.getType(), MessageType::QUERY);
    EXPECT_EQ(msg.getPayloadLength(), 0u);
    EXPECT_TRUE(msg.isValid());
}

TEST_F(MessageHeaderTest, Serialization) {
    Message msg(MessageType::PING);
    msg.writeUInt64(12345678);
    msg.writeUInt32(42);

    std::vector<uint8_t> buffer;
    Status status = msg.serialize(buffer);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(buffer.size(), sizeof(MessageHeader) + 12);  // 8 + 4 bytes payload

    // Verify header
    MessageHeader header;
    status = Message::parseHeader(buffer.data(), header);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(header.magic, PROTOCOL_MAGIC);
    EXPECT_EQ(header.version, PROTOCOL_VERSION);
    EXPECT_EQ(header.type, static_cast<uint8_t>(MessageType::PING));
    EXPECT_EQ(header.payload_length, 12u);
}

TEST_F(MessageHeaderTest, InvalidMagic) {
    uint8_t bad_header[12] = {0};
    // Wrong magic
    bad_header[0] = 'X';

    MessageHeader header;
    ErrorContext ctx;
    Status status = Message::parseHeader(bad_header, header, &ctx);
    EXPECT_NE(status, Status::OK);
}

TEST_F(MessageHeaderTest, MoveConstructor) {
    Message msg1(MessageType::QUERY);
    msg1.writeString("SELECT * FROM test");

    size_t original_size = msg1.getPayloadSize();

    Message msg2(std::move(msg1));
    EXPECT_EQ(msg2.getType(), MessageType::QUERY);
    EXPECT_EQ(msg2.getPayloadSize(), original_size);
    EXPECT_EQ(msg1.getPayloadSize(), 0u);  // Moved from
}

// ============================================================================
// Payload Encoding Tests
// ============================================================================

class PayloadEncodingTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PayloadEncodingTest, UInt8) {
    Message msg;
    msg.writeUInt8(0);
    msg.writeUInt8(127);
    msg.writeUInt8(255);

    msg.resetReadOffset();

    uint8_t v1, v2, v3;
    EXPECT_TRUE(msg.readUInt8(v1));
    EXPECT_TRUE(msg.readUInt8(v2));
    EXPECT_TRUE(msg.readUInt8(v3));
    EXPECT_EQ(v1, 0u);
    EXPECT_EQ(v2, 127u);
    EXPECT_EQ(v3, 255u);
}

TEST_F(PayloadEncodingTest, UInt16) {
    Message msg;
    msg.writeUInt16(0);
    msg.writeUInt16(0x1234);
    msg.writeUInt16(0xFFFF);

    msg.resetReadOffset();

    uint16_t v1, v2, v3;
    EXPECT_TRUE(msg.readUInt16(v1));
    EXPECT_TRUE(msg.readUInt16(v2));
    EXPECT_TRUE(msg.readUInt16(v3));
    EXPECT_EQ(v1, 0u);
    EXPECT_EQ(v2, 0x1234u);
    EXPECT_EQ(v3, 0xFFFFu);
}

TEST_F(PayloadEncodingTest, UInt32) {
    Message msg;
    msg.writeUInt32(0);
    msg.writeUInt32(0x12345678);
    msg.writeUInt32(0xFFFFFFFF);

    msg.resetReadOffset();

    uint32_t v1, v2, v3;
    EXPECT_TRUE(msg.readUInt32(v1));
    EXPECT_TRUE(msg.readUInt32(v2));
    EXPECT_TRUE(msg.readUInt32(v3));
    EXPECT_EQ(v1, 0u);
    EXPECT_EQ(v2, 0x12345678u);
    EXPECT_EQ(v3, 0xFFFFFFFFu);
}

TEST_F(PayloadEncodingTest, UInt64) {
    Message msg;
    msg.writeUInt64(0);
    msg.writeUInt64(0x123456789ABCDEF0ULL);
    msg.writeUInt64(0xFFFFFFFFFFFFFFFFULL);

    msg.resetReadOffset();

    uint64_t v1, v2, v3;
    EXPECT_TRUE(msg.readUInt64(v1));
    EXPECT_TRUE(msg.readUInt64(v2));
    EXPECT_TRUE(msg.readUInt64(v3));
    EXPECT_EQ(v1, 0u);
    EXPECT_EQ(v2, 0x123456789ABCDEF0ULL);
    EXPECT_EQ(v3, 0xFFFFFFFFFFFFFFFFULL);
}

TEST_F(PayloadEncodingTest, Int32Negative) {
    Message msg;
    msg.writeInt32(-1);
    msg.writeInt32(-2147483648);  // INT32_MIN
    msg.writeInt32(2147483647);   // INT32_MAX

    msg.resetReadOffset();

    int32_t v1, v2, v3;
    EXPECT_TRUE(msg.readInt32(v1));
    EXPECT_TRUE(msg.readInt32(v2));
    EXPECT_TRUE(msg.readInt32(v3));
    EXPECT_EQ(v1, -1);
    EXPECT_EQ(v2, -2147483648);
    EXPECT_EQ(v3, 2147483647);
}

TEST_F(PayloadEncodingTest, Float) {
    Message msg;
    msg.writeFloat(0.0f);
    msg.writeFloat(3.14159f);
    msg.writeFloat(-123.456f);

    msg.resetReadOffset();

    float v1, v2, v3;
    EXPECT_TRUE(msg.readFloat(v1));
    EXPECT_TRUE(msg.readFloat(v2));
    EXPECT_TRUE(msg.readFloat(v3));
    EXPECT_FLOAT_EQ(v1, 0.0f);
    EXPECT_FLOAT_EQ(v2, 3.14159f);
    EXPECT_FLOAT_EQ(v3, -123.456f);
}

TEST_F(PayloadEncodingTest, Double) {
    Message msg;
    msg.writeDouble(0.0);
    msg.writeDouble(3.141592653589793);
    msg.writeDouble(-123.456789012345);

    msg.resetReadOffset();

    double v1, v2, v3;
    EXPECT_TRUE(msg.readDouble(v1));
    EXPECT_TRUE(msg.readDouble(v2));
    EXPECT_TRUE(msg.readDouble(v3));
    EXPECT_DOUBLE_EQ(v1, 0.0);
    EXPECT_DOUBLE_EQ(v2, 3.141592653589793);
    EXPECT_DOUBLE_EQ(v3, -123.456789012345);
}

TEST_F(PayloadEncodingTest, LengthPrefixedString) {
    Message msg;
    msg.writeLengthPrefixedString("");
    msg.writeLengthPrefixedString("Hello, World!");
    msg.writeLengthPrefixedString("Unicode: こんにちは");

    msg.resetReadOffset();

    std::string s1, s2, s3;
    EXPECT_TRUE(msg.readLengthPrefixedString(s1));
    EXPECT_TRUE(msg.readLengthPrefixedString(s2));
    EXPECT_TRUE(msg.readLengthPrefixedString(s3));
    EXPECT_EQ(s1, "");
    EXPECT_EQ(s2, "Hello, World!");
    EXPECT_EQ(s3, "Unicode: こんにちは");
}

TEST_F(PayloadEncodingTest, NullTerminatedString) {
    Message msg;
    msg.writeNullTerminatedString("test", 16);
    msg.writeNullTerminatedString("longer string here", 16);

    msg.resetReadOffset();

    std::string s1, s2;
    EXPECT_TRUE(msg.readNullTerminatedString(s1, 16));
    EXPECT_TRUE(msg.readNullTerminatedString(s2, 16));
    EXPECT_EQ(s1, "test");
    EXPECT_EQ(s2, "longer string h");  // Truncated to 15 chars + null
}

TEST_F(PayloadEncodingTest, Bytes) {
    Message msg;
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    msg.writeBytes(data, sizeof(data));

    msg.resetReadOffset();

    uint8_t read_data[5];
    EXPECT_TRUE(msg.readBytes(read_data, 5));
    EXPECT_EQ(std::memcmp(data, read_data, 5), 0);
}

TEST_F(PayloadEncodingTest, ReadBeyondEnd) {
    Message msg;
    msg.writeUInt8(42);

    msg.resetReadOffset();

    uint8_t v1;
    uint16_t v2;
    EXPECT_TRUE(msg.readUInt8(v1));
    EXPECT_FALSE(msg.readUInt16(v2));  // Not enough data
}

// ============================================================================
// Protocol Codec Tests
// ============================================================================

class ProtocolCodecTest : public ::testing::Test {
protected:
    void SetUp() override {
        generateSessionId(session_id_);
    }
    void TearDown() override {}

    uint8_t session_id_[16];
};

TEST_F(ProtocolCodecTest, ConnectRequest) {
    Message msg = ProtocolCodec::buildConnectRequest("testdb", "test_client", 12345);
    EXPECT_EQ(msg.getType(), MessageType::CONNECT_REQUEST);

    std::string database, client_name;
    uint32_t client_pid;
    ErrorContext ctx;

    Status status = ProtocolCodec::parseConnectRequest(msg, database, client_name, client_pid, nullptr, &ctx);
    EXPECT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(database, "testdb");
    EXPECT_EQ(client_name, "test_client");
    EXPECT_EQ(client_pid, 12345u);
}

TEST_F(ProtocolCodecTest, ConnectResponse) {
    Message msg = ProtocolCodec::buildConnectResponse(true, session_id_);
    EXPECT_EQ(msg.getType(), MessageType::CONNECT_RESPONSE);

    bool success;
    uint8_t parsed_session_id[16];
    std::string error_message;
    ErrorContext ctx;

    Status status = ProtocolCodec::parseConnectResponse(msg, success, parsed_session_id, error_message, nullptr, &ctx);
    EXPECT_EQ(status, Status::OK) << ctx.message;
    EXPECT_TRUE(success);
    EXPECT_EQ(std::memcmp(session_id_, parsed_session_id, 16), 0);
}

TEST_F(ProtocolCodecTest, ConnectResponseFailure) {
    Message msg = ProtocolCodec::buildConnectResponse(false, session_id_, "Database not found");

    bool success;
    uint8_t parsed_session_id[16];
    std::string error_message;
    ErrorContext ctx;

    Status status = ProtocolCodec::parseConnectResponse(msg, success, parsed_session_id, error_message, nullptr, &ctx);
    EXPECT_EQ(status, Status::OK) << ctx.message;
    EXPECT_FALSE(success);
    EXPECT_EQ(error_message, "Database not found");
}

TEST_F(ProtocolCodecTest, AuthRequest) {
    Message msg = ProtocolCodec::buildAuthRequest(session_id_, "admin", "secret123");
    EXPECT_EQ(msg.getType(), MessageType::AUTH_REQUEST);

    uint8_t parsed_session_id[16];
    std::string username, password;
    ErrorContext ctx;

    Status status = ProtocolCodec::parseAuthRequest(msg, parsed_session_id, username, password, &ctx);
    EXPECT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(username, "admin");
    EXPECT_EQ(password, "secret123");
}

TEST_F(ProtocolCodecTest, AuthResponse) {
    Message msg = ProtocolCodec::buildAuthResponse(true, 42);
    EXPECT_EQ(msg.getType(), MessageType::AUTH_RESPONSE);

    bool success;
    uint32_t user_id;
    std::string error_message;
    ErrorContext ctx;

    Status status = ProtocolCodec::parseAuthResponse(msg, success, user_id, error_message, &ctx);
    EXPECT_EQ(status, Status::OK) << ctx.message;
    EXPECT_TRUE(success);
    EXPECT_EQ(user_id, 42u);
}

TEST_F(ProtocolCodecTest, Query) {
    std::string sql = "SELECT id, name FROM users WHERE active = true ORDER BY name";
    Message msg = ProtocolCodec::buildQuery(session_id_, sql, static_cast<uint8_t>(QueryFlags::WANT_ROWCOUNT));
    EXPECT_EQ(msg.getType(), MessageType::QUERY);

    uint8_t parsed_session_id[16];
    std::string parsed_sql;
    uint8_t flags;
    ErrorContext ctx;

    Status status = ProtocolCodec::parseQuery(msg, parsed_session_id, parsed_sql, flags, &ctx);
    EXPECT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(parsed_sql, sql);
    EXPECT_EQ(flags, static_cast<uint8_t>(QueryFlags::WANT_ROWCOUNT));
}

TEST_F(ProtocolCodecTest, QueryError) {
    Message msg = ProtocolCodec::buildQueryError(
        static_cast<uint32_t>(Status::UNDEFINED_TABLE),
        "42P01",
        "relation \"users\" does not exist",
        "Schema contains no table named users",
        "Check your spelling or create the table"
    );
    EXPECT_EQ(msg.getType(), MessageType::QUERY_ERROR);

    uint32_t error_code;
    std::string sqlstate, message, detail, hint;
    ErrorContext ctx;

    Status status = ProtocolCodec::parseQueryError(msg, error_code, sqlstate, message, detail, hint, &ctx);
    EXPECT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(error_code, static_cast<uint32_t>(Status::UNDEFINED_TABLE));
    EXPECT_EQ(sqlstate, "42P01");
    EXPECT_EQ(message, "relation \"users\" does not exist");
    EXPECT_EQ(detail, "Schema contains no table named users");
    EXPECT_EQ(hint, "Check your spelling or create the table");
}

TEST_F(ProtocolCodecTest, RowDescription) {
    std::vector<ProtocolCodec::ColumnInfo> columns = {
        {"id", WireType::INT64, 0},
        {"name", WireType::VARCHAR, 255},
        {"created_at", WireType::TIMESTAMP, 0}
    };

    Message msg = ProtocolCodec::buildRowDescription(columns);
    EXPECT_EQ(msg.getType(), MessageType::ROW_DESCRIPTION);

    std::vector<ProtocolCodec::ColumnInfo> parsed_columns;
    ErrorContext ctx;

    Status status = ProtocolCodec::parseRowDescription(msg, parsed_columns, &ctx);
    EXPECT_EQ(status, Status::OK) << ctx.message;
    ASSERT_EQ(parsed_columns.size(), 3u);
    EXPECT_EQ(parsed_columns[0].name, "id");
    EXPECT_EQ(parsed_columns[0].type, WireType::INT64);
    EXPECT_EQ(parsed_columns[1].name, "name");
    EXPECT_EQ(parsed_columns[1].type, WireType::VARCHAR);
    EXPECT_EQ(parsed_columns[2].name, "created_at");
    EXPECT_EQ(parsed_columns[2].type, WireType::TIMESTAMP);
}

TEST_F(ProtocolCodecTest, RowData) {
    std::vector<ProtocolCodec::ColumnValue> values = {
        ProtocolCodec::ColumnValue::fromInt64(42),
        ProtocolCodec::ColumnValue::fromString("John Doe"),
        ProtocolCodec::ColumnValue(nullptr)  // NULL value
    };

    Message msg = ProtocolCodec::buildRowData(values);
    EXPECT_EQ(msg.getType(), MessageType::ROW_DATA);

    std::vector<ProtocolCodec::ColumnValue> parsed_values;
    ErrorContext ctx;

    Status status = ProtocolCodec::parseRowData(msg, parsed_values, &ctx);
    EXPECT_EQ(status, Status::OK) << ctx.message;
    ASSERT_EQ(parsed_values.size(), 3u);

    EXPECT_FALSE(parsed_values[0].is_null);
    EXPECT_EQ(parsed_values[0].data.size(), 8u);

    EXPECT_FALSE(parsed_values[1].is_null);
    std::string name(parsed_values[1].data.begin(), parsed_values[1].data.end());
    EXPECT_EQ(name, "John Doe");

    EXPECT_TRUE(parsed_values[2].is_null);
}

TEST_F(ProtocolCodecTest, CommandComplete) {
    Message msg = ProtocolCodec::buildCommandComplete("SELECT 100", 100);
    EXPECT_EQ(msg.getType(), MessageType::COMMAND_COMPLETE);

    std::string command_tag;
    int64_t rows_affected;
    ErrorContext ctx;

    Status status = ProtocolCodec::parseCommandComplete(msg, command_tag, rows_affected, &ctx);
    EXPECT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(command_tag, "SELECT 100");
    EXPECT_EQ(rows_affected, 100);
}

TEST_F(ProtocolCodecTest, EndOfResults) {
    Message msg = ProtocolCodec::buildEndOfResults();
    EXPECT_EQ(msg.getType(), MessageType::END_OF_RESULTS);
    EXPECT_EQ(msg.getPayloadLength(), 0u);
}

TEST_F(ProtocolCodecTest, Transaction) {
    // BEGIN
    Message begin_msg = ProtocolCodec::buildBeginTransaction(session_id_, 1, false);
    EXPECT_EQ(begin_msg.getType(), MessageType::BEGIN_TRANSACTION);

    // COMMIT
    Message commit_msg = ProtocolCodec::buildCommit(session_id_);
    EXPECT_EQ(commit_msg.getType(), MessageType::COMMIT);

    // ROLLBACK
    Message rollback_msg = ProtocolCodec::buildRollback(session_id_);
    EXPECT_EQ(rollback_msg.getType(), MessageType::ROLLBACK);

    // SAVEPOINT
    Message savepoint_msg = ProtocolCodec::buildSavepoint(session_id_, "sp1");
    EXPECT_EQ(savepoint_msg.getType(), MessageType::SAVEPOINT);

    // ROLLBACK TO
    Message rollback_to_msg = ProtocolCodec::buildRollbackTo(session_id_, "sp1");
    EXPECT_EQ(rollback_to_msg.getType(), MessageType::ROLLBACK_TO);
}

TEST_F(ProtocolCodecTest, Ping) {
    uint64_t timestamp = 1234567890123ULL;
    uint32_t sequence = 42;

    Message msg = ProtocolCodec::buildPing(timestamp, sequence);
    EXPECT_EQ(msg.getType(), MessageType::PING);

    uint64_t parsed_timestamp;
    uint32_t parsed_sequence;
    ErrorContext ctx;

    Status status = ProtocolCodec::parsePing(msg, parsed_timestamp, parsed_sequence, &ctx);
    EXPECT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(parsed_timestamp, timestamp);
    EXPECT_EQ(parsed_sequence, sequence);
}

TEST_F(ProtocolCodecTest, Pong) {
    Message msg = ProtocolCodec::buildPong(9876543210ULL, 99);
    EXPECT_EQ(msg.getType(), MessageType::PONG);
}

TEST_F(ProtocolCodecTest, Administrative) {
    Message disconnect = ProtocolCodec::buildDisconnect();
    EXPECT_EQ(disconnect.getType(), MessageType::DISCONNECT);

    Message shutdown = ProtocolCodec::buildShutdown();
    EXPECT_EQ(shutdown.getType(), MessageType::SHUTDOWN);

    Message error = ProtocolCodec::buildProtocolError("Invalid message format");
    EXPECT_EQ(error.getType(), MessageType::PROTOCOL_ERROR);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

class UtilityFunctionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(UtilityFunctionTest, MessageTypeToString) {
    EXPECT_STREQ(messageTypeToString(MessageType::CONNECT_REQUEST), "CONNECT_REQUEST");
    EXPECT_STREQ(messageTypeToString(MessageType::QUERY), "QUERY");
    EXPECT_STREQ(messageTypeToString(MessageType::ROW_DATA), "ROW_DATA");
    EXPECT_STREQ(messageTypeToString(MessageType::PING), "PING");
}

TEST_F(UtilityFunctionTest, WireTypeToString) {
    EXPECT_STREQ(wireTypeToString(WireType::INT32), "INT32");
    EXPECT_STREQ(wireTypeToString(WireType::VARCHAR), "VARCHAR");
    EXPECT_STREQ(wireTypeToString(WireType::TIMESTAMP), "TIMESTAMP");
    EXPECT_STREQ(wireTypeToString(WireType::NULL_TYPE), "NULL");
}

TEST_F(UtilityFunctionTest, SessionIdGeneration) {
    uint8_t session1[16], session2[16];

    generateSessionId(session1);
    generateSessionId(session2);

    // Should be different
    EXPECT_NE(std::memcmp(session1, session2, 16), 0);

    // Should have correct UUID v4 version
    EXPECT_EQ((session1[6] & 0xF0), 0x40);  // Version 4
    EXPECT_EQ((session1[8] & 0xC0), 0x80);  // Variant
}

TEST_F(UtilityFunctionTest, SessionIdToString) {
    uint8_t session_id[16] = {
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
    };

    std::string str = sessionIdToString(session_id);
    EXPECT_EQ(str.length(), 36u);  // UUID format with dashes
    EXPECT_EQ(str, "12345678-9abc-def0-1122-334455667788");
}

// ============================================================================
// Protocol Session Integration Tests
// ============================================================================

class ProtocolSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!scratchbird::testing::networkTestsEnabled()) {
            GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
        }
        // Use unique Unix socket path under build/ to avoid network restrictions
        static std::atomic<uint32_t> socket_counter{0};
        uint32_t idx = socket_counter.fetch_add(1);
        std::filesystem::path base = std::filesystem::path("build") / "ipc_tests";
        std::filesystem::create_directories(base);
        socket_path_ = (base / ("wireproto_" + std::to_string(getpid()) + "_" + std::to_string(idx) + ".sock")).string();
        // Clean any stale socket file
        std::error_code ec;
        std::filesystem::remove(socket_path_, ec);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(socket_path_, ec);
    }

    std::string socket_path_;
};

TEST_F(ProtocolSessionTest, SendReceiveMessage) {
    // Set up Unix socket server and client
    IPCServerConfig server_config("proto_test", IPCMethod::UNIX_SOCKET);
    server_config.socket_path = socket_path_;
    server_config.accept_timeout_ms = 2000;
    ErrorContext ctx;

    auto server = IPCServer::create(server_config, &ctx);
    ASSERT_NE(server, nullptr);

    Status status = server->listen(&ctx);
    if (status != Status::OK) {
        if (isNetworkRestrictedError(ctx)) {
            GTEST_SKIP() << "Listen failed: " << ctx.message;
        }
        FAIL() << "Listen failed: " << ctx.message;
    }

    // Accept thread
    std::unique_ptr<IPCConnection> server_conn;
    std::thread accept_thread([&]() {
        ErrorContext actx;
        server_conn = server->accept(&actx);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Connect client
    IPCClientConfig client_config("proto_test", IPCMethod::UNIX_SOCKET);
    client_config.socket_path = socket_path_;
    auto client = IPCClient::create(client_config, &ctx);
    ASSERT_NE(client, nullptr);

    status = client->connect(&ctx);
    ASSERT_EQ(status, Status::OK) << "Connect failed: " << ctx.message;

    accept_thread.join();
    ASSERT_NE(server_conn, nullptr);

    // Create protocol sessions
    ProtocolSession client_session(client->getConnection());
    ProtocolSession server_session(server_conn.get());

    // Client sends query
    uint8_t session_id[16];
    generateSessionId(session_id);

    Message query_msg = ProtocolCodec::buildQuery(session_id, "SELECT 1", 0);
    status = client_session.sendMessage(query_msg, &ctx);
    EXPECT_EQ(status, Status::OK) << "Send failed: " << ctx.message;

    // Server receives query
    Message received_msg;
    status = server_session.receiveMessage(received_msg, &ctx);
    EXPECT_EQ(status, Status::OK) << "Receive failed: " << ctx.message;
    EXPECT_EQ(received_msg.getType(), MessageType::QUERY);

    // Parse and verify
    uint8_t parsed_session_id[16];
    std::string parsed_query;
    uint8_t flags;
    status = ProtocolCodec::parseQuery(received_msg, parsed_session_id, parsed_query, flags, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(parsed_query, "SELECT 1");

    // Server sends result
    Message result_msg = ProtocolCodec::buildCommandComplete("SELECT 1", 1);
    status = server_session.sendMessage(result_msg, &ctx);
    EXPECT_EQ(status, Status::OK);

    // Client receives result
    status = client_session.receiveMessage(received_msg, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(received_msg.getType(), MessageType::COMMAND_COMPLETE);

    // Check session stats
    EXPECT_EQ(client_session.getMessagesSent(), 1u);
    EXPECT_EQ(client_session.getMessagesReceived(), 1u);
    EXPECT_EQ(server_session.getMessagesSent(), 1u);
    EXPECT_EQ(server_session.getMessagesReceived(), 1u);

    // Cleanup
    client->disconnect();
    server_conn->close();
    server->close();
}

TEST_F(ProtocolSessionTest, FullHandshake) {
    // Set up Unix socket server and client
    IPCServerConfig server_config("handshake_test", IPCMethod::UNIX_SOCKET);
    server_config.socket_path = socket_path_;
    server_config.accept_timeout_ms = 2000;
    ErrorContext ctx;

    auto server = IPCServer::create(server_config, &ctx);
    ASSERT_NE(server, nullptr);
    Status status = server->listen(&ctx);
    if (status != Status::OK) {
        if (isNetworkRestrictedError(ctx)) {
            GTEST_SKIP() << "Listen failed: " << ctx.message;
        }
        FAIL() << "Listen failed: " << ctx.message;
    }

    std::unique_ptr<IPCConnection> server_conn;
    ErrorContext accept_ctx;
    std::thread accept_thread([&]() {
        server_conn = server->accept(&accept_ctx);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    IPCClientConfig client_config("handshake_test", IPCMethod::UNIX_SOCKET);
    client_config.socket_path = socket_path_;
    auto client = IPCClient::create(client_config, &ctx);
    ASSERT_NE(client, nullptr);
    status = client->connect(&ctx);
    if (status != Status::OK) {
        if (isNetworkRestrictedError(ctx)) {
            GTEST_SKIP() << "Connect failed: " << ctx.message;
        }
        FAIL() << "Connect failed: " << ctx.message;
    }

    accept_thread.join();
    if (!server_conn) {
        if (isNetworkRestrictedError(accept_ctx)) {
            GTEST_SKIP() << "Accept failed: " << accept_ctx.message;
        }
        FAIL() << "Accept failed: " << accept_ctx.message;
    }

    ProtocolSession client_session(client->getConnection());
    ProtocolSession server_session(server_conn.get());

    // 1. Client sends CONNECT_REQUEST
    Message connect_req = ProtocolCodec::buildConnectRequest("testdb", "test_client", 1234);
    client_session.sendMessage(connect_req, &ctx);

    // 2. Server receives and responds
    Message received;
    server_session.receiveMessage(received, &ctx);
    EXPECT_EQ(received.getType(), MessageType::CONNECT_REQUEST);

    std::string db, client_name;
    uint32_t pid;
    ProtocolCodec::parseConnectRequest(received, db, client_name, pid, nullptr, &ctx);
    EXPECT_EQ(db, "testdb");

    uint8_t session_id[16];
    generateSessionId(session_id);
    Message connect_resp = ProtocolCodec::buildConnectResponse(true, session_id);
    server_session.sendMessage(connect_resp, &ctx);

    // 3. Client receives response
    client_session.receiveMessage(received, &ctx);
    EXPECT_EQ(received.getType(), MessageType::CONNECT_RESPONSE);

    bool success;
    uint8_t recv_session_id[16];
    std::string error_msg;
    ProtocolCodec::parseConnectResponse(received, success, recv_session_id, error_msg, nullptr, &ctx);
    EXPECT_TRUE(success);

    // 4. Client sends AUTH_REQUEST
    Message auth_req = ProtocolCodec::buildAuthRequest(recv_session_id, "admin", "password");
    client_session.sendMessage(auth_req, &ctx);

    // 5. Server receives and responds
    server_session.receiveMessage(received, &ctx);
    EXPECT_EQ(received.getType(), MessageType::AUTH_REQUEST);

    Message auth_resp = ProtocolCodec::buildAuthResponse(true, 1);
    server_session.sendMessage(auth_resp, &ctx);

    // 6. Client receives auth response
    client_session.receiveMessage(received, &ctx);
    EXPECT_EQ(received.getType(), MessageType::AUTH_RESPONSE);

    uint32_t user_id;
    ProtocolCodec::parseAuthResponse(received, success, user_id, error_msg, &ctx);
    EXPECT_TRUE(success);
    EXPECT_EQ(user_id, 1u);

    // Cleanup
    client->disconnect();
    server_conn->close();
    server->close();
}

// Main provided by gtest_main
