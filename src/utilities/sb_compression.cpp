#include "sb_compression.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <iomanip>

// Standard compression library includes
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef HAVE_LZ4
#include <lz4.h>
#include <lz4hc.h>
#endif

#ifdef HAVE_ZSTD
#include <zstd.h>
#endif

#ifdef HAVE_BZIP2
#include <bzlib.h>
#endif

using namespace SBCompression;

// Constructor
SBCompressionEngine::SBCompressionEngine() {
    // Initialize algorithm names
    algorithm_names[Algorithm::NONE] = "None";
    algorithm_names[Algorithm::GZIP] = "GZIP";
    algorithm_names[Algorithm::LZ4] = "LZ4";
    algorithm_names[Algorithm::ZSTD] = "ZSTD";
    algorithm_names[Algorithm::BZIP2] = "BZIP2";
    algorithm_names[Algorithm::DEFLATE] = "DEFLATE";
    algorithm_names[Algorithm::SNAPPY] = "Snappy";
    
    // Set default configuration
    config.algorithm = Algorithm::ZSTD;
    config.level = Level::MEDIUM;
    config.buffer_size = 64 * 1024;
    config.enable_streaming = true;
    config.enable_checksum = true;
    
    // Initialize algorithms and check availability
    initializeAlgorithms();
    checkAlgorithmAvailability();
}

// Destructor
SBCompressionEngine::~SBCompressionEngine() {
    // Cleanup handled by unique_ptr destructors
}

// Configuration
void SBCompressionEngine::setConfig(const CompressionConfig& new_config) {
    config = new_config;
}

CompressionConfig SBCompressionEngine::getConfig() const {
    return config;
}

// Algorithm availability
bool SBCompressionEngine::isAlgorithmAvailable(Algorithm algorithm) const {
    auto it = algorithm_available.find(algorithm);
    return it != algorithm_available.end() && it->second;
}

std::vector<Algorithm> SBCompressionEngine::getAvailableAlgorithms() const {
    std::vector<Algorithm> available;
    for (const auto& pair : algorithm_available) {
        if (pair.second) {
            available.push_back(pair.first);
        }
    }
    return available;
}

std::string SBCompressionEngine::getAlgorithmName(Algorithm algorithm) const {
    auto it = algorithm_names.find(algorithm);
    return it != algorithm_names.end() ? it->second : "Unknown";
}

// Main compression function
bool SBCompressionEngine::compress(const std::vector<uint8_t>& input, 
                                  std::vector<uint8_t>& output,
                                  CompressionStats& stats) {
    return compress(input.data(), input.size(), output, stats);
}

bool SBCompressionEngine::compress(const uint8_t* input_data, size_t input_size,
                                  std::vector<uint8_t>& output,
                                  CompressionStats& stats) {
    try {
        // Initialize stats
        stats = CompressionStats();
        stats.original_size = input_size;
        stats.algorithm = config.algorithm;
        stats.level = static_cast<int>(config.level);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Handle no compression case
        if (config.algorithm == Algorithm::NONE) {
            output.assign(input_data, input_data + input_size);
            stats.compressed_size = input_size;
            auto end_time = std::chrono::high_resolution_clock::now();
            stats.compression_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            return true;
        }
        
        // Check algorithm availability
        if (!isAlgorithmAvailable(config.algorithm)) {
            logError("compress", "Algorithm " + getAlgorithmName(config.algorithm) + " is not available");
            return false;
        }
        
        bool success = false;
        int level = levelToInt(config.level, config.algorithm);
        
        // Dispatch to algorithm-specific implementation
        switch (config.algorithm) {
            case Algorithm::GZIP:
                success = compressWithGzip(input_data, input_size, output, level, stats);
                break;
            case Algorithm::LZ4:
                success = compressWithLZ4(input_data, input_size, output, level, stats);
                break;
            case Algorithm::ZSTD:
                success = compressWithZstd(input_data, input_size, output, level, stats);
                break;
            case Algorithm::BZIP2:
                success = compressWithBzip2(input_data, input_size, output, level, stats);
                break;
            default:
                logError("compress", "Unsupported compression algorithm");
                return false;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        stats.compression_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        stats.compressed_size = output.size();
        
        return success;
        
    } catch (const std::exception& e) {
        logError("compress", "Exception: " + std::string(e.what()));
        return false;
    }
}

// Main decompression function
bool SBCompressionEngine::decompress(const std::vector<uint8_t>& input,
                                    std::vector<uint8_t>& output,
                                    CompressionStats& stats) {
    return decompress(input.data(), input.size(), output, stats);
}

bool SBCompressionEngine::decompress(const uint8_t* input_data, size_t input_size,
                                    std::vector<uint8_t>& output,
                                    CompressionStats& stats) {
    try {
        // Initialize stats
        stats = CompressionStats();
        stats.compressed_size = input_size;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Detect compression algorithm
        Algorithm detected_algorithm = detectCompressionAlgorithm(
            std::vector<uint8_t>(input_data, input_data + std::min(input_size, size_t(64)))
        );
        
        if (detected_algorithm == Algorithm::NONE) {
            // Assume uncompressed data
            output.assign(input_data, input_data + input_size);
            stats.original_size = input_size;
            auto end_time = std::chrono::high_resolution_clock::now();
            stats.decompression_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            return true;
        }
        
        stats.algorithm = detected_algorithm;
        
        // Check algorithm availability
        if (!isAlgorithmAvailable(detected_algorithm)) {
            logError("decompress", "Algorithm " + getAlgorithmName(detected_algorithm) + " is not available");
            return false;
        }
        
        bool success = false;
        
        // Dispatch to algorithm-specific implementation
        switch (detected_algorithm) {
            case Algorithm::GZIP:
                success = decompressWithGzip(input_data, input_size, output, stats);
                break;
            case Algorithm::LZ4:
                success = decompressWithLZ4(input_data, input_size, output, stats);
                break;
            case Algorithm::ZSTD:
                success = decompressWithZstd(input_data, input_size, output, stats);
                break;
            case Algorithm::BZIP2:
                success = decompressWithBzip2(input_data, input_size, output, stats);
                break;
            default:
                logError("decompress", "Unsupported decompression algorithm");
                return false;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        stats.decompression_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        stats.original_size = output.size();
        
        return success;
        
    } catch (const std::exception& e) {
        logError("decompress", "Exception: " + std::string(e.what()));
        return false;
    }
}

// File compression
bool SBCompressionEngine::compressFile(const std::string& input_path, 
                                      const std::string& output_path,
                                      CompressionStats& stats) {
    try {
        std::ifstream input_file(input_path, std::ios::binary);
        if (!input_file.is_open()) {
            logError("compressFile", "Cannot open input file: " + input_path);
            return false;
        }
        
        std::ofstream output_file(output_path, std::ios::binary);
        if (!output_file.is_open()) {
            logError("compressFile", "Cannot create output file: " + output_path);
            return false;
        }
        
        return compressStream(input_file, output_file, stats);
        
    } catch (const std::exception& e) {
        logError("compressFile", "Exception: " + std::string(e.what()));
        return false;
    }
}

bool SBCompressionEngine::decompressFile(const std::string& input_path,
                                         const std::string& output_path,
                                         CompressionStats& stats) {
    try {
        std::ifstream input_file(input_path, std::ios::binary);
        if (!input_file.is_open()) {
            logError("decompressFile", "Cannot open input file: " + input_path);
            return false;
        }
        
        std::ofstream output_file(output_path, std::ios::binary);
        if (!output_file.is_open()) {
            logError("decompressFile", "Cannot create output file: " + output_path);
            return false;
        }
        
        return decompressStream(input_file, output_file, stats);
        
    } catch (const std::exception& e) {
        logError("decompressFile", "Exception: " + std::string(e.what()));
        return false;
    }
}

// Stream compression (placeholder implementation)
bool SBCompressionEngine::compressStream(std::istream& input, std::ostream& output,
                                         CompressionStats& stats) {
    try {
        // Read entire stream into memory for now
        // Real implementation would use streaming compression
        std::vector<uint8_t> input_data;
        input.seekg(0, std::ios::end);
        size_t size = input.tellg();
        input.seekg(0, std::ios::beg);
        
        input_data.resize(size);
        input.read(reinterpret_cast<char*>(input_data.data()), size);
        
        std::vector<uint8_t> output_data;
        bool success = compress(input_data, output_data, stats);
        
        if (success) {
            output.write(reinterpret_cast<const char*>(output_data.data()), output_data.size());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logError("compressStream", "Exception: " + std::string(e.what()));
        return false;
    }
}

bool SBCompressionEngine::decompressStream(std::istream& input, std::ostream& output,
                                          CompressionStats& stats) {
    try {
        // Read entire stream into memory for now
        // Real implementation would use streaming decompression
        std::vector<uint8_t> input_data;
        input.seekg(0, std::ios::end);
        size_t size = input.tellg();
        input.seekg(0, std::ios::beg);
        
        input_data.resize(size);
        input.read(reinterpret_cast<char*>(input_data.data()), size);
        
        std::vector<uint8_t> output_data;
        bool success = decompress(input_data, output_data, stats);
        
        if (success) {
            output.write(reinterpret_cast<const char*>(output_data.data()), output_data.size());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logError("decompressStream", "Exception: " + std::string(e.what()));
        return false;
    }
}

// Algorithm detection
Algorithm SBCompressionEngine::detectCompressionAlgorithm(const std::vector<uint8_t>& data) {
    if (data.size() < 4) {
        return Algorithm::NONE;
    }
    
    const uint8_t* bytes = data.data();
    
    // GZIP magic: 1f 8b
    if (hasGzipMagic(bytes, data.size())) {
        return Algorithm::GZIP;
    }
    
    // LZ4 magic: varies by frame format
    if (hasLZ4Magic(bytes, data.size())) {
        return Algorithm::LZ4;
    }
    
    // ZSTD magic: 28 b5 2f fd
    if (hasZstdMagic(bytes, data.size())) {
        return Algorithm::ZSTD;
    }
    
    // BZIP2 magic: 42 5a 68
    if (hasBzip2Magic(bytes, data.size())) {
        return Algorithm::BZIP2;
    }
    
    return Algorithm::NONE;
}

// Private helper methods

void SBCompressionEngine::initializeAlgorithms() {
    try {
        // Initialize algorithm-specific compressors
        // These would be created only if the libraries are available
        
#ifdef HAVE_ZLIB
        // GZIP compressor initialization would go here
#endif

#ifdef HAVE_LZ4
        // LZ4 compressor initialization would go here
#endif

#ifdef HAVE_ZSTD
        // ZSTD compressor initialization would go here
#endif

#ifdef HAVE_BZIP2
        // BZIP2 compressor initialization would go here
#endif
        
    } catch (const std::exception& e) {
        logError("initializeAlgorithms", "Exception: " + std::string(e.what()));
    }
}

void SBCompressionEngine::checkAlgorithmAvailability() {
    // Check which algorithms are available at compile time
    algorithm_available[Algorithm::NONE] = true;
    
#ifdef HAVE_ZLIB
    algorithm_available[Algorithm::GZIP] = true;
    algorithm_available[Algorithm::DEFLATE] = true;
#else
    algorithm_available[Algorithm::GZIP] = false;
    algorithm_available[Algorithm::DEFLATE] = false;
#endif

#ifdef HAVE_LZ4
    algorithm_available[Algorithm::LZ4] = true;
#else
    algorithm_available[Algorithm::LZ4] = false;
#endif

#ifdef HAVE_ZSTD
    algorithm_available[Algorithm::ZSTD] = true;
#else
    algorithm_available[Algorithm::ZSTD] = false;
#endif

#ifdef HAVE_BZIP2
    algorithm_available[Algorithm::BZIP2] = true;
#else
    algorithm_available[Algorithm::BZIP2] = false;
#endif

    // Snappy support would be checked here
    algorithm_available[Algorithm::SNAPPY] = false;
}

// Algorithm-specific compression implementations

bool SBCompressionEngine::compressWithGzip(const uint8_t* input, size_t input_size,
                                           std::vector<uint8_t>& output,
                                           int level, CompressionStats& stats) {
#ifdef HAVE_ZLIB
    try {
        // Estimate output size (input_size + 12 bytes header + 8 bytes footer + 0.1% extra)
        size_t max_output_size = input_size + 20 + (input_size / 1000);
        output.resize(max_output_size);
        
        z_stream stream;
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        stream.avail_in = input_size;
        stream.next_in = const_cast<uint8_t*>(input);
        stream.avail_out = max_output_size;
        stream.next_out = output.data();
        
        // Initialize deflate with gzip wrapper
        int ret = deflateInit2(&stream, level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
        if (ret != Z_OK) {
            logError("compressWithGzip", "deflateInit2 failed: " + std::to_string(ret));
            return false;
        }
        
        // Compress data
        ret = deflate(&stream, Z_FINISH);
        if (ret != Z_STREAM_END) {
            deflateEnd(&stream);
            logError("compressWithGzip", "deflate failed: " + std::to_string(ret));
            return false;
        }
        
        // Cleanup
        size_t compressed_size = max_output_size - stream.avail_out;
        deflateEnd(&stream);
        
        output.resize(compressed_size);
        return true;
        
    } catch (const std::exception& e) {
        logError("compressWithGzip", "Exception: " + std::string(e.what()));
        return false;
    }
#else
    logError("compressWithGzip", "GZIP support not compiled in");
    return false;
#endif
}

bool SBCompressionEngine::compressWithLZ4(const uint8_t* input, size_t input_size,
                                          std::vector<uint8_t>& output,
                                          int level, CompressionStats& stats) {
#ifdef HAVE_LZ4
    try {
        // Estimate maximum output size
        int max_output_size = LZ4_compressBound(static_cast<int>(input_size));
        output.resize(max_output_size);
        
        int compressed_size;
        if (level <= 3) {
            // Use fast compression
            compressed_size = LZ4_compress_default(
                reinterpret_cast<const char*>(input),
                reinterpret_cast<char*>(output.data()),
                static_cast<int>(input_size),
                max_output_size
            );
        } else {
            // Use high compression
            compressed_size = LZ4_compress_HC(
                reinterpret_cast<const char*>(input),
                reinterpret_cast<char*>(output.data()),
                static_cast<int>(input_size),
                max_output_size,
                level
            );
        }
        
        if (compressed_size <= 0) {
            logError("compressWithLZ4", "LZ4 compression failed");
            return false;
        }
        
        output.resize(compressed_size);
        return true;
        
    } catch (const std::exception& e) {
        logError("compressWithLZ4", "Exception: " + std::string(e.what()));
        return false;
    }
#else
    logError("compressWithLZ4", "LZ4 support not compiled in");
    return false;
#endif
}

bool SBCompressionEngine::compressWithZstd(const uint8_t* input, size_t input_size,
                                           std::vector<uint8_t>& output,
                                           int level, CompressionStats& stats) {
#ifdef HAVE_ZSTD
    try {
        // Estimate maximum output size
        size_t max_output_size = ZSTD_compressBound(input_size);
        output.resize(max_output_size);
        
        size_t compressed_size = ZSTD_compress(
            output.data(), max_output_size,
            input, input_size,
            level
        );
        
        if (ZSTD_isError(compressed_size)) {
            logError("compressWithZstd", "ZSTD compression failed: " + std::string(ZSTD_getErrorName(compressed_size)));
            return false;
        }
        
        output.resize(compressed_size);
        return true;
        
    } catch (const std::exception& e) {
        logError("compressWithZstd", "Exception: " + std::string(e.what()));
        return false;
    }
#else
    logError("compressWithZstd", "ZSTD support not compiled in");
    return false;
#endif
}

bool SBCompressionEngine::compressWithBzip2(const uint8_t* input, size_t input_size,
                                            std::vector<uint8_t>& output,
                                            int level, CompressionStats& stats) {
#ifdef HAVE_BZIP2
    try {
        // Estimate output size (slightly larger than input for worst case)
        size_t max_output_size = input_size + (input_size / 100) + 600;
        output.resize(max_output_size);
        
        unsigned int compressed_size = static_cast<unsigned int>(max_output_size);
        int ret = BZ2_bzBuffToBuffCompress(
            reinterpret_cast<char*>(output.data()), &compressed_size,
            const_cast<char*>(reinterpret_cast<const char*>(input)), static_cast<unsigned int>(input_size),
            level, 0, 30
        );
        
        if (ret != BZ_OK) {
            logError("compressWithBzip2", "BZIP2 compression failed: " + std::to_string(ret));
            return false;
        }
        
        output.resize(compressed_size);
        return true;
        
    } catch (const std::exception& e) {
        logError("compressWithBzip2", "Exception: " + std::string(e.what()));
        return false;
    }
#else
    logError("compressWithBzip2", "BZIP2 support not compiled in");
    return false;
#endif
}

// Algorithm-specific decompression implementations

bool SBCompressionEngine::decompressWithGzip(const uint8_t* input, size_t input_size,
                                             std::vector<uint8_t>& output,
                                             CompressionStats& stats) {
#ifdef HAVE_ZLIB
    try {
        // Start with estimated output size
        size_t output_size = input_size * 4; // Initial guess
        output.resize(output_size);
        
        z_stream stream;
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        stream.avail_in = input_size;
        stream.next_in = const_cast<uint8_t*>(input);
        stream.avail_out = output_size;
        stream.next_out = output.data();
        
        // Initialize inflate with gzip wrapper
        int ret = inflateInit2(&stream, 15 + 16);
        if (ret != Z_OK) {
            logError("decompressWithGzip", "inflateInit2 failed: " + std::to_string(ret));
            return false;
        }
        
        // Decompress data
        do {
            ret = inflate(&stream, Z_NO_FLUSH);
            if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                inflateEnd(&stream);
                logError("decompressWithGzip", "inflate failed: " + std::to_string(ret));
                return false;
            }
            
            if (stream.avail_out == 0 && ret != Z_STREAM_END) {
                // Need more output space
                size_t current_size = output.size();
                output.resize(current_size * 2);
                stream.next_out = output.data() + current_size;
                stream.avail_out = current_size;
            }
        } while (ret != Z_STREAM_END);
        
        // Cleanup
        size_t decompressed_size = output.size() - stream.avail_out;
        inflateEnd(&stream);
        
        output.resize(decompressed_size);
        return true;
        
    } catch (const std::exception& e) {
        logError("decompressWithGzip", "Exception: " + std::string(e.what()));
        return false;
    }
#else
    logError("decompressWithGzip", "GZIP support not compiled in");
    return false;
#endif
}

bool SBCompressionEngine::decompressWithLZ4(const uint8_t* input, size_t input_size,
                                            std::vector<uint8_t>& output,
                                            CompressionStats& stats) {
#ifdef HAVE_LZ4
    try {
        // LZ4 doesn't store the original size in the stream, so we need to estimate
        // For backup data, we typically see 2-10x compression ratios
        size_t estimated_size = input_size * 8;
        output.resize(estimated_size);
        
        int decompressed_size = LZ4_decompress_safe(
            reinterpret_cast<const char*>(input),
            reinterpret_cast<char*>(output.data()),
            static_cast<int>(input_size),
            static_cast<int>(estimated_size)
        );
        
        if (decompressed_size < 0) {
            // Try with larger buffer
            estimated_size = input_size * 20;
            output.resize(estimated_size);
            
            decompressed_size = LZ4_decompress_safe(
                reinterpret_cast<const char*>(input),
                reinterpret_cast<char*>(output.data()),
                static_cast<int>(input_size),
                static_cast<int>(estimated_size)
            );
            
            if (decompressed_size < 0) {
                logError("decompressWithLZ4", "LZ4 decompression failed");
                return false;
            }
        }
        
        output.resize(decompressed_size);
        return true;
        
    } catch (const std::exception& e) {
        logError("decompressWithLZ4", "Exception: " + std::string(e.what()));
        return false;
    }
#else
    logError("decompressWithLZ4", "LZ4 support not compiled in");
    return false;
#endif
}

bool SBCompressionEngine::decompressWithZstd(const uint8_t* input, size_t input_size,
                                             std::vector<uint8_t>& output,
                                             CompressionStats& stats) {
#ifdef HAVE_ZSTD
    try {
        // Get the original size from ZSTD frame
        unsigned long long original_size = ZSTD_getFrameContentSize(input, input_size);
        
        if (original_size == ZSTD_CONTENTSIZE_ERROR) {
            logError("decompressWithZstd", "Invalid ZSTD frame");
            return false;
        }
        
        if (original_size == ZSTD_CONTENTSIZE_UNKNOWN) {
            // Estimate size if not available in frame
            original_size = input_size * 8;
        }
        
        output.resize(original_size);
        
        size_t decompressed_size = ZSTD_decompress(
            output.data(), original_size,
            input, input_size
        );
        
        if (ZSTD_isError(decompressed_size)) {
            logError("decompressWithZstd", "ZSTD decompression failed: " + std::string(ZSTD_getErrorName(decompressed_size)));
            return false;
        }
        
        output.resize(decompressed_size);
        return true;
        
    } catch (const std::exception& e) {
        logError("decompressWithZstd", "Exception: " + std::string(e.what()));
        return false;
    }
#else
    logError("decompressWithZstd", "ZSTD support not compiled in");
    return false;
#endif
}

bool SBCompressionEngine::decompressWithBzip2(const uint8_t* input, size_t input_size,
                                              std::vector<uint8_t>& output,
                                              CompressionStats& stats) {
#ifdef HAVE_BZIP2
    try {
        // Estimate output size
        size_t estimated_size = input_size * 8;
        output.resize(estimated_size);
        
        unsigned int output_size = static_cast<unsigned int>(estimated_size);
        int ret = BZ2_bzBuffToBuffDecompress(
            reinterpret_cast<char*>(output.data()), &output_size,
            const_cast<char*>(reinterpret_cast<const char*>(input)), static_cast<unsigned int>(input_size),
            0, 0
        );
        
        if (ret != BZ_OK) {
            logError("decompressWithBzip2", "BZIP2 decompression failed: " + std::to_string(ret));
            return false;
        }
        
        output.resize(output_size);
        return true;
        
    } catch (const std::exception& e) {
        logError("decompressWithBzip2", "Exception: " + std::string(e.what()));
        return false;
    }
#else
    logError("decompressWithBzip2", "BZIP2 support not compiled in");
    return false;
#endif
}

// Magic number detection
bool SBCompressionEngine::hasGzipMagic(const uint8_t* data, size_t size) {
    return size >= 2 && data[0] == 0x1f && data[1] == 0x8b;
}

bool SBCompressionEngine::hasLZ4Magic(const uint8_t* data, size_t size) {
    // LZ4 frame format magic: 0x184D2204
    return size >= 4 && data[0] == 0x04 && data[1] == 0x22 && data[2] == 0x4D && data[3] == 0x18;
}

bool SBCompressionEngine::hasZstdMagic(const uint8_t* data, size_t size) {
    return size >= 4 && data[0] == 0x28 && data[1] == 0xB5 && data[2] == 0x2F && data[3] == 0xFD;
}

bool SBCompressionEngine::hasBzip2Magic(const uint8_t* data, size_t size) {
    return size >= 3 && data[0] == 0x42 && data[1] == 0x5A && data[2] == 0x68;
}

// Error handling
void SBCompressionEngine::logError(const std::string& operation, const std::string& error) {
    last_error = operation + ": " + error;
    error_log.push_back(last_error);
    std::cerr << "[SBCompression ERROR] " << last_error << std::endl;
}

std::string SBCompressionEngine::getLastError() const {
    return last_error;
}

std::vector<std::string> SBCompressionEngine::getErrorLog() const {
    return error_log;
}

void SBCompressionEngine::clearErrorLog() {
    error_log.clear();
    last_error.clear();
}

// Utility functions
namespace SBCompression {

int levelToInt(Level level, Algorithm algorithm) {
    switch (algorithm) {
        case Algorithm::GZIP:
        case Algorithm::DEFLATE:
            return std::min(9, std::max(1, static_cast<int>(level)));
        case Algorithm::LZ4:
            return std::min(12, std::max(1, static_cast<int>(level)));
        case Algorithm::ZSTD:
            return std::min(22, std::max(1, static_cast<int>(level)));
        case Algorithm::BZIP2:
            return std::min(9, std::max(1, static_cast<int>(level)));
        default:
            return static_cast<int>(level);
    }
}

Level intToLevel(int level_int) {
    if (level_int <= 1) return Level::FASTEST;
    if (level_int <= 3) return Level::FAST;
    if (level_int <= 6) return Level::MEDIUM;
    if (level_int <= 9) return Level::BEST;
    return Level::ULTRA;
}

Algorithm getBestSpeedAlgorithm() {
    return Algorithm::LZ4;
}

Algorithm getBestRatioAlgorithm() {
    return Algorithm::ZSTD;
}

Algorithm getBestBalanceAlgorithm() {
    return Algorithm::ZSTD;
}

} // namespace SBCompression