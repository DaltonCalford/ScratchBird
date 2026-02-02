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

/**
 * LSM Compression Interface
 *
 * Purpose: Compress/decompress SSTable blocks to reduce disk space
 *
 * Supported Algorithms:
 * - None: No compression (pass-through)
 * - Snappy: Fast compression/decompression, moderate ratio (2-4x)
 * - Zstd: Better compression ratio (3-6x), slightly slower
 *
 * Compression Strategy:
 * - Compress at block level (not per-entry)
 * - Store compression type in SSTable footer
 * - Transparent to upper layers (SSTableWriter/Reader handle it)
 *
 * Performance:
 * - Snappy: ~500 MB/s compression, ~1500 MB/s decompression
 * - Zstd (level 3): ~300 MB/s compression, ~800 MB/s decompression
 *
 * Space Savings:
 * - Snappy: 50-60% reduction on typical data
 * - Zstd: 60-70% reduction on typical data
 *
 * November 22, 2025
 */

#include <vector>
#include <cstdint>
#include <string>
#include <memory>

namespace scratchbird
{
namespace core
{

/**
 * Compression type enum
 */
enum class CompressionType : uint8_t
{
    NONE = 0,     // No compression
    SNAPPY = 1,   // Snappy compression
    ZSTD = 2      // Zstd compression
};

/**
 * Abstract compression interface
 *
 * All compression algorithms must implement this interface.
 */
class Compressor
{
public:
    virtual ~Compressor() = default;

    /**
     * Compress data
     * @param input Input data to compress
     * @param output Output buffer for compressed data
     * @return true on success, false on failure
     */
    virtual bool compress(const std::vector<uint8_t>& input,
                         std::vector<uint8_t>* output) = 0;

    /**
     * Decompress data
     * @param input Compressed input data
     * @param output Output buffer for decompressed data
     * @param original_size Original uncompressed size (hint for allocation)
     * @return true on success, false on failure
     */
    virtual bool decompress(const std::vector<uint8_t>& input,
                           std::vector<uint8_t>* output,
                           size_t original_size) = 0;

    /**
     * Get compression type
     */
    virtual CompressionType getType() const = 0;

    /**
     * Get maximum compressed size for input of given size
     * Used for buffer pre-allocation
     */
    virtual size_t getMaxCompressedSize(size_t input_size) const = 0;
};

/**
 * No compression (pass-through)
 */
class NoCompressor : public Compressor
{
public:
    bool compress(const std::vector<uint8_t>& input,
                 std::vector<uint8_t>* output) override;

    bool decompress(const std::vector<uint8_t>& input,
                   std::vector<uint8_t>* output,
                   size_t original_size) override;

    CompressionType getType() const override { return CompressionType::NONE; }

    size_t getMaxCompressedSize(size_t input_size) const override
    {
        return input_size;
    }
};

/**
 * Snappy compression
 *
 * Fast compression with moderate compression ratio.
 * Best for: General-purpose workloads, latency-sensitive applications
 */
class SnappyCompressor : public Compressor
{
public:
    bool compress(const std::vector<uint8_t>& input,
                 std::vector<uint8_t>* output) override;

    bool decompress(const std::vector<uint8_t>& input,
                   std::vector<uint8_t>* output,
                   size_t original_size) override;

    CompressionType getType() const override { return CompressionType::SNAPPY; }

    size_t getMaxCompressedSize(size_t input_size) const override;
};

/**
 * Zstd compression
 *
 * Better compression ratio, slightly slower than Snappy.
 * Best for: Space-constrained environments, archival workloads
 *
 * Compression level: 3 (default)
 * - Level 1: Fastest, ~50% ratio
 * - Level 3: Balanced (default), ~60% ratio
 * - Level 19: Best ratio (~70%), very slow
 */
class ZstdCompressor : public Compressor
{
public:
    /**
     * Constructor
     * @param compression_level Zstd compression level (1-19, default: 3)
     */
    explicit ZstdCompressor(int compression_level = 3);

    bool compress(const std::vector<uint8_t>& input,
                 std::vector<uint8_t>* output) override;

    bool decompress(const std::vector<uint8_t>& input,
                   std::vector<uint8_t>* output,
                   size_t original_size) override;

    CompressionType getType() const override { return CompressionType::ZSTD; }

    size_t getMaxCompressedSize(size_t input_size) const override;

private:
    int compression_level_;
};

/**
 * Compression factory
 *
 * Creates compressor instances based on type.
 */
class LsmCompressionFactory
{
public:
    /**
     * Create compressor for given type
     * @param type Compression type
     * @return Compressor instance (never null)
     */
    static std::unique_ptr<Compressor> create(CompressionType type);

    /**
     * Get compression type from string
     * @param name Compression name ("none", "snappy", "zstd")
     * @return Compression type
     */
    static CompressionType fromString(const std::string& name);

    /**
     * Get string representation of compression type
     * @param type Compression type
     * @return String name
     */
    static std::string toString(CompressionType type);
};

/**
 * Check if a compression type is supported in this build.
 */
bool isCompressionSupported(CompressionType type);

} // namespace core
} // namespace scratchbird
