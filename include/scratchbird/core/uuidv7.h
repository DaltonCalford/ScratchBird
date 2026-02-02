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

namespace scratchbird::core
{
    // Hash functor for ID (UuidV7Bytes) for use with unordered containers
    // Defined after std::hash specialization to avoid forward reference issues
    struct IDHash
    {
        auto operator()(const UuidV7Bytes& uuid) const -> size_t
        {
            return std::hash<UuidV7Bytes>{}(uuid);
        }
    };

    // ID type alias used throughout the codebase
    using ID = UuidV7Bytes;

} // namespace scratchbird::core
