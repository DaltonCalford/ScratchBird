#include "scratchbird/core/compressed_page_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/ondisk.h"
#include <cstring>
#include <algorithm>

namespace scratchbird {
namespace core {

CompressedPageManager::CompressedPageManager(Database* db, uint32_t page_size, 
                                           CompressionType compression_type)
    : PageManager(db, page_size), compression_type_(compression_type) {
    
    if (compression_type != CompressionType::NONE) {
        codec_ = CompressionFactory::create(compression_type);
        if (codec_) {
            // Allocate compression buffer for worst case
            compression_buffer_.resize(codec_->max_compressed_size(page_size) + 
                                     sizeof(CompressedPageHeader));
        } else if (compression_type == CompressionType::LZ4) {
            // LZ4 requested but not available, fall back to no compression
            compression_type_ = CompressionType::NONE;
        }
    }
}

CompressedPageManager::~CompressedPageManager() = default;

Status CompressedPageManager::read_page(uint32_t page_id, void* buffer, ErrorContext* ctx) {
    if (!buffer) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Null buffer in read_page");
        return Status::InvalidArgument;
    }
    
    // Read the page header first to check if it's compressed
    PageHeader temp_header;
    Status status = db_->read_page_partial(page_id, &temp_header, sizeof(PageHeader), 0, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Check if page is compressed
    if ((temp_header.flags & PAGE_FLAG_COMPRESSED) && codec_ && is_compressible_page(page_id)) {
        // Read the full page into compression buffer
        status = db_->read_page(page_id, compression_buffer_.data(), ctx);
        if (status != Status::Ok) {
            return status;
        }
        
        // Skip the page header to get to compressed data
        uint8_t* compressed_start = compression_buffer_.data() + sizeof(PageHeader);
        CompressedPageHeader* comp_header = reinterpret_cast<CompressedPageHeader*>(compressed_start);
        
        // Validate compression header
        if (comp_header->compression_type != static_cast<uint8_t>(compression_type_)) {
            SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, 
                            "Compression type mismatch in compressed page");
            return Status::PageCorrupt;
        }
        
        // Verify compressed data checksum
        uint32_t checksum = crc32c_compute(compressed_start + sizeof(CompressedPageHeader),
                                          comp_header->compressed_size, 0xFFFFFFFF) ^ 0xFFFFFFFF;
        if (checksum != comp_header->checksum) {
            SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, 
                            "Compressed data checksum mismatch");
            return Status::PageCorrupt;
        }
        
        // Decompress the data
        uint32_t decompressed_size;
        status = codec_->decompress(
            compressed_start + sizeof(CompressedPageHeader),
            comp_header->compressed_size,
            reinterpret_cast<uint8_t*>(buffer),
            page_size_,
            &decompressed_size
        );
        
        if (status != Status::Ok || decompressed_size != comp_header->uncompressed_size) {
            SET_ERROR_CONTEXT(ctx, Status::CompressionError, 
                            "Failed to decompress page");
            return Status::CompressionError;
        }
        
        // Verify the decompressed page checksum
        PageHeader* header = reinterpret_cast<PageHeader*>(buffer);
        if (!validate_page_checksum(reinterpret_cast<uint8_t*>(buffer), page_size_)) {
            SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, 
                            "Decompressed page checksum mismatch");
            return Status::PageCorrupt;
        }
        
        // Clear the compressed flag in the in-memory copy
        header->flags &= ~PAGE_FLAG_COMPRESSED;
        
        return Status::Ok;
    } else {
        // Not compressed, read normally
        return db_->read_page(page_id, buffer, ctx);
    }
}

Status CompressedPageManager::write_page(uint32_t page_id, const void* buffer, ErrorContext* ctx) {
    if (!buffer) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Null buffer in write_page");
        return Status::InvalidArgument;
    }
    
    // Check if we should compress this page
    if (codec_ && is_compressible_page(page_id) && should_compress_page(page_id, buffer)) {
        // First, update the page checksum
        uint8_t* page = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(buffer));
        PageHeader* header = reinterpret_cast<PageHeader*>(page);
        header->checksum = calculate_page_checksum(page, page_size_);
        
        // Prepare compression buffer
        PageHeader compressed_header = *header;
        compressed_header.flags |= PAGE_FLAG_COMPRESSED;
        
        // Copy header to compression buffer
        memcpy(compression_buffer_.data(), &compressed_header, sizeof(PageHeader));
        
        // Set up compressed page header
        CompressedPageHeader* comp_header = reinterpret_cast<CompressedPageHeader*>(
            compression_buffer_.data() + sizeof(PageHeader));
        comp_header->uncompressed_size = page_size_;
        comp_header->compression_type = static_cast<uint8_t>(compression_type_);
        memset(comp_header->reserved, 0, sizeof(comp_header->reserved));
        
        // Compress the page data
        uint32_t compressed_size;
        Status status = codec_->compress(
            reinterpret_cast<const uint8_t*>(buffer),
            page_size_,
            compression_buffer_.data() + sizeof(PageHeader) + sizeof(CompressedPageHeader),
            compression_buffer_.size() - sizeof(PageHeader) - sizeof(CompressedPageHeader),
            &compressed_size
        );
        
        if (status != Status::Ok) {
            // Compression failed, write uncompressed
            return db_->write_page(page_id, buffer, ctx);
        }
        
        // Check if compression is beneficial
        uint32_t total_compressed_size = sizeof(PageHeader) + sizeof(CompressedPageHeader) + 
                                        compressed_size;
        if (total_compressed_size >= page_size_ * 0.9) {
            // Not worth compressing, write uncompressed
            return db_->write_page(page_id, buffer, ctx);
        }
        
        // Update compressed header
        comp_header->compressed_size = compressed_size;
        comp_header->checksum = crc32c_compute(
            compression_buffer_.data() + sizeof(PageHeader) + sizeof(CompressedPageHeader),
            compressed_size, 0xFFFFFFFF) ^ 0xFFFFFFFF;
        
        // Update the compressed page header checksum
        compressed_header.checksum = calculate_page_checksum(compression_buffer_.data(), page_size_);
        memcpy(compression_buffer_.data(), &compressed_header, sizeof(PageHeader));
        
        // Write the compressed page
        return db_->write_page(page_id, compression_buffer_.data(), ctx);
    } else {
        // Not compressing, write normally
        return db_->write_page(page_id, buffer, ctx);
    }
}

bool CompressedPageManager::should_compress_page(uint32_t page_id, const void* buffer) const {
    if (!codec_ || !is_compressible_page(page_id)) {
        return false;
    }
    
    const PageHeader* header = reinterpret_cast<const PageHeader*>(buffer);
    
    // Don't compress already compressed pages
    if (header->flags & PAGE_FLAG_COMPRESSED) {
        return false;
    }
    
    // Check page type - some types compress better than others
    switch (header->page_type) {
        case PAGE_TYPE_DATABASE_HEADER:
        case PAGE_TYPE_SYSTEM_CATALOG:
        case PAGE_TYPE_FREE_SPACE_MAP:
        case PAGE_TYPE_TRANSACTION_MAP:
            // System pages typically don't compress well
            return false;
            
        case PAGE_TYPE_HEAP:
            // Heap pages often compress well
            return header->free_space < page_size_ * 0.5;  // Compress if more than 50% full
            
        case PAGE_TYPE_BTREE_LEAF:
            // B-tree leaf pages may compress well depending on data
            return true;
            
        case PAGE_TYPE_BTREE_INTERNAL:
        case PAGE_TYPE_BTREE_META:
            // B-tree internal/meta pages are usually small, not worth compressing
            return false;
            
        default:
            // Unknown page type, try to compress
            return true;
    }
}

} // namespace core
} // namespace scratchbird