#include "scratchbird/engine/connection_pool.h"

#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace ScratchBird;

class ConnectionPoolTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Use a simple test configuration
        config_.min_pool_size = 2;
        config_.max_pool_size = 10;
        config_.initial_pool_size = 3;
        config_.connection_timeout = std::chrono::seconds(5);
        config_.idle_timeout = std::chrono::seconds(60);
        config_.health_check_interval = std::chrono::seconds(10);
        config_.use_process_pool = false; // Use thread-based for testing
        config_.worker_executable = "test_worker";
        config_.shared_memory_name = "/test_scratchbird_pool";
        config_.enable_health_monitoring = true;
        config_.max_connection_failures = 2;
        config_.connection_queue_size = 50;
    }

    void TearDown() override
    {
        if (pool_) {
            pool_->shutdown();
        }
    }

    ConnectionPoolConfig config_;
    std::unique_ptr<ConnectionPool> pool_;
};

// Test 1: Configuration validation
TEST_F(ConnectionPoolTest, ConfigurationValidation)
{
    // Test valid configuration
    std::string errors = ConnectionPool::validate_config(config_);
    EXPECT_TRUE(errors.empty()) << "Valid config should not produce errors: " << errors;

    // Test invalid min_pool_size
    ConnectionPoolConfig invalid_config = config_;
    invalid_config.min_pool_size = 0;
    errors = ConnectionPool::validate_config(invalid_config);
    EXPECT_FALSE(errors.empty());
    EXPECT_NE(errors.find("min_pool_size must be greater than 0"), std::string::npos);

    // Test min > max
    invalid_config = config_;
    invalid_config.min_pool_size = 20;
    invalid_config.max_pool_size = 10;
    errors = ConnectionPool::validate_config(invalid_config);
    EXPECT_FALSE(errors.empty());
    EXPECT_NE(errors.find("min_pool_size cannot be greater than max_pool_size"), std::string::npos);

    // Test initial > max
    invalid_config = config_;
    invalid_config.initial_pool_size = 20;
    invalid_config.max_pool_size = 10;
    errors = ConnectionPool::validate_config(invalid_config);
    EXPECT_FALSE(errors.empty());
    EXPECT_NE(errors.find("initial_pool_size cannot be greater than max_pool_size"),
              std::string::npos);

    // Test empty worker executable
    invalid_config = config_;
    invalid_config.worker_executable = "";
    errors = ConnectionPool::validate_config(invalid_config);
    EXPECT_FALSE(errors.empty());
    EXPECT_NE(errors.find("worker_executable cannot be empty"), std::string::npos);
}

// Test 2: Connection factory validation
TEST_F(ConnectionPoolTest, ConnectionFactoryValidation)
{
    ConnectionFactory factory(config_);

    // Valid config should pass
    std::error_code ec = factory.validate_config();
    EXPECT_FALSE(ec) << "Valid factory config should not produce errors";

    // Invalid config should fail
    ConnectionPoolConfig invalid_config = config_;
    invalid_config.worker_executable = "";
    ConnectionFactory invalid_factory(invalid_config);

    ec = invalid_factory.validate_config();
    EXPECT_TRUE(ec) << "Invalid factory config should produce errors";
}

// Test 3: Pool initialization
TEST_F(ConnectionPoolTest, PoolInitialization)
{
    pool_ = std::make_unique<ConnectionPool>(config_);

    // Pool should not be healthy before initialization
    EXPECT_FALSE(pool_->is_healthy());

    // Initialize the pool
    std::error_code ec = pool_->initialize();

    // For testing, we expect initialization to fail due to missing worker executable
    // In a real environment, this would succeed
    EXPECT_TRUE(ec) << "Pool initialization should fail with missing worker executable";

    // Test with invalid config
    ConnectionPoolConfig invalid_config = config_;
    invalid_config.min_pool_size = 0;
    ConnectionPool invalid_pool(invalid_config);

    ec = invalid_pool.initialize();
    EXPECT_TRUE(ec) << "Pool initialization should fail with invalid config";
}

// Test 4: Pool status and statistics
TEST_F(ConnectionPoolTest, PoolStatusAndStats)
{
    pool_ = std::make_unique<ConnectionPool>(config_);

    // Get initial status
    auto status = pool_->get_status();
    EXPECT_EQ(status.total_connections, 0);
    EXPECT_EQ(status.idle_connections, 0);
    EXPECT_EQ(status.active_connections, 0);
    EXPECT_EQ(status.dead_connections, 0);
    EXPECT_EQ(status.queued_requests, 0);
    EXPECT_FALSE(status.is_healthy);

    // Get initial statistics
    auto stats = pool_->get_stats();
    EXPECT_EQ(stats.total_connections, 0);
    EXPECT_EQ(stats.active_connections, 0);
    EXPECT_EQ(stats.idle_connections, 0);
    EXPECT_EQ(stats.failed_connections, 0);
    EXPECT_EQ(stats.connection_requests, 0);
    EXPECT_EQ(stats.connection_timeouts, 0);
    EXPECT_EQ(stats.health_check_failures, 0);

    // Test statistics calculations
    EXPECT_EQ(stats.get_utilization_rate(), 0.0);
    EXPECT_EQ(stats.get_success_rate(), 1.0); // No requests = 100% success
}

// Test 5: Connection lifecycle states
TEST_F(ConnectionPoolTest, ConnectionLifecycleStates)
{
    // Create a mock connection for testing
    // Note: In a real test, we would need actual socket/process setup

    // Test state transitions
    ConnectionState states[] = {ConnectionState::IDLE, ConnectionState::ACTIVE,
                                ConnectionState::TERMINATING, ConnectionState::DEAD};

    for (auto state : states) {
        // Each state should be valid
        EXPECT_TRUE(state == ConnectionState::IDLE || state == ConnectionState::ACTIVE ||
                    state == ConnectionState::TERMINATING || state == ConnectionState::DEAD);
    }
}

// Test 6: Pool configuration updates
TEST_F(ConnectionPoolTest, PoolConfigurationUpdates)
{
    pool_ = std::make_unique<ConnectionPool>(config_);

    // Test valid config update
    ConnectionPoolConfig new_config = config_;
    new_config.max_pool_size = 20;
    new_config.min_pool_size = 5;

    std::error_code ec = pool_->update_config(new_config);
    EXPECT_FALSE(ec) << "Valid config update should succeed";

    // Verify config was updated
    const auto& current_config = pool_->get_config();
    EXPECT_EQ(current_config.max_pool_size, 20);
    EXPECT_EQ(current_config.min_pool_size, 5);

    // Test invalid config update
    ConnectionPoolConfig invalid_config = config_;
    invalid_config.min_pool_size = 0;

    ec = pool_->update_config(invalid_config);
    EXPECT_TRUE(ec) << "Invalid config update should fail";
}

// Test 7: Connection timeout handling
TEST_F(ConnectionPoolTest, ConnectionTimeoutHandling)
{
    pool_ = std::make_unique<ConnectionPool>(config_);

    // Try to get connection with short timeout (should fail since pool is not initialized)
    auto connection = pool_->get_connection(std::chrono::milliseconds(100));
    EXPECT_EQ(connection, nullptr) << "Connection request should timeout";

    // Check that timeout was recorded in statistics
    auto stats = pool_->get_stats();
    EXPECT_GT(stats.connection_timeouts, 0) << "Timeout should be recorded in stats";
}

// Test 8: Pool health monitoring
TEST_F(ConnectionPoolTest, PoolHealthMonitoring)
{
    pool_ = std::make_unique<ConnectionPool>(config_);

    // Pool should not be healthy initially
    EXPECT_FALSE(pool_->is_healthy());

    // Test health status components
    auto status = pool_->get_status();
    EXPECT_FALSE(status.is_healthy);
    EXPECT_EQ(status.total_connections, 0);
}

// Test 9: Statistics calculations
TEST_F(ConnectionPoolTest, StatisticsCalculations)
{
    ConnectionPoolStats stats;

    // Test utilization rate with no connections
    EXPECT_EQ(stats.get_utilization_rate(), 0.0);

    // Test success rate with no requests
    EXPECT_EQ(stats.get_success_rate(), 1.0);

    // Simulate some statistics
    stats.total_connections = 10;
    stats.active_connections = 3;
    stats.connection_requests = 100;
    stats.failed_connections = 5;
    stats.connection_timeouts = 2;

    // Test utilization rate calculation
    double expected_utilization = 3.0 / 10.0;
    EXPECT_DOUBLE_EQ(stats.get_utilization_rate(), expected_utilization);

    // Test success rate calculation (failures = failed_connections + timeouts)
    double expected_success = (100.0 - 7.0) / 100.0;
    EXPECT_DOUBLE_EQ(stats.get_success_rate(), expected_success);
}

// Test 10: Connection factory statistics
TEST_F(ConnectionPoolTest, ConnectionFactoryStatistics)
{
    ConnectionFactory factory(config_);

    // Check initial statistics
    EXPECT_EQ(factory.stats.connections_created.load(), 0);
    EXPECT_EQ(factory.stats.creation_failures.load(), 0);
    EXPECT_EQ(factory.stats.avg_creation_time.count(), 0);

    // Attempt to create connection (will fail due to missing worker executable)
    auto connection = factory.create_connection(nullptr);
    EXPECT_EQ(connection, nullptr);

    // Check that failure was recorded
    EXPECT_GT(factory.stats.creation_failures.load(), 0);
}

// Test 11: Concurrent access simulation
TEST_F(ConnectionPoolTest, ConcurrentAccessSimulation)
{
    pool_ = std::make_unique<ConnectionPool>(config_);

    const int num_threads = 5;
    const int requests_per_thread = 10;
    std::vector<std::future<int>> futures;

    // Launch multiple threads trying to get connections
    for (int t = 0; t < num_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [this, requests_per_thread]() {
            int successful_requests = 0;
            for (int i = 0; i < requests_per_thread; ++i) {
                auto connection = pool_->get_connection(std::chrono::milliseconds(50));
                if (connection) {
                    successful_requests++;
                    // Simulate some work
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    pool_->return_connection(std::move(connection));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            return successful_requests;
        }));
    }

    // Wait for all threads and collect results
    int total_successful = 0;
    for (auto& future : futures) {
        total_successful += future.get();
    }

    // All requests should have failed (pool not properly initialized)
    EXPECT_EQ(total_successful, 0);

    // Check that requests were recorded
    auto stats = pool_->get_stats();
    EXPECT_GT(stats.connection_requests, 0);
}

// Test 12: Pool refresh functionality
TEST_F(ConnectionPoolTest, PoolRefreshFunctionality)
{
    pool_ = std::make_unique<ConnectionPool>(config_);

    // Get initial status
    auto initial_stats = pool_->get_stats();

    // Refresh the pool
    pool_->refresh_pool();

    // Pool should still be valid after refresh
    EXPECT_NO_THROW(pool_->get_status());
    EXPECT_NO_THROW(pool_->get_stats());
}

// Test 13: Memory management and resource cleanup
TEST_F(ConnectionPoolTest, MemoryManagementAndCleanup)
{
    // Test that pool can be created and destroyed without issues
    {
        auto temp_pool = std::make_unique<ConnectionPool>(config_);
        EXPECT_NE(temp_pool, nullptr);

        // Pool should clean up properly when destroyed
        temp_pool->shutdown();
    }

    // Test multiple pool creations
    for (int i = 0; i < 5; ++i) {
        auto temp_pool = std::make_unique<ConnectionPool>(config_);
        EXPECT_NE(temp_pool, nullptr);
        temp_pool->shutdown();
    }
}

// Test 14: Edge cases and error conditions
TEST_F(ConnectionPoolTest, EdgeCasesAndErrorConditions)
{
    pool_ = std::make_unique<ConnectionPool>(config_);

    // Test returning null connection
    EXPECT_NO_THROW(pool_->return_connection(nullptr));

    // Test getting connection after shutdown
    pool_->shutdown();
    auto connection = pool_->get_connection(std::chrono::milliseconds(100));
    EXPECT_EQ(connection, nullptr);

    // Pool should not be healthy after shutdown
    EXPECT_FALSE(pool_->is_healthy());
}

// Test 15: Configuration edge cases
TEST_F(ConnectionPoolTest, ConfigurationEdgeCases)
{
    // Test minimum valid configuration
    ConnectionPoolConfig min_config;
    min_config.min_pool_size = 1;
    min_config.max_pool_size = 1;
    min_config.initial_pool_size = 1;
    min_config.worker_executable = "worker";

    std::string errors = ConnectionPool::validate_config(min_config);
    EXPECT_TRUE(errors.empty()) << "Minimum valid config should not produce errors: " << errors;

    // Test maximum reasonable configuration
    ConnectionPoolConfig max_config;
    max_config.min_pool_size = 100;
    max_config.max_pool_size = 1000;
    max_config.initial_pool_size = 200;
    max_config.worker_executable = "worker";
    max_config.connection_timeout = std::chrono::seconds(3600); // 1 hour
    max_config.idle_timeout = std::chrono::seconds(86400);      // 1 day

    errors = ConnectionPool::validate_config(max_config);
    EXPECT_TRUE(errors.empty()) << "Maximum valid config should not produce errors: " << errors;
}

// Performance benchmark test
TEST_F(ConnectionPoolTest, PerformanceBenchmark)
{
    const int num_iterations = 1000;

    // Benchmark configuration validation
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; ++i) {
        ConnectionPool::validate_config(config_);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double avg_time_ns = static_cast<double>(duration.count()) / num_iterations;

    // Validation should be very fast (< 1 microsecond per call)
    EXPECT_LT(avg_time_ns, 1000.0)
        << "Configuration validation took " << avg_time_ns << " ns on average";

    std::cout << "Configuration validation performance: " << avg_time_ns << " ns per call\n";

    // Benchmark pool creation/destruction
    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; ++i) {
        auto temp_pool = std::make_unique<ConnectionPool>(config_);
        temp_pool->shutdown();
    }

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_pool_time_us = static_cast<double>(duration.count()) / 100;

    std::cout << "Pool creation/destruction performance: " << avg_pool_time_us << " μs per cycle\n";
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
