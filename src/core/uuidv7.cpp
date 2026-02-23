/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <chrono>
#include <random>
#include <array>
#include <cstdint>
#include "scratchbird/core/uuidv7.h"

namespace scratchbird::core
{

    auto generateUuidV7() -> UuidV7Bytes
    {
        UuidV7Bytes out{};
        // Timestamp in milliseconds since Unix epoch
        auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now());
        uint64_t ts_ms = static_cast<uint64_t>(now.time_since_epoch().count());

        // Fill bytes with timestamp (per RFC 9562 layout semantics)
        // We construct as big-endian fields then store into bytes array.
        // bytes[0..5]: 48-bit Unix epoch milliseconds
        out.bytes[0] = static_cast<uint8_t>((ts_ms >> 40) & 0xFF);
        out.bytes[1] = static_cast<uint8_t>((ts_ms >> 32) & 0xFF);
        out.bytes[2] = static_cast<uint8_t>((ts_ms >> 24) & 0xFF);
        out.bytes[3] = static_cast<uint8_t>((ts_ms >> 16) & 0xFF);
        out.bytes[4] = static_cast<uint8_t>((ts_ms >> 8) & 0xFF);
        out.bytes[5] = static_cast<uint8_t>(ts_ms & 0xFF);

        // bytes[6]: version (0b0111) in high 4 bits + 4 bits of rand_a
        // bytes[7]: 8 bits of rand_a
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint16_t> dist16(0, 0x0FFF);
        std::uniform_int_distribution<unsigned int> dist8(0, 0xFF);
        uint16_t rand_a = dist16(gen);
        out.bytes[6] = static_cast<uint8_t>(0x70 | ((rand_a >> 8) & 0x0F));
        out.bytes[7] = static_cast<uint8_t>(rand_a & 0xFF);

        // bytes[8]: variant (10xxxxxx) in high 2 bits + 6 bits of rand_b_hi
        // bytes[9]: 8 bits of rand_b_lo
        auto rand_b = static_cast<uint16_t>((dist8(gen) << 8) | dist8(gen));
        out.bytes[8] = static_cast<uint8_t>(0x80 | ((rand_b >> 8) & 0x3F));
        out.bytes[9] = static_cast<uint8_t>(rand_b & 0xFF);

        // bytes[10..15]: random
        for (int i = 10; i < 16; ++i)
        {
            out.bytes[i] = dist8(gen);
        }

        return out;
    }

} // namespace scratchbird::core
