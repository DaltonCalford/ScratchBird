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
#include <cstddef>

namespace scratchbird
{
    namespace core
    {
        // MurmurHash3 64-bit hash function
        // This is a public domain implementation based on Austin Appleby's MurmurHash3
        // https://github.com/aappleby/smhasher

        // Hash a block of data with optional seed
        // Returns a 64-bit hash value
        uint64_t MurmurHash64(const void *key, size_t len, uint64_t seed = 0x9747b28c);

        // Convenience wrappers for common types
        inline uint64_t hash_int32(int32_t value, uint64_t seed = 0x9747b28c)
        {
            return MurmurHash64(&value, sizeof(int32_t), seed);
        }

        inline uint64_t hash_int64(int64_t value, uint64_t seed = 0x9747b28c)
        {
            return MurmurHash64(&value, sizeof(int64_t), seed);
        }

        inline uint64_t hash_double(double value, uint64_t seed = 0x9747b28c)
        {
            return MurmurHash64(&value, sizeof(double), seed);
        }

        inline uint64_t hash_string(const char *str, size_t len, uint64_t seed = 0x9747b28c)
        {
            return MurmurHash64(str, len, seed);
        }

    } // namespace core
} // namespace scratchbird
