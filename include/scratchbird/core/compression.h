#pragma once

#include "scratchbird/core/status.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace scratchbird {
namespace core {

// Compression algorithms supported
enum class CompressionType : uint8_t {
    NONE = 0,       // No compression
    LZ4 = 1,        // LZ4 compression (baseline)
    ZSTD = 2,       // Zstandard compression (future)
    SNAPPY = 3,     // Snappy compression (future)
    // Add more algorithms as needed
};

// Compression level hints
enum class CompressionLevel {
    FASTEST = 0,    // Optimize for speed
    DEFAULT = 1,    // Balance speed and ratio
    BEST = 2,       // Optimize for compression ratio
};

// Compression statistics
struct CompressionStats {
    uint64_t bytes_in = 0;          // Total uncompressed bytes
    uint64_t bytes_out = 0;         // Total compressed bytes
    uint64_t compress_time_us = 0;  // Total compression time in microseconds
    uint64_t decompress_time_us = 0; // Total decompression time in microseconds
    uint64_t compress_calls = 0;    // Number of compress calls
    uint64_t decompress_calls = 0;  // Number of decompress calls
    
    double compression_ratio() const {
        return bytes_in > 0 ? static_cast<double>(bytes_out) / bytes_in : 1.0;
    }
    
    double avg_compress_time_us() const {
        return compress_calls > 0 ? 
            static_cast<double>(compress_time_us) / compress_calls : 0.0;
    }
    
    double avg_decompress_time_us() const {
        return decompress_calls > 0 ? 
            static_cast<double>(decompress_time_us) / decompress_calls : 0.0;
    }
};

// Abstract compression interface
class CompressionCodec {
public:
    virtual ~CompressionCodec() = default;
    
    // Get the compression type
    virtual CompressionType type() const = 0;
    
    // Get algorithm name
    virtual const char* name() const = 0;
    
    // Compress data
    // Returns compressed size, or Status error
    virtual Status compress(const uint8_t* src, uint32_t src_size,
                          uint8_t* dst, uint32_t dst_capacity,
                          uint32_t* compressed_size,
                          CompressionLevel level = CompressionLevel::DEFAULT) = 0;
    
    // Decompress data
    // Returns decompressed size, or Status error
    virtual Status decompress(const uint8_t* src, uint32_t src_size,
                            uint8_t* dst, uint32_t dst_capacity,
                            uint32_t* decompressed_size) = 0;
    
    // Get maximum compressed size for given input size
    virtual uint32_t max_compressed_size(uint32_t uncompressed_size) const = 0;
    
    // Check if compression is beneficial for this data size
    virtual bool should_compress(uint32_t size) const {
        // Default: compress if larger than 256 bytes
        return size > 256;
    }
    
    // Get compression statistics
    virtual const CompressionStats& stats() const = 0;
    
    // Reset statistics
    virtual void reset_stats() = 0;
};

// Compression factory
class CompressionFactory {
public:
    // Create a compression codec
    static std::unique_ptr<CompressionCodec> create(CompressionType type);
    
    // Check if compression type is supported
    static bool is_supported(CompressionType type);
    
    // Get list of supported compression types
    static std::vector<CompressionType> supported_types();
    
    // Get name for compression type
    static const char* get_name(CompressionType type);
};

// Page compression header - stored at beginning of compressed page data
#pragma pack(push, 1)
struct CompressedPageHeader {
    uint32_t uncompressed_size;  // Original page size
    uint32_t compressed_size;    // Compressed data size (excluding this header)
    uint8_t  compression_type;   // CompressionType enum value
    uint8_t  reserved[3];        // Reserved for alignment
    uint32_t checksum;          // CRC32C of compressed data
};
#pragma pack(pop)

} // namespace core
} // namespace scratchbird