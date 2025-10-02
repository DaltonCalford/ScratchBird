#include "scratchbird/core/hash_functions.h"
#include <cstring>

namespace scratchbird::core  // NOLINT(modernize-concat-nested-namespaces)
{
        // MurmurHash3 was written by Austin Appleby, and is placed in the public domain.
        // The author hereby disclaims copyright to this source code.
        // NOLINTBEGIN(readability-*,modernize-*,readability-magic-numbers,readability-identifier-length)

        // Platform-specific functions and macros
        #define ROTL64(x, r) (((x) << (r)) | ((x) >> (64 - (r))))

        // Block read - if your platform needs to do endian-swapping or can only
        // handle aligned reads, do the conversion here
        static inline uint64_t getblock64(const uint64_t* p, int i)
        {
            return p[i];
        }

        // Finalization mix - force all bits of a hash block to avalanche
        static inline uint64_t fmix64(uint64_t k)
        {
            k ^= k >> 33;
            k *= 0xff51afd7ed558ccdULL;
            k ^= k >> 33;
            k *= 0xc4ceb9fe1a85ec53ULL;
            k ^= k >> 33;
            return k;
        }

        // MurmurHash3_x64_128 - 128-bit hash for 64-bit platforms
        // We'll use only the first 64 bits for our hash index
        uint64_t MurmurHash64(const void* key, size_t len, uint64_t seed)
        {
            const uint8_t* data = static_cast<const uint8_t*>(key);
            const int nblocks = len / 16;

            uint64_t h1 = seed;
            uint64_t h2 = seed;

            const uint64_t c1 = 0x87c37b91114253d5ULL;
            const uint64_t c2 = 0x4cf5ad432745937fULL;

            // Body - process 16-byte blocks
            const uint64_t* blocks = reinterpret_cast<const uint64_t*>(data);

            for (int i = 0; i < nblocks; i++)
            {
                uint64_t k1 = getblock64(blocks, i * 2 + 0);
                uint64_t k2 = getblock64(blocks, i * 2 + 1);

                k1 *= c1;
                k1 = ROTL64(k1, 31);
                k1 *= c2;
                h1 ^= k1;

                h1 = ROTL64(h1, 27);
                h1 += h2;
                h1 = h1 * 5 + 0x52dce729;

                k2 *= c2;
                k2 = ROTL64(k2, 33);
                k2 *= c1;
                h2 ^= k2;

                h2 = ROTL64(h2, 31);
                h2 += h1;
                h2 = h2 * 5 + 0x38495ab5;
            }

            // Tail - process remaining bytes
            const uint8_t* tail = data + nblocks * 16;

            uint64_t k1 = 0;
            uint64_t k2 = 0;

            switch (len & 15)
            {
                case 15: k2 ^= static_cast<uint64_t>(tail[14]) << 48; [[fallthrough]];
                case 14: k2 ^= static_cast<uint64_t>(tail[13]) << 40; [[fallthrough]];
                case 13: k2 ^= static_cast<uint64_t>(tail[12]) << 32; [[fallthrough]];
                case 12: k2 ^= static_cast<uint64_t>(tail[11]) << 24; [[fallthrough]];
                case 11: k2 ^= static_cast<uint64_t>(tail[10]) << 16; [[fallthrough]];
                case 10: k2 ^= static_cast<uint64_t>(tail[9]) << 8; [[fallthrough]];
                case 9:  k2 ^= static_cast<uint64_t>(tail[8]) << 0;
                    k2 *= c2;
                    k2 = ROTL64(k2, 33);
                    k2 *= c1;
                    h2 ^= k2;
                    [[fallthrough]];

                case 8:  k1 ^= static_cast<uint64_t>(tail[7]) << 56; [[fallthrough]];
                case 7:  k1 ^= static_cast<uint64_t>(tail[6]) << 48; [[fallthrough]];
                case 6:  k1 ^= static_cast<uint64_t>(tail[5]) << 40; [[fallthrough]];
                case 5:  k1 ^= static_cast<uint64_t>(tail[4]) << 32; [[fallthrough]];
                case 4:  k1 ^= static_cast<uint64_t>(tail[3]) << 24; [[fallthrough]];
                case 3:  k1 ^= static_cast<uint64_t>(tail[2]) << 16; [[fallthrough]];
                case 2:  k1 ^= static_cast<uint64_t>(tail[1]) << 8; [[fallthrough]];
                case 1:  k1 ^= static_cast<uint64_t>(tail[0]) << 0;
                    k1 *= c1;
                    k1 = ROTL64(k1, 31);
                    k1 *= c2;
                    h1 ^= k1;
            }

            // Finalization
            h1 ^= len;
            h2 ^= len;

            h1 += h2;
            h2 += h1;

            h1 = fmix64(h1);
            h2 = fmix64(h2);

            h1 += h2;
            // h2 += h1; // We don't need h2 for 64-bit output

            // Return only the first 64 bits
            return h1;
        }
        // NOLINTEND(readability-*,modernize-*,readability-magic-numbers,readability-identifier-length)

} // namespace scratchbird::core
