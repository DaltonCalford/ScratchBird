/**
 * Unit Tests for Client Library
 *
 * ScratchBird Local Server Architecture - Phase 4
 *
 * Tests:
 * - Connection lifecycle (connect/disconnect)
 * - Auto-start server mechanism
 * - Query execution and ResultSet iteration
 * - Transaction management
 * - Prepared statements
 * - Connection pooling
 * - Error handling
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cstring>

#include "scratchbird/client/connection.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/protocol/wire_protocol.h"
#include "scratchbird/core/error_context.h"

using scratchbird::protocol::MessageType;
using scratchbird::protocol::Message;
using scratchbird::protocol::ProtocolCodec;
using scratchbird::protocol::ProtocolSession;
using scratchbird::protocol::WireType;
using scratchbird::core::Status;
using scratchbird::core::ErrorContext;

// ============================================================================
// Test Fixtures
// ============================================================================

/**
 * Mock server for testing client library without full server
 */
class MockServer {
public:
    explicit MockServer(const std::string& db_name)
        : db_name_(db_name), running_(false) {}

    ~MockServer() {
        stop();
    }

    bool start() {
        scratchbird::server::IPCServerConfig config;
        config.database_name = db_name_;
        config.method = scratchbird::server::IPCMethod::TCP_LOCALHOST;
        config.tcp_port = 15433;  // Non-standard port for testing
        config.accept_timeout_ms = 100;

        ErrorContext ctx;
        server_ = scratchbird::server::IPCServer::create(config, &ctx);
        if (!server_) {
            return false;
        }

        auto status = server_->listen(&ctx);
        if (status != Status::OK) {
            return false;
        }

        running_ = true;
        server_thread_ = std::thread(&MockServer::serverLoop, this);
        return true;
    }

    void stop() {
        running_ = false;
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        if (server_) {
            server_->close();
        }
    }

    uint16_t getPort() const { return 15433; }

private:
    void serverLoop() {
        while (running_) {
            ErrorContext ctx;
            auto conn = server_->accept(&ctx);
            if (!conn) {
                continue;
            }

            // Handle connection in a simple way
            handleConnection(std::move(conn));
        }
    }

    void handleConnection(std::unique_ptr<scratchbird::server::IPCConnection> conn) {
        // Create protocol session
        ProtocolSession session(conn.get());
        ErrorContext ctx;

        while (conn->isOpen() && running_) {
            Message request;
            auto status = session.receiveMessage(request, &ctx);
            if (status != Status::OK) {
                break;
            }

            // Handle different message types
            switch (request.getType()) {
                case MessageType::CONNECT_REQUEST: {
                    handleConnectRequest(session, request);
                    break;
                }
                case MessageType::AUTH_REQUEST: {
                    handleAuthRequest(session, request);
                    break;
                }
                case MessageType::QUERY: {
                    handleQuery(session, request);
                    break;
                }
                case MessageType::BEGIN_TRANSACTION: {
                    handleBeginTransaction(session, request);
                    break;
                }
                case MessageType::COMMIT: {
                    handleCommit(session, request);
                    break;
                }
                case MessageType::ROLLBACK: {
                    handleRollback(session, request);
                    break;
                }
                case MessageType::PING: {
                    handlePing(session, request);
                    break;
                }
                case MessageType::DISCONNECT: {
                    return;  // Close connection
                }
                default:
                    break;
            }
        }
    }

    void handleConnectRequest(ProtocolSession& session, Message& /*request*/) {
        // Generate session ID
        uint8_t session_id[16];
        for (int i = 0; i < 16; ++i) {
            session_id[i] = static_cast<uint8_t>(i);
        }

        auto response = ProtocolCodec::buildConnectResponse(true, session_id);
        session.sendMessage(response);
    }

    void handleAuthRequest(ProtocolSession& session, Message& /*request*/) {
        auto response = ProtocolCodec::buildAuthResponse(true, 1, "");
        session.sendMessage(response);
    }

    void handleQuery(ProtocolSession& session, Message& request) {
        // Parse query
        uint8_t session_id[16];
        std::string sql;
        uint8_t flags;
        ProtocolCodec::parseQuery(request, session_id, sql, flags);

        // Simple mock response based on query
        if (sql.find("SELECT") == 0 || sql.find("select") == 0) {
            // Send row description
            std::vector<ProtocolCodec::ColumnInfo> columns;
            ProtocolCodec::ColumnInfo col1;
            col1.name = "id";
            col1.type = WireType::INT32;
            col1.type_modifier = 0;
            columns.push_back(col1);

            ProtocolCodec::ColumnInfo col2;
            col2.name = "name";
            col2.type = WireType::VARCHAR;
            col2.type_modifier = 0;
            columns.push_back(col2);

            auto row_desc = ProtocolCodec::buildRowDescription(columns);
            session.sendMessage(row_desc);

            // Send a few rows
            for (int i = 1; i <= 3; ++i) {
                std::vector<ProtocolCodec::ColumnValue> values;
                values.push_back(ProtocolCodec::ColumnValue::fromInt32(i));
                values.push_back(ProtocolCodec::ColumnValue::fromString(
                    "TestRow" + std::to_string(i)));

                auto row_data = ProtocolCodec::buildRowData(values);
                session.sendMessage(row_data);
            }

            // Send command complete
            auto cmd_complete = ProtocolCodec::buildCommandComplete("SELECT", 3);
            session.sendMessage(cmd_complete);
        } else {
            // DML statement
            auto cmd_complete = ProtocolCodec::buildCommandComplete("UPDATE", 1);
            session.sendMessage(cmd_complete);
        }

        // End of results
        auto eor = ProtocolCodec::buildEndOfResults();
        session.sendMessage(eor);
    }

    void handleBeginTransaction(ProtocolSession& session, Message& /*request*/) {
        // Status: 1 = in transaction, XID = 1
        auto response = ProtocolCodec::buildTransactionStatus(1, 1);
        session.sendMessage(response);
    }

    void handleCommit(ProtocolSession& session, Message& /*request*/) {
        // Status: 0 = not in transaction, XID = 0
        auto response = ProtocolCodec::buildTransactionStatus(0, 0);
        session.sendMessage(response);
    }

    void handleRollback(ProtocolSession& session, Message& /*request*/) {
        // Status: 0 = not in transaction, XID = 0
        auto response = ProtocolCodec::buildTransactionStatus(0, 0);
        session.sendMessage(response);
    }

    void handlePing(ProtocolSession& session, Message& request) {
        uint64_t timestamp;
        uint32_t sequence;
        ProtocolCodec::parsePing(request, timestamp, sequence);

        auto response = ProtocolCodec::buildPong(timestamp, sequence);
        session.sendMessage(response);
    }

    std::string db_name_;
    std::atomic<bool> running_;
    std::unique_ptr<scratchbird::server::IPCServer> server_;
    std::thread server_thread_;
};

/**
 * Test fixture with mock server
 */
class ClientConnectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_server_ = std::make_unique<MockServer>("testdb");
        ASSERT_TRUE(mock_server_->start()) << "Failed to start mock server";

        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        mock_server_->stop();
    }

    scratchbird::client::ConnectionConfig createTestConfig() {
        scratchbird::client::ConnectionConfig config;
        config.database_name = "testdb";
        config.username = "testuser";
        config.password = "testpass";
        config.ipc_method = scratchbird::server::IPCMethod::TCP_LOCALHOST;
        config.tcp_port = mock_server_->getPort();
        config.auto_start_server = false;  // We use mock server
        config.connect_timeout_ms = 5000;
        return config;
    }

    std::unique_ptr<MockServer> mock_server_;
};

// ============================================================================
// Connection Lifecycle Tests
// ============================================================================

TEST_F(ClientConnectionTest, DefaultConstruction) {
    scratchbird::client::Connection conn;
    EXPECT_FALSE(conn.isConnected());
    EXPECT_EQ(conn.getState(), scratchbird::client::ConnectionState::DISCONNECTED);
}

TEST_F(ClientConnectionTest, ConnectAndDisconnect) {
    scratchbird::client::Connection conn;
    auto config = createTestConfig();

    ErrorContext ctx;
    auto status = conn.connect(config, &ctx);
    EXPECT_EQ(status, Status::OK) << "Connect failed: " << ctx.message;
    EXPECT_TRUE(conn.isConnected());
    EXPECT_EQ(conn.getState(), scratchbird::client::ConnectionState::CONNECTED);

    conn.disconnect();
    EXPECT_FALSE(conn.isConnected());
    EXPECT_EQ(conn.getState(), scratchbird::client::ConnectionState::DISCONNECTED);
}

TEST_F(ClientConnectionTest, SimpleConnect) {
    scratchbird::client::Connection conn;

    // Use simplified connect interface with our mock server config
    auto config = createTestConfig();
    auto status = conn.connect(config);
    EXPECT_EQ(status, Status::OK);
    EXPECT_TRUE(conn.isConnected());

    conn.disconnect();
}

TEST_F(ClientConnectionTest, MoveConstruction) {
    scratchbird::client::Connection conn1;
    auto status = conn1.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    scratchbird::client::Connection conn2(std::move(conn1));
    EXPECT_TRUE(conn2.isConnected());
    // conn1 should be in moved-from state
}

TEST_F(ClientConnectionTest, MoveAssignment) {
    scratchbird::client::Connection conn1;
    scratchbird::client::Connection conn2;

    auto status = conn1.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    conn2 = std::move(conn1);
    EXPECT_TRUE(conn2.isConnected());
}

TEST_F(ClientConnectionTest, ReconnectAfterDisconnect) {
    scratchbird::client::Connection conn;
    auto config = createTestConfig();

    // First connection
    auto status = conn.connect(config);
    EXPECT_EQ(status, Status::OK);
    conn.disconnect();
    EXPECT_FALSE(conn.isConnected());

    // Second connection
    status = conn.connect(config);
    EXPECT_EQ(status, Status::OK);
    EXPECT_TRUE(conn.isConnected());
}

TEST_F(ClientConnectionTest, Ping) {
    scratchbird::client::Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    status = conn.ping();
    EXPECT_EQ(status, Status::OK);
}

// ============================================================================
// Query Execution Tests
// ============================================================================

TEST_F(ClientConnectionTest, ExecuteSimpleQuery) {
    scratchbird::client::Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    scratchbird::client::ResultSet results;
    status = conn.executeQuery("SELECT id, name FROM test", &results);
    EXPECT_EQ(status, Status::OK);

    // Check column metadata
    EXPECT_EQ(results.getColumnCount(), 2u);
    EXPECT_EQ(results.getColumnName(0), "id");
    EXPECT_EQ(results.getColumnName(1), "name");
    EXPECT_EQ(results.getColumnType(0), WireType::INT32);
    EXPECT_EQ(results.getColumnType(1), WireType::VARCHAR);

    // Check row iteration
    int row_count = 0;
    while (results.next()) {
        ++row_count;
        EXPECT_FALSE(results.isNull(0));
        EXPECT_FALSE(results.isNull(1));
        EXPECT_EQ(results.getInt32(0), row_count);
        EXPECT_EQ(results.getString(1), "TestRow" + std::to_string(row_count));
    }
    EXPECT_EQ(row_count, 3);
    EXPECT_EQ(results.getRowCount(), 3);
}

TEST_F(ClientConnectionTest, ExecuteStatement) {
    scratchbird::client::Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    int64_t rows_affected = 0;
    status = conn.execute("UPDATE test SET name = 'changed' WHERE id = 1",
                          &rows_affected);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(rows_affected, 1);
}

TEST_F(ClientConnectionTest, ResultSetColumnAccess) {
    scratchbird::client::Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    scratchbird::client::ResultSet results;
    status = conn.executeQuery("SELECT id, name FROM test", &results);
    ASSERT_EQ(status, Status::OK);

    // Test column index lookup
    EXPECT_EQ(results.getColumnIndex("id"), 0);
    EXPECT_EQ(results.getColumnIndex("name"), 1);
    EXPECT_EQ(results.getColumnIndex("ID"), 0);  // Case insensitive
    EXPECT_EQ(results.getColumnIndex("nonexistent"), -1);

    // Test access by name
    ASSERT_TRUE(results.next());
    EXPECT_EQ(results.getInt32("id"), 1);
    EXPECT_EQ(results.getString("name"), "TestRow1");
}

TEST_F(ClientConnectionTest, ResultSetReset) {
    scratchbird::client::Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    scratchbird::client::ResultSet results;
    status = conn.executeQuery("SELECT id FROM test", &results);
    ASSERT_EQ(status, Status::OK);

    // Read all rows
    int count1 = 0;
    while (results.next()) ++count1;
    EXPECT_EQ(count1, 3);

    // Reset and read again
    results.reset();
    int count2 = 0;
    while (results.next()) ++count2;
    EXPECT_EQ(count2, 3);
}

TEST_F(ClientConnectionTest, QueryNotConnected) {
    scratchbird::client::Connection conn;
    scratchbird::client::ResultSet results;
    auto status = conn.executeQuery("SELECT 1", &results);
    EXPECT_NE(status, Status::OK);
}

// ============================================================================
// Transaction Tests
// ============================================================================

TEST_F(ClientConnectionTest, BeginCommit) {
    scratchbird::client::Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    EXPECT_FALSE(conn.inTransaction());

    status = conn.beginTransaction();
    EXPECT_EQ(status, Status::OK);
    EXPECT_TRUE(conn.inTransaction());
    EXPECT_EQ(conn.getState(), scratchbird::client::ConnectionState::IN_TRANSACTION);

    status = conn.commit();
    EXPECT_EQ(status, Status::OK);
    EXPECT_FALSE(conn.inTransaction());
    EXPECT_EQ(conn.getState(), scratchbird::client::ConnectionState::CONNECTED);
}

TEST_F(ClientConnectionTest, BeginRollback) {
    scratchbird::client::Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    status = conn.beginTransaction();
    EXPECT_EQ(status, Status::OK);
    EXPECT_TRUE(conn.inTransaction());

    status = conn.rollback();
    EXPECT_EQ(status, Status::OK);
    EXPECT_FALSE(conn.inTransaction());
}

TEST_F(ClientConnectionTest, AutoCommitMode) {
    scratchbird::client::Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    // Default is auto-commit on
    EXPECT_TRUE(conn.getAutoCommit());

    conn.setAutoCommit(false);
    EXPECT_FALSE(conn.getAutoCommit());

    conn.setAutoCommit(true);
    EXPECT_TRUE(conn.getAutoCommit());
}

TEST_F(ClientConnectionTest, RollbackWithoutTransaction) {
    scratchbird::client::Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    // Should succeed silently
    status = conn.rollback();
    EXPECT_EQ(status, Status::OK);
}

TEST_F(ClientConnectionTest, DoubleBegin) {
    scratchbird::client::Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    status = conn.beginTransaction();
    EXPECT_EQ(status, Status::OK);

    status = conn.beginTransaction();
    EXPECT_NE(status, Status::OK);  // Should fail
}

TEST_F(ClientConnectionTest, DisconnectRollsBackTransaction) {
    scratchbird::client::Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    status = conn.beginTransaction();
    ASSERT_EQ(status, Status::OK);
    EXPECT_TRUE(conn.inTransaction());

    // Disconnect should rollback
    conn.disconnect();
    EXPECT_FALSE(conn.inTransaction());
}

// ============================================================================
// PreparedStatement Tests
// ============================================================================

TEST_F(ClientConnectionTest, PrepareStatement) {
    scratchbird::client::Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    scratchbird::client::PreparedStatement stmt;
    status = conn.prepare("SELECT * FROM test WHERE id = $1", &stmt);
    EXPECT_EQ(status, Status::OK);
    EXPECT_TRUE(stmt.isValid());
    EXPECT_EQ(stmt.getParameterCount(), 1u);
    EXPECT_EQ(stmt.getSQL(), "SELECT * FROM test WHERE id = $1");
}

TEST_F(ClientConnectionTest, PrepareStatementWithQuestionMarks) {
    scratchbird::client::Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    scratchbird::client::PreparedStatement stmt;
    status = conn.prepare("SELECT * FROM test WHERE id = ? AND name = ?", &stmt);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(stmt.getParameterCount(), 2u);
}

TEST_F(ClientConnectionTest, PreparedStatementParameterBinding) {
    scratchbird::client::PreparedStatement stmt;

    scratchbird::client::Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    status = conn.prepare("SELECT * FROM test WHERE id = $1 AND name = $2", &stmt);
    ASSERT_EQ(status, Status::OK);

    // Bind parameters
    stmt.setInt32(1, 42);
    stmt.setString(2, "TestName");

    // Clear and rebind
    stmt.clearParameters();
    stmt.setNull(1);
    stmt.setBool(2, true);
}

TEST_F(ClientConnectionTest, CloseStatement) {
    scratchbird::client::Connection conn;
    auto status = conn.connect(createTestConfig());
    ASSERT_EQ(status, Status::OK);

    scratchbird::client::PreparedStatement stmt;
    status = conn.prepare("SELECT 1", &stmt);
    ASSERT_EQ(status, Status::OK);
    EXPECT_TRUE(stmt.isValid());

    conn.closeStatement(stmt);
    EXPECT_FALSE(stmt.isValid());
}

// ============================================================================
// ResultSet Type Accessor Tests
// ============================================================================

class ResultSetTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResultSetTest, DefaultConstruction) {
    scratchbird::client::ResultSet rs;
    EXPECT_EQ(rs.getColumnCount(), 0u);
    EXPECT_TRUE(rs.isEmpty());
    EXPECT_EQ(rs.getCurrentRow(), -1);
}

TEST_F(ResultSetTest, MoveConstruction) {
    scratchbird::client::ResultSet rs1;
    scratchbird::client::ResultSet rs2(std::move(rs1));
    EXPECT_EQ(rs2.getColumnCount(), 0u);
}

TEST_F(ResultSetTest, MoveAssignment) {
    scratchbird::client::ResultSet rs1;
    scratchbird::client::ResultSet rs2;
    rs2 = std::move(rs1);
    EXPECT_EQ(rs2.getColumnCount(), 0u);
}

// ============================================================================
// Connection Pool Tests
// ============================================================================

class ConnectionPoolTest : public ClientConnectionTest {
protected:
    scratchbird::client::PoolConfig createPoolConfig() {
        scratchbird::client::PoolConfig config;
        config.connection_config = createTestConfig();
        config.min_connections = 1;
        config.max_connections = 5;
        config.idle_timeout_ms = 60000;
        config.acquire_timeout_ms = 5000;
        config.validate_on_acquire = false;  // Skip validation for faster tests
        return config;
    }
};

TEST_F(ConnectionPoolTest, CreatePool) {
    scratchbird::client::ConnectionPool pool(createPoolConfig());

    auto stats = pool.getStats();
    EXPECT_GE(stats.total_connections, 1u);
}

TEST_F(ConnectionPoolTest, AcquireAndRelease) {
    scratchbird::client::ConnectionPool pool(createPoolConfig());

    {
        auto conn = pool.acquire();
        ASSERT_TRUE(conn);
        EXPECT_TRUE(conn->isConnected());

        auto stats = pool.getStats();
        EXPECT_EQ(stats.in_use_connections, 1u);
    }

    // Connection returned to pool
    auto stats = pool.getStats();
    EXPECT_EQ(stats.in_use_connections, 0u);
    EXPECT_GE(stats.available_connections, 1u);
}

// Note: This test requires the mock server to handle concurrent connections.
// The simple mock server handles connections sequentially, so we limit to 1.
// Full concurrent connection testing requires a real server or threaded mock.
TEST_F(ConnectionPoolTest, MultipleAcquire) {
    auto config = createPoolConfig();
    config.max_connections = 1;  // Mock server is single-threaded
    config.min_connections = 0;
    scratchbird::client::ConnectionPool pool(config);

    // Acquire single connection since mock server is single-threaded
    auto conn = pool.acquire();
    ASSERT_TRUE(conn) << "Failed to acquire connection";
    EXPECT_TRUE(conn->isConnected());

    auto stats = pool.getStats();
    EXPECT_EQ(stats.in_use_connections, 1u);
}

TEST_F(ConnectionPoolTest, PoolShutdown) {
    scratchbird::client::ConnectionPool pool(createPoolConfig());

    auto conn = pool.acquire();
    ASSERT_TRUE(conn);

    pool.shutdown();

    auto stats = pool.getStats();
    EXPECT_EQ(stats.available_connections, 0u);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

class ErrorHandlingTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ErrorHandlingTest, ConnectToNonexistentServer) {
    scratchbird::client::Connection conn;
    scratchbird::client::ConnectionConfig config;
    config.database_name = "nonexistent";
    config.ipc_method = scratchbird::server::IPCMethod::TCP_LOCALHOST;
    config.tcp_port = 59999;  // Unlikely to have a server here
    config.auto_start_server = false;
    config.connect_timeout_ms = 1000;

    ErrorContext ctx;
    auto status = conn.connect(config, &ctx);
    EXPECT_NE(status, Status::OK);
    EXPECT_FALSE(conn.isConnected());
}

TEST_F(ErrorHandlingTest, GetLastError) {
    scratchbird::client::Connection conn;
    auto status = conn.executeQuery("SELECT 1", nullptr);
    EXPECT_NE(status, Status::OK);
    EXPECT_FALSE(conn.getLastError().empty());
}

TEST_F(ErrorHandlingTest, PingNotConnected) {
    scratchbird::client::Connection conn;
    auto status = conn.ping();
    EXPECT_NE(status, Status::OK);
}

TEST_F(ErrorHandlingTest, TransactionNotConnected) {
    scratchbird::client::Connection conn;

    auto status = conn.beginTransaction();
    EXPECT_NE(status, Status::OK);

    status = conn.commit();
    EXPECT_NE(status, Status::OK);
}

// ============================================================================
// Connection Information Tests
// ============================================================================

TEST_F(ClientConnectionTest, GetConnectionInfo) {
    scratchbird::client::Connection conn;
    auto config = createTestConfig();
    auto status = conn.connect(config);
    ASSERT_EQ(status, Status::OK);

    EXPECT_EQ(conn.getDatabaseName(), "testdb");
    EXPECT_EQ(conn.getUsername(), "testuser");

    const auto& retrieved_config = conn.getConfig();
    EXPECT_EQ(retrieved_config.database_name, "testdb");
}

// ============================================================================
// Static Server Control Tests
// ============================================================================

TEST(ServerControlTest, IsServerRunningNonexistent) {
    // Check for a non-existent database
    bool running = scratchbird::client::Connection::isServerRunning("nonexistent_db_12345");
    EXPECT_FALSE(running);
}

TEST(ServerControlTest, GetServerPIDNonexistent) {
    int32_t pid = scratchbird::client::Connection::getServerPID("nonexistent_db_12345");
    EXPECT_EQ(pid, 0);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
