// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace scratchbird::engine
{

    /// Compression levels for wire protocol
    enum class CompressionLevel : std::uint8_t {
        NONE = 0,     ///< No compression
        FAST = 1,     ///< Fast compression (zlib level 1)
        BALANCED = 6, ///< Balanced compression (zlib level 6)
        MAXIMUM = 9   ///< Maximum compression (zlib level 9)
    };

    /// Compression algorithm types
    enum class CompressionAlgorithm : std::uint8_t {
        NONE = 0, ///< No compression
        ZLIB = 1, ///< zlib/deflate (Firebird-compatible)
        LZ4 = 2,  ///< LZ4 fast compression (future extension)
        ZSTD = 3  ///< Zstandard (future extension)
    };

    /// Compression statistics snapshot (non-atomic for returning)
    struct CompressionStatistics {
        /// Compression operations
        std::uint64_t compress_operations{0};   ///< Total compression operations
        std::uint64_t decompress_operations{0}; ///< Total decompression operations

        /// Data throughput
        std::uint64_t bytes_uncompressed{0}; ///< Total uncompressed bytes
        std::uint64_t bytes_compressed{0};   ///< Total compressed bytes

        /// Performance metrics
        std::uint64_t compress_time_us{0};   ///< Total compression time (microseconds)
        std::uint64_t decompress_time_us{0}; ///< Total decompression time (microseconds)

        /// Error tracking
        std::uint64_t compress_errors{0};   ///< Compression failures
        std::uint64_t decompress_errors{0}; ///< Decompression failures

        /// Efficiency metrics
        double get_compression_ratio() const
        {
            return bytes_uncompressed > 0
                       ? static_cast<double>(bytes_compressed) / bytes_uncompressed
                       : 1.0;
        }

        double get_bandwidth_savings() const
        {
            return 1.0 - get_compression_ratio();
        }

        double get_avg_compress_time_ms() const
        {
            return compress_operations > 0
                       ? static_cast<double>(compress_time_us) / (compress_operations * 1000.0)
                       : 0.0;
        }

        double get_avg_decompress_time_ms() const
        {
            return decompress_operations > 0
                       ? static_cast<double>(decompress_time_us) / (decompress_operations * 1000.0)
                       : 0.0;
        }
    };

    /// Internal atomic statistics (for thread-safe updates)
    struct AtomicCompressionStatistics {
        /// Compression operations
        std::atomic<std::uint64_t> compress_operations{0};   ///< Total compression operations
        std::atomic<std::uint64_t> decompress_operations{0}; ///< Total decompression operations

        /// Data throughput
        std::atomic<std::uint64_t> bytes_uncompressed{0}; ///< Total uncompressed bytes
        std::atomic<std::uint64_t> bytes_compressed{0};   ///< Total compressed bytes

        /// Performance metrics
        std::atomic<std::uint64_t> compress_time_us{0}; ///< Total compression time (microseconds)
        std::atomic<std::uint64_t> decompress_time_us{
            0}; ///< Total decompression time (microseconds)

        /// Error tracking
        std::atomic<std::uint64_t> compress_errors{0};   ///< Compression failures
        std::atomic<std::uint64_t> decompress_errors{0}; ///< Decompression failures

        void reset()
        {
            compress_operations.store(0);
            decompress_operations.store(0);
            bytes_uncompressed.store(0);
            bytes_compressed.store(0);
            compress_time_us.store(0);
            decompress_time_us.store(0);
            compress_errors.store(0);
            decompress_errors.store(0);
        }

        /// Create a snapshot of the statistics
        CompressionStatistics snapshot() const
        {
            CompressionStatistics result;
            result.compress_operations = compress_operations.load();
            result.decompress_operations = decompress_operations.load();
            result.bytes_uncompressed = bytes_uncompressed.load();
            result.bytes_compressed = bytes_compressed.load();
            result.compress_time_us = compress_time_us.load();
            result.decompress_time_us = decompress_time_us.load();
            result.compress_errors = compress_errors.load();
            result.decompress_errors = decompress_errors.load();
            return result;
        }
    };

    /// Configuration for wire protocol compression
    struct CompressionConfig {
        /// Compression settings
        CompressionAlgorithm algorithm{CompressionAlgorithm::ZLIB};
        CompressionLevel level{CompressionLevel::BALANCED};

        /// Adaptive compression
        bool adaptive_level{true};           ///< Auto-adjust compression level
        std::uint32_t min_compress_size{64}; ///< Minimum size to attempt compression
        double cpu_threshold{0.8};           ///< CPU usage threshold for adaptive mode

        /// Performance thresholds
        double max_compress_time_ms{5.0};     ///< Maximum compression time threshold
        double target_compression_ratio{0.7}; ///< Target compression ratio

        /// Buffer management
        std::uint32_t buffer_size{64 * 1024};             ///< Compression buffer size (64KB)
        std::uint32_t max_message_size{16 * 1024 * 1024}; ///< Maximum message size (16MB)

        /// Quality of service
        bool enable_statistics{true};          ///< Enable compression statistics collection
        std::uint32_t stats_window_size{1000}; ///< Statistics window size for averages

        /// Validation
        bool is_valid() const
        {
            return min_compress_size > 0 && cpu_threshold > 0.0 && cpu_threshold <= 1.0 &&
                   max_compress_time_ms > 0.0 && target_compression_ratio > 0.0 &&
                   target_compression_ratio < 1.0 && buffer_size >= 1024 && // Minimum 1KB buffer
                   max_message_size >= buffer_size && stats_window_size > 0;
        }
    };

    /// Wire protocol compression engine
    class WireCompression
    {
      public:
        /// Constructor
        explicit WireCompression(const CompressionConfig& config = CompressionConfig{});

        /// Destructor
        ~WireCompression();

        /// Non-copyable, moveable
        WireCompression(const WireCompression&) = delete;
        WireCompression& operator=(const WireCompression&) = delete;
        WireCompression(WireCompression&&) = default;
        WireCompression& operator=(WireCompression&&) = default;

        /// Compression operations

        /// Compress data buffer
        /// @param input Input data to compress
        /// @param input_size Size of input data
        /// @param output Output buffer for compressed data
        /// @param output_size Size of output buffer (in), actual compressed size (out)
        /// @return true if compression successful
        bool compress(const std::uint8_t* input, std::size_t input_size, std::uint8_t* output,
                      std::size_t& output_size);

        /// Decompress data buffer
        /// @param input Compressed input data
        /// @param input_size Size of compressed data
        /// @param output Output buffer for decompressed data
        /// @param output_size Size of output buffer (in), actual decompressed size (out)
        /// @return true if decompression successful
        bool decompress(const std::uint8_t* input, std::size_t input_size, std::uint8_t* output,
                        std::size_t& output_size);

        /// Compress vector data (convenience method)
        std::vector<std::uint8_t> compress(const std::vector<std::uint8_t>& input);

        /// Decompress vector data (convenience method)
        std::vector<std::uint8_t> decompress(const std::vector<std::uint8_t>& input,
                                             std::size_t expected_size = 0);

        /// Compression decision logic

        /// Should we compress this data?
        /// @param data_size Size of data to potentially compress
        /// @param data_type Hint about data type (optional)
        /// @return true if compression is recommended
        bool should_compress(std::size_t data_size, const std::string& data_type = "") const;

        /// Get recommended compression level
        CompressionLevel get_recommended_level() const;

        /// Configuration management

        /// Update configuration (thread-safe)
        bool update_config(const CompressionConfig& config);

        /// Get current configuration
        CompressionConfig get_config() const;

        /// Statistics and monitoring

        /// Get compression statistics
        CompressionStatistics get_statistics() const;

        /// Reset statistics
        void reset_statistics();

        /// Get compression efficiency report
        std::string generate_efficiency_report() const;

        /// Adaptive compression logic

        /// Update adaptive compression based on performance
        void update_adaptive_settings();

        /// Check if adaptive mode should adjust compression level
        bool needs_level_adjustment() const;

        /// Utility methods

        /// Get maximum compressed size for input
        std::size_t get_max_compressed_size(std::size_t input_size) const;

        /// Test compression engine functionality
        bool self_test();

        /// Get algorithm name as string
        std::string get_algorithm_name() const;

        /// Get level name as string
        std::string get_level_name() const;

      private:
        /// Configuration
        CompressionConfig config_;
        mutable std::mutex config_mutex_;

        /// Statistics
        mutable AtomicCompressionStatistics stats_;

        /// Adaptive compression state
        CompressionLevel current_level_;
        std::chrono::steady_clock::time_point last_adjustment_;
        std::array<double, 10> recent_ratios_{}; ///< Recent compression ratios for averaging
        std::size_t ratio_index_{0};

        /// Thread safety
        mutable std::mutex compression_mutex_;

        /// Private helper methods

        /// Initialize compression library
        bool initialize_zlib();

        /// Cleanup compression library
        void cleanup_zlib();

        /// Compress using zlib
        bool compress_zlib(const std::uint8_t* input, std::size_t input_size, std::uint8_t* output,
                           std::size_t& output_size);

        /// Decompress using zlib
        bool decompress_zlib(const std::uint8_t* input, std::size_t input_size,
                             std::uint8_t* output, std::size_t& output_size);

        /// Update statistics after compression
        void update_compress_stats(std::size_t input_size, std::size_t output_size,
                                   std::chrono::microseconds duration);

        /// Update statistics after decompression
        void update_decompress_stats(std::size_t input_size, std::size_t output_size,
                                     std::chrono::microseconds duration);

        /// Calculate moving average of compression ratios
        double get_average_compression_ratio() const;

        /// Check CPU usage for adaptive compression
        double get_current_cpu_usage() const;

        /// Validate algorithm support
        bool is_algorithm_supported(CompressionAlgorithm algorithm) const;
    };

    /// Wire Protocol Compression Manager - per-connection compression state
    class WireCompressionManager
    {
      public:
        /// Constructor
        explicit WireCompressionManager(const CompressionConfig& config = CompressionConfig{});

        /// Destructor
        ~WireCompressionManager() = default;

        /// Non-copyable, moveable
        WireCompressionManager(const WireCompressionManager&) = delete;
        WireCompressionManager& operator=(const WireCompressionManager&) = delete;
        WireCompressionManager(WireCompressionManager&&) = default;
        WireCompressionManager& operator=(WireCompressionManager&&) = default;

        /// Connection lifecycle

        /// Initialize compression for connection
        bool initialize(std::uint32_t connection_id);

        /// Shutdown compression for connection
        void shutdown();

        /// Protocol negotiation

        /// Negotiate compression with client
        /// @param client_algorithms Algorithms supported by client
        /// @param client_levels Compression levels supported by client
        /// @return Selected algorithm and level
        std::pair<CompressionAlgorithm, CompressionLevel>
        negotiate_compression(const std::vector<CompressionAlgorithm>& client_algorithms,
                              const std::vector<CompressionLevel>& client_levels);

        /// Set negotiated compression parameters
        bool set_compression_parameters(CompressionAlgorithm algorithm, CompressionLevel level);

        /// Check if compression is negotiated and active
        bool is_compression_active() const;

        /// Message compression

        /// Compress outgoing message
        std::vector<std::uint8_t> compress_message(const std::vector<std::uint8_t>& message);

        /// Decompress incoming message
        std::vector<std::uint8_t>
        decompress_message(const std::vector<std::uint8_t>& compressed_message,
                           std::size_t expected_size = 0);

        /// Statistics and monitoring

        /// Get per-connection statistics
        CompressionStatistics get_connection_statistics() const;

        /// Reset connection statistics
        void reset_connection_statistics();

        /// Connection ID
        std::uint32_t get_connection_id() const
        {
            return connection_id_;
        }

      private:
        /// Connection state
        std::uint32_t connection_id_{0};
        bool initialized_{false};
        bool compression_active_{false};

        /// Negotiated parameters
        CompressionAlgorithm negotiated_algorithm_{CompressionAlgorithm::NONE};
        CompressionLevel negotiated_level_{CompressionLevel::NONE};

        /// Compression engine
        std::unique_ptr<WireCompression> compressor_;

        /// Per-connection statistics
        AtomicCompressionStatistics connection_stats_;
    };

    /// Global compression utilities

    /// Get default compression configuration optimized for wire protocol
    CompressionConfig get_default_wire_compression_config();

    /// Test compression performance with sample data
    struct CompressionBenchmark {
        CompressionAlgorithm algorithm;
        CompressionLevel level;
        double compression_ratio;
        double compress_time_ms;
        double decompress_time_ms;
        double throughput_mbps;
    };

    /// Benchmark compression algorithms and levels
    std::vector<CompressionBenchmark> benchmark_compression(
        const std::vector<std::uint8_t>& test_data,
        const std::vector<CompressionAlgorithm>& algorithms = {CompressionAlgorithm::ZLIB},
        const std::vector<CompressionLevel>& levels = {
            CompressionLevel::FAST, CompressionLevel::BALANCED, CompressionLevel::MAXIMUM});

    /// Compression format detection
    bool is_data_compressible(const std::uint8_t* data, std::size_t size);

    /// Estimate compression ratio without actual compression
    double estimate_compression_ratio(const std::uint8_t* data, std::size_t size);

} // namespace scratchbird::engine
