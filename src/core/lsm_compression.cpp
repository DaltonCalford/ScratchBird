/**
 * LSM Compression Implementation
 *
 * Implements Snappy and Zstd compression for SSTable blocks.
 *
 * Compression Libraries:
 * - Snappy: libsnappy-dev (apt-get install libsnappy-dev)
 * - Zstd: libzstd-dev (apt-get install libzstd-dev)
 *
 * If libraries are not available, compression gracefully degrades to NONE.
 *
 * November 22, 2025
 */

#include "scratchbird/core/lsm_compression.h"

// Try to include compression libraries
// If not available, compression will degrade to NONE
#ifdef __has_include
#  if __has_include(<snappy.h>)
#    define HAS_SNAPPY 1
#    include <snappy.h>
#  else
#    define HAS_SNAPPY 0
#  endif

#  if __has_include(<zstd.h>)
#    define HAS_ZSTD 1
#    include <zstd.h>
#  else
#    define HAS_ZSTD 0
#  endif
#else
#  define HAS_SNAPPY 0
#  define HAS_ZSTD 0
#endif

#include <cstring>
#include <algorithm>

namespace scratchbird
{
namespace core
{

// ============================================================================
// NoCompressor Implementation (Pass-through)
// ============================================================================

bool NoCompressor::compress(const std::vector<uint8_t>& input,
                           std::vector<uint8_t>* output)
{
    if (!output)
    {
        return false;
    }

    *output = input;
    return true;
}

bool NoCompressor::decompress(const std::vector<uint8_t>& input,
                             std::vector<uint8_t>* output,
                             size_t original_size)
{
    if (!output)
    {
        return false;
    }

    (void)original_size;  // Unused for no compression
    *output = input;
    return true;
}

// ============================================================================
// SnappyCompressor Implementation
// ============================================================================

bool SnappyCompressor::compress(const std::vector<uint8_t>& input,
                               std::vector<uint8_t>* output)
{
    if (!output)
    {
        return false;
    }

#if HAS_SNAPPY
    // Get max compressed size and allocate output buffer
    size_t max_compressed_size = snappy::MaxCompressedLength(input.size());
    output->resize(max_compressed_size);

    // Compress
    size_t compressed_size = 0;
    snappy::RawCompress(reinterpret_cast<const char*>(input.data()),
                       input.size(),
                       reinterpret_cast<char*>(output->data()),
                       &compressed_size);

    // Resize to actual compressed size
    output->resize(compressed_size);
    return true;
#else
    // Snappy not available - fall back to no compression
    *output = input;
    return true;
#endif
}

bool SnappyCompressor::decompress(const std::vector<uint8_t>& input,
                                 std::vector<uint8_t>* output,
                                 size_t original_size)
{
    if (!output)
    {
        return false;
    }

#if HAS_SNAPPY
    // Allocate output buffer
    output->resize(original_size);

    // Decompress
    bool success = snappy::RawUncompress(
        reinterpret_cast<const char*>(input.data()),
        input.size(),
        reinterpret_cast<char*>(output->data()));

    if (!success)
    {
        output->clear();
        return false;
    }

    return true;
#else
    // Snappy not available - assume data is uncompressed
    *output = input;
    return true;
#endif
}

size_t SnappyCompressor::getMaxCompressedSize(size_t input_size) const
{
#if HAS_SNAPPY
    return snappy::MaxCompressedLength(input_size);
#else
    return input_size;
#endif
}

// ============================================================================
// ZstdCompressor Implementation
// ============================================================================

ZstdCompressor::ZstdCompressor(int compression_level)
    : compression_level_(compression_level)
{
    // Clamp compression level to valid range (1-19)
    if (compression_level_ < 1)
    {
        compression_level_ = 1;
    }
    else if (compression_level_ > 19)
    {
        compression_level_ = 19;
    }
}

bool ZstdCompressor::compress(const std::vector<uint8_t>& input,
                             std::vector<uint8_t>* output)
{
    if (!output)
    {
        return false;
    }

#if HAS_ZSTD
    // Get max compressed size and allocate output buffer
    size_t max_compressed_size = ZSTD_compressBound(input.size());
    output->resize(max_compressed_size);

    // Compress
    size_t compressed_size = ZSTD_compress(
        output->data(),
        max_compressed_size,
        input.data(),
        input.size(),
        compression_level_);

    // Check for error
    if (ZSTD_isError(compressed_size))
    {
        output->clear();
        return false;
    }

    // Resize to actual compressed size
    output->resize(compressed_size);
    return true;
#else
    // Zstd not available - fall back to no compression
    *output = input;
    return true;
#endif
}

bool ZstdCompressor::decompress(const std::vector<uint8_t>& input,
                               std::vector<uint8_t>* output,
                               size_t original_size)
{
    if (!output)
    {
        return false;
    }

#if HAS_ZSTD
    // Allocate output buffer
    output->resize(original_size);

    // Decompress
    size_t decompressed_size = ZSTD_decompress(
        output->data(),
        original_size,
        input.data(),
        input.size());

    // Check for error
    if (ZSTD_isError(decompressed_size))
    {
        output->clear();
        return false;
    }

    // Verify size matches
    if (decompressed_size != original_size)
    {
        output->clear();
        return false;
    }

    return true;
#else
    // Zstd not available - assume data is uncompressed
    *output = input;
    return true;
#endif
}

size_t ZstdCompressor::getMaxCompressedSize(size_t input_size) const
{
#if HAS_ZSTD
    return ZSTD_compressBound(input_size);
#else
    return input_size;
#endif
}

// ============================================================================
// LsmCompressionFactory Implementation
// ============================================================================

std::unique_ptr<Compressor> LsmCompressionFactory::create(CompressionType type)
{
    switch (type)
    {
        case CompressionType::NONE:
            return std::make_unique<NoCompressor>();

        case CompressionType::SNAPPY:
#if HAS_SNAPPY
            return std::make_unique<SnappyCompressor>();
#else
            // Snappy not available - fall back to no compression
            return std::make_unique<NoCompressor>();
#endif

        case CompressionType::ZSTD:
#if HAS_ZSTD
            return std::make_unique<ZstdCompressor>(3);  // Default level 3
#else
            // Zstd not available - fall back to no compression
            return std::make_unique<NoCompressor>();
#endif

        default:
            return std::make_unique<NoCompressor>();
    }
}

CompressionType LsmCompressionFactory::fromString(const std::string& name)
{
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

    if (lower_name == "snappy")
    {
        return CompressionType::SNAPPY;
    }
    else if (lower_name == "zstd" || lower_name == "zstandard")
    {
        return CompressionType::ZSTD;
    }
    else
    {
        return CompressionType::NONE;
    }
}

std::string LsmCompressionFactory::toString(CompressionType type)
{
    switch (type)
    {
        case CompressionType::NONE:
            return "none";
        case CompressionType::SNAPPY:
            return "snappy";
        case CompressionType::ZSTD:
            return "zstd";
        default:
            return "unknown";
    }
}

bool isCompressionSupported(CompressionType type)
{
    switch (type)
    {
        case CompressionType::NONE:
            return true;
        case CompressionType::SNAPPY:
#if HAS_SNAPPY
            return true;
#else
            return false;
#endif
        case CompressionType::ZSTD:
#if HAS_ZSTD
            return true;
#else
            return false;
#endif
        default:
            return false;
    }
}

} // namespace core
} // namespace scratchbird
