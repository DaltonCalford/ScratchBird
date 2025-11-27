#include "scratchbird/core/gin_compression.h"
#include <algorithm>

namespace scratchbird
{
    namespace core
    {
        size_t encode_varbyte(uint64_t value, uint8_t *output)
        {
            if (value <= VARBYTE_1_BYTE_MAX)
            {
                // 1 byte: 0xxxxxxx
                output[0] = static_cast<uint8_t>(value);
                return 1;
            }
            else if (value <= VARBYTE_2_BYTE_MAX)
            {
                // 2 bytes: 10xxxxxx xxxxxxxx
                output[0] = 0x80 | static_cast<uint8_t>(value >> 8);
                output[1] = static_cast<uint8_t>(value & 0xFF);
                return 2;
            }
            else if (value <= VARBYTE_3_BYTE_MAX)
            {
                // 3 bytes: 110xxxxx xxxxxxxx xxxxxxxx
                output[0] = 0xC0 | static_cast<uint8_t>(value >> 16);
                output[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
                output[2] = static_cast<uint8_t>(value & 0xFF);
                return 3;
            }
            else if (value <= VARBYTE_4_BYTE_MAX)
            {
                // 4 bytes: 1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
                output[0] = 0xE0 | static_cast<uint8_t>(value >> 24);
                output[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
                output[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
                output[3] = static_cast<uint8_t>(value & 0xFF);
                return 4;
            }
            else if (value <= VARBYTE_5_BYTE_MAX)
            {
                // 5 bytes: 11110xxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
                output[0] = 0xF0 | static_cast<uint8_t>(value >> 32);
                output[1] = static_cast<uint8_t>((value >> 24) & 0xFF);
                output[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
                output[3] = static_cast<uint8_t>((value >> 8) & 0xFF);
                output[4] = static_cast<uint8_t>(value & 0xFF);
                return 5;
            }
            else if (value <= VARBYTE_6_BYTE_MAX)
            {
                // 6 bytes: 111110xx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
                output[0] = 0xF8 | static_cast<uint8_t>(value >> 40);
                output[1] = static_cast<uint8_t>((value >> 32) & 0xFF);
                output[2] = static_cast<uint8_t>((value >> 24) & 0xFF);
                output[3] = static_cast<uint8_t>((value >> 16) & 0xFF);
                output[4] = static_cast<uint8_t>((value >> 8) & 0xFF);
                output[5] = static_cast<uint8_t>(value & 0xFF);
                return 6;
            }
            else if (value <= VARBYTE_7_BYTE_MAX)
            {
                // 7 bytes: 1111110x xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
                output[0] = 0xFC | static_cast<uint8_t>(value >> 48);
                output[1] = static_cast<uint8_t>((value >> 40) & 0xFF);
                output[2] = static_cast<uint8_t>((value >> 32) & 0xFF);
                output[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
                output[4] = static_cast<uint8_t>((value >> 16) & 0xFF);
                output[5] = static_cast<uint8_t>((value >> 8) & 0xFF);
                output[6] = static_cast<uint8_t>(value & 0xFF);
                return 7;
            }
            else if (value <= VARBYTE_8_BYTE_MAX)
            {
                // 8 bytes: 11111110 xxxxxxxx * 7
                output[0] = 0xFE;
                output[1] = static_cast<uint8_t>((value >> 48) & 0xFF);
                output[2] = static_cast<uint8_t>((value >> 40) & 0xFF);
                output[3] = static_cast<uint8_t>((value >> 32) & 0xFF);
                output[4] = static_cast<uint8_t>((value >> 24) & 0xFF);
                output[5] = static_cast<uint8_t>((value >> 16) & 0xFF);
                output[6] = static_cast<uint8_t>((value >> 8) & 0xFF);
                output[7] = static_cast<uint8_t>(value & 0xFF);
                return 8;
            }
            else
            {
                // 9 bytes: 11111111 xxxxxxxx * 8 (full 64-bit)
                output[0] = 0xFF;
                output[1] = static_cast<uint8_t>((value >> 56) & 0xFF);
                output[2] = static_cast<uint8_t>((value >> 48) & 0xFF);
                output[3] = static_cast<uint8_t>((value >> 40) & 0xFF);
                output[4] = static_cast<uint8_t>((value >> 32) & 0xFF);
                output[5] = static_cast<uint8_t>((value >> 24) & 0xFF);
                output[6] = static_cast<uint8_t>((value >> 16) & 0xFF);
                output[7] = static_cast<uint8_t>((value >> 8) & 0xFF);
                output[8] = static_cast<uint8_t>(value & 0xFF);
                return 9;
            }
        }

        size_t decode_varbyte(const uint8_t *input, uint64_t *value_out)
        {
            uint8_t first = input[0];

            if ((first & VARBYTE_1_BYTE_MASK) == 0)
            {
                // 1 byte: 0xxxxxxx
                *value_out = first;
                return 1;
            }
            else if ((first & VARBYTE_2_BYTE_MASK) == 0x80)
            {
                // 2 bytes: 10xxxxxx xxxxxxxx
                *value_out = (static_cast<uint64_t>(first & 0x3F) << 8) | input[1];
                return 2;
            }
            else if ((first & VARBYTE_3_BYTE_MASK) == 0xC0)
            {
                // 3 bytes: 110xxxxx xxxxxxxx xxxxxxxx
                *value_out = (static_cast<uint64_t>(first & 0x1F) << 16) |
                             (static_cast<uint64_t>(input[1]) << 8) |
                             input[2];
                return 3;
            }
            else if ((first & VARBYTE_4_BYTE_MASK) == 0xE0)
            {
                // 4 bytes: 1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
                *value_out = (static_cast<uint64_t>(first & 0x0F) << 24) |
                             (static_cast<uint64_t>(input[1]) << 16) |
                             (static_cast<uint64_t>(input[2]) << 8) |
                             input[3];
                return 4;
            }
            else if ((first & VARBYTE_5_BYTE_MASK) == 0xF0)
            {
                // 5 bytes: 11110xxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
                *value_out = (static_cast<uint64_t>(first & 0x07) << 32) |
                             (static_cast<uint64_t>(input[1]) << 24) |
                             (static_cast<uint64_t>(input[2]) << 16) |
                             (static_cast<uint64_t>(input[3]) << 8) |
                             input[4];
                return 5;
            }
            else if ((first & VARBYTE_6_BYTE_MASK) == 0xF8)
            {
                // 6 bytes: 111110xx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
                *value_out = (static_cast<uint64_t>(first & 0x03) << 40) |
                             (static_cast<uint64_t>(input[1]) << 32) |
                             (static_cast<uint64_t>(input[2]) << 24) |
                             (static_cast<uint64_t>(input[3]) << 16) |
                             (static_cast<uint64_t>(input[4]) << 8) |
                             input[5];
                return 6;
            }
            else if ((first & VARBYTE_7_BYTE_MASK) == 0xFC)
            {
                // 7 bytes: 1111110x xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
                *value_out = (static_cast<uint64_t>(first & 0x01) << 48) |
                             (static_cast<uint64_t>(input[1]) << 40) |
                             (static_cast<uint64_t>(input[2]) << 32) |
                             (static_cast<uint64_t>(input[3]) << 24) |
                             (static_cast<uint64_t>(input[4]) << 16) |
                             (static_cast<uint64_t>(input[5]) << 8) |
                             input[6];
                return 7;
            }
            else if (first == VARBYTE_8_BYTE_PREFIX)
            {
                // 8 bytes: 11111110 xxxxxxxx * 7
                *value_out = (static_cast<uint64_t>(input[1]) << 48) |
                             (static_cast<uint64_t>(input[2]) << 40) |
                             (static_cast<uint64_t>(input[3]) << 32) |
                             (static_cast<uint64_t>(input[4]) << 24) |
                             (static_cast<uint64_t>(input[5]) << 16) |
                             (static_cast<uint64_t>(input[6]) << 8) |
                             input[7];
                return 8;
            }
            else if (first == VARBYTE_9_BYTE_PREFIX)
            {
                // 9 bytes: 11111111 xxxxxxxx * 8 (full 64-bit)
                *value_out = (static_cast<uint64_t>(input[1]) << 56) |
                             (static_cast<uint64_t>(input[2]) << 48) |
                             (static_cast<uint64_t>(input[3]) << 40) |
                             (static_cast<uint64_t>(input[4]) << 32) |
                             (static_cast<uint64_t>(input[5]) << 24) |
                             (static_cast<uint64_t>(input[6]) << 16) |
                             (static_cast<uint64_t>(input[7]) << 8) |
                             input[8];
                return 9;
            }
            else
            {
                // Invalid encoding
                *value_out = 0;
                return 0;
            }
        }

        size_t compress_posting_list(const uint64_t *tids, uint16_t count,
                                      uint8_t *compressed_out, size_t max_bytes)
        {
            if (count == 0)
            {
                return 0;
            }

            size_t bytes_written = 0;
            uint64_t prev_tid = 0;

            for (uint16_t i = 0; i < count; i++)
            {
                // Calculate delta (first TID is stored as-is)
                uint64_t delta = (i == 0) ? tids[i] : (tids[i] - prev_tid);

                // Check if we have enough space (worst case: 9 bytes for full 64-bit)
                if (bytes_written + 9 > max_bytes)
                {
                    // Not enough space - return 0 to indicate failure
                    return 0;
                }

                // Encode delta using varbyte
                size_t encoded = encode_varbyte(delta, compressed_out + bytes_written);
                bytes_written += encoded;
                prev_tid = tids[i];
            }

            return bytes_written;
        }

        size_t decompress_posting_list(const uint8_t *compressed, size_t compressed_bytes,
                                        uint64_t *tids_out, uint16_t max_tids)
        {
            size_t bytes_read = 0;
            uint16_t tid_count = 0;
            uint64_t current_tid = 0;

            while (bytes_read < compressed_bytes && tid_count < max_tids)
            {
                uint64_t delta;
                size_t decoded = decode_varbyte(compressed + bytes_read, &delta);

                if (decoded == 0)
                {
                    // Decoding error - return what we have so far
                    break;
                }

                bytes_read += decoded;
                current_tid += delta;
                tids_out[tid_count++] = current_tid;
            }

            return tid_count;
        }

        size_t estimate_compressed_size(const uint64_t *tids, uint16_t count)
        {
            if (count == 0)
            {
                return 0;
            }

            size_t estimated_bytes = 0;
            uint64_t prev_tid = 0;

            for (uint16_t i = 0; i < count; i++)
            {
                uint64_t delta = (i == 0) ? tids[i] : (tids[i] - prev_tid);

                // Estimate bytes for this delta
                if (delta <= VARBYTE_1_BYTE_MAX)
                {
                    estimated_bytes += 1;
                }
                else if (delta <= VARBYTE_2_BYTE_MAX)
                {
                    estimated_bytes += 2;
                }
                else if (delta <= VARBYTE_3_BYTE_MAX)
                {
                    estimated_bytes += 3;
                }
                else if (delta <= VARBYTE_4_BYTE_MAX)
                {
                    estimated_bytes += 4;
                }
                else if (delta <= VARBYTE_5_BYTE_MAX)
                {
                    estimated_bytes += 5;
                }
                else if (delta <= VARBYTE_6_BYTE_MAX)
                {
                    estimated_bytes += 6;
                }
                else if (delta <= VARBYTE_7_BYTE_MAX)
                {
                    estimated_bytes += 7;
                }
                else if (delta <= VARBYTE_8_BYTE_MAX)
                {
                    estimated_bytes += 8;
                }
                else
                {
                    estimated_bytes += 9;
                }

                prev_tid = tids[i];
            }

            return estimated_bytes;
        }

        bool should_compress(const uint64_t *tids, uint16_t count)
        {
            if (count == 0)
            {
                return false;
            }

            // Don't compress very small lists (overhead not worth it)
            if (count < 10)
            {
                return false;
            }

            size_t uncompressed_size = count * sizeof(uint64_t);
            size_t compressed_size = estimate_compressed_size(tids, count);

            // Compress if we save at least 10% space
            return (compressed_size * 10) < (uncompressed_size * 9);
        }

    } // namespace core
} // namespace scratchbird
