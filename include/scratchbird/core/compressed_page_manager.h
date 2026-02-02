/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/compression.h"
#include <memory>

namespace scratchbird::core
{

    /**
     * Compressed Page Manager - Extends PageManager with compression support
     *
     * Handles transparent page compression/decompression during I/O.
     * Pages are compressed when written to disk and decompressed when read.
     */
    class CompressedPageManager : public PageManager
    {
    public:
        CompressedPageManager(Database *db, uint32_t page_size,
                              CompressionType compression_type = CompressionType::LZ4);
        ~CompressedPageManager();

        // Override page I/O methods to add compression
        auto readPage(uint32_t page_id, void *buffer, ErrorContext *ctx = nullptr) -> Status;
        auto writePage(uint32_t page_id, const void *buffer, ErrorContext *ctx = nullptr) -> Status;

        // Get compression statistics
        auto compressionStats() const -> const CompressionStats &
        {
            return codec_ ? codec_->stats() : empty_stats_;
        }

        // Get compression type
        auto compressionType() const -> CompressionType
        {
            return compression_type_;
        }

        // Check if a page should be compressed
        auto shouldCompressPage(uint32_t page_id, const void *buffer) const -> bool;

    private:
        CompressionType compression_type_;
        std::unique_ptr<CompressionCodec> codec_;
        std::vector<uint8_t> compression_buffer_; // Temporary buffer for compression
        CompressionStats empty_stats_;            // Empty stats for when compression is disabled

        // Helper to check if page is compressible (not system pages)
        static auto isCompressiblePage(uint32_t page_id) -> bool
        {
            // Don't compress system pages (0-2)
            return page_id > 2;
        }
    };

} // namespace scratchbird::core
