#include "scratchbird/core/compression.h"
#include <chrono>
#include <cstring>

#ifdef HAVE_LZ4
#include <lz4.h>
#endif

namespace scratchbird {
namespace core {

#ifdef HAVE_LZ4

class LZ4Codec : public CompressionCodec {
public:
    LZ4Codec() = default;
    
    CompressionType type() const override {
        return CompressionType::LZ4;
    }
    
    const char* name() const override {
        return "LZ4";
    }
    
    Status compress(const uint8_t* src, uint32_t src_size,
                   uint8_t* dst, uint32_t dst_capacity,
                   uint32_t* compressed_size,
                   CompressionLevel level) override {
        if (!src || !dst || !compressed_size) {
            return Status::InvalidArgument("Null pointer in compress");
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        
        int compressed_bytes;
        
        switch (level) {
            case CompressionLevel::FASTEST:
                compressed_bytes = LZ4_compress_fast(
                    reinterpret_cast<const char*>(src),
                    reinterpret_cast<char*>(dst),
                    static_cast<int>(src_size),
                    static_cast<int>(dst_capacity),
                    1  // acceleration factor
                );
                break;
                
            case CompressionLevel::BEST:
                compressed_bytes = LZ4_compress_HC(
                    reinterpret_cast<const char*>(src),
                    reinterpret_cast<char*>(dst),
                    static_cast<int>(src_size),
                    static_cast<int>(dst_capacity),
                    LZ4HC_CLEVEL_MAX
                );
                break;
                
            case CompressionLevel::DEFAULT:
            default:
                compressed_bytes = LZ4_compress_default(
                    reinterpret_cast<const char*>(src),
                    reinterpret_cast<char*>(dst),
                    static_cast<int>(src_size),
                    static_cast<int>(dst_capacity)
                );
                break;
        }
        
        if (compressed_bytes <= 0) {
            return Status::CompressionError("LZ4 compression failed");
        }
        
        *compressed_size = static_cast<uint32_t>(compressed_bytes);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // Update statistics
        stats_.bytes_in += src_size;
        stats_.bytes_out += *compressed_size;
        stats_.compress_time_us += duration.count();
        stats_.compress_calls++;
        
        return Status::Ok();
    }
    
    Status decompress(const uint8_t* src, uint32_t src_size,
                     uint8_t* dst, uint32_t dst_capacity,
                     uint32_t* decompressed_size) override {
        if (!src || !dst || !decompressed_size) {
            return Status::InvalidArgument("Null pointer in decompress");
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        
        int decompressed_bytes = LZ4_decompress_safe(
            reinterpret_cast<const char*>(src),
            reinterpret_cast<char*>(dst),
            static_cast<int>(src_size),
            static_cast<int>(dst_capacity)
        );
        
        if (decompressed_bytes < 0) {
            return Status::CompressionError("LZ4 decompression failed");
        }
        
        *decompressed_size = static_cast<uint32_t>(decompressed_bytes);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // Update statistics
        stats_.decompress_time_us += duration.count();
        stats_.decompress_calls++;
        
        return Status::Ok();
    }
    
    uint32_t max_compressed_size(uint32_t uncompressed_size) const override {
        return static_cast<uint32_t>(LZ4_compressBound(static_cast<int>(uncompressed_size)));
    }
    
    const CompressionStats& stats() const override {
        return stats_;
    }
    
    void reset_stats() override {
        stats_ = CompressionStats();
    }
    
private:
    CompressionStats stats_;
};

// Factory implementation
std::unique_ptr<CompressionCodec> CompressionFactory::create(CompressionType type) {
    switch (type) {
        case CompressionType::LZ4:
            return std::make_unique<LZ4Codec>();
        case CompressionType::NONE:
            return nullptr;  // No codec needed for uncompressed
        default:
            return nullptr;  // Unsupported type
    }
}

bool CompressionFactory::is_supported(CompressionType type) {
    switch (type) {
        case CompressionType::NONE:
        case CompressionType::LZ4:
            return true;
        default:
            return false;
    }
}

std::vector<CompressionType> CompressionFactory::supported_types() {
    return {CompressionType::NONE, CompressionType::LZ4};
}

const char* CompressionFactory::get_name(CompressionType type) {
    switch (type) {
        case CompressionType::NONE:
            return "None";
        case CompressionType::LZ4:
            return "LZ4";
        case CompressionType::ZSTD:
            return "Zstandard";
        case CompressionType::SNAPPY:
            return "Snappy";
        default:
            return "Unknown";
    }
}

#else // !HAVE_LZ4

// Stub implementation when LZ4 is not available
std::unique_ptr<CompressionCodec> CompressionFactory::create(CompressionType type) {
    return nullptr;
}

bool CompressionFactory::is_supported(CompressionType type) {
    return type == CompressionType::NONE;
}

std::vector<CompressionType> CompressionFactory::supported_types() {
    return {CompressionType::NONE};
}

const char* CompressionFactory::get_name(CompressionType type) {
    switch (type) {
        case CompressionType::NONE:
            return "None";
        case CompressionType::LZ4:
            return "LZ4 (not available)";
        case CompressionType::ZSTD:
            return "Zstandard (not available)";
        case CompressionType::SNAPPY:
            return "Snappy (not available)";
        default:
            return "Unknown";
    }
}

#endif // HAVE_LZ4

} // namespace core
} // namespace scratchbird