// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/wire_compression.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace scratchbird::engine;

class WireCompressionTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create default configuration for testing
        config_.algorithm = CompressionAlgorithm::ZLIB;
        config_.level = CompressionLevel::BALANCED;
        config_.adaptive_level = false; // Disable for predictable testing
        config_.min_compress_size = 64;
        config_.cpu_threshold = 0.8;
        config_.max_compress_time_ms = 10.0;
        config_.target_compression_ratio = 0.7;
        config_.buffer_size = 4096;
        config_.enable_statistics = true;

        compressor_ = std::make_unique<WireCompression>(config_);

        // Create test data with known compression characteristics
        create_test_data();
    }

    void TearDown() override
    {
        compressor_.reset();
    }

    void create_test_data()
    {
        // Compressible data - repeating pattern
        std::string pattern = "ScratchBird Database Engine Test Data Pattern 12345";
        compressible_data_.clear();
        for (int i = 0; i < 50; ++i) {
            compressible_data_.insert(compressible_data_.end(), pattern.begin(), pattern.end());
        }

        // Incompressible data - random bytes
        incompressible_data_.resize(2048);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for (auto& byte : incompressible_data_) {
            byte = static_cast<std::uint8_t>(dis(gen));
        }

        // Small data - below minimum compression threshold
        small_data_ = {0x01, 0x02, 0x03, 0x04, 0x05};

        // Large data - structured but large
        large_data_.clear();
        for (int i = 0; i < 10000; ++i) {
            std::string record = "Record" + std::to_string(i) + " with some data content";
            large_data_.insert(large_data_.end(), record.begin(), record.end());
        }
    }

    CompressionConfig config_;
    std::unique_ptr<WireCompression> compressor_;

    std::vector<std::uint8_t> compressible_data_;
    std::vector<std::uint8_t> incompressible_data_;
    std::vector<std::uint8_t> small_data_;
    std::vector<std::uint8_t> large_data_;
};

TEST_F(WireCompressionTest, ConfigurationValidation)
{
    // Test valid configuration
    CompressionConfig valid_config;
    EXPECT_TRUE(valid_config.is_valid());

    // Test invalid configurations
    CompressionConfig invalid_config;

    // Invalid min_compress_size
    invalid_config.min_compress_size = 0;
    EXPECT_FALSE(invalid_config.is_valid());

    // Invalid cpu_threshold
    invalid_config = CompressionConfig{};
    invalid_config.cpu_threshold = 1.5; // > 1.0
    EXPECT_FALSE(invalid_config.is_valid());

    // Invalid compression ratio
    invalid_config = CompressionConfig{};
    invalid_config.target_compression_ratio = 1.5; // >= 1.0
    EXPECT_FALSE(invalid_config.is_valid());

    // Invalid buffer size
    invalid_config = CompressionConfig{};
    invalid_config.buffer_size = 512; // < 1024
    EXPECT_FALSE(invalid_config.is_valid());
}

TEST_F(WireCompressionTest, BasicCompressionDecompression)
{
    // Test compression of compressible data
    auto compressed = compressor_->compress(compressible_data_);
    EXPECT_FALSE(compressed.empty());
    EXPECT_LT(compressed.size(), compressible_data_.size()); // Should compress

    // Test decompression
    auto decompressed = compressor_->decompress(compressed, compressible_data_.size());
    EXPECT_FALSE(decompressed.empty());
    EXPECT_EQ(decompressed.size(), compressible_data_.size());
    EXPECT_EQ(decompressed, compressible_data_); // Should match original
}

TEST_F(WireCompressionTest, BufferCompressionDecompression)
{
    // Test raw buffer compression
    std::vector<std::uint8_t> output_buffer(
        compressor_->get_max_compressed_size(compressible_data_.size()));
    std::size_t output_size = output_buffer.size();

    bool compress_result = compressor_->compress(
        compressible_data_.data(), compressible_data_.size(), output_buffer.data(), output_size);

    EXPECT_TRUE(compress_result);
    EXPECT_GT(output_size, 0u);
    EXPECT_LT(output_size, compressible_data_.size());

    // Test raw buffer decompression
    std::vector<std::uint8_t> decompress_buffer(compressible_data_.size());
    std::size_t decompress_size = decompress_buffer.size();

    bool decompress_result = compressor_->decompress(output_buffer.data(), output_size,
                                                     decompress_buffer.data(), decompress_size);

    EXPECT_TRUE(decompress_result);
    EXPECT_EQ(decompress_size, compressible_data_.size());

    // Verify data integrity
    decompress_buffer.resize(decompress_size);
    EXPECT_EQ(decompress_buffer, compressible_data_);
}

TEST_F(WireCompressionTest, CompressionDecisionLogic)
{
    // Should not compress small data
    EXPECT_FALSE(compressor_->should_compress(small_data_.size()));

    // Should compress large compressible data
    EXPECT_TRUE(compressor_->should_compress(compressible_data_.size()));

    // Should not compress based on data type hints
    EXPECT_FALSE(compressor_->should_compress(1024, "image"));
    EXPECT_FALSE(compressor_->should_compress(1024, "compressed"));
    EXPECT_FALSE(compressor_->should_compress(1024, "binary"));

    // Should compress text-like data
    EXPECT_TRUE(compressor_->should_compress(1024, "text"));
    EXPECT_TRUE(compressor_->should_compress(1024, "")); // No hint
}

TEST_F(WireCompressionTest, CompressionLevels)
{
    std::vector<CompressionLevel> levels = {CompressionLevel::FAST, CompressionLevel::BALANCED,
                                            CompressionLevel::MAXIMUM};

    std::vector<std::size_t> compressed_sizes;
    std::vector<double> compression_times;

    for (auto level : levels) {
        auto config = config_;
        config.level = level;
        WireCompression level_compressor(config);

        auto start = std::chrono::high_resolution_clock::now();
        auto compressed = level_compressor.compress(large_data_);
        auto end = std::chrono::high_resolution_clock::now();

        EXPECT_FALSE(compressed.empty());

        // Verify decompression works
        auto decompressed = level_compressor.decompress(compressed, large_data_.size());
        EXPECT_EQ(decompressed, large_data_);

        compressed_sizes.push_back(compressed.size());
        auto duration = std::chrono::duration<double, std::milli>(end - start);
        compression_times.push_back(duration.count());
    }

    // Higher compression levels should generally produce smaller output
    // (though this isn't guaranteed for all data types)
    EXPECT_GT(compressed_sizes.size(), 0u);
}

TEST_F(WireCompressionTest, StatisticsCollection)
{
    // Reset statistics
    compressor_->reset_statistics();

    auto initial_stats = compressor_->get_statistics();
    EXPECT_EQ(initial_stats.compress_operations, 0u);
    EXPECT_EQ(initial_stats.decompress_operations, 0u);

    // Perform compression operations
    auto compressed1 = compressor_->compress(compressible_data_);
    auto compressed2 = compressor_->compress(large_data_);

    auto stats_after_compress = compressor_->get_statistics();
    EXPECT_EQ(stats_after_compress.compress_operations, 2u);
    EXPECT_GT(stats_after_compress.bytes_uncompressed, 0u);
    EXPECT_GT(stats_after_compress.bytes_compressed, 0u);
    EXPECT_GT(stats_after_compress.compress_time_us, 0u);

    // Test compression ratio calculation
    double ratio = stats_after_compress.get_compression_ratio();
    EXPECT_GT(ratio, 0.0);
    EXPECT_LT(ratio, 1.0); // Should be compressed

    double savings = stats_after_compress.get_bandwidth_savings();
    EXPECT_GT(savings, 0.0);
    EXPECT_LT(savings, 1.0);
    EXPECT_DOUBLE_EQ(savings, 1.0 - ratio);

    // Perform decompression operations
    compressor_->decompress(compressed1, compressible_data_.size());
    compressor_->decompress(compressed2, large_data_.size());

    auto final_stats = compressor_->get_statistics();
    EXPECT_EQ(final_stats.decompress_operations, 2u);
    EXPECT_GT(final_stats.decompress_time_us, 0u);
}

TEST_F(WireCompressionTest, ConfigurationUpdates)
{
    // Test configuration update
    auto original_config = compressor_->get_config();
    EXPECT_EQ(original_config.level, CompressionLevel::BALANCED);

    auto new_config = original_config;
    new_config.level = CompressionLevel::MAXIMUM;
    new_config.min_compress_size = 128;

    EXPECT_TRUE(compressor_->update_config(new_config));

    auto updated_config = compressor_->get_config();
    EXPECT_EQ(updated_config.level, CompressionLevel::MAXIMUM);
    EXPECT_EQ(updated_config.min_compress_size, 128u);

    // Test invalid configuration update
    new_config.min_compress_size = 0; // Invalid
    EXPECT_FALSE(compressor_->update_config(new_config));

    // Configuration should remain unchanged after failed update
    auto unchanged_config = compressor_->get_config();
    EXPECT_EQ(unchanged_config.level, CompressionLevel::MAXIMUM);
    EXPECT_EQ(unchanged_config.min_compress_size, 128u);
}

TEST_F(WireCompressionTest, SelfTest)
{
    // Test self-test functionality
    EXPECT_TRUE(compressor_->self_test());

    // Test with different algorithms
    auto config = config_;
    config.algorithm = CompressionAlgorithm::NONE;
    WireCompression none_compressor(config);
    EXPECT_TRUE(none_compressor.self_test());
}

TEST_F(WireCompressionTest, EfficiencyReport)
{
    // Perform some operations to generate statistics
    compressor_->compress(compressible_data_);
    compressor_->compress(large_data_);

    auto report = compressor_->generate_efficiency_report();
    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("Wire Compression Efficiency Report"), std::string::npos);
    EXPECT_NE(report.find("Algorithm:"), std::string::npos);
    EXPECT_NE(report.find("Compression Ratio:"), std::string::npos);
    EXPECT_NE(report.find("Bandwidth Savings:"), std::string::npos);
}

TEST_F(WireCompressionTest, ErrorHandling)
{
    // Test null input
    std::size_t output_size = 1024;
    std::vector<std::uint8_t> output(output_size);

    EXPECT_FALSE(compressor_->compress(nullptr, 100, output.data(), output_size));
    EXPECT_FALSE(compressor_->decompress(nullptr, 100, output.data(), output_size));

    // Test zero size
    output_size = 1024;
    EXPECT_FALSE(compressor_->compress(compressible_data_.data(), 0, output.data(), output_size));
    EXPECT_FALSE(compressor_->decompress(compressible_data_.data(), 0, output.data(), output_size));

    // Test null output
    output_size = 1024;
    EXPECT_FALSE(compressor_->compress(compressible_data_.data(), compressible_data_.size(),
                                       nullptr, output_size));
    EXPECT_FALSE(compressor_->decompress(compressible_data_.data(), compressible_data_.size(),
                                         nullptr, output_size));
}

TEST_F(WireCompressionTest, NoCompressionMode)
{
    // Test with no compression
    auto config = config_;
    config.algorithm = CompressionAlgorithm::NONE;
    WireCompression none_compressor(config);

    // Compression should return input unchanged
    auto result = none_compressor.compress(compressible_data_);
    EXPECT_EQ(result, compressible_data_);

    // Decompression should return input unchanged
    auto decompressed = none_compressor.decompress(compressible_data_, compressible_data_.size());
    EXPECT_EQ(decompressed, compressible_data_);

    // Should not recommend compression
    EXPECT_FALSE(none_compressor.should_compress(1024));
}

// WireCompressionManager Tests

class WireCompressionManagerTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        config_ = get_default_wire_compression_config();
        manager_ = std::make_unique<WireCompressionManager>(config_);

        connection_id_ = 12345;
        ASSERT_TRUE(manager_->initialize(connection_id_));

        // Test data
        test_message_ = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'};
        for (int i = 0; i < 20; ++i) {
            test_message_.insert(test_message_.end(), test_message_.begin(),
                                 test_message_.begin() + 12);
        }
    }

    void TearDown() override
    {
        manager_->shutdown();
        manager_.reset();
    }

    CompressionConfig config_;
    std::unique_ptr<WireCompressionManager> manager_;
    std::uint32_t connection_id_;
    std::vector<std::uint8_t> test_message_;
};

TEST_F(WireCompressionManagerTest, Initialization)
{
    EXPECT_EQ(manager_->get_connection_id(), connection_id_);
    EXPECT_FALSE(manager_->is_compression_active()); // Not negotiated yet
}

TEST_F(WireCompressionManagerTest, CompressionNegotiation)
{
    // Test negotiation with zlib support
    std::vector<CompressionAlgorithm> client_algorithms = {CompressionAlgorithm::ZLIB,
                                                           CompressionAlgorithm::NONE};
    std::vector<CompressionLevel> client_levels = {
        CompressionLevel::FAST, CompressionLevel::BALANCED, CompressionLevel::MAXIMUM};

    auto [algorithm, level] = manager_->negotiate_compression(client_algorithms, client_levels);
    EXPECT_EQ(algorithm, CompressionAlgorithm::ZLIB);
    EXPECT_NE(level, CompressionLevel::NONE);

    // Set negotiated parameters
    EXPECT_TRUE(manager_->set_compression_parameters(algorithm, level));
    EXPECT_TRUE(manager_->is_compression_active());

    // Test negotiation with no compression support
    client_algorithms = {CompressionAlgorithm::NONE};
    client_levels = {CompressionLevel::NONE};

    auto [no_algorithm, no_level] =
        manager_->negotiate_compression(client_algorithms, client_levels);
    EXPECT_EQ(no_algorithm, CompressionAlgorithm::NONE);
    EXPECT_EQ(no_level, CompressionLevel::NONE);
}

TEST_F(WireCompressionManagerTest, MessageCompressionDecompression)
{
    // Enable compression
    EXPECT_TRUE(manager_->set_compression_parameters(CompressionAlgorithm::ZLIB,
                                                     CompressionLevel::BALANCED));

    // Test message compression
    auto compressed_message = manager_->compress_message(test_message_);
    EXPECT_LT(compressed_message.size(), test_message_.size()); // Should be compressed

    // Test message decompression
    auto decompressed_message =
        manager_->decompress_message(compressed_message, test_message_.size());
    EXPECT_EQ(decompressed_message, test_message_);

    // Check connection statistics
    auto stats = manager_->get_connection_statistics();
    EXPECT_EQ(stats.compress_operations, 1u);
    EXPECT_EQ(stats.decompress_operations, 1u);
}

TEST_F(WireCompressionManagerTest, NoCompressionFallback)
{
    // Without compression negotiation, messages should pass through unchanged
    EXPECT_FALSE(manager_->is_compression_active());

    auto result = manager_->compress_message(test_message_);
    EXPECT_EQ(result, test_message_); // Should be unchanged

    auto decompressed = manager_->decompress_message(test_message_);
    EXPECT_EQ(decompressed, test_message_); // Should be unchanged
}

TEST_F(WireCompressionManagerTest, ConnectionStatistics)
{
    // Enable compression
    EXPECT_TRUE(
        manager_->set_compression_parameters(CompressionAlgorithm::ZLIB, CompressionLevel::FAST));

    // Reset statistics
    manager_->reset_connection_statistics();
    auto initial_stats = manager_->get_connection_statistics();
    EXPECT_EQ(initial_stats.compress_operations, 0u);

    // Perform operations
    manager_->compress_message(test_message_);
    manager_->compress_message(test_message_);

    auto final_stats = manager_->get_connection_statistics();
    EXPECT_EQ(final_stats.compress_operations, 2u);
    EXPECT_GT(final_stats.bytes_uncompressed, 0u);
    EXPECT_GT(final_stats.bytes_compressed, 0u);
}

// Global utility function tests

TEST(WireCompressionUtilsTest, DefaultConfiguration)
{
    auto config = get_default_wire_compression_config();
    EXPECT_TRUE(config.is_valid());
    EXPECT_EQ(config.algorithm, CompressionAlgorithm::ZLIB);
    EXPECT_TRUE(config.adaptive_level);
    EXPECT_TRUE(config.enable_statistics);
}

TEST(WireCompressionUtilsTest, CompressionBenchmark)
{
    // Create test data
    std::vector<std::uint8_t> test_data;
    std::string pattern = "Test data for benchmarking compression algorithms";
    for (int i = 0; i < 100; ++i) {
        test_data.insert(test_data.end(), pattern.begin(), pattern.end());
    }

    auto benchmarks = benchmark_compression(test_data);
    EXPECT_FALSE(benchmarks.empty());

    for (const auto& benchmark : benchmarks) {
        EXPECT_GT(benchmark.compression_ratio, 0.0);
        EXPECT_LE(benchmark.compression_ratio, 1.0);
        EXPECT_GE(benchmark.compress_time_ms, 0.0);
        EXPECT_GE(benchmark.decompress_time_ms, 0.0);
        EXPECT_GE(benchmark.throughput_mbps, 0.0);
    }
}

TEST(WireCompressionUtilsTest, DataCompressibilityDetection)
{
    // Compressible data - repeating pattern
    std::vector<std::uint8_t> compressible_data;
    std::string pattern = "Repeating pattern ";
    for (int i = 0; i < 20; ++i) {
        compressible_data.insert(compressible_data.end(), pattern.begin(), pattern.end());
    }

    EXPECT_TRUE(is_data_compressible(compressible_data.data(), compressible_data.size()));

    // Incompressible data - random
    std::vector<std::uint8_t> incompressible_data(1024);
    std::random_device rd;
    std::mt19937 gen(42); // Fixed seed for reproducible tests
    std::uniform_int_distribution<> dis(0, 255);
    for (auto& byte : incompressible_data) {
        byte = static_cast<std::uint8_t>(dis(gen));
    }

    // Random data might be detected as compressible or not - don't assert specific result
    // Just ensure the function doesn't crash
    is_data_compressible(incompressible_data.data(), incompressible_data.size());

    // Test edge cases
    EXPECT_FALSE(is_data_compressible(nullptr, 1024));
    EXPECT_FALSE(is_data_compressible(compressible_data.data(), 32)); // Too small
}

TEST(WireCompressionUtilsTest, CompressionRatioEstimation)
{
    // Highly compressible data - all zeros
    std::vector<std::uint8_t> zeros(1024, 0x00);
    double zeros_ratio = estimate_compression_ratio(zeros.data(), zeros.size());
    EXPECT_GT(zeros_ratio, 0.0);
    EXPECT_LT(zeros_ratio, 1.0);

    // Less compressible data - sequential bytes
    std::vector<std::uint8_t> sequential(256);
    std::iota(sequential.begin(), sequential.end(), 0);
    double sequential_ratio = estimate_compression_ratio(sequential.data(), sequential.size());
    EXPECT_GT(sequential_ratio, 0.0);
    EXPECT_LT(sequential_ratio, 1.0);

    // The zeros should have a better (lower) compression ratio estimate
    EXPECT_LT(zeros_ratio, sequential_ratio);
}

// Performance and stress tests (optional)

TEST(WireCompressionPerformanceTest, LargeDataCompression)
{
    // Test with large data set (1MB)
    std::vector<std::uint8_t> large_data;
    std::string record =
        "Database record with various data types: ID=12345, Name='Test User', "
        "Email='test@example.com', Data='Some long text content that should compress well'";

    while (large_data.size() < 1024 * 1024) { // 1MB
        large_data.insert(large_data.end(), record.begin(), record.end());
    }

    auto config = get_default_wire_compression_config();
    WireCompression compressor(config);

    auto start = std::chrono::high_resolution_clock::now();
    auto compressed = compressor.compress(large_data);
    auto compress_end = std::chrono::high_resolution_clock::now();

    EXPECT_FALSE(compressed.empty());
    EXPECT_LT(compressed.size(), large_data.size());

    auto decompressed = compressor.decompress(compressed, large_data.size());
    auto decompress_end = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(decompressed, large_data);

    auto compress_time = std::chrono::duration<double, std::milli>(compress_end - start);
    auto decompress_time = std::chrono::duration<double, std::milli>(decompress_end - compress_end);

    // Performance expectations (these are quite lenient)
    EXPECT_LT(compress_time.count(), 1000.0);  // Less than 1 second
    EXPECT_LT(decompress_time.count(), 500.0); // Less than 0.5 seconds

    double compression_ratio = static_cast<double>(compressed.size()) / large_data.size();
    EXPECT_LT(compression_ratio, 0.8); // At least 20% compression for structured data
}

TEST(WireCompressionPerformanceTest, ConcurrentCompression)
{
    // Test concurrent compression operations
    auto config = get_default_wire_compression_config();

    std::vector<std::uint8_t> test_data;
    std::string content = "Concurrent compression test data with repeating patterns";
    for (int i = 0; i < 200; ++i) {
        test_data.insert(test_data.end(), content.begin(), content.end());
    }

    const int num_threads = 4;
    const int operations_per_thread = 50;

    std::vector<std::thread> threads;
    std::vector<bool> results(num_threads, false);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            WireCompression compressor(config);
            bool success = true;

            for (int i = 0; i < operations_per_thread; ++i) {
                auto compressed = compressor.compress(test_data);
                if (compressed.empty()) {
                    success = false;
                    break;
                }

                auto decompressed = compressor.decompress(compressed, test_data.size());
                if (decompressed != test_data) {
                    success = false;
                    break;
                }
            }

            results[t] = success;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All threads should complete successfully
    for (bool result : results) {
        EXPECT_TRUE(result);
    }
}
