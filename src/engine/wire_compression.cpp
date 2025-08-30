// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/wire_compression.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>
#include <zlib.h>

namespace scratchbird::engine
{

    WireCompression::WireCompression(const CompressionConfig& config)
        : config_(config), current_level_(config.level),
          last_adjustment_(std::chrono::steady_clock::now())
    {
        assert(config_.is_valid());

        // Initialize compression library based on algorithm
        switch (config_.algorithm) {
        case CompressionAlgorithm::ZLIB:
            if (!initialize_zlib()) {
                // Fallback to no compression if initialization fails
                config_.algorithm = CompressionAlgorithm::NONE;
            }
            break;
        case CompressionAlgorithm::NONE:
        default:
            // No initialization needed
            break;
        }

        // Initialize recent ratios array
        recent_ratios_.fill(1.0); // Start with no compression assumption
    }

    WireCompression::~WireCompression()
    {
        cleanup_zlib();
    }

    bool WireCompression::compress(const std::uint8_t* input, std::size_t input_size,
                                   std::uint8_t* output, std::size_t& output_size)
    {
        if (!input || input_size == 0 || !output) {
            return false;
        }

        auto start_time = std::chrono::steady_clock::now();
        bool result = false;

        std::lock_guard<std::mutex> lock(compression_mutex_);

        try {
            switch (config_.algorithm) {
            case CompressionAlgorithm::ZLIB:
                result = compress_zlib(input, input_size, output, output_size);
                break;
            case CompressionAlgorithm::NONE:
            default:
                // No compression - just copy data
                if (output_size >= input_size) {
                    std::memcpy(output, input, input_size);
                    output_size = input_size;
                    result = true;
                } else {
                    result = false;
                }
                break;
            }
        } catch (const std::exception&) {
            if (config_.enable_statistics) {
                stats_.compress_errors.fetch_add(1);
            }
            result = false;
        }

        auto end_time = std::chrono::steady_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        if (result && config_.enable_statistics) {
            update_compress_stats(input_size, output_size, duration);
        }

        return result;
    }

    bool WireCompression::decompress(const std::uint8_t* input, std::size_t input_size,
                                     std::uint8_t* output, std::size_t& output_size)
    {
        if (!input || input_size == 0 || !output) {
            return false;
        }

        auto start_time = std::chrono::steady_clock::now();
        bool result = false;

        std::lock_guard<std::mutex> lock(compression_mutex_);

        try {
            switch (config_.algorithm) {
            case CompressionAlgorithm::ZLIB:
                result = decompress_zlib(input, input_size, output, output_size);
                break;
            case CompressionAlgorithm::NONE:
            default:
                // No compression - just copy data
                if (output_size >= input_size) {
                    std::memcpy(output, input, input_size);
                    output_size = input_size;
                    result = true;
                } else {
                    result = false;
                }
                break;
            }
        } catch (const std::exception&) {
            if (config_.enable_statistics) {
                stats_.decompress_errors.fetch_add(1);
            }
            result = false;
        }

        auto end_time = std::chrono::steady_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        if (result && config_.enable_statistics) {
            update_decompress_stats(input_size, output_size, duration);
        }

        return result;
    }

    std::vector<std::uint8_t> WireCompression::compress(const std::vector<std::uint8_t>& input)
    {
        if (input.empty()) {
            return {};
        }

        std::size_t max_output_size = get_max_compressed_size(input.size());
        std::vector<std::uint8_t> output(max_output_size);
        std::size_t actual_output_size = max_output_size;

        if (compress(input.data(), input.size(), output.data(), actual_output_size)) {
            output.resize(actual_output_size);
            return output;
        }

        return {}; // Return empty vector on failure
    }

    std::vector<std::uint8_t> WireCompression::decompress(const std::vector<std::uint8_t>& input,
                                                          std::size_t expected_size)
    {
        if (input.empty()) {
            return {};
        }

        // If expected size not provided, estimate it (4x compressed size is reasonable)
        std::size_t output_size = expected_size > 0 ? expected_size : input.size() * 4;
        std::vector<std::uint8_t> output(output_size);

        if (decompress(input.data(), input.size(), output.data(), output_size)) {
            output.resize(output_size);
            return output;
        }

        return {}; // Return empty vector on failure
    }

    bool WireCompression::should_compress(std::size_t data_size, const std::string& data_type) const
    {
        std::lock_guard<std::mutex> lock(config_mutex_);

        // Don't compress if compression is disabled
        if (config_.algorithm == CompressionAlgorithm::NONE) {
            return false;
        }

        // Don't compress small messages
        if (data_size < config_.min_compress_size) {
            return false;
        }

        // Don't compress if message is too large
        if (data_size > config_.max_message_size) {
            return false;
        }

        // Check CPU usage if adaptive mode is enabled
        if (config_.adaptive_level) {
            double cpu_usage = get_current_cpu_usage();
            if (cpu_usage > config_.cpu_threshold) {
                return false; // Skip compression if CPU is too busy
            }
        }

        // Data type heuristics (if provided)
        if (!data_type.empty()) {
            // Skip compression for already compressed data
            if (data_type == "image" || data_type == "compressed" || data_type == "binary") {
                return false;
            }
        }

        return true;
    }

    CompressionLevel WireCompression::get_recommended_level() const
    {
        std::lock_guard<std::mutex> lock(config_mutex_);

        if (config_.adaptive_level) {
            return current_level_;
        }

        return config_.level;
    }

    bool WireCompression::update_config(const CompressionConfig& config)
    {
        if (!config.is_valid()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(config_mutex_);
        config_ = config;

        // Update current level if not in adaptive mode
        if (!config_.adaptive_level) {
            current_level_ = config_.level;
        }

        return true;
    }

    CompressionConfig WireCompression::get_config() const
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return config_;
    }

    CompressionStatistics WireCompression::get_statistics() const
    {
        return stats_.snapshot();
    }

    void WireCompression::reset_statistics()
    {
        stats_.reset();
    }

    std::string WireCompression::generate_efficiency_report() const
    {
        auto stats = get_statistics();
        std::ostringstream report;

        report << "Wire Compression Efficiency Report\n";
        report << "==================================\n";
        report << "Algorithm: " << get_algorithm_name() << "\n";
        report << "Level: " << get_level_name() << "\n";
        report << "Operations: " << stats.compress_operations << " compress, "
               << stats.decompress_operations << " decompress\n";
        report << "Data: " << stats.bytes_uncompressed << " → " << stats.bytes_compressed
               << " bytes\n";
        report << "Compression Ratio: " << (stats.get_compression_ratio() * 100.0) << "%\n";
        report << "Bandwidth Savings: " << (stats.get_bandwidth_savings() * 100.0) << "%\n";
        report << "Average Times: " << stats.get_avg_compress_time_ms() << "ms compress, "
               << stats.get_avg_decompress_time_ms() << "ms decompress\n";
        report << "Errors: " << stats.compress_errors << " compress, " << stats.decompress_errors
               << " decompress\n";

        return report.str();
    }

    void WireCompression::update_adaptive_settings()
    {
        if (!config_.adaptive_level) {
            return;
        }

        auto now = std::chrono::steady_clock::now();
        auto time_since_adjustment =
            std::chrono::duration_cast<std::chrono::seconds>(now - last_adjustment_);

        // Only adjust every 10 seconds
        if (time_since_adjustment.count() < 10) {
            return;
        }

        double avg_ratio = get_average_compression_ratio();
        auto stats_snapshot = stats_.snapshot();
        double avg_time = stats_snapshot.get_avg_compress_time_ms();

        // Adjust compression level based on performance
        if (avg_time > config_.max_compress_time_ms) {
            // Compression is too slow, reduce level
            if (current_level_ == CompressionLevel::MAXIMUM) {
                current_level_ = CompressionLevel::BALANCED;
            } else if (current_level_ == CompressionLevel::BALANCED) {
                current_level_ = CompressionLevel::FAST;
            }
        } else if (avg_ratio > config_.target_compression_ratio &&
                   avg_time < config_.max_compress_time_ms / 2) {
            // Compression is fast and could be better, increase level
            if (current_level_ == CompressionLevel::FAST) {
                current_level_ = CompressionLevel::BALANCED;
            } else if (current_level_ == CompressionLevel::BALANCED) {
                current_level_ = CompressionLevel::MAXIMUM;
            }
        }

        last_adjustment_ = now;
    }

    bool WireCompression::needs_level_adjustment() const
    {
        if (!config_.adaptive_level) {
            return false;
        }

        auto now = std::chrono::steady_clock::now();
        auto time_since_adjustment =
            std::chrono::duration_cast<std::chrono::seconds>(now - last_adjustment_);

        return time_since_adjustment.count() >= 10; // Check every 10 seconds
    }

    std::size_t WireCompression::get_max_compressed_size(std::size_t input_size) const
    {
        switch (config_.algorithm) {
        case CompressionAlgorithm::ZLIB:
            // zlib compressBound formula
            return input_size + (input_size >> 12) + (input_size >> 14) + (input_size >> 25) + 13;
        case CompressionAlgorithm::NONE:
        default:
            return input_size; // No compression
        }
    }

    bool WireCompression::self_test()
    {
        // Test data with known compression characteristics
        std::vector<std::uint8_t> test_data;

        // Create test data - repeating pattern should compress well
        std::string pattern = "ScratchBird Database Engine Test Data Pattern";
        for (int i = 0; i < 100; ++i) {
            test_data.insert(test_data.end(), pattern.begin(), pattern.end());
        }

        // Test compression
        auto compressed = compress(test_data);
        if (compressed.empty()) {
            return false;
        }

        // Test decompression
        auto decompressed = decompress(compressed, test_data.size());
        if (decompressed.empty()) {
            return false;
        }

        // Verify data integrity
        return test_data == decompressed;
    }

    std::string WireCompression::get_algorithm_name() const
    {
        switch (config_.algorithm) {
        case CompressionAlgorithm::NONE:
            return "None";
        case CompressionAlgorithm::ZLIB:
            return "zlib";
        case CompressionAlgorithm::LZ4:
            return "LZ4";
        case CompressionAlgorithm::ZSTD:
            return "Zstandard";
        default:
            return "Unknown";
        }
    }

    std::string WireCompression::get_level_name() const
    {
        switch (current_level_) {
        case CompressionLevel::NONE:
            return "None";
        case CompressionLevel::FAST:
            return "Fast";
        case CompressionLevel::BALANCED:
            return "Balanced";
        case CompressionLevel::MAXIMUM:
            return "Maximum";
        default:
            return "Unknown";
        }
    }

    // Private helper methods

    bool WireCompression::initialize_zlib()
    {
        // zlib is stateless, no initialization needed
        return true;
    }

    void WireCompression::cleanup_zlib()
    {
        // zlib is stateless, no cleanup needed
    }

    bool WireCompression::compress_zlib(const std::uint8_t* input, std::size_t input_size,
                                        std::uint8_t* output, std::size_t& output_size)
    {
        int level = static_cast<int>(current_level_);
        if (level == 0)
            level = Z_DEFAULT_COMPRESSION;

        uLongf compressed_size = static_cast<uLongf>(output_size);

        int result =
            compress2(output, &compressed_size, input, static_cast<uLong>(input_size), level);

        if (result == Z_OK) {
            output_size = static_cast<std::size_t>(compressed_size);
            return true;
        }

        return false;
    }

    bool WireCompression::decompress_zlib(const std::uint8_t* input, std::size_t input_size,
                                          std::uint8_t* output, std::size_t& output_size)
    {
        uLongf decompressed_size = static_cast<uLongf>(output_size);

        int result = uncompress(output, &decompressed_size, input, static_cast<uLong>(input_size));

        if (result == Z_OK) {
            output_size = static_cast<std::size_t>(decompressed_size);
            return true;
        }

        return false;
    }

    void WireCompression::update_compress_stats(std::size_t input_size, std::size_t output_size,
                                                std::chrono::microseconds duration)
    {
        stats_.compress_operations.fetch_add(1);
        stats_.bytes_uncompressed.fetch_add(input_size);
        stats_.bytes_compressed.fetch_add(output_size);
        stats_.compress_time_us.fetch_add(duration.count());

        // Update recent ratios for adaptive compression
        if (config_.adaptive_level) {
            double ratio = input_size > 0 ? static_cast<double>(output_size) / input_size : 1.0;
            recent_ratios_[ratio_index_] = ratio;
            ratio_index_ = (ratio_index_ + 1) % recent_ratios_.size();
        }
    }

    void WireCompression::update_decompress_stats(std::size_t input_size, std::size_t output_size,
                                                  std::chrono::microseconds duration)
    {
        (void)input_size;  // Suppress unused parameter warning
        (void)output_size; // Suppress unused parameter warning
        stats_.decompress_operations.fetch_add(1);
        stats_.decompress_time_us.fetch_add(duration.count());
    }

    double WireCompression::get_average_compression_ratio() const
    {
        double sum = 0.0;
        for (double ratio : recent_ratios_) {
            sum += ratio;
        }
        return sum / recent_ratios_.size();
    }

    double WireCompression::get_current_cpu_usage() const
    {
        // Simplified CPU usage check - in a real implementation this would
        // read from /proc/stat or use system-specific APIs
        return 0.5; // Return moderate CPU usage for now
    }

    bool WireCompression::is_algorithm_supported(CompressionAlgorithm algorithm) const
    {
        switch (algorithm) {
        case CompressionAlgorithm::NONE:
        case CompressionAlgorithm::ZLIB:
            return true;
        case CompressionAlgorithm::LZ4:
        case CompressionAlgorithm::ZSTD:
            return false; // Not implemented yet
        default:
            return false;
        }
    }

    // WireCompressionManager implementation

    WireCompressionManager::WireCompressionManager(const CompressionConfig& config)
        : compressor_(std::make_unique<WireCompression>(config))
    {
    }

    bool WireCompressionManager::initialize(std::uint32_t connection_id)
    {
        connection_id_ = connection_id;
        initialized_ = true;
        return true;
    }

    void WireCompressionManager::shutdown()
    {
        compression_active_ = false;
        initialized_ = false;
    }

    std::pair<CompressionAlgorithm, CompressionLevel> WireCompressionManager::negotiate_compression(
        const std::vector<CompressionAlgorithm>& client_algorithms,
        const std::vector<CompressionLevel>& client_levels)
    {

        // Find best matching algorithm
        CompressionAlgorithm selected_algorithm = CompressionAlgorithm::NONE;

        // Prefer zlib if both support it
        if (std::find(client_algorithms.begin(), client_algorithms.end(),
                      CompressionAlgorithm::ZLIB) != client_algorithms.end()) {
            selected_algorithm = CompressionAlgorithm::ZLIB;
        }

        // Find best matching level
        CompressionLevel selected_level = CompressionLevel::NONE;
        if (selected_algorithm != CompressionAlgorithm::NONE) {
            // Use highest common level
            if (std::find(client_levels.begin(), client_levels.end(), CompressionLevel::MAXIMUM) !=
                client_levels.end()) {
                selected_level = CompressionLevel::MAXIMUM;
            } else if (std::find(client_levels.begin(), client_levels.end(),
                                 CompressionLevel::BALANCED) != client_levels.end()) {
                selected_level = CompressionLevel::BALANCED;
            } else if (std::find(client_levels.begin(), client_levels.end(),
                                 CompressionLevel::FAST) != client_levels.end()) {
                selected_level = CompressionLevel::FAST;
            }
        }

        return {selected_algorithm, selected_level};
    }

    bool WireCompressionManager::set_compression_parameters(CompressionAlgorithm algorithm,
                                                            CompressionLevel level)
    {
        negotiated_algorithm_ = algorithm;
        negotiated_level_ = level;
        compression_active_ = (algorithm != CompressionAlgorithm::NONE);

        // Update compressor configuration
        auto config = compressor_->get_config();
        config.algorithm = algorithm;
        config.level = level;
        return compressor_->update_config(config);
    }

    bool WireCompressionManager::is_compression_active() const
    {
        return initialized_ && compression_active_;
    }

    std::vector<std::uint8_t>
    WireCompressionManager::compress_message(const std::vector<std::uint8_t>& message)
    {
        if (!is_compression_active() || !compressor_->should_compress(message.size())) {
            return message; // Return uncompressed
        }

        auto compressed = compressor_->compress(message);
        if (compressed.empty()) {
            return message; // Return original on compression failure
        }

        // Update connection stats
        connection_stats_.compress_operations.fetch_add(1);
        connection_stats_.bytes_uncompressed.fetch_add(message.size());
        connection_stats_.bytes_compressed.fetch_add(compressed.size());

        return compressed;
    }

    std::vector<std::uint8_t>
    WireCompressionManager::decompress_message(const std::vector<std::uint8_t>& compressed_message,
                                               std::size_t expected_size)
    {
        if (!is_compression_active()) {
            return compressed_message; // Return as-is if compression not active
        }

        auto decompressed = compressor_->decompress(compressed_message, expected_size);
        if (decompressed.empty()) {
            return compressed_message; // Return original on decompression failure
        }

        // Update connection stats
        connection_stats_.decompress_operations.fetch_add(1);

        return decompressed;
    }

    CompressionStatistics WireCompressionManager::get_connection_statistics() const
    {
        return connection_stats_.snapshot();
    }

    void WireCompressionManager::reset_connection_statistics()
    {
        connection_stats_.reset();
    }

    // Global utility functions

    CompressionConfig get_default_wire_compression_config()
    {
        CompressionConfig config;
        config.algorithm = CompressionAlgorithm::ZLIB;
        config.level = CompressionLevel::BALANCED;
        config.adaptive_level = true;
        config.min_compress_size = 128;        // 128 bytes minimum
        config.cpu_threshold = 0.75;           // 75% CPU threshold
        config.max_compress_time_ms = 3.0;     // 3ms max compression time
        config.target_compression_ratio = 0.6; // 60% target ratio
        config.buffer_size = 64 * 1024;        // 64KB buffers
        config.enable_statistics = true;
        return config;
    }

    std::vector<CompressionBenchmark>
    benchmark_compression(const std::vector<std::uint8_t>& test_data,
                          const std::vector<CompressionAlgorithm>& algorithms,
                          const std::vector<CompressionLevel>& levels)
    {

        std::vector<CompressionBenchmark> results;

        for (auto algorithm : algorithms) {
            for (auto level : levels) {
                if (algorithm == CompressionAlgorithm::NONE && level != CompressionLevel::NONE) {
                    continue; // Skip invalid combinations
                }

                CompressionConfig config;
                config.algorithm = algorithm;
                config.level = level;
                config.adaptive_level = false;

                WireCompression compressor(config);

                // Warmup
                compressor.compress(test_data);

                // Benchmark compression
                auto start = std::chrono::high_resolution_clock::now();
                auto compressed = compressor.compress(test_data);
                auto end = std::chrono::high_resolution_clock::now();

                auto compress_duration = std::chrono::duration<double, std::milli>(end - start);

                if (compressed.empty()) {
                    continue; // Skip failed compressions
                }

                // Benchmark decompression
                start = std::chrono::high_resolution_clock::now();
                auto decompressed = compressor.decompress(compressed, test_data.size());
                end = std::chrono::high_resolution_clock::now();

                auto decompress_duration = std::chrono::duration<double, std::milli>(end - start);

                if (decompressed != test_data) {
                    continue; // Skip if data doesn't match
                }

                CompressionBenchmark benchmark;
                benchmark.algorithm = algorithm;
                benchmark.level = level;
                benchmark.compression_ratio =
                    static_cast<double>(compressed.size()) / test_data.size();
                benchmark.compress_time_ms = compress_duration.count();
                benchmark.decompress_time_ms = decompress_duration.count();

                // Calculate throughput (MB/s)
                double total_time_s =
                    (compress_duration.count() + decompress_duration.count()) / 1000.0;
                double data_mb = test_data.size() / (1024.0 * 1024.0);
                benchmark.throughput_mbps = total_time_s > 0 ? data_mb / total_time_s : 0.0;

                results.push_back(benchmark);
            }
        }

        return results;
    }

    bool is_data_compressible(const std::uint8_t* data, std::size_t size)
    {
        if (!data || size < 64) {
            return false; // Too small or invalid
        }

        // Simple entropy check - count unique bytes
        std::array<bool, 256> seen = {};
        std::size_t unique_bytes = 0;
        std::size_t sample_size = std::min(size, static_cast<std::size_t>(1024));

        for (std::size_t i = 0; i < sample_size; ++i) {
            if (!seen[data[i]]) {
                seen[data[i]] = true;
                unique_bytes++;
            }
        }

        // If less than 50% unique bytes in sample, data is likely compressible
        return (static_cast<double>(unique_bytes) / 256.0) < 0.5;
    }

    double estimate_compression_ratio(const std::uint8_t* data, std::size_t size)
    {
        if (!is_data_compressible(data, size)) {
            return 0.95; // Assume minimal compression for incompressible data
        }

        // Simple estimate based on repeated byte patterns
        std::array<std::size_t, 256> byte_counts = {};
        std::size_t sample_size = std::min(size, static_cast<std::size_t>(4096));

        for (std::size_t i = 0; i < sample_size; ++i) {
            byte_counts[data[i]]++;
        }

        // Calculate simple entropy estimate
        double entropy = 0.0;
        for (auto count : byte_counts) {
            if (count > 0) {
                double probability = static_cast<double>(count) / sample_size;
                entropy -= probability * std::log2(probability);
            }
        }

        // Estimate compression ratio based on entropy
        // Lower entropy = better compression
        double max_entropy = 8.0; // Maximum entropy for 8-bit data
        double compression_estimate = 0.3 + (entropy / max_entropy) * 0.6;

        return std::clamp(compression_estimate, 0.1, 0.95);
    }

} // namespace scratchbird::engine
