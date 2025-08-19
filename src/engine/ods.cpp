#include "scratchbird/engine/ods.h"

#include <cstddef>
#include <cstdint>

namespace scratchbird::engine::ods
{

    // CRC32C (Castagnoli) implementation (simple, tableless; fine for tests)
    static inline std::uint32_t crc32c_update(std::uint32_t crc, const unsigned char* buf,
                                              std::size_t len)
    {
        crc = ~crc;
        for (std::size_t i = 0; i < len; ++i) {
            crc ^= buf[i];
            for (int k = 0; k < 8; ++k) {
                std::uint32_t mask = -(crc & 1u);
                crc = (crc >> 1) ^ (0x82F63B78u & mask);
            }
        }
        return ~crc;
    }

    std::uint32_t crc32c(const void* data, std::size_t len, std::uint32_t seed)
    {
        return crc32c_update(seed, static_cast<const unsigned char*>(data), len);
    }

    // Number of usable bytes in PIP (excluding header)
    std::uint32_t bytesBitPIP(std::uint32_t page_size)
    {
        // Header occupies first 64 bytes (conservative); rest is bitmap
        const std::uint32_t hdrReserve = 64;
        if (page_size <= hdrReserve)
            return 0;
        return page_size - hdrReserve;
    }

    std::uint32_t pagesPerPIP(std::uint32_t page_size)
    {
        const std::uint32_t bytes = bytesBitPIP(page_size);
        return bytes * 8u; // 1 bit per page
    }

    std::uint32_t transPerTIP(std::uint32_t page_size)
    {
        // Reserve header; 1 byte per txn placeholder (refine later)
        const std::uint32_t hdrReserve = 64;
        if (page_size <= hdrReserve)
            return 0;
        return (page_size - hdrReserve) / 1u;
    }

    std::uint32_t gensPerPage(std::uint32_t page_size)
    {
        // Generators as 8-byte values after header reserve
        const std::uint32_t hdrReserve = 64;
        if (page_size <= hdrReserve)
            return 0;
        return (page_size - hdrReserve) / 8u;
    }

} // namespace scratchbird::engine::ods
