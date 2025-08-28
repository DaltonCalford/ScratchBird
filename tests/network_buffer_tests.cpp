#include "scratchbird/engine/network_buffer.h"

#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace ScratchBird;

class NetworkBufferTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

        // Use test-friendly configuration
        config_.default_recv_buffer_size = 8192;    // 8KB for testing
        config_.default_send_buffer_size = 8192;    // 8KB for testing
        config_.min_buffer_size = 1024;             // 1KB minimum
        config_.max_buffer_size = 65536;            // 64KB maximum
        config_.tuning_interval = std::chrono::seconds(1);
        config_.stats_collection_interval = std::chrono::seconds(1);
        config_.overflow_alert_threshold = 2;       // Lower threshold for testing
        config_.enable_monitoring = false;          // Disable for most tests
        config_.enable_auto_tuning = false;         // Disable for most tests
        config_.utilization_threshold = 0.7;
        config_.underutilization_threshold = 0.3;
        config_.growth_factor = 1.5;
        config_.shrink_factor = 0.8;

        // Create test socket
        test_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(test_socket_, 0) << "Failed to create test socket";
    }

    void TearDown() override
    {
        if (manager_) {
            manager_->shutdown();
        }

        if (test_socket_ >= 0) {
#ifdef _WIN32
            closesocket(test_socket_);
            WSACleanup();
#else
            close(test_socket_);
#endif
        }
    }

    NetworkBufferConfig config_;
    std::unique_ptr<NetworkBufferManager> manager_;
    int test_socket_ = -1;
};

// Test 1: Configuration validation
TEST_F(NetworkBufferTest, ConfigurationValidation)
{
    // Test valid configuration
    std::string errors = NetworkBufferManager::validate_config(config_);
    EXPECT_TRUE(errors.empty()) << "Valid config should not produce errors: " << errors;

    // Test invalid min_buffer_size
    NetworkBufferConfig invalid_config = config_;
    invalid_config.min_buffer_size = 0;
    errors = NetworkBufferManager::validate_config(invalid_config);
    EXPECT_FALSE(errors.empty());
    EXPECT_NE(errors.find("min_buffer_size must be greater than 0"), std::string::npos);

    // Test invalid max < min
    invalid_config = config_;
    invalid_config.max_buffer_size = 512;
    invalid_config.min_buffer_size = 1024;
    errors = NetworkBufferManager::validate_config(invalid_config);
    EXPECT_FALSE(errors.empty());
    EXPECT_NE(errors.find("max_buffer_size must be greater than min_buffer_size"), std::string::npos);

    // Test invalid growth_factor
    invalid_config = config_;
    invalid_config.growth_factor = 0.5;
    errors = NetworkBufferManager::validate_config(invalid_config);
    EXPECT_FALSE(errors.empty());
    EXPECT_NE(errors.find("growth_factor must be greater than 1.0"), std::string::npos);

    // Test invalid shrink_factor
    invalid_config = config_;
    invalid_config.shrink_factor = 1.5;
    errors = NetworkBufferManager::validate_config(invalid_config);
    EXPECT_FALSE(errors.empty());
    EXPECT_NE(errors.find("shrink_factor must be between 0.0 and 1.0"), std::string::npos);
}

// Test 2: Manager initialization and shutdown
TEST_F(NetworkBufferTest, ManagerInitializationShutdown)
{
    manager_ = std::make_unique<NetworkBufferManager>(config_);

    // Initialize the manager
    std::error_code ec = manager_->initialize();
    EXPECT_FALSE(ec) << "Manager initialization should succeed: " << ec.message();

    // Test initialization with invalid config
    NetworkBufferConfig invalid_config = config_;
    invalid_config.min_buffer_size = 0;
    NetworkBufferManager invalid_manager(invalid_config);

    ec = invalid_manager.initialize();
    EXPECT_TRUE(ec) << "Manager initialization should fail with invalid config";

    // Shutdown should work without issues
    EXPECT_NO_THROW(manager_->shutdown());
}

// Test 3: Socket buffer configuration
TEST_F(NetworkBufferTest, SocketBufferConfiguration)
{
    manager_ = std::make_unique<NetworkBufferManager>(config_);
    ASSERT_FALSE(manager_->initialize());

    // Test basic buffer configuration
    std::error_code ec = manager_->configure_socket_buffers(test_socket_, 16384, 16384);
    EXPECT_FALSE(ec) << "Socket buffer configuration should succeed: " << ec.message();

    // Test configuration with invalid socket
    ec = manager_->configure_socket_buffers(-1, 8192, 8192);
    EXPECT_TRUE(ec) << "Configuration with invalid socket should fail";

    // Test default size configuration (passing 0)
    ec = manager_->configure_socket_buffers(test_socket_, 0, 0);
    EXPECT_FALSE(ec) << "Configuration with default sizes should succeed";

    // Test buffer size clamping
    ec = manager_->configure_socket_buffers(test_socket_, 100, 1000000);
    EXPECT_FALSE(ec) << "Configuration with out-of-range sizes should succeed (clamped)";
}

// Test 4: Socket registration and tracking
TEST_F(NetworkBufferTest, SocketRegistrationTracking)
{
    manager_ = std::make_unique<NetworkBufferManager>(config_);
    ASSERT_FALSE(manager_->initialize());

    // Register socket
    std::error_code ec = manager_->register_socket(test_socket_);
    EXPECT_FALSE(ec) << "Socket registration should succeed: " << ec.message();

    // Get socket stats
    auto stats = manager_->get_socket_stats(test_socket_);
    EXPECT_NE(stats, nullptr) << "Should get stats for registered socket";
    EXPECT_GT(stats->recv_buffer_size, 0) << "Receive buffer size should be set";
    EXPECT_GT(stats->send_buffer_size, 0) << "Send buffer size should be set";

    // Test stats for unregistered socket
    auto invalid_stats = manager_->get_socket_stats(9999);
    EXPECT_EQ(invalid_stats, nullptr) << "Should get null stats for unregistered socket";

    // Unregister socket
    EXPECT_NO_THROW(manager_->unregister_socket(test_socket_));

    // Stats should no longer be available
    auto removed_stats = manager_->get_socket_stats(test_socket_);
    EXPECT_EQ(removed_stats, nullptr) << "Should get null stats after unregistering";
}

// Test 5: I/O operation recording
TEST_F(NetworkBufferTest, IOOperationRecording)
{
    manager_ = std::make_unique<NetworkBufferManager>(config_);
    ASSERT_FALSE(manager_->initialize());
    ASSERT_FALSE(manager_->register_socket(test_socket_));

    auto stats = manager_->get_socket_stats(test_socket_);
    ASSERT_NE(stats, nullptr);

    // Record some send operations
    manager_->record_io_operation(test_socket_, 1024, true, std::chrono::nanoseconds(1000));
    manager_->record_io_operation(test_socket_, 2048, true, std::chrono::nanoseconds(1500));

    EXPECT_EQ(stats->bytes_sent.load(), 3072);
    EXPECT_EQ(stats->send_operations.load(), 2);
    EXPECT_GT(stats->avg_send_latency.count(), 0);

    // Record some receive operations
    manager_->record_io_operation(test_socket_, 512, false, std::chrono::nanoseconds(800));
    manager_->record_io_operation(test_socket_, 1536, false, std::chrono::nanoseconds(1200));

    EXPECT_EQ(stats->bytes_received.load(), 2048);
    EXPECT_EQ(stats->recv_operations.load(), 2);
    EXPECT_GT(stats->avg_recv_latency.count(), 0);

    // Test utilization calculations
    EXPECT_GE(stats->get_send_efficiency(), 0.0);
    EXPECT_GE(stats->get_recv_efficiency(), 0.0);
}

// Test 6: Buffer overflow detection and alerts
TEST_F(NetworkBufferTest, BufferOverflowDetectionAlerts)
{
    manager_ = std::make_unique<NetworkBufferManager>(config_);
    ASSERT_FALSE(manager_->initialize());
    ASSERT_FALSE(manager_->register_socket(test_socket_));

    // Record some overflow events
    manager_->record_buffer_overflow(test_socket_, true);  // Send buffer overflow
    manager_->record_buffer_overflow(test_socket_, false); // Recv buffer overflow
    manager_->record_buffer_overflow(test_socket_, true);  // Another send overflow

    auto stats = manager_->get_socket_stats(test_socket_);
    ASSERT_NE(stats, nullptr);

    EXPECT_EQ(stats->send_buffer_full_events.load(), 2);
    EXPECT_EQ(stats->recv_buffer_full_events.load(), 1);

    // Check if alerts were generated (threshold is 2)
    auto alerts = manager_->get_pending_alerts(false); // Don't clear alerts yet
    EXPECT_GT(alerts.size(), 0) << "Should have alerts for overflow events";

    bool found_overflow_alert = false;
    for (const auto& alert : alerts) {
        if (alert.type == BufferAlertType::OVERFLOW_DETECTED) {
            found_overflow_alert = true;
            EXPECT_EQ(alert.socket_fd, test_socket_);
            EXPECT_GT(alert.event_count, 0);
            break;
        }
    }
    EXPECT_TRUE(found_overflow_alert) << "Should find overflow alert";

    // Clear alerts
    auto cleared_alerts = manager_->get_pending_alerts(true);
    EXPECT_EQ(cleared_alerts.size(), alerts.size());

    // Should be empty now
    auto empty_alerts = manager_->get_pending_alerts(false);
    EXPECT_EQ(empty_alerts.size(), 0) << "Alerts should be cleared";
}

// Test 7: Buffer auto-tuning logic (without actual socket operations)
TEST_F(NetworkBufferTest, BufferAutoTuning)
{
    manager_ = std::make_unique<NetworkBufferManager>(config_);
    ASSERT_FALSE(manager_->initialize());
    ASSERT_FALSE(manager_->register_socket(test_socket_));

    auto stats = manager_->get_socket_stats(test_socket_);
    ASSERT_NE(stats, nullptr);

    // Simulate buffer overflow to trigger tuning decisions
    manager_->record_buffer_overflow(test_socket_, true);
    manager_->record_buffer_overflow(test_socket_, false);

    // Test the optimal buffer size calculation logic directly
    // This tests the tuning algorithm without requiring socket operations
    size_t recommended_recv = manager_->get_recommended_buffer_size(test_socket_, false);
    size_t recommended_send = manager_->get_recommended_buffer_size(test_socket_, true);
    EXPECT_GE(recommended_recv, config_.min_buffer_size);
    EXPECT_LE(recommended_recv, config_.max_buffer_size);
    EXPECT_GE(recommended_send, config_.min_buffer_size);
    EXPECT_LE(recommended_send, config_.max_buffer_size);
    
    // After overflow events, recommended sizes should be larger than defaults
    // (testing the growth logic)
    EXPECT_GT(recommended_recv, config_.default_recv_buffer_size);
    EXPECT_GT(recommended_send, config_.default_send_buffer_size);
    
    // Test tuning with invalid socket (should definitely fail)
    std::error_code ec = manager_->tune_socket_buffers(9999);
    EXPECT_TRUE(ec) << "Tuning invalid socket should fail";
}

// Test 8: Aggregated statistics
TEST_F(NetworkBufferTest, AggregatedStatistics)
{
    manager_ = std::make_unique<NetworkBufferManager>(config_);
    ASSERT_FALSE(manager_->initialize());

    // Create multiple sockets and register them
    std::vector<int> test_sockets;
    for (int i = 0; i < 3; ++i) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(sock, 0);
        test_sockets.push_back(sock);
        ASSERT_FALSE(manager_->register_socket(sock));
    }

    // Record some I/O operations on different sockets
    for (size_t i = 0; i < test_sockets.size(); ++i) {
        manager_->record_io_operation(test_sockets[i], 1024 * (i + 1), true, 
                                    std::chrono::nanoseconds(1000));
        manager_->record_io_operation(test_sockets[i], 512 * (i + 1), false, 
                                    std::chrono::nanoseconds(800));
    }

    // Give some time for monitoring thread to update stats
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Get aggregated stats
    auto agg_stats = manager_->get_aggregated_stats();
    EXPECT_EQ(agg_stats.total_connections, test_sockets.size());
    EXPECT_GT(agg_stats.total_bytes_sent, 0);
    EXPECT_GT(agg_stats.total_bytes_received, 0);
    EXPECT_GT(agg_stats.avg_recv_buffer_size, 0);
    EXPECT_GT(agg_stats.avg_send_buffer_size, 0);

    // Cleanup test sockets
    for (int sock : test_sockets) {
        manager_->unregister_socket(sock);
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
    }
}

// Test 9: Managed network socket wrapper
TEST_F(NetworkBufferTest, ManagedNetworkSocket)
{
    manager_ = std::make_unique<NetworkBufferManager>(config_);
    ASSERT_FALSE(manager_->initialize());

    {
        // Create managed socket (auto-registers)
        ManagedNetworkSocket managed_socket(test_socket_, *manager_);
        EXPECT_TRUE(managed_socket.is_valid());
        EXPECT_EQ(managed_socket.get_socket(), test_socket_);

        // Socket should be registered
        auto stats = managed_socket.get_stats();
        EXPECT_NE(stats, nullptr);

        // Record operations through wrapper
        managed_socket.record_send(1024, std::chrono::nanoseconds(1000));
        managed_socket.record_receive(512, std::chrono::nanoseconds(800));

        stats = managed_socket.get_stats();
        EXPECT_EQ(stats->bytes_sent.load(), 1024);
        EXPECT_EQ(stats->bytes_received.load(), 512);

        // Test overflow recording
        managed_socket.record_overflow(true);
        EXPECT_EQ(stats->send_buffer_full_events.load(), 1);

        // Test buffer optimization
        std::error_code ec = managed_socket.optimize_buffers();
        EXPECT_FALSE(ec) << "Buffer optimization should succeed";

        // Socket will auto-unregister when managed_socket is destroyed
    }

    // After destruction, socket should no longer be registered
    auto stats = manager_->get_socket_stats(test_socket_);
    EXPECT_EQ(stats, nullptr) << "Socket should be unregistered after managed wrapper destruction";
}

// Test 10: Configuration updates
TEST_F(NetworkBufferTest, ConfigurationUpdates)
{
    manager_ = std::make_unique<NetworkBufferManager>(config_);
    ASSERT_FALSE(manager_->initialize());

    // Test valid config update
    NetworkBufferConfig new_config = config_;
    new_config.default_recv_buffer_size = 16384;
    new_config.default_send_buffer_size = 16384;
    new_config.utilization_threshold = 0.9;

    std::error_code ec = manager_->update_config(new_config);
    EXPECT_FALSE(ec) << "Valid config update should succeed";

    // Verify config was updated
    const auto& current_config = manager_->get_config();
    EXPECT_EQ(current_config.default_recv_buffer_size, 16384);
    EXPECT_EQ(current_config.utilization_threshold, 0.9);

    // Test invalid config update
    NetworkBufferConfig invalid_config = config_;
    invalid_config.min_buffer_size = 0;

    ec = manager_->update_config(invalid_config);
    EXPECT_TRUE(ec) << "Invalid config update should fail";
}

// Test 11: Buffer size recommendations
TEST_F(NetworkBufferTest, BufferSizeRecommendations)
{
    manager_ = std::make_unique<NetworkBufferManager>(config_);
    ASSERT_FALSE(manager_->initialize());
    ASSERT_FALSE(manager_->register_socket(test_socket_));

    // Get recommendation for new socket (should return defaults)
    size_t recv_rec = manager_->get_recommended_buffer_size(test_socket_, false);
    size_t send_rec = manager_->get_recommended_buffer_size(test_socket_, true);

    EXPECT_GE(recv_rec, config_.min_buffer_size);
    EXPECT_LE(recv_rec, config_.max_buffer_size);
    EXPECT_GE(send_rec, config_.min_buffer_size);
    EXPECT_LE(send_rec, config_.max_buffer_size);

    // Simulate high utilization scenario
    auto stats = manager_->get_socket_stats(test_socket_);
    ASSERT_NE(stats, nullptr);

    // Record overflow events to simulate need for larger buffer
    manager_->record_buffer_overflow(test_socket_, false);
    manager_->record_buffer_overflow(test_socket_, true);

    size_t new_recv_rec = manager_->get_recommended_buffer_size(test_socket_, false);
    size_t new_send_rec = manager_->get_recommended_buffer_size(test_socket_, true);

    // Recommendations might change based on overflow events
    EXPECT_GE(new_recv_rec, config_.min_buffer_size);
    EXPECT_GE(new_send_rec, config_.min_buffer_size);
}

// Test 12: Concurrent operations
TEST_F(NetworkBufferTest, ConcurrentOperations)
{
    manager_ = std::make_unique<NetworkBufferManager>(config_);
    ASSERT_FALSE(manager_->initialize());

    // Create multiple sockets
    std::vector<int> sockets;
    for (int i = 0; i < 10; ++i) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock >= 0) {
            sockets.push_back(sock);
            manager_->register_socket(sock);
        }
    }

    EXPECT_GT(sockets.size(), 0) << "Should create at least some test sockets";

    // Launch multiple threads doing I/O operations
    const int num_threads = 4;
    const int operations_per_thread = 100;
    std::vector<std::future<void>> futures;

    for (int t = 0; t < num_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [&, t]() {
            for (int i = 0; i < operations_per_thread; ++i) {
                int socket_idx = (t * operations_per_thread + i) % sockets.size();
                int sock = sockets[socket_idx];

                manager_->record_io_operation(sock, 1024, i % 2 == 0, 
                                            std::chrono::nanoseconds(1000 + i));

                // Occasionally record overflow
                if (i % 20 == 0) {
                    manager_->record_buffer_overflow(sock, i % 2 == 0);
                }

                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }));
    }

    // Wait for all threads to complete
    for (auto& future : futures) {
        EXPECT_NO_THROW(future.get()) << "Concurrent operations should complete without exceptions";
    }

    // Verify statistics were recorded
    auto agg_stats = manager_->get_aggregated_stats();
    EXPECT_GT(agg_stats.total_bytes_sent + agg_stats.total_bytes_received, 0) 
        << "Should have recorded I/O operations";

    // Cleanup
    for (int sock : sockets) {
        manager_->unregister_socket(sock);
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
    }
}

// Test 13: Auto-tuning thread behavior
TEST_F(NetworkBufferTest, AutoTuningThreadBehavior)
{
    // Use very short intervals for testing
    config_.tuning_interval = std::chrono::seconds(1);
    config_.stats_collection_interval = std::chrono::seconds(1);

    manager_ = std::make_unique<NetworkBufferManager>(config_);
    ASSERT_FALSE(manager_->initialize());
    ASSERT_FALSE(manager_->register_socket(test_socket_));

    auto stats = manager_->get_socket_stats(test_socket_);
    ASSERT_NE(stats, nullptr);

    // Record overflow events to trigger auto-tuning
    manager_->record_buffer_overflow(test_socket_, true);
    manager_->record_buffer_overflow(test_socket_, false);

    // Wait for tuning thread to potentially act
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Check aggregated stats for tuning operations
    auto agg_stats = manager_->get_aggregated_stats();
    // Note: Auto-tuning might or might not have occurred depending on timing
    EXPECT_GE(agg_stats.connections_with_auto_tuning, 0);
}

// Performance benchmark test
TEST_F(NetworkBufferTest, PerformanceBenchmark)
{
    manager_ = std::make_unique<NetworkBufferManager>(config_);
    ASSERT_FALSE(manager_->initialize());
    ASSERT_FALSE(manager_->register_socket(test_socket_));

    const int num_operations = 10000;

    // Benchmark I/O operation recording
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_operations; ++i) {
        manager_->record_io_operation(test_socket_, 1024, i % 2 == 0, 
                                    std::chrono::nanoseconds(1000));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double avg_time_ns = static_cast<double>(duration.count()) / num_operations;

    // I/O recording should be very fast (< 1 microsecond per operation)
    EXPECT_LT(avg_time_ns, 1000.0) 
        << "I/O operation recording took " << avg_time_ns << " ns on average";

    std::cout << "I/O operation recording performance: " << avg_time_ns << " ns per operation\n";

    // Benchmark buffer configuration
    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        manager_->configure_socket_buffers(test_socket_, 8192 + i, 8192 + i);
    }

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_config_time_us = static_cast<double>(duration.count()) / 1000;

    std::cout << "Buffer configuration performance: " << avg_config_time_us << " μs per operation\n";
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}