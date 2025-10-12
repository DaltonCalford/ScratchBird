#pragma once

#include <cstdint>
#include <array>
#include <functional>
#include <string>
#include <sstream>
#include <iomanip>

namespace scratchbird::core
{

    struct UuidV7Bytes
    {
        std::array<uint8_t, 16> bytes{};

        auto operator==(const UuidV7Bytes &other) const -> bool
        {
            return bytes == other.bytes;
        }

        auto operator!=(const UuidV7Bytes &other) const -> bool
        {
            return bytes != other.bytes;
        }

        auto operator<(const UuidV7Bytes &other) const -> bool
        {
            return bytes < other.bytes;
        }

        [[nodiscard]] auto toString() const -> std::string
        {
            std::stringstream ss;
            ss << std::hex << std::setfill('0');
            for (int i = 0; i < 16; ++i)
            {
                ss << std::setw(2) << static_cast<unsigned>(bytes[i]);
                if (i == 3 || i == 5 || i == 7 || i == 9)
                {
                    ss << "-";
                }
            }
            return ss.str();
        }
    };

    // Generate RFC 9562 UUID v7 bytes (time-ordered). Implementation in core.
    auto generateUuidV7() -> UuidV7Bytes;

} // namespace scratchbird::core

namespace std
{
    template <> struct hash<scratchbird::core::UuidV7Bytes>
    {
        auto operator()(const scratchbird::core::UuidV7Bytes &uuid) const -> size_t
        {
            // A simple hash function for the UUID
            size_t h = 0;
            for (size_t i = 0; i < 16; ++i)
            {
                h = h * 31 + uuid.bytes[i];
            }
            return h;
        }
    };
}
