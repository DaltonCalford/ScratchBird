#pragma once

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <functional>
#include <chrono>

namespace SBCompression {

// Compression algorithm types
enum class Algorithm {
    NONE = 0,
    GZIP = 1,
    LZ4 = 2,
    ZSTD = 3,
    BZIP2 = 4,
    DEFLATE = 5,
    SNAPPY = 6
};

// Compression levels
enum class Level {
    FASTEST = 1,
    FAST = 3,
    MEDIUM = 6,
    BEST = 9,
    ULTRA = 12  // For algorithms that support it (ZSTD)
};

// Compression statistics
struct CompressionStats {
    uint64_t original_size = 0;
    uint64_t compressed_size = 0;
    std::chrono::microseconds compression_time{0};
    std::chrono::microseconds decompression_time{0};
    Algorithm algorithm = Algorithm::NONE;
    int level = 0;
    
    double getCompressionRatio() const {
        return original_size > 0 ? static_cast<double>(compressed_size) / original_size : 0.0;
    }
    
    double getCompressionPercentage() const {
        return original_size > 0 ? (1.0 - getCompressionRatio()) * 100.0 : 0.0;
    }
    
    double getThroughputMBps() const {
        if (compression_time.count() == 0) return 0.0;
        double seconds = compression_time.count() / 1000000.0;
        double mb = original_size / (1024.0 * 1024.0);
        return mb / seconds;
    }
};

// Progress callback for long operations
using ProgressCallback = std::function<void(uint64_t processed, uint64_t total, const std::string& operation)>;

// Compression configuration
struct CompressionConfig {
    Algorithm algorithm = Algorithm::ZSTD;
    Level level = Level::MEDIUM;
    uint32_t buffer_size = 64 * 1024;  // 64KB default
    bool enable_streaming = true;
    bool enable_checksum = true;
    bool enable_dictionary = false;
    std::string dictionary_data;
    ProgressCallback progress_callback;
};

} // namespace SBCompression

// Main compression engine class
class SBCompressionEngine {
private:
    SBCompression::CompressionConfig config;
    std::map<SBCompression::Algorithm, std::string> algorithm_names;
    std::map<SBCompression::Algorithm, bool> algorithm_available;
    
    // Algorithm-specific implementations
    class GzipCompressor;
    class LZ4Compressor;
    class ZstdCompressor;
    class Bzip2Compressor;
    class DeflateCompressor;
    class SnappyCompressor;
    
    std::unique_ptr<GzipCompressor> gzip_compressor;
    std::unique_ptr<LZ4Compressor> lz4_compressor;
    std::unique_ptr<ZstdCompressor> zstd_compressor;
    std::unique_ptr<Bzip2Compressor> bzip2_compressor;
    std::unique_ptr<DeflateCompressor> deflate_compressor;
    std::unique_ptr<SnappyCompressor> snappy_compressor;

public:
    SBCompressionEngine();
    ~SBCompressionEngine();
    
    // Configuration
    void setConfig(const SBCompression::CompressionConfig& config);
    SBCompression::CompressionConfig getConfig() const;
    
    // Algorithm availability
    bool isAlgorithmAvailable(SBCompression::Algorithm algorithm) const;
    std::vector<SBCompression::Algorithm> getAvailableAlgorithms() const;
    std::string getAlgorithmName(SBCompression::Algorithm algorithm) const;
    
    // Compression operations
    bool compress(const std::vector<uint8_t>& input, 
                 std::vector<uint8_t>& output,
                 SBCompression::CompressionStats& stats);
    
    bool compress(const uint8_t* input_data, size_t input_size,
                 std::vector<uint8_t>& output,
                 SBCompression::CompressionStats& stats);
    
    bool compressStream(std::istream& input, std::ostream& output,
                       SBCompression::CompressionStats& stats);
    
    // Decompression operations
    bool decompress(const std::vector<uint8_t>& input,
                   std::vector<uint8_t>& output,
                   SBCompression::CompressionStats& stats);
    
    bool decompress(const uint8_t* input_data, size_t input_size,
                   std::vector<uint8_t>& output,
                   SBCompression::CompressionStats& stats);
    
    bool decompressStream(std::istream& input, std::ostream& output,
                         SBCompression::CompressionStats& stats);
    
    // File operations
    bool compressFile(const std::string& input_path, 
                     const std::string& output_path,
                     SBCompression::CompressionStats& stats);
    
    bool decompressFile(const std::string& input_path,
                       const std::string& output_path,
                       SBCompression::CompressionStats& stats);
    
    // Utility methods
    bool validateCompressedData(const std::vector<uint8_t>& data,
                               SBCompression::Algorithm expected_algorithm);
    
    SBCompression::Algorithm detectCompressionAlgorithm(const std::vector<uint8_t>& data);
    
    std::vector<SBCompression::CompressionStats> benchmarkAlgorithms(
        const std::vector<uint8_t>& test_data,
        const std::vector<SBCompression::Level>& levels);
    
    SBCompression::Algorithm recommendAlgorithm(const std::vector<uint8_t>& sample_data,
                                               bool prioritize_speed = false);
    
    // Dictionary management (for algorithms that support it)
    bool createDictionary(const std::vector<std::vector<uint8_t>>& training_data,
                         std::vector<uint8_t>& dictionary);
    
    bool setCompressionDictionary(const std::vector<uint8_t>& dictionary);
    bool clearCompressionDictionary();
    
    // Error handling
    std::string getLastError() const;
    std::vector<std::string> getErrorLog() const;
    void clearErrorLog();

private:
    // Error handling
    std::string last_error;
    std::vector<std::string> error_log;
    void logError(const std::string& operation, const std::string& error);
    
    // Initialization
    void initializeAlgorithms();
    void checkAlgorithmAvailability();
    
    // Algorithm-specific helpers
    bool compressWithGzip(const uint8_t* input, size_t input_size,
                         std::vector<uint8_t>& output,
                         int level, SBCompression::CompressionStats& stats);
    
    bool compressWithLZ4(const uint8_t* input, size_t input_size,
                        std::vector<uint8_t>& output,
                        int level, SBCompression::CompressionStats& stats);
    
    bool compressWithZstd(const uint8_t* input, size_t input_size,
                         std::vector<uint8_t>& output,
                         int level, SBCompression::CompressionStats& stats);
    
    bool compressWithBzip2(const uint8_t* input, size_t input_size,
                          std::vector<uint8_t>& output,
                          int level, SBCompression::CompressionStats& stats);
    
    bool decompressWithGzip(const uint8_t* input, size_t input_size,
                           std::vector<uint8_t>& output,
                           SBCompression::CompressionStats& stats);
    
    bool decompressWithLZ4(const uint8_t* input, size_t input_size,
                          std::vector<uint8_t>& output,
                          SBCompression::CompressionStats& stats);
    
    bool decompressWithZstd(const uint8_t* input, size_t input_size,
                           std::vector<uint8_t>& output,
                           SBCompression::CompressionStats& stats);
    
    bool decompressWithBzip2(const uint8_t* input, size_t input_size,
                            std::vector<uint8_t>& output,
                            SBCompression::CompressionStats& stats);
    
    // Utility helpers
    void updateProgress(uint64_t processed, uint64_t total, const std::string& operation);
    uint32_t calculateChecksum(const uint8_t* data, size_t size);
    bool verifyChecksum(const uint8_t* data, size_t size, uint32_t expected_checksum);
    
    // Magic number detection
    bool hasGzipMagic(const uint8_t* data, size_t size);
    bool hasLZ4Magic(const uint8_t* data, size_t size);
    bool hasZstdMagic(const uint8_t* data, size_t size);
    bool hasBzip2Magic(const uint8_t* data, size_t size);
};

// Compression utility functions
namespace SBCompression {

// Quick compression functions
std::vector<uint8_t> quickCompress(const std::vector<uint8_t>& data, 
                                  Algorithm algorithm = Algorithm::ZSTD,
                                  Level level = Level::MEDIUM);

std::vector<uint8_t> quickDecompress(const std::vector<uint8_t>& data);

bool quickCompressFile(const std::string& input_path, 
                      const std::string& output_path,
                      Algorithm algorithm = Algorithm::ZSTD,
                      Level level = Level::MEDIUM);

bool quickDecompressFile(const std::string& input_path,
                        const std::string& output_path);

// Algorithm information
std::string getAlgorithmDescription(Algorithm algorithm);
std::vector<Level> getSupportedLevels(Algorithm algorithm);
bool supportsStreaming(Algorithm algorithm);
bool supportsDictionaries(Algorithm algorithm);

// Performance helpers
Algorithm getBestSpeedAlgorithm();
Algorithm getBestRatioAlgorithm();
Algorithm getBestBalanceAlgorithm();

// Level conversion
int levelToInt(Level level, Algorithm algorithm);
Level intToLevel(int level_int);

} // namespace SBCompression