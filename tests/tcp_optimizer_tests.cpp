#include "scratchbird/engine/tcp_optimizer.h"

#include <chrono>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

using namespace ScratchBird;
using namespace std::chrono_literals;

class TCPOptimizerTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create a test socket for each test
        test_sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(test_sockfd_, 0) << "Failed to create test socket";
    }

    void TearDown() override
    {
        if (test_sockfd_ >= 0) {
            close(test_sockfd_);
        }
    }

    int test_sockfd_ = -1;
    TCPOptimizer optimizer_;
};

// Test TCP_NODELAY configuration
TEST_F(TCPOptimizerTest, ConfigureTCPNoDelay)
{
    NetworkConfig config;
    config.tcp_nodelay = true;

    auto result = optimizer_.configure_socket(test_sockfd_, config);
    EXPECT_FALSE(result) << "Failed to configure TCP_NODELAY: " << result.message();

    // Verify TCP_NODELAY is set
    int nodelay = 0;
    socklen_t len = sizeof(nodelay);
    ASSERT_EQ(0, getsockopt(test_sockfd_, IPPROTO_TCP, TCP_NODELAY, &nodelay, &len));
    EXPECT_EQ(1, nodelay) << "TCP_NODELAY should be enabled";
}

// Test TCP_NODELAY disable
TEST_F(TCPOptimizerTest, DisableTCPNoDelay)
{
    NetworkConfig config;
    config.tcp_nodelay = false;

    auto result = optimizer_.configure_socket(test_sockfd_, config);
    EXPECT_FALSE(result) << "Failed to configure socket: " << result.message();

    // Verify TCP_NODELAY is disabled
    int nodelay = 1;
    socklen_t len = sizeof(nodelay);
    ASSERT_EQ(0, getsockopt(test_sockfd_, IPPROTO_TCP, TCP_NODELAY, &nodelay, &len));
    EXPECT_EQ(0, nodelay) << "TCP_NODELAY should be disabled";
}

// Test keepalive configuration
TEST_F(TCPOptimizerTest, ConfigureKeepalive)
{
    NetworkConfig config;
    config.tcp_keepalive_idle = 300s;
    config.tcp_keepalive_interval = 15s;
    config.tcp_keepalive_count = 5;

    auto result = optimizer_.configure_socket(test_sockfd_, config);
    EXPECT_FALSE(result) << "Failed to configure keepalive: " << result.message();

    // Verify SO_KEEPALIVE is enabled
    int keepalive = 0;
    socklen_t len = sizeof(keepalive);
    ASSERT_EQ(0, getsockopt(test_sockfd_, SOL_SOCKET, SO_KEEPALIVE, &keepalive, &len));
    EXPECT_EQ(1, keepalive) << "SO_KEEPALIVE should be enabled";

#ifdef TCP_KEEPIDLE
    // Verify keepalive idle time
    int idle = 0;
    len = sizeof(idle);
    ASSERT_EQ(0, getsockopt(test_sockfd_, IPPROTO_TCP, TCP_KEEPIDLE, &idle, &len));
    EXPECT_EQ(300, idle) << "TCP_KEEPIDLE should be 300 seconds";
#endif

#ifdef TCP_KEEPINTVL
    // Verify keepalive interval
    int interval = 0;
    len = sizeof(interval);
    ASSERT_EQ(0, getsockopt(test_sockfd_, IPPROTO_TCP, TCP_KEEPINTVL, &interval, &len));
    EXPECT_EQ(15, interval) << "TCP_KEEPINTVL should be 15 seconds";
#endif

#ifdef TCP_KEEPCNT
    // Verify keepalive count
    int count = 0;
    len = sizeof(count);
    ASSERT_EQ(0, getsockopt(test_sockfd_, IPPROTO_TCP, TCP_KEEPCNT, &count, &len));
    EXPECT_EQ(5, count) << "TCP_KEEPCNT should be 5";
#endif
}

// Test buffer size configuration
TEST_F(TCPOptimizerTest, ConfigureBufferSizes)
{
    NetworkConfig config;
    config.socket_recv_buffer = 128 * 1024; // 128KB
    config.socket_send_buffer = 256 * 1024; // 256KB

    auto result = optimizer_.configure_socket(test_sockfd_, config);
    EXPECT_FALSE(result) << "Failed to configure buffer sizes: " << result.message();

    // Verify receive buffer size
    int rcvbuf = 0;
    socklen_t len = sizeof(rcvbuf);
    ASSERT_EQ(0, getsockopt(test_sockfd_, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &len));
    EXPECT_GE(rcvbuf, 128 * 1024) << "Receive buffer should be at least 128KB";

    // Verify send buffer size
    int sndbuf = 0;
    len = sizeof(sndbuf);
    ASSERT_EQ(0, getsockopt(test_sockfd_, SOL_SOCKET, SO_SNDBUF, &sndbuf, &len));
    EXPECT_GE(sndbuf, 256 * 1024) << "Send buffer should be at least 256KB";
}

// Test listening socket configuration
TEST_F(TCPOptimizerTest, ConfigureListenSocket)
{
    NetworkConfig config;
    config.tcp_nodelay = true;
    config.reuse_port = true;

    auto result = optimizer_.configure_listen_socket(test_sockfd_, config);
    EXPECT_FALSE(result) << "Failed to configure listen socket: " << result.message();

    // Verify SO_REUSEADDR is set
    int reuse = 0;
    socklen_t len = sizeof(reuse);
    ASSERT_EQ(0, getsockopt(test_sockfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, &len));
    EXPECT_EQ(1, reuse) << "SO_REUSEADDR should be enabled";

#ifdef SO_REUSEPORT
    // Verify SO_REUSEPORT is set if available
    int reuse_port = 0;
    len = sizeof(reuse_port);
    if (getsockopt(test_sockfd_, SOL_SOCKET, SO_REUSEPORT, &reuse_port, &len) == 0) {
        EXPECT_EQ(1, reuse_port) << "SO_REUSEPORT should be enabled";
    }
#endif
}

// Test client socket configuration
TEST_F(TCPOptimizerTest, ConfigureClientSocket)
{
    NetworkConfig config;
    config.tcp_nodelay = true;
    config.socket_recv_buffer = 64 * 1024;
    config.socket_send_buffer = 64 * 1024;

    auto result = optimizer_.configure_client_socket(test_sockfd_, config);
    EXPECT_FALSE(result) << "Failed to configure client socket: " << result.message();

    // Verify configuration was applied
    auto retrieved_config = optimizer_.get_socket_config(test_sockfd_);
    EXPECT_TRUE(retrieved_config.tcp_nodelay);
    EXPECT_GE(retrieved_config.socket_recv_buffer, 64 * 1024);
    EXPECT_GE(retrieved_config.socket_send_buffer, 64 * 1024);
}

// Test invalid socket handling
TEST_F(TCPOptimizerTest, InvalidSocketHandling)
{
    NetworkConfig config;

    auto result = optimizer_.configure_socket(-1, config);
    EXPECT_TRUE(result) << "Should fail for invalid socket";
    EXPECT_EQ(std::errc::invalid_argument, result) << "Should return invalid_argument error";
}

// Test configuration validation
TEST_F(TCPOptimizerTest, ConfigurationValidation)
{
    // Valid configuration
    NetworkConfig valid_config;
    std::string errors = TCPOptimizer::validate_config(valid_config);
    EXPECT_TRUE(errors.empty()) << "Valid config should have no errors: " << errors;

    // Invalid idle time
    NetworkConfig invalid_config;
    invalid_config.tcp_keepalive_idle = std::chrono::seconds(-1);
    errors = TCPOptimizer::validate_config(invalid_config);
    EXPECT_FALSE(errors.empty()) << "Should have validation errors";
    EXPECT_NE(std::string::npos, errors.find("tcp_keepalive_idle"))
        << "Should mention idle time error";

    // Invalid interval
    invalid_config = NetworkConfig{};
    invalid_config.tcp_keepalive_interval = std::chrono::seconds(0);
    errors = TCPOptimizer::validate_config(invalid_config);
    EXPECT_FALSE(errors.empty()) << "Should have validation errors";

    // Invalid count
    invalid_config = NetworkConfig{};
    invalid_config.tcp_keepalive_count = 0;
    errors = TCPOptimizer::validate_config(invalid_config);
    EXPECT_FALSE(errors.empty()) << "Should have validation errors";

    // Invalid buffer sizes
    invalid_config = NetworkConfig{};
    invalid_config.socket_recv_buffer = 1024; // Too small
    invalid_config.socket_send_buffer = 1024; // Too small
    errors = TCPOptimizer::validate_config(invalid_config);
    EXPECT_FALSE(errors.empty()) << "Should have validation errors";

    // Invalid backlog
    invalid_config = NetworkConfig{};
    invalid_config.listen_backlog = 0;
    errors = TCPOptimizer::validate_config(invalid_config);
    EXPECT_FALSE(errors.empty()) << "Should have validation errors";
}

// Test platform capabilities query
TEST_F(TCPOptimizerTest, PlatformCapabilities)
{
    std::string caps = TCPOptimizer::get_platform_capabilities();
    EXPECT_FALSE(caps.empty()) << "Should return platform capabilities";

    // Should mention key TCP options
    EXPECT_NE(std::string::npos, caps.find("TCP_NODELAY")) << "Should mention TCP_NODELAY support";
    EXPECT_NE(std::string::npos, caps.find("SO_KEEPALIVE")) << "Should mention keepalive support";
}

// Test OptimizedSocket RAII wrapper
TEST_F(TCPOptimizerTest, OptimizedSocketRAII)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(sockfd, 0) << "Failed to create socket";

    {
        NetworkConfig config;
        config.tcp_nodelay = true;

        OptimizedSocket opt_socket(sockfd, config);
        EXPECT_EQ(sockfd, opt_socket.get()) << "Should return correct socket fd";
        EXPECT_TRUE(opt_socket.valid()) << "Should be valid";

        // Verify configuration was applied
        int nodelay = 0;
        socklen_t len = sizeof(nodelay);
        ASSERT_EQ(0, getsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, &len));
        EXPECT_EQ(1, nodelay) << "TCP_NODELAY should be enabled";

        sockfd = -1; // OptimizedSocket owns it now
    }
    // Socket should be automatically closed by destructor
}

// Test OptimizedSocket move semantics
TEST_F(TCPOptimizerTest, OptimizedSocketMove)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(sockfd, 0) << "Failed to create socket";

    NetworkConfig config;
    OptimizedSocket socket1(sockfd, config);
    EXPECT_TRUE(socket1.valid());

    // Move constructor
    OptimizedSocket socket2 = std::move(socket1);
    EXPECT_FALSE(socket1.valid()) << "Source should be invalid after move";
    EXPECT_TRUE(socket2.valid()) << "Destination should be valid after move";
    EXPECT_EQ(sockfd, socket2.get()) << "Should have correct socket fd";

    // Move assignment
    OptimizedSocket socket3(-1);
    socket3 = std::move(socket2);
    EXPECT_FALSE(socket2.valid()) << "Source should be invalid after move";
    EXPECT_TRUE(socket3.valid()) << "Destination should be valid after move";
    EXPECT_EQ(sockfd, socket3.get()) << "Should have correct socket fd";
}

// Test OptimizedSocket reconfiguration
TEST_F(TCPOptimizerTest, OptimizedSocketReconfigure)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(sockfd, 0) << "Failed to create socket";

    NetworkConfig config;
    config.tcp_nodelay = false;

    OptimizedSocket opt_socket(sockfd, config);

    // Verify initial configuration
    int nodelay = 1;
    socklen_t len = sizeof(nodelay);
    ASSERT_EQ(0, getsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, &len));
    EXPECT_EQ(0, nodelay) << "TCP_NODELAY should be initially disabled";

    // Reconfigure
    NetworkConfig new_config;
    new_config.tcp_nodelay = true;
    auto result = opt_socket.reconfigure(new_config);
    EXPECT_FALSE(result) << "Reconfiguration should succeed";

    // Verify new configuration
    nodelay = 0;
    ASSERT_EQ(0, getsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, &len));
    EXPECT_EQ(1, nodelay) << "TCP_NODELAY should be enabled after reconfigure";
}

// Test OptimizedSocket release
TEST_F(TCPOptimizerTest, OptimizedSocketRelease)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(sockfd, 0) << "Failed to create socket";

    NetworkConfig config;
    OptimizedSocket opt_socket(sockfd, config);
    EXPECT_TRUE(opt_socket.valid());

    int released_fd = opt_socket.release();
    EXPECT_EQ(sockfd, released_fd) << "Should return original socket fd";
    EXPECT_FALSE(opt_socket.valid()) << "Should be invalid after release";

    // Manually close the socket since it's no longer owned by OptimizedSocket
    close(released_fd);
}

// Test NetworkStatsCollector
TEST_F(TCPOptimizerTest, NetworkStatsCollector)
{
    NetworkStatsCollector collector;

    // Initial stats should be zero
    auto stats = collector.get_stats();
    EXPECT_EQ(0, stats.connections_accepted);
    EXPECT_EQ(0, stats.connections_failed);
    EXPECT_EQ(0, stats.bytes_sent);
    EXPECT_EQ(0, stats.bytes_received);
    EXPECT_EQ(1.0, stats.connection_success_rate())
        << "Success rate should be 1.0 with no connections";

    // Record some events
    collector.record_connection_accepted(100ms);
    collector.record_connection_accepted(200ms);
    collector.record_connection_failed();
    collector.record_bytes_sent(1024, 50ms);
    collector.record_bytes_received(2048, 75ms);
    collector.record_keepalive_timeout();
    collector.record_socket_error();

    // Check updated stats
    stats = collector.get_stats();
    EXPECT_EQ(2, stats.connections_accepted);
    EXPECT_EQ(1, stats.connections_failed);
    EXPECT_EQ(1024, stats.bytes_sent);
    EXPECT_EQ(2048, stats.bytes_received);
    EXPECT_EQ(1, stats.keepalive_timeouts);
    EXPECT_EQ(1, stats.socket_errors);

    // Check success rate calculation
    EXPECT_DOUBLE_EQ(2.0 / 3.0, stats.connection_success_rate());

    // Check that averages are reasonable
    EXPECT_GT(stats.avg_connection_time.count(), 0);
    EXPECT_GT(stats.avg_send_latency.count(), 0);
    EXPECT_GT(stats.avg_recv_latency.count(), 0);

    // Reset stats
    collector.reset_stats();
    stats = collector.get_stats();
    EXPECT_EQ(0, stats.connections_accepted);
    EXPECT_EQ(0, stats.connections_failed);
    EXPECT_EQ(0, stats.bytes_sent);
    EXPECT_EQ(0, stats.bytes_received);
}

// Cross-platform compatibility test
TEST_F(TCPOptimizerTest, CrossPlatformCompatibility)
{
    NetworkConfig config;
    config.tcp_nodelay = true;
    config.tcp_keepalive_idle = 600s;
    config.tcp_keepalive_interval = 30s;
    config.tcp_keepalive_count = 3;
    config.socket_recv_buffer = 256 * 1024;
    config.socket_send_buffer = 256 * 1024;

    // Should work on all platforms without errors
    auto result = optimizer_.configure_socket(test_sockfd_, config);
    EXPECT_FALSE(result) << "Cross-platform configuration should work: " << result.message();

    // Should be able to retrieve configuration
    auto retrieved = optimizer_.get_socket_config(test_sockfd_);
    EXPECT_TRUE(retrieved.tcp_nodelay) << "TCP_NODELAY should be enabled";
    EXPECT_GE(retrieved.socket_recv_buffer, 256 * 1024) << "Receive buffer should be set";
    EXPECT_GE(retrieved.socket_send_buffer, 256 * 1024) << "Send buffer should be set";
}

// Performance benchmark test (basic)
TEST_F(TCPOptimizerTest, PerformanceBenchmark)
{
    NetworkConfig config;

    // Time configuration application
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        auto result = optimizer_.configure_socket(test_sockfd_, config);
        ASSERT_FALSE(result) << "Configuration should succeed in benchmark";
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should be reasonably fast (less than 1ms per configuration on average)
    EXPECT_LT(duration.count(), 1000 * 1000) << "Configuration should be fast";

    std::cout << "TCP optimization configuration: " << duration.count() / 1000.0
              << " microseconds per call (average)" << std::endl;
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
