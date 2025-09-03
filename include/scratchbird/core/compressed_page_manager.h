#pragma once

#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/compression.h"
#include <memory>

namespace scratchbird {
namespace core {

/**
 * Compressed Page Manager - Extends PageManager with compression support
 * 
 * Handles transparent page compression/decompression during I/O.
 * Pages are compressed when written to disk and decompressed when read.
 */
class CompressedPageManager : public PageManager {
public:
    CompressedPageManager(Database* db, uint32_t page_size, 
                         CompressionType compression_type = CompressionType::LZ4);
    ~CompressedPageManager();
    
    // Override page I/O methods to add compression
    Status read_page(uint32_t page_id, void* buffer, ErrorContext* ctx = nullptr);
    Status write_page(uint32_t page_id, const void* buffer, ErrorContext* ctx = nullptr);
    
    // Get compression statistics
    const CompressionStats& compression_stats() const {
        return codec_ ? codec_->stats() : empty_stats_;
    }
    
    // Get compression type
    CompressionType compression_type() const { return compression_type_; }
    
    // Check if a page should be compressed
    bool should_compress_page(uint32_t page_id, const void* buffer) const;
    
private:
    CompressionType compression_type_;
    std::unique_ptr<CompressionCodec> codec_;
    std::vector<uint8_t> compression_buffer_;  // Temporary buffer for compression
    CompressionStats empty_stats_;  // Empty stats for when compression is disabled
    
    // Helper to check if page is compressible (not system pages)
    bool is_compressible_page(uint32_t page_id) const {
        // Don't compress system pages (0-2)
        return page_id > 2;
    }
};

} // namespace core
} // namespace scratchbird