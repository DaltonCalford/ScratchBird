#include "scratchbird/core/compression.h"
#include <chrono>
#include <cstring>

#ifdef HAVE_LZ4
#include <lz4.h>
#endif

namespace scratchbird::core
{

#ifdef HAVE_LZ4

    class LZ4Codec : public CompressionCodec
    {
    public:
        LZ4Codec() = default;

        [[nodiscard]] auto type() const -> CompressionType override
        {
            return CompressionType::LZ4;
        }

        [[nodiscard]] auto name() const -> const char * override
        {
            return "LZ4";
        }

        auto compress(const uint8_t *src, uint32_t src_size, uint8_t *dst, uint32_t dst_capacity,
                      uint32_t *compressed_size, CompressionLevel level) -> Status override
        {
            if ((src == nullptr) || (dst == nullptr) || (compressed_size == nullptr))
            {
                return Status::INVALID_ARGUMENT;
            }

            auto start = std::chrono::high_resolution_clock::now();

            int compressed_bytes;

            switch (level)
            {
                case CompressionLevel::FASTEST:
                    compressed_bytes = LZ4_compress_fast(
                        reinterpret_cast<const char *>(src), reinterpret_cast<char *>(dst),
                        static_cast<int>(src_size), static_cast<int>(dst_capacity),
                        1 // acceleration factor
                    );
                    break;

                case CompressionLevel::BEST:
                    // Fallback to default compression as HC is not available
                case CompressionLevel::DEFAULT:
                default:
                    compressed_bytes = LZ4_compress_default(
                        reinterpret_cast<const char *>(src), reinterpret_cast<char *>(dst),
                        static_cast<int>(src_size), static_cast<int>(dst_capacity));
                    break;
            }

            if (compressed_bytes <= 0)
            {
                return Status::COMPRESSION_ERROR;
            }

            *compressed_size = static_cast<uint32_t>(compressed_bytes);

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

            // Update statistics
            stats_.bytes_in += src_size;
            stats_.bytes_out += *compressed_size;
            stats_.compress_time_us += duration.count();
            stats_.compress_calls++;

            return Status::OK;
        }

        auto decompress(const uint8_t *src, uint32_t src_size, uint8_t *dst, uint32_t dst_capacity,
                        uint32_t *decompressed_size) -> Status override
        {
            if ((src == nullptr) || (dst == nullptr) || (decompressed_size == nullptr))
            {
                return Status::INVALID_ARGUMENT;
            }

            auto start = std::chrono::high_resolution_clock::now();

            int decompressed_bytes = LZ4_decompress_safe(
                reinterpret_cast<const char *>(src), reinterpret_cast<char *>(dst),
                static_cast<int>(src_size), static_cast<int>(dst_capacity));

            if (decompressed_bytes < 0)
            {
                return Status::COMPRESSION_ERROR;
            }

            *decompressed_size = static_cast<uint32_t>(decompressed_bytes);

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

            // Update statistics
            stats_.decompress_time_us += duration.count();
            stats_.decompress_calls++;

            return Status::OK;
        }

        [[nodiscard]] auto maxCompressedSize(uint32_t uncompressed_size) const -> uint32_t override
        {
            return static_cast<uint32_t>(LZ4_compressBound(static_cast<int>(uncompressed_size)));
        }

        [[nodiscard]] auto stats() const -> const CompressionStats & override
        {
            return stats_;
        }

        void resetStats() override
        {
            stats_ = CompressionStats();
        }

    private:
        CompressionStats stats_;
    };

    // Factory implementation
    auto CompressionFactory::create(CompressionType type) -> std::unique_ptr<CompressionCodec>
    {
        switch (type)
        {
            case CompressionType::LZ4:
                return std::make_unique<LZ4Codec>();
            case CompressionType::NONE:
                return nullptr; // No codec needed for uncompressed
            default:
                return nullptr; // Unsupported type
        }
    }

    auto CompressionFactory::isSupported(CompressionType type) -> bool
    {
        switch (type)
        {
            case CompressionType::NONE:
            case CompressionType::LZ4:
                return true;
            default:
                return false;
        }
    }

    auto CompressionFactory::supportedTypes() -> std::vector<CompressionType>
    {
        return {CompressionType::NONE, CompressionType::LZ4};
    }

    auto CompressionFactory::getName(CompressionType type) -> const char *
    {
        switch (type)
        {
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
    std::unique_ptr<CompressionCodec> CompressionFactory::create(CompressionType type)
    {
        return nullptr;
    }

    bool CompressionFactory::isSupported(CompressionType type)
    {
        return type == CompressionType::NONE;
    }

    std::vector<CompressionType> CompressionFactory::supportedTypes()
    {
        return {CompressionType::NONE};
    }

    const char *CompressionFactory::getName(CompressionType type)
    {
        switch (type)
        {
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

} // namespace scratchbird::core
