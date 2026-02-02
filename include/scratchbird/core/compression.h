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

#include "scratchbird/core/status.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace scratchbird::core
{

    // Compression algorithms supported
    enum class CompressionType : uint8_t
    {
        NONE = 0,   // No compression
        LZ4 = 1,    // LZ4 compression (baseline)
        ZSTD = 2,   // Zstandard compression (future)
        SNAPPY = 3, // Snappy compression (future)
        // Add more algorithms as needed
    };

    // Compression level hints
    enum class CompressionLevel
    {
        FASTEST = 0, // Optimize for speed
        DEFAULT = 1, // Balance speed and ratio
        BEST = 2,    // Optimize for compression ratio
    };

    // Compression statistics
    struct CompressionStats
    {
        uint64_t bytes_in = 0;           // Total uncompressed bytes
        uint64_t bytes_out = 0;          // Total compressed bytes
        uint64_t compress_time_us = 0;   // Total compression time in microseconds
        uint64_t decompress_time_us = 0; // Total decompression time in microseconds
        uint64_t compress_calls = 0;     // Number of compress calls
        uint64_t decompress_calls = 0;   // Number of decompress calls

        [[nodiscard]] auto compressionRatio() const -> double
        {
            return bytes_in > 0 ? static_cast<double>(bytes_out) / bytes_in : 1.0;
        }

        [[nodiscard]] auto avgCompressTimeUs() const -> double
        {
            return compress_calls > 0 ? static_cast<double>(compress_time_us) / compress_calls
                                      : 0.0;
        }

        [[nodiscard]] auto avgDecompressTimeUs() const -> double
        {
            return decompress_calls > 0 ? static_cast<double>(decompress_time_us) / decompress_calls
                                        : 0.0;
        }
    };

    // Abstract compression interface
    class CompressionCodec
    {
    public:
        virtual ~CompressionCodec() = default;

        // Get the compression type
        [[nodiscard]] virtual auto type() const -> CompressionType = 0;

        // Get algorithm name
        [[nodiscard]] virtual auto name() const -> const char * = 0;

        // Compress data
        // Returns compressed size, or Status error
        virtual auto compress(const uint8_t *src, uint32_t src_size, uint8_t *dst,
                              uint32_t dst_capacity, uint32_t *compressed_size,
                              CompressionLevel level = CompressionLevel::DEFAULT) -> Status = 0;

        // Decompress data
        // Returns decompressed size, or Status error
        virtual auto decompress(const uint8_t *src, uint32_t src_size, uint8_t *dst,
                                uint32_t dst_capacity, uint32_t *decompressed_size) -> Status = 0;

        // Get maximum compressed size for given input size
        [[nodiscard]] virtual auto maxCompressedSize(uint32_t uncompressed_size) const
            -> uint32_t = 0;

        // Check if compression is beneficial for this data size
        [[nodiscard]] virtual auto shouldCompress(uint32_t size) const -> bool
        {
            // Default: compress if larger than 256 bytes
            return size > 256;
        }

        // Get compression statistics
        [[nodiscard]] virtual auto stats() const -> const CompressionStats & = 0;

        // Reset statistics
        virtual void resetStats() = 0;
    };

    // Compression factory
    class CompressionFactory
    {
    public:
        // Create a compression codec
        static auto create(CompressionType type) -> std::unique_ptr<CompressionCodec>;

        // Check if compression type is supported
        static auto isSupported(CompressionType type) -> bool;

        // Get list of supported compression types
        static auto supportedTypes() -> std::vector<CompressionType>;

        // Get name for compression type
        static auto getName(CompressionType type) -> const char *;
    };

// Page compression header - stored at beginning of compressed page data
#pragma pack(push, 1)
    struct CompressedPageHeader
    {
        uint32_t uncompressed_size; // Original page size
        uint32_t compressed_size;   // Compressed data size (excluding this header)
        uint8_t compression_type;   // CompressionType enum value
        uint8_t reserved[3];        // Reserved for alignment
        uint32_t checksum;          // CRC32C of compressed data
    };
#pragma pack(pop)

} // namespace scratchbird::core
