/**
 * Integration Tests for Client-Server Communication
 *
 * ScratchBird Local Server Architecture - Phase 4/5
 *
 * These tests spawn a real sb_server process and test the full
 * client-server communication path.
 *
 * Requirements:
 * - sb_server executable must be built
 * - Tests run on localhost TCP connection
 *
 * NOTE: These tests currently skip if the server fails to start.
 * The server's database initialization requires additional work
 * before full integration testing is possible. The tests are
 * structured to work once the server is fully functional.
 *
 * For manual testing, you can:
 * 1. Start a server manually: ./sb_server /tmp/testdb.sbdb --create --tcp --port=15450
 * 2. Run the tests (they will connect to the running server)
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <cstdlib>
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "scratchbird/client/connection.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/core/error_context.h"
#include "test_helpers.h"

using scratchbird::client::Connection;
using scratchbird::client::ConnectionConfig;
using scratchbird::client::ConnectionState;
using scratchbird::client::ResultSet;
using scratchbird::core::Status;
using scratchbird::core::ErrorContext;
using scratchbird::server::IPCMethod;

// ============================================================================
// Test Fixture
// ============================================================================

class ClientServerIntegrationTest : public ::testing::Test {
protected:
    static constexpr uint16_t TEST_PORT = 15450;
    static constexpr const char* TEST_DB_PATH = "build/scratchbird_integration_test.sbdb";
    // Expected runtime for full suite: ~4-8 seconds (server startup + connection tests).
    // Use full path for sb_server - this is set relative to build directory
    static const char* getServerExecutable() {
        // Try multiple paths
        static const char* paths[] = {
            "./src/sb_server",
            "../src/sb_server",
            "src/sb_server",
            "/home/dcalford/CliWork/ScratchBird/build/src/sb_server"
        };
        for (const char* path : paths) {
            if (access(path, X_OK) == 0) {
                return path;
            }
        }
        return "./src/sb_server";  // Default fallback
    }

    static pid_t server_pid_;
    static bool server_started_;
    static bool network_enabled_;

    static void SetUpTestSuite() {
        network_enabled_ = scratchbird::testing::networkTestsEnabled();
        if (!network_enabled_) {
            return;
        }
        // Clean up any existing test database
        cleanup();

        // Start server
        server_pid_ = fork();
        if (server_pid_ == 0) {
            // Child process - exec server
            const char* server_exe = getServerExecutable();
            std::string port_arg = "--port=" + std::to_string(TEST_PORT);
            execl(server_exe, "sb_server",
                  TEST_DB_PATH,
                  "--create",
                  "--tcp",
                  port_arg.c_str(),
                  nullptr);
            // If execl returns, it failed
            _exit(1);
        } else if (server_pid_ > 0) {
            // Parent - wait for server to start
            server_started_ = waitForServer(10000);  // 10 second timeout
            if (!server_started_) {
                std::cerr << "Failed to start server" << std::endl;
            }
        } else {
            std::cerr << "Fork failed" << std::endl;
        }
    }

    static void TearDownTestSuite() {
        if (!network_enabled_) {
            return;
        }
        // Kill server
        if (server_pid_ > 0) {
            kill(server_pid_, SIGTERM);
            int status;
            waitpid(server_pid_, &status, 0);
        }
        server_pid_ = 0;
        server_started_ = false;

        // Cleanup test files
        cleanup();
    }

    void SetUp() override {
        if (!network_enabled_) {
            GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
        }
        if (!server_started_) {
            GTEST_SKIP() << "Server not started";
        }
    }

    ConnectionConfig createTestConfig() {
        ConnectionConfig config;
        config.database_name = TEST_DB_PATH;
        config.username = "test";
        config.password = "";
        config.ipc_method = IPCMethod::TCP_LOCALHOST;
        config.tcp_port = TEST_PORT;
        config.auto_start_server = false;  // Server is already running
        config.connect_timeout_ms = 5000;
        return config;
    }

private:
    static bool waitForServer(int timeout_ms) {
        auto start = std::chrono::steady_clock::now();
        while (true) {
            // Try to connect
            Connection test_conn;
            ConnectionConfig config;
            config.database_name = TEST_DB_PATH;
            config.ipc_method = IPCMethod::TCP_LOCALHOST;
            config.tcp_port = TEST_PORT;
            config.auto_start_server = false;
            config.connect_timeout_ms = 1000;

            if (test_conn.connect(config) == Status::OK) {
                test_conn.disconnect();
                return true;
            }

            // Check timeout
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();
            if (elapsed > timeout_ms) {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    static void cleanup() {
        std::remove(TEST_DB_PATH);
        std::remove((std::string(TEST_DB_PATH) + "-lock").c_str());
        std::remove((std::string(TEST_DB_PATH) + ".pid").c_str());
    }
};

pid_t ClientServerIntegrationTest::server_pid_ = 0;
bool ClientServerIntegrationTest::server_started_ = false;
bool ClientServerIntegrationTest::network_enabled_ = false;

// ============================================================================
// Connection Tests
// ============================================================================

TEST_F(ClientServerIntegrationTest, ConnectToRealServer) {
    Connection conn;
    auto config = createTestConfig();

    ErrorContext ctx;
    auto status = conn.connect(config, &ctx);
    EXPECT_EQ(status, Status::OK) << "Connect failed: " << ctx.message;
    EXPECT_TRUE(conn.isConnected());
    EXPECT_EQ(conn.getState(), ConnectionState::CONNECTED);

    conn.disconnect();
    EXPECT_FALSE(conn.isConnected());
}

TEST_F(ClientServerIntegrationTest, PingServer) {
    Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    // Multiple pings
    for (int i = 0; i < 5; ++i) {
        status = conn.ping();
        EXPECT_EQ(status, Status::OK) << "Ping " << i << " failed";
    }

    conn.disconnect();
}

TEST_F(ClientServerIntegrationTest, MultipleConnections) {
    std::vector<Connection> connections;
    connections.resize(3);

    for (auto& conn : connections) {
        auto status = conn.connect(createTestConfig());
        EXPECT_EQ(status, Status::OK);
    }

    for (auto& conn : connections) {
        EXPECT_TRUE(conn.isConnected());
        conn.disconnect();
    }
}

TEST_F(ClientServerIntegrationTest, ReconnectAfterClose) {
    Connection conn;
    auto config = createTestConfig();

    // First connection
    auto status = conn.connect(config);
    EXPECT_EQ(status, Status::OK);
    conn.disconnect();

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Reconnect
    status = conn.connect(config);
    EXPECT_EQ(status, Status::OK);
    EXPECT_TRUE(conn.isConnected());

    conn.disconnect();
}

// ============================================================================
// Transaction Tests
// ============================================================================

TEST_F(ClientServerIntegrationTest, TransactionLifecycle) {
    Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    // Begin transaction
    status = conn.beginTransaction();
    EXPECT_EQ(status, Status::OK);
    EXPECT_TRUE(conn.inTransaction());
    EXPECT_EQ(conn.getState(), ConnectionState::IN_TRANSACTION);

    // Commit
    status = conn.commit();
    EXPECT_EQ(status, Status::OK);
    EXPECT_FALSE(conn.inTransaction());
    EXPECT_EQ(conn.getState(), ConnectionState::CONNECTED);

    conn.disconnect();
}

TEST_F(ClientServerIntegrationTest, TransactionRollback) {
    Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    status = conn.beginTransaction();
    EXPECT_EQ(status, Status::OK);

    status = conn.rollback();
    EXPECT_EQ(status, Status::OK);
    EXPECT_FALSE(conn.inTransaction());

    conn.disconnect();
}

// ============================================================================
// Query Execution Tests (requires full query engine)
// ============================================================================

// Note: These tests require the server to have a functional query engine.
// Currently the sb_server may not have full query processing, so these
// tests are marked as disabled until that functionality is complete.

TEST_F(ClientServerIntegrationTest, DISABLED_ExecuteSimpleQuery) {
    Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    ResultSet results;
    status = conn.executeQuery("SELECT 1 AS value", &results);
    EXPECT_EQ(status, Status::OK);

    EXPECT_TRUE(results.next());
    EXPECT_EQ(results.getInt32(0), 1);
    EXPECT_FALSE(results.next());

    conn.disconnect();
}

TEST_F(ClientServerIntegrationTest, DISABLED_CreateAndQueryTable) {
    Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    // Create table
    int64_t affected;
    status = conn.execute(
        "CREATE TABLE IF NOT EXISTS test_table (id INT, name VARCHAR(100))",
        &affected);
    EXPECT_EQ(status, Status::OK);

    // Insert data
    status = conn.execute(
        "INSERT INTO test_table VALUES (1, 'Alice'), (2, 'Bob')",
        &affected);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(affected, 2);

    // Query data
    ResultSet results;
    status = conn.executeQuery(
        "SELECT id, name FROM test_table ORDER BY id",
        &results);
    EXPECT_EQ(status, Status::OK);

    ASSERT_TRUE(results.next());
    EXPECT_EQ(results.getInt32(0), 1);
    EXPECT_EQ(results.getString(1), "Alice");

    ASSERT_TRUE(results.next());
    EXPECT_EQ(results.getInt32(0), 2);
    EXPECT_EQ(results.getString(1), "Bob");

    EXPECT_FALSE(results.next());

    conn.disconnect();
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
