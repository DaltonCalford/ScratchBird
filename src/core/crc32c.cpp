#include <cstddef>
#include <cstdint>
#include <array>


    namespace scratchbird::core
    {

        // Software CRC32C (Castagnoli) table-driven implementation
        static constexpr std::array<uint32_t, 256> K_CRC32C_TABLE = []
        {
            std::array<uint32_t, 256> table{};
            // Reflected polynomial for CRC32C (Castagnoli)
            const uint32_t POLY = 0x82F63B78U;
            for (uint32_t i = 0; i < 256; ++i)
            {
                uint32_t crc = i;
                for (int j = 0; j < 8; ++j)
                {
                    crc = (crc & 1U) ? ((crc >> 1) ^ POLY) : (crc >> 1);
                }
                table[i] = crc;
            }
            return table;
        }();

        auto crc32cCompute(const uint8_t *data, size_t length, uint32_t initial) -> uint32_t
        {
            uint32_t crc = initial;
            for (size_t i = 0; i < length; ++i)
            {
                crc = K_CRC32C_TABLE[(crc ^ data[i]) & 0xFFU] ^ (crc >> 8);
            }
            return crc;
        }

    } // namespace scratchbird::core

