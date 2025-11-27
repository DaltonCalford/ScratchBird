/**
 * Unit Tests for IPC Server Infrastructure
 *
 * ScratchBird Local Server Architecture - Phase 1
 *
 * Tests:
 * - Platform detection and IPC method selection
 * - Path generation for sockets/pipes
 * - Server/client creation and lifecycle
 * - Connection establishment and data transfer
 * - Timeout handling
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>

#include "scratchbird/server/ipc_server.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::server;
using namespace scratchbird::core;

// ============================================================================
// Platform Detection Tests
// ============================================================================

class IPCPlatformTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IPCPlatformTest, DefaultIPCMethod) {
    IPCMethod method = getDefaultIPCMethod();

#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    EXPECT_EQ(method, IPCMethod::UNIX_SOCKET);
#elif defined(_WIN32)
    EXPECT_EQ(method, IPCMethod::NAMED_PIPE);
#else
    EXPECT_EQ(method, IPCMethod::TCP_LOCALHOST);
#endif
}

TEST_F(IPCPlatformTest, IPCMethodToString) {
    EXPECT_STREQ(ipcMethodToString(IPCMethod::AUTO), "auto");
    EXPECT_STREQ(ipcMethodToString(IPCMethod::UNIX_SOCKET), "unix");
    EXPECT_STREQ(ipcMethodToString(IPCMethod::NAMED_PIPE), "pipe");
    EXPECT_STREQ(ipcMethodToString(IPCMethod::TCP_LOCALHOST), "tcp");
}

TEST_F(IPCPlatformTest, StringToIPCMethod) {
    // Case insensitive matching
    EXPECT_EQ(stringToIPCMethod("auto"), IPCMethod::AUTO);
    EXPECT_EQ(stringToIPCMethod("AUTO"), IPCMethod::AUTO);
    EXPECT_EQ(stringToIPCMethod("unix"), IPCMethod::UNIX_SOCKET);
    EXPECT_EQ(stringToIPCMethod("UNIX"), IPCMethod::UNIX_SOCKET);
    EXPECT_EQ(stringToIPCMethod("unix_socket"), IPCMethod::UNIX_SOCKET);
    EXPECT_EQ(stringToIPCMethod("pipe"), IPCMethod::NAMED_PIPE);
    EXPECT_EQ(stringToIPCMethod("named_pipe"), IPCMethod::NAMED_PIPE);
    EXPECT_EQ(stringToIPCMethod("tcp"), IPCMethod::TCP_LOCALHOST);
    EXPECT_EQ(stringToIPCMethod("tcp_localhost"), IPCMethod::TCP_LOCALHOST);

    // Unknown should default to AUTO
    EXPECT_EQ(stringToIPCMethod("unknown"), IPCMethod::AUTO);
    EXPECT_EQ(stringToIPCMethod(""), IPCMethod::AUTO);
}

// ============================================================================
// Path Generation Tests
// ============================================================================

class IPCPathTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IPCPathTest, UnixSocketPath) {
    std::string path = getIPCPath("testdb", IPCMethod::UNIX_SOCKET);
    EXPECT_EQ(path, "/tmp/scratchbird-testdb.sock");
}

TEST_F(IPCPathTest, NamedPipePath) {
    std::string path = getIPCPath("testdb", IPCMethod::NAMED_PIPE);
    EXPECT_EQ(path, "\\\\.\\pipe\\scratchbird-testdb");
}

TEST_F(IPCPathTest, TCPLocalhostPath) {
    std::string path = getIPCPath("testdb", IPCMethod::TCP_LOCALHOST);
    EXPECT_EQ(path, "127.0.0.1:5433");
}

TEST_F(IPCPathTest, AutoResolvesToPlatformDefault) {
    std::string auto_path = getIPCPath("testdb", IPCMethod::AUTO);
    std::string default_path = getIPCPath("testdb", getDefaultIPCMethod());
    EXPECT_EQ(auto_path, default_path);
}

TEST_F(IPCPathTest, SpecialCharactersSanitized) {
    // Special characters should be removed
    std::string path = getIPCPath("test@db#name!", IPCMethod::UNIX_SOCKET);
    EXPECT_EQ(path, "/tmp/scratchbird-testdbname.sock");
}

TEST_F(IPCPathTest, EmptyNameUsesDefault) {
    std::string path = getIPCPath("", IPCMethod::UNIX_SOCKET);
    EXPECT_EQ(path, "/tmp/scratchbird-default.sock");
}

TEST_F(IPCPathTest, PIDFilePath) {
    std::string pid_path = getPIDFilePath("mydb");

#ifdef _WIN32
    // Windows uses %TEMP% - just check it ends correctly
    EXPECT_TRUE(pid_path.find("scratchbird-mydb.pid") != std::string::npos);
#else
    EXPECT_EQ(pid_path, "/tmp/scratchbird-mydb.pid");
#endif
}

// ============================================================================
// Server Creation Tests
// ============================================================================

class IPCServerCreationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IPCServerCreationTest, CreateServerWithDefaultMethod) {
    IPCServerConfig config("test_creation_db");
    ErrorContext ctx;

    auto server = IPCServer::create(config, &ctx);
    ASSERT_NE(server, nullptr);
    EXPECT_FALSE(server->isListening());
}

TEST_F(IPCServerCreationTest, CreateServerWithTCP) {
    IPCServerConfig config("test_tcp_db", IPCMethod::TCP_LOCALHOST);
    config.tcp_port = 15433;  // Use non-standard port to avoid conflicts
    ErrorContext ctx;

    auto server = IPCServer::create(config, &ctx);
    ASSERT_NE(server, nullptr);
    EXPECT_EQ(server->getMethod(), IPCMethod::TCP_LOCALHOST);
}

TEST_F(IPCServerCreationTest, CreateClientWithDefaultMethod) {
    IPCClientConfig config("test_client_db");
    ErrorContext ctx;

    auto client = IPCClient::create(config, &ctx);
    ASSERT_NE(client, nullptr);
    EXPECT_FALSE(client->isConnected());
}

// ============================================================================
// TCP Server/Client Integration Tests
// ============================================================================

class TCPIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use unique port for each test to avoid port conflicts
        static std::atomic<uint16_t> port_counter{15500};
        test_port_ = port_counter++;
    }

    void TearDown() override {}

    uint16_t test_port_;
};

TEST_F(TCPIntegrationTest, ServerListenAndClose) {
    IPCServerConfig config("tcp_listen_test", IPCMethod::TCP_LOCALHOST);
    config.tcp_port = test_port_;
    config.accept_timeout_ms = 100;
    ErrorContext ctx;

    auto server = IPCServer::create(config, &ctx);
    ASSERT_NE(server, nullptr);

    // Start listening
    Status status = server->listen(&ctx);
    ASSERT_EQ(status, Status::OK) << "Listen failed: " << ctx.message;
    EXPECT_TRUE(server->isListening());
    EXPECT_EQ(server->getConnectionCount(), 0u);

    // Close server
    server->close();
    EXPECT_FALSE(server->isListening());
}

TEST_F(TCPIntegrationTest, ClientConnectWithoutServer) {
    IPCClientConfig config("tcp_noserver_test", IPCMethod::TCP_LOCALHOST);
    config.tcp_port = test_port_;
    config.connect_timeout_ms = 100;
    ErrorContext ctx;

    auto client = IPCClient::create(config, &ctx);
    ASSERT_NE(client, nullptr);

    // Should fail - no server running
    Status status = client->connect(&ctx);
    EXPECT_NE(status, Status::OK);
    EXPECT_FALSE(client->isConnected());
}

TEST_F(TCPIntegrationTest, ClientConnectToServer) {
    IPCServerConfig server_config("tcp_connect_test", IPCMethod::TCP_LOCALHOST);
    server_config.tcp_port = test_port_;
    server_config.accept_timeout_ms = 1000;
    ErrorContext server_ctx;

    auto server = IPCServer::create(server_config, &server_ctx);
    ASSERT_NE(server, nullptr);

    Status status = server->listen(&server_ctx);
    ASSERT_EQ(status, Status::OK) << "Server listen failed: " << server_ctx.message;

    // Start accept thread
    std::atomic<bool> client_connected{false};
    std::unique_ptr<IPCConnection> server_conn;

    std::thread accept_thread([&]() {
        ErrorContext ctx;
        server_conn = server->accept(&ctx);
        if (server_conn) {
            client_connected = true;
        }
    });

    // Give server time to start accepting
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Connect client
    IPCClientConfig client_config("tcp_connect_test", IPCMethod::TCP_LOCALHOST);
    client_config.tcp_port = test_port_;
    client_config.connect_timeout_ms = 1000;
    ErrorContext client_ctx;

    auto client = IPCClient::create(client_config, &client_ctx);
    ASSERT_NE(client, nullptr);

    status = client->connect(&client_ctx);
    EXPECT_EQ(status, Status::OK) << "Client connect failed: " << client_ctx.message;
    EXPECT_TRUE(client->isConnected());

    // Wait for server to accept
    accept_thread.join();
    EXPECT_TRUE(client_connected);
    ASSERT_NE(server_conn, nullptr);
    EXPECT_TRUE(server_conn->isOpen());

    // Cleanup
    client->disconnect();
    server_conn->close();
    server->close();
}

TEST_F(TCPIntegrationTest, DataTransfer) {
    IPCServerConfig server_config("tcp_data_test", IPCMethod::TCP_LOCALHOST);
    server_config.tcp_port = test_port_;
    server_config.accept_timeout_ms = 1000;
    ErrorContext server_ctx;

    auto server = IPCServer::create(server_config, &server_ctx);
    ASSERT_NE(server, nullptr);

    Status status = server->listen(&server_ctx);
    ASSERT_EQ(status, Status::OK);

    // Server accept thread
    std::unique_ptr<IPCConnection> server_conn;
    std::thread accept_thread([&]() {
        ErrorContext ctx;
        server_conn = server->accept(&ctx);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Connect client
    IPCClientConfig client_config("tcp_data_test", IPCMethod::TCP_LOCALHOST);
    client_config.tcp_port = test_port_;
    ErrorContext client_ctx;

    auto client = IPCClient::create(client_config, &client_ctx);
    status = client->connect(&client_ctx);
    ASSERT_EQ(status, Status::OK);

    accept_thread.join();
    ASSERT_NE(server_conn, nullptr);

    // Test sending data from client to server
    const char* test_message = "Hello, ScratchBird Server!";
    size_t msg_len = strlen(test_message) + 1;

    auto* client_conn = client->getConnection();
    ASSERT_NE(client_conn, nullptr);

    ErrorContext write_ctx;
    status = client_conn->writeExact(test_message, msg_len, &write_ctx);
    EXPECT_EQ(status, Status::OK) << "Write failed: " << write_ctx.message;

    // Server receives data
    char recv_buffer[128];
    ErrorContext read_ctx;
    status = server_conn->readExact(recv_buffer, msg_len, &read_ctx);
    EXPECT_EQ(status, Status::OK) << "Read failed: " << read_ctx.message;
    EXPECT_STREQ(recv_buffer, test_message);

    // Test sending data from server to client
    const char* response = "Message received!";
    size_t resp_len = strlen(response) + 1;

    status = server_conn->writeExact(response, resp_len, &write_ctx);
    EXPECT_EQ(status, Status::OK);

    status = client_conn->readExact(recv_buffer, resp_len, &read_ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_STREQ(recv_buffer, response);

    // Cleanup
    client->disconnect();
    server_conn->close();
    server->close();
}

TEST_F(TCPIntegrationTest, LargeDataTransfer) {
    IPCServerConfig server_config("tcp_large_test", IPCMethod::TCP_LOCALHOST);
    server_config.tcp_port = test_port_;
    server_config.accept_timeout_ms = 1000;
    ErrorContext ctx;

    auto server = IPCServer::create(server_config, &ctx);
    server->listen(&ctx);

    std::unique_ptr<IPCConnection> server_conn;
    std::thread accept_thread([&]() {
        ErrorContext actx;
        server_conn = server->accept(&actx);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    IPCClientConfig client_config("tcp_large_test", IPCMethod::TCP_LOCALHOST);
    client_config.tcp_port = test_port_;
    auto client = IPCClient::create(client_config, &ctx);
    client->connect(&ctx);

    accept_thread.join();
    ASSERT_NE(server_conn, nullptr);

    // Send 64KB of data
    const size_t data_size = 64 * 1024;
    std::vector<uint8_t> send_data(data_size);
    std::vector<uint8_t> recv_data(data_size);

    // Fill with pattern
    for (size_t i = 0; i < data_size; i++) {
        send_data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    auto* client_conn = client->getConnection();

    // Send from client
    Status status = client_conn->writeExact(send_data.data(), data_size, &ctx);
    EXPECT_EQ(status, Status::OK) << "Large write failed: " << ctx.message;

    // Receive on server
    status = server_conn->readExact(recv_data.data(), data_size, &ctx);
    EXPECT_EQ(status, Status::OK) << "Large read failed: " << ctx.message;

    // Verify data
    EXPECT_EQ(send_data, recv_data);

    client->disconnect();
    server_conn->close();
    server->close();
}

TEST_F(TCPIntegrationTest, ConnectionStats) {
    IPCServerConfig server_config("tcp_stats_test", IPCMethod::TCP_LOCALHOST);
    server_config.tcp_port = test_port_;
    ErrorContext ctx;

    auto server = IPCServer::create(server_config, &ctx);
    server->listen(&ctx);

    std::unique_ptr<IPCConnection> server_conn;
    std::thread accept_thread([&]() {
        ErrorContext actx;
        server_conn = server->accept(&actx);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    IPCClientConfig client_config("tcp_stats_test", IPCMethod::TCP_LOCALHOST);
    client_config.tcp_port = test_port_;
    auto client = IPCClient::create(client_config, &ctx);
    client->connect(&ctx);

    accept_thread.join();
    ASSERT_NE(server_conn, nullptr);

    auto* client_conn = client->getConnection();

    // Initial stats
    ConnectionStats client_stats = client_conn->getStats();
    EXPECT_EQ(client_stats.bytes_sent, 0u);
    EXPECT_EQ(client_stats.bytes_received, 0u);

    // Send some data
    const char* msg = "Test stats";
    client_conn->writeExact(msg, strlen(msg) + 1, &ctx);

    // Check stats updated
    client_stats = client_conn->getStats();
    EXPECT_GT(client_stats.bytes_sent, 0u);
    EXPECT_GT(client_stats.messages_sent, 0u);

    client->disconnect();
    server_conn->close();
    server->close();
}

// ============================================================================
// Unix Socket Tests (Linux/macOS only)
// ============================================================================

#if defined(__linux__) || defined(__APPLE__)

class UnixSocketTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate unique socket name
        static std::atomic<int> counter{0};
        db_name_ = "unix_test_" + std::to_string(counter++);
    }

    void TearDown() override {
        // Clean up socket file
        std::string socket_path = getIPCPath(db_name_, IPCMethod::UNIX_SOCKET);
        unlink(socket_path.c_str());
    }

    std::string db_name_;
};

TEST_F(UnixSocketTest, ServerListenAndClose) {
    IPCServerConfig config(db_name_, IPCMethod::UNIX_SOCKET);
    config.accept_timeout_ms = 100;
    ErrorContext ctx;

    auto server = IPCServer::create(config, &ctx);
    ASSERT_NE(server, nullptr);
    EXPECT_EQ(server->getMethod(), IPCMethod::UNIX_SOCKET);

    Status status = server->listen(&ctx);
    ASSERT_EQ(status, Status::OK) << "Listen failed: " << ctx.message;
    EXPECT_TRUE(server->isListening());

    // Verify socket file created
    std::string socket_path = server->getAddress();
    struct stat st;
    EXPECT_EQ(stat(socket_path.c_str(), &st), 0);

    server->close();
    EXPECT_FALSE(server->isListening());
}

TEST_F(UnixSocketTest, ClientConnectToServer) {
    IPCServerConfig server_config(db_name_, IPCMethod::UNIX_SOCKET);
    server_config.accept_timeout_ms = 1000;
    ErrorContext ctx;

    auto server = IPCServer::create(server_config, &ctx);
    server->listen(&ctx);

    std::unique_ptr<IPCConnection> server_conn;
    std::thread accept_thread([&]() {
        ErrorContext actx;
        server_conn = server->accept(&actx);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    IPCClientConfig client_config(db_name_, IPCMethod::UNIX_SOCKET);
    auto client = IPCClient::create(client_config, &ctx);
    Status status = client->connect(&ctx);
    EXPECT_EQ(status, Status::OK) << "Connect failed: " << ctx.message;

    accept_thread.join();
    ASSERT_NE(server_conn, nullptr);

    // Verify peer credentials available on Unix
    PeerCredentials creds = server_conn->getPeerCredentials();
    EXPECT_TRUE(creds.available);
    EXPECT_GT(creds.pid, 0u);  // Should have valid PID

    client->disconnect();
    server_conn->close();
    server->close();
}

#endif  // Unix-specific tests

// ============================================================================
// Server Running Check Tests
// ============================================================================

TEST(ServerRunningTest, NoServerRunning) {
    // With no server running, should return false
    bool running = isServerRunning("nonexistent_db_12345");
    EXPECT_FALSE(running);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
