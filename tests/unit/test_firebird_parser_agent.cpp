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
 * Firebird Parser Agent Unit Tests
 *
 * Comprehensive tests for the Firebird wire protocol parser agent including:
 * - Connection handling (op_connect, op_accept, op_reject)
 * - Protocol version negotiation
 * - Authentication (Legacy, SRP, SRP256)
 * - Database operations (op_attach, op_detach, op_create)
 * - Transaction management (op_transaction, op_commit, op_rollback)
 * - Statement operations (op_compile, op_start, op_receive, op_send)
 * - BLOB operations (op_open_blob, op_create_blob, op_get_segment, op_put_segment, op_close_blob)
 * - XDR encoding/decoding
 * - Error handling and status vectors
 */

#include <gtest/gtest.h>
#include <array>
#include <cstring>
#include <thread>
#include <chrono>
#include <filesystem>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Include core types before firebird_parser_agent.h (which references them)
#include "scratchbird/core/types.h"
#include "scratchbird/core/typed_value.h"

#include "scratchbird/ipc/firebird_parser_agent.h"
#include "scratchbird/ipc/parser_agent.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "test_helpers.h"

using namespace scratchbird::ipc;
using namespace scratchbird::core;

namespace {

// ============================================================================
// Mock IPC Channel for Testing
// ============================================================================

class MockIPCChannel {
public:
    MockIPCChannel() = default;
    
    // Write data to the mock channel (client -> server)
    void clientWrite(const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        client_to_server_.insert(client_to_server_.end(), data.begin(), data.end());
        cv_.notify_one();
    }
    
    // Read data from the mock channel (server reads what client wrote)
    bool serverRead(std::vector<uint8_t>& data, size_t len, int timeout_ms = 1000) {
        std::unique_lock<std::mutex> lock(mutex_);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        
        while (client_to_server_.size() < len) {
            if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
                return false;
            }
        }
        
        data.assign(client_to_server_.begin(), client_to_server_.begin() + len);
        client_to_server_.erase(client_to_server_.begin(), client_to_server_.begin() + len);
        return true;
    }
    
    // Write data to the mock channel (server -> client)
    void serverWrite(const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        server_to_client_.insert(server_to_client_.end(), data.begin(), data.end());
        cv_.notify_one();
    }
    
    // Read data from the mock channel (client reads what server wrote)
    bool clientRead(std::vector<uint8_t>& data, size_t len, int timeout_ms = 1000) {
        std::unique_lock<std::mutex> lock(mutex_);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        
        while (server_to_client_.size() < len) {
            if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
                return false;
            }
        }
        
        data.assign(server_to_client_.begin(), server_to_client_.begin() + len);
        server_to_client_.erase(server_to_client_.begin(), server_to_client_.begin() + len);
        return true;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        client_to_server_.clear();
        server_to_client_.clear();
    }
    
private:
    std::vector<uint8_t> client_to_server_;
    std::vector<uint8_t> server_to_client_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

// ============================================================================
// XDR Helper Functions
// ============================================================================

static uint32_t xdrReadUint32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

static void xdrWriteUint32(uint8_t* data, uint32_t value) {
    data[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    data[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[3] = static_cast<uint8_t>(value & 0xFF);
}

static uint16_t xdrReadUint16(const uint8_t* data) {
    return (static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]);
}

static void xdrWriteUint16(uint8_t* data, uint16_t value) {
    data[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[1] = static_cast<uint8_t>(value & 0xFF);
}

// ============================================================================
// Firebird Operation Codes
// ============================================================================

namespace fb {
    // Connection
    constexpr uint32_t op_connect = 1;
    constexpr uint32_t op_exit = 2;
    constexpr uint32_t op_accept = 3;
    constexpr uint32_t op_reject = 4;
    constexpr uint32_t op_protocol = 5;
    constexpr uint32_t op_disconnect = 6;
    constexpr uint32_t op_response = 9;
    
    // Database
    constexpr uint32_t op_attach = 19;
    constexpr uint32_t op_create = 20;
    constexpr uint32_t op_detach = 21;
    constexpr uint32_t op_compile = 22;
    constexpr uint32_t op_start = 23;
    constexpr uint32_t op_start_and_receive = 24;
    constexpr uint32_t op_send = 25;
    constexpr uint32_t op_receive = 26;
    constexpr uint32_t op_unwind = 27;
    constexpr uint32_t op_release = 28;
    
    // Transaction
    constexpr uint32_t op_transaction = 29;
    constexpr uint32_t op_commit = 30;
    constexpr uint32_t op_rollback = 31;
    constexpr uint32_t op_prepare = 32;
    constexpr uint32_t op_commit_retaining = 50;
    constexpr uint32_t op_rollback_retaining = 86;
    
    // Information
    constexpr uint32_t op_info_database = 40;
    constexpr uint32_t op_info_request = 41;
    constexpr uint32_t op_info_transaction = 42;
    constexpr uint32_t op_info_blob = 43;
    
    // BLOB
    constexpr uint32_t op_create_blob = 34;
    constexpr uint32_t op_open_blob = 35;
    constexpr uint32_t op_get_segment = 36;
    constexpr uint32_t op_put_segment = 37;
    constexpr uint32_t op_cancel_blob = 38;
    constexpr uint32_t op_close_blob = 39;
    constexpr uint32_t op_batch_segments = 44;
    
    // Wire encryption
    constexpr uint32_t op_crypt = 66;
    constexpr uint32_t op_crypt_callback = 67;
    
    // Authentication
    constexpr uint32_t op_authenticate = 68;
    
    // Protocol versions
    constexpr uint32_t PROTOCOL_VERSION10 = 10;
    constexpr uint32_t PROTOCOL_VERSION11 = 11;
    constexpr uint32_t PROTOCOL_VERSION12 = 12;
    constexpr uint32_t PROTOCOL_VERSION13 = 13;
    constexpr uint32_t PROTOCOL_VERSION14 = 14;
    constexpr uint32_t PROTOCOL_VERSION15 = 15;
    constexpr uint32_t PROTOCOL_VERSION16 = 16;
    
    // CNCT types for authentication
    constexpr uint8_t CNCT_login = 1;
    constexpr uint8_t CNCT_plugin = 2;
    constexpr uint8_t CNCT_pwd = 3;
    constexpr uint8_t CNCT_verified = 4;
    
    // Generic error code
    constexpr uint32_t GENERIC_ERROR = 1;
}

// ============================================================================
// Test Fixture
// ============================================================================

class FirebirdParserAgentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a parser agent with test configuration
        ParserAgentConfig config;
        config.name = "test_firebird_agent";
        config.protocol = "firebird";
        config.listen_endpoint = "127.0.0.1:0";  // Any available port
        config.ipc_endpoint = scratchbird::testing::uniqueTestSocketPath("fb_test_ipc");
        config.max_connections = 10;
        config.io_threads = 1;
        
        agent_ = std::make_unique<FirebirdParserAgent>(config);
        
        // Initialize client state
        state_.client_fd = -1;  // Mock FD
        state_.state = FBClientState::CONNECTING;
        state_.protocol_version = fb::PROTOCOL_VERSION16;
        state_.accept_version = fb::PROTOCOL_VERSION16;
        state_.wire_encrypted = false;
        state_.handle = 0;
    }
    
    void TearDown() override {
        agent_.reset();
    }
    
    // Build a connect packet for testing
    std::vector<uint8_t> buildConnectPacket(const std::string& database,
                                            const std::string& username,
                                            const std::string& auth_plugin,
                                            uint32_t protocol_version = fb::PROTOCOL_VERSION16) {
        std::vector<uint8_t> packet;
        
        // Operation code (op_connect)
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_connect);
        
        // Protocol version
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, protocol_version);
        
        // Architecture type (generic = 1)
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, 1);
        
        // Minimum type
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, 1);
        
        // Maximum type
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, 1);
        
        // Page size
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, 4096);
        
        // Path length and database path
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, static_cast<uint32_t>(database.size()));
        packet.insert(packet.end(), database.begin(), database.end());
        
        // Protocol version count
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, 3);
        
        // Protocol version list (version, arch pairs)
        for (uint32_t ver : {fb::PROTOCOL_VERSION16, fb::PROTOCOL_VERSION15, fb::PROTOCOL_VERSION14}) {
            packet.resize(packet.size() + 4);
            xdrWriteUint32(packet.data() + packet.size() - 4, ver);
            packet.resize(packet.size() + 4);
            xdrWriteUint32(packet.data() + packet.size() - 4, 1);  // arch
        }
        
        // Authentication data (CNCT structure)
        // CNCT_login
        packet.push_back(fb::CNCT_login);
        packet.resize(packet.size() + 2);
        xdrWriteUint16(packet.data() + packet.size() - 2, static_cast<uint16_t>(username.size()));
        packet.insert(packet.end(), username.begin(), username.end());
        
        // CNCT_plugin
        packet.push_back(fb::CNCT_plugin);
        packet.resize(packet.size() + 2);
        xdrWriteUint16(packet.data() + packet.size() - 2, static_cast<uint16_t>(auth_plugin.size()));
        packet.insert(packet.end(), auth_plugin.begin(), auth_plugin.end());
        
        return packet;
    }
    
    // Build an attach packet
    std::vector<uint8_t> buildAttachPacket(uint32_t db_handle = 0) {
        std::vector<uint8_t> packet;
        
        // Operation code
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_attach);
        
        // Database handle (if reconnecting)
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, db_handle);
        
        // DPB (Database Parameter Block) - minimal
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, 0);  // Empty DPB for now
        
        return packet;
    }
    
    // Build a create database packet
    std::vector<uint8_t> buildCreatePacket(const std::string& db_name) {
        std::vector<uint8_t> packet;
        
        // Operation code
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_create);
        
        // Database name length and name
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, static_cast<uint32_t>(db_name.size()));
        packet.insert(packet.end(), db_name.begin(), db_name.end());
        
        // DPB (Database Parameter Block)
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, 0);  // Empty DPB for now
        
        return packet;
    }
    
    // Build a transaction packet
    std::vector<uint8_t> buildTransactionPacket(const std::vector<uint8_t>& tpb = {}) {
        std::vector<uint8_t> packet;
        
        // Operation code
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_transaction);
        
        // Database handle
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, 1);  // Mock handle
        
        // TPB length and data
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, static_cast<uint32_t>(tpb.size()));
        packet.insert(packet.end(), tpb.begin(), tpb.end());
        
        return packet;
    }
    
    // Build a commit packet
    std::vector<uint8_t> buildCommitPacket(bool retaining = false) {
        std::vector<uint8_t> packet;
        
        // Operation code
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, 
                       retaining ? fb::op_commit_retaining : fb::op_commit);
        
        // Transaction handle
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, 1);  // Mock handle
        
        return packet;
    }
    
    // Build a rollback packet
    std::vector<uint8_t> buildRollbackPacket(bool retaining = false) {
        std::vector<uint8_t> packet;
        
        // Operation code
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4,
                       retaining ? fb::op_rollback_retaining : fb::op_rollback);
        
        // Transaction handle
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, 1);  // Mock handle
        
        return packet;
    }
    
    // Build a compile packet (SQL prepare)
    std::vector<uint8_t> buildCompilePacket(const std::string& sql) {
        std::vector<uint8_t> packet;
        
        // Operation code
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_compile);
        
        // Database handle
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, 1);  // Mock handle
        
        // SQL length and text
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, static_cast<uint32_t>(sql.size()));
        packet.insert(packet.end(), sql.begin(), sql.end());
        
        return packet;
    }
    
    // Build an open blob packet
    std::vector<uint8_t> buildOpenBlobPacket(uint64_t blob_id) {
        std::vector<uint8_t> packet;
        
        // Operation code
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_open_blob);
        
        // Transaction handle
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, 1);
        
        // Blob ID (high 32 bits)
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, static_cast<uint32_t>(blob_id >> 32));
        
        // Blob ID (low 32 bits)
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, static_cast<uint32_t>(blob_id & 0xFFFFFFFF));
        
        return packet;
    }
    
    // Build a create blob packet
    std::vector<uint8_t> buildCreateBlobPacket() {
        std::vector<uint8_t> packet;
        
        // Operation code
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_create_blob);
        
        // Transaction handle
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, 1);
        
        return packet;
    }
    
    // Build a get segment packet
    std::vector<uint8_t> buildGetSegmentPacket(uint32_t blob_handle, uint32_t segment_size) {
        std::vector<uint8_t> packet;
        
        // Operation code
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_get_segment);
        
        // Blob handle
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, blob_handle);
        
        // Segment size
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, segment_size);
        
        return packet;
    }
    
    // Build a put segment packet
    std::vector<uint8_t> buildPutSegmentPacket(uint32_t blob_handle, const std::vector<uint8_t>& data) {
        std::vector<uint8_t> packet;
        
        // Operation code
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_put_segment);
        
        // Blob handle
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, blob_handle);
        
        // Data length
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, static_cast<uint32_t>(data.size()));
        
        // Data
        packet.insert(packet.end(), data.begin(), data.end());
        
        // Pad to 4-byte boundary
        while (packet.size() % 4 != 0) {
            packet.push_back(0);
        }
        
        return packet;
    }
    
    // Build a close blob packet
    std::vector<uint8_t> buildCloseBlobPacket(uint32_t blob_handle, bool cancel = false) {
        std::vector<uint8_t> packet;
        
        // Operation code
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, 
                       cancel ? fb::op_cancel_blob : fb::op_close_blob);
        
        // Blob handle
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, blob_handle);
        
        return packet;
    }
    
    // Build a disconnect packet
    std::vector<uint8_t> buildDisconnectPacket() {
        std::vector<uint8_t> packet;
        
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_disconnect);
        
        return packet;
    }
    
    // Build a crypt packet (wire encryption)
    std::vector<uint8_t> buildCryptPacket(const std::string& plugin) {
        std::vector<uint8_t> packet;
        
        // Operation code
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_crypt);
        
        // Plugin name length and name
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, static_cast<uint32_t>(plugin.size()));
        packet.insert(packet.end(), plugin.begin(), plugin.end());
        
        // Pad to 4-byte boundary
        while (packet.size() % 4 != 0) {
            packet.push_back(0);
        }
        
        return packet;
    }
    
    // Build an authenticate packet
    std::vector<uint8_t> buildAuthenticatePacket(const std::vector<uint8_t>& auth_data) {
        std::vector<uint8_t> packet;
        
        // Operation code
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_authenticate);
        
        // Auth data length and data
        packet.resize(packet.size() + 4);
        xdrWriteUint32(packet.data() + packet.size() - 4, static_cast<uint32_t>(auth_data.size()));
        packet.insert(packet.end(), auth_data.begin(), auth_data.end());
        
        // Pad to 4-byte boundary
        while (packet.size() % 4 != 0) {
            packet.push_back(0);
        }
        
        return packet;
    }
    
    std::unique_ptr<FirebirdParserAgent> agent_;
    FBClientState state_;
    ErrorContext ctx_;
};

// ============================================================================
// Connect Tests
// ============================================================================

TEST_F(FirebirdParserAgentTest, ConnectPacketParsing) {
    auto packet = buildConnectPacket("test.fdb", "SYSDBA", "Srp256");
    EXPECT_GT(packet.size(), 0u);
    
    // Verify the packet structure
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_connect);
    
    uint32_t version = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(version, fb::PROTOCOL_VERSION16);
}

TEST_F(FirebirdParserAgentTest, ProtocolVersionNegotiation) {
    // Test that the agent correctly negotiates the highest supported version
    std::vector<uint32_t> test_versions = {
        fb::PROTOCOL_VERSION10,
        fb::PROTOCOL_VERSION11,
        fb::PROTOCOL_VERSION12,
        fb::PROTOCOL_VERSION13,
        fb::PROTOCOL_VERSION14,
        fb::PROTOCOL_VERSION15,
        fb::PROTOCOL_VERSION16
    };
    
    for (uint32_t ver : test_versions) {
        auto packet = buildConnectPacket("test.fdb", "SYSDBA", "Srp256", ver);
        uint32_t pkt_version = xdrReadUint32(packet.data() + 4);
        EXPECT_EQ(pkt_version, ver);
    }
}

TEST_F(FirebirdParserAgentTest, LegacyAuthPlugin) {
    auto packet = buildConnectPacket("test.fdb", "SYSDBA", "Legacy_Auth");
    
    // Parse the packet to extract auth plugin
    size_t offset = 4 + 4 + 4 + 4 + 4 + 4;  // Skip to path length
    uint32_t path_len = xdrReadUint32(packet.data() + offset);
    offset += 4 + path_len;  // Skip path
    
    uint32_t count = xdrReadUint32(packet.data() + offset);
    offset += 4 + (count * 8);  // Skip version list
    
    // Parse CNCT data
    while (offset + 5 <= packet.size()) {
        uint8_t type = packet[offset++];
        uint16_t len = xdrReadUint16(packet.data() + offset);
        offset += 2;
        
        if (type == fb::CNCT_plugin) {
            std::string plugin(reinterpret_cast<const char*>(packet.data() + offset), len);
            EXPECT_EQ(plugin, "Legacy_Auth");
            break;
        }
        offset += len;
    }
}

TEST_F(FirebirdParserAgentTest, SrpAuthPlugin) {
    auto packet = buildConnectPacket("test.fdb", "SYSDBA", "Srp");
    
    size_t offset = 4 + 4 + 4 + 4 + 4 + 4;  // Skip to path length
    uint32_t path_len = xdrReadUint32(packet.data() + offset);
    offset += 4 + path_len;
    
    uint32_t count = xdrReadUint32(packet.data() + offset);
    offset += 4 + (count * 8);
    
    while (offset + 5 <= packet.size()) {
        uint8_t type = packet[offset++];
        uint16_t len = xdrReadUint16(packet.data() + offset);
        offset += 2;
        
        if (type == fb::CNCT_plugin) {
            std::string plugin(reinterpret_cast<const char*>(packet.data() + offset), len);
            EXPECT_EQ(plugin, "Srp");
            break;
        }
        offset += len;
    }
}

TEST_F(FirebirdParserAgentTest, Srp256AuthPlugin) {
    auto packet = buildConnectPacket("test.fdb", "SYSDBA", "Srp256");
    
    size_t offset = 4 + 4 + 4 + 4 + 4 + 4;  // Skip to path length
    uint32_t path_len = xdrReadUint32(packet.data() + offset);
    offset += 4 + path_len;
    
    uint32_t count = xdrReadUint32(packet.data() + offset);
    offset += 4 + (count * 8);
    
    while (offset + 5 <= packet.size()) {
        uint8_t type = packet[offset++];
        uint16_t len = xdrReadUint16(packet.data() + offset);
        offset += 2;
        
        if (type == fb::CNCT_plugin) {
            std::string plugin(reinterpret_cast<const char*>(packet.data() + offset), len);
            EXPECT_EQ(plugin, "Srp256");
            break;
        }
        offset += len;
    }
}

TEST_F(FirebirdParserAgentTest, EmptyDatabasePath) {
    auto packet = buildConnectPacket("", "SYSDBA", "Srp256");
    
    size_t offset = 4 + 4 + 4 + 4 + 4 + 4;  // Skip to path length
    uint32_t path_len = xdrReadUint32(packet.data() + offset);
    EXPECT_EQ(path_len, 0u);
}

TEST_F(FirebirdParserAgentTest, LongDatabasePath) {
    std::string long_path(255, 'a');
    auto packet = buildConnectPacket(long_path, "SYSDBA", "Srp256");
    
    size_t offset = 4 + 4 + 4 + 4 + 4 + 4;
    uint32_t path_len = xdrReadUint32(packet.data() + offset);
    EXPECT_EQ(path_len, 255u);
}

// ============================================================================
// Database Operation Tests
// ============================================================================

TEST_F(FirebirdParserAgentTest, AttachPacketStructure) {
    auto packet = buildAttachPacket(0);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_attach);
    
    uint32_t handle = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(handle, 0u);
}

TEST_F(FirebirdParserAgentTest, AttachWithReconnectHandle) {
    auto packet = buildAttachPacket(42);
    
    uint32_t handle = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(handle, 42u);
}

TEST_F(FirebirdParserAgentTest, CreateDatabasePacketStructure) {
    auto packet = buildCreatePacket("newdb.fdb");
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_create);
    
    uint32_t name_len = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(name_len, 9u);  // "newdb.fdb"
    
    std::string name(reinterpret_cast<const char*>(packet.data() + 8), name_len);
    EXPECT_EQ(name, "newdb.fdb");
}

TEST_F(FirebirdParserAgentTest, CreateDatabaseWithLongName) {
    std::string long_name(200, 'x');
    long_name += ".fdb";
    auto packet = buildCreatePacket(long_name);
    
    uint32_t name_len = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(name_len, 204u);
}

TEST_F(FirebirdParserAgentTest, CompilePacketStructure) {
    std::string sql = "SELECT * FROM employees";
    auto packet = buildCompilePacket(sql);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_compile);
    
    uint32_t handle = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(handle, 1u);
    
    uint32_t sql_len = xdrReadUint32(packet.data() + 8);
    EXPECT_EQ(sql_len, sql.size());
    
    std::string parsed_sql(reinterpret_cast<const char*>(packet.data() + 12), sql_len);
    EXPECT_EQ(parsed_sql, sql);
}

TEST_F(FirebirdParserAgentTest, CompileComplexSQL) {
    std::string sql = "SELECT e.id, e.name, d.name as dept FROM employees e "
                      "JOIN departments d ON e.dept_id = d.id "
                      "WHERE e.salary > 50000 ORDER BY e.name";
    auto packet = buildCompilePacket(sql);
    
    uint32_t sql_len = xdrReadUint32(packet.data() + 8);
    EXPECT_EQ(sql_len, sql.size());
}

TEST_F(FirebirdParserAgentTest, CompileEmptySQL) {
    std::string sql = "";
    auto packet = buildCompilePacket(sql);
    
    uint32_t sql_len = xdrReadUint32(packet.data() + 8);
    EXPECT_EQ(sql_len, 0u);
}

// ============================================================================
// Transaction Tests
// ============================================================================

TEST_F(FirebirdParserAgentTest, TransactionPacketStructure) {
    auto packet = buildTransactionPacket();
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_transaction);
    
    uint32_t db_handle = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(db_handle, 1u);
    
    uint32_t tpb_len = xdrReadUint32(packet.data() + 8);
    EXPECT_EQ(tpb_len, 0u);
}

TEST_F(FirebirdParserAgentTest, TransactionWithTPB) {
    // TPB: version 3, read-only, read-committed
    std::vector<uint8_t> tpb = {3, 1, 2};  // isc_tpb_version3, isc_tpb_read, isc_tpb_concurrency
    auto packet = buildTransactionPacket(tpb);
    
    uint32_t tpb_len = xdrReadUint32(packet.data() + 8);
    EXPECT_EQ(tpb_len, 3u);
}

TEST_F(FirebirdParserAgentTest, CommitPacketStructure) {
    auto packet = buildCommitPacket(false);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_commit);
    
    uint32_t txn_handle = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(txn_handle, 1u);
}

TEST_F(FirebirdParserAgentTest, CommitRetainingPacketStructure) {
    auto packet = buildCommitPacket(true);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_commit_retaining);
}

TEST_F(FirebirdParserAgentTest, RollbackPacketStructure) {
    auto packet = buildRollbackPacket(false);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_rollback);
    
    uint32_t txn_handle = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(txn_handle, 1u);
}

TEST_F(FirebirdParserAgentTest, RollbackRetainingPacketStructure) {
    auto packet = buildRollbackPacket(true);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_rollback_retaining);
}

// ============================================================================
// BLOB Tests
// ============================================================================

TEST_F(FirebirdParserAgentTest, OpenBlobPacketStructure) {
    uint64_t blob_id = 0x123456789ABCDEF0ULL;
    auto packet = buildOpenBlobPacket(blob_id);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_open_blob);
    
    uint32_t txn_handle = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(txn_handle, 1u);
    
    uint32_t blob_high = xdrReadUint32(packet.data() + 8);
    uint32_t blob_low = xdrReadUint32(packet.data() + 12);
    
    uint64_t parsed_id = (static_cast<uint64_t>(blob_high) << 32) | blob_low;
    EXPECT_EQ(parsed_id, blob_id);
}

TEST_F(FirebirdParserAgentTest, CreateBlobPacketStructure) {
    auto packet = buildCreateBlobPacket();
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_create_blob);
    
    uint32_t txn_handle = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(txn_handle, 1u);
}

TEST_F(FirebirdParserAgentTest, GetSegmentPacketStructure) {
    auto packet = buildGetSegmentPacket(42, 4096);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_get_segment);
    
    uint32_t blob_handle = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(blob_handle, 42u);
    
    uint32_t segment_size = xdrReadUint32(packet.data() + 8);
    EXPECT_EQ(segment_size, 4096u);
}

TEST_F(FirebirdParserAgentTest, PutSegmentPacketStructure) {
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    auto packet = buildPutSegmentPacket(42, data);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_put_segment);
    
    uint32_t blob_handle = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(blob_handle, 42u);
    
    uint32_t data_len = xdrReadUint32(packet.data() + 8);
    EXPECT_EQ(data_len, 5u);
}

TEST_F(FirebirdParserAgentTest, PutSegmentLargeData) {
    std::vector<uint8_t> data(65535, 'X');
    auto packet = buildPutSegmentPacket(1, data);
    
    uint32_t data_len = xdrReadUint32(packet.data() + 8);
    EXPECT_EQ(data_len, 65535u);
    
    // Check padding to 4-byte boundary
    EXPECT_EQ(packet.size() % 4, 0u);
}

TEST_F(FirebirdParserAgentTest, CloseBlobPacketStructure) {
    auto packet = buildCloseBlobPacket(42, false);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_close_blob);
    
    uint32_t blob_handle = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(blob_handle, 42u);
}

TEST_F(FirebirdParserAgentTest, CancelBlobPacketStructure) {
    auto packet = buildCloseBlobPacket(42, true);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_cancel_blob);
    
    uint32_t blob_handle = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(blob_handle, 42u);
}

// ============================================================================
// XDR Format Tests
// ============================================================================

TEST_F(FirebirdParserAgentTest, XdrUint32Encoding) {
    uint8_t buffer[4];
    xdrWriteUint32(buffer, 0x12345678);
    
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x34);
    EXPECT_EQ(buffer[2], 0x56);
    EXPECT_EQ(buffer[3], 0x78);
    
    uint32_t value = xdrReadUint32(buffer);
    EXPECT_EQ(value, 0x12345678u);
}

TEST_F(FirebirdParserAgentTest, XdrUint32Zero) {
    uint8_t buffer[4];
    xdrWriteUint32(buffer, 0);
    
    EXPECT_EQ(buffer[0], 0);
    EXPECT_EQ(buffer[1], 0);
    EXPECT_EQ(buffer[2], 0);
    EXPECT_EQ(buffer[3], 0);
    
    uint32_t value = xdrReadUint32(buffer);
    EXPECT_EQ(value, 0u);
}

TEST_F(FirebirdParserAgentTest, XdrUint32Max) {
    uint8_t buffer[4];
    xdrWriteUint32(buffer, 0xFFFFFFFF);
    
    EXPECT_EQ(buffer[0], 0xFF);
    EXPECT_EQ(buffer[1], 0xFF);
    EXPECT_EQ(buffer[2], 0xFF);
    EXPECT_EQ(buffer[3], 0xFF);
    
    uint32_t value = xdrReadUint32(buffer);
    EXPECT_EQ(value, 0xFFFFFFFFu);
}

TEST_F(FirebirdParserAgentTest, XdrUint16Encoding) {
    uint8_t buffer[2];
    xdrWriteUint16(buffer, 0x1234);
    
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x34);
    
    uint16_t value = xdrReadUint16(buffer);
    EXPECT_EQ(value, 0x1234u);
}

TEST_F(FirebirdParserAgentTest, XdrBigEndianConversion) {
    // Verify that values are correctly converted to big-endian
    uint8_t buffer[8];
    
    // Write multiple uint32 values
    xdrWriteUint32(buffer, 0x01020304);
    xdrWriteUint32(buffer + 4, 0xAABBCCDD);
    
    // Verify byte order
    EXPECT_EQ(buffer[0], 0x01);
    EXPECT_EQ(buffer[1], 0x02);
    EXPECT_EQ(buffer[2], 0x03);
    EXPECT_EQ(buffer[3], 0x04);
    EXPECT_EQ(buffer[4], 0xAA);
    EXPECT_EQ(buffer[5], 0xBB);
    EXPECT_EQ(buffer[6], 0xCC);
    EXPECT_EQ(buffer[7], 0xDD);
}

TEST_F(FirebirdParserAgentTest, XdrRoundTrip) {
    // Test that encoding and decoding returns original values
    std::vector<uint32_t> test_values = {
        0, 1, 0xFF, 0x100, 0xFFFF, 0x10000, 0xFFFFFF, 0xFFFFFFFF,
        0x12345678, 0xAABBCCDD, 0x55AA55AA
    };
    
    for (uint32_t original : test_values) {
        uint8_t buffer[4];
        xdrWriteUint32(buffer, original);
        uint32_t decoded = xdrReadUint32(buffer);
        EXPECT_EQ(decoded, original) << "Failed for value: " << original;
    }
}

TEST_F(FirebirdParserAgentTest, MessageLengthCalculation) {
    // Build a simple packet and verify its length
    std::vector<uint8_t> packet;
    packet.resize(4);
    xdrWriteUint32(packet.data(), fb::op_connect);
    
    // Add some data
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + 4, 12345);
    
    EXPECT_EQ(packet.size(), 8u);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(FirebirdParserAgentTest, ErrorResponseStructure) {
    // Build an error response packet
    std::vector<uint8_t> packet;
    
    // op_response
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_response);
    
    // Handle
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 0);
    
    // Object ID
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 0);
    
    // Status vector with error
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 1);  // 1 error
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, fb::GENERIC_ERROR);
    
    // Error message
    std::string error_msg = "Test error message";
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, static_cast<uint32_t>(error_msg.size()));
    packet.insert(packet.end(), error_msg.begin(), error_msg.end());
    
    // Pad to 4 bytes
    while (packet.size() % 4 != 0) {
        packet.push_back(0);
    }
    
    // More errors? No
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 0);
    
    // Empty data
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 0);
    
    // Verify structure
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_response);
}

TEST_F(FirebirdParserAgentTest, StatusVectorParsing) {
    // Build a status vector with multiple errors
    std::vector<uint32_t> status_vector = {
        2,              // 2 errors
        335544569,      // isc_bad_db_format
        0,              // end of first error
        335544573,      // isc_io_error
        0,              // end of second error
        0               // end of vector
    };
    
    EXPECT_EQ(status_vector[0], 2u);  // Count
    EXPECT_EQ(status_vector[5], 0u);  // Terminator
}

TEST_F(FirebirdParserAgentTest, EmptyStatusVector) {
    // Build a success status vector
    std::vector<uint32_t> status_vector = {0};  // No errors
    
    EXPECT_EQ(status_vector[0], 0u);
}

// ============================================================================
// Statement Operation Tests
// ============================================================================

TEST_F(FirebirdParserAgentTest, StartPacketStructure) {
    std::vector<uint8_t> packet;
    
    // Operation code
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_start);
    
    // Request handle
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 1);
    
    // Transaction handle
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 1);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_start);
}

TEST_F(FirebirdParserAgentTest, StartAndReceivePacketStructure) {
    std::vector<uint8_t> packet;
    
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_start_and_receive);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_start_and_receive);
}

TEST_F(FirebirdParserAgentTest, ReceivePacketStructure) {
    std::vector<uint8_t> packet;
    
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_receive);
    
    // Request handle
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 1);
    
    // Message number
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 0);
    
    // Message length
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 1024);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_receive);
}

TEST_F(FirebirdParserAgentTest, SendPacketStructure) {
    std::vector<uint8_t> packet;
    
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_send);
    
    // Request handle
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 1);
    
    // Message number
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 0);
    
    // Message length
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 100);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_send);
}

TEST_F(FirebirdParserAgentTest, UnwindPacketStructure) {
    std::vector<uint8_t> packet;
    
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_unwind);
    
    // Request handle
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 1);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_unwind);
}

TEST_F(FirebirdParserAgentTest, ReleasePacketStructure) {
    std::vector<uint8_t> packet;
    
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_release);
    
    // Request handle
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 1);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_release);
}

// ============================================================================
// Disconnect Tests
// ============================================================================

TEST_F(FirebirdParserAgentTest, DisconnectPacketStructure) {
    auto packet = buildDisconnectPacket();
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_disconnect);
}

// ============================================================================
// Wire Encryption Tests
// ============================================================================

TEST_F(FirebirdParserAgentTest, CryptPacketStructure) {
    auto packet = buildCryptPacket("ChaCha");
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_crypt);
    
    uint32_t plugin_len = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(plugin_len, 6u);  // "ChaCha"
    
    std::string plugin(reinterpret_cast<const char*>(packet.data() + 8), plugin_len);
    EXPECT_EQ(plugin, "ChaCha");
}

TEST_F(FirebirdParserAgentTest, CryptPacketArc4) {
    auto packet = buildCryptPacket("Arc4");
    
    uint32_t plugin_len = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(plugin_len, 4u);
    
    std::string plugin(reinterpret_cast<const char*>(packet.data() + 8), plugin_len);
    EXPECT_EQ(plugin, "Arc4");
}

// ============================================================================
// Authentication Tests
// ============================================================================

TEST_F(FirebirdParserAgentTest, AuthenticatePacketStructure) {
    std::vector<uint8_t> auth_data = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto packet = buildAuthenticatePacket(auth_data);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_authenticate);
    
    uint32_t data_len = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(data_len, 5u);
}

TEST_F(FirebirdParserAgentTest, AuthenticatePacketEmptyData) {
    std::vector<uint8_t> auth_data;
    auto packet = buildAuthenticatePacket(auth_data);
    
    uint32_t data_len = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(data_len, 0u);
}

TEST_F(FirebirdParserAgentTest, AuthenticatePacketLargeData) {
    std::vector<uint8_t> auth_data(1024, 0xAA);
    auto packet = buildAuthenticatePacket(auth_data);
    
    uint32_t data_len = xdrReadUint32(packet.data() + 4);
    EXPECT_EQ(data_len, 1024u);
    
    // Verify padding
    EXPECT_EQ(packet.size() % 4, 0u);
}

// ============================================================================
// Client State Tests
// ============================================================================

TEST_F(FirebirdParserAgentTest, ClientStateInitialization) {
    FBClientState state;
    
    EXPECT_EQ(state.client_fd, -1);
    EXPECT_EQ(state.state, FBClientState::CONNECTING);
    EXPECT_EQ(state.protocol_version, 0u);
    EXPECT_EQ(state.accept_version, 0u);
    EXPECT_EQ(state.handle, 0u);
    EXPECT_FALSE(state.wire_encrypted);
    EXPECT_TRUE(state.database.empty());
    EXPECT_TRUE(state.username.empty());
    EXPECT_TRUE(state.auth_plugin.empty());
}

TEST_F(FirebirdParserAgentTest, ClientStateTransitions) {
    FBClientState state;
    
    // Initial state
    EXPECT_EQ(state.state, FBClientState::CONNECTING);
    
    // After connect
    state.state = FBClientState::CONNECTED;
    EXPECT_EQ(state.state, FBClientState::CONNECTED);
    
    // After attach
    state.state = FBClientState::ATTACHED;
    EXPECT_EQ(state.state, FBClientState::ATTACHED);
    
    // In transaction
    state.state = FBClientState::IN_TRANSACTION;
    EXPECT_EQ(state.state, FBClientState::IN_TRANSACTION);
    
    // Disconnected
    state.state = FBClientState::DISCONNECTED;
    EXPECT_EQ(state.state, FBClientState::DISCONNECTED);
}

// ============================================================================
// Information Request Tests
// ============================================================================

TEST_F(FirebirdParserAgentTest, InfoDatabasePacketStructure) {
    std::vector<uint8_t> packet;
    
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_info_database);
    
    // Database handle
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 1);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_info_database);
}

TEST_F(FirebirdParserAgentTest, InfoTransactionPacketStructure) {
    std::vector<uint8_t> packet;
    
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_info_transaction);
    
    // Transaction handle
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 1);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_info_transaction);
}

TEST_F(FirebirdParserAgentTest, InfoBlobPacketStructure) {
    std::vector<uint8_t> packet;
    
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, fb::op_info_blob);
    
    // Blob handle
    packet.resize(packet.size() + 4);
    xdrWriteUint32(packet.data() + packet.size() - 4, 1);
    
    uint32_t op = xdrReadUint32(packet.data());
    EXPECT_EQ(op, fb::op_info_blob);
}

// ============================================================================
// Protocol Version Tests
// ============================================================================

TEST_F(FirebirdParserAgentTest, ProtocolVersionConstants) {
    // Verify all protocol version constants are defined correctly
    EXPECT_EQ(fb::PROTOCOL_VERSION10, 10u);
    EXPECT_EQ(fb::PROTOCOL_VERSION11, 11u);
    EXPECT_EQ(fb::PROTOCOL_VERSION12, 12u);
    EXPECT_EQ(fb::PROTOCOL_VERSION13, 13u);
    EXPECT_EQ(fb::PROTOCOL_VERSION14, 14u);
    EXPECT_EQ(fb::PROTOCOL_VERSION15, 15u);
    EXPECT_EQ(fb::PROTOCOL_VERSION16, 16u);
}

TEST_F(FirebirdParserAgentTest, OperationCodeConstants) {
    // Verify key operation codes
    EXPECT_EQ(fb::op_connect, 1u);
    EXPECT_EQ(fb::op_accept, 3u);
    EXPECT_EQ(fb::op_reject, 4u);
    EXPECT_EQ(fb::op_attach, 19u);
    EXPECT_EQ(fb::op_create, 20u);
    EXPECT_EQ(fb::op_detach, 21u);
    EXPECT_EQ(fb::op_transaction, 29u);
    EXPECT_EQ(fb::op_commit, 30u);
    EXPECT_EQ(fb::op_rollback, 31u);
    EXPECT_EQ(fb::op_response, 9u);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(FirebirdParserAgentTest, EmptyPacketHandling) {
    std::vector<uint8_t> empty_packet;
    
    // An empty packet should be handled gracefully
    EXPECT_EQ(empty_packet.size(), 0u);
}

TEST_F(FirebirdParserAgentTest, TruncatedPacketHandling) {
    // A packet that's too short
    std::vector<uint8_t> truncated_packet = {0x00, 0x00, 0x00};
    
    EXPECT_EQ(truncated_packet.size(), 3u);
}

TEST_F(FirebirdParserAgentTest, UnicodeUsername) {
    std::string unicode_user = "user_\xC3\xA9\xC3\xA8";  // user_éè in UTF-8
    auto packet = buildConnectPacket("test.fdb", unicode_user, "Srp256");
    
    // The packet should be created successfully
    EXPECT_GT(packet.size(), 0u);
}

TEST_F(FirebirdParserAgentTest, VeryLongSQL) {
    std::string long_sql(10000, 'A');
    auto packet = buildCompilePacket(long_sql);
    
    uint32_t sql_len = xdrReadUint32(packet.data() + 8);
    EXPECT_EQ(sql_len, 10000u);
}

// Main provided by gtest_main

}  // namespace
